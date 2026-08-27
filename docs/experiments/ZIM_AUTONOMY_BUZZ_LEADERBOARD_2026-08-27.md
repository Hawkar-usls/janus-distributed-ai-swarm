# Zim Autonomy / Buzz Leaderboard Experiment — 2026-08-27

## Observation

`ZimGeek` repeatedly appears as a leader in Buzz even though its firmware explicitly protects a direct solo NerdMiner/Stratum mission and treats Buzz jobs as low-priority side-work.

This report records the code-level explanation and the architecture lesson. It does **not** claim mining superiority, SHA predictability or a hidden cryptographic advantage.

## Why Buzz Can Rank Zim Highly

Buzz's agent score uses node-reported metrics such as hash rate, best bits, share deltas, reject deltas and prediction error.

Zim reports aggregate node productivity in its legacy colony heartbeat:

```text
hashRate = gHashRate
shares   = gSharesSent
rejects  = gRejects
bestBits = gBestBits
```

In the Zim branch, `gSharesSent` combines accepted solo-pool shares with Buzz-side shares, while `gHashRate` and `gBestBits` also reflect the node's active hashing rather than only coordinator-attributed work.

Therefore Zim's independent work can legitimately increase the node's **overall-strength** score even when the work was not assigned by Buzz.

This means the current `TOP` label is closer to:

```text
TOP_STRENGTH = strongest currently observed node by aggregate score
```

than to:

```text
TOP_SWARM_WORK = largest contribution specifically to Buzz-assigned work
```

Those are different questions.

## Why Zim Can Do Both Jobs

Zim does not attempt to run every subsystem at equal priority.

Its design includes:
- direct solo Stratum as the protected primary mission;
- tiny, infrequent Buzz lazy batches;
- deliberate side-work skips;
- adaptive safe batch reduction under load;
- Quiet Canary slower telemetry/logging/study cadence;
- separate miner task execution;
- hard LCD blackout that stops rendering/SPI while mining and networking continue;
- local persistence for learned policy state.

The result is specialization rather than generic multitasking.

## What Is Actually Learned

Two useful learning patterns are present:

1. White Raven updates small local policy weights from measured events and persists them.
2. The stride bandit explores among a bounded set of allowed traversal policies and persists arm weights.

Important boundary: SHA-256 is deterministic and designed to behave pseudorandomly with respect to small input changes. A stride receiving rare good hashes is not evidence that the stride predicts SHA output. The reusable lesson is **bounded online policy learning around immutable SHA/protocol truth**, not a claim that the learner discovered a SHA shortcut.

## Frozen Decision

```text
KEEP_ZIM_UNCHANGED = true
```

Zim is retained as an autonomous-specialist experiment rather than normalized into an obedient generic worker.

The observed design motivated the mandatory swarm doctrine:

[`../AUTONOMOUS_SPECIALIST_DOCTRINE.md`](../AUTONOMOUS_SPECIALIST_DOCTRINE.md)

and the ADV adoption contract:

[`../ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`](../ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md)

## Future Measurement Without Changing Zim

If Buzz accounting is expanded later, preserve Zim behavior and add attribution only:

```text
TOTAL productivity
SOLO productivity
BUZZ-attributed productivity
```

Then compare both `TOP_STRENGTH` and `TOP_SWARM_WORK` without changing the agent under observation.
