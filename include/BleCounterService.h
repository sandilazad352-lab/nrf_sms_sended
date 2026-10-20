#pragma once

#include <Arduino.h>
#include <bluefruit.h>

#include "CounterStorage.h"

class BleCounterService {
 public:
  void begin(CounterState& state);
  void loop(uint32_t now);
  void onCountChanged(uint16_t countDelta = 1, uint32_t nowMs = 0);
  void updateBattery(uint8_t percent, uint32_t now);

  bool persistencePending() const { return persistencePending_; }
  void clearPersistencePending() { persistencePending_ = false; }

 private:
  static BleCounterService* instance_;
  static void onConnect(uint16_t connectionHandle);
  static void onDisconnect(uint16_t connectionHandle, uint8_t reason);
  static void onSettingsWrite(uint16_t connectionHandle, BLECharacteristic* characteristic,
                              uint8_t* data, uint16_t length);
  static void onSyncWrite(uint16_t connectionHandle, BLECharacteristic* characteristic,
                          uint8_t* data, uint16_t length);
  static void onResetWrite(uint16_t connectionHandle, BLECharacteristic* characteristic,
                           uint8_t* data, uint16_t length);
  static void onTimeSyncWrite(uint16_t connectionHandle, BLECharacteristic* characteristic,
                              uint8_t* data, uint16_t length);

  void refreshAdvertising();
  void sendEvent(uint8_t code);
  void updateProximity(uint32_t now);

  CounterState* state_ = nullptr;
  bool isNear_ = false;
  bool advertisingEventActive_ = false;
  bool advertisingNeedsRefresh_ = false;
  bool persistencePending_ = false;
  uint16_t eventSequence_ = 0;
  uint8_t batteryPercent_ = 0xFF;
  uint32_t lastProximitySampleMs_ = 0;
  uint32_t lastBatteryWarningMs_ = 0;
  uint32_t syncedUtcSeconds_ = 0;
  uint32_t syncAnchorMs_ = 0;
  uint32_t lastCountUtcSeconds_ = 0;
  bool clockSynchronized_ = false;
};
