#include "TimerHandler.hpp"
#include <Arduino.h>

static const unsigned long BLINK_INTERVAL_MS = 500;
static const uint8_t BLINK_COUNT = 7;

TimerHandler::TimerHandler(CRGB* leds, uint16_t ledCount)
  : _leds(leds), _ledCount(ledCount),
    _start(true), _previousMin(0),
    _blinkActive(false), _blinkCount(0), _blinkRedOn(true), _previousBlinkMillis(0) {}

bool TimerHandler::update(const AdvParams& params, bool systemOn) {
  if (!params.timEnable) {
    _start = true;
    if (_blinkActive) {
      _blinkActive = false;
      fill_solid(_leds, _ledCount, CRGB::Black);
      FastLED.show();
    }
    return false;
  }

  // Система выключена — лента не должна моргать
  if (!systemOn) {
    if (_blinkActive) {
      _blinkActive = false;
      fill_solid(_leds, _ledCount, CRGB::Black);
      FastLED.show();
    }
    return false;
  }

  unsigned long currentMin = millis() / 60000UL;
  unsigned long currentMillis = millis();

  // Инициализация времени старта при первом запуске
  if (_start) {
    _start = false;
    _previousMin = currentMin;
  }

  // Проверка: прошёл ли интервал (в минутах) — запуск моргания
  if (currentMin - _previousMin >= (unsigned long)params.interval) {
    _previousMin = currentMin;
    _blinkActive = true;
    _blinkCount = 0;
    _blinkRedOn = true;
    _previousBlinkMillis = currentMillis;
  }

  // Фаза моргания: каждые 500 мс переключаем красный / выкл, 7 раз
  if (_blinkActive) {
    if (currentMillis - _previousBlinkMillis >= BLINK_INTERVAL_MS) {
      _previousBlinkMillis = currentMillis;
      if (_blinkRedOn) {
        fill_solid(_leds, _ledCount, CRGB::Red);
      } else {
        fill_solid(_leds, _ledCount, CRGB::Black);
        _blinkCount++;
        if (_blinkCount >= BLINK_COUNT) {
          _blinkActive = false;
          FastLED.show();
          return false;
        }
      }
      _blinkRedOn = !_blinkRedOn;
    }
    FastLED.show();
    return true;
  }

  return false;
}

String TimerHandler::getStatusForLog(const AdvParams& params) const {
  String s = F("Timer: timEnable=");
  s += params.timEnable ? 1 : 0;
  s += F(" interval=");
  s += params.interval;
  s += F(" min blinkActive=");
  s += _blinkActive ? 1 : 0;
  s += F(" start=");
  s += _start ? 1 : 0;
  s += F(" prevMin=");
  s += _previousMin;
  return s;
}
