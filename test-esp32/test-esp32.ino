#define BAUD_BENCH  115200
#define PIN_ACCESS  5
#define PIN_RX 16
#define PIN_TX 17

const char* help =
  "Mock ACS ESP32 v6 -- bench: 115200, nano: 4800\r\n"
  "Commands:\r\n"
  "  O   -- assert ACCESS HIGH (card present)\r\n"
  "  C   -- deassert ACCESS LOW (card removed)\r\n"
  "  P   -- pulse ACCESS for 5s to the opposite state\r\n"
  "  H   -- print this help\r\n"
  "  ?   -- query Nano wiper ADC position (forwarded)\r\n"
  "  0-100 -- set Nano open setpoint percent (forwarded)\r\n";

void setup()
{
  pinMode(PIN_ACCESS, OUTPUT);
  digitalWrite(PIN_ACCESS, LOW);
  Serial.begin(BAUD_BENCH);
  Serial1.begin(4800, SERIAL_8N1, PIN_RX, PIN_TX);
  Serial.write(help);
}

void loop()
{
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'O':
        digitalWrite(PIN_ACCESS, HIGH);
        Serial.write("ACCESS is now HIGH.\r\n");
        break;
      case 'C':
        digitalWrite(PIN_ACCESS, LOW);
        Serial.write("ACCESS is now LOW.\r\n");
        break;
      case 'P': {
        bool orig = digitalRead(PIN_ACCESS);
        digitalWrite(PIN_ACCESS, !orig);
        Serial.write("ACCESS pulsed. Restoring in 5s...\r\n");
        delay(5000);
        digitalWrite(PIN_ACCESS, orig);
        Serial.write("ACCESS restored.\r\n");
        break;
      }
      case 'H':
        Serial.write(help);
        break;
      default:
        Serial1.write(c);
    }
  }
  if (Serial1.available()) Serial.write(Serial1.read());
}
