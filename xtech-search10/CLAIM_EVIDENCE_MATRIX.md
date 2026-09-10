# xTech|Search 10 — Claim / Evidence Matrix

This matrix is the submission firewall. A statement belongs in the white paper only when its status and evidence are clear.

## Status vocabulary

- **SUPPORTED** — directly supported by current repository material.
- **SUPPORTED IN CODE** — implementation path exists, but performance is not yet measured.
- **SUPPORTED ARCHITECTURE** — documented design/invariant; requires testing before reliability numbers are claimed.
- **OWNER-REPORTED** — stated by the developer but not yet frozen into submission evidence.
- **PROPOSED TEST / PHASE I OBJECTIVE** — future work, not current performance.
- **NOT ESTABLISHED** — evidence is insufficient; do not claim.
- **OUT OF SCOPE / NOT CLAIMED** — deliberately excluded from the xTech proposition.

## Core technical claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JANUS is a heterogeneous ESP32/M5Stack firmware project | **SUPPORTED** | `README.md`, `PROJECT_MAP.md`, `docs/architecture.md` | May state directly |
| The repository contains multiple active specialist firmware roles | **SUPPORTED** | `PROJECT_MAP.md`, `docs/current-swarm-state.md` | May state directly; do not equate repository role count with physical fleet size |
| Multiple node roles exchange selected state through ESP-NOW | **SUPPORTED** | `docs/architecture.md`, current firmware entrypoints | May state directly for supported paths |
| Current architecture exposes heartbeat / peer visibility / stale-node requirements | **SUPPORTED** | `docs/current-swarm-state.md`, `PROJECT_STATUS.json` | May state directly as architecture/current requirements |
| Nodes are intended to preserve a protected primary mission while coordinator side-work is bounded | **SUPPORTED ARCHITECTURE** | `docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`, `PROJECT_STATUS.json` | State as design rule; test continuity before claiming reliability |
| Sensor truth is required to remain distinguishable from prediction, memory, stale remote state, and UI fiction | **SUPPORTED** | `docs/technical-boundaries.md`, project invariants | May state directly |
| Blind Eye uses STHS34PF80 TMOS/PIR-style sensing as primary sensing on the current camera-absent profile | **SUPPORTED** | `firmware/blind_eye/BLIND_EYE.ino`, `docs/current-swarm-state.md` | May state directly |
| Blind Eye is a thermal camera | **NOT ESTABLISHED / INCORRECT FOR CURRENT PROFILE** | Current profile explicitly treats camera as absent | Do not claim |
| Anchor contains reconnect / rejoin / radio-blackout handling paths | **SUPPORTED IN CODE** | `firmware/anchor/Anchor.ino`, `docs/current-swarm-state.md` | State as implemented code path; measure performance separately |
| Core2 provides a human-facing swarm/telemetry surface | **SUPPORTED** | `firmware/core2/CORE2.ino`, `docs/architecture.md` | May state directly |
| Buzz contains coordinator/worker and recovery lineage | **SUPPORTED** | `firmware/buzz/Buzz.ino`, `PROJECT_STATUS.json` | May state as background engineering; mining is not the Army capability |
| Current public firmware includes larger compute/verification tracks such as PEA4/P4 | **SUPPORTED** | `PROJECT_MAP.md`, `docs/current-swarm-state.md` | May state as repository capability/extension; do not imply all are physically deployed |

## Physical-system claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| The developer currently has a ten-node physical JANUS swarm | **OWNER-REPORTED** | Developer statement; repository firmware lineage | May state as `developer-reported 10-node physical swarm` until physical evidence is frozen |
| All ten physical nodes are already mapped in this xTech branch to exact boards/firmware SHAs | **NOT ESTABLISHED** | `TEN_NODE_PHYSICAL_SYSTEM.md` intentionally leaves device slots pending | Do not claim until hardware capture is complete |
| The repository contains more firmware roles than the ten physical nodes | **SUPPORTED** | `PROJECT_MAP.md` | May state; explain that active/preserve/migration roles are not equal to physical count |
| The quick four/five-node demo is the whole JANUS swarm | **FALSE / OUT OF SCOPE** | Physical count statement and xTech docs | Never imply; quick demo is only a reviewer subset |
| The ten-node fleet proves arbitrary scale | **NOT ESTABLISHED** | No 50/100/1000-node validation | Do not claim; state tested fleet size only |

## Resilience/performance claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JANUS contains code paths intended to expose loss/stale state and recovery | **SUPPORTED IN CODE** | current firmware + docs | May state directly as implementation |
| JANUS always survives arbitrary single-node loss | **NOT ESTABLISHED** | No frozen xTech benchmark | Do not claim; make it a test question |
| Unaffected local roles continue during coordinator/peer loss | **SUPPORTED ARCHITECTURE / PROPOSED TEST** | primary-mission doctrine | Claim only after exact test confirms for named roles |
| JANUS detects a lost peer within a specific number of seconds | **NOT ESTABLISHED** | code timeouts exist, but no controlled benchmark | Do not infer a performance number from constants |
| JANUS rejoins within a specific number of seconds | **NOT ESTABLISHED** | recovery logic exists, benchmark absent | Do not claim number until repeated measurements |
| Stale information is never shown as current | **DESIGN TARGET; NOT YET QUANTIFIED** | technical truth boundary | Use `target = 0 stale-as-current events`; promote only after test |
| Packet-loss tolerance is known | **NOT ESTABLISHED** | no frozen impairment dataset | Measure under defined conditions first |
| Recovery success rate is known | **NOT ESTABLISHED** | no repeated xTech trial set | Report only after repeated trials |
| Full ten-node continuity has been independently replayed | **NOT ESTABLISHED** | no independent xTech replay | Do not claim |

## RF / communications claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| Current public implementation primarily uses ESP-NOW/Wi-Fi-class 2.4 GHz transport for selected peer state | **SUPPORTED** | firmware and docs | May state directly |
| JANUS is a general-purpose multi-hop mesh router | **NOT ESTABLISHED AS XTECH BASELINE** | Current package primarily documents peer/coordinator ESP-NOW patterns | Prefer `swarm`, `network`, or `distributed edge system`; claim multi-hop only if frozen firmware proves it |
| JANUS is anti-jam / jam resistant | **NOT ESTABLISHED** | No validated anti-jam test | Do not claim |
| JANUS provides contested-spectrum superiority | **NOT ESTABLISHED** | No validated EW test | Do not claim |
| JANUS is LPI/LPD or protected tactical RF | **NOT ESTABLISHED** | No evidence | Do not claim |
| Current transport can be experimentally impaired and recovery measured | **PROPOSED TEST** | Low-cost radio + recovery paths | Appropriate Phase I/evaluation objective |

## Security claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| Public xTech branch follows secret-separation rules | **SUPPORTED POLICY** | repository docs, placeholder configuration policy | May state as repository practice |
| JANUS is a secure military communications system | **NOT ESTABLISHED** | No military security accreditation | Do not claim |
| JANUS has DoD-accredited key management / ATO | **NOT ESTABLISHED** | No evidence | Do not claim |
| Complete fleet secure boot/provisioning has been independently verified | **NOT ESTABLISHED** | No frozen fleet audit | Do not claim |
| Security hardening can be treated as a later integration requirement | **SCOPED FUTURE WORK** | Architecture/interface framing | May state as future work, not present capability |

## Hardware / deployment claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JANUS uses widely available ESP32/M5Stack-class COTS hardware | **SUPPORTED** | repository firmware targets | May state directly |
| Exact total system cost is known | **NOT ESTABLISHED** | xTech BOM not frozen | Do not quote a total price until measured/current BOM exists |
| Current hardware is MIL-STD-qualified/ruggedized | **NOT ESTABLISHED** | No qualification evidence | Do not claim |
| Current hardware is production safety certified as a complete system | **NOT ESTABLISHED** | No certification | Do not claim |
| Per-node power/endurance is known | **NOT ESTABLISHED SYSTEM-WIDE** | mixed hardware; incomplete measurement | Measure role by role |
| Low-cost hardware is suitable for rapid architecture/failure evaluation | **REASONABLE ENGINEERING FRAMING** | COTS platform characteristics + current implementation | May state as evaluation advantage, not operational-field guarantee |

## Sensing / inference claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| Blind Eye has real TMOS/PIR-derived physical sensing | **SUPPORTED** | current Blind Eye firmware | May state directly |
| RF-lite/RF-dome experimental paths exist | **SUPPORTED IN CODE / EXPERIMENTAL** | firmware/docs | May mention as experimental lineage; do not claim calibrated human detection performance |
| JANUS performs verified biometric identification | **NOT ESTABLISHED** | No evidence | Do not claim |
| JANUS provides precision ranging from current Blind Eye sensor path | **NOT ESTABLISHED** | No validated ranging evidence | Do not claim |
| Prediction/inference is equivalent to sensor truth | **FORBIDDEN BY PROJECT BOUNDARY** | `docs/technical-boundaries.md` | Never claim |

## BLE / automation interoperability claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| Bluetooth functionality exists in some JANUS firmware lineages | **PARTIAL / DEVICE-SPECIFIC** | historical/current device-specific code paths | State only for the exact verified node/path |
| BLE interoperability is part of the frozen xTech baseline | **NOT YET FROZEN** | no exact test configuration | Include only after device/firmware capture |
| ESPHome/Home Assistant integration is implemented in the current public xTech baseline | **NOT ESTABLISHED IN CURRENT PUBLIC BASELINE** | sufficient public evidence not yet located/frozen | Do not claim until exact implementation is identified and bound to a test |

## Commercial / business claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JANUS has plausible dual-use paths in industrial/environmental telemetry, remote infrastructure, disaster-response instrumentation, and local automation research | **APPLICATION MAPPING** | architecture fit | May state as potential use cases |
| JANUS has verified paying customers | **NOT DOCUMENTED** | no frozen customer evidence | Do not invent |
| JANUS has verified revenue | **NOT DOCUMENTED** | no frozen revenue evidence | Do not invent |
| JANUS has proven strong commercial traction | **NOT DOCUMENTED** | no evidence package | Do not claim |
| Phase I could improve commercialization readiness by producing reproducible test/evaluation documentation | **PROPOSED OUTCOME** | submission plan | May state as proposed benefit |

## Army / xTech claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| The project maps to xTech C2 / Counter-C2 interests such as resilient communications and deep sensing | **RELEVANCE MAPPING, NOT PERFORMANCE CLAIM** | official RFI + JANUS architecture | May explain potential relevance without calling JANUS an operational C2 product |
| JANUS is already integrated with Army systems | **NOT ESTABLISHED** | no evidence | Do not claim |
| JANUS is combat-ready / battlefield-proven | **NOT ESTABLISHED** | no deployment evidence | Do not claim |
| JANUS provides autonomous weapon or human-targeting authority | **OUT OF SCOPE / NOT CLAIMED** | project boundary | Do not claim or propose as current capability |
| The developer/entity is eligible for xTech|Search 10 direct submission | **PENDING OFFICIAL CLARIFICATION** | detailed RFI vs conflicting website summary | Do not claim until Army replies |

## Preferred vocabulary

Use:

- `existing firmware`
- `developer-reported ten-node physical swarm`
- `active-engineering prototype`
- `heterogeneous low-cost edge nodes`
- `distributed sensor and telemetry network`
- `ESP-NOW peer/coordinator communication`
- `observable heartbeat / freshness / stale state / reconnect paths`
- `controlled disruption test`
- `candidate Army application`
- `dual-use potential`
- `implemented code path`
- `not yet measured`
- `evidence gap`

Avoid unless separately proven:

- `combat AI`
- `battlefield proven`
- `anti-jam`
- `military-grade secure`
- `mesh routing` when only peer/coordinator behavior is demonstrated
- `autonomous tactical authority`
- `AGI`
- `superiority`
- `guaranteed resilience`
- `proven scalable`
- `thermal camera` for the current camera-absent Blind Eye profile

## Promotion rule

```text
OBSERVATION / OWNER REPORT
        ->
REPOSITORY-VERIFIABLE CODE OR HARDWARE CAPTURE
        ->
FROZEN HARDWARE/SOFTWARE CONFIGURATION
        ->
TIMESTAMPED CONTROLLED TEST
        ->
RAW LOG / VIDEO / RECEIPT
        ->
REPEATED TRIALS
        ->
FAILURE TABLE
        ->
INDEPENDENT REPLAY WHERE POSSIBLE
        ->
QUANTITATIVE SUBMISSION CLAIM
```

No intermediate step may be silently skipped.

## Correction rule

If a later test contradicts a current assumption, the matrix must be updated to the weaker status. Failed evidence is preserved; it is not removed to protect the pitch.