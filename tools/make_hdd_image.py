import os
import sys

if len(sys.argv) not in (4, 5, 6):
    print("Usage: python tools/make_hdd_image.py <boot.bin> <kernel.bin> <output.img> [exec.bundle] [size_mb]")
    sys.exit(1)

boot_path = sys.argv[1]
kernel_path = sys.argv[2]
output_path = sys.argv[3]
bundle_path = sys.argv[4] if len(sys.argv) >= 5 else None
size_mb = int(sys.argv[5]) if len(sys.argv) >= 6 else 64

with open(boot_path, "rb") as boot_file:
    boot_data = boot_file.read()
with open(kernel_path, "rb") as kernel_file:
    kernel_data = kernel_file.read()

bundle_data = b""
if bundle_path:
    with open(bundle_path, "rb") as bundle_file:
        bundle_data = bundle_file.read()

if len(boot_data) != 512:
    print("Error: boot sector must be exactly 512 bytes")
    sys.exit(1)

if len(kernel_data) > 0x10000 and kernel_data[:0x10000] == b"\x00" * 0x10000:
    kernel_data = kernel_data[0x10000:]

blob = boot_data + kernel_data + bundle_data

total_size = size_mb * 1024 * 1024
if len(blob) > total_size:
    print(f"Error: image payload ({len(blob)} bytes) exceeds target size ({total_size} bytes)")
    sys.exit(1)

image = blob + b"\x00" * (total_size - len(blob))

os.makedirs(os.path.dirname(output_path), exist_ok=True)
with open(output_path, "wb") as out_file:
    out_file.write(image)

print(f"Created hard disk image: {output_path} ({size_mb} MB)")
