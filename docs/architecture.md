# Architecture

JANUS LastSwarm is organized as a set of ESP32/M5 firmware roles that can share state through ESP-NOW and, where a sketch supports it, Wi-Fi or Stratum protocol paths.

## Node Roles

- `CORE2.ino`: high-density display/control surface for swarm state.
- `Buzz.ino`: audio, worker, and ESP-NOW coordination node.
- `BLIND_EYE.ino`: sensing and presence-calibration node.
- `Beacon_A1.ino`: beacon-style state and preserve candidate.
- `Stick.ino`: mobile ESP32 swarm node.
- `ATOM_SWARM_TRON.ino`: Atom-class local/remote worker and telemetry node.
- `ATOM_MATRIX_Pyramid.ino`: Atom Matrix visual/swarm node.
- `Zim.ino`: ESP32-S3 Geek autonomous specialist with real solo work, bounded Buzz side-work, swarm reports and local adaptive state.
- `ADV_Elite`: predictive observer/terminal specialist for WORLD + SWARM + SELF, with Beacon lineage, Witness, trusted swarm participation and bounded technical side-work.

## Autonomous Specialist Architecture

JANUS is a heterogeneous collective, not an army of identical obedient workers.

Every upgraded node follows the doctrine in [`AUTONOMOUS_SPECIALIST_DOCTRINE.md`](AUTONOMOUS_SPECIALIST_DOCTRINE.md):

```text
PROTECTED PRIMARY MISSION
        +
BOUNDED SIDE-QUESTS
        +
LOCAL AUTONOMY
        +
SHARED TRUSTED STATE
        =
DISTRIBUTED SPECIALIST COGNITION
```

Core rules:
- every node declares a protected `PRIMARY_MISSION`;
- Buzz/Core/NAS work is explicitly primary or bounded side-work;
- side-work may not silently starve the node's specialization;
- new acceleration paths are promoted only after reference equivalence and benchmark gain;
- learning occurs around an immutable truth core inside a bounded safe policy space;
- useful learning may persist across reboot with versioning and flash-wear guards;
- autonomy does not remove the node from heartbeat, health, provenance or trusted swarm telemetry.

`ZimGeek` is the reference experiment that motivated this doctrine. Its current behavior is preserved rather than normalized.

`ADV_Elite` adopts the doctrine through [`ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`](ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md).

## Data Flow

1. Sensors and local firmware state produce current observations.
2. Local state machines update UI, telemetry, prediction error, and control hints.
3. The node protects its primary mission and allocates only bounded resources to side-work.
4. ESP-NOW packets distribute selected state between nodes.
5. UI nodes render swarm state without treating memory or prediction as current presence.
6. Stratum-aware sketches keep protocol events separate from UI rewards and local simulations.
7. Learned policy may change safe scheduling/weights, but never immutable sensor/protocol/user-control truth.

## Compatibility Rule

ESP-NOW packet structures are treated as an ABI. When mixing firmware generations, verify struct size, packing, version fields, and packet type bytes before flashing a swarm.
