#include "BatteryMonitor.h"

#include <nrf.h>

namespace {
uint16_t readVddhMillivolts() {
  volatile int16_t raw = 0;
  NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;
  NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;
  NRF_SAADC->CH[0].CONFIG =
      (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
      (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) |
      (SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos);
  NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDDHDIV5;
  NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;
  NRF_SAADC->RESULT.PTR = reinterpret_cast<uint32_t>(&raw);
  NRF_SAADC->RESULT.MAXCNT = 1;
  NRF_SAADC->TASKS_START = 1;
  while (!NRF_SAADC->EVENTS_STARTED) {}
  NRF_SAADC->EVENTS_STARTED = 0;
  NRF_SAADC->TASKS_SAMPLE = 1;
  while (!NRF_SAADC->EVENTS_END) {}
  NRF_SAADC->EVENTS_END = 0;
  NRF_SAADC->TASKS_STOP = 1;
  while (!NRF_SAADC->EVENTS_STOPPED) {}
  NRF_SAADC->EVENTS_STOPPED = 0;
  NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Disabled;

  if (raw <= 0) return 0;
  return static_cast<uint16_t>((static_cast<uint32_t>(raw) * 12000UL + 2047UL) / 4095UL);
}
} // namespace

uint8_t BatteryMonitor::readPercent() {
  // Approximate Li-ion percentage from voltage; calibrate on the exact board
  // and cell. This is not a fuel-gauge measurement.
  constexpr uint16_t EMPTY_MV = 3300;
  constexpr uint16_t FULL_MV = 4200;
  const uint16_t millivolts = readVddhMillivolts();
  if (!millivolts) return 0xFF;
  if (millivolts < EMPTY_MV) return 0;
  if (millivolts >= FULL_MV) return 100;
  return static_cast<uint8_t>((static_cast<uint32_t>(millivolts - EMPTY_MV) * 100UL) /
                              (FULL_MV - EMPTY_MV));
}
