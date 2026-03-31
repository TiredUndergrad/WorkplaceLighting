#ifndef MOTION_HANDLER_H
#define MOTION_HANDLER_H

#include <Arduino.h>
#include "AdvParams.h"
#include "LedStripController.hpp"

/**
 * Обработка датчика движения: при движении — плавное увеличение яркости
 * до сохранённого максимума, после таймаута без движения — затухание.
 */
class MotionHandler {
public:
  explicit MotionHandler(uint8_t motionPin);

  void begin();
  /**
   * Вызывать в loop(). Читает пин, обновляет яркость ленты по advParams и motionMaxBrightness.
   */
  void update(const AdvParams& params, uint8_t& motionMaxBrightness,
              LedStripController* led);

  /// Строка состояния для лога: пин датчика, режим движения, состояние
  String getStatusForLog(const AdvParams& params) const;

  /// Активно ли сейчас движение (при выключенном moveMode всегда true)
  bool isMotionActive() const { return _motionState; }

private:
  uint8_t _pin;
  bool _motionState;
  unsigned long _moveTimeSec;   // время последнего движения в секундах (millis/1000)
  unsigned long _prevMillis;
  bool _savedMaxOnEnable;
};

#endif
