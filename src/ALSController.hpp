#ifndef ALS_CONTROLLER_H
#define ALS_CONTROLLER_H

#include <Arduino.h>
#include "LedStripController.hpp"
#include "AdvParams.h"
#include "Filters.hpp"

class ALSController {
public:
    ALSController();
    
    void reset();
    
    // Основной метод: вызывается каждый цикл loop
    void update(uint16_t ambientLight, const AdvParams& params,
                LedStripController* led, bool motionActive);
    
    // Для логирования
    String getStatusForLog(uint16_t ambientLight, const AdvParams& params) const;

    // Автонастройка цветовой температуры + калибровка
    void startAutoTempCalibration();
    bool isAutoTempCalibrationRunning() const { return _autoTempCalibrating; }
    bool isAutoTempCalibrated() const { return _autoTempCalibrated; }
    uint8_t getAutoTempCalibrationProgress() const { return _autoTempProgress; }
    uint16_t getCurrentColorTemperature() const { return _currentTempK; }
    uint16_t getRecommendedColorTemperature(uint16_t ambientLight) const;

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
    
    // ---------- Калибровка вклада ленты по температуре ----------
    struct TemperatureCalibrationPoint {
        uint16_t temperature;          // Кельвины
        float contribution[256];       // Вклад при разной яркости
    };

    struct CalibrationData {
        static const int TEMP_POINTS = 5;
        TemperatureCalibrationPoint tempPoints[TEMP_POINTS];
        bool calibrated = false;
        uint8_t maxBrightness = 255;
        float ambientLight = 0.0f;     // Оценка естественного света

        float getContribution(uint8_t brightness, uint16_t temperature) const;
    };

    CalibrationData _cal;

    enum AutoTempCalibPhase {
        CAL_PHASE_IDLE,
        CAL_PHASE_WAIT_DARK,      // ожидание перед измерением базового уровня
        CAL_PHASE_MEASURE_BASE,   // измерение «темно»
        CAL_PHASE_SET_LEVEL,      // установка очередной температуры/яркости
        CAL_PHASE_MEASURE_LEVEL,  // измерение при заданной яркости
        CAL_PHASE_INTERPOLATE,    // интерполяция по яркостям для текущей температуры
        CAL_PHASE_DONE
    };

    AutoTempCalibPhase _calPhase = CAL_PHASE_IDLE;
    unsigned long _autoTempCalibStartMs = 0;
    uint16_t _baseLevel = 0;
    uint8_t _calTempIndex = 0;
    uint8_t _calBrightnessIndex = 0;
    unsigned long _levelStartMs = 0;
    uint8_t _samplesCollected = 0;
    uint32_t _samplesSum = 0;

    bool _autoTempCalibrating = false;
    bool _autoTempCalibrated = false;
    uint8_t _autoTempProgress = 0;
    uint16_t _currentTempK = 4000;
    
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

    // Обработка не блокирующей калибровки автоцвета
    void processAutoTempCalibration(uint16_t ambientLight, LedStripController* led);

    // Подсчёт «чистого» внешнего света (без вклада ленты)
    uint16_t computeAmbientWithoutStrip(uint16_t totalLight, const AdvParams& params,
                                        LedStripController* led);
};

#endif
