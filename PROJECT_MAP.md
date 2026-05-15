# Project Map

This repository is a focused public surface for the current LastSwarm firmware drop.

## Firmware

- `firmware/core2/CORE2.ino`
- `firmware/buzz/Buzz.ino`
- `firmware/blind_eye/BLIND_EYE.ino`
- `firmware/beacon/Beacon_A1.ino`
- `firmware/esp32_swarm/Stick.ino`
- `firmware/esp32_swarm/ATOM_SWARM_TRON.ino`
- `firmware/pyramid/ATOM_MATRIX_Pyramid.ino`
- `firmware/zim_geek/Zim.ino`

## Documentation

- `M5STACK_REVIEW_GUIDE.md`: shortest external review path.
- `docs/m5stack-showcase.md`: hardware-oriented overview.
- `docs/architecture.md`: node and packet-level architecture.
- `docs/technical-boundaries.md`: protocol, adaptive control, and validation boundaries.
- `docs/blind-eye-calibration.md`: sensing and presence calibration notes.
- `docs/beacon-preserve-policy.md`: preserve policy for Beacon behavior.
- `docs/esp-now-swarm.md`: ESP-NOW compatibility notes.

## Audits

- `audits/file_inventory.md`
- `audits/arduino_static_audit.md`
- `audits/preserve_audit.md`
- `audits/scope_audit.md`

## Excluded From Public Main

Older experiments, non-firmware side projects, runtime dumps, databases, raw logs, archives, and unrelated research notes are not part of this public main branch.
