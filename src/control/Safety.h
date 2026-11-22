#pragma once
#include "Types.h"
#include <math.h>
namespace control {
// 纯判定：非有限、冲击、低电、倾角、100ms期限按原顺序裁决；不改控制器状态。
inline Reason check(const Observation &o, const Targets &t, bool batteryOk, uint32_t elapsed) {
    float a[] = {o.pitch,
                 o.roll,
                 o.gyroX,
                 o.gyroY,
                 o.gyroZ,
                 o.accZ,
                 o.battery,
                 o.wheelAngle[0],
                 o.wheelAngle[1],
                 o.wheelVelocity[0],
                 o.wheelVelocity[1],
                 t.wheel[0],
                 t.wheel[1],
                 t.leg[0],
                 t.leg[1]};
    for (float x : a)
        if (!isfinite(x))
            return Reason::Nonfinite;
    if (o.accZ > 1.8f)
        return Reason::Shock;
    if (!batteryOk)
        return Reason::Battery;
    if (fabsf(o.pitch) > 25 || fabsf(o.roll) > 35)
        return Reason::Tilt;
    if (elapsed > 100000)
        return Reason::Deadline;
    return Reason::None;
}
inline float bound(float x, float lo, float hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
// 最终硬件域限幅：轮电压±8V，双腿计数各自取整限位。
inline Targets clamp(Targets t) {
    t.wheel[0] = bound(t.wheel[0], -8, 8);
    t.wheel[1] = bound(t.wheel[1], -8, 8);
    t.leg[0] = bound(roundf(t.leg[0]), 2047, 2437);
    t.leg[1] = bound(roundf(t.leg[1]), 1657, 2047);
    return t;
}
} // namespace control
