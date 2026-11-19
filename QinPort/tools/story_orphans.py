#!/usr/bin/env python3
"""找 story/ 裡「第 18 格以後定義了、卻沒有任何選項指到」的格子（選項包裝 X_sN_w 在檔案裡只出現 3 次：宣告、定義、登記表）。
這些格子在原版只有虛擬函式表引用，沒有呼叫點（孔刚 s19／s22 用位元組搜尋 EasyRPG.dll 確認過）。用法：python3 -I tools/story_orphans.py"""
import os, re
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'story')
n = tot = 0
for k in ('npc', 'plot', 'room', 'item'):
    for dp, _, fs in os.walk(os.path.join(root, k)):
        for f in sorted(fs):
            t = open(os.path.join(dp, f), encoding='utf-8', errors='ignore').read()
            orph = []
            for m in re.finditer(r'static intptr_t (\w+_s(\d+)_w)\(intptr_t \*a\) \{', t):
                if int(m.group(2)) < 18: continue
                tot += 1
                if len(re.findall(r'\b' + m.group(1) + r'\b', t)) <= 3: orph.append(int(m.group(2)))
            if orph:
                n += len(orph)
                print(os.path.relpath(os.path.join(dp, f), root), orph)
print(f'{n} / {tot}')
