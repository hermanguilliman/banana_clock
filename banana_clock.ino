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

enum SlotState
{
  IDLE,
  SPINNING,
  SHOWING_RESULT,
  WIN_SOUND
};
SlotState currentSlotState = IDLE;

bool dots = true;
bool rtcOk = false;
unsigned long lastUpdate = 0;
uint8_t brightness = 7;

unsigned long slotStartTime = 0;
unsigned long lastSpinFrame = 0;
unsigned long resultShowTime = 0;
unsigned long winSoundStartTime = 0;
int winLevel = 0;
int finalNumbers = 0;

bool hourlyPlayed = false;
bool eepromDirty = false;
unsigned long eepromDirtyTime = 0;
const unsigned long EEPROM_WRITE_DELAY = 3000;

int checkWinLevel(int num)
{
  int d[4];
  d[0] = num / 1000;
  d[1] = (num / 100) % 10;
  d[2] = (num / 10) % 10;
  d[3] = num % 10;

  if (d[0] == d[1] && d[1] == d[2] && d[2] == d[3])
    return 3;

  if ((d[0] == d[1] && d[1] == d[2]) || (d[1] == d[2] && d[2] == d[3]))
    return 2;

  if (d[0] == d[1] || d[1] == d[2] || d[2] == d[3])
    return 1;

  return 0;
}

void startSlotMachine()
{
  if (currentSlotState != IDLE)
    return;
  currentSlotState = SPINNING;
  slotStartTime = millis();
  lastSpinFrame = 0;
}

void updateSlotMachine()
{
  unsigned long now = millis();

  if (currentSlotState == SPINNING)
  {

    if (now - lastSpinFrame >= 80)
    {
      lastSpinFrame = now;
      int randomDisplay = random(0, 10000);
      display.showNumberDecEx(randomDisplay, 0, true);
      tone(BUZZER, (unsigned int)random(400, 800), 20UL);
    }

    if (now - slotStartTime >= 5000)
    {
      finalNumbers = random(0, 10000);
      display.showNumberDecEx(finalNumbers, 0, true);
      noTone(BUZZER);

      winLevel = checkWinLevel(finalNumbers);
      currentSlotState = SHOWING_RESULT;
      resultShowTime = now;
    }
  }

  else if (currentSlotState == SHOWING_RESULT)
  {

    if (now - resultShowTime >= 1000)
    {
      if (winLevel > 0)
      {
        currentSlotState = WIN_SOUND;
        winSoundStartTime = now;
      }
      else
      {

        tone(BUZZER, 200U, 300UL);
        currentSlotState = IDLE;
      }
    }
  }

  else if (currentSlotState == WIN_SOUND)
  {
    unsigned long elapsed = now - winSoundStartTime;

    if (winLevel == 1)
    {

      if (elapsed < 600)
      {
        if ((elapsed / 100) % 2 == 0)
          tone(BUZZER, 1318U, 50UL);
        else
          tone(BUZZER, 1567U, 50UL);
      }
      else
      {
        noTone(BUZZER);
        if (elapsed > 1000)
          currentSlotState = IDLE;
      }
    }
    else if (winLevel == 2)
    {

      if (elapsed < 1200)
      {
        int step = (elapsed / 80) % 3;
        if (step == 0)
          tone(BUZZER, 1318U, 40UL);
        else if (step == 1)
          tone(BUZZER, 1567U, 40UL);
        else
          tone(BUZZER, 2093U, 40UL);
      }
      else
      {
        noTone(BUZZER);
        if (elapsed > 1500)
          currentSlotState = IDLE;
      }
    }
    else if (winLevel == 3)
    {

      if (elapsed < 2500)
      {
        int step = (elapsed / 100) % 4;
        if (step == 0)
          tone(BUZZER, 1046U, 50UL);
        else if (step == 1)
          tone(BUZZER, 1318U, 50UL);
        else if (step == 2)
          tone(BUZZER, 1567U, 50UL);
        else
          tone(BUZZER, 2093U, 50UL);

        if ((elapsed / 200) % 2 == 0)
        {
          display.showNumberDecEx(finalNumbers, 0, true);
        }
        else
        {
          display.clear();
        }
      }
      else
      {
        noTone(BUZZER);
        display.showNumberDecEx(finalNumbers, 0, true);
        if (elapsed > 3000)
          currentSlotState = IDLE;
      }
    }
  }
}

void handleFileRead(String path)
{
  if (path.endsWith("/"))
    path += "index.html";
  String contentType = "text/plain";
  if (path.endsWith(".html"))
    contentType = "text/html";
  else if (path.endsWith(".css"))
    contentType = "text/css";
  else if (path.endsWith(".js"))
    contentType = "application/javascript";

  if (LittleFS.exists(path))
  {
    File file = LittleFS.open(path, "r");
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
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
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
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                        server.arg("h").toInt(), server.arg("m").toInt(), 0));
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
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                        server.arg("h").toInt(), server.arg("m").toInt(), server.arg("s").toInt()));
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

void handlePlayTest()
{
  startSlotMachine();
  server.send(200, "text/plain", "OK");
}

void setup()
{
  pinMode(BUZZER, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
  EEPROM.begin(16);

  brightness = EEPROM.read(0);
  if (brightness > 7)
    brightness = 7;

  if (!LittleFS.begin())
    Serial.println("LittleFS failed!");

  display.setBrightness(brightness);

  if (rtc.begin())
  {
    rtcOk = true;
  }
  else
  {
    display.showNumberDecEx(8888, 0b01000000, true);
  }

  WiFi.softAP(apSSID, apPASS);
  MDNS.begin("banana");

  server.on("/time", HTTP_GET, handleTime);
  server.on("/set", HTTP_GET, handleSetTime);
  server.on("/brightness", HTTP_GET, handleBrightness);
  server.on("/syncClient", HTTP_GET, handleSyncClient);
  server.on("/playTest", HTTP_GET, handlePlayTest);
  server.onNotFound([]()
                    { handleFileRead(server.uri()); });
  server.begin();

  startSlotMachine();
}

void loop()
{
  server.handleClient();
  MDNS.update();

  if (currentSlotState != IDLE)
  {
    updateSlotMachine();
  }
  else
  {

    if (rtcOk)
    {
      DateTime now = rtc.now();

      if (now.minute() == 0 && now.second() == 0)
      {
        if (!hourlyPlayed)
        {
          hourlyPlayed = true;
          startSlotMachine();
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
  }

  if (eepromDirty && millis() - eepromDirtyTime >= EEPROM_WRITE_DELAY)
  {
    EEPROM.write(0, brightness);
    EEPROM.commit();
    eepromDirty = false;
  }
}