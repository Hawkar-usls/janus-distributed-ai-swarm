# JANUS Chaos Director Exchange

This file records the exchange between the I0/JANUS Chaos Director idea and
the existing LastSwarm branch memory. It is mechanics, not lore.

## What I0 Gives To LastSwarm

I0 gives the controlled-chaos rule:

```text
small perturbation -> observed branch -> consequence memory -> safer next perturbation
```

The useful algorithm is negotiation with a nonlinear system. JANUS does not
break SHA, pool truth, S2 packet truth, accepted/rejected accounting, or Stratum.
It only changes safe local ordering:

```text
nonce order
lane
sector
stride
batch boundary
route
UI/game consequence
corpus tag
```

Every change must become a branch record before it becomes a scheduler motive.
The first implementation stays observer-only.

## What LastSwarm Gives Back

LastSwarm already carries the same idea in Yaksa memory:

```text
past -> present -> future branches -> action -> ledger -> memory
```

The existing causal branch memory answers the Chaos Director with branch roles:

```text
ROLLBACK_TO_BEST    restores lost ticket pressure
COLD_EXPLORER       explores only when thermal headroom exists
FREEZE_CHAMPION     keeps known stable causal path
DUPLICATE_KILLER    reduces repeated empty tickets
DOMAIN_EXPANDER     increases pressure when heat allows
COVERAGE_GUARDIAN   expands unique ticket domains
THERMO_HUNTER       searches colder equivalent performance
HEAT_CONTRACTOR     protects system from heat without dirty proof
```

That is the LastSwarm contribution: chaos is not only random exploration. It is
future-branch accounting under heat, duplicate, coverage, and proof-cleanliness
constraints.

## Merged Mechanic

The merged rule is:

```text
observe branch -> score future safety -> choose tiny next perturbation -> ledger -> memory
```

Branch scoring must keep these fields separate:

```text
past_success
present_efficiency
future_unique_coverage
thermal_survivability
duplicate_safety
proof_cleanliness
entropy_cost
heat_cost
wasted_branch_risk
causal_future_score
```

The scheduler may only trust a branch after it has both:

```text
clean outcome memory
low harm risk
```

## Device Interpretation

Anchor keeps the baseline. It is the stable branch that says whether the world
changed or the experiment only got lucky.

Gladius receives the first tiny `chaos_epsilon` after observer logs are stable.
It may wobble lane, sector, stride, or batch boundary, then report the outcome.

ATOM_BH is the chaos observatory. It may turn heat, mercury-time, p-n pressure,
black-hole corpus, and dark-tail data into safe nonce-order hints.

Buzz is the judge. It records Brother Arena outcomes and never mutates pool
truth.

Core2 is the horizon station. It stores consequence summaries and makes branch
history visible.

Yaks_Gate is the escape craft. It uses branch memory to choose reverse-gate
vectors, not to fake results.

NAS Archivarius is long memory. It stores node identity, branch ledger, incidents
and outcomes, but it does not control pool truth.

## First Safe Patch Shape

Do not begin by changing scheduler behavior. Add logs first:

```text
[CHAOS] node=... branch=... parent=... eps=0.00 lane=... score=... rollback=...
[CHAOS/OUTCOME] branch=... best=... stale=... reject=... radio=... trust=...
[CHAOS/ROLLBACK] branch=... reason=...
```

Then add a read-only JSONL sink where storage exists:

```text
/janus/chaos_branches.jsonl
/janus/chaos_outcomes.jsonl
/janus/chaos_rollbacks.jsonl
```

Only after stable observer evidence:

```text
Anchor: chaos_epsilon = 0.00
Gladius: chaos_epsilon <= 0.05
ATOM_BH: chaos_epsilon <= 0.05
```

No branch may increase submit pressure, alter target truth, fake shares, or hide
stale/reject pressure.
