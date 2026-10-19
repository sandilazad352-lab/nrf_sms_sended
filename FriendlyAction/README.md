# Friendly Action

A tiny Nice!Nano / nRF52840 Supermini demo: the onboard red LED gives three quick hello blinks, then pauses before repeating.

## Build

From this folder, run `pio run`.

## Upload

Connect the board over USB and run `pio run -t upload`. The board definition uses the `nrfutil` upload protocol. Some bootloaders may require entering UF2/bootloader mode before upload.
