#pragma once
#include <stdint.h>

// Arduino defines sq(x) as a macro. The Alien runtime intentionally owns a
// tiny local helper with that historical name, so remove the global macro
// before the runtime class is parsed. No ADV_Elite module relies on Arduino's
// sq macro; explicit multiplication remains clearer in truth-critical code.
#ifdef sq
#undef sq
#endif

namespace janus_adv_elite {

// Architecture contract for the final ADV_Elite A-mode.
// This is not the full game engine. It freezes the controls/resource/LED rules
// that the transplanted Atom survival gameplay must obey.

struct AlienSimulationPolicy {
  static constexpr float kFixedDtSeconds = 1.0f / 30.0f;
  static constexpr uint8_t kMaxCatchupSteps = 4;
};

struct AlienPerformanceGovernor {
  // Provisional thresholds. Hardware benchmark may tune them, but the ordering
  // and protected-core invariant are frozen.
  float fps_ema = 45.0f;
  uint8_t discretionary_budget_pct = 100;
  bool defer_p5 = false;
  bool throttle_p3 = false;

  void update(float measured_fps, bool active_user_gameplay) {
    if (measured_fps > 1.0f) {
      fps_ema = fps_ema * 0.90f + measured_fps * 0.10f;
    }

    if (!active_user_gameplay) {
      discretionary_budget_pct = 100;
      defer_p5 = false;
      throttle_p3 = false;
      return;
    }

    // Zim-like courtesy to the human player: optional work yields first.
    // P0/P1/P2 are outside this governor and MUST NOT be throttled here.
    if (fps_ema < 28.0f) {
      discretionary_budget_pct = 15;
      defer_p5 = true;
      throttle_p3 = true;
    } else if (fps_ema < 35.0f) {
      discretionary_budget_pct = 35;
      defer_p5 = true;
      throttle_p3 = true;
    } else if (fps_ema < 42.0f) {
      discretionary_budget_pct = 65;
      defer_p5 = true;
      throttle_p3 = false;
    } else {
      discretionary_budget_pct = 100;
      defer_p5 = false;
      throttle_p3 = false;
    }
  }

  float buzzBatchScale() const {
    if (!throttle_p3) return 1.0f;
    return discretionary_budget_pct <= 15 ? 0.25f : 0.55f;
  }
};

enum class AlienMoveControl : uint8_t {
  KEYBOARD = 0,
  GYRO = 1
};

struct AlienInputPolicy {
  AlienMoveControl movement = AlienMoveControl::KEYBOARD;
  bool paused = false;
  bool auto_fire = false;

  void toggleGyro() {
    movement = (movement == AlienMoveControl::KEYBOARD)
        ? AlienMoveControl::GYRO
        : AlienMoveControl::KEYBOARD;
    // Final adapter: capture a fresh neutral pose when entering GYRO.
  }

  void togglePause() { paused = !paused; }
  void toggleAutoFire() { auto_fire = !auto_fire; }
};

struct AlienGyroPolicy {
  // Game projection only. Canonical IMU observations remain untouched.
  float dead_zone = 0.12f;
  float side_gain = 1.20f;
  float forward_gain = 1.10f;
  float lowpass_alpha = 0.10f;
  bool adaptive_recenter = true;

  // Recenter is allowed only when gyro motion is quiet and manual intent is low.
  // The final implementation should borrow the donor WitchHunter neutral-pose,
  // gyro-bias and quiet-hand recenter strategy rather than raw tilt mapping.
};

struct AlienRgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct AlienLedPolicy {
  // Returns color only. Brightness is applied later by ADV_Elite's canonical
  // shared illumination policy; this module MUST NOT own a brightness axis.
  static AlienRgb healthColor(float hp01) {
    if (hp01 >= 0.66f) return {0, 255, 0};      // healthy
    if (hp01 >= 0.33f) return {255, 210, 0};    // wounded
    return {255, 0, 0};                         // critical
  }

  static AlienRgb damageAccent(float hp01) {
    AlienRgb base = healthColor(hp01);
    // Color-only hit accent. Shared global brightness is still applied later.
    return {
      static_cast<uint8_t>(base.r > 220 ? 255 : base.r + 35),
      static_cast<uint8_t>(base.g / 2),
      static_cast<uint8_t>(base.b / 2)
    };
  }
};

// Final LED resolver priority (implemented by the common LED adapter):
// 1. L == OFF                 -> BLACK
// 2. confirmed JANUS anomaly -> WHITE
// 3. foreground A-mode       -> AlienLedPolicy color/accent
// 4. otherwise               -> House/default JANUS color
//
// Final effective LED brightness is always:
//   led_enabled ? shared_display_brightness_step : 0
// and never a game-owned brightness value.

// One-way integration is allowed:
//   REAL ENV / real Buzz event / real pool ACCEPT -> fictional game modifier.
// Reverse integration is forbidden:
//   game state -> OBSERVED_REAL / anomaly / M2R evidence / SHA validity.

}  // namespace janus_adv_elite
