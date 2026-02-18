// #include "ALSController.hpp"
// #include <Arduino.h>

// static const float PID_KP = 0.3f, PID_KI = 0.03f, PID_KD = 0.0f;
// static const uint8_t INTERVAL_BR = 5;

// ALSController::ALSController()
//   : _pid(PID_KP, PID_KI, PID_KD),
//     _previousMillis(0), _startMillis(0), _complete(true), _timeFlag(false), _lastFiltered(0) {}

// void ALSController::reset() {
//   _complete = true;
//   _timeFlag = false;
//   _pid.reset();
// }

// void ALSController::update(uint16_t ambientLight, const AdvParams& params,
//                             LedStripController* led, bool motionActive) {
//   _filter.setK(params.k);
//   float filtered = _filter.update((float)ambientLight);
//   _lastFiltered = (uint16_t)filtered;
//   if (!params.brightMode || !led || !led->isOn()) return;

//   // Регулировкой яркости занимается ALS только при активном движении или при выключенном moveMode
//   bool allowRegulation = motionActive || !params.moveMode;

//   uint16_t ambientlight_exp = _lastFiltered;

//   if (abs((int)ambientLight - params.aimLight) < 5)
//     _complete = true;

//   if (abs((int)ambientlight_exp - params.aimLight) > 15 &&
//       !(led->getCurrentEffect().val == 0 && ambientlight_exp > (uint16_t)params.aimLight)) {
//     if (!_timeFlag) {
//       _startMillis = millis();
//       _timeFlag = true;
//     }
//     if (millis() - _startMillis > (unsigned long)params.delayALS * 1000)
//       _complete = false;
//     else
//       _complete = true;
//   } else {
//     _timeFlag = false;
//   }

//   if (allowRegulation && !_complete) {
//     unsigned long now = millis();
//     if (now - _previousMillis >= INTERVAL_BR) {
//       _previousMillis = now;
//       int target = _pid.compute((float)ambientlight_exp, (float)params.aimLight, 0.1f, 0, 255);
//       uint8_t cur = led->getCurrentEffect().val;
//       if (cur < (uint8_t)target)
//         led->setBrightness(cur + 1);
//       else if (cur > (uint8_t)target)
//         led->setBrightness(cur > 0 ? cur - 1 : 0);
//     }
//   }
// }

// String ALSController::getStatusForLog(uint16_t ambientLight, const AdvParams& params) const {
//   String s = F("ALS: brightMode=");
//   s += params.brightMode ? 1 : 0;
//   s += F(" aimLight=");
//   s += params.aimLight;
//   s += F(" raw=");
//   s += ambientLight;
//   s += F(" filtered=");
//   s += _lastFiltered;
//   s += F(" delayALS=");
//   s += params.delayALS;
//   return s;
// }


#include "ALSController.hpp"
#include <Arduino.h>

ALSController::ALSController()
    : _lastFiltered(0),
      _phase(PHASE_IDLE),
      _phaseStartTime(0),
      _lastAdjustTime(0),
      _lastBrightness(0),
      _currentAdjustInterval(MIN_ADJUST_INTERVAL) {}

void ALSController::reset() {
    _filter.reset();
    _phase = PHASE_IDLE;
    _currentAdjustInterval = MIN_ADJUST_INTERVAL;
}

void ALSController::startAdjustmentCycle() {
    _phase = PHASE_WAIT_START;
    _phaseStartTime = millis();
    _currentAdjustInterval = MIN_ADJUST_INTERVAL; // Сбрасываем на минимальный при старте
    Serial.println(F("ALS: Starting adjustment cycle in 10s"));
}

bool ALSController::isTargetReached(uint16_t target, uint16_t current) const {
    return abs((int)current - (int)target) <= HYSTERESIS;
}

unsigned long ALSController::calculateAdaptiveInterval(uint16_t target, uint16_t current) const {
    int diff = abs((int)current - (int)target);
    
    // Если разница больше порога, используем минимальный интервал (быстрая подстройка)
    if (diff > CLOSE_THRESHOLD) {
        return MIN_ADJUST_INTERVAL;
    }
    
    // Линейная интерполяция между MIN и MAX интервалами
    // Чем меньше разница, тем больше интервал
    if (diff <= HYSTERESIS) {
        return MAX_ADJUST_INTERVAL; // Уже близко к цели
    }
    
    // diff находится между HYSTERESIS и CLOSE_THRESHOLD
    float ratio = (float)(diff - HYSTERESIS) / (CLOSE_THRESHOLD - HYSTERESIS);
    // Ограничиваем ratio от 0 до 1
    ratio = constrain(ratio, 0.0f, 1.0f);
    
    // Интервал увеличивается по мере приближения к цели
    return MIN_ADJUST_INTERVAL + (unsigned long)((MAX_ADJUST_INTERVAL - MIN_ADJUST_INTERVAL) * (1.0f - ratio));
}

void ALSController::performAdjustmentStep(uint16_t target, uint16_t current, LedStripController* led) {
    if (!led) return;
    
    int diff = (int)current - (int)target;
    uint8_t currentBrightness = led->getCurrentEffect().val;
    
    if (abs(diff) <= HYSTERESIS) {
        // Цель достигнута - завершаем цикл
        _phase = PHASE_IDLE;
        Serial.println(F("ALS: Target reached, cycle complete"));
        return;
    }
    
    if (diff > 0) {
        // Слишком светло - уменьшаем яркость
        if (currentBrightness > BRIGHTNESS_STEP) {
            led->setBrightness(currentBrightness - BRIGHTNESS_STEP);
        } else {
            led->setBrightness(0);
        }
        Serial.print(F("ALS: Too bright, decreasing to "));
        Serial.println(currentBrightness - BRIGHTNESS_STEP);
    } else {
        // Слишком темно - увеличиваем яркость
        if (currentBrightness < 255 - BRIGHTNESS_STEP) {
            led->setBrightness(currentBrightness + BRIGHTNESS_STEP);
        } else {
            led->setBrightness(255);
        }
        Serial.print(F("ALS: Too dark, increasing to "));
        Serial.println(currentBrightness + BRIGHTNESS_STEP);
    }
    
    // Обновляем адаптивный интервал для следующего шага
    _currentAdjustInterval = calculateAdaptiveInterval(target, current);
    
    Serial.print(F("ALS: New adjust interval = "));
    Serial.print(_currentAdjustInterval);
    Serial.print(F("ms (diff="));
    Serial.print(abs(diff));
    Serial.println(F(")"));
}

void ALSController::update(uint16_t ambientLight, const AdvParams& params,
                            LedStripController* led, bool motionActive) {
    // Фильтруем значение освещенности
    _filter.setK(params.k);
    float filtered = _filter.update((float)ambientLight);
    _lastFiltered = (uint16_t)filtered;
    
    // Проверяем условия работы ALS
    if (!params.brightMode || !led || !led->isOn()) {
        _phase = PHASE_IDLE; // Сбрасываем цикл, если ALS выключен
        return;
    }
    
    // Регулировка разрешена только при активном движении или выключенном датчике движения
    bool allowRegulation = motionActive || !params.moveMode;
    if (!allowRegulation) {
        _phase = PHASE_IDLE; // Сбрасываем цикл, если регулировка не разрешена
        return;
    }
    
    unsigned long now = millis();
    uint16_t target = params.aimLight;
    
    switch (_phase) {
        case PHASE_IDLE:
            // Ждем начала следующего цикла
            // Можно запустить новый цикл сразу, если разница большая
            if (!isTargetReached(target, _lastFiltered)) {
                startAdjustmentCycle();
            }
            break;
            
        case PHASE_WAIT_START:
            // Ожидание 10 секунд перед началом подстройки
            if (now - _phaseStartTime >= CYCLE_INTERVAL) {
                _phase = PHASE_ADJUSTING;
                _lastAdjustTime = now;
                _lastBrightness = led->getCurrentEffect().val;
                _currentAdjustInterval = MIN_ADJUST_INTERVAL; // Начинаем с быстрой подстройки
                Serial.println(F("ALS: Starting brightness adjustment"));
            }
            break;
            
        case PHASE_ADJUSTING:
            // Проверяем, не изменились ли условия
            if (!isTargetReached(target, _lastFiltered)) {
                // Делаем шаг подстройки с адаптивным интервалом
                if (now - _lastAdjustTime >= _currentAdjustInterval) {
                    performAdjustmentStep(target, _lastFiltered, led);
                    _lastAdjustTime = now;
                }
            } else {
                // Цель достигнута - завершаем цикл
                _phase = PHASE_IDLE;
                Serial.println(F("ALS: Target reached during adjustment"));
            }
            break;
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
    s += F(" adjInt=");
    s += _currentAdjustInterval;
    s += F(" phase=");
    
    switch (_phase) {
        case PHASE_IDLE: s += F("idle"); break;
        case PHASE_WAIT_START: s += F("wait"); break;
        case PHASE_ADJUSTING: s += F("adjust"); break;
        default: s += F("unknown");
    }
    
    return s;
}