#include "PIDController.hpp"
#include <Arduino.h>

PIDController::PIDController(float kp, float ki, float kd)
  : _kp(kp), _ki(ki), _kd(kd), _integral(0), _prevErr(0) {}

void PIDController::setParams(float kp, float ki, float kd) {
  _kp = kp;
  _ki = ki;
  _kd = kd;
}

void PIDController::reset() {
  _integral = 0;
  _prevErr = 0;
}

int PIDController::compute(float input, float setpoint, float dt, int minOut, int maxOut) {
  float err = setpoint - input;
  _integral = constrain(_integral + err * dt * _ki, (float)minOut, (float)maxOut);
  float D = (dt > 0) ? (err - _prevErr) / dt : 0;
  _prevErr = err;
  float out = err * _kp + _integral + D * _kd;
  return (int)constrain(out, (float)minOut, (float)maxOut);
}
