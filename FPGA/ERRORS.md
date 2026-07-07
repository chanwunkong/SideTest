# Error Log

### ERR-001 [resolved]
- **Date:** 2026-07-07
- **Track:** XC2064
- **Task:** TASK-004
- **Command / file:** `FPGA/FPGA.html` — `simulateCombinatorial()` 佈線網格擴散段落
- **Error message:** （非執行期錯誤，屬架構正確性問題）舊版程式碼只要交點兩側的線段皆存在（`.on === true`）就自動視為水平/垂直直通合併數值，完全不檢查 `switch_box` 狀態；也就是說直通連接永遠開啟，使用者其實無法關閉它，只有「轉角/短接」才受 switch matrix 控制
- **Root cause:** TASK-001 原型把「線段是否存在」與「switch matrix 是否導通」兩個獨立概念混在一起判斷，直通情形被當成線段存在的必然結果，而非可程式化決策
- **Fix applied:** 隨 TASK-004 重構為 6 組獨立可程式化連接（WE/NS/WN/WS/EN/ES），全部一律需要 `switch_box[r][c][key] === true` 才會合併兩端數值，直通與轉角一視同仁
- **Follow-up task:** (none)
