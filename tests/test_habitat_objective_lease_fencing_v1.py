# CI replay marker: current-main lease-fencing compatibility witness for b0bb07418cb1c0e1bc2da8ae443977825c0b19d1. No executable semantics changed.
from __future__ import annotations

import multiprocessing
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from habitat_git_swarm_client import LeaseConflict  # noqa: E402
from habitat_objective_lease_fencing import (  # noqa: E402
    LeaseFenceLost,
    ObjectiveLeaseConfigurationError,
    ObjectiveLeaseFence,
    RenewableDurableLeaseStore,
)


def _race_worker(db_path: str, holder: str, gate, queue) -> None:
    try:
        fence = ObjectiveLeaseFence(db_path, holder_id=holder)
        gate.wait(timeout=10)
        lease = fence.acquire(
            "objective-race",
            authorized_by="Hawkar-usls",
            now=100.0,
            ttl_seconds=60.0,
        )
        queue.put(("ACQUIRED", holder, lease.lease_id))
    except LeaseConflict:
        queue.put(("CONFLICT", holder, None))
    except BaseException as exc:  # pragma: no cover - diagnostic channel
        queue.put(("ERROR", holder, f"{type(exc).__name__}:{exc}"))


class ObjectiveLeaseFencingTests(unittest.TestCase):
    def test_two_processes_cannot_both_acquire_same_objective(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "leases.sqlite3")
            RenewableDurableLeaseStore(db_path)
            ctx = multiprocessing.get_context("spawn")
            gate = ctx.Barrier(2)
            queue = ctx.Queue()
            workers = [
                ctx.Process(target=_race_worker, args=(db_path, holder, gate, queue))
                for holder in ("janus-a", "janus-b")
            ]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join(timeout=20)
                self.assertFalse(worker.is_alive(), "lease worker did not terminate")
                self.assertEqual(worker.exitcode, 0)
            results = [queue.get(timeout=5), queue.get(timeout=5)]
            self.assertEqual(sorted(row[0] for row in results), ["ACQUIRED", "CONFLICT"])

    def test_live_holder_can_renew_without_changing_fencing_token(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fence = ObjectiveLeaseFence(
                Path(temp_dir) / "leases.sqlite3", holder_id="janus-a"
            )
            lease = fence.acquire(
                "objective-renew",
                authorized_by="Hawkar-usls",
                now=10,
                ttl_seconds=20,
            )
            renewed = fence.renew(lease, now=20, ttl_seconds=40)
            self.assertEqual(renewed.lease_id, lease.lease_id)
            self.assertEqual(renewed.expires_at, 60.0)
            current = fence.assert_current(lease, now=30)
            self.assertEqual(current.lease_id, lease.lease_id)
            self.assertEqual(current.expires_at, 60.0)

    def test_stale_lease_cannot_resurrect_by_renewal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fence = ObjectiveLeaseFence(
                Path(temp_dir) / "leases.sqlite3", holder_id="janus-a"
            )
            lease = fence.acquire(
                "objective-stale",
                authorized_by="Hawkar-usls",
                now=10,
                ttl_seconds=5,
            )
            with self.assertRaisesRegex(LeaseFenceLost, "stale"):
                fence.renew(lease, now=15, ttl_seconds=30)

    def test_stale_takeover_invalidates_old_fencing_token(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db = Path(temp_dir) / "leases.sqlite3"
            first = ObjectiveLeaseFence(db, holder_id="janus-a")
            old = first.acquire(
                "objective-takeover",
                authorized_by="Hawkar-usls",
                now=100,
                ttl_seconds=10,
            )
            second = ObjectiveLeaseFence(db, holder_id="janus-b")
            replacement = second.acquire(
                "objective-takeover",
                authorized_by="Hawkar-usls",
                now=111,
                ttl_seconds=30,
            )
            self.assertNotEqual(old.lease_id, replacement.lease_id)
            with self.assertRaises(LeaseFenceLost):
                first.assert_current(old, now=112)
            with self.assertRaises(LeaseFenceLost):
                first.renew(old, now=112, ttl_seconds=30)
            self.assertFalse(first.release(old))
            self.assertEqual(second.assert_current(replacement, now=112), replacement)

    def test_explicit_human_authorizer_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fence = ObjectiveLeaseFence(
                Path(temp_dir) / "leases.sqlite3", holder_id="janus-a"
            )
            with self.assertRaises(PermissionError):
                fence.acquire(
                    "objective-auth",
                    authorized_by="not-trusted",
                    now=10,
                    ttl_seconds=20,
                )

    def test_binding_token_drift_fails_current_check(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db = Path(temp_dir) / "leases.sqlite3"
            first = ObjectiveLeaseFence(db, holder_id="janus-a")
            lease = first.acquire(
                "objective-binding",
                authorized_by="Hawkar-usls",
                now=10,
                ttl_seconds=10,
            )
            drifted = ObjectiveLeaseFence(
                db,
                holder_id="janus-b",
                boot_controller_head="0" * 40,
            )
            replacement = drifted.acquire(
                "objective-binding",
                authorized_by="Hawkar-usls",
                now=21,
                ttl_seconds=20,
            )
            self.assertNotEqual(replacement.source_pins, lease.source_pins)
            with self.assertRaises(LeaseFenceLost):
                first.assert_current(lease, now=22)
            self.assertEqual(drifted.assert_current(replacement, now=22), replacement)

    def test_snapshot_never_promotes_lease_into_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fence = ObjectiveLeaseFence(
                Path(temp_dir) / "leases.sqlite3", holder_id="janus-a"
            )
            fence.acquire(
                "objective-snapshot",
                authorized_by="Hawkar-usls",
                now=10,
                ttl_seconds=20,
            )
            snapshot = fence.snapshot("objective-snapshot", now=11)
            self.assertEqual(snapshot["schema"], "janus.habitat.objective_lease_fencing.v1")
            self.assertEqual(snapshot["lease_state"], "LIVE")
            self.assertFalse(snapshot["binding_tokens_are_git_membership_proof"])
            self.assertFalse(snapshot["lease_is_source_writeback_permission"])
            self.assertFalse(snapshot["lease_is_external_effect_permission"])
            self.assertFalse(snapshot["source_writeback_performed"])

    def test_invalid_sha_or_ttl_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaises(ObjectiveLeaseConfigurationError):
                ObjectiveLeaseFence(
                    Path(temp_dir) / "bad.sqlite3",
                    holder_id="janus-a",
                    boot_controller_head="ABC",
                )
            fence = ObjectiveLeaseFence(
                Path(temp_dir) / "leases.sqlite3", holder_id="janus-a"
            )
            with self.assertRaises(ObjectiveLeaseConfigurationError):
                fence.acquire(
                    "objective-bad-ttl",
                    authorized_by="Hawkar-usls",
                    now=10,
                    ttl_seconds=0,
                )


if __name__ == "__main__":
    unittest.main()
