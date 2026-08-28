#pragma once

#include <M5Cardputer.h>
#include <WiFi.h>
#include <Preferences.h>

namespace janus_adv_elite {

// ToyCastle-first Wi-Fi state machine.
// The primary password is supplied by private compile-time configuration and is
// never written by this class to source/registry. User-selected fallback
// credentials are kept locally in NVS so street/hotspot use does not require
// re-entry every time.
class AdvWifiManager {
 public:
  enum class State : uint8_t {
    IDLE = 0,
    CONNECT_PRIMARY,
    CONNECT_SAVED,
    SCANNING,
    PICKER,
    PASSWORD,
    CONNECT_PICKED,
    CONNECTED,
    ERROR_STATE
  };

  void begin(const char* primarySsid, const char* primaryPassword) {
    primarySsid_ = primarySsid ? primarySsid : "";
    primaryPassword_ = primaryPassword ? primaryPassword : "";
    loadSaved();
    state_ = WiFi.status() == WL_CONNECTED ? State::CONNECTED : State::IDLE;
  }

  void enterRadio() {
    connectedEvent_ = false;
    if (WiFi.status() == WL_CONNECTED) {
      state_ = State::CONNECTED;
      return;
    }
    startPrimary();
  }

  void leaveRadio() {
    // Keep Wi-Fi association alive for swarm/radio reuse. Only close the UI.
    if (WiFi.status() == WL_CONNECTED) state_ = State::CONNECTED;
    else if (state_ == State::PASSWORD || state_ == State::PICKER) state_ = State::IDLE;
  }

  void tick() {
    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (state_ != State::CONNECTED) {
        if (state_ == State::CONNECT_PICKED) stagePickedSave();
        state_ = State::CONNECTED;
        connectedEvent_ = true;
        error_[0] = 0;
      }
      return;
    }

    if (state_ == State::CONNECT_PRIMARY && now - stateStartedMs_ > 3300UL) {
      if (!startSaved()) startScan();
      return;
    }
    if (state_ == State::CONNECT_SAVED && now - stateStartedMs_ > 3600UL) {
      startScan();
      return;
    }
    if (state_ == State::CONNECT_PICKED && now - stateStartedMs_ > 6500UL) {
      snprintf(error_, sizeof(error_), "CONNECT FAILED");
      WiFi.disconnect(false, false);
      state_ = State::ERROR_STATE;
      stateStartedMs_ = now;
      return;
    }
    if (state_ == State::SCANNING) {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        scanCount_ = min(n, kMaxScan);
        selected_ = 0;
        state_ = scanCount_ > 0 ? State::PICKER : State::ERROR_STATE;
        if (scanCount_ == 0) snprintf(error_, sizeof(error_), "NO NETWORKS");
        stateStartedMs_ = now;
      } else if (n == WIFI_SCAN_FAILED || now - stateStartedMs_ > 10000UL) {
        snprintf(error_, sizeof(error_), "SCAN FAILED");
        state_ = State::ERROR_STATE;
        stateStartedMs_ = now;
      }
    }
  }

  bool overlayActive() const {
    return state_ != State::CONNECTED && state_ != State::IDLE;
  }

  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  bool passwordEntry() const { return state_ == State::PASSWORD; }
  State state() const { return state_; }

  void servicePersistence() {
    if (!savePending_) return;
    savePending_ = false;
    if (!store_.begin("adv_wifi", false)) return;
    store_.putString("ssid", savedSsid_);
    store_.putString("pwd", savedPassword_);
    store_.end();
  }

  bool consumeConnectedEvent() {
    bool v = connectedEvent_;
    connectedEvent_ = false;
    return v;
  }

  void forceScan() {
    connectedEvent_ = false;
    WiFi.disconnect(false, false);
    startScan();
  }

  void onUp() {
    if (state_ != State::PICKER || scanCount_ <= 0) return;
    selected_ = (selected_ + scanCount_ - 1) % scanCount_;
  }

  void onDown() {
    if (state_ != State::PICKER || scanCount_ <= 0) return;
    selected_ = (selected_ + 1) % scanCount_;
  }

  void onBackspace() {
    if (state_ != State::PASSWORD || password_.length() == 0) return;
    password_.remove(password_.length() - 1);
  }

  void onChar(char c) {
    if (state_ != State::PASSWORD) return;
    if (c < 32 || c > 126) return;
    if (password_.length() < 63) password_ += c;
  }

  void onEnter() {
    if (state_ == State::PICKER && scanCount_ > 0) {
      pickedSsid_ = WiFi.SSID(selected_);
      wifi_auth_mode_t enc = WiFi.encryptionType(selected_);
      pickedOpen_ = enc == WIFI_AUTH_OPEN;
      password_ = "";
      if (pickedOpen_) connectPicked();
      else state_ = State::PASSWORD;
      return;
    }
    if (state_ == State::PASSWORD) {
      connectPicked();
      return;
    }
    if (state_ == State::ERROR_STATE) {
      startScan();
    }
  }

  const char* activeSsid() {
    if (WiFi.status() == WL_CONNECTED) {
      connectedSsidCache_ = WiFi.SSID();
      return connectedSsidCache_.c_str();
    }
    return "offline";
  }

  void draw(M5GFX& d) {
    const uint16_t bg = d.color565(2, 6, 12);
    d.fillScreen(bg);
    d.setTextSize(1);
    d.setTextColor(TFT_CYAN, bg);
    d.setCursor(2, 2);
    d.print("R / WIFI FALLBACK");
    d.setTextColor(TFT_LIGHTGREY, bg);

    if (state_ == State::CONNECT_PRIMARY) {
      d.setCursor(2, 24); d.printf("Trying primary: %s", primarySsid_.c_str());
      d.setCursor(2, 40); d.print("No input needed...");
    } else if (state_ == State::CONNECT_SAVED) {
      d.setCursor(2, 24); d.printf("Trying saved: %s", savedSsid_.c_str());
      d.setCursor(2, 40); d.print("No input needed...");
    } else if (state_ == State::SCANNING) {
      d.setCursor(2, 24); d.print("ToyCastle unavailable.");
      d.setCursor(2, 40); d.print("Scanning nearby Wi-Fi...");
    } else if (state_ == State::PICKER) {
      d.setCursor(2, 17); d.print("ToyCastle unavailable - select network");
      int first = max(0, selected_ - 2);
      if (first + 5 > scanCount_) first = max(0, scanCount_ - 5);
      for (int row = 0; row < 5 && first + row < scanCount_; ++row) {
        int idx = first + row;
        int y = 34 + row * 15;
        bool sel = idx == selected_;
        wifi_auth_mode_t enc = WiFi.encryptionType(idx);
        d.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, bg);
        String s = WiFi.SSID(idx);
        if (s.length() > 24) s = s.substring(0, 24);
        d.setCursor(3, y);
        d.printf("%c %-24s %4d %s", sel ? '>' : ' ', s.c_str(), WiFi.RSSI(idx), enc == WIFI_AUTH_OPEN ? " " : "*");
      }
      d.setTextColor(TFT_DARKGREY, bg);
      d.setCursor(2, 120); d.print("Fn UP/DOWN  ENTER select  ESC HOME");
    } else if (state_ == State::PASSWORD) {
      d.setCursor(2, 24); d.printf("SSID: %s", pickedSsid_.c_str());
      d.setCursor(2, 42); d.print("Password:");
      String mask;
      int visible = min((int)password_.length(), 28);
      for (int i = 0; i < visible; ++i) mask += '*';
      if ((int)password_.length() > visible) mask = "..." + mask.substring(3);
      d.setTextColor(TFT_YELLOW, bg);
      d.setCursor(2, 57); d.print(mask);
      d.setTextColor(TFT_LIGHTGREY, bg);
      d.setCursor(2, 82); d.print("Type password on ADV keyboard");
      d.setCursor(2, 96); d.print("DEL erase | ENTER connect");
      d.setTextColor(TFT_DARKGREY, bg);
      d.setCursor(2, 120); d.print("Password is hidden | ESC HOME");
    } else if (state_ == State::CONNECT_PICKED) {
      d.setCursor(2, 24); d.printf("Connecting: %s", pickedSsid_.c_str());
      d.setCursor(2, 40); d.print("Saving locally only after success...");
    } else if (state_ == State::ERROR_STATE) {
      d.setTextColor(TFT_RED, bg);
      d.setCursor(2, 26); d.print(error_[0] ? error_ : "WIFI ERROR");
      d.setTextColor(TFT_LIGHTGREY, bg);
      d.setCursor(2, 46); d.print("ENTER scan again");
    }
  }

 private:
  static constexpr int kMaxScan = 16;
  State state_ = State::IDLE;
  uint32_t stateStartedMs_ = 0;
  String primarySsid_;
  String primaryPassword_;
  String savedSsid_;
  String savedPassword_;
  String pickedSsid_;
  String password_;
  String connectedSsidCache_;
  int scanCount_ = 0;
  int selected_ = 0;
  bool pickedOpen_ = false;
  bool connectedEvent_ = false;
  bool savePending_ = false;
  char error_[32] = {};
  Preferences store_;

  void loadSaved() {
    if (!store_.begin("adv_wifi", true)) return;
    savedSsid_ = store_.getString("ssid", "");
    savedPassword_ = store_.getString("pwd", "");
    store_.end();
  }

  void stagePickedSave() {
    if (pickedSsid_.length() == 0) return;
    savedSsid_ = pickedSsid_;
    savedPassword_ = password_;
    savePending_ = true;
  }

  void startPrimary() {
    if (primarySsid_.length() == 0) {
      if (!startSaved()) startScan();
      return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(primarySsid_.c_str(), primaryPassword_.c_str());
    state_ = State::CONNECT_PRIMARY;
    stateStartedMs_ = millis();
  }

  bool startSaved() {
    if (savedSsid_.length() == 0 || savedSsid_ == primarySsid_) return false;
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid_.c_str(), savedPassword_.c_str());
    state_ = State::CONNECT_SAVED;
    stateStartedMs_ = millis();
    return true;
  }

  void startScan() {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    int rc = WiFi.scanNetworks(true, true);
    if (rc == WIFI_SCAN_FAILED) {
      snprintf(error_, sizeof(error_), "SCAN START FAILED");
      state_ = State::ERROR_STATE;
    } else {
      state_ = State::SCANNING;
    }
    stateStartedMs_ = millis();
  }

  void connectPicked() {
    if (pickedSsid_.length() == 0) {
      snprintf(error_, sizeof(error_), "NO SSID");
      state_ = State::ERROR_STATE;
      return;
    }
    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    WiFi.begin(pickedSsid_.c_str(), pickedOpen_ ? nullptr : password_.c_str());
    state_ = State::CONNECT_PICKED;
    stateStartedMs_ = millis();
  }
};

}  // namespace janus_adv_elite
