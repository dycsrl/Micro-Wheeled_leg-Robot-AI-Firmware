"""核对正式新身份、逐项拒绝合同变异，并拒绝旧速度契约。"""

from pathlib import Path
import sys, json, copy

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "deployment"))
from direct_contract import EXPECTED, CONFIG_EXPECTED, validate_contract, validate_identity

meta = json.loads((root / "model/active-model.json").read_text(encoding="utf8"))
validate_identity(meta, root)
for key in EXPECTED:
    c = copy.deepcopy(EXPECTED)
    c.pop(key)
    try:
        validate_contract(c, CONFIG_EXPECTED)
    except ValueError:
        pass
    else:
        raise AssertionError(key)
for key in CONFIG_EXPECTED:
    c = copy.deepcopy(CONFIG_EXPECTED)
    c[key] = None
    try:
        validate_contract(EXPECTED, c)
    except ValueError:
        pass
    else:
        raise AssertionError(key)
old = copy.deepcopy(meta)
old["format"] = "r45-wheelref-pairs"
try:
    validate_identity(old, root)
except ValueError:
    pass
else:
    raise AssertionError("旧速度身份未阻断")
print(
    "PASS: source Actor identity (checkpoint not shipped); contract/config mutations rejected; R45 rejected"
)
