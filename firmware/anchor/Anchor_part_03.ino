  if (stride == 0) stride = 1UL;

  // Sector is a phase offset across the FULL assigned range. Older Anchor builds
  // restricted several lanes to one eighth but still ran `range` iterations,
  // re-hashing the same sector up to eight times. This keeps the brother split
  // while every lane can walk the whole Buzz range without that repetition.
  const uint32_t sector = j.minerSector % MINER_SECTORS;
  const uint32_t phase = (uint32_t)((((uint64_t)range * sector) / MINER_SECTORS +
                                     (j.minerStartOffset % range)) % range);

  uint32_t local = 0;
  switch (j.minerLane) {
    case 1: { // reverse, coprime stride: full-range permutation
      const uint32_t cursor = seed % range;
      const uint32_t walk = (uint32_t)(((uint64_t)index * stride) % range);
      local = (phase + cursor + range - walk) % range;
      break;
    }
    case 2: { // true bit-reversal permutation for power-of-two Buzz ranges
      if (janusIsPowerOfTwo(range)) {
        const uint32_t shifted = (index + (seed % range)) % range;
        local = (phase + janusBitReverseRange(shifted, range)) % range;
      } else {
        local = (phase + (seed % range) +
                 (uint32_t)(((uint64_t)index * stride) % range)) % range;
      }
      break;
    }
    case 3: { // center-out, no duplicate center
      const uint32_t center = range >> 1;
      const uint32_t step = (index + 1UL) >> 1;
      const uint32_t off = (index & 1UL)
                             ? (center + step) % range
                             : (center + range - (step % range)) % range;
      local = (phase + off) % range;
      break;
    }
    case 4: { // knight/golden phase, still a full-range permutation
      const uint32_t cursor = (seed ^ 0x9E3779B9UL) % range;
      local = (phase + cursor +
               (uint32_t)(((uint64_t)index * stride) % range)) % range;
      break;
    }
    case 5: { // deterministic random-looking affine permutation
      const uint32_t cursor = xorShift32(seed ^ 0xA5A5A5A5UL) % range;
      local = (phase + cursor +
               (uint32_t)(((uint64_t)index * stride) % range)) % range;
      break;
    }
    case 0:
    default:
      local = (phase + index) % range;
      break;
  }
  return j.startNonce + local;
}

uint16_t activeBatch() {
  uint16_t b = agentBatch ? agentBatch : REMOTE_BATCH_BASE;
  uint8_t localHint = anchorSchedulerHint();
  if (localHint == 2) b = min<uint16_t>(b, 220);
  if (localHint >= 3) b = max<uint16_t>(b, REMOTE_BATCH_BASE);
  if (anchorTranceptionHint >= 3 && anchorTranceptionLiteScore > 0.82f && ESP.getFreeHeap() > 90000) {
    b = (uint16_t)min<int>((int)REMOTE_BATCH_MAX, (int)b + 64);
  }
  if (anchorOxytocin > 58.0f && ESP.getFreeHeap() > 90000) {
    int oxyBoost = (int)((anchorOxytocin - 58.0f) * 3.0f);
    if (janusTwinPeerFresh()) oxyBoost += 24;
    if (janusTwinSameBuzzWindow(job) && janusTwinPeerBestBits >= bestBits) oxyBoost += 36;
    b = (uint16_t)min<int>((int)REMOTE_BATCH_MAX, (int)b + constrain(oxyBoost, 0, 180));
  } else if (anchorOxytocin < 24.0f || txFail > txOk + 30UL) {
    b = (uint16_t)max<int>((int)REMOTE_BATCH_MIN, (int)b - 80);
  }
  return constrain((int)b, REMOTE_BATCH_MIN, REMOTE_BATCH_MAX);
}

bool sendEspNow(const char* tag, const void* payload, size_t len) {
  if (!payload || !len) return false;
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) { txOk++; return true; }
  txFail++;
  lastTxErr = (int)err;
  Serial.printf("[ANCHOR/TXFAIL] tag=%s err=%d fail=%lu ch=%u\n", tag ? tag : "?", lastTxErr, (unsigned long)txFail, (unsigned)peerChannel);
  return false;
}

bool janusFacePeerFresh() {
  return janusFacePeerLastMs && (janusSafeAgeMs(millis(), janusFacePeerLastMs, 999999UL) < JANUS_FACE_PEER_TTL_MS);
}

bool janusFaceShareActive() {
  bool own = lastShareMs && (janusSafeAgeMs(millis(), lastShareMs, 0UL) < JANUS_FACE_SWAP_MS);
  bool peer = janusFacePeerLastMs &&
              (janusSafeAgeMs(millis(), janusFacePeerLastMs, 999999UL) < JANUS_FACE_SWAP_MS) &&
              (janusFacePeerFlags & 0x01);
  return own || peer;
}

uint8_t janusFaceBaseFace() {
  uint8_t base = JANUS_FACE_ROLE_ANCHOR;

  // If the twin reports the same static face, split by nodeId so two identical
  // boards still cannot breathe with the same face.
  if (janusFacePeerFresh() && janusFacePeerRoleFace == JANUS_FACE_ROLE_ANCHOR && janusFacePeerNode) {
    base = (workerId < janusFacePeerNode) ? JANUS_FACE_CYAN : JANUS_FACE_MAGENTA;
  }

  return base;
}

uint8_t janusFaceCurrentFace() {
  uint8_t face = janusFaceBaseFace();
  if (janusFaceShareActive()) face ^= 1;
  return face & 1;
}

float janusFacePhase(uint32_t now) {
  uint32_t seed = ((uint32_t)workerId * 977UL) ^ 0x4A4E5553UL;
  uint32_t offs = 700UL + (seed % 2600UL);
  if (janusFacePeerFresh() && workerId > janusFacePeerNode) offs += 2600UL;
  // Slow, soft breathing. One full perceived wave is roughly 45+ seconds.
  return (sinf(((float)now + (float)offs) / 7800.0f) + 1.0f) * 0.5f;
}

void janusFaceBroadcast(bool eventNow, uint16_t eventBits) {
#if JANUS_FACE_SYNC_ENABLE
  JanusFaceSyncPacket jf{};
  jf.magic[0] = 'J';
  jf.magic[1] = 'F';
  jf.version = 1;
  jf.roleFace = JANUS_FACE_ROLE_ANCHOR;
  jf.nodeId = workerId;
  jf.seq = ++janusFaceSeq;
  jf.uptimeMs = millis();
  jf.face = janusFaceCurrentFace();
  jf.brightness = ledBrightness;
  jf.eventBits = eventNow ? eventBits : (janusFaceShareActive() ? lastShareBits : 0);
  jf.flags = 0x08; // anchor
  if (eventNow || (lastShareMs && janusSafeAgeMs(millis(), lastShareMs, 0UL) < JANUS_FACE_SWAP_MS)) jf.flags |= 0x01;
  if (janusFacePeerFresh()) jf.flags |= 0x02;
  jf.colorSeed = ((uint32_t)workerId << 16) ^ ESP.getCycleCount() ^ 0xFACEA113UL;
  jf.crc = 0;
  jf.crc = rfDomeCrc32(&jf, sizeof(jf) - 4);
  sendEspNow("J/F", &jf, sizeof(jf));
  janusFaceLastTxMs = millis();
#else
  (void)eventNow; (void)eventBits;
#endif
}

bool janusFaceReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_FACE_SYNC_ENABLE
  if (!data || len != (int)sizeof(JanusFaceSyncPacket) || data[0] != 'J' || data[1] != 'F') return false;

  JanusFaceSyncPacket jf{};
  memcpy(&jf, data, sizeof(jf));
  uint32_t got = jf.crc;
  jf.crc = 0;
  uint32_t calc = rfDomeCrc32(&jf, sizeof(jf) - 4);
  if (got && got != calc) return true;

  if (jf.nodeId == workerId) return true;

  bool first = !janusFacePeerLastMs || janusFacePeerNode != jf.nodeId;
  janusFacePeerLastMs = millis();
  janusFacePeerNode = jf.nodeId;
  janusFacePeerRoleFace = jf.roleFace;
  janusFacePeerFace = jf.face;
  janusFacePeerFlags = jf.flags;
  janusFacePeerEventBits = jf.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  if (first || (jf.flags & 0x01)) {
    Serial.printf("[ANCHOR/FACE] peer=%04X face=%u roleFace=%u flags=0x%02X bits=%u rssi=%d myFace=%u swap=%u\\n",
                  jf.nodeId, jf.face, jf.roleFace, jf.flags, jf.eventBits, (int)rssi,
                  (unsigned)janusFaceCurrentFace(), janusFaceShareActive() ? 1 : 0);
  }

  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

void janusFaceTick(uint32_t now) {
#if JANUS_FACE_SYNC_ENABLE
  bool urgent = janusFaceShareActive() && (janusSafeAgeMs(now, janusFaceLastTxMs, 999999UL) > 350UL);
  if (urgent || janusSafeAgeMs(now, janusFaceLastTxMs, 999999UL) >= JANUS_FACE_TX_MS || janusFaceLastTxMs == 0) {
    janusFaceBroadcast(false, 0);
  }
#else
  (void)now;
#endif
}


void janusTwinTaskBroadcast(bool force=false) {
#if JANUS_TWIN_TASK_ENABLE
  uint32_t now = millis();
  bool active = job.active;
  bool shareActive = lastShareMs && (janusSafeAgeMs(now, lastShareMs, 0UL) < JANUS_FACE_SWAP_MS);
  bool urgent = shareActive || active || janusTwinPeerFresh();
  uint32_t interval = urgent ? JANUS_TWIN_TASK_TX_MS : (JANUS_TWIN_TASK_TX_MS * 4UL);
  if (!force && janusSafeAgeMs(now, janusTwinLastTxMs, 0UL) < interval) return;
  janusTwinLastTxMs = now;

  JanusTwinTaskPacket jt{};
  jt.magic[0] = 'J'; jt.magic[1] = 'T'; jt.version = 1;
  jt.role = JANUS_TWIN_ROLE_ANCHOR;
  jt.nodeId = workerId;
  jt.seq = ++janusTwinSeq;
  jt.uptimeMs = now;
  if (active) memcpy(jt.jobId8, job.job_id, 8);
  jt.jobFp32 = active ? janusTwinJobFp32From8(job.job_id) : 0;
  jt.jobStart = active ? job.startNonce : 0;
  jt.jobRange = active ? job.rangeSize : 0;
  jt.checked = active ? job.hashesDone : 0;
  jt.nonce = active ? job.nonce : bestNonce;
  jt.hashRate = hashRate;
  jt.bestBits = bestBits;
  jt.shares = shares;
  jt.jobsSeen = jobsSeen;
  jt.lane = active ? job.minerLane : 255;
  jt.sector = active ? job.minerSector : 255;
  jt.targetBits = targetBits;
  jt.stride = active ? job.minerStride : 0;
  jt.face = janusFaceCurrentFace();
  jt.brightness = ledBrightness;
  jt.eventBits = shareActive ? lastShareBits : 0;
  jt.flags = JANUS_TWIN_FLAG_ANCHOR;
  if (active) jt.flags |= JANUS_TWIN_FLAG_ACTIVE;
  if (shareActive) jt.flags |= JANUS_TWIN_FLAG_SHARE;
  if (janusTwinSameBuzzWindow(job)) jt.flags |= JANUS_TWIN_FLAG_SAME_JOB | JANUS_TWIN_FLAG_SPLIT;
  jt.rssi = lastRssi;
  jt.crc = 0;
  jt.crc = rfDomeCrc32(&jt, sizeof(jt) - 4);
  bool ok = sendEspNow("J/T", &jt, sizeof(jt));
  janusTwinTx++;
  if (force || shareActive || (janusTwinTx % 12UL) == 1UL || !ok) {
    const char* race = "solo";
    if (janusTwinPeerFresh() && active && jt.jobFp32 == janusTwinPeerJobFp32) {
      if (bestBits > janusTwinPeerBestBits) race = "ahead";
      else if (bestBits < janusTwinPeerBestBits) race = "behind";
      else race = (hashRate >= janusTwinPeerHashRate) ? "speed_ahead" : "speed_behind";
    }
    Serial.printf("[ANCHOR/TWIN] tx=%s seq=%lu peer=%04X fresh=%u job=%u fp=%08lX checked=%lu H=%lu best=%lu peerBest=%lu race=%s flags=0x%04X\n",
                  ok ? "OK" : "FAIL", (unsigned long)jt.seq, janusTwinPeerNode,
                  janusTwinPeerFresh() ? 1 : 0, active ? 1 : 0, (unsigned long)jt.jobFp32,
                  (unsigned long)jt.checked, (unsigned long)hashRate, (unsigned long)bestBits,
                  (unsigned long)janusTwinPeerBestBits, race, (unsigned)jt.flags);
  }
#else
  (void)force;
#endif
}

bool janusTwinTaskReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_TWIN_TASK_ENABLE
  if (!data || len != (int)sizeof(JanusTwinTaskPacket) || data[0] != 'J' || data[1] != 'T') return false;
  JanusTwinTaskPacket jt{};
  memcpy(&jt, data, sizeof(jt));
  uint32_t got = jt.crc;
  jt.crc = 0;
  uint32_t calc = rfDomeCrc32(&jt, sizeof(jt) - 4);
  if (got && got != calc) return true;
  if (jt.nodeId == workerId) return true;

  janusTwinRx++;
  bool first = !janusTwinPeerLastMs || janusTwinPeerNode != jt.nodeId;
  bool peerShare = (jt.flags & JANUS_TWIN_FLAG_SHARE);
  janusTwinPeerLastMs = millis();
  janusTwinPeerNode = jt.nodeId;
  janusTwinPeerRole = jt.role;
  janusTwinPeerJobFp32 = jt.jobFp32;
  janusTwinPeerJobStart = jt.jobStart;
  janusTwinPeerJobRange = jt.jobRange;
  janusTwinPeerChecked = jt.checked;
  janusTwinPeerHashRate = jt.hashRate;
  janusTwinPeerBestBits = jt.bestBits;
  janusTwinPeerShares = jt.shares;
  janusTwinPeerLane = jt.lane;
  janusTwinPeerSector = jt.sector;
  janusTwinPeerTargetBits = jt.targetBits;
  janusTwinPeerStride = jt.stride;
  janusTwinPeerFlags = jt.flags;
  janusTwinPeerRssi = rssi;

  janusFacePeerLastMs = millis();
  janusFacePeerNode = jt.nodeId;
  janusFacePeerRoleFace = (jt.role == JANUS_TWIN_ROLE_GLADIUS) ? JANUS_FACE_ROLE_GLADIUS : JANUS_FACE_ROLE_ANCHOR;
  janusFacePeerFace = jt.face;
  janusFacePeerFlags = peerShare ? 0x01 : 0x00;
  janusFacePeerEventBits = jt.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  if (job.active) janusTwinApplyLaneSplit(job);
  if (job.active && jt.jobFp32 == janusTwinJobFp32From8(job.job_id)) {
    if (bestBits > jt.bestBits) janusTwinRaceWins++;
    else if (bestBits < jt.bestBits) janusTwinRaceLosses++;
  }

  if (first || peerShare || (janusTwinRx % 10UL) == 1UL) {
    Serial.printf("[ANCHOR/TWIN] rx peer=%04X role=%u job=%u fp=%08lX H=%lu best=%lu shares=%lu flags=0x%04X rssi=%d myBest=%lu wins=%lu losses=%lu\n",
                  jt.nodeId, (unsigned)jt.role, (jt.flags & JANUS_TWIN_FLAG_ACTIVE) ? 1 : 0,
                  (unsigned long)jt.jobFp32, (unsigned long)jt.hashRate, (unsigned long)jt.bestBits,
                  (unsigned long)jt.shares, (unsigned)jt.flags, (int)rssi, (unsigned long)bestBits,
                  (unsigned long)janusTwinRaceWins, (unsigned long)janusTwinRaceLosses);
  }
  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

void janusTwinTaskTick(uint32_t now) {
#if JANUS_TWIN_TASK_ENABLE
  (void)now;
  janusTwinTaskBroadcast(false);
#else
  (void)now;
#endif
}



uint32_t janusJobFp32(const RemoteJobState& j) {
  return janusTwinJobFp32From8(j.job_id);
}

uint32_t janusJobFp32FromPacket(const JobPacket& jp) {
  return janusTwinJobFp32From8(jp.job_id);
}
RemoteJobState janusJobBuildFromPacket(const JobPacket& jp) {
  RemoteJobState j{};
  memcpy(j.job_id, jp.job_id, 8);
  memcpy(j.header, jp.header, 80);
  memcpy(j.target, jp.target, 32);
  j.startNonce = jp.start_nonce;
  j.rangeSize = jp.range_size;
  j.nonce = jp.start_nonce;
  j.hashesDone = 0;
  j.receivedAt = millis();
  j.active = true;
  return j;
}
bool janusJobSameStart(const RemoteJobState& a, const RemoteJobState& b) {
  return janusJobSameIdentity(a, b) &&
         a.startNonce == b.startNonce &&
         a.rangeSize == b.rangeSize;
}
void janusJobAccept(const RemoteJobState& incoming, const char* reason) {
  job = incoming;
  job.receivedAt = millis();
  job.hashesDone = 0;
  job.nonce = job.startNonce;
  job.active = true;
  configureMinerLane(job);
  targetBits = countLeadingZeroBitsBE(job.target);

  // Per-range race telemetry must describe the range that is actually running.
  // Lifetime records stay in bestBitsLifetime/bestNonceLifetime.
  bestBits = 0;
  bestNonce = 0;

  jobsAccepted++;
  lastMasterMs = millis();
  Serial.printf("[ANCHOR/JOB] accept=%lu seen=%lu reason=%s start=%08lX range=%lu fp=%08lX targetBits=%u lane=%s/s%u stride=%lu arm=%u q=%u qAge=%lums\n",
                (unsigned long)jobsAccepted, (unsigned long)jobsSeen, reason ? reason : "?",
                (unsigned long)job.startNonce, (unsigned long)job.rangeSize, (unsigned long)janusJobFp32(job),
                (unsigned)targetBits, laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                queuedJobValid ? 1 : 0,
