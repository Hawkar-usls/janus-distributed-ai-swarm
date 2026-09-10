# JANUS Resilient Edge Swarm — Technical Overview

## Candidate technology

**JANUS Resilient Edge Swarm** is the xTech-facing description of the existing `JANUS Distributed AI Swarm` engineering project. It is not a new competition-specific rewrite and does not replace the original firmware.

The developer reports a **10-node physical JANUS swarm** built from ESP32/M5Stack-class devices. The public repository contains a larger firmware surface than ten because it also preserves active specialist roles, compatibility images, migration paths, and experimental lineages. The exact physical ten-node mapping will be frozen from the actual devices before quantitative submission claims are finalized.

The core engineering idea is simple:

> Different low-cost edge nodes should be allowed to remain different. Each keeps a useful local role, shares selected state through explicit interfaces, exposes whether peer information is current or stale, and attempts recovery in ways that can be observed and measured.

The xTech submission therefore isolates one practical question from the larger JANUS project:

> **Can a physically existing heterogeneous edge swarm preserve useful local sensing/telemetry behavior and honest state visibility when peers, coordinators, or network paths become unavailable, and can recovery be reproduced rather than assumed?**

## Existing architecture

The current repository already implements and documents important pieces of that question:

- heterogeneous ESP32/M5Stack node roles rather than one identical worker image;
- ESP-NOW packet/ABI handling and peer-state exchange in multiple firmware lines;
- local sensor and state processing;
- operator/display surfaces for swarm telemetry;
- periodic heartbeat and peer visibility requirements;
- stale-node handling, reconnect/rejoin, watchdog, and channel-recovery paths;
- protected `PRIMARY_MISSION` with bounded side-work;
- local state persistence in selected nodes;
- separation between current physical sensor state, stale remote state, remembered state, prediction/inference, and UI/game state;
- explicit technical boundaries around adaptive control, claims, and protocol truth.

The repository also contains functions unrelated to the Army value proposition—multimedia, games, mining/Stratum work, research vocabulary, and project lore. Those are preserved as provenance rather than deleted. The xTech layer points reviewers only to the engineering mechanisms relevant to sensing, peer health, degradation, and recovery.

## How the ten-node physical system should be understood

The reported physical swarm is a **team of specialists**, not ten copies of one board. Current repository roles include operator surfaces, sensor nodes, coordinator/workload nodes, RF/recovery references, mobile/gate specialists, observer nodes, local autonomous specialists, visual/preserve nodes, and P4 compute/verification extensions.

This matters because heterogeneous systems fail differently. A display-heavy node, sensor-heavy node, audio node, RF anchor, and compute node do not share the same memory, thermal, power, or timing budget. JANUS treats that diversity as an engineering condition rather than an exception.

At the same time, the submission must not imply that every repository role is one of the ten currently powered devices. The physical mapping is pending evidence capture. See [`TEN_NODE_PHYSICAL_SYSTEM.md`](TEN_NODE_PHYSICAL_SYSTEM.md).

## Key architecture properties

### 1. Protected local function

Every upgraded specialist is expected to declare a protected primary mission. Coordinator or shared work must be primary or explicitly bounded side-work.

The intent is that optional participation should degrade before a node silently abandons its local purpose. For example, a sensing node should not become useless merely because a shared workload or coordinator path is unavailable.

**Evidence status:** architecture/design rule implemented across the project direction; quantitative continuity across the exact ten-device fleet remains to be tested.

### 2. Explicit freshness and truth

JANUS treats freshness as a technical property. Current sensor state, stale peer data, memory, prediction, and presentation state are not interchangeable.

This is particularly important during communication loss. A visually stable display can be misleading if the values on it are simply old. The project therefore favors explicit stale/lost states over silently preserving the last attractive value.

### 3. Observable recovery

Several firmware paths contain reconnect/rejoin, watchdog, channel-reassertion, deferred receive queues, stale TTLs, or direct recovery logic.

The important xTech claim is not `JANUS always recovers`. The useful fact is that **recovery behavior exists in implementation and can be subjected to controlled failure tests**.

### 4. Heterogeneous resource budgets

Some nodes combine radio traffic with displays, audio, SD storage, local sensing, local computation, or workload scheduling. That increases complexity, but it also creates a realistic constrained-edge testbed.

A good evaluation should therefore measure not only packet success, but also memory pressure, temperature, power, and whether useful local functions survive while recovery logic is active.

## Representative current roles

The following are repository-verifiable examples, not a frozen declaration of the exact physical ten.

### Core2 — operator and telemetry surface

`firmware/core2/CORE2.ino`

Core2 is the dense human-facing swarm station. Current firmware receives ESP-NOW swarm data, maintains node/state views, integrates local sensing, and provides a natural surface for observing peer health, staleness, and recovery.

### Blind Eye — physical sensing specialist

`firmware/blind_eye/BLIND_EYE.ino`

The current AtomS3R-class profile uses an STHS34PF80 TMOS/PIR-style sensor as its primary physical eye. The firmware explicitly treats the absence of a camera as a normal hardware fact and separates physical sensor truth from memory/prediction/UI semantics.

This should **not** be described as a thermal camera, biometric identifier, or precision ranging system.

### Anchor — RF/recovery reference

`firmware/anchor/Anchor.ino`

Anchor contains heartbeat/telemetry behavior, deferred receive handling, Wi-Fi/ESP-NOW recovery paths, and explicit radio-rejoin safeguards. It is a strong candidate for controlled loss/rejoin tests.

### Buzz — coordinator/workload and recovery lineage

`firmware/buzz/Buzz.ino`

Buzz contains mature ESP-NOW coordinator/worker logic, recovery/watchdog behavior, and concurrent multimedia/workload history. Its mining-related functionality is background engineering, not the proposed Army capability.

### Other current specialists

The repository also includes BH/BlackStar, ADV Elite, Yaks Gate, Gladius, Golcron, Zim, Pyramid, PEA4/P4 tracks, plus preserve/compatibility firmware such as Beacon, Stick, and ATOM SWARM TRON. These demonstrate that JANUS is a family of specialized edge roles rather than a single firmware image.

## Why the architecture may be relevant

Low-cost distributed edge systems are only useful if an evaluator can answer basic failure questions:

- Which node disappeared?
- When did its data stop being current?
- Which local functions continued?
- Did recovery traffic create a second failure?
- Did the restored node return with the same identity?
- Did stale or conflicting state survive after rejoin?
- What did recovery cost in memory, radio time, power, or temperature?

JANUS makes these questions explicit enough to build a controlled test protocol around them.

The official xTech|Search 10 RFI gives strong consideration to **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing. JANUS is not presented as an operational C2 system. Its potential relevance is as a low-cost edge architecture/testbed for studying distributed sensing, freshness, role continuity, and recovery under degraded connectivity.

## Technical strengths

The strongest current properties are:

1. **existing physical implementation before Phase I**, rather than a paper-only concept;
2. **heterogeneous specialist roles**, which reflect mixed-device edge environments;
3. **protected-primary-mission doctrine**, intended to limit cascading degradation;
4. **explicit stale/current/truth semantics**, valuable for operator trust;
5. **recovery code paths already present**, making failure testing immediately meaningful;
6. **low-cost COTS hardware**, useful for rapid repeatable experiments;
7. **inspectable source/provenance**, enabling claim-to-code review;
8. **a deliberate claim firewall**, preventing code existence from being called measured performance.

## Weaknesses and open questions

The project is not presented as mature in areas where evidence is missing:

- the exact ten-node physical device/firmware manifest is not yet frozen;
- controlled xTech measurements for loss detection, rejoin, packet impairment, power, and full-swarm continuity are not yet available;
- the current 2.4 GHz ESP-NOW/Wi-Fi-class transport is not a tactical anti-jam radio;
- military cybersecurity, protected key management, accreditation, and ATO are not established;
- the COTS boards are not claimed to be ruggedized or MIL-STD-qualified;
- role-specific power/endurance and environmental limits are not fully measured;
- scaling beyond the actual tested physical fleet is unproven;
- heterogeneous firmware creates version/ABI/configuration complexity;
- some project vocabulary and unrelated functions can distract from the technical claim;
- commercial customers, revenue, and validated market traction are not documented in this branch;
- direct competition eligibility for the Ukraine-based developer is awaiting written clarification.

A detailed risk register and mitigation path is maintained in [`STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md`](STRENGTHS_LIMITATIONS_AND_RISK_REGISTER.md).

## Existing implementation vs. evidence still required

### Repository-verifiable now

- multiple current firmware roles;
- ESP-NOW protocol/ABI logic in selected roles;
- sensor integration in selected roles;
- operator/telemetry surfaces;
- heartbeat/stale/recovery requirements and code paths;
- protected-specialist architecture;
- explicit sensor-truth and protocol boundaries.

### Owner-reported, pending physical capture

- ten physical JANUS devices in the present swarm;
- exact current board/model assignment of all ten;
- exact flashed firmware/commit per physical device;
- current real-world BLE/ESPHome paths not yet located/frozen in this xTech public baseline.

### Must be measured before quantitative claims

- `T_detect_loss` and `T_rejoin` distributions;
- valid packet reception under defined impairment;
- stale-state error frequency;
- local-role continuity under coordinator/path loss;
- memory/heap, thermal, and power behavior;
- recovery success rate over repeated cycles;
- scaling behavior beyond the frozen physical fleet.

## Controlled evaluation concept

The quick reviewer demonstration uses a small understandable subset. The real evaluation target is the ten-node physical swarm.

A full test series should follow:

```text
FREEZE TEN-NODE MANIFEST
        ->
BASELINE ALL NODES
        ->
REMOVE ONE ROLE
        ->
MEASURE LOSS / STALE / CONTINUITY
        ->
RESTORE AND MEASURE REJOIN
        ->
REPEAT ACROSS ROLE TYPES
        ->
IMPAIR COORDINATOR / NETWORK PATHS
        ->
MEASURE RESOURCE + POWER COST
        ->
PRESERVE FAILURES
        ->
INDEPENDENT REPLAY
```

The result should be distributions and failure tables, not one ideal video run.

## Dual-use paths

Potential civilian pathways include industrial/environmental telemetry, remote-infrastructure monitoring, disaster-response instrumentation, building/field automation research, and educational/integration kits.

These are **application pathways, not commercial traction claims**.

## xTech value proposition

The proposal is deliberately bounded:

> **Take a physically existing ten-node heterogeneous edge swarm, freeze exactly what it is, and convert its current sensing, freshness, failure, and recovery mechanisms into a reproducible evidence package that outside evaluators can stress, measure, falsify, and improve.**

That is the submission's central technical story.