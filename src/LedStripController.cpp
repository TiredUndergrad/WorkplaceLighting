#include "LedStripController.hpp"
#include <math.h>

LedStripController::LedStripController(CRGB* leds, uint16_t ledCount, uint8_t dataPin)
  : _leds(leds), _ledCount(ledCount), _dataPin(dataPin), _splittingMode(false), _isOn(false),
    _rainbowPreviousMillis(0), _twinklePreviousMillis(0), _purplePreviousMillis(0),
    _twinkleHue(0), _purpleHue(160) {
  // Инициализация параметров эффекта по умолчанию
  _effectParams.effect = 0;
  _effectParams.val = 128;
  _effectParams.speed = 1;
  _effectParams.color = 0;
  _effectParams.temp = 1;
  _effectParams.startFrom = 0;
  _effectParams.end = ledCount;
  
  // Инициализация параметров разделения
  _splittingParams.numSplit = 0;
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
    // Режим разделения на зоны
    if (_splittingParams.numSplit == 2) {
      switchEffect(_splittingParams.effect1, _splittingParams.val1, _splittingParams.speed1,
                   _splittingParams.color1, _splittingParams.temp1, 0, _ledCount / 2);
      switchEffect(_splittingParams.effect2, _splittingParams.val2, _splittingParams.speed2,
                   _splittingParams.color2, _splittingParams.temp2, _ledCount / 2, _ledCount);
    } else if (_splittingParams.numSplit == 3) {
      switchEffect(_splittingParams.effect1, _splittingParams.val1, _splittingParams.speed1,
                   _splittingParams.color1, _splittingParams.temp1, 0, _ledCount / 3);
      switchEffect(_splittingParams.effect2, _splittingParams.val2, _splittingParams.speed2,
                   _splittingParams.color2, _splittingParams.temp2, _ledCount / 3, (_ledCount * 2) / 3);
      switchEffect(_splittingParams.effect3, _splittingParams.val3, _splittingParams.speed3,
                   _splittingParams.color3, _splittingParams.temp3, (_ledCount * 2) / 3, _ledCount);
    }
  } else {
    // Обычный режим - один эффект на всю ленту
    switchEffect(_effectParams.effect, _effectParams.val, _effectParams.speed,
                 _effectParams.color, _effectParams.temp, _effectParams.startFrom, _effectParams.end);
  }
  
  FastLED.show();
}

void LedStripController::setEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp) {
  _splittingMode = false;
  _effectParams.effect = effect;
  _effectParams.val = val;
  _effectParams.speed = speed;
  _effectParams.color = color;
  _effectParams.temp = temp;
  _effectParams.startFrom = 0;
  _effectParams.end = _ledCount;
}

void LedStripController::setZoneEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end) {
  _splittingMode = false;
  _effectParams.effect = effect;
  _effectParams.val = val;
  _effectParams.speed = speed;
  _effectParams.color = color;
  _effectParams.temp = temp;
  _effectParams.startFrom = startFrom;
  _effectParams.end = end;
}

void LedStripController::setSplitting(const SplittingParams& params) {
  _splittingMode = true;
  _splittingParams = params;
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

void LedStripController::turnOn() {
  _isOn = true;
}

void LedStripController::turnOff() {
  _isOn = false;
  FastLED.clear();
  FastLED.show();
}

//--------------------------Температура белого цвета---------------------------------
void LedStripController::temperature(uint8_t val, uint8_t temp, uint8_t startFrom, uint8_t end) {
  // Преобразуем temp (0-255) в цветовую температуру (2000-8000K)
  float kelvin = 2000.0 + (temp / 255.0) * 6000.0;
  
  // Алгоритм Таннера Хеллэнда для преобразования температуры в RGB
  // Источник: http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
  float temp_kelvin = kelvin / 100.0;
  float red, green, blue;
  
  // Красный компонент
  if (temp_kelvin <= 66) {
    red = 255;
  } else {
    red = temp_kelvin - 60;
    red = 329.698727446 * pow(red, -0.1332047592);
    if (red < 0) red = 0;
    if (red > 255) red = 255;
  }
  
  // Зеленый компонент
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
  
  // Синий компонент
  if (temp_kelvin >= 66) {
    blue = 255;
  } else {
    if (temp_kelvin <= 19) {
      blue = 0;
    } else {
      blue = temp_kelvin - 10;
      blue = 138.5177312231 * log(blue) - 305.0447927307;
      if (blue < 0) blue = 0;
      if (blue > 255) blue = 255;
    }
  }
  
  CRGB color((uint8_t)red, (uint8_t)green, (uint8_t)blue);
  
  for (int i = startFrom; i < end; i++) {
    _leds[i] = color;
    _leds[i].nscale8(val);  // Установка яркости
  }
}

//---------------------------Эффект статического свечения---------------------------
void LedStripController::statick(uint8_t val, uint8_t color, uint8_t startFrom, uint8_t end) {
  for (int i = startFrom; i < end; i++) {
    _leds[i] = CHSV(color, 255, 255);  // Изменение цвета
    _leds[i].maximizeBrightness(val);  // Яркость ленты
  }
}

//-----------------------------Эффект бегущей радуги---------------------------------
void LedStripController::rainbow(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end) {
  int waveSpeed = 15;  // Скорость движения волны (меньше значение - быстрее)
  unsigned long currentMillis = millis();  // Текущее время
  const long interval = 1000 / (end - startFrom);  // Интервал для обновления волны
  
  // Условие проверяющее, прошло ли достаточно времени с момента последнего обновления светодиодов
  if (currentMillis - _rainbowPreviousMillis >= interval) {
    for (int i = startFrom; i < end; i++) {
      // Вычисление индекса цвета для текущего светодиода
      int colorIndex = i * 256 / (end - startFrom) + millis() * speed / waveSpeed;
      // Установка цвета текущего светодиода
      _leds[i] = CHSV(colorIndex, 255, 255);
      _leds[i].nscale8(val);
    }
    _rainbowPreviousMillis = currentMillis;  // Обновляем время предыдущего срабатывания
  }
}

//------------------------------Эффект пульсации---------------------------------
void LedStripController::pulse(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end) {
  // Минимальная и максимальная яркость для эффекта "Пульсация"
  static uint8_t minVal = 0;
  uint8_t maxVal = val;

  // Время одного цикла пульсации (в миллисекундах)
  int cycleTime = 3000;

  // Массив цветов радуги (в порядке: красный, оранжевый, желтый, зеленый, голубой, синий, фиолетовый)
  CRGB colors[] = { CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Indigo, CRGB::Violet };

  // Вычисление текущей яркости в зависимости от времени
  unsigned long currentMillis = millis() * speed;
  int brightness = map(constrain(currentMillis % cycleTime, 0, cycleTime), 0, cycleTime, minVal, maxVal);

  // Определение текущего цвета в зависимости от времени
  uint8_t colorIndex = (currentMillis / cycleTime) % (sizeof(colors) / sizeof(colors[0]));
  CRGB color = colors[colorIndex];
  
  for (int i = startFrom; i < end; i++) {
    _leds[i] = color;                          // Установка текущего цвета
    _leds[i].fadeToBlackBy(255 - brightness);  // Изменение яркости
  }
}

//---------------------------------Эффект мерцание-----------------------------------
void LedStripController::twinkleEffect(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end) {
  static uint8_t saturation = 255;  // Насыщенность (максимальная насыщенность)
  int speedEffect = 10;              // Начальная скорость изменения цвета (в миллисекундах)

  unsigned long currentMillis = speed * millis();  // Текущее время в миллисекундах

  if (currentMillis - _twinklePreviousMillis >= speedEffect) {  // Если прошло достаточно времени с момента последнего обновления
    _twinklePreviousMillis = currentMillis;                     // Обновление времени последнего обновления
    _twinkleHue++;                                              // Увеличение оттенка цвета
  }
  
  for (int i = startFrom; i < end; i++) {
    _leds[i] = CHSV(_twinkleHue, saturation, val);  // Устанавливаем текущий цвет на все светодиоды
  }
}

//-------------------------------Эффект переливания фиолетового----------------------
void LedStripController::smoothPurpleEffect(uint8_t val, uint8_t speed, uint8_t startFrom, uint8_t end) {
  int cycleTime = 3000;  // Время одного цикла переливания (в миллисекундах)

  unsigned long currentMillis = millis() * speed;               // Текущее время в миллисекундах
  float position = fmod(currentMillis, cycleTime) / cycleTime;  // Вычисление позиции переливания (Лежит от 0. до 1.)

  CRGB colorStart = CHSV(140, 255, val);  // Фиолетовый цвет
  CRGB colorEnd = CHSV(200, 255, val);    // Бирюзовый цвет
  CRGB color;                             // Переменная с вычисленным цветом

  // Плавное переливание цвета между colorStart (фиолетовый цвет) и colorEnd (бирюзовый цвет) в зависимости от position
  if (position <= 0.5) {
    color = colorStart.nscale8(255 - position * 510) + colorEnd.nscale8(position * 510);
  } else {
    position = 1 - position;
    color = colorStart.nscale8(255 - position * 510) + colorEnd.nscale8(position * 510);
  }

  // Заполнение светодиодов вычисленными цветами
  for (int i = startFrom; i < end; i++) {
    _leds[i] = color;
  }
}

//------------------------Переключение эффектов--------------------------------------
void LedStripController::switchEffect(uint8_t effect, uint8_t val, uint8_t speed, uint8_t color, uint8_t temp, uint8_t startFrom, uint8_t end) {
  switch (effect) {
    case 0:
      statick(val, color, startFrom, end);  // Статический эффект
      break;
    case 1:
      temperature(val, temp, startFrom, end);  // Эффект "температура"
      break;
    case 2:
      rainbow(val, speed, startFrom, end);  // Эффект "волна"
      break;
    case 3:
      pulse(val, speed, startFrom, end);  // Эффект пульсации
      break;
    case 4:
      twinkleEffect(val, speed, startFrom, end);  // Эффект мерцание
      break;
    case 5:
      smoothPurpleEffect(val, speed, startFrom, end);  // Эффект переливания фиолетового
      break;
    default:
      // По умолчанию - выключено
      for (int i = startFrom; i < end; i++) {
        _leds[i] = CRGB::Black;
      }
      break;
  }
}
