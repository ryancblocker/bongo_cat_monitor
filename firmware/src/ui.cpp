#include "ui.h"

#include <math.h>

#include "state.h"

static const char* const DEG = "\xC2\xB0";

void timeString(char* out, size_t n) {
  if (!clk.valid) {
    snprintf(out, n, "--:--");
    return;
  }
  uint32_t s = clk.secOfDay();
  int h = s / 3600, m = (s / 60) % 60;
  if (settings.h24) snprintf(out, n, "%02d:%02d", h, m);
  else snprintf(out, n, "%d:%02d %s", h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
}

// ------------------------------------------------------------------ icons (centre cx,cy; s = scale, 1 ~ 24 px)
static void cloudShape(Frame& f, float cx, float cy, float s, uint32_t c) {
  f.fillCircle(cx - 6 * s, cy + 1 * s, 6 * s, c);
  f.fillCircle(cx + 1 * s, cy - 3 * s, 7.5f * s, c);
  f.fillCircle(cx + 8 * s, cy + 1 * s, 5.5f * s, c);
  f.fillRRect(cx - 12 * s, cy + 0 * s, 25 * s, 7 * s, 3.5f * s, c);
}

static void sunShape(Frame& f, float cx, float cy, float s) {
  for (int i = 0; i < 8; i++) {
    float a = i * (float)M_PI / 4;
    f.wideLine(cx + cosf(a) * 8 * s, cy + sinf(a) * 8 * s, cx + cosf(a) * 11.5f * s, cy + sinf(a) * 11.5f * s, 1.1f * s, 0xFFB300);
  }
  f.fillCircle(cx, cy, 6 * s, 0xFFC83D);
}

static void moonShape(Frame& f, float cx, float cy, float s, uint32_t bite) {
  f.fillCircle(cx, cy, 8 * s, 0xF2EBC9);
  f.fillCircle(cx + 4.5f * s, cy - 3 * s, 7 * s, bite);
}

static void weatherIcon(Frame& f, Sky k, bool day, int cx, int cy, float s, uint32_t bg, bool onDark) {
  uint32_t cloud = onDark ? 0xD7DEE8 : 0x9DAABB;
  uint32_t darkCloud = onDark ? 0x9BA6B6 : 0x6E7B8C;
  switch (k) {
    case Sky::Clear:
      if (day) sunShape(f, cx, cy, s);
      else moonShape(f, cx, cy, s, bg);
      break;
    case Sky::Partly:
      if (day) sunShape(f, cx + 5 * s, cy - 5 * s, s * 0.8f);
      else moonShape(f, cx + 5 * s, cy - 5 * s, s * 0.75f, bg);
      cloudShape(f, cx - 2 * s, cy + 3 * s, s * 0.85f, cloud);
      break;
    case Sky::Cloudy:
      cloudShape(f, cx + 4 * s, cy - 4 * s, s * 0.7f, darkCloud);
      cloudShape(f, cx - 1 * s, cy + 2 * s, s, cloud);
      break;
    case Sky::Fog:
      for (int i = 0; i < 3; i++) f.wideLine(cx - 11 * s + (i & 1) * 3 * s, cy - 6 * s + i * 6 * s, cx + 11 * s - (i & 1) * 2 * s, cy - 6 * s + i * 6 * s, 1.6f * s, cloud);
      break;
    case Sky::Drizzle:
    case Sky::Rain:
    case Sky::Storm:
      cloudShape(f, cx, cy - 4 * s, s, k == Sky::Storm ? darkCloud : cloud);
      if (k == Sky::Storm) {
        f.triangle(cx + 1 * s, cy + 2 * s, cx - 5 * s, cy + 11 * s, cx + 0 * s, cy + 10 * s, 0xFFC83D);
        f.triangle(cx - 1 * s, cy + 9 * s, cx + 5 * s, cy + 8 * s, cx - 4 * s, cy + 17 * s, 0xFFC83D);
      } else {
        int n = k == Sky::Rain ? 3 : 2;
        for (int i = 0; i < n; i++) {
          float x = cx - 6 * s + i * (12.0f / max(1, n - 1)) * s;
          f.wideLine(x, cy + 5 * s, x - 2 * s, cy + 11 * s, 1.1f * s, 0x4A90E2);
        }
      }
      break;
    case Sky::Snow:
      cloudShape(f, cx, cy - 4 * s, s, cloud);
      for (int i = 0; i < 3; i++) f.fillCircle(cx - 6 * s + i * 6 * s, cy + 8 * s + (i & 1) * 3 * s, 1.7f * s, 0x7FB2E5);
      break;
  }
}

static void gear(Frame& f, int cx, int cy, uint32_t c) {
  for (int i = 0; i < 8; i++) {
    float a = i * (float)M_PI / 4 + 0.39f;
    f.wideLine(cx + cosf(a) * 5.5f, cy + sinf(a) * 5.5f, cx + cosf(a) * 8.8f, cy + sinf(a) * 8.8f, 1.6f, c);
  }
  f.ring(cx, cy, 7, 3, c);
}

// ------------------------------------------------------------------ main screen
static void bar(Frame& f, int x, int y, int w, int pct, bool onDark) {
  pct = constrain(pct, 0, 100);
  f.blendRRect(x, y, w, 6, 3, onDark ? c565(0xFFFFFF) : c565(0x1D2433), 40);
  uint32_t c = pct < 60 ? 0x34C759 : (pct < 85 ? 0xFFB020 : 0xFF453A);
  int fw = max(6, w * pct / 100);
  f.fillRRect(x, y, fw, 6, 3, c);
}

static void weatherCard(Frame& f, int x, int y, bool dark, uint32_t ink, uint32_t sub, uint32_t biteColor) {
  if (!weather.valid) {
    f.text("Waiting for weather\xE2\x80\xA6", x + CARD_W / 2, y + CARD_H / 2, INTER_13, sub, AL_C);
    return;
  }
  Sky k = skyFor(weather.code);
  weatherIcon(f, k, weather.day, x + 30, y + 34, 1.3f, biteColor, dark);
  char buf[48];
  snprintf(buf, sizeof buf, "%d%s", weather.temp, DEG);
  bool fits = f.textWidth(buf, INTER_40B) <= 76;  // "100°" / "-12°" step down
  f.text(buf, x + 56, y + 34, fits ? INTER_40B : INTER_22B, ink);
  int cx = x + 140;
  const char* cond = skyName(k, weather.day);
  bool big = f.textWidth(cond, INTER_15B) <= CARD_W - 146;  // step down a size before truncating
  f.text(cond, cx, y + 20, big ? INTER_15B : INTER_13, ink, AL_L, CARD_W - 146);
  f.text(clk.date, cx, y + 38, INTER_11, sub, AL_L, CARD_W - 146);
  f.text(weather.place, cx, y + 53, INTER_11, sub, AL_L, CARD_W - 146);
  if (weather.hum >= 0)
    snprintf(buf, sizeof buf, "H %d%s  L %d%s  \xC2\xB7  %d %s  \xC2\xB7  %d%%", weather.hi, DEG, weather.lo, DEG, weather.wind, weather.windUnit, weather.hum);
  else
    snprintf(buf, sizeof buf, "H %d%s  L %d%s  \xC2\xB7  %d %s", weather.hi, DEG, weather.lo, DEG, weather.wind, weather.windUnit);
  f.text(buf, x + 14, y + 69, INTER_11, sub, AL_L, CARD_W - 28);
}

static void pcCard(Frame& f, int x, int y, bool dark, uint32_t ink, uint32_t sub) {
  if (!pc.valid) {
    f.text("Waiting for your PC\xE2\x80\xA6", x + CARD_W / 2, y + CARD_H / 2, INTER_13, sub, AL_C);
    return;
  }
  bool gpu = pc.gpu >= 0 || pc.gpuT >= 0;
  int rows = gpu ? 4 : 3;
  int step = gpu ? 18 : 22, y0 = y + (CARD_H - (rows - 1) * step) / 2;
  char v[24];
  int r = 0;
  auto row = [&](const char* label, int pct, const char* value) {
    int cy = y0 + r * step;
    f.text(label, x + 14, cy, INTER_13, sub);
    bar(f, x + 58, cy - 3, 92, pct, dark);
    f.text(value, x + CARD_W - 12, cy, INTER_13, ink, AL_R);
    r++;
  };
  if (pc.cpuT >= 0) snprintf(v, sizeof v, "%d%%  %d%s", pc.cpu, pc.cpuT, DEG);
  else snprintf(v, sizeof v, "%d%%", pc.cpu);
  row("CPU", pc.cpu, v);
  if (gpu) {
    if (pc.gpu >= 0 && pc.gpuT >= 0) snprintf(v, sizeof v, "%d%%  %d%s", pc.gpu, pc.gpuT, DEG);
    else if (pc.gpu >= 0) snprintf(v, sizeof v, "%d%%", pc.gpu);
    else snprintf(v, sizeof v, "%d%s", pc.gpuT, DEG);
    row("GPU", max(0, pc.gpu), v);
  }
  snprintf(v, sizeof v, "%d%%", pc.ram);
  row("RAM", pc.ram, v);
  snprintf(v, sizeof v, "%d wpm", pc.wpm);
  row("WPM", pc.wpm * 100 / 120, v);
}

static void card(Frame& f, int which, int x, bool dark, uint32_t ink, uint32_t sub, uint32_t bite) {
  if (which == CARD_WEATHER) weatherCard(f, x, CARD_Y, dark, ink, sub, bite);
  else pcCard(f, x, CARD_Y, dark, ink, sub);
}

void drawMain(Frame& f, WeatherFx& fx, const CatAnim& cat, const MainView& v) {
  fx.draw(f);

  // cat: body -> face -> desk -> paws -> effect
  const CatImg* const* L = CAT_TABLE[settings.style % 3][settings.cat % 3][settings.coat % 3];
  f.cat(L[cat.body()]);
  f.cat(L[cat.face()]);
  f.cat(L[L_DESK]);
  if (cat.paws() != L_COUNT) f.cat(L[cat.paws()]);
  if (cat.fx() != L_COUNT) f.cat(L[cat.fx()]);

  bool dark = !fx.isDay();
  uint32_t ink = dark ? 0xF4F6FA : 0x1D2433, sub = dark ? 0xAEB8C8 : 0x566176;
  uint16_t panel = dark ? c565(0x0E1422) : c565(0xFFFFFF);
  uint8_t panelA = dark ? 175 : 195;
  uint32_t bite = dark ? 0x1B2232 : 0xF6F8FB;  // card colour behind the moon icon

  // name tag
  if (settings.name[0] && f.vis(CAT_Y + 2, 20)) {
    int w = f.textWidth(settings.name, INTER_13) + 18;
    f.blendRRect(8, CAT_Y + 4, w, 20, 10, c565(0x10141C), 120);
    f.text(settings.name, 8 + w / 2, CAT_Y + 14, INTER_13, 0xFFFFFF, AL_C);
  }

  // card (slides horizontally when changing)
  if (f.vis(CARD_Y, CARD_H) && v.nCards > 0) {
    f.blendRRect(CARD_X, CARD_Y, CARD_W, CARD_H, 14, panel, panelA);
    f.setClip(CARD_X, CARD_Y, CARD_W, CARD_H);
    float e = v.slide * v.slide * (3 - 2 * v.slide);  // smoothstep
    int dx = (int)(e * CARD_W);
    card(f, v.card, CARD_X - dx, dark, ink, sub, bite);
    if (v.next >= 0) card(f, v.next, CARD_X + CARD_W - dx, dark, ink, sub, bite);
    f.clearClip();
  }
  // page dots
  if (v.nCards > 1 && f.vis(CARD_Y + CARD_H + 2, 8)) {
    int active = v.next >= 0 && v.slide > 0.5f ? v.next : v.card;
    for (int i = 0; i < v.nCards; i++)
      f.blendCircle(114 + i * 12, CARD_Y + CARD_H + 6, 2.6f, c565(0xFFFFFF), i == active ? 230 : 90);
  }

  // header
  if (f.vis(0, HEADER_H)) {
    f.blendRect(0, 0, SW, HEADER_H, c565(0x0B0F18), dark ? 120 : 70);
    char buf[24];
    if (weather.valid) {
      weatherIcon(f, skyFor(weather.code), weather.day, 16, 15, 0.62f, 0x2A3140, true);
      snprintf(buf, sizeof buf, "%d%s", weather.temp, DEG);
    } else {
      snprintf(buf, sizeof buf, "--%s", DEG);
    }
    f.text(buf, 30, 15, INTER_17B, 0xFFFFFF);
    timeString(buf, sizeof buf);
    f.text(buf, SW / 2, 15, INTER_17B, 0xFFFFFF, AL_C);
    gear(f, GEAR_X, GEAR_Y, 0xFFFFFF);
  }
  fx.drawFlash(f);
}

// ------------------------------------------------------------------ settings screen
void drawSettings(Frame& f, bool active) {
  f.spr->fillRect(0, 0, SW, STRIP, (uint32_t)0x10141Cu);  // u32 = RGB888 (a plain int is read as RGB565)
  f.text("Settings", 16, 22, INTER_17B, 0xFFFFFF);
  f.fillCircle(220, 22, 14, 0x2A3140);
  f.wideLine(214, 16, 226, 28, 1.4f, 0xFFFFFF);
  f.wideLine(226, 16, 214, 28, 1.4f, 0xFFFFFF);

  if (configUrl[0]) {
    f.fillRRect(45, 46, 150, 150, 12, 0xFFFFFF);
    f.qr(configUrl, 54, 55, 132);
    f.text("Scan with your phone", SW / 2, 214, INTER_15B, 0xFFFFFF, AL_C);
    f.text("to pick a cat, name it and more", SW / 2, 232, INTER_11, 0xAEB8C8, AL_C);
    f.text(configUrl, SW / 2, 248, INTER_11, 0x7C8798, AL_C, 220);
  } else {
    f.text("Open Bongo Cat on your PC", SW / 2, 118, INTER_15B, 0xFFFFFF, AL_C);
    f.text("The setup code shows up here", SW / 2, 140, INTER_11, 0xAEB8C8, AL_C);
    f.text("once the app is running.", SW / 2, 156, INTER_11, 0xAEB8C8, AL_C);
  }

  // brightness slider
  char pct[8];
  snprintf(pct, sizeof pct, "%d%%", (settings.bright * 100 + 127) / 255);
  f.text("Brightness", 16, 274, INTER_13, 0xFFFFFF);
  f.text(pct, 224, 274, INTER_13, 0xAEB8C8, AL_R);
  int kx = SLIDER_X0 + (int)((settings.bright - 10) * (SLIDER_X1 - SLIDER_X0) / 245.0f);
  f.fillRRect(SLIDER_X0, SLIDER_Y - 4, SLIDER_X1 - SLIDER_X0, 8, 4, 0x2A3140);
  f.fillRRect(SLIDER_X0, SLIDER_Y - 4, max(8, kx - SLIDER_X0), 8, 4, 0xFFC83D);
  f.fillCircle(kx, SLIDER_Y, active ? 12 : 10, 0xFFFFFF);
  // dim / bright sun marks at the ends
  f.fillCircle(18, SLIDER_Y, 3, 0x7C8798);
  f.ring(224, SLIDER_Y, 6, 2, 0xFFC83D);
  f.fillCircle(224, SLIDER_Y, 3, 0xFFC83D);
}

void drawMessage(Frame& f, const char* title, const char* sub) {
  f.spr->fillRect(0, 0, SW, STRIP, (uint32_t)0x10141Cu);  // u32 = RGB888 (a plain int is read as RGB565)
  f.text(title, SW / 2, 150, INTER_17B, 0xFFFFFF, AL_C);
  f.text(sub, SW / 2, 176, INTER_13, 0xAEB8C8, AL_C, 220);
}
