# xTech|Search 10 — Concept White Paper Working Draft

> **WORKING SOURCE ONLY — NOT THE OFFICIAL SUBMISSION FORMAT.**  
> The final submission must be transferred into the official Valid Eval `Template_xTech_Search10_White_Paper.docx` and kept to exactly three pages. This file is the long-form provenance-controlled master source from which the final three-page version will be compressed. Eligibility must be resolved before submission.

**Company / eligible entity:** `[PENDING ELIGIBILITY CLARIFICATION]`  
**Proposal title:** **JANUS Resilient Edge Swarm: Low-Cost Heterogeneous Sensor and Telemetry Network for Degraded Connectivity**  
**Technology owner/developer:** Hawkar, independent researcher/developer, Ukraine  
**Repository:** `Hawkar-usls/janus-distributed-ai-swarm`

---

## 1. Introduction — 5%

JANUS Resilient Edge Swarm is an existing heterogeneous ESP32/M5Stack-class firmware system operated by its developer as a **reported 10-node physical swarm**. Different nodes perform different local roles—including sensing, operator visualization, telemetry, RF/reference functions, local computation, multimedia, and bounded distributed work—while selected state is exchanged through explicit packet interfaces, primarily ESP-NOW in the current public implementation.

The xTech effort does not begin by inventing a new swarm architecture. It begins with working firmware and physical low-cost hardware, freezes the exact ten-device configuration, and measures one bounded question: **how well can unlike edge nodes preserve useful local functions, expose peer loss and stale state honestly, and recover connectivity when peers, coordinators, or network paths are disrupted?**

JANUS is currently **active engineering**, not a production or military-certified system. That maturity boundary is part of the proposal, not hidden from it. The value of Phase I would be to convert existing implementation into a reproducible evidence package with measured strengths, failures, resource costs, and integration boundaries.

## 2. Army Benefits — 25%

xTech|Search 10 gives strong consideration to Command and Control (C2) and Counter-C2 Networks, including resilient communications and deep sensing. JANUS is **not** presented as an operational Army C2 product, protected tactical radio, or secure battlefield network. Its relevance is at a lower and testable layer: heterogeneous edge devices that must remain interpretable when connectivity becomes imperfect.

The potential Army benefit is a low-cost modular testbed for answering practical distributed-systems questions before higher-cost field integration. Rather than forcing every device into one identical role, JANUS is organized around specialized nodes with a protected local mission and bounded shared/coordinator work. Repository rules require peer/health visibility, stale-state handling, explicit sensor-truth boundaries, and inspectable recovery behavior.

That architecture creates several Army-relevant evaluation questions:

- How quickly does a lost peer become visibly stale or absent?
- Does loss of one role propagate into unrelated roles?
- Does a sensor continue its local mission when a coordinator or optional service disappears?
- Can a restored node return with the same identity without creating stale or duplicate state?
- What radio, memory, thermal, and power cost is introduced by recovery behavior?
- Which higher-level state/recovery mechanisms are tied to ESP-NOW, and which could migrate to a different transport or hardened platform later?

A positive result would not certify battlefield readiness. It would provide measured evidence about an inexpensive heterogeneous edge architecture and identify where later Army integration work would or would not be justified.

The same architecture has civilian dual-use potential in industrial/environmental telemetry, remote-infrastructure monitoring, disaster-response instrumentation, building/field automation research, and low-cost distributed-systems evaluation.

## 3. Technical Approach — 40%

### Existing technical baseline

The current public repository contains multiple active ESP32/M5Stack firmware roles and documents ESP-NOW packet/ABI behavior, local sensing, telemetry, operator surfaces, heartbeat/state visibility, stale-node requirements, recovery paths, bounded specialist behavior, and explicit technical boundaries.

The developer reports ten physical JANUS nodes in the current swarm. The public repository contains more firmware roles than the physical fleet because it also preserves compatibility images, migration targets, and experimental lineages. The final xTech baseline will therefore be created from the actual hardware, not inferred from repository file count.

Representative current roles include:

**Core2 — operator/telemetry surface.** Existing firmware provides a dense human-facing view of swarm and sensor/peer state.

**Blind Eye — physical sensing specialist.** The current AtomS3R-class profile uses an STHS34PF80 TMOS/PIR-style sensor as its primary physical sensing path, explicitly treats a missing camera as a normal hardware fact, and separates physical sensor state from memory/prediction/UI semantics.

**Anchor — RF/recovery reference.** Existing firmware includes heartbeat/telemetry behavior, deferred receive handling, reconnect/rejoin paths, and radio-blackout safeguards.

**Buzz — coordinator/workload and recovery lineage.** Existing firmware contains coordinator/worker, watchdog, reconnect, and concurrent workload history. Mining-related functions are background engineering and are not the proposed Army capability.

Other current roles—including BH/BlackStar, ADV Elite, Yaks Gate, Gladius, Golcron, Zim, Pyramid, and PEA4/P4 tracks—show the intended heterogeneity of the system. Compatibility/preserve firmware such as Beacon, Stick, and ATOM SWARM TRON remains part of project provenance. The exact physical membership of the current ten-node fleet will be frozen from the devices themselves.

### Why the architecture is technically interesting

JANUS does not claim that ESP-NOW or ESP32 hardware is novel. The candidate value is the combination of:

1. **heterogeneous specialist roles** rather than identical nodes;
2. **protected local missions** with bounded optional/shared work;
3. **explicit current/stale/memory/inference separation**;
4. **observable failure and recovery states**;
5. **low-cost COTS hardware** suitable for repeatable failure testing;
6. **inspectable source and provenance**, allowing claim-to-code review.

The same heterogeneity is also a weakness: different boards, libraries, sensors, displays, radio settings, and workloads create version/ABI/resource complexity. Phase I should measure whether that complexity remains manageable rather than assuming it does.

### Phase I work plan

**Task 1 — Freeze the ten-node reference system.** Assign immutable physical IDs, capture dated photos, board/model, node identity, firmware path, flashed commit, relevant build/library versions, sensors/peripherals, radio configuration class, and power source. Produce a hashed `TEN_NODE_TEST_MANIFEST.json`.

**Task 2 — Establish baseline behavior.** Record healthy peer visibility, physical sensor paths, local-role activity, heap/thermal state, and power where instrumentation is available.

**Task 3 — Controlled node-loss/rejoin trials.** Remove selected nodes one at a time, preserve the other devices unchanged, and timestamp last valid packet, stale/lost indication, unaffected-role behavior, first recovered packet, and return to healthy state. Repeat trials and preserve failures.

**Task 4 — Coordinator and optional-service outage trials.** Interrupt coordinator, Wi-Fi, or optional external-service paths where relevant and record exactly which local functions continue, degrade, defer, or require user intervention.

**Task 5 — Controlled 2.4 GHz impairment.** Use legal, non-destructive conditions such as distance, attenuation, shielding, coexistence load, or environmental placement to characterize packet loss and recovery. This is explicitly **not** presented as an anti-jam test.

**Task 6 — Resource-cost characterization.** Measure minimum free heap, device temperature where available, and role-specific power behavior during baseline and recovery.

**Task 7 — Reproducibility and failure ledger.** Produce raw logs, event CSVs, representative continuous video, all material failures, exact source references, and an independent replay package when a second evaluator becomes available.

### Metrics

Primary metrics include:

- `T_detect_loss` — last valid peer packet/heartbeat to stale/lost indication;
- `T_first_packet_after_restore` — restoration to first accepted peer packet;
- `T_rejoin_healthy` — restoration to healthy-peer classification;
- `rejoin_success_rate` across repeated trials;
- valid packet reception under a defined condition;
- `stale_as_current_events` with a target of zero;
- role-specific primary-mission continuity;
- secondary/cascading failure count;
- duplicate/identity-conflict events after rejoin;
- minimum free heap, temperature, and power where instrumented;
- manual interventions required for recovery.

Results will be reported as trial counts, distributions, and failure tables rather than only a fastest or best-case run.

### Known technical limitations

The proposal deliberately records the current weaknesses:

- the exact ten-node physical manifest is not yet frozen;
- resilience/rejoin/power numbers are not yet established under a controlled xTech benchmark;
- 2.4 GHz ESP-NOW/Wi-Fi-class transport is not a protected tactical or anti-jam radio;
- military cybersecurity, fleet key management, accreditation, and ATO are not established;
- current COTS boards are not claimed to be ruggedized or MIL-STD-qualified;
- scale beyond the tested physical fleet is unproven;
- heterogeneous firmware increases ABI/configuration/build complexity;
- some original JANUS code contains multimedia, mining, game, and research/lore functions that are not part of the Army value proposition;
- the current sensing roles must be described according to their actual sensors rather than inflated into imaging, ranging, or biometric capabilities.

If a controlled test reveals an additional weakness, it will be added to the evidence package rather than omitted.

## 4. Commercial Potential — 25%

JANUS uses widely available ESP32/M5Stack-class hardware and modular firmware roles, providing a low-cost path for architecture evaluation before a customer commits to a bespoke or hardened platform. The transferable pattern—specialized local roles, selected peer state, explicit freshness/stale visibility, and recovery behavior—could be relevant to remote facilities, industrial/environmental monitoring, resilient local automation, temporary field instrumentation, disaster-response sensing, and training/evaluation kits.

The current commercialization argument is based on **technical transition potential**, not invented traction. Verified customers, revenue, purchase orders, and quantified market adoption are not documented in this branch and will not be claimed without evidence.

A realistic transition path is:

```text
EXISTING PHYSICAL PROTOTYPE
-> FROZEN REFERENCE KIT
-> MEASURED RELIABILITY / FAILURE DATA
-> CLEAN INTERFACE DOCUMENTATION
-> CUSTOMER PILOT / INTEGRATION
-> HARDENED OR OEM PLATFORM IF JUSTIFIED
```

Possible business models include evaluation kits, paid integration/pilot engineering, licensed firmware/interface modules, or OEM/partner integration, subject to the repository's source-available licensing and separately negotiated commercial rights.

Phase I would materially improve commercial readiness by converting an organically evolved working system into a customer-readable reference configuration with measured reliability, known weaknesses, reproducible tests, and clearer transport/hardware integration boundaries.

## 5. Proposal Quality — 5%

This proposal uses a strict evidence firewall:

```text
EXISTING IMPLEMENTATION
!=
MEASURED PERFORMANCE
!=
PROPOSED PHASE I WORK
!=
ARMY OPERATIONAL CAPABILITY
```

Every quantitative claim will be tied to a frozen hardware/software configuration and timestamped evidence. Failed tests will remain in the record. Source timeouts will not be reported as measured latency. Owner-reported physical facts will remain labeled as such until captured. Prediction or memory will not be reported as current sensor truth.

This approach is intended to give evaluators a prototype they can challenge rather than a pitch that depends on untestable language.

---

## Finalization checklist before copying into the official template

- Resolve xTech eligibility in writing.
- Obtain the official Valid Eval white-paper template.
- Replace `[PENDING ELIGIBILITY CLARIFICATION]` with the exact eligible entity name only after confirmation.
- Freeze the exact repository commit and all ten physical node identities.
- Add dated group photo/video and per-device evidence.
- Run controlled quick-demo and full ten-node loss/rejoin tests.
- Insert only measured numbers that pass `CLAIM_EVIDENCE_MATRIX.md`.
- Preserve failures and limitations in the submission evidence set.
- Add real customer/commercial evidence only if documentary support exists.
- Compress this master source into the official three-page template without dropping the central limitations or claim firewall.