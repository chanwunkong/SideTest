#!/usr/bin/env python3
"""原版函數總表：列出 Poq.exe / EasyRPG.dll / UIWndEx.dll 每個函數，分類並標記移植狀態。

用法：python3 tools/functool.py        → 寫 FUNCS.tsv（遊戲與未完成的劇情函數，每函數一行）並印出統計
      python3 tools/functool.py next [ADDR] [N] → 從 FUNCS.tsv 第一個 todo（或 ADDR）起，列出下一批
          （同類別或連續位址，最多 N 個，預設 12）的大小與標記，和這批還沒移植的被呼叫函數
分類：
  lib    編譯器執行庫 / MFC / ATL / STL / OpenAL（平台層換掉，不翻）
  story  劇情物件的格子函數（storytool 自動翻成 story/）
  base   原版引擎的通用工具（容器、記憶體池…）：tools/base_map.tsv 寫我們用什麼代替，不讀不翻
  game   遊戲本身的邏輯 —— 要移植的就是這些
狀態：
  done   我們的程式碼（engine/src 或 story/）有註記這個原版位址，或 tools/funcs_done.txt 列出對應
  skip   tools/funcs_skip.txt 列出不做的（附原因，例如網路連線版）
  init   全域變數初值（編譯器產生的建構），隨使用它的函數一起移植
  todo   其餘
欄位 callees = 呼叫的遊戲函數中還沒完成的（移植時只要讀這些）；role = tools/roles.tsv 的一句話用途。
欄位偏移的意思記在 tools/fields.tsv；讀原版本體用 tools/fn.py --brief ADDR。
順序：依位址（= 原版原始碼檔案順序，同一類別的函數相連），一段一段做；depth 是從入口沿呼叫圖
（含虛擬函式表）走的深度，參考用。class 前面有 ~ 的是從呼叫者或相鄰函數推出來的。
"""
import os, re, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINS = ['Poq.exe', 'EasyRPG.dll', 'UIWndEx.dll']

LIB_NAME = re.compile(r'^(_|`|~|operator|thunk|Catch|Unwind|alut|Afx|Atl|std|Concurrency|guard_|'
                      r'mem|str|wcs|mbs|free|malloc|realloc|calloc|new|delete|initterm|dllmain|'
                      r'terminate|exception|crt|__|.*<)')
LIB_CLASS = re.compile(r'^(std::|ATL::|Concurrency::|C(Wnd|Dialog|Object|String|GdiObject|Bitmap|Button|Edit|'
                       r'CriticalSection|Array|List|Map|PtrArray|CmdTarget|WinApp|WinThread|Exception|File|'
                       r'Archive|Brush|Pen|Font|DC|PaintDC|ClientDC|Static|ListBox|ComboBox|ScrollBar|'
                       r'SimpleException|MemoryException|NotSupportedException|ResourceException|'
                       r'InvalidArgException|TempGdiObject|HandleMap|NoTrackObject|ThreadLocalObject|'
                       r'ProcessLocalObject|ToolTipCtrl|Rgn|Palette|Menu|ImageList)\b|type_info|bad_|exception)')
# 靜態連結的執行庫從這個位址起（看反編譯檔：之後全是 CRT / MFC 轉接 / 例外處理片段）
LIB_FROM = {'Poq.exe': '005a4cf0', 'EasyRPG.dll': '1020f61a', 'UIWndEx.dll': '10003c2d'}
# Poq.exe 開頭是增量連結的跳板表；Ghidra 在裡面拆出的片段是別的函數的尾巴，不是獨立函數
THUNK_TABLE = {'Poq.exe': ('00401000', '00410000')}
# 編譯器把各檔的全域物件建構集中在這段（沒有呼叫者，由執行庫的初始化表呼叫）：
# 內容是全域變數的初值，移植用到那些變數的函數時一起帶入，不單獨做
INITS = {'Poq.exe': ('00410000', '00433dc0')}
STORY_REF = re.compile(r'\bCER\w*?\d{3,}::vftable')
STORY_CLASS = re.compile(r'^CER(Person|Room|Item|Skill|Plot|Player|Building|Map)\d+$|^CER\w*\d{3,}$')

ADDR = re.compile(r'\bFUN_([0-9a-f]{8})\b')
NAMECALL = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
VFT = re.compile(r'\b([A-Za-z_][\w:<>,* ]*?)::vftable\b')


def parse(path):
    funcs = collections.OrderedDict()
    cur = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('// ==== '):
                p = line.split()
                cur = {'addr': p[2], 'name': p[3] if len(p) > 3 else '', 'body': []}
                funcs[cur['addr']] = cur
            elif cur:
                cur['body'].append(line)
    return funcs


def vtables(path):
    """類別 → 各格位址；位址 → 所屬類別清單。"""
    cls_of, slots = collections.defaultdict(list), collections.defaultdict(list)
    if os.path.exists(path):
        for line in open(path, encoding='utf-8'):
            c, i, a = line.rstrip('\n').split('\t')
            a = a.zfill(8)
            slots[c].append(a)
            if c not in cls_of[a]:
                cls_of[a].append(c)
    return cls_of, slots


def ported_addrs():
    """engine/src 與 story/ 裡出現的原版位址（0x 前綴或 8 位十六進位）。"""
    got = {}
    pat = re.compile(r'(?<![0-9a-fA-F])(?:0x)?([0-9a-fA-F]{6,8})(?![0-9a-fA-F])')
    for top, tag in (('engine/src', 'port'), ('story', 'story')):
        for d, _, fs in os.walk(os.path.join(ROOT, top)):
            for fn in fs:
                if not fn.endswith(('.c', '.h')):
                    continue
                for m in pat.finditer(open(os.path.join(d, fn), encoding='utf-8', errors='replace').read()):
                    got.setdefault(m.group(1).lower().zfill(8), tag)
    return got


def done_map():
    """tools/funcs_done.txt：位址 → 我們對應的程式（原版函數被我們用不同結構實作、不方便在程式裡註記時用）。"""
    out = {}
    p = os.path.join(ROOT, 'tools', 'funcs_done.txt')
    if os.path.exists(p):
        for line in open(p, encoding='utf-8'):
            line = line.split('#', 1)[0].strip()
            if line:
                a, _, why = line.partition(' ')
                out[a.lower().zfill(8)] = why.strip()
    return out


def base_map():
    """tools/base_map.tsv：(位址集合, 類別正規式清單)。"""
    addrs, pats = {}, []
    p = os.path.join(ROOT, 'tools', 'base_map.tsv')
    for line in open(p, encoding='utf-8') if os.path.exists(p) else ():
        if line.startswith('#') or not line.strip():
            continue
        k, _, why = line.rstrip('\n').partition('\t')
        if re.fullmatch(r'[0-9a-f]{8}', k):
            addrs[k] = why
        else:
            pats.append((re.compile(k), why))
    return addrs, pats


def roles():
    out = {}
    p = os.path.join(ROOT, 'tools', 'roles.tsv')
    for line in open(p, encoding='utf-8') if os.path.exists(p) else ():
        if not line.startswith('#') and '\t' in line:
            a, _, r = line.rstrip('\n').partition('\t')
            out[a.lower().zfill(8)] = r
    return out


# 容器樣板的除錯字串（"CStaticLinkList::RemoveAt() : ..."）直接表明是基底
BASE_STR = re.compile(r'"(CStaticLinkList|CContainer|CStack|CMemPool|CChain)\w*::')


def skips():
    out = []
    p = os.path.join(ROOT, 'tools', 'funcs_skip.txt')
    if os.path.exists(p):
        for line in open(p, encoding='utf-8'):
            line = line.split('#', 1)[0].strip()
            if line:
                k, _, why = line.partition(' ')
                out.append((k, why.strip()))
    return out


def analyse(b):
    funcs = parse(os.path.join(ROOT, 'decomp', b + '.c'))
    cls_of, slots = vtables(os.path.join(ROOT, 'decomp', b + '.vtables.txt'))
    by_name = collections.defaultdict(list)
    for a, f in funcs.items():
        by_name[f['name']].append(a)
    # 增量連結的跳板（thunk_FUN_x）：虛擬函式表和呼叫都經過它，追到真正的函數
    thunk = {a: f['name'][10:] for a, f in funcs.items()
             if f['name'].startswith('thunk_FUN_') and f['name'][10:] in funcs}
    res = lambda x: thunk.get(x, x)
    for a in list(cls_of):
        if a in thunk:
            for c in cls_of[a]:
                if c not in cls_of[res(a)]:
                    cls_of[res(a)].append(c)
    slots = {c: [res(x) for x in v] for c, v in slots.items()}
    edges = {}
    for a, f in funcs.items():
        body = ''.join(f['body'])
        f['lines'] = len(f['body'])
        r = set(res(m) for m in ADDR.findall(body) if m in funcs and m != a)
        for m in NAMECALL.findall(body):
            if not m.startswith('FUN_') and len(by_name.get(m, ())) == 1 and m != f['name']:
                r.add(res(by_name[m][0]))
        for c in VFT.findall(body):
            r.update(x for x in slots.get(c.strip(), ()) if x in funcs)
        edges[a] = r
    callers = collections.defaultdict(set)
    for a, r in edges.items():
        for x in r:
            callers[x].add(a)

    cat = {}
    baddr, bpats = base_map()
    for a, f in funcs.items():
        cs = cls_of.get(a, [])
        f['cls'] = '|'.join(cs[:3]) + ('|+%d' % (len(cs) - 3) if len(cs) > 3 else '')
        if a in baddr or (cs and all(any(p.fullmatch(x) for p, _ in bpats) for x in cs)) or \
                BASE_STR.search(''.join(f['body'])):
            cat[a] = 'base'
            continue
        c = cs[0] if cs else ''
        f['cls'] = '|'.join(cs[:3]) + ('|+%d' % (len(cs) - 3) if len(cs) > 3 else '')
        lo, hi = THUNK_TABLE.get(b, ('', ''))
        if a in thunk or lo <= a < hi or 'Could not recover jumptable' in ''.join(f['body']) or \
                re.search(r'__thiscall\s+(ATL|std)::', ''.join(f['body'][:6])):
            cat[a] = 'lib'  # 跳板、DLL 匯入轉接
        elif (cs and all(STORY_CLASS.match(x) for x in cs)) or STORY_REF.search(''.join(f['body'])):
            cat[a] = 'story'
        elif a >= LIB_FROM[b] or (cs and all(LIB_CLASS.match(x) for x in cs)) or (not f['name'].startswith('FUN_') and LIB_NAME.match(f['name'])):
            cat[a] = 'lib'
    # 只被執行庫呼叫的無名函數也算執行庫；只被劇情呼叫的算劇情
    changed = True
    while changed:
        changed = False
        for a in funcs:
            if a in cat or not callers[a]:
                continue
            cs = {cat.get(x) for x in callers[a]}
            if len(cs) == 1 and cs <= {'lib', 'story', 'base'}:
                cat[a] = cs.pop()
                changed = True
    for a in funcs:
        cat.setdefault(a, 'game')
    # 非虛擬的成員函數沒有類別：呼叫者（遊戲函數）全屬同一類別就歸它，寫成 ~類別
    grp = {a: f['cls'].split('|')[0] for a, f in funcs.items() if f['cls']}
    changed = True
    while changed:
        changed = False
        for a in funcs:
            if a in grp or cat[a] != 'game':
                continue
            gs = {grp.get(x, '').lstrip('~') for x in callers[a] if cat[x] == 'game'}
            if len(gs) == 1 and '' not in gs:
                grp[a] = '~' + gs.pop()
                changed = True
    # 同一個 .cpp 的函數位址相連：夾在同一類別兩個函數中間的也歸它
    order = [a for a in funcs if cat[a] == 'game']
    known = [(i, grp[a].lstrip('~')) for i, a in enumerate(order) if a in grp]
    for (i, g), (j, h) in zip(known, known[1:]):
        if g == h:
            for k in range(i + 1, j):
                grp.setdefault(order[k], '~' + g)
    for a, f in funcs.items():
        if not f['cls'] and a in grp:
            f['cls'] = grp[a]

    # 深度：從入口與所有對外名稱走呼叫圖，不進執行庫
    roots = [a for a, f in funcs.items() if f['name'] in ('entry',) or
             (cat[a] == 'game' and not f['name'].startswith('FUN_'))]
    depth = {a: 0 for a in roots}
    q = collections.deque(roots)
    while q:
        a = q.popleft()
        for x in edges[a]:
            if x not in depth and cat[x] not in ('lib', 'base'):
                depth[x] = depth[a] + 1
                q.append(x)
    return funcs, cat, depth, edges, callers


def main():
    done = ported_addrs()
    dmap = done_map()
    sk = skips()
    rl = roles()
    rows, stats = [], collections.Counter()
    for b in BINS:
        funcs, cat, depth, edges, callers = analyse(b)
        for a, f in funcs.items():
            c = cat[a]
            st, why = 'todo', ''
            body = ''.join(f['body'])
            lo, hi = INITS.get(b, ('', ''))
            if a in done:
                st = 'done'
            elif a in dmap:
                st, why = 'done', dmap[a]
            elif c == 'game' and lo <= a < hi and not callers[a]:
                st, why = 'init', '全域初值：移植用到這些變數的函數時一起帶入'
            elif c == 'story' and (re.search(r'::vftable;\s*ExceptionList = local_10;\s*return', body) or
                                   re.search(r'\(param_1 & 1\) != 0', body)):
                st = 'done'  # 劇情類別的建構／解構：story_register 取代
            else:
                for k, w in sk:
                    if k == a or (k == '~' and f['cls'].startswith('~')) or (k != '~' and not re.fullmatch(r'[0-9a-f]{8}', k) and
                                  (any(re.fullmatch(k, x) for x in f['cls'].replace('~', '').split('|')) or
                                   re.fullmatch(k, f['name']))):
                        st, why = 'skip', w
                        break
            if c in ('lib', 'base'):
                st = '-'
            elif c == 'story' and st == 'todo':
                st = 'todo'
            d = depth.get(a, 99)
            rows.append([b, d, a, f['name'], f['cls'], c, st, f['lines'], len(callers[a]), why, rl.get(a, ''), edges[a]])
            stats[(b, c, st)] += 1
        status = {r[2]: r[6] for r in rows if r[0] == b}
        for r in rows:
            if r[0] == b:
                r[11] = ' '.join(sorted(x for x in r[11] if status.get(x) in ('todo', 'init')))
    rows = [r for r in rows if r[5] == 'game' or (r[5] == 'story' and r[6] != 'done')]
    rows.sort(key=lambda r: (BINS.index(r[0]), r[2]))
    with open(os.path.join(ROOT, 'FUNCS.tsv'), 'w', encoding='utf-8') as w:
        w.write('bin\tdepth\taddr\tname\tclass\tcat\tstatus\tlines\tcallers\tnote\trole\tcallees\n')
        for r in rows:
            w.write('\t'.join(map(str, r)) + '\n')
    for b in BINS:
        print(b)
        for c in ('game', 'story', 'base', 'lib'):
            n = {s: stats[(b, c, s)] for s in ('done', 'init', 'skip', 'todo', '-') if stats[(b, c, s)]}
            print('  %-6s %s' % (c, n))


CRT = re.compile(r'\b(_?_?(?:malloc|free|realloc|calloc|mem(?:cpy|set|move|cmp)|str\w*cpy|strlen|strcmp|stricmp|'
                 r'str\w*cat|sprintf|_snprintf|atoi|atol|atof|rand|srand|time|fopen|fread|fwrite|fclose|fseek|'
                 r'operator_new|operator_delete|ftol2?\w*|libm_sse2_\w+|CxxThrowException\w*))\s*\(')


def marks(body, basemap):
    """一眼看出不用細讀的：解構子、std::string、轉接（只呼叫另一個函數）、CRT。"""
    out = []
    if '~' in body[:body.find('{')] or re.search(r'\(param_[12] & 1\) != 0', body):
        out.append('解構')
    if 'basic_string' in body or any('std::string' in basemap.get(x, '')
                                     for x in re.findall(r'FUN_([0-9a-f]{8})', body)):
        out.append('std::string')
    stmts = [l for l in body[body.find('{') + 1:body.rfind('}')].split('\n') if l.strip()]
    if [l.strip() for l in stmts] == ['return;']:
        out.append('空')
    elif len(stmts) == 1 and re.search(r'\w\(', stmts[0]):
        out.append('轉接')
    crt = sorted(set(m.group(1) for m in CRT.finditer(body)))
    if crt:
        out.append('CRT:' + ','.join(crt))
    return ' '.join(out)


def next_batch(args):
    import fn
    start = next((a.lower().removeprefix('0x').zfill(8) for a in args if not a.isdigit()), None)
    n = next((int(a) for a in args if a.isdigit()), 12)
    rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(ROOT, 'FUNCS.tsv'), encoding='utf-8')][1:]
    i = next(k for k, r in enumerate(rows) if (r[2] >= start if start else r[6] == 'todo'))
    b = rows[i][0]
    cls = rows[i][4].lstrip('~')
    batch = []
    for r in rows[i:]:
        if r[0] != b or len(batch) >= n:
            break
        c = r[4].lstrip('~')
        if cls and c and c != cls and batch:  # 類別換了就停
            break
        cls = cls or c
        if r[6] == 'todo':
            batch.append(r)
    idx = fn.index()[b]
    basemap = base_map()[0]
    status = {r[2]: r for r in rows if r[0] == b}
    mine = {r[2] for r in batch}
    print('%s 下一批（%s）：' % (b, cls or '連續位址'))
    for r in batch:
        print('  %s %-14s %4s 行  %s  %s' % (r[2], r[4][:14], r[7], marks(fn.body(b, idx[r[2]]), basemap), r[10]))
    need = sorted({x for r in batch for x in r[11].split()} - mine)
    print('還沒移植的被呼叫函數：' + (' '.join('%s(%s 行 %s)' % (x, status[x][7], status[x][4] or '-')
                                       if x in status else x for x in need) or '無'))
    print('讀本體：python3 tools/fn.py --brief ' + ' '.join(r[2] for r in batch))


if __name__ == '__main__':
    next_batch(sys.argv[2:]) if sys.argv[1:2] == ['next'] else main()
