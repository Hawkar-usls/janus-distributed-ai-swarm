#!/usr/bin/env python3
"""ESSENCE v1.1 prospective SHADOW record-reconstruction on real future CI jobs.

Uses the OLD complete per-entity SpiralLedger as fair baseline and the
existing dual-strand Genome ledger as candidate, on the same exact real
GitHub Actions completed-job records collected AFTER a preregistered freeze.
It is a task of *record recovery after completion*, NOT a prediction of a
future job result. Public GitHub sources, with dates and SHA receipts, are
not independently certified by an external custodian.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

from demiurge_spiral_swarm import SwarmSpiralController
from essence_v07_real_ci_replay import (
    SCHEMA as SOURCE_SCHEMA, digest, when, collect, LIMIT_ROWS)
from essence_v07_trial_gate import (
    PACKET, score, sha, preregistered, load as load_protocol)

ARM="idea-swarm-dual-strand-00"
ORIGIN="Hawkar-usls/janus-distributed-ai-swarm"
LOCKED="ba295aa8d44c0d735624e07dbd9163a20bb03cde28570aa516cd1708c068fef8"
EVIDENCE="janus.essence.v11.prospective_ci_shadow_receipt/1.1"
SOURCE="LIVE_PUBLIC_GITHUB_ACTIONS_METADATA"

def paired_rows(source,protocol):
    if (not isinstance(source,dict)
        or source.get("schema")!=SOURCE_SCHEMA
        or source.get("source")!=SOURCE
        or source.get("kind")!="RETROSPECTIVE_DEVELOPMENT_ONLY"):
        raise ValueError("INVALID_PUBLIC_JOB_SOURCE")
    records=source.get("records")
    if (not isinstance(records,list) or len(records)>LIMIT_ROWS
        or source.get("record_digest")!=digest(records)):
        raise ValueError("UNTRUSTED_JOB_SOURCE_DIGEST")
    hashes=source.get("response_hashes")
    if not isinstance(hashes,list) or not hashes:
        raise ValueError("NO_PUBLIC_API_PROVENANCE")
    for source_hash in hashes:
        if (not isinstance(source_hash,dict)
            or set(source_hash)!={"path","sha256"}
            or not isinstance(source_hash["path"],str)
            or "/actions/" not in source_hash["path"]
            or not isinstance(source_hash["sha256"],str)
            or len(source_hash["sha256"])!=64):
            raise ValueError("INVALID_API_RESPONSE_FINGERPRINT")
    preregistered(protocol)
    if sha(protocol)!=LOCKED:
        raise ValueError("FREEZE_PROTOCOL_CHANGED")
    eligible=[c for c in protocol["cohorts"]]
    seen=set();controller=SwarmSpiralController(working_window=32)
    sorted_rows=sorted(records,key=lambda x:(x["started_at"],x["repo"],x["job_id"]))
    for row in sorted_rows:
        if not isinstance(row,dict) or set(row)!={
            "repo","job_id","run_id","started_at","completed_at","conclusion"}:
            raise ValueError("UNTRUSTED_JOB_FIELDS")
        k=(row["repo"],row["job_id"])
        if k in seen:raise ValueError("JOB_ID_REPLAY")
        seen.add(k)
        if (row["repo"] not in (
            "Hawkar-usls/Janus_Genesis","Hawkar-usls/Janus-Demiurge",
            "Hawkar-usls/janus-meta-registry")
            or type(row["job_id"]) is not int
            or type(row["run_id"]) is not int
            or row["conclusion"] not in ("success","failure")
            or when(row["started_at"])>=when(row["completed_at"])):
            raise ValueError("INVALID_REAL_JOB")
        bad=row["conclusion"]=="failure"
        controller.integrate(
            row["repo"],{"job_id":row["job_id"],"failed":bad},
            lessons=["GITHUB_REAL_JOB_FAILURE"] if bad else [],
            promoted=not bad,
            outcome="REAL_JOB_FAILURE" if bad else "REAL_JOB_SUCCESS")
    controller.validate()
    baseline={}
    for entity,ledger in controller.ledgers.items():
        for turn in ledger.turns:
            state=turn.candidate_state
            baseline[(entity,state["job_id"])]=state["failed"]
    candidate={}
    for node in controller.genome.nodes.values():
        state=node.evidence_strand["candidate_state"]
        candidate[(node.entity_id,state["job_id"])]=state["failed"]
    rows=[]
    cohorts={x["id"]:0 for x in eligible}
    for rec in sorted_rows:
        timestamp=when(rec["started_at"])
        cohort=next((x for x in eligible
                     if when(x["start"])<=timestamp<when(x["end"])),None)
        if cohort is None:continue
        key=(rec["repo"],rec["job_id"])
        if key not in baseline or key not in candidate:
            raise ValueError("RECOVERY_MISSING_INCOMPLETE_LEDGER")
        cohorts[cohort["id"]]+=1
        rows.append({
            "task_id":f"{rec['repo']}#{rec['job_id']}",
            "origin_sha256":digest(rec),
            "event_at":rec["started_at"],
            "observed_at":rec["completed_at"],
            "actual_bad":rec["conclusion"]=="failure",
            "baseline_says_bad":baseline[key],
            "candidate_says_bad":candidate[key],
        })
    return rows,cohorts

def calculate(source,protocol):
    rows,cohorts=paired_rows(source,protocol)
    packet={
        "schema":PACKET,"candidate_id":ARM,
        "protocol_sha256":sha(protocol),
        "origin_repo":ORIGIN,
        "claimed_verifier_repo":"Hawkar-usls/Janus_Genesis",
        "claimed_production_authority":False,
        "rows":rows,"rows_sha256":sha(rows),
    }
    report=score(protocol,packet)
    if report["source_claims_not_authenticated"] is not True or report["production_authority"] is not False:
        raise ValueError("UNTRUSTED_SCIENTIFIC_AUTHORITY")
    receipt={
        "schema":EVIDENCE,"candidate_id":ARM,
        "source_class":"PUBLIC_GITHUB_REAL_COMPLETED_JOBS_AFTER_FREEZE",
        "scientific_task":"RECONSTRUCTION_AFTER_COMPLETION_NOT_PREDICTION",
        "source_payload_hashes_sha256":digest(source["response_hashes"]),
        "source_records_sha256":source["record_digest"],
        "preregistered_protocol_sha256":sha(protocol),
        "cohort_record_counts":cohorts,
        "trial_report_sha256":report["report_sha256"],
        "research_gate":report["research_gate"],
        "minimum_tasks_per_cohort":60,
        "source_not_independently_attested":True,
        "external_independent_verifier":False,
        "future_rows_not_yet_available_if_zero":sum(cohorts.values())==0,
        "result_is_not_model_learning":True,
        "working_model_modified":False,
        "production_authority":False,
    }
    receipt["receipt_sha256"]=sha(receipt)
    return packet,report,receipt

def write_new(path,data):
    path=Path(path)
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("x",encoding="utf-8") as f:
        json.dump(data,f,sort_keys=True,indent=2,ensure_ascii=False)
        f.write("\n")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",required=True)
    p.add_argument("--protocol",default="research/essence/v11/locked_v07_protocol.json")
    p.add_argument("--out",required=True)
    a=p.parse_args()
    source=json.loads(Path(a.input).read_text(encoding="utf-8"))
    protocol=load_protocol(a.protocol)
    packet,report,receipt=calculate(source,protocol)
    root=Path(a.out)
    if root.exists():raise FileExistsError("NO_PRIOR_RECEIPT_OVERWRITE")
    root.mkdir(parents=True)
    write_new(root/"raw_rows.json",packet)
    write_new(root/"paired_report.json",report)
    write_new(root/"research_receipt.json",receipt)
    print(json.dumps({k:receipt[k] for k in (
        "cohort_record_counts","research_gate","production_authority")}))
if __name__=="__main__":
    main()
