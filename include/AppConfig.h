#pragma once

#include <Arduino.h>

// Board pin numbers use the mapping in variants/nicenano/variant.h.
constexpr uint8_t IR_SENSOR_PIN = PIN_022; // Arduino pin 20, nRF P0.20
constexpr uint8_t STATUS_LED_PIN = PIN_015; // onboard red LED, active low
constexpr uint32_t SERIAL_BAUD_RATE = 115200;

// High-speed falling-edge input. The larger value is the minimum accepted
// interval; increase it if the sensor output is noisy or chatters.
constexpr uint32_t IR_DEBOUNCE_MS = 1;
constexpr uint32_t IR_REARM_MS = 5;
constexpr uint32_t BATTERY_SAMPLE_PERIOD_MS = 60000;
constexpr uint32_t PROXIMITY_SAMPLE_PERIOD_MS = 1500;
constexpr uint32_t LOW_BATTERY_REPEAT_MS = 15UL * 60UL * 1000UL;

constexpr int8_t DEFAULT_NEAR_RSSI_DBM = -65;
constexpr int8_t RSSI_HYSTERESIS_DB = 5;
constexpr uint8_t BATTERY_WARNING_PERCENT = 20;

constexpr uint16_t SERVICE_UUID = 0xFFF0;
constexpr uint16_t COUNT_UUID = 0xFFF1;
constexpr uint16_t BATTERY_UUID = 0xFFF2;
constexpr uint16_t EVENT_UUID = 0xFFF3;
constexpr uint16_t SETTINGS_UUID = 0xFFF4;
constexpr uint16_t SYNC_UUID = 0xFFF5;
constexpr uint16_t RESET_UUID = 0xFFF6;
constexpr uint16_t TIME_SYNC_UUID = 0xFFF7;
constexpr uint16_t LAST_COUNT_TIME_UUID = 0xFFF8;

constexpr uint8_t SETTINGS_NOTIFY_PROXIMITY = 0x01;
