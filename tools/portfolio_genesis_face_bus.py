#!/usr/bin/env python3
"""JANUS portfolio Face bus: bounded, read-only cross-job message exchange.

Inspired by Janus_Genesis Multi-Face and Third Wish contracts. This is a
transport/receipt pilot, NOT a model, authorization broker or proof of truth.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "configs" / "portfolio_genesis_faces_v1.json"
MESSAGE_SCHEMA = "janus.portfolio.face_observation.v1"
ACK_SCHEMA = "janus.portfolio.face_delivery_ack.v1"
SHA40 = re.compile(r"^[0-9a-f]{40}$")
REPO = re.compile(r"^Hawkar-usls/[A-Za-z0-9_.-]+$")
REF = re.compile(r"^[A-Za-z0-9_.-]+$")


class FaceBusError(ValueError):
    """Invalid or conflicting Face transport record; fail closed."""


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, ensure_ascii=False,
                      separators=(",", ":"), allow_nan=False)


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value).encode("utf-8")).hexdigest()


def face_id(repository: str) -> str:
    return "FACE_REPO_" + hashlib.sha256(repository.lower().encode("utf-8")).hexdigest()[:12].upper()


def read_manifest(path: Path = MANIFEST_PATH) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("schema") != "janus.portfolio.genesis_face_manifest.v1":
        raise FaceBusError("MANIFEST_SCHEMA")
    items = value.get("repositories")
    if not isinstance(items, list) or not items:
        raise FaceBusError("NO_REPOSITORIES")
    names = []
    for item in items:
        if not isinstance(item, dict) or set(item) != {"repository", "ref"}:
            raise FaceBusError("REPOSITORY_ENTRY_FORMAT")
        name, ref = item["repository"], item["ref"]
        if not isinstance(name, str) or not REPO.fullmatch(name):
            raise FaceBusError("REPOSITORY_NOT_ALLOWLISTED")
        if not isinstance(ref, str) or not REF.fullmatch(ref):
            raise FaceBusError("REF_NOT_ALLOWLISTED")
        names.append(name)
    if len(names) != len(set(names)):
        raise FaceBusError("DUPLICATE_REPOSITORY")
    canary = value.get("canary_repositories")
    if not isinstance(canary, list) or len(canary) != 3 or len(set(canary)) != 3:
        raise FaceBusError("CANARY_THREE_REQUIRED")
    if not set(canary).issubset(names):
        raise FaceBusError("CANARY_OUTSIDE_MANIFEST")
    policy = value.get("execution_policy")
    if not isinstance(policy, dict) or policy.get("model_calls_enabled") is not False \
            or policy.get("write_to_target_repositories") is not False \
            or policy.get("automatic_full_fleet") is not False:
        raise FaceBusError("UNSAFE_EXECUTION_POLICY")
    return value


def selected(manifest: dict[str, Any], cohort: str = "canary") -> list[dict[str, str]]:
    entries = {item["repository"]: item for item in manifest["repositories"]}
    if cohort == "canary":
        names = manifest["canary_repositories"]
    elif cohort == "all":
        names = [item["repository"] for item in manifest["repositories"]]
    else:
        raise FaceBusError("UNSUPPORTED_COHORT")
    return [entries[name] for name in names]


def face_matrix(manifest: dict[str, Any], cohort: str = "canary") -> dict[str, Any]:
    entries = selected(manifest, cohort)
    return {"include": [
        {"repository": item["repository"], "ref": item["ref"],
         "face": face_id(item["repository"]),
         "peer": face_id(entries[(i + 1) % len(entries)]["repository"])}
        for i, item in enumerate(entries)
    ]}


def head_sha(repository: str, ref: str) -> str:
    if not REPO.fullmatch(repository) or not REF.fullmatch(ref):
        raise FaceBusError("UNSAFE_GIT_SOURCE")
    result = subprocess.run(
        ["git", "ls-remote", "--exit-code",
         "https://github.com/" + repository + ".git", "refs/heads/" + ref],
        capture_output=True, text=True, timeout=45, check=False,
        env={**os.environ, "GIT_TERMINAL_PROMPT": "0"},
    )
    if result.returncode:
        raise FaceBusError("GIT_HEAD_UNAVAILABLE")
    lines = result.stdout.strip().splitlines()
    if len(lines) != 1:
        raise FaceBusError("AMBIGUOUS_GIT_HEAD")
    pieces = lines[0].split()
    if len(pieces) != 2 or pieces[1] != "refs/heads/" + ref or not SHA40.fullmatch(pieces[0]):
        raise FaceBusError("INVALID_GIT_HEAD")
    return pieces[0]


def peer_map(manifest: dict[str, Any], cohort: str) -> dict[str, dict[str, str]]:
    return {entry["face"]: entry for entry in face_matrix(manifest, cohort)["include"]}


def packet(manifest: dict[str, Any], repository: str, source_sha: str, cohort: str = "canary") -> dict[str, Any]:
    peers = peer_map(manifest, cohort)
    source_face = face_id(repository)
    if source_face not in peers or not SHA40.fullmatch(source_sha):
        raise FaceBusError("INVALID_SOURCE_OR_SHA")
    node = peers[source_face]
    value = {
        "schema": MESSAGE_SCHEMA,
        "kind": "OBSERVATION",
        "from_face": source_face,
        "to_face": node["peer"],
        "source_repo": repository,
        "source_ref": node["ref"],
        "source_sha": source_sha,
        "work_item": "READ_ONLY_GIT_HEAD",
        "observation": "Git ref observed; no model synthesis or code execution in target.",
        "authority_delta": 0,
        "mass_effect_budget_delta": 0,
        "permission_granted": False,
        "truth_authority_granted": False,
        "effect_authority_granted": False,
        "model_call_performed": False,
        "target_repository_write": False,
    }
    value["message_id"] = "MSG_" + digest(value)[:24].upper()
    value["payload_sha256"] = digest({k: v for k, v in value.items() if k != "payload_sha256"})
    return value


def validate_packet(manifest: dict[str, Any], value: Any, cohort: str) -> dict[str, Any]:
    if not isinstance(value, dict) or value.get("schema") != MESSAGE_SCHEMA:
        raise FaceBusError("PACKET_SCHEMA")
    source, sha = value.get("source_repo"), value.get("source_sha")
    if not isinstance(source, str) or not isinstance(sha, str):
        raise FaceBusError("PACKET_SOURCE_FORMAT")
    expected = packet(manifest, source, sha, cohort)
    if value != expected:
        raise FaceBusError("PACKET_CONTENT_OR_AUTHORITY_MISMATCH")
    return expected


def ack_for(manifest: dict[str, Any], message: Any, recipient_repo: str, cohort: str) -> dict[str, Any]:
    pkt = validate_packet(manifest, message, cohort)
    if pkt["to_face"] != face_id(recipient_repo):
        raise FaceBusError("ACK_NOT_ADDRESSED_TO_RECIPIENT")
    value = {
        "schema": ACK_SCHEMA,
        "kind": "ACK",
        "message_id": pkt["message_id"],
        "from_face": face_id(recipient_repo),
        "to_face": pkt["from_face"],
        "source_sha": pkt["source_sha"],
        "delivery_state": "RECEIVED_NO_EXECUTION",
        "authority_delta": 0,
        "permission_granted": False,
        "effect_authority_granted": False,
    }
    value["payload_sha256"] = digest(value)
    return value


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
                    encoding="utf-8")


def read_packets(manifest: dict[str, Any], root: Path, cohort: str) -> dict[str, dict[str, Any]]:
    peers = peer_map(manifest, cohort)
    messages: dict[str, dict[str, Any]] = {}
    for path in root.rglob("FACE_*.json"):
        pkt = validate_packet(manifest, json.loads(path.read_text(encoding="utf-8")), cohort)
        source = pkt["from_face"]
        if path.name != source + ".json" or source in messages:
            raise FaceBusError("DUPLICATE_OR_MISNAMED_SOURCE")
        messages[source] = pkt
    if set(messages) != set(peers):
        raise FaceBusError("MISSING_OR_UNKNOWN_FACE")
    return messages


def route(manifest: dict[str, Any], inbox: Path, out: Path, cohort: str) -> dict[str, Any]:
    messages = read_packets(manifest, inbox, cohort)
    for face in peer_map(manifest, cohort):
        mailbox = [msg for msg in messages.values() if msg["to_face"] == face]
        write_json(out / ("MAILBOX_" + face + ".json"), {
            "schema": "janus.portfolio.face_mailbox.v1", "recipient": face,
            "messages": mailbox, "execute_messages": False,
        })
    return {"status": "ROUTED_NO_EXECUTION", "messages": len(messages),
            "mailboxes": len(messages), "authority_delta": 0}


def receive(manifest: dict[str, Any], repository: str, mailbox_path: Path, cohort: str) -> dict[str, Any]:
    if face_id(repository) not in peer_map(manifest, cohort):
        raise FaceBusError("RECIPIENT_NOT_IN_COHORT")
    mailbox = json.loads(mailbox_path.read_text(encoding="utf-8"))
    if mailbox.get("schema") != "janus.portfolio.face_mailbox.v1" \
            or mailbox.get("recipient") != face_id(repository) \
            or mailbox.get("execute_messages") is not False \
            or not isinstance(mailbox.get("messages"), list):
        raise FaceBusError("INVALID_MAILBOX")
    acks = [ack_for(manifest, msg, repository, cohort) for msg in mailbox["messages"]]
    if len(acks) != 1:
        raise FaceBusError("EXPECTED_ONE_RING_MESSAGE")
    return acks[0]


def verify(manifest: dict[str, Any], inbox: Path, acks_root: Path, cohort: str) -> dict[str, Any]:
    messages = read_packets(manifest, inbox, cohort)
    expected_acks = {}
    for message in messages.values():
        target = next(item["repository"] for item in selected(manifest, cohort)
                      if face_id(item["repository"]) == message["to_face"])
        expected_acks[message["to_face"]] = ack_for(manifest, message, target, cohort)
    received: dict[str, dict[str, Any]] = {}
    for path in acks_root.rglob("ACK_*.json"):
        value = json.loads(path.read_text(encoding="utf-8"))
        face = value.get("from_face")
        if face not in expected_acks or path.name != "ACK_" + str(face) + ".json" \
                or face in received or value != expected_acks[face]:
            raise FaceBusError("ACK_UNTRUSTED_OR_DUPLICATE")
        received[face] = value
    if set(received) != set(expected_acks):
        raise FaceBusError("MISSING_ACK")
    return {
        "schema": "janus.portfolio.face_transport_receipt.v1",
        "status": "DELIVERY_ACK_VERIFIED_NOT_MODEL_ANALYSIS",
        "source_faces": len(messages),
        "delivery_acks": len(received),
        "source_head_values": {m["source_repo"]: m["source_sha"] for m in messages.values()},
        "authority_delta": 0,
        "model_synthesis_proven": False,
        "external_source_truth_independently_proven": False,
        "target_write_permission_granted": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)
    for command in ("matrix", "observe", "route", "receive", "verify"):
        p = sub.add_parser(command)
        p.add_argument("--cohort", choices=("canary", "all"), default="canary")
        if command in {"observe", "receive"}:
            p.add_argument("--repository", required=True)
            p.add_argument("--out", required=True)
        if command == "route":
            p.add_argument("--inbox", required=True)
            p.add_argument("--out", required=True)
        if command == "receive":
            p.add_argument("--mailbox", required=True)
        if command == "verify":
            p.add_argument("--inbox", required=True)
            p.add_argument("--acks", required=True)
            p.add_argument("--out", required=True)
        if command == "observe":
            p.add_argument("--source-sha", help="Offline exact source SHA; otherwise query remote HEAD")
    args = parser.parse_args()
    m = read_manifest()
    try:
        if args.cmd == "matrix":
            result = face_matrix(m, args.cohort)
        elif args.cmd == "observe":
            ref = next((x["ref"] for x in selected(m, args.cohort)
                        if x["repository"] == args.repository), None)
            if not ref:
                raise FaceBusError("REPOSITORY_NOT_IN_COHORT")
            sha = args.source_sha or head_sha(args.repository, ref)
            result = packet(m, args.repository, sha, args.cohort)
            write_json(Path(args.out), result)
        elif args.cmd == "route":
            result = route(m, Path(args.inbox), Path(args.out), args.cohort)
            write_json(Path(args.out) / "ROUTE_REPORT.json", result)
        elif args.cmd == "receive":
            result = receive(m, args.repository, Path(args.mailbox), args.cohort)
            write_json(Path(args.out), result)
        else:
            result = verify(m, Path(args.inbox), Path(args.acks), args.cohort)
            write_json(Path(args.out), result)
        print(canonical(result))
        return 0
    except (FaceBusError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(canonical({"status": "HOLD", "error": str(exc), "authority_delta": 0}))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
