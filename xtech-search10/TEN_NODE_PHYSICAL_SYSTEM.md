# JANUS Resilient Edge Swarm — Ten-Node Physical System

## Why this document exists

The developer reports a **10-node physical JANUS swarm**. The public repository contains more firmware roles than that physical count because it also preserves active experiments, compatibility images, migration targets, and specialist lineages.

Therefore:

```text
REPOSITORY ROLE COUNT != CURRENT PHYSICAL NODE COUNT
```

This document prevents two opposite mistakes:

- understating JANUS as only the four-node reviewer demo; and
- inventing a one-to-one mapping between every repository firmware role and the ten physical devices before that mapping is verified at the hardware.

## Current statement that may be used

> JANUS is a developer-operated heterogeneous physical swarm currently reported as ten nodes, built from ESP32/M5Stack-class devices with different sensing, display, telemetry, compute, and coordinator roles. The exact ten-device submission manifest will be frozen from the powered hardware before any quantitative xTech claim is finalized.

This wording is intentionally precise: **ten physical nodes is an owner-reported fact awaiting submission evidence capture**, while the firmware architecture and repository roles are directly inspectable in GitHub.

## Public firmware surface relevant to the physical system

The current `PROJECT_MAP.md` lists active firmware for:

- Core2
- Buzz
- BH / BlackStar
- ADV Elite
- Yaks Gate
- Anchor
- Gladius
- Golcron
- Zim
- Blind Eye
- Pyramid
- PEA4 / P4 tracks

and compatibility/preserve firmware including:

- Beacon A1
- Stick
- ATOM SWARM TRON
- Slick

Not all of these names should be presented as simultaneously powered physical nodes unless the exact hardware is observed and recorded.

## Physical freeze protocol

Before the final white paper makes any statement stronger than `developer-reported ten-node physical swarm`, each device should be assigned one immutable xTech test identifier:

| Test ID | JANUS node name | Board/model | Primary role | Firmware path | Flashed commit | Attached sensors/peripherals | Power source | Evidence status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| X10-P01 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P02 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P03 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P04 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P05 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P06 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P07 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P08 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P09 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |
| X10-P10 | TBD from device | TBD | TBD | TBD | TBD | TBD | TBD | PENDING CAPTURE |

The table is deliberately not pre-filled from memory. A device should enter it only after the physical unit is identified.

## Minimum evidence per physical node

For each `X10-Pxx` device, capture:

1. one dated photo showing the device powered;
2. board/model identification;
3. JANUS node identity displayed or reported over serial/telemetry;
4. exact firmware source path;
5. repository commit SHA used for the flash;
6. Arduino/PlatformIO board package and important library versions where practical;
7. radio channel/configuration class without publishing private credentials;
8. attached sensors, screen, audio, LoRa/GNSS, motion base, SD, or other peripherals actually present;
9. power source during evaluation;
10. one short health/heartbeat log proving that the device joined the test configuration.

## Full-system role model

The ten-node system is best explained as a **team of specialists**, not as ten copies of one board.

Possible role classes already represented in the repository include:

| Role class | Repository examples | What the class contributes |
| --- | --- | --- |
| Operator / state visualization | Core2, PEA4 | human-facing peer, sensor, and status visibility |
| Physical sensing | Blind Eye, Core2 local sensor paths, ADV Elite organs | local environmental/presence/telemetry observations |
| Coordinator / workload arbiter | Buzz | selected shared work, network state, recovery lineage |
| RF/reference/recovery | Anchor, Gladius | peer visibility, recovery, experimental radio behavior |
| Mobile/gate specialist | Yaks Gate, Stick | mobile/gate-specific local mission and signaling |
| Autonomous local specialist | Zim, ADV Elite, Golcron | protected local mission with bounded shared work |
| Observer / study node | BH / BlackStar | local observation and telemetry specialization |
| Visual/preserve node | Pyramid, Beacon | stable lineage, display, fallback/preserve role |
| Compute/verification extension | PEA4 / P4 tracks | larger edge compute, mirror/verify/observer roles |

This table explains the architecture; it is **not** a declaration that every listed example is one of the physical ten.

## Reviewer demonstrator vs. full physical swarm

The shortest reviewer demonstration remains intentionally smaller:

```text
4-5 NODE DEMONSTRATOR
= easy to understand and film

10-NODE PHYSICAL SWARM
= actual system-level evaluation target
```

The recommended quick subset is Core2 + Blind Eye + Anchor + one heterogeneous peer such as ATOM SWARM TRON, with Buzz when coordinator behavior is useful. That subset demonstrates the principle without asking a reviewer to understand every JANUS role in the first minute.

The full ten-node test then asks the harder questions:

- how many peers remain visible under one or more losses;
- whether unrelated local roles continue;
- whether recovery traffic creates secondary failures;
- whether identities remain stable after rejoin;
- whether stale state is bounded across a larger peer set;
- whether heterogeneous firmware versions preserve the agreed packet/ABI contract;
- what happens when coordinator, sensor, observer, and peripheral-heavy nodes fail in different orders.

## What ten nodes does not prove

A ten-node physical system does **not** by itself prove:

- arbitrary scalability;
- tactical-range communications;
- anti-jam behavior;
- secure military networking;
- production reliability;
- autonomous mission authority;
- commercial readiness;
- performance at 50, 100, or 1,000 nodes.

The correct claim is stronger because it is narrower: **there is a non-trivial heterogeneous physical fleet large enough to support controlled multi-node resilience experiments without pretending that prototype scale equals operational scale.**

## Final freeze rule

Once all ten rows are physically captured, create a machine-readable `TEN_NODE_TEST_MANIFEST.json` and never silently alter it for the associated test series. Any hardware, firmware, sensor, or configuration change starts a new manifest version.