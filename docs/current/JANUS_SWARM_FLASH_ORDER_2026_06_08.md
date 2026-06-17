# JANUS Swarm Flash Order 2026-06-08

## Current Critical Fault

BH can stay alive locally while disappearing from Buzz/Core2.

Observed signature:

```text
[COLONY] heartbeat err=12391 ch=10 peerCh=10 fail=...
[COLONY/RESCUE] reason=tx-fail-streak ...
wifi=6
```

`12391` is the ESP-NOW interface mismatch class: peer rebuild alone can fail because WiFi STA reconnect and ESP-NOW peer interface are no longer aligned.

## Prepared Fix

Flash BH first:

```text
LOCAL_SKETCHBOOK_PATH
```

This version adds:

```text
janusColonySend()
[COLONY/IF-RESET]
ESP_ERR_ESPNOW_IF / CHAN / NOT_INIT / NOT_FOUND recovery
WiFi reconnect cooldown while ESP-NOW interface is rebuilt
```

Compiled clean:

```text
m5stack:esp32:m5stack_atoms3r
Sketch 1263043 bytes (37%)
RAM 89296 bytes (27%)
```

The LastSwarm copy is synced:

```text
LOCAL_PATH_REDACTED
```

## Flash Order

1. `ATOM_BH`
   - Fixes the active invisibility fault.
   - Watch for `[COLONY/IF-RESET]` if the fault happens again.
   - Buzz/Core2 should rediscover BH without manual BH reboot.

2. `Yaks_Gate`
   - `LOCAL_SKETCHBOOK_PATH`
   - Already prepared with Klawyaks, BlackStar/Mercury reverse warp, IR gate and state memory.

3. `Anchor`
   - `LOCAL_SKETCHBOOK_PATH`
   - Stable Torricelli/p-n brother baseline.

4. `Gladius`
   - `LOCAL_SKETCHBOOK_PATH`
   - Experimental Torricelli/p-n brother.

Anchor before Gladius is preferred because Anchor is the baseline/control brother and Gladius can then compare against a stable reference.

## NAS Archivarius

NAS Brain was upgraded without touching `bot_hub`:

```text
LOCAL_PATH_REDACTED
```

New endpoints:

```text
GET  /api/swarm/archivarius
GET  /api/swarm/archivarius/report
POST /api/swarm/archivarius/checkin
GET  /api/swarm/firmware
POST /api/swarm/firmware
```

Archivarius records:

```text
node_id, sketch, firmware_version, role, kind, last_seen, channel, peer_channel,
features, hash_rate, best_bits, tx_ok, tx_fail, rescue_count, if_resets, issue
```

It diagnoses:

```text
esp-now-interface-mismatch
tx-fail-streak
wifi-disconnected
miner-zero-hashrate
weak-rssi
reject-pressure
```

Smoke test passed:

```text
python .\tests\smoke_test.py
smoke ok
```

## Next Logs To Capture

After flashing BH, keep logs for:

```text
[COLONY/IF-RESET]
[COLONY/HARD-RESCUE]
[COLONY] heartbeat err=12391
[TD] ... status=REMOTE/WIFI ...
[BLACKBOARD/TRON] tx=... fail=...
Buzz/Core2 node roster showing BH online/stale/lost
```

If BH still disappears after this build, the next patch is an APSTA fallback or stronger channel-owner policy, but only after confirming the new `[COLONY/IF-RESET]` behavior.
