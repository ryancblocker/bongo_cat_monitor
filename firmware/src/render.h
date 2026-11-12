// Strip renderer: every frame is drawn into a 240x40 sprite eight times (one per
// horizontal strip) and pushed with DMA while the next strip is drawn. A full
// 240x320 16-bit frame (150 KB) doesn't fit in one block of ESP32 RAM.
#pragma once
#include "cat_assets.h"
#include "fonts.h"
#include "lgfx_cyd.h"

constexpr int SW = 240, SH = 320, STRIP = 40;
constexpr int CAT_Y = 120;  // top of the 240x200 cat area

enum Align : uint8_t { AL_L, AL_C, AL_R };

// RGB888 -> RGB565
constexpr uint16_t c565(uint32_t c) {
  return (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
}
uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t);  // t=0 -> a, 255 -> b

class Frame {
 public:
  LGFX_Sprite* spr = nullptr;
  uint16_t* buf = nullptr;  // sprite pixels, byte-swapped RGB565
  int oy = 0;               // screen y of the strip's first row

  bool vis(int y, int h) const { return y + h > oy && y < oy + STRIP && y + h > cy0 && y < cy1; }
  void setClip(int x, int y, int w, int h);
  void clearClip();

  // raw pixel ops (screen coordinates, clipped)
  void sky(const uint16_t* rows);  // one colour per screen row
  void blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a);
  void blendRRect(int x, int y, int w, int h, int r, uint16_t c, uint8_t a);
  void blendCircle(float cx, float cy, float r, uint16_t c, uint8_t a);
  void blendLine(float x0, float y0, float x1, float y1, uint16_t c, uint8_t a);
  void blendAll(uint16_t c, uint8_t a);  // whole strip (lightning flash)
  void cat(const CatImg* img, int dy = 0, int dx = 0);
  // silhouette of a cat layer in one colour (aura / glow); a = alpha 0..255
  void catTint(const CatImg* img, int dx, int dy, uint16_t c, uint8_t a);

  // pixel-art helpers (no anti-aliasing)
  void fill(int x, int y, int w, int h, uint16_t c);
  void disc(int cx, int cy, int r, uint16_t c);  // filled circle on the 2 px grid
  void ditherRect(int x, int y, int w, int h, uint16_t c);  // 50% checkerboard of 2 px cells
  // char-map sprite: each char is a palette key ('.' = transparent), drawn at `scale`
  void bitmap(const char* const* rows, int w, int h, int x, int y, int scale);
  void skyBands(const uint16_t* a, const uint16_t* b);  // per-row colours, b != a = dithered row

  // LovyanGFX drawing (RGB888 colours), offset into the strip
  void text(const char* s, int x, int cy, const uint8_t* font, uint32_t color, Align al = AL_L, int maxw = 0);
  int textWidth(const char* s, const uint8_t* font);
  void fillCircle(int cx, int cy, int r, uint32_t c);
  void ring(int cx, int cy, int r, int thick, uint32_t c);
  void wideLine(int x0, int y0, int x1, int y1, float r, uint32_t c);
  void fillRRect(int x, int y, int w, int h, int r, uint32_t c);
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
  void arc(int cx, int cy, int r0, int r1, float a0, float a1, uint32_t c);
  void qr(const char* txt, int x, int y, int w);

 private:
  int cx0 = 0, cy0 = 0, cx1 = SW, cy1 = SH;
  void font(const uint8_t* f);
  const uint8_t* cur = nullptr;
  inline void px(int x, int y, uint16_t c, uint8_t a);
};

class Renderer {
 public:
  explicit Renderer(LGFX& l) : lcd(l) {}
  void begin();
  // draw(Frame&) is called once per strip; capture=true also streams the pixels to Serial
  template <class F>
  void frame(F draw, bool capture = false) {
    if (capture) Serial.printf("\nSHOT:%dx%d\n", SW, SH);
    lcd.startWrite();
    for (int s = 0; s < SH / STRIP; s++) {
      Frame& f = frames[s & 1];
      f.oy = s * STRIP;
      f.clearClip();
      draw(f);
      f.clearClip();
      lcd.waitDMA();
      lcd.pushImageDMA(0, f.oy, SW, STRIP, (lgfx::swap565_t*)f.buf);
      if (capture) {
        lcd.waitDMA();
        Serial.write((const uint8_t*)f.buf, SW * STRIP * 2);
      }
    }
    lcd.waitDMA();
    lcd.endWrite();
    if (capture) Serial.print("\nSHOT_END\n");
  }
  LGFX& lcd;

 private:
  LGFX_Sprite sprites[2];
  Frame frames[2];
};

uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a);
