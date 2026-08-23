import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from topa_epistemic_router import TOPAEpistemicRouter


class TOPAEpistemicRouterTests(unittest.TestCase):
    def setUp(self):
        self.router = TOPAEpistemicRouter()

    def test_direct_observation_requires_explicit_provenance(self):
        r = self.router.validate({
            "claim_id":"T1","origin_agent_id":"SCOUT_X",
            "raw_claim_text":"I saw X.","claim_text":"X was observed.","claim_kind":"OBSERVED",
            "provenance_channel":"DIRECT_OBSERVATION","firsthand_status":"FIRSTHAND",
            "event_time":"2026-08-23T00:00:00Z","event_location":"test-site",
            "source_pointers":["source://one"],"falsification_tests":["independent repeat"],
            "status":"SOURCE_BOUND_OBSERVATION","confidence":"MEDIUM"
        })
        self.assertTrue(r["admissible"])
        self.assertFalse(r["raw_ledger_rewritten"])

    def test_positive_claim_missing_provenance_rejected(self):
        r = self.router.assess({
            "claim_id":"T2","origin_agent_id":"SCOUT_X","claim_text":"X occurred.",
            "claim_kind":"OBSERVED","source_pointers":["source://one"],
            "status":"SOURCE_BOUND_OBSERVATION","confidence":"MEDIUM"
        })
        self.assertIn("PROVENANCE_CHANNEL_REQUIRED_FOR_PROMOTION", r["errors"])
        self.assertIn("FIRSTHAND_STATUS_REQUIRED_FOR_PROMOTION", r["errors"])

    def test_hearsay_cannot_be_direct_observation(self):
        r = self.router.assess({
            "claim_id":"T3","origin_agent_id":"SCOUT_X","claim_text":"A guest said X.",
            "claim_kind":"REPORTED","provenance_channel":"GUEST_CLAIM","firsthand_status":"HEARSAY",
            "source_pointers":["source://show"],"status":"SOURCE_BOUND_OBSERVATION","confidence":"HIGH"
        })
        self.assertIn("CONTEXT_CHANNEL_CANNOT_BECOME_DIRECT_OBSERVATION", r["errors"])
        self.assertIn("HEARSAY_CANNOT_BECOME_DIRECT_OBSERVATION", r["errors"])

    def test_institution_and_self_identification_are_not_truth(self):
        r = self.router.assess({
            "claim_id":"T4","origin_agent_id":"SCOUT_X","claim_text":"Source claims authority.",
            "claim_kind":"REPORTED","institutional_role_as_truth":True,
            "self_identification_authenticates_source":True
        })
        self.assertIn("INSTITUTIONAL_STATUS_IS_PROVENANCE_NOT_TRUTH", r["errors"])
        self.assertIn("SELF_IDENTIFIED_SOURCE_IS_NOT_AUTHENTICATED", r["errors"])

    def test_raw_ledger_rewrite_rejected(self):
        r = self.router.assess({
            "claim_id":"T5","origin_agent_id":"SCOUT_X","claim_text":"Normalized story.",
            "raw_claim_text":"Original story.","claim_kind":"REPORTED","raw_ledger_rewritten":True
        })
        self.assertIn("MODELS_MAY_NOT_REWRITE_RAW_LEDGER", r["errors"])

    def test_prediction_must_be_frozen_before_scoring(self):
        r = self.router.assess({
            "claim_id":"T6","origin_agent_id":"SCOUT_X","claim_text":"X will happen.",
            "claim_kind":"INFERRED",
            "prediction":{"frozen":False,"scored":True,"score":"PASS"}
        })
        self.assertIn("PREDICTION_NOT_FROZEN_BEFORE_SCORING", r["errors"])

    def test_post_hoc_prediction_rescue_rejected(self):
        r = self.router.assess({
            "claim_id":"T7","origin_agent_id":"SCOUT_X","claim_text":"Prediction revised after deadline.",
            "claim_kind":"INFERRED",
            "prediction":{
                "frozen":True,"scored":True,"post_hoc_redefined":True,
                "prediction_timestamp":"t0","deadline_or_window":"d",
                "success_criterion":"x","failure_criterion":"not x","score":"PASS"
            }
        })
        self.assertIn("NO_POST_HOC_REDEFINITION_TO_SAVE_A_FAILED_PREDICTION", r["errors"])

    def test_failed_prediction_cannot_remain_positive(self):
        r = self.router.assess({
            "claim_id":"T8","origin_agent_id":"SCOUT_X","claim_text":"X predicted.",
            "claim_kind":"INFERRED","provenance_channel":"MODEL_INFERENCE","firsthand_status":"NOT_APPLICABLE",
            "source_pointers":["source://claim"],"falsification_tests":["score deadline"],
            "status":"SUPPORTED_WITHIN_BOUND","confidence":"HIGH",
            "prediction":{
                "frozen":True,"scored":True,"prediction_timestamp":"t0","deadline_or_window":"d",
                "success_criterion":"x","failure_criterion":"not x","score":"FAIL"
            }
        })
        self.assertIn("FAILED_PREDICTION_CANNOT_REMAIN_POSITIVELY_PROMOTED", r["errors"])

    def test_unresolved_still_valid(self):
        r = self.router.validate({
            "claim_id":"T9","origin_agent_id":"SCOUT_X","claim_text":"Unusual event remains unexplained.",
            "claim_kind":"REPORTED","provenance_channel":"FIRSTHAND_REPORT","firsthand_status":"FIRSTHAND",
            "source_pointers":["source://one"],"alternative_hypotheses":["artifact"],
            "mundane_hypotheses":["artifact"],"falsification_tests":["independent repeat"],
            "status":"UNRESOLVED","confidence":"LOW"
        })
        self.assertTrue(r["admissible"])
        self.assertFalse(r["world_truth_implied"])


if __name__ == "__main__":
    unittest.main()
