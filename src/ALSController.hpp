#ifndef ALS_CONTROLLER_H
#define ALS_CONTROLLER_H

#include <Arduino.h>
#include "AdvParams.h"
#include "LedStripController.hpp"
#include "PIDController.hpp"
#include "ExpFilter.hpp"

/**
 * Автояркость (ALS) по датчику освещённости: ПИД-регулятор поддерживает
 * целевую освещённость (aimLight), вход фильтруется экспоненциальным фильтром.
 */
class ALSController {
public:
  ALSController();

  /**
   * Вызывать в loop() после чтения датчика. При включённом brightMode
   * обновляет яркость ленты по отфильтрованной освещённости и aimLight.
   * Регулировка яркости выполняется только при активном движении (motionActive)
   * или при выключенном режиме движения (moveMode).
   */
  void update(uint16_t ambientLight, const AdvParams& params,
              LedStripController* led, bool motionActive);

  void reset();

  /// Строка состояния для лога: автояркость, целевая освещённость, сырое/фильтрованное
  String getStatusForLog(uint16_t ambientLight, const AdvParams& params) const;

private:
  PIDController _pid;
  ExpFilter _filter;
  unsigned long _previousMillis;
  unsigned long _startMillis;
  bool _complete;
  bool _timeFlag;
  uint16_t _lastFiltered;  // последнее отфильтрованное значение для лога
};

#endif
