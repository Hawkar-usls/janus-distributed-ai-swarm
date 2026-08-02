    Serial.printf("[ANCHOR/REJOIN] recovered missing=%lums masterAge=%lums anyRxAge=%lums rescues=%u\n",
                  (unsigned long)janusSafeAgeMs(now, anchorSwarmRejoinFirstMissingMs, 0UL),
                  (unsigned long)masterAge, (unsigned long)anyRxAge,
                  (unsigned)anchorSwarmRejoinEpisodeRescues);
    anchorSwarmRejoinFirstMissingMs = 0;
    anchorSwarmRejoinEpisodeRescues = 0;
  }
  if (rejoinReason) anchorRadioRescue(rejoinReason);
  if (masterBlackout) anchorPresenceBurst("master-blackout");
  if ((txOk != anchorRadioLastTxOkSeen) || (txFail != anchorRadioLastTxFailSeen)) {
    anchorRadioLastTxOkSeen = txOk;
    anchorRadioLastTxFailSeen = txFail;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  uint32_t serialStart = millis();
  while (!Serial && (millis() - serialStart < ANCHOR_SERIAL_WAIT_MS)) delay(10);
  delay(500);
  Serial.println();
  Serial.println("================ JANUS ANCHOR BOOT ================");
  Serial.println("JANUS RF_ANCHOR_AUX v1.20 S2 SAFE_QUEUE / Buzz lottery worker / ESP-NOW brother race with Gladius / Consilium job queue / RF sleeve human sonar");
  Serial.println("[ANCHOR/SERIAL] native USB CDC full logs; UART0 TX stays silent while fullLog=0. Hold BOOT ~3s or send U to toggle UART0 logs/blue activity");
  Serial.println("[ANCHOR/SAFETY] protocol=S2 callbacks=queued discoveryRange0=ignored nonceWalk=full-range wire/header/target=frozen");

  workerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  agentEntropySeed ^= (uint32_t)ESP.getEfuseMac() ^ micros();
  loadLedBrightness();
  setupBrightnessButton();
  setupExtraLeds();

  Serial.printf("[ANCHOR/BOOT] id=%u mac=%llX ledPin=%u led=%u brightness=%u defaultBrightness=%u channel=%u uart0TX=%u uart0RX=%u uartFull=%u smallLed=%u uartMirrorDefault=%u uartInput=%u uartStatus=%u tapDir=%s\n",
                (unsigned)workerId, (unsigned long long)ESP.getEfuseMac(),
                (unsigned)ANCHOR_LED_PIN, (unsigned)ANCHOR_LED_ENABLE,
                (unsigned)ledBrightness, (unsigned)ANCHOR_LED_BRIGHTNESS,
                (unsigned)JANUS_FORCE_CHANNEL, (unsigned)JANUS_UART0_TX_PIN, (unsigned)JANUS_UART0_RX_PIN,
                janusUart0FullLog ? 1 : 0, anchorSmallLedEnabled ? 1 : 0, (unsigned)JANUS_UART0_MIRROR_ENABLE,
                (unsigned)JANUS_UART0_INPUT_ENABLE, (unsigned)JANUS_UART0_STATUS_ENABLE,
                buttonBrightnessDirUp ? "up" : "down");
#if ANCHOR_LED_ENABLE
  Serial.printf("[ANCHOR/LED] enabled normal=armymen_green twin=gladius_turquoise shareGlow=pearl_green_turquoise_swap twinRace=J/T maxFlash=white/%lums pin=%u brightness=%u buttonPin=%u minBrightness=0 tapCyclesBrightness=1 veryLongButtonTogglesUartLogsPlusSmallLed=%lums\n",
                (unsigned long)ANCHOR_LED_MAX_FLASH_MS, (unsigned)ANCHOR_LED_PIN,
                (unsigned)ledBrightness, (unsigned)ANCHOR_BUTTON_PIN,
                (unsigned long)ANCHOR_BUTTON_LOG_TOGGLE_MS);
#else
  Serial.println("[ANCHOR/LED] disabled");
#endif
  if (ANCHOR_LED_ENABLE && ANCHOR_LED_PIN > 48) {
    Serial.printf("[ANCHOR/LED] WARN pin=%u looks invalid for ESP32-S3; set ANCHOR_LED_PIN to 48 or 21 if LED stays dark\n", (unsigned)ANCHOR_LED_PIN);
  }
  anchorLedWrite(0, scaleLed(96), scaleLed(35));
  extraLedTick(millis());
  setupWiFi();
  setupEspNow();
  uint32_t now = millis();
  lastHashTickMs = now;
  anchorLoopLastMs = now;
  anchorLoopLastUs = micros();
  anchorPresenceBurst("boot");
  sendRfDome(true);
  rfDebug(now, true);
  anchorStatusTick(now, true);
  minerDebugTick(now, true);
  uart0StatusTick(now, true);
}

void loop() {
  uint32_t now = millis();
  uint32_t loopNowUs = micros();
  if (anchorLoopLastUs) {
    uint32_t intervalUs = loopNowUs - anchorLoopLastUs;
    uint32_t jitterUs = (intervalUs > 1000UL) ? (intervalUs - 1000UL) : (1000UL - intervalUs);
    anchorLoopJitterUs = (uint16_t)min<uint32_t>(65535UL, jitterUs);
    anchorLoopMaxUs = max(anchorLoopMaxUs, anchorLoopJitterUs);
  }
  anchorLoopLastUs = loopNowUs;
  anchorLoopLastMs = now;
  anchorProcessNowQueue();
  anchorSentCallbackTick();
  serialCommandTick();
  anchorWiFiTick(now);
  anchorWaitTick(now);
  ensurePeer();
  anchorRadioWatchdog(now);
  if (janusSafeAgeMs(now, anchorLastPresenceRefreshMs, 0UL) >= ANCHOR_PRESENCE_REFRESH_MS) {
    anchorLastPresenceRefreshMs = now;
    anchorPresenceBurst("ttl-refresh");
  }
  rfTick(now);
  brightnessButtonTick(now);
  brightnessSaveTick(now);
  janusFaceTick(now);
  janusTwinTaskTick(now);
  anchorTorricelliBondTick(now);
  anchorTranceptionLiteTick(now);
  anchorLedTick(now);
  extraLedTick(now);
  runMining();
  now = millis();
  janusJobHousekeeping(now);
  minerDebugTick(now, false);
  uart0StatusTick(now, false);
  if (janusSafeAgeMs(now, lastHeartbeatMs, 0UL) >= COLONY_HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }
  if (janusSafeAgeMs(now, lastEntropyMs, 0UL) >= COLONY_ENTROPY_MS) {
    lastEntropyMs = now;
    sendEntropy();
  }
  sendSwarmSense(false);
  sendAnchorPnCortex(false);
  sendRfDome(false);
  rfDebug(now, false);
  anchorStatusTick(now, false);
  delay(1);
}
