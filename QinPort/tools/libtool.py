"""秦殤資源封包工具：讀 Res/*.Lib，把 xbm 圖片解成 PNG。

格式（自行分析）：
  Lib 1.01：0x10 = 項目數 u32；0x100 起每項 (offset u32, size u32)；項目可再包一層 Lib。
  xbm：0x14 寬、0x18 高、0x20 透明色(RGB565)；0x40 起每列一個 u32 位移(相對 0x40)；
       每列是交替的 u16：透明位元組數、不透明位元組數+像素(RGB565)，直到填滿寬度。
  xbmgroup：0x10 = 幀數，0x18 起為各幀相對位移，每幀是一張 xbm。
用法：python3 -I libtool.py <Lib檔> <輸出資料夾> [最多幾張]
"""
import os, struct, sys, zlib


def lib_entries(data, base=0):
    n = struct.unpack_from('<I', data, base + 16)[0]
    return [struct.unpack_from('<II', data, base + 0x100 + i * 8) for i in range(n)]


def decode_xbm(data, base=0):
    w, h = struct.unpack_from('<II', data, base + 0x14)
    rgba = bytearray(w * h * 4)
    for y in range(h):
        p = base + 0x40 + struct.unpack_from('<I', data, base + 0x40 + y * 4)[0]
        x, opaque = 0, False
        while x < w:
            n = struct.unpack_from('<H', data, p)[0] // 2
            p += 2
            if opaque:
                for i in range(min(n, w - x)):
                    c = struct.unpack_from('<H', data, p + i * 2)[0]
                    o = (y * w + x + i) * 4
                    rgba[o:o + 4] = bytes(((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31, 255))
                p += n * 2
            x += n
            opaque = not opaque
    return w, h, bytes(rgba)


def png(path, w, h, rgba):
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def walk(data, base, size, name, out):
    """依項目類型遞迴解出 (名稱, w, h, rgba)。"""
    magic = data[base:base + 8]
    if magic.startswith(b'Lib 1.01'):
        for i, (o, s) in enumerate(lib_entries(data, base)):
            if s:
                yield from walk(data, base + o, s, f'{name}_{i}', out)
    elif magic == b'xbmgroup':
        n = struct.unpack_from('<I', data, base + 0x10)[0]
        for i in range(n):
            o = struct.unpack_from('<I', data, base + 0x18 + i * 4)[0]
            yield (f'{name}_f{i}',) + decode_xbm(data, base + o)
    elif magic.startswith(b'xbm\0'):
        yield (name,) + decode_xbm(data, base)


def selftest():
    # 2x1 圖：第一格透明、第二格純紅
    xbm = bytearray(0x40) + struct.pack('<I', 4) + struct.pack('<HHH', 2, 2, 0xF800)
    xbm[0:4] = b'xbm\0'
    struct.pack_into('<II', xbm, 0x14, 2, 1)
    assert decode_xbm(bytes(xbm)) == (2, 1, bytes(4) + bytes((255, 0, 0, 255)))


if __name__ == '__main__':
    selftest()
    src, out = sys.argv[1], sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    os.makedirs(out, exist_ok=True)
    data = open(src, 'rb').read()
    stem = os.path.splitext(os.path.basename(src))[0]
    count = 0
    for name, w, h, rgba in walk(data, 0, len(data), stem, out):
        png(os.path.join(out, name + '.png'), w, h, rgba)
        count += 1
        if count >= limit:
            break
    print(f'{count} 張 → {out}')
