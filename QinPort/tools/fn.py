#!/usr/bin/env python3
"""fn.py ADDR... → 只印出那幾個原版函數的反編譯本體（去掉例外處理雜訊）。
第一次跑會在 decomp/ 建位址索引（.fnidx.json），之後直接跳到檔案位置讀，不掃整個檔。
位址可帶 bin 前綴：EasyRPG.dll:1001a2b0；不帶就依 Poq.exe、EasyRPG.dll、UIWndEx.dll 順序找。"""
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


def main():
    idx = index()
    for a in sys.argv[1:]:
        b, _, a = a.rpartition(':')
        a = a.lower().removeprefix('0x').zfill(8)
        for bb in ([b] if b else BINS):
            if a in idx[bb]:
                print(body(bb, idx[bb][a]))
                break
        else:
            print('// %s 找不到' % a)


if __name__ == '__main__':
    main()
