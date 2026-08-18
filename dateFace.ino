// The "date face" - shows the full weekday name, day-of-month, full month
// name and a small digital time readout. Unlike the other faces this is
// drawn entirely with GFX text rather than a BMP background, so it lives
// outside the /<n>/ data folders and is handled as its own special case
// wherever currentFaceNumber is used (see DATE_FACE_NUMBER in defines.h,
// setFaceForCurrentSlot()/nextClockFace() in bmp.ino, and updateClock() in
// clock.ino).
//
// These use the proper FreeSansBold GFX fonts (bundled with the
// Adafruit_GFX_Library dependency we already pull in) rather than the
// built-in 5x7 font scaled up with setTextSize() - scaling that font up
// this much just nearest-neighbour-blows-up each pixel, which looks
// blocky/pixelated. The bitmap fonts below are drawn at their native
// resolution instead.
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

const char *dayNames[7] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };
const char *monthNames[12] = {
  "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
  "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

// Baseline Y positions for each element. Note these custom GFX fonts anchor
// text at its BASELINE (not top-left, like the built-in font does) - so
// these values are where the bottom of the capital letters/digits sits, not
// the top of the text. Chosen so the four blocks (weekday/day/month/time)
// come out evenly spaced and centered as a group within the circular
// screen - see the comment block below for how these were derived.
//
//   block           font    height   gap-before
//   weekday name    12pt    ~18px    -
//   day number (16) 24pt    ~35px    20px
//   month name      12pt    ~18px    20px
//   time (14:38)     9pt    ~13px    20px
//
// Total stack height 18+35+18+13 + 3*20 = 144px, centered on the 240px
// circle (clock_center_y=120) gives a top edge at y=48 and bottom edge at
// y=192, and baselines fall out as below.
const int WEEKDAY_BASELINE_Y = 66;
const int DAY_BASELINE_Y     = 121;
const int MONTH_BASELINE_Y   = 159;
const int TIME_BASELINE_Y    = 192;

// Remembered digital-time text, so updateDateFaceTime() can tell when it
// actually needs to redraw (and can blank out exactly the old text).
char lastDateFaceTimeStr[6] = "";

// Draws `text` centered horizontally at the given font baseline. Always
// resets back to the built-in font afterwards so the BMP faces elsewhere
// (which use setTextSize() with the classic font) aren't affected.
void drawCenteredText(const char *text, int16_t baselineY, const GFXfont *font, uint16_t color) {
  tft.setFont(font);
  tft.setTextSize(1);
  tft.setTextColor(color);

  int16_t xPos, yPos;
  uint16_t width, height;
  tft.getTextBounds(text, 0, baselineY, &xPos, &yPos, &width, &height);
  // xPos is the first glyph's own left bearing relative to the x=0 origin
  // above - these custom GFX fonts (unlike the built-in font) have
  // per-character bearing, often a few pixels positive. Ignoring it and
  // centering on width alone leaves the actual ink shifted right of true
  // center by that amount, so subtract it here to compensate.
  tft.setCursor(clock_center_x - width / 2 - xPos, baselineY);
  tft.print(text);

  tft.setFont(NULL);
}

void drawDateFace() {
  tft.fillScreen(GC9A01A_BLACK);

  timeinfo = localtime(&currentTime);

  drawCenteredText(dayNames[timeinfo->tm_wday], WEEKDAY_BASELINE_Y, &FreeSansBold12pt7b, GC9A01A_BLUE);

  char dayNumStr[3];
  snprintf(dayNumStr, sizeof(dayNumStr), "%d", timeinfo->tm_mday);
  drawCenteredText(dayNumStr, DAY_BASELINE_Y, &FreeSansBold24pt7b, GC9A01A_WHITE);

  drawCenteredText(monthNames[timeinfo->tm_mon], MONTH_BASELINE_Y, &FreeSansBold12pt7b, GC9A01A_BLUE);

  // Force the time readout below to draw fresh rather than trying to erase
  // stale text left over from before the face was last shown.
  lastDateFaceTimeStr[0] = '\0';
  updateDateFaceTime();
}

// Refreshes just the small HH:MM readout at the bottom of the date face.
// Safe to call every minute - it's a no-op if the text hasn't changed.
void updateDateFaceTime() {
  timeinfo = localtime(&currentTime);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);

  if (strcmp(timeStr, lastDateFaceTimeStr) == 0) return;

  // "00:00" is always the same width as any other HH:MM string in this
  // font (digits are fixed-width), so blank out that fixed area rather than
  // trying to track the previous string's own bounds.
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextSize(1);
  int16_t xPos, yPos;
  uint16_t width, height;
  tft.getTextBounds("00:00", 0, TIME_BASELINE_Y, &xPos, &yPos, &width, &height);
  tft.setFont(NULL);

  tft.fillRect(clock_center_x - width / 2 - 2, yPos - 2, width + 4, height + 4, GC9A01A_BLACK);

  drawCenteredText(timeStr, TIME_BASELINE_Y, &FreeSansBold9pt7b, GC9A01A_LIGHTGREY);

  strncpy(lastDateFaceTimeStr, timeStr, sizeof(lastDateFaceTimeStr));
}
