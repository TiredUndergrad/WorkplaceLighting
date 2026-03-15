#include "MyRequestHandler.hpp"
#include "AdvParams.h"
#include "ALSController.hpp"
#include <ESPmDNS.h>

extern uint16_t ambient_light;

MyRequestHandler::MyRequestHandler(WebServer& server, int ledPin, LedStripController* ledController, ALSController* alsController) 
    : _server(server), _ledPin(ledPin), _ledState(false), _isConfigured(false), _ledController(ledController), _alsController(alsController) {
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
      _server.sendHeader("Location", "/wifi-config", true);
      _server.send(302, "text/plain", ""); 
    });
    _server.on("/scanwifi", HTTP_GET, [this]() { this->handleScanWiFi(); });
    _server.on("/light", HTTP_GET, [this]() { this->handleLightData(); });
    _server.on("/wifistatus", HTTP_GET, [this]() { this->handleWiFiStatus(); });
    _server.on("/disconnectwifi", HTTP_POST, [this]() { this->handleDisconnectWiFi(); });
    
    // Обработчики для LED контроллера
    _server.on("/setmode", HTTP_POST, [this]() { this->handleSetMode(); });
    _server.on("/seteffect", HTTP_POST, [this]() { this->handleSetEffect(); });
    _server.on("/setzoneeffect", HTTP_POST, [this]() { this->handleSetZoneEffect(); });
    _server.on("/setsplitting", HTTP_POST, [this]() { this->handleSetSplitting(); });
    _server.on("/setmotion", HTTP_POST, [this]() { this->handleSetMotion(); });
    _server.on("/setals", HTTP_POST, [this]() { this->handleSetALS(); });
    _server.on("/settimer", HTTP_POST, [this]() { this->handleSetTimer(); });
    _server.on("/getsettings", HTTP_GET, [this]() { this->handleGetSettings(); });
    _server.on("/autotemp/start", HTTP_POST, [this]() { this->handleAutoTempStart(); });
    _server.on("/autotemp/status", HTTP_GET, [this]() { this->handleAutoTempStatus(); });
    
    // Универсальный обработчик для ВСЕХ статических файлов
    _server.onNotFound([this]() {
      String path = _server.uri();
      
      String spiffsPath = path;
      if (spiffsPath.startsWith("/")) {
        spiffsPath = spiffsPath.substring(1);
      }
      
      String contentType = getContentType(spiffsPath);
      
      if (SPIFFS.exists(spiffsPath)) {
        File file = SPIFFS.open(spiffsPath, "r");
        if (file) {
          _server.streamFile(file, contentType);
          file.close();
        } else {
          _server.send(500, "text/plain", "Failed to open file");
        }
      } else {
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
      
      if (MDNS.begin("workplacelighting")) {
        Serial.println("mDNS responder started");
        Serial.println("You can now access the device at: http://workplacelighting.local");
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
    delay(100);
    Serial.println("Access Point started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    if (MDNS.begin("workplacelighting")) {
      Serial.println("mDNS responder started");
      Serial.println("You can now access the device at: http://workplacelighting.local");
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
    
    _server.streamFile(file, "text/html");
    file.close();
}

void MyRequestHandler::handleWiFiConfig() {
  File file = SPIFFS.open("/wifi-config.html", "r");
  if (!file) {
    _server.send(500, "text/plain", "Wi-Fi config page not found");
    return;
  }
  
  _server.streamFile(file, "text/html");
  file.close();
}

void MyRequestHandler::handleCSS() {
  File file = SPIFFS.open("/main.css", "r");
  if (!file) {
    _server.send(404, "text/plain", "CSS file not found");
    return;
  }
  _server.streamFile(file, "text/css");
  file.close();
}

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
  
  String newSSID = _server.arg("ssid");
  String newPass = _server.arg("password");
  
  bool wasInAPMode = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
  
  WiFiSettings oldSettings;
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.get(0, oldSettings);
  EEPROM.end();
  
  bool hadPreviousConnection = (oldSettings.ssid[0] != '\0' && WiFi.status() == WL_CONNECTED);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(newSSID.c_str(), newPass.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
    _server.handleClient();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connection successful!");
    
    WiFiSettings settings;
    strncpy(settings.ssid, newSSID.c_str(), sizeof(settings.ssid));
    strncpy(settings.password, newPass.c_str(), sizeof(settings.password));
    
    EEPROM.begin(sizeof(WiFiSettings));
    EEPROM.put(0, settings);
    EEPROM.commit();
    EEPROM.end();
    
    _isConfigured = true;
    _server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"WiFi configured successfully! Device will reboot.\"}");
    delay(2000);
    ESP.restart();
  }
  else
  {
    Serial.println("Connection failed!");
    _server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to connect to WiFi. Please check your credentials.\"}");
    
    delay(500);
    
    WiFi.disconnect(true);
    delay(100);
    
    if (hadPreviousConnection && oldSettings.ssid[0] != '\0') {
      Serial.println("Attempting to restore previous WiFi connection...");
      
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP("ESP32-Config");
      delay(100);
      
      WiFi.begin(oldSettings.ssid, oldSettings.password);
      int restoreAttempts = 0;
      while (WiFi.status() != WL_CONNECTED && restoreAttempts < 10) {
        delay(500);
        restoreAttempts++;
        _server.handleClient();
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Previous WiFi connection restored!");
        _isConfigured = true;
      } else {
        Serial.println("Failed to restore previous connection, switching to AP mode only");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Config");
        delay(100);
        _isConfigured = false;
      }
    } else {
      if (wasInAPMode) {
        Serial.println("Restoring AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Config");
        delay(100);
      } else {
        Serial.println("Switching to AP+STA mode to maintain connection...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("ESP32-Config");
        delay(100);
      }
      _isConfigured = false;
    }
  }
  
  Serial.println("=== handleSaveWiFi END ===\n");
}

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

void MyRequestHandler::handleLightData() {
  String json = "{\"ambient_light\": " + String(ambient_light);
  if (_ledController) {
    // Считаем ленту "включённой" только если она реально светит,
    // а не заблокирована режимом движения/таймера.
    bool ledOn = _ledController->isOn() && !_ledController->getBlock();
    json += ",\"led_on\":" + String(ledOn ? 1 : 0);
    json += ",\"motion_enabled\":" + String(advParams.moveMode ? 1 : 0);
    json += ",\"timer_blinking\":" + String(_ledController->isTimerBlinking() ? 1 : 0);

    if (_ledController->isIndividualMode()) {
      SplittingParams sp = _ledController->getSplittingParams();
      json += ",\"zone1\":{\"brightness\":" + String(sp.val1) + ",\"temp\":" + String((sp.temp1 * 8000 / 255) + 2000) + "}";
      json += ",\"zone2\":{\"brightness\":" + String(sp.val2) + ",\"temp\":" + String((sp.temp2 * 8000 / 255) + 2000) + "}";
      json += ",\"zone3\":{\"brightness\":" + String(sp.val3) + ",\"temp\":" + String((sp.temp3 * 8000 / 255) + 2000) + "}";
    } else {
      EffectParams params = _ledController->getCurrentEffect();
      json += ",\"brightness\":" + String(params.val);
      uint16_t tempK = (uint16_t)(params.temp * 8000 / 255) + 2000;
      json += ",\"temp\":" + String(tempK);
    }
  }
  json += "}";
  _server.send(200, "application/json", json);
}

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

void MyRequestHandler::handleDisconnectWiFi() {
  WiFi.disconnect(true);
  delay(100);
  
  WiFiSettings emptySettings = {0};
  EEPROM.begin(sizeof(WiFiSettings));
  EEPROM.put(0, emptySettings);
  EEPROM.commit();
  EEPROM.end();
  
  _server.send(200, "application/json", "{\"status\":\"disconnected\",\"message\":\"Device will restart in AP mode\"}");
  delay(500);
  ESP.restart();
}

// Новый обработчик для установки режима работы (общий/индивидуальный)
void MyRequestHandler::handleSetMode() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  String mode = _server.arg("mode");
  
  if (mode == "global") {
    _ledController->setGlobalMode();
    _server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"global\"}");
  } else if (mode == "individual") {
    _ledController->setIndividualMode();
    _server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"individual\"}");
  } else {
    _server.send(400, "application/json", "{\"error\":\"Invalid mode\"}");
  }
}

// Обработчик установки эффекта для общего режима
void MyRequestHandler::handleSetEffect() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  uint8_t effect = 0, val = 128, speed = 1, color = 0, temp = 0, sat = 255, hsvVal = 255;
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
  
  _ledController->setGlobalEffect(effect, val, speed, color, temp, sat, hsvVal);
  
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Новый обработчик установки эффекта для конкретной зоны в индивидуальном режиме
void MyRequestHandler::handleSetZoneEffect() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  uint8_t zone = 0, effect = 0, val = 128, speed = 1, color = 0, temp = 0, sat = 255, hsvVal = 255;
  bool enabled = true; // По умолчанию включено
  
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "zone") zone = (uint8_t)value.toInt();
    else if (name == "effect") effect = (uint8_t)value.toInt();
    else if (name == "val") val = (uint8_t)value.toInt();
    else if (name == "speed") speed = (uint8_t)value.toInt();
    else if (name == "color") color = (uint8_t)value.toInt();
    else if (name == "temp") temp = (uint8_t)value.toInt();
    else if (name == "sat") sat = (uint8_t)value.toInt();
    else if (name == "hsvval") hsvVal = (uint8_t)value.toInt();
    else if (name == "enabled") enabled = (value.toInt() == 1);
  }
  
  if (zone >= 1 && zone <= 3) {
    _ledController->setZoneEffect(zone - 1, effect, val, speed, color, temp, sat, hsvVal, enabled);
    _server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    _server.send(400, "application/json", "{\"error\":\"Invalid zone number\"}");
  }
}

// Обработчик установки режима разделения (для обратной совместимости)
void MyRequestHandler::handleSetSplitting() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  SplittingParams params;
  params.numSplit = 3;
  
  params.effect1 = _server.arg("effect1").toInt();
  params.val1 = _server.arg("val1").toInt();
  params.speed1 = _server.arg("speed1").toInt();
  params.color1 = _server.arg("color1").toInt();
  params.temp1 = _server.arg("temp1").toInt();
  params.sat1 = _server.arg("sat1").toInt();
  params.hsvVal1 = _server.arg("hsvval1").toInt();
  
  params.effect2 = _server.arg("effect2").toInt();
  params.val2 = _server.arg("val2").toInt();
  params.speed2 = _server.arg("speed2").toInt();
  params.color2 = _server.arg("color2").toInt();
  params.temp2 = _server.arg("temp2").toInt();
  params.sat2 = _server.arg("sat2").toInt();
  params.hsvVal2 = _server.arg("hsvval2").toInt();
  
  params.effect3 = _server.arg("effect3").toInt();
  params.val3 = _server.arg("val3").toInt();
  params.speed3 = _server.arg("speed3").toInt();
  params.color3 = _server.arg("color3").toInt();
  params.temp3 = _server.arg("temp3").toInt();
  params.sat3 = _server.arg("sat3").toInt();
  params.hsvVal3 = _server.arg("hsvval3").toInt();
  
  _ledController->setIndividualMode();
  _ledController->setSplitting(params);
  
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

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

void MyRequestHandler::handleSetALS() {
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "enabled") advParams.brightMode = (value.toInt() == 1);
    else if (name == "aim_light") advParams.aimLight = (int)value.toInt();
    else if (name == "k") advParams.k = value.toFloat();
    else if (name == "delay_als") advParams.delayALS = (uint8_t)value.toInt();
    else if (name == "auto_temp") advParams.autoTempMode = (value.toInt() == 1);
    else if (name == "auto_temp_zone1") advParams.autoTempZone1 = (value.toInt() == 1);
    else if (name == "auto_temp_zone2") advParams.autoTempZone2 = (value.toInt() == 1);
    else if (name == "auto_temp_zone3") advParams.autoTempZone3 = (value.toInt() == 1);
  }
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void MyRequestHandler::handleSetTimer() {
  for (size_t i = 0; i < _server.args(); i++) {
    String name = _server.argName(i);
    String value = _server.arg(i);
    if (name == "enabled") {
      advParams.timEnable = (value.toInt() == 1);
    }
    else if (name == "interval") {
      advParams.interval = (int)value.toInt();
    }
  }
  _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void MyRequestHandler::handleGetSettings() {
  if (!_ledController) {
    _server.send(500, "application/json", "{\"error\":\"LED controller not initialized\"}");
    return;
  }
  
  String json = "{";
  
  // Определяем текущий режим
  json += "\"mode\":\"" + String(_ledController->isIndividualMode() ? "individual" : "global") + "\",";
  
  if (_ledController->isIndividualMode()) {
    SplittingParams params = _ledController->getSplittingParams();
    json += "\"numSplit\":" + String(params.numSplit) + ",";
    json += "\"zone1\":{";
    json += "\"effect\":" + String(params.effect1) + ",\"brightness\":" + String(params.val1) + ",";
    json += "\"speed\":" + String(params.speed1) + ",\"color\":" + String(params.color1) + ",";
    json += "\"temp\":" + String((params.temp1 * 8000 / 255) + 2000) + ",";
    json += "\"saturation\":" + String(params.sat1) + ",\"hsvval\":" + String(params.hsvVal1) + ",";
    json += "\"enabled\":" + String(params.enabled1 ? 1 : 0) + "},";
    
    json += "\"zone2\":{";
    json += "\"effect\":" + String(params.effect2) + ",\"brightness\":" + String(params.val2) + ",";
    json += "\"speed\":" + String(params.speed2) + ",\"color\":" + String(params.color2) + ",";
    json += "\"temp\":" + String((params.temp2 * 8000 / 255) + 2000) + ",";
    json += "\"saturation\":" + String(params.sat2) + ",\"hsvval\":" + String(params.hsvVal2) + ",";
    json += "\"enabled\":" + String(params.enabled2 ? 1 : 0) + "}";
    
    if (params.numSplit == 3) {
      json += ",\"zone3\":{";
      json += "\"effect\":" + String(params.effect3) + ",\"brightness\":" + String(params.val3) + ",";
      json += "\"speed\":" + String(params.speed3) + ",\"color\":" + String(params.color3) + ",";
      json += "\"temp\":" + String((params.temp3 * 8000 / 255) + 2000) + ",";
      json += "\"saturation\":" + String(params.sat3) + ",\"hsvval\":" + String(params.hsvVal3) + ",";
      json += "\"enabled\":" + String(params.enabled3 ? 1 : 0) + "}";
    }
  } else {
    EffectParams params = _ledController->getCurrentEffect();
    json += "\"effect\":" + String(params.effect) + ",";
    json += "\"brightness\":" + String(params.val) + ",";
    json += "\"speed\":" + String(params.speed) + ",";
    json += "\"color\":" + String(params.color) + ",";
    json += "\"temp\":" + String((params.temp * 8000 / 255) + 2000) + ",";
    json += "\"saturation\":" + String(params.sat) + ",";
    json += "\"hsvval\":" + String(params.hsvVal);
  }
  // Для UI считаем "led_on" только если лента реально светит,
  // а не заблокирована режимом движения/таймера.
  json += ",\"led_on\":" + String((_ledController->isOn() && !_ledController->getBlock()) ? 1 : 0);
  json += ",\"motion_enabled\":" + String(advParams.moveMode ? 1 : 0);
  json += ",\"timer_blinking\":" + String(_ledController->isTimerBlinking() ? 1 : 0);
  json += ",\"motion_timeout\":" + String(advParams.timeOut);
  json += ",\"auto_brightness\":" + String(advParams.brightMode ? 1 : 0);
  json += ",\"aim_light\":" + String(advParams.aimLight);
  json += ",\"als_k\":" + String(advParams.k, 4);
  json += ",\"delay_als\":" + String(advParams.delayALS);
  json += ",\"timer_enabled\":" + String(advParams.timEnable ? 1 : 0);
  json += ",\"timer_interval\":" + String(advParams.interval);
  json += ",\"auto_temp\":" + String(advParams.autoTempMode ? 1 : 0);
  json += ",\"auto_temp_zone1\":" + String(advParams.autoTempZone1 ? 1 : 0);
  json += ",\"auto_temp_zone2\":" + String(advParams.autoTempZone2 ? 1 : 0);
  json += ",\"auto_temp_zone3\":" + String(advParams.autoTempZone3 ? 1 : 0);
  json += "}";
  
  _server.send(200, "application/json", json);
}

void MyRequestHandler::handleAutoTempStart() {
  if (!_alsController) {
    _server.send(500, "application/json", "{\"error\":\"ALS controller not initialized\"}");
    return;
  }
  _alsController->startAutoTempCalibration();
  _server.send(200, "application/json", "{\"status\":\"started\"}");
}

void MyRequestHandler::handleAutoTempStatus() {
  if (!_alsController) {
    _server.send(500, "application/json", "{\"error\":\"ALS controller not initialized\"}");
    return;
  }
  String json = "{";
  json += "\"enabled\":" + String(advParams.autoTempMode ? 1 : 0);
  json += ",\"calibrated\":" + String(_alsController->isAutoTempCalibrated() ? 1 : 0);
  json += ",\"calibrating\":" + String(_alsController->isAutoTempCalibrationRunning() ? 1 : 0);
  json += ",\"progress\":" + String(_alsController->getAutoTempCalibrationProgress());
  json += ",\"current_temp\":" + String(_alsController->getCurrentColorTemperature());
  json += ",\"recommended_temp\":" + String(_alsController->getRecommendedColorTemperature(ambient_light));
  json += "}";
  _server.send(200, "application/json", json);
}