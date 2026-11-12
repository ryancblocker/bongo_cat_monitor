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
  look = 0;
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
  pokeEnd = now + 1400;
  heartAt = now;
  heart = 0;
  pawAt = now;
  pawFrame = 0;
  mood = min(100.0f, mood + 8);
  setStage(AWAKE, now);
}

void CatAnim::update(uint32_t now, float dt, uint8_t sleepMin, uint32_t lastPcMsg) {
  nowMs = now;
  // safety: no typing command for 2 s -> paws stop; PC silent for 5 s -> drift off by ourselves
  if (typingNow && now - lastType > 2000) typingNow = false;
  if (now - lastPcMsg > 5000) progress = true;

  // mood: typing (or petting) fills it, idling drains it over ~20 minutes
  if (typingNow) mood = min(100.0f, mood + dt * 1.5f);
  else mood = max(0.0f, mood - dt * 0.08f);

  if (progress && !typingNow && now > pokeEnd) {
    uint32_t total = (uint32_t)max<uint8_t>(sleepMin, 1) * 60000UL;
    uint32_t s2 = constrain(total / 5, 5000UL, 60000UL), s3 = constrain(total / 10, 3000UL, 30000UL);
    uint32_t s1 = total > s2 + s3 ? total - s2 - s3 : 1000;
    uint32_t in = now - stageAt;
    if (stage == AWAKE && in > s1) setStage(HANDS_DOWN, now);
    else if (stage == HANDS_DOWN && in > s2) {
      setStage(DROWSY, now);
      yawnEnd = now + 1600;  // big yawn on the way to sleep
    } else if (stage == DROWSY && in > s3) setStage(ASLEEP, now);
  }

  bool pawsMoving = typingNow || now < pokeEnd;
  uint16_t step = now < pokeEnd ? 120 : speed;
  if (pawsMoving && now - pawAt >= step) {
    pawFrame = (pawFrame + 1) & 3;
    pawAt = now;
  }

  // blink every 3-8 s while awake; ear twitch every 10-30 s
  if (!blinking && now >= blinkAt && stage < DROWSY) {
    blinking = true;
    blinkEnd = now + 140;
  } else if (blinking && now >= blinkEnd) {
    blinking = false;
    blinkAt = now + random(2500, 7000);
  }
  if (!twitching && now >= twitchAt) {
    twitching = true;
    twitchEnd = now + 350;
  } else if (twitching && now >= twitchEnd) {
    twitching = false;
    twitchAt = now + random(8000, 25000);
  }
  // glance left/right now and then when not typing
  if (look == 0 && now >= lookAt && !typingNow && stage == AWAKE) {
    look = random(2) ? 1 : -1;
    lookEnd = now + random(900, 1800);
  } else if (look != 0 && (now >= lookEnd || typingNow)) {
    look = 0;
    lookAt = now + random(5000, 14000);
  }
  if (now - zzzAt > 650) {
    zzz = (zzz + 1) % 3;
    zzzAt = now;
  }
  if (now - heartAt > 350) {
    heart ^= 1;
    heartAt = now;
  }
}

int CatAnim::bob() const {
  // slow breath while idle/asleep, quick bounce in step with the paws while typing
  if (typingNow) return (pawFrame & 1) ? 0 : 2;
  uint32_t period = stage >= DROWSY ? 3200 : 2200;
  return (nowMs % period) < period / 2 ? 0 : 2;
}

CatLayer CatAnim::face() const {
  if (nowMs < yawnEnd) return L_FACE_YAWN;
  if (stage >= DROWSY) return L_FACE_SLEEPY;
  if (blinking) return L_FACE_BLINK;
  if (nowMs < pokeEnd || (streakOn && typingNow)) return L_FACE_HAPPY;
  if (look < 0) return L_FACE_LOOK_L;
  if (look > 0) return L_FACE_LOOK_R;
  return L_FACE_STOCK;
}

CatLayer CatAnim::paws() const {
  if (stage >= HANDS_DOWN) return L_COUNT;  // hands under the desk
  if (typingNow || nowMs < pokeEnd) {
    if (pawFrame == 0) return L_PAWS_LEFT;
    if (pawFrame == 2) return L_PAWS_RIGHT;
  }
  return L_PAWS_UP;
}

CatLayer CatAnim::fx() const {
  if (stage == ASLEEP) return (CatLayer)(L_FX_ZZZ0 + zzz);
  // impact lines on every paw strike while typing (like the original)
  if (typingNow && pawFrame == 0) return L_FX_LEFT;
  if (typingNow && pawFrame == 2) return L_FX_RIGHT;
  return L_COUNT;
}

CatLayer CatAnim::fx2() const {
  return nowMs < pokeEnd ? (heart ? L_FX_HEART1 : L_FX_HEART0) : L_COUNT;
}

void CatAnim::debug(char* out, size_t n, uint32_t now) const {
  snprintf(out, n, "stage=%d typing=%d progress=%d speed=%d inStage=%lu sinceType=%lu face=%d paws=%d fx=%d mood=%.0f",
           stage, typingNow, progress, speed, (unsigned long)(now - stageAt), (unsigned long)(now - lastType), face(), paws(), fx(), mood);
}
