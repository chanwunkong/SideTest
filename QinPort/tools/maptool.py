"""把秦殤 .MAP 地圖的地板畫成一張 PNG（不含建築物件）。

.MAP 格式（自行分析）：0x22 寬、0x24 高(u16，格數)；0x90 磁磚封包名；
  0x196 起 9000 個 u16 對照表(目前都是 0..8999)；之後每格 10 bytes：
  (地板磁磚 u16, 疊加磁磚 u16, 可走旗標 u8, ?, ? u16, 填充)，0xFFFF = 空。
磁磚 64x32 菱形，菱形投影：sx=(x-y)*32, sy=(x+y)*16。
用法：python3 -I maptool.py <MAP> <Res資料夾> <輸出.png>
"""
import os, struct, sys
from libtool import decode_xbm, png

GRID = 0x196 + 9000 * 2


def load_map(path):
    d = open(path, 'rb').read()
    w, h = struct.unpack_from('<HH', d, 0x22)
    lib = d[0x90:0x190].split(b'\0')[0].decode()
    cells = [struct.unpack_from('<HHB', d, GRID + i * 10) for i in range(w * h)]
    return w, h, lib, cells


def render(path, res, out):
    w, h, lib, cells = load_map(path)
    tiles = open(os.path.join(res, lib), 'rb').read()
    cache = {}

    def tile(i):
        if i not in cache:
            o = struct.unpack_from('<I', tiles, 0x100 + i * 8)[0]
            cache[i] = decode_xbm(tiles, o)
        return cache[i]

    W, H = (w + h) * 32, (w + h) * 16 + 16
    img = bytearray(W * H * 4)
    for i, (ground, over, _) in enumerate(cells):
        x, y = i % w, i // w
        ox, oy = (x - y + h - 1) * 32, (x + y) * 16
        for t in (ground, over):
            if t == 0xFFFF:
                continue
            tw, th, rgba = tile(t)
            for ty in range(th):
                row = (oy + ty) * W
                for tx in range(tw):
                    p = (ty * tw + tx) * 4
                    if rgba[p + 3]:
                        q = (row + ox + tx) * 4
                        img[q:q + 4] = rgba[p:p + 4]
    png(out, W, H, bytes(img))
    print(f'{w}x{h} 格, {lib}, {len(cache)} 種磁磚 → {out}')


if __name__ == '__main__':
    render(*sys.argv[1:4])
