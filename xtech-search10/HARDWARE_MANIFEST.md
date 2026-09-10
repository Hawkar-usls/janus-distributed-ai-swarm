# xTech|Search 10 — Hardware Manifest

This manifest records the xTech-facing hardware view of the existing JANUS Resilient Edge Swarm. It does not require modification of the underlying firmware for the submission package.

## Physical swarm count

**Current physical JANUS swarm: 10 nodes**, as reported by the developer.

The exact board/model, firmware path and flashed commit for each of the ten physical devices will be frozen from the hardware before final submission. Until that capture is completed, this document does not invent a one-to-one mapping for devices that have not yet been physically identified in the submission record.

## First reproducible demonstration subset

The following four-node subset is chosen because it makes the sensing / visibility / loss / recovery story easy to evaluate. It is **not** the total swarm.

| ID | Role | Hardware / class | Existing firmware | Purpose in demonstration |
| --- | --- | --- | --- | --- |
| X10-N1 | Operator / telemetry surface | M5Stack Core2 | `firmware/core2/CORE2.ino` | Show live swarm state, peer visibility, selected sensor/telemetry state and loss/recovery behavior |
| X10-N2 | Physical sensing | AtomS3R-class + STHS34PF80 TMOS/PIR | `firmware/blind_eye/BLIND_EYE.ino` | Demonstrate real physical sensing with current-vs-memory/prediction separation |
| X10-N3 | RF anchor / resilience reference | ESP32-S3 class | `firmware/anchor/Anchor.ino` | Exercise heartbeat, radio visibility, disconnect/reconnect and rejoin behavior |
| X10-N4 | Heterogeneous edge node | Atom-class ESP32 | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` | Demonstrate that the mesh spans non-identical node roles and payloads |

## Additional existing roles available to the ten-node physical swarm / repository lineage

| Node / lineage | Existing firmware | Review relevance |
| --- | --- | --- |
| Buzz | `firmware/buzz/Buzz.ino` | Coordinator/workload and multimedia coexistence/recovery lineage |
| Zim Geek | `firmware/zim_geek/Zim.ino` | Autonomous-specialist lineage and persistent local state |
| PEA4 / P4 dual core | `firmware/p4_dual_swarm_core/JANUS_P4_DUAL_SWARM_CORE_v1_1.ino` | Compute / mirror / verification extension |
| Pyramid / Beacon / Gladius / Golcron / ADV Elite / other current firmware | See `PROJECT_MAP.md` | Existing repository lineage and optional extension roles; repository firmware count is not treated as the physical-node count |

## Physical-evidence checklist before final white paper

The repository documents the firmware and architecture, but the final submission should add a compact physical evidence package for the exact ten-node swarm and for the subset used in the xTech demonstration:

- dated group photo of all ten physical JANUS nodes;
- board/model identification for each physical node;
- stable physical ID / label for each node;
- exact firmware file + commit hash flashed to each node;
- library / board package versions when practical;
- power source used during test;
- radio mode/channel configuration used during test;
- attached sensors/peripherals for each relevant node;
- a short video or log showing normal state, induced loss, stale/lost state, continued operation of unaffected nodes, rejoin and restored state.

## Configuration safety

Do not commit private Wi-Fi credentials, API keys, wallet identifiers, private NAS endpoints or other secrets to this xTech branch. Use the repository's placeholder/example configuration policy.

The legacy `OldLastSwarm.rar` supplied during preparation is useful as historical provenance, but it should **not** be added to this public submission branch as-is because historical firmware snapshots may contain local credentials or private configuration remnants.

## Scope statement

This manifest is a review/demo presentation of the already existing system, not a new hardware design. The underlying JANUS repository remains authoritative for the actual firmware, and the physical inventory capture remains authoritative for the exact ten devices present in the current swarm.
