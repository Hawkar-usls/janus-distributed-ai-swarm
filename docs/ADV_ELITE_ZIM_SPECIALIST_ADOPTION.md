# ADV_Elite — Zim Specialist Adoption Contract

This file binds the best generalizable properties observed in ZimGeek into the future `ADV_Elite` architecture without copying Zim's mining-specific personality or treating SHA rarity as intelligence.

This is an architecture contract now; runtime implementation remains staged behind the current ADV_Elite forensic rebuild gates.

## ADV_Elite Primary Mission

`ADV_Elite` primary mission is:

```text
LOCAL_PREDICTIVE_OBSERVER_AND_TERMINAL
= WORLD + SWARM + SELF
+ truthful sensors
+ always-on anomaly attention
+ Witness/memory
+ local predictive model
+ trusted swarm participation
```

Full M2R remains manual behind `1488`.

The primary mission must remain functional without NAS, PC, GitHub gateway, Buzz or other peers.

## Bounded Side-Quests

Buzz/mining jobs, archive uploads, GitHub/HRain outbox delivery, optional heavy study and other coordinator work are side-quests unless explicitly promoted later by a separate user decision.

They must have measurable budgets and must not starve:
- real sensor acquisition;
- clock/freshness accounting;
- anomaly detector;
- Witness append path;
- ESP-NOW receive/health path;
- user controls;
- local predictive state.

A future implementation should expose side-quest pressure and defer/drop reason in telemetry.

## Quiet Canary Priority Ladder

Recommended ADV_Elite degradation order under pressure:

```text
P0  truth/safety/user controls/clock/sensors/anomaly/Witness
P1  local predictive cognition and lightweight swarm state
P2  essential swarm heartbeat / trusted peer exchange
P3  Buzz/miner/corpus side-work
P4  BrainWave/HELIOS richness and decorative UI work
P5  GitHub/HRain/archive uploads and nonessential background study
```

The single-panel Beacon HUD remains visible when possible, but rendering frequency may reduce before primary sensing/cognition is sacrificed.

## SELF_TESTED_ACCELERATION_ONLY in ADV_Elite

Use the Zim pattern for every future acceleration candidate:

```text
reference path
   vs
candidate fast path
   -> same frozen input
   -> equivalence test where semantics must match
   -> benchmark
   -> shadow/promotion decision
   -> periodic crosscheck
   -> automatic fallback on mismatch
```

Candidate areas:
- hardware vs software SHA implementation;
- M2R math kernels;
- Theta calculation kernels;
- sensor-fusion math;
- compression/serialization;
- local inference/fixed-point paths;
- SD/NVS storage fast paths.

No speedup may change sensor truth, SHA validity, packet ABI or user gates.

## Event Cadence Oracle — Borrowed From Zim NotifyOracle

Zim's useful `NotifyOracle` idea is not about predicting SHA. It learns the cadence of an external event stream and avoids starting expensive work immediately before the next likely event.

ADV_Elite should generalize this into a lightweight `EVENT_CADENCE_ORACLE` feeding its local attention/M2R context.

Candidate event streams:
- periodic swarm heartbeats;
- Buzz job/update cadence;
- GNSS fix cadence;
- sensor refresh cadence;
- gateway/outbox acknowledgement cadence;
- recurring local environment transitions when enough real history exists.

The oracle may predict **when a new observation is likely**, allowing resource scheduling such as delaying a low-priority batch or increasing sampling around an expected transition.

It must not claim access to future information. Forecasts are ordinary time-series estimates, timestamped before the event and scored afterward.

## White Raven Pattern — Tiny Local Learner

ADV_Elite should preserve a small local learner independently of heavy NAS/Habitat models.

Its role is bounded policy adaptation, not replacement of M2R or sensor truth.

Possible inputs:
- prediction error;
- anomaly rate;
- peer disagreement;
- resource pressure;
- queue/outbox pressure;
- sensor freshness;
- M2R/Theta calibration statistics;
- user mode state.

Possible outputs are only pre-approved safe policy choices:
- resource budget profile;
- sampling cadence within bounds;
- side-quest budget;
- memory retrieval priority;
- compression/batch policy;
- model/Theta weighting inside allowed ranges.

The local learner should expose `model_version`, `updates`, `confidence`, loss/reward metric and persisted-state version to Witness/swarm telemetry where useful.

## Advisory Home Cortex Pattern

Trusted swarm policy is advisory unless the node's declared primary mission explicitly delegates authority.

For ADV_Elite:

```text
SWARM POLICY -> context / recommendation
ADV LOCAL STATE + USER GATES -> final local decision
```

Buzz/Core/NAS must not silently steal the ADV primary mission or override `1488`, `112269`, `J`, `ENTER`, `L` or other frozen user controls.

## P/N Silicon-Body Self Trace

Borrow Zim's principle of observing the body of computation, not just its output.

ADV_Elite should expose a bounded self-state trace such as:
- loop jitter;
- heap/stack reserve;
- thermal/load proxy or real temperature when trustworthy;
- radio health;
- queue pressure;
- storage/outbox debt;
- compute batch pressure;
- current model/version;
- power/battery where trustworthy.

This becomes part of `SELF` in `WORLD + SWARM + SELF` and may be predicted/scored by M2R.

Self-state telemetry is observational context; it never changes protocol truth by itself.

## Display/Brain Decoupling

Borrow the architectural lesson from Zim HARD BLACKOUT:

```text
DISPLAY != BRAIN
```

A future ADV display-sleep/hard-off path may reduce or stop rendering while sensors, anomaly detection, Witness, local cognition and swarm health continue.

This is separate from the existing brightness controls and should only be implemented when Cardputer ADV hardware behavior is verified. Wake must be cold-safe and must not accidentally fire another control action.

## IMMUTABLE TRUTH CORE + LEARNABLE POLICY

ADV_Elite immutable core includes at minimum:
- observation truth classes;
- sensor freshness/stale semantics;
- SHA/verifier/target truth where mining is active;
- packet ABI/type/version truth;
- `1488` manual full-M2R gate;
- `112269` House semantics;
- `J` manual LoRa semantics;
- `ENTER` absolute audio mute;
- `L` physical LED output control;
- identity/provenance/hash-chain semantics.

Learnable policy may include bounded versions of:
- predictor/M2R weights;
- Theta contribution weight;
- sensor-fusion weights;
- sampling cadence;
- event-cadence estimates;
- memory retrieval priority;
- side-quest scheduling/budget;
- miner batch/resource scheduling;
- compression/resource policy.

The unresolved Q23 promotion policy is intentionally not decided by this file. This contract defines what may be learnable, not whether a PASS candidate auto-promotes without the user.

## Persistent Learning

ADV_Elite should preserve useful local learned state across reboot using the existing tiered-memory design:
- small versioned NVS/Preferences state for compact active parameters;
- SD/LittleFS Witness and larger episodic history where available;
- throttled writes / dirty flags;
- checkpoint on meaningful events;
- known-good fallback;
- no boot dependency on learned state.

## Autonomy Without Isolation

ADV_Elite may make local decisions independently while still telling trusted JANUS peers what happened.

Recommended telemetry fields:
- primary mission state;
- side-quest budget used/remaining;
- deferred/refused task reason;
- current load/heap/thermal proxy;
- learner/model version;
- confidence/error where meaningful;
- event-cadence forecast/error where meaningful;
- stale/unknown flags;
- Witness/archive debt;
- current House/Love/LoRa/M2R/audio/LED state.

## What Is Borrowed From Zim

Borrow:
- protected specialization;
- bounded coordinator work;
- independent local operation;
- measured resource throttling;
- self-tested acceleration;
- event-cadence prediction for resource scheduling;
- small bounded local learner;
- advisory-not-dominating swarm policy;
- silicon-body/self telemetry;
- display/brain decoupling;
- bounded online learning around frozen truth;
- persistent local learning;
- continued swarm visibility despite autonomy;
- explicit separation between overall strength and coordinator-attributed work.

Do not borrow as scientific truth:
- belief that nonce stride predicts SHA quality;
- rare-hash events as proof of cognitive improvement;
- game/persona reward as objective engineering reward;
- any mining superiority claim without reproducible controlled data.

## Acceptance Gate

A future ADV_Elite implementation satisfies this contract only if it can demonstrate:
1. primary sensors/Witness/anomaly remain alive while side-work is saturated;
2. Buzz/NAS outage does not stop the local brain;
3. side-work can be throttled or deferred visibly;
4. learned parameters survive reboot without flash-write abuse;
5. accelerated paths fall back safely on mismatch;
6. truth/protocol/user-control invariants remain unchanged;
7. event-cadence forecasts are frozen before the event and scored afterward;
8. local learner outputs remain inside a bounded safe action space;
9. display sleep/off does not stop the primary brain path when hardware support is added.
