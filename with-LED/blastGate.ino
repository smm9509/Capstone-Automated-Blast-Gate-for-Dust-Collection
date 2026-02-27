// C++ code

//motor position enum states
// we should be switching to state machines as we progress
// more robust code that will prevent future spaghetti code
enum motorPos {START, POS25, POS50, POS75, POS100} state, prevState;
volatile bool isNewState; //checks if state changes
volatile bool estopPressed = false;  //estop state

void estopISR() //handler for estop interrupt
{
  estopPressed = true; //triggers interrupt flag
}

void motorExtend()
{
  digitalWrite(5, LOW); 
  digitalWrite(6, HIGH); //extends motor forward
}
void motorRetract()
{
  digitalWrite(5, HIGH); //retracts motor backward
  digitalWrite(6, LOW); 
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

  state = START; //sets initial state at origin
  prevState = POS100; //arbitrary prevState
}

int STOPPED_DEADBAND = 30;

void loop()
{
  int potValue = analogRead(A0); //potentiometer reading
  int speed = map(potValue, 0, 1023, -255, 255); 
  //maps analog to digital range 0-1023 to -255 to 255
  //speed deadband
  if (speed > -STOPPED_DEADBAND && speed < STOPPED_DEADBAND) {
    speed = 0;
  } 
  if (estopPressed)
  {
    state = START; //retracts actuator back to start position
    estopPressed = false; //release estop
  }
  switch (state)
  {
    case START:
      break;
    case POS25: 
  		break;
  	case POS50:
  		break;
  	case POS75:
  		break;
  	case POS100:
  		break;
    default: state = START;
  }
  prevState = state; //changes state to previous

  Serial.println(speed);//debug
  if (speed > 0)
  {
    digitalWrite(10, LOW); //Idle LED deactivatesd
    digitalWrite(11, LOW); //Backward LED deactivates
    digitalWrite(12, HIGH); //Forward LED activates
    motorExtend();
  }
  else if (speed < 0)
  {
    digitalWrite(12, LOW);  //Idle LED deactivates
    digitalWrite(11, LOW);  //Forward LED deactivates
    digitalWrite(10, HIGH); //Backward LED activates
    motorRetract();
  }
  else if(speed == 0)
  {
    digitalWrite(12, LOW); //Backward LED deactivates
    digitalWrite(10, LOW); //Forward LED deactivates
    digitalWrite(11, HIGH); //Idle LED activates

  }
  analogWrite(9, abs(speed));
  //absolute so that analog always writes positive
  
}