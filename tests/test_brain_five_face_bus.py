"""Offline adversarial tests for the five-organ GitHub Actions brain Face bus."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from brain_five_face_bus import (
    FaceBusError, ack_for, face_id, load_cohort, make_packet, matrix,
    receive, route, validate_packet, witness, write_json,
)


class BrainFiveFaceBusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.faces = load_cohort()
        cls.fixture_content = {}
        for item in cls.faces:
            cls.fixture_content[item["repository"]] = (
                json.dumps({"schema": item["expected_schema"], "subject": "offline fixture"}).encode()
                if item["anchor_kind"] == "json"
                else b"# Offline test fixture: role-specific source document\n"
            )

    def fetcher(self, entry, sha):
        return self.fixture_content[entry["repository"]]

    def test_five_distinct_worker_identifiers_and_source_roles(self):
        nodes = matrix(self.faces)["include"]
        self.assertEqual(len(nodes), 5)
        self.assertEqual(len({x["face"] for x in nodes}), 5)
        self.assertEqual(len({e["role"] for e in self.faces}), 5)
        self.assertEqual(
            {e["repository"] for e in self.faces},
            {f"Hawkar-usls/{s}" for s in
             ("Demi_Head", "AIFC", "Janus", "janus-io-public", "janus-distributed-ai-swarm")}
        )
        self.assertEqual([x["peer"] for x in nodes],
                         [nodes[(i + 1) % 5]["face"] for i in range(5)])

    def test_readonly_contract_and_exact_sha_binding(self):
        for i, entry in enumerate(self.faces):
            pkt = make_packet(self.faces, entry["repository"], format(i + 1, "040x"), self.fetcher)
            self.assertEqual(validate_packet(self.faces, pkt, self.fetcher), pkt)
            self.assertFalse(pkt["model_synthesis_performed"])
            self.assertFalse(pkt["target_repository_write"])
            self.assertFalse(pkt["permission_granted"])
            self.assertFalse(pkt["effect_authority_granted"])
            self.assertEqual(pkt["authority_delta"], 0)
            self.assertEqual(pkt["claim_scope"], "SOURCE_DOCUMENT_INTEGRITY_ONLY")
        with self.assertRaises(FaceBusError):
            make_packet(self.faces, self.faces[0]["repository"], "not-a-sha", self.fetcher)

    def test_mutated_permissions_addresses_source_and_digest_are_rejected(self):
        original = make_packet(self.faces, self.faces[0]["repository"], "a" * 40, self.fetcher)
        for key, mutation in (
            ("permission_granted", True), ("truth_authority_granted", True),
            ("target_repository_write", True), ("authority_delta", 1),
            ("source_sha", "b" * 40), ("role", "SPOOFED_AUTHORITY"),
            ("anchor_sha256", "f" * 64), ("to_face", "FACE_INJECTED"),
            ("payload_sha256", "0" * 64), ("anchor_path", "README.md"),
        ):
            with self.subTest(key=key), self.assertRaises(FaceBusError):
                validate_packet(self.faces, {**original, key: mutation}, self.fetcher)

    def test_anchor_schema_mismatch_and_wrong_recipient_rejected(self):
        wrong = dict(self.fixture_content)
        target = self.faces[1]
        wrong[target["repository"]] = b'{"schema":"malicious/false"}'
        def bad_fetch(entry, sha):
            return wrong[entry["repository"]]
        with self.assertRaises(FaceBusError):
            make_packet(self.faces, target["repository"], "f" * 40, bad_fetch)
        pkt = make_packet(self.faces, self.faces[0]["repository"], "a" * 40, self.fetcher)
        with self.assertRaises(FaceBusError):
            ack_for(self.faces, pkt, self.faces[2]["repository"], self.fetcher)

    def test_five_live_job_equivalent_artifact_handoff_and_witness(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            inbox, mailboxes, acks = root / "inbox", root / "mailboxes", root / "acks"
            for i, entry in enumerate(self.faces):
                pkt = make_packet(self.faces, entry["repository"], format(i + 1, "040x"), self.fetcher)
                write_json(inbox / str(i) / (face_id(entry["repository"]) + ".json"), pkt)
            report = route(self.faces, inbox, mailboxes, self.fetcher)
            self.assertEqual(report["source_faces"], 5)
            self.assertEqual(report["mailboxes"], 5)
            for entry in self.faces:
                face = face_id(entry["repository"])
                ack = receive(self.faces, entry["repository"],
                              mailboxes / ("MAILBOX_" + face + ".json"), self.fetcher)
                write_json(acks / ("ack_" + face) / ("ACK_" + face + ".json"), ack)
            receipt = witness(self.faces, inbox, acks, self.fetcher)
            self.assertEqual(receipt["worker_count"], 5)
            self.assertEqual(receipt["delivery_ack_count"], 5)
            self.assertEqual(receipt["status"], "FIVE_FACES_DELIVERED_AND_ACKED")
            self.assertFalse(receipt["external_truth_confirmed"])
            self.assertFalse(receipt["models_executed"])
            self.assertFalse(receipt["deployment_authorized"])
            next(acks.rglob("ACK_FACE_*.json")).unlink()
            with self.assertRaises(FaceBusError):
                witness(self.faces, inbox, acks, self.fetcher)

    def test_duplicate_packet_does_not_pass_as_extra_worker(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            for i, entry in enumerate(self.faces):
                pkt = make_packet(self.faces, entry["repository"], format(i + 1, "040x"), self.fetcher)
                write_json(root / str(i) / (face_id(entry["repository"]) + ".json"), pkt)
            name = face_id(self.faces[0]["repository"]) + ".json"
            write_json(root / "duplicate" / name,
                       make_packet(self.faces, self.faces[0]["repository"], "a" * 40, self.fetcher))
            with self.assertRaises(FaceBusError):
                route(self.faces, root, root / "out", self.fetcher)


if __name__ == "__main__":
    unittest.main()
