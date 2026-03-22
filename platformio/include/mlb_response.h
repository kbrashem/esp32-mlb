#ifndef __MLB_RESPONSE_H__
#define __MLB_RESPONSE_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstdint>
#include <vector>

#define MLB_TEAMS_PER_DIV 5

typedef struct mlb_team_standing {
  String team_name;
  int wins;
  int losses;
  float win_pct;
  String games_back;
  String streak; // streak code
  int last_10_wins;
  int last_10_losses;
  int home_wins;
  int home_losses;
  int away_wins;
  int away_losses;
  int wc_rank;
  String wc_games_back;
} mlb_team_standing_t;

typedef struct mlb_division_standing {
  String division_name;
  std::vector<mlb_team_standing_t> teams;
} mlb_division_standing_t;

typedef struct mlb_standings_resp {
  std::vector<mlb_division_standing_t> divisions;
} mlb_standings_resp_t;

DeserializationError deserializeMlbStandings(WiFiClient &json,
                                             mlb_standings_resp_t &r);

// Next Game
typedef struct mlb_next_game {
  bool is_game_today;
  String game_id;
  String home_name;
  String away_name;
  String home_pitcher;
  String away_pitcher;
  String game_datetime;       // Original ISO format UTC datetime
  String game_datetime_local; // Human-readable local time
  String game_date;
} mlb_next_game_t;

DeserializationError deserializeMlbNextGame(WiFiClient &json,
                                            mlb_next_game_t &r);

#endif // __MLB_RESPONSE_H__
