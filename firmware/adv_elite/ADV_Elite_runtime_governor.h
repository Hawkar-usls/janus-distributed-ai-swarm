#pragma once

#include <Arduino.h>

namespace janus_adv_elite {

enum class AdvLoadTier : uint8_t { NORMAL=0, WARM=1, COLD=2, CRITICAL=3 };

// ZIM-style runtime governor for Cardputer ADV.
// P0/P1 sensing + anomaly + foreground interaction are never disabled.
// Background work receives only residual time and is progressively throttled
// before presentation quality is reduced.
class AdvRuntimeGovernor {
 public:
  static constexpr uint32_t kHeapLow = 102400UL;
  static constexpr uint32_t kHeapRecover = 137216UL;
  static constexpr uint32_t kHeapCritical = 76000UL;
  static constexpr int8_t kRssiLow = -78;
  static constexpr int8_t kRssiRecover = -72;

  void begin(uint32_t nowMs = 0) {
    lastEvalMs_ = nowMs;
    lastHealthyMs_ = nowMs;
    tier_ = AdvLoadTier::NORMAL;
  }

  void update(uint32_t nowMs,
              uint32_t freeHeap,
              int8_t wifiRssi,
              bool wifiConnected,
              uint32_t lastLoopUs,
              float gameFps,
              bool gameForeground,
              bool radioForeground,
              bool radioStreaming) {
    loopEmaUs_ = loopEmaUs_ == 0 ? lastLoopUs : (loopEmaUs_ * 7UL + lastLoopUs) / 8UL;
    if (lastLoopUs > loopPeakUs_) loopPeakUs_ = lastLoopUs;
    else if (nowMs - lastPeakDecayMs_ >= 1000UL) {
      lastPeakDecayMs_ = nowMs;
      loopPeakUs_ = (loopPeakUs_ * 7UL) / 8UL;
    }

    if (nowMs - lastEvalMs_ < 250UL) return;
    lastEvalMs_ = nowMs;

    const bool weakWifi = wifiConnected && wifiRssi < kRssiLow;
    const bool wifiRecovered = !wifiConnected || wifiRssi > kRssiRecover;
    const bool heapCritical = freeHeap < kHeapCritical;
    const bool heapLow = freeHeap < kHeapLow;
    const bool heapRecovered = freeHeap > kHeapRecover;
    const bool loopCritical = loopEmaUs_ > 18000UL || loopPeakUs_ > 42000UL;
    const bool loopCold = loopEmaUs_ > 11500UL || loopPeakUs_ > 26000UL;
    const bool loopWarm = loopEmaUs_ > 7500UL || loopPeakUs_ > 18000UL;
    const bool gameUnderPressure = gameForeground && gameFps > 1.0f && gameFps < 48.0f;
    const bool radioUnderPressure = radioForeground && radioStreaming && loopEmaUs_ > 9000UL;

    AdvLoadTier wanted = AdvLoadTier::NORMAL;
    if (heapCritical || loopCritical) wanted = AdvLoadTier::CRITICAL;
    else if (heapLow || loopCold || gameUnderPressure || radioUnderPressure) wanted = AdvLoadTier::COLD;
    else if (weakWifi || loopWarm) wanted = AdvLoadTier::WARM;

    if ((uint8_t)wanted > (uint8_t)tier_) {
      tier_ = wanted;
      lastTierChangeMs_ = nowMs;
      return;
    }

    const bool healthy = heapRecovered && wifiRecovered && loopEmaUs_ < 6000UL && loopPeakUs_ < 15000UL &&
                         (!gameForeground || gameFps <= 1.0f || gameFps > 54.0f);
    if (healthy) {
      if (!lastHealthyMs_) lastHealthyMs_ = nowMs;
      if (nowMs - lastHealthyMs_ >= 3500UL && (uint8_t)tier_ > (uint8_t)wanted) {
        tier_ = (AdvLoadTier)((uint8_t)tier_ - 1U); // hysteretic one-step recovery
        lastTierChangeMs_ = nowMs;
        lastHealthyMs_ = nowMs;
      }
    } else {
      lastHealthyMs_ = nowMs;
    }
  }

  AdvLoadTier tier() const { return tier_; }
  const char* label() const {
    switch (tier_) {
      case AdvLoadTier::NORMAL: return "NORM";
      case AdvLoadTier::WARM: return "WARM";
      case AdvLoadTier::COLD: return "COLD";
      default: return "CRIT";
    }
  }

  uint32_t loopEmaUs() const { return loopEmaUs_; }
  uint32_t loopPeakUs() const { return loopPeakUs_; }

  uint16_t drawIntervalMs(bool gameForeground) const {
    if (gameForeground) {
      if (tier_ == AdvLoadTier::CRITICAL) return 33;
      if (tier_ == AdvLoadTier::COLD) return 20;
      return 16;
    }
    switch (tier_) {
      case AdvLoadTier::NORMAL: return 16;
      case AdvLoadTier::WARM: return 20;
      case AdvLoadTier::COLD: return 33;
      default: return 50;
    }
  }

  uint16_t ledIntervalMs() const {
    switch (tier_) {
      case AdvLoadTier::NORMAL: return 34;
      case AdvLoadTier::WARM: return 50;
      case AdvLoadTier::COLD: return 100;
      default: return 180;
    }
  }

  uint16_t robustStatsIntervalMs() const {
    switch (tier_) {
      case AdvLoadTier::NORMAL: return 300;
      case AdvLoadTier::WARM: return 450;
      case AdvLoadTier::COLD: return 750;
      default: return 1200;
    }
  }

  uint16_t buzzBudgetUs(bool gameForeground, bool radioForeground) const {
    if (tier_ == AdvLoadTier::CRITICAL) return 0;
    uint16_t us = tier_ == AdvLoadTier::NORMAL ? 1100 : (tier_ == AdvLoadTier::WARM ? 650 : 220);
    if (gameForeground) us = min<uint16_t>(us, 220);
    if (radioForeground) us = min<uint16_t>(us, 320);
    return us;
  }

  uint8_t witnessFlushQuota() const {
    return tier_ == AdvLoadTier::NORMAL ? 2 : (tier_ == AdvLoadTier::WARM ? 1 : 0);
  }

  bool allowMaintenance() const { return tier_ != AdvLoadTier::CRITICAL; }
  bool allowRadioCatalogStart() const { return tier_ == AdvLoadTier::NORMAL || tier_ == AdvLoadTier::WARM; }
  bool allowLoRaStart(bool gameForeground, bool radioForeground) const {
    return tier_ != AdvLoadTier::CRITICAL && !gameForeground && !radioForeground;
  }

 private:
  AdvLoadTier tier_ = AdvLoadTier::NORMAL;
  uint32_t loopEmaUs_ = 0;
  uint32_t loopPeakUs_ = 0;
  uint32_t lastEvalMs_ = 0;
  uint32_t lastPeakDecayMs_ = 0;
  uint32_t lastTierChangeMs_ = 0;
  uint32_t lastHealthyMs_ = 0;
};

} // namespace janus_adv_elite
