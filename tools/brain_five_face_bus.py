#!/usr/bin/env python3
"""Five JANUS brain Face workers: exact-source contract exchange via Actions artifacts.

Read-only SHA-pinned evidence transport, no LLM inference, model calls, target
repository execution or mutation. Genesis rule: MESSAGE != COMMAND.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any, Callable
from urllib import request
from urllib.parse import quote

from portfolio_genesis_face_bus import (
    FaceBusError, canonical, digest, face_id, head_sha, read_manifest, write_json
)

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "configs" / "brain_five_faces_v1.json"
SCHEMA = "janus.brain.face_contract_observation.v1"
ACK_SCHEMA = "janus.brain.face_delivery_ack.v1"
MAX_ANCHOR = 131072
SHA40 = re.compile(r"^[0-9a-f]{40}$")
SAFE_PATH = re.compile(r"^[A-Za-z0-9_./-]+$")
SAFE_ROLE = re.compile(r"^[A-Z0-9_]{4,80}$")


def load_cohort(path: Path = CONFIG) -> list[dict[str, Any]]:
    config = json.loads(path.read_text(encoding="utf-8"))
    if config.get("schema") != "janus.brain.five_face_cohort.v1" \
            or config.get("max_parallel") != 3 \
            or config.get("default_write_permission") is not False \
            or config.get("model_calls_enabled") is not False \
            or config.get("paid_provider_calls_enabled") is not False \
            or config.get("full_auto_deployment") is not False:
        raise FaceBusError("BRAIN_COHORT_POLICY_INVALID")
    faces = config.get("faces")
    if not isinstance(faces, list) or len(faces) != 5:
        raise FaceBusError("FIVE_FACES_REQUIRED")
    allowed = {x["repository"]: x["ref"] for x in read_manifest()["repositories"]}
    seen: set[str] = set()
    for entry in faces:
        if not isinstance(entry, dict) or set(entry) != {
            "repository", "ref", "role", "anchor_path", "anchor_kind", "expected_schema"
        }:
            raise FaceBusError("FACE_ENTRY_SHAPE")
        repo = entry["repository"]
        path = entry["anchor_path"]
        if not isinstance(repo, str) or repo not in allowed or repo in seen \
                or entry["ref"] != allowed[repo]:
            raise FaceBusError("FACE_NOT_IN_PORTFOLIO_OR_DUPLICATE")
        seen.add(repo)
        if not isinstance(path, str) or not SAFE_PATH.fullmatch(path) \
                or path.startswith("/") or ".." in path.split("/") \
                or not 4 <= len(path) <= 240:
            raise FaceBusError("UNSAFE_ANCHOR_PATH")
        if not isinstance(entry["role"], str) or not SAFE_ROLE.fullmatch(entry["role"]):
            raise FaceBusError("UNSAFE_FACE_ROLE")
        if entry["anchor_kind"] == "json":
            if not path.endswith(".json") or not isinstance(entry["expected_schema"], str) \
                    or not entry["expected_schema"]:
                raise FaceBusError("JSON_ANCHOR_SCHEMA_REQUIRED")
        elif entry["anchor_kind"] == "markdown":
            if not path.endswith(".md") or entry["expected_schema"] is not None:
                raise FaceBusError("MARKDOWN_ANCHOR_FORMAT")
        else:
            raise FaceBusError("UNSUPPORTED_ANCHOR_KIND")
    return faces


def matrix(faces: list[dict[str, Any]]) -> dict[str, Any]:
    return {"include": [
        {
            "repository": entry["repository"],
            "face": face_id(entry["repository"]),
            "peer": face_id(faces[(i + 1) % len(faces)]["repository"]),
        }
        for i, entry in enumerate(faces)
    ]}


def read_anchor(entry: dict[str, Any], commit: str) -> bytes:
    """Read only one allowlisted public document at the exact source Git SHA."""
    if not isinstance(commit, str) or not SHA40.fullmatch(commit):
        raise FaceBusError("INVALID_GIT_SHA")
    url = ("https://raw.githubusercontent.com/" + entry["repository"] + "/" + commit
           + "/" + quote(entry["anchor_path"], safe="/"))
    req = request.Request(url, headers={"User-Agent": "JANUS-brain-face-bus-v1"})
    with request.urlopen(req, timeout=25) as response:
        raw = response.read(MAX_ANCHOR + 1)
    if not raw or len(raw) > MAX_ANCHOR:
        raise FaceBusError("EMPTY_OR_OVERSIZED_ANCHOR")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise FaceBusError("NON_UTF8_ANCHOR") from exc
    if entry["anchor_kind"] == "json":
        try:
            value = json.loads(text)
        except json.JSONDecodeError as exc:
            raise FaceBusError("INVALID_ANCHOR_JSON") from exc
        if not isinstance(value, dict) or value.get("schema") != entry["expected_schema"]:
            raise FaceBusError("ANCHOR_SCHEMA_MISMATCH")
    elif not text.strip():
        raise FaceBusError("EMPTY_ANCHOR_DOCUMENT")
    return raw


def make_packet(
    faces: list[dict[str, Any]], repository: str, source_sha: str,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    by_repo = {e["repository"]: (i, e) for i, e in enumerate(faces)}
    if repository not in by_repo or not isinstance(source_sha, str) \
            or not SHA40.fullmatch(source_sha):
        raise FaceBusError("SOURCE_NOT_ADMITTED")
    index, entry = by_repo[repository]
    raw = fetcher(entry, source_sha)
    if not isinstance(raw, bytes) or not raw or len(raw) > MAX_ANCHOR:
        raise FaceBusError("INVALID_ANCHOR_BYTES")
    # Validate bytes even when test injection or a mock source is used.
    try:
        decoded = raw.decode("utf-8")
        if entry["anchor_kind"] == "json":
            value = json.loads(decoded)
            if not isinstance(value, dict) or value.get("schema") != entry["expected_schema"]:
                raise FaceBusError("ANCHOR_SCHEMA_MISMATCH")
        elif not decoded.strip():
            raise FaceBusError("EMPTY_ANCHOR_DOCUMENT")
    except (ValueError, UnicodeDecodeError) as exc:
        raise FaceBusError("ANCHOR_DOCUMENT_INVALID") from exc
    packet = {
        "schema": SCHEMA,
        "kind": "SOURCE_PINNED_CONTRACT_OBSERVATION",
        "from_face": face_id(repository),
        "to_face": face_id(faces[(index + 1) % len(faces)]["repository"]),
        "source_repo": repository,
        "source_ref": entry["ref"],
        "source_sha": source_sha,
        "role": entry["role"],
        "anchor_path": entry["anchor_path"],
        "anchor_kind": entry["anchor_kind"],
        "anchor_sha256": hashlib.sha256(raw).hexdigest(),
        "anchor_bytes": len(raw),
        "claim_scope": "SOURCE_DOCUMENT_INTEGRITY_ONLY",
        "authority_delta": 0,
        "effect_budget_delta": 0,
        "permission_granted": False,
        "truth_authority_granted": False,
        "effect_authority_granted": False,
        "model_synthesis_performed": False,
        "target_repository_write": False,
    }
    packet["message_id"] = "MSG_" + digest(packet)[:24].upper()
    packet["payload_sha256"] = digest(packet)
    return packet


def validate_packet(
    faces: list[dict[str, Any]], value: Any,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    if not isinstance(value, dict) or value.get("schema") != SCHEMA:
        raise FaceBusError("BRAIN_PACKET_SCHEMA")
    repo, sha = value.get("source_repo"), value.get("source_sha")
    if not isinstance(repo, str) or not isinstance(sha, str):
        raise FaceBusError("BRAIN_PACKET_SOURCE_FORMAT")
    expected = make_packet(faces, repo, sha, fetcher)
    if value != expected:
        raise FaceBusError("BRAIN_PACKET_NOT_SOURCE_BOUND")
    return expected


def ack_for(
    faces: list[dict[str, Any]], value: Any, recipient: str,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    msg = validate_packet(faces, value, fetcher)
    allowed = {x["repository"] for x in faces}
    if recipient not in allowed or msg["to_face"] != face_id(recipient):
        raise FaceBusError("BRAIN_ACK_WRONG_RECIPIENT")
    ack = {
        "schema": ACK_SCHEMA,
        "kind": "DELIVERY_ACK",
        "from_face": face_id(recipient),
        "to_face": msg["from_face"],
        "message_id": msg["message_id"],
        "source_sha": msg["source_sha"],
        "source_anchor_sha256": msg["anchor_sha256"],
        "delivery_state": "RECEIVED_NOT_EXECUTED",
        "authority_delta": 0,
        "permission_granted": False,
        "effect_authority_granted": False,
    }
    ack["payload_sha256"] = digest(ack)
    return ack


def load_messages(
    faces: list[dict[str, Any]], root: Path,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, dict[str, Any]]:
    expected = {face_id(entry["repository"]) for entry in faces}
    found: dict[str, dict[str, Any]] = {}
    for path in root.rglob("FACE_*.json"):
        msg = validate_packet(faces, json.loads(path.read_text(encoding="utf-8")), fetcher)
        sender = msg["from_face"]
        if path.name != sender + ".json" or sender in found:
            raise FaceBusError("BRAIN_DUPLICATE_OR_WRONG_FILENAME")
        found[sender] = msg
    if set(found) != expected:
        raise FaceBusError("BRAIN_MISSING_OR_UNKNOWN_SENDER")
    return found


def route(
    faces: list[dict[str, Any]], inbox: Path, output: Path,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    messages = load_messages(faces, inbox, fetcher)
    for face in sorted(messages):
        mailbox = [msg for msg in messages.values() if msg["to_face"] == face]
        if len(mailbox) != 1:
            raise FaceBusError("BRAIN_MAILBOX_CARDINALITY")
        write_json(output / ("MAILBOX_" + face + ".json"), {
            "schema": "janus.brain.face_mailbox.v1",
            "recipient": face,
            "messages": mailbox,
            "execute_messages": False,
        })
    return {"status": "FIVE_BRAIN_MESSAGES_ROUTED_NO_EXECUTION",
            "source_faces": len(messages), "mailboxes": len(messages),
            "authority_delta": 0}


def receive(
    faces: list[dict[str, Any]], repository: str, mailbox_path: Path,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    if repository not in {e["repository"] for e in faces}:
        raise FaceBusError("BRAIN_UNKNOWN_RECIPIENT")
    mailbox = json.loads(mailbox_path.read_text(encoding="utf-8"))
    if not isinstance(mailbox, dict) \
            or set(mailbox) != {"schema", "recipient", "messages", "execute_messages"} \
            or mailbox["schema"] != "janus.brain.face_mailbox.v1" \
            or mailbox["recipient"] != face_id(repository) \
            or mailbox["execute_messages"] is not False \
            or not isinstance(mailbox["messages"], list) \
            or len(mailbox["messages"]) != 1:
        raise FaceBusError("BRAIN_MAILBOX_INVALID")
    return ack_for(faces, mailbox["messages"][0], repository, fetcher)


def witness(
    faces: list[dict[str, Any]], inbox: Path, acks: Path,
    fetcher: Callable[[dict[str, Any], str], bytes] = read_anchor,
) -> dict[str, Any]:
    messages = load_messages(faces, inbox, fetcher)
    expected: dict[str, dict[str, Any]] = {}
    for msg in messages.values():
        recipient = next(e["repository"] for e in faces
                         if face_id(e["repository"]) == msg["to_face"])
        expected[msg["to_face"]] = ack_for(faces, msg, recipient, fetcher)
    received: dict[str, dict[str, Any]] = {}
    for path in acks.rglob("ACK_FACE_*.json"):
        msg = json.loads(path.read_text(encoding="utf-8"))
        sender = msg.get("from_face") if isinstance(msg, dict) else None
        if sender not in expected or sender in received \
                or path.name != "ACK_" + sender + ".json" or msg != expected[sender]:
            raise FaceBusError("BRAIN_ACK_UNTRUSTED_OR_DUPLICATE")
        received[sender] = msg
    if set(received) != set(expected):
        raise FaceBusError("BRAIN_MISSING_ACK")
    return {
        "schema": "janus.brain.five_face_delivery_receipt.v1",
        "status": "FIVE_FACES_DELIVERED_AND_ACKED",
        "worker_count": len(messages),
        "delivery_ack_count": len(received),
        "sources": [
            {"repository": msg["source_repo"], "commit": msg["source_sha"],
             "anchor_path": msg["anchor_path"], "anchor_sha256": msg["anchor_sha256"]}
            for msg in sorted(messages.values(), key=lambda x: x["source_repo"])
        ],
        "source_contracts_verified_against_exact_commits": True,
        "external_truth_confirmed": False,
        "models_executed": False,
        "remote_code_executed": False,
        "deployment_authorized": False,
        "authority_delta": 0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)
    for command in ("matrix", "observe", "route", "receive", "witness"):
        p = sub.add_parser(command)
        if command in {"observe", "receive"}:
            p.add_argument("--repository", required=True)
            p.add_argument("--out", required=True)
        if command == "observe":
            p.add_argument("--source-sha", help="Offline test fixture only")
        if command in {"route", "witness"}:
            p.add_argument("--inbox", required=True)
            p.add_argument("--out", required=True)
        if command == "receive":
            p.add_argument("--mailbox", required=True)
        if command == "witness":
            p.add_argument("--acks", required=True)
    args = parser.parse_args()
    try:
        faces = load_cohort()
        if args.cmd == "matrix":
            value = matrix(faces)
        elif args.cmd == "observe":
            entry = next((e for e in faces if e["repository"] == args.repository), None)
            if entry is None:
                raise FaceBusError("BRAIN_UNKNOWN_SOURCE")
            sha = args.source_sha or head_sha(entry["repository"], entry["ref"])
            value = make_packet(faces, args.repository, sha)
            write_json(Path(args.out), value)
        elif args.cmd == "route":
            value = route(faces, Path(args.inbox), Path(args.out))
            write_json(Path(args.out) / "ROUTING_SUMMARY.json", value)
        elif args.cmd == "receive":
            value = receive(faces, args.repository, Path(args.mailbox))
            write_json(Path(args.out), value)
        else:
            value = witness(faces, Path(args.inbox), Path(args.acks))
            write_json(Path(args.out), value)
        print(canonical(value))
        return 0
    except (FaceBusError, OSError, ValueError, TimeoutError) as exc:
        print(canonical({"status": "HOLD", "error": str(exc), "authority_delta": 0}))
        return 2


if __name__ == "__main__":
    sys.exit(main())
