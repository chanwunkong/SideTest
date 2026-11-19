#!/bin/sh
# 主線走查：sh tools/walk.sh <腳本檔>（先 cd engine && make storytest）。
# 腳本的非 ASCII 轉成 %XX 再餵給 storytest（node 的標準輸入會弄壞部分位元組）；腳本裡的 `use_triggers` 換成 tools/triggers.tsv 的 s_pctstart（trig 場景 劇情路徑）
cd "$(dirname "$0")/.." && python3 -I -c "
import sys
t = open(sys.argv[1], encoding='utf-8').read()
if 'use_triggers' in t:
    tr = ''.join('trig %s %s\n' % (c[0], c[2]) for c in (l.split('\t') for l in open('tools/triggers.tsv', encoding='utf-8') if not l.startswith('#')) if c[1] == 's_pctstart')
    t = t.replace('use_triggers', tr)
sys.stdout.write(''.join('%%%02x' % b if b > 127 else chr(b) for b in t.encode('utf-8')))" "$1" | (cd engine && node build/storytest.js walk=- 2>&1) | grep -v "newpersonex\|没有找到物体类型"
