#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <WebServer.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <WiFi.h>

struct WiFiSettings {
  char ssid[32];
  char password[64];
};

class MyRequestHandler {
public:
    MyRequestHandler(WebServer& server, int ledPin);
    
    void begin();
    void handleClient();
    
    // Обработчики запросов
    void handleRoot();
    void handleLedOn();
    void handleLedOff();
    void handleWiFiConfig();
    void handleSaveWiFi();
    void handleScanWiFi();
    void handleLightData();
    void handleCSS();
    void handleWiFiStatus();
    void handleDisconnectWiFi();
    
private:
    WebServer& _server;
    int _ledPin;
    bool _ledState;
    bool _isConfigured;
    bool _indexExists;  // Кэш для проверки существования index.html
    
    void loadWiFiSettings();
    void checkIndexFile();  // Метод для проверки файла
    String getContentType(String filename); // Определение MIME-типа
};

#endif
