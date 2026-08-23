#!/usr/bin/env python3
"""TOPA v1.1 epistemic claim-envelope validator for JANUS distributed swarm."""
from __future__ import annotations

from typing import Any, Dict, Iterable, List

ALLOWED_KINDS = {"REPORTED", "OBSERVED", "INFERRED", "SYMBOLIC", "PEER_MESSAGE"}
ALLOWED_STATUSES = {
    "UNVERIFIED_REPORT", "SOURCE_BOUND_OBSERVATION", "INFERRED_HYPOTHESIS",
    "SYMBOLIC_CONTEXT", "SUPPORTED_WITHIN_BOUND", "WEAKENED", "FALSIFIED", "UNRESOLVED",
}
POSITIVE_STATUSES = {"SOURCE_BOUND_OBSERVATION", "SUPPORTED_WITHIN_BOUND"}
INTERPRETIVE_STATUSES = {"INFERRED_HYPOTHESIS", "SUPPORTED_WITHIN_BOUND"}


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
        claim_kind = str(record.get("claim_kind") or "REPORTED").upper()
        status = str(record.get("status") or "UNVERIFIED_REPORT").upper()
        confidence = str(record.get("confidence") or "LOW").upper()

        raw_sources = record.get("source_pointers")
        if raw_sources is None:
            raw_sources = [record["source"]] if record.get("source") else []
        if not isinstance(raw_sources, list):
            raw_sources = [raw_sources]
        source_pointers = _strings(raw_sources)

        alternatives = record.get("alternative_hypotheses") or []
        if not isinstance(alternatives, list):
            alternatives = [alternatives]
        alternative_hypotheses = _strings(alternatives)

        tests = record.get("falsification_tests") or []
        if not isinstance(tests, list):
            tests = [tests]
        falsification_tests = _strings(tests)

        mundane = record.get("mundane_hypotheses") or []
        if not isinstance(mundane, list):
            mundane = [mundane]
        mundane_hypotheses = _strings(mundane)

        errors: List[str] = []
        warnings: List[str] = []

        if not claim_id: errors.append("MISSING_CLAIM_ID")
        if not origin_agent_id: errors.append("MISSING_ORIGIN_AGENT_ID")
        if not claim_text: errors.append("MISSING_CLAIM_TEXT")
        if claim_kind not in ALLOWED_KINDS: errors.append("INVALID_CLAIM_KIND")
        if status not in ALLOWED_STATUSES: errors.append("INVALID_STATUS")
        if confidence not in {"LOW", "MEDIUM", "HIGH"}: errors.append("INVALID_CONFIDENCE")

        source_bound = bool(source_pointers)
        if status in POSITIVE_STATUSES and not source_bound:
            errors.append("NO_SOURCE_NO_POSITIVE_FACT")
        if claim_kind in {"PEER_MESSAGE", "SYMBOLIC"} and status in POSITIVE_STATUSES:
            errors.append("CONTEXT_CANNOT_BE_PROMOTED_AS_EMPIRICAL_FACT")
        if status == "SUPPORTED_WITHIN_BOUND" and not falsification_tests:
            errors.append("FALSIFICATION_ROUTE_REQUIRED_FOR_PROMOTION")

        if bool(record.get("missing_data_as_evidence")):
            errors.append("MISSING_DATA_STAYS_MISSING")
        if bool(record.get("audience_size_as_corroboration")):
            errors.append("AUDIENCE_SIZE_IS_NOT_CORROBORATION")
        if bool(record.get("repetition_as_independence")):
            errors.append("REPETITION_IS_NOT_INDEPENDENCE")

        if claim_kind in {"INFERRED", "REPORTED"} and not alternative_hypotheses:
            warnings.append("ALTERNATIVE_HYPOTHESES_NOT_RECORDED")
        if claim_kind in {"INFERRED", "REPORTED"} and not mundane_hypotheses:
            warnings.append("MUNDANE_FIRST_NOT_RECORDED")

        closed_belief_loop = status in INTERPRETIVE_STATUSES and not falsification_tests
        if closed_belief_loop:
            warnings.append("CLOSED_BELIEF_LOOP_WARNING")

        unique_sources = len(source_pointers)
        claimed_independent = bool(record.get("independent_replication"))
        independent_replication = claimed_independent and unique_sources >= 2
        if claimed_independent and unique_sources < 2:
            warnings.append("INDEPENDENCE_NOT_ESTABLISHED_BY_DISTINCT_SOURCES")

        return {
            "schema": "janus.swarm.topa_assessment.v1.1",
            "claim_id": claim_id,
            "origin_agent_id": origin_agent_id,
            "claim_text": claim_text,
            "claim_kind": claim_kind,
            "source_pointers": source_pointers,
            "source_bound": source_bound,
            "alternative_hypotheses": alternative_hypotheses,
            "mundane_hypotheses": mundane_hypotheses,
            "falsification_tests": falsification_tests,
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
            "schema": "janus.swarm.topa_epistemic_router.v1.1",
            "foundation": "Hawkar-usls/Janus_Genesis:.janus/TOPA_FOUNDATION.json",
            "case_corpus": "Hawkar-usls/janus-meta-registry:data/JANUS-TOPA-ART-BELL-REAL-CALLER-ARCHIVE-v1.0.json",
            "core_rule": self.core_rule,
            "unknown_is_supernatural": False,
            "unresolved_is_valid": True,
            "consensus_is_world_truth": False,
            "negative_results_preserved": True,
            "missing_data_stays_missing": True,
            "audience_size_is_corroboration": False,
            "repetition_is_independence": False,
            "mundane_first_required": True,
            "failed_tests_can_lower_confidence": True,
            "closed_belief_loop_warning_enabled": True,
            "same_source_multi_agent_is_independent_replication": False,
        }


def self_test() -> None:
    router = TOPAEpistemicRouter()
    unresolved = router.validate({
        "claim_id": "C-1", "origin_agent_id": "SCOUT_A",
        "claim_text": "An unusual signal was reported.", "claim_kind": "REPORTED",
        "source_pointers": ["source://one"], "alternative_hypotheses": ["instrumental artifact"],
        "mundane_hypotheses": ["instrumental artifact"],
        "falsification_tests": ["repeat with independent instrument"],
        "status": "UNRESOLVED", "confidence": "LOW",
    })
    assert unresolved["admissible"] and not unresolved["world_truth_implied"]

    peer = router.assess({
        "claim_id": "C-2", "origin_agent_id": "SCOUT_B", "claim_text": "Peer says x is true.",
        "claim_kind": "PEER_MESSAGE", "status": "SUPPORTED_WITHIN_BOUND", "confidence": "HIGH",
    })
    assert "NO_SOURCE_NO_POSITIVE_FACT" in peer["errors"]
    assert "CONTEXT_CANNOT_BE_PROMOTED_AS_EMPIRICAL_FACT" in peer["errors"]

    bad = router.assess({
        "claim_id": "C-3", "origin_agent_id": "SCOUT_C", "claim_text": "Popular repeated claim.",
        "claim_kind": "INFERRED", "source_pointers": ["source://same"],
        "status": "INFERRED_HYPOTHESIS", "confidence": "MEDIUM",
        "missing_data_as_evidence": True, "audience_size_as_corroboration": True,
        "repetition_as_independence": True,
    })
    assert "MISSING_DATA_STAYS_MISSING" in bad["errors"]
    assert "AUDIENCE_SIZE_IS_NOT_CORROBORATION" in bad["errors"]
    assert "REPETITION_IS_NOT_INDEPENDENCE" in bad["errors"]
    assert "CLOSED_BELIEF_LOOP_WARNING" in bad["warnings"]

    falsified = router.validate({
        "claim_id": "C-4", "origin_agent_id": "SCOUT_D",
        "claim_text": "Candidate explanation failed its registered test.", "claim_kind": "INFERRED",
        "source_pointers": ["source://test"], "alternative_hypotheses": ["another mechanism"],
        "mundane_hypotheses": ["measurement artifact"], "falsification_tests": ["registered test"],
        "status": "FALSIFIED", "confidence": "HIGH",
    })
    assert falsified["status"] == "FALSIFIED" and falsified["confidence_can_decrease"]
    print("JANUS_TOPA_EPISTEMIC_ROUTER_V1_1_SELF_TEST=PASS")


if __name__ == "__main__":
    self_test()
