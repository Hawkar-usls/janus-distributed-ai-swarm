<div align="center">

# JANUS Resilient Edge Swarm
### xTech|Search 10 submission review surface

`10-node physical swarm` · `heterogeneous low-cost edge nodes` · `sensing` · `telemetry` · `observable failure/recovery`

**Branch:** `submission/xtech-search10-2026`  
**Source project maturity:** `ACTIVE_ENGINEERING`  
**Submission status:** `PREPARATION / ELIGIBILITY CLARIFICATION PENDING`

</div>

## What this directory is

This directory is the clean review and evidence layer for a possible **xTech|Search 10** submission based on the existing `JANUS Distributed AI Swarm` project.

**No existing firmware is replaced, deleted, or rewritten for the competition package.** The branch was created from the repository's current `main` history. The original JANUS code, names, experiments, multimedia functions, mining lineages, and research vocabulary remain intact. This directory simply gives an external evaluator a shorter path to the engineering that is relevant to xTech.

The developer reports a **10-node physical JANUS swarm**. The exact device-by-device ten-node manifest has not yet been frozen with dated hardware evidence, so this branch deliberately distinguishes:

```text
OWNER-REPORTED PHYSICAL FACT
!=
REPOSITORY-VERIFIABLE IMPLEMENTATION
!=
CONTROLLED MEASUREMENT
!=
OPERATIONAL CAPABILITY
```

The four/five-node demonstrator described in this package is a reviewer/test subset, **not the size of the full swarm**.

## Candidate title

**JANUS Resilient Edge Swarm: Low-Cost Heterogeneous Sensor and Telemetry Network for Degraded Connectivity**

The detailed documents may also use `swarm` or `mesh` descriptively. `Network` is preferred when precision matters because the current package does not claim that every selected firmware path implements general-purpose multi-hop mesh routing.

## One-sentence concept

A heterogeneous ESP32/M5Stack edge system in which specialized low-cost nodes preserve different local roles while exchanging selected state, exposing heartbeat/freshness, and making disruption, staleness, and recovery visible enough to measure.

## Why it may be worth evaluating

JANUS does not ask xTech to fund a first paper prototype. The public repository already contains multiple ESP32/M5Stack firmware roles, sensor paths, operator surfaces, packet/ABI rules, stale-node semantics, watchdog/reconnect/rejoin logic, and a protected-primary-mission doctrine. The proposed effort is to freeze the physically existing system and determine—under controlled failure—what actually continues, what degrades, how quickly loss becomes visible, how cleanly nodes rejoin, and what those mechanisms cost in radio, memory, power, and complexity.

The strongest differentiators are:

- **heterogeneity by design** rather than a fleet of identical workers;
- **protected local missions** with bounded optional/shared work;
- **sensor truth separated from stale, remembered, inferred, and UI state**;
- **failure/recovery as observable states** rather than something hidden behind a single health flag;
- **low-cost COTS hardware** that makes repeated disruption testing practical;
- **inspectable source and claim boundaries**, including explicit statements of what is not yet proven.

The strongest current weaknesses are also recorded: the exact ten-device manifest is not yet frozen, controlled resilience metrics are not yet collected, 2.4 GHz ESP-NOW is not a tactical anti-jam transport, military cybersecurity/ruggedization are not established, scaling beyond the current physical fleet is unproven, power/endurance data is incomplete, and commercial traction is not documented. These are not hidden; they are treated as evaluation targets.

## Recommended reviewer path

For a fast review:

1. [`REVIEWER_PATH.md`](REVIEWER_PATH.md) — ten-minute route.
2. [`JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md`](JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md) — technical overview.
3. [`TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md`](TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md) — why the architecture may matter.
4. [`STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md`](STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md) — strengths, weaknesses, falsifiers, and risk controls.
5. [`CLAIM_EVIDENCE_MATRIX.md`](CLAIM_EVIDENCE_MATRIX.md) — what may and may not be claimed.
6. [`TEN_NODE_PHYSICAL_SYSTEM.md`](TEN_NODE_PHYSICAL_SYSTEM.md) — physical-fleet statement and freeze protocol.
7. [`HARDWARE_MANIFEST.md`](HARDWARE_MANIFEST.md) — current demo selection and evidence checklist.
8. [`DEMO_PLAN.md`](DEMO_PLAN.md) — quick demonstrator plus full ten-node evaluation plan.
9. [`REVIEWER_FAQ.md`](REVIEWER_FAQ.md) — skeptical questions answered directly.
10. [`submission/WHITE_PAPER_DRAFT.md`](submission/WHITE_PAPER_DRAFT.md) — scoring-aligned working source for the official three-page template.

Repository evidence remains authoritative for the underlying implementation:

- [`../PROJECT_MAP.md`](../PROJECT_MAP.md)
- [`../docs/current-swarm-state.md`](../docs/current-swarm-state.md)
- [`../docs/architecture.md`](../docs/architecture.md)
- [`../docs/technical-boundaries.md`](../docs/technical-boundaries.md)
- [`../docs/swarm-critical-rules.md`](../docs/swarm-critical-rules.md)
- [`../docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`](../docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md)

## Physical system and reviewer subset

**Full physical system:** developer-reported 10 JANUS nodes. Exact board/model-to-node capture for all ten remains a pre-submission evidence task.

**Short demonstrator:** a deliberately understandable subset, currently centered on:

| Function | Existing node | Repository path |
| --- | --- | --- |
| Operator / telemetry surface | M5Stack Core2 | `firmware/core2/CORE2.ino` |
| Physical sensing | Blind Eye / AtomS3R + STHS34PF80 | `firmware/blind_eye/BLIND_EYE.ino` |
| RF/recovery reference | Anchor / ESP32-S3 | `firmware/anchor/Anchor.ino` |
| Heterogeneous peer | ATOM SWARM TRON or another physically verified node | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` |
| Optional coordinator/workload role | Buzz | `firmware/buzz/Buzz.ino` |

The exact quick-demo peer may change after the ten physical devices are frozen. The rule is to select from what is actually powered and documented, not from memory.

## Army relevance, carefully scoped

The official xTech|Search 10 RFI gives strong consideration to **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing. JANUS is **not** presented as an operational Army C2 system, secure military network, or anti-jam radio.

The relevant engineering question is lower-level and testable:

> Can unlike low-cost edge devices preserve useful local behavior, communicate health/freshness honestly, and recover cleanly enough under degraded connectivity to justify later integration work?

A positive Phase I result would be a reproducible evidence package, not a battlefield-readiness certificate.

## Claim firewall

```text
EXISTING_IMPLEMENTATION
!=
MEASURED_PERFORMANCE
!=
PROPOSED_PHASE_I_WORK
!=
ARMY_OPERATIONAL_CAPABILITY
```

This package does **not** claim:

- battlefield deployment or combat readiness;
- anti-jam or contested-spectrum superiority;
- secure military communications certification;
- autonomous weapon or human-targeting authority;
- AGI or access to future physical information;
- arbitrary scalability beyond the tested fleet;
- commercial customers/revenue without documentary evidence;
- any quantitative performance number that is not tied to a frozen test manifest and raw evidence.

## Transparency principle

A reviewer should be able to find the weaknesses as easily as the strengths. If a controlled test fails, the failure belongs in the result set. If a capability is only present in source but not benchmarked, it is labeled `SUPPORTED IN CODE`, not `PROVEN PERFORMANCE`. If a physical fact has not yet been captured, it remains `OWNER-REPORTED` until evidence exists.

## Current administrative gate

Detailed RFI eligibility language describes qualifying U.S. small businesses, while an official webpage summary field refers to U.S.-based and allied foreign-country businesses. Because the technology owner/developer is based in Ukraine, written clarification has been requested from the Army FUZE xTech Program.

Until that reply arrives, this branch remains a **submission-preparation and evidence surface, not a claim of eligibility**.

Official references:

- Competition page: https://xtech.army.mil/competition/xtechsearch10/
- RFI: https://xtech.army.mil/wp-content/uploads/2026/09/xTechSearch-10-Competition-RFI_Final.pdf

## Submission rule

The final Part 1 paper must be transferred into the official Valid Eval xTech|Search 10 template and kept to the required page limit. This repository draft is a provenance-controlled working source; it is not a substitute for the official submission format.