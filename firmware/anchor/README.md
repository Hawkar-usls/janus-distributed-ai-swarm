# Anchor v1.20 — S/2 Safe Queue

Open `Anchor.ino` from this folder as one Arduino sketch. Arduino IDE compiles
`Anchor.ino` and all numbered `Anchor_part_*.ino` tabs together in filename order.

## Board

- Board: **ESP32S3 Dev Module**
- CPU frequency: **240 MHz**
- USB CDC On Boot: **Enabled**
- PSRAM: use the setting appropriate for the physical board; the sketch does not require it

## Public configuration

The public source deliberately contains placeholders:

```cpp
#define JANUS_WIFI_SSID "YOUR_WIFI"
#define JANUS_WIFI_PASS "YOUR_PASS"
```

Set local credentials before flashing and do not commit them.

## Current behavior

- current Buzz share packet: `S/2`
- callback-safe ESP-NOW receive queue
- Buzz `range_size == 0` discovery pings do not become fake mining jobs
- same-work queue/yield policy for useful nonce slices
- direct Buzz recovery plus broadcast path
- direct Anchor↔Gladius `J/T` twin coordination and lane splitting
- RF Dome / P/N observer telemetry
- BOOT tap cycles RGB brightness; very long hold toggles full UART logs and the small activity LED

The adaptive scheduler may change nonce order, lane, stride or sector. It must
not change the assigned header, target, SHA256d calculation or submit semantics.
