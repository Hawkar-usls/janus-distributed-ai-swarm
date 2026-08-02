void updateTachyonProphecy() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  tachyonBlendRemoteProphecies();

  float prevP = tachyonSeqGet(tachyonSeqPresence, 0, tmos_presence);
  float prevP2 = tachyonSeqGet(tachyonSeqPresence, 1, prevP);
  float prevM = tachyonSeqGet(tachyonSeqMotion, 0, tmos_motion);
  float prevM2 = tachyonSeqGet(tachyonSeqMotion, 1, prevM);

  float errP = fabsf(tachyonLastPredPresence1 - tmos_presence) / max(80.0f, max(tmos_presence, tachyonLastPredPresence1) + 1.0f);
  float errM = fabsf(tachyonLastPredMotion1 - tmos_motion) / max(60.0f, max(tmos_motion, tachyonLastPredMotion1) + 1.0f);
  tachyonLossEma = tachyonLossEma * 0.90f + constrain((errP + errM) * 0.5f, 0.0f, 5.0f) * 0.10f;

  tachyonPresenceConfidence = constrain(tachyonPresenceConfidence * 0.92f + (1.0f / (1.0f + errP * 2.5f)) * 0.08f, 0.0f, 1.0f);
  tachyonMotionConfidence = constrain(tachyonMotionConfidence * 0.92f + (1.0f / (1.0f + errM * 2.5f)) * 0.08f, 0.0f, 1.0f);

  float lr = 0.0045f;
  tachyonTrendGain = constrain(tachyonTrendGain + lr * ((errP + errM) < 0.45f ? 0.003f : -0.010f), 0.18f, 1.20f);
  tachyonMemoryGain = constrain(tachyonMemoryGain + lr * ((tachyonSeqCount > 6 && tachyonLossEma < 0.55f) ? 0.004f : -0.004f), 0.08f, 0.75f);
  tachyonRemoteGain = constrain(tachyonRemoteGain + lr * ((tachyonRemoteCount > 0 && tachyonLossEma < 0.75f) ? 0.004f : -0.003f), 0.00f, 0.45f);

  float trendP = constrain(tmos_presence - prevP2, -700.0f, 700.0f);
  float trendM = constrain(tmos_motion - prevM2, -500.0f, 500.0f);
  float memP = tachyonWeightedMemory(tachyonSeqPresence, tmos_presence);
  float memM = tachyonWeightedMemory(tachyonSeqMotion, tmos_motion);

  // Physarious-inspired tissue/energy gates: moving parts later will use these as servo caution.
  float balance = fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z - 1.0f);
  tachyonEnergy = tachyonEnergy * 0.86f + constrain(imu_shock * 0.35f + mic_rms * 14.0f + tmos_motion * 0.006f + balance * 0.20f, 0.0f, 8.0f) * 0.14f;
  tachyonLangerDrag = tachyonLangerDrag * 0.90f + constrain(balance * 0.20f + loss * 0.40f + tachyonEnergy * 0.06f, 0.0f, 2.0f) * 0.10f;
  tachyonPhysarumTrace = tachyonPhysarumTrace * 0.965f + constrain(tmos_motion / 700.0f + tmos_presence / 1600.0f, 0.0f, 2.0f) * 0.035f;
  if (tachyonPhysarumTrace < 0.025f) tachyonPhysarumTrace *= 0.65f;

  float drag = 1.0f / (1.0f + tachyonLangerDrag * 0.35f);
  float remoteP = tachyonRemotePresence * tachyonRemoteGain;
  float remoteM = tachyonRemoteMotion * tachyonRemoteGain;

  float nextP = tmos_presence * 0.56f + memP * tachyonMemoryGain + (tmos_presence + trendP * tachyonTrendGain) * 0.26f + remoteP * 0.18f;
  float nextM = tmos_motion * 0.54f + memM * tachyonMemoryGain + (tmos_motion + trendM * tachyonTrendGain) * 0.30f + remoteM * 0.18f;
  nextP *= drag;
  nextM *= drag;

  tachyonPredPresence1 = constrain(tachyonPredPresence1 * 0.68f + nextP * 0.32f, 0.0f, 3000.0f);
  tachyonPredMotion1 = constrain(tachyonPredMotion1 * 0.68f + nextM * 0.32f, 0.0f, 2000.0f);
  tachyonPredPresence2 = constrain(tachyonPredPresence2 * 0.74f + (tachyonPredPresence1 + trendP * 0.45f) * 0.26f, 0.0f, 3000.0f);
  tachyonPredMotion2 = constrain(tachyonPredMotion2 * 0.74f + (tachyonPredMotion1 + trendM * 0.45f) * 0.26f, 0.0f, 2000.0f);
  tachyonPredPresence3 = constrain(tachyonPredPresence3 * 0.80f + (tachyonPredPresence2 + trendP * 0.22f) * 0.20f, 0.0f, 3000.0f);
  tachyonPredMotion3 = constrain(tachyonPredMotion3 * 0.80f + (tachyonPredMotion2 + trendM * 0.22f) * 0.20f, 0.0f, 2000.0f);

  float pNorm = constrain(max(tachyonPredPresence1, tmos_presence) / 850.0f, 0.0f, 2.0f);
  float mNorm = constrain(max(tachyonPredMotion1, tmos_motion) / 420.0f, 0.0f, 2.0f);
  tachyonFutureStress = constrain(tachyonFutureStress * 0.84f + (pNorm * 0.34f + mNorm * 0.42f + tachyonSwarmPressure * 0.14f + tachyonLossEma * 0.10f) * 0.16f, 0.0f, 2.5f);

  float futurePower = max(tachyonPredPresence1 / 850.0f, tachyonPredMotion1 / 420.0f);
  if (futurePower > 0.95f) tachyonEventEtaMs = 450.0f + (1.0f - min(1.0f, futurePower - 0.95f)) * 650.0f;
  else if (max(tachyonPredPresence2 / 850.0f, tachyonPredMotion2 / 420.0f) > 0.95f) tachyonEventEtaMs = 1200.0f;
  else if (max(tachyonPredPresence3 / 850.0f, tachyonPredMotion3 / 420.0f) > 0.95f) tachyonEventEtaMs = 2200.0f;
  else tachyonEventEtaMs = 9999.0f;

  tachyonLastPredPresence1 = tachyonPredPresence1;
  tachyonLastPredMotion1 = tachyonPredMotion1;

  tachyonPushSequence();
#endif
}

void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tp.magic[0] != 'T' || tp.magic[1] != 'P' || tp.version != 1) return;
  if (strncmp(tp.nodeId, "BlindEye", sizeof(tp.nodeId)) == 0) return;

  uint8_t slot = tachyonFindRemoteSlot(tp.nodeId);
  JanusRemoteProphecyState& r = tachyonRemotes[slot];
  r.active = true;
  strlcpy(r.nodeId, tp.nodeId[0] ? tp.nodeId : "node", sizeof(r.nodeId));
  r.lastSeenMs = millis();
  r.seq = tp.seq;
  r.sector = tp.sector % JANUS_KENSHI_SECTORS;
  r.predictedSector = tp.predictedSector % JANUS_KENSHI_SECTORS;
  r.confidence = tp.confidence;
  r.flags = tp.flags;
  r.presence_now = isfinite(tp.presence_now) ? tp.presence_now : 0.0f;
  r.motion_now = isfinite(tp.motion_now) ? tp.motion_now : 0.0f;
  r.pred_presence_1 = isfinite(tp.pred_presence_1) ? tp.pred_presence_1 : 0.0f;
  r.pred_motion_1 = isfinite(tp.pred_motion_1) ? tp.pred_motion_1 : 0.0f;
  r.pred_presence_2 = isfinite(tp.pred_presence_2) ? tp.pred_presence_2 : 0.0f;
  r.pred_motion_2 = isfinite(tp.pred_motion_2) ? tp.pred_motion_2 : 0.0f;
  r.pred_presence_3 = isfinite(tp.pred_presence_3) ? tp.pred_presence_3 : 0.0f;
  r.pred_motion_3 = isfinite(tp.pred_motion_3) ? tp.pred_motion_3 : 0.0f;
  r.future_stress = isfinite(tp.future_stress) ? tp.future_stress : 0.0f;
  r.swarm_pressure = isfinite(tp.swarm_pressure) ? tp.swarm_pressure : 0.0f;

  // Remote futures influence the local Markov gate only when sender is confident.
  if (tp.confidence > 45) kenshiTrainMarkov(kenshiSector, r.predictedSector, constrain((float)tp.confidence / 100.0f, 0.08f, 1.0f));

  tachyonRxPackets++;
  tachyonLastRxMs = millis();
  (void)rxRssi;
#endif
}

void sendTachyonProphecyPacket(bool force) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  uint32_t interval = (tachyonFutureStress > 0.95f || kenshiBubbleState >= 3) ? JANUS_TACHYON_TX_ALERT_MS : JANUS_TACHYON_TX_BG_MS;
  if (!force && now - tachyonLastTxMs < interval) return;

  JanusTachyonProphecyPacket tp{};
  tp.magic[0] = 'T'; tp.magic[1] = 'P';
  tp.version = 1;
  tp.flags = 0;
  if (tmos_presence_now) tp.flags |= 0x01;
  if (tmos_motion_now) tp.flags |= 0x02;
  if (tachyonFutureStress > 0.95f || kenshiBubbleState >= 3) tp.flags |= 0x04;
  if (tachyonRemoteCount > 0) tp.flags |= 0x08;
  strlcpy(tp.nodeId, "BlindEye", sizeof(tp.nodeId));
  tp.seq = ++tachyonSeq;
  tp.worker_id = colonyWorkerId;
  tp.uptime_ms = now;
  tp.horizon_ms = 2200;
  tp.sector = kenshiSector;
  tp.predictedSector = kenshiPredSector;
  tp.confidence = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 0, 100);
  tp.jobState = kenshiJobState;
  tp.presence_now = tmos_presence;
  tp.motion_now = tmos_motion;
  tp.pred_presence_1 = tachyonPredPresence1;
  tp.pred_motion_1 = tachyonPredMotion1;
  tp.pred_presence_2 = tachyonPredPresence2;
  tp.pred_motion_2 = tachyonPredMotion2;
  tp.pred_presence_3 = tachyonPredPresence3;
  tp.pred_motion_3 = tachyonPredMotion3;
  tp.event_eta_ms = tachyonEventEtaMs;
  tp.future_stress = tachyonFutureStress;
  tp.swarm_pressure = tachyonSwarmPressure;

  if (janusEyeEspNowSend("TP", &tp, sizeof(tp), true)) {
    tachyonTxPackets++;
    tachyonLastTxMs = now;
  }
#else
  (void)force;
#endif
}

void tachyonProphecyTick() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  sendTachyonProphecyPacket(false);
#endif
}

struct JanusTachyonSaveBlob {
  uint32_t magic;
  uint16_t version;
  float trendGain;
  float memoryGain;
  float remoteGain;
  float pConfidence;
  float mConfidence;
};

void saveTachyonState() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  JanusTachyonSaveBlob b{};
  b.magic = 0x54505932UL; // TPY2
  b.version = 1;
  b.trendGain = tachyonTrendGain;
  b.memoryGain = tachyonMemoryGain;
  b.remoteGain = tachyonRemoteGain;
  b.pConfidence = tachyonPresenceConfidence;
  b.mConfidence = tachyonMotionConfidence;
  LittleFS.remove(JANUS_TACHYON_SAVE_FILE);
  File f = LittleFS.open(JANUS_TACHYON_SAVE_FILE, "w");
  if (!f) return;
  f.write((const uint8_t*)&b, sizeof(b));
  f.close();
#endif
}

void loadTachyonState() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  File f = LittleFS.open(JANUS_TACHYON_SAVE_FILE, FILE_READ);
  if (!f) return;
  JanusTachyonSaveBlob b{};
  size_t got = f.read((uint8_t*)&b, sizeof(b));
  f.close();
  if (got != sizeof(b) || b.magic != 0x54505932UL || b.version != 1) return;
  tachyonTrendGain = constrain(b.trendGain, 0.18f, 1.20f);
  tachyonMemoryGain = constrain(b.memoryGain, 0.08f, 0.75f);
  tachyonRemoteGain = constrain(b.remoteGain, 0.0f, 0.45f);
  tachyonPresenceConfidence = constrain(b.pConfidence, 0.0f, 1.0f);
  tachyonMotionConfidence = constrain(b.mConfidence, 0.0f, 1.0f);
#endif
}

bool eyeVisionTargetsThisEye(const JanusEyeVisionControlPacket& ec) {
#if JANUS_EYE_VISION_ENABLE
  if (ec.magic[0] != 'E' || ec.magic[1] != 'C' || ec.version != 1) return false;
  if (ec.target[0] == 0) return true;
  if (!strcasecmp(ec.target, "all")) return true;
  if (!strcasecmp(ec.target, "*")) return true;
  if (!strcasecmp(ec.target, "BlindEye")) return true;
  if (!strcasecmp(ec.target, "atom_s3r_blind_eye")) return true;
  return strstr(ec.target, "BlindEye") || strstr(ec.target, "EYE");
#else
  return false;
#endif
}

void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec) {
#if JANUS_EYE_VISION_ENABLE
  if (!eyeVisionTargetsThisEye(ec)) {
    eyeVisionControlsIgnored++;
    return;
  }
  eyeVisionControlsRx++;
  eyeVisionEnabled = ec.enable != 0;
  eyeVisionFrameMs = ec.frameMs ? constrain((int)ec.frameMs, 90, 800) : JANUS_EYE_VISION_DEFAULT_FRAME_MS;
  eyeVisionLastControlMs = millis();
  strlcpy(eyeVisionSource, ec.source[0] ? ec.source : "Core2Home", sizeof(eyeVisionSource));
  if (eyeVisionEnabled) eyeVisionLastFrameMs = 0;
#endif
}

uint8_t eyeVisionSectorIntensity(uint8_t x, uint8_t y, uint8_t sector, float power) {
  // 8 sector ray field. This is an aperture image, not fake camera pixels:
  // it visualizes scalar TMOS/motion plus predicted direction for Core2.
  static const int8_t sx[8] = {4, 6, 7, 6, 4, 1, 0, 1};
  static const int8_t sy[8] = {0, 1, 4, 6, 7, 6, 4, 1};
  int dx = (int)x - sx[sector & 7];
  int dy = (int)y - sy[sector & 7];
  float d2 = (float)(dx * dx + dy * dy);
  float v = power * 255.0f / (1.0f + d2 * 0.42f);
  return (uint8_t)constrain((int)v, 0, 255);
}

