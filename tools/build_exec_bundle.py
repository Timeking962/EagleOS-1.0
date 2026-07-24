import os
import struct
import sys

CATALOG_MAGIC = 0x54414345  # 'ECAT'
CATALOG_VERSION = 2
APP_MAGIC = 0x50504145      # 'EAPP'
APP_VERSION = 1

# flags in catalog entries
CAT_FLAG_BUILTIN_BRIDGE = 0x0001
CAT_FLAG_NATIVE_ENTRY = 0x0002

PROGRAMS = [
    (1, "PROGMAN", CAT_FLAG_BUILTIN_BRIDGE),
    (2, "CALC", CAT_FLAG_BUILTIN_BRIDGE),
    (3, "EDITOR", CAT_FLAG_BUILTIN_BRIDGE),
    (4, "FILEMAN", CAT_FLAG_BUILTIN_BRIDGE),
    (5, "INSTALL", CAT_FLAG_BUILTIN_BRIDGE),
    (6, "SYSVER", CAT_FLAG_BUILTIN_BRIDGE),
]


def encode_name(name: str) -> bytes:
    raw = name.encode("ascii", errors="ignore")[:16]
    return raw + b"\x00" * (16 - len(raw))


def build_bridge_blob(name: str) -> bytes:
    app_header = struct.pack(
        "<IHHIIIII16s",
        APP_MAGIC,
        APP_VERSION,
        0,          # flags
        0,          # image_size
        0,          # entry_offset
        0,          # reloc_offset
        0,          # reloc_count
        0,          # bss_size
        encode_name(name),
    )
    return app_header


def build_native_blob(name: str, raw_image: bytes, entry_offset: int = 0) -> bytes:
    app_header = struct.pack(
        "<IHHIIIII16s",
        APP_MAGIC,
        APP_VERSION,
        0,
        len(raw_image),
        entry_offset,
        0,
        0,
        0,
        encode_name(name),
    )
    return app_header + raw_image


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: python tools/build_exec_bundle.py <output.bundle> <calc_native.bin>")
        return 1

    output_path = sys.argv[1]
    calc_native_path = sys.argv[2]

    with open(calc_native_path, "rb") as in_file:
        calc_native_image = in_file.read()

    blobs = []
    blob_offsets = []
    cursor = 0
    for _pid, name, flags in PROGRAMS:
        if flags & CAT_FLAG_NATIVE_ENTRY:
            blob = build_native_blob(name, calc_native_image, entry_offset=0)
        else:
            blob = build_bridge_blob(name)
        blobs.append(blob)
        blob_offsets.append(cursor)
        cursor += len(blob)

    entries = []
    for i, (program_id, name, flags) in enumerate(PROGRAMS):
        entries.append(
            struct.pack(
                "<H16sIIHH",
                program_id,
                encode_name(name),
                blob_offsets[i],
                len(blobs[i]),
                flags,
                0,
            )
        )

    header_size = struct.calcsize("<IHHII")
    entry_size = struct.calcsize("<H16sIIHH")
    blob_base_offset = header_size + entry_size * len(entries)

    body = b"".join(entries) + b"".join(blobs)
    bundle_size = header_size + len(body)

    header = struct.pack(
        "<IHHII",
        CATALOG_MAGIC,
        CATALOG_VERSION,
        len(entries),
        bundle_size,
        blob_base_offset,
    )

    data = header + body

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as out_file:
        out_file.write(data)

    print(f"Created executable bundle: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
