# Golcron Astrolabe v0.1

11th Janus swarm participant: a small medallion worker with an SSD1306 OLED.

Main sketch:

```text
LOCAL_SKETCHBOOK_PATH
```

Role:

- receives Buzz `J/B` jobs on ESP-NOW channel 10
- solves them as a real worker and replies with `S/2`
- sends `JANUS` status packets so Buzz can list it as a node
- sends `S/S` SwarmSense telemetry for NAS/Buzz observer views
- uses a deterministic star-map layer to choose nonce offset/stride
- OLED shows Buzz/job state, hashrate, best bits, star, offset/stride and range progress

Hard rule:

```text
Star map = nonce traversal order only.
SHA256, block header, target compare and S/2 wire stay standard.
```

Default OLED pins:

```text
SDA: GPIO21
SCL: GPIO22
Address: SSD1306 handled by U8g2
```

Arduino IDE settings used for compile:

```text
Board: ESP32 Dev Module
CPU Frequency: 240MHz
Flash Mode: QIO
Flash Frequency: 80MHz
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs
PSRAM: Disabled
Upload Speed: 921600
Core Debug Level: None
Events Run On: Core 1
Arduino Runs On: Core 1
```

Compile check:

```text
Sketch: 938452 bytes
RAM: 48328 bytes
```

Healthy serial signature:

```text
[GOLCRON] v0.1 ready node=Golcron ch=10 worker=...
[GOLCRON] buzz=1 job=1 ... H=... best=... star=...
```
