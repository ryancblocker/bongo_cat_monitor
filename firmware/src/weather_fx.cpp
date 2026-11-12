#include "weather_fx.h"

#include <math.h>

Sky skyFor(int c) {
  if (c == 0 || c == 1) return Sky::Clear;
  if (c == 2) return Sky::Partly;
  if (c == 3) return Sky::Cloudy;
  if (c == 45 || c == 48) return Sky::Fog;
  if (c >= 51 && c <= 57) return Sky::Drizzle;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return Sky::Rain;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return Sky::Snow;
  if (c >= 95) return Sky::Storm;
  return Sky::Cloudy;
}

const char* skyName(Sky s, bool day) {
  switch (s) {
    case Sky::Clear: return day ? "SUNNY" : "CLEAR";
    case Sky::Partly: return day ? "SUN+CLOUD" : "FEW CLOUDS";
    case Sky::Cloudy: return "CLOUDY";
    case Sky::Fog: return "FOGGY";
    case Sky::Drizzle: return "DRIZZLE";
    case Sky::Rain: return "RAIN";
    case Sky::Snow: return "SNOW";
    case Sky::Storm: return "STORM";
  }
  return "";
}

static float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
static int snap(float v) { return ((int)floorf(v)) & ~1; }  // 2 px grid

// 4 sky colours, top to bottom
static void palette(Sky s, bool day, uint32_t c[4]) {
  static const uint32_t DAY_CLEAR[4] = {0x5890E8, 0x70A8F0, 0x90C0F8, 0xB0D8F8};
  static const uint32_t DAY_GREY[4] = {0x7888A0, 0x8898B0, 0x98A8C0, 0xB0C0D0};
  static const uint32_t DAY_RAIN[4] = {0x506078, 0x607088, 0x708098, 0x8898A8};
  static const uint32_t DAY_SNOW[4] = {0x98A8C8, 0xA8B8D8, 0xC0D0E8, 0xD8E4F0};
  static const uint32_t DAY_FOG[4] = {0xA0A8B8, 0xB0B8C4, 0xC0C8D0, 0xD0D6DC};
  static const uint32_t DAY_STORM[4] = {0x303848, 0x404858, 0x505868, 0x606878};
  static const uint32_t NIGHT[4] = {0x101830, 0x182440, 0x203050, 0x304068};
  static const uint32_t NIGHT_GREY[4] = {0x181C28, 0x202634, 0x283040, 0x343C50};
  const uint32_t* p;
  if (!day) p = (s == Sky::Clear || s == Sky::Partly) ? NIGHT : NIGHT_GREY;
  else switch (s) {
      case Sky::Clear: case Sky::Partly: p = DAY_CLEAR; break;
      case Sky::Cloudy: p = DAY_GREY; break;
      case Sky::Fog: p = DAY_FOG; break;
      case Sky::Snow: p = DAY_SNOW; break;
      case Sky::Storm: p = DAY_STORM; break;
      default: p = DAY_RAIN;
    }
  for (int i = 0; i < 4; i++) c[i] = p[i];
}

void WeatherFx::set(Sky s, bool d) {
  if (inited && s == kind && d == day) return;
  kind = s;
  day = d;
  reset();
}

void WeatherFx::reset() {
  inited = true;
  uint32_t c[4];
  palette(kind, day, c);
  // 4 bands with a 6 px dithered seam between each (classic 16-bit sky)
  const int bandH = SH / 4;
  for (int y = 0; y < SH; y++) {
    int b = min(3, y / bandH);
    int into = y - b * bandH;
    rowA[y] = rowB[y] = c565(c[b]);
    if (b < 3 && into >= bandH - 6) rowB[y] = c565(c[b + 1]);
  }
  nclouds = 0;
  int nc = kind == Sky::Partly ? 3 : (kind == Sky::Cloudy || kind == Sky::Rain || kind == Sky::Drizzle || kind == Sky::Storm) ? 4
           : (kind == Sky::Snow ? 2 : 0);
  for (int i = 0; i < nc; i++) clouds[nclouds++] = {rnd(-60, SW), (int16_t)snap(rnd(40, 150)), (uint8_t)(esp_random() & 1), rnd(4, 9)};
  nparts = 0;
  int n = (kind == Sky::Rain || kind == Sky::Storm) ? 60 : kind == Sky::Drizzle ? 30 : kind == Sky::Snow ? 50 : 0;
  for (int i = 0; i < n; i++) {
    bool snow = kind == Sky::Snow;
    parts[nparts++] = {rnd(0, SW), rnd(0, SH), snow ? rnd(16, 34) : rnd(200, 300), (uint8_t)(snow ? (esp_random() % 4 == 0 ? 2 : 1) : 0)};
  }
  for (auto& s : stars) s = {(int16_t)snap(rnd(4, SW - 4)), (int16_t)snap(rnd(30, 180)), (uint8_t)rnd(0, 255)};
  flash = 0;
  nextFlash = rnd(3, 7);
}

void WeatherFx::update(float dt) {
  if (!inited) reset();
  t += dt;
  for (int i = 0; i < nclouds; i++) {
    clouds[i].x += clouds[i].v * dt;
    if (clouds[i].x > SW + 10) clouds[i].x = -70;
  }
  bool snow = kind == Sky::Snow;
  for (int i = 0; i < nparts; i++) {
    P& p = parts[i];
    p.y += p.v * dt;
    p.x += snow ? sinf(t * 1.2f + i) * 10 * dt : -p.v * 0.12f * dt;
    if (p.y > SH + 8) { p.y = rnd(-16, -2); p.x = rnd(0, SW + 30); }
    if (p.x < -8) p.x += SW + 16;
    if (p.x > SW + 8) p.x -= SW + 16;
  }
  if (kind == Sky::Storm) {
    flash = max(0.0f, flash - dt * 4);
    nextFlash -= dt;
    if (nextFlash <= 0) { flash = 1; boltX = snap(rnd(40, SW - 40)); nextFlash = rnd(4, 9); }
  }
}

// block cloud: rows of 2 px cells (w = white, g = shade)
static const char* const CLOUD_BIG[] = {
    "..........wwww..........",
    "......wwwwwwwwww........",
    "....wwwwwwwwwwwwww.ww...",
    "..wwwwwwwwwwwwwwwwwwwww.",
    ".wwwwwwwwwwwwwwwwwwwwwww",
    "wwwwwwwwwwwwwwwwwwwwwwww",
    "gwwwwwwwwwwwwwwwwwwwwwwg",
    ".gggggggggggggggggggggg.",
};
static const char* const CLOUD_SMALL[] = {
    "......wwww......",
    "...wwwwwwwwww...",
    ".wwwwwwwwwwwwww.",
    "wwwwwwwwwwwwwwww",
    "gwwwwwwwwwwwwwwg",
    ".gggggggggggggg.",
};

static void cloud(Frame& f, int x, int y, bool big, uint16_t w, uint16_t g) {
  const char* const* m = big ? CLOUD_BIG : CLOUD_SMALL;
  int rows = big ? 8 : 6, cols = big ? 24 : 16;
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      char k = m[r][c];
      if (k == 'w') f.fill(x + c * 2, y + r * 2, 2, 2, w);
      else if (k == 'g') f.fill(x + c * 2, y + r * 2, 2, 2, g);
    }
}

void WeatherFx::draw(Frame& f) {
  f.skyBands(rowA, rowB);
  // sun
  if (day && (kind == Sky::Clear || kind == Sky::Partly)) {
    int cx = 196, cy = 138;
    int ph = (int)(t * 2) & 1;  // rays blink between two lengths
    uint16_t ray = c565(0xF8C030);
    for (int i = 0; i < 8; i++) {
      float a = i * (float)M_PI / 4;
      int len = (i & 1) == ph ? 3 : 2;
      for (int k = 0; k < len; k++) {
        int r = 20 + k * 4;
        f.fill(snap(cx + cosf(a) * r) - 1, snap(cy + sinf(a) * r) - 1, 4, 4, ray);
      }
    }
    f.disc(cx, cy, 15, c565(0xF09830));
    f.disc(cx, cy, 13, c565(0xF8D040));
    f.fill(cx - 8, cy - 8, 4, 4, c565(0xFFF4A0));  // shine
  } else if (!day && kind != Sky::Storm) {
    for (auto& s : stars) {
      if (kind != Sky::Clear && kind != Sky::Partly && (s.ph & 1)) continue;
      int tw = ((int)(t * 3) + s.ph) % 8;
      uint16_t c = c565(tw < 2 ? 0xFFFFFF : 0xB8C4E0);
      f.fill(s.x, s.y, 2, 2, c);
      if (tw == 0) {  // twinkle: little plus
        f.fill(s.x - 2, s.y, 2, 2, c);
        f.fill(s.x + 2, s.y, 2, 2, c);
        f.fill(s.x, s.y - 2, 2, 2, c);
        f.fill(s.x, s.y + 2, 2, 2, c);
      }
    }
    if (kind == Sky::Clear || kind == Sky::Partly) {
      int cx = 196, cy = 138;
      f.disc(cx, cy, 13, c565(0xF0E8C0));
      // bite out of the moon with the sky colour of that row
      for (int y = -12; y < 12; y += 2)
        for (int x = -12; x < 12; x += 2) {
          float dx = x + 1 - 6, dy = y + 1 + 4;
          if (dx * dx + dy * dy <= 11 * 11 && (x + 1) * (x + 1) + (y + 1) * (y + 1) <= 13 * 13)
            f.fill(cx + x, cy + y, 2, 2, rowA[cy + y]);
        }
      f.fill(cx - 8, cy + 2, 2, 2, c565(0xD8CCA0));
      f.fill(cx - 4, cy + 6, 2, 2, c565(0xD8CCA0));
    }
  }
  // clouds
  bool dark = kind == Sky::Rain || kind == Sky::Storm || kind == Sky::Drizzle;
  uint16_t cw = dark ? c565(day ? 0x98A0B0 : 0x485060) : c565(day ? 0xF8F8F8 : 0x707890);
  uint16_t cg = dark ? c565(day ? 0x707888 : 0x303848) : c565(day ? 0xC8D4E4 : 0x505870);
  for (int i = 0; i < nclouds; i++) cloud(f, snap(clouds[i].x), clouds[i].y, clouds[i].big, cw, cg);
  // fog
  if (kind == Sky::Fog) {
    for (int i = 0; i < 5; i++) {
      int y = snap(50 + i * 44 + sinf(t * 0.4f + i) * 4);
      int x = snap(fmodf(t * (6 + i * 2) + i * 60, SW + 140)) - 140;
      f.ditherRect(x, y, 160, 10, c565(0xE8ECF0));
      f.fill(x + 20, y + 2, 100, 6, c565(0xE0E4EA));
    }
  }
  // rain / snow
  if (kind == Sky::Snow) {
    uint16_t c = c565(0xFFFFFF);
    for (int i = 0; i < nparts; i++) {
      int x = snap(parts[i].x), y = snap(parts[i].y);
      f.fill(x, y, 2, 2, c);
      if (parts[i].s == 2) {
        f.fill(x - 2, y, 2, 2, c); f.fill(x + 2, y, 2, 2, c);
        f.fill(x, y - 2, 2, 2, c); f.fill(x, y + 2, 2, 2, c);
      }
    }
  } else if (nparts) {
    uint16_t c = c565(day ? 0xC8DCF8 : 0x7890C0);
    int len = kind == Sky::Drizzle ? 4 : 8;
    for (int i = 0; i < nparts; i++) f.fill(snap(parts[i].x), snap(parts[i].y), 2, len, c);
  }
  // lightning bolt: zigzag of 4 px blocks
  if (kind == Sky::Storm && flash > 0.55f) {
    int x = boltX, y = 0;
    uint16_t c = c565(0xFFF8A0);
    for (int i = 0; i < 12; i++) {
      f.fill(x, y, 4, 12, c);
      y += 12;
      x += (i % 3 == 2) ? 8 : -4;
    }
  }
}

void WeatherFx::drawFlash(Frame& f) {
  if (kind == Sky::Storm && flash > 0) f.blendAll(c565(0xFFFFFF), (uint8_t)(120 * flash));
}
