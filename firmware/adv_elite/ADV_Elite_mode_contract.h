#pragma once
#include <stdint.h>

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

struct ModeContract {
  ForegroundMode mode = ForegroundMode::HOME;
  VisualizerSource visualizer_source = VisualizerSource::ENV;
  bool zim_overlay = false;
  bool visualizer_idle_auto = false;

  // Global invariant: ESC always returns to HOME and clears presentation-only overlays.
  void escapeToHome() {
    mode = ForegroundMode::HOME;
    zim_overlay = false;
  }

  void enter(ForegroundMode next) {
    mode = next;
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
// A         -> ALIEN_SURVIVAL_A; movement + Space primary game action.
//
// Long-press router invariant:
// a recognized long press consumes the corresponding short press.

}  // namespace janus_adv_elite
