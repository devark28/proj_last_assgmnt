/**
 * Smart Traffic Light Controller
 * ================================
 * Manages traffic flow at two intersections using adaptive timing.
 * Uses non-blocking millis() for concurrent operation simulation.
 *
 * Pin Layout (Arduino Uno):
 *   Intersection 1 (North-South): Red=2, Yellow=3, Green=4, Button=5
 *   Intersection 2 (East-West)  : Red=6, Yellow=7, Green=8, Button=9
 *
 * Serial Commands (9600 baud):
 *   ? / h  - Show menu
 *   s      - Show system status
 *   l      - Print event log
 *   c      - Clear event log
 *   1      - Override Intersection 1 -> RED
 *   2      - Override Intersection 1 -> GREEN
 *   3      - Override Intersection 2 -> RED
 *   4      - Override Intersection 2 -> GREEN
 *   r      - Release all manual overrides
 *   p      - Pause / Resume automatic monitoring
 */

#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------
// Constants
// ----------------------------------------------------------------
#define NUM_INTERSECTIONS    2

#define STATE_RED            0
#define STATE_YELLOW         1
#define STATE_GREEN          2

#define BASE_GREEN_MS        5000UL   // normal green phase
#define EXTENDED_GREEN_MS    9000UL   // extended when traffic detected
#define YELLOW_MS            2000UL
#define BASE_RED_MS          7000UL

#define MAX_LOG_ENTRIES      20       // circular log buffer size

// ----------------------------------------------------------------
// Data Structures
// ----------------------------------------------------------------

typedef struct {
    char          name[16];
    uint8_t       pinRed;
    uint8_t       pinYellow;
    uint8_t       pinGreen;
    uint8_t       pinButton;

    uint8_t       state;              // current STATE_*
    uint8_t       prevButtonState;
    unsigned long lastChange;         // millis() at last state change
    unsigned long greenTime;          // current green phase duration
    unsigned long yellowTime;
    unsigned long redTime;

    int          *vehicleCount;       // dynamically allocated counter
    bool          trafficWaiting;     // button pressed while RED
    bool          manualOverride;
    uint8_t       overrideState;
} Intersection;

typedef struct {
    unsigned long timestamp;
    char          intersectionName[16];
    uint8_t       newState;
    int           vehicleCount;
} LogEntry;

// ----------------------------------------------------------------
// Globals
// ----------------------------------------------------------------
static Intersection **intersections = NULL;   // array of pointers (heap)
static LogEntry      *logBuffer     = NULL;   // circular log (heap)
static int            logCount      = 0;
static bool           monitoringActive = true;

// ----------------------------------------------------------------
// Prototypes
// ----------------------------------------------------------------
void         initSystem(void);
void         updateIntersections(void);
void         updateSingleIntersection(Intersection *inter, int otherIdx);
void         setLightState(Intersection *inter, uint8_t newState);
void         checkButtonPress(Intersection *inter);
void         addLogEntry(Intersection *inter);
void         handleSerial(void);
void         printStatus(void);
void         printLog(void);
void         clearLog(void);
void         freeAll(void);
const char  *stateName(uint8_t state);

// ----------------------------------------------------------------
// Arduino entry points
// ----------------------------------------------------------------

void setup() {
    Serial.begin(9600);
    initSystem();
    Serial.println(F("=== Smart Traffic Light Controller ==="));
    Serial.println(F("Type '?' for the command menu."));
}

void loop() {
    if (monitoringActive) {
        updateIntersections();
    }
    if (Serial.available() > 0) {
        handleSerial();
    }
}

// ----------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------

void initSystem() {
    // Allocate array of intersection pointers
    intersections = (Intersection **)malloc(NUM_INTERSECTIONS * sizeof(Intersection *));
    if (intersections == NULL) {
        Serial.println(F("FATAL: malloc failed for intersections"));
        while (1) ;  // halt
    }

    // Allocate circular log buffer
    logBuffer = (LogEntry *)malloc(MAX_LOG_ENTRIES * sizeof(LogEntry));
    if (logBuffer == NULL) {
        Serial.println(F("FATAL: malloc failed for log buffer"));
        free(intersections);
        while (1) ;
    }

    // --- Intersection 0: North-South ---
    intersections[0] = (Intersection *)malloc(sizeof(Intersection));
    if (!intersections[0]) { Serial.println(F("FATAL: alloc inter[0]")); while(1); }

    strncpy(intersections[0]->name, "North-South", 15);
    intersections[0]->pinRed          = 2;
    intersections[0]->pinYellow       = 3;
    intersections[0]->pinGreen        = 4;
    intersections[0]->pinButton       = 5;
    intersections[0]->state           = STATE_GREEN;   // starts green
    intersections[0]->prevButtonState = HIGH;
    intersections[0]->lastChange      = millis();
    intersections[0]->greenTime       = BASE_GREEN_MS;
    intersections[0]->yellowTime      = YELLOW_MS;
    intersections[0]->redTime         = BASE_RED_MS;
    intersections[0]->trafficWaiting  = false;
    intersections[0]->manualOverride  = false;
    intersections[0]->overrideState   = STATE_RED;
    intersections[0]->vehicleCount    = (int *)malloc(sizeof(int));
    if (!intersections[0]->vehicleCount) { Serial.println(F("FATAL: alloc vc[0]")); while(1); }
    *intersections[0]->vehicleCount   = 0;

    // --- Intersection 1: East-West ---
    intersections[1] = (Intersection *)malloc(sizeof(Intersection));
    if (!intersections[1]) { Serial.println(F("FATAL: alloc inter[1]")); while(1); }

    strncpy(intersections[1]->name, "East-West", 15);
    intersections[1]->pinRed          = 6;
    intersections[1]->pinYellow       = 7;
    intersections[1]->pinGreen        = 8;
    intersections[1]->pinButton       = 9;
    intersections[1]->state           = STATE_RED;     // starts red (NS goes first)
    intersections[1]->prevButtonState = HIGH;
    intersections[1]->lastChange      = millis();
    intersections[1]->greenTime       = BASE_GREEN_MS;
    intersections[1]->yellowTime      = YELLOW_MS;
    intersections[1]->redTime         = BASE_RED_MS;
    intersections[1]->trafficWaiting  = false;
    intersections[1]->manualOverride  = false;
    intersections[1]->overrideState   = STATE_RED;
    intersections[1]->vehicleCount    = (int *)malloc(sizeof(int));
    if (!intersections[1]->vehicleCount) { Serial.println(F("FATAL: alloc vc[1]")); while(1); }
    *intersections[1]->vehicleCount   = 0;

    // Configure Arduino pins
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *inter = intersections[i];
        pinMode(inter->pinRed,    OUTPUT);
        pinMode(inter->pinYellow, OUTPUT);
        pinMode(inter->pinGreen,  OUTPUT);
        pinMode(inter->pinButton, INPUT_PULLUP);
        setLightState(inter, inter->state);
    }
}

// ----------------------------------------------------------------
// Core Control Logic
// ----------------------------------------------------------------

void updateIntersections() {
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        checkButtonPress(intersections[i]);
        updateSingleIntersection(intersections[i], 1 - i);
    }
}

/* Check for vehicle button press using edge detection */
void checkButtonPress(Intersection *inter) {
    uint8_t current = digitalRead(inter->pinButton);

    // Falling edge = button pressed (INPUT_PULLUP: idle=HIGH, pressed=LOW)
    if (current == LOW && inter->prevButtonState == HIGH) {
        (*inter->vehicleCount)++;
        inter->trafficWaiting = true;

        // If currently green extend the phase dynamically
        if (inter->state == STATE_GREEN) {
            inter->greenTime = EXTENDED_GREEN_MS;
        }

        Serial.print(F("[BTN] Vehicle at "));
        Serial.print(inter->name);
        Serial.print(F("  total: "));
        Serial.println(*inter->vehicleCount);
    }
    inter->prevButtonState = current;
}

void updateSingleIntersection(Intersection *inter, int otherIdx) {
    unsigned long now     = millis();
    unsigned long elapsed = now - inter->lastChange;

    // Manual override: force state each cycle, skip normal FSM
    if (inter->manualOverride) {
        setLightState(inter, inter->overrideState);
        return;
    }

    switch (inter->state) {

        case STATE_GREEN:
            if (elapsed >= inter->greenTime) {
                setLightState(inter, STATE_YELLOW);
                inter->greenTime = BASE_GREEN_MS;  // reset for next cycle
                addLogEntry(inter);
            }
            break;

        case STATE_YELLOW:
            if (elapsed >= inter->yellowTime) {
                setLightState(inter, STATE_RED);
                inter->trafficWaiting = false;
                addLogEntry(inter);

                // Coordinate: switch the other intersection to GREEN
                if (otherIdx >= 0 && otherIdx < NUM_INTERSECTIONS) {
                    Intersection *other = intersections[otherIdx];
                    if (other->state == STATE_RED && !other->manualOverride) {
                        // Extend green for the other side if it has waiting traffic
                        other->greenTime = other->trafficWaiting
                                           ? EXTENDED_GREEN_MS
                                           : BASE_GREEN_MS;
                        setLightState(other, STATE_GREEN);
                        addLogEntry(other);
                    }
                }
            }
            break;

        case STATE_RED:
            // Waiting: transition is triggered by the other intersection's YELLOW->RED
            break;

        default:
            // Safety: invalid state detected - force RED
            Serial.print(F("ERR: invalid state on "));
            Serial.println(inter->name);
            setLightState(inter, STATE_RED);
            break;
    }
}

void setLightState(Intersection *inter, uint8_t newState) {
    // Validate before applying
    if (newState > STATE_GREEN) {
        Serial.println(F("ERR: setLightState received out-of-range state"));
        newState = STATE_RED;
    }

    // Turn all off first to prevent overlap
    digitalWrite(inter->pinRed,    LOW);
    digitalWrite(inter->pinYellow, LOW);
    digitalWrite(inter->pinGreen,  LOW);

    inter->state      = newState;
    inter->lastChange = millis();

    switch (newState) {
        case STATE_RED:    digitalWrite(inter->pinRed,    HIGH); break;
        case STATE_YELLOW: digitalWrite(inter->pinYellow, HIGH); break;
        case STATE_GREEN:  digitalWrite(inter->pinGreen,  HIGH); break;
    }
}

// ----------------------------------------------------------------
// Logging
// ----------------------------------------------------------------

void addLogEntry(Intersection *inter) {
    if (logCount >= MAX_LOG_ENTRIES) {
        // Shift oldest entry out (FIFO)
        memmove(&logBuffer[0], &logBuffer[1],
                (MAX_LOG_ENTRIES - 1) * sizeof(LogEntry));
        logCount = MAX_LOG_ENTRIES - 1;
    }
    logBuffer[logCount].timestamp    = millis();
    strncpy(logBuffer[logCount].intersectionName, inter->name, 15);
    logBuffer[logCount].intersectionName[15] = '\0';
    logBuffer[logCount].newState     = inter->state;
    logBuffer[logCount].vehicleCount = *inter->vehicleCount;
    logCount++;
}

// ----------------------------------------------------------------
// Serial Interface
// ----------------------------------------------------------------

void handleSerial() {
    char cmd = (char)Serial.read();

    switch (cmd) {
        case '?':
        case 'h':
            Serial.println(F("\n--- Command Menu ---"));
            Serial.println(F("  s  - System status"));
            Serial.println(F("  l  - Event log"));
            Serial.println(F("  c  - Clear log"));
            Serial.println(F("  1  - Override NS -> RED"));
            Serial.println(F("  2  - Override NS -> GREEN"));
            Serial.println(F("  3  - Override EW -> RED"));
            Serial.println(F("  4  - Override EW -> GREEN"));
            Serial.println(F("  r  - Release overrides"));
            Serial.println(F("  p  - Pause/Resume"));
            Serial.println(F("--------------------"));
            break;

        case 's': printStatus(); break;
        case 'l': printLog();    break;
        case 'c': clearLog();    break;

        case '1':
            intersections[0]->manualOverride = true;
            intersections[0]->overrideState  = STATE_RED;
            Serial.println(F("[OVR] North-South forced RED"));
            break;
        case '2':
            intersections[0]->manualOverride = true;
            intersections[0]->overrideState  = STATE_GREEN;
            Serial.println(F("[OVR] North-South forced GREEN"));
            break;
        case '3':
            intersections[1]->manualOverride = true;
            intersections[1]->overrideState  = STATE_RED;
            Serial.println(F("[OVR] East-West forced RED"));
            break;
        case '4':
            intersections[1]->manualOverride = true;
            intersections[1]->overrideState  = STATE_GREEN;
            Serial.println(F("[OVR] East-West forced GREEN"));
            break;

        case 'r':
            for (int i = 0; i < NUM_INTERSECTIONS; i++) {
                intersections[i]->manualOverride = false;
            }
            Serial.println(F("[OVR] All overrides released"));
            break;

        case 'p':
            monitoringActive = !monitoringActive;
            Serial.println(monitoringActive
                           ? F("[SYS] Monitoring RESUMED")
                           : F("[SYS] Monitoring PAUSED"));
            break;

        default:
            break;  // silently ignore unknown/whitespace characters
    }
    delay(10);  // small debounce so we don't read partial input
}

void printStatus() {
    Serial.println(F("\n====== SYSTEM STATUS ======"));
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *inter = intersections[i];
        Serial.print(F("[ ")); Serial.print(inter->name); Serial.println(F(" ]"));
        Serial.print(F("  State    : ")); Serial.println(stateName(inter->state));
        Serial.print(F("  Vehicles : ")); Serial.println(*inter->vehicleCount);
        Serial.print(F("  Traffic? : ")); Serial.println(inter->trafficWaiting ? F("Yes") : F("No"));
        Serial.print(F("  Override : ")); Serial.println(inter->manualOverride ? F("Active") : F("Off"));
        if (inter->manualOverride) {
            Serial.print(F("  OvrState : "));
            Serial.println(stateName(inter->overrideState));
        }
    }
    Serial.print(F("Log entries : ")); Serial.println(logCount);
    Serial.print(F("Uptime (ms) : ")); Serial.println(millis());
    Serial.println(F("==========================="));
}

void printLog() {
    if (logCount == 0) {
        Serial.println(F("Log is empty."));
        return;
    }
    Serial.println(F("\n====== EVENT LOG ======"));
    for (int i = 0; i < logCount; i++) {
        Serial.print(logBuffer[i].timestamp);
        Serial.print(F("ms  "));
        Serial.print(logBuffer[i].intersectionName);
        Serial.print(F(" -> "));
        Serial.print(stateName(logBuffer[i].newState));
        Serial.print(F("  (vehicles: "));
        Serial.print(logBuffer[i].vehicleCount);
        Serial.println(F(")"));
    }
    Serial.println(F("======================="));
}

void clearLog() {
    logCount = 0;
    Serial.println(F("Log cleared."));
}

const char *stateName(uint8_t state) {
    switch (state) {
        case STATE_RED:    return "RED";
        case STATE_YELLOW: return "YELLOW";
        case STATE_GREEN:  return "GREEN";
        default:           return "UNKNOWN";
    }
}

// ----------------------------------------------------------------
// Memory cleanup (call if resetting)
// ----------------------------------------------------------------
void freeAll() {
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        if (intersections[i]) {
            free(intersections[i]->vehicleCount);
            free(intersections[i]);
            intersections[i] = NULL;
        }
    }
    free(intersections);
    free(logBuffer);
    intersections = NULL;
    logBuffer     = NULL;
    logCount      = 0;
}
