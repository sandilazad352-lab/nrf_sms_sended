#pragma once

#include <Arduino.h>

class IrCounter {
 public:
  void begin(uint8_t pin, uint32_t debounceMs, uint32_t rearmMs);
  bool update(); // true once for each debounced active-low beam break

 private:
  static void onEdge();
  static IrCounter* instance_;
  volatile bool edgePending_ = false;
  uint8_t pin_ = 0;
  uint32_t debounceMs_ = 0;
  uint32_t rearmMs_ = 0;
  uint32_t lastCountMs_ = 0;
  bool beamBlocked_ = false;
};
