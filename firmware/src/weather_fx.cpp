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
    case Sky::Clear: return day ? "Sunny" : "Clear";
    case Sky::Partly: return day ? "Some sun" : "Few clouds";
    case Sky::Cloudy: return "Cloudy";
    case Sky::Fog: return "Foggy";
    case Sky::Drizzle: return "Drizzle";
    case Sky::Rain: return "Rain";
    case Sky::Snow: return "Snow";
    case Sky::Storm: return "Storms";
  }
  return "";
}

static float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }

// top and bottom sky colours
static void skyColors(Sky s, bool day, uint32_t& top, uint32_t& bot) {
  if (day) {
    switch (s) {
      case Sky::Clear: top = 0x3F8CE0; bot = 0xA9D6F7; return;
      case Sky::Partly: top = 0x5596D6; bot = 0xB6D7EF; return;
      case Sky::Cloudy: top = 0x8193A8; bot = 0xC3CDD8; return;
      case Sky::Fog: top = 0xA4ADB6; bot = 0xD6DADF; return;
      case Sky::Drizzle: top = 0x6F7F92; bot = 0xA9B5C2; return;
      case Sky::Rain: top = 0x56677B; bot = 0x95A3B2; return;
      case Sky::Snow: top = 0x98ACC2; bot = 0xE4EBF2; return;
      case Sky::Storm: top = 0x343B49; bot = 0x66717F; return;
    }
  } else {
    switch (s) {
      case Sky::Clear: top = 0x070E26; bot = 0x26406E; return;
      case Sky::Partly: top = 0x0B1430; bot = 0x2F4670; return;
      case Sky::Snow: top = 0x26304A; bot = 0x56677F; return;
      case Sky::Storm: top = 0x0E1018; bot = 0x2A303C; return;
      default: top = 0x161C2A; bot = 0x3A4458; return;
    }
  }
}

void WeatherFx::set(Sky s, bool d) {
  if (inited && s == kind && d == day) return;
  kind = s;
  day = d;
  reset();
}

void WeatherFx::reset() {
  inited = true;
  uint32_t top, bot;
  skyColors(kind, day, top, bot);
  for (int y = 0; y < SH; y++) rows[y] = lerp565(c565(top), c565(bot), (uint8_t)(y * 255 / (SH - 1)));

  nclouds = 0;
  int nc = kind == Sky::Partly ? 2 : (kind == Sky::Clear || kind == Sky::Snow || kind == Sky::Fog) ? (kind == Sky::Snow ? 2 : 0) : 4;
  for (int i = 0; i < nc; i++) clouds[nclouds++] = {rnd(-40, SW), rnd(40, 150), rnd(0.8f, 1.3f), rnd(5, 11)};

  nparts = 0;
  int n = 0;
  if (kind == Sky::Rain || kind == Sky::Storm) n = 70;
  else if (kind == Sky::Drizzle) n = 36;
  else if (kind == Sky::Snow) n = 55;
  for (int i = 0; i < n && i < MAXP; i++) {
    bool snow = kind == Sky::Snow;
    parts[nparts++] = {rnd(0, SW), rnd(0, SH), snow ? rnd(18, 42) : rnd(260, 380), snow ? rnd(1.0f, 2.6f) : rnd(0.6f, 1.0f)};
  }
  for (auto& s : stars) s = {(int16_t)rnd(4, SW - 4), (int16_t)rnd(30, 170), (uint8_t)rnd(0, 255)};
  flash = 0;
  nextFlash = rnd(3, 7);
}

void WeatherFx::update(float dt) {
  if (!inited) reset();
  t += dt;
  for (int i = 0; i < nclouds; i++) {
    Cloud& c = clouds[i];
    c.x += c.v * dt;
    if (c.x - 60 * c.s > SW) c.x = -60 * c.s;
  }
  bool snow = kind == Sky::Snow;
  for (int i = 0; i < nparts; i++) {
    P& p = parts[i];
    p.y += p.v * dt;
    p.x += snow ? sinf(t * 1.3f + i) * 12 * dt : -p.v * 0.18f * dt;
    if (p.y > SH + 10) {
      p.y = rnd(-20, -2);
      p.x = rnd(0, SW + 40);
    }
    if (p.x < -10) p.x += SW + 20;
    if (p.x > SW + 10) p.x -= SW + 20;
  }
  if (kind == Sky::Storm) {
    flash = max(0.0f, flash - dt * 3.5f);
    nextFlash -= dt;
    if (nextFlash <= 0) {
      flash = 1;
      boltX = rnd(30, SW - 30);
      nextFlash = rnd(4, 9);
    }
  }
}

static void drawCloud(Frame& f, float x, float y, float s, uint16_t c, uint8_t a) {
  // overlapping puffs; drawn with the same alpha so overlaps get a little denser (soft look)
  f.blendCircle(x, y, 18 * s, c, a);
  f.blendCircle(x + 20 * s, y - 10 * s, 22 * s, c, a);
  f.blendCircle(x + 44 * s, y - 2 * s, 17 * s, c, a);
  f.blendRRect((int)(x - 4 * s), (int)(y - 2 * s), (int)(56 * s), (int)(18 * s), (int)(9 * s), c, a);
}

void WeatherFx::draw(Frame& f) {
  f.sky(rows);
  // sun / moon + stars
  if (day && (kind == Sky::Clear || kind == Sky::Partly)) {
    float cx = 200, cy = 132, pulse = 1 + 0.04f * sinf(t * 1.5f);
    f.blendCircle(cx, cy, 34 * pulse, c565(0xFFF3B0), 60);
    f.blendCircle(cx, cy, 24 * pulse, c565(0xFFE27A), 110);
    for (int i = 0; i < 10; i++) {
      float a = t * 0.25f + i * (float)M_PI / 5;
      f.blendLine(cx + cosf(a) * 22, cy + sinf(a) * 22, cx + cosf(a) * 32, cy + sinf(a) * 32, c565(0xFFD24A), 200);
    }
    f.blendCircle(cx, cy, 16, c565(0xFFD84D), 255);
  } else if (!day && kind != Sky::Storm) {
    for (auto& s : stars) {
      if (kind != Sky::Clear && kind != Sky::Partly && s.ph & 1) continue;
      uint8_t a = 120 + (uint8_t)(100 * (0.5f + 0.5f * sinf(t * 2 + s.ph)));
      f.blendCircle(s.x, s.y, 1.1f, c565(0xFFFFFF), a);
    }
    if (kind == Sky::Clear || kind == Sky::Partly) {
      float cx = 196, cy = 132;
      f.blendCircle(cx, cy, 26, c565(0xF4F1DE), 40);
      f.blendCircle(cx, cy, 15, c565(0xF4F1DE), 255);
      f.blendCircle(cx + 7, cy - 5, 13, rows[(int)cy], 255);  // crescent: bite out with the sky colour
    }
  }
  // clouds
  bool dark = kind == Sky::Rain || kind == Sky::Storm || kind == Sky::Drizzle;
  uint16_t cc = dark ? c565(day ? 0x7B8796 : 0x3B4352) : c565(day ? 0xFFFFFF : 0x6A7387);
  for (int i = 0; i < nclouds; i++) drawCloud(f, clouds[i].x, clouds[i].y, clouds[i].s, cc, dark ? 200 : 170);
  // fog bands
  if (kind == Sky::Fog) {
    for (int i = 0; i < 4; i++) {
      float y = 60 + i * 50 + sinf(t * 0.4f + i) * 6;
      float x = fmodf(t * (8 + i * 3) + i * 70, SW + 120) - 120;
      f.blendRRect((int)x, (int)y, 200, 16, 8, c565(0xF2F4F6), 90);
      f.blendRRect((int)x - 160, (int)y + 22, 180, 12, 6, c565(0xF2F4F6), 70);
    }
  }
  // rain / snow
  if (kind == Sky::Snow) {
    for (int i = 0; i < nparts; i++) f.blendCircle(parts[i].x, parts[i].y, parts[i].s, c565(0xFFFFFF), 230);
  } else if (nparts) {
    uint16_t rc = c565(day ? 0xDCE8F5 : 0x9FB3CC);
    float len = kind == Sky::Drizzle ? 6 : 11;
    for (int i = 0; i < nparts; i++) {
      P& p = parts[i];
      f.blendLine(p.x, p.y, p.x + len * 0.18f, p.y - len, rc, (uint8_t)(170 * p.s));
    }
  }
  // lightning bolt
  if (kind == Sky::Storm && flash > 0.6f) {
    float x = boltX, y = 0;
    for (int i = 0; i < 6; i++) {
      float nx = x + ((i & 1) ? 10 : -12), ny = y + 22;
      f.blendLine(x, y, nx, ny, c565(0xFFF9C4), 255);
      f.blendLine(x + 1, y, nx + 1, ny, c565(0xFFF9C4), 200);
      x = nx, y = ny;
    }
  }
}

void WeatherFx::drawFlash(Frame& f) {
  if (kind == Sky::Storm && flash > 0) f.blendAll(c565(0xFFFFFF), (uint8_t)(110 * flash));
}
