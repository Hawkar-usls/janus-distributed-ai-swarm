                   strongRateX4096 * 5UL +
                   min<uint32_t>(lane.shares, 8UL) * 1800UL +
                   exploration + jitter;
  if (forceDifferent && laneIndex == forge.lane && STAR_FORGE_LANES > 1) score /= 8UL;
  return (uint16_t)min<uint32_t>(65535UL, score);
}

static uint8_t starForgeChooseLane(uint32_t contextSeed, bool forceDifferent) {
  uint8_t bestLane = 0;
  uint16_t bestScore = 0;
  for (uint8_t i = 0; i < STAR_FORGE_LANES; ++i) {
    forgeLanes[i].score = starForgeAdaptiveScore(i, contextSeed, forceDifferent);
    if (i == 0 || forgeLanes[i].score > bestScore) {
      bestScore = forgeLanes[i].score;
      bestLane = i;
    }
  }
  return bestLane;
}

static void starForgeResetLearner(uint32_t seed) {
  memset(forgeLanes, 0, sizeof(forgeLanes));
  forge.seed = seed ^ 0x57A7F06EUL;
  forge.previousLane = forge.lane = 0;
  forge.lastScore = 0;
  forge.lastBest = 0;
  forge.autoPaths = 0;
  forge.manualPlans = 0;
  for (uint8_t i = 0; i < STAR_FORGE_LANES; ++i) {
    uint32_t laneSeed = mix32(forge.seed ^ 0x51A7F000UL ^ ((uint32_t)i * 0x9E3779B9UL));
    forgeLanes[i].seed = laneSeed;
    forgeLanes[i].starIndex = (uint8_t)(laneSeed % STAR_COUNT);
    forgeLanes[i].emaBitsX256 = 256;
  }
}

static bool starForgePrepareNextSlice(const char *reason) {
  if (!job.rangeSize || job.sliceOrdinal >= job.sliceCount) return false;

  job.sliceIndex = (uint32_t)(((uint64_t)job.sliceOrdinal * (uint64_t)job.sliceOrderStride +
                              (uint64_t)job.sliceOrderOffset) % job.sliceCount);
  job.sliceStart = (uint32_t)((uint64_t)job.sliceIndex * job.sliceSpan);
  if (job.sliceStart >= job.rangeSize) return false;
  job.sliceSize = min<uint32_t>(job.sliceSpan, job.rangeSize - job.sliceStart);
  job.sliceCursor = 0;

  uint32_t context = mix32(forge.seed ^ job.replanSalt ^ job.sliceIndex ^
                           (job.sliceOrdinal * 0x45D9F3BUL) ^ totalHashes);
  bool forceDifferent = job.forceReplan;
  uint8_t laneIndex = starForgeChooseLane(context, forceDifferent);
  StarForgeLane &lane = forgeLanes[laneIndex];
  forge.previousLane = forge.lane;
  forge.lane = laneIndex;
  lane.selections++;

  uint32_t starSeed = mix32(lane.seed ^ context ^ ((uint32_t)lane.selections << 16));
  lane.starIndex = (uint8_t)(starSeed % STAR_COUNT);
  job.starIndex = lane.starIndex;
  const StarRef &s = currentStar();
  uint32_t base = mix32(starSeed ^ ((uint32_t)s.ra << 16) ^
                        ((uint32_t)(uint16_t)s.dec << 1) ^ ((uint32_t)s.house << 24));
  job.offset = job.sliceSize ? (base % job.sliceSize) : 0;
  job.stride = makeCoprimeStride(base ^ 0xF067E5E5UL ^
                                 ((uint32_t)s.mag10 << 24) ^
                                 ((uint32_t)laneIndex * 1315423911UL), job.sliceSize);

  forge.lastScore = lane.score;
  forge.lastBest = lane.bestBits;
  forge.energy = min<uint16_t>(1000, (uint16_t)(220U + min<uint16_t>(lane.score / 90U, 620U)));
  forge.heat = (uint16_t)min<uint32_t>(1000UL, forge.heat / 2U + 42U);
  forge.reforges++;
  forge.autoPaths++;
  forge.lastMs = millis();
  job.forceReplan = false;

  snprintf(statusLine, sizeof(statusLine), "AUTO %s %lu/%lu", STAR_FORGE_NAMES[forge.lane],
           (unsigned long)(job.sliceOrdinal + 1UL), (unsigned long)job.sliceCount);
  Serial.printf("[GOLCRON/AUTO_PATH] reason=%s path=%lu/%lu slice=%lu start=%lu size=%lu lane=%u:%s score=%u star=%s house=%u offset=%lu stride=%lu checked=%lu/%lu noRepeat=1\n",
                reason ? reason : "auto",
                (unsigned long)(job.sliceOrdinal + 1UL), (unsigned long)job.sliceCount,
                (unsigned long)job.sliceIndex, (unsigned long)job.sliceStart,
                (unsigned long)job.sliceSize, (unsigned)forge.lane,
                STAR_FORGE_NAMES[forge.lane], (unsigned)forge.lastScore,
                s.name, (unsigned)s.house, (unsigned long)job.offset,
                (unsigned long)job.stride, (unsigned long)job.cursor,
                (unsigned long)job.rangeSize);
  return true;
}

static bool starForgeAdvanceSlice(const char *reason) {
  if (job.sliceOrdinal < job.sliceCount) {
    StarForgeLane &lane = forgeLanes[forge.lane % STAR_FORGE_LANES];
    forge.lastScore = starForgeAdaptiveScore(forge.lane, mix32(job.replanSalt ^ job.cursor), false);
    forge.lastBest = lane.bestBits;
    Serial.printf("[GOLCRON/PATH_DONE] path=%lu/%lu slice=%lu lane=%u:%s hashes=%lu meanX256=%lu emaX256=%u best=%u strong=%lu shares=%lu checked=%lu/%lu\n",
                  (unsigned long)(job.sliceOrdinal + 1UL), (unsigned long)job.sliceCount,
                  (unsigned long)job.sliceIndex, (unsigned)forge.lane,
                  STAR_FORGE_NAMES[forge.lane], (unsigned long)lane.hashes,
                  lane.hashes ? (unsigned long)(((uint64_t)lane.bitSum * 256ULL) / lane.hashes) : 0UL,
                  (unsigned)lane.emaBitsX256, (unsigned)lane.bestBits,
                  (unsigned long)lane.strongHits, (unsigned long)lane.shares,
                  (unsigned long)job.cursor, (unsigned long)job.rangeSize);
  }
  job.sliceOrdinal++;
  if (job.sliceOrdinal >= job.sliceCount) return false;
  return starForgePrepareNextSlice(reason ? reason : "auto");
}

static void starForgeLearn(uint16_t bits, bool accepted) {
  StarForgeLane &lane = forgeLanes[forge.lane % STAR_FORGE_LANES];
  if (lane.hashes >= 1000000UL) {
    lane.hashes = max<uint32_t>(1UL, lane.hashes / 2UL);
    lane.bitSum /= 2UL;
    lane.strongHits /= 2UL;
    lane.shares /= 2UL;
  }
  lane.hashes++;
  lane.bitSum += bits;
  if (bits >= STAR_FORGE_STRONG_BITS) lane.strongHits++;
  if (accepted) lane.shares++;
  if (bits > lane.bestBits) lane.bestBits = bits;
  lane.emaBitsX256 = (uint16_t)(((uint32_t)lane.emaBitsX256 * 31UL +
                                (uint32_t)bits * 256UL) / 32UL);
  if ((lane.hashes & 255UL) == 0UL) {
    lane.score = starForgeAdaptiveScore(forge.lane, mix32(job.cursor ^ job.replanSalt), false);
  }
}

static void configureAstrolabePath(const JobPacket &jp) {
  forge.online = true;
  uint32_t seed = jobSeed(jp);
  starForgeResetLearner(seed);

  job.sliceSpan = starForgeSliceSpan(job.rangeSize);
  job.sliceCount = (uint32_t)(((uint64_t)job.rangeSize + job.sliceSpan - 1ULL) / job.sliceSpan);
  job.sliceOrdinal = 0;
  job.sliceOrderOffset = job.sliceCount ? (mix32(seed ^ 0xA57A0FF5UL) % job.sliceCount) : 0;
  job.sliceOrderStride = makeCoprimeStride(mix32(seed ^ 0xC0DEC0DEUL), max<uint32_t>(1UL, job.sliceCount));
  job.replanSalt = mix32(seed ^ 0xB16B00B5UL);
  job.forceReplan = false;

  if (!starForgePrepareNextSlice("new-job")) {
    job.active = false;
    snprintf(statusLine, sizeof(statusLine), "PATH INIT FAIL");
  }
}

static void starForgeReforgeCurrent(const char *reason) {
  forge.online = true;
  if (job.active && job.cursor < job.rangeSize) {
    job.replanSalt ^= mix32(forge.reforges ^ job.cursor ^ micros() ^ esp_random());
    job.forceReplan = true;
    forge.manualPlans++;
    forge.energy = min<uint16_t>(1000, forge.energy + 120U);
    snprintf(statusLine, sizeof(statusLine), "NEXT UNSEEN PATH");
    uiOverlay("NEXT UNSEEN PATH", 1500UL);
    Serial.printf("[GOLCRON/REPLAN] queued=1 reason=%s currentPath=%lu/%lu currentSliceProgress=%lu/%lu checked=%lu/%lu cursorReset=0\n",
                  reason ? reason : "manual",
                  (unsigned long)(job.sliceOrdinal + 1UL), (unsigned long)job.sliceCount,
                  (unsigned long)job.sliceCursor, (unsigned long)job.sliceSize,
                  (unsigned long)job.cursor, (unsigned long)job.rangeSize);
  } else {
    forge.energy = min<uint16_t>(1000, forge.energy + 180U);
    forge.heat = min<uint16_t>(1000, forge.heat + 24U);
    forge.manualPlans++;
    snprintf(statusLine, sizeof(statusLine), "FORGE READY");
    uiOverlay("FORGE READY", 1200UL);
    Serial.printf("[GOLCRON/REPLAN] queued=0 reason=%s waiting-job\n", reason ? reason : "manual");
  }
}

static void onJobPacket(const JobPacket &jp) {
  buzzSeen = true;
  lastBuzzMs = millis();
  if (jp.range_size == 0) {
    discoveryRx++;
    uiOverlay("BUZZ PING", 900UL);
    return;
  }
  if (!targetHasWork(jp.target)) {
    badJobRx++;
    snprintf(statusLine, sizeof(statusLine), "BAD ZERO TARGET");
    Serial.printf("[GOLCRON/JOB] reject=zero-target start=%08lX range=%lu bad=%lu\n",
                  (unsigned long)jp.start_nonce, (unsigned long)jp.range_size,
                  (unsigned long)badJobRx);
    return;
  }

  if (sameJobAssignment(jp)) {
    duplicateJobRx++;
    job.rxMs = millis();
    if (job.cursor < job.rangeSize) job.active = true;
    Serial.printf("[GOLCRON/JOB] duplicate=%lu resume=%u checked=%lu/%lu path=%lu/%lu cursorReset=0\n",
                  (unsigned long)duplicateJobRx, job.active ? 1 : 0,
                  (unsigned long)job.cursor, (unsigned long)job.rangeSize,
                  (unsigned long)jobPathNumber(), (unsigned long)job.sliceCount);
    return;
  }

  memset(&job, 0, sizeof(job));
  job.active = true;
  memcpy(job.jobId, jp.job_id, 8);
  memcpy(job.header, jp.header, 80);
  memcpy(job.target, jp.target, 32);
  job.startNonce = jp.start_nonce;
  job.rangeSize = jp.range_size;
  job.cursor = 0;
  job.rxMs = millis();
  job.seq = ++jobSeq;
  targetBitsNow = countLeadingZeroBits(job.target);
  jobsRx++;
  jediJobPulseUntilMs = millis() + 1600UL;
  configureAstrolabePath(jp);
  uiOverlay("BUZZ STAR MAP", 1200UL);
}

static void sendBytes(const char *tag, const uint8_t *data, size_t len) {
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, data, len);
  if (err != ESP_OK) {
    txFail++;
    ensureBroadcastPeer();
    Serial.printf("[GOLCRON/TX] %s fail err=%d ch=%u\n", tag ? tag : "-", (int)err, activeChannel);
  }
}

static uint8_t ledScale(uint8_t duty) {
  uint16_t out = duty;
  if (!JEDI_LED_ACTIVE_HIGH) out = 255U - out;
  return (uint8_t)constrain((int)out, 0, 255);
}

static void jediLedWrite(uint8_t duty) {
  if (!jediLedOk) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(JEDI_LED_PIN, ledScale(duty));
#else
  ledcWrite(JEDI_LED_CHANNEL, ledScale(duty));
#endif
}

static void backlightWrite(bool on) {
  pinMode(GOLCRON_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(GOLCRON_BACKLIGHT_PIN, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
}

static void tftPanelDisplay(bool on) {
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
  tftWriteCommand(on ? 0x29 : 0x28); // ST7789 DISPON / DISPOFF
  SPI.endTransaction();
}

static void setKyberShield(bool enabled, const char *reason) {
  kyberShield = enabled;
  if (jediProbePin >= 0) {
    digitalWrite((uint8_t)jediProbePin, LOW);
    jediProbePin = -1;
  }
  jediShareFlashUntilMs = 0;
  jediJobPulseUntilMs = 0;
  jediLedWrite(0);

  if (kyberShield) {
    backlightWrite(false);
    tftPanelDisplay(false);
  } else {
    tftPanelDisplay(true);
    backlightWrite(true);
    uiOverlay("STARS AWAKE", 1300UL);
  }

  snprintf(statusLine, sizeof(statusLine), "SCREEN %s", kyberShield ? "OFF" : "ON");
  Serial.printf("[GOLCRON/SCREEN] on=%u panel=%u backlight=%u reason=%s mining=%u\n",
                kyberShield ? 0 : 1, kyberShield ? 0 : 1, kyberShield ? 0 : 1,
                reason ? reason : "-", job.active ? 1 : 0);
}

static uint8_t softWave8(uint32_t now, uint32_t periodMs, uint8_t low, uint8_t high, uint16_t phase = 0) {
  if (!periodMs) return high;
  float t = ((float)((now + phase) % periodMs) / (float)periodMs) * 6.2831853f;
  float s = (sinf(t - 1.5707963f) + 1.0f) * 0.5f;
  s = s * s * (3.0f - 2.0f * s);
  return (uint8_t)((float)low + (float)(high - low) * s);
}

static void jediLedBegin() {
  pinMode(JEDI_LED_PIN, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  jediLedOk = ledcAttach(JEDI_LED_PIN, JEDI_LED_PWM_FREQ, JEDI_LED_PWM_BITS);
#else
  ledcSetup(JEDI_LED_CHANNEL, JEDI_LED_PWM_FREQ, JEDI_LED_PWM_BITS);
  ledcAttachPin(JEDI_LED_PIN, JEDI_LED_CHANNEL);
  jediLedOk = true;
#endif
  jediLedWrite(0);
}

static bool jediProbeTick(uint32_t now) {
  if (jediProbePin < 0) return false;
  if (now >= jediProbeUntilMs) {
    digitalWrite((uint8_t)jediProbePin, LOW);
    jediProbePin = -1;
    snprintf(statusLine, sizeof(statusLine), "PROBE DONE");
    return false;
  }
  bool on = ((now / 160UL) & 1UL) == 0;
  digitalWrite((uint8_t)jediProbePin, on ? HIGH : LOW);
  return true;
}

static void startJediProbe() {
  if (kyberShield) setKyberShield(false, "probe");
  jediLedWrite(0);
  jediProbePin = (int8_t)JEDI_PROBE_PINS[jediProbeIndex % JEDI_PROBE_PIN_COUNT];
  jediProbeIndex = (uint8_t)((jediProbeIndex + 1) % JEDI_PROBE_PIN_COUNT);
  pinMode((uint8_t)jediProbePin, OUTPUT);
  digitalWrite((uint8_t)jediProbePin, LOW);
  jediProbeUntilMs = millis() + 4500UL;
  snprintf(statusLine, sizeof(statusLine), "PROBE GPIO%u", (unsigned)jediProbePin);
  uiOverlay("JEDI PROBE", 1500UL);
  Serial.printf("[GOLCRON/JEDI] probing GPIO%u for 4.5s. If blue light does not change on any probe, it is power/USB/charge light.\n",
                (unsigned)jediProbePin);
}

static void jediLedTick(uint32_t now) {
  if (!jediLedOk || now - lastJediLedMs < JEDI_LED_MS) return;
  lastJediLedMs = now;
  if (kyberShield) {
    jediLedWrite(0);
    return;
  }
  if (jediProbeTick(now)) return;

  uint8_t duty = 0;
  if (now < jediShareFlashUntilMs) {
    uint32_t left = jediShareFlashUntilMs - now;
    uint8_t sparkle = ((now / 95UL) & 1UL) ? 255 : 82;
    duty = (uint8_t)max<uint16_t>(55, (uint16_t)sparkle * min<uint32_t>(900UL, left) / 900UL);
  } else if (now < jediJobPulseUntilMs) {
    duty = softWave8(now, 540UL, 42, 150, 113);
  } else if (job.active) {
    uint8_t focus = softWave8(now, 900UL, 18, 132, (uint16_t)(job.starIndex * 83U));
    uint8_t shimmer = (uint8_t)((mix32(totalHashes ^ now ^ job.stride) >> 24) & 0x17);
    duty = (uint8_t)min<uint16_t>(180, focus + shimmer);
  } else if (buzzSeen && now - lastBuzzMs < 12000UL) {
    duty = softWave8(now, 2200UL, 6, 70, 311);
  } else {
    duty = softWave8(now, 5200UL, 0, 24, 777);
  }
  jediLedWrite(duty);
}

static bool buttonPinDown(uint8_t pin) {
  bool level = digitalRead(pin);
  return GOLCRON_BUTTON_ACTIVE_LOW ? !level : level;
}

static void buttonTick(uint32_t now) {
  bool aDown = buttonPinDown(GOLCRON_BUTTON_A_PIN);
  bool bDown = buttonPinDown(GOLCRON_BUTTON_B_PIN);

  if (aDown && !buttonAWasDown) {
    buttonAWasDown = true;
    buttonADownMs = now;
  } else if (!aDown && buttonAWasDown) {
    uint32_t held = now - buttonADownMs;
    if (held >= GOLCRON_FRAME_HOLD_MS) {
      frameVisible = !frameVisible;
      saveUiState();