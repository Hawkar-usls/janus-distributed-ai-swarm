  if (ok || directErr == ESP_OK) shares++;
  anchorLedFlashShare(bits);

  Serial.printf("[ANCHOR/S2] tx=%s direct=%d share=%lu nonce=%08lX bits=%u targetBits=%u tail=%02X%02X%02X%02X lane=%s/s%u H=%lu best=%lu lifeBest=%lu total=%lu tx=%lu/%lu directCnt=%lu/%lu\n",
                ok ? "OK" : "FAIL", (int)directErr, (unsigned long)shares,
                (unsigned long)nonce, (unsigned)bits, (unsigned)targetBits,
                sr.hash_tail[0], sr.hash_tail[1], sr.hash_tail[2], sr.hash_tail[3],
                laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)hashRate, (unsigned long)bestBits,
                (unsigned long)bestBitsLifetime, (unsigned long)totalHashesLifetime,
                (unsigned long)txOk, (unsigned long)txFail,
                (unsigned long)buzzMasterDirectOk,
                (unsigned long)buzzMasterDirectFail);
}
void runMining() {
  uint32_t now = millis();
  if (!job.active) {
    janusJobPromoteQueued("miner_idle");
    if (janusSafeAgeMs(now, lastHashTickMs, 0UL) >= 1000UL) {
      hashRate = 0;
      hashCounter = 0;
      lastHashTickMs = now;
    }
    return;
  }

  if (janusSafeElapsed(now, job.receivedAt, JOB_TIMEOUT_MS)) {
    Serial.printf("[ANCHOR/JOB] timeout fp=%08lX start=%08lX checked=%lu age=%lums q=%u\n",
                  (unsigned long)janusJobFp32(job), (unsigned long)job.startNonce,
                  (unsigned long)job.hashesDone,
                  (unsigned long)janusSafeAgeMs(now, job.receivedAt, 0UL),
                  queuedJobValid ? 1 : 0);
    job.active = false;
    jobsExpired++;
    hashRate = 0;
    janusJobPromoteQueued("timeout_promote");
    return;
  }

  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t shareHash[32];
  uint16_t batch = activeBatch();

  for (uint16_t i = 0; i < batch; ++i) {
    if (i && (i % ANCHOR_MINER_RX_YIELD_HASHES) == 0 &&
        anchorNowQueue && uxQueueMessagesWaiting(anchorNowQueue) > 0) break;

    if (job.hashesDone >= job.rangeSize) {
      job.active = false;
      jobsDone++;
      Serial.printf("[ANCHOR/JOB] range_done done=%lu fp=%08lX start=%08lX checked=%lu best=%lu lifeBest=%lu q=%u\n",
                    (unsigned long)jobsDone, (unsigned long)janusJobFp32(job),
                    (unsigned long)job.startNonce, (unsigned long)job.hashesDone,
                    (unsigned long)bestBits, (unsigned long)bestBitsLifetime,
                    queuedJobValid ? 1 : 0);
      janusJobPromoteQueued("range_done_promote");
      break;
    }

    uint32_t nonce = nextNonce(job, job.hashesDone);
    job.nonce = nonce;
    job.hashesDone++;
    memcpy(header, job.header, sizeof(header));
    writeLE32(header + 76, nonce);
    doubleSha256(header, sizeof(header), rawHash);
    hashToShareOrder(rawHash, shareHash);
    hashCounter++;
    totalHashesLifetime++;
    uint16_t bits = countLeadingZeroBitsBE(shareHash);

    if (bits > bestBits) {
      bestBits = bits;
      bestNonce = nonce;
      if (bits > bestBitsLifetime) { bestBitsLifetime = bits; bestNonceLifetime = nonce; }
      if (bits >= 16) {
        float lift = 0.55f + (float)(bits - 15) * 0.18f;
        if (janusTwinSameBuzzWindow(job)) lift += (janusTwinPeerBestBits >= bits) ? 1.20f : 0.45f;
        anchorOxytocin = constrain(anchorOxytocin + lift, 0.0f, 100.0f);
        Serial.printf("[ANCHOR/BEST] bits=%u nonce=%08lX lane=%s/s%u stride=%lu arm=%u checked=%lu/%lu fp=%08lX life=%lu@%08lX\n",
                      (unsigned)bits, (unsigned long)nonce, laneName(job.minerLane),
                      (unsigned)job.minerSector, (unsigned long)job.minerStride,
                      (unsigned)job.minerStrideArm, (unsigned long)job.hashesDone,
                      (unsigned long)job.rangeSize, (unsigned long)janusJobFp32(job),
                      (unsigned long)bestBitsLifetime, (unsigned long)bestNonceLifetime);
      }
    }
    if (bits >= 22) tailHits++;
    if ((bits >= targetBits) && hashMeetsTargetBE(shareHash, job.target)) {
      anchorOxytocin = constrain(anchorOxytocin + 12.0f, 0.0f, 100.0f);
      anchorTorricelliVacuum = constrain(anchorTorricelliVacuum + 0.08f, 0.0f, 1.0f);
      sendShare(nonce, bits, shareHash);
      job.active = false;
      jobsDone++;
      Serial.printf("[ANCHOR/TICKET] protocol=S2 nonce=%08lX bits=%u lane=%s/s%u fp=%08lX\n",
                    (unsigned long)nonce, (unsigned)bits, laneName(job.minerLane),
                    (unsigned)job.minerSector, (unsigned long)janusJobFp32(job));
      janusJobPromoteQueued("ticket_promote");
      break;
    }
  }
  uint32_t endNow = millis();
  uint32_t elapsed = janusSafeAgeMs(endNow, lastHashTickMs, 0UL);
  if (elapsed >= 1000UL) {
    hashRate = elapsed ? (uint32_t)(((uint64_t)hashCounter * 1000ULL) / elapsed) : 0UL;
    hashRateEma = (hashRateEma <= 0.1f) ? (float)hashRate : (hashRateEma * 0.78f + (float)hashRate * 0.22f);
    hashCounter = 0;
    lastHashTickMs = endNow;
  }
}

bool validRssi(int v) { return v < -5 && v > -126; }
uint32_t rfDomeCrc32(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619UL; }
  return h;
}
void rfDomeOnCorePing(const RfDomePingPacket& ping, int8_t rssi) {
#if RF_DOME_ENABLE
  (void)ping;
  if (!validRssi((int)rssi)) return;
  uint32_t now = millis();
  rfDomeRxPing++;
  rfDomeLastPingMs = now;
  rfDomeCoreRssi = rssi;
  if (!rfDomeReady) { rfDomeCoreEma = (float)rssi; rfDomeCoreBase = (float)rssi; rfDomeVar = 4.0f; rfDomeDelta = 0.0f; rfDomeReady = true; }
  float prev = rfDomeCoreEma;
  rfDomeCoreEma = rfDomeCoreEma * 0.78f + (float)rssi * 0.22f;
  float step = fabsf((float)rssi - prev);
  rfDomeDelta = fabsf(rfDomeCoreEma - rfDomeCoreBase);
  bool hot = (step > 2.5f) || (rfDomeDelta > max(3.8f, sqrtf(max(rfDomeVar, 1.0f)) * 2.15f));
  float alpha = hot ? RF_DOME_LEARN_ALPHA_HOT : RF_DOME_LEARN_ALPHA_COLD;
  rfDomeCoreBase = rfDomeCoreBase * (1.0f - alpha) + rfDomeCoreEma * alpha;
  if (!hot) rfDomeVar = rfDomeVar * 0.965f + (step * step) * 0.035f;
  rfDomeVar = constrain(rfDomeVar, 1.0f, 160.0f);
  float noise = sqrtf(max(rfDomeVar, 1.0f));
  float motionKick = constrain((step + rfDomeDelta * 0.36f) / max(1.4f, noise * 0.82f), 0.0f, 9.0f);
  float presenceKick = constrain(rfDomeDelta / max(2.8f, noise * 1.95f), 0.0f, 6.0f);
  rfDomeMotion = constrain(rfDomeMotion * 0.70f + motionKick * 0.30f, 0.0f, 10.0f);
  rfDomePresence = constrain(rfDomePresence * 0.86f + presenceKick * 0.14f, 0.0f, 6.0f);
  float motionFast = constrain((rfDomeMotion - 1.0f) / 4.0f, 0.0f, 1.0f);
  float bodySlow = constrain((rfDomePresence - 0.35f) / 2.6f, 0.0f, 1.0f);
  rfDomeHuman = constrain(rfDomeHuman * 0.82f + (bodySlow * 0.72f + motionFast * 0.28f) * 0.18f, 0.0f, 1.0f);
  rfDomePet = constrain(rfDomePet * 0.84f + (motionFast * (1.0f - bodySlow * 0.45f)) * 0.16f, 0.0f, 1.0f);
  float signedDelta = rfDomeCoreBase - rfDomeCoreEma;
  float centerPull = constrain((signedDelta + 5.0f) / 10.0f, 0.0f, 1.0f);
  float wobble = 0.5f + 0.5f * sinf((float)now * 0.0042f + rfDomeMotion * 0.31f);
  float zone = 50.0f + (centerPull - 0.5f) * 36.0f + (wobble - 0.5f) * 12.0f;
  rfDomeZonePct = (uint8_t)constrain((int)zone, 5, 95);
  rfDomeDistanceCm = (uint16_t)constrain((int)((RF_DOME_DEFAULT_LENGTH_CM * (uint32_t)rfDomeZonePct) / 100UL), 20, 900);
