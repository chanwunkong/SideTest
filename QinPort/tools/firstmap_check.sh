#!/bin/sh
# 第一張地圖（赵村）瀏覽器檢查：sh tools/firstmap_check.sh http://127.0.0.1:8651/   （要先 make web 並用 webserve.py 開起來；只用 headless Chrome + node）
U=${1:-http://127.0.0.1:8651/}; D=$(dirname "$0"); O=/tmp/firstmap; bad=0; mkdir -p $O
chk() { if grep -aq "$2" "$O/$1.log"; then echo "ok   $3"; else echo "FAIL $3"; bad=1; fi; }
# 1. 走近 NPC 自動說話：站在老者旁 → autotalk → 劇情對話開啟（修前主角永遠帶「无对话」狀態，連點 NPC 都說不了話）
CDP_PORT=9411 node $D/webdrive_cells.mjs "$U?scene=zhaocun1&at=44,107&debug=trace" 640 480 $O/talk "wait:5000" >/dev/null 2>&1
chk talk "autotalk oldman_a" "走近老者自動觸發"; chk talk "talk oldman_a npc=[1-9]" "劇情對話開啟"
# 2. 關掉輔助（?autotalk=0）不觸發
CDP_PORT=9412 node $D/webdrive_cells.mjs "$U?scene=zhaocun1&at=44,107&debug=trace&autotalk=0" 640 480 $O/off "wait:5000" >/dev/null 2>&1
if grep -aq "autotalk oldman_a" $O/off.log; then echo "FAIL autotalk=0 仍觸發"; bad=1; else echo "ok   autotalk=0 不觸發"; fi
# 3. 新遊戲：創角（遊俠）→ 難度 → 開場日誌「重返咸阳」（亡命天涯）
CDP_PORT=9413 node $D/webdrive_cells.mjs "$U" 640 480 $O/new "wait:2000;tap:170,106;wait:500;tap:320,297;wait:3000;tap:320,240;wait:4000;tap:320,100;wait:3000" >/dev/null 2>&1
chk new "過場 pct_wmtya 開始" "新遊戲開場過場在片頭動畫場景 ptdhcj 播出"; chk new "重返京城" "過場跳過後出現開場任務日誌"
# 4. 出口：踩 go eastsouth → 阳周城（過場、孔刚入隊）
NOCLICK=1 CDP_PORT=9414 node $D/webdrive_cells.mjs "$U?scene=zhaocun1&warp=1" 640 480 $O/exit "wait:34000" >/dev/null 2>&1  # 過場約 14 秒，結束後主角被搬進衙府（換載 yzh_yafu）
chk exit "孔刚 入隊" "離開赵村進入阳周城、過場播完孔刚入隊"
[ $bad = 0 ] && echo "firstmap ok" || { echo "firstmap FAILED"; exit 1; }
