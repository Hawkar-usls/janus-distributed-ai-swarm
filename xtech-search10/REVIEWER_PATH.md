# JANUS Resilient Edge Swarm — 10-Minute Reviewer Path

This path is designed for a skeptical evaluator who wants the shortest route from claim to evidence without learning the full JANUS project vocabulary or history.

## Minute 0-1 — What is it?

Read the first sections of [`JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md`](JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md).

Core idea:

> A developer-reported **ten-node physical ESP32/M5Stack swarm** in which unlike low-cost nodes preserve different local roles, exchange selected state, expose freshness/peer health, and contain observable recovery paths that can be tested under controlled disruption.

The xTech package is a review/evidence layer over existing firmware. It is not a competition-specific rewrite.

## Minute 1-2 — What is actually distinctive?

Read [`TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md`](TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md).

The pitch does **not** depend on ESP-NOW being new. The technical interest is the combination of:

- heterogeneous specialist roles;
- protected local missions with bounded shared work;
- explicit current/stale/memory/inference separation;
- observable loss/recovery;
- low-cost COTS hardware suitable for repeatable failure testing;
- source-level inspectability and claim discipline.

## Minute 2-3 — Is there real firmware behind it?

Open `PROJECT_MAP.md` and `docs/current-swarm-state.md`.

Representative existing entrypoints:

- `firmware/core2/CORE2.ino` — operator / telemetry surface;
- `firmware/blind_eye/BLIND_EYE.ino` — TMOS/PIR physical sensing specialist;
- `firmware/anchor/Anchor.ino` — RF/recovery reference;
- `firmware/buzz/Buzz.ino` — coordinator/workload/recovery lineage;
- `firmware/zim_geek/Zim.ino`, `firmware/adv_elite/ADV_Elite.ino`, `firmware/golcron/Golcron.ino`, PEA4/P4 and other roles — evidence that the architecture is heterogeneous.

Compatibility/preserve firmware such as ATOM SWARM TRON remains useful for lineage, but repository role count is not treated as physical-node count.

## Minute 3-4 — Are the ten physical nodes documented?

Read [`TEN_NODE_PHYSICAL_SYSTEM.md`](TEN_NODE_PHYSICAL_SYSTEM.md) and [`HARDWARE_MANIFEST.md`](HARDWARE_MANIFEST.md).

Current truth:

```text
TEN PHYSICAL NODES = DEVELOPER-REPORTED
EXACT TEN-DEVICE XTECH MANIFEST = PENDING PHYSICAL CAPTURE
```

The branch intentionally leaves ten physical slots unfilled rather than guessing board/firmware assignments from memory.

## Minute 4-5 — What are the weaknesses?

Read [`STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md`](STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md).

The important limitations are stated before a reviewer has to discover them independently:

- exact ten-node manifest not yet frozen;
- no controlled xTech resilience benchmark yet;
- 2.4 GHz ESP-NOW is not anti-jam tactical RF;
- military cybersecurity/ATO is not established;
- current COTS boards are not ruggedized military hardware;
- power/endurance and scale beyond the current fleet are unproven;
- heterogeneity creates ABI/configuration complexity;
- commercial traction is not documented;
- applicant eligibility is awaiting Army clarification.

These are evaluation targets, not hidden footnotes.

## Minute 5-6 — What is claimed, and what is explicitly withheld?

Read [`CLAIM_EVIDENCE_MATRIX.md`](CLAIM_EVIDENCE_MATRIX.md) and `../docs/technical-boundaries.md`.

Key distinction:

```text
EXISTING IMPLEMENTATION
!=
MEASURED PERFORMANCE
!=
PROPOSED PHASE I WORK
!=
OPERATIONAL ARMY CAPABILITY
```

No battlefield-readiness, anti-jam, secure-military-network, autonomous-weapon, AGI, biometric-identification, arbitrary-scale, or unverified commercial-traction claim is made.

## Minute 6-7 — What does the quick demo show?

Read the Level 1 section of [`DEMO_PLAN.md`](DEMO_PLAN.md).

The short demonstrator uses a physically verified subset such as:

```text
OPERATOR SURFACE
    +
PHYSICAL SENSOR
    +
RECOVERY / RF REFERENCE
    +
HETEROGENEOUS PEER
    [+ COORDINATOR]
```

Then:

```text
HEALTHY
-> REMOVE / ISOLATE NODE
-> OBSERVE LOSS / STALE STATE
-> OBSERVE WHAT CONTINUES
-> RESTORE NODE / PATH
-> MEASURE REJOIN
```

The demo is intentionally easy to understand; it is not the full system test.

## Minute 7-8 — What does the full ten-node evaluation add?

Read Level 2 of [`DEMO_PLAN.md`](DEMO_PLAN.md).

The full plan tests different failure classes:

- one-at-a-time node loss;
- coordinator loss;
- operator-surface loss;
- sensor-role loss;
- repeated power cycling/rejoin;
- controlled 2.4 GHz impairment;
- optional external-service outage;
- concurrent display/audio/sensor/compute load;
- selected ABI/version compatibility;
- full cold restart.

Results are reported as distributions and failure tables, not one hero run.

## Minute 8-9 — Why might Army care?

The official xTech|Search 10 RFI gives strong consideration to **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing.

JANUS is **not** offered as a finished Army C2 product. It is offered as an inexpensive physically instantiated edge-systems testbed for questions that matter before field integration: freshness, peer loss, local-role continuity, heterogeneous-device compatibility, recovery cost, and transport-dependent vs transport-independent behavior.

Dual-use paths include industrial/environmental telemetry, remote infrastructure, disaster-response instrumentation, and resilient local automation research. These are potential paths, not evidence of current customers.

## Minute 9-10 — Is the team being transparent?

Read [`REVIEWER_FAQ.md`](REVIEWER_FAQ.md) and the final section of [`STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md`](STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md).

The intended review standard is:

> A weakness that is known and measured is more useful than a strength that is only asserted.

Failed tests remain in the evidence set. Source timeouts are not called measured latency. A physical statement remains owner-reported until it is captured. Experimental inference is not called physical sensor truth.

The scoring-aligned working source for the final Part 1 paper is [`submission/WHITE_PAPER_DRAFT.md`](submission/WHITE_PAPER_DRAFT.md).

## Administrative note

Eligibility is still awaiting written clarification because detailed RFI language describes qualifying U.S. small businesses while an official webpage summary field refers to U.S.-based and allied foreign-country businesses. The developer is based in Ukraine. See [`ELIGIBILITY_AND_SUBMISSION_STATUS.md`](ELIGIBILITY_AND_SUBMISSION_STATUS.md).

This branch does not assume eligibility and does not propose bypassing the competition rules.