#!/usr/bin/env python3
"""SWARM_GENOME_LEDGER consumer for janus-distributed-ai-swarm.

Connects local SwarmSpiralController turns into one append-only genealogy.
The implementation mirrors the canonical contract owned by Janus-Demiurge;
this repository remains the transport/runtime consumer.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
from typing import Any, Dict, Iterable, List, Optional, Set


GENOME_LAWS = (
    "EVERY_SPIRAL_TURN_HAS_GENEALOGY",
    "ANCESTRY_IS_APPEND_ONLY",
    "FAILURE_REMAINS_IN_LINEAGE",
    "ACTIVE_FRONTIER_DOES_NOT_ERASE_ANCESTORS",
    "CROSS_ENTITY_DERIVATION_REQUIRES_EXPLICIT_PARENT",
    "ANCESTOR_AND_DESCENDANT_TRAVERSAL_ARE_BIDIRECTIONALLY_INDEXED",
)


def _normal(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, dict):
        return {str(k): _normal(v) for k, v in value.items()}
    if isinstance(value, (list, tuple, set)):
        return [_normal(v) for v in value]
    if hasattr(value, "to_dict") and callable(value.to_dict):
        return _normal(value.to_dict())
    return repr(value)


def _fingerprint(value: Any) -> str:
    raw = json.dumps(_normal(value), ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


@dataclass
class GenomeNode:
    genome_id: str
    entity_id: str
    entity_turn: int
    spiral_fingerprint: str
    primary_parent_id: Optional[str]
    parent_ids: List[str]
    relation: str
    identity_strand: Any
    evidence_strand: Any
    fingerprint: str = ""

    def seal(self) -> "GenomeNode":
        body = asdict(self)
        body.pop("fingerprint", None)
        self.fingerprint = _fingerprint(body)
        return self

    def to_dict(self) -> Dict[str, Any]:
        return _normal(asdict(self))


class SwarmGenomeLedger:
    def __init__(self, genome_id: str = "JANUS_SWARM_GENOME") -> None:
        self.genome_id = str(genome_id)
        self.nodes: Dict[str, GenomeNode] = {}
        self.children: Dict[str, List[str]] = {}
        self.by_entity: Dict[str, List[str]] = {}
        self.by_spiral_fingerprint: Dict[str, str] = {}

    @staticmethod
    def _node_id(entity_id: str, turn: int, spiral_fingerprint: str) -> str:
        short = _fingerprint({"entity_id": entity_id, "turn": int(turn), "spiral": spiral_fingerprint})[:24]
        return f"{entity_id}:{int(turn)}:{short}"

    def register_turn(
        self,
        turn: Any,
        *,
        identity_strand: Any = None,
        evidence_strand: Any = None,
        extra_parent_ids: Optional[Iterable[str]] = None,
        relation: str = "SPIRAL_ASCENT",
    ) -> GenomeNode:
        entity_id = str(turn.entity_id)
        entity_turn = int(turn.turn)
        line = self.by_entity.get(entity_id, [])
        primary: Optional[str] = None
        if entity_turn == 0:
            if turn.parent_fingerprint is not None:
                raise ValueError("turn 0 cannot have same-entity parent")
        else:
            if len(line) != entity_turn:
                raise ValueError(f"entity lineage must be ingested sequentially: {entity_id}")
            primary = line[-1]
            if self.nodes[primary].spiral_fingerprint != turn.parent_fingerprint:
                raise ValueError(f"spiral parent mismatch: {entity_id}")

        parents: List[str] = []
        if primary is not None:
            parents.append(primary)
        for parent_id in extra_parent_ids or []:
            parent_id = str(parent_id)
            if parent_id not in self.nodes:
                raise ValueError(f"unknown genome parent: {parent_id}")
            if parent_id not in parents:
                parents.append(parent_id)

        node_id = self._node_id(entity_id, entity_turn, turn.fingerprint)
        if node_id in self.nodes:
            return self.nodes[node_id]
        if turn.fingerprint in self.by_spiral_fingerprint:
            raise ValueError("spiral fingerprint already registered")

        identity = turn.active_state_after if identity_strand is None else identity_strand
        evidence = {
            "candidate_state": turn.candidate_state,
            "outcome": turn.outcome,
            "lessons": list(turn.lessons),
            "constraints": list(turn.constraints),
            "promoted": bool(turn.promoted),
        } if evidence_strand is None else evidence_strand
        node = GenomeNode(
            genome_id=node_id,
            entity_id=entity_id,
            entity_turn=entity_turn,
            spiral_fingerprint=str(turn.fingerprint),
            primary_parent_id=primary,
            parent_ids=parents,
            relation=str(relation),
            identity_strand=_normal(identity),
            evidence_strand=_normal(evidence),
        ).seal()
        self.nodes[node_id] = node
        self.by_entity.setdefault(entity_id, []).append(node_id)
        self.by_spiral_fingerprint[node.spiral_fingerprint] = node_id
        self.children.setdefault(node_id, [])
        for parent_id in parents:
            self.children.setdefault(parent_id, []).append(node_id)
        return node

    def ancestors(self, genome_id: str, include_self: bool = False) -> List[GenomeNode]:
        if genome_id not in self.nodes:
            raise KeyError(genome_id)
        result = [self.nodes[genome_id]] if include_self else []
        seen: Set[str] = {genome_id}
        queue = list(self.nodes[genome_id].parent_ids)
        while queue:
            node_id = queue.pop(0)
            if node_id in seen:
                continue
            seen.add(node_id)
            result.append(self.nodes[node_id])
            queue.extend(self.nodes[node_id].parent_ids)
        return result

    def descendants(self, genome_id: str, include_self: bool = False) -> List[GenomeNode]:
        if genome_id not in self.nodes:
            raise KeyError(genome_id)
        result = [self.nodes[genome_id]] if include_self else []
        seen: Set[str] = {genome_id}
        queue = list(self.children.get(genome_id, []))
        while queue:
            node_id = queue.pop(0)
            if node_id in seen:
                continue
            seen.add(node_id)
            result.append(self.nodes[node_id])
            queue.extend(self.children.get(node_id, []))
        return result

    def trace_to_origins(self, genome_id: str) -> List[List[str]]:
        if genome_id not in self.nodes:
            raise KeyError(genome_id)
        def walk(node_id: str, suffix: List[str]) -> List[List[str]]:
            node = self.nodes[node_id]
            path = [node_id, *suffix]
            if not node.parent_ids:
                return [path]
            paths: List[List[str]] = []
            for parent_id in node.parent_ids:
                paths.extend(walk(parent_id, path))
            return paths
        return walk(genome_id, [])

    def validate(self) -> None:
        for entity_id, ids in self.by_entity.items():
            for expected_turn, node_id in enumerate(ids):
                node = self.nodes[node_id]
                if node.entity_id != entity_id or node.entity_turn != expected_turn:
                    raise ValueError(f"broken entity sequence: {entity_id}")
                if expected_turn == 0 and node.primary_parent_id is not None:
                    raise ValueError("turn 0 primary parent invalid")
                if expected_turn > 0 and node.primary_parent_id != ids[expected_turn - 1]:
                    raise ValueError(f"broken primary ancestry: {entity_id}")
        for node_id, node in self.nodes.items():
            body = asdict(node)
            expected = body.pop("fingerprint")
            if _fingerprint(body) != expected:
                raise ValueError(f"genome fingerprint mismatch: {node_id}")
            for parent_id in node.parent_ids:
                if parent_id not in self.nodes or node_id not in self.children.get(parent_id, []):
                    raise ValueError("parent/child index mismatch")
        for node_id in self.nodes:
            if any(node.genome_id == node_id for node in self.ancestors(node_id)):
                raise ValueError("cycle detected")

    def to_dict(self) -> Dict[str, Any]:
        self.validate()
        return {
            "schema": "janus.swarm.genome_ledger.v1",
            "genome_id": self.genome_id,
            "model": "DUAL_STRAND_SPIRAL_GENEALOGY",
            "laws": list(GENOME_LAWS),
            "nodes": {key: self.nodes[key].to_dict() for key in sorted(self.nodes)},
            "children": {key: list(value) for key, value in sorted(self.children.items())},
            "entities": {key: list(value) for key, value in sorted(self.by_entity.items())},
        }


__all__ = ["GENOME_LAWS", "GenomeNode", "SwarmGenomeLedger"]
