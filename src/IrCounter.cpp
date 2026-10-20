#include "IrCounter.h"

IrCounter* IrCounter::instance_ = nullptr;

void IrCounter::begin(uint8_t pin, uint32_t debounceMs, uint32_t rearmMs) {
  instance_ = this;
  pin_ = pin;
  const uint32_t minimumIntervalMs = debounceMs > rearmMs ? debounceMs : rearmMs;
  minimumIntervalUs_ = minimumIntervalMs * 1000UL;
  pinMode(pin_, INPUT_PULLUP);
  lastCountUs_ = micros() - minimumIntervalUs_;
  pendingCounts_ = 0;
  pendingOverflow_ = false;
  attachInterrupt(digitalPinToInterrupt(pin_), onEdge, FALLING);
}

void IrCounter::onEdge() {
  IrCounter* counter = instance_;
  if (!counter) return;

  const uint32_t nowUs = micros();
  if (static_cast<uint32_t>(nowUs - counter->lastCountUs_) < counter->minimumIntervalUs_) return;
  counter->lastCountUs_ = nowUs;
  if (counter->pendingCounts_ != UINT16_MAX) {
    ++counter->pendingCounts_;
  } else {
    counter->pendingOverflow_ = true;
  }
}

uint16_t IrCounter::update() {
  noInterrupts();
  const uint16_t count = pendingCounts_;
  pendingCounts_ = 0;
  const bool overflow = pendingOverflow_;
  pendingOverflow_ = false;
  interrupts();

  if (overflow) Serial.println(F("WARNING: IR pulse queue overflow; passages may have been lost"));
  return count;
}
