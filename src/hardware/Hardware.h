#pragma once
#include "../control/Types.h"
namespace hardware {
bool begin();
control::Observation sample(bool ai);
void maintainBattery();
bool batteryOk();
void apply(control::Targets &t, bool ai, bool active, float *focQ);
float batteryPercent();
} // namespace hardware
