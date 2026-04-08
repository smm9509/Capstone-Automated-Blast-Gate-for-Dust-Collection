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
//int kp = 1.5;
const int maxVelocity = 4000; //max velocity, assume constant, ignore static pressure loss
const int maxCFM = 1200; //max CFM with ALL GATES OPEN
const float maxArea = 143.73; //143.73 is total effective area from all diameter calculations

int activeGates;

int CFMreq[6] = {400, 350, 300, 350, 350, 450}; //bandsaw, belt sander, spindle sandar, planar, tablesaw, floor sweep
int diameter[6] = {5, 6, 6, 6, 5, 5};
float ductArea[6] = {19.64, 28.27, 28.27, 28.27, 19.64, 19.64}; //in inches, because max area is inches and i only need it as a ratio
float qFull[6] = {545.55, 785.27, 785.27, 785.27, 545.55, 545.55}; //CFM for each gate when it is fully open

uint8_t currentPacket;
uint8_t previousPacket = 0;

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

float qDevice(uint8_t currentPacket, int newGateIndex)
{
  float currentArea = 0.0;
  float allGateWeight = 0.0;
  for (int i = 0; i < 6; i++) //cycles through size of array
  {
    if (bitRead(currentPacket, i)) //outputs true if bit is 1
    {
      currentArea += ductArea[i]; 
      allGateWeight += CFMreq[i] * ductArea[i]; //weighted sum using cfmreq and ductarea for all the gates
    }
  }
  float qtotalCurrent = maxCFM * (currentArea/maxArea);  //calculates current cfm using proportion of area
  float newGateWeight = CFMreq[newGateIndex] * ductArea[newGateIndex];
  float qNewGate = qtotalCurrent * (newGateWeight/allGateWeight);
  return qNewGate;
}
bool packetTest(uint8_t currentPacket)
{
  for (int i = 0; i < 6; i++) //cycles through all active gates in packet and makes sure each of them can get their required CFM
  {
    if (bitRead(currentPacket, i))
    {
      if (qDevice(currentPacket, i) < CFMreq[i]) //any of the open gates receive less than required, return false
      return false; 
    }
  }
  return true; //else this is a valid packet
}
int newGateIndex(uint8_t newGate)
{
  for (int i = 0; i < 6; i++)
  {
    if bitRead(newGate, i)
    return i;
  }
  return -1;
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
    state = ESTOP; //goes into estop state
  }
  isNewState = (state != prevState);
  switch (state)
  {
    case IDLE:
      if (Serial.available())
      {
        currentPacket = Serial.read();
        if (packetTest(currentPacket)) //tests if currentPacket will overload CFMreq
        {
          uint8_t newGate = currentPacket & ~previousPacket; //singles out only gates that turned on
          int deviceID = newGateIndex(newGate);
          if (deviceID != -1)
          {
          targetPercent = 100 * (qDevice(currentPacket, deviceID)/qFull[deviceID]);
          targetPercent = constrain(targetPercent, 0, 100); //clamps targetPercent to a range
          targetPos = map(targetPercent, 0, 100,  0, 1023); //maps the percent given to an analog reading 0-1023

          Serial.print("Percent set:");
          Serial.println(targetPercent); //print target percent

          Serial.print("Position set:");
          Serial.println(targetPos); //print target pos

          previousPacket = currentPacket;
          state = MOVING; //goes to moving section
          }
        }
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
      if (currentPos > 0)
      {
        motorRetract(255); //retract all the way back to start
      }
      break;
    default: state = IDLE;
  }
  prevState = state; //changes state to previous

  currentPos = analogRead(A0); //position reading, analog 0 - 1023
  currentPercent = map(currentPos, 0, 1023, 0, 100); //percent reading, changes current to percent reading (debug for now)
}