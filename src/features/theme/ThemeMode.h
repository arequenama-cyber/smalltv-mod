#pragma once
#include "config.h"
#if WITH_THEME
#include "Mode.h"

class ThemeMode : public DisplayMode {
 public:
  const char* id() const override { return "theme"; }
  uint8_t modeConst() const override { return MODE_THEME; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings& s) override;

  const String& error() const { return error_; }

  // Kept for compatibility with ThemeWeb.cpp. This personalized build does not
  // keep a parsed .stheme package in heap, so release() is intentionally a no-op.
  static constexpr uint32_t ParseFreeHeap = 6144;
  static constexpr uint32_t ParseLargestBlock = 1536;
  static constexpr uint32_t RequestFreeHeap = 8192;
  static constexpr uint32_t RequestLargestBlock = 3072;
  void release(uint32_t needFree = ParseFreeHeap,
               uint32_t needLargestBlock = ParseLargestBlock) {
    (void)needFree; (void)needLargestBlock;
  }
  void freeForRequest() override {}

 private:
  enum Page : uint8_t { PAGE_GLUCOSE = 0, PAGE_WEATHER = 1, PAGE_CHRISTMAS = 2 };

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
  int8_t trendArrow() const; // -1 down, 0 flat/right, +1 up
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
