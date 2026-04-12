// Author: Claude Sonnet
// ESP32 serial passthrough
// Bridges lab bench computer (USB/UART0) to Arduino Nano (UART1 via VGA cable)
//
// Wiring — ACS 15-pin connector:
//   GPIO3 (TX) -> Pin 12 (BUSIO3) -> VGA cable -> Nano RX0 (Pin 12 on connector, D0 on Nano)
//   GPIO4 (RX) -> Pin 15 (BUSIO4) -> VGA cable -> Nano TX1 (Pin 15 on connector, D1 on Nano)
//   GND        -> Pin 3, 10, 11, 14, Shield
//
// Voltage protection on GPIO4 (RX):
//   Nano TX1 is 5V logic. GPIO4 is 3.3V tolerant only.
//   Place 1x LED in series between Nano TX1 and GPIO4 (drops ~1.8V, clamps HIGH to ~3.2V).
//   Place 100kohm resistor from GPIO4 to GND (resolves float when LED is reverse biased).
//
// Baud rates:
//   Serial  (UART0, USB to lab bench): set to match your Python serial monitor
//   Serial1 (UART1, to Nano):          must match Serial.begin() in blastGate.ino

#define BAUD_BENCH  115200  // lab bench USB baud rate
#define BAUD_NANO   4800  // Nano baud rate — change both if you change blastGate.ino

#define PIN_RX1 4  // GPIO4 — receives from Nano TX1 (via LED voltage divider)
#define PIN_TX1 3  // GPIO3 — transmits to Nano RX0
#define PIN_ACCESS 46 // goes through a level shifter

const char* help =
  "Hello! Welcome to the mock ACS ESP32. We're keeping it kinda simple. \r\n"
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
  Serial.write(help);
  Serial1.begin(BAUD_NANO, SERIAL_8N1, PIN_RX1, PIN_TX1);
}

void loop()
{
    if (Serial.available())  {
        char readed = Serial.read();
        switch (readed) {
            case 'O':
                digitalWrite(PIN_ACCESS, HIGH);
                Serial.write("ACCESS is now HIGH.\r\n");
                break;
            case 'C':
                digitalWrite(PIN_ACCESS, LOW);
                Serial.write("ACCESS is now LOW.\r\n");
                break;
            case 'P': {
                bool original = digitalRead(PIN_ACCESS);
                digitalWrite(PIN_ACCESS, !original);
                Serial.write("ACCESS pulsed. Restoring in 5s and hanging...\r\n");
                delay(5000);
                digitalWrite(PIN_ACCESS, original);
                Serial.write("ACCESS restored.\r\n");
                break;
            }
            case 'H':
                Serial.write(help);
                break;
            default:
                Serial1.write(readed);
        }
    }   // lab bench -> Nano
    if (Serial1.available()) Serial.write(Serial1.read());   // Nano -> lab bench
}
