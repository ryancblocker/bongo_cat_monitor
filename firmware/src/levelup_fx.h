// Level-up celebrations, inspired by monster-raising handhelds.
//
//  * Normal level-up: a text box slides up over the screen, "MOCHI GREW TO LV 4!" types out,
//    the EXP bar fills and flashes, the number rolls over with a "+1" pop (~3.6 s).
//  * Milestone level (5, 15, 30): the cat EVOLVES, full screen (~9.8 s): teal glow, the cat
//    goes white and flickers between its old and new forms faster and faster while light
//    streams in, a white flash, then the new form is revealed with a sparkle burst and the
//    box types out what it became.
#pragma once
#include "cat_assets.h"
#include "render.h"

// evolution stage for a level: 1 (Lv 1-4), 2 (Lv 5-14), 3 (Lv 15-29), 4 (Lv 30+)
inline int stageFor(int level) { return level >= 30 ? 4 : level >= 15 ? 3 : level >= 5 ? 2 : 1; }

class LevelUpFx {
 public:
  static constexpr uint32_t SIMPLE_MS = 3600, EVOLVE_MS = 9800;
  void start(uint32_t now, int fromLv, int toLv, bool evolve);
  bool active(uint32_t now) const { return on && now - t0 < dur(); }
  bool fullScreen() const { return evolve; }
  bool justEnded(uint32_t now);  // true once, right after it finishes
  void cancel() { on = false; ended = true; }
  void draw(Frame& f, uint32_t now, const CatImg* const* layers, const char* name) const;  // evolution
  void drawOverlay(Frame& f, uint32_t now, const char* name) const;                          // simple

 private:
  uint32_t dur() const { return evolve ? EVOLVE_MS : SIMPLE_MS; }
  bool on = false, ended = false, evolve = false;
  uint32_t t0 = 0;
  int from = 1, to = 2;
};
