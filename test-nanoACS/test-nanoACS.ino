//Author: Claude Code (Sonnet)

#include <SoftwareSerial.h>

#define BAUD_BENCH  115200
#define PIN_ACCESS  4 // red wire to pin 1 of vga adapter
#define PIN_RX      5 // "orange" (red) wire from gate Nano TX, pin 15 of VGA adapter
#define PIN_TX      6 // green wire to gate Nano RX, pin 12 of VGA adapter

const char* help =
  "Mock ACS Nano v8 -- bench: 115200, nano: 4800\r\n"
  "Commands:\r\n"
  "  O   -- assert ACCESS HIGH (card present)\r\n"
  "  C   -- deassert ACCESS LOW (card removed)\r\n"
  "  P   -- pulse ACCESS for 5s to the opposite state\r\n"
  "  H   -- print this help\r\n"
  "  ?   -- query Nano wiper ADC position (forwarded)\r\n"
  "  0-100 -- set Nano open setpoint percent (forwarded)\r\n";

SoftwareSerial SerialGate(PIN_RX, PIN_TX);
#define SerialPi Serial

void setup()
{
  pinMode(PIN_ACCESS, OUTPUT);
  digitalWrite(PIN_ACCESS, LOW);
  SerialPi.begin(BAUD_BENCH);
  SerialGate.begin(4800);
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
      case '\r': case '\n': // don't forward line endings -- gate's parseInt() treats bare \n as 0
        break;
      default:
        SerialPi.write("> ");
        SerialPi.write(c);
        SerialPi.write("\r\n");
        SerialGate.write(c);
    }
  }
  if (SerialGate.available()) SerialPi.write(SerialGate.read());
}
