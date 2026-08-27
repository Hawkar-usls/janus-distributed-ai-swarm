# Changelog

## 2026-08-27 - Autonomous specialist doctrine from Zim experiment

- Added `docs/AUTONOMOUS_SPECIALIST_DOCTRINE.md` as a mandatory design rule for future swarm-node upgrades.
- Froze `PRIMARY_MISSION + BOUNDED_SIDE_QUESTS` as the default heterogeneous-node architecture: each node protects its specialization while accepting only bounded coordinator side-work.
- Added `SELF_TESTED_ACCELERATION_ONLY`: fast paths must prove required equivalence against a trusted reference, benchmark better, retain cross-checks where practical and fall back on mismatch.
- Promoted the Zim-style Quiet Canary resource hierarchy: optional UI/logging/background study/side-work degrades before the primary mission.
- Froze `IMMUTABLE_TRUTH_CORE + LEARNABLE_POLICY_AROUND_IT` for safe online learning.
- Added versioned/throttled persistence and autonomy-without-information-isolation requirements.
- Preserved current Zim behavior as an observation target rather than normalizing it into an obedient worker.
- Added `docs/ADV_ELITE_ZIM_SPECIALIST_ADOPTION.md`, binding the generalizable Zim properties into the ADV_Elite rebuild architecture without deciding the still-separate learned-candidate auto-promotion question.
- Added a pull-request checklist and `.janus/AUTONOMOUS_SPECIALIST_DOCTRINE.json` binding so the doctrine is visible during future node reviews.

## 2026-06-17 - Sync public repo with current distributed swarm

- Expanded public firmware map from the older eight-node drop to the current
  swarm shape: Core2, Buzz, BH/BlackStar, ADV Elite, Yaks Gate, Anchor, Gladius,
  Zim, Blind Eye, Pyramid, Beacon preserve firmware, legacy bodies, and PEA4.
- Added current Anchor and Gladius brother firmware with direct Buzz recovery
  paths and Tranception-lite context.
- Added current BH/BlackStar and ADV Elite firmware.
- Added Yaks Gate StickS3 firmware folder.
- Added PEA4 ESP32-P4 pool-probe firmware and PEA4 Janus Shell v0.1
  display/touch bring-up.
- Added `docs/current-swarm-state.md` and `docs/swarm-critical-rules.md`.
- Kept stock firmware backups, vendor zip archives, raw logs and secrets out of
  the public repository.

## 2026-05-15 - Focus public main on LastSwarm firmware

- Reduced public main branch to the eight current LastSwarm firmware sketches.
- Removed unrelated archives, non-firmware side projects, runtime samples, old firmware branches, and broad experimental material from the visible repository.
- Rebuilt README, project map, M5Stack review guide, docs, and audits around the focused firmware scope.
- Preserved the broader import locally before cleaning the public review surface.

## 2026-05-15 - Latest LastSwarm firmware drop

- Imported sanitized latest candidates for Beacon, Blind Eye, Buzz, Core2, Stick, Zim, Atom Swarm TRON, and Atom Matrix Pyramid.

## 2026-05-15 - Initial import

- Created initial JANUS import structure.
- Added MIT license, security policy, config examples, and static checks.
