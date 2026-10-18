#include "settings.h"
#include <Preferences.h>
#include "board.h"
#include <math.h>

namespace {
Settings current = {true, true, 10, SCREEN_TIMEOUT_S, true, true,
                    false, 3.0f, 2.0f, 25, 10, 20, 10, 75, 90, 85, 95, NAN, NAN, 3.0f, 5.0f, "", 0, 3, false};
}

void settings_begin() {
  Preferences prefs;
  prefs.begin("bootmon", true);
  // Ook een kortere (oudere) versie inlezen; de rest houdt de standaardwaarden
  size_t len = prefs.getBytesLength("settings");
  if (len >= 6 && len <= sizeof(Settings)) {
    prefs.getBytes("settings", &current, len);
  } else if (len > sizeof(Settings)) {
    // Nieuwere versie (na een terugval van een update): het bekende begin overnemen
    uint8_t *raw = (uint8_t *)malloc(len);
    if (raw && prefs.getBytes("settings", raw, len) == len) memcpy(&current, raw, sizeof(Settings));
    free(raw);
  }
  prefs.end();
}

const Settings &settings() { return current; }

void settings_save(const Settings &s) {
  current = s;
  Preferences prefs;
  prefs.begin("bootmon", false);
  prefs.putBytes("settings", &current, sizeof(Settings));
  prefs.end();
}
