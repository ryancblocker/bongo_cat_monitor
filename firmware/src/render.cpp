#include "render.h"

#include <math.h>

static inline uint16_t sw(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

// classic RGB565 alpha blend, a = 0..255
uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a) {
  uint32_t a5 = (a + 4) >> 3;  // 0..32
  uint32_t bg = (dst | ((uint32_t)dst << 16)) & 0x07E0F81Fu;
  uint32_t fg = (src | ((uint32_t)src << 16)) & 0x07E0F81Fu;
  uint32_t r = ((((fg - bg) * a5) >> 5) + bg) & 0x07E0F81Fu;
  return (uint16_t)((r >> 16) | r);
}

uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t) { return blend565(a, b, t); }

// pixel sizes of the generated fonts (for vertical centring)
static int fontPx(const uint8_t* f) {
  if (f == INTER_11) return 11;
  if (f == INTER_13) return 13;
  if (f == INTER_15B) return 15;
  if (f == INTER_17B) return 17;
  if (f == INTER_22B) return 22;
  if (f == INTER_40B) return 40;
  return 13;
}

void Renderer::begin() {
  for (int i = 0; i < 2; i++) {
    sprites[i].setColorDepth(16);
    sprites[i].createSprite(SW, STRIP);
    sprites[i].setTextWrap(false);
    frames[i].spr = &sprites[i];
    frames[i].buf = (uint16_t*)sprites[i].getBuffer();
  }
}

void Frame::setClip(int x, int y, int w, int h) {
  cx0 = max(0, x);
  cy0 = max(0, y);
  cx1 = min(SW, x + w);
  cy1 = min(SH, y + h);
  spr->setClipRect(cx0, cy0 - oy, cx1 - cx0, cy1 - cy0);
}
void Frame::clearClip() {
  cx0 = 0, cy0 = 0, cx1 = SW, cy1 = SH;
  spr->clearClipRect();
}

inline void Frame::px(int x, int y, uint16_t c, uint8_t a) {
  if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 || y < oy || y >= oy + STRIP || a == 0) return;
  uint16_t& d = buf[(y - oy) * SW + x];
  d = a >= 250 ? sw(c) : sw(blend565(sw(d), c, a));
}

void Frame::sky(const uint16_t* rows) {
  for (int r = 0; r < STRIP; r++) {
    uint16_t v = sw(rows[oy + r]);
    uint16_t* p = buf + r * SW;
    for (int x = 0; x < SW; x++) p[x] = v;
  }
}

void Frame::blendAll(uint16_t c, uint8_t a) {
  for (int i = 0; i < SW * STRIP; i++) buf[i] = sw(blend565(sw(buf[i]), c, a));
}

void Frame::blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
  int y0 = max(max(y, oy), cy0), y1 = min(min(y + h, oy + STRIP), cy1);
  int x0 = max(x, cx0), x1 = min(x + w, cx1);
  for (int yy = y0; yy < y1; yy++) {
    uint16_t* p = buf + (yy - oy) * SW;
    for (int xx = x0; xx < x1; xx++) p[xx] = sw(blend565(sw(p[xx]), c, a));
  }
}

// rounded rectangle with anti-aliased corners
void Frame::blendRRect(int x, int y, int w, int h, int r, uint16_t c, uint8_t a) {
  if (!vis(y, h)) return;
  int y0 = max(max(y, oy), cy0), y1 = min(min(y + h, oy + STRIP), cy1);
  for (int yy = y0; yy < y1; yy++) {
    // horizontal inset at this row from the corner circles
    float dy = 0;
    if (yy < y + r) dy = (y + r) - (yy + 0.5f);
    else if (yy >= y + h - r) dy = (yy + 0.5f) - (y + h - r);
    float inset = dy > 0 ? r - sqrtf(max(0.0f, r * r - dy * dy)) : 0;
    float lx = x + inset, rx = x + w - inset;
    int xs = max((int)floorf(lx), cx0), xe = min((int)ceilf(rx), cx1);
    uint16_t* p = buf + (yy - oy) * SW;
    for (int xx = xs; xx < xe; xx++) {
      float cov = 1;
      if (xx < lx) cov = 1 - (lx - xx);
      else if (xx + 1 > rx) cov = rx - xx;
      if (cov <= 0) continue;
      uint8_t aa = (uint8_t)(a * min(1.0f, cov));
      p[xx] = sw(blend565(sw(p[xx]), c, aa));
    }
  }
}

void Frame::blendCircle(float cx, float cy, float r, uint16_t c, uint8_t a) {
  if (!vis((int)(cy - r - 1), (int)(2 * r + 3))) return;
  int y0 = max(max((int)floorf(cy - r), oy), cy0), y1 = min(min((int)ceilf(cy + r) + 1, oy + STRIP), cy1);
  for (int yy = y0; yy < y1; yy++) {
    float dy = yy + 0.5f - cy;
    float span = r * r - dy * dy;
    if (span <= 0) continue;
    float hw = sqrtf(span);
    int xs = max((int)floorf(cx - hw), cx0), xe = min((int)ceilf(cx + hw), cx1);
    uint16_t* p = buf + (yy - oy) * SW;
    for (int xx = xs; xx < xe; xx++) {
      float dx = xx + 0.5f - cx;
      float d = r - sqrtf(dx * dx + dy * dy);  // >0 inside
      if (d <= 0) continue;
      uint8_t aa = d >= 1 ? a : (uint8_t)(a * d);
      p[xx] = sw(blend565(sw(p[xx]), c, aa));
    }
  }
}

// thin anti-aliased line (Xiaolin Wu)
void Frame::blendLine(float x0, float y0, float x1, float y1, uint16_t c, uint8_t a) {
  if (!vis((int)min(y0, y1) - 1, (int)fabsf(y1 - y0) + 3)) return;
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
  if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
  float dx = x1 - x0, g = dx == 0 ? 1 : (y1 - y0) / dx;
  float y = y0;
  for (int x = (int)roundf(x0); x <= (int)roundf(x1); x++) {
    int yi = (int)floorf(y);
    float f = y - yi;
    uint8_t a1 = (uint8_t)(a * (1 - f)), a2 = (uint8_t)(a * f);
    if (steep) { px(yi, x, c, a1); px(yi + 1, x, c, a2); }
    else { px(x, yi, c, a1); px(x, yi + 1, c, a2); }
    y += g;
  }
}

// one layer of the cat: run-length rows, drawn at img->scale
void Frame::cat(const CatImg* img) {
  if (!img) return;
  int sc = img->scale;
  int top = CAT_Y + img->y;
  if (!vis(top, img->h * sc)) return;
  int r0 = max(0, (max(oy, cy0) - top) / sc);
  int r1 = min((int)img->h, (min(oy + STRIP, cy1) - top + sc - 1) / sc);
  for (int ry = r0; ry < r1; ry++) {
    const uint8_t* d = img->data + img->rows[ry];
    int x = 0;
    while (x < img->w) {
      uint8_t n = d[0], idx = d[1];
      d += 2;
      if (idx) {
        uint16_t c = img->pal[idx];
        uint8_t a = img->alpha[idx];
        for (int sy = 0; sy < sc; sy++) {
          int yy = top + ry * sc + sy;
          if (yy < oy || yy >= oy + STRIP || yy < cy0 || yy >= cy1) continue;
          uint16_t* p = buf + (yy - oy) * SW;
          int xs = img->x + x * sc, xe = xs + n * sc;
          xs = max(xs, cx0);
          xe = min(xe, cx1);
          if (a >= 250) {
            uint16_t v = sw(c);
            for (int xx = xs; xx < xe; xx++) p[xx] = v;
          } else {
            for (int xx = xs; xx < xe; xx++) p[xx] = sw(blend565(sw(p[xx]), c, a));
          }
        }
      }
      x += n;
    }
  }
}

// ------------------------------------------------------------- LovyanGFX wrappers
void Frame::font(const uint8_t* f) {
  if (f == cur) return;
  if (cur) spr->unloadFont();
  spr->loadFont(f);
  cur = f;
}

int Frame::textWidth(const char* s, const uint8_t* f) {
  font(f);
  return spr->textWidth(s);
}

void Frame::text(const char* s, int x, int cy, const uint8_t* f, uint32_t color, Align al, int maxw) {
  if (!s || !*s) return;
  int px_ = fontPx(f);
  if (!vis(cy - px_, 2 * px_)) return;
  font(f);
  String str = s;
  if (maxw > 0 && spr->textWidth(str) > maxw) {
    while (str.length() > 1 && spr->textWidth(str + "\xE2\x80\xA6") > maxw) {
      int i = str.length() - 1;
      while (i > 0 && (str[i] & 0xC0) == 0x80) i--;
      str.remove(i);
    }
    str.trim();
    str += "\xE2\x80\xA6";
  }
  spr->setTextColor(color);
  spr->setTextDatum(al == AL_L ? textdatum_t::baseline_left
                               : (al == AL_C ? textdatum_t::baseline_center : textdatum_t::baseline_right));
  int baseline = cy + (int)lroundf(px_ * 0.36f);  // Inter cap height ~0.73 em
  spr->drawString(str, x, baseline - oy);
}

void Frame::fillCircle(int cx, int cy, int r, uint32_t c) {
  if (vis(cy - r, 2 * r + 1)) spr->fillSmoothCircle(cx, cy - oy, r, c);
}
void Frame::ring(int cx, int cy, int r, int t, uint32_t c) {
  if (vis(cy - r, 2 * r + 1)) spr->fillArc(cx, cy - oy, r, r - t, 0, 360, c);
}
void Frame::wideLine(int x0, int y0, int x1, int y1, float r, uint32_t c) {
  int top = min(y0, y1) - (int)r - 1, bot = max(y0, y1) + (int)r + 1;
  if (vis(top, bot - top + 1)) spr->drawWideLine(x0, y0 - oy, x1, y1 - oy, r, c);
}
void Frame::fillRRect(int x, int y, int w, int h, int r, uint32_t c) {
  if (vis(y, h)) spr->fillSmoothRoundRect(x, y - oy, w, h, r, c);
}
void Frame::triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
  int top = min(y0, min(y1, y2)), bot = max(y0, max(y1, y2));
  if (vis(top, bot - top + 1)) spr->fillTriangle(x0, y0 - oy, x1, y1 - oy, x2, y2 - oy, c);
}
void Frame::arc(int cx, int cy, int r0, int r1, float a0, float a1, uint32_t c) {
  if (vis(cy - r0, 2 * r0 + 1)) spr->fillArc(cx, cy - oy, r0, r1, a0, a1, c);
}
void Frame::qr(const char* txt, int x, int y, int w) {
  if (vis(y, w)) spr->qrcode(txt, x, y - oy, w, 3);
}
