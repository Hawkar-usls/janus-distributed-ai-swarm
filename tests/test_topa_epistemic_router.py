import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from topa_epistemic_router import TOPAEpistemicRouter


class TOPAEpistemicRouterStrictScienceTests(unittest.TestCase):
    def setUp(self):
        self.router = TOPAEpistemicRouter()

    def test_source_fact_with_explicit_provenance_is_admissible(self):
        r = self.router.validate({
            "claim_id": "T1",
            "origin_agent_id": "SCOUT_X",
            "raw_claim_text": "Instrument log contains X.",
            "claim_text": "Source S records X.",
            "claim_kind": "OBSERVED",
            "evidence_class": "SOURCE_FACT",
            "provenance_channel": "INSTRUMENT_RECORD",
            "firsthand_status": "NOT_APPLICABLE",
            "event_time": "2026-08-23T00:00:00Z",
            "event_location": "test-site",
            "source_pointers": ["source://one"],
            "evidence_objects": ["sha256://fixture"],
            "status": "SOURCE_BOUND_FACT",
        })
        self.assertTrue(r["admissible"])
        self.assertFalse(r["world_truth_implied"])
        self.assertEqual(r["evidence_class"], "SOURCE_FACT")

    def test_source_fact_missing_provenance_is_rejected(self):
        r = self.router.assess({
            "claim_id": "T2",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "X occurred.",
            "claim_kind": "OBSERVED",
            "evidence_class": "SOURCE_FACT",
            "status": "SOURCE_BOUND_FACT",
        })
        self.assertIn("SOURCE_FACT_REQUIRES_SOURCE_POINTER", r["errors"])
        self.assertIn("SOURCE_FACT_REQUIRES_PROVENANCE_CHANNEL", r["errors"])

    def test_context_channel_does_not_become_new_independent_physical_evidence(self):
        r = self.router.assess({
            "claim_id": "T3",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "A guest said X.",
            "claim_kind": "REPORTED",
            "evidence_class": "SOURCE_FACT",
            "provenance_channel": "GUEST_CLAIM",
            "firsthand_status": "HEARSAY",
            "source_pointers": ["source://show"],
            "status": "SOURCE_BOUND_FACT",
        })
        self.assertIn("SOURCE_FACT_IS_REPORT_OCCURRENCE_ONLY_FOR_CONTEXT_CHANNEL", r["warnings"])
        self.assertFalse(r["world_truth_implied"])

    def test_institution_and_self_identification_are_not_truth(self):
        r = self.router.assess({
            "claim_id": "T4",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Source claims authority.",
            "claim_kind": "REPORTED",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["authenticate source independently"],
            "status": "HYPOTHESIS_ONLY",
            "institutional_role_as_truth": True,
            "self_identification_authenticates_source": True,
        })
        self.assertIn("INSTITUTIONAL_STATUS_IS_PROVENANCE_NOT_TRUTH", r["errors"])
        self.assertIn("SELF_IDENTIFIED_SOURCE_IS_NOT_AUTHENTICATED", r["errors"])

    def test_raw_ledger_rewrite_rejected(self):
        r = self.router.assess({
            "claim_id": "T5",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Normalized story.",
            "raw_claim_text": "Original story.",
            "claim_kind": "REPORTED",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["source comparison"],
            "status": "HYPOTHESIS_ONLY",
            "raw_ledger_rewritten": True,
        })
        self.assertIn("MODELS_MAY_NOT_REWRITE_RAW_LEDGER", r["errors"])

    def test_prediction_must_be_frozen_before_scoring(self):
        r = self.router.assess({
            "claim_id": "T6",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "X will happen.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["deadline check"],
            "status": "HYPOTHESIS_ONLY",
            "prediction": {"frozen": False, "scored": True, "score": "PASS"},
        })
        self.assertIn("PREDICTION_NOT_FROZEN_BEFORE_SCORING", r["errors"])

    def test_post_hoc_prediction_rescue_rejected(self):
        r = self.router.assess({
            "claim_id": "T7",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Prediction revised after deadline.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["deadline check"],
            "status": "HYPOTHESIS_ONLY",
            "prediction": {
                "frozen": True,
                "scored": True,
                "post_hoc_redefined": True,
                "prediction_timestamp": "t0",
                "deadline_or_window": "d",
                "success_criterion": "x",
                "failure_criterion": "not x",
                "analysis_rule": "binary frozen rule",
                "score": "PASS",
            },
        })
        self.assertIn("NO_POST_HOC_REDEFINITION_TO_SAVE_A_FAILED_PREDICTION", r["errors"])

    def test_uncalibrated_confidence_is_rejected(self):
        r = self.router.assess({
            "claim_id": "T8",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "This feels highly likely.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["independent measurement"],
            "status": "HYPOTHESIS_ONLY",
            "confidence": "HIGH",
        })
        self.assertIn("UNCALIBRATED_CONFIDENCE_FORBIDDEN", r["errors"])

    def test_legacy_survivor_status_is_rejected(self):
        r = self.router.assess({
            "claim_id": "T9",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Candidate survived several checks.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["next independent test"],
            "status": "HARD_SURVIVOR",
        })
        self.assertIn("DEPRECATED_HEURISTIC_OR_LEGACY_STATUS_FORBIDDEN_FOR_ACTIVE_CLAIM", r["errors"])

    def test_hypothesis_cannot_promote_to_fact(self):
        r = self.router.assess({
            "claim_id": "T10",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Mechanism M explains X.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "falsification_tests": ["measure prediction P"],
            "status": "SOURCE_BOUND_FACT",
        })
        self.assertIn("HYPOTHESIS_CANNOT_BE_PROMOTED_AS_EVIDENCE", r["errors"])

    def test_hypothesis_only_with_falsifier_is_valid(self):
        r = self.router.validate({
            "claim_id": "T11",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Mechanism M may explain X.",
            "claim_kind": "HYPOTHESIS",
            "evidence_class": "HYPOTHESIS_ONLY",
            "alternative_hypotheses": ["mechanism N"],
            "falsification_tests": ["measure P under frozen protocol"],
            "status": "HYPOTHESIS_ONLY",
        })
        self.assertTrue(r["admissible"])
        self.assertFalse(r["world_truth_implied"])

    def test_finite_experiment_cannot_claim_universal_proof(self):
        r = self.router.assess({
            "claim_id": "T12",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Finite replay passed.",
            "claim_kind": "MEASURED",
            "evidence_class": "REPRODUCIBLE_EXPERIMENT",
            "evidence_objects": ["code://v1", "data://fixture"],
            "status": "PROVED_IN_SCOPE",
        })
        self.assertIn("FINITE_EXPERIMENT_CANNOT_BE_PROMOTED_TO_UNIVERSAL_PROOF", r["errors"])

    def test_statistical_inference_requires_explicit_model(self):
        r = self.router.assess({
            "claim_id": "T13",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Effect appears nonzero.",
            "claim_kind": "STATISTICAL",
            "evidence_class": "STATISTICAL_INFERENCE",
            "evidence_objects": ["data://frozen"],
            "falsification_tests": ["replication"],
            "status": "SUPPORTED_BY_STATISTICAL_INFERENCE_WITH_MODEL",
        })
        self.assertIn("STATISTICAL_INFERENCE_REQUIRES_MODEL", r["errors"])

    def test_statistical_inference_with_model_and_uncertainty_is_valid(self):
        r = self.router.validate({
            "claim_id": "T14",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Effect estimate under frozen model M.",
            "claim_kind": "STATISTICAL",
            "evidence_class": "STATISTICAL_INFERENCE",
            "evidence_objects": ["data://frozen", "code://analysis-v1"],
            "alternative_hypotheses": ["null effect"],
            "falsification_tests": ["independent replication"],
            "statistical_model": {
                "target": "effect",
                "model": "M",
                "uncertainty": "95% CI",
            },
            "status": "SUPPORTED_BY_STATISTICAL_INFERENCE_WITH_MODEL",
        })
        self.assertTrue(r["admissible"])

    def test_independent_replication_requires_dependency_analysis(self):
        r = self.router.assess({
            "claim_id": "T15",
            "origin_agent_id": "SCOUT_X",
            "claim_text": "Two sources report X.",
            "claim_kind": "REPORTED",
            "evidence_class": "SOURCE_FACT",
            "provenance_channel": "FIRSTHAND_REPORT",
            "firsthand_status": "FIRSTHAND",
            "source_pointers": ["source://a", "source://b"],
            "status": "SOURCE_BOUND_FACT",
            "independent_replication": True,
        })
        self.assertIn("INDEPENDENT_REPLICATION_REQUIRES_DEPENDENCY_ANALYSIS", r["errors"])


if __name__ == "__main__":
    unittest.main()
