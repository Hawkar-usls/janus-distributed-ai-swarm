# Elite GitHub Revision Notes

Source repository checked historically:

- `Hawkar-usls/ATOM-ELITE-1989---M5stack-ATOMS3R-2026-`
- Local reference clone: `LOCAL_PATH_REDACTED`

Historical refs / checkpoints retained for provenance:

- `cefb508`: `ATOM-ELITE-v8.0.4 - Janus.ino`
- `6fe66b3`: `ATOM ELITE - JANUS ATMOSPHERE PACK v8.0.6.ino`
- `4ad2ece`: same atmosphere pack line, tagged as `v7.6.1`, `v10.1`, `Remastered`, `Remastered_Elite_v10`
- `63bab7b`: `ATOM_ELITE_JANUS_v8_6_MAXFPS_ECHO_COSMOSIM.cpp`
- `aef4502`: current historical main at review time; deleted the v8.6 cosmosim file, tag `8.8rr`

## 2026-08-27 Architecture Supersession

The previous plan to restore/expand Elite gameplay is superseded for the new `ADV_Elite` target.

The current `firmware/adv_elite/ADV_Elite.ino` game branch is retained as a **technical donor / historical lineage source**, not as the cognition base for the rebuilt ADV.

Remove from the future target:
- galaxy/flight/station/docking gameplay;
- pirates/enemies/weapons/combat;
- ore/cargo/credits/economy/factions;
- hyperspace/missions/cockpit game-state;
- game-only PilotLink/state dependencies unless a non-game technical dependency is explicitly proven.

Preserve as audited technical donor organs where useful:
- A9 miner and exact share-validity logic;
- Buzz jobs/shares and corpus persistence;
- ESP-NOW recovery;
- real-only ENV guards;
- GNSS;
- CAP-LoRa/SX1262;
- Sky Anchor technical state;
- SwarmSense/AI/E2 telemetry;
- IMU calibration;
- brightness/LED hardware UX where compatible.

## Autonomous Specialist Adoption

The rebuilt ADV immediately adopts the Zim-derived specialist doctrine:

```text
PRIMARY_MISSION + BOUNDED_SIDE_QUESTS
SELF_TESTED_ACCELERATION_ONLY
QUIET_CANARY_RESOURCE_HIERARCHY
IMMUTABLE_TRUTH_CORE + LEARNABLE_POLICY_AROUND_IT
PERSISTENT_LEARNING_WITH_FLASH_WEAR_GUARDS
AUTONOMY_WITHOUT_INFORMATION_ISOLATION
```

Canonical references:
- `docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md`
- `docs/ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`

ADV primary mission is local truthful predictive observation of `WORLD + SWARM + SELF`, Witness/memory, always-on anomaly attention and trusted swarm participation.

Buzz/mining/archive/uplink work is bounded side-work unless a later explicit architecture decision changes that classification.

## Preserve / Rebuild Boundary

`Beacon_A1.ino` remains the primary cognitive/preservation lineage for the rebuilt ADV; the Elite game branch is donor-only.

Do not raw-port autonomous game control, station flow, missions, combat or economy into the target.

Do not use this document to decide Q23 automatic learned-candidate promotion; that remains a separate architecture gate.
