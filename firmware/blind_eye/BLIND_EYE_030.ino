uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i) {
#if JANUS_MINER_V31_SCHEDULER_ENABLE
  uint32_t range = job.rangeSize ? job.rangeSize : COLONY_JOB_RANGE_DEFAULT;
  if (range == 0) range = 1;
  uint32_t sectors = JANUS_MINER_V31_SECTORS;
  uint32_t sector = job.minerSector % sectors;
  uint32_t sectorWidth = max<uint32_t>(1UL, range / sectors);
  uint32_t sectorStart = min<uint32_t>(range - 1UL, sector * sectorWidth);
  if (sector == sectors - 1 || sectorStart + sectorWidth > range) sectorWidth = range - sectorStart;
  sectorWidth = max<uint32_t>(1UL, sectorWidth);

  uint32_t local = 0;
  uint32_t stride = job.minerStride | 1UL;
  uint32_t seed = job.minerSeed ^ job.minerStartOffset;

  switch (job.minerLane) {
    case 1: { // zim_reverse: seeded cursor, odd reverse stride.
      uint32_t cursor = seed % sectorWidth;
      uint32_t walk = (uint32_t)(((uint64_t)(i % sectorWidth) * stride) % sectorWidth);
      local = sectorStart + ((cursor + sectorWidth - walk) % sectorWidth);
      break;
    }
    case 2: // bitrev: jump across scales.
      local = janusBitReverse32(seed + i) % range;
      break;
    case 3: { // janus_center: center, -1, +1, -2, +2...
      uint32_t step = (i + 1UL) >> 1;
      uint32_t center = sectorWidth >> 1;
      uint32_t off = (i & 1UL) ? (center + step) : (center + sectorWidth - (step % sectorWidth));
      local = sectorStart + (off % sectorWidth);
      break;
    }
    case 4: // knight: golden-ratio odd walk inside a locked sector.
      local = sectorStart + ((seed + i * 0x9E3779B9UL) % sectorWidth);
      break;
    case 5: { // random baseline, deterministic/reproducible per job.
      uint32_t x = janusXorShift32(seed + i * 0xA5A5A5A5UL);
      local = x % range;
      break;
    }
    case 0:
    default:
      local = (job.minerStartOffset + i) % range;
      break;
  }
  return job.startNonce + local;
#else
  return job.nonce;
#endif
}

bool looksLikeBuzzMaster(const JanusColonyPacket& pkt) {
  return strstr(pkt.nodeId, "Buzz") || strstr(pkt.nodeId, "BUZZ") ||
         strstr(pkt.role, "Buzz") || strstr(pkt.role, "BUZZ") ||
         strstr(pkt.role, "MASTER") || strstr(pkt.role, "Master");
}

bool agentRewardTargetsThisEye(const JanusAgentRewardPacket& ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return false;
  if (ar.targetNode[0] == '\0') return true;
  if (!strcmp(ar.targetNode, "*")) return true;
  if (!strcasecmp(ar.targetNode, "all")) return true;
  if (!strcasecmp(ar.targetNode, "BlindEye")) return true;
  if (!strcasecmp(ar.targetNode, "atom_s3r_blind_eye")) return true;
  return strstr(ar.targetNode, "BlindEye") || strstr(ar.targetNode, "EYE");
}

uint16_t effectiveColonyRemoteBatch() {
#if COLONY_AGENT_ENABLE
  uint16_t b = colonyAgentTargetBatch ? colonyAgentTargetBatch : COLONY_REMOTE_BATCH;

  // Headless Eye keeps sensors responsive. Agent may boost, but not enough to starve sensors.
  if (colonyAgentHint == 2) b = min<uint16_t>(b, 180);
  if (colonyAgentHint == 3 && colonyMasterSeen) b = max<uint16_t>(b, COLONY_REMOTE_BATCH);

  return constrain((int)b, COLONY_AGENT_BATCH_MIN, COLONY_AGENT_BATCH_MAX);
#else
  return COLONY_REMOTE_BATCH;
#endif
}

void onJanusAgentReward(const JanusAgentRewardPacket& ar) {
#if COLONY_AGENT_ENABLE
  if (!agentRewardTargetsThisEye(ar)) return;

  colonyAgentRewardsRx++;
  colonyAgentSeq = ar.seq;
  colonyAgentLevel = ar.rewardLevel;
  colonyAgentHint = ar.aiHint ? ar.aiHint : 1;
  colonyAgentRewardPoints = ar.rewardPoints;
  colonyAgentTargetBatch = ar.targetBatch ? ar.targetBatch : COLONY_REMOTE_BATCH;
  colonyAgentTargetBatch = constrain((int)colonyAgentTargetBatch, COLONY_AGENT_BATCH_MIN, COLONY_AGENT_BATCH_MAX);
  colonyAgentEntropySeed ^= ar.entropySeed ^ micros() ^ ((uint32_t)ar.rewardLevel << 24);
  colonyAgentLastRewardMs = millis();
  colonyAgentLastDeltaShares = ar.deltaShares;
  colonyAgentScore = ar.score;
  colonyAgentPredictedHash = ar.predictedHashRate;
  colonyAgentPredictionError = ar.predictionError;
  strlcpy(colonyAgentSource, ar.source, sizeof(colonyAgentSource));

  if (ar.deltaShares > 0 || ar.rewardLevel >= 3) colonyAgentShareRewardsRx++;

  // Do not touch status/model globals here: they are declared later.
  // updateMiniGPT() consumes colonyAgent* state safely after all model globals exist.

  Serial.printf("[AGENT] RX seq=%lu lvl=%u hint=%u pts=%u batch=%u score=%.1f predH=%.1f err=%.3f dShare=%lu\n",
                (unsigned long)ar.seq,
                (unsigned)ar.rewardLevel,
                (unsigned)ar.aiHint,
                (unsigned)ar.rewardPoints,
                (unsigned)colonyAgentTargetBatch,
                ar.score,
                ar.predictedHashRate,
                ar.predictionError,
                (unsigned long)ar.deltaShares);
#endif
}

void onJanusHeartbeat(const JanusColonyPacket& pkt);
void onJanusEntropy(const EntropyReport& er, const void* opt);
void onJanusEntropyV2(const EntropyReportV2& er2);
void onJanusAgentReward(const JanusAgentRewardPacket& ar);
void sendNodeHeartbeat();
void sendNodeEntropy();
float eyeLocalEntropy();
void updateKenshiVirtualWorld();
void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void sendKenshiBubblePacket();
void kenshiBubbleTick();
void saveKenshiState();
void loadKenshiState();
void updateTachyonProphecy();
void sendTachyonProphecyPacket(bool force=false);
void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
void tachyonProphecyTick();
void saveTachyonState();
void loadTachyonState();
void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec);
void eyeVisionTick();
void sendEyeVisionFrame(bool eventSnapshot);

void onJanusPolicyPacket(const JanusPolicyPacket& jp);
bool janusEmitEyeEvent(uint8_t eventType, uint8_t confidence, uint8_t urgency,
                       int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                       uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs);
void janusEventTick(bool force=false);
void initMotionBase();
void motionBaseTick();
void motionBaseSafeStop();
void motionBaseStopCrawler(const char* reason);
void motionBaseCrawlerTick(uint32_t now);
bool motionBaseWriteServoAngle(uint8_t ch, uint8_t angle, const char* tag);
void handleRoboZombieSerial();
void motionBasePlanTarget();
void motionBaseSendStatusEvent(bool force=false);
void motionBaseSendPowerPacket(bool force=false);
void ensureColonyPeer();
bool janusEyeEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail=true);
bool forceColonyPeerRebuild(const char* reason);

void janusEyeRecordEpisode(uint8_t eventType, uint8_t confidence, uint8_t urgency, uint8_t flags=0);
void janusEyeSemanticTick(uint32_t now, bool force=false);
void janusEyeSwarmSenseTick(uint32_t now, bool force=false);
void rfLiteOnPacketRssi(int8_t rssi);
void rfLiteTick(uint32_t now);
void rfLiteDebugTick(uint32_t now, bool force=false);
float rfLiteFusionScore();
bool tmosWarmupActive(uint32_t now);
uint8_t janusPolicySmoothMood(uint8_t rawMood, uint8_t confidence, uint32_t now);
const char* janusMoodName(uint8_t mood);
uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i);
void colonyMinerConfigureForJob(RemoteJobState& job);
const char* colonyMinerLaneName(uint8_t lane);


void sendShareResponse(const RemoteJobState& job, uint32_t nonce) {
  ShareResponse sr{};
  sr.magic[0] = 'S'; sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = colonyWorkerId;
  janusEyeEspNowSend("share", &sr, sizeof(sr), true);
  // This is a worker-found ticket sent to Buzz. Pool ACCEPT is counted by Buzz.
  colonyRemoteShares++;
}

void runRemoteMiningBatch() {
  uint32_t now = millis();

  if (!colonyJob.active) {
    if (now - colonyLastHashTickMs >= 1000) {
      colonyRemoteHashrate = 0;
      colonyHashCounter = 0;
      colonyLastHashTickMs = now;
    }
    return;
  }

  if (now - colonyJob.receivedAt > COLONY_MASTER_TIMEOUT_MS) {
    colonyJob.active = false;
    colonyJobsExpired++;
    strlcpy(colonyMode, "SEEK", sizeof(colonyMode));
    colonyRemoteHashrate = 0;
    return;
  }

  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t shareHash[32];

  uint16_t activeBatch = effectiveColonyRemoteBatch();
  for (uint16_t i = 0; i < activeBatch; i++) {
    if (colonyJob.hashesDone >= colonyJob.rangeSize) {
      colonyJob.active = false;
      colonyJobsDone++;
      strlcpy(colonyMode, "WAIT", sizeof(colonyMode));
      break;
    }

    uint32_t nonce = colonyNextNonceV31(colonyJob, colonyJob.hashesDone);
    colonyJob.nonce = nonce + 1;
    colonyJob.hashesDone++;

    memcpy(header, colonyJob.header, 80);
    writeLE32(header + 76, nonce);

    doubleSha256(header, 80, rawHash);
    hashToShareOrder(rawHash, shareHash);

    colonyHashCounter++;
    uint16_t bits = countLeadingZeroBitsBE(shareHash);
    if (bits > colonyBestBits) {
      colonyBestBits = bits;
      colonyMinerBestNonce = nonce;
    }
    if (bits >= 22) colonyMinerTailHits++;

    if ((bits >= colonyTargetBits) && hashMeetsTargetBE(shareHash, colonyJob.target)) {
      sendShareResponse(colonyJob, nonce);
      colonyJob.active = false;
      colonyJobsDone++;
      strlcpy(colonyMode, "TICKET", sizeof(colonyMode));
      break;
    }
  }

  if (now - colonyLastHashTickMs >= 1000) {
    colonyRemoteHashrate = colonyHashCounter;
    colonyHashCounter = 0;
    colonyLastHashTickMs = now;
  }
}

uint8_t getWifiChannelSafe() {
  uint8_t primary = 0; wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

bool forceColonyPeerRebuild(const char* reason) {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return false;

  uint8_t ch = getWifiChannelSafe();
  if (ch == 0 && WiFi.status() == WL_CONNECTED) ch = WiFi.channel();
  if (ch == 0) ch = 1;

  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) {
    esp_now_del_peer(JANUS_BROADCAST_MAC);
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = ch;
  peer.encrypt = false;

  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
    colonyPeerChannel = ch;
    colonyPeerRebuilds++;
    Serial.printf("[COLONY/EYE] peer ready ch=%u rebuilds=%lu reason=%s\n",
                  (unsigned)ch, (unsigned long)colonyPeerRebuilds, reason ? reason : "-");
    return true;
  }

  colonyPeerChannel = 0;
  Serial.printf("[COLONY/EYE] peer rebuild FAIL err=%d ch=%u reason=%s\n",
                (int)err, (unsigned)ch, reason ? reason : "-");
  return false;
#else
  (void)reason;
  return false;
#endif
}

void ensureColonyPeer() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;

  uint8_t ch = getWifiChannelSafe();
  if (ch == 0 && WiFi.status() == WL_CONNECTED) ch = WiFi.channel();
  if (ch == 0) ch = 1;

  bool exists = esp_now_is_peer_exist(JANUS_BROADCAST_MAC);
  if (exists && colonyPeerChannel == ch) return;

  // Non-destructive fast path: only rebuild when the peer vanished or channel changed.
  forceColonyPeerRebuild(exists ? "channel-change" : "ensure");
#endif
}

