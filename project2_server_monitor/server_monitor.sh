#!/bin/bash
# ==============================================================
# Linux Server Health Monitoring and Alert Automation Script
# ==============================================================
# Description : Monitors CPU, memory, disk usage and processes.
#               Triggers alerts when thresholds are exceeded.
#               Provides an interactive menu-driven interface.
#               Supports continuous background monitoring.
# ==============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/server_health.log"
CONFIG_FILE="${SCRIPT_DIR}/monitor.conf"
PID_FILE="${SCRIPT_DIR}/monitor.pid"
MONITOR_LOG="${SCRIPT_DIR}/monitor_bg.log"

# ---------------------------------------------------------------
# Default threshold values (percentage)
# ---------------------------------------------------------------
DEFAULT_CPU_THRESHOLD=80
DEFAULT_MEM_THRESHOLD=85
DEFAULT_DISK_THRESHOLD=90
MONITOR_INTERVAL=60   # seconds between background checks

# ---------------------------------------------------------------
# Colour codes for terminal output
# ---------------------------------------------------------------
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'  # reset

# ---------------------------------------------------------------
# Logging
# ---------------------------------------------------------------
log_message() {
    local level="$1"
    local message="$2"
    local ts
    ts=$(date '+%Y-%m-%d %H:%M:%S')
    printf "[%s] [%-7s] %s\n" "$ts" "$level" "$message" >> "$LOG_FILE"
}

log_and_print() {
    local level="$1"
    local message="$2"
    log_message "$level" "$message"
    case "$level" in
        WARNING) printf "${YELLOW}[!] %s${NC}\n" "$message" ;;
        ERROR)   printf "${RED}[ERROR] %s${NC}\n" "$message" ;;
        INFO)    printf "${GREEN}[i] %s${NC}\n" "$message" ;;
        *)       printf "%s\n" "$message" ;;
    esac
}

# ---------------------------------------------------------------
# Dependency check
# ---------------------------------------------------------------
check_dependencies() {
    local deps=("top" "free" "df" "ps" "awk" "grep" "bc" "sed")
    local missing=()
    for cmd in "${deps[@]}"; do
        if ! command -v "$cmd" &>/dev/null; then
            missing+=("$cmd")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        printf "${RED}ERROR: Missing required commands: %s${NC}\n" "${missing[*]}"
        printf "Please install the missing utilities and try again.\n"
        exit 1
    fi
}

# ---------------------------------------------------------------
# Config management
# ---------------------------------------------------------------
load_config() {
    if [[ -f "$CONFIG_FILE" ]]; then
        # shellcheck source=/dev/null
        source "$CONFIG_FILE"
    else
        CPU_THRESHOLD=$DEFAULT_CPU_THRESHOLD
        MEM_THRESHOLD=$DEFAULT_MEM_THRESHOLD
        DISK_THRESHOLD=$DEFAULT_DISK_THRESHOLD
        save_config
    fi
}

save_config() {
    cat > "$CONFIG_FILE" <<EOF
# Server Monitor Configuration
CPU_THRESHOLD=${CPU_THRESHOLD}
MEM_THRESHOLD=${MEM_THRESHOLD}
DISK_THRESHOLD=${DISK_THRESHOLD}
MONITOR_INTERVAL=${MONITOR_INTERVAL}
EOF
}

# ---------------------------------------------------------------
# Metric collection functions
# ---------------------------------------------------------------

# Returns CPU usage percentage (integer, 0-100)
get_cpu_usage() {
    local idle
    # top -bn1 outputs one-shot batch; grab %id (idle) field
    idle=$(top -bn1 | grep -E "^(%Cpu|Cpu\(s\))" | awk '{
        for (i=1; i<=NF; i++) {
            if ($i ~ /id/) { gsub(/,/, "", $(i-1)); print $(i-1); exit }
        }
    }')
    if [[ -z "$idle" || "$idle" == "N/A" ]]; then
        echo "N/A"
        return
    fi
    # usage = 100 - idle
    printf "%.1f" "$(echo "100 - $idle" | bc -l 2>/dev/null || echo 0)"
}

# Returns memory usage percentage
get_memory_usage() {
    local mem_line total used
    mem_line=$(free | grep "^Mem:")
    total=$(echo "$mem_line" | awk '{print $2}')
    used=$(echo "$mem_line" | awk '{print $3}')
    if [[ "$total" -eq 0 ]]; then
        echo "N/A"
        return
    fi
    awk "BEGIN { printf \"%.1f\", ($used / $total) * 100 }"
}

# Returns memory usage in human-readable form
get_memory_details() {
    free -h | grep "^Mem:" | awk '{printf "Used: %s / %s (available: %s)", $3, $2, $7}'
}

# Returns disk usage % for root partition
get_disk_usage() {
    df / | awk 'NR==2 { gsub(/%/, "", $5); print $5 }'
}

# Returns disk usage human-readable
get_disk_details() {
    df -h / | awk 'NR==2 { printf "Used: %s / %s (%s used)", $3, $2, $5 }'
}

# Returns number of running processes
get_process_count() {
    ps ax | wc -l | tr -d ' '
}

# Returns top-5 processes by CPU
get_top_processes() {
    ps aux --sort=-%cpu 2>/dev/null | awk 'NR==1{next} NR<=6 {
        printf "    %-12s %5s%%  %5s%%  %s\n", $1, $3, $4, $11
    }'
}

# ---------------------------------------------------------------
# Health display
# ---------------------------------------------------------------
display_health() {
    local cpu mem disk procs
    cpu=$(get_cpu_usage)
    mem=$(get_memory_usage)
    disk=$(get_disk_usage)
    procs=$(get_process_count)

    clear
    printf "${BOLD}${CYAN}"
    printf "+================================================+\n"
    printf "|       SERVER HEALTH MONITOR DASHBOARD         |\n"
    printf "+================================================+${NC}\n"
    printf "  Date/Time  : %s\n" "$(date '+%Y-%m-%d %H:%M:%S')"
    printf "  Hostname   : %s\n" "$(hostname)"
    printf "\n"

    # Colour-code metrics based on thresholds
    _print_metric "CPU Usage"  "$cpu"  "$CPU_THRESHOLD"  "%"
    _print_metric "Memory"     "$mem"  "$MEM_THRESHOLD"  "%"
    _print_metric "Disk (/)"   "$disk" "$DISK_THRESHOLD" "%"

    printf "  Processes  : %s active\n" "$procs"
    printf "\n"
    printf "  Memory  : %s\n" "$(get_memory_details)"
    printf "  Disk    : %s\n" "$(get_disk_details)"
    printf "\n"
    printf "${BOLD}  Top Processes (CPU):${NC}\n"
    printf "    %-12s %6s  %6s  %s\n" "USER" "CPU%" "MEM%" "COMMAND"
    get_top_processes
    printf "\n"
    printf "  Thresholds: CPU≥%s%% | MEM≥%s%% | DISK≥%s%%\n" \
           "$CPU_THRESHOLD" "$MEM_THRESHOLD" "$DISK_THRESHOLD"
    printf "================================================\n"

    check_thresholds "$cpu" "$mem" "$disk"
}

_print_metric() {
    local label="$1" value="$2" threshold="$3" unit="$4"
    local color="$GREEN"
    if [[ "$value" != "N/A" ]]; then
        if (( $(echo "$value >= $threshold" | bc -l 2>/dev/null || echo 0) )); then
            color="$RED"
        elif (( $(echo "$value >= $threshold * 0.85" | bc -l 2>/dev/null || echo 0) )); then
            color="$YELLOW"
        fi
    fi
    printf "  %-10s : ${color}%s%s${NC}\n" "$label" "$value" "$unit"
}

# ---------------------------------------------------------------
# Threshold checking and alerting
# ---------------------------------------------------------------
check_thresholds() {
    local cpu="$1" mem="$2" disk="$3"
    local alert_triggered=false

    if [[ "$cpu" != "N/A" ]] && \
       (( $(echo "$cpu >= $CPU_THRESHOLD" | bc -l 2>/dev/null || echo 0) )); then
        log_and_print "WARNING" "CPU at ${cpu}% — threshold ${CPU_THRESHOLD}%"
        alert_triggered=true
    fi

    if [[ "$mem" != "N/A" ]] && \
       (( $(echo "$mem >= $MEM_THRESHOLD" | bc -l 2>/dev/null || echo 0) )); then
        log_and_print "WARNING" "Memory at ${mem}% — threshold ${MEM_THRESHOLD}%"
        alert_triggered=true
    fi

    # disk is an integer from df output
    if [[ "$disk" =~ ^[0-9]+$ ]] && [[ "$disk" -ge "$DISK_THRESHOLD" ]]; then
        log_and_print "WARNING" "Disk at ${disk}% — threshold ${DISK_THRESHOLD}%"
        alert_triggered=true
    fi

    if [[ "$alert_triggered" == false ]]; then
        log_message "INFO" "Health check OK — CPU:${cpu}% MEM:${mem}% DISK:${disk}%"
    fi
}

# ---------------------------------------------------------------
# Threshold configuration
# ---------------------------------------------------------------
configure_thresholds() {
    printf "\n${BOLD}=== Configure Monitoring Thresholds ===${NC}\n"
    printf "  Current: CPU=%s%%  MEM=%s%%  DISK=%s%%\n\n" \
           "$CPU_THRESHOLD" "$MEM_THRESHOLD" "$DISK_THRESHOLD"

    _read_threshold "CPU threshold (1-100)"  CPU_THRESHOLD
    _read_threshold "Memory threshold (1-100)" MEM_THRESHOLD
    _read_threshold "Disk threshold (1-100)"   DISK_THRESHOLD

    save_config
    log_message "INFO" "Thresholds updated: CPU=${CPU_THRESHOLD} MEM=${MEM_THRESHOLD} DISK=${DISK_THRESHOLD}"
    printf "${GREEN}Thresholds saved.${NC}\n"
}

_read_threshold() {
    local label="$1"
    local -n _var="$2"   # nameref

    while true; do
        read -rp "  Enter ${label} [current: ${_var}]: " input
        if [[ -z "$input" ]]; then
            break  # keep existing value
        elif [[ "$input" =~ ^[0-9]+$ ]] && [[ "$input" -ge 1 ]] && [[ "$input" -le 100 ]]; then
            _var="$input"
            break
        else
            printf "  ${RED}Invalid. Please enter a number between 1 and 100.${NC}\n"
        fi
    done
}

# ---------------------------------------------------------------
# Log management
# ---------------------------------------------------------------
view_logs() {
    printf "\n${BOLD}=== Activity Log (last 60 lines) ===${NC}\n"
    if [[ -f "$LOG_FILE" && -s "$LOG_FILE" ]]; then
        tail -60 "$LOG_FILE"
    else
        printf "  (log file is empty or does not exist)\n"
    fi
    printf "=====================================\n"
}

clear_logs() {
    printf "\n"
    read -rp "Clear all log entries? This cannot be undone. (y/N): " confirm
    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        > "$LOG_FILE"
        log_message "INFO" "Log cleared by user"
        printf "${GREEN}Log cleared.${NC}\n"
    else
        printf "Cancelled.\n"
    fi
}

# ---------------------------------------------------------------
# Background monitoring daemon
# ---------------------------------------------------------------
start_monitoring() {
    if [[ -f "$PID_FILE" ]]; then
        local existing_pid
        existing_pid=$(cat "$PID_FILE")
        if kill -0 "$existing_pid" 2>/dev/null; then
            printf "${YELLOW}Monitoring is already running (PID %s).${NC}\n" "$existing_pid"
            return
        else
            rm -f "$PID_FILE"
        fi
    fi

    (
        log_message "INFO" "Background monitoring started"
        while true; do
            local cpu mem disk
            cpu=$(get_cpu_usage)
            mem=$(get_memory_usage)
            disk=$(get_disk_usage)

            # Re-load config in case thresholds were changed interactively
            load_config

            check_thresholds "$cpu" "$mem" "$disk"
            sleep "$MONITOR_INTERVAL"
        done
    ) >> "$MONITOR_LOG" 2>&1 &

    echo $! > "$PID_FILE"
    log_message "INFO" "Background monitoring started — PID $!"
    printf "${GREEN}Background monitoring started (PID %s, interval %ss).${NC}\n" \
           "$!" "$MONITOR_INTERVAL"
}

stop_monitoring() {
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            rm -f "$PID_FILE"
            log_message "INFO" "Background monitoring stopped"
            printf "${GREEN}Monitoring stopped (was PID %s).${NC}\n" "$pid"
        else
            rm -f "$PID_FILE"
            printf "${YELLOW}No active monitoring process found (stale PID file removed).${NC}\n"
        fi
    else
        printf "Background monitoring is not running.\n"
    fi
}

monitoring_status() {
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            printf "  Background monitoring: ${GREEN}RUNNING${NC} (PID %s)\n" "$pid"
        else
            printf "  Background monitoring: ${YELLOW}STOPPED${NC} (stale PID file)\n"
        fi
    else
        printf "  Background monitoring: ${RED}NOT RUNNING${NC}\n"
    fi
}

# ---------------------------------------------------------------
# Cleanup on exit
# ---------------------------------------------------------------
cleanup() {
    printf "\nCaught interrupt — exiting cleanly.\n"
    log_message "INFO" "Script terminated by signal"
    exit 0
}
trap cleanup INT TERM

# ---------------------------------------------------------------
# Main interactive menu
# ---------------------------------------------------------------
main_menu() {
    while true; do
        printf "\n${BOLD}${CYAN}"
        printf "+=======================================+\n"
        printf "|    Linux Server Health Monitor v1.0   |\n"
        printf "+=======================================+${NC}\n"
        monitoring_status
        printf "\n"
        printf "  1. Display current system health\n"
        printf "  2. Configure monitoring thresholds\n"
        printf "  3. View activity logs\n"
        printf "  4. Clear logs\n"
        printf "  5. Start background monitoring\n"
        printf "  6. Stop background monitoring\n"
        printf "  7. Exit\n"
        printf "\n"
        read -rp "Select option [1-7]: " choice

        case "$choice" in
            1) display_health        ;;
            2) configure_thresholds  ;;
            3) view_logs             ;;
            4) clear_logs            ;;
            5) start_monitoring      ;;
            6) stop_monitoring       ;;
            7)
                stop_monitoring 2>/dev/null || true
                log_message "INFO" "Script exited normally"
                printf "Goodbye.\n"
                exit 0
                ;;
            "")
                # user just hit enter — don't print an error
                ;;
            *)
                printf "${RED}Invalid option '%s'. Please select 1-7.${NC}\n" "$choice"
                ;;
        esac
    done
}

# ---------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------
check_dependencies
load_config
log_message "INFO" "Server Health Monitor started"
main_menu
