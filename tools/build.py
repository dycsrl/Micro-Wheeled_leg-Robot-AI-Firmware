"""从任何cwd构建；只调用run，不提供烧录入口。"""

from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", help="PlatformIO executable path")
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Use synthetic configuration; never flash resulting binary",
    )
    parser.add_argument("--log", type=Path, default=root / "build/build.log")
    args = parser.parse_args()
    pio = args.pio or shutil.which("pio") or shutil.which("platformio")
    if not pio:
        candidate = (
            Path.home() / ".platformio/penv" / ("Scripts/pio.exe" if os.name == "nt" else "bin/pio")
        )
        if candidate.is_file():
            pio = str(candidate)
    if not pio:
        parser.error("Install PlatformIO Core 6.1.18 or pass --pio")
    env = os.environ.copy()
    if args.build_only:
        env["MUSHIBOT_BUILD_ONLY"] = "1"
    else:
        env.pop("MUSHIBOT_BUILD_ONLY", None)
    log = args.log.resolve()
    log.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [pio, "run", "--project-dir", str(root), "-e", "esp32dev", "-j", "2"],
        cwd=root,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = result.stdout.decode("utf8", errors="replace")
    log.write_text(output, encoding="utf8")
    print(output)
    if result.returncode:
        return result.returncode
    binary = root / ".pio/build/esp32dev/firmware.bin"
    print(
        json.dumps(
            {
                "build_only": args.build_only,
                "bin_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "log": str(log),
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
