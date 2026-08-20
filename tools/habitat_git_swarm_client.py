"""Observer-first reference state machine for the JANUS Git Habitat swarm client.

This module deliberately contains no GitHub write transport. It models the
coordination semantics that a transport adapter must obey: typed handoff
admission, bounded leases, resumable checkpoints, source-pin reconciliation,
and jittered backoff while idle or degraded.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
from enum import Enum
import hashlib
import json
from pathlib import Path
import random
from typing import Any, Callable, Mapping


class HabitatState(str, Enum):
    BOOT = "BOOT"
    SYNC_LIVE_GIT = "SYNC_LIVE_GIT"
    WAIT = "WAIT"
    POLL = "POLL"
    OBSERVE_LEDGER = "OBSERVE_LEDGER"
    ELIGIBILITY_CHECK = "ELIGIBILITY_CHECK"
    ACQUIRE_BOUNDED_LEASE = "ACQUIRE_BOUNDED_LEASE"
    WORK_LOCALLY = "WORK_LOCALLY"
    CHECKPOINT = "CHECKPOINT"
    VERIFY = "VERIFY"
    WRITE_RECEIPT = "WRITE_RECEIPT"
    RELEASE_LEASE = "RELEASE_LEASE"
    COOLDOWN = "COOLDOWN"
    HOLD = "HOLD"


class LeaseConflict(RuntimeError):
    """Raised when a non-stale exclusive lease already exists."""


@dataclass(frozen=True)
class BackoffPolicy:
    idle_min_seconds: float = 15.0
    idle_max_seconds: float = 300.0
    multiplier: float = 1.7
    jitter: float = 0.20

    def delay(
        self,
        consecutive_errors: int,
        rng: Callable[[], float] = random.random,
    ) -> float:
        errors = max(0, int(consecutive_errors))
        base = min(
            self.idle_max_seconds,
            self.idle_min_seconds * (self.multiplier ** errors),
        )
        delta = base * self.jitter
        value = base + ((2.0 * rng()) - 1.0) * delta
        return max(0.0, min(self.idle_max_seconds, value))


@dataclass(frozen=True)
class Lease:
    lease_id: str
    task_id: str
    holder_id: str
    source_pins: dict[str, str]
    expires_at: float

    def is_stale(self, now: float) -> bool:
        return now >= self.expires_at

    def pins_match(self, live_source_pins: Mapping[str, str]) -> bool:
        return dict(live_source_pins) == self.source_pins


@dataclass
class DurableState:
    holder_id: str
    state: str = HabitatState.BOOT.value
    last_seen_ledger_head: str | None = None
    last_admitted_handoff: str | None = None
    active_lease_id: str | None = None
    checkpoint_digest: str | None = None
    last_receipt_digest: str | None = None
    resume_state: str | None = None
    consecutive_errors: int = 0

    def save(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(
            json.dumps(asdict(self), sort_keys=True, indent=2) + "\n",
            encoding="utf-8",
        )

    @classmethod
    def load(cls, path: str | Path) -> "DurableState":
        return cls(**json.loads(Path(path).read_text(encoding="utf-8")))


@dataclass
class HabitatSwarmClient:
    holder_id: str
    trusted_human_authorizers: frozenset[str] = field(
        default_factory=lambda: frozenset({"Hawkar-usls"})
    )
    backoff: BackoffPolicy = field(default_factory=BackoffPolicy)
    state: DurableState = field(init=False)

    def __post_init__(self) -> None:
        self.state = DurableState(holder_id=self.holder_id)

    @staticmethod
    def canonical_digest(value: Any) -> str:
        payload = json.dumps(
            value,
            sort_keys=True,
            separators=(",", ":"),
            ensure_ascii=False,
        ).encode("utf-8")
        return hashlib.sha256(payload).hexdigest()

    def handoff_is_admitted(self, envelope: Mapping[str, Any]) -> bool:
        """Admit only a typed, explicitly human-authorized handoff envelope."""
        if envelope.get("schema") != "janus.habitat.handoff.v1":
            return False
        task_id = envelope.get("task_id")
        source_pins = envelope.get("source_pins")
        auth = envelope.get("authorization")
        if not isinstance(task_id, str) or not task_id:
            return False
        if not isinstance(source_pins, Mapping) or not source_pins:
            return False
        if not isinstance(auth, Mapping):
            return False
        return (
            auth.get("mode") == "EXPLICIT_HUMAN"
            and auth.get("authorized_by") in self.trusted_human_authorizers
        )

    def acquire_lease(
        self,
        envelope: Mapping[str, Any],
        now: float,
        ttl_seconds: float,
        current_lease: Lease | None = None,
    ) -> Lease:
        """Acquire a bounded coordination lease; never grants source writeback."""
        if not self.handoff_is_admitted(envelope):
            raise PermissionError("handoff is not explicitly admitted")
        if current_lease is not None and not current_lease.is_stale(now):
            raise LeaseConflict(f"task already leased by {current_lease.holder_id}")

        pins = {
            str(key): str(value)
            for key, value in dict(envelope["source_pins"]).items()
        }
        seed = {
            "task_id": envelope["task_id"],
            "holder_id": self.holder_id,
            "source_pins": pins,
            "acquired_at": now,
        }
        lease_id = "lease-" + self.canonical_digest(seed)[:20]
        lease = Lease(
            lease_id=lease_id,
            task_id=str(envelope["task_id"]),
            holder_id=self.holder_id,
            source_pins=pins,
            expires_at=now + max(1.0, float(ttl_seconds)),
        )
        self.state.state = HabitatState.WORK_LOCALLY.value
        self.state.last_admitted_handoff = str(envelope["task_id"])
        self.state.active_lease_id = lease.lease_id
        self.state.resume_state = HabitatState.WORK_LOCALLY.value
        return lease

    def checkpoint(self, payload: Mapping[str, Any]) -> str:
        digest = self.canonical_digest(payload)
        self.state.state = HabitatState.CHECKPOINT.value
        self.state.checkpoint_digest = digest
        self.state.resume_state = HabitatState.WORK_LOCALLY.value
        return digest

    def resume_decision(
        self,
        lease: Lease | None,
        live_source_pins: Mapping[str, str],
        now: float,
    ) -> str:
        """Resume only when holder, lease freshness, and exact source pins agree."""
        if lease is None:
            return HabitatState.HOLD.value
        if lease.holder_id != self.holder_id:
            return HabitatState.HOLD.value
        if lease.is_stale(now):
            return HabitatState.HOLD.value
        if not lease.pins_match(live_source_pins):
            return HabitatState.HOLD.value
        return "RESUME"

    def build_receipt(
        self,
        lease: Lease,
        result: str,
        payload: Mapping[str, Any],
    ) -> dict[str, Any]:
        body = {
            "schema": "janus.habitat.swarm_client.receipt.v1",
            "task_id": lease.task_id,
            "lease_id": lease.lease_id,
            "holder_id": self.holder_id,
            "source_pins": lease.source_pins,
            "result": result,
            "checkpoint_digest": self.state.checkpoint_digest,
            "payload_digest": self.canonical_digest(payload),
            "source_writeback_performed": False,
            "destructive_effect_performed": False,
        }
        receipt_digest = self.canonical_digest(body)
        body["receipt_digest"] = receipt_digest
        self.state.state = HabitatState.WRITE_RECEIPT.value
        self.state.last_receipt_digest = receipt_digest
        return body

    def record_poll_success(self, ledger_head: str | None = None) -> None:
        self.state.state = HabitatState.WAIT.value
        self.state.last_seen_ledger_head = ledger_head
        self.state.consecutive_errors = 0

    def record_poll_error(self) -> None:
        self.state.state = HabitatState.COOLDOWN.value
        self.state.consecutive_errors += 1

    def next_poll_delay(
        self,
        rng: Callable[[], float] = random.random,
    ) -> float:
        return self.backoff.delay(self.state.consecutive_errors, rng=rng)


DEFAULT_CAPABILITIES = {
    "observe_git": True,
    "checkpoint_locally": True,
    "emit_receipt_candidate": True,
    "source_writeback": False,
    "external_effects": False,
    "destructive_actions": False,
}
