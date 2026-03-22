# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 E-Paper Weather Display - A low-power, battery-operated weather station using an ESP32 microcontroller and 7.5" E-Paper display. Fetches weather data from OpenWeatherMap and optionally displays MLB standings. Achieves ~14μA sleep current for months of battery life.

## Build Commands

All commands run from the `platformio/` directory:

```bash
cd platformio
pio run                                    # Build
pio run --target upload                    # Upload to ESP32
pio run --target monitor                   # Serial monitor (115200 baud)
pio run --target clean                     # Clean build
pio run -e dfrobot_firebeetle2_esp32e      # Build specific environment
```

Platform: espressif32 @ 6.9.0, Arduino framework, C++17, 80MHz (power-optimized).
Partition: `huge_app.csv` (needed for fonts/icons in flash).

## Architecture

### Operation Cycle (main.cpp)

The device runs a single cycle in `setup()` then deep sleeps — `loop()` never executes:

1. Check battery voltage → enter extended sleep if low
2. Connect WiFi → record RSSI → sync time via NTP
3. Fetch API data (OpenWeatherMap one-call + air pollution, MLB standings/next game)
4. Kill WiFi immediately after data fetch
5. Initialize display → render all sections → full refresh (4-15s) → power off display
6. Calculate next wake time (aligned to `SLEEP_DURATION` minute boundaries, respects `BED_TIME`/`WAKE_TIME`) → deep sleep

### Configuration Split

- **config.h** (compile-time): Display type (`DISP_BW_V2`/`DISP_3C_B`/`DISP_7C_F`/`DISP_BW_V1`), driver board (`DRIVER_DESPI_C02`/`DRIVER_WAVESHARE`), locale, units, fonts, feature flags. Has compile-time assertions ensuring exactly one option per mutually exclusive group.
- **config.cpp** (runtime): WiFi credentials, OWM API key, lat/lon, pin assignments, timezone (POSIX string), sleep schedule, battery thresholds.

### Key Source Files

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, orchestrates full wake cycle |
| `config.cpp` / `config.h` | All configuration (see above) |
| `client_utils.cpp` | WiFi connect/disconnect, NTP sync, OWM + MLB API HTTP clients |
| `api_response.cpp` | ArduinoJson streaming deserializers for OWM one-call and air pollution |
| `mlb_response.cpp` | ArduinoJson deserializers for MLB StatsAPI (standings, games) |
| `renderer.cpp` | GxEPD2-based display layout: forecast, alerts, outlook graph, status bar, MLB |
| `display_utils.cpp` | Battery ADC reading, AQI calculation, icon/bitmap selection, string helpers |

### Data Flow

API responses are stored in static globals (`owm_onecall`, `owm_air_pollution`, `mlb_standings`, `mlb_next_game`) which are passed by reference to rendering functions. WiFi is killed before rendering starts to save power.

### Display Rendering (renderer.cpp)

Uses GxEPD2 library. The display object type is selected at compile time via `#ifdef DISP_*` blocks in `renderer.h`. All rendering uses Adafruit GFX primitives with custom alignment helpers (`drawString`, `drawMultiLnString`). Full refresh only — no partial updates.

### Assets (platformio/lib/esp32-weather-epd-assets/)

Fonts in Adafruit GFX binary format and bitmap icons at multiple resolutions (16px to 196px). Selected font family is configured in `config.h` via `FONT_HEADER`. All stored in PROGMEM.

## Hardware

- **MCU**: FireBeetle 2 ESP32-E (DFR0654) — cut low-power pad to reduce sleep current by 500μA
- **Display**: 7.5" E-Paper (800x480 or 640x384), Waveshare or Good Display
- **Driver**: DESPI-C02 (recommended, RESE→0.47) or Waveshare HAT rev2.2/2.3
- **Battery**: 3.7V LiPo via JST-PH2.0 connector, monitored on ADC pin A2

E-Paper SPI pins: CS=13, DC=22, RST=21, BUSY=14, SCK=18, MOSI=23, PWR=26.

## Important Patterns

- Battery-aware scheduling: multiple sleep intervals based on voltage thresholds (warn/low/very low/critical)
- NVS (Preferences library) persists low-battery state across sleep cycles
- HTTP client supports three modes: plain HTTP, HTTPS without cert verification, HTTPS with cert verification (cert in `cert/` directory)
- `DEBUG_LEVEL` in config.h: 0=basic, 1=verbose, 2=dumps raw API JSON responses
