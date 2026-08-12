# Hangboard.html 改進計畫與修復紀錄

檢查範圍：`Hangboard.html`、`js/main.js`、`js/modules/{storage,ui,bleManager,hrManager,timer,goalManager,analytics,views,templates}.js`、`config.js`、`sw.js`、`manifest-hangboard.json`。

檢查日期：2026-08-12｜修復完成日期：2026-08-12

## P0 — 功能性 Bug

- [x] **主題切換 / 目標範圍選單失效**：`Hangboard.html` 的 inline `onchange` 改成在 `js/main.js` 用委派的 `change` 事件監聽器綁定。
- [x] **BLE 測力計讀不到目前訓練區塊**：`js/modules/bleManager.js` 補上 `import { timer } from './timer.js'` 與 `import { recordManager } from './storage.js'`，移除失效的 `typeof` 判斷。
- [x] **目標(Goal) toast 通知會噴錯**：`js/modules/goalManager.js` 補上 `import { showToast } from './ui.js'`。
- [x] **分析頁存檔後不會自動刷新**：`js/modules/analytics.js` 的 `refresh()` 改為直接呼叫同檔案內的 `insightManager` / `bodyManager`，不再依賴 `window.*`。
- [x] **課表範本自訂欄位名稱不一致**：拖拉面板預設欄位、`mathUtils.calcDecayRate`/`calcRelativeStrength` 統一改用「重量」；同時修正預設 PR 卡片種子資料的 `targetItem`（`最大懸垂` → `最大指力`，對齊實際範本標題）。

### 修復過程中額外發現並一併修好的同類 Bug
- `js/modules/hrManager.js`：無 Web Bluetooth 環境時的提示 `showToast('無藍牙環境', ...)` 同樣因未 import 而永遠不會顯示，已補上 import。
- `js/modules/goalManager.js`：`init()` 內 `EventBus.on(APP_EVENTS.RECORD_SAVED, ...)` 因 `EventBus`/`APP_EVENTS` 未 import，導致「儲存訓練後目標進度自動刷新」從未生效，已補上 import 並移除失效的 `typeof` 判斷。

## P1 — PWA 離線功能

- [x] `sw.js` 預快取清單補上 `main.js`、`goalManager.js`、`bleManager.js`、`hrManager.js`、`views.js`、`templates.js`、`tagManager.js`。
- [x] 恢復 Tailwind CDN 的離線快取（原本被註解掉），並將本地資源／CDN 資源分開處理：本地資源仍用 `cache.addAll`（缺一即視為安裝失敗），CDN 資源改為逐一快取、單一資源失敗不影響整體安裝（避免因某個外部資源抓取失敗導致整個離線快取都沒生效）。
- [x] 加強 `fetch` 攔截策略：離線且無快取命中時，頁面導覽請求會退回快取的 `Hangboard.html`，避免完全空白。
- [x] Cache 版本號由 v6 更新至 v7 以強制用戶端更新快取。

## P2 — 安全性

- [x] 新增共用 `escapeHtml()`（`js/modules/storage.js`），並套用到所有找得到的使用者輸入插值點：課表標題/標籤、目標標題、自訂欄位名稱、區塊標籤、BLE 歷史重量標籤、標籤庫等，涵蓋 `storage.js`、`views.js`、`ui.js`、`goalManager.js`、`analytics.js`、`bleManager.js`、`main.js`。
- [x] 順手修正 `ui.js` 中一個損壞的 `<option>` HTML（缺少右角括號，原本會把選項文字誤解析成標籤屬性）。

## P3 — 架構 / 效能 / 可維護性 / 無障礙

- [x] 清查全專案的 `typeof x !== 'undefined'` 偽可選依賴寫法：`bleManager.js`、`hrManager.js`、`timer.js`、`ui.js`、`analytics.js`、`goalManager.js`、`storage.js` 全數清理，改為明確 import 或直接呼叫（確認過的 identifier 都已正確 import）。保留 `storage.js` 中兩處合法用法：`typeof crypto`（瀏覽器能力偵測）與 `typeof auth`（Firebase 由 classic `<script>` 載入的全域變數，非 ES module，無法靜態 import）。
- [x] `main.js` 的標籤庫業務邏輯（`loadTagLibrary`、`renderTags`、`saveTagToHistory`、`updateHistoryTagsData`）抽成新模組 `js/modules/tagManager.js`，`main.js` 恢復為純事件委派/wiring，並額外消除了「新增標籤」在點擊與 Enter 鍵兩條路徑上的重複邏輯（皆改呼叫 `tagManager.confirmTag()`）。
- [x] 刪除 `views.js` 中重複定義、永遠不會被呼叫到的第一份 `prCard()`。
- [x] `insightManager.renderChart()`（數據洞察頁的力量曲線圖）加入資料指紋比對，並在資料有變動時改用 `chart.data = ...; chart.update()` 就地更新，取代原本每次勾選 A/B 數據點都整個銷毀重建 Chart.js 實例的做法。
- [x] Esc 鍵支援：在 `main.js` 新增依疊放順序關閉目前開啟中彈窗的邏輯（BLE 目標視窗 → 屬性編輯面板 → BLE 配對視窗 → 身體數據/PR/目標編輯 Modal → 課表編輯器），刻意排除進行中的全螢幕計時器畫面以避免誤觸中斷訓練。
- [x] 補上圖示按鈕的 `aria-label`：目標/PR卡/身體數據編輯 Modal 的 ✕ 關閉鈕、屬性編輯面板的 ✕ 關閉鈕、全螢幕計時器工具列（自動紀錄、手動紀錄、BLE、心率、麥克風）。
- [ ] **Modal 完整 focus trap（Tab 鍵循環鎖定焦點）：刻意保留未做。** 本 App 是手機觸控優先設計（viewport 已停用縮放、大量 swipe-to-close 手勢），各 Modal 顯示/隱藏機制彼此不一致（`hidden`/`open`/`translate-y-full` 三種模式混用），要做到不出錯的通用 Tab 焦點鎖定需要對七種以上 Modal 分別驗證，且無法在此環境用瀏覽器實際測試鍵盤互動，貿然實作有引入新迴歸的風險。已完成的 Esc 關閉與 aria-label 是風險較低、價值明確的部分；焦點鎖定建議之後在瀏覽器內實際測試後再補上。

## 額外發現、記錄但未動手修的問題（超出原始範圍）

- `js/modules/storage.js` 的 `recordManager.closeDetail()` 讀取 `document.getElementById('detail-sheet')` 並直接呼叫 `.classList`，但 `Hangboard.html` 裡根本沒有 `id="detail-sheet"` 或 `id="modal-day-detail"` 的元素——這兩個 id 完全不存在於目前的 HTML。實際上因為唯一呼叫路徑 `initSwipeToClose('detail-sheet', ...)` 在找不到元素時會安全地提前 return，這段死碼目前不會被觸發、也不會造成任何使用者可見的錯誤，但代表「當日訓練明細」曾經是一個可滑動關閉的 Modal，後來 UI 改成看板內嵌區塊（`detail-list`/`detail-date-title`）之後，舊的 Modal 收尾程式碼被留下來沒清掉。是否要整個刪除或是重新接上，屬於產品決策，故未在本次一併處理。
- `Hangboard.html` 中的 `modal-record-editor`（含 `record-editor-close`/`record-editor-save`/`record-editor-form`）同樣是完全沒有任何 JS 開啟、綁定或填值的孤兒 Modal。是否要實作或直接刪除，同樣需要產品決策。

## 不處理（超出範圍）

- `i18n.js`、`holdfocus-engine.js`：確認未被 Hangboard.html / main.js 引用，屬於 index.html / HoldFocus.html 的功能，與本工具無關。

---

## 驗證

- 所有修改過的 `.js` 檔案皆以 `node --check`（ES module 模式）驗證語法正確。
- 已 grep 確認被移除的舊函式（`loadTagLibrary`/`saveTagToHistory`/`updateHistoryTagsData`/`getActiveTagTab`）沒有任何殘留呼叫點。
- 已 grep 確認所有新增的 `escapeHtml` import 皆能對應到 `storage.js` 的 export。
- **尚未**在真實瀏覽器中手動測試（此環境無法啟動瀏覽器），建議之後實機測試：主題切換、目標編輯器範圍切換、BLE/心率連線提示、標籤庫新增/歷史刷新、Esc 關閉各 Modal、離線模式（開發者工具 Offline 模式重新整理）。
