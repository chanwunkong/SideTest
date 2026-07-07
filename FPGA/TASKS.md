# FPGA Task Queue

Two tracks share this queue. Tags: **[XC2064]** = historical bit-accurate replica, **[GAME]** = gamified modern FPGA teaching tool. See `CLAUDE.md` for how tasks are picked and decomposed.

---

## Active

### [XC2064] 精確度提升（在既有原型之上）
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
