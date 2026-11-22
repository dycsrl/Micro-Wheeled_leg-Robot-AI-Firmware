#pragma once
#include "protocol/Frame.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace bridge {
struct Command {
    uint32_t code, receivedMs;
};
struct Status {
    wire::Frame frame;
    float percent;
};
bool init();
bool push(wire::Frame &f);
bool pop(wire::Frame &f);
void publish(const Status &s);
Status status();
// 事件仅保存边沿与请求结果，固定容量；不记录任何认证秘密。
struct Event {
    uint32_t id, atMs, sourceUs, kind, detail, mode, reason, cycleUs;
    float pitch, roll, accZ, battery;
};
void event(uint32_t kind, uint32_t detail = 0, const wire::Frame *frame = nullptr);
bool eventAfter(uint32_t after, Event &out);
uint32_t eventTotal();
Event firstFault();
bool command(uint32_t code);
bool take(Command &c);
bool takeStop(bool &off);
uint32_t heartbeat();
void beat();
uint32_t drops();
} // namespace bridge
