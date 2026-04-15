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
// LEDs NOT INSTALLED AS OF 2026-04-08, pins are floating
static const uint8_t PIN_LED_RED    = 10;
static const uint8_t PIN_LED_YLW    = 11;
static const uint8_t PIN_LED_GRN    = 12;
// L298N subassembly
static const uint8_t PIN_ENA        = 5;   // PWM — older code used 9
static const uint8_t PIN_IN1        = 6;
static const uint8_t PIN_IN2        = 9;   // older code used 5
// Linear Actuator Servo
static const uint8_t PIN_WIPER      = A0;

static const uint32_t SERIAL_BAUD   = 4800;
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

//=============================================================================
// ISR stubs — must be free functions; forward-declared for the instances below
//=============================================================================
void estopISR();
void acsISR();

//=============================================================================
// Hardware instances
//=============================================================================
MotorDriver    motor;
JogPins        jog;
PositionSensor wiper;

InterruptInput estop = { PIN_E_STOP,     false, FALLING, estopISR };
InterruptInput acs   = { PIN_ACS_ACCESS, false, CHANGE,  acsISR   };

void estopISR() { estop.flag = true; }
void acsISR()   { acs.flag   = true; }

//=============================================================================
// setup / loop
//=============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD);

    pinMode(PIN_E_STOP,     INPUT_PULLUP);
    pinMode(PIN_ACS_ACCESS, INPUT);

    motor.setup();
    jog.setup();
    wiper.setup();
    estop.setup();
    acs.setup();

    Serial.println("HAL ready");
}

void loop() {
    if (estop.checkAndClear()) {
        motor.stop();
        Serial.println("ESTOP");
    }
    if (acs.checkAndClear()) {
        Serial.print("ACS edge, ACCESS=");
        Serial.println(digitalRead(PIN_ACS_ACCESS));
    }

    if (jogOpen()) {
        motor.drive(EXTEND, MOTOR_MIN_SPD);
    } else if (jogClose()) {
        motor.drive(RETRACT, MOTOR_MIN_SPD);
    } else {
        motor.stop();
    }

    Serial.println(wiper.readRaw());
    delay(100);
}
