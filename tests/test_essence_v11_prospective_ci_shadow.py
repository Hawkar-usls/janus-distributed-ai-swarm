"""ESSENCE v1.1: no-future leakage and fair historical-memory paired checks."""
import copy
import datetime as dt
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from essence_v11_prospective_ci_shadow import calculate, paired_rows, LOCKED, EVIDENCE
from essence_v07_real_ci_replay import SCHEMA,digest
from essence_v07_trial_gate import sha

PROTOCOL=json.loads((ROOT/"research/essence/v11/locked_v07_protocol.json").read_text())
def fixture(n1=60,n2=60,history=4):
    records=[]
    for cohort,day,n in ((0,15,n1),(1,22,n2)):
        start=dt.datetime(2026,11,day,tzinfo=dt.timezone.utc)
        for i in range(n):
            begin=start+dt.timedelta(minutes=2*i)
            end=begin+dt.timedelta(seconds=12)
            records.append({
                "repo":"Hawkar-usls/Janus_Genesis",
                "job_id":cohort*100000+i+1,
                "run_id":cohort*100000+(i//5)+1,
                "started_at":begin.isoformat(),
                "completed_at":end.isoformat(),
                "conclusion":"failure" if i%7==0 else "success",
            })
    for i in range(history):
        t=dt.datetime(2026,11,13,tzinfo=dt.timezone.utc)+dt.timedelta(minutes=i*5)
        records.append({
            "repo":"Hawkar-usls/Janus-Demiurge",
            "job_id":80000+i,"run_id":90000+i,
            "started_at":t.isoformat(),
            "completed_at":(t+dt.timedelta(seconds=3)).isoformat(),
            "conclusion":"success",
        })
    return {
      "schema":SCHEMA,"source":"LIVE_PUBLIC_GITHUB_ACTIONS_METADATA",
      "kind":"RETROSPECTIVE_DEVELOPMENT_ONLY",
      "records":records,
      "record_digest":digest(records),
      "response_hashes":[{"path":"/repos/Hawkar-usls/Janus_Genesis/actions/runs",
                          "sha256":"c"*64}],
    }

def reseal(d):d["record_digest"]=digest(d["records"])

class ProspectiveShadowContracts(unittest.TestCase):
    def test_protocol_still_frozen(self):
        self.assertEqual(sha(PROTOCOL),LOCKED)

    def test_fake_future_is_not_real_external_proof(self):
        packet, report, r=calculate(fixture(),PROTOCOL)
        self.assertEqual(r["cohort_record_counts"],{"future-1":60,"future-2":60})
        self.assertEqual(len(packet["rows"]),120)
        self.assertEqual(report["research_gate"],"HOLD_NO_REPLICATED_GAIN")
        self.assertEqual(r["research_gate"],"HOLD_NO_REPLICATED_GAIN")
        self.assertEqual(r["scientific_task"],"RECONSTRUCTION_AFTER_COMPLETION_NOT_PREDICTION")
        self.assertTrue(r["source_not_independently_attested"])
        self.assertFalse(r["working_model_modified"])
        self.assertFalse(r["production_authority"])
        for c in report["cohorts"].values():
            self.assertEqual(c["correct_candidate"],c["correct_baseline"])

    def test_only_first_future_window_is_pending(self):
        p,report,r=calculate(fixture(n2=0),PROTOCOL)
        self.assertEqual(r["cohort_record_counts"]["future-1"],60)
        self.assertEqual(r["cohort_record_counts"]["future-2"],0)
        self.assertEqual(report["research_gate"],"PENDING_FUTURE_HOLDOUT")

    def test_not_before_freeze_accepted(self):
        p,report,r=calculate(fixture(n1=0,n2=0),PROTOCOL)
        self.assertEqual(len(p["rows"]),0)
        self.assertTrue(r["future_rows_not_yet_available_if_zero"])
        self.assertEqual(report["research_gate"],"PENDING_FUTURE_HOLDOUT")

    def test_late_observation_is_after_event(self):
        p,_,_=calculate(fixture(n1=2,n2=2),PROTOCOL)
        self.assertTrue(all(
            dt.datetime.fromisoformat(x["observed_at"])>=dt.datetime.fromisoformat(x["event_at"])
            for x in p["rows"]))

    def test_replay_of_job_fails(self):
        src=fixture(n1=2,n2=2)
        src["records"].append(dict(src["records"][0]));reseal(src)
        with self.assertRaisesRegex(ValueError,"JOB_ID_REPLAY"):
            calculate(src,PROTOCOL)

    def test_forged_response_provenance_fails(self):
        src=fixture(n1=2,n2=2)
        src["response_hashes"]=[]
        with self.assertRaisesRegex(ValueError,"NO_PUBLIC_API_PROVENANCE"):
            calculate(src,PROTOCOL)

    def test_wrong_job_origin_fails(self):
        src=fixture(n1=2,n2=2)
        src["records"][0]["repo"]="Hawkar-usls/SkinGPT";reseal(src)
        with self.assertRaisesRegex(ValueError,"INVALID_REAL_JOB"):
            calculate(src,PROTOCOL)

    def test_tampered_dataset_hash_fails(self):
        src=fixture(n1=2,n2=2)
        src["records"][0]["conclusion"]="success"
        with self.assertRaisesRegex(ValueError,"UNTRUSTED_JOB_SOURCE_DIGEST"):
            calculate(src,PROTOCOL)

    def test_mutating_precommit_must_fail(self):
        p=copy.deepcopy(PROTOCOL)
        p["thresholds"]["min_cases_each_cohort"]=1
        with self.assertRaises(ValueError):
            calculate(fixture(n1=2,n2=2),p)

    def test_raw_time_is_reported_no_future_proof(self):
        packet,report,r=calculate(fixture(n1=2,n2=2),PROTOCOL)
        self.assertEqual(r["schema"],EVIDENCE)
        self.assertEqual(r["receipt_sha256"],sha({
            k:v for k,v in r.items() if k!="receipt_sha256"}))
        self.assertFalse(report["independent_external_replication"])
        self.assertEqual(packet["schema"],"janus.essence.paired_trial_packet/0.7")

if __name__=="__main__":
    unittest.main()
