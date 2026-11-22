#pragma once
#include <SimpleFOC.h>
// 不改变原库运算；冷启动仍由原构造器从零开始。
class NativeFilter : public LowPassFilter {
  public:
    using LowPassFilter::LowPassFilter;
    using LowPassFilter::operator=;
    void resumeAt(unsigned long now) {
        timestamp_prev = now;
    }
    void seedAt(float value, unsigned long now) {
        y_prev = value;
        timestamp_prev = now;
    }
};
