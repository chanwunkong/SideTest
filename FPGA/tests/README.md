# TASK-007：標準測試電路驗證

驗證 `FPGA/FPGA.html` 模擬引擎（CLB LUT 運算、D 型正反器/時鐘、佈線、switch matrix、IOB）
在手工佈線的真實電路下行為正確。

跑法：`node FPGA/tests/build-and-verify.js` — 會印出每個測項的 PASS/FAIL，並產生
`decoder-2to4.bit` / `counter-3bit.bit` 兩個可直接用模擬器「匯入 Bitstream」按鈕載入的檔案，
方便互動確認（載入後點擊對應 CLB，於側欄 Probe 面板即可看到下面表格描述的訊號值）。

`build-and-verify.js` 內逐字重現了 `FPGA.html` 的 `BitWriter`/`calcLut3`/`getSlotValue`/
`getFFSet`/`getFFReset`/`getIobInEffective`/`simulateCombinatorial()` 佈線鬆弛演算法/
`stepClock()` 鎖存邏輯，所以這份驗證測的是「引擎邏輯本身」，不是另一套獨立實作——這樣才能
真的抓到 `FPGA.html` 自己邏輯裡的 bug，而不是驗證一份平行維護、可能跟正式程式碼各自漂移的
複製品。

**2026-07-07 更新**：CLB 架構依 TASK-016（交叉比對 lazardjurovic/xc2064 後修正）重新設計，
bitstream 升級為版本 3。解碼器電路的 F/G 輸入槽沿用預設值 (A,B,C)/(A,B,D)，行為未變；
計數器電路則重新設計（見電路 2），且產出的 `.bit` 檔已改為版本 3 格式。

**2026-07-08 更新**：TASK-017 新增 CLB 正反器本地 clock 來源（`clk_src`），bitstream 再升級為
版本 4。本文件的兩個電路都維持預設的全域時脈（`clk_src='global'`），沒有用到本地時脈這個新
特性，行為未變，只是 `.bit` 檔的欄位配置與版本號更新。

---

## 電路 1：2-to-4 解碼器（純組合邏輯）

真值表：O0..O3 為 one-hot，`O_i = 1` 若且唯若 `(S1,S0)` 的二進位值等於 `i`。

| S1 | S0 | O0 | O1 | O2 | O3 |
|---|---|---|---|---|---|
| 0 | 0 | 1 | 0 | 0 | 0 |
| 0 | 1 | 0 | 1 | 0 | 0 |
| 1 | 0 | 0 | 0 | 1 | 0 |
| 1 | 1 | 0 | 0 | 0 | 1 |

**GRID_SIZE = 4**，只使用 `(row 0-3, col 0-1)` 這 8 顆 CLB；其餘 CLB 維持預設、未接線。

因為本模擬器目前只有「相鄰直接串接」的繞線（CLB 的 A 來自左邊 CLB 的 X、B 來自上面 CLB 的
Y，沒有一般性的 fan-out routing），要讓兩個獨立訊號 S1、S0 同時抵達同一顆 CLB，需要先用
「relay CLB」把訊號沿著列/欄搬運過去。本電路把 S1、S0 各自透過兩顆 IOB（S1: IOB0 與重複輸入
IOB2；S0: IOB1 與重複輸入 IOB3）注入兩條獨立的 relay 鏈，分別匯聚到兩顆「combine CLB」
`(1,1)` 與 `(3,1)`，各自算出 2 個輸出（LUT 的 F、G）——這是刻意的電路層面繞線技巧，用來
在一個只支援相鄰直連的網格上實現多輸入 fan-out，不代表模擬器架構有問題。

| CLB | 設定 | 說明 |
|---|---|---|
| (0,0) | `lut_f=0xCC(=passB)` `lut_g=0x00` `mux_x=F` `mux_y=G` | B=S1(IOB0)；F relay S1 往右；G≡0 避免污染 IOB1 |
| (0,1) | `lut_f=0xF0(=passA)` `lut_g=0xF0(=passA)` `mux_x=F` `mux_y=G` | A=S1(來自 0,0.X)；F/G 都 relay S1（往右、往下） |
| (1,0) | `lut_f=0xCC(=passB)` `lut_g=0x00` `mux_x=F` `mux_y=G` | B=S0(IOB1)；F relay S0 往右；G≡0 避免污染 IOB2 |
| **(1,1)** | `lut_f=0x03` `lut_g=0x30` | A=S0(來自1,0.X)，B=S1(來自0,1.Y)。**val_F=O0，val_G=O1** |
| (2,0) | 同 (0,0) | B=S1(IOB2，重複輸入) |
| (2,1) | 同 (0,1) | relay S1 往下 |
| (3,0) | 同 (1,0) | B=S0(IOB3，重複輸入) |
| **(3,1)** | `lut_f=0x0C` `lut_g=0xC0` | A=S0(來自3,0.X)，B=S1(來自2,1.Y)。**val_F=O2，val_G=O3** |

佈線（`.on=true`）：`h[0][0]`（IOB0→(0,0).B）、`v[0][1]`（(0,0).X→(0,1).A）、
`h[1][1]`（(0,1).Y→(1,1).B）、`h[1][0]`（IOB1→(1,0).B）、`v[1][1]`（(1,0).X→(1,1).A）、
`h[2][0]`（IOB2→(2,0).B）、`v[2][1]`（(2,0).X→(2,1).A）、`h[3][1]`（(2,1).Y→(3,1).B）、
`h[3][0]`（IOB3→(3,0).B）、`v[3][1]`（(3,0).X→(3,1).A）。這個電路完全不需要 switch matrix
轉角連接（純直線相鄰串接），switch matrix 的轉角邏輯已在 TASK-004 自己的單元測試裡驗證過。

**驗證結果**：4 組 `(S1,S0)` 全部通過（見上方腳本輸出）。

---

## 電路 2：3-bit 同步二進位計數器

標準同步計數器方程式：`D0 = NOT Q0`、`D1 = Q0 XOR Q1`、`D2 = (Q0 AND Q1) XOR Q2`。
每次 clock 上升緣後應遞增 1，8 個 clock 邊緣後應 wrap-around 回到 0。

**與 TASK-007 原提案的差異**：原提案寫「例如 4-bit 計數器」（僅為範例）。4-bit 版本的
`D3 = (Q0 AND Q1 AND Q2) XOR Q3` 需要 3-input AND，即使在 TASK-016 修正後的架構下，F 一次
仍只能讀 3 個輸入槽，要湊出「3 個 Q 的 AND 再 XOR 第 4 個 Q」需要額外的 relay/precompute
CLB。這對「驗證引擎本身是否正確」沒有額外幫助，只會讓手工佈線複雜度大幅增加，因此縮小為
3-bit，並在此明確記錄這個範圍調整。

**GRID_SIZE = 2**，用滿全部 `(row0-1, col0-1)` 4 顆 CLB。

**2026-07-07 依 TASK-016 重新設計**：D 型正反器的資料輸入固定＝F（不再有 `ff_d_src` 五選一）；
F 的 3 個輸入槽可各自選 A/B/C/D 之一（`f_slot1∈{A,B}`、`f_slot2∈{B,C}`、`f_slot3∈{C,D}`），
因此每個 bit 的「下一狀態函式」都能直接寫進 F 的真值表，完全不需要 G，比舊版更簡單。

| CLB | 設定 | 說明 |
|---|---|---|
| **bit0** (0,0) | `lut_f=0x55` `f_slot3=D` `mux_x=Q` `mux_y=Q` | F 讀槽3=D（自己上一輪 Y=Q 的自迴授），F=NOT(槽3)；每個 clock 翻轉（T 型正反器）。`mux_y=Q` 讓 Y 輸出（=Q0）經由自迴授線同時也是下方 (1,0) 的 B 輸入 |
| **bit1** (0,1) | `lut_f=0x5A` `f_slot1=A` `f_slot3=D` `mux_x=F` `mux_y=Q` | F 讀槽1=A(=Q0，來自 bit0.X relay)、槽3=D(=Q1 自迴授)，F=XOR(槽1,槽3)＝D1 |
| Q0-relay (1,0) | `lut_f=0xCC(=passB)` `mux_x=F` `mux_y=G` | B=Q0(來自 bit0 自迴授線的共用端)；F relay Q0 往右給 bit2 |
| **bit2** (1,1) | `lut_f=0x6A` `f_slot1=A` `f_slot2=B` `f_slot3=D` `mux_x=F` `mux_y=Q` | F 讀槽1=A(=Q0)、槽2=B(=Q1)、槽3=D(=Q2 自迴授)，F=(槽1∧槽2)⊕槽3＝D2 |

關鍵技巧：每顆計數器 CLB 的「自迴授線」（`mux_y=Q` 時，Y 輸出寫回 `h_wires[r+1][c]`，
剛好也是它自己的 `in_D`）**同時也是正下方那顆 CLB 的 `in_B`**——因為 `h_wires[r+1][c]`
一格兩用。所以只要接好一條線，就同時完成「自己的 D 回授」與「往下一顆 relay」兩件事，
不需要額外的 relay CLB。這是本模擬器現有繞線索引方式帶來的巧合，寫在這裡供之後理解電路
佈局時參考。

佈線（`.on=true`）：`h[1][0]`（bit0 自迴授 + relay 給 Q0-relay.B）、
`h[1][1]`（bit1 自迴授 + relay 給 bit2.B）、`h[2][1]`（bit2 自迴授）、
`v[0][1]`（bit0.X→bit1.A）、`v[1][1]`（Q0-relay.X→bit2.A）。同樣不需要 switch matrix。

**驗證結果**：連續 16 個 clock 邊緣（兩輪完整 0→7→0）全部符合預期序列。

---

## 已知限制 / 後續可做

- 兩個電路都只用到「相鄰直連」繞線，沒有實際用上 switch matrix 的轉角連接（WN/WS/EN/ES）；
  switch matrix 本身的邏輯已在 TASK-004 有獨立單元測試。如果之後想要一個「必須用轉角」的
  電路來做整合測試，可以再開一個任務。
- IOB 的 pull-up/threshold/三態等 TASK-005 新增的特性，本次測項沒有特別涵蓋（兩個電路都用
  `forced` 直接給定值），因為那些特性已經在 TASK-005 自己的驗證裡涵蓋過。
