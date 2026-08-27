#pragma once
#include <stdint.h>
#include <stddef.h>

// ADV_Elite canonical illumination policy.
//
// One brightness axis drives both the display backlight and the onboard LED.
// '[' and ']' move the shared axis. 'L' is an independent LED output gate.
// The LED gate never changes the shared brightness index, so re-enabling the
// LED restores it at exactly the current display brightness step.
//
// This file is deliberately hardware-agnostic. The final ADV_Elite adapter
// applies displayBrightness() to the display and ledBrightness() to the LED
// driver (M5.Led or FastLED). State/color logic may choose LED colour, but it
// must not create a second independent brightness axis.

namespace janus_adv_elite {

struct IlluminationPolicy {
  static constexpr uint8_t kLevels[] = {
    0, 8, 16, 28, 44, 64, 90, 125, 170, 220
  };
  static constexpr uint8_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

  uint8_t level_index = 6;   // historical useful default: 90
  bool led_enabled = true;   // manual L gate only

  uint8_t level() const {
    return kLevels[level_index < kLevelCount ? level_index : (kLevelCount - 1)];
  }

  uint8_t displayBrightness() const {
    return level();
  }

  uint8_t ledBrightness() const {
    return led_enabled ? level() : 0;
  }

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

  void toggleLed() {
    led_enabled = !led_enabled;
  }

  void setLed(bool enabled) {
    led_enabled = enabled;
  }
};

// Canonical invariant for integration/tests:
//   LED_EFFECTIVE_BRIGHTNESS = LED_ENABLED ? DISPLAY_BRIGHTNESS : 0
// There is no LED-only brightness memory and no separate LED brightness walk.

}  // namespace janus_adv_elite
