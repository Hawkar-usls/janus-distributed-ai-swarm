# Current JANUS Swarm State

Last updated: 2026-08-27.

## Node Roles

| node | hardware | role | notes |
| --- | --- | --- | --- |
| Core2 | M5Stack Core2 | Station / galaxy authority | Black-hole lab, galaxy UI, SD corpus, swarm telemetry, slow miner study |
| Buzz | ESP32-S3 class | Pool master / arbiter | Stratum authority, worker distribution, Brother Arena, audio/UI |
| BH / BlackStar | AtomS3R class | Black-hole observer | Gargantua/BlackStar visualization, BH corpus, silicon trace and lens telemetry |
| ADV Elite | Cardputer ADV | Predictive observer / sky-anchor specialist | Beacon preservation target, WORLD+SWARM+SELF local cognition, Witness, LoRa/GNSS technical organs, bounded swarm/miner/uplink side-work |
| Yaks Gate | StickS3 | Gate pilot | IR local optical beacon, Murph/maze/BlackStar escape data, NAS fallback |
| Anchor | ESP32-S3 | Stable brother | v1.20 Buzz S/2 worker, callback-safe ESP-NOW receive queue, discovery-ping handling, Gladius twin split, RF Dome and direct Buzz recovery |
| Gladius | ESP32-S3 | Experimental brother | TailGEX/bandit miner, direct Buzz protection, method scout |
| Golcron / Holocron | TTGO T-Display ESP32 | Astrolabe worker / visual charm | v1.5 adaptive non-overlapping nonce slices, autonomous Star Forge path learning, S/2 submit path and BH pixel-cosmos display |
| Zim | ESP32-S3 Geek | Autonomous specialist / scout | Solo Stratum primary mission, bounded Buzz lazy work, local White Raven/bandit memory, Home Cortex advisory input, continued swarm visibility |
| Blind Eye | AtomS3R + STHS34PF80 TMOS/PIR | TMOS-primary eye / motion memory / Buzz worker | v2.15A camera-absent hardware profile, 15 Hz TMOS truth, 900 ms anti-flicker hold, RF/IMU/mic fusion, synthetic E/F aperture, Blackboard/Kenshi/Tachyon |
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
- Blind Eye v2.15A treats the physically missing camera as a normal hardware profile and uses TMOS/PIR as its primary eye.
- Blind Eye separates `clear` from `artifact`: a valid empty room must approach `clear=1` and must not accumulate an artifact/ghost score.
- Blind Eye `E/F` frames are synthetic thermal-aperture maps from real scalar TMOS/RF telemetry, not camera pixels.
- Blind Eye waits in `SEEK` with `H=0` until a real Buzz `J/B` assignment arrives; this is a normal idle state.
- Buzz must tolerate disconnect/reconnect churn without dropping to permanent zero hash.
- Zim is intentionally preserved as an autonomous specialist rather than normalized into an obedient worker.
- Zim's observed Buzz leaderboard leadership motivated the repository-wide `PRIMARY_MISSION + BOUNDED_SIDE_QUESTS` doctrine; this is an architecture observation, not a mining-superiority claim.
- ADV_Elite adopts the generalizable Zim properties immediately at architecture level: protected primary mission, bounded side-work, Quiet Canary resource hierarchy, self-tested acceleration, bounded local learning, persistent learning state and autonomy without swarm isolation.
- ADV Elite must publish real sensor values only; no fake ENV placeholders.
- PEA4 is staged as a large terminal and observer, not a pool authority.

## Autonomous Specialist Requirement

Every active firmware should be migrated toward the rules in [`AUTONOMOUS_SPECIALIST_DOCTRINE.md`](AUTONOMOUS_SPECIALIST_DOCTRINE.md):

- declare a protected `PRIMARY_MISSION`;
- classify Buzz/Core/NAS work as primary or bounded side-work;
- define measurable side-work budgets;
- preserve the primary mission if coordinator services disappear;
- degrade optional work first under resource pressure;
- restrict online learning to a bounded safe policy around immutable truth;
- persist useful learning with versioning and flash-wear guards;
- remain visible to trusted peers even when local policy declines or defers side-work;
- validate new acceleration paths against a trusted reference before promotion.

ADV-specific binding: [`ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`](ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md).

## Swarm Visibility Requirement

Every active firmware should implement, or be migrated toward, these behaviors:

- periodic heartbeat with stable node name, kind and firmware version
- direct Buzz recovery path where applicable
- channel reassertion after ESP-NOW/Wi-Fi disruption
- stale-peer TTL instead of permanent removal
- serial logs that identify node, role, job state, hash rate, best bits and radio status
- no fake sensor values; mark absent data as absent/stale
- side-work defer/refusal state should remain visible where the node supports autonomous budgeting

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
