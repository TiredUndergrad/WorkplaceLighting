#include "ALSController.hpp"
#include <Arduino.h>

// Коэффициент сглаживания для естественного света (0..1, чем меньше, тем сильнее сглаживание)
const float ALSController::AMBIENT_SMOOTHING = 0.3f;

ALSController::ALSController()
    : _lastRawFiltered(0),
      _lastAmbientWithoutStrip(0),
      _phase(PHASE_IDLE),
      _phaseStartTime(0),
      _lastAdjustTime(0),
      _lastBrightness(0),
      _currentAdjustInterval(MIN_ADJUST_INTERVAL) {}

void ALSController::reset() {
    _filter.reset();
    _lastRawFiltered = 0;
    _lastAmbientWithoutStrip = 0;
    _phase = PHASE_IDLE;
    _currentAdjustInterval = MIN_ADJUST_INTERVAL;
    _autoTempCalibrating = false;
    _autoTempCalibrated = false;
    _autoTempProgress = 0;
    _calPhase = CAL_PHASE_IDLE;
}

void ALSController::startAdjustmentCycle() {
    _phase = PHASE_WAIT_START;
    _phaseStartTime = millis();
    _currentAdjustInterval = MIN_ADJUST_INTERVAL;
    Serial.println(F("ALS: Starting adjustment cycle in 3s"));
}

bool ALSController::isTargetReached(uint16_t target, uint16_t current) const {
    return abs((int)current - (int)target) <= HYSTERESIS;
}

unsigned long ALSController::calculateAdaptiveInterval(uint16_t target, uint16_t current) const {
    int diff = abs((int)current - (int)target);
    
    if (diff > CLOSE_THRESHOLD) {
        return MIN_ADJUST_INTERVAL;
    }
    if (diff <= HYSTERESIS) {
        return MAX_ADJUST_INTERVAL;
    }
    
    float ratio = (float)(diff - HYSTERESIS) / (CLOSE_THRESHOLD - HYSTERESIS);
    ratio = constrain(ratio, 0.0f, 1.0f);
    
    return MIN_ADJUST_INTERVAL + (unsigned long)((MAX_ADJUST_INTERVAL - MIN_ADJUST_INTERVAL) * (1.0f - ratio));
}

void ALSController::startAutoTempCalibration() {
    _autoTempCalibrating = true;
    _autoTempCalibrated = false;
    _autoTempProgress = 0;
    _autoTempCalibStartMs = millis();
    _calPhase = CAL_PHASE_WAIT_DARK;
    _baseLevel = 0;
    _calTempIndex = 0;
    _calBrightnessIndex = 0;
    _levelStartMs = 0;
    _samplesCollected = 0;
    _samplesSum = 0;
    _cal.calibrated = false;
    Serial.println(F("ALS: Auto temperature calibration started. Please block ambient light on sensor."));
}

uint16_t ALSController::getRecommendedColorTemperature(uint16_t ambientLight) const {
    static const uint16_t kruithofPoints[][2] = {
        {0,    2000},
        {50,   2700},
        {100,  3000},
        {300,  4000},
        {500,  5000},
        {1000, 5500},
        {10000,6500}
    };

    float lux = ambientLight * 0.1f;
    const int numPoints = sizeof(kruithofPoints) / sizeof(kruithofPoints[0]);

    if (lux <= kruithofPoints[0][0]) return kruithofPoints[0][1];
    if (lux >= kruithofPoints[numPoints - 1][0]) return kruithofPoints[numPoints - 1][1];

    for (int i = 0; i < numPoints - 1; i++) {
        if (lux >= kruithofPoints[i][0] && lux <= kruithofPoints[i + 1][0]) {
            float fraction = (lux - kruithofPoints[i][0]) /
                             (kruithofPoints[i + 1][0] - kruithofPoints[i][0]);
            return kruithofPoints[i][1] +
                   fraction * (kruithofPoints[i + 1][1] - kruithofPoints[i][1]);
        }
    }
    return 4000;
}

float ALSController::CalibrationData::getContribution(uint8_t brightness, uint16_t temperature) const {
    if (!calibrated) return 0.0f;

    int lowerIdx = -1;
    int upperIdx = -1;

    for (int i = 0; i < TEMP_POINTS; i++) {
        if (tempPoints[i].temperature <= temperature) lowerIdx = i;
        if (tempPoints[i].temperature >= temperature && upperIdx == -1) upperIdx = i;
    }

    if (lowerIdx == -1) return tempPoints[0].contribution[brightness];
    if (upperIdx == -1) return tempPoints[TEMP_POINTS - 1].contribution[brightness];
    if (lowerIdx == upperIdx) return tempPoints[lowerIdx].contribution[brightness];

    float tLow = tempPoints[lowerIdx].temperature;
    float tHigh = tempPoints[upperIdx].temperature;
    float fraction = (temperature - tLow) / (tHigh - tLow);

    float cLow = tempPoints[lowerIdx].contribution[brightness];
    float cHigh = tempPoints[upperIdx].contribution[brightness];

    return cLow * (1.0f - fraction) + cHigh * fraction;
}

void ALSController::performAdjustmentStep(uint16_t target, uint16_t current, LedStripController* led) {
    if (!led) return;
    
    int diff = (int)current - (int)target;
    uint8_t currentBrightness = led->getCurrentEffect().val;
    
    if (abs(diff) <= HYSTERESIS) {
        _phase = PHASE_IDLE;
        Serial.println(F("ALS: Target reached, cycle complete"));
        return;
    }
    
    if (diff > 0) {
        if (currentBrightness > BRIGHTNESS_STEP) {
            led->setBrightness(currentBrightness - BRIGHTNESS_STEP);
        } else {
            led->setBrightness(0);
        }
        Serial.print(F("ALS: Too bright, decreasing to "));
        Serial.println(currentBrightness - BRIGHTNESS_STEP);
    } else {
        if (currentBrightness < 255 - BRIGHTNESS_STEP) {
            led->setBrightness(currentBrightness + BRIGHTNESS_STEP);
        } else {
            led->setBrightness(255);
        }
        Serial.print(F("ALS: Too dark, increasing to "));
        Serial.println(currentBrightness + BRIGHTNESS_STEP);
    }
    
    _currentAdjustInterval = calculateAdaptiveInterval(target, current);
    
    Serial.print(F("ALS: New adjust interval = "));
    Serial.print(_currentAdjustInterval);
    Serial.print(F("ms (diff="));
    Serial.print(abs(diff));
    Serial.println(F(")"));
}

void ALSController::processAutoTempCalibration(uint16_t ambientLight, LedStripController* led) {
    const int BASE_SAMPLES = 15;
    const int LEVEL_SAMPLES = 3;
    const unsigned long WAIT_DARK_MS = 3000;
    const unsigned long STABILIZE_MS = 800;

    static const uint16_t calTemps[CalibrationData::TEMP_POINTS] = { 2000, 3500, 5000, 6500, 8000 };
    static const uint8_t brightnessSteps[] = { 0, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 250 };
    const int BRIGHTNESS_COUNT = sizeof(brightnessSteps) / sizeof(brightnessSteps[0]);

    unsigned long now = millis();

    switch (_calPhase) {
        case CAL_PHASE_WAIT_DARK:
            if (now - _autoTempCalibStartMs >= WAIT_DARK_MS) {
                _calPhase = CAL_PHASE_MEASURE_BASE;
                _samplesCollected = 0;
                _samplesSum = 0;
                Serial.println(F("ALS: Measuring base level (dark)..."));
            }
            break;

        case CAL_PHASE_MEASURE_BASE:
            _samplesSum += ambientLight;
            _samplesCollected++;
            if (_samplesCollected >= BASE_SAMPLES) {
                _baseLevel = (uint16_t)(_samplesSum / BASE_SAMPLES);
                Serial.print(F("ALS: Base level = "));
                Serial.println(_baseLevel);
                _calTempIndex = 0;
                _calBrightnessIndex = 0;
                _calPhase = CAL_PHASE_SET_LEVEL;
                _samplesCollected = 0;
                _samplesSum = 0;
            }
            break;

        case CAL_PHASE_SET_LEVEL: {
            if (!led || _calTempIndex >= CalibrationData::TEMP_POINTS) {
                _calPhase = CAL_PHASE_DONE;
                break;
            }

            uint16_t tempK = calTemps[_calTempIndex];
            _cal.tempPoints[_calTempIndex].temperature = tempK;
            uint8_t b = brightnessSteps[_calBrightnessIndex];

            uint16_t clamped = constrain(tempK, 2000, 10000);
            uint8_t tempNorm = (uint8_t)(((clamped - 2000) * 255) / 8000);

            led->turnOn();
            led->setGlobalEffect(1, b, 1, 0, tempNorm, 255, 255);

            _levelStartMs = now;
            _samplesCollected = 0;
            _samplesSum = 0;
            _calPhase = CAL_PHASE_MEASURE_LEVEL;
            break;
        }

        case CAL_PHASE_MEASURE_LEVEL: {
            if (now - _levelStartMs < STABILIZE_MS) break;

            _samplesSum += ambientLight;
            _samplesCollected++;

            if (_samplesCollected >= LEVEL_SAMPLES) {
                uint16_t reading = (uint16_t)(_samplesSum / LEVEL_SAMPLES);
                float contribution = (float)reading - (float)_baseLevel;
                if (contribution < 0.0f) contribution = 0.0f;

                uint8_t b = brightnessSteps[_calBrightnessIndex];
                _cal.tempPoints[_calTempIndex].contribution[b] = contribution;

                _calBrightnessIndex++;
                _samplesCollected = 0;
                _samplesSum = 0;

                if (_calBrightnessIndex >= BRIGHTNESS_COUNT) {
                    _calPhase = CAL_PHASE_INTERPOLATE;
                } else {
                    _calPhase = CAL_PHASE_SET_LEVEL;
                }
            }
            break;
        }

        case CAL_PHASE_INTERPOLATE: {
            const uint8_t* bArr = brightnessSteps;
            for (int idx = 0; idx < BRIGHTNESS_COUNT - 1; ++idx) {
                uint8_t b1 = bArr[idx];
                uint8_t b2 = bArr[idx + 1];
                float v1 = _cal.tempPoints[_calTempIndex].contribution[b1];
                float v2 = _cal.tempPoints[_calTempIndex].contribution[b2];

                for (int b = b1 + 1; b < b2; ++b) {
                    float fraction = (float)(b - b1) / (float)(b2 - b1);
                    _cal.tempPoints[_calTempIndex].contribution[b] =
                        v1 * (1.0f - fraction) + v2 * fraction;
                }
            }

            for (int b = 0; b < brightnessSteps[0]; ++b) {
                _cal.tempPoints[_calTempIndex].contribution[b] = 0.0f;
            }
            uint8_t lastB = brightnessSteps[BRIGHTNESS_COUNT - 1];
            float lastV = _cal.tempPoints[_calTempIndex].contribution[lastB];
            for (int b = lastB + 1; b <= 255; ++b) {
                _cal.tempPoints[_calTempIndex].contribution[b] = lastV;
            }

            _calTempIndex++;
            _calBrightnessIndex = 0;
            _autoTempProgress = (uint8_t)((_calTempIndex * 100) / CalibrationData::TEMP_POINTS);

            if (_calTempIndex >= CalibrationData::TEMP_POINTS) {
                _calPhase = CAL_PHASE_DONE;
            } else {
                _calPhase = CAL_PHASE_SET_LEVEL;
            }
            break;
        }

        case CAL_PHASE_DONE:
            if (led) led->setBrightness(0);
            _cal.calibrated = true;
            _autoTempCalibrating = false;
            _autoTempCalibrated = true;
            _autoTempProgress = 100;
            _calPhase = CAL_PHASE_IDLE;
            Serial.println(F("ALS: Auto temperature calibration finished"));
            // Вывод сводки для отладки
            Serial.println(F("Calibration summary (contribution at 100% brightness):"));
            for (int i = 0; i < CalibrationData::TEMP_POINTS; i++) {
                Serial.print(calTemps[i]); Serial.print(F("K: "));
                Serial.println(_cal.tempPoints[i].contribution[250]);
            }
            break;

        default:
            break;
    }
}

uint16_t ALSController::computeAmbientWithoutStrip(uint16_t totalLight, LedStripController* led) {
    if (!led || !_cal.calibrated) return totalLight; // если калибровки нет, возвращаем сырое

    EffectParams cur = led->getCurrentEffect();
    if (cur.effect != 1) return totalLight; // только для режима температуры белого

    uint16_t kelvin = 2000 + (uint16_t)(cur.temp) * 8000 / 255;
    float ledContribution = _cal.getContribution(cur.val, kelvin);
    float ambientOnly = (float)totalLight - ledContribution;
    if (ambientOnly < 0.0f) ambientOnly = 0.0f;
    
    return (uint16_t)ambientOnly;
}

void ALSController::update(uint16_t ambientLight, const AdvParams& params,
                            LedStripController* led, bool motionActive) {
    // Если идёт калибровка — обрабатываем и выходим
    if (_autoTempCalibrating) {
        processAutoTempCalibration(ambientLight, led);
        return;
    }

    // Фильтруем сырые показания для регулировки яркости
    _filter.setK(params.k);
    float filtered = _filter.update((float)ambientLight);
    _lastRawFiltered = (uint16_t)filtered;

    // Вычисляем оценку естественного света (без вклада ленты) на основе сырых данных
    // Используем сырые, потому что калибровка строилась по сырым
    uint16_t rawAmbient = computeAmbientWithoutStrip(ambientLight, led);

    // Сглаживаем оценку естественного света для температуры
    // Если лента выключена или эффект не температура, сбрасываем фильтр, чтобы быстро обновить
    static bool lastValid = false;
    static uint16_t lastRawAmbient = 0;
    if (!led || !led->isOn() || (led->getCurrentEffect().effect != 1)) {
        // Сбрасываем фильтр, чтобы при включении быстро получить актуальное значение
        _lastAmbientWithoutStrip = rawAmbient;
        lastRawAmbient = rawAmbient;
        lastValid = false;
    } else {
        // Экспоненциальное сглаживание
        if (!lastValid) {
            _lastAmbientWithoutStrip = rawAmbient;
            lastValid = true;
        } else {
            // Используем AMBIENT_SMOOTHING для плавности
            _lastAmbientWithoutStrip = (uint16_t)(_lastAmbientWithoutStrip * (1.0f - AMBIENT_SMOOTHING) + rawAmbient * AMBIENT_SMOOTHING);
        }
        lastRawAmbient = rawAmbient;
    }

    // Если лента выключена, не регулируем яркость
    if (!led || !led->isOn()) {
        _phase = PHASE_IDLE;
        return;
    }
    
    bool allowRegulation = motionActive || !params.moveMode;
    unsigned long now = millis();
    uint16_t target = params.aimLight;
    
    // Регулировка яркости по отфильтрованному общему свету
    if (params.brightMode && allowRegulation) {
        switch (_phase) {
            case PHASE_IDLE:
                if (!isTargetReached(target, _lastRawFiltered)) {
                    startAdjustmentCycle();
                }
                break;
                
            case PHASE_WAIT_START:
                if (now - _phaseStartTime >= CYCLE_INTERVAL) {
                    _phase = PHASE_ADJUSTING;
                    _lastAdjustTime = now;
                    _lastBrightness = led->getCurrentEffect().val;
                    _currentAdjustInterval = MIN_ADJUST_INTERVAL;
                    Serial.println(F("ALS: Starting brightness adjustment"));
                }
                break;
                
            case PHASE_ADJUSTING:
                if (!isTargetReached(target, _lastRawFiltered)) {
                    if (now - _lastAdjustTime >= _currentAdjustInterval) {
                        performAdjustmentStep(target, _lastRawFiltered, led);
                        _lastAdjustTime = now;
                    }
                } else {
                    _phase = PHASE_IDLE;
                    Serial.println(F("ALS: Target reached during adjustment"));
                }
                break;
        }
    } else {
        _phase = PHASE_IDLE;
    }

    // Автонастройка цветовой температуры по сглаженной оценке естественного света
    bool autoTempActive = led && led->isOn() && (
        (!led->isIndividualMode() && params.autoTempMode) ||
        (led->isIndividualMode() && (params.autoTempZone1 || params.autoTempZone2 || params.autoTempZone3))
    );
    if (autoTempActive) {
        uint16_t recommended = getRecommendedColorTemperature(_lastAmbientWithoutStrip);

        if (led->isIndividualMode()) {
            SplittingParams sp = led->getSplittingParams();
            bool changed = false;
            if (sp.effect1 == 1 && sp.enabled1 && params.autoTempZone1) {
                uint16_t curK = (sp.temp1 * 8000 / 255) + 2000;
                int16_t diff = (int16_t)recommended - (int16_t)curK;
                if (abs(diff) > 20) {
                    int step = constrain(abs(diff) / 10, 5, 50);
                    curK += (diff > 0) ? step : -step;
                    curK = constrain(curK, 2000, 10000);
                    sp.temp1 = (uint8_t)(((curK - 2000) * 255) / 8000);
                    changed = true;
                }
            }
            if (sp.effect2 == 1 && sp.enabled2 && params.autoTempZone2) {
                uint16_t curK = (sp.temp2 * 8000 / 255) + 2000;
                int16_t diff = (int16_t)recommended - (int16_t)curK;
                if (abs(diff) > 20) {
                    int step = constrain(abs(diff) / 10, 5, 50);
                    curK += (diff > 0) ? step : -step;
                    curK = constrain(curK, 2000, 10000);
                    sp.temp2 = (uint8_t)(((curK - 2000) * 255) / 8000);
                    changed = true;
                }
            }
            if (sp.effect3 == 1 && sp.enabled3 && params.autoTempZone3) {
                uint16_t curK = (sp.temp3 * 8000 / 255) + 2000;
                int16_t diff = (int16_t)recommended - (int16_t)curK;
                if (abs(diff) > 20) {
                    int step = constrain(abs(diff) / 10, 5, 50);
                    curK += (diff > 0) ? step : -step;
                    curK = constrain(curK, 2000, 10000);
                    sp.temp3 = (uint8_t)(((curK - 2000) * 255) / 8000);
                    changed = true;
                }
            }
            if (changed) led->setSplitting(sp);
        } else {
            EffectParams cur = led->getCurrentEffect();
            if (cur.effect == 1) {
                int16_t diff = (int16_t)recommended - (int16_t)_currentTempK;
                if (abs(diff) > 20) {
                    int step = constrain(abs(diff) / 10, 5, 50);
                    if (diff > 0) _currentTempK += step;
                    else _currentTempK -= step;
                    _currentTempK = constrain(_currentTempK, 2000, 10000);
                    uint8_t tempNorm = (uint8_t)(((_currentTempK - 2000) * 255) / 8000);
                    led->setGlobalEffect(cur.effect, cur.val, cur.speed, cur.color, tempNorm, cur.sat, cur.hsvVal);
                }
            }
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
    s += _lastRawFiltered;
    s += F(" ambientWithoutStrip=");
    s += _lastAmbientWithoutStrip;
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
    s += F(" autoTemp=");
    s += params.autoTempMode ? 1 : 0;
    s += F(" autoTempCalibrated=");
    s += _autoTempCalibrated ? 1 : 0;
    
    return s;
}