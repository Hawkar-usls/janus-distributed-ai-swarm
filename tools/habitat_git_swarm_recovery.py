"""Durable local recovery primitives for the JANUS Git Habitat swarm client.

This module extends the transport-neutral client with a SQLite-backed lease
arbitration point. It does not contain GitHub transport or source mutation.
SQLite `BEGIN IMMEDIATE` is used as the local compare-and-swap boundary so two
independent workers cannot both acquire the same live task lease after each has
observed an apparently empty local state.
"""
from __future__ import annotations

import json
from pathlib import Path
import sqlite3
from typing import Any, Mapping

from habitat_git_swarm_client import HabitatSwarmClient, Lease, LeaseConflict


class LeaseStoreError(RuntimeError):
    """Raised when persisted lease state is malformed or inconsistent."""


class DurableLeaseStore:
    """SQLite-backed task lease store with atomic acquire/release semantics.

    The database stores coordination metadata only. It grants no source
    writeback, command, repository mutation, or external-effect authority.
    """

    SCHEMA_VERSION = 1

    def __init__(self, path: str | Path, busy_timeout_seconds: float = 5.0) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.busy_timeout_ms = max(1, int(float(busy_timeout_seconds) * 1000))
        self._initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(
            str(self.path),
            timeout=self.busy_timeout_ms / 1000.0,
            isolation_level=None,
        )
        connection.execute(f"PRAGMA busy_timeout={self.busy_timeout_ms}")
        connection.row_factory = sqlite3.Row
        return connection

    def _initialize(self) -> None:
        with self._connect() as connection:
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS habitat_meta (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                )
                """
            )
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS task_leases (
                    task_id TEXT PRIMARY KEY,
                    lease_id TEXT NOT NULL UNIQUE,
                    holder_id TEXT NOT NULL,
                    source_pins_json TEXT NOT NULL,
                    expires_at REAL NOT NULL
                )
                """
            )
            row = connection.execute(
                "SELECT value FROM habitat_meta WHERE key='schema_version'"
            ).fetchone()
            if row is None:
                connection.execute(
                    "INSERT INTO habitat_meta(key, value) VALUES('schema_version', ?)",
                    (str(self.SCHEMA_VERSION),),
                )
            elif row["value"] != str(self.SCHEMA_VERSION):
                raise LeaseStoreError("unsupported durable lease schema version")

    @staticmethod
    def _decode_row(row: sqlite3.Row | None) -> Lease | None:
        if row is None:
            return None
        try:
            source_pins = json.loads(row["source_pins_json"])
        except json.JSONDecodeError as exc:
            raise LeaseStoreError("persisted source pins are invalid JSON") from exc
        if not isinstance(source_pins, dict) or not source_pins:
            raise LeaseStoreError("persisted source pins are invalid")
        return Lease(
            lease_id=str(row["lease_id"]),
            task_id=str(row["task_id"]),
            holder_id=str(row["holder_id"]),
            source_pins={str(key): str(value) for key, value in source_pins.items()},
            expires_at=float(row["expires_at"]),
        )

    def get(self, task_id: str) -> Lease | None:
        with self._connect() as connection:
            row = connection.execute(
                """
                SELECT task_id, lease_id, holder_id, source_pins_json, expires_at
                FROM task_leases
                WHERE task_id = ?
                """,
                (str(task_id),),
            ).fetchone()
        return self._decode_row(row)

    def acquire(
        self,
        client: HabitatSwarmClient,
        envelope: Mapping[str, Any],
        now: float,
        ttl_seconds: float,
    ) -> Lease:
        """Atomically acquire or stale-replace one task lease.

        The transaction is the arbitration boundary. `client.acquire_lease`
        remains the authority-neutral semantic validator and lease constructor.
        """
        task_id = envelope.get("task_id")
        if not isinstance(task_id, str) or not task_id:
            raise PermissionError("handoff task id is not valid")

        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                """
                SELECT task_id, lease_id, holder_id, source_pins_json, expires_at
                FROM task_leases
                WHERE task_id = ?
                """,
                (task_id,),
            ).fetchone()
            current = self._decode_row(row)
            lease = client.acquire_lease(
                envelope,
                now=now,
                ttl_seconds=ttl_seconds,
                current_lease=current,
            )
            encoded_pins = json.dumps(
                lease.source_pins,
                sort_keys=True,
                separators=(",", ":"),
                ensure_ascii=False,
            )
            if current is None:
                connection.execute(
                    """
                    INSERT INTO task_leases(
                        task_id, lease_id, holder_id, source_pins_json, expires_at
                    ) VALUES (?, ?, ?, ?, ?)
                    """,
                    (
                        lease.task_id,
                        lease.lease_id,
                        lease.holder_id,
                        encoded_pins,
                        lease.expires_at,
                    ),
                )
            else:
                cursor = connection.execute(
                    """
                    UPDATE task_leases
                    SET lease_id = ?, holder_id = ?, source_pins_json = ?, expires_at = ?
                    WHERE task_id = ? AND lease_id = ?
                    """,
                    (
                        lease.lease_id,
                        lease.holder_id,
                        encoded_pins,
                        lease.expires_at,
                        current.task_id,
                        current.lease_id,
                    ),
                )
                if cursor.rowcount != 1:
                    raise LeaseStoreError("lease compare-and-swap lost")
            connection.execute("COMMIT")
            return lease
        except BaseException:
            try:
                connection.execute("ROLLBACK")
            except sqlite3.Error:
                pass
            raise
        finally:
            connection.close()

    def release(self, lease: Lease) -> bool:
        """Release only the exact holder/lease tuple.

        This deletes coordination metadata only; it is not source cleanup.
        """
        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            cursor = connection.execute(
                """
                DELETE FROM task_leases
                WHERE task_id = ? AND lease_id = ? AND holder_id = ?
                """,
                (lease.task_id, lease.lease_id, lease.holder_id),
            )
            changed = cursor.rowcount == 1
            connection.execute("COMMIT")
            return changed
        except BaseException:
            try:
                connection.execute("ROLLBACK")
            except sqlite3.Error:
                pass
            raise
        finally:
            connection.close()

    def recovery_snapshot(self, task_id: str, now: float) -> dict[str, Any]:
        """Return bounded coordination state for restart/reconciliation."""
        lease = self.get(task_id)
        if lease is None:
            return {
                "schema": "janus.habitat.swarm_recovery_snapshot.v1",
                "task_id": str(task_id),
                "lease_state": "ABSENT",
                "source_writeback_performed": False,
                "destructive_source_effect_performed": False,
            }
        return {
            "schema": "janus.habitat.swarm_recovery_snapshot.v1",
            "task_id": lease.task_id,
            "lease_id": lease.lease_id,
            "holder_id": lease.holder_id,
            "source_pins": lease.source_pins,
            "lease_state": "STALE" if lease.is_stale(now) else "LIVE",
            "expires_at": lease.expires_at,
            "source_writeback_performed": False,
            "destructive_source_effect_performed": False,
        }


__all__ = ["DurableLeaseStore", "LeaseStoreError", "LeaseConflict"]
