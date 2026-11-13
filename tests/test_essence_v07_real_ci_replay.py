"""Deterministic synthetic contracts; real CI jobs collected only in workflow."""
import copy
import datetime as dt
import sys
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1] / "tools"))
from essence_v07_real_ci_replay import SCHEMA, compare, digest

def fixture(n=80):
    start=dt.datetime(2026,11,1,tzinfo=dt.timezone.utc)
    rows=[]
    for i in range(n):
        began=start+dt.timedelta(minutes=i*5)
        ended=began+dt.timedelta(seconds=29)
        rows.append({
            "repo":"Hawkar-usls/Janus_Genesis",
            "job_id":10000+i,"run_id":20000+(i//8),
            "started_at":began.isoformat(),
            "completed_at":ended.isoformat(),
            "conclusion":"failure" if i%7==0 else "success",
        })
    return {
        "schema":SCHEMA,
        "source":"UNIT_TEST_SYNTHETIC",
        "kind":"RETROSPECTIVE_DEVELOPMENT_ONLY",
        "records":rows,
        "response_hashes":[],
        "record_digest":digest(rows),
    }

class SwarmRealAuditContracts(unittest.TestCase):
    def test_correct_full_baseline_does_not_fabricate_gain(self):
        r=compare(fixture())
        self.assertEqual(r["records_total"],80)
        self.assertEqual(r["baseline_correct"],80)
        self.assertEqual(r["candidate_correct"],80)
        self.assertEqual(r["correctness_gain_absolute"],0)
        self.assertEqual(r["research_gate"],"HOLD_NO_BETTER_THAN_COMPLETE_BASELINE")

    def test_failed_real_style_jobs_preserved(self):
        r=compare(fixture(80))
        self.assertEqual(r["failed_jobs"],12)

    def test_no_new_training_or_autonomous_rights(self):
        r=compare(fixture())
        self.assertFalse(r["production_authority"])
        self.assertFalse(r["new_neural_training"])
        self.assertFalse(r["offspring_activated"])
        self.assertFalse(r["independent_prospective_verification"])

    def test_source_digest_must_match(self):
        d=fixture()
        d["records"][1]["conclusion"]="failure"
        with self.assertRaisesRegex(ValueError,"RECORD_TAMPERING"):
            compare(d)

    def test_duplicate_job_rejected(self):
        d=fixture()
        d["records"][1]["job_id"]=d["records"][0]["job_id"]
        d["record_digest"]=digest(d["records"])
        with self.assertRaisesRegex(ValueError,"DUPLICATE_EVENT"):
            compare(d)

    def test_start_must_precede_completion(self):
        d=fixture()
        d["records"][1]["completed_at"]=d["records"][1]["started_at"]
        d["record_digest"]=digest(d["records"])
        with self.assertRaisesRegex(ValueError,"INVALID_CHRONOLOGY"):
            compare(d)

    def test_private_repository_rejected(self):
        d=fixture()
        d["records"][0]["repo"]="Hawkar-usls/SkinGPT"
        d["record_digest"]=digest(d["records"])
        with self.assertRaisesRegex(ValueError,"UNAPPROVED_REPO"):
            compare(d)

    def test_invalid_outcome_rejected(self):
        d=fixture()
        d["records"][0]["conclusion"]="unknown"
        d["record_digest"]=digest(d["records"])
        with self.assertRaisesRegex(ValueError,"UNKNOWN_OUTCOME"):
            compare(d)

    def test_small_dataset_is_not_a_benefit_claim(self):
        r=compare(fixture(12))
        self.assertFalse(r["meets_minimum_rows"])
        self.assertEqual(r["research_gate"],"HOLD_NO_BETTER_THAN_COMPLETE_BASELINE")

    def test_no_cases_never_recommend_adoption(self):
        r=compare(fixture(0))
        self.assertEqual(r["records_total"],0)
        self.assertFalse(r["meets_minimum_rows"])
        self.assertFalse(r["production_authority"])

if __name__=="__main__":
    unittest.main()
