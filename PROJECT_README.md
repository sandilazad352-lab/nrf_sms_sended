# Post Box IR counter firmware

This project targets the NRF52840 Supermini / Nice!Nano-style PlatformIO board support in this repository. The firmware counts active-low IR beam interruptions, persists the total and sync acknowledgement in LittleFS, and exposes live data over a custom BLE GATT service.

## Hardware defaults

- IR receiver output: Arduino pin `20` (`PIN_020`, nRF `P0.20`), active low. Change `IR_SENSOR_PIN` in `include/AppConfig.h` to match your board wiring.
- High-speed counter defaults: falling-edge interrupt, 1 ms debounce/minimum filter and 5 ms minimum interval (`include/AppConfig.h`). The interrupt queues multiple pulses; the main loop batches queued counts into one flash write and one BLE update. This supports up to 200 accepted edges per second in firmware, but actual reliable speed depends on the IR sensor response, target spacing, beam alignment, and signal quality. Increase `IR_REARM_MS` if noise causes false counts or a single passage produces repeated edges.
- The sensor input uses the MCU pull-up. Use a receiver with an open-collector/open-drain output or a safe 3.3 V logic output. Do not feed 5 V into a GPIO.
- The onboard red LED on `P0.15` gives a short flash after a count.
- Battery percentage is estimated from the nRF52840 VDDH/5 ADC input, following the repository's battery example. Confirm that your exact Supermini clone routes battery voltage there before trusting this value. The percentage is a voltage estimate, not a fuel gauge.

A single beam detects passages but cannot determine direction or occupancy. To distinguish entry from exit or reject multiple crossings, add a second IR beam and extend the state machine.

## BLE protocol

The device advertises as **Post Box**. A companion phone app can connect, subscribe to notifications, read the count and battery level, set proximity preferences, and acknowledge synced count data.

| Characteristic | UUID | Format |
| --- | --- | --- |
| Count | `FFF1` | Read/notify, unsigned 32-bit little-endian total |
| Battery | `FFF2` | Read/notify, percent 0–100; `FF` means unavailable |
| Event | `FFF3` | Notify, 8 bytes: event code, battery percent, event sequence (`uint16`), count (`uint32`), little-endian |
| Settings | `FFF4` | Encrypted read/write, 2 bytes: signed RSSI threshold in dBm, flags (`bit 0` enables proximity events) |
| Sync acknowledgement | `FFF5` | Encrypted read/write, unsigned 32-bit little-endian count acknowledged by the phone |
| Set device time | `FFF7` | Encrypted write, unsigned 32-bit Unix UTC seconds, little-endian; zero invalid |
| Last count time | `FFF8` | Read/notify, unsigned 32-bit Unix UTC seconds, little-endian; zero means unknown |

Event codes are `1` for a counted passage, `2` for entering the configured RSSI boundary, `3` for leaving it, and `4` for low battery. RSSI is only a rough distance indicator and varies with body position, walls, and antenna orientation. The configured boundary is shared device state; a mobile app should keep each user's personal boundary in their own profile and apply it when that phone connects.

The advertisement contains an event marker and sequence, not the visitor count. Exact totals are available through GATT. The example company identifier is the Bluetooth SIG test value `0xFFFF`; replace it with your assigned company identifier before production.

## Phone notifications and sync

The firmware sends BLE notifications to a connected app and makes event metadata available in advertisements. It cannot display an Android/iOS push notification by itself: a companion app must scan/connect in the background, subscribe to the characteristics, keep each user's preferences, synchronize the count to your chosen storage/server, and post the OS notification. The phone acknowledges a successfully uploaded count through `FFF5`; the firmware never clears or reduces the local total.

## Power and durability

The CPU sleeps in the SoftDevice event wait between GPIO, timer, and BLE events. BLE advertising remains enabled so a nearby phone can discover the device; continuous discoverability prevents true System OFF deep sleep. Count writes use alternating CRC-checked LittleFS records so a power loss during an update leaves the prior valid record available. Flash-backed BLE bond storage is managed by the Adafruit nRF52 framework.

## Build and upload

From the repository root:

```powershell
python -m platformio run
python -m platformio run -t upload
```

## Serial monitor

Open PlatformIO Serial Monitor at **115200 baud**. The firmware does not wait for the monitor at boot; connect first or reopen the monitor to see the next messages. It reports storage status, restored count, IR passages, battery estimate, BLE connections, proximity crossings, settings changes, and sync acknowledgements.

```powershell
python -m platformio device monitor -b 115200
```

Use a compatible IR receiver and verify battery sensing and BLE behavior on your exact board before installing it in a wearable enclosure.
