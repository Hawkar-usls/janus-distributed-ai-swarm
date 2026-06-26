# A18 JANUS NAS Swarm Bridge Plan

Proposed next module:

```text
A18_JANUS_NAS_SWARM_BRIDGE
```

Purpose:

```text
Bridge Buzz/NAS/ESP32 swarm telemetry into HRain as an observer-only home-swarm layer.
```

Allowed inputs:

- Buzz uptime and health.
- ESP-NOW worker roster.
- Wi-Fi health and RSSI.
- Thermal and watchdog warnings.
- Hash telemetry and best-tail telemetry.
- Accepted/rejected remote share counters.
- Audio pressure and UI pressure.
- Oxytokin trust/stress/dopamine telemetry.
- Blackboard events.
- PEA4 presence/camera/TextCast signals.

Forbidden without a separate explicit phase:

- Changing submit pressure.
- Changing SHA/header/target/share validation.
- Giving NAS direct miner control.
- Mixing Tranception or bio telemetry into PoW scoring.
- Claiming mining profit or a proven Janus advantage.

Implementation sketch:

```text
NAS Brain writes sanitized swarm summaries
  -> A17/HRain sidecar reads summaries
  -> HRain renders home-swarm layer
  -> A14/A17 miner remains untouched
```
