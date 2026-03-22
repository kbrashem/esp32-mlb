/* Renderer for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "renderer.h"
#include "_locale.h"
#include "_strftime.h"
#include "api_response.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h"
#include "mlb_response.h"
#include <Arduino.h>

// fonts
#include FONT_HEADER

// icon header files
#include "icons/icons_128x128.h"
#include "icons/icons_160x160.h"
#include "icons/icons_16x16.h"
#include "icons/icons_196x196.h"
#include "icons/icons_24x24.h"
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_96x96.h"

#ifdef DISP_BW_V2
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif
#ifdef DISP_3C_B
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2>
    display(GxEPD2_750c_Z08(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif
#ifdef DISP_7C_F
GxEPD2_7C<GxEPD2_730c_GDEY073D46, GxEPD2_730c_GDEY073D46::HEIGHT / 4> display(
    GxEPD2_730c_GDEY073D46(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif
#ifdef DISP_BW_V1
GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT>
    display(GxEPD2_750(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif

#ifndef ACCENT_COLOR
#define ACCENT_COLOR GxEPD_BLACK
#endif

/* Returns the string width in pixels
 */
uint16_t getStringWidth(const String &text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

/* Returns the string height in pixels
 */
uint16_t getStringHeight(const String &text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return h;
}

/* Draws a string with alignment
 */
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextColor(color);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT) {
    x = x - w;
  }
  if (alignment == CENTER) {
    x = x - w / 2;
  }
  display.setCursor(x, y);
  display.print(text);
} // end drawString

/* Draws a string that will flow into the next line when max_width is reached.
 * If a string exceeds max_lines an ellipsis (...) will terminate the last word.
 * Lines will break at spaces(' ') and dashes('-').
 *
 * Note: max_width should be big enough to accommodate the largest word that
 *       will be displayed. If an unbroken string of characters longer than
 *       max_width exist in text, then the string will be printed beyond
 *       max_width.
 */
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color) {
  uint16_t current_line = 0;
  String textRemaining = text;
  // print until we reach max_lines or no more text remains
  while (current_line < max_lines && !textRemaining.isEmpty()) {
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(textRemaining, 0, 0, &x1, &y1, &w, &h);

    int endIndex = textRemaining.length();
    // check if remaining text is to wide, if it is then print what we can
    String subStr = textRemaining;
    int splitAt = 0;
    int keepLastChar = 0;
    while (w > max_width && splitAt != -1) {
      if (keepLastChar) {
        // if we kept the last character during the last iteration of this while
        // loop, remove it now so we don't get stuck in an infinite loop.
        subStr.remove(subStr.length() - 1);
      }

      // find the last place in the string that we can break it.
      if (current_line < max_lines - 1) {
        splitAt = std::max(subStr.lastIndexOf(" "), subStr.lastIndexOf("-"));
      } else {
        // this is the last line, only break at spaces so we can add ellipsis
        splitAt = subStr.lastIndexOf(" ");
      }

      // if splitAt == -1 then there is an unbroken set of characters that is
      // longer than max_width. Otherwise if splitAt != -1 then we can continue
      // the loop until the string is <= max_width
      if (splitAt != -1) {
        endIndex = splitAt;
        subStr = subStr.substring(0, endIndex + 1);

        char lastChar = subStr.charAt(endIndex);
        if (lastChar == ' ') {
          // remove this char now so it is not counted towards line width
          keepLastChar = 0;
          subStr.remove(endIndex);
          --endIndex;
        } else if (lastChar == '-') {
          // this char will be printed on this line and removed next iteration
          keepLastChar = 1;
        }

        if (current_line < max_lines - 1) {
          // this is not the last line
          display.getTextBounds(subStr, 0, 0, &x1, &y1, &w, &h);
        } else {
          // this is the last line, we need to make sure there is space for
          // ellipsis
          display.getTextBounds(subStr + "...", 0, 0, &x1, &y1, &w, &h);
          if (w <= max_width) {
            // ellipsis fit, add them to subStr
            subStr = subStr + "...";
          }
        }

      } // end if (splitAt != -1)
    } // end inner while

    drawString(x, y + (current_line * line_spacing), subStr, alignment, color);

    // update textRemaining to no longer include what was printed
    // +1 for exclusive bounds, +1 to get passed space/dash
    textRemaining = textRemaining.substring(endIndex + 2 - keepLastChar);

    ++current_line;
  } // end outer while

} // end drawMultiLnString

/* Initialize e-paper display
 */

void initDisplay() {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, HIGH);
#ifdef DRIVER_WAVESHARE
  display.init(115200, true, 2, false);
#endif
#ifdef DRIVER_DESPI_C02
  display.init(115200, true, 20, false);
#endif
  // remap spi
  SPI.end();
  SPI.begin(PIN_EPD_SCK, PIN_EPD_MISO, PIN_EPD_MOSI, PIN_EPD_CS);

  display.setRotation(0);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(false);
  display.setFullWindow();
  display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)
} // end initDisplay

/* Power-off e-paper display
 */
void powerOffDisplay() {
  display.hibernate(); // turns powerOff() and sets controller to deep sleep for
                       // minimum power use
  digitalWrite(PIN_EPD_PWR, LOW);
} // end powerOffDisplay

  /* This function is responsible for drawing the five day forecast.
   */
  void drawForecast(const owm_daily_t *daily, tm timeInfo) {
    // 5 day, forecast
    String hiStr, loStr;
    String dataStr, unitStr;
    for (int i = 0; i < 5; ++i) {
#ifndef DISP_BW_V1
      int x = 398 + (i * 82);
#elif defined(DISP_BW_V1)
    int x = 318 + (i * 64);
#endif
      // icons
      display.drawInvertedBitmap(x, 98 + 69 / 2 - 32 - 6,
                                 getDailyForecastBitmap64(daily[i]), 64, 64,
                                 GxEPD_BLACK);
      // day of week label
      display.setFont(&FONT_11pt8b);
      char dayBuffer[8] = {};
      _strftime(dayBuffer, sizeof(dayBuffer), "%a", &timeInfo); // abbrv'd day
      drawString(x + 31 - 2, 98 + 69 / 2 - 32 - 26 - 6 + 16, dayBuffer, CENTER);
      timeInfo.tm_wday = (timeInfo.tm_wday + 1) % 7; // increment to next day

      // high | low
      display.setFont(&FONT_8pt8b);
      drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 12, "|", CENTER);
#ifdef UNITS_TEMP_KELVIN
      hiStr = String(static_cast<int>(std::round(daily[i].temp.max)));
      loStr = String(static_cast<int>(std::round(daily[i].temp.min)));
#endif
#ifdef UNITS_TEMP_CELSIUS
      hiStr = String(static_cast<int>(
                  std::round(kelvin_to_celsius(daily[i].temp.max)))) +
              "\260";
      loStr = String(static_cast<int>(
                  std::round(kelvin_to_celsius(daily[i].temp.min)))) +
              "\260";
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
      hiStr = String(static_cast<int>(
                  std::round(kelvin_to_fahrenheit(daily[i].temp.max)))) +
              "\260";
      loStr = String(static_cast<int>(
                  std::round(kelvin_to_fahrenheit(daily[i].temp.min)))) +
              "\260";
#endif
      drawString(x + 31 - 4, 98 + 69 / 2 + 38 - 6 + 12, hiStr, RIGHT);
      drawString(x + 31 + 5, 98 + 69 / 2 + 38 - 6 + 12, loStr, LEFT);

// daily forecast precipitation
#if DISPLAY_DAILY_PRECIP
      float dailyPrecip;
#if defined(UNITS_DAILY_PRECIP_POP)
      dailyPrecip = daily[i].pop * 100;
      dataStr = String(static_cast<int>(dailyPrecip));
      unitStr = "%";
#else
    dailyPrecip = daily[i].snow + daily[i].rain;
#if defined(UNITS_DAILY_PRECIP_MILLIMETERS)
    // Round up to nearest mm
    dailyPrecip = std::round(dailyPrecip);
    dataStr = String(static_cast<int>(dailyPrecip));
    unitStr = String(" ") + TXT_UNITS_PRECIP_MILLIMETERS;
#elif defined(UNITS_DAILY_PRECIP_CENTIMETERS)
    // Round up to nearest 0.1 cm
    dailyPrecip = millimeters_to_centimeters(dailyPrecip);
    dailyPrecip = std::round(dailyPrecip * 10) / 10.0f;
    dataStr = String(dailyPrecip, 1);
    unitStr = String(" ") + TXT_UNITS_PRECIP_CENTIMETERS;
#elif defined(UNITS_DAILY_PRECIP_INCHES)
    // Round up to nearest 0.1 inch
    dailyPrecip = millimeters_to_inches(dailyPrecip);
    dailyPrecip = std::round(dailyPrecip * 10) / 10.0f;
    dataStr = String(dailyPrecip, 1);
    unitStr = String(" ") + TXT_UNITS_PRECIP_INCHES;
#endif
#endif
#if (DISPLAY_DAILY_PRECIP == 2) // smart
      if (dailyPrecip > 0.0f) {
#endif
        display.setFont(&FONT_6pt8b);
        drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 26, dataStr + unitStr,
                   CENTER);
#if (DISPLAY_DAILY_PRECIP == 2) // smart
      }
#endif
#endif // DISPLAY_DAILY_PRECIP
    }

  } // end drawForecast

  /* This function is responsible for drawing the current alerts if any.
   * Up to 2 alerts can be drawn.
   */
  void drawAlerts(std::vector<owm_alerts_t> & alerts, const String &city,
                  const String &date) {
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] alerts.size()    : " + String(alerts.size()));
#endif
    if (alerts.size() == 0) { // no alerts to draw
      return;
    }

    int *ignore_list = (int *)calloc(alerts.size(), sizeof(*ignore_list));
    int *alert_indices = (int *)calloc(alerts.size(), sizeof(*alert_indices));
    if (!ignore_list || !alert_indices) {
      Serial.println("Error: Failed to allocate memory while handling alerts.");
      free(ignore_list);
      free(alert_indices);
      return;
    }

    // Converts all event text and tags to lowercase, removes extra information,
    // and filters out redundant alerts of lesser urgency.
    filterAlerts(alerts, ignore_list);

    // limit alert text width so that is does not run into the location or date
    // strings
    display.setFont(&FONT_16pt8b);
    int city_w = getStringWidth(city);
    display.setFont(&FONT_12pt8b);
    int date_w = getStringWidth(date);
    int max_w = DISP_WIDTH - 2 - std::max(city_w, date_w) - (196 + 4) - 8;

    // find indices of valid alerts
    int num_valid_alerts = 0;
#if DEBUG_LEVEL >= 1
    Serial.print("[debug] ignore_list      : [ ");
#endif
    for (int i = 0; i < alerts.size(); ++i) {
#if DEBUG_LEVEL >= 1
      Serial.print(String(ignore_list[i]) + " ");
#endif
      if (!ignore_list[i]) {
        alert_indices[num_valid_alerts] = i;
        ++num_valid_alerts;
      }
    }
#if DEBUG_LEVEL >= 1
    Serial.println("]\n[debug] num_valid_alerts : " + String(num_valid_alerts));
#endif

    if (num_valid_alerts == 1) { // 1 alert
      // adjust max width to for 48x48 icons
      max_w -= 48;

      owm_alerts_t &cur_alert = alerts[alert_indices[0]];
      display.drawInvertedBitmap(196, 8, getAlertBitmap48(cur_alert), 48, 48,
                                 ACCENT_COLOR);
      // must be called after getAlertBitmap
      toTitleCase(cur_alert.event);

      display.setFont(&FONT_14pt8b);
      if (getStringWidth(cur_alert.event) <=
          max_w) { // Fits on a single line, draw along bottom
        drawString(196 + 48 + 4, 24 + 8 - 12 + 20 + 1, cur_alert.event, LEFT);
      } else { // use smaller font
        display.setFont(&FONT_12pt8b);
        if (getStringWidth(cur_alert.event) <=
            max_w) { // Fits on a single line with smaller font, draw along
                     // bottom
          drawString(196 + 48 + 4, 24 + 8 - 12 + 17 + 1, cur_alert.event, LEFT);
        } else { // Does not fit on a single line, draw higher to allow room for
                 // 2nd line
          drawMultiLnString(196 + 48 + 4, 24 + 8 - 12 + 17 - 11,
                            cur_alert.event, LEFT, max_w, 2, 23);
        }
      }
    } // end 1 alert
    else { // 2 alerts
      // adjust max width to for 32x32 icons
      max_w -= 32;

      display.setFont(&FONT_12pt8b);
      for (int i = 0; i < 2; ++i) {
        owm_alerts_t &cur_alert = alerts[alert_indices[i]];

        display.drawInvertedBitmap(196, (i * 32), getAlertBitmap32(cur_alert),
                                   32, 32, ACCENT_COLOR);
        // must be called after getAlertBitmap
        toTitleCase(cur_alert.event);

        drawMultiLnString(196 + 32 + 3, 5 + 17 + (i * 32), cur_alert.event,
                          LEFT, max_w, 1, 0);
      } // end for-loop
    } // end 2 alerts

    free(ignore_list);
    free(alert_indices);

  } // end drawAlerts

  /* This function is responsible for drawing the city string and date
   * information in the top right corner.
   */
  void drawLocationDate(const String &city, const String &date) {
    // location, date
    display.setFont(&FONT_16pt8b);
    drawString(DISP_WIDTH - 2, 23, city, RIGHT, ACCENT_COLOR);
    display.setFont(&FONT_12pt8b);
    drawString(DISP_WIDTH - 2, 30 + 4 + 17, date, RIGHT);
  } // end drawLocationDate

  /* The % operator in C++ is not a true modulo operator but it instead a
   * remainder operator. The remainder operator and modulo operator are
   * equivalent for positive numbers, but not for negatives. The follow
   * implementation of the modulo operator works for +/-a and +b.
   */
  inline int modulo(int a, int b) {
    const int result = a % b;
    return result >= 0 ? result : result + b;
  }

  /* Convert temperature in kelvin to the display y coordinate to be plotted.
   */
  int kelvin_to_plot_y(float kelvin, int tempBoundMin, float yPxPerUnit,
                       int yBoundMin) {
#ifdef UNITS_TEMP_KELVIN
    return static_cast<int>(
        std::round(yBoundMin - (yPxPerUnit * (kelvin - tempBoundMin))));
#endif
#ifdef UNITS_TEMP_CELSIUS
    return static_cast<int>(std::round(
        yBoundMin - (yPxPerUnit * (kelvin_to_celsius(kelvin) - tempBoundMin))));
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    return static_cast<int>(std::round(
        yBoundMin -
        (yPxPerUnit * (kelvin_to_fahrenheit(kelvin) - tempBoundMin))));
#endif
  }

  /* This function is responsible for drawing the status bar along the bottom of
   * the display.
   */
  void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                     int rssi, uint32_t batVoltage) {
    String dataStr;
    uint16_t dataColor = GxEPD_BLACK;
    display.setFont(&FONT_6pt8b);
    int pos = DISP_WIDTH - 2;
    const int sp = 2;

#if BATTERY_MONITORING
    // battery - (expecting 3.7v LiPo)
    uint32_t batPercent =
        calcBatPercent(batVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
#if defined(DISP_3C_B) || defined(DISP_7C_F)
    if (batVoltage < WARN_BATTERY_VOLTAGE) {
      dataColor = ACCENT_COLOR;
    }
#endif
    dataStr = String(batPercent) + "%";
#if STATUS_BAR_EXTRAS_BAT_VOLTAGE
    dataStr += " (" + String(std::round(batVoltage / 10.f) / 100.f, 2) + "v)";
#endif
    drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
    pos -= getStringWidth(dataStr) + 25;
    display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 17,
                               getBatBitmap24(batPercent), 24, 24, dataColor);
    pos -= sp + 9;
#endif

    // WiFi
    dataStr = String(getWiFidesc(rssi));
    dataColor = rssi >= -70 ? GxEPD_BLACK : ACCENT_COLOR;
#if STATUS_BAR_EXTRAS_WIFI_RSSI
    if (rssi != 0) {
      dataStr += " (" + String(rssi) + "dBm)";
    }
#endif
    drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
    pos -= getStringWidth(dataStr) + 19;
    display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 13, getWiFiBitmap16(rssi),
                               16, 16, dataColor);
    pos -= sp + 8;

    // last refresh
    dataColor = GxEPD_BLACK;
    drawString(pos, DISP_HEIGHT - 1 - 2, refreshTimeStr, RIGHT, dataColor);
    pos -= getStringWidth(refreshTimeStr) + 25;
    display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 21, wi_refresh_32x32, 32,
                               32, dataColor);
    pos -= sp;

    // status
    dataColor = ACCENT_COLOR;
    if (!statusStr.isEmpty()) {
      drawString(pos, DISP_HEIGHT - 1 - 2, statusStr, RIGHT, dataColor);
      pos -= getStringWidth(statusStr) + 24;
      display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 18, error_icon_24x24,
                                 24, 24, dataColor);
    }

  } // end drawStatusBar

  /* This function is responsible for drawing prominent error messages to the
   * screen.
   *
   * If error message line 2 (errMsgLn2) is empty, line 1 will be automatically
   * wrapped.
   */
  void drawError(const uint8_t *bitmap_196x196, const String &errMsgLn1,
                 const String &errMsgLn2) {
    display.setFont(&FONT_26pt8b);
    if (!errMsgLn2.isEmpty()) {
      drawString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1,
                 CENTER);
      drawString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21 + 55, errMsgLn2,
                 CENTER);
    } else {
      drawMultiLnString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21,
                        errMsgLn1, CENTER, DISP_WIDTH - 200, 2, 55);
    }
    display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                               DISP_HEIGHT / 1 - 196 / 2 - 21, bitmap_196x196,
                               196, 196, ACCENT_COLOR);
  } // end drawError

  void drawMlbStandings(const mlb_standings_resp_t &mlb_standings) {
    if (mlb_standings.divisions.empty()) {
      return;
    }

    String dataStr;

    const int xPos0 = 5;
    const int xCol1 = xPos0 + 280;
    const int xCol2 = xPos0 + 325;
    const int xCol3 = xPos0 + 380;

    int numDivs = std::min(static_cast<int>(mlb_standings.divisions.size()), 3);

    for (int d = numDivs - 1; d >= 0; d--) {

      const int yPos1 = 20 + d * 160;

      display.setFont(&FONT_14pt8b);
      String divName = mlb_standings.divisions[d].division_name;
      divName.replace("American League", "AL");
      divName.replace("National League", "NL");
      drawString(xPos0 + 40, yPos1, divName, LEFT);
      drawString(xCol1, yPos1, "W", RIGHT);
      drawString(xCol2, yPos1, "L", RIGHT);
      drawString(xCol3, yPos1, "GB", RIGHT);

      int numTeams = std::min(
          static_cast<int>(mlb_standings.divisions[d].teams.size()),
          MLB_TEAMS_PER_DIV);
      for (int i = 0; i < numTeams; ++i) {
        int y = yPos1 + 26 + (i * 26);
        const auto &team = mlb_standings.divisions[d].teams[i];
        display.setFont(&FONT_14pt8b);
        drawString(xPos0, y, team.team_name, LEFT);
        display.setFont(&FONT_12pt8b);
        dataStr = String(team.wins);
        drawString(xCol1, y, dataStr, RIGHT);
        dataStr = String(team.losses);
        drawString(xCol2, y, dataStr, RIGHT);
        drawString(xCol3, y, team.games_back, RIGHT);
      }
    }
  } // end drawMLBStandings

  void drawMlbNextGame(const mlb_next_game_t &mlb_next_game) {
    String dataStr;

    int xPos0 = 440;
    int yPos0 = 280;

    // Loop through strings
    display.setFont(&FONT_12pt8b);
    drawString(xPos0, yPos0, mlb_next_game.game_datetime_local, LEFT);
    yPos0 += 24;
    drawString(xPos0, yPos0, mlb_next_game.away_name, LEFT);
    yPos0 += 20;
    drawString(xPos0 + 16, yPos0, mlb_next_game.away_pitcher, LEFT);
    yPos0 += 24;
    drawString(xPos0, yPos0, "@" + mlb_next_game.home_name, LEFT);
    yPos0 += 20;
    drawString(xPos0 + 16, yPos0, mlb_next_game.home_pitcher, LEFT);

  } // end drawMLBNextGame
