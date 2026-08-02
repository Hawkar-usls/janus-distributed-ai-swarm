# Blind Eye v2.15A — TMOS Primary / Camera Absent

Blind Eye is an M5Stack AtomS3R sensor and swarm worker. On this physical unit the camera is absent by design of the current repair state; firmware treats that as a normal hardware profile rather than a fault.

## Primary sensing

- Primary eye: STHS34PF80 TMOS/PIR on Grove I2C, SDA 2 / SCL 1.
- TMOS output rate: 15 Hz.
- Current presence and motion truth use software evidence plus the TMOS hardware flags.
- Confirmed events receive a 900 ms anti-flicker hold. The hold does not feed memory back into itself.
- A valid empty room raises `clear` and drives `artifact` down. `artifact` rises only for stale/invalid frames or contradictory residual memory.
- RF-lite, IMU, magnetometer and microphone remain secondary fusion channels.

## Camera and aperture frames

`JANUS_EYE_CAMERA_PRESENT` is `0`. No camera bytes are read anywhere in this build.

Optional `E/F` packets are synthetic 8×8 TMOS/RF aperture maps made from real scalar sensor telemetry and predicted sector state. They are explicitly marked synthetic and are not represented as camera pixels. Event snapshots may be sent at low rate after real TMOS detections; continuous streaming still requires an `E/C` request.

## Swarm role

Blind Eye publishes heartbeat, entropy, SwarmSense, Blackboard events, Kenshi bubbles and Tachyon prophecy packets. Its Buzz worker remains idle in `SEEK` with `H=0` until a real `J/B` assignment arrives; that state is normal.

Atomic Motion Base support remains optional. When no base is detected on SDA 38 / SCL 39 at address `0x38`, Blind Eye enters `BASELESS_SENSOR` and continues TMOS/RF/ESP-NOW/miner operation without actuator writes.

## Arduino project

Open `BLIND_EYE.ino` in Arduino IDE. The numbered `.ino` files in this folder are ordered tabs and are compiled automatically as one sketch.

Recommended profile:

- Board: ESP32S3 Dev Module / matching AtomS3R profile
- CPU: 240 MHz
- USB CDC On Boot: Enabled
- Flash/PSRAM settings must match the physical AtomS3R board

Libraries:

- M5Unified
- ArduinoJson
- M5_STHS34PF80
- ESP32 Arduino core facilities: LittleFS, ESP-NOW, Wi-Fi, mbedTLS and I2S

Public source uses `YOUR_WIFI` and `YOUR_PASS`; keep real credentials in a local uncommitted copy.

## Hardware validation

Observed on the physical node:

- TMOS initialized at 15 Hz with camera absent reported normally.
- Quiet-room state reached `clear=1.00`, `artifact=0.00`.
- `bad=0/0`, `err=0`, fresh sensor reads.
- ESP-NOW traffic continued with increasing successful transmissions.
- Four event aperture frames were emitted after a TMOS hardware-motion flag during warmup.

## CI validation

The ordered tabs are concatenated and compiled in GitHub Actions for `esp32:esp32:esp32s3` with ESP32 Arduino core 3.3.11, M5Unified 0.2.19, ArduinoJson 7.4.3 and M5-STHS34PF80.

- assembled source: 4922 lines
- program storage: 1,162,307 bytes / 1,310,720 bytes (88%)
- global memory: 53,248 bytes / 327,680 bytes (16%)
- assembled public sketch SHA-256:

`6f4de99fc710beff9fe30c09efda8fef33be2e25b94f87aa1fe01e6b0e251137`
