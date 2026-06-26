# FORA 生理量測系統 — 研究紀錄

**日期**：2026-06-26  
**作者**：chanwunkong  
**Repository**：https://github.com/chanwunkong/SideTest  
**頁面網址**：https://chanwunkong.github.io/SideTest/side/fora.html

---

## 專案背景

協助醫護人員在病房現場使用手機連接 FORA 品牌藍牙生理量測設備，自動接收量測數值並上傳至醫院系統 API。目標是一個免安裝、開啟即用的 PWA 網頁應用。

---

## 技術架構

| 項目 | 選擇 | 原因 |
|------|------|------|
| 前端框架 | 純 HTML + Tailwind CSS (CDN) | 免建置環境，直接開啟即用 |
| 藍牙通訊 | Web Bluetooth API | 瀏覽器原生支援，無需 App |
| 條碼掃描 | html5-qrcode v2.3.8 | 支援 QR Code 與 Code 128，使用手機相機 |
| 部署 | GitHub Pages | 靜態頁面免費托管 |
| 離線支援 | PWA (Service Worker) | 支援加入主畫面、基本離線架構 |
| 本機紀錄 | localStorage | 保存最近 100 筆上傳歷史 |

**執行環境限制**：Web Bluetooth API 僅支援 Android Chrome，iOS Safari 不支援。

---

## 支援設備與藍牙協議

### 1. 紅外線額溫槍 TD-1242B / IR42
- **BLE Service**：`0x1809`（Health Thermometer）
- **Characteristic**：`0x2A1C`（Temperature Measurement）
- **資料格式**：IEEE 11073 32-bit FLOAT（Flags + 3-byte mantissa + 1-byte exponent）
- **解析**：`temperature = mantissa × 10^exponent`，Bit0 of Flags 判斷 °C / °F

### 2. 血氧濃度計 TD-8255B / FORA O2
- **BLE Service**：`00001523-1212-efde-1523-785feabcd123`（TaiDoc 自訂）
- **Characteristic**：`00001524-1212-efde-1523-785feabcd123`
- **資料格式**：8 bytes，Header `0x51`，Command `0x25` / `0x26`
  - Byte 2：SpO2（%）
  - Byte 5：心率（bpm）

### 3. 2合1血糖血壓套組 TD-3261B
- **血壓 Service**：`0x1810`（Blood Pressure），Characteristic `0x2A35`
  - 格式：IEEE 11073 SFLOAT，Byte 1-2 收縮壓，Byte 3-4 舒張壓，Byte 14 心率（Bit2 of Flags 確認是否含心率）
- **血糖 Service**：TaiDoc 自訂（同上），封包格式同血氧機

### 4. 血液六合一 FORA MD6
- **BLE Service**：TaiDoc 自訂
- **資料格式**：同血氧機封包，Byte 2-3 組合為 16-bit 量測值
- **備註**：六合一指標（血糖、尿酸、總膽固醇、血酮、血紅素、血球容積比）由試紙類型決定，BLE 封包格式相同，無法從封包自動判斷指標類型

---

## 上傳 API

**端點**：
```
POST https://www.sstcmedicare.com/imedical/multi/division/v2/api/repository/uploadPhysiologicalData?location={ward}
```

**格式**：`multipart/form-data`，單一欄位 `data`，值為 JSON 字串

**Payload 結構**：
```json
{
  "keyno": "0000030658",
  "uploadTime": "2026-05-11 22:50:31",
  "temp": 36.8,
  "pluse": 72,
  "spo2": 98,
  "sbp": 120,
  "dbp": 80,
  "glu": 100,
  "weight": null,
  "resp_rate": null
}
```

**欄位對照**：

| API 欄位 | 說明 | 來源設備 |
|----------|------|----------|
| `keyno` | 病歷號 | 手動輸入 / 條碼掃描 |
| `uploadTime` | 量測時間 | 自動記錄（最後一次量測時間） |
| `temp` | 體溫 | TD-1242B |
| `pluse` | 心率 | TD-8255B / TD-3261B |
| `spo2` | 血氧濃度 | TD-8255B |
| `sbp` | 收縮壓 | TD-3261B |
| `dbp` | 舒張壓 | TD-3261B |
| `glu` | 血糖 | TD-3261B / FORA MD6 |
| `weight` | 體重 | 手動輸入 |
| `resp_rate` | 呼吸率 | 手動輸入 |

---

## 功能實作紀錄

### 初版
- 體溫計、血氧機各自獨立連線卡片
- 系統日誌即時顯示
- 無上傳功能

### v2（第一次升級）
- 加入全部 4 台設備（TD-1242B、TD-8255B、TD-3261B、FORA MD6）
- 加入手動輸入模式切換
- 加入待上傳紀錄列表（舊版 mock，未實際呼叫 API）
- 加入病歷號輸入欄位

### v3（商業化改版）
- UI 全面改版：分頁式設計（每台設備一個 Tab）
- 系統日誌預設隱藏，點選 Header 圖示展開
- 加入條碼掃描（html5-qrcode）支援 QR Code + Code 128
- 加入病房（location）切換
- 實作真實 API 上傳（FormData POST）
- 上傳後詢問是否繼續下一位病患
- Sticky Header + Sticky 底部行動列
- 底部顯示量測值 Badge 摘要

### v4（全功能完善）
- 修正 Bug：移除 `Html5Qrcode.start()` 中無效的 `supportedScanTypes` 參數
- 上傳前預覽 Modal：一次看到全部量測值確認後再送出
- 上傳成功／失敗改在 Modal 內顯示（移除 `alert()` / `confirm()`）
- 新增「📋 紀錄」分頁：本機 localStorage 保存所有上傳歷史（最多 100 筆）
- 補充資料區塊：體重、呼吸率手動輸入
- 各 Badge 加入 × 按鈕可個別清除量測值
- Toast 通知系統取代所有原生彈窗
- 連線失敗 Toast 提示
- 掃描成功震動回饋（`navigator.vibrate`）
- 上傳中 Spinner 動畫
- BLE 不支援時顯示警告橫幅
- Location 改為文字輸入（預設 `yt`，YT 快速重設按鈕）

---

## 遇到的問題

### CORS 跨來源問題 ✅ 已解決
- **現象**：Postman 可以成功 POST，瀏覽器出現「Failed to fetch」
- **原因**：頁面在 `chanwunkong.github.io`，API 在 `sstcmedicare.com`，不同來源，瀏覽器安全政策攔截
- **解法**：由 API 提供方（Kevin）在伺服器端加入 CORS Header
- **狀態**：已解決（2026-06-26，Kevin 確認）

### keyno 無效問題（2026-06-26 測試發現）
- **現象**：數值成功上傳至系統，但 keyno 顯示「找不到」，資料無法對應到正確病患
- **截圖**：後台「今日檢測詳情」可見資料列（謝小易，體溫 36.2 / 36.9，心率 66，血氧 66），但 keyno 關聯失敗
- **原因**：測試使用的 `keyno` 不存在於醫院系統的病患資料庫中
- **結論**：API 本身正常接收資料；`keyno` 必須使用系統中已存在的有效病歷號
- **待確認**：API 回傳的錯誤格式（目前前端對 keyno 無效無特別提示）

### 重複上傳問題（2026-06-26 測試發現）
- **現象**：後台出現兩筆相同時間（03:25）、相同數值的紀錄
- **可能原因**：使用者按了兩次確認上傳，或 BLE 設備在連線期間推送兩次相同量測值
- **建議**：上傳成功後立即禁用確認按鈕，避免重複送出

---

## 待辦與已知限制

- [x] CORS 問題 — 已由 Kevin 解決
- [ ] keyno 無效時前端應顯示明確錯誤提示（目前僅顯示 HTTP 狀態碼）
- [ ] 重複上傳問題：成功後應鎖定按鈕防止重送
- [ ] FORA MD6 六合一無法從 BLE 封包自動判斷量測指標類型
- [ ] iOS 完全不支援 Web Bluetooth，無法使用藍牙功能
- [ ] `uploadTime` 目前記錄最後一次量測的時間，非每個指標各自的時間
- [ ] 病房代碼（location）目前為自由輸入，尚未與後端確認有效值清單
