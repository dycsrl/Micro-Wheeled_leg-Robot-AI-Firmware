from pathlib import Path
import sys, subprocess, json, tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from codec import decode, STRUCT

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=root.parent) as d:
    d = Path(d)
    exe = d / "control-test"
    subprocess.run(
        [
            "c++",
            "-std=c++11",
            "-Wall",
            "-Wextra",
            str(root / "tests/test_control.cpp"),
            "-o",
            str(exe),
        ],
        check=True,
    )
    subprocess.run([str(exe)], cwd=d, check=True)
    data = (d / "cpp-frame.bin").read_bytes()
    frame = decode(data)
    assert frame["input"][62] == 62
    raw = d / "synthetic.bin"
    raw.write_bytes(b'{"synthetic":true,"source":"C++ test, not robot"}\n' + data)
    result = subprocess.run(
        [
            sys.executable,
            str(root / "tools/parse.py"),
            str(raw),
            "--output",
            str(d / "parsed.jsonl"),
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert json.loads(result.stdout)["frames"] == 1
    print(
        json.dumps(
            {
                "cpp_direct_voltage_core_adapter_safety": "PASS",
                "cpp_python_wire": "PASS",
                "parse_cli": "PASS",
                "frame_bytes": STRUCT.size,
                "synthetic": True,
            }
        )
    )
