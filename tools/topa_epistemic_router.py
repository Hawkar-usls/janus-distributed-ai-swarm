#!/usr/bin/env python3
"""TOPA v1.3 strict-science claim-envelope validator.

Scientific authority is restricted to source facts, formal derivations,
reproducible experiments and specified statistical inference. Hypotheses
may generate tests but do not become evidence by score, model vote,
"survivor" rank or uncalibrated confidence.
"""
from __future__ import annotations

from typing import Any, Dict, Iterable, List

ALLOWED_KINDS = {
    "REPORTED", "OBSERVED", "MEASURED", "DERIVED", "HYPOTHESIS",
    "STATISTICAL", "SYMBOLIC", "PEER_MESSAGE",
}
ALLOWED_EVIDENCE_CLASSES = {
    "SOURCE_FACT", "FORMAL_DERIVATION", "REPRODUCIBLE_EXPERIMENT",
    "STATISTICAL_INFERENCE", "HYPOTHESIS_ONLY",
}
ALLOWED_STATUSES = {
    "SOURCE_BOUND_FACT", "PROVED_IN_SCOPE", "REFUTED", "CONDITIONAL",
    "REPRODUCED_FINITE_MECHANICS", "SUPPORTED_BY_STATISTICAL_INFERENCE_WITH_MODEL",
    "HYPOTHESIS_ONLY", "UNRESOLVED", "INSUFFICIENT_DATA", "I_DO_NOT_KNOW",
}
ALLOWED_CHANNELS = {
    "DIRECT_OBSERVATION", "FIRSTHAND_REPORT", "HEARSAY_REPORT", "PEER_MESSAGE",
    "GUEST_CLAIM", "LISTENER_EMAIL", "HOST_STATEMENT", "SYMBOLIC_CONTEXT",
    "MODEL_INFERENCE", "FORMAL_SOURCE", "INSTRUMENT_RECORD", "EXECUTABLE_REPLAY",
}
ALLOWED_FIRSTHAND = {"FIRSTHAND", "HEARSAY", "NOT_APPLICABLE", "UNKNOWN"}
CONTEXT_ONLY_CHANNELS = {
    "HEARSAY_REPORT", "PEER_MESSAGE", "GUEST_CLAIM", "LISTENER_EMAIL",
    "HOST_STATEMENT", "SYMBOLIC_CONTEXT", "MODEL_INFERENCE",
}
DEPRECATED_ACTIVE_STATUSES = {
    "UNVERIFIED_REPORT", "SOURCE_BOUND_OBSERVATION", "INFERRED_HYPOTHESIS",
    "SYMBOLIC_CONTEXT", "SUPPORTED_WITHIN_BOUND", "WEAKENED",
    "PROBABLE_CONVENTIONAL", "UNRESOLVED_NONEXOTIC", "DATA_POOR_SURVIVOR",
    "HARD_SURVIVOR", "LIKELY_CONVENTIONAL",
}


def _strings(values: Iterable[Any]) -> List[str]:
    out: List[str] = []
    for value in values:
        text = str(value).strip()
        if text and text not in out:
            out.append(text)
    return out


def _as_list(value: Any) -> List[Any]:
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


class TOPAEpistemicRouter:
    core_rule = "ANOMALY_IS_A_QUESTION_NOT_A_CONCLUSION"
    science_rule = "HEURISTIC_AUTHORITY_FORBIDDEN"

    def assess(self, record: Dict[str, Any]) -> Dict[str, Any]:
        if not isinstance(record, dict):
            raise TypeError("TOPA record must be an object")

        claim_id = str(record.get("claim_id") or "").strip()
        origin_agent_id = str(record.get("origin_agent_id") or "").strip()
        claim_text = str(record.get("claim_text") or record.get("claim") or record.get("fact") or "").strip()
        raw_claim_text = str(record.get("raw_claim_text") or claim_text).strip()
        claim_kind = str(record.get("claim_kind") or "REPORTED").upper()
        status = str(record.get("status") or "UNRESOLVED").upper()
        evidence_class = str(record.get("evidence_class") or "").upper()
        provenance_channel = str(record.get("provenance_channel") or "").upper()
        firsthand_status = str(record.get("firsthand_status") or "").upper()
        event_time = record.get("event_time")
        event_location = record.get("event_location")

        source_pointers = _strings(_as_list(record.get("source_pointers")) or _as_list(record.get("source")))
        evidence_objects = _strings(_as_list(record.get("evidence_objects")))
        alternatives = _strings(_as_list(record.get("alternative_hypotheses")))
        tests = _strings(_as_list(record.get("falsification_tests")))
        independence_evidence = _strings(_as_list(record.get("independence_evidence")))

        prediction = record.get("prediction") if isinstance(record.get("prediction"), dict) else None
        probability_model = record.get("probability_model") if isinstance(record.get("probability_model"), dict) else None
        statistical_model = record.get("statistical_model") if isinstance(record.get("statistical_model"), dict) else None

        errors: List[str] = []
        warnings: List[str] = []

        if not claim_id: errors.append("MISSING_CLAIM_ID")
        if not origin_agent_id: errors.append("MISSING_ORIGIN_AGENT_ID")
        if not claim_text: errors.append("MISSING_CLAIM_TEXT")
        if not raw_claim_text: errors.append("MISSING_RAW_CLAIM_TEXT")
        if claim_kind not in ALLOWED_KINDS: errors.append("INVALID_CLAIM_KIND")
        if status in DEPRECATED_ACTIVE_STATUSES:
            errors.append("DEPRECATED_HEURISTIC_OR_LEGACY_STATUS_FORBIDDEN_FOR_ACTIVE_CLAIM")
        elif status not in ALLOWED_STATUSES:
            errors.append("INVALID_STATUS")
        if evidence_class not in ALLOWED_EVIDENCE_CLASSES:
            errors.append("MISSING_OR_INVALID_EVIDENCE_CLASS")

        if provenance_channel and provenance_channel not in ALLOWED_CHANNELS:
            errors.append("INVALID_PROVENANCE_CHANNEL")
        if firsthand_status and firsthand_status not in ALLOWED_FIRSTHAND:
            errors.append("INVALID_FIRSTHAND_STATUS")

        if "confidence" in record and probability_model is None:
            errors.append("UNCALIBRATED_CONFIDENCE_FORBIDDEN")
        for field, code in {
            "heuristic_score": "HEURISTIC_SCORE_FORBIDDEN_AS_AUTHORITY",
            "survivor_score": "SURVIVOR_SCORE_FORBIDDEN_AS_AUTHORITY",
            "survivor_label": "SURVIVOR_LABEL_FORBIDDEN_AS_AUTHORITY",
            "model_vote_as_evidence": "MODEL_VOTE_IS_NOT_EVIDENCE",
            "consensus_as_truth": "CONSENSUS_IS_NOT_WORLD_TRUTH",
            "visual_impression_as_evidence": "VISUAL_IMPRESSION_IS_NOT_EVIDENCE",
            "audience_size_as_corroboration": "AUDIENCE_SIZE_IS_NOT_CORROBORATION",
            "repetition_as_independence": "REPETITION_IS_NOT_INDEPENDENCE",
            "institutional_role_as_truth": "INSTITUTIONAL_STATUS_IS_PROVENANCE_NOT_TRUTH",
            "self_identification_authenticates_source": "SELF_IDENTIFIED_SOURCE_IS_NOT_AUTHENTICATED",
            "missing_data_as_evidence": "MISSING_DATA_STAYS_MISSING",
            "raw_ledger_rewritten": "MODELS_MAY_NOT_REWRITE_RAW_LEDGER",
        }.items():
            if bool(record.get(field)):
                errors.append(code)

        if evidence_class == "SOURCE_FACT":
            if not source_pointers: errors.append("SOURCE_FACT_REQUIRES_SOURCE_POINTER")
            if not provenance_channel: errors.append("SOURCE_FACT_REQUIRES_PROVENANCE_CHANNEL")
            if provenance_channel in CONTEXT_ONLY_CHANNELS and status == "SOURCE_BOUND_FACT":
                warnings.append("SOURCE_FACT_IS_REPORT_OCCURRENCE_ONLY_FOR_CONTEXT_CHANNEL")

        if evidence_class == "FORMAL_DERIVATION":
            if not evidence_objects and not source_pointers:
                errors.append("FORMAL_DERIVATION_REQUIRES_DERIVATION_OR_SOURCE_OBJECT")
            if status not in {"PROVED_IN_SCOPE", "REFUTED", "CONDITIONAL", "UNRESOLVED", "INSUFFICIENT_DATA"}:
                errors.append("FORMAL_DERIVATION_STATUS_MISMATCH")

        if evidence_class == "REPRODUCIBLE_EXPERIMENT":
            if not evidence_objects:
                errors.append("REPRODUCIBLE_EXPERIMENT_REQUIRES_VERSIONED_EVIDENCE_OBJECT")
            if status == "PROVED_IN_SCOPE":
                errors.append("FINITE_EXPERIMENT_CANNOT_BE_PROMOTED_TO_UNIVERSAL_PROOF")

        if evidence_class == "STATISTICAL_INFERENCE":
            if statistical_model is None:
                errors.append("STATISTICAL_INFERENCE_REQUIRES_MODEL")
            else:
                missing = [key for key in ["target", "model", "uncertainty"] if not statistical_model.get(key)]
                if missing: errors.append("STATISTICAL_MODEL_MISSING:" + ",".join(missing))
            if status == "SUPPORTED_BY_STATISTICAL_INFERENCE_WITH_MODEL" and statistical_model is None:
                errors.append("STATISTICAL_STATUS_REQUIRES_MODEL")

        if evidence_class == "HYPOTHESIS_ONLY":
            if status not in {"HYPOTHESIS_ONLY", "UNRESOLVED", "INSUFFICIENT_DATA", "I_DO_NOT_KNOW"}:
                errors.append("HYPOTHESIS_CANNOT_BE_PROMOTED_AS_EVIDENCE")
            if not tests:
                errors.append("HYPOTHESIS_REQUIRES_FALSIFICATION_ROUTE")

        if probability_model is not None:
            missing = [key for key in ["model", "conditioning", "estimation_or_calibration", "validation", "uncertainty"] if not probability_model.get(key)]
            if missing: errors.append("PROBABILITY_MODEL_MISSING:" + ",".join(missing))

        if claim_kind in {"HYPOTHESIS", "STATISTICAL", "DERIVED"} and not alternatives:
            warnings.append("ALTERNATIVE_HYPOTHESES_NOT_RECORDED")
        if event_time is None and claim_kind in {"REPORTED", "OBSERVED", "MEASURED"}:
            warnings.append("EVENT_TIME_NOT_RECORDED")
        if event_location is None and claim_kind in {"REPORTED", "OBSERVED", "MEASURED"}:
            warnings.append("EVENT_LOCATION_NOT_RECORDED")

        closed_belief_loop = evidence_class == "HYPOTHESIS_ONLY" and not tests
        if closed_belief_loop: warnings.append("CLOSED_BELIEF_LOOP_WARNING")

        claimed_independent = bool(record.get("independent_replication"))
        independent_replication = False
        if claimed_independent:
            if len(source_pointers) < 2:
                errors.append("INDEPENDENT_REPLICATION_REQUIRES_MULTIPLE_SOURCE_POINTERS")
            if not independence_evidence:
                errors.append("INDEPENDENT_REPLICATION_REQUIRES_DEPENDENCY_ANALYSIS")
            independent_replication = len(source_pointers) >= 2 and bool(independence_evidence)

        prediction_state = None
        if prediction is not None:
            frozen = bool(prediction.get("frozen"))
            scored = bool(prediction.get("scored"))
            post_hoc_redefined = bool(prediction.get("post_hoc_redefined"))
            required_freeze = ["prediction_timestamp", "deadline_or_window", "success_criterion", "failure_criterion", "analysis_rule"]
            missing_freeze = [key for key in required_freeze if not prediction.get(key)]
            if frozen and missing_freeze: errors.append("FROZEN_PREDICTION_MISSING_CRITERIA")
            if scored and not frozen: errors.append("PREDICTION_NOT_FROZEN_BEFORE_SCORING")
            if post_hoc_redefined: errors.append("NO_POST_HOC_REDEFINITION_TO_SAVE_A_FAILED_PREDICTION")
            score = str(prediction.get("score") or "").upper() or None
            allowed_scores = {"PASS", "FAIL", "PARTIAL_WITH_PREDECLARED_TOLERANCE", "UNSCORABLE_TOO_VAGUE", "UNRESOLVED_MISSING_OUTCOME_DATA"}
            if score and score not in allowed_scores: errors.append("INVALID_PREDICTION_SCORE")
            prediction_state = {
                "frozen": frozen, "scored": scored, "score": score,
                "post_hoc_redefined": post_hoc_redefined,
                "missing_freeze_fields": missing_freeze,
            }

        heuristic_codes = ("UNCALIBRATED", "HEURISTIC", "SURVIVOR", "MODEL_VOTE", "CONSENSUS", "VISUAL")
        return {
            "schema": "janus.swarm.topa_assessment.v1.3",
            "claim_id": claim_id,
            "origin_agent_id": origin_agent_id,
            "raw_claim_text": raw_claim_text,
            "claim_text": claim_text,
            "raw_ledger_rewritten": False,
            "claim_kind": claim_kind,
            "evidence_class": evidence_class or None,
            "provenance_channel": provenance_channel or None,
            "firsthand_status": firsthand_status or None,
            "event_time": event_time,
            "event_location": event_location,
            "source_pointers": source_pointers,
            "evidence_objects": evidence_objects,
            "alternative_hypotheses": alternatives,
            "falsification_tests": tests,
            "independence_evidence": independence_evidence,
            "independent_replication": independent_replication,
            "prediction": prediction_state,
            "probability_model_present": probability_model is not None,
            "statistical_model_present": statistical_model is not None,
            "status": status,
            "closed_belief_loop": closed_belief_loop,
            "heuristic_authority_used": any(code.startswith(heuristic_codes) for code in errors),
            "errors": errors,
            "warnings": warnings,
            "admissible": not errors,
            "world_truth_implied": False,
            "core_rule": self.core_rule,
            "science_rule": self.science_rule,
        }

    def validate(self, record: Dict[str, Any]) -> Dict[str, Any]:
        assessment = self.assess(record)
        if assessment["errors"]:
            raise ValueError("TOPA_REJECT:" + ",".join(assessment["errors"]))
        return assessment

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": "janus.swarm.topa_epistemic_router.v1.3",
            "foundation": "Hawkar-usls/TOPA:protocols/TOPA_FOUNDATION.json",
            "strict_science_gate": "Hawkar-usls/TOPA:data/TOPA_STRICT_SCIENCE_GATE_V1_0.json",
            "raw_provenance_preserved": True,
            "uncalibrated_confidence_allowed": False,
            "heuristic_score_allowed_as_evidence": False,
            "survivor_label_allowed_as_evidence": False,
            "model_consensus_is_world_truth": False,
            "hypothesis_is_evidence": False,
            "prediction_must_be_frozen_before_scoring": True,
            "post_hoc_prediction_redefinition_allowed": False,
            "unresolved_is_valid": True,
        }


def self_test() -> None:
    router = TOPAEpistemicRouter()
    source_fact = router.validate({
        "claim_id": "C-1", "origin_agent_id": "SCOUT_A",
        "raw_claim_text": "Instrument log contains event E.", "claim_text": "Source S records event E.",
        "claim_kind": "OBSERVED", "evidence_class": "SOURCE_FACT",
        "provenance_channel": "INSTRUMENT_RECORD", "firsthand_status": "NOT_APPLICABLE",
        "source_pointers": ["source://instrument-log"], "evidence_objects": ["sha256://fixture"],
        "status": "SOURCE_BOUND_FACT",
    })
    assert source_fact["admissible"]

    heuristic = router.assess({
        "claim_id": "C-2", "origin_agent_id": "SCOUT_B", "claim_text": "Looks strong.",
        "claim_kind": "HYPOTHESIS", "evidence_class": "HYPOTHESIS_ONLY",
        "falsification_tests": ["independent measurement"], "status": "HYPOTHESIS_ONLY", "confidence": "HIGH",
    })
    assert "UNCALIBRATED_CONFIDENCE_FORBIDDEN" in heuristic["errors"]

    legacy = router.assess({
        "claim_id": "C-3", "origin_agent_id": "SCOUT_C", "claim_text": "Legacy survivor label.",
        "claim_kind": "HYPOTHESIS", "evidence_class": "HYPOTHESIS_ONLY",
        "falsification_tests": ["test"], "status": "HARD_SURVIVOR",
    })
    assert "DEPRECATED_HEURISTIC_OR_LEGACY_STATUS_FORBIDDEN_FOR_ACTIVE_CLAIM" in legacy["errors"]

    statistical = router.validate({
        "claim_id": "C-4", "origin_agent_id": "SCOUT_D",
        "claim_text": "Estimated effect differs from zero under model M.",
        "claim_kind": "STATISTICAL", "evidence_class": "STATISTICAL_INFERENCE",
        "evidence_objects": ["data://frozen-dataset", "code://analysis-v1"],
        "alternative_hypotheses": ["null effect"], "falsification_tests": ["replication"],
        "statistical_model": {"target": "effect", "model": "M", "uncertainty": "95% CI"},
        "status": "SUPPORTED_BY_STATISTICAL_INFERENCE_WITH_MODEL",
    })
    assert statistical["admissible"]

    prediction = router.validate({
        "claim_id": "C-5", "origin_agent_id": "SCOUT_E", "claim_text": "Event X will occur before D.",
        "claim_kind": "HYPOTHESIS", "evidence_class": "HYPOTHESIS_ONLY",
        "alternative_hypotheses": ["no event"], "falsification_tests": ["frozen deadline check"],
        "status": "HYPOTHESIS_ONLY",
        "prediction": {
            "frozen": True, "scored": True, "prediction_timestamp": "2026-08-23T00:00:00Z",
            "deadline_or_window": "2026-08-24", "success_criterion": "X observed",
            "failure_criterion": "X absent", "analysis_rule": "binary presence/absence from frozen source",
            "score": "FAIL",
        },
    })
    assert prediction["prediction"]["score"] == "FAIL"
    print("JANUS_TOPA_EPISTEMIC_ROUTER_V1_3_STRICT_SCIENCE_SELF_TEST=PASS")


if __name__ == "__main__":
    self_test()
