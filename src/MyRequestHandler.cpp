#include "MyRequestHandler.hpp"
#include "AdvParams.h"
#include <ESPmDNS.h>

extern uint16_t ambient_light;

MyRequestHandler::MyRequestHandler(WebServer& server, int ledPin, LedStripController* ledController) 
    : _server(server), _ledPin(ledPin), _ledState(false), _isConfigured(false), _ledController(ledController) {
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
    _server.on("/savewifi", HTTP_GET, [this]() { 
      // Редирект на страницу конфигурации, если кто-то попытается открыть /savewifi напрямую
      _server.sendHeader("Location", "/wifi-config", true);
      _server.send(302, "text/plain", ""); 
    });
    _server.on("/scanwifi", HTTP_GET, [this]() { this->handleScanWiFi(); });
    _server.on("/light", HTTP_GET, [this]() { this->handleLightData(); });
    _server.on("/wifistatus", HTTP_GET, [this]() { this->handleWiFiStatus(); });
    _server.on("/disconnectwifi", HTTP_POST, [this]() { this->handleDisconnectWiFi(); });
    
    // Обработчики для LED контроллера
    _server.on("/seteffect", HTTP_POST, [this]() { this->handleSetEffect(); });
    _server.on("/setsplitting", HTTP_POST, [this]() { this->handleSetSplitting(); });
    _server.on("/setmotion", HTTP_POST, [this]() { this->handleSetMotion(); });
    _server.on("/setals", HTTP_POST, [this]() { this->handleSetALS(); });
    _server.on("/settimer", HTTP_POST, [this]() { this->handleSetTimer(); });
    _server.on("/getsettings", HTTP_GET, [this]() { this->handleGetSettings(); });
    
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
    if (_ledController) {
        _ledController->turnOn();
    }
    _server.send(200, "text/plain", "LED is ON");
}

void MyRequestHandler::handleLedOff() {
    _ledState = false;
    digitalWrite(_ledPin, LOW);
    if (_ledController) {
        _ledController->turnOff();
    }
    _server.send(200, "text/plain", "LED is OFF");
}

void MyRequestHandler::handleSaveWiFi() {
  Serial.println("\n=== handleSaveWiFi START ===");
  Serial.print("Current WiFi mode: ");
  Serial.println(WiFi.getMode());
  Serial.print("Current WiFi status: ");
  Serial.println(WiFi.status());
  
  String newSSID = _server.arg("ssid");
  String newPass = _server.arg("password");
  
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(newSSID);
  Serial.print("Password length: ");
  Serial.println(newPass.length());

  // Сохраняем текущий режим WiFi
  bool wasInAPMode = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
  Serial.print("Was in AP mode: ");
  Serial.println(wasInAPMode ? "YES" : "NO");

  // Сохраняем старые настройки WiFi из EEPROM для возможного восстановления
  WiFiSettings oldSettings;
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.get(0, oldSettings);
  EEPROM.end();
  
  bool hadPreviousConnection = (oldSettings.ssid[0] != '\0' && WiFi.status() == WL_CONNECTED);
  Serial.print("Had previous WiFi connection: ");
  Serial.println(hadPreviousConnection ? "YES" : "NO");
  if (hadPreviousConnection) {
    Serial.print("Previous SSID: ");
    Serial.println(oldSettings.ssid);
  }

  // Пока НЕ сохраняем новые настройки в EEPROM - только при успешном подключении
  // Сначала пробуем подключиться

  // Пытаемся подключиться
  // Используем AP+STA режим, чтобы сохранить AP соединение во время попытки подключения
  Serial.println("Switching to AP+STA mode to maintain connection...");
  WiFi.mode(WIFI_AP_STA);
  Serial.print("WiFi mode after switch: ");
  Serial.println(WiFi.getMode());
  
  Serial.println("Calling WiFi.begin()...");
  WiFi.begin(newSSID.c_str(), newPass.c_str());
  
  Serial.println("Waiting for connection...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
    Serial.print("Attempt ");
    Serial.print(attempts);
    Serial.print("/10, WiFi status: ");
    Serial.println(WiFi.status());
    
    // Обрабатываем клиентов во время ожидания
    _server.handleClient();
  }

  Serial.print("Final WiFi status: ");
  Serial.println(WiFi.status());
  Serial.print("WiFi status == WL_CONNECTED: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "YES" : "NO");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connection successful!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Только при успешном подключении сохраняем новые настройки в EEPROM
    Serial.println("Saving new settings to EEPROM...");
    WiFiSettings settings;
    strncpy(settings.ssid, newSSID.c_str(), sizeof(settings.ssid));
    strncpy(settings.password, newPass.c_str(), sizeof(settings.password));
    
    EEPROM.begin(sizeof(WiFiSettings));
    EEPROM.put(0, settings);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("New settings saved to EEPROM");
    
    _isConfigured = true;
    // Отправляем JSON ответ (форма отправляется через fetch, URL не меняется)
    Serial.println("Sending success response...");
    _server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"WiFi configured successfully! Device will reboot.\"}");
    Serial.println("Response sent, waiting 2 seconds before restart...");
    delay(2000); // Даем время на получение ответа
    Serial.println("Restarting ESP32...");
    ESP.restart();
  }
  else
  {
    Serial.println("Connection failed!");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    
    // ВАЖНО: Отправляем ответ ПЕРЕД любыми операциями с WiFi режимом!
    // Отправляем JSON ответ (форма отправляется через fetch, URL не меняется)
    Serial.println("Sending error response...");
    _server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to connect to WiFi. Please check your credentials.\"}");
    Serial.println("Error response sent");
    
    // Даем время на отправку ответа перед изменением режима WiFi
    delay(500);
    
    // Теперь безопасно можем изменить режим WiFi
    Serial.println("Disconnecting from failed WiFi connection...");
    WiFi.disconnect(true);
    delay(100);
    Serial.print("WiFi status after disconnect: ");
    Serial.println(WiFi.status());
    
    // Пытаемся восстановить предыдущее подключение, если оно было
    if (hadPreviousConnection && oldSettings.ssid[0] != '\0') {
      Serial.println("Attempting to restore previous WiFi connection...");
      Serial.print("Previous SSID: ");
      Serial.println(oldSettings.ssid);
      
      WiFi.mode(WIFI_AP_STA); // Используем AP+STA для сохранения AP соединения
      WiFi.softAP("ESP32-Config"); // Восстанавливаем AP
      delay(100);
      
      // Пытаемся подключиться к старой сети
      WiFi.begin(oldSettings.ssid, oldSettings.password);
      int restoreAttempts = 0;
      while (WiFi.status() != WL_CONNECTED && restoreAttempts < 10) {
        delay(500);
        restoreAttempts++;
        Serial.print("Restore attempt ");
        Serial.print(restoreAttempts);
        Serial.print("/10, WiFi status: ");
        Serial.println(WiFi.status());
        _server.handleClient(); // Обрабатываем клиентов
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Previous WiFi connection restored!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        _isConfigured = true;
      } else {
        Serial.println("Failed to restore previous connection, switching to AP mode only");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Config");
        delay(100);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
        _isConfigured = false;
      }
    } else {
      // Если не было предыдущего подключения, просто возвращаемся в AP режим
      if (wasInAPMode) {
        Serial.println("Restoring AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Config");
        delay(100);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
      } else {
        Serial.println("Switching to AP+STA mode to maintain connection...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("ESP32-Config");
        delay(100);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
      }
      _isConfigured = false;
    }
  }
  
  Serial.println("=== handleSaveWiFi END ===\n");
}

// void MyRequestHandler::handleScanWiFi() {
//   int n = WiFi.scanNetworks();
//   String json = "{\"networks\": [";
  
//   for (int i = 0; i < n; ++i) {
//     if (i) json += ",";
//     json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
//   }
//   json += "]}";
  
//   _server.send(200, "application/json", json);
// }

void MyRequestHandler::handleScanWiFi() {
  int n = WiFi.scanNetworks();
  String json = "{\"networks\": [";

  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json +=
        "\"encrypted\":" +
        String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") +
        "}";
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

// Обработчик установки эффекта для одной зоны
void MyRequestHandler::handleSetEffect() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  uint8_t effect = 0, val = 128, speed = 1, color = 0, temp = 0, sat = 255, hsvVal = 255;
  // Читаем все аргументы по индексу — arg("sat")/arg("hsvval") иногда не находит POST-параметры
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "effect") effect = (uint8_t)value.toInt();
    else if (name == "val") val = (uint8_t)value.toInt();
    else if (name == "speed") speed = (uint8_t)value.toInt();
    else if (name == "color") color = (uint8_t)value.toInt();
    else if (name == "temp") temp = (uint8_t)value.toInt();
    else if (name == "sat") sat = (uint8_t)value.toInt();
    else if (name == "hsvval") hsvVal = (uint8_t)value.toInt();
  }
  
  _ledController->setEffect(effect, val, speed, color, temp, sat, hsvVal);
  
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Обработчик установки режима разделения на зоны
void MyRequestHandler::handleSetSplitting() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  SplittingParams params;
  params.numSplit = _server.arg("numSplit").toInt();
  
  auto readOpt = [this](const char* name, uint8_t def) -> uint8_t {
    String a = _server.arg(name);
    return a.length() ? (uint8_t)a.toInt() : def;
  };
  
  params.effect1 = _server.arg("effect1").toInt();
  params.val1 = _server.arg("val1").toInt();
  params.speed1 = _server.arg("speed1").toInt();
  params.color1 = _server.arg("color1").toInt();
  params.temp1 = _server.arg("temp1").toInt();
  params.sat1 = readOpt("sat1", 255);
  params.hsvVal1 = readOpt("hsvval1", 255);
  
  params.effect2 = _server.arg("effect2").toInt();
  params.val2 = _server.arg("val2").toInt();
  params.speed2 = _server.arg("speed2").toInt();
  params.color2 = _server.arg("color2").toInt();
  params.temp2 = _server.arg("temp2").toInt();
  params.sat2 = readOpt("sat2", 255);
  params.hsvVal2 = readOpt("hsvval2", 255);
  
  if (params.numSplit == 3) {
    params.effect3 = _server.arg("effect3").toInt();
    params.val3 = _server.arg("val3").toInt();
    params.speed3 = _server.arg("speed3").toInt();
    params.color3 = _server.arg("color3").toInt();
    params.temp3 = _server.arg("temp3").toInt();
    params.sat3 = readOpt("sat3", 255);
    params.hsvVal3 = readOpt("hsvval3", 255);
  } else {
    params.effect3 = 0;
    params.val3 = 0;
    params.speed3 = 1;
    params.color3 = 0;
    params.temp3 = 128;
    params.sat3 = 255;
    params.hsvVal3 = 255;
  }
  
  _ledController->setSplitting(params);
  
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Обработчик настроек датчика движения
void MyRequestHandler::handleSetMotion() {
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "enabled") advParams.moveMode = (value.toInt() == 1);
    else if (name == "timeout") advParams.timeOut = (int)value.toInt();
  }
  if (advParams.moveMode && _ledController) {
    uint8_t cur = _ledController->getCurrentEffect().val;
    motionMaxBrightness = (cur >= 10) ? cur : 200;
  }
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Обработчик настроек автояркости (ALS)
void MyRequestHandler::handleSetALS() {
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "enabled") advParams.brightMode = (value.toInt() == 1);
    else if (name == "aim_light") advParams.aimLight = (int)value.toInt();
    else if (name == "k") advParams.k = value.toFloat();
    else if (name == "delay_als") advParams.delayALS = (uint8_t)value.toInt();
  }
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Обработчик настроек таймера
void MyRequestHandler::handleSetTimer() {
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "enabled") advParams.timEnable = (value.toInt() == 1);
    else if (name == "interval") advParams.interval = (int)value.toInt();
  }
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Обработчик получения текущих настроек
void MyRequestHandler::handleGetSettings() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  String json = "{";
  
  if (_ledController->isSplittingMode()) {
    SplittingParams params = _ledController->getSplittingParams();
    json += "\"mode\":\"split" + String(params.numSplit) + "\",";
    json += "\"numSplit\":" + String(params.numSplit) + ",";
    json += "\"zone1\":{";
    json += "\"effect\":" + String(params.effect1) + ",\"brightness\":" + String(params.val1) + ",";
    json += "\"speed\":" + String(params.speed1) + ",\"color\":" + String(params.color1) + ",";
    json += "\"temp\":" + String((params.temp1 * 6000 / 255) + 2000) + ",\"saturation\":" + String(params.sat1) + ",\"hsvval\":" + String(params.hsvVal1);
    json += "},\"zone2\":{";
    json += "\"effect\":" + String(params.effect2) + ",\"brightness\":" + String(params.val2) + ",";
    json += "\"speed\":" + String(params.speed2) + ",\"color\":" + String(params.color2) + ",";
    json += "\"temp\":" + String((params.temp2 * 6000 / 255) + 2000) + ",\"saturation\":" + String(params.sat2) + ",\"hsvval\":" + String(params.hsvVal2);
    json += "}";
    if (params.numSplit == 3) {
      json += ",\"zone3\":{";
      json += "\"effect\":" + String(params.effect3) + ",\"brightness\":" + String(params.val3) + ",";
      json += "\"speed\":" + String(params.speed3) + ",\"color\":" + String(params.color3) + ",";
      json += "\"temp\":" + String((params.temp3 * 6000 / 255) + 2000) + ",\"saturation\":" + String(params.sat3) + ",\"hsvval\":" + String(params.hsvVal3);
      json += "}";
    }
  } else {
    EffectParams params = _ledController->getCurrentEffect();
    json += "\"mode\":\"single\",";
    json += "\"effect\":" + String(params.effect) + ",";
    json += "\"brightness\":" + String(params.val) + ",";
    json += "\"speed\":" + String(params.speed) + ",";
    json += "\"color\":" + String(params.color) + ",";
    json += "\"temp\":" + String((params.temp * 6000 / 255) + 2000) + ",";
    json += "\"saturation\":" + String(params.sat) + ",";
    json += "\"hsvval\":" + String(params.hsvVal);
  }
  json += ",\"motion_enabled\":" + String(advParams.moveMode ? 1 : 0);
  json += ",\"motion_timeout\":" + String(advParams.timeOut);
  json += ",\"auto_brightness\":" + String(advParams.brightMode ? 1 : 0);
  json += ",\"aim_light\":" + String(advParams.aimLight);
  json += ",\"als_k\":" + String(advParams.k, 4);
  json += ",\"delay_als\":" + String(advParams.delayALS);
  json += ",\"timer_enabled\":" + String(advParams.timEnable ? 1 : 0);
  json += ",\"timer_interval\":" + String(advParams.interval);
  json += "}";
  
  _server.send(200, "application/json", json);
}



