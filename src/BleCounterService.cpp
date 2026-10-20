#include "BleCounterService.h"

#include "AppConfig.h"

#include <string.h>

namespace {
struct __attribute__((packed)) EventPacket {
  uint8_t eventCode;
  uint8_t batteryPercent;
  uint16_t eventSequence;
  uint32_t count;
};

BLEService counterService(SERVICE_UUID);
BLECharacteristic countCharacteristic(COUNT_UUID, CHR_PROPS_READ | CHR_PROPS_NOTIFY, 4, true);
BLECharacteristic batteryCharacteristic(BATTERY_UUID, CHR_PROPS_READ | CHR_PROPS_NOTIFY, 1, true);
BLECharacteristic eventCharacteristic(EVENT_UUID, CHR_PROPS_NOTIFY, sizeof(EventPacket), true);
BLECharacteristic settingsCharacteristic(SETTINGS_UUID, CHR_PROPS_READ | CHR_PROPS_WRITE, 2, true);
BLECharacteristic syncCharacteristic(SYNC_UUID, CHR_PROPS_READ | CHR_PROPS_WRITE, 4, true);
BLECharacteristic resetCharacteristic(RESET_UUID, CHR_PROPS_WRITE, 4, true);
BLECharacteristic timeSyncCharacteristic(TIME_SYNC_UUID, CHR_PROPS_WRITE, 4, true);
BLECharacteristic lastCountTimeCharacteristic(LAST_COUNT_TIME_UUID,
                                              CHR_PROPS_READ | CHR_PROPS_NOTIFY, 4, true);
} // namespace

BleCounterService* BleCounterService::instance_ = nullptr;

void BleCounterService::begin(CounterState& state) {
  state_ = &state;
  instance_ = this;

  Bluefruit.begin(1, 0);
  Bluefruit.setName("Post Box");
  Bluefruit.setTxPower(-8);
  Bluefruit.autoConnLed(false);
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false);
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  counterService.begin();

  countCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  countCharacteristic.begin();
  countCharacteristic.write(&state_->count, sizeof(state_->count));

  batteryCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  batteryCharacteristic.begin();
  batteryCharacteristic.write(&batteryPercent_, sizeof(batteryPercent_));

  eventCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  eventCharacteristic.begin();

  settingsCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  settingsCharacteristic.setWriteCallback(onSettingsWrite);
  settingsCharacteristic.begin();
  const uint8_t settings[2] = {static_cast<uint8_t>(state_->nearRssiDbm), state_->settingsFlags};
  settingsCharacteristic.write(settings, sizeof(settings));

  syncCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  syncCharacteristic.setWriteCallback(onSyncWrite);
  syncCharacteristic.begin();
  syncCharacteristic.write(&state_->acknowledgedCount, sizeof(state_->acknowledgedCount));

  resetCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  resetCharacteristic.setWriteCallback(onResetWrite);
  resetCharacteristic.begin();

  timeSyncCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  timeSyncCharacteristic.setWriteCallback(onTimeSyncWrite);
  timeSyncCharacteristic.begin();

  lastCountTimeCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  lastCountTimeCharacteristic.begin();
  lastCountTimeCharacteristic.write(&lastCountUtcSeconds_, sizeof(lastCountUtcSeconds_));

  refreshAdvertising();
}

void BleCounterService::refreshAdvertising() {
  if (Bluefruit.Advertising.isRunning()) Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(counterService);

  // Exact counts remain in GATT; advertisements expose only an event marker.
  const uint8_t manufacturerData[] = {
    0xFF, 0xFF, // Bluetooth SIG test company ID; replace before production
    0x42, 0x43, // project marker "BC"
    0x01,       // protocol version
    static_cast<uint8_t>(advertisingEventActive_ ? 1 : 0),
    static_cast<uint8_t>(eventSequence_ & 0xFF),
    static_cast<uint8_t>(eventSequence_ >> 8)
  };
  Bluefruit.Advertising.addManufacturerData(manufacturerData, sizeof(manufacturerData));
  Bluefruit.ScanResponse.clearData();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.setInterval(160, 1600); // 100 ms fast; 1 s slow
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void BleCounterService::sendEvent(uint8_t code) {
  const EventPacket packet = {code, batteryPercent_, eventSequence_, state_->count};
  if (Bluefruit.connected() && eventCharacteristic.notifyEnabled()) {
    eventCharacteristic.notify(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  }
}

void BleCounterService::onCountChanged(uint16_t countDelta, uint32_t nowMs) {
  if (clockSynchronized_) {
    lastCountUtcSeconds_ = syncedUtcSeconds_ + (nowMs - syncAnchorMs_) / 1000UL;
  } else {
    lastCountUtcSeconds_ = 0; // Zero means the device clock has not been synced since boot.
  }
  lastCountTimeCharacteristic.write(&lastCountUtcSeconds_, sizeof(lastCountUtcSeconds_));
  if (Bluefruit.connected() && lastCountTimeCharacteristic.notifyEnabled()) {
    lastCountTimeCharacteristic.notify(&lastCountUtcSeconds_, sizeof(lastCountUtcSeconds_));
  }
  eventSequence_ = static_cast<uint16_t>(eventSequence_ + countDelta);
  advertisingEventActive_ = true;
  countCharacteristic.write(&state_->count, sizeof(state_->count));
  if (Bluefruit.connected() && countCharacteristic.notifyEnabled()) {
    countCharacteristic.notify(&state_->count, sizeof(state_->count));
  }
  sendEvent(1);

  if (Bluefruit.connected()) advertisingNeedsRefresh_ = true;
  else refreshAdvertising();
}

void BleCounterService::onTimeSyncWrite(uint16_t, BLECharacteristic*, uint8_t* data,
                                        uint16_t length) {
  if (!instance_ || length != sizeof(uint32_t)) return;
  uint32_t utcSeconds = 0;
  memcpy(&utcSeconds, data, sizeof(utcSeconds));
  // Unix time zero is the protocol's unknown/invalid sentinel.
  if (utcSeconds == 0) return;
  instance_->syncedUtcSeconds_ = utcSeconds;
  instance_->syncAnchorMs_ = millis();
  instance_->clockSynchronized_ = true;
  Serial.print(F("UTC clock synchronized: "));
  Serial.println(utcSeconds);
}

void BleCounterService::updateBattery(uint8_t percent, uint32_t now) {
  batteryPercent_ = percent;
  batteryCharacteristic.write(&batteryPercent_, sizeof(batteryPercent_));
  if (Bluefruit.connected() && batteryCharacteristic.notifyEnabled()) {
    batteryCharacteristic.notify(&batteryPercent_, sizeof(batteryPercent_));
  }
  if (batteryPercent_ != 0xFF && batteryPercent_ <= BATTERY_WARNING_PERCENT &&
      now - lastBatteryWarningMs_ >= LOW_BATTERY_REPEAT_MS) {
    lastBatteryWarningMs_ = now;
    sendEvent(4);
  }
}

void BleCounterService::onConnect(uint16_t connectionHandle) {
  Serial.println(F("BLE phone connected"));
  BLEConnection* connection = Bluefruit.Connection(connectionHandle);
  if (connection) connection->monitorRssi(BLE_GAP_RSSI_THRESHOLD_INVALID);
}

void BleCounterService::onDisconnect(uint16_t, uint8_t) {
  if (!instance_) return;
  Serial.println(F("BLE phone disconnected"));
  instance_->isNear_ = false;
  instance_->advertisingNeedsRefresh_ = true;
}

void BleCounterService::onSettingsWrite(uint16_t, BLECharacteristic*, uint8_t* data, uint16_t length) {
  if (!instance_ || !instance_->state_ || length != 2) return;
  const int8_t threshold = static_cast<int8_t>(data[0]);
  if (threshold < -90 || threshold > -35) return;
  instance_->state_->nearRssiDbm = threshold;
  instance_->state_->settingsFlags = data[1] & SETTINGS_NOTIFY_PROXIMITY;
  instance_->persistencePending_ = true;
  settingsCharacteristic.write(data, length);
  Serial.print(F("Proximity boundary set to "));
  Serial.print(static_cast<int>(threshold));
  Serial.print(F(" dBm; alerts "));
  Serial.println((instance_->state_->settingsFlags & SETTINGS_NOTIFY_PROXIMITY) ? F("enabled") : F("disabled"));
}

void BleCounterService::onSyncWrite(uint16_t, BLECharacteristic*, uint8_t* data, uint16_t length) {
  if (!instance_ || !instance_->state_ || length != sizeof(uint32_t)) return;
  uint32_t acknowledged = 0;
  memcpy(&acknowledged, data, sizeof(acknowledged));
  if (acknowledged > instance_->state_->count) acknowledged = instance_->state_->count;
  if (acknowledged > instance_->state_->acknowledgedCount) {
    instance_->state_->acknowledgedCount = acknowledged;
    instance_->persistencePending_ = true;
  }
  Serial.print(F("Sync acknowledgement: "));
  Serial.println(instance_->state_->acknowledgedCount);
  syncCharacteristic.write(&instance_->state_->acknowledgedCount,
                           sizeof(instance_->state_->acknowledgedCount));
}

void BleCounterService::onResetWrite(uint16_t, BLECharacteristic*, uint8_t* data, uint16_t length) {
  // Require an explicit ASCII token rather than accepting an accidental empty write.
  static const uint8_t resetToken[] = {'R', 'S', 'T', '1'};
  if (!instance_ || !instance_->state_ || length != sizeof(resetToken) ||
      memcmp(data, resetToken, sizeof(resetToken)) != 0) {
    Serial.println(F("Rejected invalid BLE reset command"));
    return;
  }

  instance_->state_->count = 0;
  instance_->state_->acknowledgedCount = 0;
  instance_->persistencePending_ = true;
  countCharacteristic.write(&instance_->state_->count,
                            sizeof(instance_->state_->count));
  syncCharacteristic.write(&instance_->state_->acknowledgedCount,
                           sizeof(instance_->state_->acknowledgedCount));
  ++instance_->eventSequence_;
  instance_->advertisingEventActive_ = false;
  if (Bluefruit.connected() && countCharacteristic.notifyEnabled()) {
    countCharacteristic.notify(&instance_->state_->count,
                               sizeof(instance_->state_->count));
  }
  instance_->sendEvent(5);
  instance_->advertisingNeedsRefresh_ = true;
  Serial.println(F("BLE reset accepted; count set to zero"));
}

void BleCounterService::updateProximity(uint32_t now) {
  if (now - lastProximitySampleMs_ < PROXIMITY_SAMPLE_PERIOD_MS) return;
  lastProximitySampleMs_ = now;
  if (!Bluefruit.connected()) return;

  BLEConnection* connection = Bluefruit.Connection(Bluefruit.connHandle());
  if (!connection) return;
  const int8_t rssi = connection->getRssi();
  const bool enabled = (state_->settingsFlags & SETTINGS_NOTIFY_PROXIMITY) != 0;
  if (!isNear_ && rssi >= state_->nearRssiDbm) {
    isNear_ = true;
    Serial.print(F("Phone entered near boundary (RSSI "));
    Serial.print(static_cast<int>(rssi));
    Serial.println(F(" dBm)"));
    if (enabled) sendEvent(2);
  } else if (isNear_ && rssi < state_->nearRssiDbm - RSSI_HYSTERESIS_DB) {
    isNear_ = false;
    Serial.print(F("Phone left near boundary (RSSI "));
    Serial.print(static_cast<int>(rssi));
    Serial.println(F(" dBm)"));
    if (enabled) sendEvent(3);
  }
}

void BleCounterService::loop(uint32_t now) {
  updateProximity(now);
  if (advertisingNeedsRefresh_ && !Bluefruit.connected()) {
    advertisingNeedsRefresh_ = false;
    refreshAdvertising();
  }
}
