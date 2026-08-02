         xQueueReceive(anchorNowQueue, &item, 0) == pdTRUE) {
    anchorHandleRxFrame(item);
    processed++;
  }
}

uint8_t currentChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary) return primary;
  return JANUS_FORCE_CHANNEL;
}

bool janusMacUsable(const uint8_t* mac) {
  if (!mac) return false;
  bool allZero = true, allFF = true;
  for (int i = 0; i < 6; ++i) { allZero = allZero && (mac[i] == 0x00); allFF = allFF && (mac[i] == 0xFF); }
  return !allZero && !allFF;
}
void janusFormatMac(const uint8_t* mac, char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  if (!janusMacUsable(mac)) { strlcpy(out, "--:--:--:--:--:--", outLen); return; }
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
void rememberBuzzMasterMac(const uint8_t* mac, const char* reason) {
  uint32_t now = millis();
  if (!janusMacUsable(mac)) { buzzMasterMacMissing++; return; }
  bool changed = !buzzMasterMacKnown || memcmp(buzzMasterMac, mac, 6) != 0;
  memcpy(buzzMasterMac, mac, 6); buzzMasterMacKnown = true; buzzMasterMacSeenMs = now;
  if (changed) buzzMasterPeerChannel = 0;
  if (changed || now - buzzMasterLastLogMs > 30000UL) {
    buzzMasterLastLogMs = now;
    char buf[24]; janusFormatMac(buzzMasterMac, buf, sizeof(buf));
    Serial.printf("[ANCHOR/MASTER] mac=%s reason=%s changed=%u ch=%u direct=%lu/%lu missing=%lu\n",
                  buf, reason ? reason : "?", changed ? 1 : 0, (unsigned)currentChannel(),
                  (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                  (unsigned long)buzzMasterMacMissing);
  }
}
bool ensureBuzzMasterPeer(const char* reason) {
  if (!buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return false;
  uint8_t ch = currentChannel();
  if (esp_now_is_peer_exist(buzzMasterMac) && buzzMasterPeerChannel == ch) return true;
  if (esp_now_is_peer_exist(buzzMasterMac)) esp_now_del_peer(buzzMasterMac);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, buzzMasterMac, 6); peer.channel = ch; peer.ifidx = WIFI_IF_STA; peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
    buzzMasterPeerChannel = ch;
    char buf[24]; janusFormatMac(buzzMasterMac, buf, sizeof(buf));
    Serial.printf("[ANCHOR/MASTER] peer ready mac=%s ch=%u reason=%s\n", buf, (unsigned)ch, reason ? reason : "?");
    return true;
  }
  buzzMasterPeerChannel = 0;
  char buf[24]; janusFormatMac(buzzMasterMac, buf, sizeof(buf));
  Serial.printf("[ANCHOR/MASTER] peer add fail mac=%s err=%d ch=%u reason=%s\n", buf, (int)err, (unsigned)ch, reason ? reason : "?");
  return false;
}
esp_err_t sendEspNowToBuzzMaster(const char* tag, const void* payload, size_t len) {
  if (!payload || !len || !buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return ESP_ERR_INVALID_STATE;
  if (!ensureBuzzMasterPeer(tag ? tag : "direct")) { buzzMasterDirectFail++; return ESP_ERR_ESPNOW_NOT_INIT; }
  esp_err_t err = esp_now_send(buzzMasterMac, (const uint8_t*)payload, len);
  if (err == ESP_OK) buzzMasterDirectOk++;
  else {
    buzzMasterDirectFail++; buzzMasterPeerChannel = 0;
    Serial.printf("[ANCHOR/MASTER/TXFAIL] tag=%s err=%d direct=%lu/%lu ch=%u peerCh=%u\n",
                  tag ? tag : "?", (int)err, (unsigned long)buzzMasterDirectOk,
                  (unsigned long)buzzMasterDirectFail, (unsigned)currentChannel(), (unsigned)buzzMasterPeerChannel);
  }
  return err;
}
void ensurePeer() {
  uint8_t ch = currentChannel();
  bool broadcastReady = esp_now_is_peer_exist(JANUS_BROADCAST_MAC) && peerChannel == ch;
  if (!broadcastReady) {
    if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6); peer.channel = ch; peer.ifidx = WIFI_IF_STA; peer.encrypt = false;
    if (esp_now_add_peer(&peer) == ESP_OK) { peerChannel = ch; Serial.printf("[ANCHOR] peer ready ch=%u\n", (unsigned)ch); }
  }
  if (buzzMasterMacKnown) ensureBuzzMasterPeer("ensure");
}
bool anchorWiFiConfigured() {
#if JANUS_USE_WIFI_STA
  return strlen(JANUS_WIFI_SSID) > 0 && strcmp(JANUS_WIFI_SSID, "YOUR_WIFI") != 0;
#else
  return false;
#endif
}
void anchorSetOfflineChannel() {
  esp_wifi_set_promiscuous(true); esp_wifi_set_channel(JANUS_FORCE_CHANNEL, WIFI_SECOND_CHAN_NONE); esp_wifi_set_promiscuous(false);
}
void setupWiFi() {
  WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setAutoReconnect(true);
  if (anchorWiFiConfigured()) {
    WiFi.begin(JANUS_WIFI_SSID, JANUS_WIFI_PASS); lastWiFiReconnectMs = millis(); uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && janusSafeAgeMs(millis(), t0, 0UL) < 7000UL) delay(100);
  }
  lastWiFiStatus = WiFi.status();
  if (lastWiFiStatus == WL_CONNECTED) {
    lastWiFiChannel = currentChannel();
    Serial.printf("[ANCHOR/WIFI] sta=OK rssi=%d ch=%u ip=%s autoReconnect=1\n", WiFi.RSSI(), (unsigned)lastWiFiChannel, WiFi.localIP().toString().c_str());
  } else {
    anchorSetOfflineChannel(); lastWiFiChannel = JANUS_FORCE_CHANNEL;
    Serial.printf("[ANCHOR/WIFI] sta=FAIL offline ESP-NOW channel=%u reconnect=%lums\n", (unsigned)JANUS_FORCE_CHANNEL, (unsigned long)ANCHOR_WIFI_RECONNECT_MS);
  }
}
void anchorWiFiTick(uint32_t now) {
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    uint8_t ch = currentChannel();
    if (lastWiFiStatus != WL_CONNECTED || ch != lastWiFiChannel) {
      Serial.printf("[ANCHOR/WIFI] online rssi=%d ch=%u oldCh=%u ip=%s\n", WiFi.RSSI(), (unsigned)ch, (unsigned)lastWiFiChannel, WiFi.localIP().toString().c_str());
      lastWiFiChannel = ch; peerChannel = 0; buzzMasterPeerChannel = 0; ensurePeer(); anchorPresenceBurst("wifi-online");
    }
  } else {
    if (anchorWiFiConfigured() && janusSafeAgeMs(now, lastWiFiReconnectMs, 0UL) >= ANCHOR_WIFI_RECONNECT_MS) {
      lastWiFiReconnectMs = now;
      Serial.printf("[ANCHOR/WIFI] reconnect status=%d target=%s\n", (int)status, JANUS_WIFI_SSID);
      WiFi.disconnect(false, false); WiFi.begin(JANUS_WIFI_SSID, JANUS_WIFI_PASS);
    }
    static uint32_t lastOfflineChannelSetMs = 0;
    bool reconnectWindow = janusSafeAgeMs(now, lastWiFiReconnectMs, 0UL) < 9000UL;
    if (!reconnectWindow && janusSafeAgeMs(now, lastOfflineChannelSetMs, 0UL) >= 10000UL) {
      lastOfflineChannelSetMs = now; anchorSetOfflineChannel();
      if (lastWiFiChannel != JANUS_FORCE_CHANNEL) { lastWiFiChannel = JANUS_FORCE_CHANNEL; peerChannel = 0; buzzMasterPeerChannel = 0; }
    }
  }
  lastWiFiStatus = status;
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
#else
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status)
#endif
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)info;
#else
  (void)mac_addr;
#endif
  if (status == ESP_NOW_SEND_SUCCESS) { sentCbOk++; sentCbConsecutiveFail = 0; }
  else { sentCbFail++; if (sentCbConsecutiveFail < 0xFFFFU) sentCbConsecutiveFail++; sentCbSawFailure = true; }
}
void anchorSentCallbackTick() {
