"""把秦殤場景（地板 + 建築/樹木）畫成一張 PNG。

.MAP 格式（自行分析）：0x22 寬、0x24 高(u16，格數)；0x90 磁磚封包名；
  0x196 起 u16 對照表(長度不定，目前都是 0,1,2...)；檔尾 w*h 格，每格 10 bytes：
  (地板磁磚 u16, 疊加磁磚 u16, 可走旗標 u8, ?, ? u16, 填充)，0xFFFF = 空。
磁磚 64x32 菱形，菱形投影：sx=(x-y)*32, sy=(x+y)*16。
.BNT：0x22 模型檔名；0x480 起每筆 (類型 u16, 模型編號 u16, x u32, y u32, 8 bytes)，
  類型 4 = 觸發區，後面多 128 bytes 文字。
.MDL：每個模型 = 0x03 0x00 + 名稱[32] + (?, 模型編號 u16, ...)，後接數個部件：
  "部件\\0" + 3 bytes + 動畫旗標 u8 + 圖號 u16 + 封包旗標 u16(0x20 = Res01) + 位移 x,y (i32)。
用法：python3 maptool.py <Scn> <遊戲資料夾> <輸出.png>
"""
import os, re, struct, sys
from libtool import decode_xbm, png

PART = re.compile(re.escape("部件\0".encode("gbk")))


def load_map(path):
    d = open(path, 'rb').read()
    w, h = struct.unpack_from('<HH', d, 0x22)
    lib = d[0x90:0x190].split(b'\0')[0].decode()
    grid = len(d) - w * h * 10
    cells = [struct.unpack_from('<HHB', d, grid + i * 10) for i in range(w * h)]
    return w, h, lib, cells


def load_models(path):
    """模型編號 -> [(動畫旗標, 圖號, 封包旗標, dx, dy), ...]"""
    d = open(path, 'rb').read()
    heads = [m.start() + 2 for m in re.finditer(rb'\x03\x00(?=[^\x00\xcd]{2})', d)
             if 1 <= struct.unpack_from('<H', d, m.start() + 34)[0] <= 40]
    models = {}
    for a, b in zip(heads, heads[1:] + [len(d)]):
        mid = struct.unpack_from('<H', d, a + 34)[0]
        models[mid] = [(d[p + 8],) + struct.unpack_from('<HHii', d, p + 9)
                       for p in (m.start() for m in PART.finditer(d, a, b))]
    return models


def load_bnt(path):
    d = open(path, 'rb').read()
    mdl = d[0x22:0x122].split(b'\0')[0].decode()
    p, recs = 0x480, []
    while p < len(d):
        recs.append(struct.unpack_from('<HHII', d, p))
        p += 20 + (128 if recs[-1][0] == 4 else 0)
    assert p == len(d), f'{path}: 解析長度不符'
    return mdl, recs


def scn_files(path):
    text = open(path, 'rb').read().decode('gbk')
    return re.findall(r'=(\S+\.(?:MAP|BNT))', text, re.I)


def render(scn, game, out):
    map_name, bnt_name = scn_files(scn)
    w, h, lib, cells = load_map(os.path.join(game, 'scene/Map', map_name))
    tiles = open(os.path.join(game, 'Res', lib), 'rb').read()
    res01 = open(os.path.join(game, 'Res/Res01.Lib'), 'rb').read()
    cache = {}

    def image(data, i):
        if (id(data), i) not in cache:
            o = struct.unpack_from('<I', data, 0x100 + i * 8)[0]
            cache[id(data), i] = decode_xbm(data, o)
        return cache[id(data), i]

    W, H = (w + h) * 32, (w + h) * 16 + 16
    img = bytearray(W * H * 4)

    def blit(sprite, ox, oy):
        tw, th, rgba = sprite
        for ty in range(max(0, -oy), min(th, H - oy)):
            row = (oy + ty) * W
            for tx in range(max(0, -ox), min(tw, W - ox)):
                p = (ty * tw + tx) * 4
                if rgba[p + 3]:
                    q = (row + ox + tx) * 4
                    img[q:q + 4] = rgba[p:p + 4]

    screen = lambda x, y: ((x - y + h - 1) * 32, (x + y) * 16)
    for i, (ground, over, _) in enumerate(cells):
        for t in (ground, over):
            if t != 0xFFFF:
                blit(image(tiles, t), *screen(i % w, i // w))

    mdl, recs = load_bnt(os.path.join(game, 'scene/Int', bnt_name))
    models = load_models(os.path.join(game, 'Mdl', mdl))
    drawn = skipped = 0
    for kind, mid, x, y in sorted(recs, key=lambda r: (r[2] + r[3], r[2])):
        if kind != 3:
            continue  # 觸發區不畫
        sx, sy = screen(x, y)
        for anim, pic, lib_flag, dx, dy in models.get(mid, []):
            if anim or lib_flag != 0x20:
                skipped += 1  # shortcut: 動畫部件(另一個封包)先跳過，做動畫時再補
                continue
            blit(image(res01, pic), sx + 32 + dx, sy + 16 + dy)
            drawn += 1
    png(out, W, H, bytes(img))
    print(f'{w}x{h} 格, {len(recs)} 個物件, 畫了 {drawn} 個部件, 跳過 {skipped} 個動畫部件 → {out}')


if __name__ == '__main__':
    render(*sys.argv[1:4])
