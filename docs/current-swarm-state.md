# Current JANUS Swarm State

Last updated: 2026-08-02.

## Node Roles

| node | hardware | role | notes |
| --- | --- | --- | --- |
| Core2 | M5Stack Core2 | Station / galaxy authority | Black-hole lab, galaxy UI, SD corpus, swarm telemetry, slow miner study |
| Buzz | ESP32-S3 class | Pool master / arbiter | Stratum authority, worker distribution, Brother Arena, audio/UI |
| BH / BlackStar | AtomS3R class | Black-hole observer | Gargantua/BlackStar visualization, BH corpus, silicon trace and lens telemetry |
| ADV Elite | Cardputer ADV | Ship / sky-anchor | Elite game node, Beacon functions, LoRa/GNSS anchor, SD corpus |
| Yaks Gate | StickS3 | Gate pilot | IR local optical beacon, Murph/maze/BlackStar escape data, NAS fallback |
| Anchor | ESP32-S3 | Stable brother | v1.20 Buzz S/2 worker, callback-safe ESP-NOW receive queue, discovery-ping handling, Gladius twin split, RF Dome and direct Buzz recovery |
| Gladius | ESP32-S3 | Experimental brother | TailGEX/bandit miner, direct Buzz protection, method scout |
| Golcron / Holocron | TTGO T-Display ESP32 | Astrolabe worker / visual charm | v1.5 adaptive non-overlapping nonce slices, autonomous Star Forge path learning, S/2 submit path and BH pixel-cosmos display |
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
- Anchor v1.20 uses Buzz `S/2`, a deferred receive queue and direct/twin recovery without changing pool semantics.
- Golcron starts mining automatically after a real Buzz `J/B` range arrives. It learns between disjoint untouched slices and never intentionally resets progress when planning the next path.
- Golcron's BH-style pixel cosmos, screen state and telemetry frame are UI only; they do not alter mining truth.
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

Golcron's adaptive learner is inside this boundary: it chooses the order of
non-overlapping slices and a coprime local traversal, but it does not reinterpret
work already checked.

Forbidden without an explicit protocol migration:

- changing SHA256/SHA256d math
- changing pool target interpretation
- changing header bytes other than the assigned nonce field
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
