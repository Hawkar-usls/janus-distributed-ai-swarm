# JANUS Chaos Director Mechanics

This is a mechanics document, not lore.

The useful part of the "butterfly effect" idea is not time-travel fantasy. It is controlled work with nonlinear systems:

```text
small perturbation -> observed branch -> consequence memory -> safer next perturbation
```

JANUS works with pool rhythm, SHA tails, radio timing, sensor noise, miner load, stale windows, black-hole corpus and swarm coordination. These are chaotic enough that tiny scheduling changes can produce different tails, stale/reject patterns, heat/load signatures and radio visibility.

## Core Rule

Chaos Director may perturb search order, attention, route, batch size, lane, sector, UI/story state and corpus tags.

It must never perturb:

```text
pool job truth
SHA header
target comparison
S2/share packet truth
accepted/rejected accounting
Stratum protocol
submit pressure beyond safe policy
```

## Algorithm

Every controlled change is an experiment:

```text
branch_id
parent_branch_id
job_id / job_seq
node_id
method / lane / sector / stride
perturbation_kind
perturbation_strength
start_nonce / range
before_state
after_state
best_z / best_bits
accepted / rejected / stale
hashrate
radio health
temperature / load / p-n signal
outcome_score
rollback_hint
```

The system does not assume that a better outcome proves causality. It only records that this perturbation was followed by that outcome under these conditions.

## Branch Types

Red / Trickster:

```text
small exploration perturbations
rare-tail scout
new lane / sector / stride
high curiosity, low trust
```

Blue / Shadow:

```text
stale/reject/radio failure branch
cooldown and recovery
record what not to repeat
```

Gold / Sovereign:

```text
stable baseline
repeatable route
low reject, low stale, clean radio
```

## Device Roles

Buzz:

```text
judge branch results
never mutate pool truth
award Brother Arena / worker score
```

Core2:

```text
horizon station
keeps galaxy/world consequence state
records BH study results and branch summaries on SD
```

ATOM_BH:

```text
chaos observatory
turns black-hole/mercury/p-n/heat/load corpus into safe nonce-order hints
```

Yaks_Gate:

```text
escape craft
uses branch memory to choose reverse-gate vector
IR beacon is a local optical signature, not RF control
```

Anchor:

```text
stable control branch
proves baseline before Gladius claims improvement
```

Gladius:

```text
experimental branch
tries strange nonce walks, but reports every perturbation cleanly
```

NAS Archivarius:

```text
long memory
stores branch ledger, firmware identity, incidents and outcomes
does not control pool truth
```

## Practical Mechanics For Miners

Add a bounded `chaos_epsilon`:

```text
0.00 = pure baseline
0.05 = tiny stride/sector wobble
0.10 = scout lane
0.25 = experimental branch
```

Perturb only nonce order:

```text
start_nonce offset
stride choice
sector rotation
batch boundary
lane priority
stale guard threshold within safe range
```

Do not increase submit pressure. Strong stale/reject tails go to dark-tail corpus.

## Practical Mechanics For Games

The game world should use consequence memory:

```text
faction pressure
station economy
pirate activity
BH lab progress
Yaks rescue stability
miner discoveries
sensor events
```

Small player/swarm actions can change future routes, prices, missions or danger, but every change should be traceable to a branch record.

## Logs To Add Later

```text
[CHAOS] node=... branch=... parent=... eps=... lane=... score=... rollback=...
[CHAOS/OUTCOME] branch=... best=... stale=... reject=... radio=... trust=...
[CHAOS/ROLLBACK] branch=... reason=...
```

## Storage

On SD/NAS:

```text
/janus/chaos_branches.jsonl
/janus/chaos_outcomes.jsonl
/janus/chaos_rollbacks.jsonl
```

On small devices without storage:

```text
small RAM ring
periodic S/S or P/N summary
```

## First Implementation Target

Start with observer-only:

```text
record branch/outcome
compute score
do not let the score change scheduler yet
```

Then enable small `chaos_epsilon <= 0.05` only on Gladius and BH, while Anchor stays baseline.

See also:

```text
JANUS_CHAOS_DIRECTOR_EXCHANGE.md
```

That exchange records the matching LastSwarm/Yaksa branch-memory idea:

```text
past -> present -> future branches -> action -> ledger -> memory
```

## I0 Exchange

The idea was shared with Janus I0 in:

```text
LOCAL_PATH_REDACTED
```

I0's useful reply is strict accounting:

```text
chaos is not "more random"
chaos must be a controlled perturbation against a mirror/control branch
```

I0 maps cleanly onto the ESP32 swarm:

```text
Anchor = baseline/control branch
Gladius = active perturbation branch
BH = chaos observatory / lens branch
Yaks = consequence/gate branch
Buzz = judge
Core2 = horizon station / consequence memory
NAS Archivarius = long-memory ledger
```

I0 requires an equal-exposure or known-exposure boundary for every branch:

```text
same job
disjoint nonce ranges
same target
same submit rules
separate counters
fresh-session marker
```

First ESP32 bridge target:

```text
observer-only ChaosLedger summaries in BH, Anchor, Gladius, Yaks_Gate, Core2 and NAS Archivarius
```

Only after stable logs should Gladius or BH receive bounded `chaos_epsilon`.

## Butterfly Ledger / Director Mode

Butterfly Ledger is the practical mode of Chaos Director:

```text
event -> context snapshot -> counterfactual probe plan -> repeatability score -> director verdict
```

Events:

```text
rare-tail find
accepted share
reject
stale
dark-tail
mirror-gap
NAS/ESP radio event
thermal/load window
phase change
```

Context:

```text
lane, sector, strategy, cfg, batch, hps, load, phase, mirror state,
accepted-share corpus, dark-tail corpus, job age, submit ack, source side
```

Verdicts:

```text
LUCK_ONLY
REPLAY_NEARBY
RESCOUT_NOW
PROMOTE_TO_CORPUS
AVENGERS_STONE_CANDIDATE
```

Meaning:

```text
LUCK_ONLY: archive it; do not steer from it.
REPLAY_NEARBY: probe adjacent sector/stride/batch around the trace.
RESCOUT_NOW: mirror/control pressure is high enough to answer with a JANUS rescout.
PROMOTE_TO_CORPUS: useful positive memory.
AVENGERS_STONE_CANDIDATE: rare stable pattern candidate, still not a SHA claim.
```

I0 now has an offline analyzer:

```text
LOCAL_PATH_REDACTED
```

It is observer-only and can later feed NAS Archivarius or Core2 as a report, not as direct control.
