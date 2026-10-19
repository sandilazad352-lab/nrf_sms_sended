#pragma once

#include <Arduino.h>

struct CounterState {
  uint32_t sequence;
  uint32_t count;
  uint32_t acknowledgedCount;
  int8_t nearRssiDbm;
  uint8_t settingsFlags;
};

class CounterStorage {
 public:
  bool begin();
  bool load(CounterState& state);
  bool save(CounterState& state);
};
