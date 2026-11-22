#pragma once
#include "FourCore.h"
#include "Observation.h"
namespace ai {
inline control::Targets action(onboard::FourCore &core,
                               const onboard::FourHardwareAdapter::View &) {
    control::Targets t;
    for (unsigned i = 0; i < 2; i++) {
        // 直接消费已接纳电压，不再按轮速重算P或滤波。
        t.wheel[i] = onboard::FourHardwareAdapter::voltageToHardware(i, core.voltage[i]);
        t.leg[i] = onboard::FourHardwareAdapter::servo(i, core.target[i]);
    }
    return t;
}
} // namespace ai
