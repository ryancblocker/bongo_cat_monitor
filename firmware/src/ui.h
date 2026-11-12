#pragma once
#include "cat_anim.h"
#include "render.h"
#include "weather_fx.h"

// layout (screen px)
constexpr int HEADER_H = 30;
constexpr int CARD_X = 8, CARD_Y = 36, CARD_W = 224, CARD_H = 80;
constexpr int GEAR_X = 222, GEAR_Y = 15;
constexpr int SLIDER_X0 = 40, SLIDER_X1 = 198, SLIDER_Y = 296;

enum Card : uint8_t { CARD_WEATHER = 0, CARD_PC = 1 };

struct MainView {
  int card = CARD_WEATHER;  // card showing (or sliding out)
  int next = -1;            // card sliding in, -1 = none
  float slide = 0;          // 0..1 transition progress
  int nCards = 2;
};

void drawMain(Frame& f, WeatherFx& fx, const CatAnim& cat, const MainView& v);
void drawSettings(Frame& f, bool brightActive);
void drawMessage(Frame& f, const char* title, const char* sub);
void timeString(char* out, size_t n);
