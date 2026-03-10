// C++ code

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
  digitalWrite(5, LOW); 
  digitalWrite(6, HIGH); //extends motor forward
  analogWrite(9, speed);
}
void motorRetract(int speed)
{
  digitalWrite(5, HIGH); //retracts motor backward
  digitalWrite(6, LOW); 
  analogWrite(9, speed);
}
void motorStop()
{
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  analogWrite(9, 0);
}

void setup()
{
  Serial.begin(115200); //baud rate of ESP32
  pinMode(2, INPUT_PULLUP); //moved LEDS to other pins, 2 and 3 have hardware interrupt which we need
  attachInterrupt(digitalPinToInterrupt(2), estopISR, FALLING); //enables hardware interrupt on 2, falling edge, triggers estop ISR
  pinMode(10, OUTPUT); //RED LED BACKWARD
  pinMode(11, OUTPUT); //YELLOW LED IDLE
  pinMode(12, OUTPUT); //GREEN LED FORWARD
  pinMode(5, OUTPUT); //negative motor term
  pinMode(6, OUTPUT); //positive motor term
  pinMode(9, OUTPUT); //pwm pin
  pinMode(A0, INPUT); //potentiometer reading pin

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

  currentPos = analogRead(A0); //position reading, analog 0 - 1023
  currentPercent = map(currentPos, 0, 1023, 0, 100); //percent reading, changes current to percent reading (debug for now)
  
}