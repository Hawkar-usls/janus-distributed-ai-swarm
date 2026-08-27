#pragma once
#include <stdint.h>

// RC2 compile guards.
// Arduino defines sq(x) as a macro; the Alien runtime owns a private sq(float)
// helper, so remove the macro before that header is parsed.
#ifdef sq
#undef sq
#endif

// Compatibility alias for the single RC2 pet-clamp typo in ADV_Elite.ino.
// Semantics are exactly Arduino constrain(); this can disappear when that
// source line is rewritten during the next source-format pass.
#ifndef comstrain
#define comstrain constrain
#endif

namespace janus_adv_elite {

// One foreground view at a time. JANUS core/background organs remain alive.
enum class ForegroundMode : uint8_t {
  HOME = 0,
  VISUALIZER_O,
  ZIM_VIEW_Z,
  RADIO_R,
  TAMAGOTCHI_D,
  ALIEN_SURVIVAL_A
};

enum class VisualizerSource : uint8_t {
  ENV = 0,
  MOTION,
  SWARM,
  PREDICTOR,
  SELF,
  KALEIDOSCOPE,
  COUNT
};

enum class GameMoveControl : uint8_t {
  KEYBOARD = 0,
  GYRO = 1
};

struct ModeContract {
  ForegroundMode mode = ForegroundMode::HOME;
  VisualizerSource visualizer_source = VisualizerSource::ENV;
  bool zim_overlay = false;
  bool visualizer_idle_auto = false;

  // A-mode session state. These flags never change JANUS truth state.
  GameMoveControl game_move_control = GameMoveControl::KEYBOARD;
  bool game_paused = false;
  bool game_auto_fire = false;
  bool game_stats_overlay = false;

  // Global invariant: ESC always returns HOME and clears presentation-only overlays.
  // It does not stop JANUS background organs.
  void escapeToHome() {
    mode = ForegroundMode::HOME;
    zim_overlay = false;
    game_stats_overlay = false;
  }

  void enter(ForegroundMode next) {
    mode = next;
  }

  bool inAlienGame() const {
    return mode == ForegroundMode::ALIEN_SURVIVAL_A;
  }

  void visualizerPrev() {
    uint8_t v = static_cast<uint8_t>(visualizer_source);
    const uint8_t n = static_cast<uint8_t>(VisualizerSource::COUNT);
    visualizer_source = static_cast<VisualizerSource>((v + n - 1U) % n);
  }

  void visualizerNext() {
    uint8_t v = static_cast<uint8_t>(visualizer_source);
    const uint8_t n = static_cast<uint8_t>(VisualizerSource::COUNT);
    visualizer_source = static_cast<VisualizerSource>((v + 1U) % n);
  }

  // Canonical A-mode controls. The final input router calls these only while A is foreground.
  void gameToggleGyro() {
    game_move_control = (game_move_control == GameMoveControl::KEYBOARD)
        ? GameMoveControl::GYRO
        : GameMoveControl::KEYBOARD;
    // When switching into GYRO, the hardware adapter must capture a fresh neutral pose.
  }

  void gameTogglePause() {
    game_paused = !game_paused;
    // Pause applies to game simulation only, never to JANUS P0/P1/P2 work.
  }

  void gameToggleAutoFire() {
    game_auto_fire = !game_auto_fire;
  }

  void gameToggleStatsOverlay() {
    game_stats_overlay = !game_stats_overlay;
  }
};

// Canonical input semantics for the final router:
// ESC       -> ModeContract::escapeToHome() from every foreground mode.
// ENTER     -> absolute global audio mute toggle; never mode-local confirmation.
// - / +     -> one global master volume.
// [ / ]     -> one shared display+enabled-LED brightness axis.
// L         -> independent LED physical output gate only.
// J         -> manual LoRa ON/OFF.
// 1488      -> manual full M2R gate.
// 112269    -> House mode.
// O         -> VISUALIZER_O; arrows select/transform visual source.
// Z         -> ZIM_VIEW_Z; long-Z toggles read-only resource overlay.
// R         -> RADIO_R; L/R stations, U/D local rank, Space play/pause.
// D         -> TAMAGOTCHI_D; arrows select care, Space acts.
// A         -> ALIEN_SURVIVAL_A.
// In A only:
//   arrows/WASD -> movement when KEYBOARD control is active.
//   G           -> KEYBOARD <-> GYRO; entering GYRO captures neutral pose.
//   Space       -> manual fire/action.
//   I           -> auto-fire toggle.
//   P           -> pause/unpause game simulation only.
//   long-A      -> stats/controls/perk overlay; not pause.
//
// Long-press router invariant:
// a recognized long press consumes the corresponding short press.

}  // namespace janus_adv_elite
