// Pixel-art sky behind the cat (everything on a 2 px grid, like a Game Boy Color
// screen): dithered sky bands by condition and day/night, sun, moon + stars,
// drifting block clouds, rain, snow, fog and lightning.
#pragma once
#include "render.h"

enum class Sky : uint8_t { Clear, Partly, Cloudy, Fog, Drizzle, Rain, Snow, Storm };
Sky skyFor(int wmoCode);
const char* skyName(Sky s, bool day);  // "SUNNY", "CLOUDY"...

class WeatherFx {
 public:
  void set(Sky s, bool day);
  void update(float dt);     // seconds since last frame
  void draw(Frame& f);       // background
  void drawFlash(Frame& f);  // lightning flash over everything (storm only)
  Sky sky() const { return kind; }
  bool isDay() const { return day; }

 private:
  Sky kind = Sky::Clear;
  bool day = true;
  bool inited = false;
  uint16_t rowA[SH], rowB[SH];
  float t = 0;
  struct P { float x, y, v; uint8_t s; };
  static constexpr int MAXP = 70;
  P parts[MAXP];
  int nparts = 0;
  struct Cloud { float x; int16_t y; uint8_t big; float v; };
  Cloud clouds[5];
  int nclouds = 0;
  struct Star { int16_t x, y; uint8_t ph; };
  Star stars[24];
  float flash = 0, nextFlash = 5;
  int boltX = 120;
  void reset();
};
