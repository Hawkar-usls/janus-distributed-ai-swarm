  }
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '?' || c == 'h') printHelp();
    else if (c == 's' || c == 'S') printStatus();
    else if (c == 't' || c == 'T') cycleTftProfile("serial");
    else if (c == '1') setRadioChannel(1, "serial");
    else if (c == '6') setRadioChannel(6, "serial");
    else if (c == '0') setRadioChannel(10, "serial");
    else if (c == 'c' || c == 'C') {
      scanIndex = (uint8_t)((scanIndex + 1) % JANUS_SCAN_CHANNEL_COUNT);
      setRadioChannel(JANUS_SCAN_CHANNELS[scanIndex], "serial-cycle");
    } else if (c == 'r' || c == 'R') {
      bestBits = 0;
      bestNonce = 0;
      memset(bestTail, 0, sizeof(bestTail));
      snprintf(statusLine, sizeof(statusLine), "BEST RESET");
      printStatus();
    } else if (c == 'f' || c == 'F') {
      jediShareFlashUntilMs = millis() + 900UL;
      snprintf(statusLine, sizeof(statusLine), "JEDI FLASH");
    } else if (c == 'p' || c == 'P') {
      startJediProbe();
    } else if (c == 'l' || c == 'L') {
      setKyberShield(!kyberShield, "serial");
    } else if (c == 'g' || c == 'G') {
      starForgeReforgeCurrent("serial");
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(250);
  pinMode(GOLCRON_BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(GOLCRON_BUTTON_B_PIN, INPUT);
  jediLedBegin();
  loadUiState();
  displayBegin();
  radioBegin();
  lastRateMs = millis();
  lastStatusMs = millis();
  printHelp();
  Serial.printf("[GOLCRON] %s ready node=%s display=\"%s\" ch=%u worker=%u tft=%d sprite=%d backlight=%u steady=1 led=%u btnA=%u btnB=%u observer_math=adaptive_unseen_slices pixel_cosmos=2 no_repeat=1 controls=tap_screen_hold_frame\n",
                GOLCRON_VERSION, GOLCRON_NODE_ID, GOLCRON_DISPLAY_NAME, activeChannel,
                workerId(), displayOk ? 1 : 0, charmReady ? 1 : 0,
                (unsigned)GOLCRON_BACKLIGHT_PIN, (unsigned)JEDI_LED_PIN,
                (unsigned)GOLCRON_BUTTON_A_PIN, (unsigned)GOLCRON_BUTTON_B_PIN);
}

void loop() {
  uint32_t loopUsNow = micros();
  if (lastLoopUs) {
    loopJitterUs = loopUsNow - lastLoopUs;
    if (loopJitterUs > loopMaxUs) loopMaxUs = loopJitterUs;
  }
  lastLoopUs = loopUsNow;

  handleSerial();
  drainRxQueue();
  uint32_t now = millis();
  buttonTick(now);

  uint16_t b = batchSize;
  if (!buzzSeen || now - lastBuzzMs > 12000UL) b = 40;
  if (agentHint == 3) b = min<uint16_t>(GOLCRON_MAX_BATCH, b + 120);
  if (agentHint == 2) b = max<uint16_t>(GOLCRON_MIN_BATCH, b / 2);
  b = starForgeBatch(b);
  runMinerSlice(b);
  updateRate();
  starForgeTick(now);

  channelScanTick(now);
  if (now - lastStatusMs >= STATUS_MS) {
    sendColonyStatus();
    sendSwarmSense();
    lastStatusMs = now;
    Serial.printf("[GOLCRON] buzz=%u job=%u age=%lu ch=%u H=%lu best=%lu/%u shares=%lu selfReject=%lu jobs=%lu/%lu done=%lu stale=%lu dup=%lu bad=%lu rxDrop=%lu star=%s house=%u forge=%u:%s score=%u E%u Ht%u path=%lu/%lu slice=%lu pos=%lu/%lu off=%lu stride=%lu cursor=%lu/%lu auto=%lu manual=%lu tx=%lu/%lu rssi=%d status=\"%s\"\n",
                  buzzSeen ? 1 : 0, job.active ? 1 : 0,
                  job.active ? (unsigned long)(now - job.rxMs) : 0UL,
                  (unsigned)activeChannel,
                  (unsigned long)hashRate, (unsigned long)bestBits, (unsigned)targetBitsNow,
                  (unsigned long)sharesSent, (unsigned long)shareRejectSelf,
                  (unsigned long)jobsRx, (unsigned long)discoveryRx,
                  (unsigned long)jobDoneCount, (unsigned long)jobStaleCount,
                  (unsigned long)duplicateJobRx, (unsigned long)badJobRx,
                  (unsigned long)rxDrops,
                  currentStar().name, (unsigned)currentStar().house,
                  (unsigned)forge.lane, STAR_FORGE_NAMES[forge.lane], (unsigned)forge.lastScore,
                  (unsigned)forge.energy, (unsigned)forge.heat,
                  (unsigned long)jobPathNumber(), (unsigned long)job.sliceCount,
                  (unsigned long)job.sliceIndex, (unsigned long)job.sliceCursor, (unsigned long)job.sliceSize,
                  (unsigned long)job.offset, (unsigned long)job.stride,
                  (unsigned long)job.cursor, (unsigned long)job.rangeSize,
                  (unsigned long)forge.autoPaths, (unsigned long)forge.manualPlans,
                  (unsigned long)txOk, (unsigned long)txFail, (int)lastRssi, statusLine);
  }
  if (now - lastHiveMs >= 2500UL) {
    sendHiveMetric();
    lastHiveMs = now;
  }
  if (!kyberShield && now - lastDisplayMs >= DISPLAY_MS) {
    drawDisplayPortrait();
    lastDisplayMs = now;
  }
  if (!displayLastFpsMs) displayLastFpsMs = now;
  if (now - displayLastFpsMs >= 1000UL) {
    uint32_t dt = now - displayLastFpsMs;
    displayFps = dt ? (uint16_t)((displayFrames * 1000UL) / dt) : 0;
    displayFrames = 0;
    displayLastFpsMs = now;
  }
  jediLedTick(now);
}
