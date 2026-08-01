# JANUS P–N Junction v2.0 — Physical Swarm Integration

This document links the prepared JANUS firmware swarm to the falsifiable SAT experiment in [`Hawkar-usls/Janus-P-N-junction`](https://github.com/Hawkar-usls/Janus-P-N-junction).

**No firmware or existing wire ABI is changed by this document.** The physical experiment remains a future explicit migration.

## Existing surfaces confirmed

The current swarm already contains the roles needed for a physical distributed-latency test:

- **Gladius** — experimental method scout with persistent TailGEX memory and direct brother exchange;
- **Anchor** — stable brother and fallback method;
- **J/T twin-task packets** — direct Anchor↔Gladius coordination surface;
- **P/N Cortex packets** — observer-only silicon/load/entropy trace;
- **Core2 / PEA4** — observer and display surfaces;
- **P4_A / P4_B / P4_C** — scout/intention, mirror/verify/memory, and coordinator role model;
- heartbeat, stale TTL, channel reassertion and recovery paths.

## v2.0 role map

| physical node | experimental role | rule |
|---|---|---|
| Gladius | `GLADIUS_SELECTIVE` | weakened oscillation, persistent clause charge, tested avalanche |
| Anchor | `ANCHOR_STABLE` | sleeps until the stagnation/depth activation gate is met |
| Holocron | `LEGACY_V03 + COORDINATOR` | untouched control lane, arbitration, display, independent clause verification |
| optional Zim / spare brother | `SCOUT_CHAOS` | bounded fallback only after all primary lanes stall |

## Protocol isolation

The SAT experiment must not overload or reinterpret:

- Buzz `J/B` work packets;
- share `S/2` packets;
- existing `J/T` packet layout;
- `P/N` Cortex observer semantics;
- SHA256, pool target, nonce or submit behavior.

A separate packet family is reserved:

- magic: `J/P`
- version: `2`
- purpose: SAT task identity, mode, assignment, depletion depth, hottest-clause digest and verified result.

The packet definition is maintained in:

- `Janus-P-N-junction/experiments/v2.0-selective-distributed/SWARM_INTEGRATION.md`

## Required physical checks

Before any performance claim:

1. All nodes must report identical formula and initial-assignment hashes.
2. Holocron must independently verify every clause before displaying `RECOMBINATION`.
3. The preserved v0.3 control lane must never receive experimental memory injection.
4. Elapsed latency, total work, energy and packet traffic must be reported separately.
5. Packet loss, stale nodes, recovery and memory reconstruction cost must be logged.
6. `PROVEN_NO_RECOMBINATION` requires a sound witness; timeout alone is `SEARCH_EXHAUSTED_NO_PROOF`.
7. No `J/P` event may change mining state or submit pressure.

## Current software holdout

The frozen v2.0 software holdout used seed `440224` and 64 paired 3-SAT instances from `n=32` through `n=240`:

- v0.3 median latency: `116.5` rounds;
- v2.0 median distributed latency: `87.0` rounds;
- median improvement: `25.3%`;
- paired outcomes: `25` faster, `39` tied, `0` slower;
- aggregate work ratio: `2.11×`.

The result is explicitly a **distributed-latency** result, not free computation and not a P=NP proof.

## Next action

Create the `J/P v2` library as an isolated compatibility layer, compile it first against host-side packet-size/CRC tests, and only then integrate one node at a time:

1. Holocron coordinator/verification shell;
2. Gladius Selective Field worker;
3. Anchor stable fallback;
4. optional Scout.
