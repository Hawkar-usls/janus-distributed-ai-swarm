from __future__ import annotations

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from kenshi_swarm_blackboard import KenshiSwarmBlackboard  # noqa: E402


class KenshiSwarmBlackboardTests(unittest.TestCase):
    def test_sync_state_does_not_replace_identity(self):
        board = KenshiSwarmBlackboard()
        board.register_agent("scout-a", "SCOUT", lineage_id="L-A", local_state={"x": 1})
        board.sync_local_state("scout-a", {"x": 2})
        self.assertEqual(board.agents["scout-a"]["lineage_id"], "L-A")
        self.assertEqual(board.agents["scout-a"]["local_state"]["x"], 2)

    def test_same_observation_payload_does_not_deduplicate_agents_or_events(self):
        board = KenshiSwarmBlackboard()
        board.register_agent("A", "SCOUT")
        board.register_agent("B", "SCOUT")
        e1 = board.append_event("A", "OBSERVATION", {"fact": "x", "source": "s"})
        e2 = board.append_event("B", "OBSERVATION", {"fact": "x", "source": "s"})
        self.assertNotEqual(e1.event_id, e2.event_id)
        self.assertEqual(set(board.agents), {"A", "B"})

    def test_handoff_is_routing_context(self):
        board = KenshiSwarmBlackboard()
        board.register_agent("A", "SCOUT")
        board.register_agent("B", "SCOUT")
        event = board.handoff("A", ["B"], {"source_pointer": "https://example.org"})
        inbox = board.inbox("B")
        self.assertEqual(inbox[0]["event_id"], event.event_id)
        self.assertFalse(board.to_dict()["epistemic_boundary"]["blackboard_event_is_empirical_evidence"])

    def test_expired_lease_is_superseded_not_deleted(self):
        board = KenshiSwarmBlackboard()
        board.register_agent("A", "SCOUT")
        board.register_agent("B", "SCOUT")
        first = board.claim_lease("A", "OBJ", now_turn=1, ttl_turns=1)
        second = board.claim_lease("B", "OBJ", now_turn=3, ttl_turns=1)
        self.assertIn(first.event_id, second.parent_event_ids)
        self.assertEqual(len([e for e in board.events if e.kind == "LEASE_STATE"]), 2)

    def test_stale_then_recovered_preserves_agent_lineage(self):
        board = KenshiSwarmBlackboard()
        board.register_agent("A", "SCOUT", lineage_id="LINEAGE-A")
        board.mark_stale("A", "timeout")
        board.recover("A", {"ok": True})
        self.assertEqual(board.agents["A"]["lineage_id"], "LINEAGE-A")
        self.assertEqual(board.agents["A"]["status"], "RECOVERED")


if __name__ == "__main__":
    unittest.main()
