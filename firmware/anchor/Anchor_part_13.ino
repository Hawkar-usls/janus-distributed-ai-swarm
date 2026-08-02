  JanusDebugUART.printf("[ANCHOR/UART0] v1.20 proto=S2 ch=%u job=%u H=%lu best=%lu/%u life=%lu shares=%lu rxDrop=%lu rfP=%.2f rfM=%.2f bright=%u led=%u,%u,%u tx=%lu/%lu masterAge=%lums heap=%lu cmd=?/s/r/m/+/-/0/9/l/u\n",
                        (unsigned)peerChannel, job.active ? 1 : 0,
                        (unsigned long)hashRate, (unsigned long)bestBits,
                        (unsigned)targetBits, (unsigned long)bestBitsLifetime,
                        (unsigned long)shares, (unsigned long)rxQueueDrops,
                        rfPresence, rfMotion, (unsigned)ledBrightness,
                        (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB,
                        (unsigned long)txOk, (unsigned long)txFail,
                        (unsigned long)masterAge, (unsigned long)ESP.getFreeHeap());
#else
  (void)now;
  (void)force;
#endif
}
void setupEspNow() {
  if (!anchorNowQueue) {
    anchorNowQueue = xQueueCreate(ANCHOR_RX_QUEUE_DEPTH, sizeof(AnchorNowRxItem));
    if (!anchorNowQueue) { Serial.println("[ANCHOR/ESPNOW] RX queue allocation failed"); return; }
  }
  esp_now_deinit(); delay(10); xQueueReset(anchorNowQueue);
  esp_err_t initErr = esp_now_init();
  if (initErr != ESP_OK) { Serial.printf("[ANCHOR/ESPNOW] init failed err=%d\n", (int)initErr); return; }
  esp_err_t rxErr = esp_now_register_recv_cb(onRecv);
  esp_err_t txErr = esp_now_register_send_cb(onSent);
  peerChannel = 0; buzzMasterPeerChannel = 0; ensurePeer();
  Serial.printf("[ANCHOR/ESPNOW] ready id=%u channel=%u queue=%u rxCb=%d txCb=%d protocol=S2\n",
                workerId, peerChannel, (unsigned)ANCHOR_RX_QUEUE_DEPTH, (int)rxErr, (int)txErr);
}
void anchorRadioRescue(const char* reason) {
  uint32_t now = millis();
  if (now - anchorRadioLastRescueMs < ANCHOR_RADIO_RESCUE_MIN_MS) return;
  anchorRadioLastRescueMs = now; anchorRadioRescues++; anchorSwarmRejoinSoftRescues++;
  if (anchorSwarmRejoinEpisodeRescues < 255) anchorSwarmRejoinEpisodeRescues++;
  uint8_t beforeCh = peerChannel;
  uint32_t rxAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/RADIO/RESCUE] reason=%s n=%lu ch=%u cur=%u rxAge=%lums tx=%lu/%lu direct=%lu/%lu known=%u cb=%lu/%lu err=%d\n",
                reason ? reason : "?", (unsigned long)anchorRadioRescues, (unsigned)beforeCh,
                (unsigned)currentChannel(), (unsigned long)rxAge, (unsigned long)txOk,
                (unsigned long)txFail, (unsigned long)buzzMasterDirectOk,
                (unsigned long)buzzMasterDirectFail, buzzMasterMacKnown ? 1 : 0,
                (unsigned long)sentCbOk, (unsigned long)sentCbFail, lastTxErr);
  Serial.printf("[ANCHOR/REJOIN] soft reason=%s n=%lu episode=%u masterAge=%lums anyRxAge=%lums jobAge=%lums known=%u\n",
                reason ? reason : "?", (unsigned long)anchorSwarmRejoinSoftRescues,
                (unsigned)anchorSwarmRejoinEpisodeRescues,
                (unsigned long)(lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL),
                (unsigned long)(lastAnyRxMs ? janusSafeAgeMs(now, lastAnyRxMs, 999999UL) : 999999UL),
                (unsigned long)(job.active ? janusSafeAgeMs(now, job.receivedAt, 999999UL) : 999999UL),
                buzzMasterMacKnown ? 1 : 0);
  if (buzzMasterMacKnown) buzzMasterPeerChannel = 0;
  setupEspNow();
  anchorPresenceBurst(reason ? reason : "radio-rescue");
  anchorRadioLastTxFailSeen = txFail; anchorRadioLastTxOkSeen = txOk; sentCbConsecutiveFail = 0;
}
void anchorRadioWatchdog(uint32_t now) {
  if (now - anchorRadioLastWatchMs < 2500UL) return;
  anchorRadioLastWatchMs = now; anchorSwarmRejoinLastGuardMs = now;
  uint8_t ch = currentChannel();
  bool peerMissing = !esp_now_is_peer_exist(JANUS_BROADCAST_MAC);
  bool channelMismatch = (peerChannel != 0 && peerChannel != ch);
  bool txFailStreak = (txFail >= anchorRadioLastTxFailSeen + ANCHOR_RADIO_TX_FAIL_DELTA && txOk == anchorRadioLastTxOkSeen);
  uint32_t cbOk = sentCbOk, cbFail = sentCbFail;
  uint16_t cbConsecutiveFail = sentCbConsecutiveFail;
  bool cbFailStreak = (cbConsecutiveFail >= ANCHOR_RADIO_TX_FAIL_DELTA);
  uint32_t anyRxAge = lastAnyRxMs ? janusSafeAgeMs(now, lastAnyRxMs, 999999UL) : 999999UL;
  bool rxBlackout = (now > ANCHOR_RADIO_RX_BLACKOUT_MS && anyRxAge > ANCHOR_RADIO_RX_BLACKOUT_MS);
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  bool masterBlackout = (now > ANCHOR_RADIO_MASTER_BLACKOUT_MS && masterAge > ANCHOR_RADIO_MASTER_BLACKOUT_MS);
  bool directBlackout = (buzzMasterMacKnown && buzzMasterDirectFail >= buzzMasterDirectOk + 8UL && masterAge > 4500UL);
  uint32_t jobAge = job.active ? janusSafeAgeMs(now, job.receivedAt, 999999UL) : 999999UL;
  bool jobBlackout = (now > ANCHOR_SWARM_REJOIN_BOOT_GRACE_MS &&
                      (!job.active || jobAge > ANCHOR_SWARM_REJOIN_JOB_BLACKOUT_MS) &&
                      masterAge > ANCHOR_RADIO_MASTER_BLACKOUT_MS);
  const char* rejoinReason = nullptr;
  if (peerMissing) rejoinReason = "peer-missing";
  else if (channelMismatch) rejoinReason = "channel-mismatch";
  else if (txFailStreak) rejoinReason = "tx-submit-fail-streak";
  else if (cbFailStreak) rejoinReason = "tx-callback-fail-streak";
  else if (directBlackout) rejoinReason = "buzz-direct-blackout";
  else if (masterBlackout) rejoinReason = "master-blackout";
  else if (rxBlackout) rejoinReason = "rx-blackout";
  else if (jobBlackout) rejoinReason = "job-blackout";
  if (rejoinReason) {
    if (!anchorSwarmRejoinFirstMissingMs) { anchorSwarmRejoinFirstMissingMs = now; anchorSwarmRejoinAttempts++; }
    uint32_t missingMs = janusSafeAgeMs(now, anchorSwarmRejoinFirstMissingMs, 0UL);
    if (!anchorSwarmRejoinLastStateLogMs || janusSafeAgeMs(now, anchorSwarmRejoinLastStateLogMs, 0UL) >= ANCHOR_SWARM_REJOIN_STATE_LOG_MS) {
      anchorSwarmRejoinLastStateLogMs = now;
      Serial.printf("[ANCHOR/REJOIN] state reason=%s missing=%lums masterAge=%lums anyRxAge=%lums tx=%lu/%lu cb=%lu/%lu direct=%lu/%lu jobAge=%lums known=%u rescues=%u\n",
                    rejoinReason, (unsigned long)missingMs, (unsigned long)masterAge,
                    (unsigned long)anyRxAge, (unsigned long)txOk, (unsigned long)txFail,
                    (unsigned long)cbOk, (unsigned long)cbFail,
                    (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                    (unsigned long)jobAge, buzzMasterMacKnown ? 1 : 0,
                    (unsigned)anchorSwarmRejoinEpisodeRescues);
    }
#if ANCHOR_SWARM_REJOIN_HARD_RESTART
    if (now > ANCHOR_SWARM_REJOIN_BOOT_GRACE_MS &&
        missingMs >= ANCHOR_SWARM_REJOIN_HARD_RESTART_MS &&
        anchorSwarmRejoinEpisodeRescues >= ANCHOR_SWARM_REJOIN_HARD_MIN_RESCUES) {
      anchorSwarmRejoinHardRestarts++;
      Serial.printf("[ANCHOR/REJOIN] hard_restart reason=swarm_missing_too_long n=%lu missing=%lums masterAge=%lums anyRxAge=%lums rescues=%u\n",
                    (unsigned long)anchorSwarmRejoinHardRestarts, (unsigned long)missingMs,
                    (unsigned long)masterAge, (unsigned long)anyRxAge,
                    (unsigned)anchorSwarmRejoinEpisodeRescues);
      delay(120); ESP.restart();
    }
#endif
  } else if (anchorSwarmRejoinFirstMissingMs) {
