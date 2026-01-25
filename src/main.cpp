#include <FTPServer.h> // Библиотека для FTP-сервера
#include <SPIFFS.h>    // Работа с файловой системой SPIFFS

// ВАЖНО: Включаем FastLED ПЕРЕД SparkFun_APDS9960!
// SparkFun_APDS9960 определяет макрос WAIT, который конфликтует с параметром шаблона FastLED
// Если FastLED включается первым, его шаблоны определяются до того, как WAIT станет макросом
#include <FastLED.h>
#include "LedStripController.hpp"

// Теперь включаем SparkFun_APDS9960 (после FastLED)
#include <SparkFun_APDS9960.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
#include <ESPmDNS.h>   // Библиотека для mDNS
// #include "soc/rtc_cntl_reg.h" // Для работы с brownout detector
#include "MyRequestHandler.hpp"

SparkFun_APDS9960 apds; // Создание объекта датчика освещения
uint16_t ambient_light = 0;

// Настройка GPIO
const int ledPin = 2; // D2 на плате соответствует GPIO2
const int ledStripPin = 4; // GPIO для LED ленты (можно изменить)
const int ledCount = 60; // Количество светодиодов в ленте (можно изменить)

// Массив для хранения состояния светодиодов
CRGB leds[ledCount];

// Создание контроллера LED ленты
LedStripController ledController(leds, ledCount, ledStripPin);

// Создание HTTP-сервера на порту 80
WebServer server(80);
MyRequestHandler handler(server, ledPin, &ledController);

// Объект FTP-сервера
FTPServer ftpSrv(SPIFFS);

uint16_t calculateColorTemperature(uint16_t r, uint16_t g, uint16_t b) {
  // Нормализуем значения (преобразуем к диапазону 0-1)
  float maxVal = max(r, max(g, b));
  if (maxVal == 0) return 0;
  
  float R = r / maxVal;
  float G = g / maxVal;
  float B = b / maxVal;
  
  // Альтернативная формула - более стабильная
  // Источник: https://en.wikipedia.org/wiki/Color_temperature#Approximation
  
  float X = 0.4124564 * R + 0.3575761 * G + 0.1804375 * B;
  float Y = 0.2126729 * R + 0.7151522 * G + 0.0721750 * B;
  float Z = 0.0193339 * R + 0.1191920 * G + 0.9503041 * B;
  
  // Вычисляем цветность
  if (X + Y + Z == 0) return 0;
  float x = X / (X + Y + Z);
  float y = Y / (X + Y + Z);
  
  // Формула McCamy с проверками
  float n = (x - 0.3320) / (0.1858 - y);
  
  // Проверяем, находится ли точка в допустимой области
  if (y < 0.1858 || isnan(n) || isinf(n)) {
    // Возвращаем приблизительное значение на основе отношения цветов
    float ratio = (float)r / (float)g;
    if (ratio > 1.5) return 2000;    // Теплый свет (преобладает красный)
    else if (ratio < 0.7) return 8000; // Холодный свет (преобладает синий)
    else return 4000;                 // Нейтральный свет
  }
  
  float cct = 449.0 * pow(n, 3) + 3525.0 * pow(n, 2) + 6823.3 * n + 5520.33;
  
  // Ограничиваем диапазон реалистичных значений
  if (cct < 1000) cct = 1000;
  if (cct > 20000) cct = 20000;
  
  return (uint16_t)cct;
}

void setup() {
  // // Отключение brownout detector для предотвращения перезагрузок при просадке напряжения
  // // ВНИМАНИЕ: Это временное решение! Лучше использовать качественный блок питания
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Отключаем brownout detector

  // EEPROM.begin(sizeof(WiFiSettings));
  // WiFiSettings emptySettings = {0}; // Заполняем структуру пустыми значениями
  // EEPROM.put(0, emptySettings);
  // EEPROM.commit();
  // EEPROM.end();
  // ESP.restart();
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  Wire.begin();

  // Инициализация SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS opened!");
  }
  
  // Инициализация LED контроллера
  ledController.begin();
  Serial.println("LED Strip Controller initialized");
  
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
    Serial.println("Sensor enabled!");
  } else {
    Serial.println("Error enabling sensor!");
  }

  if (apds.setAmbientLightGain(AGAIN_64X)) {
    Serial.println(F("Ambient light gain set to 64x (maximum sensitivity)"));
  } else {
    Serial.println(F("Failed to set ambient light gain"));
  }

  Wire.beginTransmission(APDS9960_I2C_ADDR); // I2C адрес APDS-9960
  Wire.write(APDS9960_ATIME); // Регистр ATIME
  Wire.write(0xC0); // Максимальное значение (255) = максимальное время интегрирования
  if (Wire.endTransmission() == 0) {
    Serial.println(F("ATIME set to maximum (0xFF) for full 16-bit range"));
  } else {
    Serial.println(F("Failed to set ATIME register"));
  }
}

void loop() {
  static uint16_t r;
  static uint16_t g;
  static uint16_t b;
  static uint16_t temp;
  apds.readRedLight(r);
  apds.readGreenLight(g);
  apds.readBlueLight(b);
  
  server.handleClient(); // Обработка HTTP-запросов
  ftpSrv.handleFTP();    // Обработка FTP-запросов
  
  // Обновление LED контроллера (должен вызываться часто для плавных эффектов)
  ledController.update();
  
  apds.readAmbientLight(ambient_light);
  temp = calculateColorTemperature(r, g, b);
  static unsigned long lastprint = 0;
  
  if (millis() - lastprint > 1000)
  {
    Serial.print("Ambient: "); Serial.println(ambient_light);
    Serial.print("R: "); Serial.println(r);
    Serial.print("G: "); Serial.println(g);
    Serial.print("B: "); Serial.println(b);
    Serial.print("Color Temperature: "); Serial.print(temp); Serial.println(" K");
    lastprint = millis();
  }
}

  // // Проверяем наличие файлов
  // if (SPIFFS.exists("/index.html")) {
  //   Serial.println("index.html found!");
  // } else {
  //   Serial.println("index.html NOT found!");
  // }
