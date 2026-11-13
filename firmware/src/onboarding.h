// First boot: an intro, the phone QR code with an egg waiting, the egg hatching into the cat
// the owner picked on the phone, then a guided tour of the controls.
//
//   INTRO  starfield, the title drops in, a glowing egg rises: "A NEW FRIEND IS WAITING..."
//          (~7 s, a tap skips it)
//   WAIT   the egg wobbles above the QR code until the phone page sends HATCH
//   HATCH  the egg rocks and cracks three times, glows, flashes white and the cat pops out
//          with shell bits and sparkles: "MOCHI HATCHED!" (~11 s)
//   TOUR   the desk screen with a spotlight on one control at a time and the cat explaining
//          it in a text box; a tap moves on. The last tap ends onboarding (saved in NVS).
#pragma once
#include "cat_assets.h"
#include "render.h"

class Onboarding {
 public:
  enum Phase : uint8_t { OFF, INTRO, WAIT, HATCH, TOUR };
  static constexpr uint32_t INTRO_MS = 7200, HATCH_MS = 11000;
  static constexpr int TOUR_STEPS = 9;

  Phase phase = OFF;
  void begin(uint32_t now) { go(INTRO, now); }
  bool hatch(uint32_t now);           // phone page confirmed the cat: true if it starts hatching
  bool update(uint32_t now);          // timed phase changes; true when the hatch just finished
  bool tap(uint32_t now);             // true when this tap finished the tour
  bool fullScreen() const { return phase == INTRO || phase == WAIT || phase == HATCH; }
  int wantCard() const;               // tour: card the step talks about (-1 = any)
  bool animating(uint32_t now) const; // WAIT only needs a redraw while the egg moves
  void draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name, const char* url) const;
  void drawTour(Frame& f, uint32_t now, const char* name) const;  // over the desk screen

 private:
  void go(Phase p, uint32_t now) { phase = p; t0 = now; }
  uint32_t t0 = 0;
  int step = 0;
};
