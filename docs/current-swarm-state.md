# Current JANUS Swarm State

Last updated: 2026-06-17.

## Node Roles

| node | hardware | role | notes |
| --- | --- | --- | --- |
| Core2 | M5Stack Core2 | Station / galaxy authority | Black-hole lab, galaxy UI, SD corpus, swarm telemetry, slow miner study |
| Buzz | ESP32-S3 class | Pool master / arbiter | Stratum authority, worker distribution, Brother Arena, audio/UI |
| BH / BlackStar | AtomS3R class | Black-hole observer | Gargantua/BlackStar visualization, BH corpus, silicon trace and lens telemetry |
| ADV Elite | Cardputer ADV | Ship / sky-anchor | Elite game node, Beacon functions, LoRa/GNSS anchor, SD corpus |
| Yaks Gate | StickS3 | Gate pilot | IR local optical beacon, Murph/maze/BlackStar escape data, NAS fallback |
| Anchor | ESP32-S3 | Stable brother | Baseline miner, direct Buzz heartbeat/share protection |
| Gladius | ESP32-S3 | Experimental brother | TailGEX/bandit miner, direct Buzz protection, method scout |
| Zim | ESP32-S3 Geek | Scout | Solo/worker scout, Stratum-aware node, Tranception-lite candidate |
| Blind Eye | sensor node | Eye / motion memory | Presence, motion, memory mirror, swarm status source |
| Pyramid | Atom Matrix | Stable visual node | Preserved stable pyramid firmware |
| Beacon A1 | Cardputer/launcher image | Legacy Beacon | Preserve candidate and fallback image |
| PEA4 | ESP32-P4 + companion ESP32-S3 | Titan terminal | Large UI, camera/sensor preprocessing, archive, NAS edge, observer-only |

## Current Integration Threads

- P/N Cortex telemetry is the preferred cross-node summary layer.
- BlackStar/BH exports lens/study/heat/load style signals to Core2 and Yaks.
- Core2 treats Gargantua Lab as a dedicated galaxy tab/study target.
- Yaks Gate uses BH/BlackStar guidance and IR only as local optical signaling.
- Anchor and Gladius must not disappear from Buzz/Core2 after transient radio loss.
- Buzz must tolerate disconnect/reconnect churn without dropping to permanent zero hash.
- ADV Elite must publish real sensor values only; no fake ENV placeholders.
- PEA4 is staged as a large terminal and observer, not a pool authority.

## Swarm Visibility Requirement

Every active firmware should implement, or be migrated toward, these behaviors:

- periodic heartbeat with stable node name, kind and firmware version
- direct Buzz recovery path where applicable
- channel reassertion after ESP-NOW/Wi-Fi disruption
- stale-peer TTL instead of permanent removal
- serial logs that identify node, role, job state, hash rate, best bits and radio status
- no fake sensor values; mark absent data as absent/stale

## Mining Boundary

Allowed adaptive choices:

- nonce order
- start nonce
- stride
- lane/method id
- sector/window selection
- batch size / local compute pressure
- local corpus selection

Forbidden without an explicit protocol migration:

- changing SHA256/SHA256d math
- changing pool target interpretation
- submitting fake/reject pressure to the pool
- increasing submit pressure from observer-only nodes
- mixing sensor prediction with current sensor truth

## PEA4 Scope

PEA4 currently has two firmware tracks:

- `firmware/pea4/PEA4.ino`: serial/NVS/P/N observer pool probe.
- `firmware/PEA4_JANUS_SHELL/PEA4_JANUS_SHELL.ino`: display/touch shell v0.1.
- `firmware/p4_dual_swarm_core/JANUS_P4_DUAL_SWARM_CORE_v1_1.ino`:
  full P4_A/P4_B/P4_C compute/protocol core with JP4 frames, mirror
  verification, anchor memory, blackboard/intention scores and NVS state.

Stock firmware is preserved as local flash backups and is not committed here.
The Janus shell rebuilds stock-like functions incrementally from source.
