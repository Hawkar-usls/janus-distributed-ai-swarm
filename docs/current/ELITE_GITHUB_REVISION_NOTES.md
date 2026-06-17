# Elite GitHub Revision Notes

Source repository checked:

- `Hawkar-usls/ATOM-ELITE-1989---M5stack-ATOMS3R-2026-`
- Local reference clone:
  `LOCAL_PATH_REDACTED`

Found refs / useful checkpoints:

- `cefb508`: `ATOM-ELITE-v8.0.4 - Janus.ino`
- `6fe66b3`: `ATOM ELITE - JANUS ATMOSPHERE PACK v8.0.6.ino`
- `4ad2ece`: same atmosphere pack line, tagged as `v7.6.1`, `v10.1`, `Remastered`, `Remastered_Elite_v10`
- `63bab7b`: `ATOM_ELITE_JANUS_v8_6_MAXFPS_ECHO_COSMOSIM.cpp`
- `aef4502`: current `main`, deleted the v8.6 cosmosim file, tag `8.8rr`

Safe ideas to reuse after professional review:

- Procedural galaxy seed with compact system data.
- Commodity market: food/textiles/liquor/luxuries/computers/machinery/alloys/medicine.
- Equipment progression: fuel scoop, galactic hyperdrive, cargo expansion, military laser, docking computer, ECM.
- Classic mission skeleton: Constrictor hunt, Thargoid documents, witchspace ambush.
- Docking phases: approach, align, tunnel, docked.
- Ship model tables: Cobra, Mamba, Transport, Viper, Sidewinder, Thargoid, Thargon, Constrictor, Adder, Asp, Python, Anaconda.
- Save blocks: pilot, learn, agent, best agent.
- Death memory / learning-pilot data as future Core2 autopilot material.

Do not raw-port without audit:

- `janusAutopilot` / agent decision loop.
- `stationFlowActive`, station timers, and auto service sequence.
- Jump / dock / mission transitions.
- Any code path that changes UI mode or ship control without explicit player/autopilot consent.

Already reintroduced safely into current Cardputer Elite:

- `S/S` SwarmSense telemetry as `CardputerElite / EliteBeaconA9`.
- `P/L` PilotLink telemetry so Core2 sees the Cardputer ship as a single pilot unit in the shared galaxy.

Next audited port candidates:

- First: ship model tables and station docking phases.
- Second: market/equipment progression.
- Third: mission skeleton.
- Last: Core2-controlled autopilot, only behind explicit ARM/ON mode with manual override always winning.
