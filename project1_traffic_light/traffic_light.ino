#include <stdlib.h>
#include <string.h>

// ---- Pin assignments ----
// Intersection 1 (North-South): Red=2, Yellow=3, Green=4, Button=5
// Intersection 2 (East-West)  : Red=6, Yellow=7, Green=8, Button=9
// LEDs: anode -> 220ohm resistor -> Arduino pin; cathode -> GND rail
// Buttons: one leg -> Arduino pin (INPUT_PULLUP); other leg -> GND rail
// Power: Arduino 5V -> breadboard positive rail, Arduino GND -> breadboard negative rail

// ---- State constants ----
#define STATE_RED    0
#define STATE_YELLOW 1
#define STATE_GREEN  2

// ---- Timing (ms) ----
#define BASE_GREEN_MS     5000UL
#define EXTENDED_GREEN_MS 9000UL
#define YELLOW_MS         2000UL
#define BASE_RED_MS       7000UL

#define NUM_INTERSECTIONS 2
#define MAX_LOG_ENTRIES   20

// ---- Data structures (must be before any function that uses them) ----
typedef struct {
    char          name[16];
    uint8_t       pinRed, pinYellow, pinGreen, pinButton;
    uint8_t       state;
    uint8_t       prevBtn;
    unsigned long lastChange;
    unsigned long greenTime;
    int          *vehicleCount;  // heap-allocated
    bool          trafficWaiting;
    bool          manualOverride;
    uint8_t       overrideState;
} Intersection;

typedef struct {
    unsigned long timestamp;
    char          intersectionName[16];
    uint8_t       newState;
    int           vehicleCount;
} LogEntry;

// ---- Globals ----
static Intersection **intersections = NULL;
static LogEntry      *logBuffer     = NULL;
static int            logCount      = 0;
static bool           monitoringActive = true;

// ---- Helper: state name string ----
const char *stateName(uint8_t s) {
    if (s == STATE_RED)    return "RED";
    if (s == STATE_YELLOW) return "YELLOW";
    if (s == STATE_GREEN)  return "GREEN";
    return "UNKNOWN";
}

// ---- Set a light and record the timestamp ----
void setLightState(Intersection *inter, uint8_t newState) {
    if (newState > STATE_GREEN) {
        Serial.println(F("ERR: bad state, forcing RED"));
        newState = STATE_RED;
    }
    digitalWrite(inter->pinRed,    LOW);
    digitalWrite(inter->pinYellow, LOW);
    digitalWrite(inter->pinGreen,  LOW);
    inter->state      = newState;
    inter->lastChange = millis();
    if (newState == STATE_RED)    digitalWrite(inter->pinRed,    HIGH);
    if (newState == STATE_YELLOW) digitalWrite(inter->pinYellow, HIGH);
    if (newState == STATE_GREEN)  digitalWrite(inter->pinGreen,  HIGH);
}

// ---- Append an event to the circular log ----
void addLogEntry(Intersection *inter) {
    if (logCount >= MAX_LOG_ENTRIES) {
        memmove(&logBuffer[0], &logBuffer[1], (MAX_LOG_ENTRIES - 1) * sizeof(LogEntry));
        logCount = MAX_LOG_ENTRIES - 1;
    }
    logBuffer[logCount].timestamp    = millis();
    strncpy(logBuffer[logCount].intersectionName, inter->name, 15);
    logBuffer[logCount].newState     = inter->state;
    logBuffer[logCount].vehicleCount = *inter->vehicleCount;
    logCount++;
}

// ---- Detect button press and update vehicle count ----
void checkButtonPress(Intersection *inter) {
    uint8_t btn = digitalRead(inter->pinButton);
    if (btn == LOW && inter->prevBtn == HIGH) {
        (*inter->vehicleCount)++;
        inter->trafficWaiting = true;
        if (inter->state == STATE_GREEN)
            inter->greenTime = EXTENDED_GREEN_MS;
        Serial.print(F("[BTN] Vehicle at "));
        Serial.print(inter->name);
        Serial.print(F("  total: "));
        Serial.println(*inter->vehicleCount);
    }
    inter->prevBtn = btn;
}

// ---- FSM update for one intersection ----
void updateSingleIntersection(Intersection *inter, int otherIdx) {
    unsigned long elapsed = millis() - inter->lastChange;

    if (inter->manualOverride) {
        setLightState(inter, inter->overrideState);
        return;
    }

    if (inter->state == STATE_GREEN && elapsed >= inter->greenTime) {
        setLightState(inter, STATE_YELLOW);
        inter->greenTime = BASE_GREEN_MS;
        addLogEntry(inter);
    }
    else if (inter->state == STATE_YELLOW && elapsed >= YELLOW_MS) {
        setLightState(inter, STATE_RED);
        inter->trafficWaiting = false;
        addLogEntry(inter);
        // hand green over to the other intersection
        Intersection *other = intersections[otherIdx];
        if (other->state == STATE_RED && !other->manualOverride) {
            other->greenTime = other->trafficWaiting ? EXTENDED_GREEN_MS : BASE_GREEN_MS;
            setLightState(other, STATE_GREEN);
            addLogEntry(other);
        }
    }
}

// ---- Poll both intersections each loop ----
void updateIntersections() {
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        checkButtonPress(intersections[i]);
        updateSingleIntersection(intersections[i], 1 - i);
    }
}

// ---- Serial output helpers ----
void clearLog() {
    logCount = 0;
    Serial.println(F("Log cleared."));
}

void printLog() {
    if (logCount == 0) { Serial.println(F("Log empty.")); return; }
    Serial.println(F("\n--- EVENT LOG ---"));
    for (int i = 0; i < logCount; i++) {
        Serial.print(logBuffer[i].timestamp);
        Serial.print(F("ms  "));
        Serial.print(logBuffer[i].intersectionName);
        Serial.print(F(" -> "));
        Serial.print(stateName(logBuffer[i].newState));
        Serial.print(F("  vehicles: "));
        Serial.println(logBuffer[i].vehicleCount);
    }
    Serial.println(F("-----------------"));
}

void printStatus() {
    Serial.println(F("\n--- STATUS ---"));
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *inter = intersections[i];
        Serial.print(F("[ ")); Serial.print(inter->name); Serial.println(F(" ]"));
        Serial.print(F("  State    : ")); Serial.println(stateName(inter->state));
        Serial.print(F("  Vehicles : ")); Serial.println(*inter->vehicleCount);
        Serial.print(F("  Traffic? : ")); Serial.println(inter->trafficWaiting ? F("Yes") : F("No"));
        Serial.print(F("  Override : ")); Serial.println(inter->manualOverride ? F("ON") : F("off"));
    }
    Serial.print(F("Log entries : ")); Serial.println(logCount);
    Serial.println(F("--------------"));
}

// ---- Serial command menu ----
void handleSerial() {
    char cmd = (char)Serial.read();
    switch (cmd) {
        case '?': case 'h':
            Serial.println(F("\n--- MENU ---"));
            Serial.println(F("s - status"));
            Serial.println(F("l - log"));
            Serial.println(F("c - clear log"));
            Serial.println(F("1 - NS force RED"));
            Serial.println(F("2 - NS force GREEN"));
            Serial.println(F("3 - EW force RED"));
            Serial.println(F("4 - EW force GREEN"));
            Serial.println(F("r - release overrides"));
            Serial.println(F("p - pause/resume"));
            Serial.println(F("------------"));
            break;
        case 's': printStatus(); break;
        case 'l': printLog();    break;
        case 'c': clearLog();    break;
        case '1': intersections[0]->manualOverride = true;  intersections[0]->overrideState = STATE_RED;   Serial.println(F("[OVR] NS -> RED"));   break;
        case '2': intersections[0]->manualOverride = true;  intersections[0]->overrideState = STATE_GREEN; Serial.println(F("[OVR] NS -> GREEN")); break;
        case '3': intersections[1]->manualOverride = true;  intersections[1]->overrideState = STATE_RED;   Serial.println(F("[OVR] EW -> RED"));   break;
        case '4': intersections[1]->manualOverride = true;  intersections[1]->overrideState = STATE_GREEN; Serial.println(F("[OVR] EW -> GREEN")); break;
        case 'r':
            for (int i = 0; i < NUM_INTERSECTIONS; i++) intersections[i]->manualOverride = false;
            Serial.println(F("Overrides released"));
            break;
        case 'p':
            monitoringActive = !monitoringActive;
            Serial.println(monitoringActive ? F("Resumed") : F("Paused"));
            break;
        default: break;
    }
    delay(10);  // debounce serial input
}

// ---- Allocate and configure one intersection ----
Intersection *makeIntersection(const char *name,
                               uint8_t r, uint8_t y, uint8_t g, uint8_t btn,
                               uint8_t initState) {
    Intersection *inter = (Intersection *)malloc(sizeof(Intersection));
    if (!inter) { Serial.println(F("FATAL: malloc")); while (1); }
    strncpy(inter->name, name, 15);
    inter->pinRed    = r; inter->pinYellow = y;
    inter->pinGreen  = g; inter->pinButton = btn;
    inter->state     = initState;
    inter->prevBtn   = HIGH;
    inter->lastChange = millis();
    inter->greenTime  = BASE_GREEN_MS;
    inter->trafficWaiting = false;
    inter->manualOverride = false;
    inter->overrideState  = STATE_RED;
    inter->vehicleCount   = (int *)malloc(sizeof(int));
    if (!inter->vehicleCount) { Serial.println(F("FATAL: malloc vc")); while (1); }
    *inter->vehicleCount  = 0;
    return inter;
}

// ---- System init ----
void initSystem() {
    intersections = (Intersection **)malloc(NUM_INTERSECTIONS * sizeof(Intersection *));
    if (!intersections) { Serial.println(F("FATAL: malloc arr")); while (1); }

    logBuffer = (LogEntry *)malloc(MAX_LOG_ENTRIES * sizeof(LogEntry));
    if (!logBuffer) { Serial.println(F("FATAL: malloc log")); while (1); }

    intersections[0] = makeIntersection("North-South", 2, 3, 4, 5, STATE_GREEN);
    intersections[1] = makeIntersection("East-West",   6, 7, 8, 9, STATE_RED);

    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *in = intersections[i];
        pinMode(in->pinRed, OUTPUT); pinMode(in->pinYellow, OUTPUT);
        pinMode(in->pinGreen, OUTPUT); pinMode(in->pinButton, INPUT_PULLUP);
        setLightState(in, in->state);
    }
}

// ---- Arduino entry points ----
void setup() {
    Serial.begin(9600);
    initSystem();
    Serial.println(F("Smart Traffic Light Controller"));
    Serial.println(F("Type '?' for menu"));
}

void loop() {
    if (monitoringActive) updateIntersections();
    if (Serial.available() > 0) handleSerial();
}
