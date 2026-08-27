# ADV_Elite Interactive Modes v1.1

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

Preferred open directory: Radio Browser API. Use `stationuuid`, `hidebroken=true`, bounded SD cache, decoder-tested codecs, and local ranking independent of public voting. Radio temporarily owns the speaker renderer while BrainWave state estimation continues. It yields before P0/P1 JANUS work.

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

Real ENV may influence a bounded comfort term; stale/unknown ENV produces UNKNOWN_ENV rather than invented values.

Controls:
- Left/Right -> select care action;
- Up/Down -> select item/amount or page;
- `Space` -> perform selected action;
- long `D` -> pet history/stats;
- `ESC` -> HOME.

## A — Alien Survival Roguelike

Short `A` enters the fictional top-down horde-survival / Alien-Shooter-inspired roguelike.

The uploaded Atom `TD_SWARM v8.16B` code is now an identified GAMEPLAY DONOR. Reusable organs include:
- fixed-step simulation (`1/30 s`) with bounded catch-up;
- spatial grid targeting;
- waves/hordes and procedural enemy roles;
- particles/beams and compact pixel graphics;
- persistent best-run records with magic/version/CRC;
- procedural day/night and weather presentation;
- hero classes, mana, independent skill cooldowns and multiple abilities;
- one-way real-event-to-game reward concepts where truth is preserved.

Do not transplant donor Wi-Fi credentials, pool identity, legacy game->entropy coupling, or any path that lets game state become JANUS truth.

### Canonical A-mode controls

- arrows / WASD -> manual movement when keyboard control is active;
- `G` -> toggle `KEYBOARD <-> GYRO` movement control;
- entering GYRO captures/recenters the neutral IMU pose; adaptive drift correction is allowed only as an input-calibration function;
- `Space` -> manual primary fire/action;
- `I` -> toggle AUTO-FIRE;
- `P` -> pause/unpause GAME SIMULATION only;
- long `A` -> run stats / controls / perk-status overlay; it is not the pause key;
- `ESC` -> HOME immediately, from play or pause;
- `ENTER` -> absolute audio mute still wins;
- `-` / `+` -> global volume still wins.

Pause freezes game simulation timers and combat progression, but does NOT freeze JANUS sensors, anomaly, Witness, swarm, clock, or protected cognition.

### GYRO control

The donor WitchHunter IMU pattern is preferred over a raw accelerometer mapping:
- capture neutral pose;
- remove gyro bias;
- derive side/forward intent from pose delta;
- low-pass roll/pitch command;
- bounded dead zone;
- adaptive recenter only while the hand is quiet and user intent is low;
- never let game recentering mutate the canonical sensor truth stream.

GYRO is a game input projection, not a replacement for the real IMU observation.

### AUTO-FIRE

AUTO-FIRE is a local gameplay convenience. It may acquire the nearest valid in-game target and fire according to the weapon cooldown. It cannot create real JANUS events, alter mining validity, or affect anomaly/M2R.

### User-active GAME PERFORMANCE PRIORITY

When `A` is active and the user is playing, ADV may temporarily give the game more discretionary compute, Zim-style, while keeping PRIMARY_MISSION intact.

Invariant:
`P0/P1/P2 ARE NEVER SACRIFICED FOR FPS`.

The following may yield first:
1. P5 archive/catalog refresh/heavy optional work;
2. nonessential P4 background rendering/BrainWave rendering not currently audible;
3. P3 Buzz/mining batch size, corpus maintenance and other bounded side work.

The SHA/verifier/target/header semantics never change. Already-found valid work is never discarded by the performance governor. The governor changes only scheduling/batch/time budget.

Suggested adaptive control uses measured render FPS / loop pressure rather than a fixed permanent throttle. Exact thresholds are hardware-tuned after Cardputer ADV benchmarks.

### Adaptive game LED

During A-mode, the LED may become a game-status renderer while preserving the canonical shared brightness axis:
- healthy -> GREEN;
- wounded -> YELLOW;
- critical -> RED;
- short damage/fire/reward accents may alter color pattern, but never create an independent brightness axis.

Canonical LED priority:
1. `L=OFF` -> BLACK, always;
2. JANUS confirmed anomaly -> WHITE, always;
3. A-mode game-status color while A is foreground;
4. House/default JANUS color policy outside the game foreground.

When A exits, LED immediately returns to the applicable JANUS/House state at the same shared brightness step. `[`/`]` continue to move display and enabled LED together while the game is active.

### Truth boundary and one-way bridges

Allowed:
`REAL ENV / REAL Buzz event / REAL pool ACCEPT -> FICTIONAL game modifier or cosmetic`.

Forbidden:
`GAME event -> OBSERVED_REAL / anomaly / M2R evidence / share validity`.

Any real-world-derived game effect must be labelled DERIVED/SIMULATED in logs.

## Long-press policy

Long presses are first-class and deterministic:
- short and long actions are documented per key;
- a long press must not also fire the short action on release;
- global safety/user-control keys keep their existing semantics;
- hold duration is centralized in one input router.

## Mode manager

Foreground states:
- HOME
- VISUALIZER_O
- ZIM_VIEW_Z
- RADIO_R
- TAMAGOTCHI_D
- ALIEN_SURVIVAL_A

Only one foreground mode is active at a time. Background JANUS core remains alive. `ESC -> HOME` is a global invariant.

## Resource hierarchy

Normal operation:
- P0 truth/clock/sensors/anomaly/Witness/user controls
- P1 local cognition/prediction/swarm model
- P2 trusted swarm communications
- P3 bounded work such as Buzz/corpus
- P4 foreground entertainment/visualization/audio
- P5 external archive/catalog refresh/heavy optional tasks

While A-mode has explicit active user input, game rendering/simulation may temporarily outrank P3 bounded work, but never P0-P2.

## Acceptance gates

1. ESC returns HOME from every mode within one input cycle.
2. ENTER mutes every audio producer, including radio and game.
3. +/- changes one global master volume used by BrainWave/radio/game/pet.
4. [ ] and L obey the canonical shared illumination policy in every mode.
5. Entering a foreground mode does not stop sensor freshness, anomaly, Witness or swarm health loops.
6. O visual transformations do not change sensing/anomaly parameters.
7. Radio Up/Down changes local ranking only and survives reboot with flash-wear guards.
8. Tamagotchi is always SIMULATED/FICTIONAL.
9. Game state is isolated from prediction/anomaly/sensor truth.
10. G toggles keyboard/gyro without mutating the canonical IMU observation stream.
11. P pauses only the game simulation.
12. I toggles auto-fire deterministically.
13. Under low game FPS, P5 then bounded P3 work yields before P0-P2.
14. Game LED health mapping obeys L, anomaly-white priority, and the shared brightness axis.
15. Donor game persistence rejects invalid magic/version/CRC.
