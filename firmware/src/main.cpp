// Bongo Cat (Ross edition) for the 2.8" ESP32-32E Cheap Yellow Display, portrait.
//
// PC -> board (one line each, 115200 baud):
//   TIME:HH:MM[:SS]   DATE:Thu, Nov 12
//   WX2:code=3|day=1|t=54|hi=60|lo=41|wind=8|wu=mph|hum=40|u=F|place=Highlands Ranch
//   PC:cpu=23|ram=41|wpm=82|cput=136|gpu=63|gput=160|u=F       (-1 = unknown)
//   SET:style=hd|coat=calico|name=Mochi|bright=200|cycle=8|cards=3|h24=0|sleep=5|focus=25
//   URL:http://192.168.1.20:8787/c/abc123/
//   FOCUS:<minutes>  start a focus session (FOCUS:0 stops it)
//   SPEED:<ms>  STOP  IDLE_START  STREAK_ON  STREAK_OFF  HEARTBEAT  PING  SHOT  DBG  UI:settings
// board -> PC:
//   HELLO:bongo2 (boot)   PONG   EVT:bright=180   EVT:level=3
//   EVT:focus=start|min=25|left=1500   EVT:focus=stop   EVT:focus=done|level=4
//   SHOT:240x320 <raw> SHOT_END
#include <Arduino.h>
#include <Preferences.h>

#include "cat_anim.h"
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

static int enabledCards(int out[2]) {
  int n = 0;
  if (settings.cards & 1) out[n++] = CARD_WEATHER;
  if (settings.cards & 2) out[n++] = CARD_PC;
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
  if ((v = field(s, "cards")).length()) settings.cards = constrain(v.toInt(), 0, 3);
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
  if (line.startsWith("SPEED:")) cat.typing(line.substring(6).toInt(), now);
  else if (line == "STOP") cat.stop(now);
  else if (line == "IDLE_START" || line == "IDLE") cat.idleStart(now);
  else if (line == "STREAK_ON") cat.streak(true);
  else if (line == "STREAK_OFF") cat.streak(false);
  else if (line == "PING") Serial.println("PONG");
  else if (line == "SHOT") shotPending = true;
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
    weather.valid = false;
  } else if (line.startsWith("PC:")) {
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
    handleSet(line.substring(4));
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

static void startSlide(int to) {
  if (to == view.card || view.next >= 0) return;
  view.next = to;
  view.slide = 0;
  view.dir = to == CARD_PC ? 1 : -1;  // PC card comes in from the right, weather from the left
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
        else if (!focus.active) {
          int ids[2], n = enabledCards(ids);
          if (n > 1) startSlide(view.card == ids[0] ? ids[1] : ids[0]);
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

  if (screen == Screen::Settings) {
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
  int ids[2], n = enabledCards(ids);
  view.nCards = n;
  if (n == 1) view.card = ids[0], view.next = -1;
  if (n == 2 && view.next < 0 && now - cardSince > settings.cycle * 1000UL) {
    startSlide(view.card == ids[0] ? ids[1] : ids[0]);
  }
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
    levelUpUntil = now + 8000;
    cat.celebrate(now);
    Serial.printf("EVT:focus=done|level=%d\n", catLevel);
  }

  fx.set(weather.valid ? skyFor(weather.code) : Sky::Clear, weather.valid ? weather.day : true);
  fx.update(dt);
  cat.update(now, dt, settings.sleepMin, lastPcMsg);
  view.t = now;
  uint32_t t0 = micros();
  rnd.frame([&](Frame& f) { drawMain(f, fx, cat, view); }, shotPending);
  if (!shotPending) frameMs = frameMs * 0.9f + (micros() - t0) / 1000.0f * 0.1f;
  shotPending = false;
}
