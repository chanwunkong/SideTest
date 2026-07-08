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
- Commit: 819e557

## 2026-07-07T05:00Z
- Track: XC2064
- Completed: TASK-006 — 新增 `BitWriter`/`BitReader` 逐 bit 封裝/解析、`serializeBitstream()`/`deserializeBitstream()`/`applyLoadedState()`，把 CLB/switch matrix/wires/IOB 配置打包成自訂的 6-byte header + bit payload 格式；新增匯出（下載 `.bit`）/匯入（讀檔重建）UI；新增 `FPGA/docs/bitstream-format.md` 明確區分模擬用格式與真實 XC2064 格式
- Errors: none
- Queued: TASK-007 [XC2064]（標準測試電路驗證）—可利用本次的匯出/匯入功能保存測試電路配置檔
- Commit: 149bb77

## 2026-07-07T06:00Z
- Track: XC2064
- Completed: TASK-007 — 新增 `FPGA/tests/build-and-verify.js`（逐字重現引擎邏輯，非平行複製品）驗證 2-to-4 解碼器（4 組測項全過）與 3-bit 同步計數器（16 個 clock 邊緣、兩輪 wrap-around 全過）；產出對應 `.bit` 檔並反向確認位元內容正確；`FPGA/tests/README.md` 記錄電路佈局、真值表、範圍調整（4-bit→3-bit）與已知限制（未涵蓋 switch matrix 轉角）
- Errors: none（兩電路的手工佈線一次到位，未觸發任何修正）
- Queued: TASK-015 [XC2064]（Long Lines 評估）是唯一剩餘的 XC2064 Active 任務；[GAME] 軌自建檔以來尚無已完成任務，依 CLAUDE.md §2 建議下一輪優先切換到 [GAME] 軌（例如 TASK-008 學習路徑大綱）
- Commit: bea99fc

## 2026-07-07T07:00Z
- Track: XC2064
- Completed: TASK-015 — 評估後決定實作最簡版全域長線：每列/每欄各一條 `h_long`/`v_long`，單一 on/off；CLB 新增 `src_a`/`src_b`（讀長線取代一般繞線）與 `drive_h_long`/`drive_v_long`（額外驅動長線），IOB 輸入新增 `drive_long`；`drawLongLines()` 橘色虛線視覺化；bitstream 升級為版本 2（CLB 25 bit、輸入 IOB 6 bit、新增長線 on 區塊），`deserializeBitstream()` 同時支援版本 1/2，已驗證 TASK-007 舊 `.bit` 檔仍可正確匯入
- Errors: none
- Queued: [XC2064] 軌目前無排定任務（TASK-002~007、015 皆已完成）；下一輪依 CLAUDE.md §2 建議切到 [GAME] 軌，優先 TASK-008（學習路徑與關卡大綱）
- Commit: eba646a

## 2026-07-07T08:00Z
- Track: XC2064
- Completed: 使用者要求「先驗證 XC2064 是否真實」——重新查證 Shirriff 逆向工程文章的更細節部分（CLB 輸入多工器、正反器 SET/RESET、IOB 輸出正反器、direct interconnect、全域時脈網路），更新 `xc2064-reference.md` §2/§7：
  - 發現本模擬器的真實落差：F/G LUT 輸入寫死為 (A,B,C)/(A,B,D)，但真實晶片是每個一般輸入都透過可程式化多工器選擇來源，F/G 各自用哪 3 個輸入也可配置 → 新增 TASK-017
  - 發現確認存在但未實作的真實特性：正反器有 SET/RESET 非同步控制線 → 新增 TASK-016
  - 確認 TASK-005「不實作 IOB 輸出正反器」的保守決定是對的（來源明確表示這塊仍未被逆向工程清楚）
  - Direct interconnect 仍無法確認，維持待確認
- Errors: none
- Queued: TASK-016（SET/RESET）、TASK-017（LUT 輸入可程式化，與「更容易操作」的 UX 討論一併決定範圍）；同時準備向使用者提出操作性改善的方案選項
- Commit: f39855c

## 2026-07-07T09:00Z
- Track: XC2064
- Completed: 用 AskUserQuestion 提出 4 個操作性改善方向（電路範本庫／初學進階模式／引導式教學／拖曳接線），使用者選擇「全部都要做」。依成本/風險排序寫入 TASKS.md：TASK-018（範本庫）→ TASK-019（模式切換）→ TASK-020（引導教學，需等前兩者穩定）→ TASK-021（拖曳接線，風險最高排最後）
- Errors: none
- Queued: 依序執行 TASK-018 起
- Commit: e032086

## 2026-07-07T10:00Z
- Track: XC2064
- Completed: TASK-018 — header 新增「載入範例電路」下拉選單，內嵌 TASK-007 兩個電路的 base64 bitstream，選取後重用既有的 `deserializeBitstream()`/`applyLoadedState()` 載入；側欄新增引導提示
- Errors: none
- Queued: TASK-019（初學／進階模式切換）
- Commit: dd0cbef

## 2026-07-07T11:00Z
- Track: XC2064
- Completed: TASK-019 — 新增 `.advanced-only` class + `uiMode`/`applyUiMode()`/`toggleUiMode()`，header 新增模式切換按鈕；長線設定、IOB 上拉/閾值/三態預設收起，LUT 真值表、輸出來源、D-FF 輸入來源、switch matrix 編輯器維持一律可見；隱藏純為 CSS，底層資料/模擬邏輯不受影響
- Errors: none
- Queued: TASK-020（引導式教學 Onboarding）
- Commit: f00b42d

## 2026-07-07T12:00Z
- Track: XC2064
- Completed: TASK-020 — 新增 6 步引導教學（目標電路由「AND 閘」縮小為「反閘」，理由同 TASK-007 的計數器縮小：本引擎相鄰直連繞線無法讓兩個不同外部訊號進同一顆 CLB，需要 4 顆 CLB 的 relay 網路，教學步驟太多）；`TUTORIAL_STEPS`/`tutorial` 狀態機、canvas 浮動教學面板、`drawTutorialHighlight()` 畫布高亮、`.tutorial-highlight` 側欄高亮、`checkTutorialProgress()` 每幀自動驗證進度；順便補齊 `NOT_B`/`PASS_B` 真值表預設
- Errors: none
- Queued: TASK-021（拖曳式接線，四項操作性改善中最後一項，風險最高）
- Commit: 58e73f1

## 2026-07-07T13:00Z
- Track: XC2064
- Completed: TASK-021 — 拖曳式接線。先確認關鍵設計限制：本引擎相鄰直連繞線意味著「沿線點亮所有線段」是電性錯誤的假連接，改用 BFS 圖搜尋（節點=每段 h/v wire，邊=6 組 switch matrix 連接，不論目前是否已開啟）找真正連通的最短路徑；新增 `findWirePath()`/`getWireNeighbors()`/`applyWirePath()`/`findDragSourceAt()`/`findDragDestinationAt()`/`connectDragToWire()`；互動改為 mousedown/mousemove/mouseup 三段式，非拖曳起點的點擊完全比照原行為（呼叫既有 `handleClick()`），確保零回歸
- Errors: none
- Queued: [XC2064] 軌四項操作性改善（TASK-018~021）與驗證後續（TASK-016/017）皆已完成或待辦；[GAME] 軌自建檔以來尚無任何完成任務，下一輪強烈建議切過去
- Commit: cd75543

## 2026-07-07T14:00Z
- Track: XC2064
- Completed: 使用者提問「通道與 CLB 是不是只有包圍、沒有連接」，順著使用者提供的 GitHub 連結 [lazardjurovic/xc2064](https://github.com/lazardjurovic/xc2064)（第三方 SystemC 重現專案）深入查證，用 curl 直接抓取 `clb_one.hpp`/`clb_pips.hpp`/`switching_matrix.hpp`/`switching_block.hpp`/`fpga.hpp` 原始碼。發現多項比 Shirriff 部落格更具體的架構細節，已記入 `xc2064-reference.md` §2/§6/§7：
  - D 型正反器的 D 輸入固定＝F，真正可配置的是 RESET（G 或 D-OR-G）與 SET（無或 F）——與本模擬器 TASK-003 的 `ff_d_src` 設計思路不同，值得跟 TASK-016 一起重新設計
  - X/Y 輸出應各自三選一（F/G/Q），本模擬器目前是兩選一
  - 正反器 clock 來源可能可選全域時脈或本地 C 輸入，本模擬器完全沒有這個選項
  - F/G 的輸入槽是「每槽二選一」的多工器樹（例如 F 第 3 槽只能選 C 或 D），比先前認知的「完全自由指派」更受限但也更具體
  - CLB 與繞線通道的真實連接是一整棵 PIP（可程式化互連點）樹，每個輸入/輸出有多個候選實體節點，比本模擬器「一側一條固定線」精細得多——這是使用者問題的直接技術背景
  - 附註第三方專案 `process_comb()` 的 f-mux 輸入選擇段疑似有變數命名筆誤，提醒這是佐證而非原廠圖，不宜照單全收
- Errors: none
- Queued: 這些新發現待使用者決定是否要求實作；尚未新增/修改任何 TASKS.md 任務項目，僅更新參考文件
- Commit: 0c87307

## 2026-07-07T15:00Z
- Track: XC2064
- Completed: 使用者要求「動手修」。將前一輪發現拆成 TASK-016（F/G 輸入槽可程式化 + D=F 固定 + SET/RESET + X/Y 三選一，合併處理因為互相牽動）與 TASK-017（本地時脈來源，延後）。實作 TASK-016：新增 `getSlotValue()`/`getFFSet()`/`getFFReset()`，`calcLut3()` 呼叫改用槽位解析，`stepClock()` 改為 SET/RESET/D=F 邏輯，`mux_x`/`mux_y` 改三選一；Sidebar 新增槽位/SET/RESET 下拉，Probe 面板新增 SET/RESET 訊號顯示；Bitstream 升級版本 3（相容讀取版本 1/2，但註明舊版非 F 的 `ff_d_src` 讀入後正反器行為會改變，屬刻意接受的已知限制）；重建 TASK-007 計數器電路（新架構下更簡單，F 直接算出目標函數不需 G）與 TASK-018 內嵌範本，解碼器電路因用預設槽位值而完全相容
- Errors: none（新舊電路皆一次驗證通過，過程中無需除錯）
- Queued: TASK-017（本地時脈來源，延後）；[GAME] 軌仍待切換
- Commit: ea52175

## 2026-07-08T00:00Z
- Track: XC2064
- Completed: 使用者選取 TASK-017（先前標記「延後，待使用者確認要做再排入」，選取本身即滿足條件）。新增 `clk_src`（'global'|'C'）+ 執行期 `prev_in_C`；新增 `updateLocalClocks()` 每幀偵測 `in_C` 的 0→1 上升緣並鎖存；`stepClock()` 的全域邏輯改為只影響 `clk_src==='global'` 的 CLB；Sidebar 新增 clock 來源下拉（advanced-only，附精確度警告）；canvas 新增橘色「C」角標；Bitstream 升級版本 4（相容讀取 1/2/3，一律預設 clk_src='global'，無破壞性差異）；重建 TASK-007/018 的 .bit 檔與內嵌範本至版本 4
- Errors: none
- Queued: [XC2064] 軌目前無排定任務；[GAME] 軌自建檔以來仍無任何完成任務，強烈建議下一輪切過去
- Commit: (pending)
