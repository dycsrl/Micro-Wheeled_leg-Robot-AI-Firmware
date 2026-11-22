import argparse, json, statistics
from pathlib import Path
from codec import STRUCT, decode

p = argparse.ArgumentParser()
p.add_argument("input")
p.add_argument("--output", required=True)
a = p.parse_args()
b = Path(a.input).read_bytes()
header, b = b.split(b"\n", 1)
meta = json.loads(header)
rows = []
for offset in range(0, len(b) - STRUCT.size + 1, STRUCT.size):
    rows.append(decode(b[offset : offset + STRUCT.size]))
with open(a.output, "x", encoding="utf-8") as out:
    for row in rows:
        out.write(json.dumps(row) + "\n")
print(
    json.dumps(
        {
            "identity": meta,
            "frames": len(rows),
            "trailing_partial_bytes": len(b) % STRUCT.size,
            "sequence_gaps": sum(
                max(0, y["seq"][0] - x["seq"][0] - 1)
                for x, y in zip(rows, rows[1:])
                if x["session"] == y["session"]
            ),
            "comparison": {
                str(m): {
                    "frames": len(r),
                    "pitch_rms_deg": (
                        statistics.mean(x["sensors"][0] ** 2 for x in r) ** 0.5 if r else None
                    ),
                    "mean_cycle_us": statistics.mean(x["cycle_us"][0] for x in r) if r else None,
                }
                for m in [1, 2]
                for r in [[x for x in rows if x["mode"] == [m]]]
            },
        },
        indent=2,
    )
)
