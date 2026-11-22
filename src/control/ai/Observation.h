#pragma once
#include "../Types.h"
namespace onboard {
// 真实硬件适配边界；不改原始观测、遥测或保护坐标。轴向尚未闭合，禁止凭闭环相关性翻号。
struct FourHardwareAdapter {
    struct View {
        float pitch, roll, gyro[3], q[2], v[2];
    };
    static View observation(const control::Observation &o) {
        constexpr float rad = 3.14159265358979323846f / 180.f;
        View x{o.pitch * rad,
               o.roll * rad,
               {o.gyroX * rad, o.gyroY * rad, o.gyroZ * rad},
               {-o.wheelAngle[0], -o.wheelAngle[1]},
               {-o.wheelVelocity[0], -o.wheelVelocity[1]}};
        // 没有独立安装偏置标定，不扣当前倾角或动态原生zero来伪造物理零点。
        return x;
    }
    // 冻结geometry.servo_to_hip的双向映射，仅目标估计，不是带载关节角。
    static float hip(unsigned i, float count) {
        return -.3297f + (i ? 1.f : -1.f) * (count - (i ? 1795.f : 2299.f)) * 2.f *
                             3.14159265358979323846f / 4096.f;
    }
    static float servo(unsigned i, float hip) {
        return (i ? 1795.f : 2299.f) +
               (i ? 1.f : -1.f) * (hip + .3297f) * 4096.f / (2.f * 3.14159265358979323846f);
    }
    // 原版明确正转矩向后；训练双轮正轴绕机身正Y、滚动向前。轮角、轮速及电压同时反号。
    // 原版右编码器为I2Ctwo/sensor2，故motor1为左、motor2为右；只改MPU总线不交换轮通道。
    static float voltageToPolicy(unsigned, float volts) {
        return -volts;
    }
    static float voltageToHardware(unsigned, float volts) {
        return -volts;
    }
};
} // namespace onboard
