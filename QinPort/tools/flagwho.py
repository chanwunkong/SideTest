#!/usr/bin/env python3
"""誰設／誰讀某個旗標：python3 -I tools/flagwho.py 旗標名…（story/ 內 "旗標" 字串後面最近的 get/set/new/find_flag 呼叫與所在格）。
顯示：檔案 第N格 動作[值]。set 的值取該呼叫前的 uVarN = 值。"""
import os, re, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'story')
names = sys.argv[1:]
for dp, _, fs in os.walk(root):
    for f in sorted(fs):
        p = os.path.join(dp, f)
        t = open(p, encoding='utf-8', errors='ignore').read()
        for part in re.split(r'(?m)^// ---- 第 ', t)[1:]:
            slot = re.match(r'(\d+)', part).group(1)
            for nm in names:
                for m in re.finditer(r'"' + re.escape(nm) + r'"', part):
                    tail = part[m.end():m.end() + 700]
                    c = re.search(r'\b(get_flag|set_flag|new_flag|find_flag)\(', tail)
                    act = c.group(1) if c else '?'
                    val = ''
                    if act == 'set_flag':
                        seg = part[max(0, m.start() - 400):m.end() + 400]
                        v = re.search(r'set_flag\([^,]+,[^,]+,\s*(\w+)\)', tail)
                        if v:
                            vv = re.search(re.escape(v.group(1)) + r'\s*=\s*(-?\w+);', part[:m.end() + 700][::-1][::-1][max(0,m.start()-300):m.end()+700])
                            val = '=' + (vv.group(1) if vv else v.group(1))
                    cmp = ''
                    if act == 'get_flag':
                        cm = re.search(r'(?:iVar\d|uVar\d|local_\w+)\s*(==|!=|<|>|<=|>=)\s*(-?\w+)|\(?(-?\d+)\s*<\s*(?:iVar\d)', tail[:400])
                        cmp = ' ' + cm.group(0) if cm else ''
                    print(f'{nm}\t{os.path.relpath(p, root)}\t第{slot}格\t{act}{val}{cmp}')
