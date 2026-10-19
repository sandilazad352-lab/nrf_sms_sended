#include <Arduino.h>

// The onboard red LED is connected to P0.15 on this board.
constexpr uint8_t FRIENDLY_LED = PIN_015;

void setup() {
  pinMode(FRIENDLY_LED, OUTPUT);
  digitalWrite(FRIENDLY_LED, HIGH);
}

void loop() {
  // Three quick hello blinks, followed by a calm pause.
  for (uint8_t blink = 0; blink < 3; ++blink) {
    digitalWrite(FRIENDLY_LED, LOW);
    delay(180);
    digitalWrite(FRIENDLY_LED, HIGH);
    delay(180);
  }
  delay(1200);
}
