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
6. truth/protocol/user-control invariants remain unchanged.
