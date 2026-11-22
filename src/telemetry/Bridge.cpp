#include "Bridge.h"
#include <Arduino.h>
namespace bridge {
// FreeRTOS静态队列提供跨核同步；控制核零等待，无网络对象。
static StaticQueue_t qcb, ccb;
static uint8_t storage[8 * sizeof(wire::Frame)], commands[8 * sizeof(Command)];
static QueueHandle_t q, cq;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static Status latest{};
static uint32_t lost = 0, hb = 0;
static bool stop = false, off = false;
// 建立固定容量8帧遥测与8条命令队列；无动态扩容。
bool init() {
    q = xQueueCreateStatic(8, sizeof(wire::Frame), storage, &qcb);
    cq = xQueueCreateStatic(8, sizeof(Command), commands, &ccb);
    return q && cq;
}
// 控制端零等待入队；满队列只增加丢帧计数，日志不可阻塞控制。
bool push(wire::Frame &f) {
    f.drops[0] = drops();
    if (xQueueSend(q, &f, 0) == pdTRUE)
        return true;
    portENTER_CRITICAL(&lock);
    ++lost;
    portEXIT_CRITICAL(&lock);
    return false;
}
bool pop(wire::Frame &f) {
    return xQueueReceive(q, &f, 0) == pdTRUE;
}
void publish(const Status &s) {
    portENTER_CRITICAL(&lock);
    latest = s;
    portEXIT_CRITICAL(&lock);
}
Status status() {
    portENTER_CRITICAL(&lock);
    auto s = latest;
    portEXIT_CRITICAL(&lock);
    return s;
}
// 停止/失能走高优先锁；普通命令满队列时锁存停止。
bool command(uint32_t code) {
    if (code == 0 || code == 4) {
        portENTER_CRITICAL(&lock);
        stop = true;
        off |= code == 4;
        portEXIT_CRITICAL(&lock);
        return true;
    }
    Command c{code, millis()};
    if (xQueueSend(cq, &c, 0) == pdTRUE)
        return true;
    command(0);
    return false;
}
static Event events[32]{}, first{};
static uint32_t total = 0;
Event firstFault() {
    portENTER_CRITICAL(&lock);
    auto e = first;
    portEXIT_CRITICAL(&lock);
    return e;
}
// 在短临界区冻结固定标量事件，最多保留32条；首个硬故障另存。
void event(uint32_t kind, uint32_t detail, const wire::Frame *frame) {
    Event e{};
    e.atMs = millis();
    e.kind = kind;
    e.detail = detail;
    // 只在临界区复制固定标量，不分配、不格式化；源时间与记录时间分列。
    portENTER_CRITICAL(&lock);
    const auto &f = frame ? *frame : latest.frame;
    e.sourceUs = f.time_us[0];
    e.mode = f.mode[0];
    e.reason = f.reason[0];
    e.cycleUs = f.cycle_us[0];
    e.pitch = f.sensors[0];
    e.roll = f.sensors[1];
    e.accZ = f.sensors[5];
    e.battery = f.sensors[6];
    e.id = ++total;
    events[(e.id - 1) % 32] = e;
    if (kind == 8 && e.mode == 3 && !first.id)
        first = e;
    portEXIT_CRITICAL(&lock);
}
uint32_t eventTotal() {
    portENTER_CRITICAL(&lock);
    auto n = total;
    portEXIT_CRITICAL(&lock);
    return n;
}
bool eventAfter(uint32_t after, Event &out) {
    portENTER_CRITICAL(&lock);
    uint32_t first = total > 32 ? total - 31 : 1;
    uint32_t next = after + 1;
    if (next < first)
        next = first;
    bool ok = next <= total;
    if (ok)
        out = events[(next - 1) % 32];
    portEXIT_CRITICAL(&lock);
    return ok;
}
bool take(Command &c) {
    return xQueueReceive(cq, &c, 0) == pdTRUE;
}
// 读取并清除停止锁，同时丢弃排队旧命令，避免停止后执行过期启动。
bool takeStop(bool &o) {
    portENTER_CRITICAL(&lock);
    bool s = stop;
    o = off;
    stop = off = false;
    portEXIT_CRITICAL(&lock);
    if (s)
        xQueueReset(cq);
    return s;
}
void beat() {
    portENTER_CRITICAL(&lock);
    hb = millis();
    portEXIT_CRITICAL(&lock);
}
uint32_t heartbeat() {
    portENTER_CRITICAL(&lock);
    auto v = hb;
    portEXIT_CRITICAL(&lock);
    return v;
}
uint32_t drops() {
    portENTER_CRITICAL(&lock);
    auto v = lost;
    portEXIT_CRITICAL(&lock);
    return v;
}
} // namespace bridge
