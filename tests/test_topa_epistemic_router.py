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
        r=self.router.validate({"claim_id":"T1","origin_agent_id":"SCOUT_X","claim_text":"Unusual event remains unexplained.","claim_kind":"REPORTED","source_pointers":["source://one"],"alternative_hypotheses":["artifact"],"mundane_hypotheses":["artifact"],"falsification_tests":["independent repeat"],"status":"UNRESOLVED","confidence":"LOW"})
        self.assertTrue(r["admissible"]); self.assertFalse(r["world_truth_implied"])

    def test_peer_consensus_is_not_fact(self):
        r=self.router.assess({"claim_id":"T2","origin_agent_id":"SCOUT_X","claim_text":"Another Scout says it is true.","claim_kind":"PEER_MESSAGE","source_pointers":[],"status":"SUPPORTED_WITHIN_BOUND","confidence":"HIGH"})
        self.assertIn("NO_SOURCE_NO_POSITIVE_FACT",r["errors"]); self.assertIn("CONTEXT_CANNOT_BE_PROMOTED_AS_EMPIRICAL_FACT",r["errors"])

    def test_same_source_is_not_independent_replication(self):
        r=self.router.validate({"claim_id":"T3","origin_agent_id":"SCOUT_X","claim_text":"Multiple agents inspected one source.","claim_kind":"OBSERVED","source_pointers":["source://same"],"status":"SOURCE_BOUND_OBSERVATION","confidence":"MEDIUM","independent_replication":True})
        self.assertFalse(r["independent_replication"]); self.assertIn("INDEPENDENCE_NOT_ESTABLISHED_BY_DISTINCT_SOURCES",r["warnings"])

    def test_closed_belief_loop_warning(self):
        r=self.router.assess({"claim_id":"T4","origin_agent_id":"SCOUT_X","claim_text":"Favored interpretation.","claim_kind":"INFERRED","source_pointers":["source://one"],"alternative_hypotheses":["alternative"],"mundane_hypotheses":["artifact"],"status":"INFERRED_HYPOTHESIS","confidence":"MEDIUM"})
        self.assertTrue(r["closed_belief_loop"]); self.assertIn("CLOSED_BELIEF_LOOP_WARNING",r["warnings"])

    def test_missing_audience_repetition_shortcuts_rejected(self):
        r=self.router.assess({"claim_id":"T5","origin_agent_id":"SCOUT_X","claim_text":"Popular repeated report.","claim_kind":"REPORTED","missing_data_as_evidence":True,"audience_size_as_corroboration":True,"repetition_as_independence":True})
        self.assertIn("MISSING_DATA_STAYS_MISSING",r["errors"]); self.assertIn("AUDIENCE_SIZE_IS_NOT_CORROBORATION",r["errors"]); self.assertIn("REPETITION_IS_NOT_INDEPENDENCE",r["errors"])

    def test_failed_hypothesis_can_lower_confidence(self):
        r=self.router.validate({"claim_id":"T6","origin_agent_id":"SCOUT_X","claim_text":"Candidate failed registered test.","claim_kind":"INFERRED","source_pointers":["source://test"],"alternative_hypotheses":["other mechanism"],"mundane_hypotheses":["artifact"],"falsification_tests":["registered test"],"status":"FALSIFIED","confidence":"HIGH"})
        self.assertEqual(r["status"],"FALSIFIED"); self.assertTrue(r["confidence_can_decrease"])


if __name__ == "__main__": unittest.main()
