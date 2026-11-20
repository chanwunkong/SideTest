# 交給另一個帳號／另一台機器：QinPort 接手說明（2026-11-20）

給接手的 Claude（或人）。所有程式與文件都在 GitHub：`https://github.com/chanwunkong/SideTest`，分支 **`qinport`**（HEAD 2a6b96a 之後的最新 commit；工作樹乾淨，沒有未推送的東西）。磁碟上資料夾叫 `qinport`，git 記錄的是 `QinPort`；新增檔案請用 `git add -f QinPort/…` 或 `git add -A ':(icase)qinport'`。

## 1. 專案是什麼
《秦殤》（Prince of Qin，2002）的**忠實原生重製引擎**：C + SDL2，目標是**手機網頁版**（Emscripten，直式／橫式都能玩，觸控）。照原版程式（Ghidra 反編譯）翻，不自己設計。使用者明確要求：**完整復刻（含非劇情自由模式）、不接受簡化版當完成、不接受自行判斷略過、查不到原版規則時明講「原版規則沒找到」並列證據；每批結果誠實回報哪些只靠編譯／單元測試、沒在實機驗證。** 動畫、圖片、音效一律用原版的。遊戲資源只讀使用者自己的正版檔，絕不進 git、絕不公開。

## 2. 不在 git 裡、接手要自己準備的東西（本機資產，約 2.3GB）
| 路徑 | 怎麼來 |
|---|---|
| `qinport/game/`（2.2GB） | 使用者自己的 Steam 正版，SteamCMD App **970500**（Windows 版）下載到這裡 |
| `qinport/decomp/`（Poq.exe.c、EasyRPG.dll.c、vtables） | `HANDOFF.md`「建置與測試」那段的 Ghidra headless 指令（`tools/ghidra/DumpC.java`、`ParamId.java`、`DumpVtables.java`），跑完再 `python3 tools/decomp_strings.py` 把 GBK 字串換回中文 |
| `qinport/story/` | `python3 tools/storytool.py`（從 EasyRPG.dll 批次抽出劇情檔） |
| `qinport/extracted/`（除錯用的圖） | 可不要 |
依賴（Homebrew）：`sdl2`、`emscripten`（emcc）、`ghidra`、`radare2`（`r2`，Ghidra 反編譯會丟回傳值，查原版回傳值要用 `r2 -q -e scr.color=0 -c "s 0x…; pd 60" game/EasyRPG.dll`）、`ffmpeg`（片頭影片轉檔）、`node`、`python3`；測試瀏覽器是**已安裝的 Google Chrome**（經 CDP，見 `tools/webdrive_example.mjs`）。**不要裝 Playwright／npm 套件**——在 Claude Code 自動模式下被分類器擋過。

## 3. 開發與測試（照 `tools/PORT_QUEUE.md`「每一批的做法」）
```
cd qinport/engine
make && make skilltest 2>&1 | tail -3        # 要印 skilltest ok
make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1   # 劇情執行（32 位元 wasm）正常印對白
make walktest                                  # 主線走查回歸，要印 walktest ok
make web                                       # 網頁版（Emscripten）→ ../web/
```
- 網頁版**不要直接在 `qinport/web/` 上做實驗**；用複本：`rsync -a --exclude game --exclude decomp --exclude extracted --exclude .git ./ /tmp/qbN/qinport/`，在複本 `make web`，再 `python3 tools/webserve.py /tmp/qbN/qinport/web game 127.0.0.1 <port>` 本機測。
- 試玩伺服器（Tailscale，只綁 Tailscale 位址，遊戲檔不公開）：穩定版是一份**固定的複本** `/tmp/qin-web`（index.html index.js index.wasm sw.js manifest.webmanifest *.png），`python3 tools/webserve.py /tmp/qin-web game <tailscale-ip> 8643`。**更新時要一次複製完三個主檔**（手機若用到新舊混搭的 index.js／index.wasm 會出「ASM_CONSTS … is not a function」；使用者那次是手機端舊快取，清網站資料就好）。
- 自動化瀏覽器驗證：`tools/webdrive_cells.mjs`（`mdrag:`、`click:`、`key:`、`vk:`、`NOCLICK=1`；每次用新的 `CDP_PORT`）、`firstmap_check.sh`、`pwa_check.mjs`、`walk.sh`、`webtrig.mjs`。測完記得殺掉殘留的 headless Chrome（`pkill -f remote-debugging-port=`）。
- 每批做完：`python3 tools/functool.py` 重產 `FUNCS.tsv`，更新 `HANDOFF.md`（新增一節）、`tools/PORT_QUEUE.md`、`roles.tsv`、`fields.tsv`、`funcs_done.txt`／`funcs_skip.txt`。commit 訊息 `feat|fix|chore(qinport): …`，push 到 `origin qinport`。

## 4. 目前進度（真實數字，不要信舊報告）
- `FUNCS.tsv`：`done` 約 1,200、`skip` 約 5,900、`todo` 0。**skip 不等於不用做**：`tools/reach.py` 的可達性分析（`tools/reach.tsv`、`reach_summary.md`）證明約 4,950 個 skip 在單機是「可達」的。之後改走**資料驅動**（`tools/coverage.py`／`coverage.md`：遊戲資料裡真的出現的每個機制，核對引擎有沒有照原版處理）＋**實際遊玩走查**，函數層級只用來查原版規則。
- 資料覆蓋矩陣：狀態、道具類型、怪物欄位、劇情命令基本上都「完整」；缺口在過場細節（見 `coverage.md`、`PORT_QUEUE.md`）。
- 已做並用瀏覽器驗證：新遊戲→五職業創角→難度→開場過場→趙村；NPC 對話（含走近自動說話輔助）、打怪（狼曾因 istrap 10 被當陷阱看不見，已修）、射箭、升級、掉落、裝備（拖放＋人偶）、商店買賣、技能視窗、換場景快取（回來 55–72ms）、存讀檔（含地上道具、怪物位置血量）、死亡（JM513）、PWA（manifest、圖示、service worker；http 非安全環境 SW 不啟用，要用 Tailscale https）、手機觸控（浮動搖桿、攻擊鍵、4 技能鍵、瞄準模式、放大鏡預覽、直式視窗放大、載入進度畫面）。
- **主線走查**（`HANDOFF.md`「主線走查」表）：第一章到第三章 headless 整條通過，第四章已進入；**卡在驪山「開館驗屍」：需要金蟾珠與任一把神劍，還沒追查取得方式**（上一個子代理剛開始就被額度中斷）。第五～十章、結局沒走。已修的斷鏈 bug 多半是同一類（Ghidra 把劇情格回傳值丟掉／void 化、虛擬格 2／11／第 15／17 格從未被呼叫、同名 ONT 入口配置）；`tools/story_orphans.py`、`flagwho.py`、`retscan.py` 是掃描工具。
- 449 個場景（扣連線版 net*／lan*）載入掃描：0 警告 0 例外。

## 5. 還沒做／待追（接手第一批可以從這裡開始）
1. 主線走查續（`PORT_QUEUE.md`「主線走查待續」）：金蟾珠與神劍的取得 → 驪山開館 → 第五～十章 → 結局；每章關鍵環節用瀏覽器實測。
2. 前面報告「未驗證／原版規則沒找到」的清單（HANDOFF 各輪最後；矩陣 `?` 剩 19 項：狀態表 9 欄、死亡方式 8 種…）。
3. 全部「單機可達」skip 的等價性驗證（約 324 個「介面→本機伺服器訊息層」特別要逐個比對；歷次抽查都抓到過漏掉的規則）。
4. 使用者延後：**片頭影片卡頓**（`/movie/*.mp4` 經 ffmpeg 轉檔，手機上很卡）。
5. 使用者提過的體驗：手機直式創角／選單畫面很小；PWA 想用 https。
6. 原版沒有而我們自訂的東西（銀行 B 鍵視窗、開鎖目標用最近的箱子、人偶姿勢、對話框色）都已用 shortcut 註解標出（`grep -rn "shortcut:" engine/src | wc -l` 約 300 處）。

## 6. 跟使用者合作的偏好（原本存在前一個帳號的記憶裡，不會自動帶過來）
- 說中文（繁體）。先給方案／草稿、使用者同意後就一路做完、測試、push，不要一直問；使用者說過「全自動不要再詢問」「額度中斷後自己接著做」。
- 不要自行判斷跳過；不要用簡化版交差；誠實說哪些沒驗證；數字要實算，不要估計。
- 手機操控要好用：UI 位置穩定、小介面要有放大鏡預覽確認；手機直式／橫式都要支援。
- 不要把工作轉給 Codex 之類外部工具除非使用者要求；使用者自己要的是節省 Claude 額度：主對話用 Sonnet、重活派 Sonnet 子代理、Haiku 做審查／抽查（`.claude/agents/qinport-audit.md`）。
- 工作方式（我們用的）：主對話只負責派工、驗證（重跑 make／skilltest／storytest）、commit、push、更新試玩複本；一次只有一個子代理改主樹（都改 main.c）；子代理 prompt 要寫清楚「不 commit／push、網頁版只在 /tmp 複本、查不到規則要明講」。

## 7. 這個帳號專屬、不會轉移的東西
- 本機記憶（`~/.claude/projects/.../memory/`）與全域 `~/.claude/CLAUDE.md`：重點已抄在第 6 節。
- 每小時自動接力排程（trigger `trig_011TaASSMphgauMPaQURJqwD`，綁在這個帳號的 session）：**已停用**。接手帳號要的話自己建一個（內容見 `PORT_QUEUE.md` 開頭的步驟）。
- Mac 上的 Tailscale 試玩伺服器（8642 開發版、8643 穩定版）：跟著那台機器；換機器要重新起。
- Claude 額度：接手帳號有自己的 5 小時額度；子代理被 429 打斷時，等重置後用 `SendMessage` 叫醒它會從停下處繼續。
- 倉庫存取：接手帳號要有 `chanwunkong/SideTest` 的存取權（私有倉庫要先加協作者）。
