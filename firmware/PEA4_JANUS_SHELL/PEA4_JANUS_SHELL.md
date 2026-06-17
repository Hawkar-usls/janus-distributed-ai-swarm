# PEA4 Janus Shell v0.1

First Janus shell for the 4.3" ESP32-P4 board.

This is a separate firmware from `PEA4` pool-probe and from the vendor stock
image. Stock firmware is preserved in flash backups, while this sketch rebuilds
the shell step by step with Janus roles.

## Current v0.1 Scope

- Real ST7701 MIPI DSI display init from vendor Arduino example.
- Real GT911 touch init from vendor Arduino example.
- Framebuffer UI without LVGL dependency.
- Launcher tiles:
  - Camera
  - Swarm
  - Titan
  - Audio
  - Corpus
  - Settings
- Safe SHA256d observer probe for local telemetry.
- NVS memory for boots, best bits, best nonce, candidate count and total hashes.
- SD_MMC pin probe using vendor mp3-player pinmap.
- ES8311/I2S/audio pin map recorded.
- Camera page reserved for vendor ESP-IDF Brookesia/esp_video pipeline.

## Hard Rules

```text
Buzz remains Stratum/pool master.
PEA4 remains observer-only in v0.1.
No target mutation.
No submit pressure.
No fake stock claims: stock functions are preserved as restorable images and
are rebuilt into Janus firmware one subsystem at a time.
```

## Arduino IDE Settings

Board:

```text
ESP32P4 Dev Module
```

Use:

```text
USB CDC On Boot: Enabled
Chip Variant: Before v3.00
Core Debug Level: None
USB DFU On Boot: Disabled
Erase All Flash Before Sketch Upload: Disabled
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 16MB (128Mb)
Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
PSRAM: Enabled
Upload Mode: UART0 / Hardware CDC
Upload Speed: 921600
USB Mode: Hardware CDC and JTAG
```

## Expected Logs

```text
[PEA4/SHELL] boot v=v0.1-janus-shell node=PEA4 role=P4_TITAN kind=p4_janus_shell observer_only=1
[PEA4/HW] chip=ESP32-P4 ...
[PEA4/I2C] bus=0
[PEA4/SD] ok=0 ...
[PEA4/SHELL] stock-like Janus launcher ready
[PEA4/SHELL] v=v0.1-janus-shell node=PEA4 role=P4_TITAN page=...
```

Touch:

- Tap a tile to open it.
- Tap the top header of any page to return home.
- In Settings, tap the backlight line area to toggle LCD backlight.

## Stock Restore Images

Latest backup:

```text
LOCAL_PATH_REDACTED
```

Files:

- `stock_current_esp32p4_COM19_16MB.bin`
- `stock_current_esp32s3_COM4_8MB.bin`
- `README_RESTORE.md`

## Next Steps

1. Flash v0.1 and confirm display orientation/touch coordinates.
2. If touch axis is rotated, adjust GT911 rotation/mirror flags.
3. Add MP3 playback tile after SD card arrives.
4. Port camera from vendor ESP-IDF `esp_brookesia_phone/components/apps/camera`.
5. Add S3 coprocessor bridge after deciding whether to overwrite or preserve its stock firmware.
