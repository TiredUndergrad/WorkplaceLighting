#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <WebServer.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <WiFi.h>
#include "LedStripController.hpp"

struct WiFiSettings {
  char ssid[32];
  char password[64];
};

class MyRequestHandler {
public:
    MyRequestHandler(WebServer& server, int ledPin, LedStripController* ledController = nullptr);
    
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
    
    // Обработчики для LED контроллера
    void handleSetMode();           // Новый: установка режима (общий/индивидуальный)
    void handleSetEffect();          // Установка эффекта для общего режима
    void handleSetZoneEffect();      // Новый: установка эффекта для конкретной зоны
    void handleSetSplitting();       // Для обратной совместимости
    void handleSetMotion();
    void handleSetALS();
    void handleSetTimer();
    void handleGetSettings();
    
private:
    WebServer& _server;
    int _ledPin;
    bool _ledState;
    bool _isConfigured;
    bool _indexExists;  // Кэш для проверки существования index.html
    LedStripController* _ledController;  // Указатель на контроллер LED ленты
    
    void loadWiFiSettings();
    void checkIndexFile();  // Метод для проверки файла
    String getContentType(String filename); // Определение MIME-типа
};

#endif