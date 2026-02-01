#include "LedStripController.hpp"
#include <math.h>

LedStripController::LedStripController(CRGB* leds, uint16_t ledCount, uint8_t dataPin)
  : _leds(leds), _ledCount(ledCount), _dataPin(dataPin), _splittingMode(false), _isOn(false), _userRequestedOff(false),
    _rainbowPreviousMillis(0), _twinklePreviousMillis(0), _purplePreviousMillis(0),
    _twinkleHue(0), _purpleHue(160) {
  // Инициализация параметров эффекта по умолчанию
  _effectParams.effect = 0;
  _effectParams.val = 128;
  _effectParams.speed = 1;
  _effectParams.color = 0;
  _effectParams.temp = 1;
  _effectParams.sat = 255;
  _effectParams.hsvVal = 255;
  _effectParams.startFrom = 0;
  _effectParams.end = ledCount;
  
  // Инициализация параметров разделения
  _splittingParams.numSplit = 0;

  // Реестр эффектов: новый эффект = добавить метод + одну строку сюда
  _effectHandlers[0] = &LedStripController::effectStatic;
  _effectHandlers[1] = &LedStripController::effectTemperature;
  _effectHandlers[2] = &LedStripController::effectRainbow;
  _effectHandlers[3] = &LedStripController::effectPulse;
  _effectHandlers[4] = &LedStripController::effectTwinkle;
  _effectHandlers[5] = &LedStripController::effectSmoothPurple;
}

void LedStripController::begin() {
  // Инициализация FastLED
  // Для ESP32 FastLED требует compile-time константу для пина в шаблоне
  // Используем switch для поддержки разных пинов
  // Если нужен другой пин, добавьте case для него
  switch (_dataPin) {
    case 2:
      FastLED.addLeds<WS2812B, 2, GRB>(_leds, _ledCount);
      break;
    case 4:
      FastLED.addLeds<WS2812B, 4, GRB>(_leds, _ledCount);
      break;
    case 5:
      FastLED.addLeds<WS2812B, 5, GRB>(_leds, _ledCount);
      break;
    case 12:
      FastLED.addLeds<WS2812B, 12, GRB>(_leds, _ledCount);
      break;
    case 13:
      FastLED.addLeds<WS2812B, 13, GRB>(_leds, _ledCount);
      break;
    case 14:
      FastLED.addLeds<WS2812B, 14, GRB>(_leds, _ledCount);
      break;
    case 15:
      FastLED.addLeds<WS2812B, 15, GRB>(_leds, _ledCount);
      break;
    case 16:
      FastLED.addLeds<WS2812B, 16, GRB>(_leds, _ledCount);
      break;
    case 17:
      FastLED.addLeds<WS2812B, 17, GRB>(_leds, _ledCount);
      break;
    case 18:
      FastLED.addLeds<WS2812B, 18, GRB>(_leds, _ledCount);
      break;
    case 19:
      FastLED.addLeds<WS2812B, 19, GRB>(_leds, _ledCount);
      break;
    case 21:
      FastLED.addLeds<WS2812B, 21, GRB>(_leds, _ledCount);
      break;
    case 22:
      FastLED.addLeds<WS2812B, 22, GRB>(_leds, _ledCount);
      break;
    case 23:
      FastLED.addLeds<WS2812B, 23, GRB>(_leds, _ledCount);
      break;
    case 25:
      FastLED.addLeds<WS2812B, 25, GRB>(_leds, _ledCount);
      break;
    case 26:
      FastLED.addLeds<WS2812B, 26, GRB>(_leds, _ledCount);
      break;
    case 27:
      FastLED.addLeds<WS2812B, 27, GRB>(_leds, _ledCount);
      break;
    case 32:
      FastLED.addLeds<WS2812B, 32, GRB>(_leds, _ledCount);
      break;
    case 33:
      FastLED.addLeds<WS2812B, 33, GRB>(_leds, _ledCount);
      break;
    default:
      // По умолчанию используем GPIO 4
      FastLED.addLeds<WS2812B, 4, GRB>(_leds, _ledCount);
      break;
  }
  
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();
}

void LedStripController::update() {
  if (!_isOn) {
    // Если лента выключена, гасим все светодиоды
    FastLED.clear();
    FastLED.show();
    return;
  }
  
  if (_splittingMode) {
    EffectParams p;
    if (_splittingParams.numSplit == 2) {
      p = { _splittingParams.effect1, _splittingParams.val1, _splittingParams.speed1,
            _splittingParams.color1, _splittingParams.temp1, _splittingParams.sat1, _splittingParams.hsvVal1,
            0, (uint8_t)(_ledCount / 2) };
      switchEffect(p);
      p = { _splittingParams.effect2, _splittingParams.val2, _splittingParams.speed2,
            _splittingParams.color2, _splittingParams.temp2, _splittingParams.sat2, _splittingParams.hsvVal2,
            (uint8_t)(_ledCount / 2), (uint8_t)_ledCount };
      switchEffect(p);
    } else if (_splittingParams.numSplit == 3) {
      p = { _splittingParams.effect1, _splittingParams.val1, _splittingParams.speed1,
            _splittingParams.color1, _splittingParams.temp1, _splittingParams.sat1, _splittingParams.hsvVal1,
            0, (uint8_t)(_ledCount / 3) };
      switchEffect(p);
      p = { _splittingParams.effect2, _splittingParams.val2, _splittingParams.speed2,
            _splittingParams.color2, _splittingParams.temp2, _splittingParams.sat2, _splittingParams.hsvVal2,
            (uint8_t)(_ledCount / 3), (uint8_t)((_ledCount * 2) / 3) };
      switchEffect(p);
      p = { _splittingParams.effect3, _splittingParams.val3, _splittingParams.speed3,
            _splittingParams.color3, _splittingParams.temp3, _splittingParams.sat3, _splittingParams.hsvVal3,
            (uint8_t)((_ledCount * 2) / 3), (uint8_t)_ledCount };
      switchEffect(p);
    }
  } else {
    switchEffect(_effectParams);
  }
  
  FastLED.show();
}

void LedStripController::setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t sat, uint8_t hsvVal) {
  _splittingMode = false;
  _effectParams.effect = effect;
  _effectParams.val = val;
  _effectParams.speed = speed;
  _effectParams.color = color;
  _effectParams.temp = temp;
  _effectParams.sat = sat;
  _effectParams.hsvVal = hsvVal;
  _effectParams.startFrom = 0;
  _effectParams.end = _ledCount;
}

void LedStripController::setZoneEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end, uint8_t sat, uint8_t hsvVal) {
  _splittingMode = false;
  _effectParams.effect = effect;
  _effectParams.val = val;
  _effectParams.speed = speed;
  _effectParams.color = color;
  _effectParams.temp = temp;
  _effectParams.sat = sat;
  _effectParams.hsvVal = hsvVal;
  _effectParams.startFrom = startFrom;
  _effectParams.end = end;
}

void LedStripController::setSplitting(const SplittingParams& params) {
  _splittingMode = true;
  _splittingParams = params;
}

void LedStripController::setBrightness(uint8_t val) {
  _effectParams.val = val;
  if (_splittingMode) {
    _splittingParams.val1 = val;
    _splittingParams.val2 = val;
    _splittingParams.val3 = val;
  }
}

EffectParams LedStripController::getCurrentEffect() const {
  return _effectParams;
}

SplittingParams LedStripController::getSplittingParams() const {
  return _splittingParams;
}

bool LedStripController::isSplittingMode() const {
  return _splittingMode;
}

void LedStripController::debugPrintParams() const {
  Serial.println(F("--- LedStripController params ---"));
  Serial.print(F("_isOn=")); Serial.println(_isOn);
  Serial.print(F("_splittingMode=")); Serial.println(_splittingMode);
  Serial.println(F("_effectParams:"));
  Serial.print(F("  effect=")); Serial.println(_effectParams.effect);
  Serial.print(F("  val=")); Serial.println(_effectParams.val);
  Serial.print(F("  speed=")); Serial.println(_effectParams.speed);
  Serial.print(F("  color=")); Serial.println(_effectParams.color);
  Serial.print(F("  temp=")); Serial.println(_effectParams.temp);
  Serial.print(F("  sat=")); Serial.println(_effectParams.sat);
  Serial.print(F("  hsvVal=")); Serial.println(_effectParams.hsvVal);
  Serial.print(F("  startFrom=")); Serial.println(_effectParams.startFrom);
  Serial.print(F("  end=")); Serial.println(_effectParams.end);
  Serial.println(F("_splittingParams:"));
  Serial.print(F("  numSplit=")); Serial.println(_splittingParams.numSplit);
  Serial.print(F("  zone1: effect=")); Serial.print(_splittingParams.effect1);
  Serial.print(F(" val=")); Serial.print(_splittingParams.val1);
  Serial.print(F(" speed=")); Serial.print(_splittingParams.speed1);
  Serial.print(F(" color=")); Serial.print(_splittingParams.color1);
  Serial.print(F(" temp=")); Serial.print(_splittingParams.temp1);
  Serial.print(F(" sat=")); Serial.print(_splittingParams.sat1);
  Serial.print(F(" hsvVal=")); Serial.println(_splittingParams.hsvVal1);
  Serial.print(F("  zone2: effect=")); Serial.print(_splittingParams.effect2);
  Serial.print(F(" val=")); Serial.print(_splittingParams.val2);
  Serial.print(F(" speed=")); Serial.print(_splittingParams.speed2);
  Serial.print(F(" color=")); Serial.print(_splittingParams.color2);
  Serial.print(F(" temp=")); Serial.print(_splittingParams.temp2);
  Serial.print(F(" sat=")); Serial.print(_splittingParams.sat2);
  Serial.print(F(" hsvVal=")); Serial.println(_splittingParams.hsvVal2);
  Serial.print(F("  zone3: effect=")); Serial.print(_splittingParams.effect3);
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
      s += F(" split=");
      s += _splittingParams.numSplit;
      s += F(" z1(e=");
      s += _splittingParams.effect1;
      s += F(",v=");
      s += _splittingParams.val1;
      s += F(") z2(e=");
      s += _splittingParams.effect2;
      s += F(",v=");
      s += _splittingParams.val2;
      s += F(") z3(e=");
      s += _splittingParams.effect3;
      s += F(",v=");
      s += _splittingParams.val3;
      s += F(")");
    } else {
      s += F(" effect=");
      s += _effectParams.effect;
      s += F(" val=");
      s += _effectParams.val;
      s += F(" speed=");
      s += _effectParams.speed;
      s += F(" color=");
      s += _effectParams.color;
      s += F(" sat=");
      s += _effectParams.sat;
      s += F(" hsvVal=");
      s += _effectParams.hsvVal;
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

//--------------------------Температура белого цвета---------------------------------
void LedStripController::effectTemperature(const EffectParams& p) {
  float kelvin = 2000.0 + (p.temp / 255.0) * 6000.0;
  float temp_kelvin = kelvin / 100.0;
  float red, green, blue;
  
  if (temp_kelvin <= 66) {
    red = 255;
  } else {
    red = temp_kelvin - 60;
    red = 329.698727446 * pow(red, -0.1332047592);
    if (red < 0) red = 0;
    if (red > 255) red = 255;
  }
  
  if (temp_kelvin <= 66) {
    green = temp_kelvin;
    green = 99.4708025861 * log(green) - 161.1195681661;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
  } else {
    green = temp_kelvin - 60;
    green = 288.1221695283 * pow(green, -0.0755148492);
    if (green < 0) green = 0;
    if (green > 255) green = 255;
  }
  
  if (temp_kelvin >= 66) {
    blue = 255;
  } else {
    if (temp_kelvin <= 19) blue = 0;
    else {
      blue = temp_kelvin - 10;
      blue = 138.5177312231 * log(blue) - 305.0447927307;
      if (blue < 0) blue = 0;
      if (blue > 255) blue = 255;
    }
  }
  
  CRGB color((uint8_t)red, (uint8_t)green, (uint8_t)blue);
  for (int i = p.startFrom; i < p.end; i++) {
    _leds[i] = color;
    _leds[i].nscale8(p.val);
  }
}

//---------------------------Эффект статического свечения (H, S, V из параметров)-----
void LedStripController::effectStatic(const EffectParams& p) {
  for (int i = p.startFrom; i < p.end; i++) {
    _leds[i] = CHSV(p.color, p.sat, p.hsvVal);
    _leds[i].nscale8(p.val);  // Общая яркость
  }
}

//-----------------------------Эффект бегущей радуги---------------------------------
void LedStripController::effectRainbow(const EffectParams& p) {
  const int waveSpeed = 15;
  unsigned long currentMillis = millis();
  const long interval = (p.end - p.startFrom) > 0 ? 1000 / (p.end - p.startFrom) : 50;

  if (currentMillis - _rainbowPreviousMillis >= interval) {
    for (int i = p.startFrom; i < p.end; i++) {
      int colorIndex = (i - p.startFrom) * 256 / (p.end - p.startFrom) + millis() * p.speed / waveSpeed;
      _leds[i] = CHSV(colorIndex, 255, 255);
      _leds[i].nscale8(p.val);
    }
    _rainbowPreviousMillis = currentMillis;
  }
}

//------------------------------Эффект пульсации---------------------------------
void LedStripController::effectPulse(const EffectParams& p) {
  static uint8_t minVal = 0;
  const int cycleTime = 3000;
  CRGB colors[] = { CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Indigo, CRGB::Violet };

  unsigned long currentMillis = millis() * p.speed;
  int brightness = map(constrain(currentMillis % cycleTime, 0, cycleTime), 0, cycleTime, minVal, p.val);
  uint8_t colorIndex = (currentMillis / cycleTime) % (sizeof(colors) / sizeof(colors[0]));
  CRGB color = colors[colorIndex];
  
  for (int i = p.startFrom; i < p.end; i++) {
    _leds[i] = color;
    _leds[i].fadeToBlackBy(255 - brightness);
  }
}

//---------------------------------Эффект мерцание-----------------------------------
void LedStripController::effectTwinkle(const EffectParams& p) {
  const int speedEffect = 10;
  unsigned long currentMillis = p.speed * millis();

  if (currentMillis - _twinklePreviousMillis >= speedEffect) {
    _twinklePreviousMillis = currentMillis;
    _twinkleHue++;
  }
  for (int i = p.startFrom; i < p.end; i++) {
    _leds[i] = CHSV(_twinkleHue, p.sat, p.val);
  }
}

//-------------------------------Эффект переливания фиолетового----------------------
void LedStripController::effectSmoothPurple(const EffectParams& p) {
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
  for (int i = p.startFrom; i < p.end; i++) {
    _leds[i] = color;
  }
}

//------------------------Переключение эффектов (реестр)-----------------------------
void LedStripController::switchEffect(const EffectParams& params) {
  if (params.effect < EFFECT_COUNT && _effectHandlers[params.effect]) {
    (this->*_effectHandlers[params.effect])(params);
  } else {
    for (int i = params.startFrom; i < params.end; i++) {
      _leds[i] = CRGB::Black;
    }
  }
}
