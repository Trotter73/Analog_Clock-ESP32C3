#define HOSTNAME "RoundClock"

#define SSID_ADDR 0 // String[60]
#define WIFI_PASSWORD_ADDR (SSID_ADDR + sizeof(ssid)) // String[60]
#define BRIGHTNESS_ADDR (WIFI_PASSWORD_ADDR + sizeof(wifiPassword)) // int

// --- OpenWeatherMap (weather face config) ---
#define OWM_API_KEY_LEN 40  // OWM keys are 32 hex chars; leaves some headroom
#define OWM_COORD_LEN 16    // enough for e.g. "-123.456789"

#define OWM_API_KEY_ADDR (BRIGHTNESS_ADDR + sizeof(int)) // char[OWM_API_KEY_LEN]
#define OWM_LAT_ADDR (OWM_API_KEY_ADDR + OWM_API_KEY_LEN) // char[OWM_COORD_LEN]
#define OWM_LON_ADDR (OWM_LAT_ADDR + OWM_COORD_LEN) // char[OWM_COORD_LEN]

// Plain HTTP, deliberately not HTTPS - avoids WiFiClientSecure/TLS on the
// ESP32-C3, which has been a source of heap fragmentation crashes on this
// chip on other projects. OpenWeatherMap's free API still answers on :80.
#define OWM_CURRENT_URL_FMT "http://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&units=metric&appid=%s"

// How often to re-fetch the weather while the weather face is showing.
#define WEATHER_REFRESH_INTERVAL_MS (15UL * 60UL * 1000UL) // 15 minutes

// Cap on how long a single OWM HTTP request is allowed to block for, in
// seconds. Without this, a stalled DNS lookup or connection can hang
// fetchWeather() - and with it the whole loop(), since everything runs
// synchronously - for a very long time (or indefinitely, depending on the
// underlying fault). Accepts decimals (e.g. 2.5).
#define HTTP_TIMEOUT_SECONDS 6
#define HTTP_TIMEOUT_MS ((unsigned long)(HTTP_TIMEOUT_SECONDS * 1000UL))

// Which icon shape to draw for the current condition - see weatherFace.ino.
// Declared here (rather than in weatherFace.ino, where it's used) because
// the Arduino build auto-generates a forward declaration for
// weatherIconTypeFor() and inserts it near the very top of the concatenated
// sketch, before any type defined further down a .ino tab would be visible -
// this needs to already be in scope from an early #include instead.
enum WeatherIconType { ICON_SUN, ICON_CLOUD, ICON_RAIN, ICON_THUNDER, ICON_SNOW, ICON_FOG };

// The timezone to sync the date/time to, using NTP
#define TZ_Europe_London "GMT0BST,M3.5.0/1,M10.5.0"
#define MY_TZ TZ_Europe_London

// NTP server list to use for syncing time.
#define NTP_SERVER "pool.ntp.org"

#define NUMBER_OF_FACE_FILES 3

// Software-drawn faces (GFX text, not a BMP background) live directly after
// the picture faces above - see dateFace.ino/weatherFace.ino and bmp.ino
// (setFaceForCurrentSlot/nextClockFace).
#define DATE_FACE_NUMBER (NUMBER_OF_FACE_FILES + 1)
#define WEATHER_FACE_NUMBER (DATE_FACE_NUMBER + 1)

// Automatic display rotation: clock face -> weather -> date -> repeat,
// advancing every ROTATION_INTERVAL_SECONDS. Driven off millis() elapsed
// time (see lastRotationMillis in bmp.ino) rather than calendar minutes, so
// this accepts decimals (e.g. 0.5 for a half-second rotation while testing)
// and sub-minute intervals generally. The BOOT button is separate - it only
// ever cycles the picture clock faces (see nextClockFace() in bmp.ino), it
// doesn't touch this rotation.
#define ROTATION_INTERVAL_SECONDS 30 // 5 minutes
#define ROTATION_INTERVAL_MS ((unsigned long)(ROTATION_INTERVAL_SECONDS * 1000UL))

#define TFT_VCC 3V3  // 3V3
#define TFT_GND GND  // GND
#define TFT_DIN 3    // SPI MOSI / SDA, GPIO3
#define TFT_CLK 4    // SPI SCLK / SCL, GPIO4
#define TFT_CS  1    // GPIO1
#define TFT_DC  10   // GPIO10
#define TFT_RST 0    // GPIO0
#define TFT_BL  -1   // not wired to a GPIO; tie board's BL pin to 3V3

#define BOOT_BUTTON_PIN 9  // BOOT button, active LOW, has an onboard pull-up

#define SCREEN_DIAMETER 240

#define SECOND_SPLIT 5

const int clock_center_x = SCREEN_DIAMETER / 2;
const int clock_center_y = SCREEN_DIAMETER / 2;

const float pi = 3.14159267 ;
  
#define CSS "<style>\
  body {\
    background: #fad7a0;\
    color: #154360;\
    padding: 20px;\
    font-size: 3em;\
    text-align: center;\
  }\
  div.container {\
    display: inline-block;\
    width: 90%;\
    background: #f8c471;\
    box-shadow: 15px 20px 20px #88888888;\
    border-radius: 20px;\
    padding: 2% 2% 4% 2%;\
    text-align: left;\
  }\
  h1 {\
    margin-top: 0;\
  }\
  input {\
    width: 100%;\
    border: 0;\
    border-bottom: 2px solid grey;\
    background: none;\
    color: #154360;\
    font-size: 1.2em;\
  }\
  input[type=\"range\"] {\
    width: 100%;\
  }\
  input[type=\"submit\"] {\
    background: #154360;\
    color: #fad7a0;\
    border: 0;\
    border-radius: 5px;\
    width: 40%;\
    padding: 3% 0;\
    cursor: pointer;\
    font-size: 1em;\
    display: block;\
    margin: 8% auto 0 auto;\
  }\
  div div {\
    position: absolute;\
    right: 2%;\
    bottom: 2%;\
    font-size: .6em;\
  }\
</style>"
