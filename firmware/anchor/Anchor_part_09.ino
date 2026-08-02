  if (anchorPrefsReady) {
    ledBrightness = anchorPrefs.getUChar("led_bri", ANCHOR_LED_BRIGHTNESS);
    janusUart0FullLog = anchorPrefs.getBool("uart_full", (JANUS_UART0_MIRROR_ENABLE != 0));
    anchorSmallLedEnabled = anchorPrefs.getBool("small_led", janusUart0FullLog);
  }
#endif
  anchorSmallLedEnabled = janusUart0FullLog || anchorSmallLedEnabled;
  clampLedBrightness();
  buttonBrightnessDirUp = (ledBrightness < ANCHOR_BRIGHTNESS_MAX);
}

void brightnessSaveTick(uint32_t now) {
#if ANCHOR_BRIGHTNESS_PERSIST
  if (brightnessDirty && anchorPrefsReady && brightnessChangedMs && now - brightnessChangedMs >= ANCHOR_BRIGHTNESS_SAVE_MS) {
    anchorPrefs.putUChar("led_bri", ledBrightness);
    brightnessDirty = false;
    Serial.printf("[ANCHOR/LED] brightness saved=%u\n", (unsigned)ledBrightness);
  }
#else
  (void)now;
#endif
}

void setUart0FullLog(bool enable, const char* reason) {
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
  bool old = janusUart0FullLog;
  janusUart0FullLog = enable;
  anchorSmallLedEnabled = enable;
#if ANCHOR_BRIGHTNESS_PERSIST
  if (anchorPrefsReady) {
    anchorPrefs.putBool("uart_full", janusUart0FullLog);
    anchorPrefs.putBool("small_led", anchorSmallLedEnabled);
  }
#endif
  if (!anchorSmallLedEnabled) anchorSmallLedWriteRaw(false);
  else anchorSmallLedWriteRaw(true);
  Serial.printf("[ANCHOR/UART0] fullLog=%u old=%u reason=%s smallLed=%s uartTxWhenOff=0 persisted=1\n",
                janusUart0FullLog ? 1 : 0, old ? 1 : 0, reason ? reason : "?",
                anchorSmallLedEnabled ? "enabled_blink" : "off");
#else
  (void)enable; (void)reason;
#endif
}
void toggleUart0FullLog(const char* reason) { setUart0FullLog(!janusUart0FullLog, reason); }
void setupBrightnessButton() {
#if ANCHOR_BUTTON_ENABLE
  pinMode(ANCHOR_BUTTON_PIN, INPUT_PULLUP);
  bool rawPressed = ANCHOR_BUTTON_ACTIVE_LOW ? (digitalRead(ANCHOR_BUTTON_PIN) == LOW) : (digitalRead(ANCHOR_BUTTON_PIN) == HIGH);
  buttonStablePressed = rawPressed; buttonLastRawPressed = rawPressed; buttonPressStartMs = rawPressed ? millis() : 0;
  Serial.printf("[ANCHOR/BUTTON] enabled pin=%u mode=tap_cycle_brightness veryLong(~%lums)=toggle_uart_logs min=%u max=%u step=%u savedBrightness=%u dir=%s uartFull=%u\n",
                (unsigned)ANCHOR_BUTTON_PIN, (unsigned long)ANCHOR_BUTTON_LOG_TOGGLE_MS,
                (unsigned)ANCHOR_BRIGHTNESS_MIN, (unsigned)ANCHOR_BRIGHTNESS_MAX,
                (unsigned)ANCHOR_BRIGHTNESS_STEP, (unsigned)ledBrightness,
                buttonBrightnessDirUp ? "up" : "down", janusUart0FullLog ? 1 : 0);
#else
  Serial.println("[ANCHOR/BUTTON] disabled");
#endif
}
void brightnessButtonTick(uint32_t now) {
#if ANCHOR_BUTTON_ENABLE
  bool rawPressed = ANCHOR_BUTTON_ACTIVE_LOW ? (digitalRead(ANCHOR_BUTTON_PIN) == LOW) : (digitalRead(ANCHOR_BUTTON_PIN) == HIGH);
  if (rawPressed != buttonLastRawPressed) { buttonLastRawPressed = rawPressed; lastButtonSampleMs = now; }
  if ((now - lastButtonSampleMs) < ANCHOR_BUTTON_DEBOUNCE_MS) return;
  if (rawPressed != buttonStablePressed) {
    buttonStablePressed = rawPressed;
    if (buttonStablePressed) {
      buttonPressStartMs = now; buttonLastRepeatMs = now; buttonLongMode = false; buttonLogToggleFired = false;
    } else {
      if (!buttonLogToggleFired) tapCycleLedBrightness("button_tap_cycle");
      buttonPressStartMs = 0; buttonLongMode = false; buttonLogToggleFired = false;
    }
  }
  if (buttonStablePressed && buttonPressStartMs && !buttonLogToggleFired && (now - buttonPressStartMs >= ANCHOR_BUTTON_LOG_TOGGLE_MS)) {
    buttonLogToggleFired = true; buttonLongMode = true; toggleUart0FullLog("button_very_long_toggle");
  }
#else
  (void)now;
#endif
}

void anchorLedTick(uint32_t now) {
#if ANCHOR_LED_ENABLE
  if (now - lastLedMs < 70UL) return;
  lastLedMs = now;
  if (ledBrightness == 0) {
    if (lastLedR || lastLedG || lastLedB) { lastLedR = 0; lastLedG = 0; lastLedB = 0; anchorLedWrite(0, 0, 0); }
    extraLedTick(now); return;
  }
  const float armyR0 = 18.0f, armyG0 = 118.0f, armyB0 = 26.0f;
  const float armyR1 = 34.0f, armyG1 = 164.0f, armyB1 = 42.0f;
  const float turqR0 = 0.0f, turqG0 = 126.0f, turqB0 = 118.0f;
  const float turqR1 = 10.0f, turqG1 = 228.0f, turqB1 = 214.0f;
  bool swapActive = janusFaceShareActive();
  uint8_t normalFace = janusFaceBaseFace() & 1;
  uint8_t swappedFace = normalFace ^ 1;
  float phase = (sinf((float)now / 1900.0f) + 1.0f) * 0.5f;
  float soft = phase * phase * (3.0f - 2.0f * phase);
  bool normalTurq = (normalFace != JANUS_FACE_AMBER);
  bool swappedTurq = (swappedFace != JANUS_FACE_AMBER);
  float fromR = normalTurq ? (turqR0 + (turqR1 - turqR0) * soft) : (armyR0 + (armyR1 - armyR0) * soft);
  float fromG = normalTurq ? (turqG0 + (turqG1 - turqG0) * soft) : (armyG0 + (armyG1 - armyG0) * soft);
  float fromB = normalTurq ? (turqB0 + (turqB1 - turqB0) * soft) : (armyB0 + (armyB1 - armyB0) * soft);
  float toR = swappedTurq ? (turqR0 + (turqR1 - turqR0) * soft) : (armyR0 + (armyR1 - armyR0) * soft);
  float toG = swappedTurq ? (turqG0 + (turqG1 - turqG0) * soft) : (armyG0 + (armyG1 - armyG0) * soft);
  float toB = swappedTurq ? (turqB0 + (turqB1 - turqB0) * soft) : (armyB0 + (armyB1 - armyB0) * soft);
  float swapMix = 0.0f, flash = 0.0f;
  if (swapActive) {
    uint32_t refMs = lastShareMs ? lastShareMs : janusFacePeerLastMs;
    float age = constrain((float)janusSafeAgeMs(now, refMs, 0UL) / (float)JANUS_FACE_SWAP_MS, 0.0f, 1.0f);
    if (age < 0.28f) { swapMix = age / 0.28f; swapMix = swapMix * swapMix * (3.0f - 2.0f * swapMix); }
    else if (age > 0.72f) { swapMix = constrain((1.0f - age) / 0.28f, 0.0f, 1.0f); swapMix = swapMix * swapMix * (3.0f - 2.0f * swapMix); }
    else swapMix = 1.0f;
    flash = constrain((0.16f - age) / 0.16f, 0.0f, 1.0f); flash = flash * flash;
  }
  float r = fromR * (1.0f - swapMix) + toR * swapMix;
  float g = fromG * (1.0f - swapMix) + toG * swapMix;
  float b = fromB * (1.0f - swapMix) + toB * swapMix;
  if (flash > 0.0f) { r = r * (1.0f - flash) + 245.0f * flash; g = g * (1.0f - flash) + 255.0f * flash; b = b * (1.0f - flash) + 225.0f * flash; }
  if (lastMaxBrightnessFlashMs && janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) {
    float age = (float)janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) / (float)ANCHOR_LED_MAX_FLASH_MS;
    float glow = (age < 0.35f) ? 1.0f : constrain((1.0f - age) / 0.65f, 0.0f, 1.0f);
    r = r * (1.0f - glow) + 255.0f * glow; g = g * (1.0f - glow) + 255.0f * glow; b = b * (1.0f - glow) + 255.0f * glow;
  }
  uint8_t rr = scaleLed((uint8_t)constrain((int)r, 0, 255));
  uint8_t gg = scaleLed((uint8_t)constrain((int)g, 0, 255));
  uint8_t bb = scaleLed((uint8_t)constrain((int)b, 0, 255));
  if (rr != lastLedR || gg != lastLedG || bb != lastLedB) {
