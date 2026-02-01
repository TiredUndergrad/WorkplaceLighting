#ifndef LED_STRIP_CONTROLLER_H
#define LED_STRIP_CONTROLLER_H

#include <FastLED.h>
#include <Arduino.h>

// Структура для параметров зон ленты
struct SplittingParams {
  uint8_t numSplit;  // Количество зон (2 или 3)
  
  // Параметры для зоны 1
  uint8_t effect1, val1, speed1, color1, temp1, sat1, hsvVal1;
  // Параметры для зоны 2
  uint8_t effect2, val2, speed2, color2, temp2, sat2, hsvVal2;
  // Параметры для зоны 3
  uint8_t effect3, val3, speed3, color3, temp3, sat3, hsvVal3;
};

// Структура для параметров эффекта
struct EffectParams {
  uint8_t effect;      // Номер эффекта
  uint8_t val;         // Яркость (общая)
  uint8_t speed;       // Скорость
  uint8_t color;       // Цвет / оттенок (H в HSV, 0-255)
  uint8_t temp;        // Температура (для белого цвета)
  uint8_t sat;         // Насыщенность (S в HSV, 0-255)
  uint8_t hsvVal;      // Яркость цвета (V в HSV, 0-255)
  uint8_t startFrom;   // Начальный индекс
  uint8_t end;         // Конечный индекс
};

// Количество эффектов (добавление нового = +1 и регистрация в .cpp)
#define EFFECT_COUNT 6

class LedStripController {
public:
  // Конструктор
  LedStripController(CRGB* leds, uint16_t ledCount, uint8_t dataPin);
  
  // Инициализация
  void begin();
  
  // Обновление эффектов (должен вызываться в loop)
  void update();
  
  // Установка эффекта для всей ленты
  void setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat = 255, uint8_t hsvVal = 255);
  
  // Установка эффекта для зоны
  void setZoneEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end, uint8_t sat = 255, uint8_t hsvVal = 255);
  
  // Установка режима разделения на зоны
  void setSplitting(const SplittingParams& params);
  
  // Установка только яркости (для ALS и датчика движения)
  void setBrightness(uint8_t val);
  
  // Получение текущих параметров
  EffectParams getCurrentEffect() const;
  SplittingParams getSplittingParams() const;
  bool isSplittingMode() const;
  
  // Получение количества светодиодов
  uint16_t getLedCount() const { return _ledCount; }
  
  // Отладка: вывод текущих параметров в Serial
  void debugPrintParams() const;
  /// Строка состояния для лога (раз в 5 с): что выводится на ленту и с какими параметрами
  String getStatusForLog() const;

  // Включение/выключение LED ленты
  void turnOn();
  /** byUser = true: выключение пользователем (движение не должно включать ленту). byUser = false: погасло по таймауту движения. */
  void turnOff(bool byUser = true);
  bool isOn() const { return _isOn; }
  /// Лента выключена пользователем — при движении не включать
  bool wasTurnedOffByUser() const { return _userRequestedOff; }
  
private:
  CRGB* _leds;
  uint16_t _ledCount;
  uint8_t _dataPin;
  
  // Режим работы
  bool _splittingMode;
  bool _isOn;
  bool _userRequestedOff;  // выключено пользователем — движение не включает ленту
  SplittingParams _splittingParams;
  EffectParams _effectParams;
  
  // Внутренние переменные для эффектов
  unsigned long _rainbowPreviousMillis;
  unsigned long _twinklePreviousMillis;
  unsigned long _purplePreviousMillis;
  uint8_t _twinkleHue;
  uint8_t _purpleHue;
  
  // Реестр эффектов: добавление нового эффекта = реализовать метод + одна строка в begin()
  typedef void (LedStripController::*EffectHandler)(const EffectParams& params);
  EffectHandler _effectHandlers[EFFECT_COUNT];

  // Приватные методы эффектов (единая сигнатура — параметры в EffectParams)
  void effectStatic(const EffectParams& params);
  void effectTemperature(const EffectParams& params);
  void effectRainbow(const EffectParams& params);
  void effectPulse(const EffectParams& params);
  void effectTwinkle(const EffectParams& params);
  void effectSmoothPurple(const EffectParams& params);

  void switchEffect(const EffectParams& params);
};

#endif // LED_STRIP_CONTROLLER_H
