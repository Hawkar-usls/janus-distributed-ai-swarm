# Anchor v1.20 — S/2 Safe Queue

The public source is split into ordered fragments to keep reviews manageable.
Before opening it in Arduino IDE, assemble the IDE-ready single file from the
repository root:

```bash
python tools/assemble_swarm_sketches.py --target anchor
```

Then open `build/Anchor/Anchor.ino`. GitHub Actions uses the same assembler before
compiling, so the checked source and the downloadable Arduino sketch are identical.

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

Set local credentials in the assembled sketch before flashing and do not commit them.

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
