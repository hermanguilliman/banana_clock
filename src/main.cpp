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

#define SDA_PIN D2
#define SCL_PIN D1

#define EEPROM_SIZE        16
#define EEPROM_ADDR_DATA   0
#define EEPROM_ADDR_CHECK  14

#define WDT_TIMEOUT_MS     30000

#define DEFAULT_BRIGHTNESS 4
#define DEFAULT_CHIME      1
#define DEFAULT_STARTUP    1

TM1637Display display(CLK, DIO);
RTC_DS3231 rtc;
ESP8266WebServer server(80);

const char *const apSSID = "BananaClock";
const char *const apPASS = "banana1234";

struct Settings
{
  uint8_t brightness;
  uint8_t hourlyChime;
  uint8_t startupChime;
  uint8_t reserved[11];
  uint8_t checksum;
};

Settings settings;
bool dots = true;
bool rtcOk = false;
unsigned long lastUpdate = 0;

bool hourlyPlayed = false;
bool eepromDirty = false;
unsigned long eepromDirtyTime = 0;
const unsigned long EEPROM_WRITE_DELAY = 3000;

uint8_t calcChecksum(const Settings *s)
{
  uint8_t sum = 0;
  const uint8_t *p = (const uint8_t *)s;
  for (int i = 0; i < EEPROM_ADDR_CHECK; i++)
    sum ^= p[i];
  return sum;
}

void loadSettings()
{
  EEPROM.get(EEPROM_ADDR_DATA, settings);
  if (calcChecksum(&settings) != settings.checksum)
  {
    settings.brightness = DEFAULT_BRIGHTNESS;
    settings.hourlyChime = DEFAULT_CHIME;
    settings.startupChime = DEFAULT_STARTUP;
    settings.checksum = calcChecksum(&settings);
  }
  if (settings.brightness > 7)
    settings.brightness = DEFAULT_BRIGHTNESS;
}

void markSettingsDirty()
{
  settings.checksum = calcChecksum(&settings);
  eepromDirty = true;
  eepromDirtyTime = millis();
}

void playStartupSound()
{
  const uint16_t notes[] = {392, 440, 523};
  const uint16_t durations[] = {180, 180, 280};
  for (int i = 0; i < 3; i++)
  {
    tone(BUZZER, notes[i], durations[i]);
    delay(durations[i] + 30);
  }
  noTone(BUZZER);
}

void playHourlySound()
{
  const uint16_t notes[] = {523, 440, 392};
  const uint16_t durations[] = {180, 180, 280};
  for (int i = 0; i < 3; i++)
  {
    tone(BUZZER, notes[i], durations[i]);
    delay(durations[i] + 30);
  }
  noTone(BUZZER);
}

void adjustTime(uint8_t h, uint8_t m, uint8_t s)
{
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                      constrain(h, 0, 23), constrain(m, 0, 59), constrain(s, 0, 59)));
}

void handleFileRead(const String &path)
{
  String filePath = path;
  if (filePath.endsWith("/"))
    filePath += "index.html";

  String contentType = "text/plain";
  if (filePath.endsWith(".html"))
    contentType = "text/html";
  else if (filePath.endsWith(".css"))
    contentType = "text/css";
  else if (filePath.endsWith(".js"))
    contentType = "application/javascript";

  if (LittleFS.exists(filePath))
  {
    File file = LittleFS.open(filePath, "r");
    server.streamFile(file, contentType);
    file.close();
    return;
  }
  server.send(404, "text/plain", "Not found");
}

void handleTime()
{
  if (!rtcOk)
  {
    server.send(200, "text/plain", "RTC Err");
    return;
  }
  DateTime now = rtc.now();
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  server.send(200, "text/plain", buf);
}

void handleSetTime()
{
  if (!rtcOk)
  {
    server.send(500, "text/plain", "RTC Error");
    return;
  }
  if (server.hasArg("h") && server.hasArg("m"))
  {
    adjustTime(server.arg("h").toInt(), server.arg("m").toInt(), 0);
  }
  server.send(200, "text/plain", "OK");
}

void handleSyncClient()
{
  if (!rtcOk)
  {
    server.send(500, "text/plain", "RTC Error");
    return;
  }
  if (server.hasArg("h") && server.hasArg("m") && server.hasArg("s"))
  {
    adjustTime(server.arg("h").toInt(), server.arg("m").toInt(), server.arg("s").toInt());
  }
  server.send(200, "text/plain", "OK");
}

void handleBrightness()
{
  if (server.hasArg("value"))
  {
    settings.brightness = constrain(server.arg("value").toInt(), 0, 7);
    display.setBrightness(settings.brightness);
    markSettingsDirty();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(settings.brightness));
  }
}

void handleHourlyChime()
{
  if (server.hasArg("value"))
  {
    settings.hourlyChime = server.arg("value").toInt() != 0;
    markSettingsDirty();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(settings.hourlyChime));
  }
}

void handleStartupChime()
{
  if (server.hasArg("value"))
  {
    settings.startupChime = server.arg("value").toInt() != 0;
    markSettingsDirty();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(settings.startupChime));
  }
}

void handlePlayStartup()
{
  playStartupSound();
  server.send(200, "text/plain", "OK");
}

void handlePlayHourly()
{
  playHourlySound();
  server.send(200, "text/plain", "OK");
}

void setup()
{
  ESP.wdtEnable(WDT_TIMEOUT_MS);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  EEPROM.begin(EEPROM_SIZE);

  loadSettings();

  delay(200);
  display.setBrightness(settings.brightness);

  for (int attempt = 0; attempt < 3; attempt++)
  {
    if (rtc.begin())
    {
      rtcOk = true;
      break;
    }
    delay(200);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
  }

  if (settings.startupChime)
    playStartupSound();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPASS);
  WiFi.setOutputPower(7);

  if (!LittleFS.begin())
    return;

  MDNS.begin("banana");
  server.on("/time", HTTP_GET, handleTime);
  server.on("/set", HTTP_GET, handleSetTime);
  server.on("/brightness", HTTP_GET, handleBrightness);
  server.on("/syncClient", HTTP_GET, handleSyncClient);
  server.on("/hourlyChime", HTTP_GET, handleHourlyChime);
  server.on("/startupChime", HTTP_GET, handleStartupChime);
  server.on("/playStartup", HTTP_GET, handlePlayStartup);
  server.on("/playHourly", HTTP_GET, handlePlayHourly);
  server.onNotFound([]() { handleFileRead(server.uri()); });
  display.setBrightness(settings.brightness);
  server.begin();
}

void loop()
{
  ESP.wdtFeed();

  server.handleClient();
  MDNS.update();

  if (rtcOk)
  {
    DateTime now = rtc.now();

    if (now.minute() == 0 && now.second() == 0)
    {
      if (!hourlyPlayed)
      {
        hourlyPlayed = true;
        if (settings.hourlyChime)
          playHourlySound();
      }
    }
    else
    {
      hourlyPlayed = false;
    }

    if (millis() - lastUpdate >= 1000)
    {
      dots = !dots;
      lastUpdate = millis();
      int displayTime = now.hour() * 100 + now.minute();
      display.showNumberDecEx(displayTime, dots ? 0b01000000 : 0, true);
    }
  }

  if (eepromDirty && millis() - eepromDirtyTime >= EEPROM_WRITE_DELAY)
  {
    settings.checksum = calcChecksum(&settings);
    EEPROM.put(EEPROM_ADDR_DATA, settings);
    EEPROM.commit();
    eepromDirty = false;
  }
}
