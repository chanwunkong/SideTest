"""把秦殤主角(紙娃娃)的各部件疊成完整角色，輸出 8 個方向的一張 PNG。

全域資源編號：Res/Combination.ini 的「最小值」決定屬於哪個封包，編號 - 最小值 = 封包內序號。
PLI/0000_001.PLI：文字 #define SPR_ROLE_<角色>_<部件>[S=影子]_<款式>_<PLI檔號> <資源編號>。
PLI/<角色>_<檔號>.PLI：0x22 部件數 n(u8)，0x28 起 n 筆 (部件名[5], 款式[5])；
  0xb6 起 u16 動作序列數，每序列 u16 幀數 + 每幀 n bytes 疊圖順序(部件索引)。
動畫(Res00 項目)：u32 序列數；每序列 (幀數, ?, ?) u32 ×3，每幀 7 個 u32：
  (?, 延遲?, 圖片編號, ?, ?, 腳底 x, 腳底 y)。圖片編號高 8 位 = 幀號(從 1 起)，低 24 位 = xbmgroup。
序列 = 動作 * 8 + 方向（動作起點見 ObjMdl/RoleMdl.txt）。
用法：python3 roletool.py <遊戲資料夾> <角色 1201> <序列起點 8> <幀> <輸出.png>
"""
import os, re, struct, sys
from libtool import decode_xbm, png

LIBS = [('ResUI', 0), ('Res00', 0x100000), ('Res01', 0x200000), ('Res02', 0x300000), ('Res03', 0x400000),
        ('Res04', 0x500000), ('Res05', 0x600000), ('Res06', 0x700000), ('Res07', 0x800000),
        ('Res08', 0x900000), ('ResUI2', 0xA00000), ('ResUI5', 0xA80000), ('ResUI3', 0xB00000), ('ResUI4', 0xC00000)]


class Res:
    def __init__(self, game):
        self.game, self.files = game, {}

    def entry(self, gid):
        name, base = max((l for l in LIBS if l[1] <= gid), key=lambda l: l[1])
        if name not in self.files:
            self.files[name] = open(os.path.join(self.game, 'Res', name + '.Lib'), 'rb').read()
        d = self.files[name]
        o, s = struct.unpack_from('<II', d, 0x100 + (gid - base) * 8)
        return d, o, s

    def sprite(self, gid):
        d, o, s = self.entry(gid)
        p, seqs = o + 4, []
        for _ in range(struct.unpack_from('<I', d, o)[0]):
            n = struct.unpack_from('<I', d, p)[0]
            seqs.append([struct.unpack_from('<IIIIIii', d, p + 12 + j * 28) for j in range(n)])
            p += 12 + 28 * n
        assert p == o + s, '動畫長度不符'
        return seqs

    def frame(self, ref):
        d, o, _ = self.entry(ref & 0xFFFFFF)
        fo = struct.unpack_from('<I', d, o + 0x18 + ((ref >> 24) - 1) * 4)[0]
        return decode_xbm(d, o + fo)


def load_pli(path):
    p = open(path, 'rb').read()
    n = p[0x22]
    parts = [(p[0x28 + i * 10:0x2d + i * 10].split(b'\0')[0].decode(), p[0x2d + i * 10:0x32 + i * 10].split(b'\0')[0].decode())
             for i in range(n)]
    q, orders = 0xb8, []
    for _ in range(struct.unpack_from('<H', p, 0xb6)[0]):
        f = struct.unpack_from('<H', p, q)[0]
        orders.append([list(p[q + 2 + k * n:q + 2 + (k + 1) * n]) for k in range(f)])
        q += 2 + f * n
    assert q == len(p), 'PLI 長度不符'
    return parts, orders


def compose(game, role, seq, frame, pli_no='001'):
    res = Res(game)
    defs = dict(re.findall(r'(SPR_\w+)\t(\d+)', open(os.path.join(game, 'PLI/0000_001.PLI'), 'rb').read().decode('latin1')))
    parts, orders = load_pli(os.path.join(game, f'PLI/{role}_{pli_no}.PLI'))
    W, H, ax, ay = 160, 160, 80, 130
    img = bytearray(W * H * 4)

    def draw(key, shadow):
        gid = defs.get(key)
        if not gid:
            return
        rec = res.sprite(int(gid))[seq][frame]
        w, h, rgba = res.frame(rec[2])
        ox, oy = ax - rec[5], ay - rec[6]
        for y in range(h):
            for x in range(w):
                p = (y * w + x) * 4
                if rgba[p + 3] and 0 <= ox + x < W and 0 <= oy + y < H:
                    q = ((oy + y) * W + ox + x) * 4
                    img[q:q + 4] = bytes((0, 0, 0, 255)) if shadow else rgba[p:p + 4]

    for name, style in parts:  # 影子先畫
        draw(f'SPR_ROLE_{role}_{name}S_{style}_{pli_no}', True)
    for i in orders[seq][frame]:
        name, style = parts[i]
        draw(f'SPR_ROLE_{role}_{name}_{style}_{pli_no}', False)
    return W, H, img


if __name__ == '__main__':
    game, role, start, frame, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
    cells = [compose(game, role, start + d, frame) for d in range(8)]
    W, H = cells[0][0], cells[0][1]
    strip = bytearray(W * 8 * H * 4)
    for d, (_, _, img) in enumerate(cells):
        for y in range(H):
            strip[(y * W * 8 + d * W) * 4:(y * W * 8 + d * W + W) * 4] = img[y * W * 4:(y + 1) * W * 4]
    png(out, W * 8, H, bytes(strip))
    print('→', out)
