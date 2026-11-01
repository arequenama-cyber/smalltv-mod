#pragma once
#include "config.h"

#if WITH_THEME

#include "Mode.h"
#include <math.h>

class ThemeMode : public DisplayMode {
 public:
  const char* id() const override { return "theme"; }
  uint8_t modeConst() const override { return MODE_THEME; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings& s) override;

  const String& error() const { return error_; }

  // JSON petit que el PRO ofereix a l'ULTRA per HTTP local.
  String bridgeJson() const {
    String out;
    out.reserve(240);

    out = F("{\"g\":");
    out += glucose_.value;

    out += F(",\"d\":\"");
    out += glucose_.deltaText;
    out += '"';

    out += F(",\"dir\":\"");
    out += glucose_.direction;
    out += '"';

    out += F(",\"gv\":");
    out += glucose_.valid ? F("true") : F("false");

    out += F(",\"t\":");
    if (isnan(weather_.temp)) out += F("null");
    else out += String(weather_.temp, 1);

    out += F(",\"f\":");
    if (isnan(weather_.feels)) out += F("null");
    else out += String(weather_.feels, 1);

    out += F(",\"h\":");
    out += weather_.humidity;

    out += F(",\"w\":");
    if (isnan(weather_.wind)) out += F("null");
    else out += String(weather_.wind, 1);

    out += F(",\"r\":");
    if (isnan(weather_.rain)) out += F("null");
    else out += String(weather_.rain, 1);

    out += F(",\"wv\":");
    out += weather_.valid ? F("true") : F("false");

    out += '}';

    return out;
  }

  static constexpr uint32_t ParseFreeHeap = 6144;
  static constexpr uint32_t ParseLargestBlock = 1536;
  static constexpr uint32_t RequestFreeHeap = 8192;
  static constexpr uint32_t RequestLargestBlock = 3072;

  void release(uint32_t needFree = ParseFreeHeap,
               uint32_t needLargestBlock = ParseLargestBlock) {
    (void)needFree;
    (void)needLargestBlock;
  }

  void freeForRequest() override {}

 private:
  enum Page : uint8_t {
    PAGE_GLUCOSE = 0,
    PAGE_WEATHER = 1,
    PAGE_CHRISTMAS = 2
  };

  struct GlucoseData {
    int value = 0;
    int delta = 0;
    char deltaText[12] = "--";
    char direction[20] = "Flat";
    bool valid = false;
    bool error = false;
    uint32_t lastOkMs = 0;
  };

  struct WeatherData {
    float temp = NAN;
    float feels = NAN;
    int humidity = -1;
    float rain = NAN;
    float wind = NAN;
    bool valid = false;
    bool error = false;
    uint32_t lastOkMs = 0;
  };

  void fetchGlucose();
  void fetchWeather();

  void render();
  void renderGlucose();
  void renderWeather();
  void renderChristmas();

  uint16_t glucoseColor(int value) const;
  int8_t trendArrow() const;
  void drawArrow(int8_t dir, int cx, int cy, uint16_t color);

  Page page_ = PAGE_GLUCOSE;

  uint32_t pageSinceMs_ = 0;
  uint32_t nextGlucoseMs_ = 0;
  uint32_t nextWeatherMs_ = 0;
  uint32_t lastClockMinute_ = 0xFFFFFFFF;

  bool needRender_ = true;

  GlucoseData glucose_;
  WeatherData weather_;

  String error_;
};

extern ThemeMode g_themeMode;

#endif
