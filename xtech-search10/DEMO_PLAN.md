# xTech|Search 10 — Controlled Demonstration and Evaluation Plan

## Objective

Use the **existing JANUS hardware and firmware** to measure how a heterogeneous edge swarm behaves when peers, coordinators, or communication paths become unavailable and later return.

This is a **test plan**, not a statement that every desired outcome has already been achieved.

The developer reports a ten-node physical JANUS swarm. The xTech evaluation is therefore split into two levels:

1. **Quick reviewer demonstrator** — a small four/five-node subset that makes the system easy to understand on video.
2. **Full ten-node evaluation** — the actual system-level stress series after all ten physical devices are frozen into a manifest.

## Test philosophy

The purpose is not to manufacture a perfect demonstration. The purpose is to make success and failure equally observable.

```text
NO FIRMWARE CHANGE DURING A RUN
NO SILENT DEVICE SUBSTITUTION
NO BEST-RUN-ONLY REPORTING
NO TIMEOUT CONSTANT -> PERFORMANCE CLAIM
NO FAILURE DELETION
```

Every quantitative result must identify the exact hardware/software manifest that produced it.

---

## Phase 0 — Freeze the configuration

Before measuring anything:

- assign physical IDs `X10-P01` through `X10-P10`;
- identify board/model and JANUS node name for every physical device;
- record exact firmware path and flashed repository commit;
- record relevant board-support/library versions;
- record attached sensors/peripherals;
- record radio channel/configuration class without exposing credentials;
- record power source;
- photograph all ten powered nodes together;
- produce `TEN_NODE_TEST_MANIFEST.json`;
- hash the manifest and preserve it with the test results.

If the configuration changes materially, begin a new manifest version.

---

## Level 1 — Quick reviewer demonstrator

### Recommended subset

The exact subset should be selected from the physically verified ten. The preferred roles are:

- **Core2** — operator and telemetry surface;
- **Blind Eye** — physical TMOS/PIR sensing specialist;
- **Anchor** — RF/recovery reference;
- **one additional heterogeneous peer** from the frozen physical fleet;
- **Buzz**, optional, when coordinator/path-loss behavior is useful.

The demo is not evidence that JANUS has only four/five nodes. It is a short explanatory slice through the larger system.

### Stage A — Healthy baseline

Power all selected devices and record a baseline window long enough to show normal heartbeat/telemetry behavior.

Confirm and timestamp where supported:

- stable node identity;
- peer/heartbeat visibility;
- current sensor-derived state from Blind Eye or another physical sensor role;
- operator display of healthy peers;
- memory/prediction/stale state not being presented as current physical truth;
- free heap / temperature / power instrumentation if available.

### Stage B — Single non-operator node loss

Physically power down or isolate one selected peer. Do not reflash or alter the remaining nodes.

Measure:

- `t_last_valid_packet`;
- `t_stale_or_lost_indication`;
- whether the missing identity remains distinguishable from healthy peers;
- whether unaffected nodes continue their local roles;
- whether the operator surface avoids indefinite stale-as-current presentation;
- whether recovery/watchdog traffic causes visible secondary degradation.

### Stage C — Coordinator or network-path disruption

Where the selected configuration supports it, interrupt the relevant coordinator/Wi-Fi/external path while local devices remain powered.

Record which functions:

- continue locally;
- enter degraded mode;
- defer optional/shared work;
- stop entirely;
- recover automatically;
- require user intervention.

The result should explicitly list both preserved and lost functions.

### Stage D — Rejoin

Restore the removed node or path without changing firmware.

Measure:

- `t_restore`;
- first radio/peer detection;
- first accepted heartbeat/telemetry packet;
- time to healthy peer classification;
- identity continuity;
- duplicate/stale/conflicting state during rejoin;
- whether unrelated nodes experience a transient failure.

### Stage E — Repeat

One successful video is not enough. Run the same disruption multiple times and preserve all runs.

---

## Level 2 — Full ten-node evaluation

Once the physical fleet is frozen, repeat the same basic failure logic across different role types.

### Test family T1 — One-at-a-time node loss

For each physical node that can be safely powered down:

1. establish healthy baseline;
2. remove only that node;
3. record peer-health/stale behavior;
4. record continuity of the other nine;
5. restore the node;
6. record rejoin;
7. repeat for multiple trials.

This reveals whether failure behavior is role-dependent.

### Test family T2 — Coordinator loss

Remove the coordinator/workload authority where applicable while leaving specialist nodes powered.

Question:

> Do protected local roles continue, and are deferred/shared functions visibly distinguished from local mission state?

### Test family T3 — Operator-surface loss

Remove or reboot the main operator/display surface while peer nodes remain running.

Question:

> Does loss of the visualization surface stop the swarm itself, or only remove human visibility until the surface returns?

### Test family T4 — Sensor-role loss

Remove a physical sensing node.

Question:

> Is the missing sensor clearly represented as stale/absent rather than replaced by memory or inference?

### Test family T5 — Repeated power cycling

Cycle selected nodes repeatedly with a defined on/off schedule.

Measure:

- successful rejoins / attempts;
- identity continuity;
- duplicate peer entries;
- stale-state cleanup;
- time-to-healthy distribution.

### Test family T6 — Network impairment

Use controlled, legal, non-destructive conditions such as distance, attenuation, shielding, channel coexistence, or controlled Wi-Fi load to create a degraded 2.4 GHz environment.

This test is **not an anti-jam test**.

Measure:

- valid packet reception rate;
- loss/stale transitions;
- reconnect/rejoin attempts;
- recovery traffic volume where measurable;
- resource/thermal changes;
- whether degradation remains local or cascades.

### Test family T7 — Optional-service outage

Where firmware uses optional NAS/Wi-Fi/Stratum/other external services, interrupt those services while preserving the local network.

Record exactly what continues and what fails. The goal is to verify the protected-primary-mission doctrine, not assume it.

### Test family T8 — Concurrent workload stress

For nodes that legitimately combine radio with display, audio, sensing, storage, or compute, run representative normal workloads while performing loss/rejoin tests.

Question:

> Does recovery remain usable under realistic resource contention?

### Test family T9 — Version/ABI compatibility

Where safe and relevant, test approved combinations of current/compatibility firmware with recorded packet structure/version information.

Do not mix arbitrary images. Every compatibility pairing must be documented.

Measure:

- packet acceptance/rejection;
- malformed/unknown packet behavior;
- stale or duplicate peer creation;
- safe fallback behavior.

### Test family T10 — Full restart / cold recovery

Power down the complete test system, then restore it from a documented sequence.

Measure:

- node appearance order;
- time to stable peer view;
- identity preservation;
- state persistence where intended;
- any manual steps required.

---

## Metrics

| Metric | Definition | Status before test |
| --- | --- | --- |
| `T_detect_loss` | Last valid packet/heartbeat -> stale/lost indication | NOT YET MEASURED |
| `T_first_packet_after_restore` | Restore event -> first accepted peer packet | NOT YET MEASURED |
| `T_rejoin_healthy` | Restore event -> healthy peer classification | NOT YET MEASURED |
| `rejoin_success_rate` | Successful healthy rejoins / attempted rejoins | NOT YET MEASURED |
| `packet_rx_rate` | Valid received packets / expected observation interval under defined condition | NOT YET MEASURED |
| `stale_as_current_events` | Stale/remembered/inferred state incorrectly presented as current physical state | TARGET = 0; NOT VERIFIED |
| `primary_mission_continuity` | Named local function remains available during defined peer/coordinator outage | TO BE TESTED BY ROLE |
| `secondary_failure_count` | Other nodes/functions unexpectedly fail during one induced fault | NOT YET MEASURED |
| `identity_conflict_events` | Duplicate/conflicting peer identity after restart/rejoin | TARGET = 0; NOT VERIFIED |
| `heap_min` | Minimum free heap during run | TO BE MEASURED |
| `temperature` | Device/MCU temperature where accessible | TO BE MEASURED |
| `power_idle` | Per-node power in baseline idle/normal state | TO BE MEASURED WHERE INSTRUMENTED |
| `power_recovery_peak` | Peak/representative power during reconnect/rejoin | TO BE MEASURED WHERE INSTRUMENTED |
| `manual_intervention_count` | User actions required to recover after induced fault | NOT YET MEASURED |

## Report distributions, not hero numbers

For timing metrics, report at least:

- number of trials `n`;
- median;
- minimum;
- maximum;
- failure count;
- conditions used.

If enough trials exist, add percentiles. Never report only the fastest run.

---

## Evidence package

Each test series should produce:

1. `TEN_NODE_TEST_MANIFEST.json` or subset manifest;
2. SHA-256 of the manifest and relevant raw logs;
3. timestamped serial/telemetry logs;
4. continuous video of representative runs;
5. event CSV with key timestamps;
6. result summary with both successes and failures;
7. anomaly/failure register;
8. exact source commit links;
9. notes describing any manual intervention;
10. independent replay receipt when a second evaluator becomes available.

## Suggested event CSV

```text
test_id,trial,node,event,timestamp_ms,value,notes
T1,01,X10-P03,last_valid_packet,...,...,...
T1,01,X10-P03,stale_indicated,...,...,...
T1,01,X10-P03,power_restored,...,...,...
T1,01,X10-P03,first_packet,...,...,...
T1,01,X10-P03,healthy_again,...,...,...
```

---

## Pass/fail philosophy

No global `PASS` should be issued merely because one node rejoins once.

Instead, each claim receives its own scoped status, for example:

```text
ANCHOR_REJOIN_UNDER_TEST_CONDITION_A = PASS (9/10)
STALE_AS_CURRENT = FAIL (1 observed event)
COORDINATOR_LOSS_LOCAL_SENSOR_CONTINUITY = PASS (10/10)
POWER_BUDGET = OPEN (instrumentation unavailable)
```

This preserves useful partial evidence without hiding failures.

## Failure handling

A failed run is evidence. Do not delete it, rerun until success, and report only the final good attempt.

For every meaningful failure record:

- exact manifest;
- trigger;
- observed behavior;
- whether failure was local or cascading;
- whether automatic recovery occurred;
- manual intervention required;
- whether the failure was reproduced;
- proposed engineering interpretation.

Interpretation must remain separate from the raw observation.

## Phase I value if selected

Without inventing a new product, a Phase I effort could convert the current prototype into a controlled evaluation package by:

- freezing the exact ten-node reference system;
- quantifying role-specific failure/recovery behavior;
- characterizing 2.4 GHz impairment without mislabeling it anti-jam testing;
- measuring memory, thermal, and power cost of resilience;
- validating freshness/stale-state semantics;
- documenting packet/ABI and transport boundaries;
- identifying which functions are transport-independent enough to migrate to other radios/hardware;
- preserving a transparent failure register;
- producing a customer-readable integration and replication package.

## Explicit exclusions

This plan does not claim or attempt to imply:

- battlefield readiness;
- secure tactical networking certification;
- anti-jam capability;
- protected military RF performance;
- autonomous weapon authority;
- biometric identification;
- arbitrary scale;
- production reliability.

Those require different evidence and, where applicable, authorized test environments.