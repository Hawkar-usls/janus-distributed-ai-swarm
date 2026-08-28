#pragma once

#include <M5Cardputer.h>
#include <math.h>

namespace janus_adv_elite {

struct BeaconHomeView {
  float tempC = NAN;
  float humidity = NAN;
  float pressureHpa = NAN;
  float entropy = 0.0f;
  float predicted = 0.0f;
  float loss = 0.0f;
  float future1 = 0.0f;
  float future2 = 0.0f;
  float future3 = 0.0f;
  float sync = NAN;
  float shock = 0.0f;
  int battery = 0;
  int wifiRssi = -127;
  uint32_t buzzHashRate = 0;
  uint32_t witnessCount = 0;
  uint32_t anomalyCount = 0;
  uint32_t loopUs = 0;
  bool shtValid = false;
  bool qmpValid = false;
  bool eyeOnline = false;
  bool wifiOnline = false;
  bool m2r = false;
  bool house = false;
  bool lora = false;
  bool love = false;
  bool anomaly = false;
  const char* status = "";
};

class BeaconHomeRenderer {
 public:
  BeaconHomeRenderer() : canvas_(&M5Cardputer.Display) {}

  bool begin() {
    if (ready_) return true;
    canvas_.setColorDepth(8);
    ready_ = canvas_.createSprite(kW, kH) != nullptr;
    if (ready_) {
      canvas_.setTextSize(1);
      canvas_.setTextWrap(false);
      canvas_.fillSprite(TFT_BLACK);
      canvas_.pushSprite(0, 0);
    }
    lastFrameUs_ = micros();
    return ready_;
  }

  bool ready() const { return ready_; }
  float fps() const { return fpsEma_; }
  uint32_t worstFrameUs() const { return worstFrameUs_; }

  void draw(const BeaconHomeView& v, uint32_t nowMs) {
    if (!ready_ && !begin()) return;

    const uint32_t frameNow = micros();
    if (lastFrameUs_ != 0) {
      const uint32_t dt = frameNow - lastFrameUs_;
      if (dt > worstFrameUs_) worstFrameUs_ = dt;
      if (dt > 0) {
        const float inst = 1000000.0f / (float)dt;
        fpsEma_ = fpsEma_ <= 0.1f ? inst : fpsEma_ * 0.90f + inst * 0.10f;
      }
    }
    lastFrameUs_ = frameNow;

    const uint16_t bg = TFT_BLACK;
    const uint16_t primary = v.anomaly ? TFT_WHITE : (v.house ? amber() : TFT_CYAN);
    const uint16_t dim = v.house ? amberDim() : cyanDim();
    const uint16_t text = TFT_WHITE;
    const uint16_t faint = rgb(58, 72, 78);
    const uint16_t good = rgb(80, 235, 170);

    canvas_.fillSprite(bg);

    // Historical Beacon silhouette: one framed instrument body, three fixed
    // columns and a separate ticker rail. One sprite push prevents LCD flicker.
    canvas_.drawRect(1, 1, 238, 119, primary);
    canvas_.drawFastHLine(2, 13, 236, dim);
    canvas_.drawFastVLine(80, 14, 105, dim);
    canvas_.drawFastVLine(159, 14, 105, dim);

    canvas_.setTextColor(primary, bg);
    canvas_.setCursor(5, 3);
    canvas_.print("JANUS // ADV BEACON");
    canvas_.setTextColor(v.wifiOnline ? good : faint, bg);
    canvas_.setCursor(168, 3);
    canvas_.printf("W%s B%02d", v.wifiOnline ? "+" : "-", constrain(v.battery, 0, 99));

    columnTitle(5, 17, "ENV", primary);
    metric(5, 31, "T", v.shtValid, v.tempC, 1, "C", text, faint);
    metric(5, 44, "H", v.shtValid, v.humidity, 0, "%", text, faint);
    metric(5, 57, "P", v.qmpValid, v.pressureHpa, 0, "h", text, faint);
    canvas_.setTextColor(text, bg);
    canvas_.setCursor(5, 70);
    canvas_.printf("SHK %5.2f", v.shock);
    canvas_.setTextColor(faint, bg);
    canvas_.setCursor(5, 86);
    canvas_.printf("ENV %s/%s", v.shtValid ? "+" : "-", v.qmpValid ? "+" : "-");
    canvas_.setCursor(5, 101);
    canvas_.printf("LOOP %4luu", (unsigned long)capU32(v.loopUs, 9999));

    columnTitle(85, 17, "MIND", primary);
    value(85, 31, "E", v.entropy, 3, text);
    value(85, 44, "PR", v.predicted, 3, text);
    value(85, 57, "LS", v.loss, 3, text);
    value(85, 70, "F1", v.future1, 2, text);
    value(85, 83, "F2", v.future2, 2, text);
    canvas_.setTextColor(v.m2r ? primary : faint, bg);
    canvas_.setCursor(85, 101);
    canvas_.printf("M2R %s", v.m2r ? "RUN" : "off");

    columnTitle(164, 17, "SWARM", primary);
    canvas_.setTextColor(v.eyeOnline ? good : faint, bg);
    canvas_.setCursor(164, 31);
    canvas_.printf("EYE %s", v.eyeOnline ? "ON" : "--");
    canvas_.setTextColor(text, bg);
    canvas_.setCursor(164, 44);
    if (isfinite(v.sync)) canvas_.printf("SYN %4.2f", v.sync); else canvas_.print("SYN ----");
    canvas_.setCursor(164, 57);
    if (v.wifiOnline) canvas_.printf("RSS %4d", v.wifiRssi); else canvas_.print("RSS ----");
    canvas_.setCursor(164, 70);
    canvas_.printf("BZ %5lu", (unsigned long)capU32(v.buzzHashRate, 99999));
    canvas_.setCursor(164, 83);
    canvas_.printf("WIT %4lu", (unsigned long)capU32(v.witnessCount, 9999));
    canvas_.setTextColor(v.anomaly ? TFT_WHITE : faint, bg);
    canvas_.setCursor(164, 101);
    canvas_.printf("AN %s %lu", v.anomaly ? "!" : "-", (unsigned long)capU32(v.anomalyCount, 99));

    chip(5, 110, "H", v.house, primary, faint);
    chip(25, 110, "J", v.lora, primary, faint);
    chip(45, 110, "L", v.love, primary, faint);
    canvas_.setTextColor(dim, bg);
    canvas_.setCursor(87, 110);
    canvas_.printf("F3 %.2f", v.future3);
    canvas_.setCursor(166, 110);
    canvas_.printf("FPS %2d", (int)constrain(fpsEma_ + 0.5f, 0.0f, 99.0f));

    // Scroll is clock-based, so dropped frames do not alter ticker speed.
    canvas_.fillRect(0, 121, 240, 14, bg);
    canvas_.drawFastHLine(0, 121, 240, dim);
    char ticker[220];
    snprintf(ticker, sizeof(ticker),
             "  JANUS // %s // E %.2f > %.2f // %s // BZ %lu // W %lu // %s  ",
             v.status ? v.status : "", v.entropy, v.predicted,
             v.eyeOnline ? "EYE ONLINE" : "EYE STALE",
             (unsigned long)v.buzzHashRate, (unsigned long)v.witnessCount,
             v.anomaly ? "ANOMALY" : "WATCH");
    canvas_.setTextColor(primary, bg);
    const int tw = max(1, canvas_.textWidth(ticker));
    const int travel = tw + kW;
    const int x = kW - (int)((nowMs / 24UL) % (uint32_t)travel);
    canvas_.setCursor(x, 125);
    canvas_.print(ticker);

    // Clipped scan segment: width is always positive and inside the sprite.
    const int sx = (int)((nowMs / 9UL) % 280UL) - 40;
    const int x0 = sx < 0 ? 0 : sx;
    const int rawX1 = sx + 40;
    const int x1 = rawX1 > kW ? kW : rawX1;
    if (x1 > x0 && x0 < kW) canvas_.drawFastHLine(x0, 122, x1 - x0, primary);

    canvas_.pushSprite(0, 0);
  }

 private:
  static constexpr int kW = 240;
  static constexpr int kH = 135;
  M5Canvas canvas_;
  bool ready_ = false;
  uint32_t lastFrameUs_ = 0;
  uint32_t worstFrameUs_ = 0;
  float fpsEma_ = 0.0f;

  static uint32_t capU32(uint32_t v, uint32_t hi) { return v > hi ? hi : v; }
  uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) const { return canvas_.color565(r, g, b); }
  uint16_t amber() const { return rgb(255, 154, 25); }
  uint16_t amberDim() const { return rgb(112, 70, 16); }
  uint16_t cyanDim() const { return rgb(20, 92, 98); }

  void columnTitle(int x, int y, const char* s, uint16_t c) {
    canvas_.setTextColor(c, TFT_BLACK);
    canvas_.setCursor(x, y);
    canvas_.print(s);
  }

  void metric(int x, int y, const char* label, bool valid, float v, int decimals,
              const char* suffix, uint16_t c, uint16_t invalid) {
    canvas_.setCursor(x, y);
    canvas_.setTextColor(valid ? c : invalid, TFT_BLACK);
    canvas_.print(label);
    canvas_.print(' ');
    if (!valid || !isfinite(v)) {
      canvas_.print("----");
      return;
    }
    canvas_.print(v, decimals);
    canvas_.print(suffix);
  }

  void value(int x, int y, const char* label, float v, int decimals, uint16_t c) {
    canvas_.setTextColor(c, TFT_BLACK);
    canvas_.setCursor(x, y);
    canvas_.print(label);
    canvas_.print(' ');
    canvas_.print(v, decimals);
  }

  void chip(int x, int y, const char* label, bool on, uint16_t onColor, uint16_t offColor) {
    canvas_.drawRect(x, y - 1, 16, 9, on ? onColor : offColor);
    canvas_.setTextColor(on ? onColor : offColor, TFT_BLACK);
    canvas_.setCursor(x + 5, y);
    canvas_.print(label);
  }
};

}  // namespace janus_adv_elite
