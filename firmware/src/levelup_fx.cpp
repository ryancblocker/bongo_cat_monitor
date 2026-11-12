// Anime LEVEL UP, about 5 s. Beats:
//   0.00 flash            0.12 crouch            0.35 LAUNCH (fists up, afterimages, energy trail)
//   0.95 hang + power-up  1.45 DIVE              1.70 IMPACT: superhero landing on the desk
//   (impact frame: black + white silhouette, then flash, heavy shake, cracks, debris, dust)
//   1.95 "LEVEL" slams    2.15 "UP!" slams       2.45 level counter, 2.85 ticks over
//   3.30 happy hop        3.40 name banner       4.60 flash out
// Vertical speed lines stream the whole time: down while rising, up while diving.
#include "levelup_fx.h"

#include <math.h>

static const uint32_t INK = 0x202838, GOLD = 0xF8C038, ORANGE = 0xF09830, PAPER = 0xF8F8F0;
static const float T_CROUCH = 0.12f, T_LAUNCH = 0.35f, T_HANG = 0.95f, T_DIVE = 1.45f, T_IMPACT = 1.70f;
static const int AIR = -118;            // how high the cat flies (px above its desk spot)
static const int IMPACT_X = 172, IMPACT_Y = 268;  // where the fist hits the desk

void LevelUpFx::start(uint32_t now, uint16_t fromLv, uint16_t toLv) {
  on = true;
  ended = false;
  t0 = now;
  from = fromLv;
  to = toLv;
}

bool LevelUpFx::justEnded(uint32_t now) {
  if (on && !ended && now - t0 >= DURATION) {
    ended = true;
    on = false;
    return true;
  }
  return false;
}

static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float easeOutCubic(float p) { return 1 - powf(1 - p, 3); }
static float easeInQuad(float p) { return p * p; }
static float easeOutBack(float p) {
  const float c1 = 2.4f, c3 = c1 + 1;
  return 1 + c3 * powf(p - 1, 3) + c1 * powf(p - 1, 2);
}
static uint32_t hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}

static void title(Frame& f, const char* s, int x, int cy, const uint8_t* font, uint32_t fill) {
  const int o = font == PX32 ? 3 : 2;
  f.text(s, x + o + 2, cy + o + 2, font, ORANGE, AL_C);
  for (int dy = -o; dy <= o; dy += o)
    for (int dx = -o; dx <= o; dx += o)
      if (dx || dy) f.text(s, x + dx, cy + dy, font, INK, AL_C);
  f.text(s, x, cy, font, fill, AL_C);
}

// cat height above its desk spot over time (negative = up)
static float catDy(float t) {
  if (t < T_CROUCH) return 0;
  if (t < T_LAUNCH) return 8 * sinf((t - T_CROUCH) / (T_LAUNCH - T_CROUCH) * (float)M_PI / 2);  // squat
  if (t < T_HANG) return 8 + (AIR - 8) * easeOutCubic((t - T_LAUNCH) / (T_HANG - T_LAUNCH));
  if (t < T_DIVE) return AIR + 4 * sinf((t - T_HANG) * 14);  // hover, trembling with power
  if (t < T_IMPACT) return AIR * (1 - easeInQuad((t - T_DIVE) / (T_IMPACT - T_DIVE)));
  if (t > 3.3f && t < 3.6f) return -4 * sinf((t - 3.3f) / 0.3f * (float)M_PI);  // happy bounce (stays on the desk)
  return 0;
}

// how far the speed lines have scrolled by time t (+ = down): integral of a piecewise speed,
// so they speed up, slow down and reverse smoothly instead of jumping
static float linePos(float t) {
  struct Seg { float a, b, v; };
  static const Seg segs[] = {{0, T_LAUNCH, 60}, {T_LAUNCH, T_HANG, 1300}, {T_HANG, T_DIVE, 160}, {T_DIVE, T_IMPACT, -1800}};
  float pos = 0;
  for (const Seg& s : segs) pos += s.v * (min(t, s.b) - s.a > 0 ? min(t, s.b) - s.a : 0);
  return pos;
}

static float catVel(float t) { return (catDy(t + 0.01f) - catDy(t)) / 0.01f; }  // px/s, + = down

// thrust flare under the cat while it's airborne: a hot band along the bottom of the body
// (which also hides where the body would meet the desk) and jagged streaks shooting away
// from the direction of travel, flickering every frame
static void energyTrail(Frame& f, int dy, float t, bool diving) {
  uint32_t flick = (uint32_t)(t * 30);
  for (int hx = 14; hx <= 114; hx += 2) {
    int x = 2 * hx;
    int y = (CAT_Y - 30 + dy + 2 * (int)(0.246f * hx + 59.8f + 2)) & ~1;
    f.fill(x, y - 4, 4, 10, c565(0xFFF8D0));  // white-hot core band
    if (diving) continue;  // diving: the afterimages carry the motion; streaks would cover the face
    uint32_t h = hash(hx * 131 + flick);
    int len = 10 + (int)(h % 46);
    if ((hx / 2) % 2 == 0) len = len * 2 / 3;
    int dir = diving ? -1 : 1;  // rising: flare points down; diving: it trails above
    int y0 = diving ? y - 6 - len : y + 6;
    // white -> gold -> orange along the streak
    int a = len / 3;
    f.fill(x, y0 + (dir > 0 ? 0 : len - a), 2 + (h >> 8 & 2), a, c565(0xFFF4C0));
    f.fill(x, y0 + (dir > 0 ? a : a), 2 + (h >> 8 & 2), a, c565(GOLD));
    f.fill(x, y0 + (dir > 0 ? 2 * a : 0), 2, a, c565(ORANGE));
  }
}

void LevelUpFx::draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name) const {
  float t = (now - t0) / 1000.0f;
  bool impactFrame = t >= T_IMPACT && t < T_IMPACT + 0.10f;
  float sinceImpact = t - T_IMPACT;

  // heavy shake after the landing, small kicks on each title slam
  int shake = 0;
  if (sinceImpact > 0.10f && sinceImpact < 0.75f) {
    int amp = (int)(10 * (1 - (sinceImpact - 0.10f) / 0.65f));
    shake = (((now / 30) & 1) ? amp : -amp) & ~1;
  }
  if ((t > 2.12f && t < 2.24f) || (t > 2.32f && t < 2.44f)) shake = ((now / 30) & 1) ? 4 : -4;

  // ---------------- background + vertical speed lines
  if (impactFrame) {
    f.fill(0, f.oy, SW, STRIP, c565(0x000000));
  } else {
    f.fill(0, f.oy, SW, STRIP, c565(0x1C0C36));
    f.ditherRect(0, f.oy, SW, STRIP, c565(0x2A1450));
  }
  float v = catVel(t);
  bool calm = t >= T_IMPACT;  // lines stop once it has landed
  float scroll = linePos(t);  // lines run against the motion: rising cat -> lines fall
  for (int i = 0; i < 44; i++) {
    uint32_t h = hash(i * 2654435761u);
    int x = (int)(h % SW) & ~1;
    int len = 30 + (int)((h >> 8) % 110);
    float base = (float)((h >> 16) % (SH + 200));
    int y = (int)fmodf(base + scroll + 40000, SH + 200) - 100;
    uint32_t col = impactFrame ? 0xFFFFFF : (i % 4 == 0 ? 0xFFFFFF : (i & 1 ? GOLD : 0x8A5CF0));
    int w = (i % 5 == 0) ? 4 : 2;
    if (calm && !impactFrame) {  // after landing the lines fade to sparse sparkles
      if (i % 3) continue;
      len = 6;
    }
    f.fill(x, y, w, len, c565(col));
  }

  int dy = (int)catDy(t);
  dy &= ~1;

  // ---------------- impact frame: white silhouette on black, radial cracks of light
  if (impactFrame) {
    for (int i = 0; i < 16; i++) {
      float a = i * (float)M_PI / 8 + 0.2f;
      f.triangle(IMPACT_X, IMPACT_Y, IMPACT_X + (int)(cosf(a - 0.04f) * 400), IMPACT_Y + (int)(sinf(a - 0.04f) * 400),
                 IMPACT_X + (int)(cosf(a + 0.04f) * 400), IMPACT_Y + (int)(sinf(a + 0.04f) * 400), 0xFFFFFF);
    }
    f.catTint(L[L_DESK], 0, 0, c565(0x000000), 255);
    f.catTint(L[L_BODY], 0, 0, c565(0xFFFFFF), 255);
    f.catTint(L[L_PAWS_LEFT], 0, 0, c565(0xFFFFFF), 255);
    return;
  }

  // ---------------- desk (stays put; shakes and cracks on impact)
  f.cat(L[L_DESK], shake / 2, shake);
  if (sinceImpact > 0) {  // cracks: jagged dark lines out from the fist
    uint16_t crack = c565(0x3A2210);
    for (int k = 0; k < 7; k++) {
      int x = IMPACT_X, y = IMPACT_Y;
      float a = (float)M_PI + k * (float)M_PI / 6 - 0.1f;  // fan out across the desk
      int steps = 6 + (int)(hash(k * 31) % 6);
      int grow = min(steps, (int)(sinceImpact * 60));
      for (int s = 0; s < grow; s++) {
        float jit = ((int)(hash(k * 97 + s) % 100) - 50) / 90.0f;
        x += (int)(cosf(a + jit) * 7);
        y += (int)(sinf(a + jit) * 4);
        if (y < IMPACT_Y - 18) y = IMPACT_Y - 18;
        f.fill((x + shake) & ~1, (y + shake / 2) & ~1, 4, 2, crack);
      }
    }
  }

  // ---------------- the cat
  bool airborne = t >= T_LAUNCH && t < T_IMPACT;
  bool diving = t >= T_DIVE && t < T_IMPACT;
  bool landed = t >= T_IMPACT;
  // afterimages trailing behind the motion
  if (airborne && fabsf(v) > 150) {
    int dir = v < 0 ? 1 : -1;  // rising -> ghosts below
    for (int k = 3; k >= 1; k--) {
      f.catTint(L[L_BODY], 0, dy + dir * k * 22, c565(k == 1 ? GOLD : 0x8A5CF0), (uint8_t)(170 - k * 40));
    }
  }
  // power-up aura while hovering
  if (t >= T_HANG - 0.1f && t < T_IMPACT) {
    uint8_t glow = (uint8_t)(150 + 100 * sinf(t * 24));
    static const int8_t ox[] = {-4, 4, 0, 0, -3, 3, -3, 3}, oy[] = {0, 0, -4, 4, -3, -3, 3, 3};
    for (int k = 0; k < 8; k++) {
      f.catTint(L[L_BODY], ox[k], dy + oy[k], c565(GOLD), glow);
      f.catTint(L[L_PAWS_UP], ox[k], dy + oy[k], c565(GOLD), glow);
    }
  }
  CatLayer face = landed ? (t > 3.3f ? L_FACE_HAPPY : L_FACE_STOCK) : (airborne ? L_FACE_HAPPY : L_FACE_BLINK);
  CatLayer paws = landed && t < 3.3f ? L_PAWS_LEFT : L_PAWS_UP;  // fist planted = superhero landing
  f.cat(L[L_BODY], dy + shake / 2, shake);
  f.cat(L[face], dy + shake / 2, shake);
  f.cat(L[paws], dy + shake / 2, shake);
  if (airborne) energyTrail(f, dy, t, diving);
  if (landed && sinceImpact < 0.5f) f.cat(L[L_FX_LEFT], shake / 2, shake);  // impact lines at the fist

  // ---------------- landing debris + dust
  if (sinceImpact > 0 && sinceImpact < 1.2f) {
    float p = sinceImpact;
    for (int i = 0; i < 26; i++) {  // chunks of desk and keycaps flying out
      uint32_t h = hash(i * 7349 + 11);
      float vx = ((int)(h % 300) - 150) * 1.6f, vy = -(150 + (int)((h >> 9) % 260));
      int x = IMPACT_X + (int)(vx * p), y = IMPACT_Y + (int)(vy * p + 520 * p * p);
      int sz = (h >> 20) & 1 ? 4 : 6;
      uint32_t col = (h >> 22) % 3 == 0 ? 0xCDD0DA : ((h >> 22) % 3 == 1 ? 0xC88A5A : 0xA86E43);
      if (y < SH) f.fill(x & ~1, y & ~1, sz, sz, c565(col));
    }
    for (int side = -1; side <= 1; side += 2) {  // dust clouds rolling out both ways
      for (int k = 0; k < 3; k++) {
        int x = IMPACT_X + side * (int)(30 + p * 120 + k * 22);
        int r = (int)(10 + p * 14 - k * 2);
        if (r > 2 && p < 0.9f) f.disc(x & ~1, (IMPACT_Y - 4 - k * 6) & ~1, r, c565(k ? 0xE8E0F0 : 0xFFFFFF));
      }
    }
    // shockwave along the desk
    if (p < 0.5f) {
      int r = (int)(20 + p * 300);
      f.fill((IMPACT_X - r) & ~1, IMPACT_Y - 2, 2 * r, 4, c565(0xFFFFFF));
    }
  }

  // ---------------- LEVEL UP! above the cat
  float p1 = clamp01((t - 1.95f) / 0.22f), p2 = clamp01((t - 2.15f) / 0.22f);
  if (t > 1.95f) title(f, "LEVEL", SW / 2 + shake, -40 + (int)(easeOutBack(p1) * 92), PX32, PAPER);
  if (t > 2.15f) title(f, "UP!", SW / 2 - shake, -40 + (int)(easeOutBack(p2) * 134), PX32, GOLD);

  // ---------------- banner: level counter, then the name
  if (t > 2.45f) {
    char buf[32];
    if (t < 3.4f) snprintf(buf, sizeof buf, "Lv%d \xE2\x96\xB6 Lv%d", from, t > 2.85f ? to : from);
    else {
      snprintf(buf, sizeof buf, "%s GOT STRONGER!", name && name[0] ? name : "YOUR CAT");
      for (char* c = buf; *c; c++) *c = toupper((unsigned char)*c);
    }
    f.fill(0, 292, SW, 28, c565(INK));
    f.fill(0, 294, SW, 24, c565(PAPER));
    f.text(buf, SW / 2, 306, PX8, t > 2.85f && t < 3.4f ? 0xD08000 : INK, AL_C, SW - 12);
    if (t > 2.85f && t < 3.3f) {  // burst when the number ticks over
      float p = (t - 2.85f) / 0.45f;
      for (int i = 0; i < 12; i++) {
        float a = i * (float)M_PI / 6;
        int r = 14 + (int)(p * 60);
        f.fill((SW / 2 + (int)(cosf(a) * r * 1.8f)) & ~1, (300 + (int)(sinf(a) * r * 0.5f)) & ~1, 4, 4, c565(0xFFF4A0));
      }
    }
  }

  // ---------------- flashes: start, landing, end
  if (t < T_CROUCH) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - t / T_CROUCH)));
  if (sinceImpact >= 0.10f && sinceImpact < 0.30f) f.blendAll(c565(0xFFFFFF), (uint8_t)(230 * (1 - (sinceImpact - 0.10f) / 0.2f)));
  if (t > 4.6f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - 4.6f) / 0.4f)));
}
