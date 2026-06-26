# NAS Brain API Map

`nas_brain/janus_nas_brain.py` exposes a lightweight HTTP API for swarm memory.

Read/status:

- `GET /api/health`
- `GET /`, `/ping`, `/health`, `/api/status`, `/api/swarm/status`
- `GET /api/swarm/nodes`
- `GET /api/swarm/archivarius`
- `GET /api/swarm/archivarius/report`
- `GET /api/swarm/library`
- `GET /api/swarm/tranception`
- `GET /api/swarm/fitness`
- `GET /api/device/latest/<id>`
- `GET /api/device/command/<id>`

Write/ingest:

- `POST /api/swarm/sense`
- `POST /api/swarm/presence`
- `POST /api/swarm/textcast`
- `POST /api/swarm/heartbeat`
- `POST /api/swarm/telemetry`
- `POST /api/device/data`
- `POST /api/swarm/archivarius/checkin`
- `POST /api/swarm/tranception`
- `POST /api/swarm/fitness`
- `POST /api/swarm/corpus`
- `POST /api/memory/add`
- `POST /api/swarm/event`
- `POST /api/hrain/event`
- `POST /api/face/reply`
- `POST /api/swarm/voice`
- `POST /api/quant/import`
- `POST /api/device/command`

Runtime outputs include node state, swarm sense, presence, TextCast, corpus, and incident memory. Keep those runtime files out of Git.
