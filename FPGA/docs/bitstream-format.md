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
┌─────────────┬───────────────┬────────────┬───────────────────────────┐
│ magic (4B)  │ ver (1B)      │ size (1B)  │ payload（逐 bit 封裝）      │
│ "XSIM"      │ = 1、2、3 或 4│ = GRID_SIZE│ MSB-first，不足 8 bit 補 0 │
└─────────────┴───────────────┴────────────┴───────────────────────────┘
```

- **magic**：固定 4 bytes `0x58 0x53 0x49 0x4D`（ASCII `"XSIM"`）。匯入時若不符即拒絕解析。
- **version**：`serializeBitstream()` 目前一律匯出 **版本 4**（TASK-017 新增 D 型正反器本地 clock
  來源欄位 `clk_src`）；`deserializeBitstream()` 同時可讀版本 1（TASK-006/007，無長線欄位）、版本 2
  （TASK-015，加入長線欄位）、版本 3（TASK-016，CLB 架構修正：F/G 輸入槽可程式化、SET/RESET、
  X/Y 三選一）與版本 4。讀取舊檔案時：
  - 長線相關欄位（版本 1 沒有的部分）一律套用預設值（`'wire'`/`false`）。
  - CLB 的 F/G 輸入槽（版本 1/2 沒有的部分）一律套用預設值 `(A,B,C)`/`(A,B,D)`，等同舊版寫死的固定
    分配，行為不變。
  - `clk_src`（版本 1/2/3 沒有的部分）一律套用預設值 `'global'`，等同「一律用全域時脈」，與版本
    1/2/3 當時的行為一致（因為那些版本還沒有本地時脈這個概念）。
  - **舊版（版本 1/2）的 `ff_d_src` 若不是 `'F'`（即 `'G'`/`'F_XOR_G'`/`'F_AND_G'`/`'F_OR_G'`），
    正反器行為會改變**：新引擎的 D 資料輸入固定＝F，無法還原舊版「D 取 G 或 F/G 組合」的語意，
    會在瀏覽器 console 印出警告但不會中止匯入。這是刻意接受的已知限制，不是 bug——TASK-007/018
    產生的 `.bit` 檔已重新以版本 4 匯出，不受影響。
  日後格式若再變動須再遞增版本號，並在 `deserializeBitstream()` 中依版本號分支處理，不得直接棄用
  舊版本讀取能力。
- **size**：晶片邊長（即 `GRID_SIZE`，例如 4/8/12/16）。匯入時會依此值重建整個網格，
  UI 的網格大小下拉選單若沒有對應選項，僅下拉選單顯示不會更新，但模擬邏輯仍正確運作。
- **payload**：所有可配置狀態，依下列固定順序逐 bit 寫入，MSB-first；不包含任何「執行期即時運算值」
  （例如 `val_F`/`val_Q`/`in_A`/線路目前電位等）——這些都是每個 iteration 由 `simulateCombinatorial()`
  重新算出來的，不屬於「配置」，因此不序列化。

---

## 2. Payload 內容與順序（版本 4）

### 2.1 CLB 配置（row-major，r=0..size-1, c=0..size-1，每顆 33 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `lut_f` | 8 | F 函數產生器的 8-bit 真值表（見 `xc2064-reference.md` §2） |
| `lut_g` | 8 | G 函數產生器的 8-bit 真值表 |
| `f_slot1` | 1 | F 輸入槽1：0=A，1=B |
| `f_slot2` | 1 | F 輸入槽2：0=B，1=C |
| `f_slot3` | 1 | F 輸入槽3：0=C，1=D |
| `g_slot1` | 1 | G 輸入槽1：0=A，1=B |
| `g_slot2` | 1 | G 輸入槽2：0=B，1=C |
| `g_slot3` | 1 | G 輸入槽3：0=C，1=D |
| `set_mode` | 1 | D 型正反器 SET：0=無，1=F |
| `reset_mode` | 1 | D 型正反器 RESET：0=G，1=（一般輸入 D）OR G |
| `mux_x` | 2 | X 輸出源，`MUX_CODES=['F','G','Q']` 索引（0/1/2；3 保留未使用） |
| `mux_y` | 2 | Y 輸出源，同上編碼 |
| `clk_src` | 1 | D 型正反器 clock 來源：0=全域時脈，1=本地一般輸入 C 上升緣（TASK-017，⚠ 僅第三方佐證，見 §7） |
| `src_a` | 1 | 0 = 一般繞線，1 = 本列全域長線（TASK-015） |
| `src_b` | 1 | 0 = 一般繞線，1 = 本欄全域長線 |
| `drive_h_long` | 1 | X 輸出是否同時（額外）驅動本列長線 |
| `drive_v_long` | 1 | Y 輸出是否同時（額外）驅動本欄長線 |

版本 3 的 CLB 為 32 bit（無 `clk_src`），讀取時一律預設 `clk_src='global'`。

**D 型正反器行為**（2026-07-07 依交叉比對修正，見 `xc2064-reference.md` §2）：資料輸入固定＝F；
`stepClock()` 上升緣時，SET 為 1 → Q←1；否則 RESET 為 1 → Q←0；否則 Q←F。這取代了版本 1/2 的
`ff_d_src` 五選一設計（F/G/F_XOR_G/F_AND_G/F_OR_G 直接當 D 輸入），因為交叉比對第三方 SystemC
重現專案（lazardjurovic/xc2064）後發現真實架構是「D 固定＝F，透過 SET/RESET 才是真正可配置的
組合機制」。

版本 2 的 CLB 為 25 bit（`lut_f`/`lut_g`/`ff_d_src`(3 bit)/`mux_x`(1 bit，F 或 Q 二選一)/
`mux_y`(1 bit，G 或 Q 二選一)/`src_a`/`src_b`/`drive_h_long`/`drive_v_long`），版本 1 為前 21 bit
（無長線 4 個欄位）。讀取版本 1/2 時，F/G 輸入槽套用 `(A,B,C)`/`(A,B,D)` 預設值、`set_mode`/
`reset_mode` 套用 `'none'`/`'G'` 預設值，`mux_x`/`mux_y` 的二選一結果對應到三選一的 `'F'`/`'Q'`
或 `'G'`/`'Q'`。

### 2.2 Switch Matrix（row-major，r=0..size, c=0..size，每交點 6 bit）

依 `SWITCH_LINKS` 固定順序：`WE, NS, WN, WS, EN, ES`，每項 1 bit（1 = 導通）。見
`xc2064-reference.md` §3 與 TASK-004 的近似拓樸說明。

### 2.3 水平線段 h_wires（row-major，r=0..size, c=0..size-1，每段 1 bit）

1 bit 表示該段金屬線是否存在（`on`）。

### 2.4 垂直線段 v_wires（row-major，r=0..size-1, c=0..size，每段 1 bit）

同上，垂直方向。

### 2.5 輸入 IOB（依索引 i=0..size-1，每個 6 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `forced` | 1 | 1 = 強制驅動，0 = 浮接 |
| `val` | 1 | 強制驅動時的值（浮接時此位仍會寫入但匯入時被忽略，實際讀值改採 `pull_up`） |
| `pull_up` | 1 | 上拉電阻是否啟用 |
| `threshold` | 1 | 0 = TTL，1 = CMOS（僅供顯示對照，不影響模擬結果） |
| `ff_enabled` | 1 | 是否經輸入正反器同步 |
| `drive_long` | 1 | 是否同時驅動本列全域長線（TASK-015；版本 1 無此欄位，預設 `false`） |

### 2.6 輸出 IOB（依索引 i=0..size-1，每個 2 bit）

| 欄位 | bits | 說明 |
|---|---|---|
| `tri_state` | 1 | 1 = 三態（Output Enable 關閉） |
| `threshold` | 1 | 0 = TTL，1 = CMOS |

### 2.7 全域長線狀態（版本 2 以上；版本 1 無此區塊，預設全部 `false`）

| 區塊 | bits | 說明 |
|---|---|---|
| `h_long[i].on`（i=0..size-1） | 1/列 | 每列一條長線的 on/off（單一開關，非逐段） |
| `v_long[i].on`（i=0..size-1） | 1/欄 | 每欄一條長線的 on/off |

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
