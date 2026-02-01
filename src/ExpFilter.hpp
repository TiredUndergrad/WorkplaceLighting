#ifndef EXP_FILTER_H
#define EXP_FILTER_H

/**
 * Экспоненциальное бегущее среднее (сглаживание).
 * Используется для фильтрации показаний датчика освещённости.
 * y_new = y_old + (x - y_old) * k
 */
class ExpFilter {
public:
  explicit ExpFilter(float k = 0.02f);

  void setK(float k);
  float getK() const { return _k; }
  float getValue() const { return _value; }

  /**
   * Добавить новое измерение и вернуть отфильтрованное значение.
   */
  float update(float newVal);
  void reset();
  void reset(float initialValue);

private:
  float _k;
  float _value;
};

#endif
