#pragma once
#include "cat_anim.h"
#include "render.h"
#include "weather_fx.h"

// layout (screen px, all on the 2 px grid)
constexpr int HEADER_H = 22;
constexpr int CARD_X = 6, CARD_Y = 28, CARD_W = 228, CARD_H = 86;
constexpr int GEAR_X = 214, GEAR_Y = 3;  // top-left of the 16x16 gear
// settings screen brightness row: [<] [##########] [>]
constexpr int BR_Y = 268, BR_MINUS_X = 14, BR_BAR_X = 54, BR_BAR_W = 132, BR_PLUS_X = 196;

enum Card : uint8_t { CARD_WEATHER = 0, CARD_PC = 1, CARD_FOCUS = 2 };
// big START button on the focus card (screen coordinates); the rest of the card is left/right halves
constexpr int FC_BTN_X = CARD_X + 46, FC_BTN_Y = CARD_Y + 44, FC_BTN_W = 136, FC_BTN_H = 34;

struct MainView {
  int card = CARD_WEATHER;  // card showing (or sliding out)
  int next = -1;            // card sliding in, -1 = none
  float slide = 0;          // 0..1 transition progress
  int dir = 1;              // +1 slide left (next card), -1 slide right (previous card)
  int nCards = 3;
  uint32_t t = 0;           // millis(), for blinking arrows
};

void drawMain(Frame& f, WeatherFx& fx, const CatAnim& cat, const MainView& v);
void drawSettings(Frame& f, uint32_t t);
void drawMessage(Frame& f, const char* title, const char* sub);
void drawCaption(Frame& f, const char* text, uint32_t t);  // demo step label at the bottom
void timeString(char* out, size_t n);
