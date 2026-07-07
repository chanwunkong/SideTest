# XC2064 規格參考文件

供 [XC2064] 軌所有任務對照使用。所有數字與架構描述均附來源；無法從公開資料確認的細節，明確標註「待確認」，**不得**憑印象填寫後續實作。

---

## 1. 晶片總覽

| 項目 | 數值 | 來源 |
|---|---|---|
| 發表年份 | 1985（首顆商用 FPGA） | Shirriff |
| CLB 數量 | 64 顆，排列為 8×8 網格 | Shirriff、Xilinx datasheet |
| IOB 數量 | 58 個 | Xilinx datasheet（經 kynix 轉述） |
| 等效邏輯閘數 | 約 1000～1200 gates（不同文件引用範圍略有差異：600–1000 / ~1200，行銷數字，非精確換算）| 多來源，數字有出入，**待確認**精確口徑 |
| 製程 | CMOS，NMOS + PMOS，雙層金屬 + 多晶矽閘 | Shirriff |
| 供電 | 5.0V | datasheets360 |
| 封裝範例 | PC68 / PD48 / PGA68 等 | datasheets360 |

---

## 2. CLB（Configurable Logic Block）內部架構

- 每個 CLB 有 **4 個一般輸入**（A, B, C, D）與 **2 個輸出**（X, Y）。
- 內部有 **兩個 3-input 查表（F、G 各為 LUT3）**：
  - 每個 LUT 由 **8 bits 記憶體 + 選擇多工器** 組成，可實作任意 3 輸入邏輯函數。
  - F、G 兩個 LUT3 可透過可程式化多工器**串接組合**，構成一個 4 輸入（或其他輸入組合）的函數 —— 這與 `FPGA/FPGA.html` 目前 UI 上的 `cfg-f` / `cfg-g` 下拉選單概念一致，但目前程式碼把 F/G 當成獨立的簡化真值表，尚未實作「兩個 LUT3 合併成一個更大函數」的正確語意（見 TASKS.md TASK-003）。
- 每個 CLB 內有 **一個 D 型正反器**（不是兩個），用於時序邏輯（計數器、移存器、狀態機等）。
  - 正反器實作方式為 **primary/secondary latch（master-slave）結構**：clock 為低電位時，第一級多工器允許資料進入 primary latch；clock 轉高時，第一級鎖回（保持），同時第二級的多工器把 primary latch 的值帶入 secondary latch；clock 轉低時第二級鎖回。整體效果為**上升緣觸發**的正反器。
  - 待確認：正反器的 clock enable / reset 訊號確切名稱與控制方式（目前僅知具備可程式化控制多工器，精確訊號命名未在檢索到的資料中列出）。
- 輸出多工器：CLB 的 X、Y 輸出可各自選擇來自組合邏輯（F 或 G）或正反器輸出（Q），對應 `FPGA/FPGA.html` 現有的 `cfg-mux-x` / `cfg-mux-y` 概念。

---

## 3. 繞線 / Switch Matrix 拓樸

- 一般用途互連（general purpose interconnect）由 CLB/IOB 列與行之間的**水平與垂直金屬線段網格**構成。
- 線段端點透過 **switch matrix** 相連以達成可程式化連線：
  - 每個「tile」（一個 CLB 加上鄰接繞線資源）含有 **2 個 switch matrix**。
  - 每個 switch matrix 為 **8-pin**，約有 **20 個可程式化連接點**；兩個 matrix 合計約 **40 個控制位元／tile**。
  - 待確認：switch matrix 的精確拓樸圖（哪些 pin 對哪些 pin 有連接可能性）未在檢索到的公開資料中取得完整圖示，需要進一步找原廠 datasheet 圖或以 Shirriff 的 die-shot 分析為準。
- **Long lines**（長線）：貫穿晶片主要軸向全長的水平/垂直線，提供**低偏斜（low-skew）**的信號分佈，常用於高扇出訊號（例如全域時脈）。
- CLB 之間另有**直接互連（direct interconnect）**可連到相鄰 CLB 的輸入，不需經過 switch matrix，用於最短延遲的連線（此點為一般 Xilinx 架構常見設計，本文件標註**待確認**是否在 XC2064 世代已存在，或是later 世代才加入）。
- 全域網路（global net）用於時脈分配。

---

## 4. IOB（Input/Output Block）架構

- 每個 IOB 可設定為輸入、輸出或雙向。
- 輸入路徑選項：
  - 直接連接（direct）或經由**正反器/閂鎖（flip-flop/latch）**同步外部訊號。
  - 輸入緩衝可選 **TTL 或 CMOS** 相容閾值。
  - 可選**上拉電阻（pull-up）**，避免未使用輸入腳浮接。
- 輸出路徑選項：
  - 輸出緩衝提供 CMOS 相容 **4 mA** 源/灌電流驅動能力，相容 CMOS 或 TTL 訊號位準。
  - 三態（three-state）輸出控制（供雙向或匯流排應用）。
- 這代表 `FPGA/FPGA.html` 目前的 IOB 僅做「輸入切換 0/1、輸出顯示」是明顯簡化版；TASK-005 需補上 TTL/CMOS 閾值選擇、pull-up、三態控制、以及可選的 IOB 正反器。

---

## 5. 組態記憶體與 Bitstream

- 組態資料儲存在**分散式 SRAM cell 陣列**中，整顆晶片的配置記憶體排列為 **160 × 71** 網格（依 Shirriff 對晶片實體的逆向工程分析）。
- 每個記憶體 cell 為 **5 電晶體 SRAM cell**。
- Bitstream 透過晶片中央的 **71-bit 垂直移位暫存器**串列載入，每次移入一整欄後，由**欄選擇電路（column select）**將該欄資料並行寫入對應的記憶體 cell，如此重複 160 欄。
- 每個 tile 分配 **8×18 bits** 的 bitstream 空間，其中約 **27 bits／tile 未使用**（對應的記憶體 cell 在版圖上直接省略以節省面積）。
- 兩階段多工器（two-stage mux）搭配 **5 個控制位元**，用於從最多 8 個候選輸入中選出 CLB 的實際輸入，避免使用完整解碼器電路。
- 重要限制：「bitstream 檔案格式直接由硬體電路版圖決定」——也就是說，**沒有從 XACT 工具的邏輯功能定義到 bitstream 位元的直接對應關係**，必須實際分析電路才能還原真正的位元語意。這代表 TASK-006（bitstream 資料結構）若要做到「真實格式」，工作量遠大於單純的狀態序列化，需要明確在文件中區分「模擬用格式」與「真實格式」（TASKS.md 已註記此點）。

---

## 6. 來源

- Ken Shirriff, ["Reverse-engineering the first FPGA chip, the XC2064"](http://www.righto.com/2020/09/reverse-engineering-first-fpga-chip.html) — CLB 內部結構、LUT3×2、正反器 master-slave 實作、switch matrix pin/bit 數、配置記憶體網格、bitstream 載入機制的主要依據。
- 綜合搜尋結果（datasheets360.com、kynix.com 對 Xilinx 原始 XC2000 系列 datasheet 的轉述）——CLB/IOB 數量、等效閘數、IOB 選項（TTL/CMOS、pull-up、flip-flop/latch、三態）。
- 原廠 PDF（`datasheets.hypertriton.com/XC2000.pdf`）已定位但**尚未能完整解析內文**（僅取得壓縮版面資料，未渲染成可讀文字）；後續若需要更精確的圖表與訊號命名，建議由人工開啟該 PDF 核對，而非依賴自動擷取。

---

## 7. 待確認清單（供後續任務追蹤）

- [ ] CLB 正反器 clock enable / reset 訊號的確切命名與控制邏輯細節。
- [ ] Switch matrix 完整拓樸圖（pin-to-pin 連接可能性表）。
- [ ] Direct interconnect（相鄰 CLB 直接互連）是否存在於 XC2064，或僅為後期世代（XC3000 以後）架構。
- [ ] IOB 是否具備獨立的輸出正反器，或僅輸入端有 flip-flop/latch 選項。
- [ ] 等效邏輯閘數的精確官方口徑（600–1000 vs ~1200 gates 兩種引用數字）。
- [ ] Master/Slave serial 組態模式的完整協定細節（本文件僅涵蓋單顆晶片內部的欄式載入機制，未涵蓋多顆菊鏈串接時的協定）。
