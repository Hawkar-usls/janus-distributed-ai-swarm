# xTech|Search 10 — Hardware Manifest

This manifest records the xTech-facing hardware view of the existing JANUS Resilient Edge Swarm. It does not require modification of the underlying firmware for the submission package.

## Physical swarm count

**Current physical JANUS swarm: 10 nodes**, as reported by the developer.

The exact board/model, JANUS identity, firmware path, flashed commit, sensors/peripherals, and power source for each of the ten physical devices will be frozen from the actual hardware before final submission. Until that capture is completed, this document does not invent a one-to-one mapping for devices that have not yet been physically identified in the submission record.

For the full physical-freeze protocol, see [`TEN_NODE_PHYSICAL_SYSTEM.md`](TEN_NODE_PHYSICAL_SYSTEM.md).

## Why the physical freeze matters

The public repository contains more firmware roles than the current physical count because it preserves active experiments, compatibility images, migration targets, and specialist lineages. Therefore repository file count is not used as evidence of how many devices are currently powered.

A quantitative result is valid only for the exact hardware/software configuration recorded in its manifest. If a board, sensor, firmware commit, library version, radio configuration, or power source changes materially, the test configuration must receive a new manifest version.

## Ten-node submission slots

These identifiers reserve the physical fleet without guessing its exact mapping:

| Physical ID | JANUS node name | Board/model | Primary role | Firmware path | Flashed commit | Peripherals | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| X10-P01 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P02 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P03 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P04 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P05 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P06 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P07 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P08 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P09 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |
| X10-P10 | TBD | TBD | TBD | TBD | TBD | TBD | PENDING |

No row should be filled from memory alone. Each row is completed only when the physical device is inspected.

## First reproducible demonstration subset

The following four/five-node subset is chosen because it makes the sensing / visibility / loss / recovery story easy to evaluate. It is **not** the total swarm and may be adjusted after the ten physical devices are frozen.

| ID | Role | Hardware / class | Existing firmware | Purpose in demonstration |
| --- | --- | --- | --- | --- |
| X10-D1 | Operator / telemetry surface | M5Stack Core2 | `firmware/core2/CORE2.ino` | Show live swarm state, peer visibility, selected sensor/telemetry state, and loss/recovery behavior |
| X10-D2 | Physical sensing | AtomS3R-class + STHS34PF80 TMOS/PIR | `firmware/blind_eye/BLIND_EYE.ino` | Demonstrate real physical sensing with current-vs-memory/prediction separation |
| X10-D3 | RF/recovery reference | ESP32-S3 class | `firmware/anchor/Anchor.ino` | Exercise heartbeat, radio visibility, disconnect/reconnect, and rejoin behavior |
| X10-D4 | Heterogeneous peer | Physically verified peer selected from the ten-node fleet | exact path frozen at test time | Demonstrate that the system spans non-identical node roles and payloads |
| X10-D5 | Optional coordinator/workload role | Buzz-class ESP32-S3 if physically present in frozen ten | `firmware/buzz/Buzz.ino` | Exercise coordinator/path-loss behavior and concurrent constrained-device workload |

`ATOM SWARM TRON` remains a good repository example of a heterogeneous Atom-class peer, but the demo should use whatever corresponding physical device is actually present and verified in the current ten-node system.

## Repository role inventory relevant to xTech

Current active firmware in `PROJECT_MAP.md` includes Core2, Buzz, BH/BlackStar, ADV Elite, Yaks Gate, Anchor, Gladius, Golcron, Zim, Blind Eye, Pyramid, PEA4, PEA4 shell, and the P4 dual-swarm core. Compatibility/preserve firmware includes Beacon A1, Stick, ATOM SWARM TRON, and Slick.

This shows architectural breadth, not simultaneous physical deployment.

## Required physical evidence before final paper

For every physical node used in a quantitative claim, capture:

- dated photo with the device powered;
- visible or serial-reported JANUS node identity;
- board/model identification;
- exact firmware source path;
- exact repository commit flashed;
- Arduino/PlatformIO board package version;
- important library versions where practical;
- attached sensors/peripherals actually present;
- power source and approximate operating mode;
- radio mode/channel/configuration class, without exposing credentials;
- one short heartbeat/health receipt showing the node joined the evaluation configuration.

For the entire ten-node fleet, capture at least one dated group image or continuous video showing the devices together.

## Configuration safety

Do not commit:

- private Wi-Fi credentials;
- API keys/tokens;
- wallet identifiers used in personal builds;
- private NAS endpoints;
- personal IP addresses or network topology that does not need to be public;
- raw credentials embedded in historical firmware snapshots.

Use placeholder/example configuration in the public branch.

The legacy `OldLastSwarm.rar` is useful as historical provenance, but it should **not** be added to this public submission branch as-is because historical firmware may contain local/private configuration remnants.

## Hardware limitations that must remain visible

The current ESP32/M5Stack-class devices are excellent low-cost evaluation hardware, but the submission does not claim that they are:

- ruggedized military devices;
- MIL-STD-qualified;
- waterproof or shock/vibration qualified as a complete system;
- protected tactical radios;
- certified secure endpoints;
- production-ready enclosures;
- optimized for maximum battery endurance.

If the architecture proves useful, a later integration path may port the same role, state, freshness, and recovery concepts to more appropriate hardware. That future port is not presented as already completed.

## Power and thermal evidence

Because JANUS is heterogeneous, one power number would be misleading. Displays, audio, sensors, SD access, radio traffic, and local computation create different budgets by role.

Where instrumentation is available, record for each tested role:

- supply voltage;
- average current in baseline state;
- peak current during reconnect/rejoin;
- approximate power in active and degraded states;
- device/MCU temperature where accessible;
- minimum free heap during the same run.

These values become claims only after measurement.

## Scope statement

This manifest is a review/evidence description of the already existing system, not a new hardware design. The underlying repository remains authoritative for firmware, and the physically captured ten-node manifest will become authoritative for the exact devices used in the xTech test series.