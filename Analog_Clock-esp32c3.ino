#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <time.h>

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "ClockTFT.h"
#include "defines.h"
#include "clock.h"
#include "esp_sntp.h"

#define FORMAT_LITTLEFS_IF_FAILED false

boolean timeIsSet = false;
boolean inApFallback = false;
time_t lastNtpSet = 0;
time_t currentTime = time(nullptr);  // time_t = seconds since epoch
struct tm *timeinfo;
time_t previousEffectTime = time(nullptr);

char ssid[60];
char wifiPassword[60];

ClockTFT tft(TFT_CS, TFT_DC, TFT_RST);

WebServer server(80);

uint8_t currentFaceNumber;
HandPosition hoursHandPositions[15];
HandPosition minutesHandPositions[15];

// Moved here (rather than clock.ino) so that bmp.ino, which is concatenated
// before clock.ino by the Arduino build, can also see these.
int hour = -1;
int minute = -1;
int splittedSecond = -1;

// Record the NPT set time
void timeUpdated(struct timeval *tv) {
  timeIsSet = true;
  lastNtpSet = time(nullptr);
  Serial.printf("NTP Updated: %s\n", ctime(&lastNtpSet));
}

void printFreeRam() {
  Serial.printf("Free ram: %d bytes\n", ESP.getFreeHeap());
}

void listAllFilesInDir(File dir, int indentation) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    // make sure to use indentation
    for (uint8_t i = 0; i < indentation; i++) {
      Serial.print("  ");
    }
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");

      // recursive file listing inside new directory
      listAllFilesInDir(entry, indentation + 1);
    } else {
      // print file size
      Serial.printf(" - %d\n", entry.size());
    }
    entry.close();
  }
}

void setup() {
  Serial.begin(115200);
  // Wait up to 3s for a serial monitor to attach (useful when debugging over
  // USB), but don't block forever - the ESP32-C3's native USB-CDC Serial
  // never becomes "true" if there's no host PC listening, which would hang
  // the board indefinitely (and the WiFi/AP setup below would never run) when
  // powered standalone from a wall adapter or power bank.
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }
  delay(2000);
  Serial.println();

  Serial.printf("TFT_DIN: %d\n", TFT_DIN);
  Serial.printf("TFT_CLK: %d\n", TFT_CLK);
  Serial.printf("TFT_CS : %d\n", TFT_CS);
  Serial.printf("TFT_DC : %d\n", TFT_DC);
  Serial.printf("TFT_RST: %d\n", TFT_RST);
  Serial.printf("TFT_BL : %d\n", TFT_BL);

  Serial.printf("Flash size: %d\n", ESP.getFlashChipSize());
  printFreeRam();

  // For the ESP the flash has to be read to a buffer
  EEPROM.begin(512);

  // BOOT button used to manually cycle clock faces (active LOW, internal pull-up)
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Setup the LCD
  // ClockTFT uses the hardware-SPI constructor, which otherwise defaults to
  // the board's default SPI pins. Explicitly remap the SPI bus to our wiring
  // (ESP32-C3's GPIO matrix lets hardware SPI live on any pin) before begin().
  SPI.begin(TFT_CLK, -1, TFT_DIN, TFT_CS); // sck, miso (unused), mosi, cs
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);
  tft.fillCircle(clock_center_x, clock_center_y, SCREEN_DIAMETER / 10, GC9A01A_BLUE);

  int16_t xPos, yPos;
  uint16_t width, height;
  tft.setTextSize(2);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS problem");
    tft.fillScreen(GC9A01A_BLACK);
    String littleFSError = "LittleFS error";
    tft.getTextBounds(littleFSError, 0, 100, &xPos, &yPos, &width, &height);
    tft.setTextColor(GC9A01A_WHITE);
    tft.setCursor(clock_center_x - width / 2, clock_center_y - 50);
    tft.println(littleFSError);
    delay(1000);
  } else {
    // Show some FS info
    Serial.printf("Total space:      %d bytes\n", LittleFS.totalBytes());
    Serial.printf("Total space used: %d bytes\n", LittleFS.usedBytes());
    listAllFilesInDir(LittleFS.open("/"), 0);
  }

  // Setup the wifi
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(WIFI_PASSWORD_ADDR, wifiPassword);

  // On a freshly erased chip, EEPROM (backed by raw flash) reads back as 0xFF
  // bytes with no null terminator anywhere in the buffer. Force-terminate
  // both strings so we never read past the end of the array.
  ssid[sizeof(ssid) - 1] = '\0';
  wifiPassword[sizeof(wifiPassword) - 1] = '\0';

  // On a freshly erased chip (or after the fix above, any device that never
  // had a password explicitly saved) the password bytes read back as 0xFF.
  // Treat that the same as "no password" (open network) rather than as a
  // garbage password that will fail to authenticate.
  if ((uint8_t)wifiPassword[0] == 0xFF) wifiPassword[0] = '\0';

  // Treat a still-erased/garbage SSID as "not configured yet" rather than
  // wasting time on a doomed connection attempt.
  bool credentialsLookValid = (strlen(ssid) > 0) && ((uint8_t)ssid[0] != 0xFF);

  if (!credentialsLookValid) {
    Serial.println("\nNo WiFi credentials stored yet (fresh chip) - skipping straight to AP setup mode.");
    ssid[0] = '\0';
    wifiPassword[0] = '\0';
  } else {
    Serial.printf("\nConnecting to WIFI '%s'... ", ssid);
  }

  WiFi.mode(WIFI_STA);
  // ESP32-C3 "Super Mini" clone boards commonly have an undersized onboard
  // 3.3V regulator that can't reliably supply the radio at default max TX
  // power - this tends to show up as flaky/invisible AP or STA behaviour
  // rather than a clean failure. Capping power avoids the brownout/RF issue.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setSleep(false); // modem sleep power-saving is a common cause of "reason 34" drops on ESP32-C3
  WiFi.hostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  if (credentialsLookValid) WiFi.begin(String(ssid), String(wifiPassword));
  if (!credentialsLookValid || WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Continuing...");
    tft.fillCircle(clock_center_x, clock_center_y, SCREEN_DIAMETER / 10, GC9A01A_RED);

    // Fully drop the STA radio before switching to AP - jumping straight
    // from a failed/pending STA connection into softAP() without settling
    // can cause softAP() to silently fail on some ESP32-C3 boards.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(200);

    // Force a fixed 2.4GHz channel (1) rather than relying on auto-select,
    // which has also been known to cause a silently-failed softAP() start.
    bool apStarted = WiFi.softAP(HOSTNAME, nullptr, 1);
    Serial.printf("WiFi.softAP() reported: %s\n", apStarted ? "success" : "FAILED");
    if (!apStarted) {
      Serial.println("Retrying softAP() once more...");
      delay(500);
      apStarted = WiFi.softAP(HOSTNAME, nullptr, 1);
      Serial.printf("WiFi.softAP() retry reported: %s\n", apStarted ? "success" : "FAILED");
    }

    inApFallback = true;
    Serial.print("AP started. Connect to '");
    Serial.print(HOSTNAME);
    Serial.print("' and browse to ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Connected to WIFI...");

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.print("WiFi Restored! IP address: ");
      Serial.println(WiFi.localIP());
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.printf("Disconnected! Reason: %u\n", info.wifi_sta_disconnected.reason);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    tft.fillCircle(clock_center_x, clock_center_y, SCREEN_DIAMETER / 10, GC9A01A_GREEN);
  }
  // WiFi.localIP() only ever reports the STATION address, so in AP fallback
  // mode this correctly (if confusingly) prints 0.0.0.0 - the real address to
  // use in that case is the AP address printed above (192.168.4.1).
  Serial.print("IP address: ");
  String ipAddress = inApFallback ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  Serial.println(ipAddress);

  tft.getTextBounds(ipAddress, 0, 100, &xPos, &yPos, &width, &height);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setCursor(clock_center_x - width / 2, clock_center_y + 50);
  tft.println(ipAddress);
  delay(1000);

  // implement NTP update of timekeeping (with automatic hourly updates)
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", MY_TZ, 1);
  tzset();

  // callback, when NTP changes the time
  sntp_set_time_sync_notification_cb(timeUpdated);
  sntp_set_sync_interval(600 * 1000);  // update every 10 minutes

  // Setup the web server
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.begin();

  // Initialize face
  randomClockFace();

  Serial.println();
  Serial.println("Running...");
}

// Polls the BOOT button with simple debouncing and advances to the next
// clock face on each press (falling edge, since the button is active LOW).
void checkBootButton() {
  static bool lastReading = HIGH;
  static bool buttonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; // ms

  bool reading = digitalRead(BOOT_BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) {
      Serial.println("Boot button pressed: switching clock face");
      nextClockFace();
    }
  }

  lastReading = reading;
}

void loop() {
  checkBootButton();

  static unsigned long lastCheck = 0;
  if (!inApFallback && WiFi.status() != WL_CONNECTED && millis() - lastCheck > 30000) {
    Serial.println("WiFi down, waiting for auto-reconnect...");
    lastCheck = millis();
    WiFi.begin();
  }

  server.handleClient();

  updateClock();
}
