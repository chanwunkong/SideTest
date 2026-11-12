"""挑出指定場景與主角實際用到的資源，輸出縮小版遊戲資料夾（給網頁版預載用）。

封包保持原格式：索引表照舊，沒用到的項目大小設成 0，所以引擎不用改。
輸出只給自己的裝置測試用，不要放到公開網路上。
用法：python3 webpack.py <遊戲資料夾> <輸出資料夾> <場景.Scn>...
"""
import os, re, shutil, struct, sys
from maptool import load_map, load_bnt, load_models, scn_files, scn_ont
from roletool import LIBS, Res, load_pli

HERO = '1201'


def find_ci(d, name):
    for n in os.listdir(d):
        if n.lower() == name.lower():
            return n
    raise FileNotFoundError(f'{d}/{name}')


def copy(game, out, rel_dir, name):
    name = find_ci(os.path.join(game, rel_dir), name)
    os.makedirs(os.path.join(out, rel_dir), exist_ok=True)
    shutil.copy(os.path.join(game, rel_dir, name), os.path.join(out, rel_dir, name))
    return name


def trim_lib(src, dst, keep):
    d = open(src, 'rb').read()
    n = struct.unpack_from('<I', d, 16)[0]
    index, body, off = bytearray(), bytearray(), 0x100 + n * 8
    for i in range(n):
        o, s = struct.unpack_from('<II', d, 0x100 + i * 8)
        if i in keep and s:
            index += struct.pack('<II', off + len(body), s)
            body += d[o:o + s]
        else:
            index += struct.pack('<II', 0, 0)
    open(dst, 'wb').write(d[:0x100] + index + body)
    return len(body)


def main(game, out, scenes):
    res = Res(game)
    gids = set()  # 全域編號（含 Res00 動畫本身）

    def add_anim(gid, seqs=None):
        """seqs = 會播的序列（None = 全部）；只打包這些序列的圖。"""
        gids.add(gid)
        for i, seq in enumerate(res.sprite(gid)):
            if seqs is None or i in seqs:
                for f in seq:
                    gids.add(f[2] & 0xFFFFFF)

    tiles, tmpls = {}, set()
    for scn in scenes:
        scn = copy(game, out, 'scene', scn)
        map_name, bnt_name = scn_files(os.path.join(game, 'scene', scn))
        map_name = copy(game, out, 'scene/Map', map_name)
        bnt_name = copy(game, out, 'scene/Int', bnt_name)
        w, h, lib, cells = load_map(os.path.join(game, 'scene/Map', map_name))
        tiles.setdefault(lib, set()).update(t for c in cells for t in c[:2] if t != 0xFFFF)
        ont = scn_ont(os.path.join(game, 'scene', scn))
        if ont:
            ont = copy(game, out, 'scene/Int', ont)
            tmpls.update(int(t) for t in re.findall(r'\n\xc4\xa3\xb0\xe5ID=(\d+)', open(os.path.join(game, 'scene/Int', ont), 'rb').read().decode('latin1')))
        mdl, recs = load_bnt(os.path.join(game, 'scene/Int', bnt_name))
        mdl = copy(game, out, 'Mdl', mdl)
        models = load_models(os.path.join(game, 'Mdl', mdl))
        for _, mid, _, _ in recs:
            for anim, pic, lib_flag, _, _ in models.get(mid, []):
                gid = pic | lib_flag << 16
                add_anim(gid, {0}) if anim else gids.add(gid)  # 場景動畫只播第 0 序列

    copy(game, out, '.', 'EasyRPG.dll')  # 場景出口定義（引擎啟動時掃描）
    # 聲音：設定檔、音樂對照表、腳步聲；音樂只放 20MB 以下的（網頁版整包預載，太大手機吃不消）
    copy(game, out, 'ini', 'Config.ini')
    copy(game, out, 'Data', 'music.txt')
    copy(game, out, 'sound', '080.wav')
    raw = open(os.path.join(game, 'Data', 'music.txt'), 'rb').read()
    music = {l.split('\t')[0].lower(): l.split('\t')[1] for l in bytes(c ^ 0x78 for c in raw[1:]).decode('gbk', 'replace').splitlines()[1:] if '\t' in l}
    for scn in scenes:
        name = music.get(scn.rsplit('.', 1)[0].lower(), 'normal1') + '.wav'
        try:
            path = os.path.join(game, 'Music', find_ci(os.path.join(game, 'Music'), name))
            if os.path.getsize(path) < 20_000_000:
                copy(game, out, 'Music', name)
        except FileNotFoundError:
            pass
    defs_name = copy(game, out, 'PLI', '0000_001.PLI')
    defs = dict(re.findall(r'(SPR_\w+)\t(\d+)', open(os.path.join(game, 'PLI', defs_name), 'rb').read().decode('latin1')))
    # 角色表：主角（圖形 1201）+ 場景 NPC 用到的模板，依「屬性」打包每個部件的動畫
    copy(game, out, 'ObjMdl', 'RoleMdl.txt')
    raw = open(os.path.join(game, 'ObjMdl', 'RoleMdl.txt'), 'rb').read()
    rows = [l.split('\t') for l in bytes(c ^ 0x78 for c in raw[1:]).decode('gbk', 'replace').splitlines()[1:]]
    for r in rows:
        if len(r) < 21 or not r[1].isdigit() or not r[2].isdigit():
            continue
        if int(r[1]) not in tmpls and r[2] != HERO:
            continue
        graphic, pli_no, styles = r[2], int(r[5]), r[3].split('*')
        first = lambda v: int(v.split('*')[0]) if v.split('*')[0].isdigit() else 0
        seqs = {first(r[7]) + d for d in range(8)} | {first(r[8]) + d for d in range(8)}  # shortcut: 只有站立和走路，做戰鬥時再加
        try:
            pli = copy(game, out, 'PLI', f'{graphic}_{pli_no:03d}.PLI')
        except FileNotFoundError:
            continue
        for (part, _), style in zip(load_pli(os.path.join(game, 'PLI', pli))[0], styles):
            for key in (f'SPR_ROLE_{graphic}_{part}_{style}_{pli_no:03d}', f'SPR_ROLE_{graphic}_{part}S_{style}_{pli_no:03d}'):
                if key in defs:
                    add_anim(int(defs[key]), seqs)

    os.makedirs(os.path.join(out, 'Res'), exist_ok=True)
    total = 0
    for lib, keep in tiles.items():
        lib = find_ci(os.path.join(game, 'Res'), lib)
        total += trim_lib(os.path.join(game, 'Res', lib), os.path.join(out, 'Res', lib), keep)
    for name, base in LIBS:
        keep = {g - base for g in gids if max((l for l in LIBS if l[1] <= g), key=lambda l: l[1])[0] == name}
        if keep:
            total += trim_lib(os.path.join(game, 'Res', name + '.Lib'), os.path.join(out, 'Res', name + '.Lib'), keep)
    print(f'{len(gids)} 個資源 + {sum(map(len, tiles.values()))} 張磁磚，資料 {total / 1e6:.1f} MB → {out}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
