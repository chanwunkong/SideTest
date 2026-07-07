# 模擬器 Bitstream 格式（TASK-006）

**這是本模擬器自訂的序列化格式，不是真實 XC2064 的物理 bitstream 位元排列。** 真實格式見
[`xc2064-reference.md`](./xc2064-reference.md) §5：160×71 分散式 SRAM 記憶體網格、71-bit 垂直移位暫存器、
逐欄並行寫入；bitstream 的位元語意完全由硬體版圖決定，需要完整的 switch matrix pin-to-pin 連接表與
CLB 內部多工器控制位元的實體位置才能還原，而這些在 §7「待確認清單」中仍未確認。因此本模擬器選擇
**自行定義一套等價、可讀寫、但物理上不對應真實晶片的格式**，並在此文件明確記錄，避免任何人誤以為
匯出的 `.bit` 檔可以燒錄到真實 XC2064 或與原廠 XACT 工具的 bitstream 互通。

實作位置：`FPGA/FPGA.html` 中的 `BitWriter` / `BitReader` / `serializeBitstream()` /
`deserializeBitstream()` / `applyLoadedState()`。

---

## 1. 檔案結構

```
┌─────────────┬───────────┬────────────┬───────────────────────────┐
│ magic (4B)  │ ver (1B)  │ size (1B)  │ payload（逐 bit 封裝）      │
│ "XSIM"      │ = 1       │ = GRID_SIZE│ MSB-first，不足 8 bit 補 0 │
└─────────────┴───────────┴────────────┴───────────────────────────┘
```

- **magic**：固定 4 bytes `0x58 0x53 0x49 0x4D`（ASCII `"XSIM"`）。匯入時若不符即拒絕解析。
- **version**：目前為 `1`。日後格式若變動需遞增版本號，並在 `deserializeBitstream()` 中判斷相容性。
- **size**：晶片邊長（即 `GRID_SIZE`，例如 4/8/12/16）。匯入時會依此值重建整個網格，
  UI 的網格大小下拉選單若沒有對應選項，僅下拉選單顯示不會更新，但模擬邏輯仍正確運作。
- **payload**：所有可配置狀態，依下列固定順序逐 bit 寫入，MSB-first；不包含任何「執行期即時運算值」
  （例如 `val_F`/`val_Q`/`in_A`/線路目前電位等）——這些都是每個 iteration 由 `simulateCombinatorial()`
  重新算出來的，不屬於「配置」，因此不序列化。

---

## 2. Payload 內容與順序

### 2.1 CLB 配置（row-major，r=0..size-1, c=0..size-1，每顆 21 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `lut_f` | 8 | F 函數產生器的 8-bit 真值表（見 `xc2064-reference.md` §2） |
| `lut_g` | 8 | G 函數產生器的 8-bit 真值表 |
| `ff_d_src` | 3 | D 型正反器輸入來源，編碼見下表 |
| `mux_x` | 1 | 0 = F，1 = Q |
| `mux_y` | 1 | 0 = G，1 = Q |

`ff_d_src` 3-bit 編碼（`FF_D_SRC_CODES`）：

| 值 | 意義 |
|---|---|
| 0 | F |
| 1 | G |
| 2 | F_XOR_G |
| 3 | F_AND_G |
| 4 | F_OR_G |
| 5–7 | 保留未使用 |

### 2.2 Switch Matrix（row-major，r=0..size, c=0..size，每交點 6 bit）

依 `SWITCH_LINKS` 固定順序：`WE, NS, WN, WS, EN, ES`，每項 1 bit（1 = 導通）。見
`xc2064-reference.md` §3 與 TASK-004 的近似拓樸說明。

### 2.3 水平線段 h_wires（row-major，r=0..size, c=0..size-1，每段 1 bit）

1 bit 表示該段金屬線是否存在（`on`）。

### 2.4 垂直線段 v_wires（row-major，r=0..size-1, c=0..size，每段 1 bit）

同上，垂直方向。

### 2.5 輸入 IOB（依索引 i=0..size-1，每個 5 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `forced` | 1 | 1 = 強制驅動，0 = 浮接 |
| `val` | 1 | 強制驅動時的值（浮接時此位仍會寫入但匯入時被忽略，實際讀值改採 `pull_up`） |
| `pull_up` | 1 | 上拉電阻是否啟用 |
| `threshold` | 1 | 0 = TTL，1 = CMOS（僅供顯示對照，不影響模擬結果） |
| `ff_enabled` | 1 | 是否經輸入正反器同步 |

### 2.6 輸出 IOB（依索引 i=0..size-1，每個 2 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `tri_state` | 1 | 1 = 三態（Output Enable 關閉） |
| `threshold` | 1 | 0 = TTL，1 = CMOS |

---

## 3. 匯出／匯入行為

- **匯出**（`exportBitstream()`）：依上述順序打包成 `Uint8Array`，包成 `Blob`
  （`application/octet-stream`）觸發瀏覽器下載，檔名 `xc2064-sim-{size}x{size}.bit`。
- **匯入**（`handleBitstreamFile()` → `deserializeBitstream()` → `applyLoadedState()`）：
  讀取檔案為 `ArrayBuffer`，驗證 magic/version，解析出的 `size` 會覆蓋目前的 `GRID_SIZE` 並呼叫
  `buildGrid()` 重建骨架，再逐項覆蓋成檔案內的配置。magic 不符或版本不支援會跳出 `alert` 並中止匯入，
  不會破壞目前畫面上的狀態。
- 側欄「Bitstream 匯出／匯入」面板會顯示最近一次匯出/匯入的位元組數與前 16 bytes 的 hex dump，
  方便直接觀察這是一份「打包過的位元資料」而非單純的 JSON 設定檔。

## 4. 與真實 XC2064 格式的差異（重申）

| | 本模擬器格式 | 真實 XC2064 |
|---|---|---|
| 資料單位 | 依邏輯欄位打包的 bit-level 格式，人為定義順序 | 由物理版圖（記憶體 cell 位置）決定，非邏輯欄位對齊 |
| 載入方式 | 一次性讀入 `ArrayBuffer` 解析 | 71-bit 垂直移位暫存器，逐欄序列載入（見 §5） |
| 是否含未用位元 | 無（緊密封裝） | 每個 tile 約 27 bits 未使用（版圖上直接省略對應記憶體 cell） |
| 可否互通 | 僅本模擬器自身可讀寫 | 需原廠 XACT 工具鏈 |
