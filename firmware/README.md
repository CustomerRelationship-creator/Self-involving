# M3 voice-body firmware

This is the ESP-IDF 6.0.2 firmware for the
`movecall-moji-esp32s3-enterprise` body. It keeps the M0 recovery diagnostics and
adds the M3 audio, network, session and visible-state paths.

## Implemented M3 behavior

- continuously captures 24 kHz, 16-bit mono audio into a bounded 512 KB PSRAM
  ring; idle audio is overwritten and never written to Flash;
- short GPIO0 press starts or cancels an authorized voice session;
- two-second press enters or leaves software mute;
- sends 20 ms PCM frames over authenticated TLS WebSocket with bounded queues;
- accepts 24 kHz PCM reply frames and plays them through ES8311;
- displays BOOTING, configuration, connecting, idle, listening, thinking,
  speaking, muted, offline and error states on the GC9A01 screen;
- reconnects Wi-Fi/WebSocket and keeps local mute, display and diagnostics
  independent from the gateway;
- never stores a model-provider API key on the device.

M3 deliberately uses button-authorized sessions. A tested ESP-SR wake model can
later call the same `StartSession()` path without changing the gateway protocol.
Until false-wake and memory measurements exist, ambient speech must not silently
open an upload session.

## Configure

Run `idf.py menuconfig`, open **Self-involving M3**, and set Wi-Fi, a `wss://`
gateway URI and a revocable per-device token. These values can also be written
to NVS namespace `m3_config` with keys `wifi_ssid`, `wifi_pass`, `gateway_uri`
and `device_token`. Do not use a model-provider API key as the device token.

## Build

```bash
cd firmware
. "$IDF_PATH/export.sh"
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

## First-device safety

Before the first write, save the full 16 MB Flash and eFuse summary:

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 \
  read_flash 0x0 0x1000000 second-device-factory-backup.bin
espefuse.py --chip esp32s3 --port /dev/cu.usbmodem21101 summary
```

Do not burn eFuses, enable Secure Boot or enable Flash Encryption until recovery
has been tested on the physical unit. The first M3 image writes the bootloader,
partition table, OTA data and application; later updates can use `app-flash`.

## Not yet a physical validation claim

CI verifies compilation and image size. The second device must still verify
ES8311 clock/slot settings, microphone channel, amplifier polarity, screen color
order, acoustic gain and at least 100 consecutive sessions before the PR leaves
Draft status.
