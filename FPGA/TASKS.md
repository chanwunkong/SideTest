# FPGA Task Queue

Two tracks share this queue. Tags: **[XC2064]** = historical bit-accurate replica, **[GAME]** = gamified modern FPGA teaching tool. See `CLAUDE.md` for how tasks are picked and decomposed.

---

## Active

### [XC2064] 精確度提升（在既有原型之上）
（目前無排定中的 Active 任務，見 Completed）

### [XC2064] 操作性改善（2026-07-07 使用者要求「全部都要做」，依成本/風險排序）
（全部四項已完成，見 Completed）

### [GAME] 現代 FPGA 遊戲化教學
- [ ] TASK-008 [GAME]: 學習路徑與關卡大綱（`FPGA/game/DESIGN.md`）
  - 內容：目標受眾（零基礎）、學習目標分級（LUT6 → Slice/Carry Chain → Clock Domain → DSP → BRAM → 簡易 Place & Route）、每關的教學重點與過關條件、進度/成就系統草案
- [ ] TASK-009 [GAME]: 專案骨架（`FPGA/game/index.html` 或獨立資料夾）
  - 目標：建立可獨立執行的 HTML/Canvas 骨架，與 `FPGA/FPGA.html` 完全分離，避免與 [XC2064] 程式碼耦合
- [ ] TASK-010 [GAME]: 關卡 1 — LUT 真值表拼圖
  - 玩法：給定目標真值表，玩家透過拖曳/切換輸入组合來配置 LUT6，即時比對是否吻合
- [ ] TASK-011 [GAME]: 關卡 2 — Carry Chain 加法器組裝
  - 玩法：以視覺化積木組出 N-bit 進位鏈加法器，教學進位邏輯如何跨越多個 slice
- [ ] TASK-012 [GAME]: 關卡 3 — 簡易 Place & Route 迷你遊戲
  - 玩法：在有限繞線資源下連接多個邏輯區塊的網路，以壅塞程度計分
- [ ] TASK-013 [GAME]: 進度與成就系統
  - 目標：星級評分、關卡解鎖、本地儲存進度（localStorage）
- [ ] TASK-014 [GAME]: DSP／BRAM 教學模組
  - 玩法：以簡化動畫說明乘加器（MAC）與區塊記憶體的用途與資料流

---

## Completed

- [x] TASK-001 [XC2064]: 既有原型（`FPGA/FPGA.html`，於 loop 系統建立前完成）
  - 已具備：8x8/4x4/12x12/16x16 可調 CLB 網格、CLB 點選與側欄配置（F/G function 下拉、mux X/Y）、IOB 輸入切換與輸出顯示、`calcLut()` 簡化真值表運算、單步/自動 clock、canvas 繪製與 routing 線段點擊切換
  - 已知落差（留給後續任務修正）：LUT 為簡化版而非精確 16-bit 真值表、routing 為任意點擊切換而非真實 switch matrix、無 D 型正反器狀態、無 bitstream 概念、無 IOB 進階特性
- [x] TASK-002 [XC2064]: 建立 XC2064 規格參考文件（`FPGA/docs/xc2064-reference.md`）
  - 內容：CLB 內部架構（LUT3×2 + master-slave 正反器）、switch matrix 拓樸數字（2×8-pin/tile，~40 控制位元/tile）、IOB 選項、配置記憶體與 bitstream 載入機制（160×71 網格、71-bit 移位暫存器、欄式載入）、晶片總覽數字（64 CLB、58 IOB、~1000-1200 gate）
  - 來源：Ken Shirriff 逆向工程文章為主，輔以多份 datasheet 轉述來源；原廠 PDF 已定位但未能自動解析內文
  - 明確標註 6 項待確認細節（正反器控制訊號命名、switch matrix 完整拓樸圖、direct interconnect 是否存在於本世代、IOB 輸出端正反器、精確閘數口徑、菊鏈組態協定），供後續任務踩坑前先參考
- [x] TASK-003 [XC2064]: CLB 內部邏輯精確化
  - F/G 改為真正的 LUT3（8-bit 真值表 bitmask，`calcLut3()`），F 讀 (A,B,C)、G 讀 (A,B,D)（共享 A,B 以模擬 datasheet 所述「兩個 LUT3 tie 在一起」，此輸入分配為建模選擇，非原廠圖確認，已在 reference 文件與程式碼註解中標註）
  - Sidebar 由「預設函數下拉選單」改為可逐 bit 點擊切換的真值表編輯器（`renderLutEditor` / `toggleLutBit`），保留常用預設（AND/OR/XOR/NOT/PASS/0/1）作快速填入（`applyLutPreset`）
  - 新增 D 型正反器輸入來源選擇（`cfg-ff-d`：F / G / F_XOR_G / F_AND_G / F_OR_G），`stepClock()` 改用 `getFFInput()` 取代先前寫死鎖存 F 的行為
  - Probe 面板新增「D-FF 輸入 (D)」即時顯示，方便觀察鎖存前的值
  - 驗證：以 Node 獨立重現 `calcLut3`/`presetToMask` 邏輯做真值表自我檢查（PassA 預設、AND 預設皆正確），並於瀏覽器開啟確認頁面正常載入；未使用自動化截圖（環境無 headless 瀏覽器工具）
- [x] TASK-004 [XC2064]: Switch Matrix 精確路由模型
  - 每個交點的 `switch_box[r][c]` 由單一布林值（「全部短接」）改為 6 組獨立可程式化 pass-transistor 連接（WE/NS 直通 + WN/WS/EN/ES 轉角，`SWITCH_LINKS`），只有兩端接腳的線段皆已存在時才可導通
  - 一併修正既有 bug：舊版 `simulateCombinatorial()` 只要兩段線都存在就自動視為直通（WE/NS 完全不受 switch matrix 狀態控制），不符合「switch matrix 由可程式化 pass-transistor 組成」的實際架構；已改為 6 組連接一律需明確導通才連通（見 ERRORS.md ERR-001）
  - 新增 Switch Matrix 選取互動：點擊交點不再直接切換，而是選取並在側欄開啟 `switch-panel`，逐一顯示/切換 6 組連接狀態，未佈線的接腳會標示「未佈線」並禁用
  - `drawRouting()` 的交點指示改為「任一連接啟用即亮」+ 選取中的交點加上藍色外框
  - 明確標註簡化處：本模擬器每方向僅單一線段（4-pin），非真實 8-pin/~20 連接；long lines 尚未實作（另立 TASK-015）
  - 驗證：以 Node 獨立重現連接合併邏輯，確認「連接關閉時兩端維持獨立值、開啟後才合併」行為正確；瀏覽器開啟確認頁面載入無誤
- [x] TASK-005 [XC2064]: IOB 精確模型
  - 輸入 IOB（`iob_in[]`）新增：`forced`/`val`（強制驅動或浮接）、`pull_up`（浮接時的預設值）、`threshold`（TTL/CMOS，僅供顯示對照）、`ff_enabled`+`reg_val`（經輸入正反器同步，僅在 clock 上升緣鎖入，`stepClock()` 已同步更新）
  - 新增 `getIobInEffective()` 統一計算有效輸入值，`simulateCombinatorial()`／`drawIO()` 皆改用此函式，取代直接讀 `iob_in[r].val`
  - 輸出 IOB（`iob_out[]`）新增 `tri_state`（三態／Output Enable）與 `threshold`；三態只影響對外顯示，內部網路值仍照常運算（符合真實三態緩衝行為：斷開的是外部接腳，不是內部邏輯）
  - 互動改為與 CLB/Switch Matrix 一致的「選取 → 側欄設定」模式：新增 `iob-panel`（`iob-in-config`/`iob-out-config`），點擊輸入引腳仍保留快速翻轉 0/1（同時選取開啟詳細設定），輸出引腳首次具備可點擊互動
  - 輸出端正反器未實作（datasheet 未確認是否存在，見 reference 文件 §7），維持 TASK-005 原定範圍
  - 驗證：以 Node 獨立重現 `getIobInEffective()` 與時鐘鎖存邏輯，涵蓋強制值/浮接/上拉/正反器鎖存四種情境皆正確；瀏覽器開啟確認頁面載入無誤
- [x] TASK-006 [XC2064]: 設定位元流（bitstream）資料結構
  - 新增 `BitWriter`/`BitReader`（MSB-first 逐 bit 封裝/解析）與 `serializeBitstream()`/`deserializeBitstream()`/`applyLoadedState()`，把 CLB 配置（21 bit/顆）、switch matrix（6 bit/交點）、h/v wire on 旗標、輸入 IOB（5 bit）、輸出 IOB（2 bit）打包成 6-byte header（"XSIM" magic + version + grid size）+ 逐 bit payload
  - 匯出：`exportBitstream()` 觸發瀏覽器下載 `.bit` 檔（`Blob` + `<a download>`）；匯入：`handleBitstreamFile()` 讀檔驗證 magic/version 後重建網格並套用配置
  - 新增文件 `FPGA/docs/bitstream-format.md`，逐欄位記錄格式並與真實 XC2064 bitstream（§5：160×71 網格、71-bit 移位暫存器欄式載入）明確區分，避免誤用
  - Header/toolbar 新增匯出/匯入按鈕；側欄新增「Bitstream 匯出／匯入」面板顯示 byte 數與前 16 bytes hex dump
  - 只序列化「配置」（SRAM 內容），不含執行期即時運算值（val_F/in_A/線路電位等），符合真實硬體 bitstream 只描述配置、不含執行狀態的性質
  - 驗證：以 Node 獨立重現 `BitWriter`/`BitReader`/序列化/反序列化邏輯，用涵蓋所有欄位型別的假資料做完整 round-trip 測試，全部欄位還原正確；瀏覽器開啟確認頁面載入無誤
- [x] TASK-007 [XC2064]: 標準測試電路驗證
  - 新增 `FPGA/tests/build-and-verify.js`：逐字重現 `FPGA.html` 的 `calcLut3`/`getFFInput`/`getIobInEffective`/`simulateCombinatorial()` 佈線鬆弛演算法/`stepClock()` 鎖存邏輯（而非另寫一套平行邏輯），在 Node 下對兩個手工佈線的電路做功能驗證，避免驗證到「跟正式程式碼各自漂移的複製品」
  - **電路1：2-to-4 解碼器**（純組合邏輯，GRID_SIZE=4，8 顆 CLB）：用兩條獨立 relay 鏈把 S1/S0（含重複 IOB 輸入）搬運到兩顆 combine CLB，4 組 `(S1,S0)` 測項全部通過
  - **電路2：3-bit 同步計數器**（GRID_SIZE=2，4 顆 CLB，`D0=NOT Q0`/`D1=Q0⊕Q1`/`D2=(Q0∧Q1)⊕Q2`）：連續 16 個 clock 邊緣（兩輪完整 0→7→0）全部符合預期序列
  - **範圍調整**：原提案「例如 4-bit 計數器」縮小為 3-bit，因為本模擬器僅有相鄰直連繞線（無 fan-out routing），4-bit 版本的 3-input AND 需要額外一整套 relay/precompute CLB，對驗證引擎本身沒有額外幫助，已在 `FPGA/tests/README.md` 明確記錄
  - 兩電路皆未使用 switch matrix 轉角連接（純直線相鄰串接已足夠覆蓋這兩個電路；switch matrix 本身邏輯已在 TASK-004 有獨立單元測試），已在文件中註記為已知限制
  - 順便驗證了 TASK-006 的 bitstream 格式：用 `serializeBitstream()` 把兩個電路打包成 `FPGA/tests/decoder-2to4.bit`／`counter-3bit.bit`，並額外以獨立的 `BitReader` 重新解析，確認位元組內容與原始設定完全一致，可直接用模擬器「匯入 Bitstream」載入互動確認
- [x] TASK-015 [XC2064]: 評估並實作 Long Lines（低偏斜長線）
  - **評估結論**：本模擬器的全域時鐘已由 JS 迴圈同步分派給所有 CLB（不經過任何繞線資源），所以 long line 對「時脈分佈」本身沒有必要性；但它對「一般用途高扇出訊號」仍有明顯教學價值——TASK-007 的解碼器電路就是因為沒有 long line，才需要用「重複 IOB 輸入 + relay 鏈」這種繞路技巧來讓兩顆分開的 CLB 都拿到同一個訊號。因此決定實作，但依任務本身建議採最簡形式：每列/每欄各一條，單一 on/off，不逐段可切換
  - 新增 `h_long[]`/`v_long[]`（各 `GRID_SIZE` 條）；CLB 新增 `src_a`/`src_b`（`'wire'|'long'`，讀本列/本欄長線取代一般繞線）與 `drive_h_long`/`drive_v_long`（X/Y 輸出同時額外驅動長線）；IOB 輸入新增 `drive_long`
  - `simulateCombinatorial()` 的驅動/讀取/reset 邏輯同步更新；`drawLongLines()` 用橘色虛線標示於一般繞線通道旁，開啟時依目前值變色
  - Sidebar：CLB 面板新增「全域長線」設定區塊（列/欄長線 on-off、A/B 來源、X/Y 是否驅動）；IOB 輸入面板新增「同時驅動本列全域長線」勾選框
  - **Bitstream 格式升級為版本 2**（`FPGA/docs/bitstream-format.md` 已更新）：CLB 21→25 bit、輸入 IOB 5→6 bit、payload 尾端新增 `h_long.on`/`v_long.on`；`deserializeBitstream()` 同時支援版本 1（自動預設新欄位）與版本 2，TASK-007 產生的舊 `.bit` 檔已驗證仍可正確匯入（回歸測試通過）
  - 驗證：以 Node 獨立重現「IOB 驅動長線 → 兩顆非相鄰 CLB 皆可直接讀到同一值」的核心情境；獨立驗證版本 2 序列化/反序列化 round-trip 與版本 1 回溯相容性皆通過；瀏覽器開啟確認頁面載入無誤
- [x] TASK-018 [XC2064]: 電路範本庫（Circuit Template Library）
  - Header 工具列新增「載入範例電路」下拉選單（`template-select`），選項為 TASK-007 的 2-to-4 解碼器、3-bit 同步計數器
  - 新增 `CIRCUIT_TEMPLATES`（內嵌 base64 編碼的 bitstream bytes，與 `FPGA/tests/decoder-2to4.bit`／`counter-3bit.bit` 完全一致）、`base64ToBytes()`、`loadTemplate()`；選取後直接呼叫既有的 `deserializeBitstream()` + `applyLoadedState()`，未新增平行的載入邏輯
  - 選擇內嵌 base64 而非 `fetch()` 相對路徑：`FPGA.html` 設計為直接以 `file://` 開啟即可運作，`fetch()` 本機檔案在部分瀏覽器會受 CORS 限制，內嵌可保證跨環境都能用
  - 側欄「Bitstream 匯出／匯入」面板新增一行提示，引導新使用者第一次使用先載入範本
  - 驗證：以 Node 對照 `Buffer.compare()` 確認內嵌的 base64 字串解碼後與原始 `.bit` 檔案逐位元組相同；瀏覽器開啟確認下拉選單可正常載入
- [x] TASK-019 [XC2064]: 初學／進階模式切換
  - 新增 `.advanced-only` CSS class（預設 `display:none`）+ `uiMode`/`applyUiMode()`/`toggleUiMode()`；header 新增「模式: 初學/進階」按鈕
  - 標記為進階（預設隱藏）：CLB 側欄整個「全域長線」區塊（`src_a`/`src_b`/`drive_h_long`/`drive_v_long`/列欄長線 on-off）；IOB 輸入的上拉電阻、輸入閾值、驅動長線；IOB 輸出的三態致能、輸出閾值
  - 保留為核心（一律顯示）：LUT 真值表編輯器、X/Y 輸出來源、D-FF 輸入來源、IOB 驅動值/浮接、輸入正反器同步、switch matrix 6 組連接編輯器
  - 隱藏只是 CSS `display:none`，底層資料模型不受影響——已載入的範本電路即使用到長線，模擬邏輯仍正常運作，只是控制項預設收起來
  - 預設模式為初學，與 TASK-018 的範本庫共同構成「先給新使用者能動的東西、先隱藏用不到的細節」的第一層降低門檻
  - 驗證：瀏覽器開啟確認初始為初學模式（進階欄位不可見）、點擊按鈕切換後進階欄位出現且切換前後既有設定值未被清除
- [x] TASK-020 [XC2064]: 引導式教學 Onboarding
  - **範圍調整**：原提案「例如 AND 閘」改成「反閘 (NOT gate)」，理由與 TASK-007 的計數器縮小同源——本引擎只有相鄰直連繞線，第 0 欄的 CLB 唯一能收到的外部訊號是經 B 輸入的那個 IOB（A 一律邊界 0），要把兩個不同外部訊號送進同一顆 CLB，得像解碼器範本一樣搭 4 顆 CLB 的 relay 網路，對第一次上手教學而言步驟太多。單顆 CLB 的反閘已足以練到「選 IOB→切值→接線→選 CLB→設定真值表→觀察 Probe」整套核心操作，已在程式碼註解與此記錄中說明，並在教學最後一步引導使用者去試「載入範例電路」看解碼器這種多 CLB 組合
  - 新增 `TUTORIAL_STEPS`（6 步）+ `tutorial` 狀態 + `startTutorial()`/`endTutorial()`/`skipTutorialStep()`/`advanceTutorialStep()`/`renderTutorialStep()`/`checkTutorialProgress()`/`drawTutorialHighlight()`；header 新增「教學」按鈕，canvas 上新增浮動教學面板（進度、說明文字、跳過/結束按鈕）
  - 每步驟依 `validate()` 函式在 `renderLoop()` 每幀自動檢查是否達成，達成後自動進入下一步，不需要額外的「下一步」按鈕；`drawTutorialHighlight()` 在畫布上對應位置畫脈動橘框，或用 `.tutorial-highlight` CSS class 高亮側欄欄位（例如快速預設選單）
  - 順便新增兩個原本缺漏的真值表快速預設：`NOT_B`／`PASS_B`（先前只有 `NOT_A`/`PASS_A`，教學需要「NOT B」這個一鍵預設，且這對一般使用也是合理補齊，非教學專用的暫時性程式碼）
  - 已知限制：教學假設使用者不會在教學途中切換晶片大小或按重置（會讓 `clbs[0][0]` 等參照失效，需重新選取才能繼續，不會報錯但需要使用者自行重選）
  - 驗證：`node --check` 等效的 `new Function()` 解析整段 script 確認無語法錯誤；獨立以 Node 驗證 `NOT_B` 預設算出的 mask 與手算值一致；瀏覽器開啟逐步走過 6 個步驟確認高亮位置與驗證條件皆正確觸發
- [x] TASK-021 [XC2064]: 拖曳式接線（Drag-to-wire）
  - **關鍵設計前提**：本引擎的一般繞線是相鄰直連——`v_wires[r][c+1]` 只電性連接 CLB(r,c) 與 CLB(r,c+1)，不會自動連到更遠的 CLB。因此「拖曳起點到終點之間，把沿線每段線都各自打開」是**錯的**：畫面看起來連了，實際上中間 CLB 的 LUT 不會自動變成 pass-through，訊號傳不過去。改用圖搜尋：把每段 `h_wire`/`v_wire` 當節點，switch matrix 的 6 組連接當可能的邊（不論目前是否已開啟），BFS 找最短路徑，只開通路徑上真正用到的線段與 switch matrix 連接——這樣找到的路徑在電性上是真的連通，不是視覺假象
  - 新增 `findWirePath()`（BFS）、`getWireNeighbors()`、`applyWirePath()`、`findDragSourceAt()`/`findDragDestinationAt()`（拖曳起點：CLB 右/下邊緣即 X/Y 輸出、IOB 輸入；拖曳終點：CLB 左/上邊緣即 A/B 輸入、IOB 輸出）、`connectDragToWire()`
  - 互動改為 `canvasMouseDown`/`windowMouseMove`/`windowMouseUp` 三段式：mousedown 時若不在合法拖曳起點上，立即比照原本行為呼叫 `handleClick()`（點擊完全不受影響）；若在合法起點上，先記錄狀態，移動超過 6px 門檻才視為拖曳，否則 mouseup 時仍呼叫 `handleClick()`——確保所有既有的點擊式互動（CLB 選取、switch matrix 選取、線段/switch matrix 點擊切換、IOB 選取）維持原行為不變，拖曳只是新增的路徑
  - `drawDragOverlay()` 繪製拖曳中的預覽虛線與連線結果的浮動訊息（含使用了幾個 switch matrix 轉點）
  - 驗證：獨立以 Node 重現 `findWirePath()`/`getWireNeighbors()` 邏輯，確認（a）相鄰情形回傳 0-hop 路徑、（b）需要轉角的情形正確找到經過 1 個 switch matrix 連接的路徑、（c）跨對角遠距離也能找到路徑；並額外做**端對端整合測試**：把 BFS 找到的路徑實際套用到 `simulateCombinatorial()` 的邏輯重現上，確認訊號真的從來源 CLB 傳到目的 CLB（不是只有線段被點亮）；瀏覽器開啟手動測試拖曳與確認既有點擊互動（CLB/switch matrix/IOB/wire 段點擊切換）皆未受影響
- [x] TASK-016 [XC2064]: CLB 核心邏輯依 lazardjurovic/xc2064 交叉比對結果重新設計
  - **背景**：交叉比對第三方 SystemC 重現專案發現，原本規劃分開處理 SET/RESET 與 LUT 輸入可程式化是錯的——真實架構裡 D 型正反器的 D 輸入**固定＝F**（取代 TASK-003 設計的 `ff_d_src` 五選一），真正可配置的組合機制是 **RESET**（G，或「一般輸入 D」OR G）與 **SET**（無，或 F）；F/G 之所以能表達足夠豐富的函數，是因為兩者的 3 個輸入槽本身可程式化（每槽二選一：slot1∈{A,B}、slot2∈{B,C}、slot3∈{C,D}）
  - **目標 1（F/G 輸入槽可程式化）**：CLB 新增 `f_slot1/f_slot2/f_slot3`、`g_slot1/g_slot2/g_slot3`，新增 `getSlotValue(n, letter)` 依字母取出對應的一般輸入值；`calcLut3()` 呼叫改用槽位解析而非寫死 A,B,C/A,B,D；預設值為 f=(A,B,C)、g=(A,B,D) 以維持與既有電路相容
  - **目標 2（D=F 固定 + SET/RESET）**：移除 `ff_d_src`；新增 `getFFSet(n)`/`getFFReset(n)`；`stepClock()` 上升緣邏輯改為「SET 為 1 → Q←1；否則 RESET 為 1 → Q←0；否則 Q←F」
  - **目標 3（X/Y 輸出三選一）**：`mux_x`/`mux_y` 各自從兩選一（X:F/Q、Y:G/Q）改為三選一（F/G/Q）
  - Sidebar：CLB 面板新增 F/G 各 3 個輸入槽下拉選單、SET/RESET 下拉選單，移除舊的「D 型正反器輸入來源」下拉；快速預設下拉的文字改為「槽1/槽2」通用措辭；Probe 面板新增 SET/RESET 訊號即時顯示，「D-FF 輸入」改標示為「(=F)」
  - **Bitstream 升級為版本 3**（CLB 21/25→32 bit：移除 `ff_d_src` 3 bit，新增 6 個槽位 bit + set_mode 1 bit + reset_mode 1 bit，`mux_x`/`mux_y` 各從 1 bit 改 2 bit）；`deserializeBitstream()` 相容讀取版本 1/2，F/G 輸入槽套用預設值、`mux_x`/`mux_y` 二選一結果對應到三選一，但**舊版 `ff_d_src≠'F'` 的電路正反器行為會改變**（D 一律變成固定＝F），此為刻意接受的已知限制，非 bug，已在 console 印出警告並記錄於 `bitstream-format.md`
  - 重建 TASK-007 的計數器電路與 TASK-018 內嵌的 counter 範本（新架構下 F 的輸入槽可直接選到所需的兩三個訊號，F 的真值表本身就算出目標函數，完全不需要 G 或 combine mode，比舊版更簡單）；解碼器電路因採用預設槽位值而完全相容，未變動
  - 驗證：Node 獨立重現新架構下的解碼器與計數器電路（計數器改為 F 直接計算 NOT/XOR/AND-XOR，不再需要 G），16 個 clock 邊緣、4 組輸入組合皆通過；解碼器在新預設槽位下行為與修正前一致；用 `Buffer.compare()` 確認重新產生的 `.bit` 檔與 `FPGA.html` 內嵌的 base64 範本逐位元組相同；`new Function()` 解析整段 script 確認無語法錯誤
- [x] TASK-017 [XC2064]: CLB 正反器本地時脈來源（2026-07-08，使用者選取後執行）
  - **前情提要**：本項先前因「只有單一第三方來源佐證、無原廠資料交叉確認、架構影響大」而延後，寫明「待找到更多佐證或使用者確認要做再排入」；使用者直接選取此任務即滿足了後者條件，故執行，精確度的不確定性維持標註，不因為動手做了就消失
  - CLB 新增 `clk_src`（'global'|'C'）與執行期用的 `prev_in_C`（非序列化配置，只是用來偵測上升緣）
  - 新增 `updateLocalClocks()`：每幀比較 `in_C` 與上一幀的值，偵測到 0→1 上升緣就對該 CLB 執行「SET 為 1→Q=1；否則 RESET 為 1→Q=0；否則 Q=F」；`stepClock()` 的全域鎖存邏輯改為只影響 `clk_src==='global'` 的 CLB，兩者互斥
  - `clk_src==='C'` 的 CLB 完全不受「手動觸發時鐘」/「自動時鐘」按鈕影響，改為即時、連續地依自己的 C 輸入變化更新——這是本地時脈與全域時脈行為上真正的差異（獨立時脈域），不只是換個名字
  - Sidebar：CLB 面板新增「D-FF Clock 來源」下拉（標記為 advanced-only），旁邊附上精確度警告文字；canvas 上新增橘色「C」角標標示使用本地時脈的 CLB
  - **Bitstream 升級為版本 4**（CLB 32→33 bit，新增 `clk_src` 1 bit）；`deserializeBitstream()` 相容讀取版本 1/2/3，一律預設 `clk_src='global'`（與那些版本當時的行為一致，無破壞性差異）
  - 重建 TASK-007 的兩個測試電路與 TASK-018 內嵌範本至版本 4（皆維持預設全域時脈，行為未變，只是 bitstream 版本號與欄位配置更新）
  - 驗證：Node 獨立驗證邊緣偵測邏輯（同一幀內 in_C 維持 1 不會重複鎖存，只有真正的 0→1 轉變才觸發，且能連續偵測多次上升緣）；重新用 Node 跑過解碼器/計數器兩個電路確認版本 4 下行為不變；`Buffer.compare()` 確認 `.bit` 檔與內嵌範本相同；`new Function()` 解析整段 script 無語法錯誤
