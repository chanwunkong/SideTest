#!/usr/bin/env python3
"""可達性分析：status=skip 的函數，單機遊戲真的會不會執行到。輸出 tools/reach.tsv、tools/reach_summary.md。
邊：直接呼叫／函式指標引用(FUN_x、具名)、vftable（RTA：類別被某可達函數建構才算，且該格有被虛擬呼叫）、
Poq→EasyRPG 匯入、資料區函式指標（初始化陣列=嚴格根，其餘訊息表等=寬鬆根）。
等級：Y 嚴格可達；? 只靠寬鬆邊（vtable 格未被呼叫、資料表）；N 不可達（net=只能從連線程式碼到、dead=圖上沒路）。
用法：python3 tools/reach.py"""
import collections, csv, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINS = ['Poq.exe', 'EasyRPG.dll', 'UIWndEx.dll']
NETCLS = re.compile(r'^(CNet\w*|\w*Socket\w*|CMultiPlayerDialog|CCreateNetGameDialog|CJoinNetGameDialog)$')
NETFN = re.compile(r'\b(PlayerLogout|WSAStartup|gethostbyname|connect|accept|recv|send|closesocket)\s*\(')
TOK = re.compile(r'[A-Za-z_~][A-Za-z0-9_:~]*')
VFT = re.compile(r'([A-Za-z_][\w:<>,~]*?)::vftable\b')
VCALL = re.compile(r'\(\*\*\(code \*\*\)(.{0,100}?)\)\(')
NETGUARD = re.compile(r'DAT_00942f28 (==|!=) 2')

# ---------- PE ----------
def pe_load(path):
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3c)[0]
    ns, = struct.unpack_from('<H', d, e + 6); osz, = struct.unpack_from('<H', d, e + 20)
    opt = e + 24; base, = struct.unpack_from('<I', d, opt + 28)
    secs = []
    for i in range(ns):
        _, vs, va, rs, rp, _, _, _, _, ch = struct.unpack_from('<8sIIIIIIHHI', d, opt + osz + i * 40)
        secs.append((va, max(vs, rs), rp, ch, rs))
    def off(rva):
        for va, s, rp, _, _ in secs:
            if va <= rva < va + s: return rva - va + rp
    def cs(o): return d[o:d.index(b'\0', o)].decode('latin1')
    return d, base, off, cs, opt, secs

def pe_imports(path):
    d, base, off, cs, opt, _ = pe_load(path)
    out = []; imp = struct.unpack_from('<I', d, opt + 96 + 8)[0]
    if not imp: return out
    o = off(imp)
    while True:
        oft, _, _, nm, ft = struct.unpack_from('<IIIII', d, o)
        if not nm: break
        dll = cs(off(nm)); t = off(oft or ft); k = 0
        while True:
            v, = struct.unpack_from('<I', d, t + 4 * k)
            if not v: break
            out.append((dll, cs(off(v & 0x7fffffff) + 2) if not v & 0x80000000 else 'ord%d' % (v & 0xffff)))
            k += 1
        o += 20
    return out

def pe_exports(path):
    d, base, off, cs, opt, _ = pe_load(path)
    exp, = struct.unpack_from('<I', d, opt + 96)
    if not exp: return []
    o = off(exp)
    nf, nn, af, an, ao = struct.unpack_from('<IIIII', d, o + 20)
    nm = {}
    for i in range(nn):
        r, = struct.unpack_from('<I', d, off(an) + 4 * i); q, = struct.unpack_from('<H', d, off(ao) + 2 * i)
        nm[q] = cs(off(r))
    return [(nm.get(i, 'ord%d' % i), '%08x' % (base + struct.unpack_from('<I', d, off(af) + 4 * i)[0])) for i in range(nf)]

def data_ptrs(path, starts):
    """資料區(非可執行)裡指到函數起點的雙字：{addr: [位置...]}"""
    d, base, off, cs, opt, secs = pe_load(path)
    out = collections.defaultdict(list)
    stub = {}  # 增量連結的 jmp 跳板（Ghidra 沒當函數）→ 目標
    for va, s, rp, ch, rs in secs:
        if not ch & 0x20000000: continue
        for i in range(0, min(rs, s) - 5):
            if d[rp + i] == 0xe9:
                t = (base + va + i + 5 + struct.unpack_from('<i', d, rp + i + 1)[0]) & 0xffffffff
                if '%08x' % t in starts: stub[base + va + i] = '%08x' % t
    for va, s, rp, ch, rs in secs:
        if ch & 0x20000000: continue
        for p in range(rp, rp + min(rs, s) - 3, 4):
            v, = struct.unpack_from('<I', d, p)
            h = '%08x' % v
            if h in starts: out[h].append(base + va + p - rp)
            elif v in stub: out[stub[v]].append(base + va + p - rp)
    return out, stub, (d, base, secs)

# ---------- 解析 ----------
def parse(b):
    funcs = {}; cur = None; stack = []; pend = None; elsenet = False
    for line in open(os.path.join(ROOT, 'decomp', b + '.c'), encoding='utf-8', errors='replace'):
        if line.startswith('// ==== '):
            p = line.split()
            cur = funcs[p[2]] = {'name': p[3] if len(p) > 3 else '', 'text': [], 'net': []}
            stack = []; pend = None; elsenet = False
            continue
        if cur is None: continue
        s = line.strip()
        m = NETGUARD.search(line) if 'DAT_00942f28' in line else None
        if m and s.startswith(('if', '}')) is not None:
            pend = 'net' if m.group(1) == '==' else 'net_else'
        if s == 'else {' or s == 'else' or s.startswith('else '):
            if elsenet: pend = 'net'
            elsenet = False
        innet = any(k == 'net' for k in stack)
        (cur['net'] if innet else cur['text']).append(line)
        for ch in line:
            if ch == '{':
                stack.append(pend if pend else 'x'); pend = None
            elif ch == '}' and stack:
                k = stack.pop()
                if k == 'net_else': elsenet = True
    return funcs

def vtabs(b):
    ents = collections.defaultdict(list)  # cls -> [(idx, addr)]
    p = os.path.join(ROOT, 'decomp', b + '.vtables.txt')
    if os.path.exists(p):
        for l in open(p, encoding='utf-8'):
            c, i, a = l.rstrip('\n').split('\t'); ents[c].append((int(i), a.zfill(8)))
    return ents

def slot_of(s):
    ms = re.findall(r'\+ (0x[0-9a-fA-F]+|\d+)\s*$', s.strip().rstrip(')').strip() + ' ')
    m = re.findall(r'\+ (0x[0-9a-fA-F]+|\d+)', s)
    if not m: return 0
    v = int(m[-1], 0)
    return v // 4 if v % 4 == 0 else None

def main():
    FU = list(csv.DictReader(open(os.path.join(ROOT, 'FUNCS.tsv'), encoding='utf-8'), delimiter='\t', quoting=csv.QUOTE_NONE))
    clsof = {(r['bin'], r['addr']): r['class'] for r in FU}
    G = {}  # bin -> info
    for b in BINS:
        fs = parse(b); vt = vtabs(b)
        G[b] = {'f': fs, 'vt': vt, 'by': {}}
        for a, f in fs.items():
            f['text'] = ''.join(f['text']); f['nettext'] = ''.join(f['net'])
            G[b]['by'].setdefault(f['name'], a)
    # 虛擬表資料
    for b in BINS:
        g = G[b]; fs = g['f']
        g['slot_ents'] = collections.defaultdict(list)  # slot -> [(cls, addr)]
        g['vaddrs'] = set()
        for c, l in g['vt'].items():
            for i, a in l:
                if a in fs: g['slot_ents'][i].append((c, a)); g['vaddrs'].add(a)
    # 每個函數的邊
    exports = {b: pe_exports(os.path.join(ROOT, 'game', b)) for b in BINS if b != 'Poq.exe'}
    exname = {}
    for b, l in exports.items():
        for n, a in l:
            if a in G[b]['f']:
                exname.setdefault(G[b]['f'][a]['name'], (b, a))
    for b in BINS:
        g = G[b]; fs = g['f']
        for a, f in fs.items():
            for key, txt in (('E', f['text']), ('EN', f['nettext'])):
                dr, cross, cls, vs = set(), set(), set(), set()
                for t in TOK.findall(txt):
                    if t.startswith('FUN_') and t[4:] in fs: dr.add(t[4:])
                    elif t in g['by']:
                        dr.add(g['by'][t])
                        if t.startswith('thunk_FUN_') and t[10:] in fs: dr.add(t[10:])  # 跳板本體不含目標
                    elif b == 'Poq.exe' and t in exname: cross.add(exname[t])
                cls.update(c.strip() for c in VFT.findall(txt))
                for m in VCALL.finditer(txt):
                    sl = slot_of(m.group(1))
                    vs.add(sl)
                dr.discard(a)
                f[key] = (dr, cross, cls, vs)
            f['netfn'] = bool(NETFN.search(f['text']))
            f['unres'] = len(re.findall(r'\(\*\(code \*\)(?!PTR_guard)', f['text']))
    # 網路節點
    NET = {b: set() for b in BINS}
    for b in BINS:
        for a, f in G[b]['f'].items():
            c = clsof.get((b, a), '')
            if any(NETCLS.match(x.lstrip('~')) for x in c.split('|') if x) or f['netfn']:
                NET[b].add(a)
            elif any(NETCLS.match(x.strip()) for x in f['E'][2]):
                NET[b].add(a)  # 建構網路類別
    # 資料區函式指標
    strict_roots = [('Poq.exe', a) for a in G['Poq.exe']['f'] if G['Poq.exe']['f'][a]['name'] == 'entry']
    loose_roots = []
    dp_stats = {}
    for b in BINS:
        g = G[b]; fs = g['f']
        dp, stub, (dd, pbase, psecs) = data_ptrs(os.path.join(ROOT, 'game', b), fs)
        import bisect
        ss = sorted(int(x, 16) for x in fs)
        nraw = 0
        for va, sz, rp, ch, rs in psecs:  # 原始機器碼的 call/jmp（含 Ghidra 沒當函數的 jmp 跳板）
            if not ch & 0x20000000: continue
            for i in range(0, min(rs, sz) - 5):
                if dd[rp + i] in (0xe8, 0xe9):
                    t = (pbase + va + i + 5 + struct.unpack_from('<i', dd, rp + i + 1)[0]) & 0xffffffff
                    th = stub.get(t, '%08x' % t)
                    if th not in fs: continue
                    k = bisect.bisect_right(ss, pbase + va + i) - 1
                    if k < 0: continue
                    ca = '%08x' % ss[k]
                    if ca != th and th not in fs[ca]['E'][0] and th not in fs[ca]['EN'][0]:  # 只在連線區塊出現的呼叫不算單機
                        fs[ca]['E'][0].add(th); fs[ca]['EN'][0].add(th); nraw += 1
        print(b, 'raw call edges added', nraw, file=sys.stderr)
        pos = {p: a for a, l in dp.items() for p in l}
        runs = set()
        for p in sorted(pos):
            run = [p]
            while run[-1] + 4 in pos: run.append(run[-1] + 4)
            if len(run) >= 2 and p - 4 not in pos:
                for q in run:
                    if pos[q] not in g['vaddrs']: runs.add(pos[q])
        n1 = n2 = 0
        for a in dp:
            if a in g['vaddrs']: continue
            if a in runs: strict_roots.append((b, a)); n1 += 1
            else: loose_roots.append((b, a)); n2 += 1
        dp_stats[b] = (n1, n2)
        for n, a in pe_exports(os.path.join(ROOT, 'game', b)) if b != 'Poq.exe' else []:
            if a in fs: loose_roots.append((b, a))
        for a, f in fs.items():
            if f['name'] in ('entry', 'DllMain') or f['name'].endswith('DllEntryPoint'):
                strict_roots.append((b, a))
    # Poq 真正的 CRT 起點（entry 是加殼存根）：呼叫 AfxWinMain 轉接的函數
    for a, f in G['Poq.exe']['f'].items():
        if 'thunk_FUN_005a9eab' in f['text'] and f['name'] not in ('entry',): strict_roots.append(('Poq.exe', a))
    # 全域物件建構（靜態初始化沒有呼叫者）：CWinApp 子類=嚴格根；其餘「沒呼叫者又設 vftable」=寬鬆根
    called = set()
    for b in BINS:
        for f in G[b]['f'].values():
            called.update((b, d) for d in f['E'][0] | f['EN'][0])
    for b in BINS:
        for a, f in G[b]['f'].items():
            if (b, a) in called or not f['E'][2]: continue
            if 'CWinApp::CWinApp(' in f['text']: strict_roots.append((b, a))
            elif len(f['text']) < 600: loose_roots.append((b, a))
    for b in ('EasyRPG.dll', 'UIWndEx.dll'):
        for a, f in G[b]['f'].items():
            if f['name'] == 'entry': strict_roots.append((b, a))

    def run(roots, block, phase, st):
        """st: 共用狀態；回傳新達到的節點數"""
        parent, level, inst, called, q = st['parent'], st['level'], st['inst'], st['called'], collections.deque()
        def visit(n, par, et):
            if n in level: return
            if block and n[1] in NET[n[0]]:
                st['netseen'].add(n); return
            level[n] = phase; parent[n] = (par, et); q.append(n)
            nm = G[n[0]]['f'][n[1]]['name']
            if nm.startswith('thunk_FUN_') and nm[10:] in G[n[0]]['f']: visit((n[0], nm[10:]), n, 'thunk')
        for r in roots: visit(r, None, 'root')
        def activate_class(b, c, par):
            for i, a in G[b]['vt'].get(c, ()):
                if a in G[b]['f'] and (phase != 'Y' or i in called[b]): visit((b, a), par, 'vt:' + c.split('::')[-1] + '#%d' % i)
        if phase != 'Y':
            for (b, c), par in list(inst.items()): activate_class(b, c, par)
        while q:
            n = q.popleft(); b, a = n; f = G[b]['f'][a]
            dr, cross, cls, vs = f['E'] if block else (lambda x, y: tuple(s | t for s, t in zip(x, y)))(f['E'], f['EN'])
            for d in dr: visit((b, d), n, 'call')
            for c in cross: visit(c, n, 'import')
            for c in cls:
                if (b, c) not in inst:
                    inst[(b, c)] = n; activate_class(b, c, n)
            for s in vs:
                if s is None or s in called[b]: continue
                called[b].add(s)
                for c, ea in G[b]['slot_ents'][s]:
                    if (b, c) in inst: visit((b, ea), n, 'virt#%d' % s)
    def newstate(): return {'parent': {}, 'level': {}, 'inst': {}, 'called': collections.defaultdict(set), 'netseen': set()}
    S = newstate()
    run(strict_roots, True, 'Y', S)
    nY = len(S['level'])
    run(loose_roots, True, '?', S)  # 繼續：先對已建構類別放寬，再加資料表根
    # 無阻擋全圖（只用來區分 net / dead）
    U = newstate()
    run(strict_roots + loose_roots, False, '?', U)
    if os.environ.get('REACH_DEBUG'):
        print(len(S['level']), len(S['inst']), ('Poq.exe', 'CGameApp') in S['inst'], S['level'].get(('Poq.exe', '00461bd0')), [x for x in strict_roots if x[0]=='Poq.exe'][:50])
        import collections as C; print(C.Counter(v for v in S['level'].values()), C.Counter(k[0] for k in S['level']))
    if os.environ.get('REACH_ORPH'):
        rev = collections.defaultdict(set)
        for b in BINS:
            for a, f in G[b]['f'].items():
                for d in f['E'][0] | f['EN'][0]: rev[(b, d)].add((b, a))
                for c in f['E'][1]: rev[c].add((b, a))
                for c in f['E'][2]:
                    for i, ea in G[b]['vt'].get(c, ()): rev[(b, ea)].add((b, a))
        vts = {(b, a) for b in BINS for a in G[b]['vaddrs']}
        miss = [(r['bin'], r['addr']) for r in FU if r['status'] == 'done' and (r['bin'], r['addr']) not in S['level']]
        top = collections.Counter(); ex = {}
        for m in miss:
            seen = {m}; st_ = [m]
            while st_:
                x = st_.pop()
                if not rev[x] - seen and x not in S['level']: top[x] += 1; ex.setdefault(x, m)
                for y in rev[x]:
                    if y not in seen and y not in S['level']: seen.add(y); st_.append(y)
        print(len(miss)); print([(m[1], len(rev[m]), [x[1] for x in rev[m]][:3], m in U['level']) for m in miss])
        for x, c in top.most_common(40): print(x, G[x[0]]['f'][x[1]]['name'], c, 'vtable' if x in vts else '', 'dp' if False else '')
    # 已完成的函數也要知道（驗證用）
    ST = {(r['bin'], r['addr']): r for r in FU}
    def path(n):
        out = []; cur = n
        while cur:
            p, et = S['parent'][cur]; out.append((cur, et)); cur = p
        out.reverse()
        names = lambda x: ('%s%s' % ('' if x[0] == 'Poq.exe' else x[0][:4] + ':', x[1]))
        parts = [names(x) + ('' if i == 0 else '') for i, (x, et) in enumerate(out)]
        ets = [et for _, et in out]
        hops = len(parts) - 1
        if len(parts) > 7:
            parts = parts[:1] + ['…'] + parts[-5:]; ets = ets[:1] + [''] + ets[-5:]
        s = parts[0]
        for p_, e_ in zip(parts[1:], ets[1:]): s += ' -%s-> %s' % (e_ if e_ else '', p_)
        return s, hops, any(e and (e.startswith('virt') or e.startswith('vt:')) for e in ets)
    rows = []
    for r in FU:
        if r['status'] != 'skip': continue
        n = (r['bin'], r['addr'])
        if n in S['level']:
            lv = S['level'][n]; pth, hops, vv = path(n)
        elif n in U['level']:
            lv, pth, hops, vv = 'N-net', '', 0, False
        else:
            lv, pth, hops, vv = 'N-dead', '', 0, False
        rows.append((lv, int(r['lines'] or 0), r, pth, hops, vv))
    order = {'Y': 0, '?': 1, 'N-net': 2, 'N-dead': 3}
    rows.sort(key=lambda x: (order[x[0]], -x[1]))
    with open(os.path.join(ROOT, 'tools', 'reach.tsv'), 'w', encoding='utf-8') as f:
        f.write('reach\tvia\tlines\tbin\taddr\tname\tclass\tpath\tskip_note\n')
        for lv, ln, r, pth, hops, vv in rows:
            f.write('\t'.join([lv, 'virt' if vv else 'direct', str(ln), r['bin'], r['addr'], r['name'], r['class'], pth, r['note'].replace('\t', ' ')]) + '\n')
    summary(rows, S, U, G, NET, dp_stats, strict_roots, loose_roots, ST, path, clsof)

# ---------- 報告 ----------
def band(a):
    return a[:4] if a[0] == '1' else a[:3] + '0'

def summary(rows, S, U, G, NET, dp_stats, sr, lr, ST, path, clsof):
    import random
    w = []
    A = w.append
    cnt = collections.Counter(); ln = collections.Counter()
    for lv, l, r, _, _, _ in rows: cnt[lv] += 1; ln[lv] += l
    A('# 可達性分析（skip 函數是否在單機會執行到）\n')
    A('由 `tools/reach.py` 產生；輸出 `tools/reach.tsv`。等級：**Y** 嚴格可達（直接呼叫／匯入／被建構類別的已被虛擬呼叫格）；**?** 只靠寬鬆邊（類別已建構但該格沒看到虛擬呼叫、資料表函式指標、DLL 未被 Poq 匯入的匯出）——依規則當成單機可達；**N-net** 只有連線程式碼（網路類別、WinSock 函數、`DAT_00942f28==2` 區塊）走得到；**N-dead** 圖上完全沒路。\n')
    A('## 統計\n')
    A('| 等級 | 函數數 | 行數 |\n|---|---|---|')
    for k in ('Y', '?', 'N-net', 'N-dead'): A('| %s | %d | %d |' % (k, cnt[k], ln[k]))
    A('| 合計 | %d | %d |' % (sum(cnt.values()), sum(ln.values())))
    reach = cnt['Y'] + cnt['?']; rl = ln['Y'] + ln['?']
    A('\n單機可達（Y+?）：**%d 個、%d 行**；不可達（N-net+N-dead）：%d 個、%d 行。\n' % (reach, rl, cnt['N-net'] + cnt['N-dead'], ln['N-net'] + ln['N-dead']))
    A('根：嚴格 %d、寬鬆 %d；資料區函式指標（嚴格=連續初始化陣列／寬鬆=其他）：%s；網路節點 %s。\n' % (len(sr), len(lr), {b: v for b, v in dp_stats.items()}, {b: len(v) for b, v in NET.items()}))
    unres = sum(f['unres'] for b in G for f in G[b]['f'].values())
    A('圖的已知缺口：函式指標呼叫 `(*(code *)x)()` 共 %d 處沒解開（只靠「FUN_x 被引用」和資料表補）；Ghidra 對 WS2_32／BNNet 匯入只顯示 `Ordinal_N`，網路函數辨識只用類別名與具名 WinSock 呼叫；MFC 訊息表只能靠資料區指標掃描（寬鬆根）；虛擬呼叫以「格號＋已建構類別」近似（沒有型別資訊）。\n' % unres)
    # 分組
    A('## 依類別／位址段的可達 skip（每組前 20）\n')
    grp = collections.defaultdict(list)
    for x in rows:
        if x[0] in ('Y', '?'):
            cl = x[2]['class'].lstrip('~').split('|')[0] or '(無類別)'
            grp[(x[2]['bin'], cl)].append(x)
    big = sorted(grp.items(), key=lambda kv: -sum(x[1] for x in kv[1]))
    A('### 類別（行數前 40 組）\n')
    for (b, cl), l in big[:40]:
        A('#### %s %s：%d 個 / %d 行\n' % (b, cl, len(l), sum(x[1] for x in l)))
        A('| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |\n|---|---|---|---|---|---|')
        for lv, ln_, r, p, h, vv in l[:20]: A('| %s | %s | %d | %s | %s | %s |' % (r['addr'], r['name'], ln_, lv, p.replace('|', '/'), r['note'][:60].replace('|', '/')))
        A('')
    A('### 位址段（每 64KB 一段，只列個數與行數）\n')
    seg = collections.defaultdict(lambda: [0, 0])
    for lv, l, r, _, _, _ in rows:
        if lv in ('Y', '?'):
            k = (r['bin'], r['addr'][:4] if r['bin'] != 'Poq.exe' else r['addr'][:4]); seg[k][0] += 1; seg[k][1] += l
    A('| bin | 位址段 | 個數 | 行數 |\n|---|---|---|---|')
    for (b, k), (c, l) in sorted(seg.items(), key=lambda kv: -kv[1][1])[:40]: A('| %s | %s0000.. | %d | %d |' % (b, k, c, l))
    A('')
    # 原因統計
    A('### 可達 skip 的原始 skip 原因（前 25 種）\n')
    rc = collections.Counter(); rlines = collections.Counter()
    for lv, l, r, _, _, _ in rows:
        if lv in ('Y', '?'):
            k = re.sub(r'[0-9a-fx]{4,}', 'N', r['note'])[:50]; rc[k] += 1; rlines[k] += l
    A('| 個數 | 行數 | 原因 |\n|---|---|---|')
    for k, c in rc.most_common(25): A('| %d | %d | %s |' % (c, rlines[k], k.replace('|', '/')))
    A('')
    # 遊戲規則候選與移植批次
    ex = re.compile('解構|平台層|函式庫|STL|樣板|工具箱|連線版|nlohmann|建構|工具函式|空函式|UIWndEx|死碼')
    cand = [x for x in rows if x[0] in ('Y', '?') and not ex.search(x[2]['note'])]
    A('## 單機可達且「不是明顯平台／解構／容器」的 skip（需要逐一判斷是否有遊戲規則）\n')
    A('共 %d 個、%d 行（可達 skip 的其餘 %d 個多為解構、平台層、容器／STL、UI 樣板，仍可達但多半不含規則，解構要看有沒有副作用）。\n' % (len(cand), sum(x[1] for x in cand), sum(1 for x in rows if x[0] in ('Y', '?')) - len(cand)))
    A('已讀本體（`tools/fn.py --brief`）確認：')
    A('- Poq 0x4774d0（2110 行）：`#`／`$` 文字命令接收（#ObjSpeed 等），`$` 轉給已 done 的 0x47d4e0；可達（聊天輸入），屬開發／GM 命令，不是劇情分派（劇情分派在別處）。')
    A('- EasyRPG 0x10030c90（408 行）：屬性變動重算，Str／Con／Dex／Wisdom 的相依順序，`DAT_10478d40==0` 時走裝備特性清單 → 明顯遊戲規則，單機可達（Y，直接呼叫）。')
    A('- EasyRPG 0x10057f50／0x10056120：隊友升級後 Upgrade_ 屬性重算（HP_MAX／MP_MAX 隨等級），解析 "x y z" 字串三個浮點 → 遊戲規則，Y。')
    A('- EasyRPG 0x1003d890：選項 `Chosen` 紀錄與迴圈 → 對話選項規則，Y；0x1003f7d0：依房間編號找房名（開場房間查找），Y，原註記「引擎有 story_start_room」，需確認是否等價。')
    A('- 其餘「同步封包送出器」「說明文字」「物件表／容器」等依註記判斷屬單機直接讀共用物件或顯示，但**註記是宣稱，不是證據**；可達性分析證明它們在原版單機會被執行，是否等價需逐批對照。\n')
    A('### 可達候選前 60（按行數）\n')
    A('| 位址 | bin | 行 | 等級 | via | 類別 | skip 原因 |\n|---|---|---|---|---|---|---|')
    for lv, l, r, p_, h, vv in cand[:60]: A('| %s | %s | %d | %s | %s | %s | %s |' % (r['addr'], r['bin'][:4], l, lv, 'virt' if vv else 'direct', r['class'][:16], r['note'][:70].replace('|', '/')))
    A('')
    A('## 建議移植批次（候選依 bin、位址排序切段，每批約 45 個，位址相近＝同檔／同類別）\n')
    bykey = collections.defaultdict(list)
    for x in cand: bykey[x[2]['bin']].append(x)
    bn = 0; btab = []
    for b in BINS:
        l = sorted(bykey[b], key=lambda x: x[2]['addr'])
        # 巨大函數（>600 行）單獨成批，其餘每 45 個一批
        cur = []
        def flush():
            nonlocal cur, bn
            if cur:
                bn += 1; btab.append((bn, b, cur[0][2]['addr'], cur[-1][2]['addr'], len(cur), sum(x[1] for x in cur), collections.Counter(re.sub(r'[0-9a-fx]{4,}', 'N', x[2]['note'])[:22] for x in cur).most_common(1)[0][0]))
            cur = []
        for x in l:
            if x[1] > 600:
                flush(); cur = [x]; flush(); continue
            cur.append(x)
            if len(cur) >= 45: flush()
        flush()
    A('| 批 | bin | 位址範圍 | 個數 | 行數 | 主要註記 |\n|---|---|---|---|---|---|')
    for t in btab: A('| %d | %s | %s–%s | %d | %d | %s |' % (t[0], t[1], t[2], t[3], t[4], t[5], t[6].replace('|', '/')))
    A('\n（單獨成批者為超過 600 行的大函數。批內相依以位址鄰近近似，沒有再做呼叫圖聚類。）\n')
    open(os.path.join(ROOT, 'tools', 'reach_batches.tmp'), 'w').write(str(len(btab)))
    # 驗證
    A('## 驗證\n')
    random.seed(1)
    done = [n for n, r in ST.items() if r['status'] == 'done' and n[1] in G[n[0]]['f']]
    samp = random.sample(done, 20)
    ok = sum(1 for n in samp if n in S['level'])
    okY = sum(1 for n in samp if S['level'].get(n) == 'Y')
    A('已 done 抽樣 20 個（seed=1）：判成可達 %d/20（其中嚴格 Y %d）。' % (ok, okY))
    miss = [n for n in samp if n not in S['level']]
    for n in miss: A('- 漏判：%s:%s %s（%s）' % (n[0], n[1], G[n[0]]['f'][n[1]]['name'], ST[n]['class']))
    alld = [n for n in done]
    okall = sum(1 for n in alld if n in S['level'])
    A('全部 done（%d 個）判成可達 %d 個（%.1f%%）。' % (len(alld), okall, 100.0 * okall / max(1, len(alld))))
    netc = [(r['bin'], r['addr']) for r in ST.values() if re.match(r'^~?(CNet\w*|CServerSocket|CClientSocket|CSocketItem)(\||$)', r['class']) and r['addr'] in G[r['bin']]['f']]
    random.seed(2)
    ns = random.sample(netc, min(20, len(netc)))
    okn = sum(1 for n in ns if n not in S['level'])
    A('\n連線類別（CNet*／CServerSocket／CClientSocket／CSocketItem，共 %d 個）抽樣 %d 個：判成不可達 %d。' % (len(netc), len(ns), okn))
    for n in ns:
        if n in S['level']: A('- 誤判可達：%s:%s %s（%s）路徑 %s' % (n[0], n[1], G[n[0]]['f'][n[1]]['name'], ST[n]['class'], path(n)[0]))
    allnet_ok = sum(1 for n in netc if n not in S['level'])
    A('其中在「不擋連線」的全圖裡有路可達的：%d/%d（代表圖本身連得到它們，被擋是因為只經過連線節點）。注意：連線類別本來就是封鎖種子，這項只驗證封鎖有生效、沒有從別的路漏進單機，不是獨立證據。' % (sum(1 for n in netc if n in U['level']), len(netc)))
    A('全部連線類別函數判成不可達 %d/%d。\n' % (allnet_ok, len(netc)))
    A('誠實說明：done 全部都被判成可達的比例就是覆蓋率下限的估計；漏判的 done 函數代表圖有缺口（虛擬呼叫、MFC 訊息表等），skip 的 N 結果因此只能視為「圖上沒路」而非證明。N-dead 的函數須再用 `tools/fn.py` 看本體或查資料區才能放心略過。\n')
    open(os.path.join(ROOT, 'tools', 'reach_summary.md'), 'w', encoding='utf-8').write('\n'.join(w))

if __name__ == '__main__':
    main()
