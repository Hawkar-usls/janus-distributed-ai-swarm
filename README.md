# JANUS LastSwarm Firmware Suite

## What This Is

JANUS LastSwarm is a focused ESP32 / M5Stack firmware suite built around eight current LastSwarm sketches. It covers ESP-NOW swarm communication, device UI, sensing, telemetry, local memory, adaptive control hints, and strict separation between sensor truth, prediction, and protocol events.

This public repository is intentionally narrow. It contains the current firmware entrypoints, minimal configuration examples, safety notes, and static audits for these sketches only.

## Firmware Entrypoints

| component | path | role |
| --- | --- | --- |
| Core2 / SwarmSense | `firmware/core2/CORE2.ino` | Main Core2 display/control surface |
| Buzz | `firmware/buzz/Buzz.ino` | Audio, worker, ESP-NOW, and Stratum-aware node |
| Blind Eye | `firmware/blind_eye/BLIND_EYE.ino` | Presence/sensor calibration node |
| Beacon | `firmware/beacon/Beacon_A1.ino` | Beacon preserve candidate |
| Stick | `firmware/esp32_swarm/Stick.ino` | Mobile ESP32 swarm node |
| Atom Swarm TRON | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` | Atom swarm/TRON node |
| Atom Matrix Pyramid | `firmware/pyramid/ATOM_MATRIX_Pyramid.ino` | Atom Matrix visual/swarm node |
| Zim Geek | `firmware/zim_geek/Zim.ino` | ESP32-S3 Geek node with swarm reporting |

## Hardware

- M5Stack Core2
- M5Stick / Cardputer-style ESP32 nodes
- M5 Atom / Atom Matrix / AtomS3-style nodes
- ESP32-S3 Geek-style node
- TMOS / PIR / STHS34PF80-style presence sensing
- ENV / pressure / temperature / microphone paths where supported by firmware
- ESP-NOW swarm communication
- Wi-Fi where a sketch explicitly uses network or Stratum paths

## Design Rules

- Preserve working behavior before simplifying.
- Treat ESP-NOW packet formats as an ABI.
- Keep current sensor readings separate from memory and prediction.
- Keep pool/Stratum events separate from local simulation and UI rewards.
- Prefer non-blocking UI, audio, sensor, and swarm loops.
- Treat heat, jitter, and throttling as control signals.
- Keep secrets out of firmware and use examples under `configs/examples/`.

## Repository Layout

- `firmware/`: the eight current LastSwarm sketches.
- `docs/`: architecture, hardware, calibration, and technical boundary notes.
- `audits/`: focused inventory and static firmware audits.
- `configs/examples/`: placeholder config and `secrets.example.h`.
- `tests/static_checks/`: lightweight secret marker check.

## What Is Deliberately Not Here

This public main branch does not include older JANUS experiments, non-firmware side projects, raw runtime dumps, databases, archives, unrelated research notes, or historical scratch files. The goal is a clean technical review surface for the current LastSwarm firmware.

## Quick Start

1. Open the relevant `.ino` in Arduino IDE or PlatformIO.
2. Install the board support and libraries required by that sketch.
3. Copy `configs/examples/secrets.example.h` into a local ignored `secrets.h` if needed.
4. Fill local Wi-Fi or worker values only in ignored local files.
5. Verify board pins, optional sensors, and ESP-NOW packet compatibility before flashing mixed firmware generations.

## Review Path

Start here:

- `M5STACK_REVIEW_GUIDE.md`
- `docs/m5stack-showcase.md`
- `docs/technical-boundaries.md`
- `audits/arduino_static_audit.md`
- `audits/file_inventory.md`

## Safety

No private keys, Wi-Fi passwords, tokens, raw databases, model checkpoints, or bulk telemetry dumps are intended to be committed. Mining-related firmware paths are preserved as protocol/telemetry work and require careful network and hardware review before use.

## License

Released under the MIT License. Confirm third-party library and hardware-vendor license requirements before production use.
