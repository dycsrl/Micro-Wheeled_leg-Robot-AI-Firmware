#pragma once
#include <SimpleFOC.h>
// 保持原库调用；只为本帧撤销计算同一时间、同一前态的备选，不再推进共享PID。
class NativePid : public PIDController {
  public:
    using PIDController::PIDController;
    using PIDController::operator=;
    // 正常交还只刷新真实采样时钟和误差，保留输出及积分，不积算冻结期间。
    void resumeAt(float error, unsigned long now) {
        error_prev = error;
        timestamp_prev = now;
    }
    float integralValue() const {
        return integral_prev;
    }
    unsigned long sampleTime() const {
        return timestamp_prev;
    }
    float previewAt(float error, unsigned long now) {
        // 与本工程 SimpleFOC 的 pid.cpp 顺序相同，包含积分/输出限幅和斜率限制。
        float Ts = (now - timestamp_prev) * 1e-6f;
        if (Ts <= 0 || Ts > 0.5f)
            Ts = 1e-3f;
        float proportional = P * error;
        float integral = integral_prev + I * Ts * 0.5f * (error + error_prev);
        integral = _constrain(integral, -limit, limit);
        float derivative = D * (error - error_prev) / Ts;
        float output = proportional + integral + derivative;
        output = _constrain(output, -limit, limit);
        if (output_ramp > 0) {
            float rate = (output - output_prev) / Ts;
            if (rate > output_ramp)
                output = output_prev + output_ramp * Ts;
            else if (rate < -output_ramp)
                output = output_prev - output_ramp * Ts;
        }
        integral_prev = integral;
        output_prev = output;
        error_prev = error;
        timestamp_prev = now;
        return output;
    }
};
