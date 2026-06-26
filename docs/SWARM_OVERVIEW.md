# JANUS ESP32/M5 Swarm Overview

This repository contains sanitized firmware and NAS-side support code for the JANUS home swarm.

Current actor model:

- Buzz: main/master swarm node, Stratum-facing verifier, ESP-NOW job broadcaster, audio/UI node.
- ESP32/M5 workers: worker, sensor, display, cockpit, and observer nodes.
- PEA4: Titan/camera/presence observer candidate when present.
- NAS Brain: API, long memory, node roster, TextCast, presence, health, and Tranception placeholder.
- PC/A17 HRain: miner plus observer-only visual brain.

The safe data path is:

```text
ESP32/M5 swarm
  -> NAS Brain
  -> sanitized summaries
  -> HRain sidecar
  -> UI/reporting only
```

Mining truth remains separate. NAS and HRain must not mutate SHA, Stratum, target checks, submit behavior, mirror control, or worker nonce ownership.
