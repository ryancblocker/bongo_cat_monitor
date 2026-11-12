// Bongo cat behaviour, driven by the PC app's typing commands:
//   SPEED:<ms>   typing; paws alternate every <ms> (the app sends 30..500, faster typing = smaller)
//   STOP         stopped typing: paws up, wait
//   IDLE_START   let the cat drift off: paws hidden -> sleepy -> asleep with Zzz
//   STREAK_ON/OFF  happy face while typing fast
// If the PC goes quiet for 5 s the cat drifts off on its own.
#pragma once
#include <stddef.h>
#include "cat_assets.h"

class CatAnim {
 public:
  void typing(uint16_t speedMs, uint32_t now);
  void stop(uint32_t now);
  void idleStart(uint32_t now);
  void streak(bool on) { streakOn = on; }
  void poke(uint32_t now);  // screen tapped: a happy double-tap
  void update(uint32_t now, uint8_t sleepMin, uint32_t lastPcMsg);
  // layers to draw, back to front (L_COUNT = none)
  CatLayer body() const { return twitching ? L_BODY_TWITCH : L_BODY; }
  CatLayer face() const;
  CatLayer paws() const;
  CatLayer fx() const;
  bool asleep() const { return stage >= 3; }
  void debug(char* out, size_t n, uint32_t now) const;

 private:
  enum Stage : uint8_t { AWAKE = 0, HANDS_DOWN = 1, DROWSY = 2, ASLEEP = 3 };
  uint8_t stage = AWAKE;
  bool typingNow = false, progress = false, streakOn = false;
  uint16_t speed = 200;
  uint32_t stageAt = 0, lastType = 0, pawAt = 0;
  uint8_t pawFrame = 0;  // 0 left down, 1 up, 2 right down, 3 up
  bool blinking = false, twitching = false;
  uint32_t blinkAt = 3000, blinkEnd = 0, twitchAt = 12000, twitchEnd = 0;
  uint32_t pokeEnd = 0, zzzAt = 0;
  uint8_t zzz = 0;
  void setStage(uint8_t s, uint32_t now) { stage = s; stageAt = now; }
};
