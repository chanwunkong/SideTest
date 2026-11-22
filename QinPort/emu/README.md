# 用 x86 模擬器直接跑原版：可行性驗證（2026-11-22 開始）

**不影響線上版本**：這個資料夾是獨立的實驗，不被 `engine/`、`web/` 引用，8643 上的版本維持不動。

## 工具
- `imports.py`：統計原版每個檔案要替換的系統匯入（Poq.exe + 6 個 DLL 合計去重約 1000 個，其中 user32 264、kernel32 234、gdi32 148、ole32 83、msvcp140 58…）。
- `loader.py`：用 Unicorn（x86 模擬）載入 Poq.exe、omutil、mfc140、easyrpg、gfx3d、mfx、uiwndex（皆為遊戲資料夾裡的原版檔），重定位、解析匯入、其餘由主機端提供。需 `pip install unicorn pefile capstone`。

## 目前進度
- 七個模組載入、重定位、匯入解析成功；已有 TEB／PEB／GDT 與 kernel32／UCRT 的基本主機函式。
- DllMain（omutil）開始跑 CRT 初始化，呼叫了約 9 種主機函式後在 0x40014000 出錯（呼叫了一個沒映射的位址），還在查。Poq 進入點 0x9c9310 出現 CPU 例外（0x9cb3e3），還在查。
- 還沒到「建立視窗」或「讀第一個遊戲檔」。

## 已知缺口（要提供的東西）
- 遊戲資料夾沒有 msvcp140.dll、vcruntime140.dll、ucrtbase（api-ms-win-crt-*）、d3dx9_43.dll；要自己實作或取得微軟再發行版。
- mfc140.dll 會再呼叫約 800 個 user32／gdi32／ole32 函式，需要一個視窗管理員與繪圖子集的主機端實作。
- Gfx3D 用 DirectDraw 7（CDD）＋ Direct3D 9，需要 COM 物件（vtable）的主機端實作。
