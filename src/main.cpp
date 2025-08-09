#include <FTPServer.h> // Библиотека для FTP-сервера
#include <SPIFFS.h>    // Работа с файловой системой SPIFFS
#include <SparkFun_APDS9960.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

SparkFun_APDS9960 apds; // Создание объекта датчика освещения
uint16_t ambient_light = 0;

// Wi-Fi настройки
const char *ssid = "KLOPOVNIK_69";
const char *password = "KLOPOVNIK_69";

// Настройка GPIO
const int ledPin = 2; // D2 на плате соответствует GPIO2

// Создание HTTP-сервера на порту 80
WebServer server(80);

// Текущий статус светодиода
bool ledState = false;

// Объект FTP-сервера
FTPServer ftpSrv(SPIFFS);

// Функция для обработки включения светодиода
void handleLedOn() {
  ledState = true;
  digitalWrite(ledPin, HIGH);
  server.send(200, "text/plain", "LED is ON");
}

// Функция для обработки выключения светодиода
void handleLedOff() {
  ledState = false;
  digitalWrite(ledPin, LOW);
  server.send(200, "text/plain", "LED is OFF");
}

// Функция для обслуживания главной страницы
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// Обработчки страницы с настройкой Wi-Fi
void handleWiFiConfig() {
  File file = SPIFFS.open("/wifi-config.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Wi-Fi config page not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// Функция для обработки запроса о текущем уровне освещенности
void handleLightData() {
  // Отправляем текущие данные об освещенности в формате JSON
  String response = "{\"ambient_light\": " + String(ambient_light) + "}";
  server.send(200, "application/json", response);
}

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  // Инициализация SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS opened!");
  }

  // Обработчики для маршрутов
  server.on("/", HTTP_GET, handleRoot);           // Главная страница
  server.on("/wifi", HTTP_GET, handleWiFiConfig); // Страница настроек Wi-Fi
  server.on("/led/on", HTTP_POST, handleLedOn);   // Включение светодиода
  server.on("/led/off", HTTP_POST, handleLedOff); // Выключение светодиода
  server.on("/light", HTTP_GET, handleLightData); // Маршрут для данных освещенности

  server.begin();

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
