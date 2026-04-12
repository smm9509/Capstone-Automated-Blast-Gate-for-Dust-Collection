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
#define BAUD_NANO   115200  // Nano baud rate — change both if you change blastGate.ino

#define PIN_RX1 4  // GPIO4 — receives from Nano TX1 (via LED voltage divider)
#define PIN_TX1 3  // GPIO3 — transmits to Nano RX0

void setup()
{
  Serial.begin(BAUD_BENCH);
  Serial1.begin(BAUD_NANO, SERIAL_8N1, PIN_RX1, PIN_TX1);
}

void loop()
{
  if (Serial.available())  Serial1.write(Serial.read());   // lab bench -> Nano
  if (Serial1.available()) Serial.write(Serial1.read());   // Nano -> lab bench
}
