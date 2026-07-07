# FPGA Task Queue

Two tracks share this queue. Tags: **[XC2064]** = historical bit-accurate replica, **[GAME]** = gamified modern FPGA teaching tool. See `CLAUDE.md` for how tasks are picked and decomposed.

---

## Active

### [XC2064] 精確度提升（在既有原型之上）
- [ ] TASK-005 [XC2064]: IOB 精確模型（依據 `FPGA/docs/xc2064-reference.md` §4）
  - 目標：實作可程式化輸入緩衝閾值（TTL/CMOS）、三態輸出控制（4mA 驅動）、上拉電阻、輸入端 flip-flop/latch 選項
  - 影響檔案：`FPGA/FPGA.html` 的 IOB 相關繪製與狀態
  - 輸出端是否也有正反器仍待確認，先只實作輸入端
- [ ] TASK-006 [XC2064]: 設定位元流（bitstream）資料結構
  - 目標：定義一份對應目前所有可配置狀態（CLB 配置、routing、IOB）的序列化格式，提供匯出/匯入
  - 不要求與真實 XC2064 bitstream 位元排列完全一致，但需在文件中明確標註「模擬用格式」vs「真實格式」的差異
- [ ] TASK-007 [XC2064]: 標準測試電路驗證
  - 目標：在模擬器上搭建至少 2 個經典電路（例如 4-bit 計數器、2-to-4 解碼器），確認功能正確
  - 產出：`FPGA/tests/` 下的電路配置檔 + 驗證說明
- [ ] TASK-015 [XC2064]: 評估並視需要加入 Long Lines（低偏斜長線）
  - 目標：TASK-004 完成的 switch matrix 僅涵蓋單段式一般繞線；datasheet 提到的 long lines（貫穿全軸、用於全域時脈等高扇出訊號）尚未實作
  - 若採用單獨一條可全域點亮/點滅的長線層即可教學說明其用途，不需要每段都可切換

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
