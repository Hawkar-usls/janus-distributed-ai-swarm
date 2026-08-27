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
- `firmware/blind_eye/BLIND_EYE.ino` — Blind Eye v2.15A TMOS-primary, camera-absent truth sensor, RF fusion and event aperture
- `firmware/pyramid/ATOM_MATRIX_Pyramid.ino`
- `firmware/pea4/PEA4.ino`
- `firmware/PEA4_JANUS_SHELL/PEA4_JANUS_SHELL.ino`
- `firmware/p4_dual_swarm_core/JANUS_P4_DUAL_SWARM_CORE_v1_1.ino`

The Anchor, Golcron and Blind Eye folders are multi-tab Arduino sketches. Open
their primary `.ino`; Arduino compiles the numbered tabs in the same folder
together.

## Compatibility / Preserve Firmware

- `firmware/beacon/Beacon_A1.ino`
- `firmware/esp32_swarm/Stick.ino`
- `firmware/esp32_swarm/ATOM_SWARM_TRON.ino`
- `firmware/legacy/Slick.ino`

## Current Documentation

- `README.md`: current public swarm overview.
- `docs/current-swarm-state.md`: current node roles and integration status.
- `docs/swarm-critical-rules.md`: critical sketch rules for future edits.
- `docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`: mandatory `PRIMARY_MISSION + BOUNDED_SIDE_QUESTS` doctrine derived from the Zim experiment.
- `docs/JANUS_AUTONOMOUS_SPECIALIST_DOCTRINE_2026-08-27.json`: machine-readable specialist doctrine.
- `docs/ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`: ADV_Elite adoption contract for Zim's generalizable strengths.
- `.github/PULL_REQUEST_TEMPLATE.md`: review checklist that requires mission/budget/truth/learning/acceleration/persistence checks.
- `docs/current/`: copied latest working notes from the private LastSwarm lab.
- `docs/architecture.md`: architecture overview and specialist-node model.
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
