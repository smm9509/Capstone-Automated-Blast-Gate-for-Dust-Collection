#include <EEPROM.h>

// pin definitions for soldered hardware (rev 2026-04-08)
// 15-pin connector
static const uint8_t PIN_RX         = 0;   // RX0
static const uint8_t PIN_TX         = 1;   // TX1
static const uint8_t PIN_ACS_ACCESS = 2;   // D2 — hardware interrupt
// devices on board
static const uint8_t PIN_E_STOP     = 3;   // D3 — hardware interrupt
static const uint8_t PIN_JOG_OPEN   = 4;
static const uint8_t PIN_JOG_CLOSE  = 7;
static const uint8_t LED_RED    = 10;
static const uint8_t LED_YLW    = 11;
static const uint8_t LED_GRN    = 12;
// L298N subassembly
static const uint8_t PIN_ENA        = 5;   // PWM — older code used 9
static const uint8_t PIN_IN1        = 6;
static const uint8_t PIN_IN2        = 9;   // older code used 5
// Linear Actuator Servo
static const uint8_t PIN_WIPER      = A0;

static const uint32_t SERIAL_BAUD   = 4800;
static const uint32_t SERIAL_SILENCE_US = 10e6 / SERIAL_BAUD;
static const uint8_t  MOTOR_MIN_SPD = 80;  // physical min is ~50; 80 avoids buzzing

//=============================================================================
// MotorDriver
//=============================================================================
enum Direction { EXTEND, RETRACT };

struct MotorDriver {
    void setup() {
        pinMode(PIN_IN1, OUTPUT);
        pinMode(PIN_IN2, OUTPUT);
        pinMode(PIN_ENA, OUTPUT);
        stop();
    }

    void drive(Direction dir, uint8_t spd) {
        if (dir == EXTEND) {
            digitalWrite(PIN_IN1, LOW);
            digitalWrite(PIN_IN2, HIGH);
        } else {
            digitalWrite(PIN_IN1, HIGH);
            digitalWrite(PIN_IN2, LOW);
        }
        analogWrite(PIN_ENA, spd);
    }

    void stop() {
        digitalWrite(PIN_IN1, LOW);
        digitalWrite(PIN_IN2, LOW);
        analogWrite(PIN_ENA, 0);
    }
};
MotorDriver    MOTOR;

//=============================================================================
// JogPins
//=============================================================================
struct JogPins {
    void setup() {
        pinMode(PIN_JOG_OPEN,  INPUT_PULLUP);
        pinMode(PIN_JOG_CLOSE, INPUT_PULLUP);
    }
};

bool jogOpen()  { return !digitalRead(PIN_JOG_OPEN);  } // active-low
bool jogClose() { return !digitalRead(PIN_JOG_CLOSE); } // active-low

//=============================================================================
// InterruptInput
//=============================================================================
struct InterruptInput {
    uint8_t pin;
    volatile bool flag;
    uint8_t mode;     // FALLING, CHANGE, etc.
    void (*isr)();

    void setup() {
        // INPUT vs INPUT_PULLUP determined by caller before setup()
        attachInterrupt(digitalPinToInterrupt(pin), isr, mode);
    }

    // Returns true and clears flag if it was set. Safe on AVR (single-byte volatile).
    bool checkAndClear() {
        if (!flag) return false;
        flag = false;
        return true;
    }
};

//=============================================================================
// PositionSensor  (calibration deferred — just raw read for now)
//=============================================================================
struct PositionSensor {
    void setup() {
        pinMode(PIN_WIPER, INPUT);
    }

    uint16_t readRaw() {
        return analogRead(PIN_WIPER);
    }
};
PositionSensor WIPER;

//=============================================================================
// GateController  (P-loop, copied from blastGate MOVING case)
// originally designed by Vincent, adapted by Liz
//=============================================================================
struct GateController {
    uint16_t wiperMin = 110;  // fallback; replace with EEPROM cal later
    uint16_t wiperMax = 917;
    uint16_t setpointPercent = 0; // uninitialized setpoint, is valid if you assume ACCESS is low during startup
    // in normal operation, setpoint will be bimodal, 0 and somewhere around 30, the second value is set over serial.
    int      deadband = 25;

    // Returns true when target is reached.
    bool update() {
        int currentPos = WIPER.readRaw();
        int targetPos  = map(setpointPercent, 0, 100, wiperMin, wiperMax);
        int error      = targetPos - currentPos;
        int spd        = map(abs(error), 0, 100, (int)MOTOR_MIN_SPD, 255);
        spd            = constrain(spd, (int)MOTOR_MIN_SPD, 255);
        if      (error >  deadband) { MOTOR.drive(EXTEND,  spd); }
        else if (error < -deadband) { MOTOR.drive(RETRACT, spd); }
        else                        { MOTOR.stop(); return true;  }
        return false;
    }
};

//=============================================================================
// ISR stubs — must be free functions; forward-declared for the instances below
//=============================================================================
void estopISR();

//=============================================================================
// instances
//=============================================================================
JogPins        jog;

InterruptInput estop = { PIN_E_STOP,     false, FALLING, estopISR };

void estopISR() { estop.flag = false; } // ADJUSTED estop.flag to FALSE so that e-stop does nothing
bool acs_prev = false;

GateController ctrl;
int  openPercent = 0; //remembers the amount to open the gate when ACCESS is high

enum motorStateEnum {IDLE, MOVING, LOCKOUT, JOGGING} state, prevState;
bool isNewState = false;

void setup() {
    Serial.begin(SERIAL_BAUD);

    pinMode(PIN_E_STOP,     INPUT_PULLUP);
    pinMode(PIN_ACS_ACCESS, INPUT);

    MOTOR.setup();
    jog.setup();
    WIPER.setup();
    //TODO: on reboot, sticky open position if ACCESS is high, close if ACCESS is low.
    estop.setup();

    Serial.print(";blast gate HAL ready\n");
}

void loop() {
    if (estop.checkAndClear()) {
        MOTOR.stop();
        state = LOCKOUT;
    }

    if (Serial.available()) {
        if (Serial.peek() == ':') {
            Serial.read();  // consume ':'
            char     buf[8];
            uint8_t  n      = 0;
            bool     got_nl = false;
            unsigned long t = millis();
            while (!got_nl && millis() - t < 20) {
                if (Serial.available()) {
                    char c = Serial.read();
                    if (c == '\n')              { got_nl = true; }
                    else if (n < sizeof(buf)-1) { buf[n++] = c;  }
                }
            }
            buf[n] = '\0';
            delayMicroseconds(SERIAL_SILENCE_US);
            if (!got_nl) {
                Serial.print(";ERR timeout\n");
            } else switch (buf[0]) {
                case '?': //pot query
                    Serial.print(";P" + String(WIPER.readRaw()) + "\n");
                    break;
                case 'S':{ //setpoint
                    int new_openPercent = atoi(buf + 1);
                    bool change = bool(openPercent-new_openPercent); 
                    openPercent=new_openPercent;
                    Serial.print(";S" + String(openPercent) + "\n");
                    if (digitalRead(PIN_ACS_ACCESS) && state == IDLE) {
                        if (change) state = MOVING;
                    }
                    break;}
                default:
                    Serial.print(";ERR unknown\n");
            }
        } else {
            Serial.read();  // swallow noise
        }
    }

    //=========================================================================
    // state machine
    //=========================================================================
    isNewState = (state != prevState);
    switch (state) {
        case IDLE:
        {
            bool access_val = digitalRead(PIN_ACS_ACCESS);
            if (access_val != acs_prev) {
                state = MOVING;
                acs_prev = access_val;
            }
            if (jogOpen() || jogClose()) state = JOGGING;
            break;
        }
        case MOVING:
            ctrl.setpointPercent = digitalRead(PIN_ACS_ACCESS) * openPercent;
            if (ctrl.update()) {
                state = IDLE;
                Serial.print(";I\n");
            }
            break;
        case JOGGING:
            if (jogOpen()) {
                MOTOR.drive(EXTEND, MOTOR_MIN_SPD);
            } else if (jogClose()) {
                MOTOR.drive(RETRACT, MOTOR_MIN_SPD);
            } else {
                MOTOR.stop();
                state = IDLE;
            }
            //TODO: add a label that if the user wants to hold the jogged position, they should push the E-stop.
            break;
        case LOCKOUT:
        default:
            //TODO: safety logic for exiting LOCKOUT state after e-stop or obstruction
            // blink red LED 2
            if (millis() % 100 < 50) {
                digitalWrite(LED_RED, HIGH);
            } else {
                digitalWrite(LED_RED, LOW);
            }
            break;
    }
    prevState = state;
}
