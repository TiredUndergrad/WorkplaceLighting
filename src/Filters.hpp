// Filters.hpp
#ifndef FILTERS_HPP
#define FILTERS_HPP

// Фильтр низких частот (простое экспоненциальное сглаживание)
class LowPassFilter {
public:
    LowPassFilter() : _prev(0), _k(0.1f) {}
    
    explicit LowPassFilter(float k) : _prev(0), _k(k) {}
    
    void setK(float k) { 
        _k = k; 
    }
    
    float update(float raw) {
        _prev = _k * raw + (1.0f - _k) * _prev;
        return _prev;
    }
    
    void reset() { 
        _prev = 0; 
    }
    
    void reset(float initialValue) {
        _prev = initialValue;
    }
    
    float getCurrent() const {
        return _prev;
    }
    
private:
    float _prev;
    float _k;
};

// Экспоненциальный фильтр (альтернативная реализация)
class ExpFilter {
public:
    explicit ExpFilter(float k = 0.1f) : _k(k), _value(0) {}
    
    void setK(float k) {
        _k = k;
    }
    
    float update(float newVal) {
        _value += (newVal - _value) * _k;
        return _value;
    }
    
    void reset() {
        _value = 0;
    }
    
    void reset(float initialValue) {
        _value = initialValue;
    }
    
    float getCurrent() const {
        return _value;
    }
    
private:
    float _k;
    float _value;
};

#endif // FILTERS_HPP