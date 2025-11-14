#include "mlb_response.h"
#include "_strftime.h"
#include "config.h"
#include <ArduinoJson.h>
#include <vector>

DeserializationError deserializeMlbStandings(WiFiClient &json,
                                             mlb_standings_resp_t &r) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  for (JsonObject division : doc["records"].as<JsonArray>()) {
    mlb_division_standing_t division_standing;
    division_standing.division_name =
        division["division"]["name"].as<const char *>();

    for (JsonObject team : division["teamRecords"].as<JsonArray>()) {
      mlb_team_standing_t team_standing;
      team_standing.team_name = team["team"]["name"].as<const char *>();
      team_standing.wins = team["wins"].as<int>();
      team_standing.losses = team["losses"].as<int>();
      team_standing.win_pct = team["winningPercentage"].as<float>();
      team_standing.games_back = team["gamesBack"].as<float>();
      team_standing.streak = team["streak"]["streakCode"].as<const char *>();
      team_standing.last_10_wins =
          team["records"]["splitRecords"][8]["wins"].as<int>();
      team_standing.last_10_losses =
          team["records"]["splitRecords"][8]["losses"].as<int>();
      team_standing.home_wins =
          team["records"]["splitRecords"][0]["wins"].as<int>();
      team_standing.home_losses =
          team["records"]["splitRecords"][0]["losses"].as<int>();
      team_standing.away_wins =
          team["records"]["splitRecords"][1]["wins"].as<int>();
      team_standing.away_losses =
          team["records"]["splitRecords"][1]["losses"].as<int>();
      team_standing.wc_rank = team["wildCardRank"].as<int>();
      team_standing.wc_games_back = team["wildCardGamesBack"].as<int>();

      division_standing.teams.push_back(team_standing);
    }

    r.divisions.push_back(division_standing);
  }

  return error;
} // end deserializeMlbStandings

DeserializationError deserializeMlbNextGame(WiFiClient &json,
                                            mlb_next_game_t &g) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  if (doc["dates"].size() == 0) {
    g.is_game_today = false;
    return error;
  }

  g.is_game_today = true;
  g.game_id = doc["dates"][0]["games"][0]["gamePk"].as<const char *>();
  g.home_name = doc["dates"][0]["games"][0]["teams"]["home"]["team"]["name"]
                    .as<const char *>();
  g.away_name = doc["dates"][0]["games"][0]["teams"]["away"]["team"]["name"]
                    .as<const char *>();

  // Store the original ISO datetime string
  const char *rawDatetime =
      doc["dates"][0]["games"][0]["gameDate"].as<const char *>();
  g.game_datetime = rawDatetime;

  // Convert ISO datetime string to human-readable local time
  // Format of ISO string: "2023-04-15T20:10:00Z" (UTC time)
  struct tm timeinfo = {};
  int year, month, day, hour, minute, second;

  // Parse the ISO datetime string
  if (sscanf(rawDatetime, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour,
             &minute, &second) == 6) {

    // Set up the tm structure with UTC time
    timeinfo.tm_year = year - 1900; // years since 1900
    timeinfo.tm_mon = month - 1;    // months since January (0-11)
    timeinfo.tm_mday = day;         // day of the month (1-31)
    timeinfo.tm_hour = hour;        // hours since midnight (0-23)
    timeinfo.tm_min = minute;       // minutes after the hour (0-59)
    timeinfo.tm_sec = second;       // seconds after the minute (0-59)

    // Convert UTC time to time_t
    time_t utcTime = mktime(&timeinfo);

    // Adjust for timezone difference between GMT and local time
    // mktime() assumes the tm struct is in local time, so we need to correct
    // for the timezone difference
    time_t gmtOffset = 0;
    {
      struct tm tmGMT = {};
      struct tm tmLocal = {};
      time_t now = time(NULL);
      gmtime_r(&now, &tmGMT);
      localtime_r(&now, &tmLocal);

      // Calculate offset from struct tm differences
      time_t gmtNow = mktime(&tmGMT);
      time_t localNow = mktime(&tmLocal);
      gmtOffset = localNow - gmtNow;
    }

    // Apply the timezone correction for the correct local time
    time_t localTime = utcTime + gmtOffset;

    // Convert to readable string
    struct tm *localTimeinfo = localtime(&localTime);
    char timeBuffer[32] = {};
    _strftime(timeBuffer, sizeof(timeBuffer), "%a, %b %d %H:%M", localTimeinfo);

    // Store the formatted local time
    g.game_datetime_local = timeBuffer;
  } else {
    // If parsing fails, just store the original string
    g.game_datetime_local = rawDatetime;
  }

  g.game_date = doc["dates"][0]["games"][0]["officialDate"].as<const char *>();

  return error;
} // end deserializeMlbNextGame
