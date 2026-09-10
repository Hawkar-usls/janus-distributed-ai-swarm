# xTech|Search 10 — Claim / Evidence Matrix

This matrix is the submission firewall. A statement belongs in the white paper only when its status and evidence are clear.

| Candidate statement | Status | Existing evidence | Submission rule |
| --- | --- | --- | --- |
| JANUS is a heterogeneous ESP32/M5Stack firmware project | **SUPPORTED** | `README.md`, `PROJECT_MAP.md`, `docs/architecture.md` | May state directly |
| Multiple node roles exchange selected state through ESP-NOW | **SUPPORTED** | `docs/architecture.md`, current firmware entrypoints | May state directly |
| Current project exposes heartbeat / peer visibility / stale-node handling requirements | **SUPPORTED** | `docs/current-swarm-state.md`, `PROJECT_STATUS.json` | May state directly as architecture/implemented paths where applicable |
| Nodes are intended to preserve a protected primary mission while coordinator side-work is bounded | **SUPPORTED ARCHITECTURE** | `docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`, `PROJECT_STATUS.json` | State as design rule; do not convert into quantified reliability claim without a test |
| Blind Eye uses STHS34PF80 TMOS/PIR as primary sensing on the current camera-absent profile | **SUPPORTED** | `firmware/blind_eye/BLIND_EYE.ino` | May state directly |
| Blind Eye distinguishes current sensor state from memory/prediction/UI state | **SUPPORTED** | `docs/technical-boundaries.md`, `BLIND_EYE.ino` | May state directly |
| Anchor contains reconnect / rejoin / blackout-handling paths | **SUPPORTED IN CODE** | `firmware/anchor/Anchor.ino`, `docs/current-swarm-state.md` | State as implemented code path; measure before claiming performance |
| Core2 provides a human-facing swarm/telemetry surface | **SUPPORTED** | `firmware/core2/CORE2.ino`, `docs/architecture.md` | May state directly |
| Buzz contains coordinator/worker and recovery lineage | **SUPPORTED** | `firmware/buzz/Buzz.ino`, `PROJECT_STATUS.json` | May state as background engineering; mining is not the Army capability claim |
| JANUS continues useful operation after arbitrary node loss | **NOT YET QUANTIFIED** | Architecture suggests graceful degradation, but no frozen xTech benchmark exists | Phrase as a test objective until measured |
| JANUS recovers within a specific number of seconds | **NOT ESTABLISHED** | Timeouts exist in code; no controlled benchmark reported here | Do not claim a number until test evidence exists |
| JANUS is anti-jam / jam resistant | **NOT ESTABLISHED** | No validated anti-jam test | Do not claim |
| JANUS is a secure military communications system | **NOT ESTABLISHED** | No military security certification | Do not claim |
| JANUS is combat-ready / battlefield-proven | **NOT ESTABLISHED** | No operational military deployment evidence | Do not claim |
| JANUS has commercial customers / revenue / strong market traction | **NOT DOCUMENTED IN PUBLIC REPO** | No verified customer/revenue evidence in this branch | Do not invent; add only documentary evidence if available |
| Physical JANUS hardware exists and is personally used for sensing/multimedia/compute experiments | **OWNER-REPORTED; NEEDS SUBMISSION EVIDENCE** | Hardware owner statement; firmware repository | Can become supported with dated photos/video, manifest and test receipt |
| BLE interoperability is part of the candidate xTech demo | **PARTIAL / DEVICE-SPECIFIC** | Some JANUS lineages contain Bluetooth functionality; not yet frozen as an xTech test | Include only after selecting and documenting exact firmware/hardware path |
| ESPHome/Home Assistant integration is implemented in the current public xTech baseline | **NOT ESTABLISHED IN CURRENT PUBLIC BASELINE** | No sufficient public evidence found during xTech preparation | Do not claim until exact implementation is located/frozen |
| The project aligns with xTech C2 / Counter-C2 priorities | **RELEVANCE MAPPING, NOT PERFORMANCE CLAIM** | Official RFI priority: resilient communications / deep sensing; existing JANUS edge architecture | May explain potential relevance without calling JANUS an operational C2 system |
| The project is eligible for xTech|Search 10 | **PENDING OFFICIAL CLARIFICATION** | Detailed RFI says qualifying U.S. small business; webpage has conflicting allied-country summary field | Do not claim eligibility until xTech responds |

## Submission vocabulary

Prefer:

- `existing firmware`
- `working engineering prototype / active engineering`
- `heterogeneous low-cost edge nodes`
- `sensor and telemetry mesh`
- `observable heartbeat / stale state / reconnect paths`
- `controlled disruption test`
- `candidate Army application`
- `dual-use potential`

Avoid unless separately proven:

- `combat AI`
- `battlefield proven`
- `anti-jam`
- `military-grade secure`
- `autonomous tactical authority`
- `AGI`
- `superiority`
- `guaranteed resilience`

## Promotion rule

```text
CODE PATH
-> FROZEN HARDWARE/SOFTWARE CONFIGURATION
-> TIMESTAMPED TEST
-> RAW LOG / RECEIPT
-> REPEATABILITY CHECK
-> QUANTITATIVE SUBMISSION CLAIM
```

No intermediate step may be silently skipped.
