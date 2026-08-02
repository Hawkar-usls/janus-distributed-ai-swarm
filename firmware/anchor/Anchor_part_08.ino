                    (unsigned long)anchorPnTx, laneName(pn.lane), (unsigned)pn.sector,
                    (unsigned long)pn.hash_rate, (unsigned)pn.best_bits, (unsigned)pn.target_bits,
                    thermal, load, rfBody, tail, anchorOxytocin, anchorTorricelliVacuum,
                    anchorTranceptionLiteScore, (unsigned)anchorTranceptionHint, (unsigned)pn.flags);
    }
  } else {
    anchorPnFail++;
    if ((anchorPnFail & 0x07UL) == 1UL) {
      Serial.printf("[ANCHOR/PN] fail=%lu lastErr=%d\n", (unsigned long)anchorPnFail, lastTxErr);
    }
  }
}

void anchorPresenceBurst(const char* reason) {
  uint32_t now = millis();
  if (anchorLastPresenceBurstMs && janusSafeAgeMs(now, anchorLastPresenceBurstMs, 0UL) < ANCHOR_PRESENCE_BURST_MIN_MS) return;
  anchorLastPresenceBurstMs = now;
  anchorPresenceBursts++;
  lastHeartbeatMs = 0; lastEntropyMs = 0; lastSwarmSenseMs = 0; anchorPnLastMs = 0;
  sendHeartbeat(); sendEntropy(); sendSwarmSense(true); sendAnchorPnCortex(true); janusTwinTaskBroadcast(true);
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  uint32_t anyRxAge = lastAnyRxMs ? janusSafeAgeMs(now, lastAnyRxMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/PRESENCE] burst=%lu reason=%s masterAge=%lums anyRxAge=%lums H=%lu best=%lu job=%u tx=%lu/%lu direct=%lu/%lu known=%u cb=%lu/%lu heap=%lu\n",
                (unsigned long)anchorPresenceBursts, reason ? reason : "?", (unsigned long)masterAge, (unsigned long)anyRxAge,
                (unsigned long)hashRate, (unsigned long)bestBits, job.active ? 1 : 0,
                (unsigned long)txOk, (unsigned long)txFail, (unsigned long)buzzMasterDirectOk,
                (unsigned long)buzzMasterDirectFail, buzzMasterMacKnown ? 1 : 0,
                (unsigned long)sentCbOk, (unsigned long)sentCbFail, (unsigned long)ESP.getFreeHeap());
}

void rfDebug(uint32_t now, bool force=false) {
  if (!force && now - lastRfDebugMs < RF_DEBUG_MS) return;
  lastRfDebugMs = now;
  uint32_t age = rfLastPacketMs ? janusSafeAgeMs(now, rfLastPacketMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/RF] ready=%u rssi=%d ema=%.1f base=%.1f noise=%.1f drift=%.1f P=%.2f M=%.2f entropy=%.2f pkt=%lu age=%lums pr=%.2f lane=%s/s%u stride=%lu arm=%u H=%lu best=%lu tail=%lu tx=%lu/%lu\n",
                rfReady ? 1 : 0, (int)lastRssi, rfEma, rfBase, rfNoise, rfDrift,
                rfPresence, rfMotion, rfEntropy, (unsigned long)rfRxPackets, (unsigned long)age,
                rfPacketPressure, laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)tailHits,
                (unsigned long)txOk, (unsigned long)txFail);
}

bool auxLedPinValid(int pin) {
  if (pin < 0 || pin > 48) return false;
#if ANCHOR_LED_ENABLE
  if (pin == ANCHOR_LED_PIN) return false;
#endif
  return true;
}
void anchorSmallLedWriteRaw(bool on) {
  if (!auxLedPinValid(ANCHOR_EXTRA_BLUE_PIN)) return;
  bool level = ANCHOR_EXTRA_BLUE_ACTIVE_LOW ? !on : on;
  digitalWrite(ANCHOR_EXTRA_BLUE_PIN, level ? HIGH : LOW);
  anchorSmallLedLastState = on;
}
void auxLedOffPin(int pin, bool activeLow) {
  if (!auxLedPinValid(pin)) return;
  digitalWrite(pin, activeLow ? HIGH : LOW);
}
void setupExtraLeds() {
#if ANCHOR_EXTRA_LED_OFF_ENABLE
  if (auxLedPinValid(ANCHOR_EXTRA_BLUE_PIN)) { pinMode(ANCHOR_EXTRA_BLUE_PIN, OUTPUT); anchorSmallLedWriteRaw(false); }
  if (auxLedPinValid(ANCHOR_EXTRA_YELLOW_PIN)) { pinMode(ANCHOR_EXTRA_YELLOW_PIN, OUTPUT); auxLedOffPin(ANCHOR_EXTRA_YELLOW_PIN, ANCHOR_EXTRA_YELLOW_ACTIVE_LOW); }
  Serial.printf("[ANCHOR/EXTRA_LED] smallLedPin=%d smallLed=%u tiedToUART0FullLog=1 yellowPin=%d note='PWR LED may be hardware-only'\n",
                (int)ANCHOR_EXTRA_BLUE_PIN, anchorSmallLedEnabled ? 1 : 0, (int)ANCHOR_EXTRA_YELLOW_PIN);
#else
  Serial.println("[ANCHOR/EXTRA_LED] disabled");
#endif
}
void extraLedTick(uint32_t now) {
#if ANCHOR_EXTRA_LED_OFF_ENABLE
  bool on = false;
  if (anchorSmallLedEnabled) {
    if (lastMaxBrightnessFlashMs && janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) on = true;
    else if (lastShareMs && janusSafeAgeMs(now, lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS) on = ((now / 110UL) % 2UL) == 0;
    else if (job.active) on = ((now / 520UL) % 2UL) == 0;
    else { uint32_t m = now % 2400UL; on = (m < 75UL) || (m > 245UL && m < 315UL); }
  }
  if (on != anchorSmallLedLastState) anchorSmallLedWriteRaw(on);
  auxLedOffPin(ANCHOR_EXTRA_YELLOW_PIN, ANCHOR_EXTRA_YELLOW_ACTIVE_LOW);
#else
  (void)now;
#endif
}
void anchorLedWrite(uint8_t r, uint8_t g, uint8_t b) {
#if ANCHOR_LED_ENABLE
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(ANCHOR_LED_PIN, r, g, b);
  #else
    neopixelWrite(ANCHOR_LED_PIN, r, g, b);
  #endif
#else
  (void)r; (void)g; (void)b;
#endif
}
uint8_t scaleLed(uint8_t v) { return (uint8_t)(((uint16_t)v * (uint16_t)ledBrightness) / 255U); }
void clampLedBrightness() {
  if (ledBrightness < ANCHOR_BRIGHTNESS_MIN) ledBrightness = ANCHOR_BRIGHTNESS_MIN;
  if (ledBrightness > ANCHOR_BRIGHTNESS_MAX) ledBrightness = ANCHOR_BRIGHTNESS_MAX;
}
void triggerMaxBrightnessFlash(const char* reason) {
  lastMaxBrightnessFlashMs = millis(); lastLedMs = 0;
  Serial.printf("[ANCHOR/LED] max brightness reached -> white flash %lums reason=%s next_taps=dim_down\n",
                (unsigned long)ANCHOR_LED_MAX_FLASH_MS, reason ? reason : "?");
}
void setLedBrightness(uint8_t value, const char* reason) {
  uint8_t old = ledBrightness; ledBrightness = value; clampLedBrightness();
  if (ledBrightness == old) return;
  brightnessChangedMs = millis(); brightnessDirty = true; lastLedMs = 0;
  Serial.printf("[ANCHOR/LED] brightness=%u old=%u reason=%s dir=%s hint='tap cycles 0->max->0 / long toggles UART logs / serial +/-'\n",
                (unsigned)ledBrightness, (unsigned)old, reason ? reason : "?", buttonBrightnessDirUp ? "up" : "down");
  if (old < ANCHOR_BRIGHTNESS_MAX && ledBrightness >= ANCHOR_BRIGHTNESS_MAX) triggerMaxBrightnessFlash(reason);
}
void stepLedBrightness(int delta, const char* reason) {
  int v = (int)ledBrightness + delta;
  if (v > ANCHOR_BRIGHTNESS_MAX) v = ANCHOR_BRIGHTNESS_MAX;
  if (v < ANCHOR_BRIGHTNESS_MIN) v = ANCHOR_BRIGHTNESS_MIN;
  setLedBrightness((uint8_t)v, reason);
}
void tapCycleLedBrightness(const char* reason) {
  if (ledBrightness >= ANCHOR_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= ANCHOR_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;
  stepLedBrightness(buttonBrightnessDirUp ? ANCHOR_BRIGHTNESS_STEP : -ANCHOR_BRIGHTNESS_STEP,
                    reason ? reason : "button_tap_cycle");
  if (ledBrightness >= ANCHOR_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= ANCHOR_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;
}
void loadLedBrightness() {
#if ANCHOR_BRIGHTNESS_PERSIST
  anchorPrefsReady = anchorPrefs.begin("janusAnc", false);
