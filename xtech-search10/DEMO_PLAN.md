# xTech|Search 10 — Controlled Demonstration Plan

## Demonstration objective

Show, on existing JANUS hardware and firmware, that a small heterogeneous edge network can make peer state visible, detect loss/staleness, preserve unaffected local roles, and expose recovery/rejoin when a node or communication path returns.

This is a **measurement/demo plan**, not a claim that all outcomes have already been achieved quantitatively.

## Preferred setup

- Core2 — operator and telemetry surface
- Blind Eye — physical TMOS/PIR sensing node
- Anchor — RF anchor / recovery-oriented node
- ATOM SWARM TRON — heterogeneous peer
- Buzz — optional coordinator/workload node

All devices should be flashed from one frozen repository commit and documented in the test receipt.

## Demo sequence

### Stage A — Normal operation

Power all selected nodes and confirm:

- stable node identities are visible;
- heartbeat / telemetry traffic is present where supported;
- Blind Eye publishes real sensor-derived state;
- the operator surface shows current peers without treating stale/predicted information as current sensor truth.

Record a baseline window before introducing disruption.

### Stage B — Single-node loss

Physically power down or isolate one non-operator node. Do not modify firmware during the run.

Observe and timestamp:

- last valid packet/heartbeat;
- when the node becomes absent/stale according to the relevant firmware path;
- whether unaffected nodes continue their local primary role;
- whether the UI/telemetry surface distinguishes the missing node from a healthy one;
- whether stale state is prevented from being presented indefinitely as current truth.

### Stage C — Coordinator/path disruption

Where the selected configuration supports it, interrupt the relevant Wi-Fi/coordinator path while leaving local ESP32 nodes powered.

Observe:

- which functions remain local;
- which functions degrade or defer;
- whether optional work yields before the protected primary mission;
- whether recovery attempts remain visible rather than silently masking the outage.

### Stage D — Rejoin

Restore the removed node or communication path.

Record:

- first radio/peer detection;
- first accepted heartbeat/telemetry packet;
- time until the node returns to the healthy peer set;
- whether identity/state continuity is preserved;
- any duplicate, stale or conflicting state during recovery.

## Metrics to collect

| Metric | Definition | Claim status before test |
| --- | --- | --- |
| `T_detect_loss` | Time from last valid peer packet to loss/stale indication | NOT YET MEASURED |
| `T_rejoin` | Time from restored power/path to recognized healthy peer state | NOT YET MEASURED |
| `packet_rx_rate` | Received valid packets / expected observation window | NOT YET MEASURED |
| `stale_state_events` | Count of stale state incorrectly shown/used as current | TARGET = 0; NOT YET VERIFIED |
| `primary_mission_continuity` | Whether unaffected node's declared primary function continues during peer/coordinator loss | TO BE TESTED |
| `heap_min` | Minimum free heap on selected devices during run | TO BE TESTED |
| `device_temperature` | Available device/MCU temperature telemetry during run | TO BE TESTED WHERE SUPPORTED |
| `power_draw` | Per-node or system power during representative states | TO BE MEASURED IF INSTRUMENTATION AVAILABLE |

## Evidence package

A useful xTech evidence bundle should contain:

1. `TEST_MANIFEST.json` — exact firmware commit, boards, versions and configuration class;
2. timestamped serial/telemetry logs;
3. short continuous video of baseline -> induced loss -> stale/lost indication -> rejoin;
4. a compact CSV of event timestamps;
5. one-page result summary containing both successes and failures.

## Failure handling

A failed test is evidence, not something to hide. If a node fails to rejoin, the submission should report the observed failure mode and make the Phase I work plan about reproducing, measuring and fixing that specific limitation.

## Phase I extension if selected

Without inventing a new product, a six-month Phase I could turn the existing system into a controlled evaluation package by:

- freezing a reproducible hardware/software configuration;
- quantifying loss detection and recovery across repeated trials;
- adding controlled packet-loss / RF-degradation test conditions;
- measuring resource and power cost of resilience mechanisms;
- validating stale-state and sensor-truth behavior;
- documenting interfaces required for later Army/customer integration.

This plan intentionally avoids claims of anti-jam performance, secure tactical networking, battlefield readiness or operational C2 authority unless a later authorized test actually establishes them.
