void motionBaseCrawlerTick(uint32_t now) {
#if JANUS_ROBOZOMBIE_ENABLE
  static bool pulseActive = false;
  static uint32_t phaseUntilMs = 0;

  if (!motionBasePresent) {
    // v2.14C: no base = штатный sensor-only mode. No actuator writes, no stop spam.
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    return;
  }

  if (!roboZombieAnyLegPresent()) {
    // Modular build: no legs installed. Keep sensors/head/swarm/miner alive.
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    return;
  }

  bool powerOk = motionBasePowerOkForActuators();
  float conf = roboZombieComputeConfidence();
  bool hot = tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now ||
             tachyonPredMotion1 > 30.0f || tachyonFutureStress > 0.72f || kenshiBubbleState >= 2 || conf > 0.72f;

  bool externalOrCharging = (motionBasePowerFlags & (0x04 | 0x20 | 0x40)) != 0;
  bool autoPowerOk = externalOrCharging || !motionBasePowerPresent || motionBaseBusMv <= 0 || motionBaseBusMv >= JANUS_ROBOZOMBIE_AUTO_MIN_MV;
  if (!autoPowerOk) roboZombieAutoBlockedLowPowerMs = now;

  bool autoAllowed = (JANUS_ROBOZOMBIE_AUTO_CRAWL_ENABLE != 0) && hot && autoPowerOk && (motionBaseArmed || roboZombieLocalArm);
  bool manualAllowed = roboZombieCrawlerManualEnable && !roboZombiePassiveMode;
  if (roboZombiePassiveMode) autoAllowed = false;
  bool allowed = motionBasePresent && !roboZombiePassiveMode && powerOk && (motionBaseArmed || roboZombieLocalArm) && (manualAllowed || autoAllowed);

  if (!allowed) {
    if (now - roboZombieCrawlerLastStopMs >= JANUS_ROBOZOMBIE_IDLE_STOP_MS ||
        roboZombieLastLeftValue != JANUS_ROBOZOMBIE_SERVO_STOP || roboZombieLastRightValue != JANUS_ROBOZOMBIE_SERVO_STOP) {
      motionBaseStopCrawler(powerOk ? (autoPowerOk ? "idle" : "auto-low-batt") : "low-power");
    }
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    return;
  }

  if (pulseActive) {
    if (now < phaseUntilMs) return;
    motionBaseStopCrawler("pulse-end");
    pulseActive = false;
    uint16_t rest = JANUS_ROBOZOMBIE_REST_MS;
    if (conf > 1.15f || manualAllowed) rest = (uint16_t)max(250, (int)JANUS_ROBOZOMBIE_REST_MS - 110);
    if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !externalOrCharging) rest += 180;
    phaseUntilMs = now + rest;
    return;
  }

  if (now < phaseUntilMs) return;

  int16_t err = motionBaseTargetAngle - JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  int8_t base = (int8_t)constrain((int)roboZombieBasePull, 12, 78);

  // Confidence makes it braver; weak battery makes it polite.
  if (!manualAllowed) {
    base = (int8_t)constrain((int)roundf(16.0f + conf * 19.0f), 14, (int)roboZombieBasePull);
  }
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !externalOrCharging) {
    if (base > JANUS_ROBOZOMBIE_LOW_BATT_PULL_CAP) base = JANUS_ROBOZOMBIE_LOW_BATT_PULL_CAP;
  }

  int8_t soft = (int8_t)max(7, base / 3);
  int8_t pivot = (int8_t)max(6, base / 4);
  int8_t left = base;
  int8_t right = base;
  const char* tag = "forward-conf";

  if (err < -JANUS_ROBOZOMBIE_PIVOT_ERR_DEG && conf > 0.85f) {
    left = -pivot;
    right = base;
    tag = "pivot-left";
  } else if (err > JANUS_ROBOZOMBIE_PIVOT_ERR_DEG && conf > 0.85f) {
    left = base;
    right = -pivot;
    tag = "pivot-right";
  } else if (err < -JANUS_ROBOZOMBIE_CENTER_DEADBAND) {
    left = soft;
    right = base;
    tag = "turn-left";
  } else if (err > JANUS_ROBOZOMBIE_CENTER_DEADBAND) {
    left = base;
    right = soft;
    tag = "turn-right";
  }

  // One-legged fallback: don't try to pivot with a missing opposite leg.
  if (roboZombieLeftLegPresent && !roboZombieRightLegPresent) {
    left = (err > JANUS_ROBOZOMBIE_CENTER_DEADBAND) ? base : soft;
    right = 0;
    tag = "one-leg-left";
  } else if (!roboZombieLeftLegPresent && roboZombieRightLegPresent) {
    left = 0;
    right = (err < -JANUS_ROBOZOMBIE_CENTER_DEADBAND) ? base : soft;
    tag = "one-leg-right";
  }

  motionBaseCrawlerWriteSpeeds(left, right, tag);
  roboZombieCrawlerPulses++;
  roboZombieCrawlerLastPulseMs = now;
  pulseActive = true;
  uint16_t pulse = JANUS_ROBOZOMBIE_PULSE_MS;
  if (conf > 1.25f || manualAllowed) pulse = (uint16_t)min(240, (int)JANUS_ROBOZOMBIE_PULSE_MS + 35);
  phaseUntilMs = now + pulse;
#else
  (void)now;
#endif
}

void handleRoboZombieSerial() {
#if JANUS_ROBOZOMBIE_ENABLE
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    switch (c) {
      case 'a': case 'A':
        if (roboZombiePassiveMode) {
          roboZombiePassiveMode = false;
          motionBaseTrackEnabled = true;
          Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by arm key");
        }
        roboZombieLocalArm = !roboZombieLocalArm;
        if (!roboZombieLocalArm) motionBaseStopCrawler("local-disarm");
        Serial.printf("[ROBOZOMBIE] localArm=%u passive=%u\n", roboZombieLocalArm ? 1 : 0, roboZombiePassiveMode ? 1 : 0);
        break;
      case 'g': case 'G':
        if (roboZombiePassiveMode) {
          roboZombiePassiveMode = false;
          motionBaseTrackEnabled = true;
          roboZombieLocalArm = true;
          Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by crawl key");
        }
        roboZombieCrawlerManualEnable = !roboZombieCrawlerManualEnable;
        if (!roboZombieCrawlerManualEnable) motionBaseStopCrawler("manual-off");
        Serial.printf("[ROBOZOMBIE] crawlerManual=%u passive=%u\n", roboZombieCrawlerManualEnable ? 1 : 0, roboZombiePassiveMode ? 1 : 0);
        break;
      case 'h': case 'H':
        roboZombieHeadPresent = !roboZombieHeadPresent;
        motionBaseLastSentAngle = -1;
        Serial.printf("[ROBOZOMBIE] headPresent=%u mode=%s\n", roboZombieHeadPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'j': case 'J':
        roboZombieLeftLegPresent = !roboZombieLeftLegPresent;
        motionBaseStopCrawler("left-present-toggle");
        Serial.printf("[ROBOZOMBIE] leftLegPresent=%u mode=%s\n", roboZombieLeftLegPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'k': case 'K':
        roboZombieRightLegPresent = !roboZombieRightLegPresent;
        motionBaseStopCrawler("right-present-toggle");
        Serial.printf("[ROBOZOMBIE] rightLegPresent=%u mode=%s\n", roboZombieRightLegPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'S':
        // HARD PASSIVE SENSOR MODE: stop S2/S4 rotors, disarm local/Core motion,
        // freeze head writes, but keep TMOS/RF/ESP-NOW/miner/learning alive.
        roboZombiePassiveMode = true;
        roboZombieCrawlerManualEnable = false;
        roboZombieLocalArm = false;
        motionBaseArmed = false;
        motionBaseTrackEnabled = false;
        motionBaseTargetAngle = motionBaseServoAngle;
        motionBaseStopCrawler("SERIAL-S-PASSIVE");
        Serial.println("[ROBOZOMBIE] PASSIVE_EYE ON: S2/S4 rotors stopped, head frozen, sensors/swarm alive. Press a/g/1/2/3 to wake.");
        break;
      case 's': case '0':
        roboZombieCrawlerManualEnable = false;
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
        motionBaseStopCrawler("serial-stop");
        Serial.println("[ROBOZOMBIE] STOP once + head center. Capital S = persistent PASSIVE_EYE.");
        break;
      case '1':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_MIN_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target LEFT %d\n", (int)motionBaseTargetAngle);
        break;
      case '2':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target CENTER %d\n", (int)motionBaseTargetAngle);
        break;
      case '3':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_MAX_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target RIGHT %d\n", (int)motionBaseTargetAngle);
        break;
      case '+': case '=':
        roboZombieBasePull = (uint8_t)constrain((int)roboZombieBasePull + 4, 10, 70);
        Serial.printf("[ROBOZOMBIE] pull=%u\n", (unsigned)roboZombieBasePull);
        break;
      case '-': case '_':
        roboZombieBasePull = (uint8_t)constrain((int)roboZombieBasePull - 4, 10, 70);
        Serial.printf("[ROBOZOMBIE] pull=%u\n", (unsigned)roboZombieBasePull);
        break;
      case 'l': case 'L':
        roboZombieLeftReverse = !roboZombieLeftReverse;
        motionBaseStopCrawler("flip-left");
        Serial.printf("[ROBOZOMBIE] leftReverse=%u\n", roboZombieLeftReverse ? 1 : 0);
        break;
      case 'r': case 'R':
        roboZombieRightReverse = !roboZombieRightReverse;
        motionBaseStopCrawler("flip-right");
        Serial.printf("[ROBOZOMBIE] rightReverse=%u\n", roboZombieRightReverse ? 1 : 0);
        break;
      case 'p': case 'P':
        Serial.printf("[ROBOZOMBIE] mode=%s map S1=head S2=left360 S4=right360 headP=%u leftP=%u rightP=%u arm=%u crawl=%u passive=%u Lrev=%u Rrev=%u pull=%u conf=%.2f Lval=%u Rval=%u base=%u power=%u mv=%d pct=%u flags=0x%02X pulses=%lu\n",
                      roboZombieBodyModeName(), roboZombieHeadPresent ? 1 : 0,
                      roboZombieLeftLegPresent ? 1 : 0, roboZombieRightLegPresent ? 1 : 0,
                      roboZombieLocalArm ? 1 : 0, roboZombieCrawlerManualEnable ? 1 : 0, roboZombiePassiveMode ? 1 : 0,
                      roboZombieLeftReverse ? 1 : 0, roboZombieRightReverse ? 1 : 0,
                      (unsigned)roboZombieBasePull, roboZombieGaitConfidence,
                      (unsigned)roboZombieLastLeftValue, (unsigned)roboZombieLastRightValue,
                      motionBasePresent ? 1 : 0, motionBasePowerPresent ? 1 : 0,
                      (int)motionBaseBusMv, (unsigned)motionBaseBatteryPct,
                      (unsigned)motionBasePowerFlags, (unsigned long)roboZombieCrawlerPulses);
        break;
      default:
        Serial.printf("[ROBOZOMBIE] keys: S passive, s stop, a arm/wake, g crawl/wake, h head-present, j left-present, k right-present, 1/2/3 target, +/- pull, l/r reverse, p print. got='%c'\n", c);
        break;
    }
  }
#endif
}

