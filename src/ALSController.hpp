// #ifndef ALS_CONTROLLER_H
// #define ALS_CONTROLLER_H

// #include <Arduino.h>
// #include "AdvParams.h"
// #include "LedStripController.hpp"
// #include "PIDController.hpp"
// #include "ExpFilter.hpp"

// /**
//  * Автояркость (ALS) по датчику освещённости: ПИД-регулятор поддерживает
//  * целевую освещённость (aimLight), вход фильтруется экспоненциальным фильтром.
//  */
// class ALSController {
// public:
//   ALSController();

//   /**
//    * Вызывать в loop() после чтения датчика. При включённом brightMode
//    * обновляет яркость ленты по отфильтрованной освещённости и aimLight.
//    * Регулировка яркости выполняется только при активном движении (motionActive)
//    * или при выключенном режиме движения (moveMode).
//    */
//   void update(uint16_t ambientLight, const AdvParams& params,
//               LedStripController* led, bool motionActive);

//   void reset();

//   /// Строка состояния для лога: автояркость, целевая освещённость, сырое/фильтрованное
//   String getStatusForLog(uint16_t ambientLight, const AdvParams& params) const;

// private:
//   PIDController _pid;
//   ExpFilter _filter;
//   unsigned long _previousMillis;
//   unsigned long _startMillis;
//   bool _complete;
//   bool _timeFlag;
//   uint16_t _lastFiltered;  // последнее отфильтрованное значение для лога
// };

// #endif


#ifndef ALS_CONTROLLER_H
#define ALS_CONTROLLER_H

#include <Arduino.h>
#include "LedStripController.hpp"
#include "AdvParams.h"

// Фильтр низких частот (простое экспоненциальное сглаживание)
class LowPassFilter {
public:
    LowPassFilter() : _prev(0), _k(0.1f) {}
    void setK(float k) { _k = k; }
    float update(float raw) {
        _prev = _k * raw + (1 - _k) * _prev;
        return _prev;
    }
    void reset() { _prev = 0; }
private:
    float _prev;
    float _k;
};

class ALSController {
public:
    ALSController();
    
    void reset();
    
    // Основной метод: вызывается каждый цикл loop
    void update(uint16_t ambientLight, const AdvParams& params,
                LedStripController* led, bool motionActive);
    
    // Для логирования
    String getStatusForLog(uint16_t ambientLight, const AdvParams& params) const;

private:
    LowPassFilter _filter;
    uint16_t _lastFiltered;
    
    // Для цикла подстройки
    enum AdjustmentPhase {
        PHASE_IDLE,           // Ожидание следующего цикла
        PHASE_WAIT_START,     // Ожидание перед началом подстройки (10 сек)
        PHASE_ADJUSTING       // Активная подстройка яркости
    };
    
    AdjustmentPhase _phase;
    unsigned long _phaseStartTime;
    unsigned long _lastAdjustTime;
    int _lastBrightness;
    float _currentAdjustInterval;  // Текущий адаптивный интервал
    
    // Константы
    static const unsigned long CYCLE_INTERVAL = 3000;  // мс секунд между циклами
    static const unsigned long MIN_ADJUST_INTERVAL = 200;    // Минимальный интервал (далеко от цели)
    static const unsigned long MAX_ADJUST_INTERVAL = 500;   // Максимальный интервал (близко к цели)
    static const uint8_t BRIGHTNESS_STEP = 1;           // Шаг изменения яркости
    static const uint8_t HYSTERESIS = 50;               // Гистерезис для остановки
    static const uint16_t CLOSE_THRESHOLD = 500;         // Порог "близости" к цели
    
    // Вспомогательные методы
    void startAdjustmentCycle();
    void performAdjustmentStep(uint16_t target, uint16_t current, LedStripController* led);
    bool isTargetReached(uint16_t target, uint16_t current) const;
    unsigned long calculateAdaptiveInterval(uint16_t target, uint16_t current) const;
};

#endif
