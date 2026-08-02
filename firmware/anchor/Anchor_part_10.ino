    lastLedR = rr; lastLedG = gg; lastLedB = bb;
    anchorLedWrite(rr, gg, bb);
  }
  extraLedTick(now);
#else
  (void)now;
#endif
}

void anchorStatusTick(uint32_t now, bool force=false) {
  if (!force && janusSafeAgeMs(now, lastStatusMs, 0UL) < ANCHOR_STATUS_MS) return;
  lastStatusMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  uint32_t jobAge = job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL;
  UBaseType_t qDepth = anchorNowQueue ? uxQueueMessagesWaiting(anchorNowQueue) : 0;
  Serial.printf("[ANCHOR/STATUS] v=1.20 proto=S2 id=%u ch=%u wifi=%u masterAge=%lums job=%u qjob=%u jobAge=%lums jobs=%lu/%lu/%lu accept=%lu queued=%lu yield=%lu repl=%lu dup=%lu ping=%lu bad=%lu H=%lu best=%lu lifeBest=%lu nonce=%08lX shares=%lu rxQ=%u drop=%lu rxProc=%lu cb=%lu/%lu rfP=%.2f rfM=%.2f ent=%.2f oxy=%.1f vac=%.2f tx=%lu/%lu heap=%lu loop=%u/%u led=%u,%u,%u bri=%u shareGlow=%lums\n",
                (unsigned)workerId, (unsigned)peerChannel, WiFi.status() == WL_CONNECTED ? 1 : 0,
                (unsigned long)masterAge, job.active ? 1 : 0, queuedJobValid ? 1 : 0, (unsigned long)jobAge,
                (unsigned long)jobsSeen, (unsigned long)jobsDone, (unsigned long)jobsExpired, (unsigned long)jobsAccepted,
                (unsigned long)jobsQueued, (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork,
                (unsigned long)jobsDroppedDuplicate, (unsigned long)jobsDiscoveryPings, (unsigned long)jobsInvalid,
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)bestBitsLifetime,
                (unsigned long)bestNonce, (unsigned long)shares, (unsigned)qDepth, (unsigned long)rxQueueDrops,
                (unsigned long)rxQueueProcessed, (unsigned long)sentCbOk, (unsigned long)sentCbFail,
                rfPresence, rfMotion, rfEntropy, anchorOxytocin, anchorTorricelliVacuum,
                (unsigned long)txOk, (unsigned long)txFail, (unsigned long)ESP.getFreeHeap(),
                (unsigned)anchorLoopJitterUs, (unsigned)anchorLoopMaxUs,
                (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB, (unsigned)ledBrightness,
                (unsigned long)((lastShareMs && janusSafeAgeMs(now, lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS)
                  ? (ANCHOR_LED_SHARE_MS - janusSafeAgeMs(now, lastShareMs, 0UL)) : 0UL));
}
bool looksLikeBuzz(const JanusColonyPacket& pkt) {
  return janusWireFieldContains(pkt.nodeId, sizeof(pkt.nodeId), "Buzz", true) ||
         janusWireFieldContains(pkt.role, sizeof(pkt.role), "MASTER", true) ||
         janusWireFieldContains(pkt.role, sizeof(pkt.role), "Buzz", true);
}
bool agentTargetsThisNode(const JanusAgentRewardPacket& ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return false;
  if (ar.targetNode[0] == '\0') return true;
  if (janusWireFieldEquals(ar.targetNode, sizeof(ar.targetNode), "*")) return true;
  if (janusWireFieldEquals(ar.targetNode, sizeof(ar.targetNode), "all", true)) return true;
  if (janusWireFieldEquals(ar.targetNode, sizeof(ar.targetNode), JANUS_NODE_ID, true)) return true;
  if (janusWireFieldContains(ar.targetNode, sizeof(ar.targetNode), "Anchor", true)) return true;
  if (janusWireFieldContains(ar.targetNode, sizeof(ar.targetNode), "RF", true)) return true;
  return false;
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void onRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  if (!data || len < 2 || len > ANCHOR_RX_MAX_LEN || !anchorNowQueue) return;
  AnchorNowRxItem item{};
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->src_addr) memcpy(item.mac, info->src_addr, sizeof(item.mac));
  item.rssi = (info && info->rx_ctrl) ? (int8_t)info->rx_ctrl->rssi : -127;
#else
  if (mac) memcpy(item.mac, mac, sizeof(item.mac));
  item.rssi = -127;
#endif
  item.len = (uint8_t)len;
  memcpy(item.data, data, (size_t)len);
  if (xQueueSend(anchorNowQueue, &item, 0) != pdTRUE) rxQueueDrops++;
}

void anchorHandleRxFrame(const AnchorNowRxItem& item) {
  const uint8_t* data = item.data;
  const int len = item.len;
  const uint8_t* srcMac = item.mac;
  lastRssi = item.rssi;
  rfOnPacketRssi(lastRssi);
  rxSeen++;
  rxQueueProcessed++;
  lastAnyRxMs = millis();
  if ((rxSeen % ANCHOR_RX_DEBUG_EVERY) == 1UL) {
    Serial.printf("[ANCHOR/RX] n=%lu len=%d magic=%02X%02X rssi=%d qDrop=%lu masterAge=%lums job=%u\n",
                  (unsigned long)rxSeen, len, data[0], data[1], (int)lastRssi,
                  (unsigned long)rxQueueDrops,
                  (unsigned long)(lastMasterMs ? janusSafeAgeMs(millis(), lastMasterMs, 999999UL) : 999999UL),
                  job.active ? 1 : 0);
  }
  if (janusFaceReceive(data, len, lastRssi)) return;
  if (janusTwinTaskReceive(data, len, lastRssi)) return;
  if (len == (int)sizeof(RfDomePingPacket) && data[0] == 'R' && data[1] == 'P') {
    RfDomePingPacket rp{}; memcpy(&rp, data, sizeof(rp));
    if (rp.source[0] == '\0' || janusWireFieldContains(rp.source, sizeof(rp.source), "Core2", true)) rfDomeOnCorePing(rp, lastRssi);
    return;
  }
  if (len == (int)sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{}; memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0) {
      rxJanus++;
      if (looksLikeBuzz(pkt)) {
        lastMasterMs = millis();
        rememberBuzzMasterMac(srcMac, "buzz-heartbeat");
        if ((rxJanus % 8UL) == 1UL) {
          char node[sizeof(pkt.nodeId) + 1]; char role[sizeof(pkt.role) + 1];
          janusCopyWireField(pkt.nodeId, sizeof(pkt.nodeId), node, sizeof(node));
          janusCopyWireField(pkt.role, sizeof(pkt.role), role, sizeof(role));
          Serial.printf("[ANCHOR/BUZZ] rxJanus=%lu node=%s role=%s H=%lu best=%lu shares=%lu rssi=%d\n",
                        (unsigned long)rxJanus, node, role, (unsigned long)pkt.hashRate,
                        (unsigned long)pkt.bestBits, (unsigned long)pkt.shares, (int)lastRssi);
        }
      }
    }
    return;
  }
  if (len == (int)sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket jp{}; memcpy(&jp, data, sizeof(jp));
    rememberBuzzMasterMac(srcMac, "buzz-job"); janusJobHandlePacket(jp); return;
  }
  if (len == (int)sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    JanusAgentRewardPacket ar{}; memcpy(&ar, data, sizeof(ar));
    if (!agentTargetsThisNode(ar)) return;
    rxAgent++; agentRewards++; agentLevel = ar.rewardLevel; agentHint = ar.aiHint ? ar.aiHint : 1;
    agentBatch = ar.targetBatch ? ar.targetBatch : REMOTE_BATCH_BASE;
    agentBatch = constrain((int)agentBatch, REMOTE_BATCH_MIN, REMOTE_BATCH_MAX);
    agentEntropySeed ^= ar.entropySeed ^ micros() ^ ((uint32_t)ar.rewardLevel << 24);
    agentScore = ar.score; agentPredH = ar.predictedHashRate; agentErr = ar.predictionError;
    Serial.printf("[ANCHOR/AGENT] rx=%lu lvl=%u hint=%u batch=%u score=%.1f predH=%.1f err=%.3f dShare=%lu\n",
                  (unsigned long)agentRewards, (unsigned)agentLevel, (unsigned)agentHint,
                  (unsigned)agentBatch, agentScore, agentPredH, agentErr, (unsigned long)ar.deltaShares);
    return;
  }
}

void anchorProcessNowQueue(uint8_t budget = ANCHOR_RX_PROCESS_BUDGET) {
  if (!anchorNowQueue) return;
  AnchorNowRxItem item{};
  uint8_t processed = 0;
  while (processed < budget &&
