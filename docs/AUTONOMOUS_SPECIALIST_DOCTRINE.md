# JANUS Autonomous Specialist Doctrine

This document is a mandatory design rule for future JANUS swarm-node upgrades.

The doctrine was extracted from the observed behavior of `ZimGeek`: a node can preserve a strong local mission, accept only bounded coordinator side-work, remain visible to the swarm, learn locally inside safe policy boundaries, and still contribute useful telemetry and work.

The goal is not to clone Zim's personality or mining-specific behavior into every device. The goal is to preserve the architectural properties that make heterogeneous specialists stronger than a fleet of identical obedient workers.

## 1. PRIMARY_MISSION + BOUNDED_SIDE_QUESTS

Every node MUST declare a primary mission.

Examples:
- Blind Eye: truthful presence/motion sensing and local memory.
- Buzz: coordination, pool/work distribution, swarm scoring and arbitration.
- Zim: autonomous solo Stratum specialist plus bounded Buzz work.
- ADV_Elite: local predictive observer/terminal for WORLD + SWARM + SELF, Witness and swarm participation.

Coordinator requests are side-quests unless the node's declared role makes coordination itself the primary mission.

A node MUST define bounded side-work budgets in one or more measurable dimensions:
- wall-clock time per interval;
- CPU time or batch count;
- radio airtime;
- heap/stack reserve;
- thermal/load ceiling;
- queue depth;
- energy/battery budget where available.

A coordinator request MUST NOT silently starve the primary mission.

Recommended state:

```text
PRIMARY_MISSION = protected
SIDE_QUEST_BUDGET = explicit + bounded
SIDE_QUEST_DROP_REASON = observable
```

A node may defer, skip, shorten or reject side-work when the primary mission, resource safety or fresh local truth requires it.

## 2. SELF_TESTED_ACCELERATION_ONLY

Any new fast path must prove equivalence to a trusted reference path before promotion.

Canonical sequence:

```text
NEW_FAST_PATH
  -> VERIFY_AGAINST_KNOWN_TRUTH
  -> BENCHMARK
  -> PROMOTE_OR_REJECT
  -> PERIODIC_CROSSCHECK
  -> FALLBACK_ON_MISMATCH
```

Examples include:
- hardware SHA vs trusted software SHA;
- optimized sensor fusion vs reference implementation;
- faster M2R math kernels;
- compressed inference or fixed-point paths;
- alternative storage/serialization paths.

Speed alone is not sufficient. If semantic output differs where equivalence is required, the optimization is rejected or returned to shadow mode.

## 3. QUIET_CANARY RESOURCE HIERARCHY

Optional behavior degrades before the primary mission.

When heap, thermal/load, Wi-Fi quality, radio queues or timing jitter degrade, reduce nonessential work first.

Typical reduction order:
1. decorative animation / verbose logs;
2. optional telemetry frequency;
3. background study and archive work;
4. side-quest batch size / side-work frequency;
5. optional network uploads;
6. only then reduce the primary mission if required for hardware safety or correctness.

A node should not try to do everything at full cadence simultaneously.

Throttle state MUST be observable in health telemetry or logs.

## 4. IMMUTABLE_TRUTH_CORE + LEARNABLE_POLICY_AROUND_IT

Learning is allowed only around truths that must remain frozen.

Examples of immutable truth cores:
- SHA-256/SHA256d semantics;
- target comparison and submit validity;
- packet ABI/version/type semantics;
- current sensor observation vs predicted/simulated state;
- user safety/control gates;
- authenticated identity and provenance fields.

Learnable policy may choose among explicitly safe actions, for example:
- scheduling;
- batch size;
- lane/sector/window;
- sampling cadence inside safe bounds;
- M2R model weights;
- Theta contribution weight;
- sensor-fusion weights;
- memory retrieval priority;
- compression level;
- side-quest budget allocation.

The action space MUST be bounded before online learning begins.

Reward MUST prefer measurable outcomes over lore or aesthetic signals. Rare hashes, resonance values or attractive correlations are not proof of better policy unless they improve a frozen evaluation metric.

## 5. PERSISTENT LEARNING WITH FLASH-WEAR GUARDS

Useful local learning should survive reboot when hardware permits it.

Persistence rules:
- keep dirty flags;
- throttle NVS/Preferences writes;
- checkpoint on meaningful events where justified;
- maintain version/magic/schema identifiers;
- reject incompatible persisted state safely;
- keep a known-good fallback/default state;
- avoid writing every learning step to flash;
- use SD/LittleFS for larger append history when available.

Learning persistence MUST NOT make boot dependent on a valid learned state.

## 6. AUTONOMY WITHOUT INFORMATION ISOLATION

A node may be autonomous without disappearing from collective cognition.

Even when it declines or delays swarm work, it should continue sharing bounded state such as:
- alive/heartbeat;
- role and firmware version;
- primary mission state;
- side-quest budget and refusal/defer reason;
- health/load/heap/radio state;
- local learning version/confidence where appropriate;
- prediction/anomaly/Witness summary where appropriate;
- stale/unknown markers.

Freedom of local policy does not imply secrecy from trusted peers.

## 7. TOTAL STRENGTH != COORDINATOR CONTRIBUTION

Leaderboards SHOULD distinguish at least two concepts when data permits:

```text
TOP_STRENGTH   = overall node productivity/capability
TOP_SWARM_WORK = contribution specifically attributable to coordinator-assigned work
```

Do not punish autonomous nodes merely because useful local work was not assigned by Buzz. Also do not accidentally label private/solo productivity as coordinator work when attribution matters.

Zim's current behavior is preserved as an experiment; this rule is for future accounting clarity, not for forcing Zim to change.

## 8. REVIEW CONTRACT

When upgrading any swarm node, reviewers should answer:
- What is this node's primary mission?
- What side-work budget is allowed?
- What happens when Buzz/Core/NAS disappears?
- Which truth core is immutable?
- What policy variables may learn?
- What reference path validates acceleration?
- What is persisted, how often, and how is rollback handled?
- What telemetry remains visible if side-work is refused?

If these questions cannot be answered, the node upgrade is incomplete.
