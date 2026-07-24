from pathlib import Path
for filename in ['build/boot.bin', 'build/kernel.bin', 'build/eagleos.img']:
    p = Path(filename)
    print(f'{filename}: exists={p.exists()} size={p.stat().st_size if p.exists() else None}')
    if not p.exists():
        continue
    data = p.read_bytes()
    if filename == 'build/boot.bin':
        print(' boot magic', data[510:512].hex())
        print(' boot text', data[:64].hex())
        idx = data.find(b'[boot] bootloader loaded')
        print(' boot msg idx', idx)
    if filename == 'build/eagleos.img':
        print(' img magic', data[510:512].hex())
        print(' img boot text', data[:64].hex())
        idx = data[:512].find(b'[boot] bootloader loaded')
        print(' img boot msg idx', idx)
        if idx >= 0:
            print(' img boot msg', data[idx:idx+32])
    if filename == 'build/kernel.bin':
        idx = data.find(b'Entering kernel')
        print(' kernel msg idx', idx)
        idx2 = data.find(b'kernel_main reached')
        print(' kernel_main msg idx', idx2)
