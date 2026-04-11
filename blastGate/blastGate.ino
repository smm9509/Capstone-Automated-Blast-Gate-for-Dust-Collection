// C++ code

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
    static const uint8_t OVERRIDE_OPEN = 4;
    static const uint8_t OVERRIDE_CLOSE = 7;
// connections to L298N subassembly
    static const uint8_t ENA = 5; //warning: older code assumes ENA was wired to pin 9 instead of 5
    static const uint8_t IN1 = 6;
    static const uint8_t IN2 = 9; //warning: older code assumes IN2 was wired to pin 5 instead of 9
// connections to Linear Actuator Servo
    static const uint8_t WIPER = A0;

//motor position enum states
// we should be switching to state machines as we progress
// more robust code that will prevent future spaghetti code
enum motorPos {IDLE, MOVING, ESTOP} state, prevState;
volatile bool isNewState; //checks if state changes
volatile bool estopPressed = false;  //estop state
int targetPercent;
int targetPos;
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
  pinMode(E_STOP_PIN, INPUT_PULLUP); //moved LEDS to other pins, 2 and 3 have hardware interrupt which we need
  attachInterrupt(digitalPinToInterrupt(E_STOP_PIN), estopISR, FALLING); //enables hardware interrupt on 2, falling edge, triggers estop ISR
  pinMode(LED_RED, OUTPUT); //RED LED BACKWARD
  pinMode(LED_YLW, OUTPUT); //YELLOW LED IDLE
  pinMode(LED_GRN, OUTPUT); //GREEN LED FORWARD
  pinMode(IN1, OUTPUT); //negative motor term
  pinMode(IN2, OUTPUT); //positive motor term
  pinMode(ENA, OUTPUT); //pwm pin
  pinMode(WIPER, INPUT); //potentiometer reading pin

  state = IDLE; //sets initial state at origin
  prevState = ESTOP; //arbitrary prevState
}

void loop()
{
  if (estopPressed)
  {
    state = ESTOP; //retracts actuator back to start position
    estopPressed = false; //release estop
  }
  isNewState = (state != prevState);
  switch (state)
  {
    case IDLE:
      if (isNewState) Serial.println("Enter positon from 0-100%");
      if (Serial.available())
      {
        targetPercent = Serial.parseInt(); //reads integers only, but the /n remains
        while (Serial.available()) //clears /n by reading the serial again
        {
          Serial.read(); //reads the serial which is usually /n and clears it
          //** somethign to note: if user puts a float, it will run the integer half and the decimal half
        }
        targetPercent = constrain(targetPercent, 0, 100); //clamps targetPercent to a range
        targetPos = map(targetPercent, 0, 100,  0, 1023); //maps the percent given to an analog reading 0-1023
        Serial.print("Percent set:");
        Serial.println(targetPercent); //print target percent
        Serial.print("Position set:");
        Serial.println(targetPos); //print target pos
        state = MOVING; //goes to moving section
      }
      break;
    case MOVING:
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
  currentPercent = map(currentPos, 0, 1023, 0, 100); //percent reading, changes current to percent reading (debug for now)

}
