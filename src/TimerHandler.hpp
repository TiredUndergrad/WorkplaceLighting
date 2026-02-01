#ifndef TIMER_HANDLER_H
#define TIMER_HANDLER_H

#include <Arduino.h>
#include <FastLED.h>
#include "AdvParams.h"

/**
 * Таймер напоминаний: по истечении интервала (в минутах) лента
 * моргает красным 7 раз (500 мс вкл / 500 мс выкл) как напоминание.
 */
class TimerHandler {
public:
  TimerHandler(CRGB* leds, uint16_t ledCount);

  /**
   * Вызывать в loop().
   * @param systemOn true, если система (лента) включена пользователем — иначе моргание не показываем
   * @return true, если идёт фаза моргания (уже вызван FastLED.show(), не вызывать ledController.update())
   */
  bool update(const AdvParams& params, bool systemOn = true);

  /// Строка состояния для лога: таймер, интервал, фаза моргания
  String getStatusForLog(const AdvParams& params) const;

private:
  CRGB* _leds;
  uint16_t _ledCount;
  bool _start;               // первый запуск после включения таймера
  unsigned long _previousMin;
  bool _blinkActive;
  uint8_t _blinkCount;
  bool _blinkRedOn;
  unsigned long _previousBlinkMillis;
};

#endif
