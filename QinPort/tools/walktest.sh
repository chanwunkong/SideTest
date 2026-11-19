#!/bin/sh
# 主線走查回歸：engine/test/walk/ch*.txt 與 open17.txt 每個都要印 walk ok（先 cd engine && make storytest）
# explore.txt 是可達性探索報告，不算回歸。腳本語法見 engine/src/story/storytest.c 的 walk_test
D=$(cd "$(dirname "$0")/.." && pwd)
bad=0
for f in "$D"/engine/test/walk/ch*.txt "$D"/engine/test/walk/open17.txt; do
  if sh "$D/tools/walk.sh" "$f" | tail -1 | grep -q "walk ok"; then echo "ok   $(basename "$f")"; else echo "FAIL $(basename "$f")"; bad=1; fi
done
[ $bad = 0 ] && echo "walktest ok" || { echo "walktest FAILED"; exit 1; }
