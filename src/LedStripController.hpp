#ifndef LED_STRIP_CONTROLLER_H
#define LED_STRIP_CONTROLLER_H

#include <FastLED.h>
#include <Arduino.h>

// Зависимости — только указатели, оркестрация в update()
class TimerHandler;
class MotionHandler;
class ALSController;
struct AdvParams;

// Структура для параметров зон ленты
struct SplittingParams {
  uint8_t numSplit = 3;  // Всегда 3 для трех физических лент
  
  // Параметры для ленты 1
  uint8_t effect1, val1, speed1, color1, temp1, sat1, hsvVal1;
  bool enabled1;  // Флаг включения зоны 1
  
  // Параметры для ленты 2
  uint8_t effect2, val2, speed2, color2, temp2, sat2, hsvVal2;
  bool enabled2;  // Флаг включения зоны 2
  
  // Параметры для ленты 3
  uint8_t effect3, val3, speed3, color3, temp3, sat3, hsvVal3;
  bool enabled3;  // Флаг включения зоны 3
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
};

// Количество эффектов
#define EFFECT_COUNT 6

class LedStripController {
public:
  // Конструктор: принимает три отдельных массива светодиодов и их длины
  LedStripController(CRGB* leds1, uint16_t count1,
                     CRGB* leds2, uint16_t count2,
                     CRGB* leds3, uint16_t count3);
  
  // Инициализация
  void begin();
  
  /** Зависимости: таймер, движение, автояркость. Можно передать nullptr. */
  void setDependencies(TimerHandler* timer, MotionHandler* motion, ALSController* als);

  /**
   * Главный цикл: обновляет таймер, движение, ALS и отрисовку.
   * Вызывать из loop() после чтения ambient_light.
   */
  void update(AdvParams& advParams, uint8_t& motionMaxBrightness, uint16_t ambientLight);

  /** Внутренняя отрисовка эффектов (без подсистем). Вызывается из update(). */
  void update();
  
  // ============= УПРАВЛЕНИЕ РЕЖИМАМИ =============
  void setGlobalMode() { _splittingMode = false; }
  void setIndividualMode() { _splittingMode = true; }
  bool isIndividualMode() const { return _splittingMode; }
  
  // ============= УПРАВЛЕНИЕ ЭФФЕКТАМИ =============
  void setGlobalEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat = 255, uint8_t hsvVal = 255) {
    setEffect(effect, val, speed, color, temp, sat, hsvVal);
  }
  void setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat = 255, uint8_t hsvVal = 255);
  void setZoneEffect(uint8_t zoneIndex, uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat = 255, uint8_t hsvVal = 255, bool enabled = true);
  void setSplitting(const SplittingParams& params);
  void setBrightness(uint8_t val);
  
  // ============= ПОЛУЧЕНИЕ ПАРАМЕТРОВ =============
  EffectParams getCurrentEffect() const;
  SplittingParams getSplittingParams() const;
  bool isSplittingMode() const { return _splittingMode; }
  uint16_t getLedCount(uint8_t zone) const;
  
  // ============= УПРАВЛЕНИЕ ПИТАНИЕМ =============
  void turnOn();
  void turnOff(bool byUser = true);
  bool isOn() const { return _isOn; }
  void setBlock(bool block) { _block = block; }
  bool getBlock() { return _block; }
  bool wasTurnedOffByUser() const { return _userRequestedOff; }
  
  /** Идёт ли сейчас мигание таймера (делегируется в TimerHandler, если задан). */
  bool isTimerBlinking() const;
  /** Остановить мигание таймера (делегируется в TimerHandler, если задан). */
  void stopTimerBlinking();

  // ============= ОТЛАДКА =============
  void debugPrintParams() const;
  String getStatusForLog() const;
  
private:
  // Три отдельных массива для трех лент
  CRGB* _leds[3];
  uint16_t _ledCounts[3];
  
  // Режим работы
  bool _splittingMode;
  bool _isOn;
  bool _userRequestedOff;
  bool _block;
  
  // Параметры эффектов
  EffectParams _globalEffectParams;
  SplittingParams _splittingParams;
  
  // Переменные для эффектов
  unsigned long _rainbowPreviousMillis[3];
  unsigned long _twinklePreviousMillis[3];
  unsigned long _purplePreviousMillis[3];
  uint8_t _twinkleHue[3];
  uint8_t _purpleHue[3];

  // Подсистемы (опционально, оркестрация в update())
  TimerHandler* _timerHandler = nullptr;
  MotionHandler* _motionHandler = nullptr;
  ALSController* _alsController = nullptr;

  // Реестр эффектов
  typedef void (LedStripController::*EffectHandler)(const EffectParams& params, uint8_t zoneIndex);
  EffectHandler _effectHandlers[EFFECT_COUNT];
  
  // Приватные методы эффектов
  void effectStatic(const EffectParams& params, uint8_t zoneIndex);
  void effectTemperature(const EffectParams& params, uint8_t zoneIndex);
  void effectRainbow(const EffectParams& params, uint8_t zoneIndex);
  void effectPulse(const EffectParams& params, uint8_t zoneIndex);
  void effectTwinkle(const EffectParams& params, uint8_t zoneIndex);
  void effectSmoothPurple(const EffectParams& params, uint8_t zoneIndex);
  
  // Применяет эффект к конкретной ленте
  void applyEffectToZone(uint8_t zoneIndex, const EffectParams& params);
};

#endif // LED_STRIP_CONTROLLER_H