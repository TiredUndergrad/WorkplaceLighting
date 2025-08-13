#include <FTPServer.h> // Библиотека для FTP-сервера
#include <SPIFFS.h>    // Работа с файловой системой SPIFFS
#include <SparkFun_APDS9960.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
#include "MyRequestHandler.hpp"

SparkFun_APDS9960 apds; // Создание объекта датчика освещения
uint16_t ambient_light = 0;

// Настройка GPIO
const int ledPin = 2; // D2 на плате соответствует GPIO2

// Создание HTTP-сервера на порту 80
WebServer server(80);
MyRequestHandler handler(server, ledPin);

// Объект FTP-сервера
FTPServer ftpSrv(SPIFFS);


void setup() {

  // EEPROM.begin(sizeof(WiFiSettings));
  // WiFiSettings emptySettings = {0}; // Заполняем структуру пустыми значениями
  // EEPROM.put(0, emptySettings);
  // EEPROM.commit();
  // EEPROM.end();
  // ESP.restart();
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);

  // Инициализация SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS opened!");
  }
  
  handler.begin();

  // Инициализация FTP-сервера
  ftpSrv.begin("admin", "12345"); // Логин: admin, Пароль: 12345
  Serial.println("FTP server started. You can connect using FTP client.");

  // Инициализация датчика освещенности
  if (apds.init()) {
    Serial.println(F("APDS-9960 initialization complete"));
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
  }

  if (apds.enableLightSensor(false)) {
    Serial.println(F("Light sensor is now running"));
  } else {
    Serial.println(F("Something went wrong during light sensor init!"));
  }
}

void loop() {
  server.handleClient(); // Обработка HTTP-запросов
  ftpSrv.handleFTP();    // Обработка FTP-запросов
  apds.readAmbientLight(ambient_light);
  // Serial.println(ambient_light);
}

  // // Проверяем наличие файлов
  // if (SPIFFS.exists("/index.html")) {
  //   Serial.println("index.html found!");
  // } else {
  //   Serial.println("index.html NOT found!");
  // }
