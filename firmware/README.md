# M0 hardware self-test firmware

This directory contains the first bootable firmware for the unflashed
`movecall-moji-esp32s3-enterprise` device. It is intentionally limited to safe,
reversible board diagnostics.

## What it does

- keeps the speaker power amplifier disabled on every boot;
- verifies the expected 16 MB Flash and 8 MB Octal PSRAM;
- runs a 512 KB PSRAM write/read pattern test;
- validates the custom partition table and records a bounded NVS boot counter;
- probes the two expected ES8311 I2C addresses without changing codec registers;
- initializes the official Espressif GC9A01 driver and displays color bands;
- polls GPIO0: short press changes the screen test pattern, two-second press
  toggles software mute/black screen;
- logs internal RAM and PSRAM watermarks every ten seconds.

It does **not** yet initialize I2S, play audio, connect Wi-Fi, upload microphone
data, implement wake words, or contact a remote model.

## Requirements

- ESP-IDF 6.0.2
- Espressif Component Manager with network access on the first build
- USB-C data cable

The build pins `espressif/esp_lcd_gc9a01` to 2.0.4. No model or animation asset
is downloaded into the firmware.

## First-device safety procedure

Before writing anything, save the complete 16 MB Flash image:

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 \
  read_flash 0x0 0x1000000 second-device-factory-backup.bin
```

Also save `espefuse.py summary` output. Do not burn eFuses, enable Secure Boot,
or enable Flash Encryption during M0.

## Build and flash

```bash
cd firmware
. "$IDF_PATH/export.sh"
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem21101 flash monitor
```

The first flash writes the bootloader, partition table and application. Later
M0-only application updates can use `idf.py app-flash` after the partition table
has been verified.

## Expected display

- green/white/blue bands: all critical non-display checks passed;
- red/amber/black bands: at least one critical check failed;
- LED on after a successful diagnostic pass;
- the amplifier remains disabled in either case.

Only use GPIO0 after the application has booted. Holding it while resetting the
board selects the ESP32-S3 ROM download mode.

The actual panel orientation, color order, backlight polarity, LED polarity and
ES8311 address still require confirmation on the second physical device.
