// Anime LEVEL UP with the full-body cat, about 5.4 s. Beats:
//   0.00 flash
//   0.12 RISE      the cat rockets up from below the screen in its flying pose (fist up),
//                  riding a thrust flare, afterimages trailing; vertical speed lines stream down
//   0.80 PEAK      hovers, trembling, golden aura
//   1.30 FALL      snaps into the landing pose and drops fist-first; the lines reverse
//   1.62 IMPACT    punch-slam into the ground: impact frame (black + white silhouette + light
//                  rays), flash, heavy shake, the ground cracks open beneath it, rubble, dust
//   1.90 "LEVEL" slams in, 2.10 "UP!" slams in
//   2.40 the level box rises: EXP bar fills, flashes, the number rolls over, "+1" pops
//   3.40 name line        4.95 flash out
#include "levelup_fx.h"

#include <math.h>

static const uint32_t INK = 0x202838, GOLD = 0xF8C038, ORANGE = 0xF09830, PAPER = 0xF8F8F0, RIM = 0x98B0D0;
static const float T_RISE = 0.12f, T_PEAK = 0.80f, T_FALL = 1.30f, T_IMPACT = 1.62f;
static const int GROUND_Y = 268;                  // the landing pose's feet stand here
static const int PAW_X = 60;                       // where the fist hits
static const int FLY_START = 290, FLY_PEAK = -14;  // flying pose: off the bottom -> whole cat in view
static const int FALL_FROM = -200;                 // landing pose starts its drop up here

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
static float easeInCubic(float p) { return p * p * p; }
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

// speed-line scroll (+ = down): integral of a piecewise speed so the lines accelerate, slow
// and reverse smoothly instead of jumping
static float linePos(float t) {
  struct Seg { float a, b, v; };
  static const Seg segs[] = {{0, T_RISE, 80}, {T_RISE, T_PEAK, 1500}, {T_PEAK, T_FALL, 160}, {T_FALL, T_IMPACT, -2200}};
  float pos = 0;
  for (const Seg& s : segs) pos += s.v * (min(t, s.b) - s.a > 0 ? min(t, s.b) - s.a : 0);
  return pos;
}

// cracks: jagged lines racing out from the fist through the ground; 'grow' 0..1
static void cracks(Frame& f, float grow, int shake) {
  uint16_t dark = c565(0x120A1E), lit = c565(0xF8C038);
  for (int k = 0; k < 10; k++) {
    int x = PAW_X, y = GROUND_Y + 2;
    float a = (k < 5 ? 0.15f : (float)M_PI - 0.15f) + (k % 5) * (k < 5 ? 0.30f : -0.30f);  // fan both ways, downward
    int steps = 9 + (int)(hash(k * 31) % 9), n = (int)(steps * grow);
    for (int s = 0; s < n; s++) {
      float jit = ((int)(hash(k * 97 + s) % 100) - 50) / 60.0f;
      int nx = x + (int)(cosf(a + jit) * 10), ny = y + (int)(fabsf(sinf(a + jit)) * 6) + 1;
      int len = max(abs(nx - x), abs(ny - y)) / 2 + 1;
      for (int i = 0; i <= len; i++) {
        int px = (x + (nx - x) * i / len + shake) & ~1, py = (y + (ny - y) * i / len + shake / 2) & ~1;
        f.fill(px, py, 4, 2, dark);
        if (grow < 0.6f && s < 3) f.fill(px, py, 2, 2, lit);  // glowing right at the start
      }
      x = nx, y = ny;
      if (y > SH) break;
    }
  }
  // crater under the fist
  f.fill((PAW_X - 22 + shake) & ~1, GROUND_Y, 44, 4, dark);
  f.fill((PAW_X - 14 + shake) & ~1, GROUND_Y + 4, 28, 4, dark);
}

void LevelUpFx::draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name) const {
  float t = (now - t0) / 1000.0f;
  float since = t - T_IMPACT;
  bool impactFrame = since >= 0 && since < 0.10f;
  bool rising = t >= T_RISE && t < T_FALL;
  bool falling = t >= T_FALL && t < T_IMPACT;
  bool landed = t >= T_IMPACT;

  int shake = 0;
  if (since > 0.10f && since < 0.85f) {
    int amp = (int)(12 * (1 - (since - 0.10f) / 0.75f));
    shake = (((now / 30) & 1) ? amp : -amp) & ~1;
  }
  if ((t > 2.06f && t < 2.18f) || (t > 2.26f && t < 2.38f)) shake = ((now / 30) & 1) ? 4 : -4;

  // ---------------- impact frame: black, white light rays from the fist, white silhouette
  if (impactFrame) {
    f.fill(0, f.oy, SW, STRIP, c565(0x000000));
    for (int i = 0; i < 20; i++) {
      float a = (float)M_PI + i * (float)M_PI / 19;
      f.triangle(PAW_X, GROUND_Y, PAW_X + (int)(cosf(a - 0.03f) * 420), GROUND_Y + (int)(sinf(a - 0.03f) * 420),
                 PAW_X + (int)(cosf(a + 0.03f) * 420), GROUND_Y + (int)(sinf(a + 0.03f) * 420), 0xFFFFFF);
    }
    f.fill(0, GROUND_Y, SW, 2, c565(0xFFFFFF));
    f.catTint(L[L_HERO_LAND], 0, 0, c565(0xFFFFFF), 255);
    return;
  }

  // ---------------- sky + vertical anime speed lines
  f.fill(0, f.oy, SW, STRIP, c565(0x1C0C36));
  f.ditherRect(0, f.oy, SW, STRIP, c565(0x2A1450));
  float scroll = linePos(t);
  for (int i = 0; i < 48; i++) {
    uint32_t h = hash(i * 2654435761u);
    int x = (int)(h % SW) & ~1;
    int len = 30 + (int)((h >> 8) % 130);
    int y = (int)fmodf((float)((h >> 16) % (SH + 220)) + scroll + 40000, SH + 220) - 110;
    uint32_t col = i % 4 == 0 ? 0xFFFFFF : (i & 1 ? GOLD : 0x8A5CF0);
    int w = (i % 5 == 0) ? 4 : 2;
    if (landed) {  // after the slam the lines settle into sparse sparkles
      if (i % 3) continue;
      len = 6;
    }
    f.fill(x, y, w, len, c565(col));
  }

  // ---------------- ground
  if (f.vis(GROUND_Y, SH - GROUND_Y)) {
    f.fill(0, GROUND_Y + shake / 2, SW, SH - GROUND_Y, c565(0x3A3050));
    f.ditherRect(0, GROUND_Y + 6 + shake / 2, SW, SH - GROUND_Y, c565(0x302744));
    f.fill(0, GROUND_Y + shake / 2, SW, 2, c565(0x6A5C8C));
    if (landed) cracks(f, clamp01(since / 0.35f), shake);
  }

  // ---------------- the cat
  if (rising) {
    float p = clamp01((t - T_RISE) / (T_PEAK - T_RISE));
    int dy = (int)(FLY_START + (FLY_PEAK - FLY_START) * easeOutCubic(p));
    if (t >= T_PEAK) dy = FLY_PEAK + (int)(3 * sinf((t - T_PEAK) * 32));  // trembling with power
    dy &= ~1;
    bool fast = p < 0.85f;
    if (fast)
      for (int k = 3; k >= 1; k--)  // afterimages below
        f.catTint(L[L_HERO_FLY], 0, dy + k * 28, c565(k == 1 ? GOLD : 0x8A5CF0), (uint8_t)(170 - k * 40));
    if (t >= T_PEAK - 0.12f) {  // golden aura while it powers up
      uint8_t glow = (uint8_t)(150 + 100 * sinf(t * 26));
      static const int8_t ox[] = {-4, 4, 0, 0, -3, 3, -3, 3}, oy[] = {0, 0, -4, 4, -3, -3, 3, 3};
      for (int k = 0; k < 8; k++) f.catTint(L[L_HERO_FLY], ox[k], dy + oy[k], c565(GOLD), glow);
    }
    // thrust flare from the feet (screen x ~96..176, feet at y 270 + dy)
    uint32_t flick = (uint32_t)(t * 30);
    for (int x = 96; x < 176; x += 2) {
      uint32_t h = hash(x * 131 + flick);
      int len = 12 + (int)(h % (fast ? 64 : 26));
      int y0 = 268 + dy, a = len / 3;
      f.fill(x, y0, 2, a, c565(0xFFF4C0));
      f.fill(x, y0 + a, 2, a, c565(GOLD));
      f.fill(x, y0 + 2 * a, 2, a, c565(ORANGE));
    }
    f.cat(L[L_HERO_FLY], dy);
  } else {
    int dy = 0;
    if (falling) {
      dy = (int)(FALL_FROM * (1 - easeInCubic((t - T_FALL) / (T_IMPACT - T_FALL)))) & ~1;
      for (int k = 3; k >= 1; k--)  // afterimages above
        f.catTint(L[L_HERO_LAND], 0, dy - k * 30, c565(k == 1 ? GOLD : 0x8A5CF0), (uint8_t)(170 - k * 40));
    }
    if (t < T_RISE) dy = 0;
    f.cat(L[L_HERO_LAND], dy + shake / 2, shake);
  }

  // ---------------- rubble + dust + shockwave after the slam
  if (landed && since < 1.3f) {
    float p = since;
    for (int i = 0; i < 30; i++) {
      uint32_t h = hash(i * 7349 + 11);
      float vx = ((int)(h % 300) - 150) * 1.4f, vy = -(180 + (int)((h >> 9) % 300));
      int x = PAW_X + 20 + (int)(vx * p), y = GROUND_Y + (int)(vy * p + 600 * p * p);
      int sz = (h >> 20) & 1 ? 4 : 6;
      if (y < GROUND_Y + 4) f.fill(x & ~1, y & ~1, sz, sz, c565((h >> 22) & 1 ? 0x6A5C8C : 0x8A7CAC));
    }
    for (int side = -1; side <= 1; side += 2)
      for (int k = 0; k < 4; k++) {
        int x = PAW_X + 30 + side * (int)(30 + p * 120 + k * 22);
        int r = (int)(12 + p * 18 - k * 3);
        if (r > 2 && p < 1.0f) f.disc(x & ~1, (GROUND_Y - 6 - k * 4) & ~1, r, c565(k ? 0xD8CCE8 : 0xF0E8F8));
      }
    if (p < 0.45f) {
      int r = (int)(20 + p * 360);
      f.fill(max(0, PAW_X - r) & ~1, GROUND_Y - 2, min(SW, 2 * r), 4, c565(0xFFFFFF));
    }
  }

  // ---------------- LEVEL UP! above the cat
  float p1 = clamp01((t - 1.90f) / 0.22f), p2 = clamp01((t - 2.10f) / 0.22f);
  if (t > 1.90f) title(f, "LEVEL", SW / 2 + shake, -40 + (int)(easeOutBack(p1) * 64), PX32, PAPER);
  if (t > 2.10f) title(f, "UP!", SW / 2 - shake, -40 + (int)(easeOutBack(p2) * 96), PX32, GOLD);

  // ---------------- the level box: rises, EXP fills, flashes, the number rolls over
  if (t > 2.40f) {
    float rise = easeOutBack(clamp01((t - 2.40f) / 0.25f));
    int by = SH - (int)(rise * 42), bx = 8, bw = SW - 16, bh = 40;
    // Pokémon text box
    f.fill(bx + 2, by, bw - 4, bh, c565(INK));
    f.fill(bx, by + 2, bw, bh - 4, c565(INK));
    f.fill(bx + 4, by + 2, bw - 8, bh - 4, c565(RIM));
    f.fill(bx + 2, by + 4, bw - 4, bh - 8, c565(RIM));
    f.fill(bx + 6, by + 4, bw - 12, bh - 8, c565(PAPER));
    f.fill(bx + 4, by + 6, bw - 8, bh - 12, c565(PAPER));
    // level number, rolling from old to new like a slot machine at 2.95 s
    f.text("Lv", bx + 14, by + 14, PX8, INK);
    float roll = clamp01((t - 2.95f) / 0.28f);
    char a[8], b[8];
    snprintf(a, sizeof a, "%d", from);
    snprintf(b, sizeof b, "%d", to);
    f.setClip(bx + 30, by + 4, 48, 20);
    int off = (int)(easeOutBack(roll) * 20) & ~1;
    f.text(a, bx + 32, by + 14 - off, PX16, INK);
    f.text(b, bx + 32, by + 34 - off, PX16, roll > 0 ? 0xD08000 : INK);
    f.clearClip();
    // EXP bar fills, then flashes white when it tops out
    float fill = clamp01((t - 2.55f) / 0.40f);
    int ex = bx + 80, ew = bw - 96, fw = (int)((ew - 4) * fill) & ~1;
    f.text("EXP", ex, by + 14, PX8, 0x687088);
    f.fill(ex + 28, by + 10, ew - 28, 8, c565(INK));
    f.fill(ex + 30, by + 12, ew - 32, 4, c565(0x485068));
    bool flashBar = t > 2.95f && t < 3.15f && ((now / 50) & 1);
    f.fill(ex + 30, by + 12, (int)((ew - 32) * fill) & ~1, 4, c565(flashBar ? 0xFFFFFF : 0x58A0F0));
    (void)fw;
    // "+1" pops up out of the box when the number changes
    if (t > 2.95f && t < 3.8f) {
      float q = clamp01((t - 2.95f) / 0.85f);
      title(f, "+1", bx + 60, by - 6 - (int)(q * 26), PX16, GOLD);
      for (int i = 0; i < 10; i++) {  // sparkle burst around the number
        float ang = i * (float)M_PI / 5;
        int r = 10 + (int)(q * 44);
        f.fill((bx + 44 + (int)(cosf(ang) * r * 1.6f)) & ~1, (by + 14 + (int)(sinf(ang) * r * 0.6f)) & ~1, 4, 4, c565(0xFFF4A0));
      }
    }
    // second line: who levelled up
    if (t > 3.4f) {
      char buf[32];
      snprintf(buf, sizeof buf, "%s GOT STRONGER!", name && name[0] ? name : "YOUR CAT");
      for (char* c = buf; *c; c++) *c = toupper((unsigned char)*c);
      f.text(buf, SW / 2, by + 29, PX8, INK, AL_C, bw - 20);
    }
  }

  // ---------------- flashes: start, slam, end
  if (t < T_RISE) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - t / T_RISE)));
  if (since >= 0.10f && since < 0.30f) f.blendAll(c565(0xFFFFFF), (uint8_t)(230 * (1 - (since - 0.10f) / 0.2f)));
  if (t > 4.95f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - 4.95f) / 0.45f)));
}
