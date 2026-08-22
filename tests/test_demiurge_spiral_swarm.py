from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from demiurge_spiral_swarm import (  # noqa: E402
    PreservingWindow,
    SPIRAL_LAWS,
    SwarmSpiralController,
)


class SwarmSpiralContractTests(unittest.TestCase):
    def test_failure_becomes_lesson_without_replacing_healthy_active_state(self):
        swarm = SwarmSpiralController()
        first = swarm.integrate("gladius", {"status": "FRESH", "score": 10})
        failed = swarm.record_failure(
            "gladius",
            {"status": "BAD_CANDIDATE", "score": -4},
            "Candidate degraded radio health",
        )

        self.assertEqual(first.turn, 0)
        self.assertEqual(failed.turn, 1)
        self.assertEqual(failed.outcome, "INTEGRATED_LESSON")
        self.assertFalse(failed.promoted)
        self.assertEqual(failed.active_state_after, {"status": "FRESH", "score": 10})
        self.assertEqual(failed.parent_fingerprint, first.fingerprint)
        self.assertIn("Candidate degraded radio health", failed.lessons)
        self.assertEqual(len(swarm.genome.nodes), 2)
        swarm.validate()

    def test_recovery_is_new_turn_not_rollback(self):
        swarm = SwarmSpiralController()
        swarm.integrate("anchor", {"status": "FRESH", "rescue_count": 0})
        swarm.record_failure("anchor", {"status": "RADIO_FAULT"}, "TX fail streak")
        recovered = swarm.recover(
            "anchor",
            {"status": "RECOVERED", "rescue_count": 1},
            "radio rescue succeeded",
        )

        ledger = swarm.ledger("anchor")
        self.assertEqual([turn.turn for turn in ledger.turns], [0, 1, 2])
        self.assertEqual(recovered.outcome, "RECOVERED_AND_ASCENDED")
        self.assertEqual(recovered.active_state_after["rescue_count"], 1)
        self.assertEqual(len(ledger.turns), 3)
        self.assertEqual(len(swarm.genome.by_entity["anchor"]), 3)
        swarm.validate()

    def test_stale_node_moves_to_known_roster_but_identity_survives(self):
        swarm = SwarmSpiralController()
        swarm.integrate("blind-eye", {"status": "FRESH", "kind": "OBSERVER"})
        stale = swarm.mark_stale("blind-eye", "heartbeat age exceeded")

        self.assertEqual(stale.entity_id, "blind-eye")
        self.assertEqual(stale.outcome, "ASCENDED_TO_KNOWN_ROSTER")
        self.assertEqual(stale.active_state_after["status"], "STALE_KNOWN_ROSTER")
        self.assertEqual(len(swarm.ledger("blind-eye").turns), 2)
        self.assertEqual(len(swarm.genome.by_entity["blind-eye"]), 2)
        swarm.validate()

    def test_cross_entity_derivation_links_separate_spiral_ledgers(self):
        swarm = SwarmSpiralController()
        swarm.integrate("scout-template", {"role": "SCOUT_TEMPLATE"})
        parent = swarm.genome.by_entity["scout-template"][-1]
        swarm.derive(
            "scout-cosmos",
            {"role": "COSMOS_SCOUT"},
            parent_genome_ids=[parent],
            relation="SPECIALIZED_FROM",
            lessons=["Specialized without replacing the parent scout identity."],
        )
        child = swarm.genome.by_entity["scout-cosmos"][-1]

        self.assertEqual(swarm.genome.trace_to_origins(child), [[parent, child]])
        self.assertIn(child, [n.genome_id for n in swarm.genome.descendants(parent)])
        self.assertIn(parent, [n.genome_id for n in swarm.genome.ancestors(child)])
        swarm.validate()

    def test_bounded_frontier_archives_overflow_instead_of_erasing_it(self):
        window = PreservingWindow(2)
        window.append("turn-0")
        window.append("turn-1")
        window.append("turn-2")
        window.append("turn-3")

        self.assertEqual(window.active, ["turn-2", "turn-3"])
        self.assertEqual(window.archive, ["turn-0", "turn-1"])
        self.assertEqual(window.all_items(), ["turn-0", "turn-1", "turn-2", "turn-3"])

    def test_persisted_receipt_contains_turns_genome_and_canonical_laws(self):
        swarm = SwarmSpiralController(working_window=1)
        swarm.integrate("buzz", {"status": "FRESH"})
        swarm.record_failure("buzz", {"status": "BAD_ROUTE"}, "route failed")
        swarm.recover("buzz", {"status": "RECOVERED"})

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "receipt.json"
            swarm.save(path)
            payload = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual(payload["model"], "SPIRAL_ACCUMULATIVE_WITH_SHARED_GENOME")
        self.assertEqual(len(payload["entities"]["buzz"]["turns"]), 3)
        self.assertEqual(len(payload["chronicle"]), 3)
        self.assertEqual(len(payload["genome"]["nodes"]), 3)
        self.assertEqual(payload["genome"]["model"], "DUAL_STRAND_SPIRAL_GENEALOGY")
        self.assertGreaterEqual(len(payload["frontier"]["archive"]), 2)
        for law in SPIRAL_LAWS:
            self.assertIn(law, payload["laws"])

    def test_no_delete_entity_api_exists(self):
        swarm = SwarmSpiralController()
        self.assertFalse(hasattr(swarm, "delete_entity"))
        self.assertFalse(hasattr(swarm, "remove_entity"))
        self.assertFalse(hasattr(swarm, "purge_entity"))
        self.assertFalse(hasattr(swarm.genome, "delete_node"))
        self.assertFalse(hasattr(swarm.genome, "remove_node"))
        self.assertFalse(hasattr(swarm.genome, "purge_node"))


if __name__ == "__main__":
    unittest.main()
