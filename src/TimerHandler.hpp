#ifndef TIMER_HANDLER_H
#define TIMER_HANDLER_H

#include <Arduino.h>
#include <FastLED.h>
#include "LedStripController.hpp"
#include "AdvParams.h"

class TimerHandler {
public:
    TimerHandler(LedStripController* ledController);
    
    // Обновление состояния таймера, возвращает true если идёт мигание
    bool update(const AdvParams& params, bool systemOn);

    /// Идёт ли сейчас мигание
    bool isBlinking() const { return _blinkActive; }
    
    // Получение статуса для лога
    String getStatusForLog(const AdvParams& params) const;
    
    // Принудительная остановка мигания
    void stopBlinking();

private:
    LedStripController* _ledController;
    
    // Переменные для отсчета времени
    bool _start;
    unsigned long _previousMin;
    
    // Переменные для мигания
    bool _blinkActive;
    uint8_t _blinkCount;
    bool _blinkState;  // true - подсветка включена (макс яркость), false - выключена
    unsigned long _previousBlinkMillis;
    
    // Сохраненные параметры для восстановления
    bool _savedSplittingMode;
    SplittingParams _savedSplitParams;
    EffectParams _savedGlobalParams;
    
    // Константы
    static const unsigned long BLINK_INTERVAL_MS = 500;
    static const uint8_t BLINK_COUNT = 7;
    
    // Сохранение текущих параметров подсветки
    void saveCurrentParameters();
    
    // Восстановление сохраненных параметров
    void restoreParameters();
    
    // Применение максимальной яркости к текущим параметрам
    void applyMaxBrightness();
};

#endif