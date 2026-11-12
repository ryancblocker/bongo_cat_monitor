// Bongo Cat (Ross edition) for the 2.8" ESP32-32E Cheap Yellow Display, portrait.
//
// PC -> board (one line each, 115200 baud):
//   TIME:HH:MM[:SS]   DATE:Thu, Nov 12
//   WX2:code=3|day=1|t=54|hi=60|lo=41|wind=8|wu=mph|hum=40|u=F|place=Highlands Ranch
//   PC:cpu=23|ram=41|wpm=82|cput=136|gpu=63|gput=160|u=F       (-1 = unknown)
//   SET:style=hd|coat=calico|name=Mochi|bright=200|cycle=8|cards=3|h24=0|sleep=5|focus=25
//   URL:http://192.168.1.20:8787/c/abc123/
//   FOCUS:<minutes>  start a focus session (FOCUS:0 stops it)
//   DEMO  tour of every screen and animation (DEMO:0 or a tap stops it)   LEVELUP  preview the level-up
//   SPEED:<ms>  STOP  IDLE_START  STREAK_ON  STREAK_OFF  HEARTBEAT  PING  SHOT  DBG  UI:settings
// board -> PC:
//   HELLO:bongo2 (boot)   PONG   EVT:bright=180   EVT:level=3
//   EVT:focus=start|min=25|left=1500   EVT:focus=stop   EVT:focus=done|level=4
//   SHOT:240x320 <raw> SHOT_END
#include <Arduino.h>
#include <Preferences.h>

#include "cat_anim.h"
#include "levelup_fx.h"
#include "render.h"
#include "state.h"
#include "ui.h"
#include "weather_fx.h"

LGFX lcd;
static Renderer rnd(lcd);
static WeatherFx fx;
static CatAnim cat;
static MainView view;

Settings settings;
Weather weather;
PcStats pc;
Clock clk;
char configUrl[96] = "";
Focus focus;
uint16_t catLevel = 1;
uint32_t levelUpUntil = 0;
uint32_t lastPcMsg = 0;

enum class Screen : uint8_t { Main, Settings };
static Screen screen = Screen::Main;
static bool settingsDirty = true, shotPending = false;
static uint32_t saveAt = 0, cardSince = 0, settingsSince = 0;
static float frameMs = 0;  // smoothed render time
static LevelUpFx levelUp;
static bool levelUpReal = false;  // false = demo/preview: don't show the "grew to" box after

// ------------------------------------------------------------------ demo tour
// Shows every screen and animation for ~1 minute without touching the real data:
// weather, PC stats, settings and the focus timer are backed up and restored, and
// anything the PC sends meanwhile goes into the backups.
struct Demo {
  bool on = false;
  int step = -1;
  uint32_t stepAt = 0;
  Weather wx;
  PcStats pc;
  Settings set;
  Focus fo;
} demo;

struct DemoStep { uint16_t ms; const char* caption; };
static const DemoStep STEPS[] = {
    {3000, "HELLO!"}, {3000, "TYPING"}, {3000, "TYPING FAST"}, {3500, "PC STATS"},
    {2500, "SOME SUN"}, {2500, "CLOUDY"}, {2500, "RAIN"}, {3500, "STORM"}, {2500, "SNOW"},
    {2500, "FOG"}, {3000, "STARRY NIGHT"}, {2500, "PET ME!"}, {2200, "YAWN..."}, {3500, "ZZZ..."},
    {3000, "FOCUS TIMER"}, {5200, "LEVEL UP!"},
    {1100, "WHITE"}, {1100, "BLACK"}, {1100, "GREY"}, {1100, "ORANGE TABBY"}, {1100, "CALICO"},
    {1100, "TUXEDO"}, {1100, "SIAMESE"}, {1100, "CREAM"}, {1600, "ORIGINAL 8-BIT"}, {3000, "OPTIONS"},
};
static const int N_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);
static const int STEP_COATS = 16, STEP_8BIT = 24, STEP_OPTIONS = 25, STEP_LEVELUP = 15;

static void demoWeather(int code, bool day) {
  weather.valid = true;
  weather.code = code;
  weather.day = day;
  weather.temp = code >= 71 && code <= 77 ? 28 : 54;
  weather.hi = weather.temp + 6;
  weather.lo = weather.temp - 13;
  weather.wind = code >= 95 ? 24 : 8;
  weather.hum = code >= 51 ? 88 : 40;
  if (!weather.place[0]) strlcpy(weather.place, "HIGHLANDS RANCH", sizeof weather.place);
}

static void demoStop() {
  if (!demo.on) return;
  demo.on = false;
  weather = demo.wx;
  pc = demo.pc;
  uint8_t bright = settings.bright;
  settings = demo.set;
  settings.bright = bright;
  focus = demo.fo;
  levelUp.cancel();
  screen = Screen::Main;
  view.next = -1;
  cardSince = millis();
  cat.stop(millis());
  settingsDirty = true;
  Serial.println("EVT:demo=end");
}

static void demoStart() {
  if (demo.on) return;
  demo = Demo();
  demo.wx = weather;
  demo.pc = pc;
  demo.set = settings;
  demo.fo = focus;
  demo.on = true;
  pc = PcStats();  // sample stats for the PC card
  pc.valid = true;
  pc.cpu = 37, pc.ram = 52, pc.wpm = 88, pc.cpuT = 140, pc.gpu = 71, pc.gpuT = 158;
  pc.at = millis();
  Serial.println("EVT:demo=start");
}

static void demoTick(uint32_t now) {
  if (!demo.on) return;
  if (demo.step < 0 || now - demo.stepAt >= STEPS[demo.step].ms) {
    demo.step++;
    demo.stepAt = now;
    if (demo.step >= N_STEPS) return demoStop();
    int s = demo.step;
    // things that happen once at the start of a step
    screen = s == STEP_OPTIONS ? Screen::Settings : Screen::Main;
    settingsDirty = true;
    if (s == 0) { demoWeather(0, true); view.card = CARD_WEATHER; cat.stop(now); }
    if (s == 3) view.card = CARD_PC;
    if (s == 4) { view.card = CARD_WEATHER; cat.stop(now); }
    static const int codes[] = {2, 3, 61, 95, 71, 45};
    if (s >= 4 && s <= 9) demoWeather(codes[s - 4], true);
    if (s == 10) demoWeather(0, false);
    if (s == 11) { demoWeather(0, true); cat.poke(now); }
    if (s == 14) { view.card = CARD_FOCUS; focus.active = true; focus.startMs = now - 7 * 60000UL; focus.durMs = 25 * 60000UL; }
    if (s == STEP_LEVELUP) { focus = demo.fo; levelUp.start(now, catLevel, catLevel + 1); levelUpReal = false; }
    if (s >= STEP_COATS && s < STEP_8BIT) { settings.style = STYLE_HD; settings.coat = s - STEP_COATS; cat.stop(now); }
    if (s == STEP_8BIT) settings.style = STYLE_EIGHTBIT;
    if (s == STEP_OPTIONS) settings.style = demo.set.style, settings.coat = demo.set.coat;
  }
  // things that hold for the whole step
  int s = demo.step;
  lastPcMsg = now;  // keep the cat from drifting off on its own
  cardSince = now;  // no auto-cycling: the script picks the card
  view.next = -1;
  if (s == 1 || s == 3) cat.typing(380, now);
  if (s == 2) { cat.streak(true); cat.typing(90, now); }
  if (s == 3) cat.streak(false);
  if (s == 12) cat.force(2, now, true);
  if (s == 13) cat.force(3, now);
}

// ------------------------------------------------------------------ settings in NVS
static const uint32_t SETTINGS_VER = 4;  // 4: focus length

static void loadSettings() {
  Preferences p;
  p.begin("bongo", true);
  if (p.getUInt("ver", 0) == SETTINGS_VER && p.getBytesLength("s") == sizeof(Settings)) p.getBytes("s", &settings, sizeof(Settings));
  catLevel = max<uint16_t>(1, p.getUShort("lvl", 1));
  p.end();
  settings.name[sizeof(settings.name) - 1] = 0;
}

static void saveLevel() {
  Preferences p;
  p.begin("bongo", false);
  p.putUShort("lvl", catLevel);
  p.end();
}

static void startFocus(int minutes) {
  minutes = constrain(minutes, 1, 180);
  focus.active = true;
  focus.startMs = millis();
  focus.durMs = (uint32_t)minutes * 60000UL;
  if (settings.cards & 4) {  // jump to the focus card
    view.card = CARD_FOCUS;
    view.next = -1;
    cardSince = millis();
  }
  Serial.printf("EVT:focus=start|min=%d|left=%d\n", minutes, minutes * 60);
}

static void stopFocus() {
  if (!focus.active) return;
  focus.active = false;
  Serial.println("EVT:focus=stop");
}

static void saveSettings() {
  Preferences p;
  p.begin("bongo", false);
  p.putUInt("ver", SETTINGS_VER);
  p.putBytes("s", &settings, sizeof(Settings));
  p.end();
}

void saveSettingsSoon() { saveAt = millis() + 1500; }
void applyBrightness() { lcd.setBrightness(max<uint8_t>(settings.bright, 10)); }

static int enabledCards(int out[3]) {
  int n = 0;
  if (settings.cards & 1) out[n++] = CARD_WEATHER;
  if (settings.cards & 2) out[n++] = CARD_PC;
  if (settings.cards & 4) out[n++] = CARD_FOCUS;
  return n;
}

// ------------------------------------------------------------------ protocol
// value of "key" in "a=1|key=val|b=2" ("" if missing)
static String field(const String& s, const char* key) {
  String k = String(key) + "=";
  int i = 0;
  while (i < (int)s.length()) {
    int end = s.indexOf('|', i);
    if (end < 0) end = s.length();
    if (s.startsWith(k, i)) return s.substring(i + k.length(), end);
    i = end + 1;
  }
  return "";
}
static int ifield(const String& s, const char* key, int def) {
  String v = field(s, key);
  return v.length() ? v.toInt() : def;
}
static int indexOf(const String& v, const char* const* names, int n, int def) {
  for (int i = 0; i < n; i++)
    if (v.equalsIgnoreCase(names[i])) return i;
  return def;
}

static void handleSet(const String& s) {
  String v;
  if ((v = field(s, "style")).length()) settings.style = indexOf(v, STYLE_NAMES, 2, settings.style);
  if ((v = field(s, "coat")).length()) settings.coat = indexOf(v, COAT_NAMES, COAT_COUNT, settings.coat);
  if (s.indexOf("name=") >= 0) {
    v = field(s, "name");
    v.trim();
    strlcpy(settings.name, v.c_str(), sizeof(settings.name));
  }
  if ((v = field(s, "bright")).length()) {
    settings.bright = constrain(v.toInt(), 10, 255);
    applyBrightness();
  }
  if ((v = field(s, "cycle")).length()) settings.cycle = constrain(v.toInt(), 3, 60);
  if ((v = field(s, "cards")).length()) settings.cards = constrain(v.toInt(), 0, 7);
  if ((v = field(s, "h24")).length()) settings.h24 = v.toInt() != 0;
  if ((v = field(s, "sleep")).length()) settings.sleepMin = constrain(v.toInt(), 1, 60);
  if ((v = field(s, "focus")).length()) settings.focusMin = constrain(v.toInt(), 5, 120);
  saveSettingsSoon();
  settingsDirty = true;
  Serial.printf("EVT:level=%d\n", catLevel);  // lets the app show the level after it syncs
}

static void handleLine(String line) {
  line.trim();
  if (!line.length()) return;
  uint32_t now = millis();
  lastPcMsg = now;
  bool typingCmd = line.startsWith("SPEED:") || line == "STOP" || line.startsWith("IDLE") || line.startsWith("STREAK");
  if (demo.on && typingCmd) return;  // the demo script drives the cat
  if (line.startsWith("FOCUS:")) demoStop();
  if (line.startsWith("SPEED:")) cat.typing(line.substring(6).toInt(), now);
  else if (line == "STOP") cat.stop(now);
  else if (line == "IDLE_START" || line == "IDLE") cat.idleStart(now);
  else if (line == "STREAK_ON") cat.streak(true);
  else if (line == "STREAK_OFF") cat.streak(false);
  else if (line == "PING") Serial.println("PONG");
  else if (line == "SHOT") shotPending = true;
  else if (line == "DEMO") demoStart();
  else if (line == "DEMO:0") demoStop();
  else if (line == "LEVELUP") { levelUp.start(now, catLevel, catLevel + 1); levelUpReal = false; screen = Screen::Main; }
  else if (line.startsWith("FOCUS:")) {
    int m = line.substring(6).toInt();
    if (m > 0) startFocus(m);
    else stopFocus();
  }
  else if (line == "UI:settings") screen = Screen::Settings, settingsDirty = true, settingsSince = now;
  else if (line == "UI:main") screen = Screen::Main;
  else if (line == "DBG") {
    char b[160];
    cat.debug(b, sizeof b, now);
    Serial.printf("DBG:%s screen=%d sleepMin=%d frameMs=%.1f heap=%u maxBlock=%u\n", b, (int)screen, settings.sleepMin,
                  frameMs, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
  else if (line.startsWith("TIME:")) {
    String t = line.substring(5);
    int h = t.substring(0, 2).toInt(), m = t.substring(3, 5).toInt(), s = t.length() >= 8 ? t.substring(6, 8).toInt() : 0;
    if (t.length() >= 5 && t[2] == ':' && h < 24 && m < 60) {
      clk.baseSec = h * 3600 + m * 60 + s;
      clk.baseMs = now;
      clk.valid = true;
    }
  } else if (line.startsWith("DATE:")) {
    strlcpy(clk.date, line.substring(5).c_str(), sizeof(clk.date));
  } else if (line.startsWith("WX2:")) {
    Weather& weather = demo.on ? demo.wx : ::weather;
    String s = line.substring(4);
    weather.code = ifield(s, "code", 0);
    weather.day = ifield(s, "day", 1) != 0;
    weather.temp = ifield(s, "t", 0);
    weather.hi = ifield(s, "hi", weather.temp);
    weather.lo = ifield(s, "lo", weather.temp);
    weather.wind = ifield(s, "wind", 0);
    weather.hum = ifield(s, "hum", -1);
    String u = field(s, "u");
    weather.unit = u.length() ? u[0] : 'F';
    strlcpy(weather.windUnit, field(s, "wu").c_str(), sizeof(weather.windUnit));
    strlcpy(weather.place, field(s, "place").c_str(), sizeof(weather.place));
    weather.valid = s.length() > 0 && field(s, "t").length() > 0;
    weather.at = now;
  } else if (line.startsWith("WX2")) {  // "WX2:" with nothing = weather turned off
    (demo.on ? demo.wx : weather).valid = false;
  } else if (line.startsWith("PC:")) {
    PcStats& pc = demo.on ? demo.pc : ::pc;
    String s = line.substring(3);
    pc.cpu = ifield(s, "cpu", 0);
    pc.ram = ifield(s, "ram", 0);
    pc.wpm = ifield(s, "wpm", 0);
    pc.cpuT = ifield(s, "cput", -1);
    pc.gpu = ifield(s, "gpu", -1);
    pc.gpuT = ifield(s, "gput", -1);
    String u = field(s, "u");
    pc.unit = u.length() ? u[0] : 'F';
    pc.valid = true;
    pc.at = now;
  } else if (line.startsWith("SET:")) {
    if (demo.on) std::swap(settings, demo.set);
    handleSet(line.substring(4));
    if (demo.on) std::swap(settings, demo.set);
  } else if (line.startsWith("URL:")) {
    strlcpy(configUrl, line.substring(4).c_str(), sizeof(configUrl));
    settingsDirty = true;
  }
}

static void readSerial() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleLine(buf);
      buf = "";
    } else if (c != '\r' && buf.length() < 300) {
      buf += c;
    }
  }
}

// ------------------------------------------------------------------ touch
static void setupTouch() {
  Preferences p;
  p.begin("touch", false);
  uint16_t cal[8];
  bool have = p.getBytesLength("calp") == sizeof(cal);  // "calp": portrait (the ticker stored landscape "cal")
  int32_t x, y;
  bool held = lcd.getTouch(&x, &y);  // hold the screen while powering on to recalibrate
  if (have && !held) {
    p.getBytes("calp", cal, sizeof(cal));
    lcd.setTouchCalibrate(cal);
  } else {
    rnd.frame([](Frame& f) { drawMessage(f, "TOUCH SETUP", "TAP EACH ARROW"); });
    delay(2500);
    lcd.fillScreen(0x000000u);
    lcd.calibrateTouch(cal, 0xFFFFFFu, 0x000000u, 24);
    p.putBytes("calp", cal, sizeof(cal));
  }
  p.end();
}

static void startSlide(int to);
static void advanceCard() {
  int ids[3], n = enabledCards(ids);
  if (n < 2) return;
  int i = 0;
  while (i < n && ids[i] != view.card) i++;
  startSlide(ids[(i + 1) % n]);
}

static void startSlide(int to) {
  if (to == view.card || view.next >= 0) return;
  view.next = to;
  view.slide = 0;
  int ids[3], n = enabledCards(ids);
  view.dir = (n > 0 && to == ids[0]) ? -1 : 1;  // wrapping back to the first card slides the other way
}

static void setBright(int b) {
  b = constrain(b, 10, 255);
  if (b != settings.bright) {
    settings.bright = b;
    applyBrightness();
    settingsDirty = true;
  }
}

static void handleTouch(uint32_t now) {
  static bool down = false, dragging = false;
  static int32_t sx = 0, sy = 0;
  int32_t x, y;
  bool t = lcd.getTouch(&x, &y);
  if (t && !down) {  // press
    down = true;
    sx = x, sy = y;
    dragging = screen == Screen::Settings && y > BR_Y - 6 && x >= BR_BAR_X - 4 && x < BR_BAR_X + BR_BAR_W + 4;
  }
  if (t && dragging) {  // drag across the brightness blocks
    setBright(10 + (int)((x - BR_BAR_X) * 245.0f / BR_BAR_W));
    settingsSince = now;
  }
  if (!t && down) {  // release = tap
    down = false;
    if (demo.on) {
      demoStop();
      return;
    }
    if (dragging) {
      dragging = false;
      saveSettingsSoon();
      Serial.printf("EVT:bright=%d\n", settings.bright);
      return;
    }
    if (screen == Screen::Main) {
      if (sx > GEAR_X - 14 && sy < HEADER_H + 10) {
        screen = Screen::Settings;
        settingsDirty = true;
        settingsSince = now;
      } else if (sy >= CARD_Y && sy < CARD_Y + CARD_H) {
        if (now < levelUpUntil) levelUpUntil = 0;  // tap closes the level-up box
        else if (view.card == CARD_FOCUS && !focus.active && sx >= FC_BTN_X - 6 && sy >= FC_BTN_Y - 6) {
          startFocus(settings.focusMin);  // ▶ START on the focus card
        } else {
          advanceCard();
          cardSince = now;
        }
      } else if (sy > CAT_Y + 30) {
        cat.poke(now);
      }
    } else {
      settingsSince = now;
      if (sx > 150 && sy < 40) {
        screen = Screen::Main;
      } else if (sy >= FO_Y && sy < FO_Y + FO_H) {
        if (sx >= FO_BTN_X) {  // START / STOP
          if (focus.active) stopFocus();
          else {
            startFocus(settings.focusMin);
            screen = Screen::Main;
          }
        } else if (!focus.active) {  // tap the length to cycle 15 / 25 / 45 / 60
          int i = 0;
          while (i < 4 && FOCUS_CHOICES[i] != settings.focusMin) i++;
          settings.focusMin = FOCUS_CHOICES[(i + 1) % 4];
          saveSettingsSoon();
        }
        settingsDirty = true;
      } else if (sy > BR_Y - 6) {  // ◀ / ▶ step one block
        if (sx < BR_BAR_X) setBright(settings.bright - 25);
        else if (sx >= BR_BAR_X + BR_BAR_W) setBright(settings.bright + 25);
        saveSettingsSoon();
        Serial.printf("EVT:bright=%d\n", settings.bright);
      }
    }
  }
}

// ------------------------------------------------------------------ main
void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  loadSettings();
  lcd.init();
  lcd.setRotation(0);
  applyBrightness();
  rnd.begin();
  setupTouch();
  Serial.println("HELLO:bongo2");
  Serial.printf("EVT:level=%d\n", catLevel);
  cardSince = millis();
}

void loop() {
  static uint32_t last = millis(), lastSettingsDraw = 0;
  readSerial();
  handleTouch(millis());
  // read the clock after input: serial/touch handlers stamp events with millis(), and
  // an older "now" would make (now - event) wrap around to a huge unsigned number
  uint32_t now = millis();
  if (saveAt && now > saveAt) {
    saveSettings();
    saveAt = 0;
  }

  demoTick(now);
  if (screen == Screen::Settings) {
    if (demo.on) settingsSince = now;
    if (now - settingsSince > 60000) screen = Screen::Main;  // back to the cat after a minute
    else {
      if (settingsDirty || shotPending || now - lastSettingsDraw > (focus.active ? 1000UL : 5000UL)) {
        rnd.frame([&](Frame& f) { drawSettings(f, now); }, shotPending);
        settingsDirty = shotPending = false;
        lastSettingsDraw = now;
      }
      delay(10);
      return;
    }
  }

  float dt = min(0.1f, (now - last) / 1000.0f);
  last = now;

  // cards: which are on, auto-advance, slide animation
  int ids[3], n = enabledCards(ids);
  view.nCards = n;
  bool known = false;
  for (int i = 0; i < n; i++) known |= ids[i] == view.card;
  if (n > 0 && !known) view.card = ids[0], view.next = -1;  // its card was switched off
  view.lastCard = n > 0 && view.card == ids[n - 1];
  if (n > 1 && view.next < 0 && now - cardSince > settings.cycle * 1000UL) advanceCard();
  if (view.next >= 0) {
    view.slide += dt / 0.45f;
    if (view.slide >= 1) {
      view.card = view.next;
      view.next = -1;
      view.slide = 0;
      cardSince = now;
    }
  }

  if (focus.active && now - focus.startMs >= focus.durMs) {  // session finished: level up!
    focus.active = false;
    catLevel++;
    saveLevel();
    levelUp.start(now, catLevel - 1, catLevel);
    levelUpReal = true;
    screen = Screen::Main;  // the level-up plays full screen, even from OPTIONS
    cat.celebrate(now);
    Serial.printf("EVT:focus=done|level=%d\n", catLevel);
  }
  if (levelUp.justEnded(now) && levelUpReal) {  // after the big animation, the "grew to" box
    levelUpUntil = now + 6000;
    cat.celebrate(now);
  }

  fx.set(weather.valid ? skyFor(weather.code) : Sky::Clear, weather.valid ? weather.day : true);
  fx.update(dt);
  cat.update(now, dt, settings.sleepMin, lastPcMsg);
  view.t = now;
  uint32_t t0 = micros();
  if (levelUp.active(now)) {
    const CatImg* const* L = CAT_TABLE[settings.style % 2][settings.coat % COAT_COUNT];
    rnd.frame([&](Frame& f) { levelUp.draw(f, now, L, settings.name); }, shotPending);
  } else {
    rnd.frame([&](Frame& f) {
      drawMain(f, fx, cat, view);
      if (demo.on) drawCaption(f, STEPS[demo.step].caption, now);
    }, shotPending);
  }
  if (!shotPending) frameMs = frameMs * 0.9f + (micros() - t0) / 1000.0f * 0.1f;
  shotPending = false;
}
