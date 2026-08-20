from __future__ import annotations

import multiprocessing
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from habitat_git_swarm_client import (  # noqa: E402
    DurableState,
    HabitatState,
    HabitatSwarmClient,
    Lease,
)
from habitat_git_swarm_recovery import (  # noqa: E402
    DurableLeaseStore,
    LeaseConflict,
    LeaseStoreError,
)


def admitted(task_id: str = "P0-LEASE", sha: str = "abc") -> dict:
    return {
        "schema": "janus.habitat.handoff.v1",
        "task_id": task_id,
        "source_pins": {"Hawkar-usls/Hrain": sha},
        "authorization": {
            "mode": "EXPLICIT_HUMAN",
            "authorized_by": "Hawkar-usls",
        },
    }


def _competing_acquire_worker(
    db_path: str,
    holder_id: str,
    start_gate,
    result_queue,
) -> None:
    try:
        store = DurableLeaseStore(db_path, busy_timeout_seconds=10)
        client = HabitatSwarmClient(holder_id)
        start_gate.wait(timeout=10)
        lease = store.acquire(client, admitted(), now=100.0, ttl_seconds=60.0)
        result_queue.put(("ACQUIRED", holder_id, lease.lease_id))
    except LeaseConflict:
        result_queue.put(("CONFLICT", holder_id, None))
    except BaseException as exc:  # pragma: no cover - diagnostic channel
        result_queue.put(("ERROR", holder_id, f"{type(exc).__name__}:{exc}"))


class HabitatGitSwarmRecoveryGauntletTests(unittest.TestCase):
    def test_two_processes_cannot_both_acquire_same_live_task(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "leases.sqlite3")
            DurableLeaseStore(db_path)  # freeze schema before process race

            ctx = multiprocessing.get_context("spawn")
            gate = ctx.Barrier(2)
            queue = ctx.Queue()
            processes = [
                ctx.Process(
                    target=_competing_acquire_worker,
                    args=(db_path, holder, gate, queue),
                )
                for holder in ("face-a", "face-b")
            ]
            for process in processes:
                process.start()
            for process in processes:
                process.join(timeout=20)
                self.assertFalse(process.is_alive(), "worker did not terminate")
                self.assertEqual(process.exitcode, 0)

            results = [queue.get(timeout=5), queue.get(timeout=5)]
            terminals = sorted(row[0] for row in results)
            self.assertEqual(terminals, ["ACQUIRED", "CONFLICT"], results)

            persisted = DurableLeaseStore(db_path).get("P0-LEASE")
            self.assertIsNotNone(persisted)
            assert persisted is not None
            acquired_holder = next(row[1] for row in results if row[0] == "ACQUIRED")
            self.assertEqual(persisted.holder_id, acquired_holder)

    def test_session_drop_recovers_same_live_lease_and_checkpoint(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            store = DurableLeaseStore(root / "leases.sqlite3")
            first = HabitatSwarmClient("face-a")
            lease = store.acquire(first, admitted(sha="source-a"), now=10, ttl_seconds=100)
            checkpoint_digest = first.checkpoint({"step": 7, "safe": True})
            state_path = root / "worker-state.json"
            first.state.save(state_path)

            # Simulate loss of the in-memory Chat/session/process state. Recovery
            # gets only the durable worker checkpoint and the shared lease DB.
            recovered = HabitatSwarmClient("face-a")
            recovered.state = DurableState.load(state_path)
            recovered_lease = DurableLeaseStore(root / "leases.sqlite3").get(lease.task_id)

            self.assertIsNotNone(recovered_lease)
            assert recovered_lease is not None
            self.assertEqual(recovered_lease.lease_id, lease.lease_id)
            self.assertEqual(recovered.state.checkpoint_digest, checkpoint_digest)
            self.assertEqual(recovered.state.active_lease_id, lease.lease_id)
            self.assertEqual(
                recovered.resume_decision(
                    recovered_lease,
                    {"Hawkar-usls/Hrain": "source-a"},
                    now=20,
                ),
                "RESUME",
            )
            self.assertEqual(
                recovered.resume_decision(
                    recovered_lease,
                    {"Hawkar-usls/Hrain": "changed"},
                    now=20,
                ),
                HabitatState.HOLD.value,
            )

    def test_stale_crashed_holder_can_be_replaced_but_old_release_cannot_delete_new_lease(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            store = DurableLeaseStore(Path(temp_dir) / "leases.sqlite3")
            first = HabitatSwarmClient("face-a")
            old = store.acquire(first, admitted(), now=100, ttl_seconds=10)

            second = HabitatSwarmClient("face-b")
            replacement = store.acquire(second, admitted(), now=111, ttl_seconds=30)
            self.assertNotEqual(replacement.lease_id, old.lease_id)
            self.assertEqual(replacement.holder_id, "face-b")

            self.assertFalse(store.release(old))
            persisted = store.get(old.task_id)
            self.assertIsNotNone(persisted)
            assert persisted is not None
            self.assertEqual(persisted.lease_id, replacement.lease_id)
            self.assertTrue(store.release(replacement))
            self.assertIsNone(store.get(old.task_id))

    def test_forged_holder_cannot_release_real_lease(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            store = DurableLeaseStore(Path(temp_dir) / "leases.sqlite3")
            real = store.acquire(
                HabitatSwarmClient("face-a"),
                admitted(),
                now=10,
                ttl_seconds=100,
            )
            forged = Lease(
                lease_id=real.lease_id,
                task_id=real.task_id,
                holder_id="face-b",
                source_pins=real.source_pins,
                expires_at=real.expires_at,
            )
            self.assertFalse(store.release(forged))
            self.assertEqual(store.get(real.task_id), real)

    def test_corrupt_persisted_pins_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = Path(temp_dir) / "leases.sqlite3"
            store = DurableLeaseStore(db_path)
            lease = store.acquire(
                HabitatSwarmClient("face-a"),
                admitted(),
                now=10,
                ttl_seconds=100,
            )
            with sqlite3.connect(str(db_path)) as connection:
                connection.execute(
                    "UPDATE task_leases SET source_pins_json = ? WHERE task_id = ?",
                    ("{not-json", lease.task_id),
                )
            with self.assertRaises(LeaseStoreError):
                store.get(lease.task_id)

    def test_recovery_snapshot_never_claims_source_effect(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            store = DurableLeaseStore(Path(temp_dir) / "leases.sqlite3")
            store.acquire(
                HabitatSwarmClient("face-a"),
                admitted(),
                now=10,
                ttl_seconds=5,
            )
            live = store.recovery_snapshot("P0-LEASE", now=12)
            stale = store.recovery_snapshot("P0-LEASE", now=16)
            self.assertEqual(live["lease_state"], "LIVE")
            self.assertEqual(stale["lease_state"], "STALE")
            for snapshot in (live, stale):
                self.assertFalse(snapshot["source_writeback_performed"])
                self.assertFalse(snapshot["destructive_source_effect_performed"])


if __name__ == "__main__":
    unittest.main()
