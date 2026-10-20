# Post Box Mobile App Developer Guide

This document is the implementation brief for the Android/iOS companion app for the Post Box NRF52 firmware. The repository contains firmware only; it does not include a mobile app, backend, account system, or push-notification server.

## Product goal

Build a clear, reliable app that can find and connect to a Post Box over Bluetooth Low Energy (BLE), display its cumulative IR passage count and battery estimate, configure proximity alerts, synchronize count state, and let an authorized user reset the device count.

Treat the device count as a cumulative total. A reset sets the device's total to zero; it does not delete the app's locally stored history or any backend records.

## Suggested first release

Implement the first release as a phone app with:

- Device discovery, connect/disconnect, pairing, and reconnect.
- Live count and battery status.
- Count, battery, and device event notifications while connected.
- Per-user RSSI boundary and proximity-alert preference.
- Local count sync state. Backend upload can be added later behind an interface.
- A confirmed reset action that writes the firmware reset command and verifies the result.
- Clear permission, connection, pending-write, and error states.

Choose the framework and minimum Android/iOS versions with the product team. Keep BLE access behind a small platform service/repository so screens do not depend directly on a BLE library. Use a maintained BLE library that supports scanning, GATT notifications, encrypted writes, and the required OS background behavior.

## Main screens

1. **Setup and device list**: Explain Bluetooth permissions, scan for nearby Post Box devices, show connection state, and allow the user to select and pair a device.
2. **Counter**: Show the current total, battery percent or “Unavailable,” last update time, connection state, and sync state. Update from notifications and reconcile with a read after reconnect or app resume.
3. **Alerts**: Let the user choose a near/far RSSI threshold and turn proximity alerts on or off. Explain that RSSI is a radio signal estimate, not a reliable distance in meters.
4. **Sync and history**: Show last successfully saved/synced total and time, with retry. Label history as app/backend records; firmware provides a cumulative total rather than a passage-by-passage log.
5. **Device settings**: Provide reconnect, local nickname, firmware/protocol information when available, help, and a destructive “Reset device count” action.

Before reset, show a confirmation naming the selected device and explaining that its device count and sync acknowledgement will become zero. After confirmation, disable repeat taps while the BLE write is in flight. On success, read the count and acknowledgement again and show completion only after confirming both are zero. If the write response is lost, read the device state before offering retry.

## BLE protocol

Advertised local name: `Post Box`  
Primary service UUID: `FFF0`

Use the service UUID and/or local name during scanning. Names can be shortened by an OS, so the service UUID is preferable where supported. All multi-byte integers below are little-endian. Connect, discover services/characteristics, enable needed notifications, and perform encrypted writes only after the operating system has established an encrypted BLE connection/bond.

| Characteristic | UUID | Access | Data |
| --- | --- | --- | --- |
| Count | `FFF1` | Read, notify | 4-byte unsigned cumulative count |
| Battery | `FFF2` | Read, notify | One byte: `0..100` percent; `FF` means unavailable |
| Event | `FFF3` | Notify | 8 bytes: event code `u8`, battery `u8`, event sequence `u16`, count `u32` |
| Settings | `FFF4` | Read, write (encrypted) | Two bytes: signed RSSI dBm `i8`, flags `u8` |
| Sync acknowledgement | `FFF5` | Read, write (encrypted) | 4-byte unsigned acknowledged count |
| Reset count | `FFF6` | Write (encrypted) | Exact bytes `52 53 54 31` (ASCII `RST1`) |
| Set device time | `FFF7` | Write (encrypted) | 4-byte unsigned Unix UTC seconds, little-endian; zero is invalid |
| Last count time | `FFF8` | Read, notify | 4-byte unsigned Unix UTC seconds, little-endian; zero means unknown |

### Event codes

| Code | Meaning |
| --- | --- |
| `1` | IR passage counted |
| `2` | Connected phone entered the configured RSSI boundary |
| `3` | Connected phone left the boundary |
| `4` | Battery estimate is at or below the firmware warning threshold |
| `5` | Counter reset completed; event count is zero |

The event packet count is the cumulative count at the time of the event. Use it to refresh the UI, but do not add it to the previous count. Events may be missed or repeated; the cumulative count read is authoritative.

After connecting and establishing encryption, the app should write the phone's current Unix UTC seconds to `FFF7`. Subscribe to `FFF8` to receive/read the UTC timestamp of the most recent count batch. The existing `FFF3` event packet remains 8 bytes and unchanged. The device uses its running low-power millisecond clock to estimate UTC between phone syncs, so it does not need to stay connected to the phone; clock drift can accumulate between syncs. The clock is unknown after a reboot or power loss until the app syncs again; `FFF8` reports zero for count batches recorded before that sync. `FFF8` is only the latest count timestamp, not an event log; the app should persist every count/time observation it receives if it needs history.

### Settings encoding

- RSSI threshold is a signed two's-complement byte. Firmware accepts `-90` through `-35` dBm.
- Flags bit 0 enables proximity events; write other bits as zero.
- Example: `-65 dBm`, proximity enabled = `BF 01`.
- Firmware stores one shared setting set on the device, not a separate setting for every phone. Save each user's preference in the app and reapply it after connecting; the last connected app that writes settings takes effect.

### Reset behavior

Only send reset after the user confirms. Write exactly four bytes `52 53 54 31` to `FFF6`. The firmware sets count and sync acknowledgement to zero, queues persistence, updates the readable values, notifies the count if subscribed, and emits event code `5` if event notifications are enabled. Then read `FFF1` and `FFF5` and verify zero.

Reset keeps proximity settings and BLE bonds. It does not remove app data, server history, or other phones' records. Before sending `RST1`, persist a reset boundary with the enrolled app device ID, timestamp, final observed `FFF1` count, and a unique epoch/record ID. A successful reset event reports count zero; it does not include the pre-reset count. Keep the boundary after reset so a later count of (for example) 3 is not mistaken for a decrease or data loss.

The app should distinguish the device count in the current hardware epoch, an optional cycle count since an app-defined baseline, and a lifetime total. Calculate lifetime total as completed hardware epochs' final counts plus the current epoch's device count; do not add software-only cycle snapshots again. Firmware has no arbitrary count-write/import command. A “restore” action can only correct the app's logical display/history; it cannot restore the physical counter and must be labeled accordingly.

## Connection and sync flow

On connect:

1. Discover service `FFF0` and characteristics.
2. Enable notifications for `FFF1`, `FFF2`, `FFF3`, and `FFF8`.
3. Read count, last count time, battery, settings, and acknowledgement.
4. Write current Unix UTC seconds to `FFF7` after encryption is established.
5. Apply the current user's saved settings to `FFF4` if they differ.
6. Save the count and its timestamp locally. If a backend is configured, upload idempotently using an enrolled app/backend device ID and a stable request key.
7. Write the exact count to `FFF5` only after the selected local/backend sync step succeeds; read it back to confirm.

On app resume, reconnect or read the count when possible. Never depend only on notifications. On disconnect, preserve the latest local reading and show its timestamp. Retry using bounded backoff and make all sync operations safe to repeat.

The acknowledgement indicates that the app/backend handled readings up to that total; it does not clear the device counter. Do not write a larger backend count to the device: firmware has no import/restore command.

## Notifications and background limits

The firmware sends GATT notifications while connected. The app can post local Android/iOS notifications from received events if the user grants notification permission. The firmware cannot directly push to a phone, and BLE advertisements alone do not display a notification.

Background scanning and BLE connection behavior are controlled by the phone OS and vary by platform, OS version, and permissions. Explain when alerts require the app to remain connected or allowed to run in the background. Request only the Bluetooth/nearby-device, location (if required by the selected Android version/library), and notification permissions needed by the app. Do not promise an alert when the OS has suspended scanning or the device is disconnected.

The firmware's advertisement manufacturer block is currently:

```text
FF FF | 42 43 | 01 | eventActive | eventSequence:u16 LE
```

Exact count is not advertised. `FF FF` is a test company identifier in current firmware and must be replaced with an assigned identifier before commercial release. The 16-bit sequence wraps and restarts after reboot; do not use it alone as a durable event ID.

## App data model

Store at minimum:

- **User preference**: RSSI threshold, proximity enabled, count notifications enabled.
- **Device record**: app-assigned/enrolled ID, local nickname, last observed count, last acknowledged count, last sync time, last connection time, and reset timestamps.
- **Optional backend reading**: enrolled device ID, cumulative count, received timestamp, idempotency key, and reset epoch/boundary.

Do not treat a BLE MAC address as permanent identity; BLE addresses may be randomized. Do not attribute a passage to a specific person: the hardware reports an aggregate count only.

## Error handling

- Device not found or disappears during connect: show retry and last known data/time.
- Pairing or encrypted write fails: explain pairing is needed and provide a retry path.
- Notification setup fails: keep read/refresh available and report live updates as unavailable.
- Settings write fails: keep the user's app preference and show that the device update is pending.
- Sync or backend upload fails: do not advance acknowledgement; retain local data and allow retry.
- Battery is `FF`: show “Unavailable,” not 0%.
- Count reaches `UINT32_MAX`: display the value; firmware saturates instead of wrapping.
- Reset write is uncertain: read count/acknowledgement before retrying.
- Device count is lower than the previous app reading: check for a reset boundary and report the change; never silently merge or overwrite.

## Acceptance checklist

- App discovers and connects to the device on the selected supported Android/iOS versions.
- It handles pairing and encrypted writes to `FFF4`, `FFF5`, `FFF6`, and `FFF7`.
- On each connection it syncs device time from the phone and records `FFF8` timestamps with count updates; it treats zero as unknown and does not infer history the device did not retain.
- Count and battery read correctly; unavailable battery displays distinctly from empty battery.
- Count changes update from notifications and recover through reads after reconnect.
- User settings persist in the app and are reapplied on connection.
- Acknowledgement advances only after the configured sync save/upload succeeds.
- Reset requires confirmation, writes the exact token, verifies count and acknowledgement are zero, and retains app/server history.
- The reset history preserves the final pre-reset device count and timestamp; the dashboard can show the previous count and a clearly defined lifetime total.
- Any reset undo/correction is labeled as app-side state and never represented as a count written back to the device.
- Duplicate/missed notifications do not corrupt cumulative count or sync state.
- UI reports offline, disconnected, pending, and permission-limited states accurately.
- No UI claims passage-by-passage history, person identification, exact distance from RSSI, or guaranteed background alerts.

## Firmware reference

See `APP_HANDOFF.md` for the existing firmware protocol handoff, `src/BleCounterService.cpp` for BLE behavior, `include/AppConfig.h` for UUIDs, and `src/CounterStorage.cpp` for persistent state. Firmware build command: `python -m platformio run` from the firmware project root.
