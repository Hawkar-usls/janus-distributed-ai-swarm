<div align="center">

# JANUS Resilient Edge Swarm
### xTech|Search 10 submission review surface

`10-node physical swarm` · `modular role-based architecture` · `heterogeneous low-cost edge nodes` · `sensing` · `telemetry` · `observable failure/recovery` · `separate JUXTAPOSE research asset`

**Branch:** `submission/xtech-search10-2026`  
**Source project maturity:** `ACTIVE_ENGINEERING`  
**Submission status:** `PREPARATION / ELIGIBILITY CLARIFICATION PENDING`

</div>

## Core value proposition: expandability without forced uniformity

The strongest architectural feature of JANUS is **modularity and extensibility**.

The current ten-node physical swarm is the evidence-bearing system that exists now. It is not intended to define a permanent ceiling or a fixed appliance. JANUS is organized around specialized roles so that new sensors, operator surfaces, compute nodes, gateways, mobile/robotic platforms, aerial/drone payload nodes, and other authorized modules can be added through bounded interfaces without forcing every existing device to become the same hardware or firmware image.

The reusable idea is:

```text
NEW PLATFORM / SENSOR / COMPUTE DEVICE
            -> platform-specific adapter
            -> stable identity + declared role
            -> bounded inputs / outputs / authority
            -> health + freshness + provenance
            -> JANUS shared-state interfaces
```

The xTech package does **not** claim universal plug-and-play. A new module must still be implemented, integrated, and tested. The value proposition is that JANUS already embodies a role/interface discipline intended to make that extension measurable rather than requiring a redesign of the entire swarm.

A separate JANUS project, **SkinGPT v0.3**, is relevant as an example of an adjacent module class: it is an experimental tactile/thermal/mechanical sensing and data-acquisition subsystem with ESP32-S3 acquisition, thermal zones, piezo input, optional IMU/thermal-array support, telemetry, calibration, and provenance tooling. It is **not currently represented as integrated into the ten-node xTech baseline**; it is included as evidence that the broader JANUS work already extends beyond the current swarm into another concrete sensing subsystem.

Mobile or actuated platforms are treated the same way: as possible future node classes, not as capabilities that are silently claimed today. A drone/mobile platform could expose health, payload sensor state, position/platform telemetry, and bounded mission state through an adapter. An actuated or turret-form **sensor/observation mount** could expose position, health, and authorized actuation state. This xTech package does **not** claim weapon control, autonomous targeting, fire control, or human-targeting authority.

See [`MODULARITY_AND_EXTENSION_MODEL.md`](MODULARITY_AND_EXTENSION_MODEL.md).

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

## Two existing assets, kept separate

This xTech review surface exposes two bodies of existing JANUS work without pretending that they are already one integrated product:

### A. JANUS Resilient Edge Swarm — physical / firmware asset

A developer-reported ten-node heterogeneous ESP32/M5Stack-class physical swarm with sensing, operator surfaces, telemetry, specialized local roles, stale-state semantics, and recovery-oriented firmware paths.

### B. JUXTAPOSE — algorithmic / experimental-method asset

A separately developed exact-backed adaptive communications-search architecture. JUXTAPOSE may rank which authorized path to check first and how broadly to search, but fresh end-to-end measurement remains the only authority for current connectivity. Its frozen v5.1 evidence is synthetic, not hardware/field validation.

The relationship is intentionally stated as:

```text
EXISTING JANUS SWARM
+
SEPARATE EXISTING JUXTAPOSE RESEARCH
=
BREADTH OF CURRENT DEVELOPER CAPABILITY

NOT

JUXTAPOSE IS ALREADY INTEGRATED INTO THE SWARM
```

See [`JUXTAPOSE_CAPABILITY_AND_SWARM_RELATION.md`](JUXTAPOSE_CAPABILITY_AND_SWARM_RELATION.md).

## Candidate title

**JANUS Resilient Edge Swarm: Modular Heterogeneous Sensor, Telemetry and Edge-Integration Network for Degraded Connectivity**

The detailed documents may also use `swarm` or `mesh` descriptively. `Network` is preferred when precision matters because the current package does not claim that every selected firmware path implements general-purpose multi-hop mesh routing.

## One-sentence concept

A modular heterogeneous ESP32/M5Stack edge system in which specialized low-cost nodes preserve different local roles, can be extended through bounded role/interface adapters, exchange selected state, expose heartbeat/freshness, and make disruption, staleness, and recovery visible enough to measure.

## Why it may be worth evaluating

JANUS does not ask xTech to fund a first paper prototype. The public repository already contains multiple ESP32/M5Stack firmware roles, sensor paths, operator surfaces, packet/ABI rules, stale-node semantics, watchdog/reconnect/rejoin logic, and a protected-primary-mission doctrine. The proposed effort is to freeze the physically existing system and determine—under controlled failure—what actually continues, what degrades, how quickly loss becomes visible, how cleanly nodes rejoin, and what those mechanisms cost in radio, memory, power, and complexity.

The central architectural question is broader than resilience alone: **can a new specialist be added without rewriting every existing node?** That question turns extensibility into something measurable. A modularity evaluation can freeze the existing ten-node baseline, add one new benign module through a documented adapter, and measure exactly what code, ABI, configuration, resource, and recovery changes were required.

JUXTAPOSE adds context about what the developer has already done beyond firmware integration: formal problem definition, exact-measurement authority, adaptive search-order control, OOD handling, frozen synthetic gates, negative-result preservation, and an explicit hardware-validation next step. It is included as an adjacent capability, **not as a hidden feature of the current swarm**.

The strongest differentiators are:

- **modularity and extensibility as the primary design objective**;
- **heterogeneity by design** rather than a fleet of identical workers;
- **role-preserving integration** so new modules do not need to reproduce every existing function;
- **protected local missions** with bounded optional/shared work;
- **sensor truth separated from stale, remembered, inferred, and UI state**;
- **failure/recovery as observable states** rather than something hidden behind a single health flag;
- **low-cost COTS hardware** that makes repeated disruption and integration testing practical;
- **inspectable source and claim boundaries**, including explicit statements of what is not yet proven;
- **an existing adjacent adaptive-search research asset** whose authority is explicitly limited to search order/width until fresh measurement confirms current connectivity.

The strongest current weaknesses are also recorded: the exact ten-device manifest is not yet frozen, controlled resilience metrics are not yet collected, arbitrary third-party-platform integration is not proven, 2.4 GHz ESP-NOW is not a tactical anti-jam transport, military cybersecurity/ruggedization are not established, scaling beyond the current physical fleet is unproven, power/endurance data is incomplete, commercial traction is not documented, SkinGPT is not yet integrated into the xTech baseline, and JUXTAPOSE has not yet been validated on the real swarm. These are not hidden; they are treated as evaluation targets.

## Recommended reviewer path

For a fast review:

1. [`MODULARITY_AND_EXTENSION_MODEL.md`](MODULARITY_AND_EXTENSION_MODEL.md) — core architectural value: how new roles can join without forcing uniformity.
2. [`REVIEWER_PATH.md`](REVIEWER_PATH.md) — ten-minute route.
3. [`JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md`](JANUS_RESILIENT_EDGE_SWARM_OVERVIEW.md) — technical overview.
4. [`JUXTAPOSE_CAPABILITY_AND_SWARM_RELATION.md`](JUXTAPOSE_CAPABILITY_AND_SWARM_RELATION.md) — separate adaptive-search capability and its exact relationship to the physical swarm.
5. [`TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md`](TECHNICAL_DIFFERENTIATORS_AND_USE_CASES.md) — why the architecture may matter.
6. [`STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md`](STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md) — strengths, weaknesses, falsifiers, and risk controls.
7. [`CLAIM_EVIDENCE_MATRIX.md`](CLAIM_EVIDENCE_MATRIX.md) — what may and may not be claimed.
8. [`TEN_NODE_PHYSICAL_SYSTEM.md`](TEN_NODE_PHYSICAL_SYSTEM.md) — physical-fleet statement and freeze protocol.
9. [`HARDWARE_MANIFEST.md`](HARDWARE_MANIFEST.md) — current demo selection and evidence checklist.
10. [`DEMO_PLAN.md`](DEMO_PLAN.md) — quick demonstrator plus full ten-node evaluation plan.
11. [`REVIEWER_FAQ.md`](REVIEWER_FAQ.md) — skeptical questions answered directly.
12. [`submission/WHITE_PAPER_DRAFT.md`](submission/WHITE_PAPER_DRAFT.md) — scoring-aligned working source for the official three-page template.

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

## JUXTAPOSE relation to a future hardware test

The current swarm is not being modified to make JUXTAPOSE appear integrated. Instead, the physically existing ten-node system is treated as a plausible **future falsification substrate** for the separate algorithm.

A future experiment could expose authorized real peer/interface/path candidates to JUXTAPOSE, allow it to rank search order and width, and still require a fresh real health check before any path is treated as currently valid.

```text
SWARM OBSERVES
      ↓
JUXTAPOSE PRIORITIZES
      ↓
REAL NETWORK MEASURES
      ↓
MEASUREMENT DECIDES CURRENT TRUTH
      ↓
RESULT BECOMES LATER LEARNING RECEIPT
```

Until that controlled integration/test exists, this remains a **future validation path**, not a current swarm performance claim.

## Army relevance, carefully scoped

The official xTech|Search 10 RFI gives strong consideration to **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing. JANUS is **not** presented as an operational Army C2 system, secure military network, anti-jam radio, or autonomous weapons system.

The relevant engineering questions are lower-level and testable:

> Can unlike low-cost edge devices preserve useful local behavior, communicate health/freshness honestly, recover cleanly under degraded connectivity, and accept new specialist modules without unnecessary changes to unrelated nodes?

JUXTAPOSE adds a second bounded question:

> Can exact-backed adaptive ordering reduce wasted route/path search on real heterogeneous hardware without ever granting prediction authority over current connectivity truth?

A positive result would be a reproducible evidence package, not a battlefield-readiness certificate.

## Claim firewall

```text
EXISTING_IMPLEMENTATION
!=
MEASURED_PERFORMANCE
!=
PROPOSED_PHASE_I_WORK
!=
ARMY_OPERATIONAL_CAPABILITY

MODULAR_ARCHITECTURE
!=
UNIVERSAL_PLUG_AND_PLAY

CANDIDATE_EXTENSION
!=
CURRENTLY_INTEGRATED_CAPABILITY

JUXTAPOSE_SYNTHETIC_EVIDENCE
!=
JANUS_HARDWARE_EVIDENCE

SEPARATE_EXISTING_CAPABILITIES
!=
CURRENT_INTEGRATION
```

This package does **not** claim:

- battlefield deployment or combat readiness;
- anti-jam or contested-spectrum superiority;
- secure military communications certification;
- autonomous weapon, fire-control, or human-targeting authority;
- AGI or access to future physical information;
- arbitrary scalability beyond the tested fleet;
- universal integration with drones, actuated platforms, SkinGPT, or other third-party hardware without implementation evidence;
- commercial customers/revenue without documentary evidence;
- any quantitative performance number that is not tied to its exact evidence domain;
- that JUXTAPOSE is already embedded in or controlling the present JANUS swarm.

## Transparency principle

A reviewer should be able to find the weaknesses as easily as the strengths. If a controlled test fails, the failure belongs in the result set. If a capability is only present in source but not benchmarked, it is labeled `SUPPORTED IN CODE`, not `PROVEN PERFORMANCE`. If a physical fact has not yet been captured, it remains `OWNER-REPORTED` until evidence exists. If an algorithm has only synthetic evidence, it stays synthetic until the real hardware test is actually run. If a platform is merely a plausible extension, it stays a `CANDIDATE EXTENSION` until a concrete adapter and physical test exist.

## Current administrative gate

Detailed RFI eligibility language describes qualifying U.S. small businesses, while an official webpage summary field refers to U.S.-based and allied foreign-country businesses. Because the technology owner/developer is based in Ukraine, written clarification has been requested from the Army FUZE xTech Program.

Until that reply arrives, this branch remains a **submission-preparation and evidence surface, not a claim of eligibility**.

Official references:

- Competition page: https://xtech.army.mil/competition/xtechsearch10/
- RFI: https://xtech.army.mil/wp-content/uploads/2026/09/xTechSearch-10-Competition-RFI_Final.pdf

## Submission rule

The final Part 1 paper must be transferred into the official Valid Eval xTech|Search 10 template and kept to the required page limit. This repository draft is a provenance-controlled working source; it is not a substitute for the official submission format.