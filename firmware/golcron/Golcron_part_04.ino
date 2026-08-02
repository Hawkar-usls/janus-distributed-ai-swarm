    charm.drawRect(4, 195, 123, 38, charmView == 2 ? violetSoft : cyanDim);
    if (charmView == 1) {
      charm.setTextColor(gold, TFT_TRANSPARENT);
      charm.drawString("MINER FIELD", 8, 200);
      charm.setTextColor(text, TFT_TRANSPARENT);
      snprintf(line, sizeof(line), "J%lu S%lu TX%lu/%lu", (unsigned long)jobsRx,
               (unsigned long)sharesSent, (unsigned long)txOk, (unsigned long)txFail);
      charm.drawString(line, 8, 212);
      snprintf(line, sizeof(line), "P%lu/%lu POS%lu", (unsigned long)jobPathNumber(),
               (unsigned long)job.sliceCount, (unsigned long)job.sliceCursor);
      charm.drawString(line, 8, 223);
    } else if (charmView == 2) {
      charm.setTextColor(violet, TFT_TRANSPARENT);
      charm.drawString("CHARM DIAG", 8, 200);
      charm.setTextColor(text, TFT_TRANSPARENT);
      snprintf(line, sizeof(line), "FPS%u R%lu H%lu", (unsigned)displayFps,
               (unsigned long)lastRenderUs, (unsigned long)ESP.getFreeHeap());
      charm.drawString(line, 8, 212);
      snprintf(line, sizeof(line), "LJ%lu MX%lu", (unsigned long)loopJitterUs,
               (unsigned long)loopMaxUs);
      charm.drawString(line, 8, 223);
    } else {
      charm.setTextColor(gold, TFT_TRANSPARENT);
      snprintf(line, sizeof(line), "%s / %s", STAR_FORGE_NAMES[forge.lane], job.active ? "FORGE" : "CALM");
      charm.drawString(line, 8, 200);
      charm.setTextColor(text, TFT_TRANSPARENT);
      snprintf(line, sizeof(line), "%s H%u  P%lu/%lu", currentStar().name,
               (unsigned)currentStar().house, (unsigned long)jobPathNumber(),
               (unsigned long)job.sliceCount);
      charm.drawString(line, 8, 212);
      snprintf(line, sizeof(line), "E%u HT%u R%d FPS%u", (unsigned)forge.energy,
               (unsigned)forge.heat, (int)lastRssi, (unsigned)displayFps);
      charm.drawString(line, 8, 223);
    }
  }

  if (overlayUntilMs > now) {
    int boxY = frameVisible ? 176 : 215;
    charm.fillRect(10, boxY, 111, 15, charm.color565(2, 9, 17));
    charm.drawRect(10, boxY, 111, 15, goldSoft);
    charm.setTextColor(text, TFT_TRANSPARENT);
    charm.drawString(overlayLine, 15, boxY + 4);
  }

  charm.pushSprite(0, 0);
  lastRenderUs = micros() - renderStartUs;
  if (lastRenderUs > maxRenderUs) maxRenderUs = lastRenderUs;
  displayFrames++;
}

static void runMinerSlice(uint16_t batch) {
  if (!job.active) return;
  if (millis() - job.rxMs > JOB_TTL_MS) {
    job.active = false;
    jobStaleCount++;
    snprintf(statusLine, sizeof(statusLine), "JOB STALE %lu/%lu",
             (unsigned long)job.cursor, (unsigned long)job.rangeSize);
    return;
  }
  if (job.cursor >= job.rangeSize) {
    job.sliceOrdinal = job.sliceCount;
    job.active = false;
    jobDoneCount++;
    snprintf(statusLine, sizeof(statusLine), "RANGE DONE");
    uiOverlay("RANGE COMPLETE", 1800UL);
    return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  uint8_t header[80];
  uint8_t hash1[32];
  uint8_t hash2[32];
  uint8_t shareHash[32];

  for (uint16_t i = 0; i < batch && job.active && job.cursor < job.rangeSize; ++i) {
    if (job.sliceCursor >= job.sliceSize) {
      if (!starForgeAdvanceSlice("auto-unseen")) {
        job.sliceOrdinal = job.sliceCount;
        job.active = false;
        jobDoneCount++;
        snprintf(statusLine, sizeof(statusLine), "RANGE DONE");
        uiOverlay("RANGE COMPLETE", 1800UL);
        break;
      }
    }

    uint32_t mappedLocal = job.sliceSize ?
      (uint32_t)(((uint64_t)job.sliceCursor * (uint64_t)job.stride + job.offset) % job.sliceSize) : 0;
    uint32_t nonce = job.startNonce + job.sliceStart + mappedLocal;
    job.sliceCursor++;
    job.cursor++;

    memcpy(header, job.header, 80);
    writeLE32(header + 76, nonce);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, header, 80);
    mbedtls_sha256_finish(&ctx, hash1);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, hash1, 32);
    mbedtls_sha256_finish(&ctx, hash2);
    hashToShareOrder(hash2, shareHash);

    totalHashes++;
    windowHashes++;
    uint16_t bits = countLeadingZeroBits(shareHash);
    bool accepted = false;
    if (bits >= targetBitsNow) {
      if (hashMeetsTargetBytes(shareHash, job.target)) accepted = true;
      else shareRejectSelf++;
    }
    starForgeLearn(bits, accepted);

    if (bits > bestBits) {
      bestBits = bits;
      bestNonce = nonce;
      memcpy(bestTail, shareHash + 28, 4);
      starForgeObserve(bits, false);
    }
    if (accepted) {
      sendShare(nonce, bits, shareHash);
      snprintf(statusLine, sizeof(statusLine), "SHARE z%u", (unsigned)bits);
      uiOverlay("STAR SHARE SENT", 1800UL);
    }

    if ((i & 63U) == 63U && rxHead != rxTail) break;
  }
  mbedtls_sha256_free(&ctx);

  if (job.active && job.cursor >= job.rangeSize) {
    job.sliceOrdinal = job.sliceCount;
    job.active = false;
    jobDoneCount++;
    snprintf(statusLine, sizeof(statusLine), "RANGE DONE");
    uiOverlay("RANGE COMPLETE", 1800UL);
  }
}

static void updateRate() {
  uint32_t now = millis();
  if (now - lastRateMs < 1000UL) return;
  uint32_t dt = now - lastRateMs;
  hashRate = dt ? (uint32_t)((uint64_t)windowHashes * 1000ULL / dt) : 0;
  windowHashes = 0;
  lastRateMs = now;
}

static void ensureBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = activeChannel;
  peer.encrypt = false;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("[GOLCRON/RADIO] peer add fail err=%d ch=%u\n", (int)err, activeChannel);
  }
}

static void setRadioChannel(uint8_t ch, const char *reason) {
  if (!ch) ch = JANUS_SWARM_CHANNEL;
  activeChannel = ch;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(activeChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  ensureBroadcastPeer();
  Serial.printf("[GOLCRON/RADIO] channel=%u reason=%s\n", activeChannel, reason ? reason : "-");
}

static void channelScanTick(uint32_t now) {
#if JANUS_ENABLE_CHANNEL_SCAN
  if (buzzSeen && now - lastBuzzMs < 12000UL) return;
  if (job.active) return;
  if (now - lastScanMs < 3500UL) return;
  lastScanMs = now;
  scanIndex = (uint8_t)((scanIndex + 1) % JANUS_SCAN_CHANNEL_COUNT);
  setRadioChannel(JANUS_SCAN_CHANNELS[scanIndex], "scan");
#else
  (void)now;
#endif
}

static void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(100);
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[GOLCRON] ESP-NOW init failed, reboot");
    delay(500);
    ESP.restart();
  }
  setRadioChannel(JANUS_SWARM_CHANNEL, "boot");
  ensureBroadcastPeer();
  esp_now_register_recv_cb([](const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!data || len < 2) return;
    int8_t rxRssi = -127;
    if (info && info->rx_ctrl) rxRssi = info->rx_ctrl->rssi;
    if (len == (int)sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
      RxItem item{};
      item.kind = RX_JOB;
      item.rssi = rxRssi;
      memcpy(&item.body.job, data, sizeof(JobPacket));
      enqueueRx(item);
      return;
    }
    if (len == (int)sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
      RxItem item{};
      item.kind = RX_REWARD;
      item.rssi = rxRssi;
      memcpy(&item.body.reward, data, sizeof(JanusAgentRewardPacket));
      enqueueRx(item);
      return;
    }
  });
  esp_now_register_send_cb([](
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    const wifi_tx_info_t *,
#else
    const uint8_t *,
#endif
    esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) txOk++;
    else txFail++;
  });
}

static void drainRxQueue() {
  RxItem item;
  uint8_t drained = 0;
  while (dequeueRx(&item) && drained++ < RX_QUEUE_DEPTH) {
    lastRssi = item.rssi;
    if (item.kind == RX_JOB) {
      onJobPacket(item.body.job);
      if (activeChannel != JANUS_SWARM_CHANNEL && buzzSeen) {
        snprintf(statusLine, sizeof(statusLine), "LOCK CH%u", activeChannel);
      }
    } else if (item.kind == RX_REWARD) {
      if (item.body.reward.version == 1) applyAgentReward(item.body.reward);
    }
  }
}

static void displayBegin() {
  tftProfile = TFT_PROFILES[tftProfileIndex % TFT_PROFILE_COUNT];
  backlightWrite(true);
  tftDirectInit();
  tftFillScreenRaw(directColor565(180, 0, 0));
  delay(180);
  tftFillScreenRaw(directColor565(0, 130, 0));
  delay(180);
  tftFillScreenRaw(directColor565(0, 0, 180));
  delay(180);
  charm.setColorDepth(16);
  charmReady = charm.createSprite(135, 240);
  displayOk = charmReady;
  if (charmReady) {
    charm.fillSprite(TFT_BLACK);
    charm.setTextDatum(TL_DATUM);
    charm.setTextFont(1);
    charm.setTextColor(TFT_CYAN, TFT_BLACK);
    charm.drawString("GOLCRON", 8, 10);
    charm.drawString("ASTROLABE", 8, 28);
    charm.drawString("BH PIXEL COSMOS", 8, 48);
    charm.drawString("USB TOP", 8, 68);
    charm.drawString("A TAP=SCREEN", 8, 88);
    charm.drawString("A HOLD=FRAME", 8, 104);
    charm.pushSprite(0, 0);
  } else {
    tftFillScreenRaw(directColor565(80, 0, 0));
  }
  Serial.printf("[GOLCRON/TFT] direct=1 ready=%u profile=%s off=%u/%u madctl=0x%02X ST7789 135x240 portrait BL=%u steady=1 CS=%u DC=%u RST=%u MOSI=%u SCLK=%u buttons=%u/%u\n",
                displayOk ? 1 : 0, tftProfile.name,
                (unsigned)tftProfile.xOffset, (unsigned)tftProfile.yOffset, (unsigned)tftProfile.madctl,
                (unsigned)GOLCRON_BACKLIGHT_PIN, (unsigned)TFT_CS, (unsigned)TFT_DC,
                (unsigned)TFT_RST, (unsigned)TFT_MOSI, (unsigned)TFT_SCLK,
                (unsigned)GOLCRON_BUTTON_A_PIN, (unsigned)GOLCRON_BUTTON_B_PIN);
}

static void cycleTftProfile(const char *reason) {
  tftProfileIndex = (uint8_t)((tftProfileIndex + 1) % TFT_PROFILE_COUNT);
  displayBegin();
  snprintf(statusLine, sizeof(statusLine), "TFT %s", tftProfile.name);
  uiOverlay("TFT PROFILE", 1200UL);
  Serial.printf("[GOLCRON/TFT] profile-cycle idx=%u/%u reason=%s name=%s\n",
                (unsigned)tftProfileIndex, (unsigned)TFT_PROFILE_COUNT,
                reason ? reason : "-", tftProfile.name);
}

static void printHelp() {
  Serial.println("[GOLCRON/CMD] ? help | s status | t tft profile | c cycle channel | 1 channel 1 | 6 channel 6 | 0 channel 10 | r reset best | f force flash | p probe safe pins | l light shield | g reforge");
  Serial.println("[GOLCRON/KEYS] A(GPIO0) tap=screen on/off | A hold=toggle telemetry frame | B(GPIO35) tap=view | B hold=plan next unseen path");
}

static void printStatus() {
  Serial.printf("[GOLCRON] v=%s node=%s display=\"%s\" buzz=%u job=%u ch=%u worker=%u H=%lu best=%lu/%u nonce=%08lX shares=%lu selfReject=%lu jobs=%lu done=%lu stale=%lu discovery=%lu dup=%lu bad=%lu rxDrop=%lu tx=%lu/%lu rssi=%d batch=%u hint=%u star=%s house=%u lane=%u:%s score=%u path=%lu/%lu slice=%lu pos=%lu/%lu offset=%lu stride=%lu cursor=%lu/%lu auto=%lu manual=%lu heap=%lu status=\"%s\"\n",
                GOLCRON_VERSION, GOLCRON_NODE_ID, GOLCRON_DISPLAY_NAME,
                buzzSeen ? 1 : 0, job.active ? 1 : 0, activeChannel, workerId(),
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned)targetBitsNow,
                (unsigned long)bestNonce,
                (unsigned long)sharesSent, (unsigned long)shareRejectSelf,
                (unsigned long)jobsRx, (unsigned long)jobDoneCount, (unsigned long)jobStaleCount,
                (unsigned long)discoveryRx, (unsigned long)duplicateJobRx, (unsigned long)badJobRx,
                (unsigned long)rxDrops, (unsigned long)txOk, (unsigned long)txFail, (int)lastRssi,
                (unsigned)batchSize, (unsigned)agentHint,
                currentStar().name, (unsigned)currentStar().house,
                (unsigned)forge.lane, STAR_FORGE_NAMES[forge.lane], (unsigned)forge.lastScore,
                (unsigned long)jobPathNumber(), (unsigned long)job.sliceCount,
                (unsigned long)job.sliceIndex, (unsigned long)job.sliceCursor, (unsigned long)job.sliceSize,
                (unsigned long)job.offset, (unsigned long)job.stride,
                (unsigned long)job.cursor, (unsigned long)job.rangeSize,
                (unsigned long)forge.autoPaths, (unsigned long)forge.manualPlans,
                (unsigned long)ESP.getFreeHeap(), statusLine);
  Serial.printf("[GOLCRON/JEDI] led=%u pin=%u backlight=%u activeHigh=%u pattern=%s\n",
                jediLedOk ? 1 : 0, (unsigned)JEDI_LED_PIN, (unsigned)GOLCRON_BACKLIGHT_PIN, (unsigned)JEDI_LED_ACTIVE_HIGH,
                job.active ? "focus" : ((buzzSeen && millis() - lastBuzzMs < 12000UL) ? "calm" : "meditation"));
  Serial.printf("[GOLCRON/JEDI] probe=%d nextProbeIndex=%u note=\"blue USB-side light may be hardwired power/charge\"\n",
                (int)jediProbePin, (unsigned)jediProbeIndex);
  Serial.printf("[GOLCRON/BUTTON] A=%u B=%u tapMin=%lums frameHold=%lums screen=%u frame=%u view=%u\n",
                (unsigned)GOLCRON_BUTTON_A_PIN, (unsigned)GOLCRON_BUTTON_B_PIN,
                (unsigned long)GOLCRON_BUTTON_TAP_MIN_MS, (unsigned long)GOLCRON_FRAME_HOLD_MS,
                kyberShield ? 0 : 1, frameVisible ? 1 : 0, (unsigned)charmView);
  Serial.printf("[GOLCRON/FORGE] online=%u lane=%u name=%s energy=%u heat=%u score=%u best=%u fires=%lu paths=%lu auto=%lu manual=%lu noRepeat=1\n",
                forge.online ? 1 : 0, (unsigned)forge.lane, STAR_FORGE_NAMES[forge.lane],
                (unsigned)forge.energy, (unsigned)forge.heat, (unsigned)forge.lastScore,
                (unsigned)forge.lastBest, (unsigned long)forge.fires, (unsigned long)forge.reforges,
                (unsigned long)forge.autoPaths, (unsigned long)forge.manualPlans);
  for (uint8_t i = 0; i < STAR_FORGE_LANES; ++i) {
    const StarForgeLane &lane = forgeLanes[i];
    uint32_t meanX256 = lane.hashes ? (uint32_t)(((uint64_t)lane.bitSum * 256ULL) / lane.hashes) : 0UL;
    Serial.printf("[GOLCRON/LANE] i=%u name=%s select=%u hashes=%lu meanX256=%lu emaX256=%u best=%u strong=%lu shares=%lu score=%u\n",
                  (unsigned)i, STAR_FORGE_NAMES[i], (unsigned)lane.selections,
                  (unsigned long)lane.hashes, (unsigned long)meanX256,
                  (unsigned)lane.emaBitsX256, (unsigned)lane.bestBits,
                  (unsigned long)lane.strongHits, (unsigned long)lane.shares,
                  (unsigned)lane.score);