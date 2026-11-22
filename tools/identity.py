from pathlib import Path
import hashlib, json, os, subprocess, sys

Import("env")
from SCons.Script import COMMAND_LINE_TARGETS

p = Path(env.subst("$PROJECT_DIR")).resolve()
sys.path.insert(0, str(p / "deployment"))
from direct_contract import validate_identity

meta = json.loads((p / "model/active-model.json").read_text(encoding="utf-8"))
validate_identity(meta, p)
a = hashlib.sha256((p / "model/Actor.h").read_bytes()).hexdigest()
out = Path(env.subst("$BUILD_DIR")) / "generated"
out.mkdir(parents=True, exist_ok=True)
if os.environ.get("MUSHIBOT_BUILD_ONLY") == "1":
    if any(t in COMMAND_LINE_TARGETS for t in ("upload", "uploadfs", "program")):
        raise RuntimeError("BUILD-ONLY configuration cannot be uploaded")
    # 测试配置不是真实网络；仅供编译，不得烧录这个二进制。
    (out / "secrets.h").write_text(
        '#pragma once\n#define STA_SSID ""\n#define STA_PASS ""\n#define AP_SSID "BUILD-ONLY-DO-NOT-FLASH"\n#define AP_PSW "build-only-not-a-real-secret"\n',
        encoding="utf8",
    )
else:
    if not (p / "include/secrets.h").is_file():
        raise RuntimeError(
            "Missing include/secrets.h: copy secrets.example.h and fill locally; for compilation only use tools/build.py --build-only"
        )
    # 不读取配置值；保留原有本地网络配置，不额外限制AP密码长度。
    (out / "secrets.h").unlink(missing_ok=True)
h = hashlib.sha256()
for folder in ["src", "protocol", "model"]:
    for f in sorted((p / folder).rglob("*")):
        if f.is_file():
            h.update(f.relative_to(p).as_posix().encode())
            h.update(f.read_bytes())
b = h.hexdigest()
identity = {
    k: meta.get(k)
    for k in [
        "generation",
        "seed",
        "step",
        "new_step",
        "source_step",
        "iteration",
        "checkpoint_sha256",
    ]
}


def literal(x):
    return json.dumps(x, ensure_ascii=True)


header = (
    "#pragma once\n#define MODEL_SHA "
    + literal(a)
    + "\n#define FIRMWARE_SHA "
    + literal(b)
    + "\n#define MODEL_IDENTITY_JSON "
    + literal(json.dumps(identity, separators=(",", ":")))
    + "\n"
)
(out / "identity.h").write_text(header, encoding="utf8")
print("[build_tag] " + b)
print("[model_identity] " + json.dumps(identity, separators=(",", ":")))
env.Prepend(CPPPATH=[str(out)])
