#include "onboarding.h"

#include <math.h>

#include "ui.h"

static const uint32_t INK = 0x202838, PAPER = 0xF8F8F0, RIM = 0x98B0D0, SUB = 0x687088, GOLD = 0xF8C038;

static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static uint32_t hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}
static float easeOutBounce(float p) {
  const float n = 7.5625f, d = 2.75f;
  if (p < 1 / d) return n * p * p;
  if (p < 2 / d) return n * (p -= 1.5f / d) * p + 0.75f;
  if (p < 2.5f / d) return n * (p -= 2.25f / d) * p + 0.9375f;
  return n * (p -= 2.625f / d) * p + 0.984375f;
}

// handheld-RPG text box
static void box(Frame& f, int x, int y, int w, int h) {
  f.fill(x + 2, y, w - 4, h, c565(INK));
  f.fill(x, y + 2, w, h - 4, c565(INK));
  f.fill(x + 4, y + 2, w - 8, h - 4, c565(RIM));
  f.fill(x + 2, y + 4, w - 4, h - 8, c565(RIM));
  f.fill(x + 6, y + 4, w - 12, h - 8, c565(PAPER));
  f.fill(x + 4, y + 6, w - 8, h - 12, c565(PAPER));
}

// lines that type themselves out one after another, 24 characters a second
static void typedLines(Frame& f, const char* const* lines, int n, int x, int y, float since) {
  int shown = since > 0 ? (int)(since * 24) : 0;
  for (int i = 0; i < n && shown > 0; i++) {
    char buf[40];
    int len = strlen(lines[i]), k = min(len, min(shown, (int)sizeof(buf) - 1));
    memcpy(buf, lines[i], k);
    buf[k] = 0;
    f.text(buf, x, y + i * 16, PX8, INK);
    shown -= len;
  }
}
static int totalChars(const char* const* lines, int n) {
  int c = 0;
  for (int i = 0; i < n; i++) c += strlen(lines[i]);
  return c;
}
static void moreArrow(Frame& f, int x, int y, uint32_t now) {  // blinking ▼ = tap to go on
  if (!((now / 450) & 1)) return;
  f.fill(x, y, 10, 2, c565(INK));
  f.fill(x + 2, y + 2, 6, 2, c565(INK));
  f.fill(x + 4, y + 4, 2, 2, c565(INK));
}

static void upperName(char* out, size_t n, const char* name) {
  strlcpy(out, name && name[0] ? name : "YOUR CAT", n);
  for (char* c = out; *c; c++) *c = toupper((unsigned char)*c);
}

// ------------------------------------------------------------------ the egg
// cream shell with teal spots, 1 px... well, 2 px pixel-art outline. tilt rocks it around the
// bottom point; cracks = 0..3; white = 0..255 glows it toward white.
static const int8_t SPOTS[][3] = {{-14, 22, 7}, {12, 38, 9}, {-6, 60, 6}, {18, 64, 5}, {-20, 48, 4}, {4, 12, 4}};
static const int8_t CRACKS[3][6][2] = {
    {{6, 16}, {11, 22}, {7, 27}, {13, 32}, {0, 0}, {0, 0}},
    {{-4, 34}, {-10, 39}, {-5, 44}, {-12, 50}, {-8, 55}, {0, 0}},
    {{-22, 42}, {-14, 46}, {-6, 41}, {2, 47}, {10, 42}, {22, 47}},
};

static void drawEgg(Frame& f, int cx, int bottom, int H, float tilt, int cracks, uint8_t white) {
  int W = H * 4 / 5;
  int top = bottom - H;
  if (!f.vis(top - 6, H + 12)) return;
  float sh = tanf(tilt);
  auto hw = [&](float yy) {  // egg profile: two half-ellipses, a taller one on top
    const float yc = H * 0.6f;
    float a = yy < yc ? yc : H - yc;
    float u = (yy - yc) / a;
    return u * u >= 1 ? 0.0f : W / 2.0f * sqrtf(1 - u * u);
  };
  auto ox = [&](float yy) { return (int)roundf((H - yy) * sh); };
  uint16_t shell = lerp565(c565(0xF8F0D8), c565(0xFFFFFF), white);
  uint16_t shade = lerp565(c565(0xE0CCA4), c565(0xFFFFFF), white);
  uint16_t hi = c565(0xFFFFFF);
  uint16_t ink = lerp565(c565(INK), c565(0xFFFFFF), white);
  for (int yy = -2; yy < H + 2; yy += 2) {  // outline: the same shape grown by 2 px
    float w = max(hw(yy + 1.0f), max(hw(yy - 1.0f), hw(yy + 3.0f))) + 2;
    if (w < 3) continue;
    int x0 = (cx + ox(yy) - (int)w) & ~1, x1 = (cx + ox(yy) + (int)w) & ~1;
    f.fill(x0, top + yy, x1 - x0 + 2, 2, ink);
  }
  for (int yy = 2; yy < H; yy += 2) {
    int w = (int)hw(yy);
    if (w < 2) continue;
    int x0 = (cx + ox(yy) - w) & ~1, x1 = (cx + ox(yy) + w) & ~1;
    f.fill(x0, top + yy, x1 - x0, 2, shell);
    int s = (x1 - x0) / 4;  // shaded right quarter
    f.fill((x1 - s) & ~1, top + yy, s & ~1, 2, shade);
  }
  for (auto& s : SPOTS) {  // spots ride along with the tilt
    float yy = s[1] * H / 80.0f;
    int r = max(2, s[2] * H / 80);
    f.disc(cx + ox(yy) + s[0] * H / 80, top + (int)yy, r, lerp565(c565(0x58B8A8), c565(0xFFFFFF), white));
  }
  f.fill((cx + ox(H * 0.2f) - W / 5) & ~1, top + H / 5, 4, 8, hi);  // shine
  f.fill((cx + ox(H * 0.2f) - W / 5 + 4) & ~1, top + H / 5 - 4, 4, 4, hi);
  for (int c = 0; c < cracks && c < 3; c++) {
    for (int i = 0; i + 1 < 6; i++) {
      const int8_t* a = CRACKS[c][i];
      const int8_t* b = CRACKS[c][i + 1];
      if (!b[0] && !b[1]) break;
      float ya = a[1] * H / 80.0f, yb = b[1] * H / 80.0f;
      f.wideLine(cx + ox(ya) + a[0] * H / 80, top + (int)ya, cx + ox(yb) + b[0] * H / 80, top + (int)yb, 1.0f, 0x202838);
    }
  }
}

// ------------------------------------------------------------------ phases
bool Onboarding::hatch(uint32_t now) {
  if (phase != INTRO && phase != WAIT) return false;
  go(HATCH, now);
  return true;
}

bool Onboarding::update(uint32_t now) {
  if (phase == INTRO && now - t0 > INTRO_MS) go(WAIT, now);
  if (phase == HATCH && now - t0 > HATCH_MS) {
    go(TOUR, now);
    step = 0;
    return true;
  }
  return false;
}

bool Onboarding::tap(uint32_t now) {
  if (phase == INTRO) go(WAIT, now);
  else if (phase == TOUR) {
    if (now - t0 < 400) return false;  // don't skip a step by accident
    if (++step >= TOUR_STEPS) {
      phase = OFF;
      return true;
    }
    t0 = now;
  }
  return false;
}

bool Onboarding::animating(uint32_t now) const {
  if (phase != WAIT) return true;
  return (now - t0) % 3000 < 1100;  // the egg wiggles for a second every 3 s
}

// ------------------------------------------------------------------ intro + QR + hatch
static void stars(Frame& f, uint32_t now, int n) {
  for (int i = 0; i < n; i++) {
    uint32_t h = hash(i * 2654435761u + 17);
    int x = (h % 120) * 2, y = ((h >> 8) % 160) * 2;
    if (!f.vis(y - 2, 6)) continue;
    uint32_t tw = (now / 120 + (h >> 20)) % 24;
    uint16_t c = c565(tw < 3 ? 0xFFFFFF : ((h >> 4) & 1 ? 0x98B0D0 : 0x6070A0));
    f.fill(x, y, 2, 2, c);
    if (tw == 1) {  // twinkle: a little plus
      f.fill(x - 2, y, 6, 2, c);
      f.fill(x, y - 2, 2, 6, c);
    }
  }
}

static void bigTitle(Frame& f, const char* s, int cx, int cy, uint32_t color) {
  f.text(s, cx + 3, cy + 3, PX32, INK, AL_C);  // drop shadow, then outline, then the letters
  for (int dx = -2; dx <= 2; dx += 2)
    for (int dy = -2; dy <= 2; dy += 2) f.text(s, cx + dx, cy + dy, PX32, INK, AL_C);
  f.text(s, cx, cy, PX32, color, AL_C);
}

void Onboarding::draw(Frame& f, uint32_t now, const CatImg* const* L, const char* name, const char* url) const {
  float t = (now - t0) / 1000.0f;
  char nm[20];
  upperName(nm, sizeof nm, name);

  if (phase == INTRO) {
    f.fill(0, f.oy, SW, STRIP, c565(0x101830));
    stars(f, now, 70);
    // shooting star
    float s = (t - 0.6f) / 0.9f;
    if (s > 0 && s < 1) {
      int x = -20 + (int)(s * 280), y = 30 + (int)(s * 70);
      for (int k = 0; k < 8; k++) f.fill((x - k * 6) & ~1, (y - k * 2) & ~1, 4, 2, lerp565(c565(0xFFFFFF), c565(0x101830), k * 30));
    }
    // title drops in with a bounce, letter by letter feel: BONGO then CAT
    float a = clamp01((t - 1.4f) / 0.7f), b = clamp01((t - 1.8f) / 0.7f);
    if (a > 0) bigTitle(f, "BONGO", SW / 2, -40 + (int)(easeOutBounce(a) * 106), GOLD);
    if (b > 0) bigTitle(f, "CAT", SW / 2, -40 + (int)(easeOutBounce(b) * 146), 0xF8F8F0);
    // the egg rises into a pulsing glow
    float e = clamp01((t - 2.6f) / 0.9f);
    if (e > 0) {
      int by = 330 - (int)((1 - powf(1 - e, 3)) * 90);
      int pulse = (int)(4 * sinf(t * 4));
      f.disc(SW / 2, by - 34, 54 + pulse, c565(0x1C2A50));
      f.disc(SW / 2, by - 34, 44 + pulse, c565(0x2A3E70));
      f.disc(SW / 2, by - 34, 36 + pulse, c565(0x3C5690));
      float wob = (t > 3.8f && t < 4.6f) ? 0.18f * sinf((t - 3.8f) * 16) * (1 - (t - 3.8f) / 0.8f) : 0;
      drawEgg(f, SW / 2, by, 72, wob, 0, 0);
    }
    if (t > 4.4f) {
      box(f, 6, 254, 228, 60);
      const char* l[] = {"A NEW FRIEND IS", "WAITING FOR YOU..."};
      typedLines(f, l, 2, 20, 276, t - 4.6f);
    }
    if (t < 0.5f) f.blendAll(c565(0x000000), (uint8_t)(255 * (1 - t / 0.5f)));
    if (t > INTRO_MS / 1000.0f - 0.4f) f.blendAll(c565(0x101830), (uint8_t)(255 * clamp01((t - (INTRO_MS / 1000.0f - 0.4f)) / 0.4f)));
    return;
  }

  if (phase == WAIT) {
    f.fill(0, f.oy, SW, STRIP, c565(0x101830));
    stars(f, now, 40);
    float w = ((now - t0) % 3000) / 1000.0f;  // a wiggle every 3 s
    float wob = w < 1.0f ? 0.16f * sinf(w * 18) * (1 - w) : 0;
    f.disc(SW / 2, 62, 34, c565(0x1C2A50));
    f.disc(SW / 2, 62, 26, c565(0x2A3E70));
    drawEgg(f, SW / 2, 96, 64, wob, 0, 0);
    if (url && url[0]) {
      box(f, 50, 106, 140, 140);
      f.qr(url, 60, 116, 120);
    } else {
      box(f, 50, 106, 140, 140);
      f.text("?", SW / 2, 176, PX32, SUB, AL_C);
    }
    box(f, 6, 254, 228, 60);
    if (url && url[0]) {
      f.text("SCAN WITH YOUR PHONE", 20, 276, PX8, INK);
      f.text("TO CHOOSE YOUR CAT!", 20, 292, PX8, INK);
    } else {
      f.text("PLUG ME INTO YOUR PC", 20, 276, PX8, INK);
      f.text("AND OPEN BONGO CAT.", 20, 292, PX8, INK);
    }
    return;
  }

  // ---- HATCH
  const float C1 = 2.0f, C2 = 3.7f, C3 = 5.3f, GLOW = 5.6f, FLASH = 6.5f, REVEAL = 6.8f;
  static const uint32_t RINGS[] = {0x203060, 0x2C4078, 0x3C5490, 0x5470A8, 0x7894C4, 0xA8C0E0};
  static const int RADII[] = {0, 150, 118, 90, 66, 44};
  float glow = clamp01((t - GLOW) / (FLASH - GLOW));
  for (int i = 0; i < 6; i++) {
    uint16_t c = lerp565(c565(RINGS[i]), c565(0xFFFFFF), (uint8_t)(glow * glow * 200));
    if (i == 0) f.fill(0, f.oy, SW, STRIP, c);
    else f.disc(SW / 2, 160, RADII[i] + ((int)(3 * sinf(t * 3 + i)) & ~1), c);
  }
  const int EGG_BOTTOM = 214, EGG_H = 96;
  if (t < REVEAL) {
    // rocking: three bursts, each bigger and faster, a crack at the end of each
    float wob = 0;
    struct { float a, b, amp, hz; } R[] = {{0.8f, C1, 0.12f, 9}, {2.5f, C2, 0.2f, 13}, {4.2f, C3, 0.28f, 18}};
    for (auto& r : R)
      if (t > r.a && t < r.b) wob = r.amp * sinf((t - r.a) * r.hz) * sinf((float)M_PI * (t - r.a) / (r.b - r.a));
    if (t > GLOW) wob = 0.05f * sinf(t * 60);  // shaking with energy
    int cracks = (t > C1) + (t > C2) + (t > C3);
    drawEgg(f, SW / 2, EGG_BOTTOM, EGG_H, wob, cracks, (uint8_t)(glow * 255));
    // a few chips pop off at each crack
    for (float ct : {C1, C2, C3}) {
      float p = (t - ct) / 0.5f;
      if (p <= 0 || p >= 1) continue;
      for (int i = 0; i < 4; i++) {
        uint32_t h = hash((int)(ct * 10) * 31 + i);
        float vx = ((int)(h % 60) - 30) * 1.0f, vy = -40.0f - (h >> 8) % 40;
        int x = SW / 2 + (int)(vx * p), y = EGG_BOTTOM - 60 + (int)(vy * p + 120 * p * p);
        f.fill(x & ~1, y & ~1, 4, 4, c565(0xF8F0D8));
      }
    }
    if (t > GLOW) {  // light rays turning behind the egg
      for (int i = 0; i < 8; i++) {
        float a = t * 1.5f + i * (float)M_PI / 4;
        f.wideLine(SW / 2, 160, SW / 2 + (int)(cosf(a) * 140), 160 + (int)(sinf(a) * 140), 1.5f, 0xFFFFFF);
      }
      drawEgg(f, SW / 2, EGG_BOTTOM, EGG_H, 0.05f * sinf(t * 60), 3, (uint8_t)(glow * 255));
    }
  } else {
    // the cat! shell halves fall away, sparkles, hearts
    const CatImg* cat = L[L_STAND1];
    float hop = t - REVEAL < 0.5f ? sinf((float)M_PI * (t - REVEAL) / 0.5f) : 0;
    f.cat(cat, -(int)(hop * 14) & ~1);
    float p = clamp01((t - REVEAL) / 1.4f);
    if (p < 1) {
      for (int i = 0; i < 10; i++) {  // shell chips bursting out and falling
        uint32_t h = hash(i * 97 + 5);
        float vx = ((int)(h % 200) - 100) * 1.4f, vy = -90.0f - (h >> 8) % 90;
        int x = SW / 2 + (int)(vx * p), y = 150 + (int)(vy * p + 260 * p * p);
        int sz = 6 + (h >> 16) % 3 * 2;
        f.fill(x & ~1, y & ~1, sz + 4, sz + 4, c565(INK));
        f.fill((x + 2) & ~1, (y + 2) & ~1, sz, sz, c565(i % 4 == 0 ? 0x58B8A8 : 0xF8F0D8));
      }
    }
    float q = clamp01((t - REVEAL) / 1.3f);
    if (q < 1) {
      for (int i = 0; i < 14; i++) {
        float a = i * (float)M_PI / 7 + 0.3f;
        int r = 50 + (int)(q * 110);
        int x = (SW / 2 + (int)(cosf(a) * r)) & ~1, y = (150 + (int)(sinf(a) * r * 0.85f)) & ~1;
        uint16_t c = c565(i % 3 == 0 ? 0xF8C038 : 0xFFFFFF);
        f.fill(x - 4, y, 10, 2, c);
        f.fill(x, y - 4, 2, 10, c);
      }
    }
    for (int i = 0; i < 3; i++) {  // hearts floating up
      float hp = (t - REVEAL - 0.8f - i * 0.5f) / 2.0f;
      if (hp <= 0 || hp >= 1) continue;
      int x = SW / 2 + (i - 1) * 50 + (int)(6 * sinf(hp * 9)), y = 120 - (int)(hp * 70);
      f.text("\xE2\x99\xA5", x, y, PX16, 0xE84858, AL_C);
    }
  }
  // text box
  box(f, 6, 248, 228, 66);
  if (t < FLASH) {
    const char* l[] = {"OH? THE EGG IS", "MOVING!"};
    typedLines(f, l, 2, 20, 270, t - 0.4f);
  } else {
    char a[40], b[40];
    snprintf(a, sizeof a, "%s HATCHED!", nm);
    snprintf(b, sizeof b, "SAY HI TO YOUR NEW PAL!");
    const char* l[] = {a, b};
    typedLines(f, l, 2, 20, 270, t - 6.9f);
    if (t > 6.9f + totalChars(l, 2) / 24.0f + 0.3f) moreArrow(f, 214, 300, now);
  }
  // flash between the egg and the cat, fade into the tour at the end
  if (t >= FLASH - 0.2f && t < REVEAL) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - (FLASH - 0.2f)) / 0.2f)));
  else if (t >= REVEAL && t < REVEAL + 0.6f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * (1 - (t - REVEAL) / 0.6f)));
  else if (t > HATCH_MS / 1000.0f - 0.5f) f.blendAll(c565(0xFFFFFF), (uint8_t)(255 * clamp01((t - (HATCH_MS / 1000.0f - 0.5f)) / 0.5f)));
}

// ------------------------------------------------------------------ the tour
struct TourStep {
  int8_t card;           // card to show (-1 = leave it)
  int16_t x, y, w, h;    // spotlight (w = 0: none)
  bool boxTop;           // text box at the top (the spotlight is low)
  const char* lines[3];  // "%s" = the cat's name
};
static const TourStep TOUR_TEXT[Onboarding::TOUR_STEPS] = {
    {-1, 0, 0, 0, 0, false, {"HI! I'M %s!", "LET ME SHOW YOU", "AROUND. TAP TO GO ON."}},
    {-1, 0, 156, 240, 164, true, {"WHEN YOU TYPE ON YOUR", "PC, I TYPE TOO! TYPE", "FAST TO MAKE ME HAPPY."}},
    {-1, 2, 118, 170, 44, false, {"THESE HEARTS ARE MY", "MOOD. TYPING FILLS", "THEM BACK UP."}},
    {CARD_WEATHER, CARD_X - 4, CARD_Y - 4, CARD_W + 8, CARD_H + 8, false, {"TAP THE RIGHT HALF OF", "THIS CARD TO GO ON,", "THE LEFT HALF TO GO BACK."}},
    {CARD_FOCUS, CARD_X - 4, CARD_Y - 4, CARD_W + 8, CARD_H + 8, false, {"PRESS START TO FOCUS.", "FINISH AND I LEVEL UP.", "QUIT AND I GET SAD..."}},
    {CARD_FOCUS, CARD_X - 4, CARD_Y - 4, CARD_W + 8, CARD_H + 8, false, {"AT LV 5, 15 AND 30 I", "EVOLVE! NEW GEAR SHOWS", "UP IN YOUR WARDROBE."}},
    {-1, 30, 150, 200, 110, true, {"TAP ME TO PET ME!", "IF YOU STOP TYPING FOR", "A WHILE, I TAKE A NAP."}},
    {-1, GEAR_X - 6, 0, 30, 24, false, {"TAP THE GEAR FOR", "BRIGHTNESS AND THE CODE", "FOR YOUR PHONE PAGE."}},
    {-1, 0, 0, 0, 0, false, {"THAT'S EVERYTHING!", "LET'S GET TO WORK,", "PARTNER!"}},
};

int Onboarding::wantCard() const { return phase == TOUR ? TOUR_TEXT[step].card : -1; }

void Onboarding::drawTour(Frame& f, uint32_t now, const char* name) const {
  if (phase != TOUR) return;
  const TourStep& s = TOUR_TEXT[step];
  float t = (now - t0) / 1000.0f;
  uint16_t dim = c565(0x101830);
  if (s.w) {  // darken everything but the spotlight, gold frame blinking around it
    f.blendRect(0, 0, SW, s.y, dim, 150);
    f.blendRect(0, s.y + s.h, SW, SH - s.y - s.h, dim, 150);
    f.blendRect(0, s.y, s.x, s.h, dim, 150);
    f.blendRect(s.x + s.w, s.y, SW - s.x - s.w, s.h, dim, 150);
    uint16_t g = c565(((now / 300) & 1) ? 0xF8C038 : 0xFFF0A0);
    f.fill(s.x, s.y, s.w, 2, g);
    f.fill(s.x, s.y + s.h - 2, s.w, 2, g);
    f.fill(s.x, s.y, 2, s.h, g);
    f.fill(s.x + s.w - 2, s.y, 2, s.h, g);
  } else {
    f.blendAll(dim, 110);
  }
  int by = s.boxTop ? 6 : 248;
  box(f, 6, by, 228, 66);
  char nm[20], buf[3][40];
  upperName(nm, sizeof nm, name);
  const char* l[3];
  for (int i = 0; i < 3; i++) {
    snprintf(buf[i], sizeof buf[i], s.lines[i], nm);
    l[i] = buf[i];
  }
  typedLines(f, l, 3, 20, by + 18, t - 0.15f);
  if (t > 0.15f + totalChars(l, 3) / 24.0f) moreArrow(f, 216, by + 57, now);
  // step dots
  for (int i = 0; i < TOUR_STEPS; i++) f.fill(SW / 2 - TOUR_STEPS * 4 + i * 8, by + 60, 4, 2, c565(i == step ? GOLD : RIM));
}
