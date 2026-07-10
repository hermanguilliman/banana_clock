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

#define EEPROM_ADDR_BRIGHTNESS 0
#define EEPROM_ADDR_CHIME      1
#define EEPROM_ADDR_STARTUP    2
#define EEPROM_SIZE            16

TM1637Display display(CLK, DIO);
RTC_DS3231 rtc;
ESP8266WebServer server(80);

const char *const apSSID = "BananaClock";
const char *const apPASS = "banana1234";

bool dots = true;
bool rtcOk = false;
unsigned long lastUpdate = 0;
uint8_t brightness = 7;

bool hourlyChimeEnabled = true;
bool startupChimeEnabled = true;
bool hourlyPlayed = false;
bool eepromDirty = false;
unsigned long eepromDirtyTime = 0;
const unsigned long EEPROM_WRITE_DELAY = 3000;

void playStartupSound()
{
  const uint16_t notes[] = {523, 659, 784, 1047};
  const uint16_t durations[] = {100, 100, 100, 200};
  for (int i = 0; i < 4; i++)
  {
    tone(BUZZER, notes[i], durations[i]);
    delay(durations[i] + 20);
  }
  noTone(BUZZER);
}

void playHourlySound()
{
  const uint16_t notes[] = {1047, 784, 659, 523};
  const uint16_t durations[] = {150, 150, 150, 250};
  for (int i = 0; i < 4; i++)
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
    brightness = constrain(server.arg("value").toInt(), 0, 7);
    display.setBrightness(brightness);
    eepromDirty = true;
    eepromDirtyTime = millis();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(brightness));
  }
}

void handleHourlyChime()
{
  if (server.hasArg("value"))
  {
    hourlyChimeEnabled = server.arg("value").toInt() != 0;
    eepromDirty = true;
    eepromDirtyTime = millis();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(hourlyChimeEnabled ? 1 : 0));
  }
}

void handleStartupChime()
{
  if (server.hasArg("value"))
  {
    startupChimeEnabled = server.arg("value").toInt() != 0;
    eepromDirty = true;
    eepromDirtyTime = millis();
    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(200, "text/plain", String(startupChimeEnabled ? 1 : 0));
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
  delay(500);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  Serial.begin(115200);
  Wire.begin();
  EEPROM.begin(EEPROM_SIZE);

  brightness = EEPROM.read(EEPROM_ADDR_BRIGHTNESS);
  if (brightness > 7)
    brightness = 7;
  display.setBrightness(brightness);

  hourlyChimeEnabled = EEPROM.read(EEPROM_ADDR_CHIME) != 0;
  startupChimeEnabled = EEPROM.read(EEPROM_ADDR_STARTUP) != 0;

  rtcOk = rtc.begin();
  if (!rtcOk)
    Serial.println("RTC failed");

  if (startupChimeEnabled)
    playStartupSound();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPASS);
  WiFi.setOutputPower(10);

  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed");
    return;
  }

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
  server.begin();
}

void loop()
{
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
        if (hourlyChimeEnabled)
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
    EEPROM.write(EEPROM_ADDR_BRIGHTNESS, brightness);
    EEPROM.write(EEPROM_ADDR_CHIME, hourlyChimeEnabled ? 1 : 0);
    EEPROM.write(EEPROM_ADDR_STARTUP, startupChimeEnabled ? 1 : 0);
    EEPROM.commit();
    eepromDirty = false;
  }
}
