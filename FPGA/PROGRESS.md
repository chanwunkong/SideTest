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
- Commit: (pending — see next entry)
