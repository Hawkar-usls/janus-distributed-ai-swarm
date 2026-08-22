#!/usr/bin/env python3
"""JANUS Distributed Swarm adapter for the Demiurge spiral evolution law.

The swarm may change active state, recover, go stale, fail experiments, or move
between bodies/roles. What it must not do is erase a learning identity or make a
failed turn disappear from its lineage.

This module is deliberately transport-neutral. It does not flash devices, touch
pool/Stratum truth, mutate SHA-256 work, or grant external-effect authority.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass, field
import hashlib
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional


SPIRAL_LAWS = (
    "NO_LEARNING_ENTITY_DELETION",
    "FAILURE_BECOMES_LESSON",
    "IDENTITY_PERSISTS_ACROSS_TURNS",
    "ITERATION_IS_SPIRAL_NOT_RING",
    "ACTIVE_FRONTIER_MAY_CHANGE_WITHOUT_ERASING_LINEAGE",
    "BOUNDED_WORKING_MEMORY_REQUIRES_ARCHIVE_OR_SUMMARY",
)


def _jsonable(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, dict):
        return {str(k): _jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple, set)):
        return [_jsonable(v) for v in value]
    if hasattr(value, "to_dict") and callable(value.to_dict):
        return _jsonable(value.to_dict())
    return repr(value)


def fingerprint_payload(value: Any) -> str:
    raw = json.dumps(
        _jsonable(value), ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


@dataclass
class SpiralTurn:
    entity_id: str
    turn: int
    parent_fingerprint: Optional[str]
    state_before: Any
    candidate_state: Any
    active_state_after: Any
    outcome: str
    lessons: List[str] = field(default_factory=list)
    constraints: List[str] = field(default_factory=list)
    promoted: bool = False
    fingerprint: str = ""

    def seal(self) -> "SpiralTurn":
        body = asdict(self)
        body.pop("fingerprint", None)
        self.fingerprint = fingerprint_payload(body)
        return self

    def to_dict(self) -> Dict[str, Any]:
        return _jsonable(asdict(self))


class SpiralLedger:
    """Append-only lineage for one swarm identity."""

    def __init__(self, entity_id: str):
        if not entity_id:
            raise ValueError("entity_id must be non-empty")
        self.entity_id = str(entity_id)
        self.turns: List[SpiralTurn] = []

    @property
    def parent_fingerprint(self) -> Optional[str]:
        return self.turns[-1].fingerprint if self.turns else None

    @property
    def active_state(self) -> Any:
        return self.turns[-1].active_state_after if self.turns else None

    def ascend(
        self,
        *,
        state_before: Any,
        candidate_state: Any,
        active_state_after: Any,
        lessons: Optional[Iterable[str]] = None,
        constraints: Optional[Iterable[str]] = None,
        promoted: bool = False,
        outcome: Optional[str] = None,
    ) -> SpiralTurn:
        lesson_list = [str(x) for x in (lessons or []) if str(x).strip()]
        constraint_list = [str(x) for x in (constraints or []) if str(x).strip()]
        if outcome is None:
            outcome = "ASCENDED" if promoted else (
                "INTEGRATED_LESSON" if lesson_list else "NO_ASCENT"
            )
        turn = SpiralTurn(
            entity_id=self.entity_id,
            turn=len(self.turns),
            parent_fingerprint=self.parent_fingerprint,
            state_before=_jsonable(state_before),
            candidate_state=_jsonable(candidate_state),
            active_state_after=_jsonable(active_state_after),
            outcome=str(outcome),
            lessons=lesson_list,
            constraints=constraint_list,
            promoted=bool(promoted),
        ).seal()
        self.turns.append(turn)
        return turn

    def validate(self) -> None:
        parent = None
        for index, turn in enumerate(self.turns):
            if turn.turn != index:
                raise ValueError(f"turn index mismatch for {self.entity_id}")
            if turn.parent_fingerprint != parent:
                raise ValueError(f"broken parent chain for {self.entity_id}")
            body = asdict(turn)
            expected = body.pop("fingerprint")
            actual = fingerprint_payload(body)
            if expected != actual:
                raise ValueError(f"fingerprint mismatch for {self.entity_id} turn {index}")
            parent = turn.fingerprint

    def to_dict(self) -> Dict[str, Any]:
        return {
            "entity_id": self.entity_id,
            "turns": [turn.to_dict() for turn in self.turns],
        }


class PreservingWindow:
    """Bounded active window whose overflow is archived instead of deleted."""

    def __init__(self, maxlen: int):
        if maxlen <= 0:
            raise ValueError("maxlen must be positive")
        self.maxlen = int(maxlen)
        self.active: List[Any] = []
        self.archive: List[Any] = []

    def append(self, value: Any) -> None:
        if len(self.active) >= self.maxlen:
            self.archive.append(self.active.pop(0))
        self.active.append(value)

    def all_items(self) -> List[Any]:
        return [*self.archive, *self.active]


class SwarmSpiralController:
    """Lossless learning/lifecycle controller for heterogeneous swarm nodes."""

    def __init__(self, working_window: int = 32):
        self.ledgers: Dict[str, SpiralLedger] = {}
        self.frontier = PreservingWindow(working_window)
        self.chronicle: List[Dict[str, Any]] = []

    def ledger(self, entity_id: str) -> SpiralLedger:
        entity_id = str(entity_id)
        if entity_id not in self.ledgers:
            self.ledgers[entity_id] = SpiralLedger(entity_id)
        return self.ledgers[entity_id]

    def integrate(
        self,
        entity_id: str,
        candidate_state: Any,
        *,
        lessons: Optional[Iterable[str]] = None,
        constraints: Optional[Iterable[str]] = None,
        promoted: bool = True,
        outcome: Optional[str] = None,
    ) -> SpiralTurn:
        ledger = self.ledger(entity_id)
        before = ledger.active_state
        after = candidate_state if promoted or before is None else before
        turn = ledger.ascend(
            state_before=before,
            candidate_state=candidate_state,
            active_state_after=after,
            lessons=lessons,
            constraints=constraints,
            promoted=promoted,
            outcome=outcome,
        )
        event = {
            "entity_id": entity_id,
            "turn": turn.turn,
            "outcome": turn.outcome,
            "fingerprint": turn.fingerprint,
        }
        self.frontier.append(event)
        self.chronicle.append(event)
        return turn

    def record_failure(
        self,
        entity_id: str,
        attempted_state: Any,
        lesson: str,
        *,
        constraints: Optional[Iterable[str]] = None,
    ) -> SpiralTurn:
        return self.integrate(
            entity_id,
            attempted_state,
            lessons=[lesson],
            constraints=constraints,
            promoted=False,
            outcome="INTEGRATED_LESSON",
        )

    def mark_stale(self, entity_id: str, reason: str) -> SpiralTurn:
        ledger = self.ledger(entity_id)
        previous = ledger.active_state or {"status": "KNOWN"}
        if isinstance(previous, dict):
            next_state = dict(previous)
        else:
            next_state = {"previous_state": previous}
        next_state.update({"status": "STALE_KNOWN_ROSTER", "stale_reason": str(reason)})
        return self.integrate(
            entity_id,
            next_state,
            lessons=["Stale identity moved out of the active frontier without erasing lineage."],
            promoted=True,
            outcome="ASCENDED_TO_KNOWN_ROSTER",
        )

    def recover(self, entity_id: str, recovered_state: Any, lesson: str = "Recovery completed") -> SpiralTurn:
        return self.integrate(
            entity_id,
            recovered_state,
            lessons=[lesson],
            promoted=True,
            outcome="RECOVERED_AND_ASCENDED",
        )

    def validate(self) -> None:
        if len(self.chronicle) != sum(len(x.turns) for x in self.ledgers.values()):
            raise ValueError("global chronicle lost or duplicated a turn")
        for ledger in self.ledgers.values():
            ledger.validate()

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": "janus.swarm.spiral_state.v1",
            "model": "SPIRAL_ACCUMULATIVE_NO_LEARNING_ENTITY_DELETION",
            "laws": list(SPIRAL_LAWS),
            "entities": {key: value.to_dict() for key, value in sorted(self.ledgers.items())},
            "frontier": {
                "active": _jsonable(self.frontier.active),
                "archive": _jsonable(self.frontier.archive),
            },
            "chronicle": _jsonable(self.chronicle),
        }

    def save(self, path: str | Path) -> None:
        self.validate()
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(self.to_dict(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _demo(output: Optional[str]) -> Dict[str, Any]:
    swarm = SwarmSpiralController(working_window=2)
    swarm.integrate("anchor-01", {"status": "FRESH", "role": "ANCHOR"})
    swarm.record_failure(
        "anchor-01",
        {"status": "RADIO_FAULT", "tx_fail": 4},
        "Radio fault retained as recovery lesson; healthy parent state remains active.",
    )
    swarm.recover(
        "anchor-01",
        {"status": "RECOVERED", "role": "ANCHOR", "rescue_count": 1},
        "ESP-NOW rescue succeeded; recovery is a new turn, not a history reset.",
    )
    swarm.mark_stale("scout-ghost", "no fresh heartbeat")
    swarm.validate()
    if output:
        swarm.save(output)
    return {
        "status": "PASS",
        "entities": len(swarm.ledgers),
        "turns": len(swarm.chronicle),
        "archived_frontier_events": len(swarm.frontier.archive),
        "laws": list(SPIRAL_LAWS),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="JANUS swarm spiral runtime smoke runner")
    parser.add_argument("--demo", action="store_true", help="run deterministic spiral smoke scenario")
    parser.add_argument("--output", help="optional JSON receipt path")
    args = parser.parse_args()
    if not args.demo:
        parser.error("use --demo for the transport-neutral smoke run")
    print(json.dumps(_demo(args.output), ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
