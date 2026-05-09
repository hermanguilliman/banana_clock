#include <Arduino.h>
#include <TM1637Display.h>
#include <Wire.h>
#include "RTClib.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <EEPROM.h>

#define CLK D5
#define DIO D6
#define BUZZER D7

TM1637Display display(CLK, DIO);
RTC_DS3231 rtc;
ESP8266WebServer server(80);

const char *apSSID = "BananaClock";
const char *apPASS = "banana1234";

// ====================== STATES & VARS ======================
bool dots = true;
bool rtcOk = false;
unsigned long lastUpdate = 0;
uint8_t brightness = 7;

// EEPROM Debounce
bool eepromDirty = false;
unsigned long eepromDirtyTime = 0;
const unsigned long EEPROM_WRITE_DELAY = 3000; // 3 сек

// Settings
uint8_t startupMelody = 1;
uint8_t startupAnim = 1;
uint8_t hourlyMelody = 1;
uint8_t hourlyAnim = 1;

// ====================== STRUCTURES ======================
struct Note { int freq; int dur; };
struct AnimFrame { int val; uint8_t dots; int dur; };

struct Melody { const Note* notes; int len; };
struct Animation { const AnimFrame* frames; int len; };

// ====================== DATA ======================
const Note tetris[] = { {659, 500}, {494, 250}, {523, 250}, {587, 500}, {523, 250}, {494, 250}, {440, 500}, {440, 250}, {523, 250}, {659, 500}, {587, 250}, {523, 250}, {494, 500}, {494, 250}, {523, 250}, {587, 500}, {659, 500}, {523, 500}, {440, 500} };
const Note starWars[] = { {466, 140}, {466, 140}, {466, 140}, {698, 560}, {1047, 560}, {932, 140}, {880, 140}, {784, 140}, {1397, 560}, {1047, 280}, {932, 140}, {880, 140}, {784, 140}, {1397, 560}, {1047, 280}, {932, 140}, {880, 140}, {932, 140}, {784, 560} };
const Note gadget[] = { {220, 125}, {247, 62}, {262, 125}, {294, 62}, {330, 125}, {0, 62}, {262, 166}, {311, 166}, {247, 166}, {294, 166}, {262, 166}, {220, 125}, {247, 62}, {262, 125}, {294, 62}, {330, 125}, {0, 62}, {440, 166}, {415, 500}, {220, 125}, {247, 62}, {262, 125}, {294, 62}, {330, 125}, {0, 62}, {262, 166}, {311, 166}, {247, 166}, {294, 166}, {262, 166}, {440, 125}, {415, 62}, {392, 125}, {370, 62}, {349, 125} };
const Note coin[] = { {988, 100}, {1319, 500} };
const Note pacDeath[] = { {500, 100}, {460, 100}, {400, 100}, {350, 120}, {300, 140}, {220, 180}, {150, 400} };
const Note powerUp[] = { {262, 80}, {330, 80}, {392, 80}, {523, 80}, {659, 80}, {784, 80}, {1047, 300} };
const Note laser[] = { {1500, 60}, {1200, 60}, {900, 60}, {600, 60}, {400, 80}, {200, 150} };
const Note oneUp[] = { {262, 80}, {330, 80}, {392, 80}, {523, 200} };
const Note jump[] = { {200, 60}, {300, 60}, {400, 60}, {600, 100}, {800, 150} };

const Melody melodies[] = {
  {tetris, sizeof(tetris)/sizeof(Note)}, {starWars, sizeof(starWars)/sizeof(Note)}, {gadget, sizeof(gadget)/sizeof(Note)},
  {coin, sizeof(coin)/sizeof(Note)}, {pacDeath, sizeof(pacDeath)/sizeof(Note)}, {powerUp, sizeof(powerUp)/sizeof(Note)},
  {laser, sizeof(laser)/sizeof(Note)}, {oneUp, sizeof(oneUp)/sizeof(Note)}, {jump, sizeof(jump)/sizeof(Note)}
};

const AnimFrame animFill[] = { {1, 0, 600}, {11, 0, 600}, {111, 0, 600}, {1111, 0, 600}, {8888, 0, 800}, {0, 0, 600} };
const AnimFrame animSpin[] = { {1111, 0, 400}, {2222, 0, 400}, {3333, 0, 400}, {4444, 0, 400}, {5555, 0, 400}, {6666, 0, 400}, {7777, 0, 400}, {8888, 0, 400}, {9999, 0, 400}, {0, 0, 400} };
const AnimFrame animCountdown[] = { {9999, 0, 500}, {7777, 0, 500}, {5555, 0, 500}, {3333, 0, 500}, {1111, 0, 500}, {0, 0, 500} };
const AnimFrame animPulse[] = { {8888, 0b01000000, 300}, {0, 0, 300}, {8888, 0b01000000, 300}, {0, 0, 300}, {8888, 0b01000000, 500} };
const AnimFrame animSnake[] = { {1, 0, 200}, {11, 0, 200}, {111, 0, 200}, {1111, 0, 300}, {111, 0, 200}, {11, 0, 200}, {1, 0, 200}, {0, 0, 300} };
const AnimFrame animRandom[] = { {4821, 0, 150}, {7395, 0, 150}, {1048, 0, 150}, {6792, 0, 150}, {2148, 0, 150}, {8888, 0, 400} };

const Animation animations[] = {
  {animFill, sizeof(animFill)/sizeof(AnimFrame)}, {animSpin, sizeof(animSpin)/sizeof(AnimFrame)}, {animCountdown, sizeof(animCountdown)/sizeof(AnimFrame)},
  {animPulse, sizeof(animPulse)/sizeof(AnimFrame)}, {animSnake, sizeof(animSnake)/sizeof(AnimFrame)}, {animRandom, sizeof(animRandom)/sizeof(AnimFrame)}
};

// ====================== PLAYBACK ENGINE ======================
bool isPlaying = false;
int melodyIdx = 0, animIdx = 0;
unsigned long melodyTimer = 0, animTimer = 0;
int curMelodyId = 0, curAnimId = 0;

void startPlay(int melodyId, int animId) {
  if (melodyId == 0 && animId == 0) return;
  isPlaying = true;
  melodyIdx = 0; animIdx = 0;
  melodyTimer = millis(); animTimer = millis();
  curMelodyId = melodyId; curAnimId = animId;

  if (curMelodyId > 0 && curMelodyId <= 9) {
    Note n = melodies[curMelodyId-1].notes[0];
    if (n.freq > 0) tone(BUZZER, n.freq, n.dur * 0.9);
  }
  if (curAnimId > 0 && curAnimId <= 6) {
    AnimFrame f = animations[curAnimId-1].frames[0];
    display.showNumberDecEx(f.val, f.dots, true);
  }
}

void updatePlayback() {
  if (!isPlaying) return;
  unsigned long now = millis();

  if (curMelodyId > 0 && curMelodyId <= 9) {
    const Melody& m = melodies[curMelodyId-1];
    if (melodyIdx < m.len) {
      Note n = m.notes[melodyIdx];
      if (now - melodyTimer >= (unsigned long)n.dur) {
        melodyIdx++; melodyTimer = now;
        if (melodyIdx < m.len) {
          n = m.notes[melodyIdx];
          if (n.freq == 0) noTone(BUZZER);
          else tone(BUZZER, n.freq, n.dur * 0.9);
        } else noTone(BUZZER);
      }
    }
  }

  if (curAnimId > 0 && curAnimId <= 6) {
    const Animation& a = animations[curAnimId-1];
    AnimFrame f = a.frames[animIdx];
    if (now - animTimer >= (unsigned long)f.dur) {
      animIdx++; animTimer = now;
      if (animIdx >= a.len) {
        if (curMelodyId > 0 && melodyIdx < melodies[curMelodyId-1].len) animIdx = 0;
      }
      if (animIdx < a.len) {
        f = a.frames[animIdx];
        display.showNumberDecEx(f.val, f.dots, true);
      }
    }
  }

  bool mEnd = (curMelodyId == 0) || (curMelodyId > 0 && melodyIdx >= melodies[curMelodyId-1].len);
  bool aEnd = (curAnimId == 0) || (curAnimId > 0 && animIdx >= animations[curAnimId-1].len);

  if (mEnd && aEnd) { isPlaying = false; noTone(BUZZER); }
}

// ====================== FILE SYSTEM ======================
void handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";

  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    server.streamFile(file, contentType);
    file.close();
    return;
  }
  server.send(404, "text/plain", "Not found");
}

// ====================== API ======================
void handleTime() {
  if (!rtcOk) { server.send(200, "text/plain", "Err: No RTC"); return; }
  DateTime now = rtc.now();
  char buf[9]; sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  server.send(200, "text/plain", buf);
}

void handleSetTime() {
  if (!rtcOk) { server.send(500, "text/plain", "RTC Error"); return; }
  if (server.hasArg("h") && server.hasArg("m")) {
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), server.arg("h").toInt(), server.arg("m").toInt(), 0));
  }
  server.send(200, "text/plain", "OK");
}

void handleSyncClient() {
  if (!rtcOk) { server.send(500, "text/plain", "RTC Error"); return; }
  if (server.hasArg("h") && server.hasArg("m") && server.hasArg("s")) {
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), server.arg("h").toInt(), server.arg("m").toInt(), server.arg("s").toInt()));
  }
  server.send(200, "text/plain", "OK");
}

void handleBrightness() {
  if (server.hasArg("value")) {
    brightness = constrain(server.arg("value").toInt(), 0, 7);
    display.setBrightness(brightness);
    eepromDirty = true; eepromDirtyTime = millis(); // Отложенная запись
    server.send(200, "text/plain", "OK");
  } else {
    // ИСПРАВЛЕНО: Возвращаем текущее значение яркости при GET-запросе без параметров
    server.send(200, "text/plain", String(brightness));
  }
}

void handleGetSettings() {
  String json = "{\"sm\":" + String(startupMelody) + ",\"sa\":" + String(startupAnim) + ",\"hm\":" + String(hourlyMelody) + ",\"ha\":" + String(hourlyAnim) + "}";
  server.send(200, "application/json", json);
}

void handleSaveSettings() {
  if (server.hasArg("sm")) { startupMelody = constrain(server.arg("sm").toInt(), 0, 9); EEPROM.write(1, startupMelody); }
  if (server.hasArg("sa")) { startupAnim = constrain(server.arg("sa").toInt(), 0, 6); EEPROM.write(2, startupAnim); }
  if (server.hasArg("hm")) { hourlyMelody = constrain(server.arg("hm").toInt(), 0, 9); EEPROM.write(3, hourlyMelody); }
  if (server.hasArg("ha")) { hourlyAnim = constrain(server.arg("ha").toInt(), 0, 6); EEPROM.write(4, hourlyAnim); }
  EEPROM.commit();
  server.send(200, "text/plain", "OK");
}

void handlePlayTest() {
  if (!isPlaying) {
    startPlay(server.hasArg("m") ? server.arg("m").toInt() : 0, server.hasArg("a") ? server.arg("a").toInt() : 0);
  }
  server.send(200, "text/plain", "OK");
}

// ====================== SETUP ======================
void setup() {
  pinMode(BUZZER, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
  EEPROM.begin(16);
  
  brightness = EEPROM.read(0); if (brightness > 7) brightness = 7;
  startupMelody = EEPROM.read(1); if (startupMelody > 9) startupMelody = 1;
  startupAnim = EEPROM.read(2); if (startupAnim > 6) startupAnim = 1;
  hourlyMelody = EEPROM.read(3); if (hourlyMelody > 9) hourlyMelody = 1;
  hourlyAnim = EEPROM.read(4); if (hourlyAnim > 6) hourlyAnim = 1;

  if (!LittleFS.begin()) { Serial.println("LittleFS failed!"); return; }

  display.setBrightness(brightness);

  if (rtc.begin()) {
    rtcOk = true;
  } else {
    rtcOk = false;
    display.showNumberDecEx(0, 0b01000000, true);
    delay(2000);
    Serial.println("RTC FAILED! Clock mode disabled.");
  }

  WiFi.softAP(apSSID, apPASS);
  WiFi.setOutputPower(10);

  MDNS.begin("banana");

  server.onNotFound([]() { handleFileRead(server.uri()); });
  server.on("/time", HTTP_GET, handleTime);
  server.on("/brightness", HTTP_GET, handleBrightness);
  server.on("/set", HTTP_GET, handleSetTime);
  server.on("/syncClient", HTTP_GET, handleSyncClient);
  server.on("/getSettings", HTTP_GET, handleGetSettings);
  server.on("/saveSettings", HTTP_GET, handleSaveSettings);
  server.on("/playTest", HTTP_GET, handlePlayTest);
  server.begin();

  if (rtcOk) startPlay(startupMelody, startupAnim);
}

// ====================== LOOP ======================
bool hourlyPlayed = false;

void loop() {
  server.handleClient();
  MDNS.update();
  updatePlayback();

  if (eepromDirty && millis() - eepromDirtyTime >= EEPROM_WRITE_DELAY) {
    EEPROM.write(0, brightness);
    EEPROM.commit();
    eepromDirty = false;
  }

  if (rtcOk && !isPlaying) {
    DateTime now = rtc.now();

    if (now.minute() == 0 && now.second() == 0) {
      if (!hourlyPlayed) {
        hourlyPlayed = true;
        if (!isPlaying) startPlay(hourlyMelody, hourlyAnim);
      }
    } else {
      hourlyPlayed = false;
    }

    if (millis() - lastUpdate >= 1000) {
      dots = !dots;
      lastUpdate = millis();
      int displayTime = now.hour() * 100 + now.minute();
      display.showNumberDecEx(displayTime, dots ? 0b01000000 : 0, true);
    }
  } else if (!rtcOk && !isPlaying) {
    if (millis() - lastUpdate >= 1000) {
      dots = !dots;
      lastUpdate = millis();
      display.showNumberDecEx(0, dots ? 0b01000000 : 0, true);
    }
  }
}