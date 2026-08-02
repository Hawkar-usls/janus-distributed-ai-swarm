# Project Map

This repository is a focused public surface for the current JANUS distributed
AI/swarm firmware drop.

## Active Firmware

- `firmware/core2/CORE2.ino`
- `firmware/buzz/Buzz.ino`
- `firmware/blackhole_bh/ATOM_BH.ino`
- `firmware/adv_elite/ADV_Elite.ino`
- `firmware/yaks_gate/`
- `firmware/anchor/Anchor.ino` — Anchor v1.20 S/2 safe-queue stable brother
- `firmware/gladius/Gladius.ino`
- `firmware/golcron/Golcron.ino` — Golcron v1.5 adaptive unseen-path worker and BH pixel cosmos charm
- `firmware/zim_geek/Zim.ino`
- `firmware/blind_eye/BLIND_EYE.ino`
- `firmware/pyramid/ATOM_MATRIX_Pyramid.ino`
- `firmware/pea4/PEA4.ino`
- `firmware/PEA4_JANUS_SHELL/PEA4_JANUS_SHELL.ino`
- `firmware/p4_dual_swarm_core/JANUS_P4_DUAL_SWARM_CORE_v1_1.ino`

The Anchor and Golcron folders are multi-tab Arduino sketches. Open their
primary `.ino`; Arduino compiles the numbered tabs in the same folder together.

## Compatibility / Preserve Firmware

- `firmware/beacon/Beacon_A1.ino`
- `firmware/esp32_swarm/Stick.ino`
- `firmware/esp32_swarm/ATOM_SWARM_TRON.ino`
- `firmware/legacy/Slick.ino`

## Current Documentation

- `README.md`: current public swarm overview.
- `docs/current-swarm-state.md`: current node roles and integration status.
- `docs/swarm-critical-rules.md`: critical sketch rules for future edits.
- `docs/current/`: copied latest working notes from the private LastSwarm lab.
- `docs/architecture.md`: older architecture overview retained for review.
- `docs/technical-boundaries.md`: protocol and validation boundaries.
- `docs/esp-now-swarm.md`: ESP-NOW compatibility notes.
- `M5STACK_REVIEW_GUIDE.md`: shortest external review path.

## Audits

- `audits/file_inventory.md`
- `audits/arduino_static_audit.md`
- `audits/preserve_audit.md`
- `audits/scope_audit.md`

## Excluded From Public Main

The public branch excludes stock binary backups, vendor zip archives, raw logs,
NAS databases, API keys, Wi-Fi credentials, local wallet secrets, large model
files, and historical scratch folders.
