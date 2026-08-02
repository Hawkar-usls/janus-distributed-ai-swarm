                  rfPresence, rfMotion, rfDrift, (unsigned long)hashRate, (unsigned long)bestBits);
  }
}
void sendSwarmSense(bool force=false) {
  uint32_t now = millis();
  uint32_t interval = (rfPresenceNow || rfMotionNow || bestBits >= 22) ? SWARMSENSE_ALERT_MS : SWARMSENSE_TX_MS;
  if (!force && janusSafeAgeMs(now, lastSwarmSenseMs, 0UL) < interval) return;
  lastSwarmSenseMs = now;
  const uint16_t batch = activeBatch();
  const uint16_t loopPeak = max(anchorLoopMaxUs, anchorLoopJitterUs);
  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S'; ss.version = 1; ss.worker_id = workerId;
  strlcpy(ss.nodeId, JANUS_NODE_ID, sizeof(ss.nodeId));
  strlcpy(ss.kind, "rf_anchor_aux", sizeof(ss.kind));
  ss.seq = ++ssSeq; ss.uptime_ms = now; ss.micros_tail = micros(); ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = anchorLoopJitterUs; ss.loop_max_us = loopPeak; ss.rssi = lastRssi;
  ss.radio_mode = (WiFi.status() == WL_CONNECTED) ? 2 : 1; ss.bt_flags = 0;
  if (rfPresenceNow) ss.bt_flags |= 0x01;
  if (rfMotionNow) ss.bt_flags |= 0x02;
  if (janusTwinPeerFresh()) ss.bt_flags |= 0x20;
  if (rfReady) ss.bt_flags |= 0x40;
  if (rfEntropy > 1.18f) ss.bt_flags |= 0x80;
  ss.palette = job.minerLane; ss.knn_label = job.minerSector;
  ss.knn_confidence = (uint8_t)constrain(max((int)(rfPresence * 100.0f), (int)anchorOxytocin), 0, 100);
  ss.ai_hint = max<uint8_t>((anchorOxytocin > 72.0f || bestBits >= 22 || rfEntropy > 1.18f) ? 3 : 1, anchorSchedulerHint());
  ss.thermal_load = (uint8_t)constrain((int)(((uint32_t)batch * 100UL) / max<uint16_t>(1, REMOTE_BATCH_MAX)), 0, 100);
  ss.effective_batch = batch; ss.dynamic_batch = batch; ss.hash_rate = hashRate; ss.total_hashes = totalHashesLifetime;
  ss.best_bits = (uint16_t)min<uint32_t>(65535UL, bestBits);
  ss.hash_eff_x1000 = (uint16_t)constrain((int)((hashRateEma * 1000.0f) / (float)max<uint16_t>(1, batch)), 0, 65535);
  ss.prediction_error_x1000 = (int16_t)constrain((int)(rfDrift * 15.0f), -32768, 32767);
  ss.entropy_x1000 = (uint16_t)constrain((int)(localEntropy() * 1000.0f), 0, 65535);
  ss.touch_delta = (uint16_t)constrain((int)(rfPresence * 100.0f + rfMotion * 12.0f), 0, 65535);
  ss.job_age_s = job.active ? (uint16_t)min(65535UL, janusSafeAgeMs(now, job.receivedAt, 0UL) / 1000UL) : 0U;
  ss.nonce_remaining_l16 = (job.active && job.rangeSize > job.hashesDone) ? (uint16_t)((job.rangeSize - job.hashesDone) & 0xFFFF) : 0;
  ss.flags = 0;
  if (job.active) ss.flags |= 0x0001;
  if (queuedJobValid) ss.flags |= 0x0002;
  if (janusTwinPeerFresh()) ss.flags |= 0x0004;
  if (rfDomeReady || rfDomeRxPing) ss.flags |= 0x0008;
  if (anchorOxytocin > 64.0f) ss.flags |= 0x0020;
  if (anchorTranceptionHint >= 3) ss.flags |= 0x0040;
  bool ok = sendEspNow("S/S", &ss, sizeof(ss));
  anchorLoopMaxUs = anchorLoopJitterUs;
  swarmSenseTx++;
  if (force || (swarmSenseTx % ANCHOR_TX_LOG_EVERY) == 1UL || !ok) {
    Serial.printf("[ANCHOR/SENSE] tx=%s n=%lu flags=0x%04X bt=0x%02X kind=%s rfP=%.2f rfM=%.2f ent=%.2f job=%u batch=%u H=%lu eff=%u total=%lu jitter=%u/%u\n",
                  ok ? "OK" : "FAIL", (unsigned long)swarmSenseTx, (unsigned)ss.flags, (unsigned)ss.bt_flags, ss.kind,
                  rfPresence, rfMotion, rfEntropy, job.active ? 1 : 0, (unsigned)batch, (unsigned long)hashRate,
                  (unsigned)ss.hash_eff_x1000, (unsigned long)ss.total_hashes, (unsigned)ss.loop_jitter_us, (unsigned)ss.loop_max_us);
  }
}
uint16_t anchorScaleX1000(float v, float lo, float hi) {
  if (hi <= lo) return 0;
  return (uint16_t)constrain((int)((v - lo) * 1000.0f / (hi - lo) + 0.5f), 0, 65535);
}
uint32_t anchorCurrentJobSignature() {
  uint32_t h = 0xA9C40A11UL ^ ((uint32_t)workerId << 16) ^ janusJobFp32(job);
  h ^= job.startNonce ^ job.rangeSize ^ job.minerSeed ^ job.minerStride;
  h ^= ((uint32_t)job.minerLane << 24) ^ ((uint32_t)job.minerSector << 16) ^ ((uint32_t)job.minerStrideArm << 8) ^ (uint32_t)targetBits;
  return rfDomeCrc32(&h, sizeof(h));
}
void sendAnchorPnCortex(bool force) {
  uint32_t now = millis();
  if (!force && anchorPnLastMs && janusSafeAgeMs(now, anchorPnLastMs, 0UL) < ANCHOR_PN_CORTEX_MS) return;
  anchorPnLastMs = now;
  uint16_t batch = activeBatch();
  float heapPressure = 1.0f - constrain((float)ESP.getFreeHeap() / 240000.0f, 0.0f, 1.0f);
  float hashLoad = constrain((float)hashRate / 14000.0f, 0.0f, 2.5f);
  float batchLoad = constrain((float)batch / (float)REMOTE_BATCH_MAX, 0.0f, 1.3f);
  float rfBody = constrain(rfPresence * 0.34f + rfMotion * 0.10f + rfEntropy * 0.20f + rfPacketPressure * 0.16f, 0.0f, 4.0f);
  float dome = constrain(rfDomePresence * 0.30f + rfDomeMotion * 0.08f + rfDomeHuman * 0.34f + rfDomePet * 0.22f, 0.0f, 4.0f);
  float oxy = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  float thermal = constrain(0.10f + hashLoad * 0.42f + batchLoad * 0.18f + heapPressure * 0.22f + rfBody * 0.10f + oxy * 0.08f, 0.0f, 4.0f);
  float load = constrain(0.12f + hashLoad * 0.52f + batchLoad * 0.25f + (job.active ? 0.16f : 0.0f) + dome * 0.10f + oxy * 0.16f, 0.0f, 4.0f);
  float entropy = constrain(localEntropy() * 0.20f + rfEntropy * 0.45f + dome * 0.25f + anchorTorricelliVacuum * 0.42f + oxy * 0.28f, 0.0f, 6.0f);
  uint16_t targetForTail = targetBits ? targetBits : 22;
  if (targetForTail < 1) targetForTail = 1;
  float tail = constrain(((float)bestBits / (float)targetForTail) + (float)tailHits / 120.0f, 0.0f, 6.0f);
  JanusPnCortexPacket pn{};
  pn.magic[0] = 'P'; pn.magic[1] = 'N'; pn.version = 1; pn.role = JANUS_ROLE_ANCHOR_PN; pn.worker_id = workerId;
  strlcpy(pn.nodeId, JANUS_NODE_ID, sizeof(pn.nodeId)); strlcpy(pn.kind, "anchor_pn_lab", sizeof(pn.kind));
  pn.seq = ++anchorPnSeq; pn.uptime_ms = now; pn.job_sig = anchorCurrentJobSignature(); pn.prev_hash = anchorPnPrevHash;
  pn.hash_rate = hashRate; pn.total_hashes = totalHashesLifetime; pn.target_bits = targetBits ? targetBits : 22;
  pn.best_bits = (uint16_t)min<uint32_t>(65535UL, bestBits); pn.lane = job.minerLane; pn.sector = job.minerSector; pn.flags = 0;
  if (job.active) pn.flags |= 0x01;
  if (lastMasterMs && janusSafeAgeMs(now, lastMasterMs, 999999UL) < MASTER_TIMEOUT_MS) pn.flags |= 0x02;
  if (rfDomeReady || rfDomeRxPing) pn.flags |= 0x04;
  if (janusTwinPeerFresh()) pn.flags |= 0x08;
  if (agentLevel || agentEntropySeed) pn.flags |= 0x10;
  if (anchorOxytocin > 64.0f || anchorTorricelliVacuum > 0.66f) pn.flags |= 0x20;
  if (anchorTranceptionHint >= 3) pn.flags |= 0x40;
  pn.rssi = lastRssi; pn.thermal_x1000 = anchorScaleX1000(thermal, 0.0f, 4.0f); pn.load_x1000 = anchorScaleX1000(load, 0.0f, 4.0f);
  pn.jitter_us = anchorLoopJitterUs; pn.entropy_x1000 = anchorScaleX1000(entropy, 0.0f, 6.0f); pn.tail_x1000 = anchorScaleX1000(tail, 0.0f, 6.0f);
  pn.voltage_mv = 0;
  pn.ir_phase = (uint16_t)((pn.job_sig ^ pn.prev_hash ^ ((uint32_t)rfDomeZonePct << 8) ^ ((uint32_t)bestBits << 3) ^ job.minerStride ^
                            ((uint32_t)(anchorOxytocin * 10.0f) << 1) ^ ((uint32_t)(anchorTranceptionLiteScore * 1000.0f) << 4) ^
                            ((uint32_t)anchorTranceptionLane << 12)) & 0xFFFFUL);
  pn.reserved = (uint16_t)constrain((int)(anchorOxytocin * 10.0f), 0, 1000);
  pn.packet_hash = 0; pn.packet_hash = rfDomeCrc32(&pn, sizeof(pn)); anchorPnPrevHash = pn.packet_hash;
  bool ok = sendEspNow("P/N", &pn, sizeof(pn));
  if (ok) {
    anchorPnTx++;
    if ((anchorPnTx & 0x07UL) == 1UL) {
      Serial.printf("[ANCHOR/PN] tx=%lu lane=%s/s%u H=%lu best=%u/%u heat=%.2f load=%.2f rf=%.2f tail=%.2f oxy=%.1f vac=%.2f tl=%.2f/%u flags=0x%02X\n",
