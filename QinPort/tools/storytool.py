"""把原版劇情引擎 EasyRPG.dll 的劇情物件（房間、NPC、道具、技能、劇情事件）從 Ghidra 反編譯結果轉成一個物件一個 C 檔。

來源：decomp/EasyRPG.dll.c（先跑過 decomp_strings.py）、decomp/EasyRPG.dll.vtables.txt。
- 物件由 FUN_1007f970(工廠, 路徑, 種類, 0) 登記；工廠裡 `*p = 類別::vftable` 指出類別。
- 物件自己的方法 = 虛擬函式表裡只有少數類別共用的格子（多數類別共用的是基底的預設）。
- 選項處理 LAB_xxx 是 `mov eax,[ecx]; jmp [eax+N]` 跳板 → 換成同物件第 N/4 格的方法名。
- 引擎函式依 API 表換成好讀的名字；沒命名的保留 FUN_ 位址。
輸出 story/<種類>/<路徑>.c，以及 story/index.txt（路徑、種類、類別、檔案）。
用法：python3 storytool.py <遊戲資料夾> <decomp 資料夾> <輸出資料夾>
"""
import os, re, struct, sys
from collections import Counter, defaultdict

KINDS = {0: 'room', 1: 'npc', 2: 'item', 5: 'skill', 6: 'plot'}

# 引擎函式命名（看反編譯內容或原版錯誤訊息得出）
API = {
    '100542e0': 'say',            # (格式, ...) 顯示一句話
    '1005eed0': 'clear_options',  # (自己) 清掉對話選項
    '1005f780': 'option',         # (自己, 鍵, 處理, 0, 0, 0, 文字) 加一個回答選項
    '100558b0': 'player',         # () 主角
    '1005df70': 'get_flag',       # (物件, 名稱) 旗標值；沒有 = -1（CERPerson::GetFlag）
    '1005f270': 'set_flag',       # (物件, 名稱, 值)（CERPerson::SetFlag）
    '1005e850': 'new_flag',       # (物件, 名稱, 值)（CERPerson::NewFlag）
    '1005da60': 'find_flag',      # (物件, 名稱)
    '10054ed0': 'find_item',      # (物件, 別名) 身上的道具
    '1003d3e0': 'add_money',      # (物件, 金額) 檢查上限 5 億
    '1004b0d0': 'get_amount',     # (道具) 數量或金額
    '1004dd00': 'quest_log',      # 任務日誌（LogCount）
    '100226e0': 'get_int',        # (物件, 屬性)
    '10023c90': 'set_int',        # (物件, 屬性, 值)
    '100231d0': 'new_int',        # (物件, 屬性, 值)
    '10023c60': 'set_path',       # (自己, 路徑)
    '10023cf0': 'set_name',       # (自己, 名稱)
    '10023c40': 'set_alias',      # (自己, 別名)
    '10023d20': 'set_string',     # (物件, 屬性, 文字)
    '1005d390': 'room_init',
    '1005cd60': 'npc_init',
    '1005d8b0': 'add_exit',       # (自己, 方向, 目的房間, 入口編號)
    '1005f320': 'add_npc',        # (自己, 路徑, 數量)
    '1005f300': 'add_item',       # (自己, 路徑, 數量)
    '10052a00': 'rand_n',         # (n) 0..n-1
    '1004dcc0': 'journal',        # (?, ?, 文字) 任務日誌記一筆
    '1004f1a0': 'start_plot',     # (劇情事件路徑)
    '1003fe30': 'find_room',      # (房間路徑)
    '1003fd90': 'find_alias',     # (別名, ?) 找物件
    '1003cbf0': 'set_int_alias',  # (別名, 屬性, 值, ?)
    '10054960': 'place_npc',      # (房間路徑, NPC 路徑, 數量)
    '10054880': 'give_object',    # (物件, 路徑, 數量) 產生物件放進去（SetObject）
    '10052550': 'spawn',          # 依 StartPoint、等級產生怪物
    '1004c5a0': 'set_spawn_count',  # (數量, 組, 依隊伍人數放大)
    '1004c550': 'set_spawn_max',    # (上限, 依隊伍人數放大)
    '100558c0': 'current_room',   # () 主角所在房間
    '100521f0': 'drop_items',     # (物件, ?, ?, ?) 掉寶（item_%d）
    '1003c6a0': 'set_attr',       # (物件, 屬性, 值) 會同步 HP、MP 等
    '1003fab0': 'find_in_bag',    # (物件, 別名)
    '10052a20': 'random_exits',   # (房間, ?, ?)
    '1005cab0': 'item_init',
    '1005d4a0': 'skill_init',
    '100f94c0': 'chest_init',
    '100f95b0': 'random_chest_init',
    '1005cd40': 'plot_init',
}


STORY_BASE, STORY_END = 0x10100000, 0x101e0000  # 劇情物件的方法在這段（之前是引擎、基底的預設和 API，之後是 C 執行庫）


def _fix(body, own, table, prefix, names, stub_slot):
    body = re.sub(r'^\s*(?:/\*.*?\*/\s*)+', '', body, flags=re.S)  # 簽名前的 Ghidra 警告註解
    body = re.sub(r',unaff_\w+\)', ')', body)
    body = re.sub(r'(?<=[\s=(,])_"', '"', body)  # _DAT_x（複製字串內容）被換成字串時多出的底線  # Ghidra 把 say 的可變參數多猜一個
    def slot_ref(mm):  # 跳板 → 同物件那一格；那格不是自己的（基底預設）就用原函式
        n = stub_slot(mm.group(1))
        if n is None:
            return mm.group(0)
        return f'{prefix}_s{n}' if n in own else f'FUN_{table[n]}' if n in table else mm.group(0)
    body = re.sub(r'&?LAB_([0-9a-f]{8})', slot_ref, body)
    def call(mm):  # 自己的方法直接呼叫；引擎函式一律經 engine_call（WebAssembly 不容許參數不符的呼叫）
        fn, is_call = mm.group(2), mm.group(3)
        if fn in names:
            return names[fn] + ('(' if is_call else '_w')
        if fn in API:
            return API[fn] + '(' if is_call else f'(intptr_t)0x{fn}'
        return f'ENGINE(0x{fn}' + (', ' if is_call else '') if is_call else f'(intptr_t)0x{fn}'
    body = re.sub(r'\b(thunk_)?FUN_([0-9a-f]{8})\b(\()?', call, body)
    body = re.sub(r'ENGINE\((0x[0-9a-f]{8}), \)', r'ENGINE(\1)', body)
    for fn, nm in names.items():  # 自己的方法被當值傳（選項處理）→ 用統一參數的包裝
        body = re.sub(r'\b' + nm + r'\b(?!\s*\()', nm + '_w', body)
    return body

def _wrapper(nm, b):  # intptr_t nm_w(intptr_t *a)：a[0] = this，其餘依原參數轉型
    sig = re.sub(r'\s+', ' ', b.split('{', 1)[0]).strip()
    ret = sig.split(nm + '(')[0].replace('__thiscall', '').replace('__fastcall', '').replace('__cdecl', '').replace('static', '').strip()
    params = sig.split(nm + '(', 1)[1].rsplit(')', 1)[0].strip()
    args = []
    if params and params != 'void':
        for i, p_ in enumerate(params.split(',')):
            ty = re.sub(r'\w+$', '', p_.strip()).strip()
            args.append(f'({ty})a[{i}]')
    callx = f'{nm}({", ".join(args)})'
    body_ = f'{callx}; return 0;' if ret == 'void' else f'return (intptr_t){callx};'
    return f'static intptr_t {nm}_w(intptr_t *a) {{ (void)a; {body_} }}'



def _fix_shared(body, names, stub_slot):
    return _fix(body, {}, {}, None, names, stub_slot)


def pe_reader(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n, opt = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 52)[0]
    secs = []
    for k in range(n):
        s = pe + 24 + opt + k * 40
        vs, va, rs, ro = struct.unpack_from('<IIII', d, s + 8)
        secs.append((base + va, ro, rs))

    def at(va, size):
        for v, ro, rs in secs:
            if v <= va < v + rs:
                return d[ro + va - v:ro + va - v + size]
        return b''
    return at


def main(game, decomp, out):
    at = pe_reader(os.path.join(game, 'EasyRPG.dll'))
    src = open(os.path.join(decomp, 'EasyRPG.dll.c'), encoding='utf-8').read()
    funcs = {}
    for f in re.split(r'\n// ==== ', src)[1:]:
        funcs[f.split(' ', 1)[0]] = f.split('\n', 1)[1].strip('\n')
    vt = defaultdict(dict)
    for line in open(os.path.join(decomp, 'EasyRPG.dll.vtables.txt'), encoding='utf-8'):
        cls, slot, fn = line.split()
        vt[cls][int(slot)] = fn.zfill(8)
    shared = Counter(fn for t in vt.values() for fn in set(t.values()))

    stub_cache = {}

    def stub_slot(lab):  # LAB_xxx 跳板 → 第幾格
        if lab not in stub_cache:
            b = at(int(lab, 16), 8)  # mov eax,[ecx]; jmp [eax+disp8] 或 [eax+disp32]
            stub_cache[lab] = b[4] // 4 if b[:4] == b'\x8b\x01\xff\x60' else \
                struct.unpack_from('<I', b, 4)[0] // 4 if b[:4] == b'\x8b\x01\xff\xa0' else None
        return stub_cache[lab]

    index = []
    objs = []
    # 第 4 個參數 1 = 新遊戲的開場房間（原版 0x1003f7d0 找的就是它；只有秦殇/赵村/ROOM/赵村）
    for fac, path, kind, start in re.findall(r'FUN_1007f970\(FUN_([0-9a-f]{8}),"([^"]*)",(\d),(\d)\)', src):
        m = re.search(r'= ([\w:]+)::vftable;', funcs.get(fac, ''))
        if not m:
            continue
        cls = m.group(1)
        table = vt.get(cls, {})
        alias = None
        own = {s: fn for s, fn in table.items() if shared[fn] <= 2 and fn in funcs}
        init = own.get(1)
        if init:
            am = re.search(r'FUN_10023c40\((?:[^,"]*,)?"([^"]+)"\)', funcs[init])
            alias = am.group(1) if am else None
        objs.append((path, kind, start, cls, table, own, alias))

    # 共用的劇情函式：很多類別共用、從基底繼承來的格子（例如 15 家店的進貨 0x101db580），或劇情呼叫別的類別的方法
    # （衍生類別的第 1 格先呼叫基底的第 1 格）。位址在 STORY_BASE 之後的才是劇情；之前是引擎和基底的預設
    def story_fn(fn):
        return STORY_BASE <= int(fn, 16) < STORY_END and fn in funcs and fn not in API
    def refs(fn):
        return {r for r in re.findall(r'\b(?:thunk_)?FUN_([0-9a-f]{8})\b', funcs[fn]) if story_fn(r) and r != fn}
    common = set()
    for path, kind, start, cls, table, own, alias in objs:
        common |= {fn for s, fn in table.items() if s and s not in own and story_fn(fn)}
        for fn in own.values():
            common |= refs(fn) - set(own.values())
    todo = list(common)
    while todo:
        for r in refs(todo.pop()) - common:
            common.add(r)
            todo.append(r)

    for path, kind, start, cls, table, own, alias in objs:
        prefix = re.sub(r'\W', '_', alias) if alias else 'obj'
        names = {fn: f'{prefix}_s{s}' for s, fn in own.items()}

        fix = lambda body: _fix(body, own, table, prefix, names, stub_slot)
        wrapper = _wrapper

        kname = KINDS.get(int(kind), f'kind{kind}')
        rel = os.path.join(kname, *[re.sub(r'[\\/:*?"<>|]', '_', p) for p in path.split('/')[1:]]) + '.c'
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        bodies = {s: 'static ' + fix(funcs[own[s]]) for s in sorted(own) if s}  # 第 0 格是解構，不要
        text = '\n'.join(bodies.values())
        decl = ['#include "qinstory.h"', '']
        for s, b in bodies.items():  # 先宣告自己的方法：選項會指到後面才定義的格子
            decl.append(re.sub(r'\s+', ' ', b.split('{', 1)[0]).strip() + ';')
            decl.append(f'static intptr_t {names[own[s]]}_w(intptr_t *a);')
        for v in sorted(set(re.findall(r'\b_?(?:DAT|UNK|LAB)_[0-9a-f]{8}\b|\bextraout_\w+\b', text))):
            decl.append(f'static intptr_t {v};')  # 原版的全域資料；&DAT_x 是位址、DAT_x 是值
        reg = f'story_reg_{len(index)}'
        with open(dst, 'w', encoding='utf-8') as w:
            w.write(f'// {path}（{kname}，類別 {cls}，原版 EasyRPG.dll）\n// 由 tools/storytool.py 從反編譯產生；s<N> = 虛擬函式表第 N 格\n')
            w.write('\n'.join(decl) + '\n\n')
            for s, b in bodies.items():
                w.write(f'// ---- 第 {s} 格（原 {own[s]}）\n' + b + '\n' + wrapper(names[own[s]], b) + '\n\n')
            inherit = {s: fn for s, fn in table.items() if s and s not in own and fn in common}
            for sl, fn in inherit.items():  # 繼承來的格子：到共用函式表找原位址
                w.write(f'static intptr_t {prefix}_i{sl}(intptr_t *a) {{ return story_call_addr(0x{fn}u, a); }}\n')
            slots = ', '.join([f'{{{s}, {names[own[s]]}_w}}' for s in bodies] + [f'{{{sl}, {prefix}_i{sl}}}' for sl in inherit])
            w.write(f'void {reg}(void) {{\n    static const StorySlot slots[] = {{{slots}}};\n'
                    f'    story_register("{path}", "{alias or ""}", {kind}, {start}, "{cls}", slots, {len(bodies) + len(inherit)});\n}}\n')
        index.append(f'{path}\t{kname}\t{cls}\t{rel}\t{len(own)}')
    # 共用函式全部放 _shared.c，依原位址登記（story_fn_add），engine_call 找不到的位址會來這裡找
    names = {fn: f'shared_{fn}' for fn in common}
    fix = lambda body: _fix_shared(body, names, stub_slot)
    bodies = {fn: 'static ' + fix(funcs[fn]) for fn in sorted(common)}
    text = '\n'.join(bodies.values())
    with open(os.path.join(out, '_shared.c'), 'w', encoding='utf-8') as w:
        w.write('// 很多劇情類別共用的方法（基底類別的格子、被別的類別呼叫的方法）；由 tools/storytool.py 產生\n#include "qinstory.h"\n\n')
        for fn, b in bodies.items():
            w.write(re.sub(r'\s+', ' ', b.split('{', 1)[0]).strip() + ';\n')
        for v in sorted(set(re.findall(r'\b_?(?:DAT|UNK|LAB)_[0-9a-f]{8}\b|\bextraout_\w+\b', text))):
            w.write(f'static intptr_t {v};\n')
        for fn, b in bodies.items():
            w.write(f'\n// ---- 原 {fn}\n' + b + '\n' + _wrapper(names[fn], b) + '\n')
        w.write('void story_reg_shared(void) {\n' + ''.join(f'    story_fn_add(0x{fn}u, {names[fn]}_w);\n' for fn in bodies) + '}\n')
    print('共用函式', len(common))
    with open(os.path.join(out, 'index.txt'), 'w', encoding='utf-8') as w:
        w.write('\n'.join(index) + '\n')
    with open(os.path.join(out, '_all.c'), 'w', encoding='utf-8') as w:  # 一次登記全部物件
        w.write('#include "qinstory.h"\n' + ''.join(f'void story_reg_{i}(void);\n' for i in range(len(index))))
        w.write('void story_reg_shared(void);\nvoid story_register_all(void) {\n    story_reg_shared();\n' + ''.join(f'    story_reg_{i}();\n' for i in range(len(index))) + '}\n')
    print('物件', len(index), '→', out)


if __name__ == '__main__':
    main(*sys.argv[1:4])
