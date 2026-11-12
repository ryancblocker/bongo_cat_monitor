// Everything the screen shows, fed by the PC over USB serial (see protocol.cpp).
#pragma once
#include <Arduino.h>

struct Settings {          // persisted in NVS ("bongo")
  uint8_t style = 0;       // CatStyle: 0 cartoon, 1 pixel, 2 8bit
  uint8_t cat = 0;         // CatKind: classic, chonky, kitten
  uint8_t coat = 2;        // CatCoat: default, grey, tabby
  char name[17] = "";      // shown on a tag above the cat ("" = hidden)
  uint8_t bright = 200;    // backlight 10..255
  uint8_t cycle = 8;       // seconds per card (3..60)
  uint8_t cards = 3;       // bit 0 weather card, bit 1 PC card
  bool h24 = false;
  uint8_t sleepMin = 5;    // minutes without typing before the cat dozes off
};

struct Weather {
  bool valid = false;
  int code = 0;            // WMO weather code
  bool day = true;
  int temp = 0, hi = 0, lo = 0, wind = 0, hum = -1;
  char unit = 'F';         // temperature unit
  char windUnit[5] = "mph";
  char place[32] = "";
  uint32_t at = 0;         // millis() of last update
};

struct PcStats {
  bool valid = false;
  int cpu = 0, ram = 0, wpm = 0;
  int cpuT = -1, gpu = -1, gpuT = -1;  // -1 = unknown
  char unit = 'F';
  uint32_t at = 0;
};

struct Clock {
  bool valid = false;
  uint32_t baseSec = 0;    // seconds since midnight at baseMs
  uint32_t baseMs = 0;
  char date[24] = "";      // "Thu, Nov 12" from the PC
  uint32_t secOfDay() const { return (baseSec + (millis() - baseMs) / 1000) % 86400; }
};

extern Settings settings;
extern Weather weather;
extern PcStats pc;
extern Clock clk;
extern char configUrl[96];     // phone config page, from the PC ("" = app not running)
extern uint32_t lastPcMsg;     // millis() of the last line from the PC

void saveSettingsSoon();       // debounced NVS write
void applyBrightness();
