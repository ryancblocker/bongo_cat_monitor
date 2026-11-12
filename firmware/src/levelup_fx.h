// Full-screen anime-style LEVEL UP (about 5 s), see levelup_fx.cpp for the beats:
//   impact flash -> rotating speed lines on deep purple -> the cat rises with a pulsing
//   gold aura, energy sparks and a shockwave -> "LEVEL" slams down, "UP!" slams up with
//   screen shake -> "Lv 2 > Lv 3" ticks over with a burst -> name banner -> flash out.
#pragma once
#include "cat_assets.h"
#include "render.h"

class LevelUpFx {
 public:
  static constexpr uint32_t DURATION = 5000;
  void start(uint32_t now, uint16_t fromLv, uint16_t toLv);
  bool active(uint32_t now) const { return on && now - t0 < DURATION; }
  bool justEnded(uint32_t now);  // true once, right after it finishes
  void cancel() { on = false; ended = true; }
  void draw(Frame& f, uint32_t now, const CatImg* const* layers, const char* name) const;

 private:
  bool on = false, ended = false;
  uint32_t t0 = 0;
  uint16_t from = 1, to = 2;
};
