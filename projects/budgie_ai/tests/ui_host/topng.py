import struct, sys, zlib
W = H = 480
for path in sys.argv[1:]:
    data = open(path, 'rb').read()
    rows = b''.join(b'\x00' + bytes(x for i in range(W) for x in (data[(y*W+i)*4+2], data[(y*W+i)*4+1], data[(y*W+i)*4])) for y in range(H))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b'')
    open(path.replace('.raw', '.png'), 'wb').write(png)
