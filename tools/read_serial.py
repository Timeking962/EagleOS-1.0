from pathlib import Path
p = Path('build/serial.log')
print('exists', p.exists())
if p.exists():
    data = p.read_bytes()
    print('size', len(data))
    print('ascii:', ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[:200]))
    print('hex:', ' '.join(f'{b:02X}' for b in data[:200]))
