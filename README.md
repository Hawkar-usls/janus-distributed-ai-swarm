<div align="center">

# JANUS Distributed AI Swarm
### ESP32 / M5Stack firmware & telemetry experiments

![Status](https://img.shields.io/badge/status-active%20engineering-2f81f7)
![Class](https://img.shields.io/badge/class-embedded%20%2F%20distributed-6e7681)
![License](https://img.shields.io/badge/license-source--available%20evaluation-orange)

`heterogeneous nodes` · `explicit protocol boundaries` · `observer-first review`

</div>

## Abstract

This repository contains the public firmware surface for a heterogeneous ESP32/M5Stack swarm: UI stations, telemetry/observer nodes, experimental workers, and ESP32-P4 bring-up code.

It is an **embedded-systems engineering project**. Node names and visual themes are project vocabulary, not claims about intelligence or physical prediction.

The maintained repository is now **source-available for evaluation, not open source**. Historical revisions were previously published under MIT; those historical grants remain valid for the material received under them. See [`LICENSE`](LICENSE) and [`LICENSE_HISTORY.md`](LICENSE_HISTORY.md).

## Current status

| Area | Public scope |
| --- | --- |
| Firmware | multiple ESP32/M5Stack sketches and compatibility snapshots |
| Protocols | ESP-NOW packet/ABI rules and recovery behavior |
| Observability | telemetry, heartbeat, stale-node and observer paths |
| Builds | selected assembled sketches compiled/static-checked in CI |
| P4 work | observer/UI/protocol bring-up; separate tracks remain separate until tested |

Machine-readable status: [`PROJECT_STATUS.json`](PROJECT_STATUS.json)

## Core invariants

```text
MATURITY = ACTIVE_ENGINEERING
SENSOR_TRUTH != PREDICTION_OR_UI_FICTION
OBSERVER_ONLY_SUBMIT_PRESSURE = 0
SHA256_SEMANTICS = UNCHANGED
TARGET_MATH = UNCHANGED
POOL_DIFFICULTY_SEMANTICS = UNCHANGED
SUBMIT_SEMANTICS = UNCHANGED
```

Adaptive experiments may change scheduling choices such as nonce order, lane, stride, sector, or batch pressure. They are not permitted to reinterpret the hash function or acceptance target.

## Boundary

This repository does **not** claim:

- artificial general intelligence;
- precognition or access to future physical information;
- mining advantage or profitability;
- production safety certification;
- autonomous authority over people or external systems.

## Reviewer path

- [`PROJECT_MAP.md`](PROJECT_MAP.md) — current components and file map.
- [`docs/current-swarm-state.md`](docs/current-swarm-state.md) — current working state.
- [`docs/swarm-critical-rules.md`](docs/swarm-critical-rules.md) — invariants and recovery rules.
- [`docs/architecture.md`](docs/architecture.md) — architecture.
- [`docs/technical-boundaries.md`](docs/technical-boundaries.md) — claim/safety boundaries.
- [`M5STACK_REVIEW_GUIDE.md`](M5STACK_REVIEW_GUIDE.md) — hardware-oriented review path.
- [`IP_NOTICE.md`](IP_NOTICE.md) — provenance, HELIOS relationship and IP boundaries.
- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) — external dependency/notice register.
- [`portfolio-visibility.json`](https://github.com/Hawkar-usls/Janus/blob/main/portfolio-visibility.json) — account-wide maturity classification.

## Repository layout

```text
firmware/          active and compatibility sketches
tools/             sketch assembly / build helpers
docs/              architecture and current notes
audits/            public review snapshots
configs/examples/  placeholder configuration
tests/static_checks/ lightweight public checks
```

Selected multi-tab sketches can be assembled with:

```bash
python tools/assemble_swarm_sketches.py
```

Use the board settings documented next to the target firmware. Keep Wi-Fi credentials, API keys, wallet-like identifiers, and private NAS endpoints in ignored local configuration.

## Commercial / HELIOS boundary

This swarm contains background engineering relevant to later JANUS systems, including generalized coordinator/worker and recovery patterns used as lineage for JANUS HELIOS.

Public access does **not** grant rights to deploy later source-available revisions commercially, white-label them, sublicense them, or treat this repository as automatically included in a HELIOS acquisition.

A commercial or acquisition transaction must state exactly which repository snapshot, code, background IP, know-how, and rights are included.

Historical MIT snapshots remain subject to the historical MIT permissions described in [`LICENSE_HISTORY.md`](LICENSE_HISTORY.md); the project does not pretend those already-granted rights can be revoked retroactively.

## Safety

Hardware, RF, power, thermal, and unattended-operation risks depend on the physical build and deployment. Public firmware and CI results are not a safety certification.

## License

Current maintained revisions: **JANUS Distributed AI Swarm Source-Available Evaluation License v1.1**.

Commercial, production, OEM, white-label, hosted, sublicensing, and acquisition rights require a separate written agreement unless a specific file states otherwise.

Historical MIT boundary: see [`LICENSE_HISTORY.md`](LICENSE_HISTORY.md).

Third-party libraries/vendor sources retain their own licenses; see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and upstream terms before redistribution or deployment.

Presentation follows the account's [public repository standard](https://github.com/Hawkar-usls/Janus/blob/main/docs/PUBLIC_REPOSITORY_PRESENTATION_STANDARD.md).
