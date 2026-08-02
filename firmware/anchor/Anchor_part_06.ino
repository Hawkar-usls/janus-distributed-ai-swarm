  float conf = 8.0f + min(38.0f, (float)rfDomeRxPing * 1.7f) + rfDomePresence * 13.0f + rfDomeMotion * 4.5f + rfDomeHuman * 18.0f;
  rfDomeConfidence = (uint8_t)constrain((int)conf, 0, 100);
#endif
}

void sendRfDome(bool force) {
#if RF_DOME_ENABLE
  uint32_t now = millis();
  bool coreFresh = rfDomeLastPingMs && janusSafeAgeMs(now, rfDomeLastPingMs, 999999UL) < RF_DOME_CORE_TIMEOUT_MS;
  bool alert = (rfDomePresence > 0.45f || rfDomeMotion > 1.20f || rfDomeHuman > 0.28f || rfDomePet > 0.32f);
  uint32_t interval = alert ? RF_DOME_ALERT_TX_MS : RF_DOME_TX_MS;
  if (!force && janusSafeAgeMs(now, rfDomeLastTxMs, 0UL) < interval) return;
  rfDomeLastTxMs = now;
  RfDomeSonarPacket rs{};
  rs.magic[0] = 'R'; rs.magic[1] = 'S'; rs.version = 1;
  if (coreFresh) rs.flags |= 0x01;
  if (rfDomePresence > 0.45f) rs.flags |= 0x02;
  if (rfDomeMotion > 1.20f) rs.flags |= 0x04;
  if (rfDomeHuman > 0.34f) rs.flags |= 0x08;
  if (rfDomePet > 0.38f) rs.flags |= 0x10;
  if (rfDomeRxPing < RF_DOME_MIN_CONF_PACKETS) rs.flags |= 0x20;
  strlcpy(rs.anchorId, JANUS_NODE_ID, sizeof(rs.anchorId));
  rs.seq = ++rfDomeSeq; rs.uptimeMs = now; rs.coreRssi = rfDomeCoreRssi; rs.ambientRssi = lastRssi;
  rs.coreEma_x10 = (int16_t)constrain((int)(rfDomeCoreEma * 10.0f), -32768, 32767);
  rs.coreBase_x10 = (int16_t)constrain((int)(rfDomeCoreBase * 10.0f), -32768, 32767);
  rs.coreDelta_x10 = (int16_t)constrain((int)(rfDomeDelta * 10.0f), -32768, 32767);
  rs.coreVar_x10 = (uint16_t)constrain((int)(rfDomeVar * 10.0f), 0, 65535);
  rs.motion_x100 = (uint16_t)constrain((int)(rfDomeMotion * 100.0f), 0, 65535);
  rs.presence_x100 = (uint16_t)constrain((int)(rfDomePresence * 100.0f), 0, 65535);
  rs.human_x100 = (uint16_t)constrain((int)(rfDomeHuman * 100.0f), 0, 10000);
  rs.pet_x100 = (uint16_t)constrain((int)(rfDomePet * 100.0f), 0, 10000);
  rs.zonePct = rfDomeZonePct; rs.distanceCm = rfDomeDistanceCm; rs.confidence = rfDomeConfidence; rs.domeLengthCm = RF_DOME_DEFAULT_LENGTH_CM; rs.packetsSeen = rfDomeRxPing;
  rs.crc = 0; rs.crc = rfDomeCrc32(&rs, sizeof(rs));
  bool ok = sendEspNow("R/S", &rs, sizeof(rs));
  rfDomeTx++;
  bool rfDomeShouldLog = force || !ok || ((rfDomeTx % 12UL) == 1UL);
  if (alert && janusSafeElapsed(now, rfDomeLastLogMs, RF_DOME_LOG_MS)) rfDomeShouldLog = true;
  if (rfDomeShouldLog) {
    rfDomeLastLogMs = now;
    Serial.printf("[RF/DOME] tx=%s seq=%lu coreFresh=%u rssi=%d ema=%.1f base=%.1f d=%.1f var=%.1f P=%.2f M=%.2f human=%.2f pet=%.2f zone=%u dist=%ucm conf=%u flags=0x%02X pings=%lu log=throttle/%lums\n",
                  ok ? "OK" : "FAIL", (unsigned long)rs.seq, coreFresh ? 1 : 0, (int)rs.coreRssi,
                  rfDomeCoreEma, rfDomeCoreBase, rfDomeDelta, rfDomeVar, rfDomePresence, rfDomeMotion, rfDomeHuman, rfDomePet,
                  (unsigned)rs.zonePct, (unsigned)rs.distanceCm, (unsigned)rs.confidence, (unsigned)rs.flags, (unsigned long)rfDomeRxPing,
                  (unsigned long)RF_DOME_LOG_MS);
  }
#endif
}

void rfOnPacketRssi(int8_t rssi) {
  if (!validRssi((int)rssi)) return;
  rfRxPackets++;
  rfLastPacketMs = millis();
  if (validRssi((int)lastRssi)) rfLastPacketDrift = fabsf((float)rssi - (float)lastRssi);
  lastRssi = rssi;
  float kick = constrain(rfLastPacketDrift / 12.0f, 0.0f, 1.5f);
  rfPacketPressure = constrain(rfPacketPressure * 0.82f + kick * 0.18f, 0.0f, 2.0f);
}

void rfTick(uint32_t now) {
  if (now - lastRfSampleMs < RF_SAMPLE_MS) return;
  lastRfSampleMs = now;
  int rssiNow = validRssi((int)lastRssi) ? lastRssi : ((WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127);
  if (!validRssi(rssiNow)) {
    rfPresence *= 0.94f;
    rfMotion *= 0.90f;
    rfEntropy *= 0.92f;
    rfPacketPressure *= 0.96f;
    rfPresenceNow = false;
    rfMotionNow = false;
    return;
  }
  if (!rfReady) {
    rfEma = (float)rssiNow;
    rfBase = (float)rssiNow;
    rfNoise = 3.0f;
    rfReady = true;
    return;
  }
  float prev = rfEma;
  rfEma = rfEma * 0.86f + (float)rssiNow * 0.14f;
  float step = fabsf((float)rssiNow - prev);
  rfDrift = fabsf(rfEma - rfBase);
  bool hot = step > 3.0f || rfDrift > max(4.5f, rfNoise * 1.85f) || rfPacketPressure > 0.65f;
  float alpha = hot ? 0.0007f : 0.0060f;
  rfBase = rfBase * (1.0f - alpha) + rfEma * alpha;
  if (!hot) rfNoise = rfNoise * 0.975f + step * 0.025f;
  rfNoise = constrain(rfNoise, 1.2f, 12.0f);

  float motionNorm = constrain((step + rfLastPacketDrift * 0.55f) / 3.2f, 0.0f, 5.0f);
  float presenceNorm = constrain(rfDrift / max(rfNoise * 2.2f + 1.0f, 1.0f), 0.0f, 4.0f);
  rfMotion = constrain(rfMotion * 0.78f + motionNorm * 0.22f + rfPacketPressure * 0.08f, 0.0f, 12.0f);
  rfPresence = constrain(rfPresence * 0.88f + (presenceNorm + rfPacketPressure * 0.20f) * 0.12f, 0.0f, 3.0f);
  rfEntropy = constrain(rfEntropy * 0.86f + (motionNorm * 0.22f + presenceNorm * 0.30f + rfPacketPressure * 0.24f) * 0.14f, 0.0f, 4.0f);
  rfPresenceNow = rfPresence > 0.42f;
  rfMotionNow = rfMotion > 1.05f || motionNorm > 1.65f;
  rfPacketPressure *= 0.985f;
  rfLastPacketDrift *= 0.92f;
}

float localEntropy() {
  float agent = (float)(agentEntropySeed & 0xFFFF) / 65535.0f;
  return constrain(rfEntropy * 0.85f + rfMotion * 0.08f + rfPresence * 0.35f + rfPacketPressure * 0.50f + agent * 0.75f + (float)agentLevel * 0.10f, 0.0f, 9999.0f);
}

void sendHeartbeat() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, JANUS_NODE_ID, sizeof(pkt.nodeId));
  strlcpy(pkt.role, JANUS_NODE_ROLE, sizeof(pkt.role));
  pkt.seq = ++seqNo;
  pkt.hashRate = hashRate;
  pkt.shares = shares;
  pkt.rejects = rejects;
  pkt.bestBits = bestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = targetBits;
  pkt.aiBatch = activeBatch();
  pkt.aiHint = anchorSchedulerHint();
  pkt.jobAgeMs = job.active ? janusSafeAgeMs(millis(), job.receivedAt, 0UL) : 0;
  pkt.rssi = lastRssi;
  pkt.uptime = millis();
  bool ok = sendEspNow("JANUS", &pkt, sizeof(pkt));
  esp_err_t directErr = sendEspNowToBuzzMaster("JANUS-direct", &pkt, sizeof(pkt));
  heartbeatTx++;
  if ((ANCHOR_TX_LOG_EVERY <= 1UL) || ((heartbeatTx % ANCHOR_TX_LOG_EVERY) == 1UL) || !ok || (buzzMasterMacKnown && directErr != ESP_OK)) {
    Serial.printf("[ANCHOR/HB] tx=%s direct=%d known=%u n=%lu seq=%lu ch=%u peerCh=%u masterAge=%lums H=%lu best=%lu shares=%lu directCnt=%lu/%lu missing=%lu\n",
                  ok ? "OK" : "FAIL", (int)directErr, buzzMasterMacKnown ? 1 : 0,
                  (unsigned long)heartbeatTx, (unsigned long)pkt.seq,
                  (unsigned)peerChannel, (unsigned)buzzMasterPeerChannel,
                  (unsigned long)(lastMasterMs ? janusSafeAgeMs(millis(), lastMasterMs, 999999UL) : 999999UL),
                  (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)shares,
                  (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                  (unsigned long)buzzMasterMacMissing);
  }
}

void sendEntropy() {
  EntropyReportV2 er{};
  er.magic[0] = 'E'; er.magic[1] = '2';
  er.worker_id = workerId;
  strlcpy(er.nodeId, JANUS_NODE_ID, sizeof(er.nodeId));
  er.local_entropy = localEntropy();
  er.prediction_error = rfDrift * 0.015f;
  er.sync_hint = constrain((rfReady ? 0.55f : 0.0f) + anchorTorricelliVacuum * 0.34f + anchorOxytocin * 0.0016f, 0.0f, 1.0f);
  er.fit = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  er.sensor_flags = 0x88;
  er.values[0] = rfPresence;
  er.values[1] = rfMotion;
  er.values[2] = rfEntropy;
  er.values[3] = rfDrift;
  er.values[4] = rfNoise;
  er.values[5] = rfPacketPressure;
  er.values[6] = (float)hashRate;
  er.values[7] = (float)bestBits;
  er.uptime_ms = millis();
  bool ok = sendEspNow("E2", &er, sizeof(er));
  entropyTx++;
  if ((ANCHOR_TX_LOG_EVERY <= 1UL) || ((entropyTx % ANCHOR_TX_LOG_EVERY) == 1UL) || !ok) {
    Serial.printf("[ANCHOR/E2] tx=%s n=%lu entropy=%.2f rfP=%.2f rfM=%.2f drift=%.1f H=%lu best=%lu\n",
                  ok ? "OK" : "FAIL", (unsigned long)entropyTx, er.local_entropy,
