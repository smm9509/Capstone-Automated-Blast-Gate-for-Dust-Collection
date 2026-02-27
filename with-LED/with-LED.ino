// C++ code
//
void setup()
{
  Serial.begin(115200); //baud rate of ESP32
  pinMode(2, OUTPUT); //LED Backward
  pinMode(3, OUTPUT); //LED Forward
  pinMode(4, OUTPUT); //Led Idle
  pinMode(5, OUTPUT); //negative motor term
  pinMode(6, OUTPUT); //positive motor term
  pinMode(9, OUTPUT); //pwm pin
  pinMode(A0, INPUT); //potentiometer reading pin
}

void loop()
{
  int potValue = analogRead(A0); //potentiometer reading
  int speed = map(potValue, 0, 1023, -255, 255); 
  
  //maps analog to digital range 0-1023 to -255 to 255
  Serial.println(speed);//debug
  if (speed > 0)
  {
    digitalWrite(4, LOW); //Idle LED deactivates
    digitalWrite(5, LOW); //extend actuator motor forward
    digitalWrite(2, LOW); //Backward LED deactivates
    digitalWrite(6, HIGH);
    digitalWrite(3, HIGH); //Forward LED activates
  }
  else if (speed < 0)
  {
    digitalWrite(4, LOW);  //Idle LED deactivates
    digitalWrite(5, HIGH); //retract actuator motorbackward
    digitalWrite(3, LOW);  //Forward LED deactivates
    digitalWrite(6, LOW);
    digitalWrite(2, HIGH); //Backward LED activates
  }
  else if(speed == 0)
  {
    digitalWrite(2, LOW); //Backward LED deactivates
    digitalWrite(3, LOW); //Forward LED deactivates
    digitalWrite(4, HIGH); //Idle LED activates

  }
  analogWrite(9, abs(speed));
  //absolute so that analog always writes positive
  
}
