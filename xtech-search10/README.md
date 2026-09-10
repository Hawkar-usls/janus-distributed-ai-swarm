<div align="center">

# JANUS Resilient Edge Swarm
### xTech|Search 10 submission review surface

`low-cost heterogeneous edge nodes` · `sensing` · `telemetry` · `degraded-network recovery`

**Branch:** `submission/xtech-search10-2026`  
**Source project maturity:** `ACTIVE_ENGINEERING`  
**Submission status:** `PREPARATION / ELIGIBILITY CLARIFICATION PENDING`

</div>

## Purpose

This directory is a presentation and evidence layer for a possible **xTech|Search 10** submission based on the existing `JANUS Distributed AI Swarm` project.

**No existing firmware is replaced, deleted, or rewritten for this competition package.** The branch was created from the existing repository and preserves the full project history and current firmware surface. The files in this directory organize what already exists, identify what can be demonstrated, and separate implemented engineering from proposed Phase I work.

## Candidate title

**JANUS Resilient Edge Swarm: Low-Cost Heterogeneous Sensor and Telemetry Mesh for Degraded Networks**

## One-sentence concept

A heterogeneous ESP32/M5Stack edge network in which specialized low-cost nodes preserve their primary local function while exchanging selected state over ESP-NOW, exposing heartbeat/stale-node status, and recovering peer/coordinator connectivity after disruption.

## Recommended reviewer path

1. [`JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md`](JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md)
2. [`CLAIM_EVIDENCE_MATRIX.md`](CLAIM_EVIDENCE_MATRIX.md)
3. [`HARDWARE_MANIFEST.md`](HARDWARE_MANIFEST.md)
4. [`DEMO_PLAN.md`](DEMO_PLAN.md)
5. [`submission/WHITE_PAPER_DRAFT.md`](submission/WHITE_PAPER_DRAFT.md)
6. Existing repository evidence: [`../PROJECT_MAP.md`](../PROJECT_MAP.md), [`../docs/current-swarm-state.md`](../docs/current-swarm-state.md), [`../docs/architecture.md`](../docs/architecture.md), [`../docs/technical-boundaries.md`](../docs/technical-boundaries.md)

## Existing candidate configuration

The primary xTech demonstration configuration is intentionally smaller than the full JANUS swarm:

| Function | Existing node | Current repository path |
| --- | --- | --- |
| Operator / telemetry surface | M5Stack Core2 | `firmware/core2/CORE2.ino` |
| Sensing node | Blind Eye / AtomS3R + STHS34PF80 | `firmware/blind_eye/BLIND_EYE.ino` |
| Resilience / RF anchor | Anchor / ESP32-S3 | `firmware/anchor/Anchor.ino` |
| Heterogeneous edge node | ATOM SWARM TRON | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` |
| Optional coordinator / workload node | Buzz | `firmware/buzz/Buzz.ino` |

Other firmware remains part of the repository but is not required to explain the core xTech concept.

## Competition alignment

The official xTech|Search 10 RFI describes an open-topic competition and gives strong consideration to technologies aligned with Army PIT focus areas. This package maps primarily to **Command and Control (C2) and Counter-C2 Networks**, especially resilient communications and deep sensing, and secondarily to distributed/edge operations relevant to **Adaptive Sustainment**.

Official references:

- Competition page: https://xtech.army.mil/competition/xtechsearch10/
- Full RFI: https://xtech.army.mil/wp-content/uploads/2026/09/xTechSearch-10-Competition-RFI_Final.pdf

The official Part 1 white paper is limited to three pages and is scored on Introduction (5%), Army Benefits (25%), Technical Approach (40%), Commercial Potential (25%), and Proposal Quality (5%). The final submission must use the official Valid Eval template.

## Claim firewall

```text
EXISTING_IMPLEMENTATION
!=
PROPOSED_PHASE_I_WORK
!=
ARMY_OPERATIONAL_CAPABILITY
```

This package does **not** claim battlefield deployment, combat readiness, tactical superiority, autonomous weapon authority, AGI, precognition, production safety certification, or any performance result that is not supported by repository evidence or a reproducible hardware test.

## Current gate

The detailed RFI eligibility section states that applicants must be small, for-profit, independent U.S. businesses with qualifying ownership/control and no more than 500 employees. The xTech webpage currently also contains a conflicting summary field referring to U.S.-based and allied foreign country businesses. A clarification request has been sent to the xTech Program because the technology owner/developer is based in Ukraine.

Until xTech answers, this branch remains a **submission-preparation surface, not a claim of eligibility**.
