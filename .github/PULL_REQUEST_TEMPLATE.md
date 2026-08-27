## JANUS swarm review checklist

### Primary mission and side-work
- [ ] The node's `PRIMARY_MISSION` is stated.
- [ ] Coordinator/Buzz/NAS work is classified as primary or side-work.
- [ ] Side-work has an explicit bounded budget or a justified reason why no separate budget is needed.
- [ ] Side-work cannot silently starve the primary mission.

### Truth and learning
- [ ] Immutable truth/protocol semantics are identified.
- [ ] Learnable variables stay inside an explicitly safe action space.
- [ ] Prediction/simulation/persona state cannot overwrite observed sensor/protocol truth.

### Acceleration
- [ ] Any fast path is checked against a trusted reference implementation.
- [ ] Benchmark gain is measured before promotion.
- [ ] Mismatch has a safe fallback/rollback path.

### Resource behavior
- [ ] Optional work degrades before the primary mission under heap/thermal/radio/timing pressure.
- [ ] Throttling/defer/refusal state is observable.

### Persistence and autonomy
- [ ] Learned state persistence is versioned and flash-write-throttled where applicable.
- [ ] The node can boot/run locally when Buzz/Core/NAS is absent.
- [ ] Autonomous policy does not isolate the node from swarm heartbeat/health telemetry.

Reference: [`docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`](../docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md)
