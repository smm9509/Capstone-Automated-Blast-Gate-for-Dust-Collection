// C++ code
#include <EEPROM.h>

// pin definitions for soldered hardware (rev 2026-04-08)
// pin definitions - a representation of the hardware as it is wired today.
// 15-pin connector
    static const uint8_t RX = 0; // pin RX0
    static const uint8_t TX = 1; // pin TX1
    static const uint8_t ACS_ACCESS = 2; //pin D2
    //static const uint8_t BUSIO1 = ; //Tom asked to express the gate status on a binary signal, but we haven't sorted that out yet.
// devices on board - HARDWARE NOT INSTALLED AS OF 2026-04-08
    static const uint8_t E_STOP_PIN = 3;
    static const uint8_t LED_RED = 10;
    static const uint8_t LED_YLW = 11;
    static const uint8_t LED_GRN = 12;
    // TODO: add hardware for powered-on manual override with 3-position ON-OFF-ON switch
    static const uint8_t JOG_OPEN = 4;
    static const uint8_t JOG_CLOSE = 7;
// connections to L298N subassembly
    static const uint8_t ENA = 5; //warning: older code assumes ENA was wired to pin 9 instead of 5
    static const uint8_t IN1 = 6;
    static const uint8_t IN2 = 9; //warning: older code assumes IN2 was wired to pin 5 instead of 9
// connections to Linear Actuator Servo
    static const uint8_t WIPER = A0;

// state machine variables
// we should be switching to state machines as we progress
// more robust code that will prevent future spaghetti code
enum motorStateEnum {IDLE, MOVING, ESTOP} state, prevState;
volatile bool isNewState; //checks if state changes
volatile bool estopPressed = false;  //estop state
int targetPercent;
int targetPos;
volatile bool acsAccessChanged = false; //set by ISR on any edge
int openSetpoint = 50; //TODO: updated from serial by Vincen's branch

//wiper calibration
uint16_t wiperMin;
uint16_t wiperMax;
const uint32_t wiper_calibration_magic = 0xBE291A1; //change this value to force recalibration after flashing new firmware
const uint16_t wiper_calibration_addr = 0x318; //between 0x200 and 0x400 to fit in unused area of Arduino Nano EEPROM
struct WiperCalibration {
    uint32_t magic;
    uint16_t minVal;
    uint16_t maxVal;
};

int currentPos = 0;
int currentPercent = 0;
const int minSpeed = 80; //physical minimum is 50 but it buzzes so 80 is safer
int error;
int speed;
const int deadband = 0;
int kp = 1.5;

void estopISR() //handler for estop interrupt
{
  estopPressed = true; //triggers interrupt flag
}
void acsAccessISR()
{
  acsAccessChanged = true;
}

void motorExtend(int speed)
{
  digitalWrite(IN2, LOW);
  digitalWrite(IN1, HIGH); //extends motor forward
  analogWrite(ENA, speed);
}
void motorRetract(int speed)
{
  digitalWrite(IN2, HIGH); //retracts motor backward
  digitalWrite(IN1, LOW);
  analogWrite(ENA, speed);
}
void motorStop()
{
  digitalWrite(IN2, LOW);
  digitalWrite(IN1, LOW);
  analogWrite(ENA, 0);
}

void setup()
{
  Serial.begin(115200); //baud rate of ESP32
  pinMode(ACS_ACCESS, INPUT);
  attachInterrupt(digitalPinToInterrupt(ACS_ACCESS), acsAccessISR, CHANGE);
  pinMode(E_STOP_PIN, INPUT_PULLUP); //moved LEDS to other pins, 2 and 3 have hardware interrupt which we need
  attachInterrupt(digitalPinToInterrupt(E_STOP_PIN), estopISR, FALLING); //enables hardware interrupt on 2, falling edge, triggers estop ISR
  pinMode(JOG_OPEN, INPUT_PULLUP);
  pinMode(JOG_CLOSE, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT); //RED LED BACKWARD
  pinMode(LED_YLW, OUTPUT); //YELLOW LED IDLE
  pinMode(LED_GRN, OUTPUT); //GREEN LED FORWARD
  pinMode(IN1, OUTPUT); //negative motor term
  pinMode(IN2, OUTPUT); //positive motor term
  pinMode(ENA, OUTPUT); //pwm pin
  pinMode(WIPER, INPUT); //potentiometer reading pin

  {  //wiper calibration block
    WiperCalibration cal;
    EEPROM.get(wiper_calibration_addr, cal);
    if (cal.magic == wiper_calibration_magic) {
      wiperMin = cal.minVal;
      wiperMax = cal.maxVal;
    } else {
      //TODO: get the user's attention that calibration is needed. Even if serial is not connected.
      // LEDs? Morse code C on the builtin LED?
      Serial.println("Please jog the gate to closed position");
      //wait/poll for closing to finish
      {
        bool jog_close_was_pressed = false;
        while (!jog_close_was_pressed || !digitalRead(JOG_CLOSE)) {
          if (!digitalRead(JOG_CLOSE)) {
            motorRetract(minSpeed);
            jog_close_was_pressed = true;
          } else if (!digitalRead(JOG_OPEN)) {
            motorExtend(minSpeed);
          } else {
            motorStop();
          }
        }
      }
      wiperMin = analogRead(WIPER);
      Serial.println("Please jog the gate to open position");
      //wait/poll for opening to finish
      {
        bool jog_open_was_pressed = false;
        while (!jog_open_was_pressed || !digitalRead(JOG_OPEN)) {
          if (!digitalRead(JOG_OPEN)) {
            motorExtend(minSpeed);
            jog_open_was_pressed = true;
          } else if (!digitalRead(JOG_CLOSE)) {
            motorRetract(minSpeed);
          } else {
            motorStop();
          }
        }
      }
      wiperMax = analogRead(WIPER);

      //save calibration
      cal.magic = wiper_calibration_magic;
      cal.minVal = wiperMin;
      cal.maxVal = wiperMax;
      EEPROM.put(wiper_calibration_addr, cal);
    }
  }

  state = IDLE;       //sets initial state at origin
  prevState = ESTOP;  //arbitrary prevState
}

void loop() {
  if (estopPressed) {
    state = ESTOP; //retracts actuator back to start position
    estopPressed = false; //release estop
  }
  if (acsAccessChanged) {
    acsAccessChanged = false;
    if (digitalRead(ACS_ACCESS)) {
      targetPercent = openSetpoint;
    } else {
      targetPercent = 0;
    }
    if (state == IDLE) state = MOVING;
  }
  if (Serial.available()) {
    if (Serial.peek() == int('?')) {  //READ case
      while (Serial.available()) { Serial.read(); }
      Serial.println(analogRead(WIPER));
    } else { //WRITE setpoint case
      targetPercent = Serial.parseInt();  //reads integers only, but the /n remains
      //TODO: Vincent — replace this with ACS communication protocol
      while (Serial.available())  //clears /n by reading the serial again
      {
        Serial.read();  //reads the serial which is usually /n and clears it
                        //** somethign to note: if user puts a float, it will run the integer half and the decimal half
      }
      targetPercent = constrain(targetPercent, 0, 100);            //clamps targetPercent to a range
      targetPos = map(targetPercent, 0, 100, wiperMin, wiperMax);  //maps the percent given to an analog reading 0-1023
      Serial.print("Percent set:");
      Serial.println(targetPercent);  //print target percent
      openSetpoint = targetPercent;   //use serial to set the opening amount
      Serial.print("Position set:");
      Serial.println(targetPos);  //print target pos
      if (state == IDLE) state = MOVING; //goes to moving section
    }
  }
  isNewState = (state != prevState);
  switch (state) {
    case IDLE:
      if (isNewState) Serial.println("Enter positon from 0-100%");
      break;
    case MOVING:
      targetPos = map(targetPercent, 0, 100, wiperMin, wiperMax);
      error = targetPos - currentPos; //error is difference between target and current for controls, doubles as a comparison, + = extend, - = retract
      speed = map(abs(error), 0, 100, minSpeed, 255); //proportional speed to error distance, P
      //usually 1023 but i put 100 so that it goes max speed thne slows down when it gets closer instead of gradual
      speed = constrain(speed, minSpeed, 255); //safety constraint
      if (error > deadband) //targetpos is greater than currentpos, extend, deadband for small errors
      {
        motorExtend(speed); //keeps extending until current reaches target
      }
      else if (error < -deadband) //targetpos is less than currentpos, retract
      {
        motorRetract(speed); //keeps retracting until current reaches target
      }
      else
      {
        motorStop(); //stop motor
        state = IDLE; //wait for next command
      }
      break;
    case ESTOP:
      motorStop(); //stops motor
      break;
    default: state = IDLE;
  }
  prevState = state; //changes state to previous

  currentPos = analogRead(WIPER); //position reading, analog 0 - 1023
  currentPercent = map(currentPos, wiperMin, wiperMax, 0, 100); //percent reading, changes current to percent reading (debug for now)
}
