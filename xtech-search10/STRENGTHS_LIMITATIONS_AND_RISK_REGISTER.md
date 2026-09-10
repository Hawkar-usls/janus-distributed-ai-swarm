# JANUS Resilient Edge Swarm — Strengths, Limitations, and Risk Register

## Purpose

This document is intentionally candid. It records what makes JANUS worth evaluating, what is already implemented, what remains unproven, and what could fail in an xTech evaluation.

The goal is not to make the prototype sound more mature than it is. The goal is to make the engineering state legible enough that an external evaluator can decide what is interesting, what is risky, and what should be tested next.

## Executive assessment

JANUS is an **active-engineering heterogeneous ESP32/M5Stack swarm** with a developer-reported **10-node physical deployment** and a larger public firmware surface containing multiple active and compatibility roles. Its strongest feature is not a single sensor or radio trick. Its strongest feature is the architecture around unlike low-cost nodes: specialized local missions, selected state exchange, explicit heartbeat/staleness, bounded coordinator side-work, observable recovery paths, and a deliberate separation between physical sensor truth and prediction/memory/UI state.

Its largest weakness is equally clear: the current public project is a sophisticated prototype/research system, **not a field-hardened military product**. The exact ten-device physical manifest has not yet been frozen into the xTech evidence package, quantitative resilience benchmarks are not yet collected under a controlled xTech protocol, and no anti-jam, military cybersecurity, environmental qualification, production-readiness, or commercial-traction claim is established.

That combination is why the project is appropriate for evaluation: there is enough real implementation to test, while the remaining uncertainty is concrete and measurable.

---

## Strengths

### 1. A physical system exists before the competition effort

The xTech concept does not begin as a paper architecture. The developer reports a ten-node physical JANUS swarm used for sensing, multimedia, compute/nerd-mining experiments, and distributed firmware work. The public repository contains the firmware lineages behind this system.

**Why this matters:** a Phase I effort can begin by freezing and measuring an existing system rather than spending the first months only creating a first prototype.

**Evidence status:** physical existence is owner-reported until dated photos, exact board identities, flashed commit hashes, and a test receipt are captured in the xTech package.

### 2. Heterogeneous nodes instead of identical copies

The repository contains multiple active roles: Core2, Buzz, BlackStar/BH, ADV Elite, Yaks Gate, Anchor, Gladius, Golcron, Zim, Blind Eye, Pyramid, PEA4/P4, plus compatibility/preserve firmware such as Beacon, Stick, and ATOM SWARM TRON.

The architectural idea is that a node keeps its own specialization while participating in a shared swarm. This is more representative of real edge environments than assuming every device has the same processor, screen, sensors, power budget, and mission.

**Potential benefit:** existing sensors, displays, radios, and specialized edge devices can be integrated incrementally rather than replaced by one uniform platform.

### 3. Primary mission is explicitly protected

JANUS uses a repository-wide `PRIMARY_MISSION + BOUNDED_SIDE_QUESTS` doctrine. Optional coordinator or shared work is expected to yield before the node's protected local role is silently starved.

**Potential benefit:** graceful degradation can be expressed at the firmware-policy level. A sensor should remain a sensor even if optional coordinator services disappear.

**Important limit:** this is an implemented architecture rule, not yet a quantified guarantee across all ten physical nodes. It must be verified under controlled disruption.

### 4. Failure and staleness are first-class states

The repository requires heartbeat/health visibility, stale-peer handling, reconnect/rejoin logic, and explicit distinction between current, stale, remembered, inferred, and presentation state.

**Potential benefit:** an operator or downstream system can know that information is old or a peer is missing instead of receiving a deceptively clean aggregate picture.

### 5. Sensor truth is separated from inference and UI fiction

The public technical boundary explicitly forbids prediction or memory from being silently presented as current sensor truth. Blind Eye is the clearest example: its camera-absent profile treats absence of a camera as a real hardware fact and uses TMOS/PIR as the primary physical sensor path.

**Potential benefit:** this creates a cleaner trust boundary for later fusion, automation, or human review.

### 6. Recovery behavior exists in firmware rather than only in diagrams

Current firmware includes reconnect, radio-rescue, rejoin, channel-reassertion, queueing, stale-node, watchdog, and fallback paths in different nodes.

**Potential benefit:** the xTech effort can test actual failure/recovery code rather than merely proposing that resilience will be added later.

### 7. Low-cost, widely available hardware class

JANUS is built primarily around ESP32/M5Stack-class commercial hardware rather than a bespoke processor platform.

**Potential benefit:** low barrier to replication, rapid iteration, replaceable nodes, and inexpensive destructive/failure testing.

**Important limit:** no exact xTech bill of materials or per-node cost is frozen yet, so the submission should not quote a total system price until the physical manifest is captured.

### 8. Inspectable implementation and provenance

The public repository exposes current firmware, architecture documents, technical boundaries, audits, and project status. The xTech branch adds a claim/evidence matrix rather than hiding uncertainty.

**Potential benefit:** reviewers can trace a statement back to code or a test artifact and can see which claims are intentionally withheld.

### 9. Dual-use architecture

The same edge pattern can plausibly be evaluated for industrial/environmental telemetry, remote infrastructure, disaster-response instrumentation, resilient building/field sensing, and educational/maker edge networks.

**Important limit:** these are application paths, not evidence of existing customers or revenue.

---

## Current weaknesses and limitations

### L1 — Exact ten-node physical manifest is not yet frozen

**State:** OPEN EVIDENCE GAP.

We know the developer reports ten physical nodes, but this xTech branch does not yet bind all ten physical devices to exact board model, node identity, firmware path, flashed commit, sensor/peripheral set, and power source.

**Why it matters:** without that freeze, a reviewer cannot reproduce the exact system being demonstrated.

**Mitigation:** create a dated `TEN_NODE_TEST_MANIFEST.json` from the actual powered devices before any quantitative submission claim.

### L2 — Quantitative resilience performance is not yet established

**State:** OPEN TEST GAP.

Timeout constants and recovery logic in source code are not measurements. There is not yet a frozen xTech dataset for loss-detection time, rejoin time, packet success under impairment, stale-state errors, or full-swarm continuity.

**Mitigation:** repeated controlled trials with timestamps, raw logs, video, and distributions rather than a best-case number.

### L3 — 2.4 GHz ESP-NOW is not a tactical anti-jam radio

**State:** KNOWN TECHNOLOGY BOUNDARY.

ESP-NOW/Wi-Fi-class radios are useful for low-cost local mesh experiments but should not be described as jam-resistant, long-range tactical communications, LPI/LPD, or protected military RF.

**Risks:** interference, channel contention, multipath, coexistence with Wi-Fi, limited range, and environment-dependent packet loss.

**Mitigation:** present ESP-NOW as the current low-cost transport used to evaluate higher-level node behavior. Measure RF impairment honestly. Treat later transport substitution as an integration question, not as something already solved.

### L4 — Military cybersecurity is not established

**State:** OPEN SECURITY GAP.

The repository does not establish a DoD-authorized security architecture, military key management, accredited cryptography, secure boot/provisioning across the complete physical fleet, penetration-test results, or an Authority to Operate.

**Mitigation:** do not use `military-grade secure`. Document the present trust boundary and packet interfaces; isolate secrets from the public branch; make security hardening a later integration workstream if required by a customer.

### L5 — Commercial/maker hardware is not environmentally qualified

**State:** KNOWN HARDWARE GAP.

The current devices are not claimed to be MIL-STD qualified, ruggedized, waterproof, shock-qualified, vibration-qualified, EMI/EMC certified as a complete system, or temperature-qualified for field deployment.

**Mitigation:** use the existing hardware as an inexpensive architecture demonstrator. If the architecture proves useful, port the same role/protocol concepts to an appropriate rugged platform.

### L6 — Power/endurance budget is not yet frozen

**State:** OPEN MEASUREMENT GAP.

Different JANUS nodes have different displays, sensors, radios, audio paths, and workloads. A single power number would be misleading.

**Mitigation:** measure idle/active/recovery power per selected node and report the distribution. Treat power as a role-specific engineering trade rather than one headline value.

### L7 — Scale beyond the physical ten-node system is unproven

**State:** OPEN SCALING GAP.

The public firmware surface contains more roles than the reported physical ten-node deployment, but this is not evidence that the architecture has been validated at 50, 100, or 1,000 simultaneously active nodes.

**Mitigation:** claim only the actual tested fleet size. If scaling matters, add staged emulation/hardware-in-the-loop tests after the ten-node baseline is frozen.

### L8 — Heterogeneity creates integration and configuration complexity

**State:** REAL ENGINEERING TRADE-OFF.

The same diversity that makes JANUS interesting also increases firmware-version, ABI, library, board-support, power, and radio-configuration complexity.

**Failure modes:** packet-structure drift, stale compatibility snapshots, inconsistent channel settings, library regressions, and board-specific behavior.

**Mitigation:** freeze packet/ABI definitions, version every node, keep compatibility tests, record exact build environments, and use a manifest for each evaluation run.

### L9 — Some JANUS firmware contains research/lore or multimedia functions that are not part of the Army value proposition

**State:** PRESENTATION RISK.

The repository intentionally preserves experimental vocabulary, games, multimedia, mining, and research lineages. Those are part of project provenance but can distract an evaluator from the bounded edge-network engineering claim.

**Mitigation:** do not rewrite or erase the original project. Use this xTech directory as a clean reviewer surface that maps each relevant claim to the original firmware while clearly labeling unrelated functionality as background engineering.

### L10 — Sensing capability is role-specific and limited by the physical sensor

**State:** KNOWN SENSOR BOUNDARY.

For example, Blind Eye's current STHS34PF80 path is a real TMOS/PIR-style presence/motion sensing channel, not a thermal camera, precision imager, biometric identifier, or precision ranging sensor.

**Mitigation:** state exactly what the sensor measures, preserve `sensor truth != inference`, and quantify only what controlled calibration supports.

### L11 — Optional external services can fail

**State:** TESTABLE DEPENDENCY RISK.

Some firmware lineages include Wi-Fi, NAS, Stratum, or coordinator-connected functions. The design doctrine says optional/shared work should degrade before the protected local mission, but this must be demonstrated for the selected physical configuration.

**Mitigation:** explicitly test local-only behavior with Wi-Fi/coordinator/NAS paths interrupted and record which functions continue, pause, or fail.

### L12 — Commercial traction is not yet documented

**State:** BUSINESS GAP.

No verified customer, revenue, purchase order, or paid pilot evidence is currently frozen in this xTech branch.

**Mitigation:** do not manufacture traction. Position Phase I as converting working engineering into measured reliability, a reproducible evaluation kit, and integration documentation that can support later government or commercial pilots.

### L13 — xTech applicant eligibility is unresolved

**State:** ADMINISTRATIVE BLOCKER.

The developer is based in Ukraine. Detailed RFI language and a conflicting website summary require written clarification from the Army FUZE xTech Program.

**Mitigation:** do not submit through an ineligible entity or misstate ownership. Wait for written guidance and follow the permitted direct or partner route.

---

## Risk register

| ID | Risk | Likelihood before testing | Impact | Current evidence | Reduction path |
| --- | --- | --- | --- | --- | --- |
| R1 | Node does not rejoin after induced outage | Medium / unknown | High | Recovery paths exist in firmware | Repeated hard-loss/rejoin trials; preserve failures |
| R2 | Stale peer state remains visible too long | Medium / unknown | High | TTL/stale rules documented | Timestamp last packet vs stale indication; target zero false-current events |
| R3 | Coordinator loss starves a local role | Unknown | High | Protected-primary-mission doctrine | Interrupt coordinator and measure local-role continuity |
| R4 | 2.4 GHz interference causes unstable mesh | Medium | Medium/High | Transport is ESP-NOW/Wi-Fi class | Controlled interference/attenuation tests; log packet rate and recovery |
| R5 | Firmware/version mismatch breaks packet ABI | Medium | High | ABI is explicitly treated as a compatibility boundary | Freeze versions, packet sizes, schema IDs, compile/static checks |
| R6 | Display/audio/compute load affects networking | Medium | Medium | Multi-function nodes exist; throttling paths present | Stress tests with representative concurrent workload |
| R7 | Power/thermal limits trigger instability | Unknown | Medium/High | Per-node guards exist in some firmware | Measure current, voltage, temperature, heap during tests |
| R8 | Sensor inference is mistaken for physical truth | Low by design, unquantified | High | Explicit technical firewall | Test stale/inferred/current labels; preserve raw sensor channel |
| R9 | Security expectations exceed prototype scope | High if phrasing is vague | High | No military security certification claimed | Explicit security boundary and later hardening plan |
| R10 | Reviewer is distracted by unrelated project lore | Medium | Medium | Original firmware intentionally preserves lineage | xTech reviewer path, bounded vocabulary, claim-evidence mapping |
| R11 | Ten-node physical configuration cannot be reproduced exactly | Medium until frozen | High | Count is owner-reported | Device-by-device manifest + photos + flashed SHA + build versions |
| R12 | Eligibility prevents direct submission | Unknown | High | Written clarification pending | Follow Army guidance; compliant partner route only if permitted |

---

## What would falsify or weaken the xTech proposition?

The project should be considered weaker if controlled tests show any of the following and no bounded remediation exists:

- a single non-critical node failure repeatedly collapses unrelated local functions;
- stale data is routinely presented as current after peer loss;
- rejoin behavior is inconsistent or requires manual reflashing/reconfiguration;
- recovery traffic overwhelms useful sensing/telemetry under realistic impairment;
- the ten-node system cannot be frozen and reproduced from documented firmware/build inputs;
- resource, power, or thermal costs make the proposed edge behavior impractical for the intended role;
- the current architecture cannot cleanly separate transport-specific logic from higher-level node state/recovery behavior.

These are not findings today. They are explicit ways the hypothesis can fail.

## What would strengthen the proposition?

The strongest evidence would be boring, repeatable evidence:

1. exact ten-node device/firmware manifest;
2. continuous video and raw logs of baseline -> disruption -> stale/loss -> continued unaffected roles -> rejoin;
3. multiple trials with distributions for detection/rejoin and packet success;
4. an honest failure table, including runs that did not recover cleanly;
5. per-node power/thermal/resource measurements;
6. a transport/interface description showing what is ESP-NOW-specific and what is general node-state/recovery logic;
7. an independent replay by a second evaluator from the frozen package.

## Disclosure rule

```text
A WEAKNESS THAT IS KNOWN AND MEASURED
IS MORE USEFUL THAN A STRENGTH THAT IS ONLY ASSERTED.
```

This xTech package therefore treats limitations as part of the engineering evidence, not as material to conceal.