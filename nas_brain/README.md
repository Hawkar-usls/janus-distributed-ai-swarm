# JANUS NAS Brain

`janus_nas_brain.py` is the lightweight NAS-side swarm memory and API node.

It is not a miner, not a Stratum proxy, and not a firmware flashing tool. It stores swarm telemetry, node presence, TextCast/SSID observations, Archivarius reports, corpus entries, and compact Tranception-style fitness hints.

Primary endpoints:

- `GET /api/health`
- `GET /api/swarm/nodes`
- `POST /api/swarm/sense`
- `GET|POST /api/swarm/tranception`
- `POST /api/swarm/presence`
- `POST /api/swarm/textcast`
- `POST /api/swarm/heartbeat`
- `POST /api/swarm/telemetry`
- `POST /api/device/data`
- `GET /api/device/latest/<id>`
- `GET|POST /api/device/command`
- `POST /api/memory/add`
- `POST /api/hrain/event`

Runtime JSONL/database outputs belong on the NAS or local runtime folder and must not be committed.

Safe Janus/HRain use:

```text
ESP32/M5 swarm
  -> NAS Brain
  -> sanitized swarm_state / swarm_nodes / swarm_sense summaries
  -> A17/HRain sidecar observer layer
  -> miner remains untouched
```
