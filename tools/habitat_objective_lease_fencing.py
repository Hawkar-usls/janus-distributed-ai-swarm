#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Renewable fenced objective leases for JANUS Habitat local controllers.

This additive layer sits on the green recovery primitive from PR #5. It does
not modify that historical head.

The durable lease ID is used as a fencing token:
- a live exact holder may renew before expiry;
- an expired lease may not resurrect itself;
- stale takeover creates a different lease ID;
- an old holder must fail ``assert_current`` after takeover;
- exact release cannot delete a replacement lease.

Controller/supervisor SHA strings are lease binding tokens here. Equality is
checked, but this module does not claim to prove Git membership or provenance.
"""
from __future__ import annotations

import json
import math
import re
import sqlite3
from pathlib import Path
from typing import Any, Mapping

from habitat_git_swarm_client import HabitatSwarmClient, Lease, LeaseConflict
from habitat_git_swarm_recovery import DurableLeaseStore, LeaseStoreError


SCHEMA = "janus.habitat.objective_lease_fencing.v1"
PINNED_BOOT_CONTROLLER_HEAD = "9622241625eb6e6ee56f0fe955bdcf5a2a7bc607"
PINNED_DEMIURGE_SUPERVISOR_HEAD = "74c8a9dc090dba4d3bd7d497e1ff75223e6fe6c0"
DEFAULT_AUTHORIZERS = frozenset({"Hawkar-usls"})
MAX_OBJECTIVE_ID_CHARS = 128
MAX_HOLDER_ID_CHARS = 128
_SAFE_ID = re.compile(r"^[A-Za-z0-9_.:-]+$")


class LeaseFenceLost(RuntimeError):
    """Raised when the caller no longer owns the exact current live lease."""


class ObjectiveLeaseConfigurationError(ValueError):
    pass


def _safe_id(value: Any, label: str, maximum: int) -> str:
    if (
        not isinstance(value, str)
        or not value
        or len(value) > maximum
        or _SAFE_ID.fullmatch(value) is None
    ):
        raise ObjectiveLeaseConfigurationError(
            f"{label} must match {_SAFE_ID.pattern} and be <= {maximum} chars"
        )
    return value


def _sha1(value: Any, label: str) -> str:
    if (
        not isinstance(value, str)
        or len(value) != 40
        or any(char not in "0123456789abcdef" for char in value)
    ):
        raise ObjectiveLeaseConfigurationError(
            f"{label} must be 40 lowercase hexadecimal characters"
        )
    return value


def _ttl(value: Any) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ObjectiveLeaseConfigurationError("ttl_seconds must be finite and >= 1")
    result = float(value)
    if not math.isfinite(result) or result < 1.0:
        raise ObjectiveLeaseConfigurationError("ttl_seconds must be finite and >= 1")
    return result


def _now(value: Any) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ObjectiveLeaseConfigurationError("now must be a finite number")
    result = float(value)
    if not math.isfinite(result):
        raise ObjectiveLeaseConfigurationError("now must be a finite number")
    return result


class RenewableDurableLeaseStore(DurableLeaseStore):
    """Add renewal/current-owner fencing without rewriting PR #5."""

    @staticmethod
    def _same_lease(left: Lease, right: Lease) -> bool:
        return (
            left.task_id == right.task_id
            and left.lease_id == right.lease_id
            and left.holder_id == right.holder_id
            and left.source_pins == right.source_pins
        )

    def assert_current(
        self,
        lease: Lease,
        *,
        now: float,
        expected_source_pins: Mapping[str, str] | None = None,
    ) -> Lease:
        current = self.get(lease.task_id)
        if current is None:
            raise LeaseFenceLost("lease is absent")
        if not self._same_lease(current, lease):
            raise LeaseFenceLost("lease fencing token is no longer current")
        now = _now(now)
        if current.is_stale(now):
            raise LeaseFenceLost("lease is stale")
        if expected_source_pins is not None:
            expected = {str(k): str(v) for k, v in expected_source_pins.items()}
            if current.source_pins != expected:
                raise LeaseFenceLost("lease source binding tokens drifted")
        return current

    def renew(
        self,
        lease: Lease,
        *,
        now: float,
        ttl_seconds: float,
        expected_source_pins: Mapping[str, str] | None = None,
    ) -> Lease:
        """Renew only an exact, still-live fencing token.

        Once a lease is stale, it is not revivable by its old holder. The caller
        must reacquire through normal arbitration, allowing another worker to win.
        """
        now = _now(now)
        ttl_seconds = _ttl(ttl_seconds)
        expected = (
            {str(k): str(v) for k, v in expected_source_pins.items()}
            if expected_source_pins is not None
            else dict(lease.source_pins)
        )

        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                """
                SELECT task_id, lease_id, holder_id, source_pins_json, expires_at
                FROM task_leases
                WHERE task_id = ?
                """,
                (lease.task_id,),
            ).fetchone()
            current = self._decode_row(row)
            if current is None:
                raise LeaseFenceLost("lease disappeared before renewal")
            if not self._same_lease(current, lease):
                raise LeaseFenceLost("lease fencing token changed before renewal")
            if current.is_stale(now):
                raise LeaseFenceLost("stale lease cannot be renewed")
            if current.source_pins != expected:
                raise LeaseFenceLost("lease source binding tokens drifted")

            new_expiry = now + ttl_seconds
            cursor = connection.execute(
                """
                UPDATE task_leases
                SET expires_at = ?
                WHERE task_id = ?
                  AND lease_id = ?
                  AND holder_id = ?
                  AND source_pins_json = ?
                  AND expires_at = ?
                """,
                (
                    new_expiry,
                    current.task_id,
                    current.lease_id,
                    current.holder_id,
                    json.dumps(
                        current.source_pins,
                        sort_keys=True,
                        separators=(",", ":"),
                        ensure_ascii=False,
                    ),
                    current.expires_at,
                ),
            )
            if cursor.rowcount != 1:
                raise LeaseFenceLost("lease compare-and-swap lost during renewal")
            connection.execute("COMMIT")
            return Lease(
                lease_id=current.lease_id,
                task_id=current.task_id,
                holder_id=current.holder_id,
                source_pins=dict(current.source_pins),
                expires_at=new_expiry,
            )
        except BaseException:
            try:
                connection.execute("ROLLBACK")
            except sqlite3.Error:
                pass
            raise
        finally:
            connection.close()


class ObjectiveLeaseFence:
    """Typed facade for one admitted Demiurge objective lease."""

    def __init__(
        self,
        db_path: str | Path,
        *,
        holder_id: str,
        trusted_human_authorizers: frozenset[str] = DEFAULT_AUTHORIZERS,
        boot_controller_head: str = PINNED_BOOT_CONTROLLER_HEAD,
        supervisor_head: str = PINNED_DEMIURGE_SUPERVISOR_HEAD,
    ) -> None:
        self.holder_id = _safe_id(holder_id, "holder_id", MAX_HOLDER_ID_CHARS)
        self.boot_controller_head = _sha1(
            boot_controller_head, "boot_controller_head"
        )
        self.supervisor_head = _sha1(supervisor_head, "supervisor_head")
        self.binding_tokens = {
            "boot_controller_git_sha1": self.boot_controller_head,
            "demiurge_supervisor_git_sha1": self.supervisor_head,
        }
        self.client = HabitatSwarmClient(
            holder_id=self.holder_id,
            trusted_human_authorizers=frozenset(trusted_human_authorizers),
        )
        self.store = RenewableDurableLeaseStore(db_path)

    @staticmethod
    def task_id(objective_id: str) -> str:
        objective_id = _safe_id(
            objective_id, "objective_id", MAX_OBJECTIVE_ID_CHARS
        )
        return f"demiurge-objective:{objective_id}"

    def envelope(self, objective_id: str, *, authorized_by: str) -> dict[str, Any]:
        authorized_by = _safe_id(
            authorized_by, "authorized_by", MAX_HOLDER_ID_CHARS
        )
        return {
            "schema": "janus.habitat.handoff.v1",
            "task_id": self.task_id(objective_id),
            "source_pins": dict(self.binding_tokens),
            "authorization": {
                "mode": "EXPLICIT_HUMAN",
                "authorized_by": authorized_by,
            },
            "purpose": "LOCAL_OBJECTIVE_EXECUTION_EXCLUSIVITY",
            "source_writeback": False,
            "external_effect_authority": False,
        }

    def acquire(
        self,
        objective_id: str,
        *,
        authorized_by: str,
        now: float,
        ttl_seconds: float,
    ) -> Lease:
        return self.store.acquire(
            self.client,
            self.envelope(objective_id, authorized_by=authorized_by),
            now=_now(now),
            ttl_seconds=_ttl(ttl_seconds),
        )

    def assert_current(self, lease: Lease, *, now: float) -> Lease:
        return self.store.assert_current(
            lease,
            now=now,
            expected_source_pins=self.binding_tokens,
        )

    def renew(self, lease: Lease, *, now: float, ttl_seconds: float) -> Lease:
        return self.store.renew(
            lease,
            now=now,
            ttl_seconds=ttl_seconds,
            expected_source_pins=self.binding_tokens,
        )

    def release(self, lease: Lease) -> bool:
        return self.store.release(lease)

    def snapshot(self, objective_id: str, *, now: float) -> dict[str, Any]:
        snapshot = self.store.recovery_snapshot(self.task_id(objective_id), _now(now))
        snapshot.update(
            {
                "schema": SCHEMA,
                "binding_tokens_are_git_membership_proof": False,
                "lease_is_source_writeback_permission": False,
                "lease_is_external_effect_permission": False,
            }
        )
        return snapshot


__all__ = [
    "LeaseConflict",
    "LeaseFenceLost",
    "LeaseStoreError",
    "ObjectiveLeaseConfigurationError",
    "ObjectiveLeaseFence",
    "RenewableDurableLeaseStore",
]
