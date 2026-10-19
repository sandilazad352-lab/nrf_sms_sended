#include <Arduino.h>
#include <nrf_soc.h>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "BleCounterService.h"
#include "CounterStorage.h"
#include "IrCounter.h"

namespace {
CounterState counterState = {};
CounterStorage counterStorage;
IrCounter irCounter;
BleCounterService bleService;
bool storageReady = false;
uint32_t lastBatterySampleMs = 0;
uint32_t lastPersistenceRetryMs = 0;
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(100);
  Serial.println();
  Serial.println(F("IR Book Counter booting"));
  Serial.print(F("IR sensor: Arduino pin 20 / P0.20; serial: "));
  Serial.println(SERIAL_BAUD_RATE);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  irCounter.begin(IR_SENSOR_PIN, IR_DEBOUNCE_MS, IR_REARM_MS);
  storageReady = counterStorage.begin();
  Serial.print(F("Flash storage: "));
  Serial.println(storageReady ? F("ready") : F("FAILED - count will not survive reset"));
  if (storageReady) {
    if (!counterStorage.load(counterState)) {
      Serial.println(F("Warning: could not load counter state; using defaults"));
    }
  } else {
    counterState.count = 0;
    counterState.acknowledgedCount = 0;
    counterState.nearRssiDbm = DEFAULT_NEAR_RSSI_DBM;
    counterState.settingsFlags = SETTINGS_NOTIFY_PROXIMITY;
  }
  Serial.print(F("Restored count: "));
  Serial.println(counterState.count);

  bleService.begin(counterState);
  Serial.println(F("BLE advertising as 'Book Counter' (service FFF0)"));
  lastBatterySampleMs = millis() - BATTERY_SAMPLE_PERIOD_MS;
}

void loop() {
  const uint32_t now = millis();

  if (irCounter.update()) {
    if (counterState.count != UINT32_MAX) ++counterState.count;
    // Commit before advertising or notifying the new total.
    if (storageReady) {
      if (!counterStorage.save(counterState)) {
        Serial.println(F("ERROR: failed to persist count"));
      }
    }
    Serial.print(F("IR passage counted. Total: "));
    Serial.println(counterState.count);
    bleService.onCountChanged();
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(30);
    digitalWrite(STATUS_LED_PIN, HIGH);
  }

  bleService.loop(now);

  if (now - lastBatterySampleMs >= BATTERY_SAMPLE_PERIOD_MS) {
    lastBatterySampleMs = now;
    const uint8_t batteryPercent = BatteryMonitor::readPercent();
    Serial.print(F("Battery estimate: "));
    if (batteryPercent == 0xFF) {
      Serial.println(F("unavailable (check board battery-sense wiring)"));
    } else {
      Serial.print(batteryPercent);
      Serial.println(F("%"));
    }
    bleService.updateBattery(batteryPercent, now);
  }

  if (storageReady && bleService.persistencePending() && now - lastPersistenceRetryMs >= 1000) {
    lastPersistenceRetryMs = now;
    if (counterStorage.save(counterState)) {
      bleService.clearPersistencePending();
      Serial.println(F("BLE settings/sync state saved"));
    } else {
      Serial.println(F("ERROR: failed to persist BLE settings/sync state"));
    }
  }

  // Keep BLE advertising active while sleeping the CPU between events.
  sd_app_evt_wait();
}
