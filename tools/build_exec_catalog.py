import os
import struct
import sys

CATALOG_MAGIC = 0x54414345  # 'ECAT'
CATALOG_VERSION = 1

PROGRAMS = [
    (1, "PROGMAN"),
    (2, "CALC"),
    (3, "EDITOR"),
]


def encode_name(name: str) -> bytes:
    raw = name.encode("ascii", errors="ignore")[:16]
    return raw + b"\x00" * (16 - len(raw))


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python tools/build_exec_catalog.py <output.catalog>")
        return 1

    output_path = sys.argv[1]
    header = struct.pack("<IHH", CATALOG_MAGIC, CATALOG_VERSION, len(PROGRAMS))
    entries = []
    for program_id, name in PROGRAMS:
        entries.append(struct.pack("<H16s", program_id, encode_name(name)))

    data = header + b"".join(entries)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as out_file:
        out_file.write(data)

    print(f"Created executable catalog: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
