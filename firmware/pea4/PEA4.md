# PEA4 / Janus P4 2-in-1 v0.2D

Main sketch:

```text
LOCAL_SKETCHBOOK_PATH
```

`PEA4` is now a single firmware name and a single Arduino sketch. It combines:

- stock-like Janus launcher shell for the 4.3 inch ESP32-P4 board
- ST7701 display bring-up
- GT911 touch bring-up
- Camera / Swarm / Titan / Audio / Corpus / Settings pages
- SD_MMC pin probe
- ES8311 audio pin map
- P4_A / P4_B / P4_C dual swarm core roles
- observer SHA-256 compute stream
- best_z proof events
- JP4 serial bridge frames
- mirror verification queue
- anchor memory ring
- NVS memory for role, mode, job, best_z, digest, intention and blackboard scores
- touch tap fix with saved press coordinates
- visible BACK control
- right swipe returns home
- left swipe switches to next page
- persistent bottom navigation bar
- immediate touch navigation on press
- broad back rescue zone
- long press on any non-home page returns home
- last raw touch coordinates displayed in footer
- I2C touch scan diagnostics
- quiet SD no-card soft-fail
- camera page now shows a real bring-up status instead of fake preview
- Serial `PEA4/PN` heartbeat for NAS / companion bridge intake

Hard rules:

```text
PEA4 observes, computes, verifies, displays and archives.
Buzz remains the Stratum and pool authority.
PEA4 does not submit shares.
PEA4 does not mutate target or pool math.
```

The old standalone file:

```text
LOCAL_SKETCHBOOK_PATH
```

is now only a donor/reserve core. Flash `PEA4.ino`.

## Serial Commands

```text
help
status
json
jp4
role A
role B
role C
mode AUTO
mode HASH
mode SCOUT
mode MIRROR
mode VERIFY
mode RELAY
mode IDLE
job A1B2C3D4 22
seed DEADBEEF
save
reset
clearstate
```

JP4 frame shape:

```text
JP4,1,node_hex,role,mode,seq,nonce,salt_hex,job_seed_hex,best_z,digest,hps*checksum
```

Presence heartbeat:

```text
[PEA4/PN] node=PEA4 ... radio=0 needs_bridge=1 cam=0 ... stack="ESP-IDF esp_video / MIPI-CSI"
```

Use JP4 over USB Serial now. Later the companion ESP32-S3/C6 can bridge these
frames to ESP-NOW, Wi-Fi or NAS.

## Camera Status

The stock camera is not an Arduino `esp_camera` path. The vendor package uses
ESP-IDF `esp_video` with MIPI-CSI camera support, so the Arduino shell keeps the
camera tile and diagnostics but does not start a real preview yet.

Vendor camera reference:

```text
LOCAL_PATH_REDACTED
```

Next camera step: port that IDF `video_lcd_display`/Brookesia camera pipeline
into a Janus IDF build, then reconnect its frame status back into this shell.

## Buzz Visibility

PEA4 is an ESP32-P4 compute/display node. It currently has no ESP-NOW radio path
inside this Arduino sketch, so Buzz will not count it as the 10th worker by
itself. For Buzz visibility we need a companion ESP32-C6/S3 bridge that reads
`JP4` / `PEA4/PN` from PEA4 and rebroadcasts SwarmSense/OXY presence on channel
10.

Until that bridge exists:

```text
PEA4 alive on Serial: yes
PEA4 visible to NAS bridge: ready
PEA4 visible to Buzz over ESP-NOW: no, bridge required
```


## Arduino IDE Settings

```text
Board: ESP32P4 Dev Module
Port: COM19
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

PSRAM must stay enabled because the 480x800 framebuffer lives there.

The board can sometimes flash with `USB-OTG (TinyUSB)` and `CDC Disabled`, but
for Janus logs and JP4 Serial bridge use `USB CDC On Boot: Enabled` and
`USB Mode: Hardware CDC and JTAG`.

Serial Monitor:

```text
Baud: 115200
USB CDC On Boot must be Enabled
USB Mode must be Hardware CDC and JTAG
```

## Stock Firmware Preservation

The vendor stock shell is not embedded as binary code here. It is preserved as a
flash backup, and PEA4 rebuilds the useful shell shape step by step.

Local backup folder:

```text
LOCAL_PATH_REDACTED
```

## Compile Check

Last checked with Arduino CLI:

```text
Sketch: 528318 bytes
RAM: 29104 bytes
Target: esp32:esp32:esp32p4, 16MB flash, PSRAM enabled
```
