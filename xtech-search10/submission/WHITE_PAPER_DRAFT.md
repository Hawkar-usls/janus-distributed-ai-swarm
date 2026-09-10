# xTech|Search 10 — Concept White Paper Working Draft

> **WORKING SOURCE ONLY — NOT THE OFFICIAL SUBMISSION FORMAT.**  
> The final submission must be transferred into the official Valid Eval `Template_xTech_Search10_White_Paper.docx` and kept to exactly three pages. This file is the long-form provenance-controlled master source from which the final three-page version will be compressed. Eligibility must be resolved before submission.

**Company / eligible entity:** `[PENDING ELIGIBILITY CLARIFICATION]`  
**Proposal title:** **JANUS Resilient Edge Swarm: Modular Heterogeneous Sensor, Telemetry and Edge-Integration Network for Degraded Connectivity**  
**Technology owner/developer:** Hawkar, independent researcher/developer, Ukraine  
**Repository:** `Hawkar-usls/janus-distributed-ai-swarm`

---

## 1. Introduction — 5%

JANUS Resilient Edge Swarm is an existing heterogeneous ESP32/M5Stack-class firmware system operated by its developer as a **reported 10-node physical swarm**. Different nodes perform different local roles—including sensing, operator visualization, telemetry, RF/reference functions, local computation, multimedia, and bounded distributed work—while selected state is exchanged through explicit packet interfaces, primarily ESP-NOW in the current public implementation.

The central design feature is **modularity and extensibility**. JANUS is not intended to be a fixed ten-device appliance. The current ten-node fleet is the evidence-bearing baseline that exists now; the architecture is organized around role-preserving interfaces so that additional sensors, operator surfaces, compute devices, gateways, mobile/robotic platforms, aerial/drone payload nodes, and other authorized modules can be added without forcing every existing node to become the same hardware or firmware image.

The practical engineering question is therefore broader than simple node count: **can a new specialist join the system without rewriting every existing specialist?** JANUS attempts to make that question measurable through stable identity, declared primary mission, bounded inputs/outputs, freshness/provenance semantics, resource budgets, failure behavior, recovery behavior, and explicit authority boundaries.

The xTech effort does not begin by inventing a new swarm architecture. It begins with working firmware and physical low-cost hardware, freezes the exact ten-device configuration, measures the existing system as-is, and then—only as a separate extension test—can add one new benign module through a documented adapter to quantify integration cost and compatibility impact.

JANUS is currently **active engineering**, not a production or military-certified system. That maturity boundary is part of the proposal, not hidden from it.

The developer also has two separately developed adjacent research assets relevant to future extension:

- **JUXTAPOSE**, an exact-backed adaptive communications-search architecture previously packaged for DARPA evaluation. It is not represented as already integrated into the current swarm.
- **SkinGPT v0.3**, a laboratory tactile/thermal/mechanical sensing and data-acquisition subsystem with ESP32-S3 acquisition, thermal-zone sensing, piezo input, optional IMU/thermal-array support, telemetry, calibration, and provenance tooling. It is not represented as currently integrated into the ten-node xTech baseline.

These adjacent projects are included to show existing developer capability and realistic extension paths, not to inflate the present hardware claim.

## 2. Army Benefits — 25%

xTech|Search 10 gives strong consideration to Command and Control (C2) and Counter-C2 Networks, including resilient communications and deep sensing. JANUS is **not** presented as an operational Army C2 product, protected tactical radio, secure battlefield network, or autonomous weapons system. Its relevance is at a lower and testable layer: heterogeneous edge devices that must remain useful, observable, and extensible when connectivity and hardware composition are imperfect.

The potential Army benefit is a low-cost modular testbed for answering practical distributed-systems questions before higher-cost field integration. Rather than forcing every device into one identical role, JANUS is organized around specialized nodes with a protected local mission and bounded shared/coordinator work. Repository rules require peer/health visibility, stale-state handling, explicit sensor-truth boundaries, and inspectable recovery behavior.

The modularity model adds a second Army-relevant value: a future integrator should be able to attach a new sensing, compute, operator, or mobile-platform role through a bounded adapter rather than redesign unrelated nodes. Examples include an environmental sensor package, a mobile ground robot, an aerial/drone payload node that exposes platform health and sensor telemetry, or an actuated pan-tilt/turret-form **observation mount** that exposes position and health. These examples describe extension classes only. The present proposal does **not** claim flight-control integration, autonomous navigation, weapon control, targeting, fire control, or human-targeting authority.

That architecture creates several Army-relevant evaluation questions:

- How quickly does a lost peer become visibly stale or absent?
- Does loss of one role propagate into unrelated roles?
- Does a sensor continue its local mission when a coordinator or optional service disappears?
- Can a restored node return with the same identity without creating stale or duplicate state?
- What radio, memory, thermal, and power cost is introduced by recovery behavior?
- Can a new specialist be added while leaving unrelated nodes unchanged?
- What exact code, ABI, build, configuration, and resource changes are required for that extension?
- Which higher-level state/recovery mechanisms are tied to ESP-NOW, and which could migrate to a different transport or hardened platform later?

JUXTAPOSE adds a complementary future question: when several authorized paths or interfaces are available but current viability changes, can adaptive ordering reduce wasted checks while keeping fresh end-to-end measurement as the sole authority for present connectivity?

SkinGPT adds a different extension example: can an independently developed tactile/thermal/mechanical sensing subsystem be exposed to the swarm through a clean role/telemetry adapter while preserving its own scientific limits and provenance?

A positive result would not certify battlefield readiness. It would provide measured evidence about an inexpensive heterogeneous edge architecture and identify where later Army integration work would or would not be justified.

The same modularity has civilian dual-use potential in industrial/environmental telemetry, remote-infrastructure monitoring, disaster-response instrumentation, mobile robotics, building/field automation research, and low-cost distributed-systems evaluation.

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

### Modularity / extension contract

JANUS modularity is not defined as "any device plugs in automatically." It is defined as a bounded integration discipline.

A new module should explicitly declare:

```text
STABLE IDENTITY
+ PRIMARY LOCAL MISSION
+ AUTHORIZED INPUTS
+ AUTHORIZED OUTPUTS
+ FRESHNESS / PROVENANCE SEMANTICS
+ RESOURCE BUDGET
+ FAILURE BEHAVIOR
+ RECOVERY BEHAVIOR
+ AUTHORITY BOUNDARY
```

The platform-specific adapter maps the new hardware into that contract. The reusable portion is the role/interface discipline above the hardware.

This gives JANUS a useful falsifiable modularity test: freeze the ten-node baseline, add one new benign module, and measure whether unrelated nodes had to change. The result should report integration time, changed files, ABI/schema changes, configuration changes, resource cost, failure isolation, and recovery behavior. A failed or expensive integration would remain evidence against the modularity claim.

### Candidate extension classes

**Additional sensors.** Environmental, thermal, acoustic, inertial, mechanical, or equipment-health packages can be represented as specialist nodes if their physical measurements remain distinguishable from inference and stale state.

**Mobile/robotic and aerial/drone platforms.** A future adapter could expose platform health, position/state, payload telemetry, and bounded mission status. This is an architectural extension class, not a claim of current flight-control, autonomous navigation, or operational UAS integration.

**Actuated observation platforms.** A pan-tilt or turret-form sensor/observation mount could expose actuator state, position, health, and sensor outputs through a bounded role. This proposal explicitly excludes weapon control, autonomous targeting, fire control, and human-targeting authority.

**Compute / verification nodes.** Larger compute devices can join as bounded analysis, verification, archive, or coordination services. Existing PEA4/P4 work is relevant as a repository example of extending toward a different compute class.

**SkinGPT sensing subsystem.** SkinGPT v0.3 provides a separately implemented tactile/thermal/mechanical data-acquisition path. A future integration would map its provenance-bound telemetry into a JANUS specialist role without changing SkinGPT's scientific status or pretending its rule-based classifier is a trained GPT/neural model.

### Separate existing algorithmic capability — JUXTAPOSE

JUXTAPOSE is an **experimental transport-agnostic exact-backed adaptive search architecture for resilient communications**. It is maintained as a separate capability statement, not as a hidden current Swarm feature.

Its central rule is:

```text
LEARNING MAY CHANGE SEARCH ORDER / WIDTH
BUT
FRESH END-TO-END MEASUREMENT REMAINS CURRENT TRUTH AUTHORITY
```

If a predicted first choice fails or the environment becomes unfamiliar, JUXTAPOSE widens search. If the measurement budget is exhausted before sufficient evidence exists, it returns an explicit `UNKNOWN_RESOURCE_LIMIT` rather than fabricating `CONNECTED` or `DISCONNECTED`.

The frozen v5.1 synthetic holdout used **20,000 episodes** and passed **15/15 preregistered gates**. In that synthetic test only, validated connectivity was 98.215% versus 97.785% for a uniform-random comparison; mean mission/search-resource cost was 1.389 versus 1.914; mean exact checks were 1.340 versus 1.502; complete-outage controls produced zero false `CONNECTED`; and 82.138% of OOD episodes remained COLD/new. Those values are **not claimed as JANUS hardware performance**.

Negative lineage is preserved as part of the evidence: v1 exposed probe overhead/route thrashing; v5 exposed premature confidence in combined OOD regimes; v5.1 repaired that specific failure; no independent external replication or field validation exists yet; and the independent end-to-end gain of every individual submodule is not established in every setting.

### Why the architecture is technically interesting

JANUS does not claim that ESP-NOW or ESP32 hardware is novel. The candidate value is the combination of:

1. **modularity / extensibility as the primary architecture objective**;
2. **heterogeneous specialist roles** rather than identical nodes;
3. **role-preserving integration contracts** for new modules;
4. **protected local missions** with bounded optional/shared work;
5. **explicit current/stale/memory/inference separation**;
6. **observable failure and recovery states**;
7. **low-cost COTS hardware** suitable for repeatable failure and integration testing;
8. **inspectable source and provenance**, allowing claim-to-code review;
9. **adjacent implemented work** such as SkinGPT and JUXTAPOSE, kept separate until actual integration evidence exists.

The same heterogeneity is also a weakness: different boards, libraries, sensors, displays, radio settings, actuators, and workloads create version/ABI/resource complexity. Phase I should measure whether the integration discipline actually controls that complexity rather than assuming it does.

### Phase I work plan

**Task 1 — Freeze the ten-node reference system.** Assign immutable physical IDs, capture dated photos, board/model, node identity, firmware path, flashed commit, relevant build/library versions, sensors/peripherals, radio configuration class, and power source. Produce a hashed `TEN_NODE_TEST_MANIFEST.json`.

**Task 2 — Establish baseline behavior.** Record healthy peer visibility, physical sensor paths, local-role activity, heap/thermal state, and power where instrumentation is available.

**Task 3 — Controlled node-loss/rejoin trials.** Remove selected nodes one at a time, preserve the other devices unchanged, and timestamp last valid packet, stale/lost indication, unaffected-role behavior, first recovered packet, and return to healthy state. Repeat trials and preserve failures.

**Task 4 — Coordinator and optional-service outage trials.** Interrupt coordinator, Wi-Fi, or optional external-service paths where relevant and record exactly which local functions continue, degrade, defer, or require user intervention.

**Task 5 — Controlled 2.4 GHz impairment.** Use legal, non-destructive conditions such as distance, attenuation, shielding, coexistence load, or environmental placement to characterize packet loss and recovery. This is explicitly **not** presented as an anti-jam test.

**Task 6 — Resource-cost characterization.** Measure minimum free heap, device temperature where available, and role-specific power behavior during baseline and recovery.

**Task 7 — Modularity falsification test.** With the ten-node baseline frozen, add one benign new specialist module through a documented adapter. Record every changed file/configuration, interface/schema impact, resource cost, failure behavior, and whether unrelated baseline nodes required modification. SkinGPT is a candidate sensing-module example; another physically available sensor/compute device could be used if more practical.

**Task 8 — Reproducibility and failure ledger.** Produce raw logs, event CSVs, representative continuous video, all material failures, exact source references, and an independent replay package when a second evaluator becomes available.

**Optional Task 9 — JUXTAPOSE hardware falsification.** Only after the unchanged Swarm baseline is characterized, expose a controlled set of authorized real path/interface candidates to the separate JUXTAPOSE controller. Compare adaptive ordering with a simple baseline while preserving fresh real measurement as connectivity authority. Report both performance and any failure to transfer from synthetic to physical conditions.

### Metrics

Primary Swarm metrics include:

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

Modularity metrics should include:

- number of unrelated baseline nodes requiring firmware changes;
- changed-file and interface/schema count;
- integration/configuration effort;
- additional memory/radio/power overhead where measurable;
- compatibility failures introduced by the new module;
- loss/rejoin behavior of the new module;
- whether the new node's authority remained inside its declared boundary.

If optional JUXTAPOSE hardware validation is performed, additional metrics would include exact checks per recovered connection/path decision, search/resource cost, false-current-connectivity events, OOD/novelty handling, widening behavior after prediction failure, and matched comparison against a simple non-adaptive baseline.

Results will be reported as trial counts, distributions, and failure tables rather than only a fastest or best-case run.

### Known technical limitations

The proposal deliberately records the current weaknesses:

- the exact ten-node physical manifest is not yet frozen;
- resilience/rejoin/power numbers are not yet established under a controlled xTech benchmark;
- modularity has not been proven as universal plug-and-play across arbitrary third-party platforms;
- no current xTech evidence establishes flight-control/drone integration;
- no current xTech evidence establishes SkinGPT integration into the ten-node swarm;
- any actuated/turret-form example is limited to a future observation/sensor platform role and does not establish weapons integration;
- 2.4 GHz ESP-NOW/Wi-Fi-class transport is not a protected tactical or anti-jam radio;
- military cybersecurity, fleet key management, accreditation, and ATO are not established;
- current COTS boards are not claimed to be ruggedized or MIL-STD-qualified;
- scale beyond the tested physical fleet is unproven;
- heterogeneous firmware increases ABI/configuration/build complexity;
- some original JANUS code contains multimedia, mining, game, and research/lore functions that are not part of the Army value proposition;
- the current sensing roles must be described according to their actual sensors rather than inflated into imaging, ranging, biometric, or medical capabilities;
- JUXTAPOSE currently has **synthetic evidence only** and has not been validated on the ten-node physical Swarm.

If a controlled test reveals an additional weakness, it will be added to the evidence package rather than omitted.

## 4. Commercial Potential — 25%

JANUS uses widely available ESP32/M5Stack-class hardware and modular firmware roles, providing a low-cost path for architecture evaluation before a customer commits to a bespoke or hardened platform. The commercial value is not tied to selling one fixed ten-node kit. The stronger transition hypothesis is a reusable **role/interface architecture plus integration method** that can accept different sensors, compute devices, operator surfaces, mobile platforms, and gateways while preserving identity, freshness, bounded authority, and observable failure/recovery semantics.

This could support remote facilities, industrial/environmental monitoring, resilient local automation, temporary field instrumentation, disaster-response sensing, mobile robotics, training/evaluation kits, or OEM integration. SkinGPT illustrates a separately implemented sensing subsystem that could become one such module after a real integration test.

The current commercialization argument is based on **technical transition potential**, not invented traction. Verified customers, revenue, purchase orders, and quantified market adoption are not documented in this branch and will not be claimed without evidence.

A realistic transition path is:

```text
EXISTING PHYSICAL 10-NODE PROTOTYPE
-> FROZEN REFERENCE SYSTEM
-> MEASURED RESILIENCE
-> MEASURED MODULARITY / INTEGRATION COST
-> CLEAN ROLE + INTERFACE DOCUMENTATION
-> CUSTOMER PILOT / NEW MODULE INTEGRATION
-> HARDENED OR OEM PLATFORM IF JUSTIFIED
```

JUXTAPOSE offers a separate potential software/control-plane transition path if physical testing supports its synthetic result: a transport-agnostic route/path search-order layer that could sit above multiple authorized networking mechanisms while retaining fresh measurement as truth authority.

Possible business models include evaluation kits, paid integration/pilot engineering, licensed firmware/interface modules, or OEM/partner integration, subject to the repository's source-available licensing and separately negotiated commercial rights.

Phase I would materially improve commercial readiness by converting an organically evolved working system into a customer-readable reference configuration with measured reliability, measured extension cost, known weaknesses, reproducible tests, and clearer transport/hardware integration boundaries.

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

MODULAR ARCHITECTURE
!=
UNIVERSAL PLUG-AND-PLAY

CANDIDATE DRONE / ACTUATED / SKINGPT EXTENSION
!=
CURRENT INTEGRATION

JUXTAPOSE SYNTHETIC RESULT
!=
JANUS HARDWARE RESULT

SEPARATE EXISTING CAPABILITIES
!=
CURRENT INTEGRATION
```

Every quantitative claim will be tied to a frozen evidence domain. Failed tests will remain in the record. Source timeouts will not be reported as measured latency. Owner-reported physical facts will remain labeled as such until captured. Prediction or memory will not be reported as current sensor truth. JUXTAPOSE will not be represented as integrated into the physical Swarm until a real integration and controlled test actually occur. SkinGPT, drone/mobile-platform, and actuated-platform examples remain extension paths until a named adapter and physical test exist.

This approach is intended to give evaluators artifacts they can challenge rather than a pitch that depends on untestable language.

---

## Finalization checklist before copying into the official template

- Resolve xTech eligibility in writing.
- Obtain the official Valid Eval white-paper template.
- Replace `[PENDING ELIGIBILITY CLARIFICATION]` with the exact eligible entity name only after confirmation.
- Freeze the exact repository commit and all ten physical node identities.
- Add dated group photo/video and per-device evidence.
- Run controlled quick-demo and full ten-node loss/rejoin tests.
- Preserve **modularity/extensibility as the primary value proposition** in the compressed three-page version.
- Decide which one benign new-module integration is practical enough to test before submission or during Phase I.
- If SkinGPT is mentioned, label it as a separate existing sensing research asset and not a current Swarm integration.
- If mobile/drone or actuated-platform examples are mentioned, label them as extension classes rather than implemented capabilities.
- Decide whether the final three-page submission has enough space to mention JUXTAPOSE directly or keep it as linked supporting evidence.
- If JUXTAPOSE is mentioned, preserve the words `separate`, `synthetic evidence`, and `not currently integrated` unless later evidence changes that status.
- Insert only measured numbers that pass `CLAIM_EVIDENCE_MATRIX.md`.
- Preserve failures and limitations in the submission evidence set.
- Add real customer/commercial evidence only if documentary support exists.
- Compress this master source into the official three-page template without dropping the central limitations or claim firewall.