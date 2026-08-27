# ADV_Elite Interactive Modes v1

Status: architecture frozen for final single ADV_Elite composition.

## Core rule

ADV_Elite remains one autonomous JANUS organ. Foreground entertainment/visual modes never replace or suspend the PRIMARY_MISSION.

PRIMARY_MISSION remains active in background:
- real sensor acquisition and freshness checks;
- anomaly detector;
- Witness/event recording;
- trusted swarm RX/TX and self-health;
- local predictor/M2R bookkeeping allowed by its current gate state;
- memory/provenance safeguards.

Foreground modes are BOUNDED_SIDE_QUESTS. Under CPU/heap/radio pressure they degrade or pause before P0/P1 JANUS functions.

## Global controls — always available

- `ESC` -> immediate return to canonical Beacon HOME HUD from any foreground mode. No mode may trap ESC.
- `ENTER` -> absolute user audio mute/unmute. This overrides BrainWave, radio, game, pet and notification sounds.
- `-` / `+` -> global master volume down/up. All audio producers read the same master volume state.
- `[` / `]` -> shared display + enabled-LED brightness axis.
- `L` -> LED physical output gate only; never changes display brightness or brightness index.
- `J` -> manual LoRa ON/OFF gate.
- `1488` -> exclusive manual full M2R gate.
- `112269` -> House mode.

Mode-local controls must not silently override these globals.

## O — Oscilloscope / Kaleidoscope Desk Mode

Purpose: diagnostic + recreational full-screen visualizer suitable as a desk screensaver.

Short `O` enters the visualizer. `ESC` returns HOME.

Left/Right cycles the visual source list:
1. ENV — temperature/humidity/pressure trends.
2. MOTION — local IMU + trusted Blind Eye motion/presence stream.
3. SWARM — swarm entropy/activity/peer cadence.
4. PREDICTOR — prediction error / residual / uncertainty.
5. SELF — loop jitter, heap/resource pressure and other self telemetry.
6. KALEIDOSCOPE — abstract visualization derived from the current real WORLD+SWARM+SELF state.

Kaleidoscope is explicitly `VISUALIZATION`, never an observation, anomaly or evidence source.

Up/Down adjusts display transform only (gain/time-scale/detail depending on visualizer implementation); it must not change sensor sensitivity, anomaly thresholds or predictor state.

Long `O` toggles optional idle-auto-visualizer policy. Idle timeout remains user-configurable; automatic entry changes presentation only.

The visualizer may lower its own frame rate under Quiet Canary pressure.

## Z — ZIM Resource View

Short `Z` opens an explanatory resource/autonomy view showing, at minimum:
- PRIMARY_MISSION;
- current bounded side quests;
- approximate CPU/time budgets;
- deferred/refused work;
- reason for deferral;
- queue/storage/radio pressure;
- current autonomous learner/promotion state.

This is a view into ADV_Elite itself; it does not control or mutate the canonical Zim node.

Long `Z` toggles a compact resource overlay over HOME. The overlay is read-only.

## R — Internet Radio

Short `R` enters full internet-radio mode.

Controls:
- Left/Right -> previous/next station in the locally ranked list.
- Up -> increase local user preference score for current station.
- Down -> decrease local user preference score for current station.
- `Space` -> play/pause current station.
- long `R` -> refresh/rebuild the cached online station catalogue when network/resources allow.
- `ESC` -> HOME.
- `ENTER` remains absolute mute.
- `-` / `+` remain global volume.

### Directory policy

Preferred open directory: Radio Browser API.

Rules:
- discover/rotate available Radio Browser API servers instead of hard-coding one server;
- use `stationuuid` as stable station identity;
- query `hidebroken=true`;
- initially prefer codecs the final Cardputer ADV decoder actually passes in hardware tests (MP3 first; AAC/AAC+ only after decoder validation);
- cache a bounded station catalogue to SD for fast boot and offline browsing;
- never store API results as trusted sensor truth.

### Ranking

Local station rank is independent of public Radio Browser voting.

Suggested score:
`LOCAL_RANK = USER_SCORE + FAVORITE_BONUS + RECENCY/PLAY_SUCCESS - FAILURE_PENALTY`

Up/Down modifies `USER_SCORE` only. A station that repeatedly fails to connect is demoted locally without deleting the user's preference history.

When a station is actually played, the client may call Radio Browser's documented click-counter endpoint as normal directory etiquette; local Up/Down must not spam or impersonate public votes.

### Audio ownership

Radio temporarily owns the speaker renderer. BrainWave state estimation continues, but BrainWave audio rendering pauses while radio is playing. When radio stops/exits, BrainWave rendering may resume according to the user's current audio state.

Radio is a bounded side quest: if decoding/network buffering threatens P0/P1 JANUS work, reduce visualization rate/buffer ambition or pause radio before dropping sensor/anomaly/Witness functions.

No Wi-Fi password, token or secret may be compiled into the public repository.

## D — ENV Tamagotchi

Short `D` enters a persistent advanced virtual-pet mode.

The pet is explicitly a FICTIONAL/SIMULATED organism. Its state must never be exported as OBSERVED_REAL.

Core needs:
- hunger;
- thirst;
- cleanliness;
- sleep/energy;
- mood/comfort;
- health;
- age;
- interaction/play;
- persistent history across reboot.

ENV coupling is contextual, not medical/biological truth. Real temperature/humidity/pressure may influence a bounded `comfort` term; stale/unknown ENV produces `UNKNOWN_ENV`, not invented values.

Recommended controls inside D:
- Left/Right -> select care action;
- Up/Down -> select item/amount or pet page;
- `Space` -> perform selected action;
- long `D` -> pet history/stats page;
- `ESC` -> HOME.

Pet time progression uses monotonic/RTC-aware timestamps and persists with flash-wear guards. Background JANUS continues normally.

## A — Alien Survival Roguelike

Short `A` enters a lightweight fictional survival roguelike inspired by top-down horde survival / Alien Shooter-style pacing.

This is entertainment only and is completely separated from JANUS truth semantics.

Suggested controls:
- arrows or WASD -> movement;
- `Space` -> fire/use primary action;
- long `A` -> pause/status overlay;
- `ESC` -> HOME immediately.

The game may have waves, hordes, procedural rooms/arena modifiers, pickups, leveling, perks, bosses, local high scores and persistent unlocks, but:
- no game variable may be emitted as OBSERVED_REAL;
- game state may not affect anomaly/M2R/sensor truth;
- it must have a fixed CPU/frame budget and yield under Quiet Canary;
- sound obeys ENTER mute and global +/- volume;
- LED effects obey the shared brightness axis and L gate.

The currently supplied Beacon/ADV code in the design discussion does NOT contain an Alien Shooter implementation; it is a JANUS/Beacon sketch. Therefore the roguelike must be implemented as a new isolated module or transplanted only from a separately identified game source.

## Long-press policy

Long presses are first-class but must remain deterministic:
- short and long actions are documented per key;
- a long press must not also fire the short action on release;
- global safety/user-control keys keep their existing semantics;
- hold duration should be centralized in one input router, not reimplemented independently by every mode.

## Mode manager

Recommended foreground states:
- HOME
- VISUALIZER_O
- ZIM_VIEW_Z
- RADIO_R
- TAMAGOTCHI_D
- ALIEN_SURVIVAL_A

Only one foreground mode is active at a time. Background JANUS core remains alive.

`ESC -> HOME` is a global invariant.

## Resource hierarchy

P0 — truth/clock/sensors/anomaly/Witness/user controls
P1 — local cognition/prediction/swarm model
P2 — trusted swarm communications
P3 — bounded work such as Buzz/corpus
P4 — foreground entertainment/visualization/audio
P5 — external archive/catalog refresh/heavy optional tasks

Radio/game/tamagotchi/visualizer must degrade before P0-P2.

## Acceptance gates

1. ESC returns HOME from every mode within one input cycle.
2. ENTER mutes every audio producer, including radio and game.
3. +/- changes one global master volume used by BrainWave/radio/game/pet.
4. [ ] and L continue to obey the canonical shared illumination policy in every mode.
5. Entering a foreground mode does not stop sensor freshness, anomaly, Witness or swarm health loops.
6. O visual transformations do not change sensing/anomaly parameters.
7. Radio Up/Down changes local ranking only and survives reboot with flash-wear guards.
8. Radio catalogue cache can fail/offline without blocking HOME or JANUS core.
9. Tamagotchi state is always classified SIMULATED/FICTIONAL.
10. Game state is isolated from prediction/anomaly/sensor truth.
11. Under induced CPU/network pressure, P4 features degrade before P0/P1.
