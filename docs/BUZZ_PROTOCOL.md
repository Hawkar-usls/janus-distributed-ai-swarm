# Buzz Protocol Notes

Buzz is the master node for the current home swarm.

Observed protocol layers in the current sketch:

- Stratum pool connection and watchdogs.
- `J/B` job distribution to ESP-NOW workers.
- `S/2` and legacy share response handling.
- Remote share verification before Stratum relay.
- `S/S` SwarmSense intake and NAS forwarding.
- `J/E` blackboard semantic events.
- `J/P` policy packets from Home Cortex style controllers.
- `O/X` Oxytokin enzyme packets for own/foreign swarm markers and social telemetry.
- `G/M` Gladius TailGEX memory packets as observe/diagnostic input.

Safety boundary:

```text
SHA/header/target/share validity/submit pressure = frozen protocol truth.
NAS/Buzz ecology, audio, UI, blackboard, Tranception and GEX = observer/advisory layers only.
```

Tranception is currently a placeholder pull from NAS Brain. Buzz may read compact hints, but those hints must not change SHA headers, pool target comparison, accepted/rejected accounting, Stratum behavior, or mirror control.
