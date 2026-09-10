# xTech|Search 10 — Hardware Manifest

This manifest identifies the preferred **existing** hardware/firmware subset for a JANUS Resilient Edge Swarm demonstration. It does not require modification of the underlying firmware for the submission package.

## Primary demonstration set

| ID | Role | Hardware / class | Existing firmware | Purpose in demonstration |
| --- | --- | --- | --- | --- |
| X10-N1 | Operator / telemetry surface | M5Stack Core2 | `firmware/core2/CORE2.ino` | Show live swarm state, peer visibility, selected sensor/telemetry state and loss/recovery behavior |
| X10-N2 | Physical sensing | AtomS3R-class + STHS34PF80 TMOS/PIR | `firmware/blind_eye/BLIND_EYE.ino` | Demonstrate real physical sensing with current-vs-memory/prediction separation |
| X10-N3 | RF anchor / resilience reference | ESP32-S3 class | `firmware/anchor/Anchor.ino` | Exercise heartbeat, radio visibility, disconnect/reconnect and rejoin behavior |
| X10-N4 | Heterogeneous edge node | Atom-class ESP32 | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` | Demonstrate that the mesh spans non-identical node roles and payloads |

## Optional demonstration nodes

| Node | Existing firmware | Why optional |
| --- | --- | --- |
| Buzz | `firmware/buzz/Buzz.ino` | Useful for coordinator/workload and multimedia coexistence/recovery history, but not required to explain the core sensing/resilience concept |
| Zim Geek | `firmware/zim_geek/Zim.ino` | Strong autonomous-specialist lineage and persistent local state, but mining/game presentation can distract from the core xTech engineering story |
| PEA4 / P4 dual core | `firmware/p4_dual_swarm_core/JANUS_P4_DUAL_SWARM_CORE_v1_1.ino` | Useful as future compute/verification extension; not necessary for the simplest physical demo |
| Pyramid / Beacon / Gladius / Golcron / ADV Elite / other current nodes | See `PROJECT_MAP.md` | Preserve as project lineage and optional extensions rather than forcing the evaluator to understand the full swarm at once |

## Physical-evidence checklist before final white paper

The repository documents the firmware and architecture, but the final submission should add a compact physical evidence package for the exact devices used in the xTech demonstration:

- dated photo of all selected powered nodes together;
- board/model identification for each node;
- exact firmware file + commit hash flashed to each node;
- library / board package versions when practical;
- power source used during test;
- radio mode/channel configuration used during test;
- sensor attached to Blind Eye and its calibration/warm-up conditions;
- a short video or log showing normal state, induced loss, stale/lost state, rejoin, and restored state.

## Configuration safety

Do not commit private Wi-Fi credentials, API keys, wallet identifiers, private NAS endpoints or other secrets to this xTech branch. Use the repository's placeholder/example configuration policy.

The legacy `OldLastSwarm.rar` supplied during preparation is useful as historical provenance, but it should **not** be added to this public submission branch as-is because historical firmware snapshots may contain local credentials or private configuration remnants.

## Scope statement

This manifest is a review/demo selection, not a new hardware design. The underlying JANUS repository remains authoritative for the actual firmware.
