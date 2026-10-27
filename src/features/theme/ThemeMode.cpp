#include "ThemeMode.h"
#if WITH_THEME

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

#include "Platform.h"
#include "Gfx.h"

ThemeMode g_themeMode;

namespace {
constexpr uint32_t GLUCOSE_REFRESH_MS = 30000UL;
constexpr uint32_t WEATHER_REFRESH_MS = 60000UL;
constexpr uint32_t PAGE_DWELL_MS = 10000UL;

const char* NIGHTSCOUT_URL =
  "https://elisa-nightscout.onrender.com/api/v2/properties/bgnow,delta,direction";

const char* WEATHER_URL =
  "https://api.open-meteo.com/v1/forecast"
  "?latitude=41.26823&longitude=2.01890"
  "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
  "precipitation,wind_speed_10m"
  "&temperature_unit=celsius&wind_speed_unit=kmh"
  "&timezone=Europe%2FMadrid";

bool getJson(const char* url, JsonDocument& doc, String& error) {
  if (WiFi.status() != WL_CONNECTED) {
    error = "WiFi offline";
    return false;
  }

  SecureClient client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(7000);
#if defined(SMALLTV_FAMILY_ESP32)
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
  if (!http.begin(client, url)) {
    error = "connect failed";
    return false;
  }

  const int code = http.GET();
  if (code < 200 || code >= 300) {
    error = "HTTP " + String(code);
    http.end();
    return false;
  }

  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) {
    error = "JSON parse failed";
    return false;
  }
  error = "";
  return true;
}

void printAt(Arduino_GFX* gfx, const char* text, int x, int y,
             uint8_t size, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void centered(Arduino_GFX* gfx, const char* text, int y,
              uint8_t size, uint16_t color) {
  (void)gfx;
  gfxDrawCentered(text, y, size, color);
}

bool localNow(struct tm& t) {
  if (!platformTimeValid()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return true;
}
} // namespace

void ThemeMode::begin(const Settings&) {
  page_ = PAGE_GLUCOSE;
  pageSinceMs_ = millis();
  nextGlucoseMs_ = millis();
  nextWeatherMs_ = millis();
  lastClockMinute_ = 0xFFFFFFFF;
  needRender_ = true;
  error_ = "";
}

void ThemeMode::invalidate(const Settings&) {
  nextGlucoseMs_ = millis();
  nextWeatherMs_ = millis();
  needRender_ = true;
}

void ThemeMode::wake(const Settings&) {
  needRender_ = true;
}

void ThemeMode::service(const Settings&) {
  const uint32_t now = millis();

  if ((int32_t)(now - nextGlucoseMs_) >= 0) {
    nextGlucoseMs_ = now + GLUCOSE_REFRESH_MS;
    fetchGlucose();
  }
  if ((int32_t)(now - nextWeatherMs_) >= 0) {
    nextWeatherMs_ = now + WEATHER_REFRESH_MS;
    fetchWeather();
  }

  if (now - pageSinceMs_ >= PAGE_DWELL_MS) {
    pageSinceMs_ = now;
    page_ = static_cast<Page>((static_cast<uint8_t>(page_) + 1) % 3);
    needRender_ = true;
  }

  if (platformTimeValid()) {
    const uint32_t minute = (uint32_t)(time(nullptr) / 60);
    if (minute != lastClockMinute_) {
      lastClockMinute_ = minute;
      needRender_ = true;
    }
  }

  if (needRender_) {
    render();
    needRender_ = false;
  }
}

void ThemeMode::fetchGlucose() {
  JsonDocument doc;
  String err;
  if (!getJson(NIGHTSCOUT_URL, doc, err)) {
    glucose_.error = true;
    error_ = "Nightscout: " + err;
    if (page_ == PAGE_GLUCOSE) needRender_ = true;
    return;
  }

  JsonVariantConst v = doc["bgnow"]["last"];
  if (v.isNull()) {
    glucose_.error = true;
    error_ = "Nightscout: bgnow.last missing";
    if (page_ == PAGE_GLUCOSE) needRender_ = true;
    return;
  }

  glucose_.value = v.as<int>();
  glucose_.delta = doc["delta"]["mgdl"] | 0;

  String dtext = doc["delta"]["display"] | "";
  if (!dtext.length()) dtext = String(glucose_.delta >= 0 ? "+" : "") + String(glucose_.delta);
  strlcpy(glucose_.deltaText, dtext.c_str(), sizeof(glucose_.deltaText));

  String direction = doc["direction"]["value"] | "Flat";
  strlcpy(glucose_.direction, direction.c_str(), sizeof(glucose_.direction));

  glucose_.valid = true;
  glucose_.error = false;
  glucose_.lastOkMs = millis();
  error_ = "";
  if (page_ == PAGE_GLUCOSE) needRender_ = true;
}

void ThemeMode::fetchWeather() {
  JsonDocument doc;
  String err;
  if (!getJson(WEATHER_URL, doc, err)) {
    weather_.error = true;
    error_ = "Weather: " + err;
    if (page_ == PAGE_WEATHER) needRender_ = true;
    return;
  }

  JsonObjectConst c = doc["current"].as<JsonObjectConst>();
  if (c.isNull()) {
    weather_.error = true;
    error_ = "Weather: current missing";
    if (page_ == PAGE_WEATHER) needRender_ = true;
    return;
  }

  weather_.temp = c["temperature_2m"] | NAN;
  weather_.feels = c["apparent_temperature"] | NAN;
  weather_.humidity = c["relative_humidity_2m"] | -1;
  weather_.rain = c["precipitation"] | NAN;
  weather_.wind = c["wind_speed_10m"] | NAN;

  weather_.valid = !isnan(weather_.temp);
  weather_.error = !weather_.valid;
  weather_.lastOkMs = weather_.valid ? millis() : weather_.lastOkMs;
  error_ = weather_.valid ? "" : "Weather: invalid values";
  if (page_ == PAGE_WEATHER) needRender_ = true;
}

uint16_t ThemeMode::glucoseColor(int v) const {
  // Preserve the visual ranges used by the current working .stheme.
  if (v < 70) return C_RED;
  if (v < 79) return C_BLUE;
  if (v <= 140) return C_GREEN;
  if (v <= 180) return C_YELLOW;
  return C_RED;
}

int8_t ThemeMode::trendArrow() const {
  const String d(glucose_.direction);
  if (d == "DoubleUp" || d == "SingleUp" || d == "FortyFiveUp") return +1;
  if (d == "DoubleDown" || d == "SingleDown" || d == "FortyFiveDown") return -1;
  return 0; // Flat / NONE / unknown => right arrow
}

void ThemeMode::drawArrow(int8_t dir, int cx, int cy, uint16_t color) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;

  // Three discrete arrows only: up / right / down.
  // The shaft is a filled rectangle and the head a filled triangle, so no
  // overdraw can erase the body and the result stays solid on the 240x240 panel.
  const int shaftLen = 24;
  const int shaftTh  = 5;
  const int headLen  = 10;
  const int headHalf = 9;

  if (dir > 0) { // up
    const int tipY = cy - shaftLen / 2 - headLen;
    const int baseY = tipY + headLen;
    gfx->fillRect(cx - shaftTh / 2, baseY, shaftTh, shaftLen, color);
    gfx->fillTriangle(cx, tipY,
                      cx - headHalf, baseY,
                      cx + headHalf, baseY, color);
  } else if (dir < 0) { // down
    const int tipY = cy + shaftLen / 2 + headLen;
    const int baseY = tipY - headLen;
    gfx->fillRect(cx - shaftTh / 2, cy - shaftLen / 2, shaftTh, shaftLen, color);
    gfx->fillTriangle(cx, tipY,
                      cx - headHalf, baseY,
                      cx + headHalf, baseY, color);
  } else { // right
    const int tipX = cx + shaftLen / 2 + headLen;
    const int baseX = tipX - headLen;
    gfx->fillRect(cx - shaftLen / 2, cy - shaftTh / 2, shaftLen, shaftTh, color);
    gfx->fillTriangle(tipX, cy,
                      baseX, cy - headHalf,
                      baseX, cy + headHalf, color);
  }
}

void ThemeMode::render() {
  switch (page_) {
    case PAGE_GLUCOSE:   renderGlucose(); break;
    case PAGE_WEATHER:   renderWeather(); break;
    case PAGE_CHRISTMAS: renderChristmas(); break;
  }
}

void ThemeMode::renderGlucose() {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  centered(gfx, "GLUCOSA", 7, 2, C_GRAY);

  if (!glucose_.valid) {
    centered(gfx, "--", 64, 6, C_WHITE);
    centered(gfx, glucose_.error ? "sense dades" : "carregant...", 142, 2, C_GRAY);
    return;
  }

  char value[8];
  snprintf(value, sizeof(value), "%d", glucose_.value);
  const uint16_t vc = glucoseColor(glucose_.value);
  centered(gfx, value, 42, gfxFitSize(value, 230, 7), vc);
  centered(gfx, "mg/dL", 113, 2, C_GRAY);

  printAt(gfx, glucose_.deltaText, 34, 156, 3, C_WHITE);
  drawArrow(trendArrow(), 174, 170, C_WHITE);

  struct tm t{};
  if (localNow(t)) {
    char wd[8], hm[8];
    strftime(wd, sizeof(wd), "%a", &t);   // Tue, Wed...
    strftime(hm, sizeof(hm), "%H:%M", &t);
    printAt(gfx, wd, 44, 216, 2, C_GRAY);
    printAt(gfx, hm, 145, 216, 2, C_WHITE);
  }

  if (glucose_.error) gfx->fillCircle(6, 6, 3, C_RED);
}

void ThemeMode::renderWeather() {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  centered(gfx, "GAVA MAR", 8, 2, C_GRAY);

  if (!weather_.valid) {
    centered(gfx, "--", 64, 6, C_WHITE);
    centered(gfx, weather_.error ? "sense dades" : "carregant...", 142, 2, C_GRAY);
    return;
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f C", weather_.temp);
  centered(gfx, buf, 52, gfxFitSize(buf, 232, 6), C_WHITE);

  snprintf(buf, sizeof(buf), "Sens %.1f C", weather_.feels);
  centered(gfx, buf, 118, 2, C_GRAY);

  snprintf(buf, sizeof(buf), "Hum %d%%", weather_.humidity);
  printAt(gfx, buf, 16, 154, 2, C_WHITE);

  snprintf(buf, sizeof(buf), "Vent %.0f", weather_.wind);
  printAt(gfx, buf, 136, 154, 2, C_WHITE);

  snprintf(buf, sizeof(buf), "Pluja %.1f mm", weather_.rain);
  centered(gfx, buf, 184, 2, C_GRAY);

  struct tm t{};
  if (localNow(t)) {
    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &t);
    centered(gfx, hm, 216, 2, C_DGRAY);
  }

  if (weather_.error) gfx->fillCircle(6, 6, 3, C_RED);
}

void ThemeMode::renderChristmas() {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  centered(gfx, "NADAL", 14, 3, C_WHITE);

  struct tm nowTm{};
  if (!localNow(nowTm)) {
    centered(gfx, "--", 75, 6, C_WHITE);
    centered(gfx, "esperant hora", 158, 2, C_GRAY);
    return;
  }

  time_t now = time(nullptr);
  struct tm target = nowTm;
  target.tm_mon = 11;   // December
  target.tm_mday = 25;
  target.tm_hour = 0;
  target.tm_min = 0;
  target.tm_sec = 0;
  target.tm_isdst = -1;

  time_t christmas = mktime(&target);
  if (now >= christmas + 24 * 3600) {
    target.tm_year += 1;
    christmas = mktime(&target);
  }

  long days = (long)ceil(difftime(christmas, now) / 86400.0);
  if (days < 0) days = 0;

  char n[12];
  snprintf(n, sizeof(n), "%ld", days);
  centered(gfx, n, 72, gfxFitSize(n, 220, 7), C_WHITE);
  centered(gfx, days == 1 ? "dia" : "dies", 145, 3, C_GRAY);
  centered(gfx, "25 / 12", 198, 2, C_DGRAY);
}

#endif
