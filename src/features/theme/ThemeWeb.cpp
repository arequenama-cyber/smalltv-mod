#include "ThemeWeb.h"

#if WITH_THEME

// Personalized SmallTV build:
// Theme mode has been repurposed as the Nightscout / Gava Mar weather /
// Christmas carousel, so the original .stheme web-management API is disabled.
//
// Keeping this function as a no-op preserves the interface expected by the
// rest of the firmware without pulling in ThemeFile/LittleFS package handling.
void themeWebBegin(WebServerClass& server, Settings& settings, bool (*requireAuth)()) {
  (void)server;
  (void)settings;
  (void)requireAuth;
}

#endif