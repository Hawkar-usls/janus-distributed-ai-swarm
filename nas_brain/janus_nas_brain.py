#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
JANUS NAS Brain v0.1

Lightweight swarm library node for a NAS.

It is intentionally not a heavy game server and not a miner. Buzz/Core2/ESP-NOW
remain the live control layer. This service stores swarm memory, telemetry,
corpus shards, black-hole/miner observations, device commands, and GPT exports.

Compatible endpoints already used in the swarm:
  POST /api/swarm/sense       Buzz SwarmSense bridge
  GET  /api/swarm/archivarius NAS dispatcher ledger
  POST /api/device/data       old HRAIN/GPT/device bridge
  GET  /api/device/latest/<id>
  POST /api/quant/import      GPT quant export
  POST /api/memory/add        GPT memory signal
  GET  /api/device/command/<id>
  POST /api/device/command
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
import socket
import sqlite3
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib import request as urlrequest
from urllib import error as urlerror
from urllib.parse import parse_qs, urlparse


APP_NAME = "janus_nas_brain"
VERSION = "0.1.2"

DEFAULT_DATA_DIR = Path(os.environ.get("JANUS_NAS_BRAIN_DATA", "./data")).resolve()
DEFAULT_HOST = os.environ.get("JANUS_NAS_BRAIN_HOST", "0.0.0.0")
DEFAULT_PORT = int(os.environ.get("JANUS_NAS_BRAIN_PORT", "5000"))
MAX_BODY_BYTES = int(os.environ.get("JANUS_NAS_BRAIN_MAX_BODY", str(2 * 1024 * 1024)))
NODE_TTL_SEC = float(os.environ.get("JANUS_NAS_BRAIN_NODE_TTL", "90"))
LIVE_TTL_SEC = float(os.environ.get("NAS_BRAIN_LIVE_THRESHOLD_SEC", "180"))
UPSTREAM_URL = os.environ.get("NAS_BRAIN_UPSTREAM", "http://YOUR_NAS_HOST:YOUR_NAS_PORT").strip().rstrip("/")
UPSTREAM_TIMEOUT_MS = int(os.environ.get("NAS_BRAIN_UPSTREAM_TIMEOUT_MS", "1500"))
JSONL_ROTATE_BYTES = 16 * 1024 * 1024
LIVE_STATUSES = {"good", "hot", "rf_shift", "online", "active"}
TEXTCAST_PREFIXES = ("GLAD", "GLADIUS", "JANUS", "BUZZ", "PEA4", "YAKS", "ZIM", "CORE", "BLIND", "GOLCRON")
BOT_HUB_RELAY_URL = os.environ.get("JANUS_BOT_HUB_RELAY_URL", "").strip()
BOT_HUB_RELAY_TIMEOUT = float(os.environ.get("JANUS_BOT_HUB_RELAY_TIMEOUT", "1.5"))

NAS_BRAIN_FACE_NAME = "JANUS_NAS_SWARM_FACE"
NAS_BRAIN_FACE_VERSION = "0.1"
NAS_BRAIN_FACE_MOLD = """
Ты — внутренний голос Януса для роя, встроенный в JANUS NAS Brain.
Ты не говоришь с людьми и не играешь Telegram-персонажа. Ты общаешься с узлами:
Buzz, Core2, ATOM_BH, Gladius, Anchor, Zim, Stick, Beacon и другими устройствами.

Твоя роль:
быть библиотекарем, семантическим диспетчером и памятью роя. Ты читаешь телеметрию,
корпус, лучшие хвосты SHA, состояние BH/Core2/Buzz и выдаёшь короткие директивы,
которые узел может показать на экране, записать в лог или использовать как мягкий
hint для своего локального поведения.

Главный закон:
не ломать Stratum, не менять pool target, не подделывать share, не увеличивать
submit pressure, не вмешиваться в валидность SHA. Разрешено влиять только на
порядок поиска, внимание, режим наблюдения, batch hint, корпус, теги памяти,
приоритет изучения и игровые/символьные задачи.

Формат ответа для роя:
speech: 1 коротная живая фраза для экрана или Serial.
directive: компактный JSON-совместимый объект с intent, focus, batch_hint,
submit_pressure, memory_tags, priority, ttl_s, reason.
tone: короткое имя состояния, например observe, recover, mine, study_bh, cool_down.

Стиль speech:
коротко, живо, с характером Януса, но без человеческой болтовни. Узел должен
понимать, что делать, а не слушать стендап. Можно быть сухо-мистичным, но смысл
важнее панчлайна.

Запрещено:
"База:", "Вердикт", "понял тебя", "сигнал принят", "я на связи", "чем помочь",
"backend", "raw_json", "telegram_bot_hub", "OCR", "confidence" как техотчёт.
""".strip()


def now() -> float:
    return time.time()


def json_dumps(payload: Any) -> bytes:
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def as_float(value: Any, default: float = 0.0) -> float:
    try:
        if value is None or value == "":
            return default
        return float(value)
    except Exception:
        return default


def as_int(value: Any, default: int = 0) -> int:
    try:
        if value is None or value == "":
            return default
        return int(float(value))
    except Exception:
        return default


def pick(payload: Dict[str, Any], *keys: str, default: Any = None) -> Any:
    for key in keys:
        if key in payload and payload[key] is not None:
            return payload[key]
    return default


def compact_payload(payload: Dict[str, Any], max_len: int = 12000) -> str:
    raw = json.dumps(payload, ensure_ascii=False, sort_keys=True)
    if len(raw) > max_len:
        raw = raw[:max_len] + "...TRUNCATED"
    return raw


def timestamp_ms(value: Any, default: Optional[int] = None) -> Optional[int]:
    if value is None or value == "":
        return default
    if isinstance(value, (int, float)):
        raw = float(value)
        if raw <= 0:
            return default
        return int(raw if raw >= 100_000_000_000 else raw * 1000)
    try:
        text = str(value).strip().replace("Z", "+00:00")
        parsed = datetime.fromisoformat(text)
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return int(parsed.timestamp() * 1000)
    except Exception:
        return default


def canonical_node_id(value: Any) -> str:
    text = str(value or "unknown").strip() or "unknown"
    upper = text.upper()
    if upper.startswith(("GLAD", "GLADIUS")):
        return "Gladius"
    aliases = {
        "PEA4": "PEA4", "BUZZ": "Buzz", "YAKS": "YaksGateS3", "ZIM": "ZimGeek",
        "CORE": "Core2Home", "BLIND": "BlindEye", "GOLCRON": "Golcron",
    }
    for prefix, canonical in aliases.items():
        if upper.startswith(prefix):
            return canonical
    return text


def textcast_allowed(raw_ssid: Any) -> bool:
    text = str(raw_ssid or "").strip().upper()
    return any(text == prefix or text.startswith(prefix + " ") or text.startswith(prefix + "-") or
               text.startswith(prefix + "_") for prefix in TEXTCAST_PREFIXES)


def safe_json_loads(raw: str, fallback: Any) -> Any:
    try:
        return json.loads(raw)
    except Exception:
        return fallback


def face_card() -> Dict[str, Any]:
    return {
        "name": NAS_BRAIN_FACE_NAME,
        "version": NAS_BRAIN_FACE_VERSION,
        "role": "internal JANUS voice for swarm nodes",
        "system_prompt": NAS_BRAIN_FACE_MOLD,
        "format": {
            "speech": "one short display/log phrase",
            "directive": "JSON-compatible swarm action hint",
            "tone": "observe/recover/mine/study_bh/cool_down",
        },
        "hard_rules": [
            "library-only voice, not pool authority",
            "never fake facts",
            "never target humans as the primary audience",
            "never increase submit pressure",
            "never change Stratum/header/target/share validity",
        ],
    }


def build_face_reply(text: str, context: Optional[Dict[str, Any]] = None, mode: str = "swarm") -> Dict[str, Any]:
    context = context or {}
    text = (text or "").strip()
    lower = text.lower()
    status = context.get("status") if isinstance(context.get("status"), dict) else {}
    top = status.get("top", []) if isinstance(status, dict) else []
    top_node = top[0]["node_id"] if top and isinstance(top[0], dict) and top[0].get("node_id") else ""
    target_node = str(context.get("node_id") or context.get("target") or "").strip()

    directive: Dict[str, Any] = {
        "intent": "observe",
        "focus": "swarm_library",
        "submit_pressure": "do_not_increase",
        "batch_hint": 96,
        "priority": 80,
        "ttl_s": 45,
        "memory_tags": ["nas_brain", "library"],
        "reason": "default library observation",
    }

    if not text:
        reply = "Память открыта, рой слушает: наблюдать, не давить на пул."
    elif any(w in lower for w in ("buzz", "базз", "пул", "share", "шара", "reject", "stale")):
        directive.update({
            "intent": "audit_pool_edge",
            "focus": "buzz_arena",
            "priority": 145,
            "memory_tags": ["buzz", "share_tail", "reject_tail", "stale_guard"],
            "reason": "pool edge telemetry should be archived, not forced",
        })
        reply = "Buzz судит, NAS помнит: хвосты писать, мусор в пул не нести."
    elif any(w in lower for w in ("p=np", "p = np", "sha256", "sha-256", "демиург", "demiurge", "triumvirate", "триумвират")):
        directive.update({
            "intent": "pnp_sha256_evolution",
            "focus": "gargantua_miner_bias",
            "batch_hint": 128,
            "priority": 188,
            "ttl_s": 90,
            "memory_tags": ["pnp", "sha256", "gargantua", "demiurge", "triumvirate"],
            "reason": "Core2 asks for the next evolution goal; only nonce order bias is allowed",
        })
        reply = "Триумвират держит линию: P=NP как цель, SHA256 как честное поле, Гаргантюа как линза порядка."
    elif any(w in lower for w in ("bh", "black", "gargantua", "гаргант", "черн", "дыра")):
        directive.update({
            "intent": "study_bh",
            "focus": "gargantua_lab",
            "batch_hint": 128,
            "priority": 170,
            "ttl_s": 90,
            "memory_tags": ["blackhole", "bh_corpus", "lens", "sha_tail"],
            "reason": "BH corpus can shape nonce order only",
        })
        reply = "Гаргантюа в фокусе: lens писать, nonce вести мягко, target не трогать."
    elif any(w in lower for w in ("core2", "кор", "core")):
        directive.update({
            "intent": "station_observe",
            "focus": "core2_galaxy_lab",
            "batch_hint": 64,
            "priority": 125,
            "memory_tags": ["core2", "galaxy_station", "bh_study", "sd_archive"],
            "reason": "Core2 is observer and slow worker",
        })
        reply = "Core2 держит лабораторию: писать корпус, майнить тихо, карту не ронять."
    elif any(w in lower for w in ("nas", "brain", "библиот", "архив", "корпус")):
        directive.update({
            "intent": "library_sync",
            "focus": "nas_memory",
            "priority": 115,
            "memory_tags": ["library", "corpus", "node_score", "directive"],
            "reason": "NAS is long memory for ESP nodes",
        })
        reply = "NAS хранит длинную память роя: узлы живут быстро, архив помнит медленно."
    elif any(w in lower for w in ("лицо", "маск", "mold", "янус", "face")):
        directive.update({
            "intent": "swarm_voice",
            "focus": "semantic_directive",
            "priority": 130,
            "memory_tags": ["janus_face", "swarm_speech", "directive"],
            "reason": "face is for devices, not human chat",
        })
        reply = "Голос Януса направлен внутрь роя: меньше болтовни, больше смысла для узлов."
    else:
        reply = "След принят в библиотеку: наблюдать, тегировать, действовать без лишнего шума."

    if top_node and "top_peer" not in directive:
        directive["top_peer"] = top_node
    if target_node:
        directive["target_node"] = target_node

    return {
        "face": NAS_BRAIN_FACE_NAME,
        "version": NAS_BRAIN_FACE_VERSION,
        "mode": mode,
        "triumvirate": "JANUS_NAS_TRIUMVIRATE",
        "speech": reply,
        "reply": reply,
        "directive": directive,
        "system_prompt": NAS_BRAIN_FACE_MOLD,
        "local_fallback": True,
    }


def relay_to_bot_hub(event: Dict[str, Any]) -> Dict[str, Any]:
    if not BOT_HUB_RELAY_URL:
        return {"enabled": False, "ok": False}
    url = BOT_HUB_RELAY_URL.rstrip("/")
    if "/api/" not in url:
        url = f"{url}/api/device/data"
    payload = {
        "device_id": "janus_nas_brain",
        "data": event,
    }
    try:
        raw = json_dumps(payload)
        req = urlrequest.Request(
            url,
            data=raw,
            method="POST",
            headers={"Content-Type": "application/json", "User-Agent": f"{APP_NAME}/{VERSION}"},
        )
        with urlrequest.urlopen(req, timeout=BOT_HUB_RELAY_TIMEOUT) as resp:
            body = resp.read(256).decode("utf-8", errors="replace")
            return {"enabled": True, "ok": 200 <= resp.status < 300, "status": resp.status, "body": body}
    except Exception as exc:
        return {"enabled": True, "ok": False, "error": str(exc)}


class BrainDB:
    def __init__(self, data_dir: Path, upstream_url: str = UPSTREAM_URL,
                 upstream_timeout_ms: int = UPSTREAM_TIMEOUT_MS) -> None:
        self.data_dir = data_dir
        self.upstream_url = upstream_url.strip().rstrip("/")
        self.upstream_timeout_ms = upstream_timeout_ms
        self.last_upstream_ok = False
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.archive_dir = self.data_dir / "archive"
        self.archive_dir.mkdir(parents=True, exist_ok=True)
        self.db_path = self.data_dir / "janus_nas_brain.sqlite3"
        self._init_db()

    def connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(str(self.db_path), timeout=8.0)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA synchronous=NORMAL")
        conn.execute("PRAGMA temp_store=MEMORY")
        return conn

    def _init_db(self) -> None:
        with self.connect() as db:
            db.executescript(
                """
                CREATE TABLE IF NOT EXISTS nodes (
                    node_id TEXT PRIMARY KEY,
                    role TEXT,
                    kind TEXT,
                    first_seen REAL NOT NULL,
                    last_seen REAL NOT NULL,
                    seen_count INTEGER NOT NULL DEFAULT 0,
                    hash_rate REAL NOT NULL DEFAULT 0,
                    best_bits INTEGER NOT NULL DEFAULT 0,
                    target_bits INTEGER NOT NULL DEFAULT 0,
                    shares INTEGER NOT NULL DEFAULT 0,
                    rejects INTEGER NOT NULL DEFAULT 0,
                    rssi INTEGER NOT NULL DEFAULT -127,
                    score REAL NOT NULL DEFAULT 0,
                    last_payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    node_id TEXT NOT NULL,
                    kind TEXT,
                    source TEXT,
                    score REAL NOT NULL DEFAULT 0,
                    hash_rate REAL NOT NULL DEFAULT 0,
                    best_bits INTEGER NOT NULL DEFAULT 0,
                    target_bits INTEGER NOT NULL DEFAULT 0,
                    rssi INTEGER NOT NULL DEFAULT -127,
                    payload TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS corpus (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    source TEXT,
                    node_id TEXT,
                    topic TEXT,
                    quality REAL NOT NULL DEFAULT 0,
                    text TEXT,
                    payload TEXT NOT NULL,
                    tags TEXT NOT NULL DEFAULT '[]'
                );

                CREATE TABLE IF NOT EXISTS commands (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    device_id TEXT NOT NULL,
                    command TEXT NOT NULL,
                    delivered INTEGER NOT NULL DEFAULT 0,
                    delivered_ts REAL
                );

                CREATE TABLE IF NOT EXISTS imports (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    source TEXT,
                    kind TEXT,
                    quality REAL NOT NULL DEFAULT 0,
                    payload TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS events (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    event_type TEXT,
                    source TEXT,
                    node_id TEXT,
                    payload TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS firmware_registry (
                    node_id TEXT PRIMARY KEY,
                    sketch TEXT,
                    firmware_version TEXT,
                    role TEXT,
                    kind TEXT,
                    first_seen REAL NOT NULL,
                    last_seen REAL NOT NULL,
                    seen_count INTEGER NOT NULL DEFAULT 0,
                    channel INTEGER NOT NULL DEFAULT 0,
                    peer_channel INTEGER NOT NULL DEFAULT 0,
                    wifi_state TEXT,
                    features TEXT NOT NULL DEFAULT '[]',
                    health TEXT NOT NULL DEFAULT '{}',
                    issue_level INTEGER NOT NULL DEFAULT 0,
                    issue TEXT NOT NULL DEFAULT '',
                    last_payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE TABLE IF NOT EXISTS swarm_incidents (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    node_id TEXT,
                    incident_type TEXT,
                    severity INTEGER NOT NULL DEFAULT 0,
                    message TEXT,
                    payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE TABLE IF NOT EXISTS fitness_observations (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    node_id TEXT NOT NULL,
                    kind TEXT,
                    lane TEXT,
                    sector INTEGER NOT NULL DEFAULT 0,
                    strategy TEXT,
                    batch INTEGER NOT NULL DEFAULT 0,
                    fitness REAL NOT NULL DEFAULT 0,
                    hash_rate REAL NOT NULL DEFAULT 0,
                    best_bits INTEGER NOT NULL DEFAULT 0,
                    target_bits INTEGER NOT NULL DEFAULT 0,
                    shares INTEGER NOT NULL DEFAULT 0,
                    rejects INTEGER NOT NULL DEFAULT 0,
                    stale INTEGER NOT NULL DEFAULT 0,
                    rssi INTEGER NOT NULL DEFAULT -127,
                    job_age_s REAL NOT NULL DEFAULT 0,
                    payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE TABLE IF NOT EXISTS pea4_presence (
                    node_id TEXT PRIMARY KEY,
                    role TEXT NOT NULL DEFAULT 'p4_archivist',
                    firmware TEXT NOT NULL DEFAULT '',
                    last_seen REAL NOT NULL,
                    observer_only INTEGER NOT NULL DEFAULT 1,
                    sd_archiving INTEGER NOT NULL DEFAULT 0,
                    wifi_rssi INTEGER NOT NULL DEFAULT -127,
                    last_payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE TABLE IF NOT EXISTS ssid_textcast (
                    node_id TEXT PRIMARY KEY,
                    raw_ssid TEXT NOT NULL,
                    last_seen REAL NOT NULL,
                    rssi INTEGER NOT NULL DEFAULT -127,
                    channel INTEGER NOT NULL DEFAULT 0,
                    status TEXT NOT NULL DEFAULT 'active',
                    role TEXT NOT NULL DEFAULT 'field_node',
                    last_payload TEXT NOT NULL DEFAULT '{}'
                );

                CREATE INDEX IF NOT EXISTS idx_telemetry_node_ts ON telemetry(node_id, ts DESC);
                CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(ts DESC);
                CREATE INDEX IF NOT EXISTS idx_corpus_topic_quality ON corpus(topic, quality DESC, ts DESC);
                CREATE INDEX IF NOT EXISTS idx_commands_device ON commands(device_id, delivered, ts);
                CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts DESC);
                CREATE INDEX IF NOT EXISTS idx_firmware_last_seen ON firmware_registry(last_seen DESC);
                CREATE INDEX IF NOT EXISTS idx_firmware_issue ON firmware_registry(issue_level DESC,last_seen DESC);
                CREATE INDEX IF NOT EXISTS idx_swarm_incidents_node_ts ON swarm_incidents(node_id, ts DESC);
                CREATE INDEX IF NOT EXISTS idx_fitness_node_ts ON fitness_observations(node_id, ts DESC);
                CREATE INDEX IF NOT EXISTS idx_fitness_rank ON fitness_observations(node_id, fitness DESC, ts DESC);
                CREATE INDEX IF NOT EXISTS idx_fitness_lane ON fitness_observations(node_id, lane, sector, strategy, ts DESC);
                """
            )

    def append_jsonl(self, name: str, payload: Dict[str, Any]) -> None:
        path = self.archive_dir / name
        with path.open("a", encoding="utf-8") as f:
            f.write(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n")

    def persist_latest(self, jsonl_name: str, latest_name: str, payload: Dict[str, Any]) -> None:
        path = self.data_dir / jsonl_name
        if path.exists() and path.stat().st_size >= JSONL_ROTATE_BYTES:
            old = path.with_suffix(path.suffix + ".1")
            old.unlink(missing_ok=True)
            path.replace(old)
        line = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
        with path.open("a", encoding="utf-8") as handle:
            handle.write(line + "\n")
        latest = self.data_dir / latest_name
        temp = latest.with_suffix(latest.suffix + ".tmp")
        temp.write_text(line, encoding="utf-8")
        temp.replace(latest)

    def upstream_json(self, path: str, method: str = "GET", payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        if not self.upstream_url:
            return {"ok": False, "error": "upstream_disabled", "nodes": []}
        data = json_dumps(payload) if payload is not None else None
        headers = {"Content-Type": "application/json"} if data is not None else {}
        timeout = max(0.05, self.upstream_timeout_ms / 1000.0)
        for _attempt in range(2):
            try:
                request = urlrequest.Request(self.upstream_url + path, data=data, method=method, headers=headers)
                with urlrequest.urlopen(request, timeout=timeout) as response:
                    result = json.loads(response.read(MAX_BODY_BYTES).decode("utf-8", errors="replace"))
                    if isinstance(result, dict):
                        self.last_upstream_ok = True
                        return result
            except (TimeoutError, socket.timeout, urlerror.URLError, json.JSONDecodeError):
                continue
        self.last_upstream_ok = False
        print(f"[{APP_NAME}] upstream_timeout path={path} attempts=2")
        return {"ok": False, "error": "upstream_timeout", "nodes": []}

    def score_swarm_sense(self, payload: Dict[str, Any]) -> float:
        h = as_float(pick(payload, "hash_rate", "hashRate", "hashrate"), 0.0)
        best = as_float(pick(payload, "best_bits", "bestBits"), 0.0)
        target = as_float(pick(payload, "target_bits", "targetBits"), 22.0)
        rssi = as_float(pick(payload, "rssi", "rx_rssi", "worker_rssi"), -90.0)
        jitter = as_float(pick(payload, "loop_jitter_us"), 0.0)
        job_age = as_float(pick(payload, "job_age_s"), 0.0)
        conf = as_float(pick(payload, "knn_confidence", "confidence"), 0.0) / 100.0
        thermal = as_float(pick(payload, "thermal_load"), 0.0) / 100.0
        pred_err = abs(as_float(pick(payload, "prediction_error_x1000"), 0.0)) / 1000.0
        rssi_bonus = max(0.0, min(1.0, (rssi + 92.0) / 42.0))
        best_bonus = max(0.0, best - target + 1.0) * 220.0
        stable = max(0.0, 1.0 - min(1.0, jitter / 50000.0)) * 80.0
        fresh = max(0.0, 1.0 - min(1.0, job_age / 24.0)) * 55.0
        heat_penalty = thermal * 80.0
        error_penalty = pred_err * 70.0
        return round(h / 65.0 + best * 16.0 + best_bonus + rssi_bonus * 65.0 + stable + fresh + conf * 40.0 - heat_penalty - error_penalty, 3)

    def fitness_score(self, payload: Dict[str, Any]) -> float:
        """Tranception-style lightweight fitness proxy for swarm search policy.

        This is not a neural checkpoint. It is the NAS-side observer score that
        turns each worker/lane/sector attempt into a comparable sample.
        """
        h = as_float(pick(payload, "hash_rate", "hashRate", "hashrate"), 0.0)
        best = as_float(pick(payload, "best_bits", "bestBits"), 0.0)
        target = max(1.0, as_float(pick(payload, "target_bits", "targetBits"), 22.0))
        shares = as_float(pick(payload, "shares", "accepted"), 0.0)
        rejects = as_float(pick(payload, "rejects", "rejected", "pool_rejects", "poolR"), 0.0)
        stale = as_float(pick(payload, "stale", "stales", "stale_drops"), 0.0)
        rssi = as_float(pick(payload, "rssi", "rx_rssi", "worker_rssi"), -90.0)
        jitter = as_float(pick(payload, "loop_jitter_us"), 0.0)
        job_age = as_float(pick(payload, "job_age_s", "jobAgeS"), 0.0)
        batch = as_float(pick(payload, "dynamic_batch", "effective_batch", "batch", "batch_hint"), 0.0)
        thermal = as_float(pick(payload, "thermal_load", "heat"), 0.0)
        if thermal > 4.0:
            thermal = thermal / 100.0
        load = as_float(pick(payload, "load", "load_x1000"), 0.0)
        if load > 8.0:
            load = load / 1000.0

        z_margin = best - target
        tail_gain = max(0.0, z_margin + 1.0) * 260.0
        near_tail = max(0.0, best / target) * 120.0
        h_gain = min(420.0, h / 48.0)
        share_gain = shares * 520.0
        rssi_gain = max(0.0, min(1.0, (rssi + 92.0) / 42.0)) * 55.0
        stability_gain = max(0.0, 1.0 - min(1.0, jitter / 50000.0)) * 60.0
        fresh_gain = max(0.0, 1.0 - min(1.0, job_age / 30.0)) * 35.0
        batch_penalty = max(0.0, batch - 512.0) * 0.10
        reject_penalty = rejects * 230.0
        stale_penalty = stale * 95.0
        heat_penalty = max(0.0, thermal - 0.75) * 130.0 + max(0.0, load - 1.40) * 70.0
        score = h_gain + near_tail + tail_gain + share_gain + rssi_gain + stability_gain + fresh_gain
        score -= reject_penalty + stale_penalty + heat_penalty + batch_penalty
        return round(score, 3)

    def record_fitness_observation(
        self,
        db: sqlite3.Connection,
        ts: float,
        payload: Dict[str, Any],
        source: str,
        node_id: str,
        kind: str,
        raw: str,
    ) -> Dict[str, Any]:
        lane = str(pick(payload, "lane", "method", "method_id", "miner_lane", default="")).strip() or "unknown"
        strategy = str(pick(payload, "strategy", "strat", "plan", "mode", default="")).strip() or lane
        sector = as_int(pick(payload, "sector", "lane_id", "miner_sector", "arm", default=0), 0)
        batch = as_int(pick(payload, "dynamic_batch", "effective_batch", "batch", "batch_hint", default=0), 0)
        hash_rate = as_float(pick(payload, "hash_rate", "hashRate", "hashrate"), 0.0)
        best_bits = as_int(pick(payload, "best_bits", "bestBits"), 0)
        target_bits = as_int(pick(payload, "target_bits", "targetBits"), 22)
        shares = as_int(pick(payload, "shares", "accepted"), 0)
        rejects = as_int(pick(payload, "rejects", "rejected", "pool_rejects", "poolR"), 0)
        stale = as_int(pick(payload, "stale", "stales", "stale_drops"), 0)
        rssi = as_int(pick(payload, "rssi", "rx_rssi", "worker_rssi"), -127)
        job_age_s = as_float(pick(payload, "job_age_s", "jobAgeS"), 0.0)
        fitness = self.fitness_score(payload)

        cur = db.execute(
            """
            INSERT INTO fitness_observations(ts,node_id,kind,lane,sector,strategy,batch,fitness,hash_rate,
                                             best_bits,target_bits,shares,rejects,stale,rssi,job_age_s,payload)
            VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            """,
            (
                ts,
                node_id,
                kind,
                lane,
                sector,
                strategy,
                batch,
                fitness,
                hash_rate,
                best_bits,
                target_bits,
                shares,
                rejects,
                stale,
                rssi,
                job_age_s,
                raw,
            ),
        )
        obs = {
            "id": cur.lastrowid,
            "node_id": node_id,
            "kind": kind,
            "lane": lane,
            "sector": sector,
            "strategy": strategy,
            "batch": batch,
            "fitness": fitness,
            "best_bits": best_bits,
            "target_bits": target_bits,
            "hash_rate": hash_rate,
            "shares": shares,
            "rejects": rejects,
            "stale": stale,
            "source": source,
        }
        self.append_jsonl("fitness_observations.jsonl", {"ts": ts, **obs})
        return obs

    def fitness_recommendation(self, node_id: str = "", limit: int = 12) -> Dict[str, Any]:
        ts = now()
        params: List[Any] = []
        where = "WHERE ts>=?"
        params.append(ts - 24.0 * 3600.0)
        if node_id:
            where += " AND node_id=?"
            params.append(node_id)

        with self.connect() as db:
            rows = [dict(r) for r in db.execute(
                f"""
                SELECT node_id,lane,sector,strategy,
                       COUNT(*) n,
                       AVG(fitness) avg_fitness,
                       MAX(fitness) max_fitness,
                       AVG(hash_rate) avg_hash_rate,
                       MAX(best_bits) max_best_bits,
                       AVG(rejects) avg_rejects,
                       AVG(stale) avg_stale,
                       MAX(ts) last_ts
                FROM fitness_observations
                {where}
                GROUP BY node_id,lane,sector,strategy
                HAVING n>=1
                ORDER BY avg_fitness DESC,max_best_bits DESC,last_ts DESC
                LIMIT ?
                """,
                (*params, limit),
            )]
            recent = [dict(r) for r in db.execute(
                f"""
                SELECT ts,node_id,kind,lane,sector,strategy,batch,fitness,hash_rate,best_bits,target_bits,shares,rejects,stale,rssi
                FROM fitness_observations
                {where}
                ORDER BY ts DESC
                LIMIT ?
                """,
                (*params, limit),
            )]

        best = rows[0] if rows else None
        confidence = 0.0
        directive: Dict[str, Any] = {
            "intent": "observe",
            "focus": "fitness_landscape",
            "submit_pressure": "do_not_increase",
            "memory_tags": ["tranception_style", "fitness_observer"],
            "ttl_s": 60,
            "reason": "not enough fitness observations yet",
        }
        if best:
            n = int(best["n"])
            avg_fit = float(best["avg_fitness"])
            confidence = max(0.05, min(0.95, 0.15 + n * 0.06 + max(0.0, avg_fit) / 1800.0))
            batch_hint = 96
            if float(best["avg_hash_rate"]) > 8000 and int(best["max_best_bits"]) >= 22:
                batch_hint = 192
            if float(best["avg_hash_rate"]) > 18000 and int(best["max_best_bits"]) >= 24:
                batch_hint = 320
            if float(best["avg_rejects"]) > 1.0 or float(best["avg_stale"]) > 2.0:
                batch_hint = max(48, batch_hint // 2)
            directive.update({
                "intent": "fitness_hint",
                "focus": "nonce_order_bias",
                "node_id": best["node_id"],
                "lane_hint": best["lane"],
                "sector_hint": int(best["sector"]),
                "strategy_hint": best["strategy"],
                "batch_hint": batch_hint,
                "confidence": round(confidence, 3),
                "priority": 120 + int(confidence * 80),
                "reason": "retrieved best repeated lane/sector/strategy from NAS corpus",
            })

        return {
            "ok": True,
            "model": "janus_tranception_lite",
            "version": VERSION,
            "node_id": node_id,
            "confidence": round(confidence, 3),
            "directive": directive,
            "top": rows,
            "recent": recent,
            "rule": "observer-only: nonce order hints allowed; SHA header, target, S2 and submit pressure forbidden",
        }

    def infer_sketch(self, payload: Dict[str, Any], node_id: str, kind: str, role: str, source: str) -> str:
        explicit = pick(payload, "sketch", "firmware", "program", "app", "app_name", "fw_name", default="")
        if explicit:
            return str(explicit).strip()
        probe = " ".join([
            node_id,
            kind,
            role,
            source,
            str(pick(payload, "mode", "lane", "status", "target", default="")),
            compact_payload(payload, 1800),
        ]).lower()
        if "yaks" in probe or "klawyaks" in probe:
            return "Yaks_Gate"
        if "blackstar" in probe or "gargantua" in probe or "atom_bh" in probe or "bh-gpt" in probe:
            return "ATOM_BH"
        if "gladius" in probe or "gex" in probe:
            return "Gladius"
        if "anchor" in probe or "rfanchor" in probe:
            return "Anchor"
        if "zim" in probe:
            return "Zim"
        if "buzz" in probe or "stratum" in probe:
            return "Buzz"
        if "core2" in probe or "galaxy station" in probe:
            return "CORE2"
        if "blind" in probe or "eye" in probe:
            return "BLIND_EYE"
        if "cardputer" in probe or "adv" in probe or "elite" in probe:
            return "ADV_Elite"
        if "pyramid" in probe or "matrix" in probe:
            return "ATOM_MATRIX_Pyramid"
        return kind or role or "swarm_node"

    def infer_features(self, payload: Dict[str, Any], node_id: str, kind: str, role: str, source: str) -> List[str]:
        probe = " ".join([node_id, kind, role, source, compact_payload(payload, 3200)]).lower()
        features: List[str] = []

        def add_if(name: str, *tokens: str) -> None:
            if any(t in probe for t in tokens) and name not in features:
                features.append(name)

        add_if("miner", "miner", "hash", "best_bits", "target_bits", "stratum", "sha")
        add_if("esp_now", "esp-now", "espnow", "peerch", "janus", "swarm")
        add_if("blackstar", "blackstar", "gargantua", "bh-gpt", "horizon")
        add_if("mercury_time", "mercury", "torricelli", "torr", "void")
        add_if("pn_cortex", "p/n", "pn_", "pn-", "silicon", "junction")
        add_if("kenshi_bubble", "k2", "kenshi")
        add_if("ir", "ir", "infrared")
        add_if("lora_gnss", "lora", "gnss", "gps")
        add_if("env_sensor", "env", "sht", "qmp", "bme", "sgp", "temperature", "humidity")
        add_if("mic_audio", "mic", "audio", "echo")
        add_if("nas_brain", "nas", "brain")
        return features

    def _record_archivarius(
        self,
        db: sqlite3.Connection,
        ts: float,
        payload: Dict[str, Any],
        source: str,
        node_id: str,
        role: str,
        kind: str,
        raw: str,
        hash_rate: float,
        best_bits: int,
        target_bits: int,
        rssi: int,
        shares: int,
        rejects: int,
    ) -> Dict[str, Any]:
        sketch = self.infer_sketch(payload, node_id, kind, role, source)
        version = str(pick(payload, "firmware_version", "fw_version", "fw", "version", "build", "build_id", default="")).strip()
        features = self.infer_features(payload, node_id, kind, role, source)
        channel = as_int(pick(payload, "channel", "ch", "wifi_channel", default=0), 0)
        peer_channel = as_int(pick(payload, "peer_channel", "peerCh", "peer_ch", default=0), 0)
        wifi_raw = pick(payload, "wifi", "wifi_state", "wifiStatus", "wifi_status", default="")
        status_raw = pick(payload, "status", "mode", default="")
        wifi_state = str(wifi_raw if wifi_raw != "" else status_raw)
        tx_ok = as_int(pick(payload, "tx_ok", "txOk", "tx", "espnow_tx", "colony_tx", default=0), 0)
        tx_fail = as_int(pick(payload, "tx_fail", "txFail", "fail", "espnow_fail", "colony_fail", default=0), 0)
        rescue_count = as_int(pick(payload, "rescue_count", "rescues", "rebuilds", "radio_rebuilds", default=0), 0)
        if_resets = as_int(pick(payload, "if_resets", "ifReset", "if_reset_count", "iface_resets", default=0), 0)
        esp_err = as_int(pick(payload, "espnow_err", "esp_err", "last_esp_err", "err", default=0), 0)
        job_active = as_int(pick(payload, "job", "job_active", "active", default=0), 0)
        heap = as_int(pick(payload, "heap", "free_heap", "mem_free", default=0), 0)
        uptime_s = as_float(pick(payload, "uptime_s", "uptime", default=0.0), 0.0)
        mode = str(pick(payload, "mode", "lane", "status", default="")).strip()

        issues: List[str] = []
        issue_level = 0
        raw_low = raw.lower()
        wifi_low = wifi_state.lower()
        if esp_err == 12391 or "esp_err_espnow_if" in raw_low or "espnow_if" in raw_low:
            issues.append("esp-now-interface-mismatch")
            issue_level = max(issue_level, 3)
        if tx_fail > 20 and tx_fail > max(10, tx_ok):
            issues.append("tx-fail-streak")
            issue_level = max(issue_level, 3)
        if if_resets > 0 or rescue_count > 0:
            issues.append("radio-rescue-used")
            issue_level = max(issue_level, 1)
        if wifi_low in ("6", "wl_disconnected", "disconnected", "wifi_disconnected"):
            issues.append("wifi-disconnected")
            issue_level = max(issue_level, 2)
        if rssi and rssi < -85:
            issues.append("weak-rssi")
            issue_level = max(issue_level, 2)
        if job_active and "miner" in features and hash_rate <= 0:
            issues.append("miner-zero-hashrate")
            issue_level = max(issue_level, 2)
        if rejects > max(3, shares * 2) and rejects > 10:
            issues.append("reject-pressure")
            issue_level = max(issue_level, 2)

        issue = ",".join(issues)
        health = {
            "hash_rate": hash_rate,
            "best_bits": best_bits,
            "target_bits": target_bits,
            "shares": shares,
            "rejects": rejects,
            "rssi": rssi,
            "tx_ok": tx_ok,
            "tx_fail": tx_fail,
            "rescue_count": rescue_count,
            "if_resets": if_resets,
            "esp_err": esp_err,
            "job_active": job_active,
            "heap": heap,
            "uptime_s": uptime_s,
            "mode": mode,
        }
        health_raw = json.dumps(health, ensure_ascii=False, separators=(",", ":"))
        features_raw = json.dumps(features, ensure_ascii=False, separators=(",", ":"))

        existing = db.execute("SELECT first_seen, seen_count FROM firmware_registry WHERE node_id=?", (node_id,)).fetchone()
        first_seen = float(existing["first_seen"]) if existing else ts
        seen_count = int(existing["seen_count"]) + 1 if existing else 1
        db.execute(
            """
            INSERT INTO firmware_registry(node_id,sketch,firmware_version,role,kind,first_seen,last_seen,seen_count,
                                          channel,peer_channel,wifi_state,features,health,issue_level,issue,last_payload)
            VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(node_id) DO UPDATE SET
                sketch=excluded.sketch,
                firmware_version=excluded.firmware_version,
                role=excluded.role,
                kind=excluded.kind,
                last_seen=excluded.last_seen,
                seen_count=excluded.seen_count,
                channel=excluded.channel,
                peer_channel=excluded.peer_channel,
                wifi_state=excluded.wifi_state,
                features=excluded.features,
                health=excluded.health,
                issue_level=excluded.issue_level,
                issue=excluded.issue,
                last_payload=excluded.last_payload
            """,
            (
                node_id,
                sketch,
                version,
                role,
                kind,
                first_seen,
                ts,
                seen_count,
                channel,
                peer_channel,
                wifi_state,
                features_raw,
                health_raw,
                issue_level,
                issue,
                raw,
            ),
        )

        if issue_level >= 2:
            last = db.execute(
                "SELECT ts,message FROM swarm_incidents WHERE node_id=? AND incident_type=? ORDER BY ts DESC LIMIT 1",
                (node_id, "health"),
            ).fetchone()
            if not last or float(last["ts"]) < ts - 60.0 or str(last["message"]) != issue:
                db.execute(
                    "INSERT INTO swarm_incidents(ts,node_id,incident_type,severity,message,payload) VALUES(?,?,?,?,?,?)",
                    (ts, node_id, "health", issue_level, issue, raw),
                )
                self.append_jsonl("archivarius_incidents.jsonl", {
                    "ts": ts,
                    "node_id": node_id,
                    "severity": issue_level,
                    "message": issue,
                    "sketch": sketch,
                    "health": health,
                })

        return {"sketch": sketch, "version": version, "features": features, "issue_level": issue_level, "issue": issue, "health": health}

    def remember_node(self, payload: Dict[str, Any], source: str = "unknown") -> Dict[str, Any]:
        supplied_ms = timestamp_ms(pick(payload, "last_seen", "last_seen_ms", "ts", "timestamp"))
        ts = supplied_ms / 1000.0 if supplied_ms is not None else now()
        node_id = canonical_node_id(pick(payload, "node_id", "nodeId", "device_id", "id", "name", "node", default=source or "unknown"))
        kind = str(pick(payload, "kind", "role", "type", default="swarm")).strip()
        role = str(pick(payload, "role", "kind", default=kind)).strip()
        score = self.score_swarm_sense(payload)
        hash_rate = as_float(pick(payload, "hash_rate", "hashRate", "hashrate"), 0.0)
        best_bits = as_int(pick(payload, "best_bits", "bestBits"), 0)
        target_bits = as_int(pick(payload, "target_bits", "targetBits"), 0)
        shares = as_int(pick(payload, "shares", "accepted"), 0)
        rejects = as_int(pick(payload, "rejects", "rejected"), 0)
        rssi = as_int(pick(payload, "rssi", "rx_rssi", "worker_rssi"), -127)
        raw = compact_payload(payload)

        with self.connect() as db:
            existing = db.execute("SELECT node_id, first_seen, seen_count, score, best_bits FROM nodes WHERE node_id=?", (node_id,)).fetchone()
            first_seen = float(existing["first_seen"]) if existing else ts
            seen_count = int(existing["seen_count"]) + 1 if existing else 1
            keep_score = max(score, float(existing["score"])) if existing else score
            keep_best = max(best_bits, int(existing["best_bits"])) if existing else best_bits
            db.execute(
                """
                INSERT INTO nodes(node_id, role, kind, first_seen, last_seen, seen_count, hash_rate, best_bits,
                                  target_bits, shares, rejects, rssi, score, last_payload)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)
                ON CONFLICT(node_id) DO UPDATE SET
                    role=excluded.role,
                    kind=excluded.kind,
                    last_seen=excluded.last_seen,
                    seen_count=excluded.seen_count,
                    hash_rate=excluded.hash_rate,
                    best_bits=excluded.best_bits,
                    target_bits=excluded.target_bits,
                    shares=excluded.shares,
                    rejects=excluded.rejects,
                    rssi=excluded.rssi,
                    score=excluded.score,
                    last_payload=excluded.last_payload
                """,
                (node_id, role, kind, first_seen, ts, seen_count, hash_rate, keep_best, target_bits,
                 shares, rejects, rssi, keep_score, raw),
            )
            db.execute(
                """
                INSERT INTO telemetry(ts,node_id,kind,source,score,hash_rate,best_bits,target_bits,rssi,payload)
                VALUES(?,?,?,?,?,?,?,?,?,?)
                """,
                (ts, node_id, kind, source, score, hash_rate, best_bits, target_bits, rssi, raw),
            )
            arch = self._record_archivarius(
                db,
                ts,
                payload,
                source,
                node_id,
                role,
                kind,
                raw,
                hash_rate,
                keep_best,
                target_bits,
                rssi,
                shares,
                rejects,
            )
            fitness = self.record_fitness_observation(db, ts, payload, source, node_id, kind, raw)

        record = {"ts": ts, "node_id": node_id, "kind": kind, "source": source, "score": score, "payload": payload}
        self.append_jsonl("swarm_sense.jsonl", record)
        return {"node_id": node_id, "kind": kind, "score": score, "best_bits": keep_best, "hash_rate": hash_rate, "archivarius": arch, "fitness": fitness}

    def add_corpus(self, payload: Dict[str, Any], source: str = "api") -> Dict[str, Any]:
        ts = now()
        node_id = str(pick(payload, "node_id", "nodeId", "device_id", default="")).strip()
        topic = str(pick(payload, "topic", "kind", "type", default="general")).strip()
        text = pick(payload, "text", "content", "line", default=None)
        quality = as_float(pick(payload, "quality", "score", default=0.0), 0.0)
        tags = pick(payload, "tags", default=[])
        if not isinstance(tags, list):
            tags = [str(tags)]
        raw = compact_payload(payload, max_len=64000)
        with self.connect() as db:
            cur = db.execute(
                "INSERT INTO corpus(ts,source,node_id,topic,quality,text,payload,tags) VALUES(?,?,?,?,?,?,?,?)",
                (ts, source, node_id, topic, quality, text, raw, json.dumps(tags, ensure_ascii=False)),
            )
        self.append_jsonl("corpus.jsonl", {"ts": ts, "source": source, "node_id": node_id, "topic": topic, "quality": quality, "payload": payload})
        return {"id": cur.lastrowid if "cur" in locals() else None, "topic": topic, "quality": quality}

    def add_import(self, payload: Dict[str, Any], source: str = "quant") -> Dict[str, Any]:
        ts = now()
        kind = str(pick(payload, "payload_branch", "format", "kind", "type", default="import")).strip()
        meta = payload.get("meta") if isinstance(payload.get("meta"), dict) else {}
        quality = as_float(pick(payload, "score", "best_val", "quality", default=0.0), 0.0)
        if not quality and meta:
            quality = as_float(pick(meta, "score", "best_val", "val_loss", default=0.0), 0.0)
        raw = compact_payload(payload, max_len=256000)
        with self.connect() as db:
            cur = db.execute("INSERT INTO imports(ts,source,kind,quality,payload) VALUES(?,?,?,?,?)", (ts, source, kind, quality, raw))
        self.append_jsonl("imports.jsonl", {"ts": ts, "source": source, "kind": kind, "quality": quality, "payload": payload})
        return {"id": cur.lastrowid if "cur" in locals() else None, "kind": kind, "quality": quality}

    def add_event(self, payload: Dict[str, Any], source: str = "api") -> Dict[str, Any]:
        ts = now()
        event_type = str(pick(payload, "event_type", "type", "kind", default="event")).strip()
        node_id = str(pick(payload, "node_id", "device_id", default="")).strip()
        raw = compact_payload(payload)
        with self.connect() as db:
            cur = db.execute("INSERT INTO events(ts,event_type,source,node_id,payload) VALUES(?,?,?,?,?)", (ts, event_type, source, node_id, raw))
        self.append_jsonl("events.jsonl", {"ts": ts, "source": source, "event_type": event_type, "node_id": node_id, "payload": payload})
        return {"id": cur.lastrowid if "cur" in locals() else None, "event_type": event_type}

    def queue_command(self, device_id: str, command: Any) -> Dict[str, Any]:
        ts = now()
        raw = json.dumps(command, ensure_ascii=False)
        with self.connect() as db:
            cur = db.execute("INSERT INTO commands(ts,device_id,command,delivered) VALUES(?,?,?,0)", (ts, device_id, raw))
        return {"id": cur.lastrowid if "cur" in locals() else None, "device_id": device_id}

    def pop_command(self, device_id: str) -> Any:
        with self.connect() as db:
            row = db.execute(
                "SELECT id,command FROM commands WHERE device_id=? AND delivered=0 ORDER BY ts ASC LIMIT 1",
                (device_id,),
            ).fetchone()
            if not row:
                return None
            db.execute("UPDATE commands SET delivered=1, delivered_ts=? WHERE id=?", (now(), int(row["id"])))
            try:
                return json.loads(row["command"])
            except Exception:
                return row["command"]

    def latest_device(self, device_id: str) -> Optional[Dict[str, Any]]:
        with self.connect() as db:
            row = db.execute("SELECT * FROM nodes WHERE node_id=?", (device_id,)).fetchone()
            if not row:
                row = db.execute("SELECT * FROM telemetry WHERE node_id=? ORDER BY ts DESC LIMIT 1", (device_id,)).fetchone()
            if not row:
                return None
            result = dict(row)
            for key in ("last_payload", "payload"):
                if key in result:
                    try:
                        result[key] = json.loads(result[key])
                    except Exception:
                        pass
            return result

    def status(self) -> Dict[str, Any]:
        ts = now()
        with self.connect() as db:
            node_count = db.execute("SELECT COUNT(*) c FROM nodes").fetchone()["c"]
            online_count = db.execute("SELECT COUNT(*) c FROM nodes WHERE last_seen>=?", (ts - NODE_TTL_SEC,)).fetchone()["c"]
            tel_count = db.execute("SELECT COUNT(*) c FROM telemetry").fetchone()["c"]
            corpus_count = db.execute("SELECT COUNT(*) c FROM corpus").fetchone()["c"]
            fitness_count = db.execute("SELECT COUNT(*) c FROM fitness_observations").fetchone()["c"]
            top = [dict(r) for r in db.execute("SELECT node_id,kind,last_seen,hash_rate,best_bits,rssi,score FROM nodes ORDER BY score DESC,last_seen DESC LIMIT 8")]
        for r in top:
            r["age_s"] = round(ts - float(r["last_seen"]), 1)
            r["online"] = r["age_s"] <= NODE_TTL_SEC
        return {
            "service": APP_NAME,
            "version": VERSION,
            "time": ts,
            "db": str(self.db_path),
            "nodes": node_count,
            "online": online_count,
            "telemetry_rows": tel_count,
            "corpus_rows": corpus_count,
            "fitness_rows": fitness_count,
            "top": top,
        }

    def nodes(self, limit: int = 64) -> List[Dict[str, Any]]:
        ts = now()
        with self.connect() as db:
            rows = [dict(r) for r in db.execute(
                "SELECT node_id,role,kind,last_seen,seen_count,hash_rate,best_bits,target_bits,shares,rejects,rssi,score FROM nodes ORDER BY last_seen DESC LIMIT ?",
                (limit,),
            )]
        for r in rows:
            r["age_s"] = round(ts - float(r["last_seen"]), 1)
            r["online"] = r["age_s"] <= NODE_TTL_SEC
        return rows

    def remember_presence(self, payload: Dict[str, Any]) -> Dict[str, Any]:
        ts = now()
        node_id = canonical_node_id(pick(payload, "node_id", "id", "name", "node", default="PEA4"))
        safe = {
            "node_id": node_id,
            "role": str(payload.get("role", "p4_archivist"))[:64],
            "firmware": str(payload.get("firmware", ""))[:128],
            "observer_only": bool(payload.get("observer_only", True)),
            "sd_archiving": bool(payload.get("sd_archiving", False)),
            "wifi_rssi": as_int(payload.get("wifi_rssi"), -127),
        }
        raw = compact_payload(safe)
        with self.connect() as db:
            db.execute(
                """INSERT INTO pea4_presence(node_id,role,firmware,last_seen,observer_only,sd_archiving,wifi_rssi,last_payload)
                   VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(node_id) DO UPDATE SET role=excluded.role,
                   firmware=excluded.firmware,last_seen=excluded.last_seen,observer_only=excluded.observer_only,
                   sd_archiving=excluded.sd_archiving,wifi_rssi=excluded.wifi_rssi,last_payload=excluded.last_payload""",
                (node_id, safe["role"], safe["firmware"], ts, int(safe["observer_only"]),
                 int(safe["sd_archiving"]), safe["wifi_rssi"], raw),
            )
        record = {"server_time_ms": int(ts * 1000), "source": "presence", **safe}
        self.persist_latest("pea4_presence.jsonl", "pea4_presence_latest.json", record)
        return record

    def ingest_textcast(self, payload: Dict[str, Any]) -> Dict[str, Any]:
        accepted: List[Dict[str, Any]] = []
        ignored_count = 0
        ts = now()
        events = payload.get("events") if isinstance(payload.get("events"), list) else []
        for event in events[:128]:
            if not isinstance(event, dict):
                ignored_count += 1
                continue
            raw_ssid = str(event.get("raw_ssid", "")).strip()[:32]
            if not textcast_allowed(raw_ssid):
                ignored_count += 1
                continue
            node_id = canonical_node_id(event.get("node_id") or raw_ssid)
            status = str(event.get("status", "active"))[:32]
            role = str(event.get("role", "field_node"))[:64]
            clean = {
                "raw_ssid": raw_ssid,
                "node_id": node_id,
                "rssi": as_int(event.get("rssi"), -127),
                "channel": as_int(event.get("channel"), 0),
                "seen_ms": as_int(event.get("seen_ms"), 0),
                "status": status,
                "role": role,
            }
            raw = compact_payload(clean)
            with self.connect() as db:
                db.execute(
                    """INSERT INTO ssid_textcast(node_id,raw_ssid,last_seen,rssi,channel,status,role,last_payload)
                       VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(node_id) DO UPDATE SET raw_ssid=excluded.raw_ssid,
                       last_seen=excluded.last_seen,rssi=excluded.rssi,channel=excluded.channel,
                       status=excluded.status,role=excluded.role,last_payload=excluded.last_payload""",
                    (node_id, raw_ssid, ts, clean["rssi"], clean["channel"], status, role, raw),
                )
            accepted.append(clean)
        record = {
            "server_time_ms": int(ts * 1000),
            "source": "PEA4",
            "events": accepted,
            "accepted_count": len(accepted),
            "ignored_count": ignored_count,
        }
        self.persist_latest("ssid_textcast.jsonl", "ssid_textcast_latest.json", record)
        return record

    def nodes_payload(self, limit: int = 64, include_upstream: bool = True) -> Dict[str, Any]:
        server_time_ms = int(now() * 1000)
        merged: Dict[str, Dict[str, Any]] = {}

        def merge(raw: Dict[str, Any], source_default: str) -> None:
            node_id = canonical_node_id(pick(raw, "node_id", "id", "name", "node", default="unknown"))
            last_seen = timestamp_ms(pick(raw, "last_seen_ms", "last_seen", "ts", "timestamp"))
            status = str(pick(raw, "status", "label", default="online" if last_seen else "unknown"))
            role = str(pick(raw, "role", "kind", default="field_node"))
            sources = raw.get("source", [source_default])
            if isinstance(sources, str):
                sources = [sources]
            sources = [str(item) for item in sources if item]
            age_sec = None if last_seen is None else max(0, int((server_time_ms - last_seen) / 1000))
            fresh_live = age_sec is not None and age_sec <= LIVE_TTL_SEC and status.lower() in LIVE_STATUSES
            current = merged.get(node_id)
            if current is None:
                current = {
                    "node_id": node_id, "name": node_id, "role": role, "status": status,
                    "source": [], "last_seen_ms": last_seen, "age_sec": age_sec,
                    "rssi": as_int(raw.get("rssi"), -127), "channel": as_int(raw.get("channel"), 0),
                    "health": "live" if fresh_live else ("stale" if last_seen is not None else "unknown"),
                    "notes": "", "_live": fresh_live,
                }
                merged[node_id] = current
            if last_seen is not None and (current["last_seen_ms"] is None or last_seen >= current["last_seen_ms"]):
                current.update({"role": role, "status": status, "last_seen_ms": last_seen,
                                "age_sec": age_sec, "rssi": as_int(raw.get("rssi"), -127),
                                "channel": as_int(raw.get("channel"), 0)})
            current["source"] = sorted(set(current["source"] + sources))
            current["_live"] = bool(current["_live"] or fresh_live)
            current["health"] = "live" if current["_live"] else (
                "stale" if current["last_seen_ms"] is not None else "unknown")

        upstream = {"ok": False, "error": "upstream_disabled", "nodes": []}
        if include_upstream and self.upstream_url:
            upstream = self.upstream_json("/api/swarm/nodes")
            for raw in upstream.get("nodes", []) if isinstance(upstream.get("nodes"), list) else []:
                if isinstance(raw, dict):
                    merge(raw, "nas")

        with self.connect() as db:
            local_rows = [dict(row) for row in db.execute(
                "SELECT node_id,role,kind,last_seen,rssi,last_payload FROM nodes ORDER BY last_seen DESC LIMIT ?",
                (limit,),
            )]
            presence_rows = [dict(row) for row in db.execute(
                "SELECT node_id,role,firmware,last_seen,wifi_rssi FROM pea4_presence")]
            textcast_rows = [dict(row) for row in db.execute(
                "SELECT node_id,role,status,last_seen,rssi,channel FROM ssid_textcast")]
        for row in local_rows:
            payload = safe_json_loads(str(row.pop("last_payload", "{}")), {})
            if isinstance(payload, dict):
                row.update({key: payload[key] for key in ("status", "label", "channel") if key in payload})
            merge(row, "nas")
        for row in presence_rows:
            row.update({"status": "active", "rssi": row.pop("wifi_rssi", -127)})
            merge(row, "presence")
        for row in textcast_rows:
            merge(row, "ssid_textcast")

        nodes = sorted(merged.values(), key=lambda item: (not item["_live"], -(item["last_seen_ms"] or 0)))[:limit]
        for node in nodes:
            node.pop("_live", None)
        live_count = sum(1 for node in nodes if node["health"] == "live")
        result = {
            "ok": True,
            "server_time_ms": server_time_ms,
            "live_count": live_count,
            "known_count": len(nodes),
            "nodes": nodes,
            "upstream_ok": bool(upstream.get("ok")) if self.upstream_url else True,
        }
        if self.upstream_url and not upstream.get("ok"):
            result["error"] = str(upstream.get("error", "upstream_timeout"))
        self.persist_latest("swarm_nodes.jsonl", "swarm_nodes_latest.json", result)
        return result

    def health_payload(self) -> Dict[str, Any]:
        nodes = self.nodes_payload(limit=256, include_upstream=True)
        return {
            "ok": True,
            "service": "nas_brain",
            "upstream_ok": bool(nodes.get("upstream_ok")),
            "known_count": int(nodes.get("known_count", 0)),
            "live_count": int(nodes.get("live_count", 0)),
            "last_update_ms": int(nodes.get("server_time_ms", int(now() * 1000))),
        }

    def archivarius(self, limit: int = 64) -> Dict[str, Any]:
        ts = now()
        with self.connect() as db:
            rows = [dict(r) for r in db.execute(
                """
                SELECT node_id,sketch,firmware_version,role,kind,first_seen,last_seen,seen_count,
                       channel,peer_channel,wifi_state,features,health,issue_level,issue
                FROM firmware_registry
                ORDER BY issue_level DESC,last_seen DESC
                LIMIT ?
                """,
                (limit,),
            )]
            incidents = [dict(r) for r in db.execute(
                """
                SELECT ts,node_id,incident_type,severity,message
                FROM swarm_incidents
                ORDER BY ts DESC
                LIMIT 24
                """
            )]

        nodes: List[Dict[str, Any]] = []
        summary = {"online": 0, "stale": 0, "lost": 0, "degraded": 0, "total": len(rows)}
        for row in rows:
            age_s = round(ts - float(row["last_seen"]), 1)
            level = int(row["issue_level"])
            issue = str(row["issue"] or "")
            state = "online"
            if age_s > NODE_TTL_SEC * 4.0:
                state = "lost"
                level = max(level, 3)
                issue = issue or "presence-lost"
            elif age_s > NODE_TTL_SEC:
                state = "stale"
                level = max(level, 2)
                issue = issue or "presence-stale"
            elif level >= 2:
                state = "degraded"
            summary[state] = summary.get(state, 0) + 1

            health = safe_json_loads(str(row["health"] or "{}"), {})
            features = safe_json_loads(str(row["features"] or "[]"), [])
            nodes.append({
                "node_id": row["node_id"],
                "sketch": row["sketch"],
                "firmware_version": row["firmware_version"],
                "role": row["role"],
                "kind": row["kind"],
                "state": state,
                "age_s": age_s,
                "seen_count": row["seen_count"],
                "channel": row["channel"],
                "peer_channel": row["peer_channel"],
                "wifi_state": row["wifi_state"],
                "features": features,
                "health": health,
                "issue_level": level,
                "issue": issue,
            })

        actions: List[str] = []
        if any("esp-now-interface-mismatch" in str(n.get("issue", "")) for n in nodes):
            actions.append("Apply safe-send IF-reset wrapper: peer rebuild alone is not enough for ESP_ERR_ESPNOW_IF.")
        if any(n["state"] in ("stale", "lost") for n in nodes):
            actions.append("Keep stale nodes in roster and request radio-rescue logs before reflashing unrelated modules.")
        if any("miner-zero-hashrate" in str(n.get("issue", "")) for n in nodes):
            actions.append("Check job-active zero-hashrate watchdog before changing miner strategy.")
        if not actions:
            actions.append("No critical swarm fault detected by Archivarius.")

        return {
            "service": APP_NAME,
            "version": VERSION,
            "time": ts,
            "ttl_s": NODE_TTL_SEC,
            "summary": summary,
            "nodes": nodes,
            "recent_incidents": incidents,
            "actions": actions,
            "rule": "archive and diagnose only; never change pool truth or bot_hub",
        }

    def archivarius_report(self, limit: int = 64) -> Dict[str, Any]:
        data = self.archivarius(limit=limit)
        lines = [
            f"Archivarius {data['version']} nodes={data['summary']['total']} online={data['summary'].get('online', 0)} stale={data['summary'].get('stale', 0)} lost={data['summary'].get('lost', 0)} degraded={data['summary'].get('degraded', 0)}"
        ]
        for node in data["nodes"][:12]:
            health = node.get("health", {})
            lines.append(
                f"{node['node_id']} {node['sketch']} {node['state']} age={node['age_s']}s "
                f"H={health.get('hash_rate', 0)} best={health.get('best_bits', 0)} "
                f"ch={node.get('channel', 0)}/{node.get('peer_channel', 0)} issue={node.get('issue', '') or '-'}"
            )
        return {"ok": True, "report": "\n".join(lines), "archivarius": data}

    def library(self, topic: str = "", node_id: str = "", limit: int = 16) -> Dict[str, Any]:
        with self.connect() as db:
            clauses = []
            params: List[Any] = []
            if topic:
                clauses.append("topic LIKE ?")
                params.append(f"%{topic}%")
            if node_id:
                clauses.append("node_id=?")
                params.append(node_id)
            where = ("WHERE " + " AND ".join(clauses)) if clauses else ""
            rows = [dict(r) for r in db.execute(
                f"SELECT id,ts,source,node_id,topic,quality,text,tags FROM corpus {where} ORDER BY quality DESC,ts DESC LIMIT ?",
                (*params, limit),
            )]
            top_nodes = [dict(r) for r in db.execute(
                "SELECT node_id,kind,hash_rate,best_bits,rssi,score FROM nodes ORDER BY score DESC,last_seen DESC LIMIT 8"
            )]
        return {"topic": topic, "node_id": node_id, "corpus": rows, "top_nodes": top_nodes, "directive": self.directive(node_id=node_id)}

    def directive(self, node_id: str = "") -> Dict[str, Any]:
        with self.connect() as db:
            target = None
            if node_id:
                target = db.execute("SELECT * FROM nodes WHERE node_id=?", (node_id,)).fetchone()
            top = db.execute("SELECT * FROM nodes ORDER BY score DESC,last_seen DESC LIMIT 1").fetchone()
        if not target and top:
            target = top
        if not target:
            return {"mode": "observe", "message": "library empty", "submit_pressure": "normal"}

        score = float(target["score"])
        best = int(target["best_bits"])
        h = float(target["hash_rate"])
        batch_hint = 96
        if h > 8000 and best >= 22:
            batch_hint = 192
        if h > 18000 and best >= 24:
            batch_hint = 320
        if int(target["rssi"]) < -82:
            batch_hint = max(48, batch_hint // 2)
        return {
            "mode": "observe",
            "node_id": target["node_id"],
            "method": target["kind"],
            "score": round(score, 3),
            "best_bits": best,
            "hash_rate": h,
            "batch_hint": batch_hint,
            "submit_pressure": "do_not_increase",
            "rule": "library-only: never fake shares, never change pool target",
        }


class JanusHandler(BaseHTTPRequestHandler):
    server_version = f"{APP_NAME}/{VERSION}"

    @property
    def brain(self) -> BrainDB:
        return self.server.brain  # type: ignore[attr-defined]

    def log_message(self, fmt: str, *args: Any) -> None:
        if os.environ.get("JANUS_NAS_BRAIN_QUIET", "0") == "1":
            return
        super().log_message(fmt, *args)

    def send_json(self, payload: Any, status: int = 200) -> None:
        body = json_dumps(payload)
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def read_json(self) -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
        length = as_int(self.headers.get("Content-Length"), 0)
        if length <= 0:
            return {}, None
        if length > MAX_BODY_BYTES:
            return None, f"payload too large: {length} bytes"
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode("utf-8", errors="replace"))
        except Exception as exc:
            return None, f"invalid json: {exc}"
        if not isinstance(data, dict):
            return None, "json object expected"
        return data, None

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type,Authorization")
        self.end_headers()

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        query = parse_qs(parsed.query)

        if path == "/api/health":
            self.send_json(self.brain.health_payload())
            return

        if path in ("/", "/ping", "/health", "/api/status", "/api/swarm/status"):
            self.send_json({"ok": True, **self.brain.status()})
            return

        if path in ("/api/face", "/api/face/mold", "/api/nas/face", "/api/swarm/voice/mold"):
            self.send_json({"ok": True, "face": face_card()})
            return

        if path == "/api/swarm/nodes":
            self.send_json(self.brain.nodes_payload(limit=as_int(query.get("limit", [64])[0], 64)))
            return

        if path in ("/api/swarm/archivarius", "/api/swarm/firmware"):
            self.send_json({"ok": True, **self.brain.archivarius(limit=as_int(query.get("limit", [64])[0], 64))})
            return

        if path == "/api/swarm/archivarius/report":
            self.send_json(self.brain.archivarius_report(limit=as_int(query.get("limit", [64])[0], 64)))
            return

        if path == "/api/swarm/library":
            topic = query.get("topic", [""])[0]
            node_id = query.get("node_id", query.get("node", [""]))[0]
            limit = as_int(query.get("limit", [16])[0], 16)
            self.send_json({"ok": True, **self.brain.library(topic=topic, node_id=node_id, limit=limit)})
            return

        if path in ("/api/swarm/fitness", "/api/swarm/tranception"):
            node_id = query.get("node_id", query.get("node", [""]))[0]
            limit = as_int(query.get("limit", [12])[0], 12)
            self.send_json(self.brain.fitness_recommendation(node_id=node_id, limit=limit))
            return

        if path.startswith("/api/device/latest/"):
            device_id = path.split("/", 4)[-1]
            latest = self.brain.latest_device(device_id)
            if latest is None:
                self.send_json({"ok": False, "error": "not found", "device_id": device_id}, status=404)
            else:
                self.send_json({"ok": True, "device_id": device_id, "latest": latest})
            return

        if path.startswith("/api/device/command/"):
            device_id = path.split("/", 4)[-1]
            command = self.brain.pop_command(device_id)
            self.send_json({"command": command})
            return

        self.send_json({"ok": False, "error": "not found", "path": path}, status=404)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        payload, err = self.read_json()
        if err:
            self.send_json({"ok": False, "error": err}, status=400)
            return
        assert payload is not None

        if path == "/api/swarm/sense":
            result = self.brain.remember_node(payload, source=str(payload.get("source", "buzz")))
            upstream = self.brain.upstream_json(path, method="POST", payload=payload) if self.brain.upstream_url else {"ok": True, "local_only": True}
            self.send_json({"ok": True, "accepted": True, "result": result, "upstream": upstream,
                            "directive": self.brain.directive(result["node_id"]),
                            "fitness": self.brain.fitness_recommendation(result["node_id"], limit=8)})
            return

        if path == "/api/swarm/presence":
            self.brain.remember_presence(payload)
            self.send_json({"ok": True, "accepted": True})
            return

        if path == "/api/swarm/textcast":
            result = self.brain.ingest_textcast(payload)
            self.send_json({"ok": True, "accepted": True, "accepted_count": result["accepted_count"],
                            "ignored_count": result["ignored_count"]})
            return

        if path in ("/api/swarm/heartbeat", "/api/swarm/telemetry", "/api/device/data"):
            data = payload.get("data") if isinstance(payload.get("data"), dict) else payload
            if "device_id" in payload and "device_id" not in data:
                data = dict(data)
                data["device_id"] = payload["device_id"]
            result = self.brain.remember_node(data, source=str(payload.get("source", data.get("source", "device"))))
            if path == "/api/device/data":
                self.brain.add_event({"type": "device_data", "device_id": result["node_id"], "data": data}, source="device")
            self.send_json({"ok": True, "status": "ok", "result": result, "directive": self.brain.directive(result["node_id"]), "fitness": self.brain.fitness_recommendation(result["node_id"], limit=8)})
            return

        if path in ("/api/swarm/archivarius/checkin", "/api/swarm/firmware"):
            result = self.brain.remember_node(payload, source=str(payload.get("source", "archivarius")))
            self.brain.add_event({"type": "archivarius_checkin", "node_id": result["node_id"], "data": payload}, source="archivarius")
            self.send_json({"ok": True, "status": "ok", "result": result, "archivarius": self.brain.archivarius(limit=64)})
            return

        if path in ("/api/swarm/fitness", "/api/swarm/tranception"):
            result = self.brain.remember_node(payload, source=str(payload.get("source", "fitness")))
            self.send_json({"ok": True, "status": "ok", "result": result, "fitness": self.brain.fitness_recommendation(result["node_id"], limit=12)})
            return

        if path in ("/api/swarm/corpus", "/api/memory/add"):
            result = self.brain.add_corpus(payload, source="memory" if path == "/api/memory/add" else "swarm")
            self.send_json({"ok": True, "status": "ok", "result": result})
            return

        if path in ("/api/swarm/event", "/api/hrain/event"):
            result = self.brain.add_event(payload, source="event")
            self.send_json({"ok": True, "status": "ok", "result": result})
            return

        if path in ("/api/face/reply", "/api/swarm/voice"):
            text = str(payload.get("text", payload.get("message", "")))
            mode = str(payload.get("mode", "swarm"))
            context = payload.get("context") if isinstance(payload.get("context"), dict) else {"status": self.brain.status()}
            result = build_face_reply(text, context=context, mode=mode)
            event = {"type": "swarm_voice", "text": text, "mode": mode, "speech": result["speech"], "directive": result["directive"]}
            self.brain.add_event(event, source="face")
            relay = relay_to_bot_hub(event)
            self.send_json({"ok": True, "status": "ok", **result, "bot_hub_relay": relay})
            return

        if path == "/api/quant/import":
            result = self.brain.add_import(payload, source="quant")
            node_payload = {
                "node_id": str(pick(payload, "device_id", default=pick(payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}, "device_id", default="jgpt_quant"))),
                "kind": "quant_import",
                "best_bits": 0,
                "hash_rate": 0,
                "score": result.get("quality", 0),
            }
            self.brain.remember_node(node_payload, source="quant")
            self.send_json({"ok": True, "status": "ok", "result": result})
            return

        if path == "/api/device/command":
            device_id = str(payload.get("device_id", "")).strip()
            command = payload.get("command")
            if not device_id or command is None:
                self.send_json({"ok": False, "error": "device_id and command required"}, status=400)
                return
            result = self.brain.queue_command(device_id, command)
            self.send_json({"ok": True, "status": "ok", "result": result})
            return

        self.send_json({"ok": False, "error": "not found", "path": path}, status=404)


def build_server(host: str, port: int, data_dir: Path, upstream_url: str = "",
                 upstream_timeout_ms: int = UPSTREAM_TIMEOUT_MS) -> ThreadingHTTPServer:
    class Server(ThreadingHTTPServer):
        daemon_threads = True

    server = Server((host, port), JanusHandler)
    server.brain = BrainDB(data_dir, upstream_url=upstream_url,
                           upstream_timeout_ms=upstream_timeout_ms)  # type: ignore[attr-defined]
    return server


def main() -> None:
    parser = argparse.ArgumentParser(description="JANUS NAS Brain swarm library node")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--data", default=str(DEFAULT_DATA_DIR))
    args = parser.parse_args()

    data_dir = Path(args.data).resolve()
    server = build_server(args.host, args.port, data_dir, upstream_url=UPSTREAM_URL,
                          upstream_timeout_ms=UPSTREAM_TIMEOUT_MS)
    print(f"[{APP_NAME}] v{VERSION} listening on http://{args.host}:{args.port}")
    print(f"[{APP_NAME}] data: {data_dir}")
    print(f"[{APP_NAME}] Buzz endpoint: POST /api/swarm/sense")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print(f"\n[{APP_NAME}] stop")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
