#include "MotionHandler.hpp"
#include <Arduino.h>

MotionHandler::MotionHandler(uint8_t motionPin)
  : _pin(motionPin), _motionState(true), _moveTimeSec(0), _prevMillis(0), _savedMaxOnEnable(false) {}

void MotionHandler::begin() {
  pinMode(_pin, INPUT);
}

void MotionHandler::update(const AdvParams& params, 
                           uint8_t& motionMaxBrightness,
                           LedStripController* led) {
  if (!led) return;

  unsigned long now = millis();
  unsigned long secNow = now / 1000UL;

  static bool lastState = false;

  if (params.moveMode != lastState) {
    lastState = params.moveMode;
    if (!params.moveMode) {
      led->setBlock(false);
    }
  }
  
  if (params.moveMode) {
    if (!_savedMaxOnEnable) {
      motionMaxBrightness = led->getCurrentEffect().val;
      if (motionMaxBrightness < 10) motionMaxBrightness = 200;
      _savedMaxOnEnable = true;
    }
    bool motionNow = (digitalRead(_pin) == HIGH);
    if (motionNow) {
      _motionState = true;
      _moveTimeSec = secNow;
    }
    if (secNow - _moveTimeSec >= (unsigned long)params.timeOut && _motionState)
      _motionState = false;

    uint8_t cur = led->getCurrentEffect().val;
    if (_motionState) {
      led->setBlock(false);
      if (now - _prevMillis >= 10 && !params.brightMode) {
        _prevMillis = now;
        if (cur < motionMaxBrightness)
          led->setBrightness((uint8_t)constrain((int)cur + 5, 0, motionMaxBrightness));
      }
    } else {
      if (now - _prevMillis >= 10) {
        _prevMillis = now;
        if (cur > 0) {
          led->setBrightness((uint8_t)constrain((int)cur - 5, 0, 255));
        } else {
          led->setBlock(true);  // погасло по таймауту — при следующем движении снова включить
        }
      }
    }
  } else {
    _savedMaxOnEnable = false;
    _motionState = true;
    _moveTimeSec = secNow;
  }
}

String MotionHandler::getStatusForLog(const AdvParams& params) const {
  String s = F("Motion: pin=");
  s += (digitalRead(_pin) == HIGH) ? F("HIGH") : F("LOW");
  s += F(" moveMode=");
  s += params.moveMode ? 1 : 0;
  s += F(" motionState=");
  s += _motionState ? 1 : 0;
  s += F(" timeout=");
  s += params.timeOut;
  return s;
}
