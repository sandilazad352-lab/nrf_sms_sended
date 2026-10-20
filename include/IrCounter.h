#pragma once

#include <Arduino.h>

class IrCounter {
 public:
  void begin(uint8_t pin, uint32_t debounceMs, uint32_t rearmMs);
  uint16_t update(); // drain all falling-edge counts queued since the last call

 private:
  static void onEdge();
  static IrCounter* instance_;
  volatile uint16_t pendingCounts_ = 0;
  volatile bool pendingOverflow_ = false;
  uint8_t pin_ = 0;
  uint32_t minimumIntervalUs_ = 0;
  volatile uint32_t lastCountUs_ = 0;
};
