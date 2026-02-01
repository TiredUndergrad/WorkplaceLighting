#include "ExpFilter.hpp"

ExpFilter::ExpFilter(float k) : _k(k), _value(0) {}

void ExpFilter::setK(float k) {
  _k = k;
}

float ExpFilter::update(float newVal) {
  _value += (newVal - _value) * _k;
  return _value;
}

void ExpFilter::reset() {
  _value = 0;
}

void ExpFilter::reset(float initialValue) {
  _value = initialValue;
}
