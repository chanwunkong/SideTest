#!/bin/sh
# 連續跑 N 輪 Codex 移植（tools/CODEX_TASK.md），每輪測試過就 commit + push；失敗就停。
# 只印給審查用的短摘要：每輪的回報、改了哪些檔、測試最後一行。
n=${1:-1}
extra=$2  # 只給第 1 輪的額外指示
top=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) || exit 1
cd "$top/QinPort" || exit 1
i=0
while [ $i -lt "$n" ]; do
  i=$((i + 1))
  log=/tmp/qinport_codex_$i.log rep=/tmp/qinport_codex_report_$i.md
  { [ $i -eq 1 ] && [ -n "$extra" ] && printf "%s\n\n" "$extra"; cat tools/CODEX_TASK.md; } |
    codex exec -C . --sandbox workspace-write -o "$rep" - > "$log" 2>&1
  echo "== 第 $i 輪 codex exit $?（紀錄 $log）"
  cat "$rep"
  git -C "$top" diff --stat -- QinPort | tail -1
  t=$(cd engine && make >/dev/null 2>&1 && make skilltest 2>&1 | tail -1)
  echo "== test: $t"
  if [ "$t" != "skilltest ok" ] || git -C "$top" diff --quiet -- QinPort/HANDOFF.md; then
    echo "== 停：測試沒過或 HANDOFF 沒更新，留給 Claude 看"; exit 1
  fi
  git -C "$top" add -A ':(icase)qinport' ':(icase,exclude)qinport/tools/decomp_strings.py'
  msg=$(grep -m1 -E '範圍|本批' "$rep" | sed -E 's/^[- ]*(範圍|本批)：*//' | tr -d '`')
  git -C "$top" commit -q -m "chore(qinport): port ${msg:-codex batch}" -m "Ported by Codex from tools/CODEX_TASK.md; reviewed by test run.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" || exit 1
  for d in 2 4 8 16; do git -C "$top" push -q origin qinport && break; sleep $d; done
done
