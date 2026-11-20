你接手《秦殤》（QinPort）的完整復刻，在 /Users/martini/github/SideTest，分支 qinport（已含全部工作，沒有未推送的東西）。

先讀這三份，不要先動手：
1. qinport/tools/ACCOUNT_HANDOFF.md（交接說明：目標、規則、環境、目前真實進度、待辦、跟使用者合作的偏好）
2. qinport/tools/PORT_QUEUE.md（每一批的做法與驗證步驟）
3. qinport/HANDOFF.md 最後幾節（「主線走查」表、「實際遊玩驗證」各輪）

然後直接接著做，不要問使用者，除非遇到只有使用者能決定的事：
- 先確認環境：`git status`、`cd qinport/engine && make && make skilltest 2>&1 | tail -3`（要印 skilltest ok）、`make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1`、`make walktest`。
- 第一批：主線走查續——追查驪山「開館驗屍」需要的金蟾珠與任一把神劍怎麼取得（NPC 贈與、怪物掉落、商店、任務獎勵、打造配方），修斷鏈，再從第四章走到第十章和結局，每章最關鍵的 2～3 個環節用瀏覽器實測。
- 之後依 PORT_QUEUE.md 與 ACCOUNT_HANDOFF.md「還沒做」清單繼續。

工作方式：主對話用 Sonnet（/model sonnet）；重活派 Sonnet 子代理（general-purpose，一次只一個改主樹）；Haiku 做審查／抽查（.claude/agents/qinport-audit.md）。子代理的 prompt 要寫：不 git commit／push、網頁版只在 /tmp 複本編與測、查不到原版規則要明講。每批做完你（主對話）重跑 make／skilltest／storytest／walktest，通過才 commit 並 push 到 origin qinport，並更新試玩穩定版複本（步驟在 PORT_QUEUE.md 第 3 點）。

使用者要求（絕對遵守）：完整復刻、不接受簡化版當完成、不接受自行判斷略過、查不到原版規則時明講並列證據、誠實回報哪些只靠編譯／單元測試沒在實機驗證、數字要實算不要估計。使用者說中文（繁體）。

遇到額度中斷（429）：等重置後用 SendMessage 叫醒被中斷的子代理，從停下處繼續。若你有排程工具，可建立每小時的自動接力（內容照 PORT_QUEUE.md 開頭步驟），讓額度恢復後自己繼續。
