// The "weather face" - shows a small icon for the current condition, the
// current temperature, humidity, and wind speed, pulled from
// OpenWeatherMap. Like the date face, this is drawn entirely with GFX
// primitives rather than a BMP background - see
// DATE_FACE_NUMBER/WEATHER_FACE_NUMBER in defines.h,
// setFaceForCurrentSlot()/nextClockFace() in bmp.ino, and updateClock() in
// clock.ino.
//
// Uses one free, no-subscription OpenWeatherMap endpoint:
//  - /data/2.5/weather - current conditions (temp, humidity, condition, wind)
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

struct WeatherData {
  float tempC;
  int humidityPct;   // -1 if unavailable
  int pressureHpa;   // -1 if unavailable
  float windMph;      // NAN if unavailable
  char condition[16];
  char location[17]; // town/city name, per OWM's own geocoding - may be
                      // truncated for long names, see WEATHER_LOCATION_BASELINE_Y
  bool valid;         // true once we've successfully fetched at least once
};

WeatherData currentWeather = { 0, -1, -1, NAN, "", "", false };
unsigned long lastWeatherFetchMillis = 0;

boolean weatherConfigured() {
  return owmApiKey[0] != '\0' && owmLat[0] != '\0' && owmLon[0] != '\0';
}

// --- Weather icon ---------------------------------------------------------
//
// Drawn procedurally with the same GFX primitives the clock hands/ticks
// already use (fillCircle, writeFatLine, etc.) rather than as bitmap
// assets - keeps this face code-only, no extra LittleFS icon files to
// manage or flash.
//
// WeatherIconType itself lives in defines.h, not here - the Arduino build
// auto-generates a forward declaration for weatherIconTypeFor() and inserts
// it near the very top of the concatenated sketch (before this enum
// definition would otherwise appear), so the type needs to already be
// visible from an early #include rather than defined further down this tab.

// --- Layout ----------------------------------------------------------------
//
// Five rows (location / icon / temp / humidity / wind), evenly spaced and
// vertically centered in the circular screen, the same way the date face's
// rows are - see the comment there for the general approach. Baselines are
// for GFX custom fonts, which anchor text at the baseline, not the top.
//
//   row              font/size  height   gap-before
//   location          9pt        ~13px    -
//   icon (drawn, not text)       ~40px    14px
//   temp              24pt       ~35px    14px
//   humidity + pressure 12pt     ~18px    14px
//   wind               9pt       ~13px    14px
//
// Total stack height 13+40+35+18+13 + 4*14 = 175px, centered on the 240px
// circle (clock_center_y=120) gives a top edge at y=33 and bottom edge at
// y=208.
const int WEATHER_LOCATION_BASELINE_Y = 46;
const int ICON_CENTER_Y = 80;
const int ICON_RADIUS = 20;
const int WEATHER_TEMP_BASELINE_Y = 149;
const int WEATHER_STATS_BASELINE_Y = 181; // combined humidity + pressure row
const int WEATHER_WIND_BASELINE_Y = 208;

// Maps an OpenWeatherMap "weather[0].main" category onto one of our simple
// icon shapes. Anything not explicitly listed (Mist/Smoke/Haze/Dust/Fog/
// Sand/Ash/Squall/Tornado, or an unrecognised value) falls back to the fog
// icon - they're rare, and a generic "hazy" glyph is a reasonable stand-in.
WeatherIconType weatherIconTypeFor(const char *condition) {
  if (strcmp(condition, "Clear") == 0) return ICON_SUN;
  if (strcmp(condition, "Clouds") == 0) return ICON_CLOUD;
  if (strcmp(condition, "Rain") == 0 || strcmp(condition, "Drizzle") == 0) return ICON_RAIN;
  if (strcmp(condition, "Thunderstorm") == 0) return ICON_THUNDER;
  if (strcmp(condition, "Snow") == 0) return ICON_SNOW;
  return ICON_FOG;
}

void drawSunIcon(int cx, int cy, int r, uint16_t color) {
  tft.fillCircle(cx, cy, r * 0.55, color);
  for (int angleDeg = 0; angleDeg < 360; angleDeg += 45) {
    float angle = angleDeg * pi / 180.0;
    int x1 = cx + (int)(r * 0.65 * cos(angle));
    int y1 = cy + (int)(r * 0.65 * sin(angle));
    int x2 = cx + (int)(r * cos(angle));
    int y2 = cy + (int)(r * sin(angle));
    tft.writeFatLine(x1, y1, x2, y2, color, 2);
  }
}

// Shared cloud shape (three overlapping puffs + a flat base) used by the
// cloud, rain, thunder and snow icons.
void drawCloudShape(int cx, int cy, int r, uint16_t color) {
  tft.fillCircle(cx - r * 0.5, cy + r * 0.1, r * 0.45, color);
  tft.fillCircle(cx + r * 0.5, cy + r * 0.1, r * 0.45, color);
  tft.fillCircle(cx, cy - r * 0.15, r * 0.55, color);
  tft.fillRect(cx - r * 0.75, cy + r * 0.05, r * 1.5, r * 0.45, color);
}

void drawRainIcon(int cx, int cy, int r, uint16_t cloudColor, uint16_t dropColor) {
  drawCloudShape(cx, cy - r * 0.2, r * 0.85, cloudColor);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * r * 0.4;
    int y1 = cy + r * 0.35;
    int y2 = y1 + r * 0.4;
    tft.writeFatLine(x, y1, x - 2, y2, dropColor, 2);
  }
}

void drawThunderIcon(int cx, int cy, int r, uint16_t cloudColor, uint16_t boltColor) {
  drawCloudShape(cx, cy - r * 0.2, r * 0.85, cloudColor);
  // A simple zigzag bolt beneath the cloud.
  int x0 = cx + 2, y0 = cy + r * 0.3;
  int x1 = cx - 4, y1 = cy + r * 0.55;
  int x2 = cx + 4, y2 = cy + r * 0.55;
  int x3 = cx - 2, y3 = cy + r * 0.85;
  tft.writeFatLine(x0, y0, x1, y1, boltColor, 2);
  tft.writeFatLine(x1, y1, x2, y2, boltColor, 2);
  tft.writeFatLine(x2, y2, x3, y3, boltColor, 2);
}

void drawSnowIcon(int cx, int cy, int r, uint16_t cloudColor, uint16_t flakeColor) {
  drawCloudShape(cx, cy - r * 0.2, r * 0.85, cloudColor);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * r * 0.4;
    int y = cy + r * 0.55;
    tft.writeFatLine(x - 3, y, x + 3, y, flakeColor, 1);
    tft.writeFatLine(x, y - 3, x, y + 3, flakeColor, 1);
  }
}

// Three short horizontal bars, standing in for mist/fog/haze/etc.
void drawFogIcon(int cx, int cy, int r, uint16_t color) {
  for (int i = -1; i <= 1; i++) {
    int y = cy + i * r * 0.35;
    int halfWidth = r * (i == 0 ? 0.9 : 0.7);
    tft.writeFatLine(cx - halfWidth, y, cx + halfWidth, y, color, 2);
  }
}

void drawWeatherIcon(const char *condition) {
  int cx = clock_center_x;
  int cy = ICON_CENTER_Y;
  int r = ICON_RADIUS;

  switch (weatherIconTypeFor(condition)) {
    case ICON_SUN:
      drawSunIcon(cx, cy, r, GC9A01A_YELLOW);
      break;
    case ICON_CLOUD:
      drawCloudShape(cx, cy, r, GC9A01A_LIGHTGREY);
      break;
    case ICON_RAIN:
      drawRainIcon(cx, cy, r, GC9A01A_LIGHTGREY, GC9A01A_BLUE);
      break;
    case ICON_THUNDER:
      drawThunderIcon(cx, cy, r, GC9A01A_LIGHTGREY, GC9A01A_YELLOW);
      break;
    case ICON_SNOW:
      drawSnowIcon(cx, cy, r, GC9A01A_LIGHTGREY, GC9A01A_WHITE);
      break;
    case ICON_FOG:
    default:
      drawFogIcon(cx, cy, r, GC9A01A_LIGHTGREY);
      break;
  }
}

// Fetches current conditions and updates currentWeather. Returns false
// (leaving any previous currentWeather in place) if WiFi, the HTTP request,
// or the JSON parsing fail.
boolean fetchWeather() {
  // Always stamp the attempt time, success or fail, so a persistently
  // failing fetch is retried at most once per WEATHER_REFRESH_INTERVAL_MS
  // rather than being hammered every minute.
  lastWeatherFetchMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("fetchWeather: WiFi not connected");
    return false;
  }

  char url[160];
  snprintf(url, sizeof(url), OWM_CURRENT_URL_FMT, owmLat, owmLon, owmApiKey);

  HTTPClient http;
  http.begin(url);
  // Explicit timeouts - without these, a stalled DNS lookup or connection
  // (flaky WiFi, DNS hiccup) can block this call indefinitely, which would
  // freeze the whole loop() - including face rotation - since everything
  // here runs synchronously on one thread.
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("fetchWeather: request failed, HTTP %d\n", httpCode);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("fetchWeather: JSON parse failed: %s\n", err.c_str());
    return false;
  }

  float tempC = doc["main"]["temp"] | NAN;
  int humidityPct = doc["main"]["humidity"] | -1;
  int pressureHpa = doc["main"]["pressure"] | -1;
  float windMs = doc["wind"]["speed"] | NAN;
  const char *conditionStr = doc["weather"][0]["main"] | "";
  const char *locationStr = doc["name"] | "";

  if (isnan(tempC)) {
    Serial.println("fetchWeather: response missing temp");
    return false;
  }

  currentWeather.tempC = tempC;
  currentWeather.humidityPct = humidityPct;
  currentWeather.pressureHpa = pressureHpa;
  currentWeather.windMph = isnan(windMs) ? NAN : windMs * 2.23694f;
  strncpy(currentWeather.condition, conditionStr, sizeof(currentWeather.condition));
  currentWeather.condition[sizeof(currentWeather.condition) - 1] = '\0';
  // Long place names get truncated to fit the row's width - OWM's own
  // geocoding, so this is whatever name they associate with the coordinates.
  strncpy(currentWeather.location, locationStr, sizeof(currentWeather.location));
  currentWeather.location[sizeof(currentWeather.location) - 1] = '\0';
  currentWeather.valid = true;

  return true;
}

void drawWeatherFace() {
  if (!weatherConfigured()) {
    tft.fillScreen(GC9A01A_BLACK);
    drawCenteredText("Weather not", WEEKDAY_BASELINE_Y, &FreeSansBold12pt7b, GC9A01A_LIGHTGREY);
    drawCenteredText("configured", DAY_BASELINE_Y - 10, &FreeSansBold12pt7b, GC9A01A_LIGHTGREY);
    drawCenteredText("Configure at:", 146, &FreeSansBold9pt7b, GC9A01A_LIGHTGREY);

    char ipStr[24];
    // Same pattern the boot-screen IP display uses (Analog_Clock-esp32c3.ino)
    // - if WiFi setup never succeeded, we're serving the config page from
    // the fallback access point, not the home network.
    String ipAddress = inApFallback ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    snprintf(ipStr, sizeof(ipStr), "%s", ipAddress.c_str());
    drawCenteredText(ipStr, TIME_BASELINE_Y, &FreeSansBold9pt7b, GC9A01A_LIGHTGREY);
    return;
  }

  // Reuse the cached reading if it's still fresh rather than hitting the
  // API every time the rotation lands on this face - with a 3-slot rotation
  // this face could otherwise get re-fetched every ROTATION_INTERVAL_SECONDS.
  // Actual fetches stay throttled to WEATHER_REFRESH_INTERVAL_MS regardless
  // of how often we're asked to draw.
  boolean needsFetch = !currentWeather.valid ||
                        (millis() - lastWeatherFetchMillis >= WEATHER_REFRESH_INTERVAL_MS);
  boolean ok = needsFetch ? fetchWeather() : true;

  tft.fillScreen(GC9A01A_BLACK);

  if (!ok && !currentWeather.valid) {
    // Never successfully fetched - nothing sensible to show yet.
    drawCenteredText("Weather", WEEKDAY_BASELINE_Y, &FreeSansBold12pt7b, GC9A01A_LIGHTGREY);
    drawCenteredText("unavailable", DAY_BASELINE_Y - 10, &FreeSansBold12pt7b, GC9A01A_LIGHTGREY);
    return;
  }

  // If this fetch failed but we have a previous successful reading, keep
  // showing that stale-but-plausible data rather than an error page.
  if (currentWeather.location[0] != '\0') {
    drawCenteredText(currentWeather.location, WEATHER_LOCATION_BASELINE_Y, &FreeSansBold9pt7b, GC9A01A_LIGHTGREY);
  }

  drawWeatherIcon(currentWeather.condition);

  char tempStr[8];
  snprintf(tempStr, sizeof(tempStr), "%.0fC", currentWeather.tempC);
  drawCenteredText(tempStr, WEATHER_TEMP_BASELINE_Y, &FreeSansBold24pt7b, GC9A01A_WHITE);

  char statsStr[24];
  if (currentWeather.humidityPct >= 0 && currentWeather.pressureHpa >= 0) {
    snprintf(statsStr, sizeof(statsStr), "%d%%  %dhPa", currentWeather.humidityPct, currentWeather.pressureHpa);
  } else if (currentWeather.humidityPct >= 0) {
    snprintf(statsStr, sizeof(statsStr), "%d%%", currentWeather.humidityPct);
  } else if (currentWeather.pressureHpa >= 0) {
    snprintf(statsStr, sizeof(statsStr), "%dhPa", currentWeather.pressureHpa);
  } else {
    snprintf(statsStr, sizeof(statsStr), "--");
  }
  drawCenteredText(statsStr, WEATHER_STATS_BASELINE_Y, &FreeSansBold12pt7b, GC9A01A_BLUE);

  char windStr[16];
  if (isnan(currentWeather.windMph)) {
    snprintf(windStr, sizeof(windStr), "Wind: --");
  } else {
    snprintf(windStr, sizeof(windStr), "Wind: %.0f mph", currentWeather.windMph);
  }
  drawCenteredText(windStr, WEATHER_WIND_BASELINE_Y, &FreeSansBold9pt7b, GC9A01A_LIGHTGREY);
}

// Called once a minute while the weather face is showing. Only actually
// re-fetches/redraws once WEATHER_REFRESH_INTERVAL_MS has passed since the
// last attempt - otherwise a no-op, so the display just sits still the rest
// of the time.
void updateWeatherFace() {
  if (!weatherConfigured()) return; // static "not configured" message stays as-is

  if (millis() - lastWeatherFetchMillis < WEATHER_REFRESH_INTERVAL_MS) return;

  drawWeatherFace();
}
