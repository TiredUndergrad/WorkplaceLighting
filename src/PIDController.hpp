#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

/**
 * ПИД-регулятор для поддержания заданного значения (setpoint).
 * Используется в автояркости (ALS) для выхода на целевую освещённость.
 */
class PIDController {
public:
  PIDController(float kp = 0.3f, float ki = 0.03f, float kd = 0.0f);

  void setParams(float kp, float ki, float kd);
  void reset();

  /**
   * Вычисляет управляющий сигнал.
   * @param input   текущее значение (например, освещённость)
   * @param setpoint целевое значение
   * @param dt      шаг времени
   * @param minOut  минимальный выход
   * @param maxOut  максимальный выход
   * @return ограниченный выход [minOut, maxOut]
   */
  int compute(float input, float setpoint, float dt, int minOut, int maxOut);

private:
  float _kp, _ki, _kd;
  float _integral;
  float _prevErr;
};

#endif
