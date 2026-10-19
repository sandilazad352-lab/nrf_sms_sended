#pragma once

#include <Arduino.h>

class BatteryMonitor {
 public:
  // Return 0-100, or 0xFF if the ADC reports no usable reading.
  static uint8_t readPercent();
};
