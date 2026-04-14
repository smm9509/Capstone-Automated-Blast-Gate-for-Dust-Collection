#define BAUD_BENCH  115200
#define PIN_ACCESS  5 // red wire to pin 1
#define PIN_RX 16 // red or orange wire to pin 15, with an LED and a resistor connected to step down the voltage.
#define PIN_TX 17 // green wire to pin 12

const char* help =
  "Mock ACS ESP32 v6 -- bench: 115200, nano: 4800\r\n"
  "Commands:\r\n"
  "  O   -- assert ACCESS HIGH (card present)\r\n"
  "  C   -- deassert ACCESS LOW (card removed)\r\n"
  "  P   -- pulse ACCESS for 5s to the opposite state\r\n"
  "  H   -- print this help\r\n"
  "  ?   -- query Nano wiper ADC position (forwarded)\r\n"
  "  0-100 -- set Nano open setpoint percent (forwarded)\r\n";

#define SerialGate Serial1
#define SerialPi Serial

void setup()
{
  pinMode(PIN_ACCESS, OUTPUT);
  digitalWrite(PIN_ACCESS, LOW);
  SerialPi.begin(BAUD_BENCH);
  SerialGate.begin(4800, SERIAL_8N1, PIN_RX, PIN_TX);
  SerialPi.write(help);
}

void loop()
{
  if (SerialPi.available()) {
    char c = SerialPi.read();
    switch (c) {
      case 'O':
        digitalWrite(PIN_ACCESS, HIGH);
        SerialPi.write("ACCESS is now HIGH.\r\n");
        break;
      case 'C':
        digitalWrite(PIN_ACCESS, LOW);
        SerialPi.write("ACCESS is now LOW.\r\n");
        break;
      case 'P': {
        bool orig = digitalRead(PIN_ACCESS);
        digitalWrite(PIN_ACCESS, !orig);
        SerialPi.write("ACCESS pulsed. Restoring in 5s...\r\n");
        delay(5000);
        digitalWrite(PIN_ACCESS, orig);
        SerialPi.write("ACCESS restored.\r\n");
        break;
      }
      case 'H':
        SerialPi.write(help);
        break;
      default:
        SerialGate.write(c);
    }
  }
  if (SerialGate.available()) SerialPi.write(SerialGate.read());
}
