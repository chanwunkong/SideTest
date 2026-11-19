# QinPort 完整移植佇列（接力用，額度中斷後照這份繼續，不要問使用者）

目標：做出完整可玩的《秦殤》（含非劇情模式）。使用者明確要求：不接受「自行判斷略過」「簡化版當完成」。只有 tools/reach.tsv 證明「只能從連線模式到達」的函數才可以不移植。查不到原版規則時，回報裡寫「原版規則沒找到」並列出函數，不要編造。

## 新策略：資料驅動補齊，函數層級只用來查原版規則（2026-11-19）
逐個函數移植太慢（單機可達 skip 約 1000 個）。改成：`python3 -I tools/coverage.py` 從資料（Data／ObjMdl／scene／Rec／story）統計「遊戲真的會出現的機制」，輸出 tools/coverage.md（各項資料次數＋引擎 完整／部分／無／?）。
只補矩陣裡「無」「部分」，依資料次數由多到少；需要原版規則時才用 `python3 tools/fn.py --brief`／`r2` 查函數。改了引擎就更新 coverage.py 開頭的 S[...] 狀態表並重跑。
**本輪（資料驅動第 1 輪）補了**：對話語音 `audio_voice`（Rec '@' 4，game/sound/NNN.wav，919/935 個鍵有檔）、Rec '@' 11 音效、BNT 觸發區 `openall <別名>`（shhl_neicheng 約 90 處，原本只認 s_pctstart）。
**矩陣裡尚未補的（依優先序；資料驅動第 2 輪 2026-11-19 已劃掉做完的）**：
1. ~~Rec '#' 7 建築狀態~~：做了（編號 − 2001 = BNT 順序，0x4a2600 配號），標「部分」——同場景第 2 次進入偏移差 1、地上道具也佔編號，原版規則沒找到。
2. ~~Rec '@' 4／6 對話框~~：做了（原版就是頭頂文字框 0x4722f0／0x5963e0，沒有說話者名與頭像；語音播完才收；框色字色沒查到）。
3. ~~'#' 1 子類 1、'#' 3 子碼、'@' 8／10／12~~：做了。'#' 6（地上道具）資料封包比原版讀的欄位短，原版規則沒找到；'#' 8／9 要 MethodMain 出手動畫（0x43acb0／0x43af50／0x43c4f0／0x450700 的 CARROW 等），**還沒做**。
4. Attack 11 分身 0x1003df00（複製施放者裝備子物件與技能，技能等級上限 9 → 0x10037d70；原版規則已讀，缺「分身有自己的技能列」模型）；偷竊／開鎖 0x10034010／0x10034900（Attack 5／6，資料 0 次）；銀行 0x1003ac40；丟錢 0x1003b640／0x10054830；0x1003de10、0x10035540、0x10038240、SetSkill。
5. 章節配音 `sound\C%.3d.wav`（Poq 0x566920／0x566860，c011…）、天氣疾風與色調、CZhenMng、CGroup、錄影回放除錯指令；Rec 多場景（#13 子類 2 換場景）原版載另一份場景拷貝，這裡只標 foreign 擋建築封包，角色封包還是畫在原房間。
6. 矩陣 `?` 還剩 19 項：狀態表 9 欄（temp、全身变色、不发送到Client、缩放、旋转、幻影移动、武器变色、幻影攻击、头变色）是否逐欄消費、技能死亡方式 8 種（actor_kill 固定 mode 1，原版把技能欄 12 傳給 0x486910 的 mode 沒查）、參數 69、屬性 40、地圖視窗 JM60 用 "Map" 遮罩只顯示去過的地點（site 序號 ↔ Map 號對照沒查）。

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
