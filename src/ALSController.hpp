#ifndef ALS_CONTROLLER_H
#define ALS_CONTROLLER_H

#include <Arduino.h>
#include "LedStripController.hpp"
#include "AdvParams.h"
#include "Filters.hpp"

class ALSController {
public:
    // Управляет автояркостью и автотемпературой по датчику освещенности.
    ALSController();
    void reset();
    // Основной тик: фильтрует свет и при необходимости корректирует ленты.
    void update(uint16_t ambientLight, const AdvParams& params,
                LedStripController* led, bool motionActive);
    String getStatusForLog(uint16_t ambientLight, const AdvParams& params) const;

    // Автонастройка цветовой температуры + калибровка
    void startAutoTempCalibration();
    bool isAutoTempCalibrationRunning() const { return _autoTempCalibrating; }
    bool isAutoTempCalibrated() const { return _autoTempCalibrated; }
    uint8_t getAutoTempCalibrationProgress() const { return _autoTempProgress; }
    uint16_t getCurrentColorTemperature() const { return _currentTempK; }
    uint16_t getRecommendedColorTemperature(uint16_t ambientLight) const;

private:
    // Отфильтрованные значения датчика и оценка света без вклада лент.
    LowPassFilter _filter;              // Фильтр для сырых показаний датчика
    uint16_t _lastRawFiltered;           // Отфильтрованное общее освещение (для яркости)
    uint16_t _lastAmbientWithoutStrip;   // Оценка естественного света (для температуры)
    
    // Фазы плавной подстройки яркости к целевому уровню.
    enum AdjustmentPhase {
        PHASE_IDLE,
        PHASE_WAIT_START,
        PHASE_ADJUSTING
    };
    
    AdjustmentPhase _phase;
    unsigned long _phaseStartTime;
    unsigned long _lastAdjustTime;
    int _lastBrightness;
    unsigned long _currentAdjustInterval;

    // Таблица вклада ленты в показания датчика при разных температурах.
    struct TemperatureCalibrationPoint {
        uint16_t temperature;
        float contribution[256];
    };

    struct CalibrationData {
        static const int TEMP_POINTS = 5;
        TemperatureCalibrationPoint tempPoints[TEMP_POINTS];
        bool calibrated = false;
        float getContribution(uint8_t brightness, uint16_t temperature) const;
    };

    CalibrationData _cal;

    // Состояние пошаговой калибровки автотемпературы.
    enum AutoTempCalibPhase {
        CAL_PHASE_IDLE,
        CAL_PHASE_WAIT_DARK,
        CAL_PHASE_MEASURE_BASE,
        CAL_PHASE_SET_LEVEL,
        CAL_PHASE_MEASURE_LEVEL,
        CAL_PHASE_INTERPOLATE,
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
    
    // Пороговые значения и интервалы регулировки.
    static const unsigned long CYCLE_INTERVAL = 3000;      // Пауза между циклами проверки автояркости.
    static const unsigned long MIN_ADJUST_INTERVAL = 200;  // Минимальная задержка между шагами изменения яркости.
    static const unsigned long MAX_ADJUST_INTERVAL = 500;  // Максимальная задержка между шагами изменения яркости.
    static const uint8_t BRIGHTNESS_STEP = 1;              // Размер одного шага изменения яркости.
    static const uint8_t HYSTERESIS = 50;                  // Допустимая зона отклонения от целевой освещенности.
    static const uint16_t CLOSE_THRESHOLD = 500;           // Граница, после которой подстройка считается близкой к цели.
    static const float AMBIENT_SMOOTHING;                  // Коэффициент сглаживания для естественного света.

    // Внутренние шаги регулировки и калибровки.
    void startAdjustmentCycle();
    void performAdjustmentStep(uint16_t target, uint16_t current, LedStripController* led);
    bool isTargetReached(uint16_t target, uint16_t current) const;
    unsigned long calculateAdaptiveInterval(uint16_t target, uint16_t current) const;
    void processAutoTempCalibration(uint16_t ambientLight, LedStripController* led);
    uint16_t computeAmbientWithoutStrip(uint16_t totalLight, LedStripController* led);
};

#endif
