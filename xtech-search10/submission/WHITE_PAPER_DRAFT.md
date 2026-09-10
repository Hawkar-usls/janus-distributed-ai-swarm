# xTech|Search 10 — Concept White Paper Working Draft

> **WORKING SOURCE ONLY — NOT THE OFFICIAL SUBMISSION FORMAT.**  
> The final submission must be transferred into the official Valid Eval `Template_xTech_Search10_White_Paper.docx` and kept to exactly three pages. Eligibility must be resolved before submission.

**Company / eligible entity:** `[PENDING ELIGIBILITY CLARIFICATION]`  
**Proposal title:** **JANUS Resilient Edge Swarm: Low-Cost Heterogeneous Sensor and Telemetry Mesh for Degraded Networks**  
**Technology owner/developer:** Hawkar, independent researcher/developer, Ukraine  
**Repository:** `Hawkar-usls/janus-distributed-ai-swarm`

---

## 1. Introduction — 5%

JANUS Resilient Edge Swarm is an existing heterogeneous ESP32/M5Stack firmware system deployed by its developer as a **10-node physical swarm**. It combines low-cost sensing, telemetry, local processing, operator visualization and peer/coordinator recovery behavior across specialized edge nodes. The project is currently classified as active engineering, not a production or military-certified system.

The proposed xTech effort does not begin by inventing a new swarm architecture. It begins with working firmware and physical low-cost hardware, freezes a reproducible configuration, and measures a narrow question: **how well can heterogeneous edge nodes preserve useful local functions, expose loss/stale state, and recover connectivity when peers or network paths are disrupted?**

The technology under review is the ten-node JANUS swarm. For a short first demonstration, a focused subset of existing Core2, Blind Eye, Anchor and ATOM SWARM TRON firmware can be used, with Buzz available as an optional coordinator/workload node. That subset is a demonstration configuration, not the size of the system. The project primarily uses ESP-NOW for selected peer state in the current public implementation and preserves explicit boundaries between current sensor truth, memory, prediction and UI state.

## 2. Army Benefits — 25%

xTech|Search 10 gives strong consideration to Command and Control (C2) and Counter-C2 Networks, including resilient communications and deep sensing. JANUS is **not** presented as an operational Army C2 system. Its relevance is a lower-level edge architecture that can be independently evaluated as a building block for distributed sensing and local telemetry under degraded connectivity.

The potential Army benefit is a low-cost, modular testbed for exploring how non-identical edge devices behave when communication becomes intermittent. Rather than requiring every device to perform the same role, JANUS nodes preserve specialized local functions while sharing bounded state. Existing architecture rules require heartbeat/health visibility, explicit stale-node handling, protected primary missions, bounded coordinator side-work, and separation of sensor truth from inferred or remembered state.

This creates a useful evaluation pattern for distributed sensing: an operator can observe which nodes are healthy, stale or recovering; a sensing node can retain its local mission when optional coordinator functions disappear; and recovery paths can be measured rather than hidden behind a single aggregate status.

Potential Army-relevant evaluation questions include: time to identify a lost peer, time to rejoin after restoration, frequency of stale information being mistaken for current state, continuity of unaffected local sensing during coordinator/path disruption, and resource/power cost of the recovery mechanisms.

The same architecture has civilian dual-use potential in industrial and environmental telemetry, remote-infrastructure monitoring, disaster-response sensor deployment, local building/field instrumentation and resilient maker/education edge networks.

## 3. Technical Approach — 40%

### Existing technical baseline

The current repository contains multiple ESP32/M5Stack firmware roles and documents ESP-NOW packet/ABI handling, telemetry, heartbeat/state visibility, recovery behavior and explicit technical boundaries. The developer reports ten physical JANUS nodes in the current swarm; the final submission inventory will freeze the exact board/model, firmware path and flashed commit for each physical device.

The preferred **first demonstration subset** is:

**Core2 — operator/telemetry surface.** Existing firmware provides a dense human-facing view of swarm state and selected sensor/peer information.

**Blind Eye — physical sensing node.** The current AtomS3R-class profile uses an STHS34PF80 TMOS/PIR sensor as its primary eye, treats the absence of a camera as a normal hardware profile, and separates physical sensor state from memory/prediction/UI semantics.

**Anchor — RF/recovery reference node.** Existing firmware contains heartbeat, ESP-NOW handling, reconnect/rejoin and radio-blackout safeguards intended to keep node state and recovery observable.

**ATOM SWARM TRON — heterogeneous peer.** This separate Atom-class firmware lineage demonstrates that the architecture is not limited to identical devices or identical payloads.

Buzz may be included when coordinator/workload behavior is useful. Its mining-related code is background engineering rather than the proposed Army capability. The remaining physical JANUS nodes remain part of the system and can be included in later or full-swarm trials; they are not removed from the technology claim simply because the first reviewer demo uses a smaller subset.

### Phase I measurement plan

If selected for a Phase I effort, the first task would be to freeze one exact ten-node hardware/software configuration: board models, firmware commits, library versions, radio configuration, sensors and power sources. No performance number would be claimed from source code alone.

Controlled trials would then run staged conditions: normal full-swarm baseline operation; physical removal/isolation of selected nodes; coordinator or network-path disruption where applicable; and restoration/rejoin. Timestamped logs and continuous video would record the last valid heartbeat, stale/lost indication, unaffected-node behavior, first recovered packet and return to healthy peer state.

Primary metrics would include `T_detect_loss`, `T_rejoin`, valid packet reception rate, stale-state errors, primary-mission continuity, minimum free heap and—where instrumentation is available—power draw and device temperature. Repeated trials would establish distributions rather than a single best-case result.

The final technical output would be a reproducible evaluation package: frozen ten-node manifest, logs, event CSV, test procedure, observed failures and a concise interface description for later customer integration.

### Technical boundaries

The proposal does not claim anti-jam capability, secure tactical networking certification, battlefield readiness, autonomous weapon authority, AGI, precognition or combat superiority. If a controlled test fails, that failure remains part of the evidence and becomes a defined engineering target rather than being removed from the result set.

## 4. Commercial Potential — 25%

JANUS uses widely available ESP32/M5Stack-class hardware and modular firmware roles, creating a low-cost path to experimentation and deployment in non-defense edge-sensing environments. The same core pattern—specialized local nodes, selected peer state, heartbeat/stale visibility and recovery—can be relevant to remote facilities, industrial/environmental monitoring, resilient local automation, field instrumentation and disaster-response deployments where replacing an entire centralized system is undesirable.

At present, **verified commercial customers, revenue and quantified market traction are not documented in the public repository and will not be invented for this submission**. The owner reports physically operating the ten-node JANUS swarm for sensing, multimedia and compute/nerd-mining experiments; dated physical evidence and exact hardware manifests should be added before final submission.

A realistic commercialization path would package the current engineering into a documented hardware-agnostic edge-node framework, reproducible evaluation kit and integration interface rather than selling the existing project lore as a finished product. Potential business models could include evaluation kits, integration/pilot engineering, licensed firmware modules or OEM/partner integration, subject to the repository's source-available licensing and any separately negotiated rights.

Phase I would materially improve commercial readiness by converting existing code paths into measured reliability data, a frozen reference configuration and a customer-readable interface/test package.

## 5. Proposal Quality — 5%

This proposal intentionally distinguishes source-code implementation, measured evidence, proposed Phase I work and future Army application. Every quantitative claim will be tied to a frozen firmware/hardware configuration and timestamped test evidence. Existing repository claim boundaries remain in force throughout the xTech submission.

---

## Finalization checklist before copying into the official template

- Resolve xTech eligibility in writing.
- Obtain the official Valid Eval white-paper template.
- Replace `[PENDING ELIGIBILITY CLARIFICATION]` with the exact eligible entity name only after confirmed.
- Freeze the exact repository commit and **all ten physical node identities**.
- Add dated group photos/video evidence of the physical swarm.
- Run controlled full-swarm and selected-node loss/rejoin tests and insert only measured numbers that pass the claim-evidence gate.
- Add any real commercial/customer evidence if available; otherwise preserve the current limitation statement.
- Compress/edit into the official three-page template without dropping claim boundaries.
