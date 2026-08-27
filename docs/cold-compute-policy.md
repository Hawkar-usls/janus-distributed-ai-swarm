# Cold Compute Policy

LastSwarm firmware treats heat, low heap, weak signal, radio queue pressure, timing jitter, and power limits where available as control inputs. A node may reduce display rate, packet rate, work batch, optional background study, archive traffic, or other side-work when runtime conditions degrade.

This policy now follows the Zim-derived `QUIET_CANARY_RESOURCE_HIERARCHY` from [`AUTONOMOUS_SPECIALIST_DOCTRINE.md`](AUTONOMOUS_SPECIALIST_DOCTRINE.md).

## Priority Rule

A node does not try to do everything at full cadence simultaneously.

Optional work degrades before the node's protected primary mission:

```text
1. decorative UI / verbose logging
2. optional telemetry frequency
3. background study / archive work
4. coordinator side-quest batch or frequency
5. optional network uploads
6. primary mission only when required for hardware safety or correctness
```

For sensing nodes, fresh sensor truth outranks decorative work.
For coordinator nodes, essential arbitration/queue correctness outranks presentation.
For autonomous workers, their declared primary mission outranks bounded side-quests.
For ADV_Elite, truth/clock/sensors/anomaly/Witness and local predictive state outrank mining/archive/decorative side-work.

Throttle/defer state should be visible in health telemetry or logs.

## Review Points

- Avoid blocking delays in critical UI, sensor, audio, and ESP-NOW loops.
- Keep optional heavy logic behind timing gates.
- Prefer measured state over fixed assumptions.
- Protect explicit heap/stack/thermal/radio reserves.
- Treat throttling as a resource-allocation decision, not as a cosmetic issue.
- Record which workload was reduced and why.
- Do not silently let Buzz/Core/NAS side-work starve the local primary mission.
