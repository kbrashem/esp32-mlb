#include "mlb_response.h"
#include "_strftime.h"
#include "config.h"
#include <ArduinoJson.h>
#include <vector>

DeserializationError deserializeMlbStandings(WiFiClient &json,
                                             mlb_standings_resp_t &r) {
  JsonDocument filter;
  for (int d = 0; d < 3; ++d) {
    filter["records"][d]["division"]["name"] = true;
    for (int t = 0; t < MLB_TEAMS_PER_DIV; ++t) {
      auto tr = filter["records"][d]["teamRecords"][t];
      tr["team"]["name"] = true;
      tr["wins"] = true;
      tr["losses"] = true;
      tr["winningPercentage"] = true;
      tr["gamesBack"] = true;
      tr["streak"]["streakCode"] = true;
      tr["records"]["splitRecords"] = true;
      tr["wildCardRank"] = true;
      tr["wildCardGamesBack"] = true;
    }
  }

  JsonDocument doc;

  DeserializationError error =
      deserializeJson(doc, json, DeserializationOption::Filter(filter));
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
      team_standing.games_back = team["gamesBack"].as<String>();
      team_standing.streak = team["streak"]["streakCode"].as<const char *>();
      // Search split records by type to avoid fragile index assumptions
      for (JsonObject sr : team["records"]["splitRecords"].as<JsonArray>()) {
        const char *type = sr["type"].as<const char *>();
        if (!type) continue;
        if (strcmp(type, "lastTen") == 0) {
          team_standing.last_10_wins = sr["wins"].as<int>();
          team_standing.last_10_losses = sr["losses"].as<int>();
        } else if (strcmp(type, "home") == 0) {
          team_standing.home_wins = sr["wins"].as<int>();
          team_standing.home_losses = sr["losses"].as<int>();
        } else if (strcmp(type, "away") == 0) {
          team_standing.away_wins = sr["wins"].as<int>();
          team_standing.away_losses = sr["losses"].as<int>();
        }
      }
      team_standing.wc_rank = team["wildCardRank"].as<int>();
      team_standing.wc_games_back = team["wildCardGamesBack"].as<String>();

      division_standing.teams.push_back(team_standing);
    }

    r.divisions.push_back(division_standing);
  }

  return error;
} // end deserializeMlbStandings

DeserializationError deserializeMlbNextGame(WiFiClient &json,
                                            mlb_next_game_t &g) {
  JsonDocument filter;
  auto game = filter["dates"][0]["games"][0];
  game["gamePk"] = true;
  game["teams"]["home"]["team"]["name"] = true;
  game["teams"]["home"]["probablePitcher"]["fullName"] = true;
  game["teams"]["away"]["team"]["name"] = true;
  game["teams"]["away"]["probablePitcher"]["fullName"] = true;
  game["gameDate"] = true;
  game["officialDate"] = true;

  JsonDocument doc;

  DeserializationError error =
      deserializeJson(doc, json, DeserializationOption::Filter(filter));
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
  g.game_id = String(doc["dates"][0]["games"][0]["gamePk"].as<int>());
  g.home_name = doc["dates"][0]["games"][0]["teams"]["home"]["team"]["name"]
                    .as<const char *>();
  g.away_name = doc["dates"][0]["games"][0]["teams"]["away"]["team"]["name"]
                    .as<const char *>();

  const char *homePitcher = doc["dates"][0]["games"][0]["teams"]["home"]
                                ["probablePitcher"]["fullName"]
                                    .as<const char *>();
  g.home_pitcher = homePitcher ? homePitcher : "TBD";

  const char *awayPitcher = doc["dates"][0]["games"][0]["teams"]["away"]
                                ["probablePitcher"]["fullName"]
                                    .as<const char *>();
  g.away_pitcher = awayPitcher ? awayPitcher : "TBD";

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

    // Convert UTC tm to time_t.
    // mktime() interprets its argument as local time, so we temporarily
    // switch to UTC, call mktime, then restore the original timezone.
    char *origTz = getenv("TZ");
    String savedTz;
    if (origTz) {
      savedTz = origTz;
    }
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t utcTime = mktime(&timeinfo);
    if (origTz) {
      setenv("TZ", savedTz.c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();

    // Convert UTC time_t to local time using the system timezone
    struct tm localTimeinfo = {};
    localtime_r(&utcTime, &localTimeinfo);

    char timeBuffer[32] = {};
    _strftime(timeBuffer, sizeof(timeBuffer), "%a, %b %d %H:%M",
              &localTimeinfo);

    // Store the formatted local time
    g.game_datetime_local = timeBuffer;
  } else {
    // If parsing fails, just store the original string
    g.game_datetime_local = rawDatetime;
  }

  g.game_date = doc["dates"][0]["games"][0]["officialDate"].as<const char *>();

  return error;
} // end deserializeMlbNextGame
