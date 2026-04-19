// C++ code

//motor position enum states
enum motorPos {IDLE, MOVING, ESTOP} state, prevState;
volatile bool isNewState;
volatile bool estopPressed = false;
int targetPercent;
int targetPos;
int currentPos = 0;
int currentPercent = 0;
const int minSpeed = 80;
int error;
int speed;
const int deadband = 0;
const int maxCFM = 1200;
const float maxArea = 143.73;

int activeGates;

int CFMreq[6] = {400, 350, 300, 350, 350, 450}; //bandsaw, belt sander, spindle sander, planar, tablesaw, floor sweep
int diameter[6] = {5, 6, 6, 6, 5, 5};
float ductArea[6] = {19.64, 28.27, 28.27, 28.27, 19.64, 19.64}; //area of blast gate in inches
float qFull[6] = {545.55, 785.27, 785.27, 785.27, 545.55, 545.55}; //q = 4000 * A = Cubic feet per inch

uint8_t currentPacket;
uint8_t previousPacket = 0;

void estopISR()
{
  estopPressed = true;
}
void motorExtend(int speed)
{
  digitalWrite(5, LOW);
  digitalWrite(6, HIGH);
  analogWrite(9, speed);
}
void motorRetract(int speed)
{
  digitalWrite(5, HIGH);
  digitalWrite(6, LOW);
  analogWrite(9, speed);
}
void motorStop()
{
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  analogWrite(9, 0);
}

float qDevice(uint8_t packet, int gateIndex)
{
  float qDemand = 0.0;
  float allGateWeight = 0.0;
  //qDemand will decide if system is sufficient/starved
  //allGateWeight will be used for distribution if system is starved

  //calculates total demand of all the gates
  for (int i = 0; i < 6; i++)
  {
    if (bitRead(packet, i))
    {
      qDemand += qFull[i]; //for each active gate open, add their max capacity to qDemand, aka their CFM at 100%
      allGateWeight += CFMreq[i]; //adds each CFMrequirement, I realize we only care about CFMreq and NOT area
    }
  }
  if (allGateWeight == 0) return 0.0; //prevent dividing by zero

  //if total demand is less than max CFM, our system is sufficient, will tell new gate to move to their full capacity aka 100% position
  if (qDemand <= maxCFM) 
  {
    return qFull[gateIndex];
  }
  else //trigger starvation balancing, qDemand > maxCFM BUT totalCFMreq < maxCFM, it can still operate but has to distribute
  {
    float newGateWeight = CFMreq[gateIndex]; //takes in CFMreq of new gate
    return maxCFM * (newGateWeight / allGateWeight); //distributes maxCFM comparing cfmrequirements of each gate
  } 
}

bool packetTest(uint8_t packet) //packet is valid ONLY if each gate's calculated CFM is greater than its CFMREQ
{
  for (int i = 0; i < 6; i++)
  {
    if (bitRead(packet, i))
    {
      if (qDevice(packet, i) < CFMreq[i]) //if any gate calculation goes below CFMreq, reject packet
        return false;
    }
  }
  return true;
}

int newGateIndex(uint8_t newGate)
{
  for (int i = 0; i < 6; i++)
  {
    if (bitRead(newGate, i)) //finds the index of the newgate and returns it
      return i;
  }
  return -1;
}

void setup()
{
  Serial.begin(115200, SERIAL_8N1);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), estopISR, FALLING);
  //pinMode(10, OUTPUT); //RED LED
  //pinMode(11, OUTPUT); //YELLOW LED
  //pinMode(12, OUTPUT); //GREEN LED
  pinMode(5, OUTPUT);  //negative motor term
  pinMode(6, OUTPUT);  //positive motor term
  pinMode(9, OUTPUT);  //pwm pin
  pinMode(A0, INPUT);  //potentiometer

  state = IDLE;
  prevState = ESTOP;
}

void loop()
{
  if (estopPressed)
  {
    state = ESTOP;
  }
  isNewState = (state != prevState);
  switch (state)
  {
    case IDLE:
      if (isNewState) Serial.println("Waiting for packet...");
      if (Serial.available())
      {
        currentPacket = Serial.read(); 
        if (packetTest(currentPacket))
        {
          uint8_t newGate = currentPacket & ~previousPacket; //finds the different bit of the current vs previous
          int deviceID = newGateIndex(newGate); //finds ID of the different bit aka newGate
          if (deviceID != -1) 
          {
            float q = qDevice(currentPacket, deviceID); //calculates the flow of current gate
            targetPercent = 100.0 * (q / qFull[deviceID]); //finds the targetpercent needed to reach that flow
            targetPercent = constrain(targetPercent, 0, 100); 
            targetPos = map(targetPercent, 0, 100, 0, 1023);
            previousPacket = currentPacket;
            state = MOVING;
          }
        }
        else Serial.println("PACKET REJECTED: System Overload, New Gate exceeds CFM requirements");
      }
      break;
      
    case MOVING:
      error = targetPos - currentPos;
      speed = map(abs(error), 0, 100, minSpeed, 255);
      speed = constrain(speed, minSpeed, 255);
      if (error > deadband)
      {
        motorExtend(speed);
      }
      else if (error < -deadband)
      {
        motorRetract(speed);
      }
      else
      {
        motorStop();
        state = IDLE;
      }
      break;

    case ESTOP:
      if (currentPos > 0)
      {
        motorRetract(255);
      }
      else 
      {
        motorStop();
        estopPressed = false;
      }
      break;
    default: state = IDLE;
  }
  prevState = state;
  currentPos = analogRead(A0);
  currentPercent = map(currentPos, 0, 1023, 0, 100);
}
}