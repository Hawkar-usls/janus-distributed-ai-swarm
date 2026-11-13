#!/usr/bin/env python3
"""ESSENCE v1.2 cross-ancestry research tournament on actual CI durations.

True source data: previously closed public GitHub Actions job times (from v0.7
read-only collector). No claim of experimental independence or future sample.
Generate several bounded pure predictors inspired by earlier JANUS variants:
Spiral ordered memory, Slime EMA trace, Trinity three-view council, and
cross-repository shrinkage. Select on chrono validation only, assess the
same untouched final chronology against strongest frozen baseline. Never
change working models or infer performance on unseen devices from this data.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import json
import math
from pathlib import Path
import statistics
from essence_v07_real_ci_replay import SCHEMA, digest, when, LIMIT_ROWS

SCHEMA_OUT="janus.essence.cross_ancestry_research_tournament/1.2"
BASES=("repo_median","global_median","repo_recent5")
CHILDREN=("spiral_recent3","slime_ema20","slime_ema50",
          "trinity_3view","crossrepo_shrinkage")
PARENT_BLOBS={
    "SpiralLedger":"f4bcb0817dd91057a73e1a873eb48470058786de",
    "SwarmGenomeLedger":"b4c116cd1d6500d16c5052b8cb733113935c61ee",
    "SlimeQuantization":"73bfc26f4384c0ff199f06c3ca6e857c5cdbc8e1",
    "TrinityCouncil":"b130377b76c8316c17757f0b50a1d9f5743c90a1",
}
APPROVED_REPOS=frozenset(("Hawkar-usls/Janus_Genesis",
                          "Hawkar-usls/Janus-Demiurge",
                          "Hawkar-usls/janus-meta-registry"))
MAX_SECONDS=4*3600
MIN_DEV=20
FUTURE_GATE=60

def validate(source):
    if (not isinstance(source,dict) or source.get("schema")!=SCHEMA
        or source.get("source")!="LIVE_PUBLIC_GITHUB_ACTIONS_METADATA"
        or source.get("kind")!="RETROSPECTIVE_DEVELOPMENT_ONLY"):
        raise ValueError("SOURCE_NOT_PUBLIC_RETROSPECTIVE")
    rows=source.get("records")
    if (not isinstance(rows,list) or len(rows)>LIMIT_ROWS
        or source.get("record_digest")!=digest(rows)):
        raise ValueError("SOURCE_RECORD_HASH_MISMATCH")
    pins=source.get("response_hashes")
    if (not isinstance(pins,list) or not pins
        or any(not isinstance(x,dict) or not isinstance(x.get("sha256"),str)
               or len(x["sha256"])!=64 for x in pins)):
        raise ValueError("SOURCE_API_FINGERPRINTS_REQUIRED")
    seen=set()
    result=[]
    for r in rows:
        if not isinstance(r,dict) or set(r)!={
            "repo","job_id","run_id","started_at","completed_at","conclusion"}:
            raise ValueError("UNTRUSTED_JOB_DATA")
        if (r["repo"] not in APPROVED_REPOS or type(r["job_id"]) is not int
            or type(r["run_id"]) is not int
            or r["conclusion"] not in ("success","failure")):
            raise ValueError("UNKNOWN_OR_UNSAFE_SOURCE")
        key=(r["repo"],r["job_id"])
        if key in seen:raise ValueError("DUPLICATE_PUBLIC_JOB_ID")
        seen.add(key)
        start,end=when(r["started_at"]),when(r["completed_at"])
        duration=(end-start).total_seconds()
        if not 1<=duration<=MAX_SECONDS:
            raise ValueError("INVALID_JOB_RUNTIME")
        result.append({
            "repo":r["repo"],"job_id":r["job_id"],"start":start,
            "end":end,"duration":float(duration)
        })
    return sorted(result,key=lambda x:(x["start"],x["repo"],x["job_id"]))

def ema(values,alpha):
    acc=float(values[0])
    for x in values[1:]:
        acc=alpha*float(x)+(1-alpha)*acc
    return acc

def predict(strategy,history,current):
    """ONLY jobs finished BEFORE current job starts may contribute."""
    if strategy not in BASES+CHILDREN:
        raise ValueError("UNREGISTERED_MICROCORE_STRATEGY")
    prior=sorted((x for x in history if x["end"]<current["start"]),
                 key=lambda x:(x["end"],x["repo"],x["job_id"]))
    global_values=[x["duration"] for x in prior]
    repo_values=[x["duration"] for x in prior if x["repo"]==current["repo"]]
    global_mid=statistics.median(global_values) if global_values else 60.0
    repo_mid=statistics.median(repo_values) if len(repo_values)>=2 else global_mid
    recent5=statistics.median(repo_values[-5:]) if repo_values else global_mid
    if strategy=="repo_median":return repo_mid
    if strategy=="global_median":return global_mid
    if strategy=="repo_recent5":return recent5
    if strategy=="spiral_recent3":
        return statistics.median(repo_values[-3:]) if repo_values else global_mid
    if strategy in ("slime_ema20","slime_ema50"):
        vals=repo_values or global_values or [60.0]
        return ema(vals[-32:],0.20 if strategy=="slime_ema20" else 0.50)
    if strategy=="trinity_3view":
        vals=repo_values or global_values or [60.0]
        views=(global_mid,repo_mid,ema(vals[-32:],0.25))
        return statistics.median(views)
    if strategy=="crossrepo_shrinkage":
        n=len(repo_values)
        return (n*repo_mid+5*global_mid)/(n+5)
    raise ValueError("UNREACHABLE")

def score(rows,strategy,observations):
    errors=[]
    for current in observations:
        estimate=predict(strategy,rows,current)
        if not math.isfinite(estimate) or not 1<=estimate<=MAX_SECONDS:
            raise ValueError("UNBOUNDED_MICROCORE_PREDICTION")
        errors.append(abs(estimate-current["duration"]))
    return round(sum(errors)/len(errors),6) if errors else None

def tournament(source):
    rows=validate(source)
    n=len(rows)
    if n<MIN_DEV:
        return {
            "schema":SCHEMA_OUT,"source_records":n,
            "research_gate":"HOLD_INSUFFICIENT_DEVELOPMENT_ROWS",
            "children_tested":0,"source_sha256":source["record_digest"],
            "production_authority":False,"weights_modified":False,
            "independently_verified":False
        }
    # The tune/validation partition is earlier than final test. Report full
    # winners and LOSERS. All methods still use past completed jobs only.
    validation_start=n//2
    test_start=max(validation_start+1,(n*3)//4)
    validation=rows[validation_start:test_start]
    test=rows[test_start:]
    validation_base={name:score(rows,name,validation) for name in BASES}
    validation_children={name:score(rows,name,validation) for name in CHILDREN}
    best_baseline=min(BASES,key=lambda x:(validation_base[x],x))
    best_child=min(CHILDREN,key=lambda x:(validation_children[x],x))
    test_base=score(rows,best_baseline,test)
    test_child=score(rows,best_child,test)
    delta=(test_base-test_child)/test_base if test_base>0 else 0.0
    lineage={
        "ancestral_sources":PARENT_BLOBS,
        "parents":[best_baseline,best_child],
        "proposed_role":"planning",
        "source_record_sha256":source["record_digest"],
        "candidate_id":"child-"+digest({
            "parent_blobs":PARENT_BLOBS,
            "strategy":best_child,
            "source_record_sha256":source["record_digest"]})[:16],
        "authority":"INERT_STRATEGY_SPEC_ONLY",
    }
    result={
        "schema":SCHEMA_OUT,
        "source_class":"REAL_DATED_PUBLIC_JOBS_RETROSPECTIVE_DEVELOPMENT",
        "records_total":n,
        "source_records_sha256":source["record_digest"],
        "api_sources_sha256":digest(source["response_hashes"]),
        "validation_n":len(validation),"untouched_test_n":len(test),
        "frozen_baseline_candidates":validation_base,
        "research_candidate_mutations":validation_children,
        "selected_frozen_baseline":best_baseline,
        "selected_research_child":best_child,
        "test_mae_s_baseline":test_base,
        "test_mae_s_candidate":test_child,
        "test_relative_gain":round(delta,6),
        "child_lineage":lineage,
        "winner_selection_only_uses_earlier_validation":True,
        "prediction_uses_only_pre_start_completion":True,
        "scientific_gate":"DEVELOPMENT_ONLY_NOT_INDEPENDENT_PROSPECTIVE",
        "note":"Same public dataset for local validation/test; not independent laboratory replication",
        "independent_evaluation_count":0,
        "learning_generation_promoted":0,
        "candidate_model_weights_modified":False,
        "parent_model_replaced":False,
        "production_authority":False,
    }
    result["report_sha256"]=digest(result)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    source=json.loads(Path(a.source).read_text(encoding="utf-8"))
    result=tournament(source)
    output=Path(a.out)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("x",encoding="utf-8") as fp:
        json.dump(result,fp,sort_keys=True,indent=2)
        fp.write("\n")
    print(json.dumps({k:result.get(k) for k in
          ("records_total","selected_frozen_baseline","selected_research_child",
           "test_relative_gain","scientific_gate","research_gate","production_authority")}))
if __name__=="__main__":
    main()
