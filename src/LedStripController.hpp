#ifndef LED_STRIP_CONTROLLER_H
#define LED_STRIP_CONTROLLER_H

#include <FastLED.h>

// Структура для параметров зон ленты
struct SplittingParams {
  uint8_t numSplit;  // Количество зон (2 или 3)
  
  // Параметры для зоны 1
  uint8_t effect1;
  uint8_t val1;
  uint8_t speed1;
  uint8_t color1;
  uint8_t temp1;
  
  // Параметры для зоны 2
  uint8_t effect2;
  uint8_t val2;
  uint8_t speed2;
  uint8_t color2;
  uint8_t temp2;
  
  // Параметры для зоны 3
  uint8_t effect3;
  uint8_t val3;
  uint8_t speed3;
  uint8_t color3;
  uint8_t temp3;
};

// Структура для параметров эффекта
struct EffectParams {
  uint8_t effect;      // Номер эффекта
  uint8_t val;         // Яркость
  uint8_t speed;        // Скорость
  uint8_t color;        // Цвет (для HSV)
  uint8_t temp;         // Температура (для белого цвета)
  uint8_t startFrom;    // Начальный индекс
  uint8_t end;          // Конечный индекс
};

class LedStripController {
public:
  // Конструктор
  LedStripController(CRGB* leds, uint16_t ledCount, uint8_t dataPin);
  
  // Инициализация
  void begin();
  
  // Обновление эффектов (должен вызываться в loop)
  void update();
  
  // Установка эффекта для всей ленты
  void setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp);
  
  // Установка эффекта для зоны
  void setZoneEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end);
  
  // Установка режима разделения на зоны
  void setSplitting(const SplittingParams& params);
  
  // Получение текущих параметров
  EffectParams getCurrentEffect() const;
  SplittingParams getSplittingParams() const;
  bool isSplittingMode() const;
  
  // Получение количества светодиодов
  uint16_t getLedCount() const { return _ledCount; }
  
  // Включение/выключение LED ленты
  void turnOn();
  void turnOff();
  bool isOn() const { return _isOn; }
  
private:
  CRGB* _leds;
  uint16_t _ledCount;
  uint8_t _dataPin;
  
  // Режим работы
  bool _splittingMode;
  bool _isOn;
  SplittingParams _splittingParams;
  EffectParams _effectParams;
  
  // Внутренние переменные для эффектов
  unsigned long _rainbowPreviousMillis;
  unsigned long _twinklePreviousMillis;
  unsigned long _purplePreviousMillis;
  uint8_t _twinkleHue;
  uint8_t _purpleHue;
  
  // Приватные методы эффектов
  void temperature(uint8_t val, uint8_t temp, uint8_t startFrom, uint8_t end);
  void statick(uint8_t val, uint8_t color, uint8_t startFrom, uint8_t end);
  void rainbow(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end);
  void pulse(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end);
  void twinkleEffect(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end);
  void smoothPurpleEffect(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end);
  void switchEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end);
};

#endif // LED_STRIP_CONTROLLER_H
