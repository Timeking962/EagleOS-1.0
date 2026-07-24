from pathlib import Path
for name in ['build/boot.bin', 'build/kernel.bin', 'build/eagleos.img']:
    p = Path(name)
    print(name, 'exists', p.exists(), 'size', p.stat().st_size if p.exists() else 'n/a')
    if p.exists():
        data = p.read_bytes()
        if name == 'build/boot.bin':
            print('boot magic', data[510:512].hex())
            print('boot first 32', data[:32].hex())
            idx = data.find(b'[boot] bootloader loaded')
            print('boot msg offset', idx)
            if idx>=0:
                print(data[idx:idx+32])
        elif name == 'build/kernel.bin':
            idx = data.find(b'Entering kernel')
            print('kernel msg offset', idx)
            idx2 = data.find(b'kernel_main reached')
            print('kernel_main msg offset', idx2)
            if idx>=0:
                print(data[idx:idx+32])
            if idx2>=0:
                print(data[idx2:idx2+32])
        elif name == 'build/eagleos.img':
            print('img first 16', data[:16].hex())
            print('img offset512 first 16', data[512:528].hex())
