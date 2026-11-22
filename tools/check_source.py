"""验证当前模型、供应库和项目依赖闭包；不连接硬件。"""

from pathlib import Path
import hashlib, json, sys, ast

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "deployment"))
from direct_contract import validate_identity

validate_identity(json.loads((root / "model/active-model.json").read_text(encoding="utf8")), root)
lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf8"))
for rel, digest in lock["vendored_files"].items():
    if hashlib.sha256((root / rel).read_bytes()).hexdigest() != digest:
        raise ValueError("供应库指纹不符：" + rel)
for folder in ("tools", "tests", "deployment"):
    for f in (root / folder).glob("*.py"):
        ast.parse(f.read_text(encoding="utf8"), filename=str(f))
assert not (root / ".git").exists(), "固件必须保持单仓目录，不能嵌套.git"
print(
    json.dumps(
        {
            "actor_contract": "PASS",
            "vendored_files": len(lock["vendored_files"]),
            "python_syntax": "PASS",
        }
    )
)
