from pathlib import Path
import re, struct

FIELDS = [
    (t, n, int(c))
    for t, n, c in re.findall(
        r"FIELD\((\w+),(\w+),(\d+)\)",
        (Path(__file__).resolve().parents[1] / "protocol/fields.def").read_text(),
    )
]
STRUCT = struct.Struct("<" + "".join(str(c) + ("I" if t == "u32" else "f") for t, n, c in FIELDS))


def decode(data):
    values = iter(STRUCT.unpack(data))
    out = {n: [next(values) for _ in range(c)] for t, n, c in FIELDS}
    if out["magic"] != [0x32544252] or out["version"] != [1] or out["bytes"] != [STRUCT.size]:
        raise ValueError("bad frame")
    return out


def encode(d):
    return STRUCT.pack(*(x for t, n, c in FIELDS for x in d[n]))
