#pragma once
#include <stdint.h>
#include <stddef.h>

// ADV_Elite canonical illumination policy.
//
// One brightness axis drives both the display backlight and the onboard LED.
// '[' and ']' move the shared axis. 'L' is an independent LED output gate.
// The LED gate never changes the shared brightness index, so re-enabling the
// LED restores it at exactly the current display brightness step.

namespace janus_adv_elite {

struct IlluminationPolicy {
  static constexpr uint8_t kLevels[] = {
    0, 8, 16, 28, 44, 64, 90, 125, 170, 220
  };
  static constexpr uint8_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

  uint8_t level_index = 6;
  bool led_enabled = true;

  uint8_t level() const {
    return kLevels[level_index < kLevelCount ? level_index : (kLevelCount - 1)];
  }
  uint8_t displayBrightness() const { return level(); }
  uint8_t ledBrightness() const { return led_enabled ? level() : 0; }

  bool stepDown() {
    if (level_index == 0) return false;
    --level_index;
    return true;
  }
  bool stepUp() {
    if (level_index + 1 >= kLevelCount) return false;
    ++level_index;
    return true;
  }
  void toggleLed() { led_enabled = !led_enabled; }
  void setLed(bool enabled) { led_enabled = enabled; }
};

// Canonical invariant:
// LED_EFFECTIVE_BRIGHTNESS = LED_ENABLED ? DISPLAY_BRIGHTNESS : 0

}  // namespace janus_adv_elite

// Arduino compile hygiene: the core defines sq(x) as a macro while the donor
// Alien runtime owns a local sq() helper. The runtime is included immediately
// after this header, so remove only the macro spelling; arithmetic is unchanged.
#ifdef sq
#undef sq
#endif

// RC2 compatibility shim for one historical misspelling in the first-flash
// pet comfort expression. It is intentionally local to the sketch include
// chain and maps to Arduino's normal clamp primitive.
#ifndef comstrain
#define comstrain constrain
#endif
