# JANUS Distributed AI Swarm

Public firmware surface for the current JANUS ESP32 / M5Stack swarm.

This repository tracks the active swarm shape as of August 2026: Core2 station,
Buzz pool master, BlackHole/BH observer, Cardputer ADV Elite, Yaks Gate,
Anchor/Gladius brothers, Golcron/Holocron astrolabe, Zim, Blind Eye, Pyramid,
legacy body nodes, and the PEA4 ESP32-P4 Titan bring-up.

The repo intentionally stores source sketches, docs, examples and review notes.
It does not store private credentials, raw runtime dumps, stock firmware images,
large vendor archives, databases, or model checkpoints.

## Current Swarm Entrypoints

| component | path | current role |
| --- | --- | --- |
| Core2 / Swarm Station | `firmware/core2/CORE2.ino` | Galaxy station, black-hole lab, swarm telemetry, SD corpus, slow miner study |
| Buzz | `firmware/buzz/Buzz.ino` | Stratum/pool master, swarm arbiter, audio/UI node, Brother Arena judge |
| BH / BlackStar | `firmware/blackhole_bh/ATOM_BH.ino` | Black-hole visual/miner observer, silicon trace/lens model, BH study target |
| ADV Elite | `firmware/adv_elite/ADV_Elite.ino` | Cardputer ADV Elite game node, Beacon tab, LoRa/GNSS sky-anchor, SD miner corpus |
| Yaks Gate | `firmware/yaks_gate/` | StickS3 gate pilot, IR local beacon, Murph/maze/BlackStar escape telemetry |
| Anchor | `firmware/anchor/Anchor.ino` | v1.20 stable brother miner: Buzz S/2 shares, callback-safe ESP-NOW queue, Gladius twin split and RF Dome |
| Gladius | `firmware/gladius/Gladius.ino` | Experimental brother miner, TailGEX/bandit method search, direct Buzz protection |
| Golcron / Holocron | `firmware/golcron/Golcron.ino` | TTGO T-Display adaptive unseen-path S/2 worker with autonomous Star Forge learning and BH pixel cosmos |
| Zim Geek | `firmware/zim_geek/Zim.ino` | Solo/worker scout, Stratum-aware explorer, Tranception-lite candidate node |
| Blind Eye | `firmware/blind_eye/BLIND_EYE.ino` | v2.15A AtomS3R TMOS-primary camera-absent eye: RF/IMU/mic fusion, 900 ms truth hold, synthetic E/F aperture, Blackboard/Kenshi/Tachyon and Buzz worker |
| Pyramid | `firmware/pyramid/ATOM_MATRIX_Pyramid.ino` | Stable visual/swarm pyramid node |
| Beacon A1 | `firmware/beacon/Beacon_A1.ino` | Legacy Beacon preserve candidate and launcher fallback |
| Legacy bodies | `firmware/esp32_swarm/`, `firmware/legacy/` | Older Stick/TRON/Slick bodies retained for compatibility and review |
| PEA4 Pool Probe | `firmware/pea4/PEA4.ino` | ESP32-P4 Titan observer/probe with P/N memory, no share pressure |
| PEA4 Janus Shell | `firmware/PEA4_JANUS_SHELL/` | ESP32-P4 stock-like Janus shell v0.1 with ST7701/GT911 UI bring-up |
| P4 Dual Swarm Core | `firmware/p4_dual_swarm_core/` | Full P4_A/P4_B/P4_C compute/protocol core with JP4 frames and NVS memory |

## Swarm Rules

- Buzz remains the Stratum and pool authority.
- ESP-NOW packet layouts are treated as ABI unless a migration is explicit.
- Nodes must keep sensor truth separate from prediction, memory and UI fiction.
- Adaptive mining may choose nonce order, lane, stride, sector or batch pressure,
  but must not change SHA256, target math, pool difficulty or submit semantics.
- Observer-only nodes may record near-tails, stale tails, rejected tails and
  silicon/heat/load traces, but must not increase pool submit pressure.
- Swarm visibility must include recovery paths: direct Buzz heartbeat/share,
  peer rebuild, channel reassertion, stale-node TTL and no-permanent-disappear
  behavior.
- IR on Stick-class devices is local optical signaling only. LoRa/GNSS sky-anchor
  behavior belongs to ADV/LoRa-capable hardware, not Stick IR.
- NAS Brain may act as a library/Archivarius layer, not as a required hard
  dependency for firmware loops.

## P4 / PEA4 Role

PEA4 is not just another miner. It is the future large terminal and Titan node:

- dashboard and visual shell
- camera/sensor preprocessing
- SD/FATFS corpus and archive surface
- NAS edge/library bridge
- P/N Cortex observer
- future Brother Arena and swarm state monitor

Current PEA4 firmwares are observer-first. They compute and visualize, but do
not mutate target math and do not submit shares unless explicitly promoted later.

P4 tracks are kept separate:

- `firmware/PEA4_JANUS_SHELL/`: screen/touch shell.
- `firmware/p4_dual_swarm_core/`: compute/protocol core.

They should be tested independently before a future merged
`PEA4_TITAN_SHELL_CORE`.

## Repository Layout

- `firmware/`: active and compatibility sketches. Anchor, Golcron and Blind Eye
  are stored as ordered Arduino tabs. Anchor and Golcron can also be assembled
  into IDE-ready `.ino` files by `tools/assemble_swarm_sketches.py`.
- `tools/assemble_swarm_sketches.py`: creates `build/Anchor/Anchor.ino` and
  `build/Golcron/Golcron.ino`; CI compiles those exact outputs.
- `docs/`: architecture, technical rules and current swarm notes.
- `docs/current/`: latest LastSwarm working notes copied from the private lab.
- `audits/`: previous public inventory/static audit reports.
- `configs/examples/`: placeholder config and `secrets.example.h`.
- `tests/static_checks/`: lightweight secret marker check.

## Quick Start

1. Open the primary `.ino` for multi-tab nodes such as Blind Eye. For Anchor or
   Golcron, you may also run `python tools/assemble_swarm_sketches.py` and open
   the generated sketch under `build/`.
2. Use the board settings documented next to that firmware.
3. Keep local Wi-Fi, wallet, API and NAS values in ignored files.
4. Flash one node at a time and verify serial logs before moving to the next.
5. After mixed-generation updates, check Buzz and Core2 visibility before
   assuming a node is healthy.

## Review Path

Start here:

- `PROJECT_MAP.md`
- `docs/current-swarm-state.md`
- `docs/swarm-critical-rules.md`
- `docs/architecture.md`
- `docs/technical-boundaries.md`
- `M5STACK_REVIEW_GUIDE.md`

## Safety

No private keys, Wi-Fi passwords, tokens, raw databases, stock binary backups or
large telemetry dumps are intended to be committed. Mining-related firmware
paths are experimental protocol/telemetry work and require careful network,
electrical and thermal review before long unattended runs.

## License

Released under the MIT License for the code authored in this repository. Confirm
third-party library and hardware-vendor license requirements before production
or redistribution, especially for vendor display/touch/camera sources.
