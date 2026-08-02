  return n;
}

void janusCopyWireField(const char* field, size_t fieldCap, char* out, size_t outCap) {
  if (!out || outCap == 0) return;
  size_t n = janusWireFieldLength(field, fieldCap);
  if (n >= outCap) n = outCap - 1;
  if (field && n) memcpy(out, field, n);
  out[n] = '\0';
}

bool janusWireCharEqual(char a, char b, bool ignoreCase) {
  if (!ignoreCase) return a == b;
  return tolower((unsigned char)a) == tolower((unsigned char)b);
}

bool janusWireFieldEquals(const char* field, size_t fieldCap, const char* text, bool ignoreCase = false) {
  if (!field || !text) return false;
  size_t fieldLen = janusWireFieldLength(field, fieldCap);
  size_t textLen = strlen(text);
  if (fieldLen != textLen) return false;
  for (size_t i = 0; i < textLen; ++i) {
    if (!janusWireCharEqual(field[i], text[i], ignoreCase)) return false;
  }
  return true;
}

bool janusWireFieldContains(const char* field, size_t fieldCap, const char* needle, bool ignoreCase = false) {
  if (!field || !needle || !needle[0]) return false;
  size_t fieldLen = janusWireFieldLength(field, fieldCap);
  size_t needleLen = strlen(needle);
  if (needleLen > fieldLen) return false;
  for (size_t i = 0; i + needleLen <= fieldLen; ++i) {
    bool match = true;
    for (size_t j = 0; j < needleLen; ++j) {
      if (!janusWireCharEqual(field[i + j], needle[j], ignoreCase)) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

bool janusTargetAllZero(const uint8_t target[32]) {
  if (!target) return true;
  uint8_t any = 0;
  for (uint8_t i = 0; i < 32; ++i) any |= target[i];
  return any == 0;
}

uint32_t janusGcd32(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

uint32_t janusCoprimeStride(uint32_t stride, uint32_t range) {
  if (range <= 1UL) return 1UL;
  stride %= range;
  if (stride == 0) stride = 1;
  if ((range & 1UL) == 0) stride |= 1UL;
  uint32_t attempts = 0;
  while (janusGcd32(stride, range) != 1UL && attempts++ < 128UL) {
    stride += 2UL;
    if (stride >= range) stride = (stride % range) | 1UL;
  }
  return janusGcd32(stride, range) == 1UL ? stride : 1UL;
}

bool janusIsPowerOfTwo(uint32_t v) {
  return v && ((v & (v - 1UL)) == 0);
}

uint8_t janusLog2Pow2(uint32_t v) {
  uint8_t bits = 0;
  while (v > 1UL) {
    v >>= 1;
    bits++;
  }
  return bits;
}

uint32_t janusBitReverseRange(uint32_t value, uint32_t range) {
  if (range <= 1UL) return 0UL;
  if (!janusIsPowerOfTwo(range)) return value % range;
  uint8_t bits = janusLog2Pow2(range);
  if (bits == 0) return 0UL;
  return bitReverse32(value) >> (32U - bits);
}

bool janusJobSameIdentity(const RemoteJobState& a, const RemoteJobState& b) {
  // Match Gladius/Buzz Consilium semantics: one job_id can carry many nonce
  // windows, while start/range distinguish individual slices.
  return memcmp(a.job_id, b.job_id, sizeof(a.job_id)) == 0;
}


uint16_t countLeadingZeroBitsBE(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t b = h[i];
    if (b == 0) { bits += 8; continue; }
    for (int k = 7; k >= 0; k--) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

void writeLE32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void doubleSha256(const uint8_t *data, size_t len, uint8_t out[32]) {
  uint8_t first[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, first);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, first, 32);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

bool hashMeetsTargetBE(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

uint32_t bitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  x = (x << 16) | (x >> 16);
  return x;
}

uint32_t xorShift32(uint32_t x) {
  if (!x) x = 0xA5A5A5A5UL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

const char* laneName(uint8_t lane) {
  switch (lane) {
    case 0: return "linear";
    case 1: return "zim_reverse";
    case 2: return "bitrev";
    case 3: return "janus_center";
    case 4: return "knight";
    case 5: return "random";
    default: return "linear";
  }
}

uint32_t strideArmValue(uint8_t arm) {
  static const uint32_t arms[] = {
    1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL,
    4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
  };
  return arms[arm % (sizeof(arms) / sizeof(arms[0]))] | 1UL;
}

uint32_t janusTwinJobFp32From8(const uint8_t jid[8]) {
  uint32_t h = 0xB00B5EEDUL;
  for (uint8_t i = 0; i < 8; i++) { h ^= jid[i]; h *= 16777619UL; }
  return h;
}

bool janusTwinPeerFresh() {
#if JANUS_TWIN_TASK_ENABLE
  return janusTwinPeerLastMs && (janusSafeAgeMs(millis(), janusTwinPeerLastMs, 999999UL) < JANUS_TWIN_TASK_PEER_TTL_MS);
#else
  return false;
#endif
}

bool janusTwinSameBuzzWindow(const RemoteJobState& j) {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinPeerFresh()) return false;
  if (!j.active) return false;
  uint32_t fp = janusTwinJobFp32From8(j.job_id);
  if (!fp || fp != janusTwinPeerJobFp32) return false;
  // v1.19: same Buzz work means same job fingerprint, not necessarily same startNonce.
  // Buzz deliberately hands brothers different nonce windows; requiring equal start
  // made the split logic blind exactly when it was needed most.
  if (janusTwinPeerTargetBits && targetBits && janusTwinPeerTargetBits != targetBits) return false;
  return true;
#else
  (void)j;
  return false;
#endif
}

void anchorTorricelliBondTick(uint32_t now) {
  if (anchorTorricelliLastMs && janusSafeAgeMs(now, anchorTorricelliLastMs, 0UL) < 850UL) return;
  float dt = anchorTorricelliLastMs ? min(4.0f, (float)janusSafeAgeMs(now, anchorTorricelliLastMs, 0UL) / 1000.0f) : 1.0f;
  anchorTorricelliLastMs = now;

  bool twinFresh = janusTwinPeerFresh();
  bool sameJob = janusTwinSameBuzzWindow(job);
  uint32_t txTotal = txOk + txFail + 8UL;
  float radioClean = constrain(1.0f - ((float)txFail / (float)txTotal), 0.0f, 1.0f);
  uint16_t target = targetBits ? targetBits : 22;
  float progress = constrain((float)bestBits / (float)max<uint16_t>(1, target), 0.0f, 1.8f);
  bool brotherAhead = twinFresh && (janusTwinPeerBestBits > bestBits || janusTwinPeerHashRate > hashRate + 800UL);
  bool freshShare = lastShareMs && janusSafeAgeMs(now, lastShareMs, 999999UL) < 30000UL;

  float pressure = 0.0f;
  pressure += twinFresh ? 0.40f : -0.28f;
  pressure += sameJob ? 0.28f : 0.0f;
  pressure += brotherAhead ? 0.26f : 0.08f;
  pressure += freshShare ? 0.42f : 0.0f;
  pressure += radioClean * 0.18f + progress * 0.16f;
  pressure -= (txFail > txOk + 20UL) ? 0.35f : 0.0f;

  anchorOxytocin = constrain(anchorOxytocin + pressure * dt, 0.0f, 100.0f);

  float vacuumTarget = constrain(0.20f + radioClean * 0.34f + (twinFresh ? 0.22f : 0.0f) +
                                 (sameJob ? 0.16f : 0.0f) + progress * 0.08f -
                                 rfEntropy * 0.025f, 0.0f, 1.0f);
  anchorTorricelliVacuum = constrain(anchorTorricelliVacuum * 0.92f + vacuumTarget * 0.08f, 0.0f, 1.0f);
}

uint8_t anchorSchedulerHint() {
  uint8_t hint = agentHint ? agentHint : 1;
  if (anchorTranceptionHint > hint) hint = anchorTranceptionHint;
  return (uint8_t)constrain((int)hint, 1, 4);
}

void anchorTranceptionLiteTick(uint32_t now) {
  if (anchorTranceptionLastMs &&
      janusSafeAgeMs(now, anchorTranceptionLastMs, 0UL) < ANCHOR_TRANCEPTION_LITE_MS) {
    return;
  }
  anchorTranceptionLastMs = now;

  uint16_t target = targetBits ? targetBits : 22;
  float bestFit = constrain((float)bestBits / (float)max<uint16_t>(1, target), 0.0f, 1.65f);
  float hashFit = constrain((float)hashRate / 22000.0f, 0.0f, 1.35f);
  float txTotal = (float)(txOk + txFail + 8UL);
  float radioClean = constrain(1.0f - ((float)txFail / txTotal), 0.0f, 1.0f);
  float twin = janusTwinPeerFresh() ? 1.0f : 0.0f;
  float sameJob = janusTwinSameBuzzWindow(job) ? 1.0f : 0.0f;
  float oxy = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  float tailFit = constrain((float)tailHits / 96.0f, 0.0f, 1.0f);
  float jobAgePenalty = 0.0f;
  if (job.active) {
    uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
    jobAgePenalty = age > JOB_TIMEOUT_MS ? 0.22f : constrain((float)age / 90000.0f, 0.0f, 0.12f);
  }

  float score = 0.0f;
  score += bestFit * 0.34f;
  score += hashFit * 0.18f;
  score += radioClean * 0.14f;
  score += twin * 0.08f + sameJob * 0.08f;
  score += anchorTorricelliVacuum * 0.08f + oxy * 0.06f + tailFit * 0.04f;
  score -= jobAgePenalty;
  anchorTranceptionLiteScore = constrain(score, 0.0f, 1.50f);

  uint8_t oldHint = anchorTranceptionHint;
  if (anchorTranceptionLiteScore >= 0.98f || bestBits >= target) anchorTranceptionHint = 4;
  else if (anchorTranceptionLiteScore >= 0.78f) anchorTranceptionHint = 3;
  else if (anchorTranceptionLiteScore < 0.46f || radioClean < 0.72f) anchorTranceptionHint = 2;
  else anchorTranceptionHint = 1;

  if (anchorTranceptionHint >= 4) anchorTranceptionLane = (bestBits & 1U) ? 4 : 3;
  else if (anchorTranceptionHint >= 3) anchorTranceptionLane = (uint8_t)((workerId + bestBits + tailHits) % 3U) + 3U;
  else if (anchorTranceptionHint == 2) anchorTranceptionLane = 1;
  else anchorTranceptionLane = 0;

  anchorTranceptionReports++;
  if (oldHint != anchorTranceptionHint || (anchorTranceptionReports & 0x0FUL) == 1UL) {
    Serial.printf("[ANCHOR/TL] score=%.3f hint=%u lane=%s best=%lu/%u H=%lu clean=%.2f twin=%u same=%u oxy=%.1f vac=%.2f tail=%lu wire=frozen\n",
                  anchorTranceptionLiteScore, (unsigned)anchorTranceptionHint,
                  laneName(anchorTranceptionLane), (unsigned long)bestBits, (unsigned)target,
                  (unsigned long)hashRate, radioClean, twin > 0.5f ? 1 : 0,
                  sameJob > 0.5f ? 1 : 0, anchorOxytocin, anchorTorricelliVacuum,
                  (unsigned long)tailHits);
  }
}

void janusTwinApplyLaneSplit(RemoteJobState& j) {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinSameBuzzWindow(j)) return;
  uint8_t oldSector = j.minerSector;
  uint8_t oldLane = j.minerLane;
  uint32_t oldStride = j.minerStride;

  // Anchor takes the even/high-energy claws, Gladius normally takes the opposite
  // side. If peer reports the same sector, rotate away deterministically.
  static const uint8_t anchorSectors[4] = {6, 4, 2, 0};
  uint8_t pick = (uint8_t)(((j.minerSeed >> 24) ^ jobsSeen ^ janusTwinPeerNode) & 0x03);
  j.minerSector = anchorSectors[pick] % MINER_SECTORS;
  if (janusTwinPeerSector < MINER_SECTORS && j.minerSector == janusTwinPeerSector) {
    j.minerSector = anchorSectors[(pick + 1) & 0x03] % MINER_SECTORS;
  }

  // If both brothers accidentally choose the same walk style, Anchor phase-shifts
  // the lane/stride instead of brute-copying Gladius' path.
  if (j.minerLane == janusTwinPeerLane) {
    j.minerLane = (uint8_t)((j.minerLane + 2 + (workerId & 1)) % 6);
    if (j.minerLane == 0 && janusTwinPeerLane == 0) j.minerLane = 3;
  }
  if (j.minerStride == janusTwinPeerStride) {
    j.minerStrideArm = (uint8_t)((j.minerStrideArm + 5) % 17);
    j.minerStride = janusCoprimeStride(strideArmValue(j.minerStrideArm),
                                      j.rangeSize ? j.rangeSize : JOB_RANGE_DEFAULT);
  }

  j.minerStartOffset ^= 0x6A09E667UL ^ ((uint32_t)janusTwinPeerNode << 8) ^ ((uint32_t)j.minerSector << 24);
  if (oldSector != j.minerSector || oldLane != j.minerLane || oldStride != j.minerStride) {
    janusTwinSplitApplied++;
    Serial.printf("[ANCHOR/TWIN] split Buzz window peer=%04X fp=%08lX sector %u->%u lane %s->%s stride %lu->%lu splits=%lu\n",
                  janusTwinPeerNode, (unsigned long)janusTwinPeerJobFp32,
                  (unsigned)oldSector, (unsigned)j.minerSector, laneName(oldLane), laneName(j.minerLane),
                  (unsigned long)oldStride, (unsigned long)j.minerStride, (unsigned long)janusTwinSplitApplied);
  }
#else
  (void)j;
#endif
}
void configureMinerLane(RemoteJobState& j) {
  uint32_t seed = micros() ^ ESP.getCycleCount() ^ agentEntropySeed ^ (jobsSeen << 16) ^ workerId;
  for (uint8_t i = 0; i < 8; ++i) seed = xorShift32(seed ^ j.job_id[i]);
  uint8_t localHint = anchorSchedulerHint();
  j.minerSeed = seed;
  j.minerStrideArm = (uint8_t)((seed ^ (seed >> 8) ^ agentLevel ^ anchorTranceptionHint) % 17);
  j.minerStride = strideArmValue(j.minerStrideArm);

  uint8_t selector = (uint8_t)((seed ^ (seed >> 11) ^ localHint ^ jobsSeen) % 100);
  if (localHint >= 3 || agentLevel >= 2) {
    if (selector < 42) j.minerLane = 1;
    else if (selector < 64) j.minerLane = 4;
    else if (selector < 80) j.minerLane = 2;
    else if (selector < 92) j.minerLane = 3;
    else j.minerLane = 5;
  } else {
    if (selector < 38) j.minerLane = 0;
    else if (selector < 67) j.minerLane = 1;
    else if (selector < 80) j.minerLane = 2;
    else if (selector < 92) j.minerLane = 3;
    else j.minerLane = 5;
  }

  if (j.minerLane == 0 || j.minerLane == 1) j.minerSector = 6 % MINER_SECTORS;
  else if (j.minerLane == 4) j.minerSector = 7 % MINER_SECTORS;
  else j.minerSector = (uint8_t)((seed >> 24) % MINER_SECTORS);
  if (anchorTranceptionHint >= 3 && anchorTranceptionLane <= 5 && ((seed >> 5) & 0x03U) == 0) {
    j.minerLane = anchorTranceptionLane;
  }

  j.minerStartOffset = bitReverse32(seed ^ 0xC4111903UL);
  j.minerStride = janusCoprimeStride(j.minerStride, j.rangeSize ? j.rangeSize : JOB_RANGE_DEFAULT);
  janusTwinApplyLaneSplit(j);
  j.minerStride = janusCoprimeStride(j.minerStride, j.rangeSize ? j.rangeSize : JOB_RANGE_DEFAULT);
  laneSwitches++;
}
uint32_t nextNonce(const RemoteJobState& j, uint32_t i) {
  const uint32_t range = j.rangeSize ? j.rangeSize : JOB_RANGE_DEFAULT;
  if (range <= 1UL) return j.startNonce;

  const uint32_t index = i % range;
  const uint32_t seed = j.minerSeed ^ j.minerStartOffset;
  uint32_t stride = j.minerStride % range;
