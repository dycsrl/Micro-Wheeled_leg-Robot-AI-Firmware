"""运行真实主循环正文，桩替代硬件/RTOS/网络；不是实机测试。"""

from pathlib import Path
import tempfile, subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "src/app/main.cpp").read_text(encoding="utf8")
source = "\n".join(x for x in source.splitlines() if not x.startswith("#include"))
# setup依赖真实板端初始化；宿主只执行既有loop及停止入口。
import re

start = re.search(r"void\s+setup\s*\(\s*\)\s*\{", source).start()
end = re.search(r"void\s+loop\s*\(\s*\)\s*\{", source).start()
source = source[:start] + source[end:]
prefix = r"""
#include <cmath>
#include <cstring>
#include <cassert>
#include "src/control/Safety.h"
#include "protocol/Frame.h"
uint32_t clockUs=2000000;uint32_t micros(){return clockUs++;}uint32_t millis(){return clockUs/1000;}
void vTaskDelay(int){clockUs+=2000;}void delay(int){}void delayMicroseconds(int n){clockUs+=n;}
int esp_random(){return 1;}void esp_task_wdt_init(int,bool){}void esp_task_wdt_add(void*){}void esp_task_wdt_reset(){}
namespace native {struct NativeController {float pitchZero=-1.6,terms[10]={};static int calls;void enter(const control::Observation&,bool){}control::Targets step(const control::Observation&){++calls;return {};}};int NativeController::calls=0;}
namespace ai {struct AiController {struct Core {float obs[63]={},previous[4]={},target[2]={},wtarget[2]={},voltage[2]={};}core;float raw[4]={};int inferenceUs=0;bool enter(const control::Observation&,const control::Targets&){return true;}bool step(const control::Observation&,control::Targets&){return true;}};}
namespace hardware {control::Observation obs;control::Targets output;bool refresh=false;bool batteryOk(){return true;}bool begin(){return true;}control::Observation sample(bool b){refresh=b;obs.nowUs=micros();return obs;}void apply(control::Targets&t,bool,bool,float*f){output=t;f[0]=f[1]=0;}float batteryPercent(){return 80;}void maintainBattery(){}}
namespace bridge {struct Command {uint32_t code,receivedMs;};struct Status {wire::Frame frame;float percent;};bool pending=false,stopping=false,disable=false;int code=0;bool init(){return true;}bool takeStop(bool&o){o=disable;bool s=stopping;stopping=false;return s;}bool take(Command&c){if(!pending)return false;pending=false;c={uint32_t(code),millis()};return true;}uint32_t heartbeat(){return millis();}void event(int,int,wire::Frame*){}void push(wire::Frame&){}void publish(const Status&){} }
bool startNetwork(){return true;}
"""
tests = r"""
void command(int c){bridge::pending=true;bridge::code=c;loop();}
int main(){
 hardware::obs.battery=7.5;hardware::obs.accZ=1;hardwareReady=true;
 command(2);assert(mode==Mode::Stopped&&!hardware::output.torque); // 上电非法AI不可使能舵机
 command(1);assert(mode==Mode::Native&&hardware::output.torque&&hardware::refresh);
 hardware::obs.roll=NAN;int n=native::NativeController::calls;loop();assert(mode==Mode::Fault&&native::NativeController::calls==n);
 command(1);assert(mode==Mode::Fault&&native::NativeController::calls==n); // 坏传感器不能重新进入
 hardware::obs.roll=0;command(1);assert(mode==Mode::Native);
 bridge::stopping=true;bridge::disable=true;loop();assert(mode==Mode::ServoOff&&!hardware::output.torque);
 command(2);assert(mode==Mode::ServoOff&&!hardware::output.torque);
 command(1);assert(mode==Mode::Native&&hardware::refresh);
}
"""
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / "main.cpp").write_text(prefix + source + tests, encoding="utf8")
    subprocess.run(
        [
            "c++",
            "-std=c++11",
            "-Wall",
            "-Wextra",
            "-I",
            str(root),
            str(p / "main.cpp"),
            "-o",
            str(p / "test"),
        ],
        check=True,
    )
    subprocess.run([str(p / "test")], check=True)
print(
    "PASS: real main loop with hardware stubs; boot AI rejection, fresh restart, nonfinite pre-gate, fault recovery, servo-off latch"
)
