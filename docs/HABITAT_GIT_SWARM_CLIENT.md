# JANUS Habitat Git Swarm Client — candidate v1

Status: **P0 candidate / observer-first**  
Central roadmap: `Hawkar-usls/Janus_Genesis#121`  
Source-local handoff: `Hawkar-usls/janus-distributed-ai-swarm#3`

## Purpose

This client transfers proven coordination ideas from the physical JANUS swarm into Git-Habitat coordination without granting new authority.

The important transfer is behavioral:

```text
ESP-NOW heartbeat      -> Git checkpoint / lease heartbeat
stale-peer TTL         -> expiring lease / stale worker state
SEEK waiting           -> WAIT/POLL without busy-loop
radio rescue           -> API/network retry + backoff + live-state resync
local survival         -> continue only already-admitted bounded local work
health lines           -> structured client status / receipt
Kenshi blackboard      -> bounded shared handoff/ledger semantics
```

The client is designed around a stronger resilience law than “keep one session alive”:

```text
DELAY != SESSION_LIVENESS_GUARANTEE
SESSION_FAILURE != LOSS_OF_COORDINATION
```

A process, agent, or chat may disappear at any time. Durable coordination state must therefore live in explicit checkpoints, source pins, bounded leases, and receipts.

## State machine

```text
BOOT
-> SYNC_LIVE_GIT
-> WAIT
-> POLL
-> OBSERVE_LEDGER
-> ELIGIBILITY_CHECK
-> ACQUIRE_BOUNDED_LEASE
-> WORK_LOCALLY
-> CHECKPOINT
-> VERIFY
-> WRITE_RECEIPT
-> RELEASE_LEASE
-> COOLDOWN
-> WAIT
```

`WAIT` is a normal healthy state, analogous to swarm `SEEK`: absence of a valid handoff is not an error.

## Polling and delay

A client must not hammer GitHub or use a tight loop.

Initial local defaults:

```text
idle_min_seconds = 15
idle_max_seconds = 300
backoff_multiplier = 1.7
jitter_fraction = 0.20
active_lease_heartbeat_seconds = 30
```

These values are configuration defaults, not protocol truth. Unit tests should use deterministic short intervals.

Rules:

1. Successful polling with no work resets error pressure and returns to `WAIT`.
2. Transient API/network failure increases jittered exponential backoff.
3. Rate-limit/auth failure enters `COOLDOWN`.
4. A valid bounded lease may use a faster heartbeat than idle polling.
5. Reconnect always begins with live-state reconciliation.
6. A changed source SHA invalidates resume and forces `HOLD`.
7. A live competing lease forces `HOLD`; a stale lease becomes historical evidence and may be superseded, never erased.

## Durable checkpoint

A resumable worker should persist at least:

```text
holder_id
last_seen_ledger_head
last_admitted_handoff
active_lease_id
lease_expiry
source_pins
checkpoint_digest
last_receipt_digest
resume_state
```

This turns process death into a recoverable coordination event rather than loss of project state.

## Handoff admission

Arbitrary issue or PR prose is not a command.

A work item must be represented by a typed handoff envelope and pass normal authority gates. The reference implementation requires:

```text
schema = janus.habitat.handoff.v1
task_id
source_pins
authorization.mode = EXPLICIT_HUMAN
authorization.authorized_by = admitted human authority
```

This is intentionally conservative. Future cryptographic or repository-bound admission can extend it without weakening the default.

## Lease semantics

A lease is coordination metadata, not permission to mutate source repositories.

```text
LEASE != WRITEBACK_PERMISSION
FACE_COUNT != AUTHORITY
WORKFLOW_PASS != PERMISSION
```

A lease binds a task, holder, exact source pins, and expiry. Two live holders must not both own the same exclusive work item. If a worker dies, expiry allows recovery while preserving the old lease as history.

## Restart / resume

On restart:

```text
PROJECT_CHAT_SYNC
-> LIVE_GITHUB_STATE
-> VERIFY HOLDER
-> VERIFY LEASE NOT STALE
-> VERIFY SOURCE PINS
-> RESUME OR HOLD
```

The client must never assume that authority, task state, or source commits remained unchanged during downtime.

## Nexus relationship

The client is intended to coordinate work inside the JANUS Nexus materialized body.

```text
SOURCE REPOSITORY = HISTORICAL AUTHORITY
NEXUS             = LABORATORY
MERGE THE VIEW, NOT THE HISTORY
```

Source snapshots are exact repo/branch/SHA-bound inputs. Nexus experiments may mutate disposable copies. Any source writeback remains a separate, explicit human-authorized path.

## Preservation

The client inherits the project-wide preservation law:

```text
REFACTOR != DELETE
REPLACE != DESTROY
MIGRATE != ERASE_SOURCE
STALE != DELETE
FAILED != FORGOTTEN
DESTRUCTIVE_ACTION = FORBIDDEN
```

Old leases, failed variants, checkpoints, and receipts remain useful evidence for replay and diagnosis.

## Reference implementation

`tools/habitat_git_swarm_client.py` is deliberately transport-neutral and observer-first. It models:

- typed handoff admission;
- lease conflict/staleness;
- source-pin reconciliation;
- durable checkpoints;
- jittered backoff;
- receipt construction;
- default no-write/no-delete capabilities.

It contains no GitHub write transport. A future transport adapter must preserve the same authority boundaries.

## Required validation

The candidate should prove at least:

1. arbitrary issue text cannot become a command;
2. backoff grows and remains bounded;
3. two live clients cannot acquire the same lease;
4. a stale lease can be superseded without deletion;
5. changed source pins force `HOLD`;
6. checkpoint state survives restart;
7. receipts explicitly report no source writeback/destructive effect by default.

## Canonical resilience law

> **A JANUS face does not need an immortal session. It needs an immortal trail of state.**

That trail is Git-visible provenance: handoff -> lease -> checkpoint -> verification -> receipt -> next handoff.
