# Demiurge Spiral Binding for JANUS Distributed AI Swarm

This repository consumes the canonical spiral-evolution law from `Hawkar-usls/Janus-Demiurge`.
The binding is machine-readable in `.janus/DEMIURGE_SPIRAL_LINK.json` and implemented locally by
`tools/demiurge_spiral_swarm.py`.

## Core transition

```text
ENTITY_n
  + EXPERIENCE_n
  + FAILED_ATTEMPTS_n
  + CONSTRAINTS_n
        |
        v
     INTEGRATE
        |
        v
ENTITY_n+1
```

A return to the same logical slot is never treated as a reset to the same state:

```text
A0 -> B0 -> C0 -> ASCEND
                    |
                    v
A1 -> B1 -> C1 -> ASCEND -> ...
```

## Swarm meaning

A node may become stale, degraded, recovering, rebuilt, moved to another body, or superseded at the
active frontier. Its prior learning lineage remains inspectable. A failed candidate is retained as an
`INTEGRATED_LESSON`; it does not silently replace a healthy active state. Recovery produces a new
`RECOVERED_AND_ASCENDED` turn rather than pretending the failed turn never happened.

Bounded RAM/storage is still allowed. The rule is not "everything must stay in hot memory". The rule
is: when the active window overflows, move old records into an archive or durable summary instead of
silently erasing the learning lineage.

## Composition with existing swarm law

This binding strengthens existing rules rather than replacing them:

- stale devices move to the known roster instead of disappearing;
- local function survives game/UI/evolution layers;
- radio failure becomes a recovery lesson;
- observer/scheduler experiments may change order, lane, sector, batch and UI state only;
- sensor truth remains separate from inference or lore;
- SHA-256, target math, pool/Stratum truth, accepted/rejected accounting and submit semantics remain frozen.

Ephemeral coordination metadata such as an expired task lease may still be released. The no-deletion
law applies to learning identity and its evidence/lessons, not to temporary locks, sockets, caches or
other disposable coordination mechanics.

## Runtime adapter

`SwarmSpiralController` provides:

- `integrate(...)` — promote a new active turn;
- `record_failure(...)` — keep the active parent and append the failed attempt as a lesson;
- `mark_stale(...)` — move identity to `STALE_KNOWN_ROSTER` without erasing lineage;
- `recover(...)` — create a new recovery turn;
- `validate()` — verify turn numbers, parent fingerprints and content hashes;
- `save(...)` — persist the complete lineage receipt.

The adapter is transport-neutral: it performs no flash, pool submission, network control, Terminal
bypass or other external effect.

## Gate

`.github/workflows/validate-demiurge-spiral-swarm.yml` verifies the binding contract, runs the
regression suite, and executes a deterministic smoke scenario that creates a persisted spiral receipt.
