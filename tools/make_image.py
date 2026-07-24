import os
import sys

if len(sys.argv) not in (4, 5):
    print("Usage: python tools/make_image.py <boot.bin> <kernel.bin> <output.img> [exec.catalog]")
    sys.exit(1)

boot_path, kernel_path, output_path = sys.argv[1:4]
catalog_path = sys.argv[4] if len(sys.argv) == 5 else None

with open(boot_path, "rb") as boot_file:
    boot_data = boot_file.read()
with open(kernel_path, "rb") as kernel_file:
    kernel_data = kernel_file.read()

catalog_data = b""
if catalog_path:
    with open(catalog_path, "rb") as catalog_file:
        catalog_data = catalog_file.read()

# If the kernel binary was generated with a 0x10000 link base, objcopy may
# include a leading 64KB zero gap. The bootloader loads the kernel from disk
# sector 2 into physical 0x10000, so the stored file should start with code.
if len(kernel_data) > 0x10000 and kernel_data[:0x10000] == b"\x00" * 0x10000:
    kernel_data = kernel_data[0x10000:]

image = boot_data + kernel_data + catalog_data
if len(image) < 2880 * 512:
    image += b"\x00" * ((2880 * 512) - len(image))

os.makedirs(os.path.dirname(output_path), exist_ok=True)
with open(output_path, "wb") as out_file:
    out_file.write(image)

print(f"Created raw disk image: {output_path}")
