# Reset and Previous Count: App Integration Plan

## Scope

This document records companion-app requirements for the firmware in this repository. The mobile app source is maintained separately; UI components, storage services, and app repository classes must be implemented and reviewed in that app repository. This firmware repository documents the BLE contract and integration expectations only.

See [APP_DEVELOPER_GUIDE.md](../APP_DEVELOPER_GUIDE.md) for the complete BLE protocol and app behavior, and [APP_HANDOFF.md](../APP_HANDOFF.md) for the implementation handoff.

## Firmware contract

- `FFF1` is the device's current cumulative `uint32` count, readable and notifiable.
- `FFF6` accepts exactly four ASCII bytes, `RST1`, over an encrypted BLE link. It resets the device count and sync acknowledgement (`FFF5`) to zero.
- After reset, the app should read `FFF1` and `FFF5` and confirm both are zero. If a write result is uncertain, read device state before offering a retry.
- The firmware has no command to set or restore an arbitrary count. Reset does not erase app or server history.
- Firmware persists the device count and sync acknowledgement. It does not retain an event-by-event passage history; the app must record reset boundaries and readings if it needs history.
- `FFF7` accepts a 4-byte little-endian Unix UTC timestamp over an encrypted link. The app should send the phone's current time after each connection.
- `FFF8` is a readable/notifiable 4-byte UTC timestamp for the latest count batch. Zero means unknown; firmware keeps the 8-byte `FFF3` event packet unchanged and does not provide a durable timestamp log.
- Firmware estimates time between syncs from its running low-power clock. Time is unknown again after reboot or battery power loss until the phone reconnects and syncs it.

## App requirements

### Reset and previous-count record

Before sending `RST1`, save a reset-boundary record containing the enrolled app device ID, timestamp, final observed device count, and a unique epoch or record ID. Preserve that record after reset. Present the previous count and reset time in the app's dashboard/history.

Do not label the app's reset action a software fallback as if it reset the physical device. If `FFF6` is unavailable or the encrypted write cannot be completed, explain that the hardware count was not reset. A software-only cycle boundary may be offered as a separate app feature, provided the app records its offset and clearly distinguishes the displayed cycle count from the hardware's cumulative count.

### Count calculations and restore behavior

Define count semantics explicitly in the app:

- **Device count:** the value currently reported by `FFF1` within the current hardware epoch.
- **Cycle count:** the count since the last app-defined cycle boundary, calculated using the stored baseline for that cycle.
- **Lifetime total:** sum of completed hardware epochs' final counts plus the current device count. Do not also add software-only cycle snapshots, which would double count passages.

An undo action cannot write a previous count back to the hardware. If offered after a hardware reset, it must be described as a logical app correction: preserve the reset record, adjust the app's display/baseline as defined by the app's data model, and continue reconciling new readings from `FFF1`. Do not claim the restored value is the device count. Prefer correcting or annotating the reset record over deleting audit history.

### UI and history

The app may show the latest previous count and timestamp, current device/cycle count, and lifetime total. History should distinguish hardware reset boundaries from software-only cycle boundaries. The app repository should own persistence, calculations, and undo/correction behavior; screens should render that state rather than independently changing BLE baselines.

## Implementation checklist for the companion app repository

1. Review the BLE service and encryption requirements in the firmware guides.
2. Persist reset boundaries before issuing the reset command.
3. Send the exact `RST1` payload only after user confirmation and with the required encrypted connection.
4. Read back `FFF1` and `FFF5`; report success only when both are zero.
5. Implement device, cycle, and lifetime counts with tests for resets, reconnects, missed notifications, repeated commands, and software-only cycle boundaries.
6. Add previous-count and reset-time display plus correction history with wording that distinguishes app state from hardware state.

## Verification boundary

Firmware verification belongs in this repository and should validate the documented GATT behavior. UI, durable app storage, lifetime-count calculations, and reset-history interaction must be verified in the separate companion-app repository and on a connected device.
