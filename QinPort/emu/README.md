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

## 2026-11-22 進度
- 修正自寫 `_initterm` 機器碼（非檢查版誤把 nop 改成 0x07 = `pop es`，導致跳進 .rdata）；**OmUtil.dll 的 DllMain 已能完整跑完**（回傳 1）。
- 新增 capstone 反組譯的「最近 70 條指令」環，出錯時自動列出。
- 未實作的系統函式改為「記錄並回傳 0 繼續」（注意：stdcall 引數個數未知時堆疊會歪，仍需逐個補實作）。
- **發現**：`Poq.exe` 檔頭進入點 0x9c9310 位於 `.bind` 區段（熵 7.98，加密外殼，會用 int3/SEH 反除錯）；真正的 CRT 啟動在 0x401834（跳到 `__scrt_common_main_seh` 0x5a7a43）。loader 已改從這裡起跑，略過外殼。
- 現況：`mfc140.dll` 的 DllMain 跑到呼叫 `vcruntime140.dll` 內部函式 `__vcrt_InitializeCriticalSectionEx`（遊戲資料夾沒有 vcruntime140，需自己實作整批 `__vcrt_*`／msvcp140）。尚未到視窗建立、尚未量到檔案讀取模式。

## 2026-11-22 第二輪：跑到視窗、讀到第一個遊戲檔
**怎麼跑**：`python fetch_redist.py ../game`（從遊戲自帶的 `_CommonRedist/vcredist/2019/VC_redist.x86.exe` 用 bsdtar 解出真正的 vcruntime140／msvcp140／ucrtbase 到 `redist/`，不進 git），再 `MAXINS=250000000 python loader.py ../game 0`（約 2 分鐘）。`HOST_CRT=1` 可改回主機端自寫 CRT。

**關鍵發現**
- Poq 真正的 CRT 進入點是 **0x5a7dc7**（`WinMainCRTStartup`，由增量連結跳板 0x40611d 呼叫），之前的 0x401834 只是跳板表的一項。
- 遊戲自帶的 `mfc140.dll`（14.0.23026）**是對的**：匯出表有 14448 個序號，是 pefile 的 `symbols` 漏讀（只給 9268 個）。loader 改成直接讀 EAT。
- 載入**真的** ucrtbase／vcruntime140／msvcp140 比自己重寫 CRT／iostream 划算：locale、printf、stdio、C++ 例外都是原版程式碼，只需要補 kernel32（約 300 個主機函式）。
- C++ 例外需要 x86 SEH：已實作 `RaiseException`／`RtlUnwind`／`RtlCaptureContext`，用「主機端 generator ↔ guest 呼叫」的機制（TRAMP 位址 + 以 esp 丟棄被 unwind 掉的框架），同一機制也用來從 `CreateWindowEx` 回呼 WndProc、CBT 掛鉤。
- `libvlc_new` 回傳 NULL 會讓 Poq 丟 `std::exception("Wrapping a NULL instance")`；libvlc／OpenAL 給非空假控制代碼即可。

**進度**：七個原版模組 + 真 CRT 的 DllMain 全過；Poq 起動→MFC 註冊視窗類別→`CreateWindowEx`（800x600，style 0xc20000，WM_NCCREATE/NCCALCSIZE/CREATE/SHOWWINDOW 都走完）→ `DirectDrawCreate`／`SetCooperativeLevel`／`CreateSurface(primary)`（COM 假物件，DDraw7 + Surface7 vtable）→ 開始讀遊戲檔。跑 2.5 億條指令時還在跑（沒有崩潰、沒有未實作函式），停在 Res 封包索引掃描中。

**檔案存取模式（全都在「視窗建好之後」、第一個訊息迴圈之前）**
1. `Data/music.txt`：一次整檔讀 3250 B（`fopen` 風格）。
2. `Res/Combination.json`：整檔讀 887 B。
3. 依序掃描封包 `Res/ResUI.lib`、`Res00.lib`…`Res04.lib`（ResUI 90 MB）：先 `seek` 到檔尾附近（ResUI 為 offset 90037046）讀索引／檔尾資訊，再以 **約 512 B 的小塊**反覆讀索引表，每讀一次前後夾大量 `seek`（同一位置重複 seek，應是 std::ifstream/filebuf 每次都 seekoff）。250M 指令內：ResUI 132 次讀 71 KB／8854 次 seek；Res01 408 次／212 KB／26516 次 seek；Res02 476 次／247 KB；Res03 933 次／481 KB／60097 次 seek。總計 14 萬筆檔案事件。
- 結論：**啟動期**讀的是各 `.lib` 封包的**索引**（小塊、隨機 seek），不是資源本體；資源本體（每場景）要等進到場景後才讀，這一輪尚未觀察到。對網路版的意義：不能整包下載 90 MB 封包，需 HTTP Range（檔尾索引 + 按需區段）或預先把索引拆出來；每場景的實際區段要在下一輪（過了索引掃描、進主迴圈）再量。

**主機函式數量**（loader.py）：`@api` 302、`@crt` 192（僅 HOST_CRT 模式／fallback）、`API[...]=` 表式 119，另有 `winapi_sig.py` 801 條 stdcall 簽章（未實作者清堆疊回傳 0）、COM 方法 ≈80 個（DDraw7 30 + Surface7 49 + Clipper/Palette）。仍只靠「預設回傳 0」的：BugTrap 全部、version.dll、CoInitialize、CompareStringA 等。

**剩餘缺口**
- Direct3D9（`Direct3DCreate9` 還是預設回 NULL）與 D3DX9：IDirect3D9(17 方法)／IDirect3DDevice9(119 方法)／Texture／VertexBuffer 等 COM 與 12 個 d3dx 函式；GetProcAddress 查不到的 kernel32 新函式（SubmitThreadpoolWork、GetUserDefaultLocaleName…）暫回 NULL。
- DirectInput8、WS2_32/BNNet（連線）、OpenAL 實際音訊、VLC 播放、Gdiplus、uxtheme、IME 都是 0 回傳。
- 還沒進入訊息迴圈：GetMessage 閒置計數、計時器（WM_TIMER／timeSetEvent）、輸入事件注入未做；視窗標題解碼（cp950）顯示亂碼待查。
- 速度：約 2 百萬指令／秒（Python Unicorn hook 開銷）；索引掃描就要 2 分鐘以上，需關掉 ring/trace hook 或改用 UC_HOOK_BLOCK。

**下一步 3 件事**
1. 把 `_ring` 指令 hook 改成可關閉，並加 `MAXINS` 提高到數十億，讓索引掃描跑完，量「進主迴圈後、每場景」的檔案讀取（Range 清單）。
2. 補 `Direct3DCreate9`→IDirect3D9/IDirect3DDevice9/Texture9 COM 假物件（vtable 序已知）與 D3DX 12 個函式，讓 Gfx3D 初始化完成。
3. 實作訊息迴圈驅動（WM_TIMER／WM_PAINT／滑鼠鍵盤注入）與 DirectInput8，進入遊戲第一個場景並記錄場景載入的檔案區段。
