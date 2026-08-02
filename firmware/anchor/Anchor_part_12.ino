  uint32_t ok = sentCbOk;
  uint32_t fail = sentCbFail;
  if (ok == sentCbLastReportedOk && fail == sentCbLastReportedFail) return;
  uint32_t total = ok + fail;
  bool logNow = sentCbSawFailure || ((total % ANCHOR_SENT_LOG_EVERY) == 1UL);
  if (logNow) {
    Serial.printf("[ANCHOR/SENT] cb=%s ok=%lu fail=%lu txLocal=%lu/%lu ch=%u qDrop=%lu\n",
                  sentCbSawFailure ? "FAIL_SEEN" : "OK", (unsigned long)ok, (unsigned long)fail,
                  (unsigned long)txOk, (unsigned long)txFail, (unsigned)peerChannel, (unsigned long)rxQueueDrops);
    sentCbSawFailure = false;
  }
  sentCbLastReportedOk = ok;
  sentCbLastReportedFail = fail;
}
void minerDebugTick(uint32_t now, bool force);
void uart0StatusTick(uint32_t now, bool force);
void serialCommandTick() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') continue;
    if (c == '?' || c == 'h' || c == 'H') {
      Serial.println("[ANCHOR/CMD] v1.20 protocol=S2 keys: s=status r=rf m=miner +=brighter -=dimmer 0=all_leds_off 9=max l=led u=toggle_uart_full_logs+smallLED b=heartbeat e=entropy p=sense j=job x=reboot ?=help; BOOT tap cycles brightness 0->max->0, very long toggles logs+smallLED");
    } else if (c == 's' || c == 'S') anchorStatusTick(millis(), true);
    else if (c == 'r' || c == 'R') rfDebug(millis(), true);
    else if (c == 'm' || c == 'M') minerDebugTick(millis(), true);
    else if (c == '+') { buttonBrightnessDirUp = true; stepLedBrightness(ANCHOR_BRIGHTNESS_STEP, "serial_plus"); }
    else if (c == '-') { buttonBrightnessDirUp = false; stepLedBrightness(-ANCHOR_BRIGHTNESS_STEP, "serial_minus"); }
    else if (c == '0') { buttonBrightnessDirUp = true; setLedBrightness(0, "serial_all_leds_off"); }
    else if (c == '9') { buttonBrightnessDirUp = false; setLedBrightness(ANCHOR_BRIGHTNESS_MAX, "serial_max_brightness"); }
    else if (c == 'u' || c == 'U') toggleUart0FullLog("serial_u_toggle");
    else if (c == 'l' || c == 'L') {
      Serial.printf("[ANCHOR/LED] brightness=%u rgb=%u,%u,%u shareGlow=%lums maxFlash=%lums buttonPin=%u extraBluePin=%d extraYellowPin=%d uartFull=%u smallLed=%u uartMirrorDefault=%u uartInput=%u uartStatus=%u tapDir=%s\n",
                    (unsigned)ledBrightness, (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB,
                    (unsigned long)((lastShareMs && janusSafeAgeMs(millis(), lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS) ? (ANCHOR_LED_SHARE_MS - janusSafeAgeMs(millis(), lastShareMs, 0UL)) : 0UL),
                    (unsigned long)((lastMaxBrightnessFlashMs && janusSafeAgeMs(millis(), lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) ? (ANCHOR_LED_MAX_FLASH_MS - janusSafeAgeMs(millis(), lastMaxBrightnessFlashMs, 0UL)) : 0UL),
                    (unsigned)ANCHOR_BUTTON_PIN, (int)ANCHOR_EXTRA_BLUE_PIN, (int)ANCHOR_EXTRA_YELLOW_PIN,
                    janusUart0FullLog ? 1 : 0, anchorSmallLedEnabled ? 1 : 0, (unsigned)JANUS_UART0_MIRROR_ENABLE,
                    (unsigned)JANUS_UART0_INPUT_ENABLE, (unsigned)JANUS_UART0_STATUS_ENABLE,
                    buttonBrightnessDirUp ? "up" : "down");
    } else if (c == 'b' || c == 'B') sendHeartbeat();
    else if (c == 'e' || c == 'E') sendEntropy();
    else if (c == 'p' || c == 'P') sendSwarmSense(true);
    else if (c == 'j' || c == 'J') {
      Serial.printf("[ANCHOR/JOBSTATE] active=%u seen=%lu accepted=%lu done=%lu exp=%lu q=%u queued=%lu yielded=%lu repl=%lu dup=%lu age=%lums start=%08lX range=%lu doneHashes=%lu lane=%s/s%u stride=%lu arm=%u targetBits=%u qStart=%08lX qAge=%lums\n",
                    job.active ? 1 : 0, (unsigned long)jobsSeen, (unsigned long)jobsAccepted, (unsigned long)jobsDone,
                    (unsigned long)jobsExpired, queuedJobValid ? 1 : 0, (unsigned long)jobsQueued,
                    (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork, (unsigned long)jobsDroppedDuplicate,
                    (unsigned long)(job.active ? janusSafeAgeMs(millis(), job.receivedAt, 0UL) : 0UL),
                    (unsigned long)job.startNonce, (unsigned long)job.rangeSize, (unsigned long)job.hashesDone,
                    laneName(job.minerLane), (unsigned)job.minerSector, (unsigned long)job.minerStride,
                    (unsigned)job.minerStrideArm, (unsigned)targetBits,
                    (unsigned long)(queuedJobValid ? queuedJob.startNonce : 0UL),
                    (unsigned long)(queuedJobValid ? janusSafeAgeMs(millis(), queuedJobAtMs, 0UL) : 0UL));
    } else if (c == 'x' || c == 'X') { Serial.println("[ANCHOR/CMD] rebooting"); delay(100); ESP.restart(); }
    else Serial.printf("[ANCHOR/CMD] got='%c' ; press ? for help\n", c);
  }
}
void minerDebugTick(uint32_t now, bool force=false) {
  if (!force && janusSafeAgeMs(now, lastMinerLogMs, 0UL) < ANCHOR_MINER_DEBUG_MS) return;
  lastMinerLogMs = now;
  uint32_t jobAge = job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL;
  uint32_t left = (job.active && job.rangeSize > job.hashesDone) ? (job.rangeSize - job.hashesDone) : 0UL;
  uint16_t batch = activeBatch();
  Serial.printf("[ANCHOR/MINER] proto=S2 speed=%luH/s ema=%.0f active=%u q=%u age=%lums batch=%u checked=%lu/%lu left=%lu lane=%s/s%u stride=%lu arm=%u best=%lu/%u life=%lu nonce=%08lX shares=%lu jobs=%lu/%lu/%lu accept=%lu queued=%lu yield=%lu repl=%lu dup=%lu ping=%lu bad=%lu total=%lu tail=%lu\n",
                (unsigned long)hashRate, hashRateEma, job.active ? 1 : 0, queuedJobValid ? 1 : 0,
                (unsigned long)jobAge, (unsigned)batch, (unsigned long)job.hashesDone,
                (unsigned long)job.rangeSize, (unsigned long)left, laneName(job.minerLane),
                (unsigned)job.minerSector, (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                (unsigned long)bestBits, (unsigned)targetBits, (unsigned long)bestBitsLifetime,
                (unsigned long)bestNonce, (unsigned long)shares, (unsigned long)jobsSeen,
                (unsigned long)jobsDone, (unsigned long)jobsExpired, (unsigned long)jobsAccepted,
                (unsigned long)jobsQueued, (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork,
                (unsigned long)jobsDroppedDuplicate, (unsigned long)jobsDiscoveryPings, (unsigned long)jobsInvalid,
                (unsigned long)totalHashesLifetime, (unsigned long)tailHits);
}
void anchorWaitTick(uint32_t now) {
  if (now - lastWaitLogMs < ANCHOR_WAIT_LOG_MS) return;
  lastWaitLogMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/WAIT] alive uptime=%lums ch=%u rx=%lu janus=%lu jobs=%lu agent=%lu masterAge=%lums job=%u q=%u H=%lu best=%lu rfReady=%u rfP=%.2f rfM=%.2f tx=%lu/%lu heap=%lu\n",
                (unsigned long)now, (unsigned)peerChannel, (unsigned long)rxSeen, (unsigned long)rxJanus,
                (unsigned long)rxJobs, (unsigned long)rxAgent, (unsigned long)masterAge,
                job.active ? 1 : 0, queuedJobValid ? 1 : 0, (unsigned long)hashRate,
                (unsigned long)bestBits, rfReady ? 1 : 0, rfPresence, rfMotion,
                (unsigned long)txOk, (unsigned long)txFail, (unsigned long)ESP.getFreeHeap());
}
void uart0StatusTick(uint32_t now, bool force=false) {
#if JANUS_UART0_STATUS_ENABLE
  if (!janusUart0FullLog) return;
  if (!force && janusSafeAgeMs(now, lastUart0StatusMs, 0UL) < JANUS_UART0_STATUS_MS) return;
  lastUart0StatusMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
