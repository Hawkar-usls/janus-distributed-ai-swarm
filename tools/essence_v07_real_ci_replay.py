#!/usr/bin/env python3
"""ESSENCE v0.7: compare real PUBLIC CI history reconstruction in Swarm.

The fair baseline is the *existing complete per-entity SpiralLedger*, not a
deliberately lossy cache. The candidate is the repo's SwarmGenomeLedger.
Reads dated public GitHub Actions job metadata; never logs, secrets, private
repos or workflow writes. Old jobs are retrospective DEVELOPMENT ONLY.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import urllib.request

from demiurge_spiral_swarm import SwarmSpiralController

REPOS = ("Janus_Genesis", "Janus-Demiurge", "janus-meta-registry")
OWNER = "Hawkar-usls"
SCHEMA = "janus.essence.v07.swarm_real_ci_audit/0.1"
LIMIT_ROWS = 320
MIN_ROWS = 60

def canonical(x):
    return json.dumps(x,sort_keys=True,separators=(",",":"),ensure_ascii=False,allow_nan=False).encode()

def digest(x):
    return hashlib.sha256(canonical(x)).hexdigest()

def when(s):
    t = datetime.fromisoformat(s.replace("Z","+00:00"))
    if t.tzinfo is None: raise ValueError("NAIVE_TIME")
    return t.astimezone(timezone.utc)

def api(path,token):
    if not (path.startswith("/repos/Hawkar-usls/Janus_Genesis/actions/") or
            path.startswith("/repos/Hawkar-usls/Janus-Demiurge/actions/") or
            path.startswith("/repos/Hawkar-usls/janus-meta-registry/actions/")):
        raise ValueError("UNTRUSTED_API_PATH")
    request=urllib.request.Request(
        "https://api.github.com"+path,headers={
            "Accept":"application/vnd.github+json","User-Agent":"ESSENCE-V07-SwarmAudit/0.1",
            **({"Authorization":"Bearer "+token} if token else {})})
    with urllib.request.urlopen(request,timeout=25) as response:
        raw=response.read(2_000_001)
    if len(raw)>2_000_000: raise ValueError("OVERSIZE_API_RESPONSE")
    return json.loads(raw),hashlib.sha256(raw).hexdigest()

def collect(token="", max_runs_per_repo=16):
    """Live read-only fetch; dates are supplied by GitHub, not invented locally."""
    if type(max_runs_per_repo) is not int or not 1<=max_runs_per_repo<=30:
        raise ValueError("RUN_LIMIT")
    rows, payload_hashes = [], []
    for name in REPOS:
        rpath=f"/repos/{OWNER}/{name}/actions/runs?per_page=40&status=completed"
        runs,sh=api(rpath,token);payload_hashes.append({"path":rpath,"sha256":sh})
        data=runs.get("workflow_runs")
        if not isinstance(data,list):raise ValueError("INVALID_RUNS")
        for run in data[:max_runs_per_repo]:
            if run.get("status")!="completed" or type(run.get("id")) is not int:
                continue
            jpath=f"/repos/{OWNER}/{name}/actions/runs/{run['id']}/jobs?per_page=100"
            batch,jsha=api(jpath,token);payload_hashes.append({"path":jpath,"sha256":jsha})
            if batch.get("total_count",0)>100:raise ValueError("TRUNCATED_JOB_PAGE")
            jobs=batch.get("jobs")
            if not isinstance(jobs,list):raise ValueError("INVALID_JOBS")
            for job in jobs:
                if (job.get("conclusion") not in ("success","failure") or
                    job.get("status")!="completed" or type(job.get("id")) is not int):
                    continue
                try:
                    begin,end=when(job["started_at"]),when(job["completed_at"])
                except (ValueError,KeyError,TypeError):
                    continue
                if end<=begin:continue
                rows.append({
                    "repo":f"{OWNER}/{name}","job_id":job["id"],
                    "run_id":run["id"],"started_at":begin.isoformat(),
                    "completed_at":end.isoformat(),
                    "conclusion":job["conclusion"],
                })
    ids=set(); unique=[]
    for item in sorted(rows,key=lambda x:(x["started_at"],x["repo"],x["job_id"])):
        key=(item["repo"],item["job_id"])
        if key in ids: continue
        ids.add(key);unique.append(item)
    unique=unique[-LIMIT_ROWS:]
    return {"schema":SCHEMA,"source":"LIVE_PUBLIC_GITHUB_ACTIONS_METADATA",
            "kind":"RETROSPECTIVE_DEVELOPMENT_ONLY","records":unique,
            "response_hashes":payload_hashes,"source_raw_logs_collected":False,
            "record_digest":digest(unique)}

def compare(dataset):
    if dataset.get("schema")!=SCHEMA:
        raise ValueError("INVALID_REAL_EVENT_SCHEMA")
    records=dataset.get("records")
    if not isinstance(records,list) or not 0<=len(records)<=LIMIT_ROWS:
        raise ValueError("INVALID_RECORD_COUNT")
    if dataset.get("record_digest")!=digest(records):
        raise ValueError("RECORD_TAMPERING")
    seen=set()
    for row in records:
        if set(row)!={"repo","job_id","run_id","started_at","completed_at","conclusion"}:
            raise ValueError("INVALID_RECORD_FIELDS")
        if row["repo"] not in tuple(f"{OWNER}/{x}" for x in REPOS):
            raise ValueError("UNAPPROVED_REPO")
        if type(row["job_id"]) is not int or type(row["run_id"]) is not int:
            raise ValueError("INVALID_ID")
        key=(row["repo"],row["job_id"])
        if key in seen:raise ValueError("DUPLICATE_EVENT")
        seen.add(key)
        if when(row["started_at"])>=when(row["completed_at"]):
            raise ValueError("INVALID_CHRONOLOGY")
        if row["conclusion"] not in ("failure","success"):
            raise ValueError("UNKNOWN_OUTCOME")
    controller=SwarmSpiralController(working_window=32)
    for row in sorted(records,key=lambda x:(x["started_at"],x["repo"],x["job_id"])):
        entity=row["repo"]
        failed=row["conclusion"]=="failure"
        controller.integrate(
            entity,{"job_id":row["job_id"],"failed":failed},
            lessons=["REAL_CI_JOB_FAILED"] if failed else [],
            promoted=not failed,
            outcome="REAL_CI_FAILURE" if failed else "REAL_CI_SUCCESS")
    controller.validate()
    baseline={}
    for entity, ledger in controller.ledgers.items():
        for turn in ledger.turns:
            baseline[(entity,turn.candidate_state["job_id"])]=turn.candidate_state["failed"]
    candidate={}
    for node in controller.genome.nodes.values():
        event=node.evidence_strand["candidate_state"]
        candidate[(node.entity_id,event["job_id"])]=event["failed"]
    truth={(r["repo"],r["job_id"]):(r["conclusion"]=="failure") for r in records}
    baseline_correct=sum(baseline.get(k)==v for k,v in truth.items())
    candidate_correct=sum(candidate.get(k)==v for k,v in truth.items())
    result={
        "schema":SCHEMA,"kind":"RETROSPECTIVE_DEVELOPMENT_ONLY",
        "sample_origin":"ACTUAL_DATED_PUBLIC_GITHUB_ACTIONS_JOBS",
        "source_hashes_sha256":digest(dataset["response_hashes"]),
        "records_sha256":digest(records),
        "records_total":len(records),
        "distinct_repos":sorted({r["repo"] for r in records}),
        "failed_jobs":sum(truth.values()),
        "baseline":"ORIGINAL_COMPLETE_PER_ENTITY_SPIRAL_LEDGER",
        "baseline_correct":baseline_correct,
        "candidate":"SWARM_DUAL_STRAND_GENOME_LEDGER",
        "candidate_correct":candidate_correct,
        "correctness_gain_absolute":round((candidate_correct-baseline_correct)/len(records),6) if records else 0,
        "research_gate":"HOLD_NO_BETTER_THAN_COMPLETE_BASELINE" if candidate_correct<=baseline_correct else "HOLD_RETROSPECTIVE_ONLY",
        "minimum_rows_for_informative_pilot":MIN_ROWS,
        "meets_minimum_rows":len(records)>=MIN_ROWS,
        "independent_prospective_verification":False,
        "new_neural_training":False,"production_authority":False,"offspring_activated":False,
    }
    result["report_sha256"]=digest(result)
    return result

def save_new(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("x",encoding="utf-8") as stream:
        json.dump(data,stream,ensure_ascii=False,sort_keys=True,indent=2)
        stream.write("\n")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("mode",choices=("collect","compare"))
    p.add_argument("--infile")
    p.add_argument("--out",required=True)
    p.add_argument("--max-runs",type=int,default=16)
    args=p.parse_args()
    if args.mode=="collect":
        value=collect(os.environ.get("GH_TOKEN",""),args.max_runs)
    else:
        if not args.infile: p.error("--infile is required for compare")
        raw=Path(args.infile)
        if raw.stat().st_size>2_000_000:raise ValueError("OVERSIZE_FILE")
        value=compare(json.loads(raw.read_text(encoding="utf-8")))
    save_new(args.out,value)
    print(json.dumps({"records":len(value.get("records",[])) or value.get("records_total"),
                      "research_gate":value.get("research_gate","COLLECTED_ONLY"),
                      "production_authority":False}))
if __name__=="__main__":
    main()
