# Beacon Preserve Policy

`firmware/beacon/Beacon_A1.ino` is the focused Beacon entrypoint and preservation source for the future `ADV_Elite` rebuild.

The preserve rule means the public firmware should not be reduced until behavior has been checked on hardware. Before simplifying, identify which code is responsible for:

- beacon state;
- ESP-NOW packet exchange;
- UI or indicator behavior;
- timing loops;
- memory or telemetry paths;
- optional sensor or audio paths.

When changing Beacon behavior, document removed functions and packet changes in the audit files.

## ADV_Elite Specialist Adoption

The future `ADV_Elite` target must also inherit the generalizable Zim specialist doctrine defined in:

- [`AUTONOMOUS_SPECIALIST_DOCTRINE.md`](AUTONOMOUS_SPECIALIST_DOCTRINE.md)
- [`ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`](ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md)

This means the rebuilt ADV is not a generic obedient worker. Its protected primary mission is local truthful observation/prediction of `WORLD + SWARM + SELF`, Witness/memory and trusted swarm participation.

Buzz/mining/archive/uplink work is bounded side-work unless a later explicit architecture decision changes that classification.

Preserve these implementation requirements during the rebuild:
- primary mission survives Buzz/Core/NAS loss;
- optional work degrades before sensors/anomaly/Witness/local cognition;
- accelerated paths require reference equivalence + benchmark + fallback;
- learning is bounded around immutable truth/protocol/user-control semantics;
- useful learned state may persist with versioning and flash-wear guards;
- autonomy never means loss of swarm heartbeat/health/provenance visibility.

This policy does not resolve the still-separate Q23 question of automatic learned-candidate promotion.
