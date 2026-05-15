# M5Stack Review Guide

JANUS LastSwarm is an independent ESP32/M5Stack-oriented firmware suite. It is not an official M5Stack product. The repository is scoped for external hardware review: eight current sketches, focused documentation, minimal config examples, and static audits.

## 10-Minute Path

1. Read `README.md` for scope.
2. Read `docs/m5stack-showcase.md` for the hardware story.
3. Read `docs/technical-boundaries.md` for the protocol, AI-control, and sensor-truth boundaries.
4. Inspect the current firmware entrypoints:
   - `firmware/core2/CORE2.ino`
   - `firmware/buzz/Buzz.ino`
   - `firmware/blind_eye/BLIND_EYE.ino`
   - `firmware/beacon/Beacon_A1.ino`
   - `firmware/esp32_swarm/Stick.ino`
   - `firmware/esp32_swarm/ATOM_SWARM_TRON.ino`
   - `firmware/pyramid/ATOM_MATRIX_Pyramid.ino`
   - `firmware/zim_geek/Zim.ino`
5. Check `audits/arduino_static_audit.md` and `audits/file_inventory.md`.

## What To Notice

- Multiple ESP32/M5 nodes communicate through ESP-NOW.
- Firmware combines display UI, sensing, telemetry, local state, and remote memory packets.
- Prediction, memory, and current presence are treated as separate states.
- Stratum/PoW-related firmware paths are documented as protocol and telemetry work.
- The public repository is intentionally narrow: no non-firmware side projects, raw dumps, broad archives, or unrelated research notes.

## Strong M5Stack-Relevant Pieces

- `CORE2.ino`: Core2 control/display/swarm surface.
- `Buzz.ino`: audio/autopilot/worker lineage with ESP-NOW and Stratum awareness.
- `BLIND_EYE.ino`: sensing and calibration path with now/memory/prediction separation.
- `Beacon_A1.ino`: beacon preserve candidate.
- `Stick.ino`: mobile swarm node.
- `ATOM_SWARM_TRON.ino`: Atom swarm node with local/remote worker modes and telemetry.
- `ATOM_MATRIX_Pyramid.ino`: Atom Matrix visual/swarm candidate.
- `Zim.ino`: ESP32-S3 Geek node with swarm reporting and local adaptive state.

## Scope Boundaries

- Firmware claims require board-level validation.
- AI claims are limited to adaptive control, scoring, memory, prediction-error tracking, and workload policy.
- Mining claims are limited to protocol handling, telemetry, scheduling, verification, and measurement.
- Public distribution excludes secrets, private keys, raw databases, raw telemetry dumps, and unrelated experiments.
