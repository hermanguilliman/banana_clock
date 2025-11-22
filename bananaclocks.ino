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

const char *apSSID = "BananaClock";
const char *apPASS = "banana1234";

ESP8266WebServer server(80);
bool dots = true;
unsigned long lastUpdate = 0;
uint8_t brightness = 7;

// ====================== ЗВУКИ ======================
void beep(int freq, int duration)
{
  tone(BUZZER, freq, duration);
  delay(duration + 10);
  noTone(BUZZER);
}

void beepStartup()
{
  beep(800, 100);
  delay(100);
  beep(1000, 100);
  delay(100);
  beep(1200, 150);
}

void beepConfirm()
{
  beep(1200, 80);
  delay(100);
  beep(1500, 120);
}

// ====================== ФАЙЛЫ ======================
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

// ====================== API ======================
void handleTime()
{
  DateTime now = rtc.now();
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  server.send(200, "text/plain", buf);
}

void handleCurrentBrightness()
{
  server.send(200, "text/plain", String(brightness));
}

void handleSetTime()
{
  if (server.hasArg("h") && server.hasArg("m"))
  {
    int h = server.arg("h").toInt();
    int m = server.arg("m").toInt();
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, 0));
  }
  server.send(200, "text/plain", "OK");
}

void handleSyncClient()
{
  if (server.hasArg("h") && server.hasArg("m") && server.hasArg("s"))
  {
    int h = server.arg("h").toInt();
    int m = server.arg("m").toInt();
    int s = server.arg("s").toInt();
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));
  }
  server.send(200, "text/plain", "OK");
}

void handleBrightness()
{
  if (server.hasArg("value"))
  {
    int val = constrain(server.arg("value").toInt(), 0, 7);
    brightness = val;
    display.setBrightness(brightness);
    EEPROM.write(0, brightness);
    EEPROM.commit();
  }
  server.send(200, "text/plain", "OK");
}

void handleBeep()
{
  if (server.hasArg("type") && server.arg("type") == "confirm")
  {
    beepConfirm();
  }
  server.send(200, "text/plain", "OK");
}

// ====================== SETUP ======================
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
  {
    Serial.println("LittlebtFS failed!");
    return;
  }

  if (!rtc.begin())
  {
    display.showNumberDecEx(9999, 0b01000000, true);
    while (true)
      delay(1);
  }

  WiFi.softAP(apSSID, apPASS);
  Serial.println("\n=== BananaClock v3.0 Dark ===");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  MDNS.begin("banana");

  server.onNotFound([]()
                    { handleFileRead(server.uri()); });

  server.on("/time", HTTP_GET, handleTime);
  server.on("/brightness", HTTP_GET, []()
            {
    if (server.hasArg("value")) {
      // Если пришла ?value=... — меняем яркость
      int val = constrain(server.arg("value").toInt(), 0, 7);
      brightness = val;
      display.setBrightness(brightness);
      EEPROM.write(0, brightness);
      EEPROM.commit();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", String(brightness));
    } });
  server.on("/set", HTTP_GET, handleSetTime);
  server.on("/syncClient", HTTP_GET, handleSyncClient);
  server.on("/beep", HTTP_GET, handleBeep);

  server.begin();
  display.setBrightness(brightness);

  beepStartup();
}

// ====================== LOOP ======================
void loop()
{
  server.handleClient();
  MDNS.update();

  if (millis() - lastUpdate >= 1000)
  {
    dots = !dots;
    lastUpdate = millis();
  }

  DateTime now = rtc.now();
  int displayTime = now.hour() * 100 + now.minute();
  display.showNumberDecEx(displayTime, dots ? 0b01000000 : 0, true);
}