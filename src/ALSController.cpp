#include "ALSController.hpp"
#include <Arduino.h>

static const float PID_KP = 0.3f, PID_KI = 0.03f, PID_KD = 0.0f;
static const uint8_t INTERVAL_BR = 5;

ALSController::ALSController()
  : _pid(PID_KP, PID_KI, PID_KD),
    _previousMillis(0), _startMillis(0), _complete(true), _timeFlag(false), _lastFiltered(0) {}

void ALSController::reset() {
  _complete = true;
  _timeFlag = false;
  _pid.reset();
}

void ALSController::update(uint16_t ambientLight, const AdvParams& params,
                            LedStripController* led, bool motionActive) {
  _filter.setK(params.k);
  float filtered = _filter.update((float)ambientLight);
  _lastFiltered = (uint16_t)filtered;
  if (!params.brightMode || !led || !led->isOn()) return;

  // Регулировкой яркости занимается ALS только при активном движении или при выключенном moveMode
  bool allowRegulation = motionActive || !params.moveMode;

  uint16_t ambientlight_exp = _lastFiltered;

  if (abs((int)ambientLight - params.aimLight) < 5)
    _complete = true;

  if (abs((int)ambientlight_exp - params.aimLight) > 15 &&
      !(led->getCurrentEffect().val == 0 && ambientlight_exp > (uint16_t)params.aimLight)) {
    if (!_timeFlag) {
      _startMillis = millis();
      _timeFlag = true;
    }
    if (millis() - _startMillis > (unsigned long)params.delayALS * 1000)
      _complete = false;
    else
      _complete = true;
  } else {
    _timeFlag = false;
  }

  if (allowRegulation && !_complete) {
    unsigned long now = millis();
    if (now - _previousMillis >= INTERVAL_BR) {
      _previousMillis = now;
      int target = _pid.compute((float)ambientlight_exp, (float)params.aimLight, 0.1f, 0, 255);
      uint8_t cur = led->getCurrentEffect().val;
      if (cur < (uint8_t)target)
        led->setBrightness(cur + 1);
      else if (cur > (uint8_t)target)
        led->setBrightness(cur > 0 ? cur - 1 : 0);
    }
  }
}

String ALSController::getStatusForLog(uint16_t ambientLight, const AdvParams& params) const {
  String s = F("ALS: brightMode=");
  s += params.brightMode ? 1 : 0;
  s += F(" aimLight=");
  s += params.aimLight;
  s += F(" raw=");
  s += ambientLight;
  s += F(" filtered=");
  s += _lastFiltered;
  s += F(" delayALS=");
  s += params.delayALS;
  return s;
}
