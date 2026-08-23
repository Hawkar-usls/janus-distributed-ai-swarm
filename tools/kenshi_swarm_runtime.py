#!/usr/bin/env python3
"""Primary Kenshi-founded JANUS swarm runtime.

This composes the persistent-local-agent blackboard with the existing spiral
controller and SWARM_GENOME_LEDGER. A blackboard event can become context for a
new spiral turn, and the resulting genome node is acknowledged back onto the
blackboard without mutating or deduplicating either agent identity.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, Iterable, Optional

from demiurge_spiral_swarm import SwarmSpiralController
from kenshi_swarm_blackboard import KenshiSwarmBlackboard


class KenshiSpiralSwarmRuntime:
    def __init__(self, *, blackboard_window: int = 128, spiral_window: int = 32):
        self.blackboard = KenshiSwarmBlackboard(active_window=blackboard_window)
        self.spiral = SwarmSpiralController(working_window=spiral_window)

    def register_agent(self, agent_id: str, role: str, *, lineage_id: Optional[str] = None, local_state: Any = None) -> Dict[str, Any]:
        agent = self.blackboard.register_agent(agent_id, role, lineage_id=lineage_id, local_state=local_state)
        if not self.spiral.ledger(agent_id).turns:
            state = {
                "role": role,
                "status": agent["status"],
                "local_state": agent["local_state"],
                "lineage_id": agent["lineage_id"],
            }
            turn = self.spiral.integrate(
                agent_id,
                state,
                lessons=["Kenshi-founded local identity registered without collapsing into shared state."],
                promoted=True,
                outcome="REGISTERED_AND_ASCENDED",
            )
            node_id = self.spiral.genome.by_entity[agent_id][-1]
            event = self.blackboard.append_event(
                agent_id,
                "STATE_DELTA",
                {"registration": True, "state": state},
                genome_node_ids=[node_id],
            )
            self.blackboard.bind_genome(agent_id, node_id, parent_event_ids=[event.event_id])
            return {"agent": agent, "turn": turn.to_dict(), "genome_node_id": node_id, "event_id": event.event_id}
        return {"agent": agent, "turn": None, "genome_node_id": self.spiral.genome.by_entity[agent_id][-1]}

    def emit(self, agent_id: str, kind: str, payload: Any, *, recipients: Optional[Iterable[str]] = None, source_bound: bool = False, parent_event_ids: Optional[Iterable[str]] = None):
        return self.blackboard.append_event(
            agent_id,
            kind,
            payload,
            recipient_ids=recipients,
            source_bound=source_bound,
            parent_event_ids=parent_event_ids,
        )

    def handoff(self, sender_id: str, recipients: Iterable[str], payload: Any, *, parent_event_ids: Optional[Iterable[str]] = None):
        return self.blackboard.handoff(sender_id, recipients, payload, parent_event_ids=parent_event_ids)

    def commit_turn(
        self,
        agent_id: str,
        candidate_state: Any,
        *,
        trigger_event_ids: Iterable[str],
        lessons: Optional[Iterable[str]] = None,
        constraints: Optional[Iterable[str]] = None,
        promoted: bool = True,
        outcome: Optional[str] = None,
    ) -> Dict[str, Any]:
        trigger_ids = [str(x) for x in trigger_event_ids]
        known = {e.event_id for e in self.blackboard.events}
        unknown = [event_id for event_id in trigger_ids if event_id not in known]
        if unknown:
            raise ValueError(f"unknown trigger events: {unknown}")

        turn = self.spiral.integrate(
            agent_id,
            candidate_state,
            lessons=lessons,
            constraints=constraints,
            promoted=promoted,
            outcome=outcome,
        )
        node_id = self.spiral.genome.by_entity[agent_id][-1]
        ack = self.blackboard.append_event(
            agent_id,
            "CAUSAL_ACK",
            {
                "workflow_route": "BLACKBOARD_CONTEXT_TO_SPIRAL_TURN",
                "trigger_event_ids": trigger_ids,
                "genome_node_id": node_id,
                "turn": turn.turn,
                "outcome": turn.outcome,
                "scientific_truth_implied": False,
            },
            parent_event_ids=trigger_ids,
            genome_node_ids=[node_id],
        )
        bind = self.blackboard.bind_genome(agent_id, node_id, parent_event_ids=[ack.event_id])
        return {
            "turn": turn.to_dict(),
            "genome_node_id": node_id,
            "causal_ack_event_id": ack.event_id,
            "genome_bind_event_id": bind.event_id,
        }

    def record_failure(self, agent_id: str, attempted_state: Any, lesson: str, *, trigger_event_ids: Iterable[str]) -> Dict[str, Any]:
        return self.commit_turn(
            agent_id,
            attempted_state,
            trigger_event_ids=trigger_event_ids,
            lessons=[lesson],
            promoted=False,
            outcome="INTEGRATED_LESSON",
        )

    def validate(self) -> None:
        self.blackboard.validate()
        self.spiral.validate()
        if set(self.blackboard.agents) != set(self.spiral.ledgers):
            raise ValueError("blackboard and spiral identity sets diverged")

    def to_dict(self) -> Dict[str, Any]:
        self.validate()
        return {
            "schema": "janus.swarm.kenshi_spiral_runtime.v1",
            "model": "KENSHI_LOCAL_AGENTS_PLUS_BLACKBOARD_PLUS_SPIRAL_GENOME",
            "foundation": "Hawkar-usls/Janus_Genesis:.janus/KENSHI_SWARM_FOUNDATION.json",
            "core_rule": "SYNC_STATE_NOT_IDENTITY",
            "blackboard": self.blackboard.to_dict(),
            "spiral": self.spiral.to_dict(),
            "epistemic_boundary": {
                "blackboard_event_is_empirical_evidence": False,
                "causal_ack_proves_scientific_truth": False,
                "same_source_multi_agent_is_independent_replication": False,
            },
        }

    def save(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(self.to_dict(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def self_test() -> None:
    runtime = KenshiSpiralSwarmRuntime(blackboard_window=4, spiral_window=4)
    runtime.register_agent("A", "SCOUT", lineage_id="L-A", local_state={"phase": 0})
    runtime.register_agent("B", "SCOUT", lineage_id="L-B", local_state={"phase": 0})
    observation = runtime.emit("A", "OBSERVATION", {"fact": "x", "source": "s"}, source_bound=True)
    handoff = runtime.handoff("A", ["B"], {"request": "verify x"}, parent_event_ids=[observation.event_id])
    result = runtime.commit_turn(
        "B",
        {"role": "SCOUT", "status": "ACTIVE", "local_state": {"phase": 1}},
        trigger_event_ids=[handoff.event_id],
        lessons=["Peer handoff changed the local work frontier without replacing identity."],
    )
    assert result["genome_node_id"] in runtime.spiral.genome.by_entity["B"]
    assert runtime.blackboard.agents["A"]["lineage_id"] == "L-A"
    assert runtime.blackboard.agents["B"]["lineage_id"] == "L-B"
    runtime.validate()
    print("JANUS_KENSHI_SPIRAL_RUNTIME_SELF_TEST=PASS")


if __name__ == "__main__":
    self_test()
