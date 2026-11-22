#pragma once
#include <stdint.h>
namespace control {
enum class Mode : uint32_t { Stopped, Native, Ai, Fault, ServoOff };
enum class Reason : uint32_t {
    None,
    Boot,
    Operator,
    LinkLost,
    Nonfinite,
    Tilt,
    Battery,
    Deadline,
    Actor,
    NotStanding,
    UserNative,
    Shock
};
struct Observation {
    uint32_t nowUs = 0;
    float pitch = 0, roll = 0, gyroX = 0, gyroY = 0, gyroZ = 0, accZ = 0, battery = 0;
    float wheelAngle[2] = {}, wheelVelocity[2] = {};
};
struct Targets {
    float wheel[2] = {}, leg[2] = {2047, 2047};
    uint16_t speed = 400;
    uint8_t acceleration = 100;
    bool torque = true;
};
} // namespace control
