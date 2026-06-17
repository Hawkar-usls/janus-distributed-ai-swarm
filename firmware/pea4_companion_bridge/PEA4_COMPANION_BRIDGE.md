# PEA4 Companion Bridge v0.1

This is the radio voice for PEA4.

PEA4 itself is an ESP32-P4 compute/display node. In the current Arduino shell it
does not send ESP-NOW packets directly, so Buzz cannot count it as the 10th swarm
participant unless a companion ESP32-S3/C6 rebroadcasts its telemetry.

Main sketch:

```text
LOCAL_SKETCHBOOK_PATH
```

What it does:

- listens for PEA4 `JP4` and `[PEA4/PN]` text frames on USB Serial and UART1
- sends Buzz-compatible SwarmSense `S/S` packets on ESP-NOW channel 10
- reports `nodeId=PEA4` and `kind=p4_titan_bridge`
- carries PEA4 hash rate, best bits, target, digest scent and bridge freshness
- observer-only: no shares, no pool submit, no scheduler pressure

Default UART wiring if a physical bridge is used:

```text
Bridge RX: GPIO18
Bridge TX: GPIO17
Baud: 115200
```

If no UART is wired, the bridge still announces PEA4 but with stale/low
confidence until it sees real `JP4` / `PEA4/PN` frames.

Compile check:

```text
Target tested: esp32:esp32:esp32s3
Sketch: 907085 bytes
RAM: 44900 bytes
```
