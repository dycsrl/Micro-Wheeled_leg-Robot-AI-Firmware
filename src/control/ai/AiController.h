#pragma once
#include "Actor.h"
#include "Action.h"
#include <Arduino.h>
namespace ai {
class AiController {
    onboard::ActorWorkspace workspace;
    uint32_t estimateTime = 0;
    // 以1ms离散步推进旧腿目标/估计；超过100ms拒绝，避免无界追赶。
    bool advance(uint32_t now, const float *v) {
        // 保留1ms离散推进，超长间隔明确失败而不进行无界追赶。
        if (uint32_t(now - estimateTime) > 100000)
            return false;
        while (uint32_t(now - estimateTime) >= 1000) {
            core.substep(v, false);
            estimateTime += 1000;
        }
        return true;
    }

  public:
    onboard::FourCore core;
    float raw[4] = {};
    uint32_t inferenceUs = 0;
    // 从最后已施加输出播种AI私有状态；返回false时不得接管。
    bool enter(const control::Observation &o, const control::Targets &applied) {
        auto v = onboard::FourHardwareAdapter::observation(o);
        float hip[2], u[2];
        for (unsigned i = 0; i < 2; i++) {
            hip[i] = onboard::FourHardwareAdapter::hip(i, applied.leg[i]);
            u[i] = -applied.wheel[i];
        }
        estimateTime = o.nowUs;
        return core.seed(v.q, v.v, u, hip);
    }
    // 输入本轮观测，输出候选硬件目标；同步Actor推理前后推进旧动作再接纳新动作。
    bool step(const control::Observation &o, control::Targets &out) {
        auto v = onboard::FourHardwareAdapter::observation(o);
        if (!advance(o.nowUs, v.v) || !core.observe(v.pitch, v.roll, v.gyro, v.q, v.v))
            return false;
        uint32_t start = micros();
        bool ok = onboard::actor(core.obs, raw, workspace);
        inferenceUs = micros() - start;
        if (!ok || !advance(micros(), v.v) || !core.action(raw, v.v))
            return false;
        out = action(core, v);
        return true;
    }
};
} // namespace ai
