#include "cat_anim.h"

#include <Arduino.h>

void CatAnim::typing(uint16_t ms, uint32_t now) {
  if (ms == 0) return stop(now);
  if (!typingNow) {
    pawFrame = 0;
    pawAt = now;
  }
  speed = constrain(ms, 30, 600);
  typingNow = true;
  progress = false;
  lastType = now;
  setStage(AWAKE, now);
}

void CatAnim::stop(uint32_t now) {
  typingNow = false;
  progress = false;
  setStage(AWAKE, now);
}

void CatAnim::idleStart(uint32_t now) {
  typingNow = false;
  if (!progress) setStage(AWAKE, now);
  progress = true;
}

void CatAnim::poke(uint32_t now) {
  pokeEnd = now + 900;
  pawAt = now;
  pawFrame = 0;
  if (stage) setStage(AWAKE, now);
  stageAt = now;
}

void CatAnim::update(uint32_t now, uint8_t sleepMin, uint32_t lastPcMsg) {
  // safety: no typing command for 2 s -> paws stop; PC silent for 5 s -> drift off by ourselves
  if (typingNow && now - lastType > 2000) typingNow = false;
  if (now - lastPcMsg > 5000) progress = true;

  if (progress && !typingNow && now > pokeEnd) {
    uint32_t total = (uint32_t)max<uint8_t>(sleepMin, 1) * 60000UL;
    // stage lengths: most of the wait awake, then hands down, drowsy, asleep
    uint32_t s2 = constrain(total / 5, 5000UL, 60000UL), s3 = constrain(total / 10, 3000UL, 30000UL);
    uint32_t s1 = total > s2 + s3 ? total - s2 - s3 : 1000;
    uint32_t in = now - stageAt;
    if (stage == AWAKE && in > s1) setStage(HANDS_DOWN, now);
    else if (stage == HANDS_DOWN && in > s2) setStage(DROWSY, now);
    else if (stage == DROWSY && in > s3) setStage(ASLEEP, now);
  }

  bool pawsMoving = typingNow || now < pokeEnd;
  uint16_t step = now < pokeEnd ? 110 : speed;
  if (pawsMoving && now - pawAt >= step) {
    pawFrame = (pawFrame + 1) & 3;
    pawAt = now;
  }

  // blink every 3-8 s while awake; ear twitch every 10-30 s
  if (!blinking && now >= blinkAt && stage < DROWSY) {
    blinking = true;
    blinkEnd = now + 160;
  } else if (blinking && now >= blinkEnd) {
    blinking = false;
    blinkAt = now + random(3000, 8000);
  }
  if (!twitching && now >= twitchAt) {
    twitching = true;
    twitchEnd = now + 400;
  } else if (twitching && now >= twitchEnd) {
    twitching = false;
    twitchAt = now + random(10000, 30000);
  }
  if (now - zzzAt > 700) {
    zzz = (zzz + 1) % 3;
    zzzAt = now;
  }
}

CatLayer CatAnim::face() const {
  if (stage >= DROWSY) return L_FACE_SLEEPY;
  if (blinking) return L_FACE_BLINK;
  if (millis() < pokeEnd || (streakOn && typingNow)) return L_FACE_HAPPY;
  return L_FACE_STOCK;
}

CatLayer CatAnim::paws() const {
  if (stage >= HANDS_DOWN) return L_COUNT;  // hands under the desk
  if (typingNow || millis() < pokeEnd) {
    if (pawFrame == 0) return L_PAWS_LEFT;
    if (pawFrame == 2) return L_PAWS_RIGHT;
  }
  return L_PAWS_UP;
}

CatLayer CatAnim::fx() const {
  if (stage == ASLEEP) return (CatLayer)(L_FX_ZZZ0 + zzz);
  bool fast = typingNow && speed <= 150;  // smaller step = faster typing
  if (fast && pawFrame == 0) return L_FX_LEFT;
  if (fast && pawFrame == 2) return L_FX_RIGHT;
  return L_COUNT;
}

void CatAnim::debug(char* out, size_t n, uint32_t now) const {
  snprintf(out, n, "stage=%d typing=%d progress=%d speed=%d inStage=%lu sinceType=%lu face=%d paws=%d fx=%d",
           stage, typingNow, progress, speed, (unsigned long)(now - stageAt), (unsigned long)(now - lastType), face(), paws(), fx());
}
