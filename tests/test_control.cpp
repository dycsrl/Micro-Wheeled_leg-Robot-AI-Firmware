#include <cassert>
#include <cmath>
#include <fstream>
#include "../src/control/ai/FourCore.h"
#include "../src/control/ai/Observation.h"
#include "../src/control/Safety.h"
#include "../src/control/ai/Action.h"
#include "../protocol/Frame.h"
int main() {
    control::Observation o;
    o.battery = 7.5;
    o.accZ = 1;
    o.pitch = 180;
    o.wheelVelocity[0] = 3;
    auto v = onboard::FourHardwareAdapter::observation(o);
    assert(fabs(v.pitch - 3.14159265f) < 1e-6);
    assert(v.v[0] == -3);
    o.pitch = 0;
    control::Targets t;
    assert(control::check(o, t, true, 1000) == control::Reason::None);
    assert(control::check(o, t, false, 1000) == control::Reason::Battery);
    assert(control::check(o, t, true, 100001) == control::Reason::Deadline);
    o.roll = 36;
    assert(control::check(o, t, true, 1000) == control::Reason::Tilt);
    o.roll = 0;
    t.wheel[0] = NAN;
    assert(control::check(o, t, true, 1000) == control::Reason::Nonfinite);
    t.wheel[0] = 100;
    t = control::clamp(t);
    assert(t.wheel[0] == 8);
    onboard::FourCore core;
    float q[2] = {0, 0}, vel[2] = {0, 0}, u[2] = {0, 0}, hip[2] = {-.3297f, -.3297f},
          g[3] = {0, 0, 0};
    assert(core.seed(q, vel, u, hip));
    assert(core.observe(0, 0, g, q, vel));
    for (int i = 0; i < 21; i++)
        assert(core.obs[i] == core.obs[i + 21] && core.obs[i] == core.obs[i + 42]);
    // 只测真实控制核，不运行旧速度模型冒充直压候选。
    u[0] = 8;
    u[1] = -3.7f;
    assert(core.seed(q, vel, u, hip));
    assert(fabs(core.seedProjection[0] + .6f) < 1e-6f);
    assert(core.seedProjection[1] == 0);
    assert(core.previous[2] == 1 && core.previous[3] == -.5f);
    assert(core.observe(0, 0, g, q, vel));
    float old[63];
    memcpy(old, core.obs, sizeof old);
    float a[4] = {1, -1, 2, -2};
    assert(core.action(a, vel));
    assert(core.voltage[0] == 7.4f && core.voltage[1] == -7.4f);
    float target = core.target[0], est = core.estimate[0];
    vel[0] = 1000;
    vel[1] = -1000;
    core.substep(vel, true);
    assert(fabs(core.target[0] - target - .003f) < 1e-6);
    assert(fabs(core.estimate[0] - (est + (core.desired[0] - est) * .025f)) < 1e-6);
    assert(core.voltage[0] == 7.4f && core.voltage[1] == -7.4f);
    auto command = ai::action(core, v);
    assert(command.wheel[0] == -7.4f && command.wheel[1] == 7.4f);
    assert(core.observe(0, 0, g, q, vel));
    assert(memcmp(core.obs, old + 21, 42 * sizeof(float)) == 0);
    for (int h = 0; h < 3; h++)
        assert(core.obs[h * 21 + 17] == 0 && core.obs[h * 21 + 18] == 0);
    assert(core.obs[57] == 1 && core.obs[58] == -1 && core.obs[61] == 1 && core.obs[62] == -1);
    a[0] = NAN;
    assert(!core.action(a, vel));
    assert(core.previous[2] == 1);
    a[0] = 0;
    a[2] = 0;
    a[3] = .25f;
    assert(core.action(a, vel));
    assert(core.voltage[0] == 0 && core.voltage[1] == 1.85f);

    wire::Frame frame{};
    frame.magic[0] = 0x32544252;
    frame.version[0] = 1;
    frame.bytes[0] = sizeof(frame);
    frame.input[62] = 62;
    std::ofstream out("cpp-frame.bin", std::ios::binary);
    out.write((char *)&frame, sizeof(frame));
}
