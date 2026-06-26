# Buzz Master Node

`Buzz.ino` is the sanitized public copy of the current Buzz master sketch.

Buzz is the swarm master node: it owns the pool-facing Stratum connection, validates local and ESP-NOW remote shares before relay, publishes jobs to worker nodes, maintains audio/UI behavior, and forwards observer telemetry to NAS Brain.

Public placeholders:

- `YOUR_WIFI_SSID`
- `YOUR_WIFI_PASSWORD`
- `YOUR_NAS_HOST`
- `YOUR_NAS_PORT`
- `YOUR_POOL_HOST_HERE`
- `YOUR_WALLET_HERE`

Safety boundary:

- Buzz may distribute work to ESP-NOW workers.
- Buzz verifies remote shares before pool relay.
- NAS/SwarmSense/Tranception/Blackboard layers are observer or advisory only.
- Do not let NAS output rewrite SHA headers, pool target checks, share validity, submit pressure, or worker nonce ownership.
