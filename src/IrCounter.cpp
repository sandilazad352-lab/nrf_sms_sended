#include "IrCounter.h"

IrCounter* IrCounter::instance_ = nullptr;

void IrCounter::begin(uint8_t pin, uint32_t debounceMs, uint32_t rearmMs) {
  instance_ = this;
  pin_ = pin;
  debounceMs_ = debounceMs;
  rearmMs_ = rearmMs;
  pinMode(pin_, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_), onEdge, CHANGE);
  lastCountMs_ = millis() - rearmMs_;
}

void IrCounter::onEdge() {
  if (instance_) instance_->edgePending_ = true;
}

bool IrCounter::update() {
  if (!edgePending_) return false;
  noInterrupts();
  edgePending_ = false;
  interrupts();

  delay(debounceMs_);
  const bool blocked = digitalRead(pin_) == LOW;
  const uint32_t now = millis();
  if (blocked && !beamBlocked_ && now - lastCountMs_ >= rearmMs_) {
    beamBlocked_ = true;
    lastCountMs_ = now;
    return true;
  }
  if (!blocked) beamBlocked_ = false;
  return false;
}
