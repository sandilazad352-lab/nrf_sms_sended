# IR Book Counter companion app handoff

## Purpose

Build a phone app that connects to the NRF52840 IR counter firmware, shows and synchronizes its cumulative count, lets each person configure their own proximity preference, and can show phone notifications for new count events and proximity events.

The firmware is split across `src/` and `include/`. The IR input defaults to Arduino pin 20 (`PIN_020`, nRF `P0.20`). This repository does not contain a mobile app or a cloud service. This handoff is intended for the mobile-app developer; the embedded firmware engineer owns the device firmware and hardware integration.

## App responsibilities

- Discover the device advertising as **Book Counter** and connect to its BLE GATT service.
- Pair/bond with the device when the OS requests it. The settings and sync characteristics require an encrypted BLE link.
- Read and display the current count and battery estimate.
- Subscribe to count, battery, and event notifications.
- Persist each user's personal boundary preference in the app profile. On connection, send that user's selected RSSI threshold to the device. Firmware stores one shared threshold, so the active phone is the last writer; it is not a separate setting per bonded phone.
- Keep a local copy of the last successfully synced device count. If a backend is later added, upload the cumulative reading idempotently and only acknowledge it after the upload succeeds.
- Show local phone notifications when subscribed events arrive. The firmware cannot create Android or iOS notifications itself.
- Provide a clear connection, sync, and battery status, including when the device is out of range or the app is not allowed to scan in the background.

## MVP screens

1. **Device setup:** BLE permission request, nearby device list, connect/pair, connection result, and a short explanation of background operation.
2. **Counter home:** current total, last event, battery percent or “unavailable,” sync status, and device connection state.
3. **Boundary and alerts:** per-user proximity threshold, proximity-alert toggle, and count-event notification toggle.
4. **Sync/history:** last successful sync time, last acknowledged device count, retry action, and a simple count history sourced from app/server records.
5. **Device settings:** reconnect, rename locally, unpair instructions, firmware/version placeholder, and privacy/help information.

Avoid claiming a precise distance in meters from RSSI. RSSI varies with the phone, orientation, walls, pockets, and radio interference; expose a “near/far” threshold or a calibrated slider instead.

## BLE service and characteristics

The custom 16-bit service UUID is `FFF0`. Scan for the service UUID and/or local name `Book Counter`; do not depend only on the name because operating systems may shorten scan-response names.

| Characteristic | UUID | Properties and access | Wire format |
| --- | --- | --- | --- |
| Count | `FFF1` | Read, notify; open read | 4-byte unsigned cumulative count, little-endian |
| Battery | `FFF2` | Read, notify; open read | 1 byte: 0–100 percent; `0xFF` means unavailable |
| Event | `FFF3` | Notify | 8 bytes: `eventCode:u8`, `batteryPercent:u8`, `eventSequence:u16 LE`, `count:u32 LE` |
| Settings | `FFF4` | Read, write; encrypted link required | 2 bytes: `nearRssiDbm:i8`, `flags:u8` |
| Sync acknowledgement | `FFF5` | Read, write; encrypted link required | 4-byte unsigned acknowledged count, little-endian |
| Reset count | `FFF6` | Write; encrypted link required | Exact four ASCII bytes `RST1` (`52 53 54 31` hex); resets device count and sync acknowledgement to zero |

Event codes:

| Code | Meaning |
| --- | --- |
| `1` | IR beam passage counted |
| `2` | Connected phone entered the configured RSSI boundary |
| `3` | Connected phone left the configured RSSI boundary. A disconnect is reported by BLE connection state, not by this event packet. |
| `4` | Battery estimate at or below the firmware low-battery threshold |
| `5` | Counter reset completed; count is zero |

Settings byte details:

- `nearRssiDbm`: signed two's-complement byte. Firmware accepts `-90` through `-35` dBm. Example: `-65` is byte `0xBF`.
- `flags` bit 0 enables proximity events; all other bits are reserved and should be written as zero.
- Firmware currently stores one threshold and flag set for the whole device. The phone should store its own user preference and write it on connection. If several registered phones take turns, each phone's value replaces the previous device value.

Example settings payload for `-65 dBm` with proximity enabled: `BF 01`.

## Count sync behavior

The device maintains a cumulative total and does not clear it after a sync. The app should:

1. Read `FFF1` on connect and whenever the app returns to the foreground.
2. Compare it with the app's last stored device count; never assume a notification is the only source of truth.
3. Save the latest reading locally and, if configured, upload it to the backend using a stable device identity and idempotency key.
4. After the reading is safely saved/uploaded, write that exact count to `FFF5`.
5. Read `FFF5` back to confirm the acknowledgement.
6. Retry safely after disconnects or failures. A failed sync must not clear or decrement the device count.

The sync acknowledgement means “the app/backend has handled counts up to this total”; it does not modify the counter. To reset the count, require an in-app confirmation, connect and pair with the device, then write the exact bytes `52 53 54 31` to `FFF6`. A successful command sets both the count and acknowledged count to zero, persists that state, and notifies subscribed clients with event code 5. Read `FFF1` and `FFF5` afterward to confirm. Do not optimistically clear app or server history; reset only the device's cumulative counter. Reset does not erase saved proximity settings or BLE bonds. If the write result is uncertain, read the count before retrying.

The current firmware has no command to import a server total or restore a count to a replacement board. Full multi-device restore/merge therefore needs a separate authenticated import command. The current firmware also exposes a total, not a durable event-by-event history; the app should not promise historical per-person passage records.

## Proximity and phone notifications

The device can evaluate RSSI proximity while a phone is connected. When the threshold is crossed, it emits event code 2 or 3. RSSI uses 5 dB hysteresis to reduce rapid near/far flapping.

For reliable proximity alerts, the app needs an OS-permitted BLE background strategy and must subscribe to `FFF3`. If the OS suspends the app or drops the BLE connection, the device cannot deliver a GATT notification to that phone. Count events are also represented in advertisement metadata, but an advertisement alone does not display a phone notification; the app must scan and post the local OS notification.

Ask for notification permission before enabling alerts. Explain the platform's Bluetooth/background location or nearby-device permissions in plain language. Keep background scanning and connection attempts battery-aware, deduplicate repeated advertisement sequences, and reconnect with bounded backoff.

## Advertising payload

The advertisement includes the `FFF0` service, TX power, a manufacturer-data block, and a scan-response name. Manufacturer bytes are:

```text
FF FF | 42 43 | 01 | eventActive | eventSequence:u16 LE
```

- `FF FF` is the Bluetooth SIG test company identifier and must be replaced with an assigned company ID before a commercial release.
- `42 43` is the project marker (`BC`).
- `01` is protocol version 1.
- `eventActive` is 0 or 1.
- `eventSequence` increments when the IR counter records a passage and wraps at 16 bits.
- Exact visitor count is not broadcast; read it through `FFF1` after connecting.

The sequence restarts when the board reboots. Deduplicate using sequence plus the last known cumulative count, not sequence alone.

## Data model suggestions

Keep separate records for:

- **User preferences:** RSSI boundary, proximity enabled, count notifications enabled, display units, quiet hours.
- **Device state:** locally assigned nickname, last known cumulative count, last acknowledged count, last sync time, firmware protocol version, bond/connection status.
- **Backend sync record (optional):** device identity, cumulative count, received timestamp, upload idempotency key.

Do not use the BLE MAC address as a permanent account identity. Use an app/backend-issued device enrollment ID or a user-confirmed device record; BLE addresses can be randomized.

## Error and edge cases

- Device disappears during connect, read, notify subscription, or sync.
- Pairing/encryption is cancelled or bond becomes stale; offer a retry and clear-bond instructions.
- Settings write fails; keep the user's selected value and show “pending device update.”
- Notification is duplicated or missed; reconcile from the cumulative count on reconnect.
- Count reaches `UINT32_MAX`; firmware saturates there instead of wrapping.
- Battery returns `0xFF`; show “unavailable,” not 0%.
- App is offline; keep local data and retry backend sync later.
- Backend count is higher than the device count; do not silently write it to the device. Current firmware has no import/restore command.
- More than one phone is registered; the current firmware allows one active central connection and has one shared proximity threshold. Each app must reapply its own preference when it connects.

## Acceptance checklist

- App discovers and connects to the Book Counter on supported Android/iOS versions selected for release.
- Pairing/encrypted writes work with `FFF4` and `FFF5`.
- Current count and battery display correctly, including `0xFF` battery-unavailable handling.
- A beam-break count updates the UI through notifications and is recovered by a fresh read after reconnect.
- Settings survive app restart and are applied to the device on each connection.
- A count is acknowledged only after the app's local/backend sync step succeeds.
- Duplicate event delivery does not double-add the cumulative count.
- App explains when OS background restrictions prevent real-time alerts.
- User can export or remove their app-held data according to the product's privacy requirements.

## Firmware references

- BLE protocol implementation: `src/BleCounterService.cpp` and `include/AppConfig.h`
- IR sensor input: `src/IrCounter.cpp` and `include/AppConfig.h`
- Persistent count storage: `src/CounterStorage.cpp`
- Board/pin setup and low-power notes: `PROJECT_README.md`
- Build command from repository root: `python -m platformio run`
