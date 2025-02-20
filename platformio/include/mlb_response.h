/*
 * Header file for responses from the mlb API
 */

#ifndef __MLB_RESPONSE_H__
#define __MLB_RESPONSE_H__

#include <Arduino.h>
/*#include <ArduinoJson.h>*/
#include <HTTPClient.h>
#include <WiFi.h>
#include <nlohmann/json.hpp> // https://registry.platformio.org/libraries/johboh/nlohmann-json
using json = nlohmann::json;

#define MLB_NUM_DIVISIONS 6
#define MLB_TEAMS_PER_DIVISION 5

// Standings call:
// https://statsapi.mlb.com/api/v1/standings?leagueId=103,104&season=2025&standingsTypes=regularSeason&hydrate=division,conference,sport,league

#endif // __MLB_RESPONSE_H__
