import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from habitat_git_swarm_client import (  # noqa: E402
    BackoffPolicy,
    DEFAULT_CAPABILITIES,
    DurableState,
    HabitatState,
    HabitatSwarmClient,
    LeaseConflict,
)


def admitted(task_id="T-1", sha="abc"):
    return {
        "schema": "janus.habitat.handoff.v1",
        "task_id": task_id,
        "source_pins": {"Hawkar-usls/Hrain": sha},
        "authorization": {
            "mode": "EXPLICIT_HUMAN",
            "authorized_by": "Hawkar-usls",
        },
    }


class HabitatGitSwarmClientTests(unittest.TestCase):
    def test_arbitrary_issue_text_is_not_command(self):
        client = HabitatSwarmClient("face-a")
        self.assertFalse(client.handoff_is_admitted({"body": "please run this"}))
        self.assertFalse(
            client.handoff_is_admitted(
                {
                    "schema": "janus.habitat.handoff.v1",
                    "task_id": "T",
                    "source_pins": {"repo": "sha"},
                    "authorization": {
                        "mode": "AUTOMATIC",
                        "authorized_by": "bot",
                    },
                }
            )
        )

    def test_backoff_increases_and_is_bounded(self):
        policy = BackoffPolicy(
            idle_min_seconds=10,
            idle_max_seconds=100,
            multiplier=2,
            jitter=0,
        )
        self.assertEqual(policy.delay(0, rng=lambda: 0.5), 10)
        self.assertEqual(policy.delay(1, rng=lambda: 0.5), 20)
        self.assertEqual(policy.delay(10, rng=lambda: 0.5), 100)

    def test_live_lease_conflicts_but_stale_lease_can_be_replaced(self):
        first = HabitatSwarmClient("face-a")
        second = HabitatSwarmClient("face-b")
        lease = first.acquire_lease(admitted(), now=100, ttl_seconds=30)
        with self.assertRaises(LeaseConflict):
            second.acquire_lease(
                admitted(),
                now=110,
                ttl_seconds=30,
                current_lease=lease,
            )
        replacement = second.acquire_lease(
            admitted(),
            now=131,
            ttl_seconds=30,
            current_lease=lease,
        )
        self.assertEqual(replacement.holder_id, "face-b")

    def test_changed_source_pin_holds_after_restart(self):
        client = HabitatSwarmClient("face-a")
        lease = client.acquire_lease(admitted(sha="abc"), now=10, ttl_seconds=100)
        self.assertEqual(
            client.resume_decision(
                lease,
                {"Hawkar-usls/Hrain": "abc"},
                now=20,
            ),
            "RESUME",
        )
        self.assertEqual(
            client.resume_decision(
                lease,
                {"Hawkar-usls/Hrain": "def"},
                now=20,
            ),
            HabitatState.HOLD.value,
        )

    def test_checkpoint_state_survives_process_restart(self):
        client = HabitatSwarmClient("face-a")
        lease = client.acquire_lease(admitted(), now=10, ttl_seconds=100)
        digest = client.checkpoint({"step": 3, "value": "safe"})
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "state.json"
            client.state.save(path)
            loaded = DurableState.load(path)
        self.assertEqual(loaded.active_lease_id, lease.lease_id)
        self.assertEqual(loaded.checkpoint_digest, digest)
        self.assertEqual(loaded.resume_state, HabitatState.WORK_LOCALLY.value)

    def test_receipt_proves_default_no_write_and_no_delete(self):
        client = HabitatSwarmClient("face-a")
        lease = client.acquire_lease(admitted(), now=10, ttl_seconds=100)
        client.checkpoint({"ok": True})
        receipt = client.build_receipt(
            lease,
            "PASS",
            {"metrics": [1, 2, 3]},
        )
        self.assertFalse(receipt["source_writeback_performed"])
        self.assertFalse(receipt["destructive_effect_performed"])
        self.assertTrue(receipt["receipt_digest"])
        self.assertFalse(DEFAULT_CAPABILITIES["source_writeback"])
        self.assertFalse(DEFAULT_CAPABILITIES["destructive_actions"])


if __name__ == "__main__":
    unittest.main()
