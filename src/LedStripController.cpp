#include "LedStripController.hpp"
#include "TimerHandler.hpp"
#include "MotionHandler.hpp"
#include "ALSController.hpp"
#include "AdvParams.h"
#include <math.h>

// Конструктор: принимает три ленты
LedStripController::LedStripController(CRGB* leds1, uint16_t count1,
  CRGB* leds2, uint16_t count2,
  CRGB* leds3, uint16_t count3)
: _isOn(false), _userRequestedOff(false), _splittingMode(false), _block(false) {
  
  _leds[0] = leds1;
  _leds[1] = leds2;
  _leds[2] = leds3;
  _ledCounts[0] = count1;
  _ledCounts[1] = count2;
  _ledCounts[2] = count3;

  // Инициализация параметров эффекта по умолчанию для всех лент
  _globalEffectParams = {1, 128, 1, 0, 32, 255, 255};

  // Инициализация параметров разделения (три ленты)
  _splittingParams.numSplit = 3;
  _splittingParams.effect1 = 1;
  _splittingParams.val1 = 128;
  _splittingParams.speed1 = 1;
  _splittingParams.color1 = 0;
  _splittingParams.temp1 = 32;
  _splittingParams.sat1 = 255;
  _splittingParams.hsvVal1 = 255;
  _splittingParams.enabled1 = true;

  _splittingParams.effect2 = 1;
  _splittingParams.val2 = 128;
  _splittingParams.speed2 = 1;
  _splittingParams.color2 = 0;
  _splittingParams.temp2 = 32;
  _splittingParams.sat2 = 255;
  _splittingParams.hsvVal2 = 255;
  _splittingParams.enabled2 = true;

  _splittingParams.effect3 = 1;
  _splittingParams.val3 = 128;
  _splittingParams.speed3 = 1;
  _splittingParams.color3 = 0;
  _splittingParams.temp3 = 32;
  _splittingParams.sat3 = 255;
  _splittingParams.hsvVal3 = 255;
  _splittingParams.enabled3 = true;

  // Инициализация переменных для эффектов для каждой зоны
  for (int i = 0; i < 3; i++) {
    _rainbowPreviousMillis[i] = 0;
    _twinklePreviousMillis[i] = 0;
    _purplePreviousMillis[i] = 0;
    _twinkleHue[i] = 0;
    _purpleHue[i] = 160;
  }

  // Реестр эффектов
  _effectHandlers[0] = &LedStripController::effectStatic;
  _effectHandlers[1] = &LedStripController::effectTemperature;
  _effectHandlers[2] = &LedStripController::effectRainbow;
  _effectHandlers[3] = &LedStripController::effectPulse;
  _effectHandlers[4] = &LedStripController::effectTwinkle;
  _effectHandlers[5] = &LedStripController::effectSmoothPurple;
}

void LedStripController::begin() {
  FastLED.setMaxRefreshRate(90);
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();
}

void LedStripController::setDependencies(TimerHandler* timer, MotionHandler* motion, ALSController* als) {
  _timerHandler = timer;
  _motionHandler = motion;
  _alsController = als;
}

void LedStripController::update(AdvParams& advParams, uint8_t& motionMaxBrightness, uint16_t ambientLight) {
  // 1. Таймер: если идёт мигание, он сам управляет подсветкой — выходим
  if (_timerHandler && _timerHandler->update(advParams, _isOn))
    return;

  // 2. Движение и автояркость обновляют состояние ленты (вкл/выкл, яркость)
  if (_motionHandler)
    _motionHandler->update(advParams, motionMaxBrightness, this);
  if (_alsController)
    _alsController->update(ambientLight, advParams, this, _motionHandler ? _motionHandler->isMotionActive() : true);

  // 3. Отрисовка эффектов
  update();
}

void LedStripController::update() {
  if (!_isOn) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  if (_block) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  if (_splittingMode) {
    // Режим разделения: применяем к каждой ленте свои параметры
    EffectParams p;
    
    // Лента 1 - применяем только если включена
    if (_splittingParams.enabled1) {
      p = { _splittingParams.effect1, _splittingParams.val1, _splittingParams.speed1,
            _splittingParams.color1, _splittingParams.temp1, _splittingParams.sat1, _splittingParams.hsvVal1 };
      applyEffectToZone(0, p);
    } else {
      // Если зона выключена - гасим её
      for (int i = 0; i < _ledCounts[0]; i++) {
        _leds[0][i] = CRGB::Black;
      }
    }
    
    // Лента 2
    if (_splittingParams.enabled2) {
      p = { _splittingParams.effect2, _splittingParams.val2, _splittingParams.speed2,
            _splittingParams.color2, _splittingParams.temp2, _splittingParams.sat2, _splittingParams.hsvVal2 };
      applyEffectToZone(1, p);
    } else {
      for (int i = 0; i < _ledCounts[1]; i++) {
        _leds[1][i] = CRGB::Black;
      }
    }
    
    // Лента 3
    if (_splittingParams.enabled3) {
      p = { _splittingParams.effect3, _splittingParams.val3, _splittingParams.speed3,
            _splittingParams.color3, _splittingParams.temp3, _splittingParams.sat3, _splittingParams.hsvVal3 };
      applyEffectToZone(2, p);
    } else {
      for (int i = 0; i < _ledCounts[2]; i++) {
        _leds[2][i] = CRGB::Black;
      }
    }
    
  } else {
    // Режим "все вместе": применяем одинаковые параметры ко всем трем лентам
    // В этом режиме все зоны всегда включены (или выключены общей кнопкой)
    for (int zone = 0; zone < 3; zone++) {
      applyEffectToZone(zone, _globalEffectParams);
    }
  }
  
  FastLED.show();
}

// Приватный метод: применяет эффект к конкретной ленте
void LedStripController::applyEffectToZone(uint8_t zoneIndex, const EffectParams& params) {
  if (zoneIndex > 2) return;
  if (params.effect < EFFECT_COUNT && _effectHandlers[params.effect]) {
    (this->*_effectHandlers[params.effect])(params, zoneIndex);
  } else {
    // Если эффект не найден, гасим ленту
    for (int i = 0; i < _ledCounts[zoneIndex]; i++) {
      _leds[zoneIndex][i] = CRGB::Black;
    }
  }
}

// Установка эффекта для ВСЕХ лент
void LedStripController::setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat, uint8_t hsvVal) {
  _splittingMode = false; // Выходим из режима разделения
  
  // Сохраняем в глобальные параметры
  _globalEffectParams.effect = effect;
  _globalEffectParams.val = val;
  _globalEffectParams.speed = speed;
  _globalEffectParams.color = color;
  _globalEffectParams.temp = temp;
  _globalEffectParams.sat = sat;
  _globalEffectParams.hsvVal = hsvVal;
  
  // Дублируем в параметры разделения, чтобы при следующем включении режима разделения
  // ленты стартовали с последними заданными для них параметрами.
  _splittingParams.effect1 = effect; _splittingParams.val1 = val; _splittingParams.speed1 = speed; _splittingParams.color1 = color; _splittingParams.temp1 = temp; _splittingParams.sat1 = sat; _splittingParams.hsvVal1 = hsvVal;
  _splittingParams.effect2 = effect; _splittingParams.val2 = val; _splittingParams.speed2 = speed; _splittingParams.color2 = color; _splittingParams.temp2 = temp; _splittingParams.sat2 = sat; _splittingParams.hsvVal2 = hsvVal;
  _splittingParams.effect3 = effect; _splittingParams.val3 = val; _splittingParams.speed3 = speed; _splittingParams.color3 = color; _splittingParams.temp3 = temp; _splittingParams.sat3 = sat; _splittingParams.hsvVal3 = hsvVal;
}

// Установка эффекта для конкретной ленты
void LedStripController::setZoneEffect(uint8_t zoneIndex, uint8_t effect, uint8_t val, 
                                       uint8_t speed, uint8_t color, uint8_t temp, 
                                       uint8_t sat, uint8_t hsvVal, bool enabled) {
    // Включаем режим разделения, т.к. мы задаем параметры для конкретной ленты
    _splittingMode = true;
    
    // Обновляем нужную зону в _splittingParams
    switch (zoneIndex) {
        case 0:
            _splittingParams.effect1 = effect; 
            _splittingParams.val1 = val; 
            _splittingParams.speed1 = speed; 
            _splittingParams.color1 = color; 
            _splittingParams.temp1 = temp; 
            _splittingParams.sat1 = sat; 
            _splittingParams.hsvVal1 = hsvVal; 
            _splittingParams.enabled1 = enabled;
            break;
        case 1:
            _splittingParams.effect2 = effect; 
            _splittingParams.val2 = val; 
            _splittingParams.speed2 = speed; 
            _splittingParams.color2 = color; 
            _splittingParams.temp2 = temp; 
            _splittingParams.sat2 = sat; 
            _splittingParams.hsvVal2 = hsvVal; 
            _splittingParams.enabled2 = enabled;
            break;
        case 2:
            _splittingParams.effect3 = effect; 
            _splittingParams.val3 = val; 
            _splittingParams.speed3 = speed; 
            _splittingParams.color3 = color; 
            _splittingParams.temp3 = temp; 
            _splittingParams.sat3 = sat; 
            _splittingParams.hsvVal3 = hsvVal; 
            _splittingParams.enabled3 = enabled;
            break;
    }
}

void LedStripController::setSplitting(const SplittingParams& params) {
  _splittingMode = true; // Переключаемся в режим разделения
  _splittingParams = params;
  _splittingParams.numSplit = 3; // Принудительно 3, игнорируем входящее значение
}

void LedStripController::setBrightness(uint8_t val) {
  // Обновляем яркость везде
  _globalEffectParams.val = val;
  
  _splittingParams.val1 = val;
  _splittingParams.val2 = val;
  _splittingParams.val3 = val;
}

EffectParams LedStripController::getCurrentEffect() const {
  return _globalEffectParams;
}

SplittingParams LedStripController::getSplittingParams() const {
  return _splittingParams;
}

uint16_t LedStripController::getLedCount(uint8_t zone) const {
  if (zone < 3) return _ledCounts[zone];
  return 0;
}

// ---------- Методы эффектов ----------

void LedStripController::effectStatic(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  for (int i = 0; i < count; i++) {
    leds[i] = CHSV(p.color, p.sat, p.hsvVal);
    leds[i].nscale8(p.val);
  }
}

void LedStripController::effectTemperature(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  
  // p.temp уже нормализован в диапазоне 0-255 на сервере
  // Конвертируем обратно в Кельвины для логики
  uint16_t kelvin = 2000 + (p.temp * 8000 / 255);
  
  uint8_t hue;
  uint8_t saturation;
  uint8_t brightness = 255; // Яркость цвета
  
  if (kelvin <= 6000) {
      // От 2000K до 6000K: hue от 24 до 40
      hue = map(kelvin, 2000, 6000, 24, 50);
      
      // Насыщенность от 255 (2000K) до 0 (6000K)
      saturation = map(kelvin, 2000, 6000, 255, 0);
  } else {
      // От 6000K до 10000K: hue от 40 до 141
      // hue = map(kelvin, 6000, 10000, 50, 141);
      hue = 141;
      // Насыщенность от 0 (6000K) до 87 (10000K)
      saturation = map(kelvin, 6000, 10000, 0, 87);
  }
  
  // Ограничиваем значения
  hue = constrain(hue, 0, 255);
  saturation = constrain(saturation, 0, 255);
  
  for (int i = 0; i < count; i++) {
      leds[i] = CHSV(hue, saturation, brightness);
      leds[i].nscale8(p.val); // Применяем общую яркость
  }
}

void LedStripController::effectRainbow(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  
  const int waveSpeed = 15;
  unsigned long currentMillis = millis();
  const long interval = count > 0 ? 1000 / count : 50;

  if (currentMillis - _rainbowPreviousMillis[zoneIndex] >= interval) {
    for (int i = 0; i < count; i++) {
      int colorIndex = i * 256 / count + millis() * p.speed / waveSpeed;
      leds[i] = CHSV(colorIndex, 255, 255);
      leds[i].nscale8(p.val);
    }
    _rainbowPreviousMillis[zoneIndex] = currentMillis;
  }
}

void LedStripController::effectPulse(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  
  static uint8_t minVal = 0;
  const int cycleTime = 3000;
  CRGB colors[] = { CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Indigo, CRGB::Violet };

  unsigned long currentMillis = millis() * p.speed;
  int brightness = map(constrain(currentMillis % cycleTime, 0, cycleTime), 0, cycleTime, minVal, p.val);
  uint8_t colorIndex = (currentMillis / cycleTime) % (sizeof(colors) / sizeof(colors[0]));
  CRGB color = colors[colorIndex];
  
  for (int i = 0; i < count; i++) {
    leds[i] = color;
    leds[i].fadeToBlackBy(255 - brightness);
  }
}

void LedStripController::effectTwinkle(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  
  const int speedEffect = 10;
  unsigned long currentMillis = p.speed * millis();

  if (currentMillis - _twinklePreviousMillis[zoneIndex] >= speedEffect) {
    _twinklePreviousMillis[zoneIndex] = currentMillis;
    _twinkleHue[zoneIndex]++;
  }
  for (int i = 0; i < count; i++) {
    leds[i] = CHSV(_twinkleHue[zoneIndex], p.sat, p.val);
  }
}

void LedStripController::effectSmoothPurple(const EffectParams& p, uint8_t zoneIndex) {
  CRGB* leds = _leds[zoneIndex];
  uint16_t count = _ledCounts[zoneIndex];
  
  const int cycleTime = 3000;
  unsigned long currentMillis = millis() * p.speed;
  float position = fmod((float)currentMillis, (float)cycleTime) / cycleTime;

  CRGB colorStart = CHSV(140, 255, p.val);
  CRGB colorEnd = CHSV(200, 255, p.val);
  CRGB color;
  if (position <= 0.5) {
    color = colorStart.nscale8(255 - position * 510) + colorEnd.nscale8(position * 510);
  } else {
    float pos = 1 - position;
    color = colorStart.nscale8(255 - pos * 510) + colorEnd.nscale8(pos * 510);
  }
  for (int i = 0; i < count; i++) {
    leds[i] = color;
  }
}

bool LedStripController::isTimerBlinking() const {
  return _timerHandler ? _timerHandler->isBlinking() : false;
}

void LedStripController::stopTimerBlinking() {
  if (_timerHandler)
    _timerHandler->stopBlinking();
}

// ---------- Методы для отладки ----------

void LedStripController::debugPrintParams() const {
  Serial.println(F("--- LedStripController params (3 strips) ---"));
  Serial.print(F("_isOn=")); Serial.println(_isOn);
  Serial.print(F("_splittingMode=")); Serial.println(_splittingMode);
  
  Serial.println(F("_globalEffectParams (all strips):"));
  Serial.print(F("  effect=")); Serial.println(_globalEffectParams.effect);
  Serial.print(F("  val=")); Serial.println(_globalEffectParams.val);
  Serial.print(F("  speed=")); Serial.println(_globalEffectParams.speed);
  Serial.print(F("  color=")); Serial.println(_globalEffectParams.color);
  Serial.print(F("  temp=")); Serial.println(_globalEffectParams.temp);
  Serial.print(F("  sat=")); Serial.println(_globalEffectParams.sat);
  Serial.print(F("  hsvVal=")); Serial.println(_globalEffectParams.hsvVal);
  
  Serial.println(F("_splittingParams (per strip):"));
  Serial.print(F("  Strip1: effect=")); Serial.print(_splittingParams.effect1);
  Serial.print(F(" val=")); Serial.print(_splittingParams.val1);
  Serial.print(F(" speed=")); Serial.print(_splittingParams.speed1);
  Serial.print(F(" color=")); Serial.print(_splittingParams.color1);
  Serial.print(F(" temp=")); Serial.print(_splittingParams.temp1);
  Serial.print(F(" sat=")); Serial.print(_splittingParams.sat1);
  Serial.print(F(" hsvVal=")); Serial.println(_splittingParams.hsvVal1);
  
  Serial.print(F("  Strip2: effect=")); Serial.print(_splittingParams.effect2);
  Serial.print(F(" val=")); Serial.print(_splittingParams.val2);
  Serial.print(F(" speed=")); Serial.print(_splittingParams.speed2);
  Serial.print(F(" color=")); Serial.print(_splittingParams.color2);
  Serial.print(F(" temp=")); Serial.print(_splittingParams.temp2);
  Serial.print(F(" sat=")); Serial.print(_splittingParams.sat2);
  Serial.print(F(" hsvVal=")); Serial.println(_splittingParams.hsvVal2);
  
  Serial.print(F("  Strip3: effect=")); Serial.print(_splittingParams.effect3);
  Serial.print(F(" val=")); Serial.print(_splittingParams.val3);
  Serial.print(F(" speed=")); Serial.print(_splittingParams.speed3);
  Serial.print(F(" color=")); Serial.print(_splittingParams.color3);
  Serial.print(F(" temp=")); Serial.print(_splittingParams.temp3);
  Serial.print(F(" sat=")); Serial.print(_splittingParams.sat3);
  Serial.print(F(" hsvVal=")); Serial.println(_splittingParams.hsvVal3);
  
  Serial.println(F("----------------------------------"));
}

String LedStripController::getStatusForLog() const {
  String s = F("LED: ");
  s += _isOn ? F("ON") : F("OFF");
  
  if (_isOn) {
    if (_splittingMode) {
      s += F(" split mode.");
      s += F(" Strip1(e=");
      s += String(_splittingParams.effect1);
      s += F(",v=");
      s += String(_splittingParams.val1);
      s += F(")");
      s += F(" Strip2(e=");
      s += String(_splittingParams.effect2);
      s += F(",v=");
      s += String(_splittingParams.val2);
      s += F(")");
      s += F(" Strip3(e=");
      s += String(_splittingParams.effect3);
      s += F(",v=");
      s += String(_splittingParams.val3);
      s += F(")");
    } else {
      s += F(" global mode.");
      s += F(" effect=");
      s += String(_globalEffectParams.effect);
      s += F(" val=");
      s += String(_globalEffectParams.val);
      s += F(" speed=");
      s += String(_globalEffectParams.speed);
      s += F(" color=");
      s += String(_globalEffectParams.color);
      s += F(" sat=");
      s += String(_globalEffectParams.sat);
      s += F(" hsvVal=");
      s += String(_globalEffectParams.hsvVal);
    }
  }
  return s;
}

void LedStripController::turnOn() {
  _isOn = true;
  _userRequestedOff = false;
}

void LedStripController::turnOff(bool byUser) {
  _isOn = false;
  if (byUser) _userRequestedOff = true;
  FastLED.clear();
  FastLED.show();
}
