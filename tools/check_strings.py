import os
p='build/kernel.bin'
b=open(p,'rb').read()
print('kernel.bin size', len(b))
if b.find(b'Entering kernel')!=-1:
    off=b.find(b'Entering kernel')
    print('Found "Entering kernel" at', hex(off))
    print(b[off:off+32])
else:
    print('Entering kernel not found')
if b.find(b'kernel_main reached')!=-1:
    off=b.find(b'kernel_main reached')
    print('Found "kernel_main reached" at', hex(off))
    print(b[off:off+32])
else:
    print('kernel_main reached not found')
