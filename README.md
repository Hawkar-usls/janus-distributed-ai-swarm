<div align="center">

# JANUS Distributed AI Swarm
### ESP32 / M5Stack firmware & telemetry experiments

![Status](https://img.shields.io/badge/status-active%20engineering-2f81f7)
![Class](https://img.shields.io/badge/class-embedded%20%2F%20distributed-6e7681)

`heterogeneous nodes` · `explicit protocol boundaries` · `observer-first review`

</div>

## Abstract

This repository contains the public firmware surface for a heterogeneous ESP32/M5Stack swarm: UI stations, telemetry/observer nodes, experimental workers, and ESP32-P4 bring-up code.

It is an **embedded-systems engineering project**. Node names and visual themes are project vocabulary, not claims about intelligence or physical prediction.

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

## Safety

Hardware, RF, power, thermal, and unattended-operation risks depend on the physical build and deployment. Public firmware and CI results are not a safety certification.

## License

MIT for code authored in this repository. Third-party libraries/vendor sources retain their own licenses; check them before redistribution.

Presentation follows the account's [public repository standard](https://github.com/Hawkar-usls/Janus/blob/main/docs/PUBLIC_REPOSITORY_PRESENTATION_STANDARD.md). No affiliation with MIT is implied by the presentation style.
