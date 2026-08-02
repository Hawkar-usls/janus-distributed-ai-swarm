      uiOverlay(frameVisible ? "TELEMETRY FRAME ON" : "FULL COSMOS MODE", 1800UL);
      snprintf(statusLine, sizeof(statusLine), "FRAME %s", frameVisible ? "ON" : "OFF");
      Serial.printf("[GOLCRON/FRAME] visible=%u held=%lums screen=%u\n",
                    frameVisible ? 1 : 0, (unsigned long)held, kyberShield ? 0 : 1);
    } else if (held >= GOLCRON_BUTTON_TAP_MIN_MS) {
      setKyberShield(!kyberShield, "button-a-tap");
    }
    buttonAWasDown = false;
  }

  if (bDown && !buttonBWasDown) {
    buttonBWasDown = true;
    buttonBLongHandled = false;
    buttonBDownMs = now;
  } else if (bDown && !buttonBLongHandled && now - buttonBDownMs >= GOLCRON_FRAME_HOLD_MS) {
    buttonBLongHandled = true;
    starForgeReforgeCurrent("button-b");
  } else if (!bDown && buttonBWasDown) {
    if (!buttonBLongHandled && now - buttonBDownMs > GOLCRON_BUTTON_TAP_MIN_MS) {
      charmView = (uint8_t)((charmView + 1) % 3);
      saveUiState();
      const char *viewName = charmView == 0 ? "COSMOS VIEW" : (charmView == 1 ? "MINER VIEW" : "CHARM DIAG");
      uiOverlay(viewName, 1200UL);
      snprintf(statusLine, sizeof(statusLine), "%s", viewName);
    }
    buttonBWasDown = false;
  }
}

static void starForgeObserve(uint16_t bits, bool accepted) {
  forge.online = true;
  uint16_t gain = (uint16_t)min<uint32_t>(180UL, (uint32_t)bits * (accepted ? 7UL : 3UL));
  forge.energy = (uint16_t)min<uint32_t>(1000UL, forge.energy + gain + (accepted ? 90UL : 0UL));
  forge.heat = (uint16_t)min<uint32_t>(1000UL, forge.heat + (accepted ? 42UL : 12UL));
  if (bits > forge.lastBest) forge.lastBest = bits;
  if (accepted) forge.fires++;
}

static void starForgeTick(uint32_t now) {
  forge.online = true;
  if (now - forge.lastMs < 500UL) return;
  uint32_t dt = now - forge.lastMs;
  forge.lastMs = now;
  uint16_t cool = (uint16_t)min<uint32_t>(120UL, dt / 18UL);
  if (forge.heat > cool) forge.heat -= cool;
  else forge.heat = 0;
  uint16_t drain = job.active ? (uint16_t)min<uint32_t>(55UL, 8UL + hashRate / 1100UL) : 18U;
  if (forge.energy > drain) forge.energy -= drain;
  else forge.energy = job.active ? 16U : 0U;
  if (job.active && buzzSeen) forge.energy = min<uint16_t>(1000, forge.energy + 3);
}

static uint16_t starForgeBatch(uint16_t base) {
  forge.online = true;
  if (!job.active) return base;
  int boost = (int)(forge.energy / 9U) + (int)(forge.lastScore / 18U);
  if (forge.heat > 720U) boost -= (int)((forge.heat - 720U) / 3U);
  if (millis() - job.rxMs > JOB_TTL_MS - 1700UL) boost -= 70;
  int out = (int)base + constrain(boost, -120, 220);
  return (uint16_t)constrain(out, (int)GOLCRON_MIN_BATCH, (int)GOLCRON_MAX_BATCH);
}

static void sendShare(uint32_t nonce, uint16_t bits, const uint8_t shareHash[32]) {
  ShareResponseV2 sr{};
  sr.magic[0] = 'S';
  sr.magic[1] = '2';
  memcpy(sr.job_id, job.jobId, 8);
  sr.nonce = nonce;
  sr.worker_id = workerId();
  sr.bits = bits;
  sr.total_hashes_l32 = totalHashes;
  memcpy(sr.hash_tail, shareHash + 28, 4);
  sendBytes("share", (const uint8_t *)&sr, sizeof(sr));
  sharesSent++;
  starForgeObserve(bits, true);
  jediShareFlashUntilMs = millis() + 900UL;
}

static void sendColonyStatus() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, GOLCRON_NODE_ID, sizeof(pkt.nodeId));
  strlcpy(pkt.role, GOLCRON_ROLE, sizeof(pkt.role));
  pkt.seq = ++seqNo;
  pkt.hashRate = hashRate;
  pkt.shares = sharesSent;
  pkt.rejects = rejectsLocal;
  pkt.bestBits = bestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = targetBitsNow;
  pkt.aiBatch = batchSize;
  pkt.aiHint = agentHint;
  pkt.jobAgeMs = job.active ? (millis() - job.rxMs) : 0;
  pkt.rssi = lastRssi;
  pkt.uptime = millis() / 1000UL;
  sendBytes("janus", (const uint8_t *)&pkt, sizeof(pkt));
}

static void sendSwarmSense() {
  SwarmSensePacket ss{};
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = workerId();
  strlcpy(ss.nodeId, GOLCRON_NODE_ID, sizeof(ss.nodeId));
  strlcpy(ss.kind, "holocron_ast", sizeof(ss.kind));
  ss.seq = ++seqNo;
  ss.uptime_ms = millis();
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = (uint16_t)min<uint32_t>(65535UL, loopJitterUs);
  ss.loop_max_us = (uint16_t)min<uint32_t>(65535UL, loopMaxUs);
  ss.rssi = lastRssi;
  ss.radio_mode = activeChannel;
  ss.palette = 6;
  ss.knn_label = job.active ? 2 : 1;
  ss.knn_confidence = buzzSeen ? 86 : 24;
  ss.ai_hint = agentHint;
  ss.thermal_load = 20;
  ss.effective_batch = batchSize;
  ss.dynamic_batch = batchSize;
  ss.hash_rate = hashRate;
  ss.total_hashes = totalHashes;
  ss.best_bits = bestBits > 65535UL ? 65535U : (uint16_t)bestBits;
  ss.hash_eff_x1000 = targetBitsNow ? (uint16_t)min(3000UL, bestBits * 1000UL / targetBitsNow) : 0;
  ss.entropy_x1000 = (uint16_t)((job.offset ^ job.stride ^ micros()) % 1000U);
  ss.job_age_s = job.active ? (uint16_t)min(65535UL, (millis() - job.rxMs) / 1000UL) : 65535;
  ss.nonce_remaining_l16 = job.active && job.rangeSize > job.cursor ? (uint16_t)((job.rangeSize - job.cursor) & 0xFFFF) : 0;
  ss.flags = 0x0041; // worker + astrolabe
  if (forge.online) ss.flags |= 0x0200;
  sendBytes("sense", (const uint8_t *)&ss, sizeof(ss));
}

static void sendHiveMetric() {
  HiveMetricPacket hm{};
  hm.magic[0] = 'H';
  hm.magic[1] = 'M';
  hm.version = 2;
  hm.worker_id = workerId();
  strlcpy(hm.nodeId, GOLCRON_NODE_ID, sizeof(hm.nodeId));
  strlcpy(hm.kind, "holocron_ast", sizeof(hm.kind));
  hm.seq = ++seqNo;
  hm.uptime_ms = millis();
  hm.free_heap = ESP.getFreeHeap();
  hm.min_free_heap = ESP.getMinFreeHeap();
  hm.cpu_mhz = ESP.getCpuFreqMHz();
  hm.loop_jitter_us = (uint16_t)min<uint32_t>(65535UL, loopJitterUs);
  hm.loop_max_us = (uint16_t)min<uint32_t>(65535UL, loopMaxUs);
  hm.rssi = lastRssi;
  hm.bt_flags = (displayOk ? 0x01 : 0x00) | (job.active ? 0x04 : 0x00) | (frameVisible ? 0x08 : 0x00);
  hm.palette = frameVisible ? 7 : 8;
  hm.effective_batch = batchSize;
  hm.hash_rate = hashRate;
  hm.total_hashes = totalHashes;
  hm.shares = sharesSent;
  hm.rejects = rejectsLocal + shareRejectSelf;
  hm.best_bits = bestBits > 65535UL ? 65535U : (uint16_t)bestBits;
  hm.job_age_ms = job.active ? (millis() - job.rxMs) : 0;
  hm.nonce_remaining = job.active && job.rangeSize > job.cursor ? (job.rangeSize - job.cursor) : 0;
  hm.reward_level = sharesSent ? 2 : 1;
  hm.ai_hint = agentHint;
  hm.target_batch = batchSize;
  hm.prediction_error_x1000 = (int16_t)constrain((int)hashRate - (int)(batchSize * 3), -32768, 32767);
  hm.entropy_x1000 = (uint16_t)((job.offset ^ job.stride ^ micros() ^ totalHashes) % 1000U);
  hm.random_tail = (uint16_t)(esp_random() & 0xFFFF);
  hm.reserved = 0x0A57 | (forge.online ? 0x2000 : 0x0000);
  sendBytes("hive", (const uint8_t *)&hm, sizeof(hm));
}

static void drawDisplayPortrait() {
  if (!displayOk || !charmReady || kyberShield) return;
  uint32_t renderStartUs = micros();
  uint32_t now = millis();
  updateVisualState(now);

  const int W = 135;
  const int H = 240;
  const int SAFE_W = 131;
  const int cx = 65;
  const int cy = 113;
  uint16_t black = charm.color565(0, 0, 1);
  uint16_t deep = charm.color565(1, 2, 9);
  uint16_t navy = charm.color565(2, 7, 20);
  uint16_t glass = charm.color565(5, 20, 34);
  uint16_t cyan = charm.color565(74, 232, 255);
  uint16_t cyanSoft = charm.color565(18, 96, 132);
  uint16_t cyanDim = charm.color565(8, 43, 67);
  uint16_t gold = charm.color565(255, 205, 82);
  uint16_t goldSoft = charm.color565(150, 94, 26);
  uint16_t whiteGold = charm.color565(255, 244, 190);
  uint16_t green = charm.color565(92, 232, 176);
  uint16_t violet = charm.color565(172, 126, 255);
  uint16_t violetSoft = charm.color565(76, 46, 126);
  uint16_t magenta = charm.color565(214, 72, 208);
  uint16_t red = charm.color565(255, 82, 72);
  uint16_t dim = charm.color565(35, 55, 76);
  uint16_t text = charm.color565(222, 242, 238);

  charm.fillSprite(deep);
  for (int y = 0; y < H; y += 2) {
    int dy = abs(y - cy);
    uint8_t glow = (uint8_t)max(0, 27 - dy / 5);
    charm.drawFastHLine(0, y, SAFE_W,
      charm.color565(1 + glow / 12, 3 + glow / 3, 10 + glow));
  }
  charm.fillRect(SAFE_W, 0, W - SAFE_W, H, black);
  charm.fillRect(SAFE_W - 1, 0, 1, H, charm.color565(3, 14, 24));

  uint32_t stableSeed = mix32((uint32_t)workerId() ^ 0xB1A4C05FUL);
  for (uint8_t i = 0; i < 150; ++i) {
    uint32_t h = mix32(stableSeed + (uint32_t)i * 0x9E3779B9UL);
    uint8_t depth = (uint8_t)(1U + ((h >> 28) & 3U));
    int x = 2 + (int)(h % (SAFE_W - 5));
    int baseY = (int)((h >> 8) % 236U);
    int drift = (int)(visualPhase * (float)(2U + depth) * 7.0f);
    int y = (baseY + drift) % 236;
    uint8_t twinkle = (uint8_t)((h >> 20) + (now / (120U + depth * 51U)));
    uint16_t c = depth == 4 ? cyanSoft : (depth == 3 ? violetSoft : (depth == 2 ? dim : glass));
    if ((twinkle & 0x1FU) < 3U) c = depth >= 3 ? cyan : whiteGold;
    charm.drawPixel(x, y, c);
    if (depth == 4 && (h & 0x4000U) && x + 1 < SAFE_W - 1) charm.drawPixel(x + 1, y, c);
  }

  float heat = smoothForgeHeat / 1000.0f;
  float energy = smoothForgeEnergy / 1000.0f;
  float pulse = cosmosPulse;
  float armSpin = orbitPhase * (0.72f + energy * 0.24f);

  // Four broad pixel-galaxy arms: this is deliberately much closer to BH than the old astrolabe rails.
  for (uint8_t arm = 0; arm < 4; ++arm) {
    float armBase = armSpin + (float)arm * 1.5707963f;
    for (uint8_t p = 0; p < 42; ++p) {
      float radius = 4.0f + (float)p * 1.48f + pulse * 1.5f;
      float a = armBase + radius * (0.112f + heat * 0.016f) + sinf(dustPhase + p * 0.31f) * 0.055f;
      int x = cx + (int)(cosf(a) * radius);
      int y = cy + (int)(sinf(a) * radius * 0.68f);
      if (x < 2 || x > SAFE_W - 3 || y < 34 || y > 188) continue;
      uint16_t c;
      if (p < 8) c = whiteGold;
      else if (p < 18) c = goldSoft;
      else if (arm == forge.lane % 4) c = cyan;
      else c = (arm & 1U) ? violetSoft : cyanSoft;
      charm.drawPixel(x, y, c);
      if (((p + arm) % 5U) == 0U && x + 1 < SAFE_W - 2) charm.drawPixel(x + 1, y, c);
      if (pulse > 0.55f && ((p + arm) % 11U) == 0U) charm.drawPixel(x, y - 1, whiteGold);
    }
  }

  // Dense accretion ribbon around the central Holocron source.
  for (uint8_t i = 0; i < 112; ++i) {
    float a = orbitPhase * 1.42f + (float)i * 0.0560999f;
    float wobble = sinf(a * 3.0f + dustPhase) * (1.3f + heat * 2.0f);
    float rx = 18.0f + wobble;
    float ry = 7.0f + cosf(a * 2.0f) * 1.4f;
    int x = cx + (int)(cosf(a) * rx);
    int y = cy + (int)(sinf(a) * ry);
    uint16_t c = (i & 7U) < 2U ? whiteGold : ((i & 1U) ? gold : cyan);
    charm.drawPixel(x, y, c);
    if ((i % 9U) == 0U) charm.drawPixel(x, y + 1, goldSoft);
  }

  // Two faint astrolabe meridians preserve Golcron's own identity inside the BH family look.
  for (uint8_t rail = 0; rail < 2; ++rail) {
    float phase = orbitPhase * (rail ? -0.36f : 0.29f) + rail * 1.1f;
    int rx = 49 + rail * 8;
    int ry = 19 + rail * 9;
    for (uint8_t p = 0; p < 72; ++p) {
      float a = phase + (float)p * 0.0872665f;
      int x = cx + (int)(cosf(a) * (float)rx);
      int y = cy + (int)(sinf(a) * (float)ry);
      if (x > 2 && x < SAFE_W - 2 && y > 39 && y < 184) charm.drawPixel(x, y, rail ? violetSoft : cyanDim);
    }
  }

  // Expanding universe-birth wave on a new job, better hash, or share.
  if (pulse > 0.025f) {
    float normalized = 1.0f - min(1.0f, pulse / 1.32f);
    int waveR = 18 + (int)(normalized * 53.0f);
    charm.drawCircle(cx, cy, waveR, pulse > 0.72f ? gold : cyanSoft);
    if (pulse > 0.78f) charm.drawCircle(cx, cy, waveR + 2, violetSoft);
  }

  // Five Star Forge direction rays, with the active lane visibly hotter.
  for (uint8_t lane = 0; lane < 5; ++lane) {
    float a = orbitPhase * 0.43f + (float)lane * 1.256637f + currentStar().house * 0.021f;
    int r0 = 22;
    int r1 = 38 + (int)(energy * 21.0f);
    int x0 = cx + (int)(cosf(a) * r0);
    int y0 = cy + (int)(sinf(a) * r0 * 0.72f);
    int x1 = cx + (int)(cosf(a) * r1);
    int y1 = cy + (int)(sinf(a) * r1 * 0.72f);
    bool activeLane = lane == forge.lane;
    charm.drawLine(x0, y0, x1, y1, activeLane ? gold : cyanDim);
    if (activeLane) {
      charm.fillCircle(x1, y1, job.active ? 3 : 2, whiteGold);
      charm.drawCircle(x1, y1, 5, goldSoft);
    }
  }

  // Three long-lived pixel comets derived from hash state; unlike the old view they cross the full cosmos.
  uint32_t cometSeed = mix32(stableSeed ^ job.offset ^ job.stride ^ ((uint32_t)bestTail[0] << 24));
  for (uint8_t c = 0; c < 3; ++c) {
    uint32_t h = mix32(cometSeed + c * 0x45D9F3BUL);
    float a = visualPhase * (1.45f + c * 0.17f) + (float)(h & 255U) * 0.0245437f;
    float radius = 48.0f + (float)((h >> 8) & 23U);
    int x = cx + (int)(cosf(a) * radius);
    int y = cy + (int)(sinf(a) * radius * 0.67f);
    int tx = cx + (int)(cosf(a - 0.20f) * (radius - 14.0f));
    int ty = cy + (int)(sinf(a - 0.20f) * (radius - 14.0f) * 0.67f);
    if (x > 3 && x < SAFE_W - 3 && y > 28 && y < 193) {
      uint16_t cc = c == 0 ? gold : (c == 1 ? cyan : magenta);
      charm.drawLine(tx, ty, x, y, c == 0 ? goldSoft : (c == 1 ? cyanSoft : violetSoft));
      charm.fillCircle(x, y, c == 0 ? 2 : 1, cc);
    }
  }

  // Reverse-black-hole / Holocron core: bright accretion rim, dark center, newborn-universe seed.
  int corePulse = (int)((0.5f + 0.5f * sinf(orbitPhase * 3.4f)) * 2.0f + pulse * 2.0f);
  charm.drawCircle(cx, cy, 17 + corePulse, cyanDim);
  charm.drawCircle(cx, cy, 14 + corePulse, goldSoft);
  charm.fillCircle(cx, cy, 10 + corePulse / 2, black);
  charm.drawCircle(cx, cy, 10 + corePulse / 2, whiteGold);
  charm.drawCircle(cx, cy, 6, cyan);
  charm.fillCircle(cx, cy, job.active ? 3 : 2, whiteGold);
  charm.drawPixel(cx - 1, cy - 1, gold);

  charm.setTextDatum(TL_DATUM);
  charm.setTextFont(1);
  char line[42];

  if (frameVisible) {
    charm.fillRect(4, 4, 123, 18, navy);
    charm.drawRect(4, 4, 123, 18, buzzSeen ? cyanSoft : dim);
    charm.setTextColor(gold, TFT_TRANSPARENT);
    charm.drawString("GOLCRON", 9, 9);
    charm.setTextColor(buzzSeen ? green : red, TFT_TRANSPARENT);
    charm.drawString(buzzSeen ? "LINK" : "VOID", 93, 9);

    charm.fillRect(4, 24, 123, 19, charm.color565(1, 6, 15));
    charm.setTextColor(text, TFT_TRANSPARENT);
    snprintf(line, sizeof(line), "CH%u %.1fkH  B%lu/%u", (unsigned)activeChannel,
             smoothHashRateK, (unsigned long)bestBits, (unsigned)targetBitsNow);
    charm.drawString(line, 8, 30);

    charm.fillRect(4, 195, 123, 38, charm.color565(1, 6, 15));