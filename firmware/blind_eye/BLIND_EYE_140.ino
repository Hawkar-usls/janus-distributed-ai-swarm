void updateKenshiVirtualWorld() {
#if JANUS_KENSHI_BUS_ENABLE
  kenshiDecayAndCountNodes();

  float sensorPower =
    constrain(tmos_presence / 350.0f, 0.0f, 1.4f) +
    constrain(tmos_motion / 180.0f, 0.0f, 1.6f) +
    constrain(mic_rms * 9.0f, 0.0f, 1.2f) +
    constrain(imu_shock / 5.0f, 0.0f, 1.1f) +
    constrain(rf_motion_energy / 8.0f + rf_presence_score * 0.50f, 0.0f, 1.2f) +
    constrain(loss * 1.4f, 0.0f, 1.0f);

  if (colonyMasterSeen) sensorPower += 0.18f;
  if (colonyJob.active) sensorPower += 0.22f;
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) sensorPower += 0.20f;

  kenshiEventPower = kenshiEventPower * 0.82f + sensorPower * 0.18f;
  kenshiSector = kenshiInferSector();

  if (kenshiSector != kenshiLastSector || kenshiEventPower > 0.40f) {
    kenshiTrainMarkov(kenshiLastSector, kenshiSector, kenshiEventPower);
    kenshiLastSector = kenshiSector;
  }
  kenshiPredSector = kenshiPredictNextSector(kenshiSector);

  kenshiWorldFlags = 0;
  if (tmos_presence > 10.0f) kenshiWorldFlags |= K_WORLD_PRESENCE;
  if (tmos_motion > 6.0f)    kenshiWorldFlags |= K_WORLD_MOTION;
  if (mic_rms > 0.015f)      kenshiWorldFlags |= K_WORLD_MIC;
  if (imu_shock > 1.30f)     kenshiWorldFlags |= K_WORLD_SHOCK;
  if (colonyMasterSeen)      kenshiWorldFlags |= K_WORLD_MASTER;
  if (colonyJob.active)      kenshiWorldFlags |= K_WORLD_JOB;
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) kenshiWorldFlags |= K_WORLD_AGENT;
  if (loss > 0.35f)          kenshiWorldFlags |= K_WORLD_UNSTABLE;
  if (loss > 0.12f)          kenshiWorldFlags |= K_WORLD_TRAINING;
  if (sync_hint < 0.45f)     kenshiWorldFlags |= K_WORLD_LOW_SYNC;
  if (rf_presence_now || rf_motion_now) kenshiWorldFlags |= K_WORLD_RF;

  if (kenshiEventPower > 2.05f || fabsf(z_activity) > 3.2f || fabsf(z_loss) > 3.0f) {
    kenshiBubbleState = 3;
    kenshiJobState = 3; // alert
  } else if (tmos_motion > 9.0f || tmos_presence > 25.0f || mic_rms > 0.02f || rf_motion_now || rf_presence_now) {
    kenshiBubbleState = 2;
    kenshiJobState = 2; // track
  } else if (loss > 0.12f || kenshiVirtualNodes > 0) {
    kenshiBubbleState = 1;
    kenshiJobState = 4; // learn/virtual schedule
  } else {
    kenshiBubbleState = 0;
    kenshiJobState = 1; // watch
  }

  float conf = sync_hint * 0.55f + fit * 0.18f + (1.0f / (1.0f + loss)) * 0.27f;
  kenshiConfidence = constrain(kenshiConfidence * 0.86f + conf * 0.14f, 0.0f, 1.5f);

  int p = (int)(kenshiEventPower * 58.0f + kenshiConfidence * 42.0f + kenshiActiveNodes * 8);
  if (kenshiBubbleState == 3) p += 55;
  if (colonyJob.active) p += 18;
  if (tachyonFutureStress > 0.85f) p += (int)(tachyonFutureStress * 22.0f);
  if (tachyonRemoteCount > 0) p += 6;
  kenshiPriority = (uint8_t)constrain(p, 0, 255);
#endif
}

void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi) {
#if JANUS_KENSHI_BUS_ENABLE
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (strncmp(kp.nodeId, "BlindEye", sizeof(kp.nodeId)) == 0) return;

  uint8_t slot = kenshiFindNodeSlot(kp.nodeId);
  JanusKenshiNode& n = kenshiNodes[slot];
  n.active = true;
  strlcpy(n.nodeId, kp.nodeId[0] ? kp.nodeId : "node", sizeof(n.nodeId));
  n.lastSeenMs = millis();
  n.seq = kp.seq;
  n.worldFlags = kp.worldFlags;
  n.entropy = isfinite(kp.entropy) ? kp.entropy : 0.0f;
  n.activity = isfinite(kp.activity) ? kp.activity : 0.0f;
  n.confidence = isfinite(kp.confidence) ? kp.confidence : 0.0f;
  n.sector = kp.sector % JANUS_KENSHI_SECTORS;
  n.predictedSector = kp.predictedSector % JANUS_KENSHI_SECTORS;
  n.jobState = kp.jobState;
  n.priority = kp.priority;
  n.rssi = rxRssi ? rxRssi : kp.rssi;
  n.flags = kp.flags;

  if (n.priority > 96 || (kp.flags & 0x03)) {
    kenshiTrainMarkov(kenshiSector, n.predictedSector, constrain(n.confidence, 0.05f, 1.0f));
  }

  kenshiRxPackets++;
  kenshiLastRxMs = millis();
#endif
}

void sendKenshiBubblePacket() {
#if JANUS_KENSHI_BUS_ENABLE
  JanusKenshiPacket kp{};
  kp.magic[0] = 'K';
  kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0;
  if (kenshiBubbleState >= 2) kp.flags |= 0x01;
  if (kenshiBubbleState >= 3) kp.flags |= 0x02;
  if (kenshiVirtualNodes > 0) kp.flags |= 0x04;
  if (motionBasePresent) kp.flags |= 0x08; // Atomic Motion Base / servo planner present
  strlcpy(kp.nodeId, "BlindEye", sizeof(kp.nodeId));
  kp.seq = ++kenshiSeq;
  kp.worker_id = colonyWorkerId;
  kp.uptime_ms = millis();
  kp.activeBubbleNodes = kenshiActiveNodes;
  kp.virtualNodes = kenshiVirtualNodes;
  kp.worldFlags = kenshiWorldFlags;
  kp.sector = kenshiSector;
  kp.predictedSector = kenshiPredSector;
  kp.jobState = kenshiJobState;
  kp.priority = kenshiPriority;
  kp.rssi = (int8_t)wifi_rssi;
  kp.entropy = eyeLocalEntropy();
  kp.activity = activity;
  kp.confidence = kenshiConfidence;
  kp.values[0] = tmos_presence;
  kp.values[1] = tmos_motion;
  kp.values[2] = mic_rms;
  kp.values[3] = imu_shock;
  kp.values[4] = loss;
  kp.values[5] = fit + (motionBasePresent ? (float)motionBaseServoAngle / 180.0f : 0.0f);

  if (janusEyeEspNowSend("K2", &kp, sizeof(kp), true)) {
    kenshiTxPackets++;
    kenshiLastTxMs = millis();
  }
#endif
}

void kenshiBubbleTick() {
#if JANUS_KENSHI_BUS_ENABLE
  uint32_t now = millis();
  uint32_t interval = JANUS_KENSHI_BG_TX_MS;
  if (kenshiBubbleState >= 3 || kenshiPriority >= 170) interval = JANUS_KENSHI_ALERT_TX_MS;
  else if (kenshiBubbleState >= 2 || kenshiPriority >= 90) interval = JANUS_KENSHI_ACTIVE_TX_MS;

  if (now - kenshiLastTxMs >= interval) sendKenshiBubblePacket();
#endif
}

struct JanusKenshiSaveBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t sectors;
  uint8_t lastSector;
  uint8_t reserved[3];
  float markov[JANUS_KENSHI_SECTORS][JANUS_KENSHI_SECTORS];
};

void saveKenshiState() {
#if JANUS_KENSHI_BUS_ENABLE
  if (!kenshiStateDirty) return;
  JanusKenshiSaveBlob b{};
  b.magic = 0x4B324559UL; // K2EY
  b.version = 1;
  b.sectors = JANUS_KENSHI_SECTORS;
  b.lastSector = kenshiLastSector;
  memcpy(b.markov, kenshiMarkov, sizeof(kenshiMarkov));

  LittleFS.remove(JANUS_KENSHI_SAVE_FILE);
  File f = LittleFS.open(JANUS_KENSHI_SAVE_FILE, "w");
  if (!f) return;
  f.write((const uint8_t*)&b, sizeof(b));
  f.close();
  kenshiStateDirty = false;
#endif
}

void loadKenshiState() {
#if JANUS_KENSHI_BUS_ENABLE
  File f = LittleFS.open(JANUS_KENSHI_SAVE_FILE, FILE_READ);
  if (!f) {
    for (uint8_t i = 0; i < JANUS_KENSHI_SECTORS; ++i) {
      for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
        kenshiMarkov[i][j] = (i == j) ? 0.18f : 0.02f;
      }
    }
    return;
  }

  JanusKenshiSaveBlob b{};
  size_t got = f.read((uint8_t*)&b, sizeof(b));
  f.close();

  if (got != sizeof(b) || b.magic != 0x4B324559UL || b.version != 1 || b.sectors != JANUS_KENSHI_SECTORS) {
    for (uint8_t i = 0; i < JANUS_KENSHI_SECTORS; ++i) {
      for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
        kenshiMarkov[i][j] = (i == j) ? 0.18f : 0.02f;
      }
    }
    return;
  }

  memcpy(kenshiMarkov, b.markov, sizeof(kenshiMarkov));
  kenshiLastSector = b.lastSector % JANUS_KENSHI_SECTORS;
#endif
}



// ========================= JANUS TACHYON PROPHECY + EYE VISION =========================

float tachyonSeqGet(float* arr, uint8_t back, float fallback) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tachyonSeqCount == 0) return fallback;
  if (back >= tachyonSeqCount) back = tachyonSeqCount - 1;
  int idx = (int)tachyonSeqPos - 1 - (int)back;
  while (idx < 0) idx += JANUS_TACHYON_SEQ_N;
  return arr[idx % JANUS_TACHYON_SEQ_N];
#else
  return fallback;
#endif
}

float tachyonWeightedMemory(float* arr, float fallback) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tachyonSeqCount == 0) return fallback;
  float sum = 0.0f;
  float wsum = 0.0f;
  uint8_t n = min<uint8_t>(tachyonSeqCount, 8);
  for (uint8_t i = 0; i < n; ++i) {
    float w = 1.0f / (1.0f + i * 0.55f);
    sum += tachyonSeqGet(arr, i, fallback) * w;
    wsum += w;
  }
  return (wsum > 0.001f) ? (sum / wsum) : fallback;
#else
  return fallback;
#endif
}

uint8_t tachyonFindRemoteSlot(const char* nodeId) {
  uint8_t freeSlot = 255;
  uint8_t oldestSlot = 0;
  uint32_t oldestAge = 0;
  for (uint8_t i = 0; i < JANUS_TACHYON_REMOTE_N; ++i) {
    if (tachyonRemotes[i].active && nodeId && nodeId[0] && strncmp(tachyonRemotes[i].nodeId, nodeId, sizeof(tachyonRemotes[i].nodeId)) == 0) return i;
    if (!tachyonRemotes[i].active && freeSlot == 255) freeSlot = i;
    uint32_t age = millis() - tachyonRemotes[i].lastSeenMs;
    if (age > oldestAge) { oldestAge = age; oldestSlot = i; }
  }
  return (freeSlot != 255) ? freeSlot : oldestSlot;
}

void tachyonBlendRemoteProphecies() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  float p = 0.0f;
  float m = 0.0f;
  float sp = 0.0f;
  float wsum = 0.0f;
  uint8_t count = 0;

  for (uint8_t i = 0; i < JANUS_TACHYON_REMOTE_N; ++i) {
    JanusRemoteProphecyState& r = tachyonRemotes[i];
    if (!r.active) continue;
    uint32_t age = now - r.lastSeenMs;
    if (age > JANUS_TACHYON_REMOTE_TTL_MS) {
      r.active = false;
      continue;
    }

    float ageW = constrain(1.0f - (float)age / (float)JANUS_TACHYON_REMOTE_TTL_MS, 0.0f, 1.0f);
    float cW = constrain((float)r.confidence / 100.0f, 0.05f, 1.0f);
    float w = ageW * cW;
    p += r.pred_presence_1 * w;
    m += r.pred_motion_1 * w;
    sp += (r.future_stress + r.swarm_pressure) * 0.5f * w;
    wsum += w;
    count++;
  }

  tachyonRemoteCount = count;
  if (wsum > 0.001f) {
    tachyonRemotePresence = tachyonRemotePresence * 0.86f + (p / wsum) * 0.14f;
    tachyonRemoteMotion = tachyonRemoteMotion * 0.86f + (m / wsum) * 0.14f;
    tachyonSwarmPressure = tachyonSwarmPressure * 0.88f + constrain(sp / wsum, 0.0f, 2.0f) * 0.12f;
  } else {
    tachyonRemotePresence *= 0.96f;
    tachyonRemoteMotion *= 0.96f;
    tachyonSwarmPressure *= 0.94f;
  }
#endif
}

void tachyonPushSequence() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  tachyonSeqPresence[tachyonSeqPos] = constrain(tmos_presence, 0.0f, 3000.0f);
  tachyonSeqMotion[tachyonSeqPos] = constrain(tmos_motion, 0.0f, 2000.0f);
  tachyonSeqActivity[tachyonSeqPos] = constrain(activity, 0.0f, 20.0f);
  tachyonSeqPos = (tachyonSeqPos + 1) % JANUS_TACHYON_SEQ_N;
  if (tachyonSeqCount < JANUS_TACHYON_SEQ_N) tachyonSeqCount++;
#endif
}

