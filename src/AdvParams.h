#ifndef ADV_PARAMS_H
#define ADV_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

// Параметры автояркости (ALS), автоцвета и датчика движения
struct AdvParams {
  bool brightMode;    // Режим автояркости по датчику освещённости
  int aimLight;      // Целевая освещённость (0–65535)
  float k;           // Коэффициент экспоненциального сглаживания
  uint8_t delayALS;  // Задержка (сек) перед изменением яркости
  bool moveMode;     // Режим датчика движения
  int timeOut;       // Таймаут (сек) после движения до затухания
  bool timEnable;   // Таймер напоминаний (пока заглушка)
  int interval;      // Интервал таймера (мин)
  bool autoTempMode; // Автоматическая коррекция цветовой температуры (глобальный режим)
  bool autoTempZone1; // Автокоррекция температуры для зоны 1
  bool autoTempZone2; // Автокоррекция температуры для зоны 2
  bool autoTempZone3; // Автокоррекция температуры для зоны 3
};

// Глобальные настройки (определяются в main.cpp)
extern AdvParams advParams;
extern uint8_t motionMaxBrightness;  // Яркость при движении (сохраняется при включении режима)

#endif
