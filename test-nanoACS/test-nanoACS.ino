//Author: Claude Code (Sonnet)

#include <SoftwareSerial.h>

#define BAUD_BENCH  115200
#define VERSION "nanoACS v11 2026-04-15\r\n"
#define PIN_ACCESS  4 // red wire to pin 1 of vga adapter
#define PIN_RX      5 // "orange" (red) wire from gate Nano TX, pin 15 of VGA adapter
#define PIN_TX      6 // green wire to gate Nano RX, pin 12 of VGA adapter

const char* help =
  "Commands:\r\n"
  "  V        -- print version string\r\n"
  "  O        -- assert ACCESS HIGH (tool present)\r\n"
  "  C        -- deassert ACCESS LOW (tool removed)\r\n"
  "  P        -- pulse ACCESS for 5s to opposite state\r\n"
  "  H        -- print this help\r\n"
  "  E        -- toggle hex echo (shows raw bytes received)\r\n"
  "Gate Nano (forwarded, framed :CMD\\n):\r\n"
  "  :?\\n     -- query wiper ADC position\r\n"
  "  :S<n>\\n  -- set open setpoint percent (0-100)\r\n";

SoftwareSerial SerialGate(PIN_RX, PIN_TX);
#define SerialPi Serial
bool echo_hex = false;

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
      case 'V':
        SerialPi.write(VERSION);
        break;
      case 'H':
        SerialPi.write(help);
        break;
      case 'E':
        echo_hex = !echo_hex;
        SerialPi.write(echo_hex ? "Hex echo ON\r\n" : "Hex echo OFF\r\n");
        break;
      default:
        if (echo_hex) {
          char tmp[6];
          sprintf(tmp, "[%02X]", (uint8_t)c);
          SerialPi.write(tmp);
        }
        SerialGate.write(c);
    }
  }
  if (SerialGate.available()) SerialPi.write(SerialGate.read());
}
