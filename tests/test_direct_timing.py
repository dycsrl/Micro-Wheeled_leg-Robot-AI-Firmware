"""真实AiController：仅替换Arduino时钟和actor，验证旧desired结清后才接纳。"""

from pathlib import Path
import subprocess, tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "src/control/ai/AiController.h").read_text(encoding="utf8")
src = "\n".join(s for s in src.splitlines() if not s.startswith("#include") and s != "#pragma once")
prefix = """
#include <cassert>
#include "src/control/ai/Action.h"
uint32_t now=0;uint32_t micros(){return now;}
namespace onboard {struct ActorWorkspace {};bool actor(const float*,float* a,ActorWorkspace&){now+=7000;a[0]=1;a[1]=-1;a[2]=.25f;a[3]=-.5f;return true;}}
"""
test = """
int main(){ai::AiController ctl;control::Observation o;control::Targets applied,out;
 applied.leg[0]=2299;applied.leg[1]=1795;applied.wheel[0]=-3.7f;applied.wheel[1]=3.7f;
 assert(ctl.enter(o,applied));assert(ctl.core.previous[2]==.5f);
 now=1000;o.nowUs=1000;assert(ctl.step(o,out));
 assert(fabs(ctl.core.target[0]+.3297f)<1e-6);assert(fabs(ctl.core.estimate[0]+.3297f)<1e-6);
 assert(fabs(out.wheel[0]+1.85f)<1e-6);assert(fabs(out.wheel[1]-3.7f)<1e-6);
 assert(ctl.core.obs[57]==.5f&&ctl.core.obs[61]==.5f);
 now=9000;o.nowUs=9000;assert(ctl.step(o,out));
 assert(fabs(ctl.core.target[0]-(-.3297f+8*.003f))<1e-6);
}
"""
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / "timing.cpp").write_text(prefix + src + test, encoding="utf8")
    subprocess.run(
        ["c++", "-std=c++11", "-I", str(root), str(p / "timing.cpp"), "-o", str(p / "timing")],
        check=True,
    )
    subprocess.run([str(p / "timing")], check=True)
print(
    "PASS: real AiController enter/7ms synchronous inference boundary; old desired settled; no added delay"
)
