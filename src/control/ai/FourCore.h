#pragma once
#include <math.h>
#include <string.h>
#include <stdio.h>
namespace onboard {
// 与冻结 four_env.py 对齐；本核不读特权状态、不持有硬件。
struct FourCore {
    float obs[63] = {0}, anchor[2] = {0}, target[2] = {-.3297f, -.3297f},
          estimate[2] = {-.3297f, -.3297f};
    float desired[2] = {-.3297f, -.3297f}, reserved[2] = {0}, voltage[2] = {0}, previous[4] = {0};
    bool initialized = false;
    // 接管电压超过模型域时保留投影差，供交接诊断；不新增过渡。
    float seedProjection[2] = {0};
    static float clip(float x, float lo, float hi) {
        return x < lo ? lo : x > hi ? hi : x;
    }
    void reset(const float *q) {
        *this = FourCore{};
        anchor[0] = q[0];
        anchor[1] = q[1];
    }
    // 从最后实际施加的模型域电压播种，超过±7.4V时显式记录投影。
    bool seed(const float *q, const float *v, const float *u, const float *hip) {
        for (unsigned i = 0; i < 2; ++i)
            if (!isfinite(q[i]) || !isfinite(v[i]) || !isfinite(u[i]) || !isfinite(hip[i]))
                return false;
        reset(q);
        for (unsigned i = 0; i < 2; ++i) {
            target[i] = desired[i] = estimate[i] = hip[i];
            voltage[i] = clip(u[i], -7.4f, 7.4f);
            seedProjection[i] = voltage[i] - u[i];
            previous[i + 2] = voltage[i] / 7.4f;
            // 腿沿用原等效值，轮历史为已生效电压的归一化值。
            previous[i] = clip((hip[i] + .3297f) / .12f, -1, 1);
        }
        return true;
    }
    // 保留21维布局；17/18是恒零保留槽，15/16为已接纳轮动作，19/20为生效电压。
    // 按scale缩放、裁剪并维护三帧history，得到63维obs供下一节点推理；首次用当前帧填满历史。
    // Actor输入为21维单帧×3帧历史=63；这里无Critic，不能把训练侧65维评论器观测输入Actor。
    bool observe(float p, float r, const float *gyro, const float *q, const float *v) {
        const float scale[21] = {.2f, .2f, 5, 5, 5, 10, 10, 30, 30,   .5f, .5f,
                                 .5f, .2f, 1, 1, 1, 1,  30, 30, 7.4f, 7.4f};
        float f[21] = {p,
                       r,
                       gyro[0],
                       gyro[1],
                       gyro[2],
                       q[0] - anchor[0],
                       q[1] - anchor[1],
                       v[0],
                       v[1],
                       estimate[0],
                       estimate[1],
                       -.3297f,
                       0,
                       previous[0],
                       previous[1],
                       previous[2],
                       previous[3],
                       0,
                       0,
                       voltage[0],
                       voltage[1]};
        for (unsigned i = 0; i < 21; ++i) {
            if (!isfinite(f[i]))
                return false;
            f[i] = clip(f[i] / scale[i], -20, 20);
        }
        if (initialized) {
            memmove(obs, obs + 21, 42 * sizeof(float));
            memcpy(obs + 42, f, 21 * sizeof(float));
        } else {
            for (unsigned h = 0; h < 3; ++h)
                memcpy(obs + h * 21, f, 21 * sizeof(float));
            initialized = true;
        }
        return true;
    }
    // 动作四维依次为左腿、右腿、左轮、右轮；腿偏移0.12rad，轮为7.4V归一化电压。
    bool action(const float *a, const float *v) {
        for (unsigned i = 0; i < 4; ++i)
            if (!isfinite(a[i]))
                return false;
        for (unsigned i = 0; i < 2; ++i)
            if (!isfinite(v[i]))
                return false;
        for (unsigned i = 0; i < 4; ++i)
            previous[i] = clip(a[i], -1, 1);
        for (unsigned i = 0; i < 2; ++i) {
            desired[i] = clip(-.3297f + .12f * previous[i], -.5f, -.12f);
            voltage[i] = 7.4f * previous[i + 2];
        }
        return true;
    }
    // 每1ms更新腿限速目标及一阶位置估计；估计不是实测舵机角度。
    void substep(const float *, bool) {
        for (unsigned i = 0; i < 2; ++i) {
            target[i] += clip(desired[i] - target[i], -.003f, .003f);
            estimate[i] += (desired[i] - estimate[i]) * .025f;
        }
    }
    // 仅训练几何映射的逆；计数零位及真实带载关节位置尚未标定。
    float servo(unsigned i) const {
        return (i ? 1795.f : 2299.f) +
               (i ? 1.f : -1.f) * (target[i] + .3297f) * 4096.f / (2.f * 3.14159265358979323846f);
    }
};
} // namespace onboard
