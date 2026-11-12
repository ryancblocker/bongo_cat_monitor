// Animated sky behind the cat: gradient by condition and day/night, plus sun,
// moon + stars, drifting clouds, rain, snow, fog and lightning.
#pragma once
#include "render.h"

enum class Sky : uint8_t { Clear, Partly, Cloudy, Fog, Drizzle, Rain, Snow, Storm };
Sky skyFor(int wmoCode);
const char* skyName(Sky s, bool day);  // "Clear", "Partly cloudy"...

class WeatherFx {
 public:
  void set(Sky s, bool day);
  void update(float dt);     // seconds since last frame
  void draw(Frame& f);       // background: sky, sun/moon, clouds, particles
  void drawFlash(Frame& f);  // lightning flash over everything (storm only)
  Sky sky() const { return kind; }
  bool isDay() const { return day; }

 private:
  Sky kind = Sky::Clear;
  bool day = true;
  bool inited = false;
  uint16_t rows[SH];
  float t = 0;
  struct P { float x, y, v, s; };
  static constexpr int MAXP = 80;
  P parts[MAXP];
  int nparts = 0;
  struct Cloud { float x, y, s, v; };
  Cloud clouds[5];
  int nclouds = 0;
  struct Star { int16_t x, y; uint8_t ph; };
  Star stars[28];
  float flash = 0, nextFlash = 5;
  float boltX = 120;
  void reset();
};
