from __future__ import annotations

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from kenshi_swarm_runtime import KenshiSpiralSwarmRuntime  # noqa: E402


class KenshiSpiralRuntimeTests(unittest.TestCase):
    def test_handoff_can_birth_new_spiral_turn_without_identity_collapse(self):
        runtime = KenshiSpiralSwarmRuntime()
        runtime.register_agent("A", "SCOUT", lineage_id="L-A")
        runtime.register_agent("B", "SCOUT", lineage_id="L-B")
        observation = runtime.emit("A", "OBSERVATION", {"fact": "x", "source": "s"}, source_bound=True)
        handoff = runtime.handoff("A", ["B"], {"verify": "x"}, parent_event_ids=[observation.event_id])
        result = runtime.commit_turn(
            "B",
            {"role": "SCOUT", "status": "ACTIVE", "verified": True},
            trigger_event_ids=[handoff.event_id],
            lessons=["Verification followed peer context."],
        )
        self.assertEqual(runtime.blackboard.agents["A"]["lineage_id"], "L-A")
        self.assertEqual(runtime.blackboard.agents["B"]["lineage_id"], "L-B")
        self.assertIn(result["genome_node_id"], runtime.spiral.genome.by_entity["B"])
        ack = next(e for e in runtime.blackboard.events if e.event_id == result["causal_ack_event_id"])
        self.assertIn(handoff.event_id, ack.parent_event_ids)
        self.assertFalse(ack.payload["scientific_truth_implied"])

    def test_failure_is_bound_to_event_and_retained_as_lesson(self):
        runtime = KenshiSpiralSwarmRuntime()
        runtime.register_agent("A", "SCOUT")
        request = runtime.emit("A", "REQUEST_HELP", {"query": "q"})
        result = runtime.record_failure(
            "A",
            {"status": "FETCH_FAILED"},
            "Fetch failed; absence not proven.",
            trigger_event_ids=[request.event_id],
        )
        turn = runtime.spiral.ledger("A").turns[-1]
        self.assertEqual(turn.outcome, "INTEGRATED_LESSON")
        self.assertFalse(turn.promoted)
        self.assertIn("Fetch failed; absence not proven.", turn.lessons)
        self.assertTrue(result["causal_ack_event_id"])
        runtime.validate()


if __name__ == "__main__":
    unittest.main()
