#include "ThemeWeb.h"

#if WITH_THEME

#include "ThemeMode.h"

void themeWebBegin(WebServerClass& server, Settings& settings, bool (*requireAuth)()) {
  (void)settings;
  (void)requireAuth;

#if defined(SMALLTV_ESP32_PRO)

  server.on("/api/theme-bridge", HTTP_GET, [&server]() {
    String body = g_themeMode.bridgeJson();

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
  });

#endif
}

#endif
