#!/usr/bin/env python3
"""讀原版 Dlg/*.dlg 文字表（Poq.exe CDlgFile::ReadItem 0x4c0xxx / 0x4c1b10）。
用法：python3 tools/dlgtool.py game/Dlg/UI_C.dlg [編號…]  → 每行「編號<TAB>文字」（UTF-8）
檔頭 0x50 = 索引表位置、0x58 = 筆數；索引每筆 8 bytes（有效旗標, 資料位置）。
資料：u32 長度 + 內容；內容每個 u32 依位元組 i 與 (0xAD − i) 互斥；第一個 u32 = 欄位數，
每欄先一個 u32 種類：0x64/0x65 整數；0x66–0x69 字串（u32 長度 n + n bytes，第 i 個 byte 與 (n − i + 0xA9) 互斥）。"""
import struct, sys

def u32x(b, p):
    return struct.unpack('<I', bytes(b[p + i] ^ ((0xAD - i) & 0xFF) for i in range(4)))[0]

def read(path):
    d = open(path, 'rb').read()
    table, count = struct.unpack_from('<I', d, 0x50)[0], struct.unpack_from('<I', d, 0x58)[0]
    out = {}
    for k in range(count):
        flag, off = struct.unpack_from('<iI', d, table + k * 8)
        if flag < 0 or not off: continue
        n = struct.unpack_from('<I', d, off)[0]
        b, p, texts = d[off + 4:off + 4 + n], 4, []
        for _ in range(u32x(b, 0)):
            kind = u32x(b, p); p += 4
            if kind in (0x64, 0x65): p += 4
            elif 0x66 <= kind <= 0x69:
                m = u32x(b, p); p += 4
                s = bytes(b[p + i] ^ ((m - i + 0xA9) & 0xFF) for i in range(m)); p += m
                texts.append(s.split(b'\0')[0].decode('gbk', 'replace'))
            else: break
        out[k] = texts
    return out

if __name__ == '__main__':
    t = read(sys.argv[1])
    want = [int(x) for x in sys.argv[2:]]
    for k in (want or sorted(t)):
        print(k, *t.get(k, []), sep='\t')
