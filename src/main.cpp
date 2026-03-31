#include <FTPServer.h>
#include <SPIFFS.h>

#include <FastLED.h>
#include "LedStripController.hpp"
#include "AdvParams.h"
#include "MotionHandler.hpp"
#include "ALSController.hpp"
#include "TimerHandler.hpp"

#include <SparkFun_APDS9960.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include "MyRequestHandler.hpp"

SparkFun_APDS9960 apds;
uint16_t ambient_light = 0;

AdvParams advParams = { false, 700, 0.02f, 3, false, 60, false, 60, false, false, false, false };
uint8_t motionMaxBrightness = 200;

const int ledPin = 2;
const int motionSensorPin = 12;

// Определяем количество светодиодов для каждой ленты
#define NUM_LEDS_1 60
#define NUM_LEDS_2 60
#define NUM_LEDS_3 60

// Объявляем три отдельных массива для трёх лент
CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];
CRGB leds3[NUM_LEDS_3];

// Создаём контроллер с тремя лентами
LedStripController ledController(leds1, NUM_LEDS_1, leds2, NUM_LEDS_2, leds3, NUM_LEDS_3);

// Создаём подсистемы (таймер, движение, автояркость)
MotionHandler motionHandler(motionSensorPin);
ALSController alsController;
TimerHandler timerHandler(&ledController);

WebServer server(80);
MyRequestHandler handler(server, ledPin, &ledController, &alsController);
FTPServer ftpSrv(SPIFFS);

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.begin(115200);
  Wire.begin();

  if (SPIFFS.begin(true)) Serial.println("SPIFFS opened!");
  
  // Инициализация трёх независимых лент на разных пинах
  FastLED.addLeds<WS2812B, 4, GRB>(leds1, NUM_LEDS_1);   // Лента 1 на пине 4
  FastLED.addLeds<WS2812B, 16, GRB>(leds2, NUM_LEDS_2);  // Лента 2 на пине 16
  FastLED.addLeds<WS2812B, 17, GRB>(leds3, NUM_LEDS_3);  // Лента 3 на пине 17
  
  ledController.begin();
  Serial.println("LED Strip Controller initialized");

  // Главный — LED-контроллер: получает указатели на подсистемы и оркестрирует их в update()
  ledController.setDependencies(&timerHandler, &motionHandler, &alsController);

  motionHandler.begin();
  handler.begin();

  ftpSrv.begin("admin", "12345");
  Serial.println("FTP server started.");

  if (apds.init())
    Serial.println(F("APDS-9960 initialization complete"));
  else
    Serial.println(F("APDS-9960 init failed!"));

  apds.enableLightSensor(false);
  apds.setAmbientLightGain(AGAIN_64X);

  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(APDS9960_ATIME);
  Wire.write(0xC0);
  Wire.endTransmission();
}

void loop() {
  static unsigned long lastLogMs = 0;
  const unsigned long LOG_INTERVAL_MS = 5000;
  
  server.handleClient();
  ftpSrv.handleFTP();

  apds.readAmbientLight(ambient_light);

  // Единая точка обновления: таймер → движение → ALS → отрисовка (всё внутри контроллера)
  ledController.update(advParams, motionMaxBrightness, ambient_light);

  // Лог состояния раз в 5 секунд
  if (millis() - lastLogMs >= LOG_INTERVAL_MS) {
    lastLogMs = millis();
    Serial.println(F("========== LOG 5s =========="));
    Serial.print(F("Ambient: "));
    Serial.println(ambient_light);
    Serial.println(ledController.getStatusForLog());
    Serial.println(motionHandler.getStatusForLog(advParams));
    Serial.println(alsController.getStatusForLog(ambient_light, advParams));
    Serial.println(timerHandler.getStatusForLog(advParams));
    Serial.print(F("Adv: brightMode="));
    Serial.print(advParams.brightMode ? 1 : 0);
    Serial.print(F(" moveMode="));
    Serial.print(advParams.moveMode ? 1 : 0);
    Serial.print(F(" timEnable="));
    Serial.print(advParams.timEnable ? 1 : 0);
    Serial.print(F(" motionMaxBr="));
    Serial.println(motionMaxBrightness);
    Serial.println(F("============================="));
  }
}