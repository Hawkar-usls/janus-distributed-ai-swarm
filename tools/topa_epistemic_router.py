#!/usr/bin/env python3
"""TOPA v1.2 epistemic claim-envelope validator for JANUS distributed swarm.

The router validates provenance, falsifiability and frozen prediction semantics.
It never decides world truth and never rewrites the raw claim ledger.
"""
from __future__ import annotations

from typing import Any, Dict, Iterable, List

ALLOWED_KINDS = {"REPORTED", "OBSERVED", "INFERRED", "SYMBOLIC", "PEER_MESSAGE"}
ALLOWED_STATUSES = {
    "UNVERIFIED_REPORT", "SOURCE_BOUND_OBSERVATION", "INFERRED_HYPOTHESIS",
    "SYMBOLIC_CONTEXT", "SUPPORTED_WITHIN_BOUND", "WEAKENED", "FALSIFIED", "UNRESOLVED",
}
POSITIVE_STATUSES = {"SOURCE_BOUND_OBSERVATION", "SUPPORTED_WITHIN_BOUND"}
INTERPRETIVE_STATUSES = {"INFERRED_HYPOTHESIS", "SUPPORTED_WITHIN_BOUND"}
ALLOWED_CHANNELS = {
    "DIRECT_OBSERVATION", "FIRSTHAND_REPORT", "HEARSAY_REPORT", "PEER_MESSAGE",
    "GUEST_CLAIM", "LISTENER_EMAIL", "HOST_STATEMENT", "SYMBOLIC_CONTEXT", "MODEL_INFERENCE",
}
ALLOWED_FIRSTHAND = {"FIRSTHAND", "HEARSAY", "NOT_APPLICABLE", "UNKNOWN"}
CONTEXT_ONLY_CHANNELS = {
    "HEARSAY_REPORT", "PEER_MESSAGE", "GUEST_CLAIM", "LISTENER_EMAIL",
    "HOST_STATEMENT", "SYMBOLIC_CONTEXT", "MODEL_INFERENCE",
}


def _strings(values: Iterable[Any]) -> List[str]:
    out: List[str] = []
    for value in values:
        text = str(value).strip()
        if text and text not in out:
            out.append(text)
    return out


class TOPAEpistemicRouter:
    core_rule = "ANOMALY_IS_A_QUESTION_NOT_A_CONCLUSION"

    def assess(self, record: Dict[str, Any]) -> Dict[str, Any]:
        if not isinstance(record, dict):
            raise TypeError("TOPA record must be an object")

        claim_id = str(record.get("claim_id") or "").strip()
        origin_agent_id = str(record.get("origin_agent_id") or "").strip()
        claim_text = str(record.get("claim_text") or record.get("claim") or record.get("fact") or "").strip()
        raw_claim_text = str(record.get("raw_claim_text") or claim_text).strip()
        claim_kind = str(record.get("claim_kind") or "REPORTED").upper()
        status = str(record.get("status") or "UNVERIFIED_REPORT").upper()
        confidence = str(record.get("confidence") or "LOW").upper()
        provenance_channel = str(record.get("provenance_channel") or "").upper()
        firsthand_status = str(record.get("firsthand_status") or "").upper()
        event_time = record.get("event_time")
        event_location = record.get("event_location")

        raw_sources = record.get("source_pointers")
        if raw_sources is None:
            raw_sources = [record["source"]] if record.get("source") else []
        if not isinstance(raw_sources, list):
            raw_sources = [raw_sources]
        source_pointers = _strings(raw_sources)

        alternatives = record.get("alternative_hypotheses") or []
        if not isinstance(alternatives, list): alternatives = [alternatives]
        alternative_hypotheses = _strings(alternatives)

        tests = record.get("falsification_tests") or []
        if not isinstance(tests, list): tests = [tests]
        falsification_tests = _strings(tests)

        mundane = record.get("mundane_hypotheses") or []
        if not isinstance(mundane, list): mundane = [mundane]
        mundane_hypotheses = _strings(mundane)

        prediction = record.get("prediction") if isinstance(record.get("prediction"), dict) else None
        errors: List[str] = []
        warnings: List[str] = []

        if not claim_id: errors.append("MISSING_CLAIM_ID")
        if not origin_agent_id: errors.append("MISSING_ORIGIN_AGENT_ID")
        if not claim_text: errors.append("MISSING_CLAIM_TEXT")
        if not raw_claim_text: errors.append("MISSING_RAW_CLAIM_TEXT")
        if claim_kind not in ALLOWED_KINDS: errors.append("INVALID_CLAIM_KIND")
        if status not in ALLOWED_STATUSES: errors.append("INVALID_STATUS")
        if confidence not in {"LOW", "MEDIUM", "HIGH"}: errors.append("INVALID_CONFIDENCE")

        if provenance_channel and provenance_channel not in ALLOWED_CHANNELS:
            errors.append("INVALID_PROVENANCE_CHANNEL")
        if firsthand_status and firsthand_status not in ALLOWED_FIRSTHAND:
            errors.append("INVALID_FIRSTHAND_STATUS")
        if status in POSITIVE_STATUSES and not provenance_channel:
            errors.append("PROVENANCE_CHANNEL_REQUIRED_FOR_PROMOTION")
        if status in POSITIVE_STATUSES and not firsthand_status:
            errors.append("FIRSTHAND_STATUS_REQUIRED_FOR_PROMOTION")
        if not provenance_channel:
            warnings.append("PROVENANCE_CHANNEL_NOT_RECORDED")
        if not firsthand_status:
            warnings.append("FIRSTHAND_STATUS_NOT_RECORDED")

        source_bound = bool(source_pointers)
        if status in POSITIVE_STATUSES and not source_bound:
            errors.append("NO_SOURCE_NO_POSITIVE_FACT")
        if claim_kind in {"PEER_MESSAGE", "SYMBOLIC"} and status in POSITIVE_STATUSES:
            errors.append("CONTEXT_CANNOT_BE_PROMOTED_AS_EMPIRICAL_FACT")
        if provenance_channel in CONTEXT_ONLY_CHANNELS and status == "SOURCE_BOUND_OBSERVATION":
            errors.append("CONTEXT_CHANNEL_CANNOT_BECOME_DIRECT_OBSERVATION")
        if firsthand_status == "HEARSAY" and status == "SOURCE_BOUND_OBSERVATION":
            errors.append("HEARSAY_CANNOT_BECOME_DIRECT_OBSERVATION")
        if status == "SUPPORTED_WITHIN_BOUND" and not falsification_tests:
            errors.append("FALSIFICATION_ROUTE_REQUIRED_FOR_PROMOTION")

        if bool(record.get("missing_data_as_evidence")): errors.append("MISSING_DATA_STAYS_MISSING")
        if bool(record.get("audience_size_as_corroboration")): errors.append("AUDIENCE_SIZE_IS_NOT_CORROBORATION")
        if bool(record.get("repetition_as_independence")): errors.append("REPETITION_IS_NOT_INDEPENDENCE")
        if bool(record.get("institutional_role_as_truth")): errors.append("INSTITUTIONAL_STATUS_IS_PROVENANCE_NOT_TRUTH")
        if bool(record.get("self_identification_authenticates_source")): errors.append("SELF_IDENTIFIED_SOURCE_IS_NOT_AUTHENTICATED")
        if bool(record.get("raw_ledger_rewritten")): errors.append("MODELS_MAY_NOT_REWRITE_RAW_LEDGER")
        if bool(record.get("channel_reclassified_without_provenance")): errors.append("CHANNEL_MAY_NOT_BE_SILENTLY_RECLASSIFIED")

        if claim_kind in {"INFERRED", "REPORTED"} and not alternative_hypotheses:
            warnings.append("ALTERNATIVE_HYPOTHESES_NOT_RECORDED")
        if claim_kind in {"INFERRED", "REPORTED"} and not mundane_hypotheses:
            warnings.append("MUNDANE_FIRST_NOT_RECORDED")
        if event_time is None: warnings.append("EVENT_TIME_NOT_RECORDED")
        if event_location is None: warnings.append("EVENT_LOCATION_NOT_RECORDED")

        closed_belief_loop = status in INTERPRETIVE_STATUSES and not falsification_tests
        if closed_belief_loop:
            warnings.append("CLOSED_BELIEF_LOOP_WARNING")

        unique_sources = len(source_pointers)
        claimed_independent = bool(record.get("independent_replication"))
        independent_replication = claimed_independent and unique_sources >= 2
        if claimed_independent and unique_sources < 2:
            warnings.append("INDEPENDENCE_NOT_ESTABLISHED_BY_DISTINCT_SOURCES")

        prediction_state = None
        if prediction is not None:
            frozen = bool(prediction.get("frozen"))
            scored = bool(prediction.get("scored"))
            post_hoc_redefined = bool(prediction.get("post_hoc_redefined"))
            required_freeze = ["prediction_timestamp", "deadline_or_window", "success_criterion", "failure_criterion"]
            missing_freeze = [k for k in required_freeze if not prediction.get(k)]
            if frozen and missing_freeze:
                errors.append("FROZEN_PREDICTION_MISSING_CRITERIA")
            if scored and not frozen:
                errors.append("PREDICTION_NOT_FROZEN_BEFORE_SCORING")
            if post_hoc_redefined:
                errors.append("NO_POST_HOC_REDEFINITION_TO_SAVE_A_FAILED_PREDICTION")
            score = str(prediction.get("score") or "").upper() or None
            if score and score not in {"PASS", "FAIL", "PARTIAL_WITH_PREDECLARED_TOLERANCE", "UNSCORABLE_TOO_VAGUE", "UNRESOLVED_MISSING_OUTCOME_DATA"}:
                errors.append("INVALID_PREDICTION_SCORE")
            if score == "FAIL" and status in POSITIVE_STATUSES:
                errors.append("FAILED_PREDICTION_CANNOT_REMAIN_POSITIVELY_PROMOTED")
            prediction_state = {
                "frozen": frozen,
                "scored": scored,
                "score": score,
                "post_hoc_redefined": post_hoc_redefined,
                "missing_freeze_fields": missing_freeze,
                "confidence_may_decrease_on_failure": True,
            }

        return {
            "schema": "janus.swarm.topa_assessment.v1.2",
            "claim_id": claim_id,
            "origin_agent_id": origin_agent_id,
            "raw_claim_text": raw_claim_text,
            "claim_text": claim_text,
            "raw_ledger_rewritten": False,
            "claim_kind": claim_kind,
            "provenance_channel": provenance_channel or None,
            "firsthand_status": firsthand_status or None,
            "event_time": event_time,
            "event_location": event_location,
            "source_pointers": source_pointers,
            "source_bound": source_bound,
            "alternative_hypotheses": alternative_hypotheses,
            "mundane_hypotheses": mundane_hypotheses,
            "falsification_tests": falsification_tests,
            "prediction": prediction_state,
            "status": status,
            "confidence": confidence,
            "independent_replication": independent_replication,
            "closed_belief_loop": closed_belief_loop,
            "confidence_can_decrease": True,
            "errors": errors,
            "warnings": warnings,
            "admissible": not errors,
            "world_truth_implied": False,
            "core_rule": self.core_rule,
        }

    def validate(self, record: Dict[str, Any]) -> Dict[str, Any]:
        assessment = self.assess(record)
        if assessment["errors"]:
            raise ValueError("TOPA_REJECT:" + ",".join(assessment["errors"]))
        return assessment

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": "janus.swarm.topa_epistemic_router.v1.2",
            "foundation": "Hawkar-usls/Janus_Genesis:.janus/TOPA_FOUNDATION.json",
            "case_corpus": "Hawkar-usls/janus-meta-registry:data/JANUS-TOPA-ART-BELL-REAL-CALLER-ARCHIVE-v1.2.json",
            "observer_pattern": "Hawkar-usls/janus-meta-registry:data/JANUS-TOPA-ART-BELL-OBSERVER-NODE-PREY-2006-v1.0.json",
            "observer_pattern_is_empirical_evidence": False,
            "core_rule": self.core_rule,
            "raw_provenance_preserved": True,
            "channel_classification_required_for_promotion": True,
            "firsthand_hearsay_distinct": True,
            "prediction_must_be_frozen_before_scoring": True,
            "post_hoc_prediction_redefinition_allowed": False,
            "model_may_rewrite_raw_ledger": False,
            "unknown_is_supernatural": False,
            "unresolved_is_valid": True,
            "consensus_is_world_truth": False,
            "same_source_multi_agent_is_independent_replication": False,
        }


def self_test() -> None:
    router = TOPAEpistemicRouter()
    observed = router.validate({
        "claim_id": "C-1", "origin_agent_id": "SCOUT_A",
        "raw_claim_text": "I saw a light at 03:00.", "claim_text": "A light was reported at 03:00.",
        "claim_kind": "OBSERVED", "provenance_channel": "DIRECT_OBSERVATION", "firsthand_status": "FIRSTHAND",
        "event_time": "03:00", "event_location": "test-site", "source_pointers": ["source://one"],
        "falsification_tests": ["independent instrument check"], "status": "SOURCE_BOUND_OBSERVATION", "confidence": "MEDIUM",
    })
    assert observed["admissible"] and observed["raw_ledger_rewritten"] is False

    hearsay = router.assess({
        "claim_id": "C-2", "origin_agent_id": "SCOUT_B", "claim_text": "A guest said x happened.",
        "claim_kind": "REPORTED", "provenance_channel": "GUEST_CLAIM", "firsthand_status": "HEARSAY",
        "source_pointers": ["source://show"], "status": "SOURCE_BOUND_OBSERVATION", "confidence": "HIGH",
    })
    assert "CONTEXT_CHANNEL_CANNOT_BECOME_DIRECT_OBSERVATION" in hearsay["errors"]
    assert "HEARSAY_CANNOT_BECOME_DIRECT_OBSERVATION" in hearsay["errors"]

    prediction = router.validate({
        "claim_id": "C-3", "origin_agent_id": "SCOUT_C", "claim_text": "Event X will occur before deadline D.",
        "claim_kind": "INFERRED", "provenance_channel": "MODEL_INFERENCE", "firsthand_status": "NOT_APPLICABLE",
        "source_pointers": ["source://claim"], "alternative_hypotheses": ["no event"], "mundane_hypotheses": ["coincidence"],
        "falsification_tests": ["score after deadline"], "status": "INFERRED_HYPOTHESIS", "confidence": "LOW",
        "prediction": {"frozen": True, "scored": True, "prediction_timestamp": "2026-08-23T00:00:00Z", "deadline_or_window": "2026-08-24", "success_criterion": "X observed", "failure_criterion": "X absent", "score": "FAIL"}
    })
    assert prediction["prediction"]["score"] == "FAIL" and prediction["confidence_can_decrease"]

    rescued = router.assess({
        "claim_id": "C-4", "origin_agent_id": "SCOUT_D", "claim_text": "Prediction rescued after failure.",
        "claim_kind": "INFERRED", "prediction": {"frozen": True, "scored": True, "post_hoc_redefined": True, "prediction_timestamp": "t0", "deadline_or_window": "d", "success_criterion": "x", "failure_criterion": "not x", "score": "PASS"}
    })
    assert "NO_POST_HOC_REDEFINITION_TO_SAVE_A_FAILED_PREDICTION" in rescued["errors"]
    print("JANUS_TOPA_EPISTEMIC_ROUTER_V1_2_SELF_TEST=PASS")


if __name__ == "__main__":
    self_test()
