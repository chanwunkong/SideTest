# QinPort 完整移植佇列（接力用，額度中斷後照這份繼續，不要問使用者）

目標：做出完整可玩的《秦殤》（含非劇情模式）。使用者明確要求：不接受「自行判斷略過」「簡化版當完成」。只有 tools/reach.tsv 證明「只能從連線模式到達」的函數才可以不移植。查不到原版規則時，回報裡寫「原版規則沒找到」並列出函數，不要編造。

## 每一批的做法（Sonnet 子代理，model=sonnet，主樹一次只能一個）
1. 派 general-purpose 子代理，prompt 要包含：照 tools/CODEX_TASK.md 的規則；不要 git commit／push；網頁版只在 /tmp 複本編（`rsync -a --exclude game --exclude decomp --exclude extracted ../ /tmp/qbN/qinport/`），不動 qinport/web/ 與 8642／8643 伺服器；驗證 `cd engine && make && make skilltest 2>&1 | tail -3` 印 skilltest ok、`make web` 能編、動到 story 就跑 `make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1`；更新 HANDOFF／roles／fields／FUNCS（`python3 tools/functool.py`）。
2. 子代理回報後我（主對話）重跑 make／skilltest／storytest，通過才 `git add -A ':(icase)qinport' ':(icase,exclude)qinport/tools/decomp_strings.py'` commit（結尾 Co-Authored-By 行照系統提示）並 push origin qinport。
3. 更新給使用者試玩的穩定版：`rsync` 複本 → `cd /tmp/qbuild/qinport/engine && make web` → 把 web/ 的 index.html index.js index.wasm sw.js manifest.webmanifest *.png 複製到 /tmp/qin-web/（8643 伺服器：`python3 tools/webserve.py /tmp/qin-web game 100.83.41.116 8643`，沒在跑就重開）。
4. 立刻派下一批，不等使用者。

## 佇列（依序；做完一項在這裡劃掉並寫 commit）
- [ ] 世界物件互動：砍樹／挖礦／打水缸（BNT 類型 4，原版點擊命令 0x80 打、0x85 開、0x96 門 0x10、0x17→0x11 撿）、點門走到門點（代理 a2c2b57 進行中；回報後驗證 commit）
- [ ] EasyRPG 規則函數：0x10030c90（屬性重算 408 行）、0x10057f50（隊友升級屬性）、0x1003d890（對話選項 Chosen）
- [ ] Poq「介面→本機伺服器訊息層」324 個（約 4 萬行，tools/reach.tsv 的 skip_note 含「介面→本機伺服器」）：逐個與引擎現有實作比對是否等價，不等價照原版改；原先「引擎已有替代」的宣稱都要驗證
- [ ] 0x50c390（2409 行）、0x4774d0（2110 行，`#`／`$` 文字命令接收：GM／除錯／聊天指令，單機可用的要做）
- [ ] tools/reach.tsv 其餘「單機可達（Y 或 ?）」skip（約 1535 個、9.8 萬行，扣掉純解構／平台層／容器後），依 reach_summary.md 的 48 批切法，每批約 45 個
- [ ] 補件：tools/reach.tsv 沒解開的 174 處函式指標呼叫與 Y 偏寬的虛擬呼叫，重跑 `python3 tools/reach.py`
- [ ] 之前列的缺口：技能打造（扣精氣 (等級+10)×5、五行換算 0x10058900、m_pProductFinish、m_pMake／m_pEnchase／m_LookOver）、商店修理／鑑定、聲望與五行攻防、六項屬性加值回算、超重速度懲罰、分身換裝、色表／混色／光影模式、章節配音與音效、設定視窗項目生效、標題畫面、小地圖校對、遮擋排序、遊戲速度、劇情命令分派其餘 case（# 6/7/8/9、@ 5/8/10/11/12）、隊友裝備外觀頭像、死亡介面觸發點、dead_obj 缺口
- [ ] 之前「劇情沒用到」清單：天氣疾風與色調、銀行、丟錢、開鎖偷竊、遊戲時鐘設定、CZhenMng、CGroup、錄影回放、除錯指令
- [ ] 使用者延後：片頭影片卡頓（之後處理）

## 狀態備忘
- 穩定版試玩網址：http://100.83.41.116:8643/（Tailscale）；PWA 完整離線快取需要 https（Tailscale serve），步驟在 HANDOFF。
- 測試瀏覽器：用已裝的 Google Chrome 經 CDP（tools/webdrive_example.mjs、webdrive_cells.mjs、firstmap_check.sh、pwa_check.mjs）；不要安裝任何東西（Playwright 安裝被分類器擋過）。
- 被分類器擋過：npm／playwright 安裝。funcs_skip.txt 的編輯曾被擋，使用者授權後可改。
- 使用者要求每批結果誠實回報：哪些只靠編譯／單元測試、沒在實機驗證。
