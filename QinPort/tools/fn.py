#!/usr/bin/env python3
"""fn.py ADDR... → 只印出那幾個原版函數的反編譯本體（去掉例外處理雜訊）。
第一次跑會在 decomp/ 建位址索引（.fnidx.json），之後直接跳到檔案位置讀，不掃整個檔。
位址可帶 bin 前綴：EasyRPG.dll:1001a2b0；不帶就依 Poq.exe、EasyRPG.dll、UIWndEx.dll 順序找。
--brief：再刪掉 security cookie、區域變數宣告和 GPOINT 解構、縮短 ATL/STL 樣板名、去掉 thunk_，
並用 tools/funcs_done.txt、base_map.tsv 把已知函數換成 ⟨我們的名字⟩，
用 fields.tsv 把 this/param_1 的已知偏移換成 this->⟨欄位⟩（只在 FUNCS.tsv 確定類別、且偏移沒有歧義時）。"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINS = ['Poq.exe', 'EasyRPG.dll', 'UIWndEx.dll']
NOISE = re.compile(r'.*(ExceptionList|puStack_c|local_8 = |/\* WARNING).*\n')


def index():
    p = os.path.join(ROOT, 'decomp', '.fnidx.json')
    srcs = [os.path.join(ROOT, 'decomp', b + '.c') for b in BINS]
    if os.path.exists(p) and all(os.path.getmtime(p) >= os.path.getmtime(s) for s in srcs):
        return json.load(open(p))
    idx = {}
    for b, s in zip(BINS, srcs):
        d, pos = {}, 0
        with open(s, 'rb') as f:
            for line in f:
                if line.startswith(b'// ==== '):
                    d[line.split()[2].decode()] = pos
                pos += len(line)
        idx[b] = d
    json.dump(idx, open(p, 'w'))
    return idx


def body(b, off):
    out = []
    with open(os.path.join(ROOT, 'decomp', b + '.c'), 'rb') as f:
        f.seek(off)
        for i, line in enumerate(f):
            if i and line.startswith(b'// ==== '):
                break
            out.append(line.decode('utf-8', 'replace'))
    return re.sub(r'\n\s*\n', '\n', NOISE.sub('', ''.join(out)))


TEMPLATES = [
    (r'(ATL::)?CStringT<char,(class_)?StrTraitMFC_DLL<char,(class_)?ATL::ChTraitsCRT<char>_>_>', 'CString'),
    (r'(std::)?basic_string<char,(struct_)?std::char_traits<char>,(class_)?std::allocator<char>_>', 'std::string'),
    (r'(std::)?basic_(\w+)<char,(struct_)?std::char_traits<char>_>', r'std::\2'),
    (r'(ATL::)?CSimpleArray<([^,<>]+),ATL::CSimpleArrayEqualHelper<\2>_>', r'CSimpleArray<\2>'),
    (r'_Tree_unchecked_const_iterator<(class_)?std::_Tree_val<(struct_)?std::_Tree_simple_types<unsigned_int>_>,'
     r'(struct_)?std::_Iterator_base0>', 'set<uint>::const_iterator'),
]
COOKIE = r'\w+ \^ (\(int\)register0x00000010 - 4U|\(uint\)\(?&?\w+( \+ 0x[0-9a-f]+)?\)?)'
DECL = re.compile(r'  (?!(return|goto|delete|throw|case)\b)[\w:<>,* ]+? \**\w+( \[\d+\])?;')


def tsv(name):
    p = os.path.join(ROOT, 'tools', name)
    for line in open(p, encoding='utf-8') if os.path.exists(p) else ():
        if line.strip() and not line.startswith('#'):
            yield line.rstrip('\n')


def known():
    """(位址 → 我們的名字, (類別, 偏移) → 欄位名, 位址 → FUNCS.tsv 確定的類別清單)。"""
    names = {}
    for line in tsv('base_map.tsv'):
        k, _, why = line.partition('\t')
        if re.fullmatch(r'[0-9a-f]{8}', k):
            names[k] = why
    for line in tsv('funcs_done.txt'):
        line = line.split('#', 1)[0].strip()
        a, _, why = line.partition(' ')
        if a:
            names[a.lower().zfill(8)] = why.strip()
    fields = {}
    for line in tsv('fields.tsv'):
        c, off, mean, ours = (line.split('\t') + ['', '', ''])[:4]
        name = ours if ours and ours != '-' and not ours.startswith('（') else mean.split('（')[0].strip()
        for o in re.split(r'[／/]', off):
            try:
                fields.setdefault((c, int(o.lstrip('+'), 16)), set()).add(name)
            except ValueError:
                pass
    cls = {}
    p = os.path.join(ROOT, 'FUNCS.tsv')
    for line in open(p, encoding='utf-8') if os.path.exists(p) else ():
        r = line.split('\t')
        if r[4] and not r[4].startswith('~'):
            cls[r[2]] = r[4].split('|')
    return names, fields, cls


def brief(text, a, kn):
    names, fields, cls = kn
    lines = text.split('\n')
    i = next((k for k, l in enumerate(lines) if l == '{'), len(lines)) + 1
    while i < len(lines) and DECL.fullmatch(lines[i]):
        del lines[i]
    text = '\n'.join(lines)
    ck = re.search(r'.*\b(\w+) = (?=DAT_)' + COOKIE + r';\n', text)
    if ck:  # Ghidra 有時把留在暫存器的 cookie 當參數傳下去：剩下的用法改名 cookie
        text = re.sub(r'\b%s\b' % ck.group(1), 'cookie', text.replace(ck.group(0), ''))
        text = re.sub(r'.*\b\w+ = cookie;\n', '', text)
    text = re.sub(r'.*\b[\w:]+\(' + COOKIE + r'[,)].*;\n', '', text)
    text = re.sub(r'.*\bGPOINT::~GPOINT\(.*;\n', '', text)  # GPOINT 是兩個 int，解構什麼都不做
    for k, v in TEMPLATES:
        text = re.sub(k, v, text)
    text = text.replace('thunk_FUN_', 'FUN_')
    text = re.sub(r'\bFUN_([0-9a-f]{8})\b(?=\s*\()',
                  lambda m: '⟨%s⟩' % names[m.group(1)] if m.group(1) in names else m.group(0), text)

    sig = text[:text.find('{')]
    base = ('this', 'param_1') if '__thiscall' in sig else ('param_1',) if '__fastcall' in sig else ()

    def field(m):
        if m.group(2) not in base:
            return m.group(0)
        off = int(m.group(3), 0)
        got = set().union(*(fields.get((c, off), set()) for c in cls.get(a, [])))
        if len(got) != 1:  # 沒記錄或同偏移有兩種意思：保留原樣
            return m.group(0)
        t = re.sub(r'\*\((.*?) \*\)', r'\1', m.group(1) or '')
        t = '' if not m.group(1) or t in ('int', 'uint', 'undefined4') else ':' + t  # 不是 4 位元組才標型別
        return ('' if m.group(1) else '&') + m.group(2) + '->⟨%s%s⟩' % (got.pop(), t)
    return re.sub(r'(\*\([\w ]+\*\))?\((this|param_1) \+ (0x[0-9a-f]+|\d+)\)', field, text)


def main():
    idx = index()
    args = [a for a in sys.argv[1:] if a != '--brief']
    kn = known() if len(args) < len(sys.argv) - 1 else None
    for a in args:
        b, _, a = a.rpartition(':')
        a = a.lower().removeprefix('0x').zfill(8)
        for bb in ([b] if b else BINS):
            if a in idx[bb]:
                t = body(bb, idx[bb][a])
                print(brief(t, a, kn) if kn else t)
                break
        else:
            print('// %s 找不到' % a)


if __name__ == '__main__':
    main()
