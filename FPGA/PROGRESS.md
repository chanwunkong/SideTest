# Progress Log

## 2026-07-07T00:00Z
- Completed: CLAUDE.md loop rules, TASKS.md scaffold (retroactively logged existing FPGA/FPGA.html prototype as TASK-001), ERRORS.md scaffold
- Errors: none
- Queued: TASK-002 [XC2064] (規格參考文件), TASK-008 [GAME] (學習路徑大綱)
- Commit: (pending)

## 2026-07-07T01:00Z
- Track: XC2064
- Completed: TASK-002 — 建立 `FPGA/docs/xc2064-reference.md`，透過 WebSearch/WebFetch 查核 Ken Shirriff 的逆向工程文章與多份 datasheet 轉述來源，記錄 CLB（LUT3×2 + master-slave 正反器）、switch matrix（2×8-pin/tile）、IOB、bitstream 載入（160×71 網格）等架構事實，並列出 6 項待確認細節。同步修正 TASK-003/004/005 的描述使其對齊 reference 文件的正確架構（先前描述誤設 F/G 為單一 16-bit 真值表，已改為兩個 LUT3 合併模型）
- Errors: none
- Queued: TASK-003 [XC2064]（CLB 內部邏輯精確化）
- Commit: 0e7f9e9

## 2026-07-07T02:00Z
- Track: XC2064
- Completed: TASK-003 — `FPGA/FPGA.html` 的 F/G 改為真正 LUT3（8-bit 真值表 bitmask，`calcLut3()`），F 讀 (A,B,C)、G 讀 (A,B,D)；sidebar 換成可點擊真值表編輯器 + 快速預設；新增 D-FF 輸入來源選擇（F/G/XOR/AND/OR）取代原本寫死鎖存 F；`stepClock()` 改用 `getFFInput()`；Probe 面板新增 D-FF 輸入即時值
- Errors: none
- Queued: TASK-004 [XC2064]（Switch Matrix 精確路由模型）
- Commit: af97209

## 2026-07-07T03:00Z
- Track: XC2064
- Completed: TASK-004 — 交點的 `switch_box` 從單一布林值改為 6 組獨立可程式化連接（WE/NS 直通、WN/WS/EN/ES 轉角），新增 switch matrix 選取互動與側欄 `switch-panel`；一併修正 ERR-001（舊版直通連接不受 switch matrix 控制的架構性 bug）
- Errors: ERR-001 [resolved]
- Queued: TASK-005 [XC2064]（IOB 精確模型），另新增 TASK-015 [XC2064]（Long lines 評估，延伸自 TASK-004 範圍外項目）
- Commit: 030842c

## 2026-07-07T04:00Z
- Track: XC2064
- Completed: TASK-005 — 輸入 IOB 新增 forced/浮接/pull_up/threshold(TTL-CMOS)/ff_enabled+reg_val（輸入正反器同步），輸出 IOB 新增 tri_state/threshold；新增 `getIobInEffective()` 統一有效值計算；互動改為選取 → 側欄 `iob-panel` 設定（輸入引腳保留快速翻轉，輸出引腳首次可點擊）；輸出端正反器因 datasheet 未確認故不實作
- Errors: none
- Queued: TASK-006 [XC2064]（bitstream 資料結構）
- Commit: (pending)
