import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from topa_epistemic_router import TOPAEpistemicRouter


class TOPAEpistemicRouterTests(unittest.TestCase):
    def setUp(self):
        self.router = TOPAEpistemicRouter()

    def test_unresolved_is_valid(self):
        result = self.router.validate({
            "claim_id": "T1",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Unusual event remains unexplained.",
            "claim_kind": "REPORTED",
            "source_pointers": ["source://one"],
            "alternative_hypotheses": ["artifact"],
            "falsification_tests": ["independent repeat"],
            "status": "UNRESOLVED",
            "confidence": "LOW",
        })
        self.assertTrue(result["admissible"])
        self.assertFalse(result["world_truth_implied"])

    def test_peer_consensus_is_not_fact(self):
        result = self.router.assess({
            "claim_id": "T2",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Another Scout says it is true.",
            "claim_kind": "PEER_MESSAGE",
            "source_pointers": [],
            "status": "SUPPORTED_WITHIN_BOUND",
            "confidence": "HIGH",
        })
        self.assertIn("NO_SOURCE_NO_POSITIVE_FACT", result["errors"])
        self.assertIn("CONTEXT_CANNOT_BE_PROMOTED_AS_EMPIRICAL_FACT", result["errors"])

    def test_same_source_is_not_independent_replication(self):
        result = self.router.validate({
            "claim_id": "T3",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Multiple agents inspected one source.",
            "claim_kind": "OBSERVED",
            "source_pointers": ["source://same"],
            "status": "SOURCE_BOUND_OBSERVATION",
            "confidence": "MEDIUM",
            "independent_replication": True,
        })
        self.assertFalse(result["independent_replication"])
        self.assertIn("INDEPENDENCE_NOT_ESTABLISHED_BY_DISTINCT_SOURCES", result["warnings"])

    def test_supported_claim_requires_falsifier(self):
        result = self.router.assess({
            "claim_id": "T4",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Candidate interpretation.",
            "claim_kind": "INFERRED",
            "source_pointers": ["source://one"],
            "alternative_hypotheses": ["alternative"],
            "status": "SUPPORTED_WITHIN_BOUND",
            "confidence": "MEDIUM",
        })
        self.assertIn("FALSIFICATION_ROUTE_REQUIRED_FOR_PROMOTION", result["errors"])

    def test_falsified_result_is_preserved(self):
        result = self.router.validate({
            "claim_id": "T5",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Candidate failed the registered test.",
            "claim_kind": "INFERRED",
            "source_pointers": ["source://test"],
            "alternative_hypotheses": ["other mechanism"],
            "falsification_tests": ["registered test"],
            "status": "FALSIFIED",
            "confidence": "HIGH",
        })
        self.assertEqual(result["status"], "FALSIFIED")


if __name__ == "__main__":
    unittest.main()
