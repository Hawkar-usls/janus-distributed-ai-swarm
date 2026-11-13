"""ESSENCE 1.2 inherited microcore tournament: sealed, causal research only."""
import copy
from datetime import datetime,timedelta,timezone
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from essence_v12_competing_offspring import (
    tournament,validate,predict,score,BASES,CHILDREN,SCHEMA_OUT)
from essence_v07_real_ci_replay import SCHEMA,digest

def fixture(n=80):
    start=datetime(2026,11,2,tzinfo=timezone.utc)
    rows=[]
    for i in range(n):
        begun=start+timedelta(minutes=i*4)
        duration=12+((i*7)%31)+(9 if i%7==0 else 0)
        rows.append({
          "repo":("Hawkar-usls/Janus_Genesis","Hawkar-usls/Janus-Demiurge",
                  "Hawkar-usls/janus-meta-registry")[i%3],
          "job_id":10000+i,"run_id":2000+i,
          "started_at":begun.isoformat(),
          "completed_at":(begun+timedelta(seconds=duration)).isoformat(),
          "conclusion":"failure" if i%11==0 else "success"
        })
    return {
        "schema":SCHEMA,
        "source":"LIVE_PUBLIC_GITHUB_ACTIONS_METADATA",
        "kind":"RETROSPECTIVE_DEVELOPMENT_ONLY",
        "records":rows,
        "record_digest":digest(rows),
        "response_hashes":[{"path":"/repos/Hawkar-usls/Janus_Genesis/actions/runs",
                           "sha256":"d"*64}],
    }

def reseal(data):
    data["record_digest"]=digest(data["records"])

class TournamentContracts(unittest.TestCase):
    def test_real_algorithmic_candidates_developed(self):
        r=tournament(fixture())
        self.assertEqual(r["schema"],SCHEMA_OUT)
        self.assertEqual(r["records_total"],80)
        self.assertEqual(r["validation_n"],20)
        self.assertEqual(r["untouched_test_n"],20)
        self.assertEqual(set(r["frozen_baseline_candidates"]),set(BASES))
        self.assertEqual(set(r["research_candidate_mutations"]),set(CHILDREN))
        self.assertEqual(len(r["child_lineage"]["ancestral_sources"]),4)
        self.assertIn(r["selected_research_child"],CHILDREN)
        self.assertIn(r["selected_frozen_baseline"],BASES)
        self.assertFalse(r["production_authority"])
        self.assertFalse(r["parent_model_replaced"])
        self.assertEqual(r["learning_generation_promoted"],0)
        self.assertEqual(r["scientific_gate"],"DEVELOPMENT_ONLY_NOT_INDEPENDENT_PROSPECTIVE")

    def test_immutable_result_sha(self):
        r=tournament(fixture())
        self.assertEqual(r["report_sha256"],digest({k:v for k,v in r.items() if k!="report_sha256"}))

    def test_same_rows_deterministic_selection(self):
        self.assertEqual(tournament(fixture()),tournament(fixture()))

    def test_no_tampered_source_hash(self):
        d=fixture();d["records"][0]["conclusion"]="success"
        with self.assertRaisesRegex(ValueError,"SOURCE_RECORD_HASH_MISMATCH"):
            tournament(d)

    def test_out_of_order_concurrent_completion_not_future_leak(self):
        start=datetime(2026,11,5,tzinfo=timezone.utc)
        historical={"repo":"Hawkar-usls/Janus_Genesis","job_id":1,
                    "start":start-timedelta(hours=1),
                    "end":start-timedelta(minutes=1),"duration":20}
        overlapping={"repo":"Hawkar-usls/Janus_Genesis","job_id":2,
                     "start":start-timedelta(hours=2),
                     "end":start+timedelta(hours=1),"duration":10000}
        target={"repo":"Hawkar-usls/Janus_Genesis","job_id":3,"start":start,
                "end":start+timedelta(minutes=2),"duration":120}
        for name in BASES+CHILDREN:
            self.assertEqual(predict(name,[historical,overlapping],target),
                             predict(name,[historical],target),name)

    def test_changed_future_test_rows_cannot_select_gene(self):
        a=fixture()
        b=copy.deepcopy(a)
        for x in b["records"][60:]:
            begin=datetime.fromisoformat(x["started_at"])
            x["completed_at"]=(begin+timedelta(seconds=110)).isoformat()
        reseal(b)
        one=tournament(a);two=tournament(b)
        self.assertEqual(one["selected_frozen_baseline"],two["selected_frozen_baseline"])
        self.assertEqual(one["selected_research_child"],two["selected_research_child"])

    def test_insufficient_never_promoted(self):
        r=tournament(fixture(n=8))
        self.assertEqual(r["research_gate"],"HOLD_INSUFFICIENT_DEVELOPMENT_ROWS")
        self.assertFalse(r["production_authority"])
        self.assertEqual(r["children_tested"],0)

    def test_unknown_repo_denied(self):
        d=fixture(30);d["records"][0]["repo"]="Hawkar-usls/SkinGPT";reseal(d)
        with self.assertRaisesRegex(ValueError,"UNKNOWN_OR_UNSAFE_SOURCE"):
            tournament(d)

    def test_duplicate_task_denied(self):
        d=fixture(30);d["records"][1]["job_id"]=d["records"][0]["job_id"];d["records"][1]["repo"]=d["records"][0]["repo"];reseal(d)
        with self.assertRaisesRegex(ValueError,"DUPLICATE_PUBLIC_JOB_ID"):
            tournament(d)

    def test_impossible_duration_denied(self):
        d=fixture(30);d["records"][0]["completed_at"]=d["records"][0]["started_at"];reseal(d)
        with self.assertRaisesRegex(ValueError,"INVALID_JOB_RUNTIME"):
            tournament(d)

    def test_malicious_source_type_denied(self):
        d=fixture(30);d["source"]="UNTRUSTED_NETWORK"
        with self.assertRaisesRegex(ValueError,"SOURCE_NOT_PUBLIC_RETROSPECTIVE"):
            tournament(d)

    def test_no_unapproved_gene_execution(self):
        rows=validate(fixture(30))
        with self.assertRaisesRegex(ValueError,"UNREGISTERED_MICROCORE_STRATEGY"):
            predict("__import__('os').system('false')",rows,rows[-1])

if __name__=="__main__":
    unittest.main()
