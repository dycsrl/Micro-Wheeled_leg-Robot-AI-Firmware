"""遥测首行文本与直压合同一致；正式解析器保留身份，帧布局不变。"""

from pathlib import Path
import json, re, subprocess, sys, tempfile, unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from codec import STRUCT, FIELDS, encode


class Header(unittest.TestCase):
    def test_contract_and_consumer(self):
        source = (ROOT / "src/network/Network.cpp").read_text(encoding="utf8")
        # 字符串内也含分号；以赋值后块结束定位，不把参数文本截断。
        line = re.search(r"header\s*=\s*String\(.*?;\s*\}", source, re.S).group(0)
        # 取真实C++字符串字面量，以合成身份替换两个编译期hash。
        literals = re.findall(r'"(?:\\.|[^"\\])*"', line)
        parts = [json.loads(x) for x in literals]
        self.assertEqual(len(parts), 3)
        header = json.loads(parts[0] + "model-test" + parts[1] + "firmware-test" + parts[2])
        contract = json.loads((ROOT / "model/active-model.json").read_text(encoding="utf8"))[
            "deployment_contract"
        ]
        text = header["parameters"]
        self.assertIn(contract["version"], text)
        for value in [
            "U_model=7.4*clip(a,-1,1) V",
            "no wheel LPF/P",
            "hip=.12 rad",
            "control_dt=10ms",
            "substep_dt=1ms",
            "history=3",
        ]:
            self.assertIn(value, text)
        self.assertEqual(contract["control_dt_s"], 0.01)
        self.assertEqual(contract["sim_dt_s"], 0.001)
        self.assertNotIn("P=.08", text)
        self.assertNotIn("wheel=30", text)
        self.assertEqual(header["protocol"], 1)
        self.assertEqual(STRUCT.size, 516)
        frame = {n: [0] * c for t, n, c in FIELDS}
        frame.update(magic=[0x32544252], version=[1], bytes=[STRUCT.size])
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "sample.raw"
            out = Path(tmp) / "frames.jsonl"
            raw.write_bytes((json.dumps(header) + "\n").encode() + encode(frame))
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(ROOT / "tools/parse.py"),
                    str(raw),
                    "--output",
                    str(out),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            summary = json.loads(result.stdout)
            self.assertEqual(summary["identity"], header)
            self.assertEqual(summary["frames"], 1)
            self.assertEqual(summary["trailing_partial_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
