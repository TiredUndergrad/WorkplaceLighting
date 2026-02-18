#include "TimerHandler.hpp"
#include <Arduino.h>

TimerHandler::TimerHandler(LedStripController* ledController)
  : _ledController(ledController),
    _start(true), _previousMin(0),
    _blinkActive(false), _blinkCount(0), _blinkState(true),
    _previousBlinkMillis(0), _savedSplittingMode(false) {}

void TimerHandler::stopBlinking() {
    if (_blinkActive) {
        _blinkActive = false;
        restoreParameters();
        FastLED.clear();
        FastLED.show();
    }
}

void TimerHandler::saveCurrentParameters() {
    _savedSplittingMode = _ledController->isIndividualMode();
    
    if (_savedSplittingMode) {
        _savedSplitParams = _ledController->getSplittingParams();
    } else {
        _savedGlobalParams = _ledController->getCurrentEffect();
    }
}

void TimerHandler::restoreParameters() {
    if (_savedSplittingMode) {
        _ledController->setIndividualMode();
        _ledController->setSplitting(_savedSplitParams);
    } else {
        _ledController->setGlobalMode();
        _ledController->setGlobalEffect(
            _savedGlobalParams.effect,
            _savedGlobalParams.val,
            _savedGlobalParams.speed,
            _savedGlobalParams.color,
            _savedGlobalParams.temp,
            _savedGlobalParams.sat,
            _savedGlobalParams.hsvVal
        );
    }
    _ledController->turnOn(); // Убеждаемся, что подсветка включена
}

void TimerHandler::applyMaxBrightness() {
    if (!_ledController) return;
    
    bool wasIndividual = _ledController->isIndividualMode();
    
    if (wasIndividual) {
        // В индивидуальном режиме - применяем макс яркость к каждой ленте
        SplittingParams params = _ledController->getSplittingParams();
        
        // Сохраняем все параметры, но устанавливаем максимальную яркость
        params.val1 = 255;
        params.val2 = 255;
        params.val3 = 255;
        
        // Для статического цвета также устанавливаем максимальную яркость цвета
        if (params.effect1 == 0) params.hsvVal1 = 255;
        if (params.effect2 == 0) params.hsvVal2 = 255;
        if (params.effect3 == 0) params.hsvVal3 = 255;
        
        _ledController->setSplitting(params);
    } else {
        // В глобальном режиме
        EffectParams params = _ledController->getCurrentEffect();
        
        params.val = 255; // Максимальная общая яркость
        
        // Для статического цвета также устанавливаем максимальную яркость цвета
        if (params.effect == 0) {
            params.hsvVal = 255;
        }
        
        _ledController->setGlobalEffect(
            params.effect, params.val, params.speed,
            params.color, params.temp, params.sat, params.hsvVal
        );
    }
}

bool TimerHandler::update(const AdvParams& params, bool systemOn) {
    if (!params.timEnable) {
        _start = true;
        if (_blinkActive) {
            _blinkActive = false;
            restoreParameters();
            FastLED.clear();
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
    // Запускаем мигание только если система включена
    if (systemOn && !_blinkActive && (currentMin - _previousMin >= (unsigned long)params.interval)) {
        _previousMin = currentMin;
        
        // Сохраняем текущие параметры перед началом мигания
        saveCurrentParameters();
        
        _blinkActive = true;
        _blinkCount = 0;
        _blinkState = true;
        _previousBlinkMillis = currentMillis;
    }

    // Фаза мигания: каждые 500 мс переключаем подсветку, 7 раз
    if (_blinkActive) {
        if (currentMillis - _previousBlinkMillis >= BLINK_INTERVAL_MS) {
            _previousBlinkMillis = currentMillis;
            
            if (_blinkState) {
                // Включаем подсветку с текущим эффектом на максимальной яркости
                if (systemOn) {
                    applyMaxBrightness();
                } else {
                    // Если система выключена, но мы в мигании - это странно,
                    // но на всякий случай используем красный
                    SplittingParams tempParams = _ledController->getSplittingParams();
                    tempParams.effect1 = 0; tempParams.color1 = 0; tempParams.sat1 = 255; 
                    tempParams.hsvVal1 = 255; tempParams.val1 = 255;
                    tempParams.effect2 = 0; tempParams.color2 = 0; tempParams.sat2 = 255; 
                    tempParams.hsvVal2 = 255; tempParams.val2 = 255;
                    tempParams.effect3 = 0; tempParams.color3 = 0; tempParams.sat3 = 255; 
                    tempParams.hsvVal3 = 255; tempParams.val3 = 255;
                    _ledController->setIndividualMode();
                    _ledController->setSplitting(tempParams);
                }
                _ledController->update();
            } else {
                // Выключаем подсветку (черный)
                FastLED.clear();
                FastLED.show();
                
                _blinkCount++;
                
                // После последнего выключения восстанавливаем исходные параметры
                if (_blinkCount >= BLINK_COUNT) {
                    _blinkActive = false;
                    
                    // Восстанавливаем исходные параметры, но только если система включена
                    if (systemOn) {
                        restoreParameters();
                        _ledController->update();
                    }
                }
            }
            _blinkState = !_blinkState;
        }
        return true;
    }

    return false;
}

String TimerHandler::getStatusForLog(const AdvParams& params) const {
    String s = F("Timer: enabled=");
    s += params.timEnable ? 1 : 0;
    s += F(" interval=");
    s += params.interval;
    s += F(" min blink=");
    s += _blinkActive ? 1 : 0;
    s += F(" count=");
    s += _blinkCount;
    return s;
}