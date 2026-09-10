# xTech|Search 10 — Claim / Evidence Matrix

This matrix is the submission firewall. A statement belongs in the white paper only when its status and evidence are clear.

## Status vocabulary

- **SUPPORTED** — directly supported by current repository material.
- **SUPPORTED IN CODE** — implementation path exists, but performance is not yet measured.
- **SUPPORTED ARCHITECTURE** — documented design/invariant; requires testing before reliability numbers are claimed.
- **OWNER-REPORTED** — stated by the developer but not yet frozen into submission evidence.
- **SUPPORTED IN SEPARATE RESEARCH ASSET** — supported by the separately preserved JUXTAPOSE package, not by current Swarm firmware.
- **SYNTHETIC EVIDENCE ONLY** — measured in a frozen synthetic test; not a physical-hardware or field result.
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

## JUXTAPOSE claims

JUXTAPOSE is a **separate existing research asset**, documented in `JUXTAPOSE_CAPABILITY_AND_SWARM_RELATION.md`. It must not be silently promoted into current Swarm functionality.

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JUXTAPOSE is an exact-backed adaptive communications-search architecture | **SUPPORTED IN SEPARATE RESEARCH ASSET** | frozen JUXTAPOSE/DARPA package | May state as separate existing developer capability |
| JUXTAPOSE is transport-agnostic at the decision/search layer | **SUPPORTED IN SEPARATE RESEARCH ASSET** | JUXTAPOSE architecture | May state as architecture; transport-specific performance still requires validation |
| JUXTAPOSE may reorder authorized candidate checks and search width | **SUPPORTED IN SEPARATE RESEARCH ASSET** | JUXTAPOSE decision contract | May state directly |
| JUXTAPOSE prediction is allowed to declare current connectivity without a fresh check | **FORBIDDEN BY JUXTAPOSE BOUNDARY** | exact-measurement authority rule | Never claim |
| JUXTAPOSE returns `UNKNOWN_RESOURCE_LIMIT` when evidence budget is exhausted with unresolved candidates | **SUPPORTED IN SEPARATE RESEARCH ASSET** | JUXTAPOSE decision contract | May state as algorithm semantics |
| JUXTAPOSE v5.1 was evaluated on 20,000 frozen synthetic episodes and passed 15/15 preregistered gates | **SYNTHETIC EVIDENCE ONLY** | frozen v5.1 package | May state only with `synthetic` / `local twin` qualifier |
| JUXTAPOSE v5.1 achieved 98.215% validated connectivity vs 97.785% uniform-random in its frozen synthetic holdout | **SYNTHETIC EVIDENCE ONLY** | frozen v5.1 result | Do not imply hardware, field, or Army-network performance |
| JUXTAPOSE v5.1 reduced mean mission/search-resource cost from 1.914 to 1.389 in the frozen synthetic comparison | **SYNTHETIC EVIDENCE ONLY** | frozen v5.1 result | May state only inside the exact synthetic evidence domain |
| JUXTAPOSE complete-outage controls produced zero false `CONNECTED` in the frozen v5.1 test | **SYNTHETIC EVIDENCE ONLY** | frozen v5.1 result | Do not generalize to all real networks |
| JUXTAPOSE is already integrated into the current JANUS physical swarm | **NOT ESTABLISHED / CURRENTLY NOT CLAIMED** | xTech branch explicitly keeps assets separate | Do not claim |
| The current JANUS swarm is a plausible physical falsification substrate for JUXTAPOSE | **PROPOSED TEST / ARCHITECTURAL FIT** | JUXTAPOSE swarm-reference mapping + existing Swarm interfaces | May state as a future controlled validation path |
| JUXTAPOSE creates RF reachability when no physical path exists | **FALSE / OUT OF SCOPE** | algorithm boundary | Never claim |
| JUXTAPOSE makes ESP-NOW anti-jam or military-grade | **FALSE / OUT OF SCOPE** | algorithm boundary | Never claim |
| JUXTAPOSE independently creates general-purpose multi-hop routing | **NOT CLAIMED** | JUXTAPOSE consumes authorized route/path candidates from underlying system | Do not claim |
| JUXTAPOSE has independent external field replication | **NOT ESTABLISHED** | no external/field validation yet | Do not claim |
| Earlier JUXTAPOSE failures were removed from the record | **FALSE** | v1/v5 negative lineage is intentionally preserved | Preserve failures in reviewer materials |

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
| JUXTAPOSE is already a deployed commercial product | **NOT ESTABLISHED** | research package only | Do not claim |
| JUXTAPOSE could become a separate transport-agnostic control-plane component if real validation succeeds | **CONDITIONAL TRANSITION PATH** | architecture + proposed hardware test | May state only as future possibility contingent on validation |

## Army / xTech claims

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| The project maps to xTech C2 / Counter-C2 interests such as resilient communications and deep sensing | **RELEVANCE MAPPING, NOT PERFORMANCE CLAIM** | official RFI + JANUS architecture | May explain potential relevance without calling JANUS an operational C2 product |
| JANUS is already integrated with Army systems | **NOT ESTABLISHED** | no evidence | Do not claim |
| JANUS is combat-ready / battlefield-proven | **NOT ESTABLISHED** | no deployment evidence | Do not claim |
| JANUS provides autonomous weapon or human-targeting authority | **OUT OF SCOPE / NOT CLAIMED** | project boundary | Do not claim or propose as current capability |
| The developer/entity is eligible for xTech|Search 10 direct submission | **PENDING OFFICIAL CLARIFICATION** | detailed RFI vs conflicting website summary | Do not claim until Army replies |
| A separate JUXTAPOSE package was previously prepared/shared for DARPA evaluation | **OWNER/PROVENANCE STATEMENT** | developer statement + preserved DARPA package artifacts | May state as provenance; do not imply DARPA endorsement or validation |
| DARPA endorsement of JUXTAPOSE exists | **NOT ESTABLISHED** | submission/sharing does not equal endorsement | Never imply |

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
- `separate existing JUXTAPOSE research asset`
- `synthetic evidence only`
- `fresh measurement remains current connectivity authority`
- `optional future hardware falsification`

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
- `JUXTAPOSE-integrated swarm` before a real integration/test exists
- `DARPA validated` or `DARPA approved`

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

For JUXTAPOSE specifically:

```text
FROZEN SYNTHETIC RESULT
!=
PHYSICAL SWARM RESULT

SEPARATE RESEARCH ASSET
!=
CURRENT SWARM INTEGRATION
```

No intermediate step may be silently skipped.

## Correction rule

If a later test contradicts a current assumption, the matrix must be updated to the weaker status. Failed evidence is preserved; it is not removed to protect the pitch.