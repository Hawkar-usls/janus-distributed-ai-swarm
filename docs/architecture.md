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
- `Zim.ino`: ESP32-S3 Geek node with swarm reports and local adaptive state.

## Data Flow

1. Sensors and local firmware state produce current observations.
2. Local state machines update UI, telemetry, prediction error, and control hints.
3. ESP-NOW packets distribute selected state between nodes.
4. UI nodes render swarm state without treating memory or prediction as current presence.
5. Stratum-aware sketches keep protocol events separate from UI rewards and local simulations.

## Compatibility Rule

ESP-NOW packet structures are treated as an ABI. When mixing firmware generations, verify struct size, packing, version fields, and packet type bytes before flashing a swarm.
