# JANUS Resilient Edge Swarm — 10-Minute Reviewer Path

This path is designed for an evaluator who does not need the full JANUS project vocabulary or history.

## Minute 0-2 — What is it?

Read [`JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md`](JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md).

Core idea: existing low-cost heterogeneous ESP32/M5Stack nodes perform different local roles, exchange selected state, expose peer health/staleness, and contain recovery paths that can be measured under controlled disruption.

## Minute 2-4 — Is there real firmware behind it?

Inspect the four preferred existing entrypoints:

- `firmware/core2/CORE2.ino` — operator / telemetry surface
- `firmware/blind_eye/BLIND_EYE.ino` — TMOS/PIR sensing node
- `firmware/anchor/Anchor.ino` — RF anchor / recovery-oriented node
- `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` — heterogeneous Atom-class peer

Optional context:

- `firmware/buzz/Buzz.ino` — coordinator/workload/multimedia lineage
- `docs/current-swarm-state.md` — current node roles and recovery rules
- `docs/architecture.md` — data flow and specialist architecture

## Minute 4-6 — What is claimed, and what is not?

Read [`CLAIM_EVIDENCE_MATRIX.md`](CLAIM_EVIDENCE_MATRIX.md) and `../docs/technical-boundaries.md`.

Key distinction:

```text
EXISTING CODE
!=
MEASURED PERFORMANCE
!=
PROPOSED PHASE I WORK
!=
OPERATIONAL ARMY CAPABILITY
```

No battlefield-readiness, anti-jam, secure-military-network, autonomous-weapons, AGI or superiority claim is made by this package.

## Minute 6-8 — What would be demonstrated?

Read [`DEMO_PLAN.md`](DEMO_PLAN.md).

The proposed controlled demo is deliberately simple:

```text
ALL NODES HEALTHY
      ->
REMOVE / ISOLATE ONE NODE
      ->
LOSS OR STALE STATE BECOMES VISIBLE
      ->
UNAFFECTED LOCAL ROLES CONTINUE WHERE SUPPORTED
      ->
RESTORE NODE / PATH
      ->
MEASURE REJOIN AND STATE RECOVERY
```

The test records loss-detection time, rejoin time, packet reception, stale-state errors, resource state and local-role continuity.

## Minute 8-10 — Why xTech?

The official xTech|Search 10 RFI gives strong consideration to **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing. JANUS is offered as a low-cost edge-systems engineering candidate that can be independently stressed and measured, not as an already fielded C2 product.

The working scoring-aligned draft is in [`submission/WHITE_PAPER_DRAFT.md`](submission/WHITE_PAPER_DRAFT.md).

## Administrative note

Eligibility is still awaiting written clarification because the detailed RFI describes qualifying U.S. small businesses while the official webpage currently contains a conflicting allied-country summary field. See [`ELIGIBILITY_AND_SUBMISSION_STATUS.md`](ELIGIBILITY_AND_SUBMISSION_STATUS.md).
