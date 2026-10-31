"""Offline checks for Genesis-inspired 50-repository Face bus and handoff safety."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from portfolio_genesis_face_bus import (FaceBusError, ack_for, face_id, face_matrix,
    packet, read_manifest, receive, route, selected, validate_packet, verify, write_json)


class PortfolioFaceBusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = read_manifest()

    def test_manifest_has_fifty_distinct_source_faces_without_scheduling_fifty_vms(self):
        self.assertEqual(len(self.manifest["repositories"]), 50)
        all_nodes = face_matrix(self.manifest, "all")["include"]
        self.assertEqual(len(set(x["face"] for x in all_nodes)), 50)
        self.assertEqual(len(face_matrix(self.manifest)["include"]), 3)
        self.assertFalse(self.manifest["execution_policy"]["automatic_full_fleet"])
        self.assertFalse(self.manifest["execution_policy"]["model_calls_enabled"])
        self.assertFalse(self.manifest["execution_policy"]["write_to_target_repositories"])
        self.assertEqual({x["repository"] for x in selected(self.manifest, "canary")},
                         set(self.manifest["canary_repositories"]))
        self.assertIn("Hawkar-usls/Cardputer-Game-Station-Emulators",
                      {i["repository"] for i in all_nodes})

    def test_canary_ring_has_three_distinct_destinations(self):
        nodes = face_matrix(self.manifest)["include"]
        self.assertEqual([n["peer"] for n in nodes],
                         [nodes[1]["face"], nodes[2]["face"], nodes[0]["face"]])
        self.assertEqual({x["face"] for x in nodes}, {x["peer"] for x in nodes})

    def test_reject_authority_escalation_and_source_forgery(self):
        name = self.manifest["canary_repositories"][0]
        valid = packet(self.manifest, name, "a" * 40)
        self.assertEqual(validate_packet(self.manifest, valid, "canary"), valid)
        for key, altered in (
            ("permission_granted", True),
            ("authority_delta", 1),
            ("model_call_performed", True),
            ("target_repository_write", True),
            ("source_sha", "b" * 40),
            ("to_face", "FACE_UNRECOGNIZED"),
            ("payload_sha256", "f" * 64),
        ):
            with self.subTest(key=key), self.assertRaises(FaceBusError):
                validate_packet(self.manifest, {**valid, key: altered}, "canary")
        with self.assertRaises(FaceBusError):
            packet(self.manifest, name, "not-a-git-sha")

    def test_real_interjob_protocol_synthetic_replay_and_all_acks(self):
        entries = selected(self.manifest, "canary")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            inbox, mailboxes, ackdir = root / "inbox", root / "mailboxes", root / "acks"
            for i, entry in enumerate(entries):
                msg = packet(self.manifest, entry["repository"], format(i + 1, "040x"))
                write_json(inbox / f"artifact-{i}" / (face_id(entry["repository"]) + ".json"), msg)
            result = route(self.manifest, inbox, mailboxes, "canary")
            self.assertEqual(result["status"], "ROUTED_NO_EXECUTION")
            for entry in entries:
                target = face_id(entry["repository"])
                ack = receive(self.manifest, entry["repository"],
                              mailboxes / ("MAILBOX_" + target + ".json"), "canary")
                write_json(ackdir / f"ack-artifact-{target}" / ("ACK_" + target + ".json"), ack)
                self.assertFalse(ack["effect_authority_granted"])
            verified = verify(self.manifest, inbox, ackdir, "canary")
            self.assertEqual(verified["source_faces"], 3)
            self.assertEqual(verified["delivery_acks"], 3)
            self.assertFalse(verified["model_synthesis_proven"])
            self.assertFalse(verified["external_source_truth_independently_proven"])
            self.assertFalse(verified["target_write_permission_granted"])
            next(iter(ackdir.rglob("ACK_*.json"))).unlink()
            with self.assertRaises(FaceBusError):
                verify(self.manifest, inbox, ackdir, "canary")

    def test_missing_and_duplicate_messages_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)
            name = self.manifest["canary_repositories"][0]
            msg = packet(self.manifest, name, "a" * 40)
            filename = face_id(name) + ".json"
            write_json(path / filename, msg)
            with self.assertRaises(FaceBusError):
                route(self.manifest, path, path / "out", "canary")
            write_json(path / "other" / filename, msg)
            with self.assertRaises(FaceBusError):
                route(self.manifest, path, path / "out", "canary")

    def test_message_is_not_command_even_with_many_faces(self):
        m = self.manifest
        msg = packet(m, m["canary_repositories"][0], "c" * 40)
        self.assertEqual(msg["kind"], "OBSERVATION")
        self.assertEqual(msg["authority_delta"], 0)
        self.assertFalse(msg["truth_authority_granted"])
        self.assertFalse(msg["effect_authority_granted"])
        self.assertFalse(msg["permission_granted"])
        self.assertEqual(len(face_matrix(m, "all")["include"]), 50)


if __name__ == "__main__":
    unittest.main()
