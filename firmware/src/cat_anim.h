// Bongo cat behaviour, driven by the PC app's typing commands:
//   SPEED:<ms>   typing; paws alternate every <ms> (the app sends 30..500, faster typing = smaller)
//   STOP         stopped typing: paws up, wait
//   IDLE_START   let the cat drift off: paws hidden -> yawn -> drowsy -> asleep with Zzz
//   STREAK_ON/OFF  happy face while typing fast
// While awake it breathes, blinks, twitches an ear and glances around. Tapping it makes
// it wave with hearts. Mood (0..4 hearts) fills while you type and slowly drains.
#pragma once
#include <stddef.h>

#include "cat_assets.h"

class CatAnim {
 public:
  void typing(uint16_t speedMs, uint32_t now);
  void stop(uint32_t now);
  void idleStart(uint32_t now);
  void streak(bool on) { streakOn = on; }
  void poke(uint32_t now);
  void celebrate(uint32_t now);  // level up: long happy wave with hearts, mood full
  void force(uint8_t stage, uint32_t now, bool yawn = false);  // demo: jump to a sleep stage
  void update(uint32_t now, float dt, uint8_t sleepMin, uint32_t lastPcMsg);

  // layers to draw, back to front (L_COUNT = none)
  CatLayer body() const { return twitching ? L_BODY_TWITCH : L_BODY; }
  CatLayer face() const;
  CatLayer paws() const;
  CatLayer fx() const;
  CatLayer fx2() const;  // hearts when tapped
  int bob() const;       // breathing: 0 or 2 px down
  int hearts() const {   // mood as 0..4 hearts
    if (mood <= 0) return 0;
    int h = (int)(mood / 25.0f + 0.999f);
    return h > 4 ? 4 : h;
  }
  bool asleep() const { return stage >= ASLEEP; }
  void debug(char* out, size_t n, uint32_t now) const;

 private:
  enum Stage : uint8_t { AWAKE = 0, HANDS_DOWN = 1, DROWSY = 2, ASLEEP = 3 };
  uint8_t stage = AWAKE;
  bool typingNow = false, progress = false, streakOn = false;
  uint16_t speed = 200;
  uint32_t stageAt = 0, lastType = 0, pawAt = 0, nowMs = 0;
  uint8_t pawFrame = 0;  // 0 left down, 1 up, 2 right down, 3 up
  bool blinking = false, twitching = false;
  uint32_t blinkAt = 3000, blinkEnd = 0, twitchAt = 12000, twitchEnd = 0;
  int8_t look = 0;  // -1 left, 0 ahead, 1 right
  uint32_t lookAt = 6000, lookEnd = 0;
  uint32_t yawnEnd = 0, pokeEnd = 0, zzzAt = 0, heartAt = 0;
  uint8_t zzz = 0, heart = 0;
  float mood = 60;  // 0..100
  void setStage(uint8_t s, uint32_t now) { stage = s; stageAt = now; }
};
