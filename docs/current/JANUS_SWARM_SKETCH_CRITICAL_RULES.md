# JANUS Swarm Sketch Critical Rules

This file is the baseline contract for every Janus swarm sketch. If a new sketch or patch breaks one of these rules, it is not ready to flash.

This document supersedes the older local rule/mold files such as `node_BIO.txt`, `JANUS_MICROGPT_ESP32_BASE v0.1.txt`, and older pasted swarm prompts. Those files remain historical notes only. The preserved useful idea from them is simple: every node must boot alone, run alone, learn/cache locally, sync opportunistically, and never block its sensor/UI/game/miner loop because NAS, WiFi, Buzz, Core2, or another peer is unavailable.

## 1. Radio Rescue Is Mandatory

Every ESP-NOW swarm node must be able to return to the swarm without a manual reboot.

Required behavior:
- Track TX success/fail counters.
- Track last RX time from Buzz/Core2/any swarm authority.
- Track ESP-NOW broadcast peer channel.
- Detect peer missing, channel mismatch, RX blackout, and TX-fail streak.
- Treat `ESP_ERR_ESPNOW_IF`, `ESP_ERR_ESPNOW_CHAN`, `ESP_ERR_ESPNOW_NOT_INIT`, and `ESP_ERR_ESPNOW_NOT_FOUND` as radio-interface faults, not as ordinary send misses.
- Active ESP-NOW sketches should send through a local safe-send wrapper, not scattered direct `esp_now_send()` calls.
- On failure, perform a guarded ESP-NOW rescue:
  - `esp_now_deinit()`
  - `esp_now_init()`
  - re-register RX/TX callbacks
  - recreate broadcast peer on the current WiFi channel
  - send a short burst of heartbeat, SwarmSense, and PN/Cortex packets
- If `ESP_ERR_ESPNOW_IF` repeats, peer rebuild alone is not enough. Rebuild the WiFi STA interface calmly, disable WiFi sleep, deinit/init ESP-NOW, recreate the broadcast peer, and pause ordinary WiFi reconnect attempts for a few seconds so both systems do not fight.
- Do not reboot the ESP unless the sketch has a very long guarded blackout policy and no safer recovery remains.

Healthy logs should include something like:

```text
[COLONY/RESCUE] reason=tx-fail-streak ...
[COLONY/IF-RESET] reason=... err=12391 ...
[COLONY] peer ready ch=10 ...
```

## 2. Alive Means Visible

If a device is alive locally, the swarm must keep receiving a presence signal from it.

Minimum presence packets:
- `JANUS` heartbeat for old worker tables.
- `S/S` SwarmSense for Buzz/Core2/NAS telemetry.
- `P/N` PN-Cortex for p-n/silicon/labyrinth language.
- Specialized packet only if needed, never as the only presence path.

BH, Gladius, Anchor, Zim, Yaks, ADV, Core2, Buzz, BlindEye and future P4 nodes must all obey this.

## 3. SHA And Pool Truth Stay Frozen

Swarm AI can choose order, lane, stride, sector, batch, visual meaning and corpus hints.

It must not corrupt:
- pool job header
- target comparison
- nonce submission format
- accepted/rejected accounting
- S2 share packet truth
- Stratum protocol correctness

Experimental methods are observer or scheduler layers. They are not allowed to fake shares.

## 4. No Fake Sensors

Never send placeholder sensor values as real telemetry.

Rules:
- If ENV/SHT/QMP/BPS/IMU/mic data is not fresh, mark it stale or absent.
- Do not use `17`, `0`, random fallback, miner heat, or game heat as fake temperature/humidity/pressure.
- UI may display `--`, `stale`, or `hold`.
- Packets must carry flags that tell the receiver whether data is real and fresh.

ADV/Cardputer must keep real-only `A9FieldSense` style telemetry separate from game/miner telemetry.

## 5. Semantic Identity Must Not Collide

Node names and roles must map cleanly.

Examples:
- `A9FieldSense` is real ADV sensor telemetry.
- `CardputerElite` is game/pilot telemetry.
- `blackstar_bh` is BH/Gargantua.
- `anchor_pn_lab` is Anchor.
- `gladius_pn_lab` is Gladius.
- `yaks_gate` is Yaks Gate.

Do not reuse Beacon/Cardputer/ADV names for unrelated fake packets. It can overwrite fixed slots on Core2 and make real nodes appear broken.

## 6. UI Must Show State, Not Hide It

If a subsystem is unhealthy, show it as degraded, shadow, stale, or recovering.

Do not make it disappear instantly unless it was never known.

Recommended visibility:
- Fresh: normal color.
- Recently stale: dim/shadow with age.
- Long stale: move to archive/known roster, not active roster.
- Recovering radio: show `RESCUE` or `RADIO`.

## 7. Controls Must Stay Predictable

Common terminal interaction:
- Arrow-style navigation where available.
- `Enter` activates/toggles selected row.
- `Esc` closes/back/exits modal windows.
- Tabs switch sections.
- Avoid hidden direct hotkeys that conflict with text/game input.

Stick/Yaks style:
- Top hold controls brightness.
- Ten quick top taps toggle local IR.
- Short top/blue taps may pulse or interact, but must not change brightness.

## 8. Device Must Keep Working If NAS Is Gone

NAS is a brain/library, not a life-support dependency.

Rules:
- HTTP timeouts must be short.
- NAS failure must not block screen, mining, radio, controls, or local game loop.
- Use circuit breakers after repeated NAS failures.
- Log NAS result as `ok`, `fail`, `cooldown`, or `skip`.

## 9. Preserve Local Function While Adding Game Layer

Game visuals are not allowed to delete device utility.

Examples:
- BH remains black hole visual plus miner plus mic/telemetry plus PN-Cortex.
- Core2 remains station, galaxy, BH lab, policy/blackboard and miner observer.
- Yaks remains gate visual plus miner observer plus IR beacon.
- ADV remains Elite/game pilot plus real sensor bridge plus LoRa/GNSS future anchor.
- Anchor and Gladius remain miners/labs even when their visuals or roles evolve.

## 10. Every Sketch Needs Health Lines

Each node should periodically print one compact health line with:
- node name/version
- WiFi channel
- ESP-NOW peer channel
- RX age
- TX ok/fail
- rescue count
- hash rate
- best bits / target bits
- current lane/mode
- heap if useful

These lines are the first thing to check before changing code.

## 11. Compile And Flash Contract

Before giving a sketch for flashing:
- Sync the edited copy to `LOCAL_SKETCHBOOK_PATH` when that is the Arduino IDE target.
- Compile with the correct board FQBN and `--libraries LOCAL_SKETCHBOOK_PATH`.
- If compile times out, check whether `arduino-cli` is still running before assuming failure.
- Report exact flash target and expected first boot log markers.

## 12. Swarm Language Is Additive

The p-n/silicon/labyrinth language, Kenshi bubble, Murph signal, BH corpus, and Yaks gate signs are swarm semantics.

They should ride on top of existing packets:
- `P/N` for silicon/body/labyrinth metrics.
- `K/2` for Kenshi bubble state.
- `T/P` for prophecy/time hints.
- `S/S` for ordinary device telemetry.

They must not replace basic heartbeat and visibility.

## 13. A9.11 IO Layer Is Canon

`RBLGANUL_A9_11_V32_ACTIVE_TRIUNE_SOVEREIGN_GATE_50_50_IO_SINGLE.py` is a current design source for swarm mining behavior.

Use it as doctrine, not as a file to blindly port line-for-line to ESP32.

Preserved rules:
- zbits are rarity logs, not SHA direction claims.
- TruthGate: submit only pool-reconstructable canonical work.
- StaleGuard: if a cleaner newer job exists, drop the old candidate instead of submitting it.
- Reconnect old-round candidates go to observer logs, not to pool.
- Wire/header/nonce/extranonce/target comparison remain frozen.
- Active logic may change scheduler phase, lane pressure, or batch pressure only inside safe local policy.

## 14. Corpus Split Is Mandatory

Accepted proof corpus and dark-tail corpus are separate things.

Accepted corpus:
- accepted pool shares
- reproducible header/nonce/job proof
- best_z and accepted tail buckets
- safe to use as positive memory

Dark-tail corpus:
- pool rejected shares
- stale-guard drops
- reconnect old-round drops
- locally strong candidates that must not be submitted
- safe to study, never safe to spam into the pool

ESP32 storage should use bounded files on SD or LittleFS where available, with rotation:
- `accepted_corpus`
- `tail_events`
- `witchhunter_dark_tail`
- `stride_memory`
- `best_brain`

If storage is missing, keep a small RAM ring and report `corpus=ram`.

## 15. Red Blue Gold Gate

A9.11 SovereignGate maps cleanly onto swarm behavior:

- Red / Trickster: exploration pressure, rare-tail hunger, new lane/scout/rescout.
- Blue / Shadow: recovery pressure, reject/stale/radio/cooldown, back off and repair.
- Gold / Sovereign: frozen wire, stability, hold, low-reject confidence.

On ESP32 this must be represented as telemetry and safe scheduler hints:
- `P/N` should carry the current face/gate/lane pressure where possible.
- `S/S` should carry health, stale, rescue and corpus counters.
- Buzz/Core2 may display or arbitrate the face.
- No face is allowed to fake shares or alter Stratum truth.

## 16. 50/50 Experiments Need Clean Accounting

When comparing two methods, preserve the experiment boundary:
- same pool job
- disjoint nonce ranges
- same target
- same submit rules
- separate counters for accepted, rejected, stale, best_z, H/s, jitter and radio health

Mirror/control lanes must stay untouched by the JANUS active gate. If the active gate changes something, mark exactly which half it changed.

Brother Arena, Zim, Gladius, Anchor, Yaks and future P4 must log enough to reproduce:
- worker_id
- method/lane/sector
- job_id/job_seq
- start_nonce/range
- best_z
- accepted/rejected/stale
- radio rescue count
- corpus counter

## 17. Brother Oxytocin And Torricelli Bond

Gladius and Anchor are the silicon twin brothers. Their p-n language is mandatory, and their `oxytocin`/Torricelli layer is a real scheduler motive, not just lore.

Rules:
- `oxytocin` may boost local batch, lane confidence, TailGEX/stride learning, story pressure and UI/state flags.
- It rises from fresh twin contact, same Buzz job, clean radio, best_z progress, and valid shares.
- It falls when the brother disappears, radio TX fails, or stale pressure grows.
- Torricelli vacuum/pressure is the stable bridge metaphor: it may be sent in `P/N`, `S/S`, logs and corpus labels.
- It must never mutate SHA headers, targets, S2 packet truth, accepted/rejected accounting, or Stratum protocol.

Expected healthy markers:

```text
[ANCHOR/PN] ... oxy=.. vac=..
[ANCHOR/STATUS] ... oxy=.. vac=..
[GLADIUS/PN] ... oxy=.. vac=..
[TAILGEX/RWD] reason=torricelli_oxy ...
```

## 18. BlackStar Mercury Time

BH/BlackStar owns the mercury-time field.

Rules:
- `mercuryTorr` may use real pressure when the sensor value is sane; otherwise hold a neutral 760 torr model.
- `torricelliVoid`, `mercuryTime`, `hawkingVapor` and `horizonBalance` are semantic physics for BH/Yaks/Core2/PN-Cortex.
- They may influence BlackStar scheduler-only lane selection, local nonce order, UI, story state, P/N fields and blackboard events.
- They must not mutate SHA headers, target comparison, S2 share packets, accepted/rejected accounting or Stratum protocol.

Expected healthy markers:

```text
[BLACKSTAR/MERCURY] tx Hg=.. void=.. time=.. vapor=.. balance=..
[BLACKSTAR/GPT] ... Hg=.. void=.. time=..
[PN/CORTEX] ... Hg=.. void=.. time=..
[TD] ... Hg=.. void=.. time=..
```

## 19. Yaks Mercury Reverse Warp

Yaks Gate is allowed to escape BlackStar only through a short reverse-warp window.

Rules:
- The warp may be armed only by fresh BH mercury-time telemetry plus fresh Anchor and Gladius readiness.
- Anchor readiness comes from p-n telemetry/oxytocin/vacuum and means "hold the Torricellian void".
- Gladius readiness comes from p-n telemetry/oxytocin/vacuum and means "cut the reverse lane".
- Saved state may remember the last field, but must not relaunch warp after reboot without fresh packets.
- StickS3 still transmits only local 38 kHz IR optical sigils; it must not use LoRa/RF.
- MercuryWarp may influence Yaks visual, K2 flags, P/N telemetry, local IR sigil content, corpus labels and game state.
- MercuryWarp must not mutate SHA headers, targets, S2 packets, accepted/rejected accounting, pool submit pressure or Stratum truth.

Expected healthy markers:

```text
[YAKS/WARP] launch reason=... Hg=... A=... G=...
[YAKS] ... Hg=... warp=1/... A=... G=...
Yaks P/N flags include bit5 when MercuryWarp is armed/active.
K2 worldFlags include MERCURY/BROTHERS/WARP bits when active.
```

## 20. NAS Archivarius Registry

`janus_nas_brain` may act as Archivarius: a library-side dispatcher ledger for firmware identity, telemetry history and failure diagnosis.

Rules:
- Archivarius is not Buzz, not Stratum, not the pool, and not a forced game server.
- It may store node firmware/sketch/version, last seen time, features, radio health, miner health and incident history.
- It may expose `/api/swarm/archivarius`, `/api/swarm/archivarius/report`, and `/api/swarm/archivarius/checkin`.
- It may recommend flash order or recovery action from observed symptoms.
- It must not touch `bot_hub` unless the user explicitly asks for relay wiring.
- It must not increase submit pressure, fake shares, alter target truth, or block local node behavior if NAS is offline.

Every network-capable node should eventually be able to report a compact check-in:

```text
node_id, sketch, firmware_version, kind, hash_rate, best_bits, target_bits,
channel, peer_channel, tx_ok, tx_fail, rescue_count, if_resets, last_err
```

If Archivarius reports `esp-now-interface-mismatch`, update that sketch with the radio safe-send/IF-reset pattern before adding unrelated features.

## 21. Chaos Director Is Observer-First

`JANUS_CHAOS_DIRECTOR_MECHANICS.md` defines the swarm's controlled-chaos layer.

Rules:
- Chaos Director records small perturbations and consequences as branch/outcome memory.
- It starts observer-only.
- It may later tune nonce order, lane, sector, batch boundary, route, UI/game consequence and corpus tags.
- Anchor remains the baseline/control branch.
- Gladius and BH may receive tiny `chaos_epsilon` scout pressure after observer logs prove stable.
- It must never mutate pool job truth, SHA header, target comparison, S2 packets, accepted/rejected accounting or Stratum behavior.

Healthy future markers:

```text
[CHAOS] node=... branch=... eps=... lane=... score=...
[CHAOS/OUTCOME] branch=... best=... stale=... reject=... radio=...
[CHAOS/ROLLBACK] branch=... reason=...
```

## 22. Metaphor Must Become Math

Janus swarm language may use images such as BlackStar, Murph, mercury time, labyrinth, p-n light, Yaks Gate or Butterfly Director, but implementation must always translate them into measurable mechanics.

Rules:
- Treat every poetic phrase as a design shorthand, not as a physics claim.
- Before coding, map it to variables, packets, counters, thresholds, state machines, scheduler policy, corpus labels or UI.
- If a claim cannot be measured, reproduced, bounded or logged, keep it as visual/lore text only.
- Prefer terms such as entropy, state transition, signal, noise, correlation, counterfactual probe, feedback, hysteresis, cooldown, confidence, error rate and repeatability.
- Never present coincidences, rare hashes, p-n/IR effects, radio behavior or sensor noise as proof of hidden causality.
- Do not add mystical theory to logs, docs or code comments. Logs should show data, not belief.
- Safe swarm sentence pattern: "metaphor -> measurable proxy -> allowed influence -> forbidden influence".

Example:

```text
"Silicon flashes guide Yaks" ->
proxy: heat/load/hashrate/bestBits/IR pulse cadence/PN flags ->
allowed: visual, corpus tag, lane hint, cooldown, gate charge ->
forbidden: fake share, fake target, changed SHA header, fake sensor value
```

## 23. Buzz Visibility Is Not Generic RX

A node may be alive, hashing, hearing siblings and still be invisible to Buzz. Any swarm sketch that must appear in the Buzz worker table needs a separate master-freshness path.

Rules:
- Do not treat arbitrary ESP-NOW RX as proof that Buzz can see the node.
- Track fresh Buzz evidence separately: `J/B` job/discovery packet, Buzz `JANUS` master heartbeat, or targeted agent/reward packet.
- If `masterAge` exceeds the node limit, trigger `master-blackout` rescue even when sibling packets are still arriving.
- A rescue must rebuild/verify ESP-NOW peer/channel and immediately send a presence burst.
- Presence burst should include the node's main heartbeat plus its compact telemetry families, such as `JANUS`, `S/S`, `P/N`, sibling race packet and any node-specific memory packet.
- Keep a cooldown so rescue does not spam the radio.
- This rule must not alter SHA header, target comparison, S2 packets, accepted/rejected accounting, pool submit pressure or Stratum behavior.

Healthy markers:

```text
[NODE/PRESENCE] burst=... reason=boot|master-blackout|radio-rescue masterAge=...
[NODE/RADIO/RESCUE] reason=master-blackout ...
```

## 24. Tranception-Style Fitness Belongs On NAS

`OATML-Markslab/Tranception` is a useful design pattern for fitness landscapes and retrieval-guided scoring, but the real model is too heavy for ESP32-class nodes. Use it as a NAS-side observer concept, not as firmware.

Rules:
- `janus_nas_brain` owns the Tranception-style layer.
- ESP32 nodes send measurable attempts: node, lane, sector, strategy, batch, hash_rate, best_bits, target_bits, shares, rejects, stale, rssi and job_age.
- NAS stores those attempts as `fitness_observations`.
- NAS may retrieve repeated strong regions and return compact `fitness_hint` directives.
- Allowed influence: nonce order, lane/sector choice, batch hint, corpus tag, replay priority and UI/game explanation.
- Forbidden influence: SHA header, pool target, S2 validity, accepted/rejected accounting and submit pressure.
- Buzz remains the pool master; NAS remains the library/oracle.

Healthy markers:

```text
GET  /api/swarm/tranception?node_id=Gladius
POST /api/swarm/fitness
directive.intent=fitness_hint
directive.submit_pressure=do_not_increase
model=janus_tranception_lite
```
