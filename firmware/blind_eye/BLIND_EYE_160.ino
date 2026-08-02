void sendEyeVisionFrame(bool eventSnapshot) {
#if JANUS_EYE_VISION_ENABLE
  if (!eyeVisionEnabled && !eventSnapshot) return;
  JanusEyeFramePacket ef{};
  ef.magic[0] = 'E'; ef.magic[1] = 'F';
  ef.version = 1;
  ef.width = JANUS_EYE_VISION_W;
  ef.height = JANUS_EYE_VISION_H;
  ef.seq = ++eyeVisionSeq;
  ef.min_x10 = 0;
  ef.max_x10 = (int16_t)constrain((int)(max(tmos_presence, tmos_motion) * 10.0f), 0, 32767);
  ef.flags = 0x04; // synthetic aperture from real scalar sensor data; never camera pixels
  if (eventSnapshot) ef.flags |= 0x08;
  if (tmos_motion_now || (tachyonPredMotion1 > 38.0f && tmos_motion_memory > 0.45f)) ef.flags |= 0x01;
  if (tmos_presence_now || (tachyonPredPresence1 > 46.0f && tmos_presence_memory > 0.45f)) ef.flags |= 0x02;
  if (tmos_presence_now) ef.flags |= JANUS_EYE_FLAG_PRESENCE_NOW;
  if (tmos_motion_now) ef.flags |= JANUS_EYE_FLAG_MOTION_NOW;

  float pPower = constrain(max(tmos_presence, tachyonPredPresence1 * tmos_presence_memory) / 260.0f, 0.0f, 1.0f);
  float mPower = constrain(max(tmos_motion, tachyonPredMotion1 * tmos_motion_memory) / 180.0f, 0.0f, 1.0f);
  if (!tmos_presence_now && !tmos_motion_now && tmos_occupancy < 0.18f) {
    pPower *= 0.18f;
    mPower *= 0.18f;
  }
  float base = constrain(0.04f + pPower * 0.55f + mPower * 0.35f + tachyonFutureStress * 0.03f + tmos_occupancy * 0.05f, 0.0f, 1.0f);
  uint8_t curSector = kenshiSector & 7;
  uint8_t nextSector = kenshiPredSector & 7;

  for (uint8_t y = 0; y < JANUS_EYE_VISION_H; ++y) {
    for (uint8_t x = 0; x < JANUS_EYE_VISION_W; ++x) {
      float cx = (float)x - 3.5f;
      float cy = (float)y - 3.5f;
      float r2 = cx * cx + cy * cy;
      int aperture = (int)(base * 120.0f / (1.0f + r2 * 0.12f));
      int rayNow = eyeVisionSectorIntensity(x, y, curSector, mPower) / 2;
      int rayNext = eyeVisionSectorIntensity(x, y, nextSector, constrain(tachyonFutureStress, 0.0f, 1.0f)) / 3;
      int pulse = (int)(18.0f + 12.0f * sinf((float)millis() * 0.004f + (float)(x + y)));
      ef.pixels[y * JANUS_EYE_VISION_W + x] = (uint8_t)constrain(aperture + rayNow + rayNext + pulse, 0, 255);
    }
  }

  if (janusEyeEspNowSend("E/F", &ef, sizeof(ef), true)) {
    eyeVisionFramesTx++;
    if (eventSnapshot) eyeVisionEventFramesTx++;
    eyeVisionLastFrameMs = millis();
    if (eventSnapshot) eyeVisionLastEventFrameMs = eyeVisionLastFrameMs;
  }
#endif
}

void eyeVisionTick() {
#if JANUS_EYE_VISION_ENABLE
  uint32_t now = millis();
  if (eyeVisionEnabled && now - eyeVisionLastControlMs > JANUS_EYE_VISION_IDLE_MS) {
    eyeVisionEnabled = false;
  }

  if (eyeVisionEnabled) {
    if (now - eyeVisionLastFrameMs >= eyeVisionFrameMs) sendEyeVisionFrame(false);
    return;
  }

  // Camera-free event snapshot: a real TMOS detection may emit a low-rate aperture
  // frame even when Core2 has not requested a continuous stream. No idle radio spam.
  if ((tmos_presence_now || tmos_motion_now) &&
      now - eyeVisionLastEventFrameMs >= JANUS_EYE_EVENT_FRAME_MS) {
    sendEyeVisionFrame(true);
  }
#endif
}



// ========================= JANUS RF FUSION / RUVIEW-LITE =========================
// Stable Arduino layer: RSSI drift and ESP-NOW RX signal pressure.
// CSI phase processing will be a later compile-time experimental layer.

bool rfLiteValidRssi(int v) {
  return v < -5 && v > -126;
}

float rfLiteFusionScore() {
#if JANUS_RF_LITE_ENABLE
  return constrain(rf_presence_score + rf_motion_energy * 0.070f + rf_packet_pressure * 0.35f, 0.0f, 2.8f);
#else
  return 0.0f;
#endif
}

void rfLiteOnPacketRssi(int8_t rssi) {
#if JANUS_RF_LITE_ENABLE
  if (!rfLiteValidRssi((int)rssi)) return;
  rf_rx_packets++;
  rf_last_packet_ms = millis();
  if (rf_last_packet_rssi != -127) {
    rf_last_packet_drift = fabsf((float)rssi - (float)rf_last_packet_rssi);
  }
  rf_last_packet_rssi = rssi;
  float packetKick = constrain(rf_last_packet_drift / 12.0f, 0.0f, 1.5f);
  rf_packet_pressure = constrain(rf_packet_pressure * 0.82f + packetKick * 0.18f, 0.0f, 2.0f);
#else
  (void)rssi;
#endif
}

void rfLiteTick(uint32_t now) {
#if JANUS_RF_LITE_ENABLE
  if (now - rf_last_sample_ms < JANUS_RF_LITE_SAMPLE_MS) return;
  rf_last_sample_ms = now;

  int rssiNow = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  if (!rfLiteValidRssi(rssiNow)) {
    rf_presence_score *= 0.94f;
    rf_motion_energy *= 0.90f;
    rf_entropy *= 0.92f;
    rf_packet_pressure *= 0.96f;
    rf_presence_now = false;
    rf_motion_now = false;
    return;
  }

  if (!rf_ready || rf_rssi_ema < -126.0f || rf_rssi_baseline < -126.0f) {
    rf_rssi_ema = (float)rssiNow;
    rf_rssi_baseline = (float)rssiNow;
    rf_rssi_noise = 2.8f;
    rf_abs_drift = 0.0f;
    rf_motion_energy = 0.0f;
    rf_presence_score = 0.0f;
    rf_entropy = 0.0f;
    rf_packet_pressure = 0.0f;
    rf_ready = true;
    rf_samples = 1;
    return;
  }

  float prevEma = rf_rssi_ema;
  rf_rssi_ema = rf_rssi_ema * 0.86f + (float)rssiNow * 0.14f;
  float instantStep = fabsf((float)rssiNow - prevEma);
  rf_abs_drift = fabsf(rf_rssi_ema - rf_rssi_baseline);

  bool hot = (instantStep > JANUS_RF_LITE_MOTION_LEVEL_DB) ||
             (rf_abs_drift > max(4.5f, rf_rssi_noise * 1.85f)) ||
             (rf_packet_pressure > 0.65f);

  float baseAlpha = hot ? JANUS_RF_LITE_BASELINE_ALPHA_HOT : JANUS_RF_LITE_BASELINE_ALPHA_QUIET;
  rf_rssi_baseline = rf_rssi_baseline * (1.0f - baseAlpha) + rf_rssi_ema * baseAlpha;

  if (!hot) {
    rf_rssi_noise = rf_rssi_noise * (1.0f - JANUS_RF_LITE_NOISE_ALPHA) + instantStep * JANUS_RF_LITE_NOISE_ALPHA;
  } else {
    rf_rssi_noise = rf_rssi_noise * 0.996f + min(instantStep, rf_rssi_noise) * 0.004f;
  }
  rf_rssi_noise = constrain(rf_rssi_noise, 1.2f, 12.0f);

  float motionNorm = constrain((instantStep + rf_last_packet_drift * 0.55f) / max(JANUS_RF_LITE_MOTION_LEVEL_DB, 0.5f), 0.0f, 5.0f);
  float presenceNorm = constrain(rf_abs_drift / max(rf_rssi_noise * 2.2f + 1.0f, 1.0f), 0.0f, 4.0f);
  float packetAge = (rf_last_packet_ms == 0) ? 99999.0f : (float)((now >= rf_last_packet_ms) ? (now - rf_last_packet_ms) : 0UL);
  float packetFresh = constrain(1.0f - packetAge / (float)JANUS_RF_LITE_PACKET_TTL_MS, 0.0f, 1.0f);

  rf_motion_energy = constrain(rf_motion_energy * 0.78f + motionNorm * 0.22f + rf_packet_pressure * 0.08f, 0.0f, 12.0f);
  rf_presence_score = constrain(rf_presence_score * 0.88f + (presenceNorm + packetFresh * rf_packet_pressure * 0.35f) * 0.12f, 0.0f, 3.0f);
  rf_entropy = constrain(rf_entropy * 0.86f + (motionNorm * 0.22f + presenceNorm * 0.30f + rf_packet_pressure * 0.24f) * 0.14f, 0.0f, 4.0f);

  rf_presence_now = rf_presence_score > JANUS_RF_LITE_PRESENCE_LEVEL;
  rf_motion_now = rf_motion_energy > 1.05f || motionNorm > 1.65f;
  if (rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL && (rf_motion_now || rf_presence_now)) rf_anomaly_count++;

  rf_packet_pressure *= 0.985f;
  rf_last_packet_drift *= 0.92f;
  rf_samples++;
#else
  (void)now;
#endif
}

bool tmosWarmupActive(uint32_t now) {
  return tmosWarmupUntilMs && now < tmosWarmupUntilMs;
}

void rfLiteDebugTick(uint32_t now, bool force) {
#if JANUS_RF_LITE_ENABLE
  if (!force && now - rf_last_debug_ms < JANUS_RF_LITE_DEBUG_MS) return;
  rf_last_debug_ms = now;
  uint32_t packetAge = rf_last_packet_ms ? ((now >= rf_last_packet_ms) ? (now - rf_last_packet_ms) : 0UL) : 999999UL;
  uint32_t warmLeft = tmosWarmupActive(now) ? (tmosWarmupUntilMs - now) : 0;
  Serial.printf("[EYE/RF] ready=%u rssi=%d ema=%.1f base=%.1f noise=%.1f drift=%.1f P=%.2f M=%.2f entropy=%.2f pkt=%lu age=%lums pr=%.2f last=%d anomaly=%lu warmup=%lus lane=%s/s%u stride=%lu arm=%u tail=%lu bestN=%08lX\n",
                rf_ready ? 1 : 0,
                (int)((WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127),
                rf_rssi_ema,
                rf_rssi_baseline,
                rf_rssi_noise,
                rf_abs_drift,
                rf_presence_score,
                rf_motion_energy,
                rf_entropy,
                (unsigned long)rf_rx_packets,
                (unsigned long)packetAge,
                rf_packet_pressure,
                (int)rf_last_packet_rssi,
                (unsigned long)rf_anomaly_count,
                (unsigned long)(warmLeft / 1000UL),
                colonyMinerLaneName(colonyJob.minerLane),
                (unsigned)colonyJob.minerSector,
                (unsigned long)colonyJob.minerStride,
                (unsigned)colonyJob.minerStrideArm,
                (unsigned long)colonyMinerTailHits,
                (unsigned long)colonyMinerBestNonce);
#else
  (void)now; (void)force;
#endif
}

// ========================= JANUS COLONY EYE HOOKS =========================
float eyeLocalEntropy() {
  float agentEntropy = (float)(colonyAgentEntropySeed & 0xFFFF) / 65535.0f;
  return constrain(tmos_presence * 0.002f + tmos_motion * 0.004f + mic_rms * 20.0f +
                   mag_norm * 0.010f + imu_shock * 0.20f + loss * 1.5f +
                   rf_entropy * 0.85f + rf_motion_energy * 0.06f + rf_presence_score * 0.35f +
                   agentEntropy * 0.75f + (float)colonyAgentLevel * 0.10f + tachyonFutureStress * 0.45f,
                   0.0f, 9999.0f);
}

void onJanusHeartbeat(const JanusColonyPacket& pkt) {}
void onJanusEntropy(const EntropyReport& er, const void* opt) {}
void onJanusEntropyV2(const EntropyReportV2& er2) {}

void sendNodeHeartbeat() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, "BlindEye", sizeof(pkt.nodeId));
  strlcpy(pkt.role, "EYE_BLIND", sizeof(pkt.role));
  pkt.seq = ++colonySeq;
  pkt.hashRate = colonyRemoteHashrate;
  pkt.shares = colonyRemoteShares;
  pkt.rejects = colonyRemoteRejects;
  pkt.bestBits = colonyBestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = colonyTargetBits;
  pkt.aiBatch = effectiveColonyRemoteBatch();

  uint8_t localHint = (loss > 0.25f || kenshiBubbleState >= 3 || tachyonFutureStress > 1.20f) ? 2 : ((sync_hint > 0.85f || kenshiPriority > 120 || tachyonFutureStress > 0.70f) ? 3 : 1);
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS && colonyAgentHint > localHint) {
    localHint = colonyAgentHint;
  }
  pkt.aiHint = localHint;
  pkt.jobAgeMs = colonyJob.active ? (millis() - colonyJob.receivedAt) : 0xFFFFFFFFUL;
  pkt.rssi = colonyLastRssi;
  pkt.uptime = millis();
  janusEyeEspNowSend("HB", &pkt, sizeof(pkt), true);
}

