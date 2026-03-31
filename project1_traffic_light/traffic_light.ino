#include <stdlib.h>
#include <string.h>

#define STATE_RED             0
#define STATE_YELLOW          1
#define STATE_GREEN           2
#define BASE_GREEN_MS         5000UL
#define EXTENDED_GREEN_MS     9000UL
#define YELLOW_MS             2000UL
#define NUM_INTERSECTIONS     2
#define MAX_LOG_ENTRIES       20

// Pin assignments:
// Intersection 0 (North-South): Red=2, Yellow=3, Green=4, Button=5
// Intersection 1 (East-West)  : Red=6, Yellow=7, Green=8, Button=9
// LEDs  : anode -> 220ohm resistor -> pin; cathode -> GND rail
// Buttons: one leg -> pin (INPUT_PULLUP); other leg -> GND rail
// Power : Arduino 5V -> breadboard + rail; Arduino GND -> breadboard - rail

typedef struct {
    char          name[16];
    uint8_t       pinRed, pinYellow, pinGreen, pinButton;
    uint8_t       state;
    uint8_t       prevBtn;
    unsigned long lastChange;
    unsigned long greenTime;
    int           vehicleCount;
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

// Declare every function manually so Arduino's auto-generator
// has nothing to produce — avoids the struct-scope bug in Tinkercad.
void setLightState(Intersection *inter, uint8_t newState);
void addLogEntry(Intersection *inter);
void checkButtonPress(Intersection *inter);
void updateSingleIntersection(Intersection *inter, int otherIdx);
void updateIntersections();
void clearLog();
void printLog();
void printStatus();
void handleSerial();
void initSystem();

// Static arrays — no malloc needed for a fixed 2-intersection system
static Intersection  inters[NUM_INTERSECTIONS];
static Intersection *intersections[NUM_INTERSECTIONS];
static LogEntry      logBuffer[MAX_LOG_ENTRIES];
static int           logCount = 0;
static bool          monitoringActive = true;

// ----------------------------------------------------------------

const char *stateName(uint8_t s) {
    if (s == STATE_RED)    return "RED";
    if (s == STATE_YELLOW) return "YELLOW";
    if (s == STATE_GREEN)  return "GREEN";
    return "UNKNOWN";
}

void setLightState(Intersection *inter, uint8_t newState) {
    if (newState > STATE_GREEN) newState = STATE_RED;
    digitalWrite(inter->pinRed,    LOW);
    digitalWrite(inter->pinYellow, LOW);
    digitalWrite(inter->pinGreen,  LOW);
    inter->state      = newState;
    inter->lastChange = millis();
    if (newState == STATE_RED)    digitalWrite(inter->pinRed,    HIGH);
    if (newState == STATE_YELLOW) digitalWrite(inter->pinYellow, HIGH);
    if (newState == STATE_GREEN)  digitalWrite(inter->pinGreen,  HIGH);
    Serial.print(inter->name); Serial.print(F(" -> ")); Serial.println(stateName(newState));
}

void addLogEntry(Intersection *inter) {
    if (logCount >= MAX_LOG_ENTRIES) {
        memmove(&logBuffer[0], &logBuffer[1], (MAX_LOG_ENTRIES - 1) * sizeof(LogEntry));
        logCount = MAX_LOG_ENTRIES - 1;
    }
    logBuffer[logCount].timestamp    = millis();
    strncpy(logBuffer[logCount].intersectionName, inter->name, 15);
    logBuffer[logCount].newState     = inter->state;
    logBuffer[logCount].vehicleCount = inter->vehicleCount;
    logCount++;
}

void checkButtonPress(Intersection *inter) {
    uint8_t btn = digitalRead(inter->pinButton);
    if (btn == LOW && inter->prevBtn == HIGH) {
        inter->vehicleCount++;
        inter->trafficWaiting = true;
        if (inter->state == STATE_GREEN)
            inter->greenTime = EXTENDED_GREEN_MS;
        Serial.print(F("[BTN] Vehicle at "));
        Serial.print(inter->name);
        Serial.print(F("  total: "));
        Serial.println(inter->vehicleCount);
    }
    inter->prevBtn = btn;
}

void updateSingleIntersection(Intersection *inter, int otherIdx) {
    unsigned long elapsed = millis() - inter->lastChange;
    if (inter->manualOverride) { setLightState(inter, inter->overrideState); return; }

    if (inter->state == STATE_GREEN && elapsed >= inter->greenTime) {
        setLightState(inter, STATE_YELLOW);
        inter->greenTime = BASE_GREEN_MS;
        addLogEntry(inter);
    } else if (inter->state == STATE_YELLOW && elapsed >= YELLOW_MS) {
        setLightState(inter, STATE_RED);
        inter->trafficWaiting = false;
        addLogEntry(inter);
        Intersection *other = intersections[otherIdx];
        if (other->state == STATE_RED && !other->manualOverride) {
            other->greenTime = other->trafficWaiting ? EXTENDED_GREEN_MS : BASE_GREEN_MS;
            setLightState(other, STATE_GREEN);
            addLogEntry(other);
        }
    }
}

void updateIntersections() {
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        checkButtonPress(intersections[i]);
        updateSingleIntersection(intersections[i], 1 - i);
    }
}

void clearLog() { logCount = 0; Serial.println(F("Log cleared.")); }

void printLog() {
    if (logCount == 0) { Serial.println(F("Log empty.")); return; }
    Serial.println(F("\n--- EVENT LOG ---"));
    for (int i = 0; i < logCount; i++) {
        Serial.print(logBuffer[i].timestamp);        Serial.print(F("ms  "));
        Serial.print(logBuffer[i].intersectionName); Serial.print(F(" -> "));
        Serial.print(stateName(logBuffer[i].newState)); Serial.print(F("  v:"));
        Serial.println(logBuffer[i].vehicleCount);
    }
    Serial.println(F("-----------------"));
}

void printStatus() {
    Serial.println(F("\n--- STATUS ---"));
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *inter = intersections[i];
        Serial.print(F("[ ")); Serial.print(inter->name); Serial.println(F(" ]"));
        Serial.print(F("  State   : ")); Serial.println(stateName(inter->state));
        Serial.print(F("  Vehicles: ")); Serial.println(inter->vehicleCount);
        Serial.print(F("  Waiting : ")); Serial.println(inter->trafficWaiting ? F("Yes") : F("No"));
        Serial.print(F("  Override: ")); Serial.println(inter->manualOverride ? F("ON") : F("off"));
    }
    Serial.print(F("Log entries: ")); Serial.println(logCount);
    Serial.println(F("--------------"));
}

void handleSerial() {
    char cmd = (char)Serial.read();
    switch (cmd) {
        case '?': case 'h':
            Serial.println(F("s=status l=log c=clear 1=NS-RED 2=NS-GRN 3=EW-RED 4=EW-GRN r=release p=pause"));
            break;
        case 's': printStatus(); break;
        case 'l': printLog();    break;
        case 'c': clearLog();    break;
        case '1': intersections[0]->manualOverride=true;  intersections[0]->overrideState=STATE_RED;   Serial.println(F("[OVR] NS->RED")); break;
        case '2': intersections[0]->manualOverride=true;  intersections[0]->overrideState=STATE_GREEN; Serial.println(F("[OVR] NS->GRN")); break;
        case '3': intersections[1]->manualOverride=true;  intersections[1]->overrideState=STATE_RED;   Serial.println(F("[OVR] EW->RED")); break;
        case '4': intersections[1]->manualOverride=true;  intersections[1]->overrideState=STATE_GREEN; Serial.println(F("[OVR] EW->GRN")); break;
        case 'r':
            for (int i = 0; i < NUM_INTERSECTIONS; i++) intersections[i]->manualOverride = false;
            Serial.println(F("Overrides released")); break;
        case 'p':
            monitoringActive = !monitoringActive;
            Serial.println(monitoringActive ? F("Resumed") : F("Paused")); break;
        default: break;
    }
}

void initSystem() {
    intersections[0] = &inters[0];
    strncpy(inters[0].name, "North-South", 15);
    inters[0].pinRed=2; inters[0].pinYellow=3; inters[0].pinGreen=4; inters[0].pinButton=5;
    inters[0].state=STATE_GREEN; inters[0].prevBtn=HIGH;
    inters[0].lastChange=0; inters[0].greenTime=BASE_GREEN_MS;
    inters[0].vehicleCount=0; inters[0].trafficWaiting=false;
    inters[0].manualOverride=false; inters[0].overrideState=STATE_RED;

    intersections[1] = &inters[1];
    strncpy(inters[1].name, "East-West", 15);
    inters[1].pinRed=6; inters[1].pinYellow=7; inters[1].pinGreen=8; inters[1].pinButton=9;
    inters[1].state=STATE_RED; inters[1].prevBtn=HIGH;
    inters[1].lastChange=0; inters[1].greenTime=BASE_GREEN_MS;
    inters[1].vehicleCount=0; inters[1].trafficWaiting=false;
    inters[1].manualOverride=false; inters[1].overrideState=STATE_RED;
}

void setup() {
    Serial.begin(9600);
    initSystem();
    for (int i = 0; i < NUM_INTERSECTIONS; i++) {
        Intersection *in = intersections[i];
        pinMode(in->pinRed,    OUTPUT);
        pinMode(in->pinYellow, OUTPUT);
        pinMode(in->pinGreen,  OUTPUT);
        pinMode(in->pinButton, INPUT_PULLUP);
        setLightState(in, in->state);
    }
    Serial.println(F("Traffic Light Controller ready. Type '?' for help."));
}

void loop() {
    if (monitoringActive) updateIntersections();
    if (Serial.available() > 0) handleSerial();
}
