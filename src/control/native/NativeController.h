#pragma once
#include "../Types.h"
#include "NativePid.h"
#include "NativeFilter.h"
namespace native {
// 只保留零摇杆、无跳跃的站立分支；所有PID/滤波器由此控制器独占。
class NativeController {
    NativePid pitch{1, 0, 0, 100000, 8}, gyro{.06, 0, 0, 100000, 8}, distance{.5, 0, 0, 100000, 8},
        speed{.7, 0, 0, 100000, 8};
    NativePid yawAngle{1, 0, 0, 100000, 8}, yawGyro{.04, 0, 0, 100000, 8},
        output{1, .5, 0, 100000, 8}, zero{.002, 0, 0, 100000, 4}, roll{8, 0, 0, 100000, 450};
    NativeFilter joy{.2}, zeroFilter{.1}, rollFilter{.3}, heightFilter{.1};
    int height = 30;
    float distanceZero = .5f, lastSpeed = 0, speedDiff = 0;
    uint32_t speedTime = 0;

  public:
    float terms[10] = {};
    float pitchZero = -1.6f;
    // 原生控制器独占PID/滤波状态；冷入清零，回切保留积分并重锚轮位。
    void enter(const control::Observation &o, bool resume) {
        if (!resume)
            *this = NativeController{};
        distanceZero = -.5f * (o.wheelAngle[0] + o.wheelAngle[1]);
        lastSpeed = -.5f * (o.wheelVelocity[0] + o.wheelVelocity[1]);
        speedDiff = 0;
        speedTime = millis();
        auto now = _micros();
        pitch.resumeAt(o.pitch - pitchZero, now);
        gyro.resumeAt(o.gyroY, now);
        distance.resumeAt(0, now);
        speed.resumeAt(lastSpeed, now);
        yawAngle.resumeAt(0, now);
        yawGyro.resumeAt(o.gyroZ, now);
        output.resumeAt(0, now);
        zero.resumeAt(0, now);
        roll.resumeAt(o.roll, now);
        joy.resumeAt(now);
        zeroFilter.resumeAt(now);
        rollFilter.resumeAt(now);
        if (resume)
            heightFilter.seedAt(height, now);
    }
    // 零摇杆站立计算；返回候选电压/舵机计数，不直接访问执行器。
    control::Targets step(const control::Observation &o) {
        control::Targets t;
        height = int(heightFilter(50));
        float d = -.5f * (o.wheelAngle[0] + o.wheelAngle[1]),
              v = -.5f * (o.wheelVelocity[0] + o.wheelVelocity[1]);
        terms[0] = pitchZero;
        terms[1] = pitch(o.pitch - pitchZero);
        terms[2] = gyro(o.gyroY);
        terms[3] = speed(v - .1f * joy(0));
        if (uint32_t(millis() - speedTime) >= 100) {
            speedDiff = v - lastSpeed;
            lastSpeed = v;
            speedTime = millis();
        }
        if (fabsf(speedDiff) > 10 || fabsf(v) > 15) {
            distanceZero = d;
            output.error_prev = 0;
        }
        terms[4] = distance(d - distanceZero);
        float u = terms[1] + terms[2] + terms[4] + terms[3];
        terms[5] = u;
        if (fabsf(u) < 5 && fabsf(terms[4]) < 4) {
            u = output(u);
            pitchZero -= zero(zeroFilter(terms[4]));
            pitchZero = constrain(pitchZero, -10.f, 10.f);
        } else
            output.error_prev = 0;
        // 本轮算完速度项后才更新下一轮P，保持原高度滤波与整数截断。
        speed.P = height < 50 ? .8f : (height < 64 ? .6f : .5f);
        float yaw = yawAngle(0) + yawGyro(o.gyroZ);
        t.wheel[0] = -.5f * (u + yaw);
        t.wheel[1] = -.5f * (u - yaw);
        float r = roll(rollFilter(o.roll));
        t.leg[0] = constrain(2047.f + 8.4f * (height - 20) - r, 2047.f, 2437.f);
        t.leg[1] = constrain(2047.f - 8.4f * (height - 20) - r, 1657.f, 2047.f);
        terms[6] = u;
        terms[7] = pitchZero;
        terms[8] = distanceZero;
        terms[9] = yaw;
        return t;
    }
};
} // namespace native
