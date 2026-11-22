#include "../hardware/Hardware.h"
#include "../control/native/NativeController.h"
#include "../control/ai/AiController.h"
#include "../control/Safety.h"
#include "../network/Network.h"
#include "../telemetry/Bridge.h"
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "identity.h"
using namespace control;
namespace {
Mode mode = Mode::Stopped;
Reason reason = Reason::Boot;
native::NativeController nativeControl;
ai::AiController aiControl;
Targets applied;
uint32_t session, seq = 0, previous = 0, nativeSince = 0;
bool hardwareReady = false, legArmed = false;
wire::Frame trigger{};
// 锁存停止状态及原因；人工停止和舵机失能分别维护腿使能意图。
void stop(Reason r, bool off = false) {
    Mode next = off ? Mode::ServoOff : (r == Reason::Operator ? Mode::Stopped : Mode::Fault);
    if (mode != next || reason != r) {
        trigger.mode[0] = uint32_t(next);
        trigger.reason[0] = uint32_t(r);
        bridge::event(8, 0, &trigger);
    }
    mode = next;
    reason = r;
    if (r == Reason::Operator)
        legArmed = !off;
}
// 控制故障交还原生；停止、硬故障和初始化失败绝不自动恢复动力。
// 仅在运行态且硬保护通过时交回原生；不会解除停止或硬故障锁。
bool returnNative(Reason r, const Observation &o) {
    if (!hardwareReady || (mode != Mode::Ai && mode != Mode::Native))
        return false;
    Reason hard = check(o, Targets{}, hardware::batteryOk(), 0);
    if (hard != Reason::None) {
        stop(hard);
        return false;
    }
    bool changed = mode == Mode::Ai;
    if (changed) {
        nativeControl.enter(o, true);
        mode = Mode::Native;
        nativeSince = millis();
    }
    if (changed || reason != r) {
        trigger.mode[0] = uint32_t(mode);
        trigger.reason[0] = uint32_t(r);
        bridge::event(8, 0, &trigger);
    }
    reason = r;
    return changed;
}
} // namespace
// 初始化跨核桥、硬件和网络；FOC对齐可能转动轮子，完成后动力仍失能。
void setup() {
    Serial.setTxBufferSize(1024);
    Serial.begin(115200);
    Serial.printf("[BOOT] firmware=%s reset=%d heap=%u\n", FIRMWARE_SHA, int(esp_reset_reason()),
                  ESP.getFreeHeap());
    Serial.println("[BOOT] bridge begin");
    // &rebot step 1：建立固定队列，初始化唯一硬件owner，上电禁止运行。
    applied.torque = false;
    session = esp_random();
    if (!bridge::init()) {
        Serial.println("[BOOT] FAIL bridge");
        for (;;)
            delay(1000);
    }
    Serial.println("[BOOT] bridge ready; hardware begin");
    hardwareReady = hardware::begin();
    if (!hardwareReady)
        stop(Reason::Nonfinite);
    Serial.printf("[BOOT] hardware ready=%d; network begin\n", hardwareReady);
    if (!startNetwork()) {
        Serial.println("[BOOT] FAIL network task");
        stop(Reason::LinkLost);
    }
    esp_task_wdt_init(3, true);
    esp_task_wdt_add(nullptr);
    Serial.printf("[BOOT] setup done mode=%u heap=%u\n", unsigned(mode), ESP.getFreeHeap());
}
// 唯一控制主干：采集、状态切换、控制、保护、执行、冻结遥测；网络不能阻塞本核。
void loop() {
    uint32_t begin = micros();
    if (mode != Mode::Ai) {
        uint32_t t = micros();
        vTaskDelay(1);
        if (uint32_t(micros() - t) < 1500)
            delayMicroseconds(1500 - (micros() - t));
    }
    // &rebot step 2：收会话命令及停止锁，停止永远高于起立。
    bool off = false, stopped = bridge::takeStop(off);
    bridge::Command cmd{};
    bool requested = false;
    if (!stopped)
        while (bridge::take(cmd))
            requested = true;
    if (stopped)
        stop(Reason::Operator, off);
    // &rebot step 3：IMU更新；AI单次刷新轮反馈，原生读取前轮缓存。
    auto o = hardware::sample(mode != Mode::Native || (!stopped && requested && cmd.code == 2));
    // 非有限传感器先于控制器：避免NaN污染原生PID和滤波状态。
    // 捕获本轮原始观测，保护触发处立即锁存，不等后续reason覆盖。
    trigger.time_us[0] = o.nowUs;
    trigger.cycle_us[0] = previous ? o.nowUs - previous : 0;
    trigger.sensors[0] = o.pitch;
    trigger.sensors[1] = o.roll;
    trigger.sensors[5] = o.accZ;
    trigger.sensors[6] = o.battery;
    Reason inputFault = check(o, Targets{}, hardware::batteryOk(), micros() - o.nowUs);
    if ((mode == Mode::Ai || mode == Mode::Native) && inputFault != Reason::None) {
        if (inputFault == Reason::Deadline)
            returnNative(inputFault, o);
        else
            stop(inputFault);
    }
    uint32_t cycle = previous ? o.nowUs - previous : 0;
    previous = o.nowUs;
    bool live = uint32_t(millis() - bridge::heartbeat()) < 1500;
    if ((mode == Mode::Ai || mode == Mode::Native) && !live)
        returnNative(Reason::LinkLost, o);
    // &rebot step 4：仅显式切换；AI要求原生稳定运行至少500ms且姿态通过。
    if (requested && !stopped && live && hardwareReady && inputFault == Reason::None &&
        uint32_t(millis() - cmd.receivedMs) < 500) {
        if (cmd.code == 1 && mode != Mode::Ai) {
            nativeControl.enter(o, false);
            mode = Mode::Native;
            reason = Reason::None;
            nativeSince = millis();
        } else if (cmd.code == 3 && mode == Mode::Ai) {
            nativeControl.enter(o, true);
            mode = Mode::Native;
            reason = Reason::UserNative;
            nativeSince = millis();
        } else if (cmd.code == 2) {
            if (mode == Mode::Native && uint32_t(millis() - nativeSince) >= 500 &&
                fabsf(o.pitch - nativeControl.pitchZero) < 20 && aiControl.enter(o, applied)) {
                mode = Mode::Ai;
                reason = Reason::None;
            } else
                reason = Reason::NotStanding;
        }
    }
    if (mode == Mode::Native || mode == Mode::Ai)
        legArmed = true;
    Targets candidate;
    candidate.torque = legArmed;
    // &rebot step 5：各控制器独占状态，观测到候选目标，AI同步真实Actor。
    bool aiFrame = mode == Mode::Ai, nativeFrame = mode == Mode::Native;
    if (aiFrame) {
        if (!aiControl.step(o, candidate) && returnNative(Reason::Actor, o)) {
            candidate = nativeControl.step(o);
            nativeFrame = true;
        }
    } else if (nativeFrame)
        candidate = nativeControl.step(o);
    uint32_t candidateUs = micros();
    // &rebot step 6：控制超时交回原生；同轮重算候选，硬保护与迟到停止优先。
    if (mode == Mode::Ai || mode == Mode::Native) {
        Reason r = check(o, candidate, hardware::batteryOk(), micros() - o.nowUs);
        if (r != Reason::None && r != Reason::Deadline)
            stop(r);
        else if (r == Reason::Deadline || cycle > 100000) {
            if (returnNative(Reason::Deadline, o)) {
                candidate = nativeControl.step(o);
                nativeFrame = true;
            }
        }
    }
    if (bridge::takeStop(off))
        stop(Reason::Operator, off);
    if ((mode == Mode::Ai || mode == Mode::Native) &&
        uint32_t(millis() - bridge::heartbeat()) >= 1500) {
        if (returnNative(Reason::LinkLost, o)) {
            candidate = nativeControl.step(o);
            nativeFrame = true;
        }
    }
    // 回切后的原生候选也须通过全部硬保护，不复用失败AI或旧候选。
    if (mode == Mode::Ai || mode == Mode::Native) {
        Reason r = check(o, candidate, hardware::batteryOk(), 0);
        if (r != Reason::None)
            stop(r);
    }
    if (bridge::takeStop(off))
        stop(Reason::Operator, off);
    Targets target = clamp(candidate);
    bool active = mode == Mode::Ai || mode == Mode::Native;
    if (!active) {
        target = Targets{};
        target.torque = legArmed;
    }
    // &rebot step 7：唯一执行出口；原生FOC在move前，AI准备q后本轮FOC。
    uint32_t outputStart = micros();
    float foc[2];
    hardware::apply(target, mode == Mode::Ai, active, foc);
    applied = target;
    uint32_t outputEnd = micros();
    // &rebot step 8：冻结本轮事实；命令不是实测，网络只消费固定副本。
    wire::Frame f{};
    f.magic[0] = 0x32544252;
    f.version[0] = 1;
    f.bytes[0] = sizeof(f);
    f.session[0] = session;
    f.seq[0] = ++seq;
    f.time_us[0] = o.nowUs;
    f.mode[0] = uint32_t(mode);
    f.reason[0] = uint32_t(reason);
    f.cycle_us[0] = cycle;
    f.control_us[0] = micros() - begin;
    f.valid[0] = (nativeFrame ? 1 : 0) | (aiFrame ? 2 : 0);
    float sensors[] = {o.pitch,           o.roll,
                       o.gyroX,           o.gyroY,
                       o.gyroZ,           o.accZ,
                       o.battery,         o.wheelAngle[0],
                       o.wheelAngle[1],   o.wheelVelocity[0],
                       o.wheelVelocity[1]};
    memcpy(f.sensors, sensors, sizeof sensors);
    // 旧布局兼容：wheel_target/p_error保留槽恒零，p_output是模型域电压命令，不是P误差。
    if (aiFrame) {
        memcpy(f.input, aiControl.core.obs, sizeof f.input);
        memcpy(f.action, aiControl.raw, sizeof f.action);
        memcpy(f.clipped_action, aiControl.core.previous, sizeof f.clipped_action);
        f.inference_us[0] = aiControl.inferenceUs;
        for (unsigned i = 0; i < 2; i++) {
            f.hip_target[i] = aiControl.core.target[i];
            f.wheel_target[i] = 0;
            f.p_error[i] = 0;
            f.p_output[i] = aiControl.core.voltage[i];
        }
    }
    if (nativeFrame)
        memcpy(f.native, nativeControl.terms, sizeof f.native);
    for (unsigned i = 0; i < 2; i++) {
        f.candidate[i] = candidate.wheel[i];
        f.candidate[i + 2] = candidate.leg[i];
        f.applied[i] = applied.wheel[i];
        f.applied[i + 2] = applied.leg[i];
        f.foc_q[i] = foc[i];
    }
    f.phase_us[0] = begin;
    f.phase_us[1] = o.nowUs;
    f.phase_us[2] = candidateUs;
    f.phase_us[3] = outputStart;
    f.phase_us[4] = outputEnd;
    f.phase_us[5] = micros();
    bridge::push(f);
    bridge::publish({f, hardware::batteryPercent()});
    // &rebot step 9：分摊电池维护、喂狗；不等待WiFi，不运行串口控制。
    hardware::maintainBattery();
    esp_task_wdt_reset();
}
