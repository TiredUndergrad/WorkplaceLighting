#include "MyRequestHandler.hpp"
#include <ESPmDNS.h>

extern uint16_t ambient_light;

MyRequestHandler::MyRequestHandler(WebServer& server, int ledPin) 
    : _server(server), _ledPin(ledPin), _ledState(false), _isConfigured(false) {
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
}

void MyRequestHandler::begin() {
    loadWiFiSettings();
    
    // Регистрация обработчиков
    _server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    _server.on("/wifi-config", HTTP_GET, [this]() { this->handleWiFiConfig(); });
    _server.on("/main.css", HTTP_GET, [this]() { this->handleCSS(); });
    _server.on("/led/on", HTTP_POST, [this]() { this->handleLedOn(); });
    _server.on("/led/off", HTTP_POST, [this]() { this->handleLedOff(); });
    _server.on("/savewifi", HTTP_POST, [this]() { this->handleSaveWiFi(); });
    _server.on("/scanwifi", HTTP_GET, [this]() { this->handleScanWiFi(); });
    _server.on("/light", HTTP_GET, [this]() { this->handleLightData(); });
    _server.on("/wifistatus", HTTP_GET, [this]() { this->handleWiFiStatus(); });
    _server.on("/disconnectwifi", HTTP_POST, [this]() { this->handleDisconnectWiFi(); });
    
    // Универсальный обработчик для ВСЕХ статических файлов
    _server.onNotFound([this]() {
      String path = _server.uri();
      
      Serial.printf("onNotFound called for: %s\n", path.c_str());
      
      // Убираем начальный слеш для SPIFFS
      String spiffsPath = path;
      if (spiffsPath.startsWith("/")) {
        spiffsPath = spiffsPath.substring(1);
      }
      
      Serial.printf("SPIFFS path: %s\n", spiffsPath.c_str());
      
      // Автоматически определяем MIME-тип
      String contentType = getContentType(spiffsPath);
      Serial.printf("Content-Type: %s\n", contentType.c_str());
      
      // Отдаем файл из SPIFFS
      if (SPIFFS.exists(spiffsPath)) {
        Serial.printf("File exists, opening...\n");
        File file = SPIFFS.open(spiffsPath, "r");
        if (file) {
          Serial.printf("File opened, size: %d bytes\n", file.size());
          _server.streamFile(file, contentType);
          file.close();
          Serial.println("File sent successfully!");
        } else {
          Serial.println("Failed to open file!");
          _server.send(500, "text/plain", "Failed to open file");
        }
      } else {
        Serial.printf("File not found: %s\n", spiffsPath.c_str());
        _server.send(404, "text/plain", "File not found: " + path);
      }
    });
    
    _server.begin();
}

void MyRequestHandler::loadWiFiSettings() {
  WiFiSettings settings;
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.get(0, settings);
  EEPROM.end();

  // Изначально считаем, что не настроено
  _isConfigured = false;

  if (settings.ssid[0] != '\0') {
    Serial.print("Attempting to connect to WiFi: ");
    Serial.println(settings.ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.ssid, settings.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      attempts++;
      Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      _isConfigured = true;
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      
      // Инициализация mDNS
      if (MDNS.begin("workplacelighting")) {
        Serial.println("mDNS responder started");
        Serial.println("You can now access the device at: http://workplacelighting.local");
        // Добавляем сервисы
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("ftp", "tcp", 21);
      } else {
        Serial.println("Error setting up MDNS responder!");
      }
    } else {
      Serial.println("Failed to connect to WiFi network!");
      WiFi.disconnect(true);
      delay(100);
    }
  }

  if (!_isConfigured) {
    Serial.println("Starting Access Point mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Config");
    delay(100); // Даем время для инициализации AP
    Serial.println("Access Point started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Инициализация mDNS для режима Access Point
    if (MDNS.begin("workplacelighting")) {
      Serial.println("mDNS responder started");
      Serial.println("You can now access the device at: http://workplacelighting.local");
      // Добавляем сервисы
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ftp", "tcp", 21);
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
  }
}

void MyRequestHandler::handleClient() {
    _server.handleClient();
}

void MyRequestHandler::handleRoot() {
    if (!SPIFFS.exists("/index.html")) {
        handleWiFiConfig();
        return;
    }

    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        _server.send(500, "text/plain", "File not found");
        return;
    }
    
    // // Добавляем заголовки кэширования
    // _server.sendHeader("Cache-Control", "public, max-age=3600");
    // _server.sendHeader("ETag", String(file.size()));
    
    _server.streamFile(file, "text/html");
    file.close();
}

void MyRequestHandler::handleWiFiConfig() {
  File file = SPIFFS.open("/wifi-config.html", "r");
  if (!file) {
    _server.send(500, "text/plain", "Wi-Fi config page not found");
    return;
  }
  
  // // Добавляем заголовки кэширования
  // _server.sendHeader("Cache-Control", "public, max-age=3600");
  // _server.sendHeader("ETag", String(file.size()));
  
  _server.streamFile(file, "text/html");
  file.close();
}

// Обработчик для CSS файлов
void MyRequestHandler::handleCSS() {
  File file = SPIFFS.open("/main.css", "r");
  if (!file) {
    _server.send(404, "text/plain", "CSS file not found");
    return;
  }
  _server.streamFile(file, "text/css");
  file.close();
}

// Автоматическое определение MIME-типов
String MyRequestHandler::getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".svg")) return "image/svg+xml";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".txt")) return "text/plain";
  if (filename.endsWith(".xml")) return "text/xml";
  if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

void MyRequestHandler::handleLedOn() {
    _ledState = true;
    digitalWrite(_ledPin, HIGH);
    _server.send(200, "text/plain", "LED is ON");
}

void MyRequestHandler::handleLedOff() {
    _ledState = false;
    digitalWrite(_ledPin, LOW);
    _server.send(200, "text/plain", "LED is OFF");
}

void MyRequestHandler::handleSaveWiFi() {
  String newSSID = _server.arg("ssid");
  String newPass = _server.arg("password");

  // Сохраняем в EEPROM
  WiFiSettings settings;
  strncpy(settings.ssid, newSSID.c_str(), sizeof(settings.ssid));
  strncpy(settings.password, newPass.c_str(), sizeof(settings.password));
  
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.put(0, settings);
  EEPROM.commit();
  EEPROM.end();

  // Пытаемся подключиться
  WiFi.begin(newSSID.c_str(), newPass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _isConfigured = true;
    _server.send(200, "text/plain", "WiFi configured successfully! Device will reboot.");
    delay(1000);
    ESP.restart();
  } else {
    _server.send(500, "text/plain", "Failed to connect to WiFi");
  }
}

void MyRequestHandler::handleScanWiFi() {
  int n = WiFi.scanNetworks();
  String json = "{\"networks\": [";
  
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
  }
  json += "]}";
  
  _server.send(200, "application/json", json);
}

// Функция для обработки запроса о текущем уровне освещенности
void MyRequestHandler::handleLightData() {
  // Отправляем текущие данные об освещенности в формате JSON
  String response = "{\"ambient_light\": " + String(ambient_light) + "}";
  _server.send(200, "application/json", response);
}

// Функция для получения статуса WiFi подключения
void MyRequestHandler::handleWiFiStatus() {
  String json = "{";
  
  if (WiFi.status() == WL_CONNECTED) {
    json += "\"connected\":true,";
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"mode\":\"station\"";
  } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    json += "\"connected\":false,";
    json += "\"ssid\":\"ESP32-Config\",";
    json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"rssi\":0,";
    json += "\"mode\":\"access_point\"";
  } else {
    json += "\"connected\":false,";
    json += "\"ssid\":\"\",";
    json += "\"ip\":\"\",";
    json += "\"rssi\":0,";
    json += "\"mode\":\"disconnected\"";
  }
  
  json += "}";
  _server.send(200, "application/json", json);
}

// Функция для разрыва WiFi соединения и перехода в режим Access Point
void MyRequestHandler::handleDisconnectWiFi() {
  // Отключаемся от WiFi сети
  WiFi.disconnect(true);
  delay(100);
  
  // Очищаем настройки из EEPROM
  WiFiSettings emptySettings = {0};
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.put(0, emptySettings);
  EEPROM.commit();
  EEPROM.end();
  
  // Перезагружаемся для перехода в AP режим
  _server.send(200, "application/json", "{\"status\":\"disconnected\",\"message\":\"Device will restart in AP mode\"}");
  delay(500);
  ESP.restart();
}



