// Game Boy Color style UI: everything on a 2 px grid, Press Start 2P text,
// Pokémon-style text boxes (ink frame, light rim, notched corners, blinking ▼),
// HP-bar stats and Tamagotchi mood hearts.
#include "ui.h"

#include <math.h>

#include "levelup_fx.h"
#include "state.h"

// palette
static const uint32_t INK = 0x202838, PAPER = 0xF8F8F0, RIM = 0x98B0D0, SUB = 0x687088;
static const uint32_t HDR = 0x283050, HDR_LINE = 0x181C30, GOLD = 0xF8C038;
static const char* const DEG = "\xC2\xB0";
static const char* const HEART = "\xE2\x99\xA5";  // ♥
static const char* const LEFT = "\xE2\x97\x80";   // ◀
static const char* const RIGHT = "\xE2\x96\xB6";  // ▶

void timeString(char* out, size_t n) {
  if (!clk.valid) {
    snprintf(out, n, "--:--");
    return;
  }
  uint32_t s = clk.secOfDay();
  int h = s / 3600, m = (s / 60) % 60;
  if (settings.h24) snprintf(out, n, "%02d:%02d", h, m);
  else snprintf(out, n, "%d:%02d%s", h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
}

static void upper(char* s) {
  for (; *s; s++) *s = toupper((unsigned char)*s);
}

static int ev(int v) { return v & ~1; }  // snap to the 2 px grid

// Pokémon text box: ink frame, light rim, paper inside, notched corners
static void box(Frame& f, int x, int y, int w, int h) {
  if (!f.vis(y, h)) return;
  f.fill(x + 2, y, w - 4, h, c565(INK));
  f.fill(x, y + 2, w, h - 4, c565(INK));
  f.fill(x + 4, y + 2, w - 8, h - 4, c565(RIM));
  f.fill(x + 2, y + 4, w - 4, h - 8, c565(RIM));
  f.fill(x + 6, y + 4, w - 12, h - 8, c565(PAPER));
  f.fill(x + 4, y + 6, w - 8, h - 12, c565(PAPER));
}

// ------------------------------------------------------------------ pixel icons (2 px cells)
static void cloudIcon(Frame& f, int cx, int cy, int R, uint32_t fillc, uint32_t shade) {
  struct D { float x, y, r; } d[] = {{-0.38f, 0.12f, 0.42f}, {0.08f, -0.18f, 0.56f}, {0.5f, 0.14f, 0.38f}};
  int bx = ev(cx - (int)(0.8f * R)), by = ev(cy + (int)(0.1f * R)), bw = ev((int)(1.42f * R)), bh = ev((int)(0.46f * R));
  for (auto& p : d) f.disc(ev(cx + (int)(p.x * R)), ev(cy + (int)(p.y * R)), (int)(p.r * R) + 2, c565(INK));
  f.fill(bx - 2, by, bw + 4, bh + 2, c565(INK));
  for (auto& p : d) f.disc(ev(cx + (int)(p.x * R)), ev(cy + (int)(p.y * R)), (int)(p.r * R), c565(fillc));
  f.fill(bx, by, bw, bh - 2, c565(fillc));
  f.fill(bx, by + bh - 2, bw, 2, c565(shade));
}

static void sunIcon(Frame& f, int cx, int cy, int R) {
  int b = R > 10 ? 4 : 2;
  for (int i = 0; i < 8; i++) {
    float a = i * (float)M_PI / 4;
    f.fill(ev(cx + (int)(cosf(a) * R * 0.92f) - b / 2), ev(cy + (int)(sinf(a) * R * 0.92f) - b / 2), b, b, c565(0xF09830));
  }
  int r = (int)(R * 0.6f);
  f.disc(cx, cy, r + 2, c565(INK));
  f.disc(cx, cy, r, c565(0xF8D040));
  f.fill(ev(cx - r / 2), ev(cy - r / 2), 2, 2, c565(0xFFF8C0));
}

static void moonIcon(Frame& f, int cx, int cy, int R, uint32_t bg) {
  int r = (int)(R * 0.62f);
  f.disc(cx, cy, r + 2, c565(INK));
  f.disc(cx, cy, r, c565(0xF0E8C0));
  f.disc(ev(cx + r / 2 + 2), ev(cy - r / 3), r, c565(bg));
}

static void weatherIcon(Frame& f, Sky k, bool day, int cx, int cy, int R, uint32_t bg) {
  uint32_t white = 0xF8F8F8, light = 0xC8D0DC, grey = 0xA0A8B8, dgrey = 0x707888;
  int b = R > 10 ? 4 : 2;
  switch (k) {
    case Sky::Clear:
      if (day) sunIcon(f, cx, cy, R);
      else moonIcon(f, cx, cy, R, bg);
      break;
    case Sky::Partly:
      if (day) sunIcon(f, ev(cx + R / 3), ev(cy - R / 3), R * 3 / 4);
      else moonIcon(f, ev(cx + R / 3), ev(cy - R / 3), R * 3 / 4, bg);
      cloudIcon(f, ev(cx - R / 6), ev(cy + R / 4), R * 3 / 4, white, light);
      break;
    case Sky::Cloudy:
      cloudIcon(f, ev(cx + R / 4), ev(cy - R / 4), R * 2 / 3, grey, dgrey);
      cloudIcon(f, ev(cx - R / 8), ev(cy + R / 6), R * 3 / 4, white, light);
      break;
    case Sky::Fog:
      for (int i = 0; i < 3; i++) {
        int y = ev(cy - R / 2 + i * R / 2);
        f.fill(ev(cx - R + (i & 1) * 4), y, ev(2 * R - 4), b, c565(INK));
        f.fill(ev(cx - R + 2 + (i & 1) * 4), y, ev(2 * R - 8), 2, c565(light));
      }
      break;
    case Sky::Drizzle:
    case Sky::Rain:
    case Sky::Snow:
    case Sky::Storm: {
      bool storm = k == Sky::Storm;
      cloudIcon(f, cx, ev(cy - R / 4), R * 3 / 4, storm ? grey : white, storm ? dgrey : light);
      int y0 = ev(cy + R / 2);
      for (int i = 0; i < 3; i++) {
        int x = ev(cx - R / 2 + i * R / 2);
        if (k == Sky::Snow) f.fill(x, y0 + (i & 1) * b, b, b, c565(0x80B0E8));
        else if (!storm && (k == Sky::Rain || i != 1)) f.fill(x, y0 + (i & 1) * 2, 2, b + 2, c565(0x3870D0));
      }
      if (storm) {
        f.fill(ev(cx), y0 - 2, b, b + 2, c565(0xF8C030));
        f.fill(ev(cx) - b, y0 + b, b, b + 2, c565(0xF8C030));
      }
      break;
    }
  }
}

static const char* const GEAR[] = {
    "...ww...",
    ".w.ww.w.",
    "..wwww..",
    "www..www",
    "www..www",
    "..wwww..",
    ".w.ww.w.",
    "...ww...",
};

// ------------------------------------------------------------------ main screen pieces
static void hpBar(Frame& f, int x, int y, int w, int pct) {
  pct = constrain(pct, 0, 100);
  f.fill(x, y, w, 8, c565(INK));
  f.fill(x + 2, y + 2, w - 4, 4, c565(0x485068));
  uint32_t c = pct < 60 ? 0x40C850 : (pct < 85 ? 0xF8C030 : 0xE84830);
  int fw = ev((w - 4) * pct / 100);
  if (pct > 0 && fw < 2) fw = 2;
  f.fill(x + 2, y + 2, fw, 4, c565(c));
  if (fw > 2) f.fill(x + 2, y + 2, fw, 2, lerp565(c565(c), c565(0xFFFFFF), 90));  // shine
}

// ◀ ▶ in the bottom corners: tapping the left / right half of the card goes back / forward
static void cardArrows(Frame& f, uint32_t t) {
  int nudge = ((t / 450) & 1) ? 2 : 0;
  int y = CARD_Y + CARD_H - 12;
  f.text(LEFT, CARD_X + 12 - nudge, y, PX8, INK);
  f.text(RIGHT, CARD_X + CARD_W - 20 + nudge, y, PX8, INK);
}

static const char* const THERMO[] = {  // 5x10, drawn at 1x next to temperatures
    ".kkk.",
    ".kwk.",
    ".kwk.",
    ".krk.",
    ".krk.",
    ".krk.",
    "kkrkk",
    "krrrk",
    "krrrk",
    ".kkk.",
};

static void weatherCard(Frame& f, int x, int y) {
  if (!weather.valid) {
    f.text("WAITING FOR", x + CARD_W / 2, y + 36, PX8, SUB, AL_C);
    f.text("WEATHER...", x + CARD_W / 2, y + 50, PX8, SUB, AL_C);
    return;
  }
  Sky k = skyFor(weather.code);
  weatherIcon(f, k, weather.day, x + 30, y + 30, 16, PAPER);
  char buf[40];
  snprintf(buf, sizeof buf, "%d%s", weather.temp, DEG);
  f.text(buf, x + 56, y + 24, PX16, INK);
  f.text(skyName(k, weather.day), x + 56, y + 44, PX8, INK, AL_L, 76);
  int rx = x + 136;
  snprintf(buf, sizeof buf, "H%d%s L%d%s", weather.hi, DEG, weather.lo, DEG);
  f.text(buf, rx, y + 16, PX8, SUB, AL_L, CARD_W - 144);
  snprintf(buf, sizeof buf, "WIND %d", weather.wind);
  f.text(buf, rx, y + 30, PX8, SUB, AL_L, CARD_W - 144);
  if (weather.hum >= 0) {
    snprintf(buf, sizeof buf, "HUM %d%%", weather.hum);
    f.text(buf, rx, y + 44, PX8, SUB, AL_L, CARD_W - 144);
  }
  char place[40];
  strlcpy(place, weather.place, sizeof place);
  upper(place);
  f.text(place, x + 12, y + 62, PX8, INK, AL_L, CARD_W - 24);
  char date[24];
  strlcpy(date, clk.date, sizeof date);
  upper(date);
  f.text(date, x + 28, y + 74, PX8, SUB, AL_L, CARD_W - 56);  // between the ◀ ▶ arrows
}

static void pcCard(Frame& f, int x, int y) {
  if (!pc.valid) {
    f.text("WAITING FOR", x + CARD_W / 2, y + 36, PX8, SUB, AL_C);
    f.text("YOUR PC...", x + CARD_W / 2, y + 50, PX8, SUB, AL_C);
    return;
  }
  bool gpu = pc.gpu >= 0 || pc.gpuT >= 0;
  int step = gpu ? 16 : 20;
  int y0 = y + 12 + (gpu ? 0 : 4);
  // columns: label | HP bar | value (left-aligned, all rows) | thermometer + temp
  const int barX = x + 46, barW = 72, valX = x + 126, tempX = x + 170;
  char v[16];
  int r = 0;
  auto row = [&](const char* label, int pct, const char* value, int temp) {
    int cy = y0 + r * step;
    f.text(label, x + 12, cy, PX8, INK);
    hpBar(f, barX, cy - 4, barW, pct);
    if (value[0]) f.text(value, valX, cy, PX8, INK);
    if (temp >= 0) {
      f.bitmap(THERMO, 5, 10, tempX, cy - 5, 1);
      char t[12];
      snprintf(t, sizeof t, "%d%s", temp, DEG);
      f.text(t, tempX + 8, cy, PX8, INK);
    }
    r++;
  };
  snprintf(v, sizeof v, "%d%%", pc.cpu);
  row("CPU", pc.cpu, v, pc.cpuT);
  if (gpu) {
    if (pc.gpu >= 0) snprintf(v, sizeof v, "%d%%", pc.gpu);
    else v[0] = 0;
    row("GPU", max(0, pc.gpu), v, pc.gpuT);
  }
  snprintf(v, sizeof v, "%d%%", pc.ram);
  row("RAM", pc.ram, v, -1);
  snprintf(v, sizeof v, "%d", pc.wpm);
  row("WPM", pc.wpm * 100 / 120, v, -1);
}

// EXP bar: how far this focus session has got toward the next level
static void expBar(Frame& f, int x, int y, int pct) {
  f.text("EXP", x, y + 4, PX8, SUB);
  int bx = x + 28, w = CARD_W - 52, fw = ev((w - 4) * pct / 100);
  f.fill(bx, y, w, 8, c565(INK));
  f.fill(bx + 2, y + 2, w - 4, 4, c565(0x485068));
  f.fill(bx + 2, y + 2, fw, 4, c565(0x58A0F0));
  if (fw > 2) f.fill(bx + 2, y + 2, fw, 2, c565(0x98C8F8));  // shine
}

static void focusCard(Frame& f, int x, int y, uint32_t now) {
  char buf[24];
  int pct = 0;
  if (focus.active && focus.durMs) pct = (int)min<uint32_t>((now - focus.startMs) * 100 / focus.durMs, 100);
  if (focus.active) {
    uint32_t left = focus.leftSec(now);
    snprintf(buf, sizeof buf, "%02lu:%02lu", (unsigned long)(left / 60), (unsigned long)(left % 60));
  } else {
    snprintf(buf, sizeof buf, "FOCUS %dM", settings.focusMin);
  }
  f.text(focus.active ? "FOCUS" : buf, x + 12, y + 16, PX8, INK);
  char lv[24];
  snprintf(lv, sizeof lv, "Lv%d %s Lv%d", catLevel, RIGHT, catLevel + 1);
  f.text(lv, x + CARD_W - 12, y + 16, PX8, SUB, AL_R);
  expBar(f, x + 12, y + 28, pct);
  if (focus.active) {
    f.text(buf, x + CARD_W / 2, y + 56, PX16, INK, AL_C);  // big countdown
    f.text("KEEP GOING!", x + CARD_W / 2, y + 74, PX8, SUB, AL_C);
  } else {
    // big START button
    int bx = x + (FC_BTN_X - CARD_X), by = FC_BTN_Y, w = FC_BTN_W, h = FC_BTN_H;
    f.fill(bx + 2, by, w - 4, h, c565(INK));
    f.fill(bx, by + 2, w, h - 4, c565(INK));
    f.fill(bx + 2, by + 2, w - 4, h - 4, c565(GOLD));
    f.fill(bx + 2, by + 2, w - 4, 2, c565(0xFCE088));      // top highlight
    f.fill(bx + 2, by + h - 6, w - 4, 4, c565(0xD8A020));  // pressed-edge shade
    int tx = bx + 22, ty = by + h / 2 - 2;                 // ▶ drawn in 2 px steps
    for (int i = 0; i < 6; i++) f.fill(tx + i * 2, ty - 10 + i * 2, 2, 20 - i * 4, c565(INK));
    f.text("START", bx + w / 2 + 12, ty, PX16, INK, AL_C);
  }
}

static void levelUpBox(Frame& f, uint32_t now) {
  box(f, CARD_X, CARD_Y, CARD_W, CARD_H);
  char nm[20];
  strlcpy(nm, settings.name[0] ? settings.name : "YOUR CAT", sizeof nm);
  upper(nm);
  f.text(nm, CARD_X + CARD_W / 2, CARD_Y + 22, PX8, INK, AL_C, CARD_W - 24);
  f.text("GREW TO", CARD_X + CARD_W / 2, CARD_Y + 38, PX8, INK, AL_C);
  char buf[16];
  snprintf(buf, sizeof buf, "LEVEL %d!", catLevel);
  f.text(buf, CARD_X + CARD_W / 2, CARD_Y + 60, PX16, INK, AL_C);
  if ((now / 450) & 1) f.text("\xE2\x96\xBC", CARD_X + CARD_W - 20, CARD_Y + CARD_H - 12, PX8, INK);
}

static void card(Frame& f, int which, int x) {
  if (which == CARD_WEATHER) weatherCard(f, x, CARD_Y);
  else if (which == CARD_PC) pcCard(f, x, CARD_Y);
  else focusCard(f, x, CARD_Y, millis());
}

void drawMain(Frame& f, WeatherFx& fx, const CatAnim& cat, const MainView& v) {
  fx.draw(f);

  // cat: body -> face -> desk -> paws -> effects (body, face and paws breathe together)
  const CatImg* const* L = CAT_TABLE[settings.style % 2][settings.coat % COAT_COUNT];
  int dy = cat.bob();
  int stage = stageFor(catLevel);  // evolution: gear and fluff it has earned stay on
  if (stage >= 3) f.cat(L[L_FLUFF], dy);
  f.cat(L[cat.body()], dy);
  f.cat(L[cat.face()], dy);
  if (stage >= 2) f.cat(L[L_GEAR2 + stage - 2], dy);
  f.cat(L[stage >= 4 && L[L_DESK_RGB] ? L_DESK_RGB : L_DESK]);
  if (cat.paws() != L_COUNT) f.cat(L[cat.paws()], dy);
  if (cat.fx() != L_COUNT) f.cat(L[cat.fx()]);
  if (cat.fx2() != L_COUNT) f.cat(L[cat.fx2()]);

  // name plate: name + level, mood hearts below (Tamagotchi style)
  if (f.vis(CAT_Y, 40)) {
    char nm[17], lv[10];
    strlcpy(nm, settings.name, sizeof nm);
    upper(nm);
    snprintf(lv, sizeof lv, "Lv%d", catLevel);
    int top = (nm[0] ? f.textWidth(nm, PX8) + 8 : 0) + f.textWidth(lv, PX8);
    int w = ev(max(top, 4 * 10) + 22);
    int y = CAT_Y + 2;
    box(f, 6, y, w, 36);
    if (nm[0]) f.text(nm, 16, y + 13, PX8, INK);
    f.text(lv, 6 + w - 10, y + 13, PX8, SUB, AL_R);
    int h = cat.hearts();
    for (int i = 0; i < 4; i++) f.text(HEART, 16 + i * 10, y + 25, PX8, i < h ? 0xE84858 : 0xC8D0DC);
  }

  // card: level-up box, focus timer, or the cycling weather / PC cards
  if (f.vis(CARD_Y, CARD_H) && v.t < levelUpUntil) {
    levelUpBox(f, v.t);
  } else if (f.vis(CARD_Y, CARD_H) && v.nCards > 0) {
    box(f, CARD_X, CARD_Y, CARD_W, CARD_H);
    f.setClip(CARD_X + 6, CARD_Y + 4, CARD_W - 12, CARD_H - 8);
    float e = v.slide * v.slide * (3 - 2 * v.slide);
    int dx = ev((int)(e * CARD_W)) * v.dir;
    card(f, v.card, CARD_X - dx);
    if (v.next >= 0) card(f, v.next, CARD_X + v.dir * CARD_W - dx);
    f.clearClip();
    if (v.nCards > 1 && v.next < 0) cardArrows(f, v.t);
  }

  // header bar
  if (f.vis(0, HEADER_H + 2)) {
    f.fill(0, 0, SW, HEADER_H, c565(HDR));
    f.fill(0, HEADER_H, SW, 2, c565(HDR_LINE));
    char buf[24];
    if (weather.valid) {
      weatherIcon(f, skyFor(weather.code), weather.day, 12, 11, 8, HDR);
      snprintf(buf, sizeof buf, "%d%s", weather.temp, DEG);
    } else {
      snprintf(buf, sizeof buf, "--%s", DEG);
    }
    f.text(buf, 26, 11, PX8, 0xFFFFFF);
    timeString(buf, sizeof buf);
    f.text(buf, SW / 2 + 4, 11, PX8, 0xFFFFFF, AL_C);
    f.bitmap(GEAR, 8, 8, GEAR_X + 2, GEAR_Y + 1, 2);
  }
  fx.drawFlash(f);
}

// ------------------------------------------------------------------ settings ("OPTIONS")
void drawSettings(Frame& f, uint32_t t) {
  f.fill(0, f.oy, SW, STRIP, c565(HDR));
  f.ditherRect(0, f.oy, SW, STRIP, c565(0x2C3658));

  // plain title (not a box, so it doesn't read as a button)
  f.text("OPTIONS", 14, 22, PX16, INK);
  f.text("OPTIONS", 12, 20, PX16, PAPER);
  box(f, 152, 6, 82, 28);
  f.text(LEFT, 166, 20, PX8, INK);
  f.text("BACK", 180, 20, PX8, INK);

  if (configUrl[0]) {
    box(f, 44, 46, 152, 152);
    f.qr(configUrl, 54, 56, 132);
    box(f, 6, 206, 228, 34);
    f.text("SCAN TO PICK YOUR CAT!", SW / 2, 223, PX8, INK, AL_C);
  } else {
    box(f, 6, 84, 228, 76);
    f.text("OPEN BONGO CAT", SW / 2, 106, PX8, INK, AL_C);
    f.text("ON YOUR PC. THE", SW / 2, 122, PX8, INK, AL_C);
    f.text("CODE SHOWS UP HERE.", SW / 2, 138, PX8, INK, AL_C);
  }

  // brightness: [<] [##########] [>]
  box(f, 6, BR_Y - 18, 228, 52);
  int seg = (int)roundf((settings.bright - 10) * 10.0f / 245);
  char pct[8];
  snprintf(pct, sizeof pct, "%d%%", (settings.bright * 100 + 127) / 255);
  f.text("BRIGHTNESS", 18, BR_Y - 4, PX8, INK);
  f.text(pct, 222, BR_Y - 4, PX8, SUB, AL_R);
  f.text(LEFT, BR_MINUS_X + 6, BR_Y + 16, PX8, INK);
  f.text(RIGHT, BR_PLUS_X + 18, BR_Y + 16, PX8, INK);
  for (int i = 0; i < 10; i++) {
    int x = BR_BAR_X + i * 13;
    f.fill(x, BR_Y + 10, 12, 12, c565(INK));
    f.fill(x + 2, BR_Y + 12, 8, 8, c565(i < seg ? GOLD : 0xC8D0DC));
  }
  (void)t;
}

void drawCaption(Frame& f, const char* text, uint32_t t) {
  if (!f.vis(286, 30)) return;
  box(f, 6, 286, 228, 30);
  f.text("DEMO", 18, 301, PX8, SUB);
  f.text(RIGHT, 56 + (((t / 450) & 1) ? 2 : 0), 301, PX8, INK);
  f.text(text, 72, 301, PX8, INK, AL_L, 150);
}

void drawMessage(Frame& f, const char* title, const char* sub) {
  f.fill(0, f.oy, SW, STRIP, c565(HDR));
  box(f, 10, 116, 220, 80);
  f.text(title, SW / 2, 140, PX8, INK, AL_C, 200);
  f.text(sub, SW / 2, 164, PX8, SUB, AL_C, 200);
}
