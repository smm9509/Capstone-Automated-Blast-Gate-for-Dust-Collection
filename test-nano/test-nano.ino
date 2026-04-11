
// pin definitions - a representation of the hardware as it is wired today.
// 15-pin connector
    static const uint8_t RX = 0; // pin RX0
    static const uint8_t TX = 1; // pin TX1
    static const uint8_t ACS_ACCESS = 2; //pin D2
    //static const uint8_t BUSIO1 = ; //Tom asked to express the gate status on a binary signal, but we haven't sorted that out yet.
// devices on board - HARDWARE NOT INSTALLED AS OF 2026-04-11
    static const uint8_t E_STOP = 3;
    static const uint8_t LED_RED = 10;
    static const uint8_t LED_YLW = 11;
    static const uint8_t LED_GRN = 12;
    // TODO: add hardware for powered-on manual override with 3-position ON-OFF-ON switch
    static const uint8_t OVERRIDE_OPEN = 4;
    static const uint8_t OVERRIDE_CLOSE = 7;
// connections to L298N subassembly
    static const uint8_t ENA = 5; //warning: older code assumes ENA was wired to pin 9 instead of 5
    static const uint8_t IN1 = 6;
    static const uint8_t IN2 = 9; //warning: older code assumes IN2 was wired to pin 5 instead of 9
// connections to Linear Actuator Servo
    static const uint8_t WIPER = A0;

void setup() {
}
void loop() {
}
