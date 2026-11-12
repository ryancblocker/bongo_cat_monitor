#include "levelup_fx.h"

#include <math.h>

static const uint32_t INK = 0x202838, PAPER = 0xF8F8F0, RIM = 0x98B0D0, SUB = 0x687088, GOLD = 0xF8C038;
static const char* const FORMS[] = {"", "A KITTEN!", "A TYPIST!", "A PRO GAMER!", "KEYBOARD KING!"};

void LevelUpFx::start(uint32_t now, int fromLv, int toLv, bool evo) {
  on = true;
  ended = false;
  evolve = evo;
  t0 = now;
  from = fromLv < 1 ? 1 : fromLv;
  to = toLv;
}

bool LevelUpFx::justEnded(uint32_t now) {
  if (on && !ended && now - t0 >= dur()) {
    ended = true;
    on = false;
    return true;
  }
  return false;
}

static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float easeOutBack(float p) {
  const float c1 = 2.0f, c3 = c1 + 1;
  return 1 + c3 * powf(p - 1, 3) + c1 * powf(p - 1, 2);
}
static uint32_t hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}

// handheld-RPG text box: dark outline, blue-grey rim, paper inside, cut corners
static void box(Frame& f, int x, int y, int w, int h) {
  f.fill(x + 2, y, w - 4, h, c565(INK));
  f.fill(x, y + 2, w, h - 4, c565(INK));
  f.fill(x + 4, y + 2, w - 8, h - 4, c565(RIM));
  f.fill(x + 2, y + 4, w - 4, h - 8, c565(RIM));
  f.fill(x + 6, y + 4, w - 12, h - 8, c565(PAPER));
  f.fill(x + 4, y + 6, w - 8, h - 12, c565(PAPER));
}

// text that types itself out at 22 characters a second, starting 'since' seconds ago
static void typed(Frame& f, const char* s, int x, int cy, float since, uint32_t color) {
  if (since <= 0) return;
  char buf[40];
  int n = (int)(since * 22), len = strlen(s);
  if (n > len) n = len;
  if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
  memcpy(buf, s, n);
  buf[n] = 0;
  f.text(buf, x, cy, PX8, color);
}

static void upperName(char* out, size_t n, const char* name) {
  strlcpy(out, name && name[0] ? name : "YOUR CAT", n);
  for (char* c = out; *c; c++) *c = toupper((unsigned char)*c);
}

static void downArrow(Frame& f, int x, int y) {  // "more" cursor, 2 px pixels
  f.fill(x, y, 10, 2, c565(INK));
  f.fill(x + 2, y + 2, 6, 2, c565(INK));
  f.fill(x + 4, y + 4, 2, 2, c565(INK));
}

// ======================================================================= evolution
// Beats: 0.0 fade in, cat on glow | 0.5 "MOCHI IS GLOWING..." types | 1.6 cat turns white |
// 2.0-6.2 flicker old/new form 2 Hz -> ~24 Hz, light streams in, glow brightens |
// 6.05 white-out | 6.6 reveal + sparkle burst | 7.0 "MOCHI BECAME A TYPIST!" | 9.3 fade out
void LevelUpFx::draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name) const {
  float t = (now - t0) / 1000.0f;
  int fromSt = stageFor(from), toSt = stageFor(to);
  const CatImg* oldCat = L[L_STAND1 + fromSt - 1];
  const CatImg* newCat = L[L_STAND1 + toSt - 1];
  const int CX = 120, CY = 150;  // glow centre (behind the cat's body)
  const float T_FLICK = 2.0f, T_FLASH = 6.2f, T_REVEAL = 6.6f;

  // background: concentric teal glow that brightens as the change builds
  float build = clamp01((t - T_FLICK) / (T_FLASH - T_FLICK));
  if (t >= T_REVEAL) build = 0.25f * (1 - clamp01((t - T_REVEAL) / 1.5f));
  static const uint32_t RINGS[] = {0x0C3434, 0x124A48, 0x1C6460, 0x2C807A, 0x50A49A, 0x8CCCC0, 0xC8EEE4};
  static const int RADII[] = {0, 170, 136, 106, 80, 56, 34};
  for (int i = 0; i < 7; i++) {
    uint16_t c = lerp565(c565(RINGS[i]), c565(0xFFFFFF), (uint8_t)(build * build * 170));
    if (i == 0) f.fill(0, f.oy, SW, STRIP, c);
    else f.disc(CX, CY, RADII[i] + ((int)(3 * sinf(t * 3 + i)) & ~1), c);
  }

  if (t < T_FLICK) {
    f.cat(oldCat);
    if (t > 1.6f) f.catTint(oldCat, 0, 0, c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - 1.6f) / 0.4f)));
  } else if (t < T_REVEAL) {
    // flicker between the old and new silhouettes; the frequency ramps up 2 Hz -> 24 Hz
    float u = t - T_FLICK, D = T_FLASH - T_FLICK;
    float phase = 2 * 2 * u + 2 * 22 * u * u * u / (3 * D * D);
    f.catTint(((int)phase & 1) ? newCat : oldCat, 0, 0, c565(0xFFFFFF), 255);
    // specks of light streaming in toward the cat
    for (int i = 0; i < 20; i++) {
      uint32_t h = hash(i * 7919 + 3);
      float a = (h % 628) / 100.0f;
      float cyc = fmodf(u * (0.7f + ((h >> 10) % 10) / 10.0f) + ((h >> 16) % 100) / 100.0f, 1.0f);
      float r = 180 * (1 - cyc);
      int x = CX + (int)(cosf(a) * r), y = CY + (int)(sinf(a) * r * 0.9f);
      int sz = cyc > 0.7f ? 2 : 4;
      f.fill(x & ~1, y & ~1, sz, sz, c565(0xFFFFFF));
    }
  } else {
    f.cat(newCat);
    float p = clamp01((t - T_REVEAL) / 1.3f);  // sparkle burst outward
    if (p < 1) {
      for (int i = 0; i < 16; i++) {
        float a = i * (float)M_PI / 8 + 0.2f;
        int r = 40 + (int)(p * 120);
        int x = (CX + (int)(cosf(a) * r)) & ~1, y = (CY + (int)(sinf(a) * r * 0.85f)) & ~1;
        uint16_t c = c565(i & 1 ? 0xFFFFFF : 0xFFF4A0);
        int s = p < 0.6f ? 2 : 1;  // stars shrink as they fly out
        f.fill(x - 2 * s, y, 2 + 4 * s, 2, c);
        f.fill(x, y - 2 * s, 2, 2 + 4 * s, c);
      }
    }
  }

  // text box
  box(f, 6, 248, 228, 66);
  char nm[20], a[40];
  upperName(nm, sizeof nm, name);
  if (t < T_FLASH) {
    snprintf(a, sizeof a, "%s IS", nm);
    typed(f, a, 20, 270, t - 0.5f, INK);
    typed(f, "GLOWING...!?", 20, 288, t - 0.5f - strlen(a) / 22.0f, INK);
  } else {
    snprintf(a, sizeof a, "%s BECAME", nm);
    typed(f, a, 20, 270, t - 7.0f, INK);
    typed(f, FORMS[toSt], 20, 288, t - 7.0f - strlen(a) / 22.0f, toSt == 4 ? 0xB86800 : INK);
    if (t > 8.4f && ((now / 450) & 1)) downArrow(f, 212, 298);
  }

  // fades and the white-out between the forms
  if (t < 0.4f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - t / 0.4f)));
  else if (t >= T_FLASH - 0.15f && t < T_REVEAL) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - (T_FLASH - 0.15f)) / 0.15f)));
  else if (t >= T_REVEAL && t < T_REVEAL + 0.5f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - (t - T_REVEAL) / 0.5f)));
  else if (t > 9.3f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - 9.3f) / 0.5f)));
}

// ======================================================================= normal level-up
void LevelUpFx::drawOverlay(Frame& f, uint32_t now, const char* name) const {
  float t = (now - t0) / 1000.0f;
  float rise = t < 3.2f ? clamp01(easeOutBack(clamp01(t / 0.3f))) : 1 - clamp01((t - 3.2f) / 0.35f);
  const int bx = 6, bw = 228, bh = 70;
  int by = SH - ((int)(rise * 76) & ~1);
  if (!f.vis(by, bh)) return;
  box(f, bx, by, bw, bh);
  char nm[20], buf[48];
  upperName(nm, sizeof nm, name);
  snprintf(buf, sizeof buf, "%s GREW TO LV %d!", nm, to);
  if (strlen(buf) > 24) snprintf(buf, sizeof buf, "LEVEL UP! NOW LV %d!", to);
  typed(f, buf, bx + 14, by + 18, t - 0.3f, INK);

  // level number rolls over like a slot machine once the EXP bar is full
  float roll = clamp01((t - 1.55f) / 0.28f);
  char a[8], b[8];
  snprintf(a, sizeof a, "%d", from);
  snprintf(b, sizeof b, "%d", to);
  f.text("Lv", bx + 14, by + 46, PX8, INK);
  f.setClip(bx + 32, by + 36, 48, 20);
  int off = (int)(easeOutBack(roll) * 20) & ~1;
  f.text(a, bx + 34, by + 46 - off, PX16, INK);
  f.text(b, bx + 34, by + 66 - off, PX16, 0xC07000);
  f.clearClip();

  float fill = clamp01((t - 1.0f) / 0.55f);
  int ex = bx + 86, ew = bw - 100;
  f.text("EXP", ex, by + 46, PX8, SUB);
  f.fill(ex + 28, by + 42, ew - 28, 8, c565(INK));
  f.fill(ex + 30, by + 44, ew - 32, 4, c565(0x485068));
  bool flashBar = t > 1.55f && t < 1.85f && ((now / 60) & 1);
  f.fill(ex + 30, by + 44, (int)((ew - 32) * fill) & ~1, 4, c565(flashBar ? 0xFFFFFF : 0x58A0F0));

  if (t > 1.55f && t < 2.6f) {  // "+1" floats up out of the box with sparkles
    float q = clamp01((t - 1.55f) / 1.05f);
    int py = by + 46 - ((int)(easeOutBack(clamp01(q * 2.5f)) * 12) & ~1);  // pops just above the row
    if (q > 0.8f && ((now / 60) & 1)) py = -100;  // blinks out
    f.text("+1", bx + 58, py + 2, PX8, INK);
    f.text("+1", bx + 56, py, PX8, GOLD);
    for (int i = 0; i < 8; i++) {
      float ang = i * (float)M_PI / 4;
      int r = 8 + (int)(q * 28);
      f.fill((bx + 46 + (int)(cosf(ang) * r * 1.5f)) & ~1, (by + 46 + (int)(sinf(ang) * r * 0.6f)) & ~1, 2, 2, c565(0xFFF4A0));
    }
  }
}
