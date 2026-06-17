# Technical Boundaries

This document defines the claims made by the public LastSwarm firmware repository.

## Firmware Scope

The public main branch contains current active LastSwarm sketches, compatibility firmware, supporting documentation, config examples, audits, and a lightweight secret marker check.

It does not include non-firmware side projects, raw logs, runtime dumps, databases, older broad archives, or unrelated experiments.

## Adaptive Control

The firmware may use terms such as agent, memory, prediction, confidence, reward, or policy. In this repository these mean local adaptive control concepts:

- tracking local and remote state;
- scoring telemetry or sensor changes;
- recording memory-like summaries;
- estimating prediction error;
- choosing UI, workload, or packet behavior;
- throttling or gating work under weak signal, low heap, or heat.

## Sensor Truth

Presence-related firmware must distinguish:

- current sensor readings;
- remembered previous state;
- predicted or inferred state;
- remote swarm reports;
- UI/game state.

Prediction must not be reported as current presence. Memory must not be reported as current presence.

## Stratum And PoW

Some firmware paths contain Stratum or PoW-related code. Those paths are documented as protocol, telemetry, and scheduling work. Pool accepts, local fallback events, UI rewards, and game rewards are separate event classes.

## Validation Requirements

Before public performance claims, collect timestamped evidence for:

- firmware file and commit hash;
- board model and library versions;
- ESP-NOW packet sizes and version bytes;
- sensor calibration values;
- accepted/rejected network events where applicable;
- local fallback events where applicable;
- temperature, heap, and throttling state where available.
