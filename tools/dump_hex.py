import os
p='build\\kernel.bin'
with open(p,'rb') as f:
    b=f.read()
print('kernel.bin size', os.path.getsize('build\\kernel.bin'))

def find_and_print(s):
    idx = b.find(s)
    if idx>=0:
        print('Found', s, 'at', hex(idx))
        print(b[idx:idx+64])
    else:
        print(s, 'not found')

find_and_print(b'Entering kernel')
find_and_print(b'kernel_main reached')

img='build\\eagleos.img'
with open(img,'rb') as f:
    b0=f.read(64)
    f.seek(512)
    b1=f.read(64)
print('eagleos.img size', os.path.getsize(img))
print('offset0', ' '.join(['%02X'%c for c in b0]))
print('offset512', ' '.join(['%02X'%c for c in b1]))
