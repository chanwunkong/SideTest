"""角色圖資源覆蓋表：RoleMdl.txt／BeastieMdl.txt 每個角色 × 動作組（站、走、戰鬥站、戰鬥走、挨打、死亡、出招）
是否能從 Res*.Lib 解到動畫與圖片（同 engine/src/res.c role_load 的路徑：PLI/<圖形>_<PLI>.PLI + 0000_001.PLI 的 SPR_ROLE_ 定義 → 全域編號 → 封包）。
結果：resolved = 有資料且解得到；absent = 原版資料本身沒有（PLI 缺檔、定義 0、序列不存在，原版也畫不出）；
BROKEN = 資料有但我們解不到（索引錯、封包缺、圖壞）→ 程式結束碼 1。
用法：python3 -I tools/role_coverage.py [遊戲資料夾=game] [--all]（--all 連 resolved 的角色都印）"""
import os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
LIBS = [('ResUI', 0), ('Res00', 0x100000), ('Res01', 0x200000), ('Res02', 0x300000), ('Res03', 0x400000), ('Res04', 0x500000),
        ('Res05', 0x600000), ('Res06', 0x700000), ('Res07', 0x800000), ('Res08', 0x900000), ('ResUI2', 0xA00000),
        ('ResUI5', 0xA80000), ('ResUI3', 0xB00000), ('ResUI4', 0xC00000), ('ResRole12Spr', 0xD00000), ('ResRole12', 0xE00000)]  # 同 res.c LIBS

args = [a for a in sys.argv[1:] if not a.startswith('--')]
GAME = args[0] if args else os.path.join(HERE, '..', 'game')
SHOW_ALL = '--all' in sys.argv


def ci(d, name):  # 不分大小寫找檔
    low = {n.lower(): n for n in os.listdir(d)}
    return os.path.join(d, low[name.lower()]) if name.lower() in low else None


def read(sub, name):
    p = ci(os.path.join(GAME, sub), name) if os.path.isdir(os.path.join(GAME, sub)) else None
    return open(p, 'rb').read() if p else None


def unxor(d):
    return bytes(b ^ 0x78 for b in d[1:]) if d and d[0] == 0x80 else d


_libs = {}


def entry(gid):
    """全域編號 → (封包資料, 偏移, 大小)；封包缺檔回 None；項目空回 size 0。"""
    gid &= 0xFFFFFF
    name, base = [l for l in LIBS if l[1] <= gid][-1]
    if name not in _libs:
        p = ci(os.path.join(GAME, 'Res'), name + '.Lib')
        _libs[name] = open(p, 'rb').read() if p else None
    d = _libs[name]
    if d is None:
        return None
    i = gid - base
    if i >= struct.unpack_from('<I', d, 16)[0]:
        return d, 0, 0
    o, s = struct.unpack_from('<II', d, 0x100 + i * 8)
    return d, o, s


def anim(gid):
    """動畫 → 每序列的幀圖片編號清單；None = 解不到。"""
    e = entry(gid)
    if not e or not e[2] or e[2] < 4:
        return None
    d, o, s = e
    n = struct.unpack_from('<I', d, o)[0]
    p, seqs = o + 4, []
    if n > 4096:
        return None
    for _ in range(n):
        if p + 12 > o + s:
            break
        f = struct.unpack_from('<I', d, p)[0]
        p += 12
        if f > 256 or p + f * 28 > o + s:
            break
        seqs.append([struct.unpack_from('<I', d, p + j * 28 + 8)[0] for j in range(f)])
        p += f * 28
    return seqs or None


_img = {}


def image_ok(ref):
    if ref in _img:
        return _img[ref]
    e = entry(ref & 0xFFFFFF)
    ok = False
    if e and e[2] >= 0x18:
        d, o, s = e
        if d[o:o + 8] == b'xbmgroup':
            n, f = struct.unpack_from('<I', d, o + 0x10)[0], max((ref >> 24) - 1, 0)
            ok = f < n and 0x18 + (f + 1) * 4 <= s and struct.unpack_from('<I', d, o + 0x18 + f * 4)[0] < s
        else:
            ok = s >= 0x40
    _img[ref] = ok
    return ok


defs = (read('PLI', '0000_001.PLI') or b'').decode('latin1')
DEF = {m.group(1): int(m.group(2)) for m in re.finditer(r'#define\s+(\S+)\s+(\d+)', defs)}


def load_pli(graphic, pli):
    """回傳 (部件[(部件名, 款式)], 序列數) 或 None（PLI 缺檔）。"""
    p = read('PLI', '%s_%03d.PLI' % (graphic[:8], pli))
    if not p or len(p) < 0x2a or p[:3] != b'PLI' or 0x28 + p[0x22] * 10 + 0x22 > len(p):
        return None
    n = p[0x22]
    parts = [(p[0x28 + i * 10:0x2d + i * 10].split(b'\0')[0].decode(), p[0x2d + i * 10:0x32 + i * 10].split(b'\0')[0].decode()) for i in range(n)]
    t = 0x28 + n * 10 + 0x20
    return parts, struct.unpack_from('<H', p, t)[0]


def role_anims(graphic, pli, styles):
    """該角色各部件（含影子）的 (名稱, 編號)；styles 同 role_load：依部件順序覆蓋款式，000 = 不畫。"""
    info = load_pli(graphic, pli)
    if not info:
        return None, None
    parts, nseq = info
    st = [s for s in styles.split('*')] if styles else []
    out = []
    for i, (name, style) in enumerate(parts):
        if st and st[0]:
            style = st[0][:3]
        st = st[1:] if st else []
        if style == '000':
            continue
        out.append((name, DEF.get('SPR_ROLE_%s_%s_%s_%03d' % (graphic, name, style, pli), 0)))
    return out, nseq


def check_seq(parts_anims, nseq, s):
    """序列 s 是否可畫：PLI 有此序列、至少一個身體部件有此序列且第一張圖解得到。回傳 'ok'/'absent'/'BROKEN'。"""
    if s >= nseq:
        return 'absent'
    have = False
    for gid, a in parts_anims:
        if gid == 0:
            continue
        if a is None:
            return 'BROKEN'  # 定義有編號但封包解不到
        if s < len(a) and a[s]:
            have = True
            if not all(image_ok(r) for r in a[s]):
                return 'BROKEN'
    return 'ok' if have else 'absent'


def col_int(c, i):
    return int(c[i]) if len(c) > i and c[i].strip().lstrip('-').isdigit() else None


def table(path):
    d = unxor(read('ObjMdl', path))
    return [l.split('\t') for l in d.decode('gbk', 'replace').split('\n')[1:] if l.strip()] if d else []


# 招式 → 動作起點（wSprIndex）；角色招式表 RoleMdl.rmd 依列序（同 res.c：每「招式数」開新列）
meth = {}
md = unxor(read('ObjMdl', 'MethodMdl.ini'))
cur = None
for line in (md or b'').decode('latin1').split('\n'):
    k, _, v = line.strip().partition('=')
    if k == 'wAutoID':
        cur = int(v)
    elif k == 'wSprIndex' and cur is not None:
        meth[cur] = int(v)
rmd = []
for line in (unxor(read('ObjMdl', 'RoleMdl.rmd')) or b'').decode('latin1').split('\n'):
    k, _, v = line.strip().partition('=')
    if k == '\xd5\xd0\xca\xbd\xca\xfd':
        rmd.append([])
    elif k == '\xd5\xd0\xca\xbd' and rmd:
        rmd[-1].append(int(v))

# 怪物表引用的角色
mon_roles = set()
for f in sorted(os.listdir(os.path.join(GAME, 'Data'))):
    if re.match(r'npc_.*\.txt$', f, re.I):
        for c in [l.split('\t') for l in (unxor(read('Data', f)) or b'').decode('gbk', 'replace').split('\n')[1:]]:
            for k in range(34, 39):
                if col_int(c, k) and col_int(c, k) > 0:
                    mon_roles.add(col_int(c, k))

rows, bad = [], 0
GROUPS = ['stand', 'walk', 'fstand', 'fwalk', 'hurt', 'die', 'cast']
for ri, c in enumerate(table('RoleMdl.txt')):
    if len(c) < 21 or not c[1].strip():
        continue
    rid, graphic, styles, pli = int(c[1]), c[2], c[3], col_int(c, 5) or 1
    parts, nseq = role_anims(graphic, pli, styles)
    res = {}
    if parts is None:
        res = {g: 'absent' for g in GROUPS}
        why = 'PLI %s_%03d.PLI missing' % (graphic, pli)
    else:
        pa = [(gid, anim(gid) if gid else None) for _, gid in parts]
        # 有款式但定義 0（例如無影子的部件）= 沒這部件，不算壞
        starts = {'stand': col_int(c, 7), 'walk': col_int(c, 8), 'fstand': col_int(c, 9),
                  'fwalk': col_int(c, 10), 'hurt': col_int(c, 11), 'die': col_int(c, 14)}
        why = ''
        for g, s0 in starts.items():
            if s0 is None or (s0 == 0 and g not in ('stand', 'walk')):  # 空欄／0 = 這角色沒有這動作
                res[g] = 'n/a'
            else:
                r = [check_seq(pa, nseq, s0 + dd) for dd in range(8)]
                res[g] = 'ok' if r.count('ok') == 8 else ('BROKEN' if 'BROKEN' in r else ('absent' if 'ok' not in r else 'partial'))
        sk = rmd[ri] if ri < len(rmd) else []
        cs = [meth[m] for m in sk if m in meth]
        r = [check_seq(pa, nseq, s0 + dd) for s0 in cs for dd in range(8)]
        res['cast'] = 'n/a' if not cs else ('ok' if r.count('ok') == len(r) else ('BROKEN' if 'BROKEN' in r else ('absent' if 'ok' not in r else 'partial')))
        if all(gid == 0 for _, gid in parts):
            why = 'no SPR_ROLE defines for %s_%03d (style %s)' % (graphic, pli, styles)
    rows.append((rid, c[0], graphic, rid in mon_roles, res, why))

# 小動物／陷阱：單一動畫
for c in table('BeastieMdl.txt'):
    if len(c) < 7 or col_int(c, 1) is None or col_int(c, 1) <= 0:
        continue
    gid = col_int(c, 2) or 0
    a = anim(gid) if gid else None
    if not gid:
        st, why = 'absent', 'no gid'
    elif a is None:
        st, why = 'BROKEN', 'gid %d unreadable' % gid
    else:
        st, why = ('ok' if all(image_ok(r) for s in a for r in s) else 'BROKEN'), ''
    rows.append((col_int(c, 1), c[0], 'anim %d' % gid, False, {'stand': st}, why))

cnt = {'ok': 0, 'absent': 0, 'BROKEN': 0, 'partial': 0}
print('%-5s %-10s %-10s %-3s %s' % ('id', 'name', 'graphic', 'mon', ' '.join('%-7s' % g for g in GROUPS)))
for rid, name, gfx, mon, res, why in rows:
    vals = [v for v in res.values() if v != 'n/a']
    state = 'BROKEN' if 'BROKEN' in vals else ('absent' if vals and all(v == 'absent' for v in vals) else ('partial' if 'partial' in vals or 'absent' in vals else 'ok'))
    cnt[state] += 1
    bad += state == 'BROKEN'
    if SHOW_ALL or state != 'ok':
        print('%-5d %-10s %-10s %-3s %s %s' % (rid, name[:10], gfx, 'Y' if mon else '', ' '.join('%-7s' % res.get(g, '-') for g in GROUPS), why))
print('roles %d: ok %d, partial %d (some actions absent in data), absent %d (no data at all), BROKEN %d' % (len(rows), cnt['ok'], cnt['partial'], cnt['absent'], cnt['BROKEN']))
sys.exit(1 if bad else 0)
