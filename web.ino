void handleRoot() {
  if (server.method() == HTTP_POST) {
    tft.fillScreen(GC9A01A_BLACK);
    for (uint16_t degrees = 0; degrees < 360; degrees += 10) {
      float angle = 2.0 * pi / 360 * degrees;
      int x = clock_center_x + (SCREEN_DIAMETER / 2 - 50) * sin(angle);
      int y = clock_center_y - (SCREEN_DIAMETER / 2 - 50) * cos(angle);
      tft.fillCircle(x, y, 10, GC9A01A_YELLOW);
      delay(50);
    }
    delay(500);

    snprintf(ssid, sizeof(ssid), "%s", server.arg("ssid").c_str());
    snprintf(wifiPassword, sizeof(wifiPassword), "%s", server.arg("password").c_str());
    snprintf(owmApiKey, sizeof(owmApiKey), "%s", server.arg("owmApiKey").c_str());
    snprintf(owmLat, sizeof(owmLat), "%s", server.arg("owmLat").c_str());
    snprintf(owmLon, sizeof(owmLon), "%s", server.arg("owmLon").c_str());

    // Store values in EEProm. Always write the password (even if blank) -
    // an open network genuinely has no password, and skipping the write
    // here used to leave stale/garbage EEPROM bytes behind instead of an
    // empty string, which broke connecting to open APs.
    EEPROM.put(SSID_ADDR, ssid);
    EEPROM.put(WIFI_PASSWORD_ADDR, wifiPassword);
    EEPROM.put(OWM_API_KEY_ADDR, owmApiKey);
    EEPROM.put(OWM_LAT_ADDR, owmLat);
    EEPROM.put(OWM_LON_ADDR, owmLon);
    EEPROM.commit();

    delay(500);
    ESP.restart();
  }

  // Read back to check if the values are stored correctly
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(WIFI_PASSWORD_ADDR, wifiPassword);
  EEPROM.get(OWM_API_KEY_ADDR, owmApiKey);
  EEPROM.get(OWM_LAT_ADDR, owmLat);
  EEPROM.get(OWM_LON_ADDR, owmLon);

  // Same freshly-erased-flash handling as setup() - these get printed
  // straight into the HTML below, so a stray 0xFF byte would otherwise
  // show up as a literal garbage character in the form fields.
  owmApiKey[sizeof(owmApiKey) - 1] = '\0';
  owmLat[sizeof(owmLat) - 1] = '\0';
  owmLon[sizeof(owmLon) - 1] = '\0';
  if ((uint8_t)owmApiKey[0] == 0xFF) owmApiKey[0] = '\0';
  if ((uint8_t)owmLat[0] == 0xFF) owmLat[0] = '\0';
  if ((uint8_t)owmLon[0] == 0xFF) owmLon[0] = '\0';

  String rootForm = String(CSS) + "<html>\
  <body>\
    <div class=\"container\">\
      <h1>Settings</h1>\
      <form method=\"POST\" action=\"/\">\
        SSID:</br>\
        <input type=\"text\" name=\"ssid\" value=\""
                    + String(ssid) + "\"></br></br>\
        Password:</br>\
        <input type=\"text\" name=\"password\" value=\""
                    + String(wifiPassword) + "\"></br></br>\
        OpenWeatherMap API key:</br>\
        <input type=\"text\" name=\"owmApiKey\" value=\""
                    + String(owmApiKey) + "\"></br></br>\
        Latitude:</br>\
        <input type=\"text\" name=\"owmLat\" value=\""
                    + String(owmLat) + "\"></br></br>\
        Longitude:</br>\
        <input type=\"text\" name=\"owmLon\" value=\""
                    + String(owmLon) + "\">\
        <input type=\"submit\" value=\"Submit\">\
      </form>\
    </div>\
  </body>\
</html>";

  server.send(200, "text/html", rootForm);
}
