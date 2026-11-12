#include "levelup_fx.h"

#include <math.h>

static const uint32_t INK = 0x202838, GOLD = 0xF8C038, ORANGE = 0xF09830, PAPER = 0xF8F8F0;
static const int CX = 120, CY = 140;  // burst centre (behind the cat's head)

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
static float easeOutBack(float p) {  // overshoot then settle: the "slam"
  const float c1 = 2.2f, c3 = c1 + 1;
  return 1 + c3 * powf(p - 1, 3) + c1 * powf(p - 1, 2);
}
static uint32_t hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}

// text with a thick ink outline and a gold drop shadow, like an anime title card
static void title(Frame& f, const char* s, int x, int cy, const uint8_t* font, uint32_t fill) {
  const int o = font == PX32 ? 3 : 2;
  f.text(s, x + o + 1, cy + o + 1, font, ORANGE, AL_C);
  for (int dy = -o; dy <= o; dy += o)
    for (int dx = -o; dx <= o; dx += o)
      if (dx || dy) f.text(s, x + dx, cy + dy, font, INK, AL_C);
  f.text(s, x, cy, font, fill, AL_C);
}

void LevelUpFx::draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name) const {
  float t = (now - t0) / 1000.0f;

  // background: deep purple with a lighter core
  f.fill(0, f.oy, SW, STRIP, c565(0x241040));
  f.disc(CX, CY, 150, c565(0x2E1658));
  f.disc(CX, CY, 100, c565(0x3A1C70));

  // rotating speed lines (thin wedges, alternating gold / orange)
  const int N = 28;
  float rot = t * 0.35f;
  float pulse = 1 + 0.08f * sinf(t * 18);  // jittery, like hand-drawn focus lines
  for (int i = 0; i < N; i++) {
    float a = rot + i * 2 * (float)M_PI / N;
    float w = (i % 3 == 0 ? 0.05f : 0.025f) * pulse;
    float r0 = 46 + (hash(i + (uint32_t)(t * 12)) % 18), r1 = 420;
    int x0 = CX + (int)(cosf(a) * r0), y0 = CY + (int)(sinf(a) * r0);
    f.triangle(x0, y0, CX + (int)(cosf(a - w) * r1), CY + (int)(sinf(a - w) * r1),
               CX + (int)(cosf(a + w) * r1), CY + (int)(sinf(a + w) * r1), i & 1 ? ORANGE : GOLD);
  }
  f.disc(CX, CY, 44, c565(0x4A2490));

  // screen shake on each slam
  int shake = 0;
  if ((t > 0.70f && t < 0.95f) || (t > 0.95f && t < 1.20f) || (t > 2.0f && t < 2.2f)) shake = ((now / 33) & 1) ? 3 : -3;

  // shockwave rings
  for (float start : {0.55f, 2.0f}) {
    float p = (t - start) / 0.6f;
    if (p > 0 && p < 1) {
      int r = 20 + (int)(p * 170);
      f.ring(CX, CY, r, 6, p < 0.5f ? 0xFFFFFF : GOLD);
    }
  }

  // the cat rises into place with a golden aura
  float rise = clamp01((t - 0.2f) / 0.6f);
  int dy = -52 + (int)((1 - easeOutBack(rise)) * 70) + (int)(sinf(t * 4) * 3) + shake;
  uint8_t glow = (uint8_t)(150 + 90 * sinf(t * 9));
  const CatLayer body[] = {L_BODY, L_PAWS_UP};
  for (CatLayer l : body)
    for (int k = 0; k < 8; k++) {
      static const int8_t ox[] = {-4, 4, 0, 0, -3, 3, -3, 3}, oy[] = {0, 0, -4, 4, -3, -3, 3, 3};
      f.catTint(L[l], ox[k] + shake, dy + oy[k], c565(GOLD), glow);
    }
  f.cat(L[L_BODY], dy, shake);
  f.cat(L[L_FACE_HAPPY], dy, shake);
  f.cat(L[L_PAWS_UP], dy, shake);
  if (t > 2.0f && t < 2.25f) {  // power-up flash on the cat when the level ticks over
    f.catTint(L[L_BODY], shake, dy, c565(0xFFFFFF), 200);
  }

  // energy sparks streaming upward
  for (int i = 0; i < 36; i++) {
    uint32_t h = hash(i * 7919);
    float speed = 60 + (h % 90);
    int x = ((h >> 8) % SW) & ~1;
    int y = (int)(SH + 20 - fmodf(t * speed + (h >> 16) % SH, SH + 40)) & ~1;
    int sz = (h & 1) ? 4 : 2;
    f.fill(x, y, sz, sz, c565((h >> 4) & 1 ? 0xFFF4A0 : GOLD));
    if (sz == 4) f.fill(x + 1, y + 4, 2, 4, c565(ORANGE));  // little trail
  }

  // "LEVEL" slams down, "UP!" slams up
  float p1 = clamp01((t - 0.70f) / 0.28f), p2 = clamp01((t - 0.95f) / 0.28f);
  if (t > 0.70f) title(f, "LEVEL", SW / 2 + shake, -30 + (int)(easeOutBack(p1) * 82), PX32, PAPER);
  if (t > 0.95f) title(f, "UP!", SW / 2 - shake, SH + 30 - (int)(easeOutBack(p2) * 114), PX32, GOLD);  // lands at y 236

  // level counter ticks over at 2.0 s
  if (t > 1.4f) {
    char buf[24];
    snprintf(buf, sizeof buf, "Lv%d \xE2\x96\xB6 Lv%d", from, t > 2.0f ? to : from);
    int y = 274;
    f.fill(40, y - 12, 160, 24, c565(INK));
    f.fill(42, y - 10, 156, 20, c565(0x3A1C70));
    f.text(buf, SW / 2, y, PX8, t > 2.0f ? GOLD : PAPER, AL_C);
    if (t > 2.0f && t < 2.6f) {  // sparkle burst around the counter
      float p = (t - 2.0f) / 0.6f;
      for (int i = 0; i < 12; i++) {
        float a = i * (float)M_PI / 6;
        int r = 20 + (int)(p * 70);
        f.fill((SW / 2 + (int)(cosf(a) * r * 1.6f)) & ~1, (y + (int)(sinf(a) * r * 0.6f)) & ~1, 4, 4, c565(0xFFF4A0));
      }
    }
  }
  // name banner
  if (t > 2.6f) {
    char buf[32];
    snprintf(buf, sizeof buf, "%s GOT STRONGER!", name && name[0] ? name : "YOUR CAT");
    for (char* c = buf; *c; c++) *c = toupper((unsigned char)*c);
    f.fill(0, 292, SW, 28, c565(INK));
    f.fill(0, 294, SW, 24, c565(PAPER));
    f.text(buf, SW / 2, 306, PX8, INK, AL_C, SW - 12);
  }

  // impact flash at the start, flash out at the end
  if (t < 0.18f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - t / 0.18f)));
  if (t > 4.25f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - 4.25f) / 0.35f)));
}
