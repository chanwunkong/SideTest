# QinPort 交接文件

《秦殤》（Prince of Qin，2002，目標軟件）的**原生重製引擎**。不用 Box86/Wine 等轉譯器，用 C + SDL2 從頭寫引擎，直接讀使用者自己在 Steam 買的原版資源檔。

**目前的目標：做成線上手機版。**也就是網頁版（`make web`），手機橫向、觸控遊玩，透過 Tailscale 連 Mac 上的伺服器。所有工作都以手機網頁版能玩、能測為準。

**方向（使用者 2026-11-12 決定）：隨時用手機玩原版劇情，而且之後能交給 AI 修改、擴充。**
- 不用串流；Mac 只當伺服器提供檔案，遊戲在手機瀏覽器裡跑。
- 原版程式先用 Ghidra 整個反編譯成 C（`decomp/`，只在本機當參考，不進 git）。查規則一律查這份，不要再用 radare2 一段段讀機器碼（太耗 token）。
- 劇情（房間、出口、NPC、對話、任務、事件）用工具從 `EasyRPG.dll` 批次抽成看得懂的劇情檔，引擎讀劇情檔來跑。AI 之後擴充就是改劇情檔。
- 功能照「從開場一路玩下去」的順序補：開場房間 → 對話框 → 跟 NPC 對話 → 任務 → 道具 → 存檔。
- 使用者原話：「我要的是復刻遊戲，而不是要你重新推理遊戲」。照原版的程式翻，不要自己設計。
- 「擴充」指的是**改玩法**，像葉舞的秦殤改版：新打造系統（普系、五行系，材料可疊加、圖紙、煉化、強化）、調降精氣消耗、前期難度和升級速度、新怪物、BOSS 和高等級地圖、提高等級上限、顯示傷害和經驗、商店調整、銷毀裝備換經驗、高解析度、連線組隊。所以：
  - 數值表（道具、技能、怪物、招式、經驗）一律讀原版的文字表；改版放在另一個 mod 資料夾，同名檔案覆蓋原版，原版檔不動。
  - 遊戲規則（傷害、命中、升級、掉寶、打造）集中在看得懂的地方，數字不要散在程式各處。
  - 地圖、怪物用原版格式讀，新地圖、新怪物照同樣格式加進 mod 資料夾就能用。

## 原則（使用者的要求）

- **目標是復刻原版**：遊戲行為、數值、手感都以原版為準。技術選擇自己決定，依據是原版資源或執行檔的分析結果，不要一直問使用者選哪個方案。只有真的需要使用者實機測試或回報手感時才請他看。
- **平台**：線上手機版（上面說的目標）。Mac 原生版只是開發測試用。平板、R36S 掌機都暫停，不要做。
- **輸入**：所有輸入（滑鼠、觸控、手把、鍵盤）都轉成同一套指令。介面依螢幕邊緣排版，不寫死 800×600 座標。
- **授權**：遊戲資源只從使用者的正版讀取，絕不打包或散布。網頁測試版只開在 Tailscale 內網。
- 動畫、圖片、音效一律用原版的，不重畫。

## 目錄

| 路徑 | 內容 | 進 git |
|---|---|---|
| `QinPort/game/` | 原版遊戲檔（SteamCMD 下載的 Windows 版，App 970500） | 否 |
| `QinPort/engine/src/res.c/.h` | 讀資源：封包、圖片、動畫、場景、模型、主角 PLI | 是 |
| `QinPort/engine/src/main.c` | 主迴圈、繪製、角色與 NPC、場景切換、輸入 | 是 |
| `QinPort/engine/src/audio.c/.h` | 音樂串流、音效 | 是 |
| `QinPort/engine/Makefile` | `make`（Mac 原生）、`make web`（Emscripten）、`make r36s`（aarch64 Linux） | 是 |
| `QinPort/r36s/` | PortMaster 版：`QinPort.sh` 啟動檔＋`qinport/qin`（產出，不進 git）＋`qinport/game/`（使用者自己放） | 只有 .sh |
| `QinPort/tools/*.py` | Python 工具：libtool（封包/圖片）、maptool（場景渲染）、roletool（主角疊圖）、roomtool（DLL 房間表）、webserve（網頁版伺服器）、webpack（舊的整包精簡，已不需要） | 是 |
| `QinPort/web/shell.html` | 網頁版外框 | 是 |
| `QinPort/web/game/`, `web/index.*` | 網頁版產出 | 否 |
| `QinPort/extracted/` | 解出來的圖，除錯用 | 否 |
| `QinPort/decomp/` | Ghidra 反編譯的 `Poq.exe.c`、`EasyRPG.dll.c`（每個函式前有 `// ==== 位址 名稱`） | 否 |
| `QinPort/tools/ghidra/DumpC.java` | 反編譯腳本，用法見「建置與測試」 | 是 |

分支：`qinport`，已推到 origin。這台 Mac 沒有 `gh`，所以還沒開 PR。
**注意**：磁碟上的資料夾名是小寫 `qinport`，git 記錄的是 `QinPort`。新增檔案要用完整路徑 `git add QinPort/…/檔名`，`git add QinPort` 不會把新檔加進去。

## 建置與測試

```bash
cd QinPort/engine && make                          # Mac 原生版
cd QinPort && ./engine/qin game CHENG001.Scn       # 800×600；加 --640 是 R36S 視野
./engine/qin game xianyang --shot /tmp/a.bmp       # 畫面穩定後存檔結束（自動測試用）
./engine/qin game xianyang --warp 2 --shot /tmp/b.bmp  # 站到第 2 個觸發區，測場景切換

# 網頁版（手機透過 Tailscale 測）：遊戲檔不打包，進場景時才跟伺服器批次要
cd engine && make web
cd .. && python3 tools/webserve.py web game 100.83.41.116 8642   # Mac 的 Tailscale IP；只記 404
# 手機打開 http://100.83.41.116:8642/  （?scene=別名&warp=N 可直接測觸發區）


# R36S（暫停；PortMaster，ArkOS／ROCKNIX／muOS 都適用）
cd engine && make r36s        # zig 交叉編譯；第一次會下載 Debian 11 arm64 的 SDL2 標頭到 engine/build/
# 複製到 SD 卡：r36s/QinPort.sh → ports/；r36s/qinport/ → ports/qinport/；game/ → ports/qinport/game/
# 不用實機的測試：Lima 的 Debian arm64 VM（limactl shell qin，~/qin 有遊戲檔）
limactl copy r36s/qinport/qin qin:/tmp/q && limactl shell qin bash -c 'cd ~/qin && cp /tmp/q qin && SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./qin game xianyang --640 --shot /tmp/a.bmp'
```

- 反編譯（約 1 小時，Ghidra 12 要 JDK 21 以上）：
  ```bash
  export JAVA_HOME=/opt/homebrew/opt/openjdk@25/libexec/openjdk.jdk/Contents/Home
  for f in EasyRPG.dll Poq.exe; do /opt/homebrew/Cellar/ghidra/12.2/libexec/support/analyzeHeadless /tmp/ghproj qin -import game/$f -overwrite -scriptPath tools/ghidra -postScript DumpC.java "$PWD/decomp"; done
  ```
- 依賴：Homebrew 的 `ghidra`、`sdl2`、`emscripten`、`radare2`（逆向用）、`zig` 和 `lima`（R36S）。
- 自動測網頁版：`/Users/martini/github/grid-icon/node_modules/playwright` 搭配 `chromium.launch({channel:'chrome'})`，用 iPhone landscape 裝置模擬。範例腳本在 `/tmp/qintest/t.cjs`，可能已被清掉。
- `.bmp` 截圖轉 PNG：見 `tools/libtool.py` 的 `png()`。

## 已解開的檔案格式

### 加密
`ini/*.ini`、`Data/*.txt`、`ObjMdl/*.txt` 的第一個 byte 是 `0x80`，其餘每個 byte XOR `0x78`，解出來是 GBK 文字（道具表等用 tab 分隔）。

### Lib 封包（`Res/*.Lib`、`Bei.lib` 等）
- `"Lib 1.01"`；0x10 = 項目數 u32；0x100 起每項 `(offset u32, size u32)`。
- 項目可以再包一層 Lib。
- `test.lib`、`test2.Lib` 是另一種格式 `DistanceLib`，還沒拆。

### 全域資源編號（`Res/Combination.ini`）
編號落在哪個區間，就屬於哪個封包；編號減掉區間起點 = 封包內的序號。

| 起點 | 封包 | 起點 | 封包 |
|---|---|---|---|
| 0x000000 | ResUI | 0x600000 | Res05 |
| 0x100000 | Res00（動畫定義） | 0x700000 | Res06 |
| 0x200000 | Res01（建築圖） | 0x800000 | Res07 |
| 0x300000 | Res02 | 0x900000 | Res08 |
| 0x400000 | Res03 | 0xA00000 | ResUI2 |
| 0x500000 | Res04 | 0xA80000 | ResUI5 |
| | | 0xB00000 | ResUI3 |
| | | 0xC00000 | ResUI4 |

### xbm 圖片
- 0x14 = 寬、0x18 = 高；0x20 = 透明色（RGB565 0xF81F）。
- 0x40 起每列一個 u32 位移（相對 0x40）。
- 每列是交替的 u16：透明 bytes 數 → 不透明 bytes 數 + RGB565 像素 → …，直到填滿寬度。

- **半透明 xbm**：0x24 = 1 時，0x40 起先是 w×h bytes 的 alpha（長度在 0x28），列位移表接在後面（相對位移表開頭）。特效和約 1/10 的建築圖是這種。0x24 = 0x01000001 的位移表仍在 0x40，0x28 的用途還沒查。

### xbmgroup
0x10 = 幀數；0x18 起是各幀相對位移，每幀是一張 xbm。圖片 ref 的高 8 位 = 幀號（從 1 起），低 24 位 = 全域編號。

### 動畫（Res00 的項目）
- u32 = 序列數。
- 每序列：`(幀數, ?, ?)` 三個 u32，接著每幀 7 個 u32：`(?, 25?, 圖片ref, ?, ?, 腳底x, 腳底y)`。
- 繪製位置 = 錨點 −（腳底x, 腳底y）。
- **序列 = 動作 × 8 + 方向**。動畫方向從「右」開始逆時針排。程式裡的換算：畫面方向（0 = 上，順時針）→ 動畫方向，對照表是 `{2,1,0,7,6,5,4,3}`，已用手機實測確認。

### 場景
- **`.Scn`**（GBK 文字）：第 1 行 `Map名称=`，第 2 行 `IntBld名称=`，第 3 行 `IntObj名称=`，第 4 行 `入口点数=`；之後每個入口點依序是 `前场景名称、x、y、角色初始方向`。
- **`.MAP`**：
  - `"SoloMap"`；0x22 = 寬、0x24 = 高（u16，單位是格）；0x90 = 磁磚封包名。
  - 0x196 起是 u16 對照表（目前都是恆等，長度不固定）。
  - **檔尾 w×h 格**，每格 10 bytes：`(地面磁磚 u16, 疊加磁磚 u16, 可走 u8（1 = 可走）, ?, ? u16, 填充)`；0xFFFF = 空。
  - 磁磚是 64×32 的菱形，投影公式：`sx = (x−y+h−1)·32`，`sy = (x+y)·16`。
- **`.BNT`**：0x22 = 模型檔名；0x480 起每筆 `(類型 u16, 模型編號 u16, x u32, y u32, 4 bytes, 名稱長度 i32)`，接著名稱（−1 = 沒有）。類型 4 是觸發區，名稱後再接 128 bytes 事件文字，例如 `go east`。全部 BNT 都驗證過長度。
- **`.MDL`**：
  - 模型 = `03 00` + 名稱[32] +（部件數 u16, 模型編號 u16, …）。
  - 部件 = GBK `部件\0` + 3 bytes + 動畫旗標 u8 + **全域編號 u32** + dx, dy（i32）。
  - 動畫部件的編號落在 Res00（動畫定義），一般部件落在 Res01。
  - 錨點 = 格子中心（菱形左上 + 32, +16）。
- **`.ONT`**（GBK 文字）：NPC 和物件的擺放。**還沒做。**

### 主角紙娃娃（PLI）
- `PLI/0000_001.PLI` 是文字：`#define SPR_ROLE_<角色>_<部件>[S = 影子]_<款式>_<PLI檔號>\t<全域編號>`。
- `PLI/<角色>_<檔號>.PLI`：
  - 0x22 = 部件數 n（u8）；0x28 起 n 筆 `(部件名[5], 款式[5])`。
  - `0x28 + n×10 + 0x20` 起：u16 序列數；每序列一個 u16 幀數，接著每幀 n bytes 的疊圖順序（部件索引）。（主角 n = 11 時剛好是 0xb6）
  - 部件：CA HD LA LE LF RA RE RF RH SD TK。
  - `_001` 是平時，`_002` 是戰鬥（推測）。
- `ObjMdl/RoleMdl.txt`：角色表。遊俠圖形 ID = 1201，`平時休息 = 0`、`平時走路 = 8`、`戰鬥休息 = 16`、`戰鬥走路 = 24*64`（都是動作起點的序列號），平時速度 = 8，戰鬥速度 = 8。其他職業：力士 1206、刺客 1203、術士 1204。

### 角色與 NPC
- **`ObjMdl/RoleMdl.txt`**（XOR 加密，tab 分隔）欄位：0 名稱、1 ID、2 圖形 ID（= PLI 檔號）、3 屬性（每個部件的款式，依 PLI 部件順序，`000` = 不畫）、5 動作方式（= PLI 的 `_00N`）、7 平時休息、8 平時走路、20 平時速度。全部 441 個角色的屬性數都等於部件數。
- 主角用圖形 1201 那一列（ID 29）的屬性 `009*…*001*000*009`（布衣，沒有盾），不是 PLI 的預設款式。
- **`.ONT`**（GBK 文字）：每個物件以 `iType=` 開頭，類型 = 平時邏輯：
  - 12：站立
  - 13：在 `x, y, cx, cy` 範圍內閒逛
  - 14：沿 `PathMapX/PathMapY` 路線巡邏
  - 15：跟隨（`a, s, d, x, y`）
  - 共用欄位：`模板ID`（= RoleMdl ID）、`地图坐标X/Y`、`方向`、`事件ID`（對話用）。
- 圖形 ID 是名字（例如 `ZHIZHU`）的是怪物，用另一套格式，還沒做。

### 聲音
- 音樂 `Music/*.wav`：PCM，取樣率不統一（22.05k 或 44.1k），最大 45MB。引擎用 `SDL_AudioStream` 邊讀檔邊轉換，循環播放。
- `Data/music.txt`（XOR）：場景別名 → 背景音樂、戰鬥音樂。找不到就用 `normal1`。
- 音效 `sound/NNN.wav`：22.05k 單聲道，檔名 = 音效編號。腳步聲 80 號（某些地面 81 號，地面類型還沒對應）。
- 音量照 `ini/Config.ini` 的 `[音频]`：音效 100、音乐音量 15（百分比）。已用 SDL disk 驅動驗證輸出 = 原檔 × 15%。

## 原版執行檔逆向（`Poq.exe`，PE32 i386，radare2）

反編譯結果只在本機給自己參考，不散布。

**角色結構欄位**（從除錯字串 `m_iB_C_MOVE_SPEED…` 在 0x477613 的用法得出；結構是 pack(1)）：

| 欄位 | 位移 |
|---|---|
| `m_nNormalSpeed` | +0x6f |
| `m_nFightSpeed` | +0x3fb |
| `m_iSpeedCent` | +0x569（預設 100） |
| `m_iB_C_MOVE_SPEED` | +0x541 |

**GetSpeed**（0x488540，虛擬函式表第 13 格，位移 0x34）：
```
if ([+0x40f]) s = [+0x40f] - 1                         // 強制速度
else if (旗標 0x10000 且 [+0x73] == 0xb) s = 平時速度 * 3 / 2
else s = 戰鬥中 ? 戰鬥速度 : 平時速度
s = s * 速度百分比 / 100（兩者都非 0 時最小為 1）
s = s * (100 + B_C_MOVE_SPEED) / 100
if ([+0x2e3] > 100) s /= 2                            // 推測是負重過重
```
- 呼叫端（0x493fbe、0x4941f4、0x494205、0x494444、0x48b2d5）把 s、s×2、s×3、s×4 傳給 **SetMoveSpeed（0x496ae0）**。

**SetMoveSpeed（0x496ae0）**：
- 呼叫 0x4c9050 查速度對照表（速度夾在 0..59），回傳三個值 `(A, B, C)`：
  - A 表在 0x5f21d0，B 表在 0x5f23b0，C 表在 0x5f22c0。
  - 意義是「每 B 拍移動 A + C/10 個單位」，規律為**每拍移動 速度 ÷ 3**。速度 8 → (2, 1, 7)，也就是每拍 2.7。
  - 結果存在 [+0xab]、[+0xaf]、[+0xb3]。
- 接著比較（目前速度 / 平時速度）和門檻 5、4、3、2、1.5、1，得到 −5…−1，推測是加快動畫播放的級數，讓腳步跟上移動速度。

**節拍與單位（已確認，引擎已照做）**
- 主迴圈 0x449f50：`while (經過 >= 16ms) { 邏輯一拍; 經過 -= 16 }` 再畫面。**一拍 = 16ms**。
- 對照表輸出：A → [+0xaf]、B → [+0xab]（拍數）、C → [+0xb3]。每拍計數 [+0xbb] % B == 0 時移動。
- 位置是像素（GPOINT [+0x34]/[+0x38]，浮點累加 [+0x3dd]/[+0x3e1]），每拍移動 A + C/10 **畫面像素**，各方向等速。遊俠 = 每秒約 169 像素（網頁實測 167）。
- **動畫每幀的第 2 個 u32 = 停留拍數**：站立是 25、10…，走路是 3、2、2…。速度比調整：比值 ≥ 1.5 時延遲減 1～5，< 1 時加 1～7（0x496b61 起）。
- 腳步聲：每 (15 − A) × 2 拍一次（0x48704f）。
- 0x503f20 的 250/500ms 是介面閃爍；0x404e2b 是音量。`游戏速度`（0x4f4b06）的用途還沒追。

### 劇情程式的結構（從 `decomp/EasyRPG.dll.c` 看，2026-11-12）
- `tools/decomp_strings.py` 把反編譯裡的 GBK 字串換回中文（反編譯後要跑一次）。`decomp/EasyRPG.dll.vtables.txt` 是每個類別的虛擬函式表（`tools/ghidra/DumpVtables.java`，用 `-process EasyRPG.dll -noanalysis` 跑）。
- 每個 NPC 是一個類別（例如绣女 = `CERPerson02724`），由 `FUN_1007f970(工廠, 路徑, 1, 0)` 登記。虛擬函式表第 1 格 = 設定（路徑、名稱、別名、`FUN_10023c90(屬性, 值)`），第 17～42 格 = 對話處理，其餘是基底類別的預設。
- 對話：`FUN_100542e0(文字)` 顯示一句；`FUN_1005f780(鍵如 "ask3", 處理, 0,0,0, 選項文字)` 加一個回答選項；處理是 `mov eax,[ecx]; jmp [eax+N]` 的小跳板 = 呼叫同一個 NPC 的第 N/4 格；`FUN_1005df70` 回傳任務狀態（例如 `"shuxiu"`，−1 = 沒接）。
- 約 5000 個函式（10 萬行）是劇情，常用的引擎函式約 40 個。
- **注意**：Ghidra 把部分引擎函式的呼叫方式猜錯（ecx 傳 this 的 `__thiscall` 被當成一般函式），所以參數會錯位，例如 `FUN_100558b0("shuxiu")` 其實只是取主角、字串是下一個呼叫的參數。自動轉換前要先在 Ghidra 把這些函式設成正確的呼叫方式再重新反編譯。
- `tools/ghidra/ParamId.java`（匯入前開 Decompiler Parameter ID）已修好上面的呼叫方式問題：現在 `get_flag(player(), "shuxiu")` 這類參數是對的。
- **`tools/storytool.py` 已能產生劇情檔**：`python3 tools/storytool.py game decomp story` → `story/<room|npc|item|skill|plot>/<路徑>.c`，共 2539 個物件（房間 491、NPC 1208、道具 553、技能 206、劇情事件 81），加 `story/index.txt`。`story/` 是原版程式翻出來的，跟 `decomp/` 一樣只留本機、不進 git。檔案裡 `<別名>_s<N>` = 該物件虛擬函式表第 N 格；選項處理已換成對應格的方法名；引擎函式依 `API` 表換名，沒命名的還是 `FUN_位址`。
- **劇情已能執行（2026-11-12）**：`engine/src/story/`：`qinstory.h`（劇情檔共用：Ghidra 型別、引擎函式巨集）、`story_rt.c`（照原版的物件模型：具名整數屬性、旗標、裝的東西、對話選項；引擎函式依原版位址在 `engine_call` 分派，還沒翻的第一次呼叫會印出位址）、`story.h`（給引擎：`story_new`、`story_talk`、`story_choose`、`story_dialog`）。
  - 引擎函式一律經 `engine_call(位址, 參數[8])`，劇情方法被當選項處理傳遞時用 `<名>_w(intptr_t *a)` 包裝：WebAssembly 不容許參數不符的呼叫。
  - **劇情只在 32 位元下才對**（原版把 −1 寫成 `0xffffffff` 等），所以只編進網頁版；測試也用 WebAssembly：`cd engine && make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1`（參數是依序選的選項）。`STORY_TRACE=1` 會印出每個引擎呼叫。
  - 绣女沒接任務時的對話已照原版跑出來。主角的類別（`秦殇/主角`）還沒找。
- **網頁版已能跟 NPC 對話（2026-11-12）**：點有「事件ID」的 NPC（.ONT 的事件ID = 劇情 NPC 的別名）→ `story_room(場景別名)` 產生房間（第 1 格的 `add_npc` 放進 NPC）→ `story_talk` → 畫面下方對話框，點選項 `story_choose`。文字用原版字型 `font/16Point.fnt`（0xB4 起依 Unicode 每字 32 bytes，16 列 × 16 位元，0 = 筆畫；`13Point` 每字 26 bytes；檔尾每字 2 bytes 的表還沒查）。劇情只編進網頁版（`-DSTORY`，有 `../story` 才編；`index.wasm` 因此約 8.7MB）。對話框還不是原版介面圖。
  - 主角類別：劇情模式是 `秦殇/主角/游侠扶苏`（`story_player`）。
- **開場房間（已找到）**：原版 `FUN_1007f970` 的第 4 個參數 = 1 的房間就是新遊戲起點（`0x1003f7d0` 單機時找它），只有 `秦殇/赵村/ROOM/赵村`（別名 `zhaocun1`）。網頁版網址沒帶 `?scene=` 就從這裡開始（`story_start_room`）。站在哪個入口點還沒照原版對（目前用場景第一個入口）；開場劇情「初遇赵茜」怎麼觸發也還沒接。
- 下一步：把 `story_rt.c` 印出的「還沒翻的原版函式」照 `decomp/` 逐一補上；接進 `main.c`（點 NPC → `story_talk`，畫對話框和選項）。
- （舊）讓這些劇情檔能在引擎裡編譯執行。要先照原版補上它用到的物件模型（`CERObject` 的具名整數和字串屬性、`CERPerson` 的旗標、房間裝物件、對話選項）和 API 函式，從 `decomp/EasyRPG.dll.c` 照翻。先以咸陽开場和绣女的對話為第一個能玩的目標。
- 打算的格式：劇情保留成 C（每個 NPC、房間一個檔，引擎函式換成好讀的名字，例如 `say`、`option`、`quest_state`），編進引擎；AI 改劇情就是改這些檔，Mac 重新建置。

### 世界與場景切換（`EasyRPG.dll`，原版劇情引擎）
- 劇情、房間、出口、NPC 都編譯在 `EasyRPG.dll` 裡，結構像文字 MUD。`tools/roomtool.py` 和 `res.c` 的 `world_load` 會掃描每個房間的初始化程式，依序呼叫的設定函式如下：
  - `0x1005d390`：房間開頭
  - `0x10023c60`：路徑，例如 `秦殇/咸阳/ROOM/绣女家`
  - `0x10023cf0`：名稱
  - `0x10023c40`：別名（= 場景檔名，例如 `xy_hs2`）
  - `0x1005d8b0`：出口（方向, 目的路徑, 入口編號）
  - `0x1005f320`：NPC（路徑, 數量）
- 抽出 497 個房間、1015 個出口、1634 個 NPC；484 個別名對得到 `.Scn`。
- 觸發區事件 `go <方向>` 對應目前房間的出口，切到目的房間的 `.Scn`，主角站到該場景「前场景名称 = 入口編號」的入口點。全部場景 2479/2544 個觸發區都對得到；對不到的都在隨機戰場（`go out` 由劇情決定）。
- 真正的咸陽是 `xianyang.Scn`，`CHENG001` 是開發用的測試場景。新遊戲的開場房間是赵村（`zhaocun1`），見「劇情程式的結構」。

### 技能系統（EasyRPG.dll）
- 技能表 skill_N.txt：第 15/16 欄精气消耗 → 技能整數 MPU；17~20 作用範圍；21/22 我方/敌方状态；23/24 作用時間（沒填預設 5）；第 26 欄起每 5 欄一組參數（屬性ID、初始、增量、是否对敌、是否百分比）。值一律 = (技能等級 − 1) × 增量 + 初始（0x100757a0、0x10024xxx）。
- 參數屬性代號 20~99 對名稱表 .data 0x1039b5f0（"Invincible" 起每格 0x40；41 AP、53 DP、60 HR(閃避)、61 SR(命中)、62 HP_SPEED、63 MP_SPEED）；250 起是攻擊加成（0x10033040），ID > 500 拆成 ID−300、ID−299。
- 能不能用（0x1004c2a0）：技能等級 > 0、MP ≥ MPU、武器限制；出手時 Poq.exe 0x48ffe0 送 0x13a → 0x1003f370 扣 MPU。
- 打中（0x1003f590）：有我方状态且身上沒有 → 加 Status_N（"經過秒數 時間 技能"，0x10037f40），技能參數套到身上（0x1002fe20）。
- 重算（0x1002d8d0）：基礎 X0 → 固定加值 → 回復速度（0x1002fa90，依職業）→ 職業倍率（0x1002d510）→ 百分比（加上目前值 × v/100）。
- 系統心跳每秒一次（Poq.exe refresh 指令 → USER_Refresh）：狀態計時（0x10055e90），回 HP/MP = (速度[非戰鬥 ×3] + 零頭)/100（0x10054450）。

- 技能欄位補充：第 6 欄 W_Type（武器限制）、第 11 欄 Attack方式（= 0 不造成傷害，0x10025b70）、第 20 欄作用範圍最大增量。
- 能不能用（0x1004c2a0）還要比主角的 Hand（0x1003b760，右手刀類 8 → 0）：W_Type -1 不限、相同可、0 也收 Hand<0、1 收 2、4 收 5。
- 打中（0x1003f590 → 0x1002fe20）：是否对敌 = 1 的參數套在對方，其他套在自己；我方状态給自己、敌方状态給對方。
- 狀態分類表 .data 0x1039f5c8（11 類 × 13 格）；加狀態時互斥（0x10054d50）：同為 1/3/4/6 類互斥，3 蓋 4、4 蓋 2/3/5、7 蓋 6、11 蓋 10。
- 參數處理函式表 .data 0x1039e628（每個屬性 ID 一格）。252~261 = MAP1~5 / MAP1~5_MAX（最大值用最小值那格的百分比）；269 寫入的變數 dll 裡沒人讀。
- 物理損血最後：總損 ≥ 0（不到 1 點）才補成 3，不是「小於 3 補 3」。

- 屬性分類 0x100364a0：< 20 → 改技能自己的欄位（表 .data 0x1039e8f4：10 AttackID、11 SUR、12 RangeMin、13 RangeMax、14 Time、15 MPU、16 Speed、17 EDR、18 B_C_JDR、19 B_C_DJHF）；20~99、292、293、295 → 立即生效（0x10035fd0，只有 21/22/292/293 改 HP/MP）；200~299 → 攻擊暫存（210~220 偶數 = 眩暈/中毒/冰凍/失明/石化/潰散機率、222/223 吸血/吸精氣、250~261 AP/MAP、287 自損）。
- 技能基底預設（0x1005d4a0）：Level 0、RangeMin 0、RangeMax 1、Time 5、Attack 1、Status/EStatus -1。
- 打中觸發狀態（0x10033140 後段）：機率 → 成功率 = 機率×100/(機率+K) × MAP 該屬性比例；時間 = ((機率+隨機)/D + C) × (100−免疫)/100。狀態 10/1/4/33/2/17，K 200/100/100/100/500，D 30/10/20/15/6，C 3/12/6/8/15；潰散直接機率、7 秒。中毒 HP_SPEED = −8×MAP2；失明命中閃避減半；石化 DP×2。負回復扣到 0 會死。
- 狀態效果表 ObjMdl/RoleEffectMdl.txt（Poq.exe 0x48b2f0）：速度變化、攻击不能、頭/身/腳效果、聲音。加不上的狀態規則 0x10058410。
- 怪物表技能1 後面：打击恢复率、減傷、7 種免疫、是否陷阱、生命恢复率（HP_SPEED = 率 × HP_MAX / 10）；移动速度、攻击频率 → B_C_MOVE_SPEED、B_C_FIGHT_SPEED。
- 速度（Poq.exe 0x488540）：速度 × 狀態% × (100 + v×150/(v+150))/100。攻擊間隔（0x48d330）：主角 × 100/(100 + v×120/(v+120))，怪物 + 隨機(間隔)。
- Attack方式 3 = 召喚（0x100355d0，273 選召喚兽、278 上限）。
- 招式觸發 type（Poq.exe 0x43c4f0）：0 打中（ID 選打法，0x438a00：5~10 目標點周圍 0~5 格、11~13 自己周圍 3 格、30 直接打目標、其他前方第一個）、1 音效、2 招式單位、5/7 衝/跳到目標點、8 角色旗標、11 震畫面、13 速度、16 多個單位（0x14 向前、0x16 四周）。招式單位（MethodUnit_Mdl.ini）自己也有觸發，弓箭等飛到才算打中。

**飛行物、衝刺、召喚、怪物選招（已照做）**
- 招式單位 wManualID 201 = 飛行物（Poq.exe 0x450700 依 dData2 分類，2 = CARROW）。放出時（0x4368c0）拍數 = 距離（一格 64）÷ bData3，每拍等速；拍數用完在目標點觸發打中（0x436d60 → 0x4396d0）。單位觸發 flag 5 = 放出時、1 = 打中時。動畫前 16 個序列是方向，從朝右起逆時針。
- 衝刺／跳躍：type 5、7 → 角色動作 10、7 → 移動狀態 0x15、0x16（0x496550）。跳躍高度用速度/2、每拍 +4。
- 召喚（0x100355d0）：同種的已有上限隻就不召；召前 6 種時先移掉別種（狼、白狼算同種）。0x1003dd80 只是字串比較。
- RangeMin、B_C_JDR、B_C_DJHF 經 0x524 存進 Poq.exe 的 SkillUnit（+0x1c、+0x24、+0x28），Poq.exe 沒有讀。只讀 +0x18 AttackID、+0x20 RangeMax、+0x30 Enable。
- 怪物表有技能1～技能8。選招在 0x4906c0，看招式表的 wManualID（+0x22）和 Reserve3（+0x34）。怪物 AI 主體在 0x4911b0，包含遠程怪拉開距離（GetFleePos），這部分還沒翻。

### 技能補完（2026-11-14）
- 隊伍：Actor.side（0 主角這邊、1 怪物），hostile = side 不同；主人那邊的（召喚、馴服、陷阱、分身）閒逛／追擊以主人為中心，索敵找最近的別隊。
- Attack方式（EasyRPG 0x10025b70）：1、4、10 物理；2 馴服（0x10035370：istrap 10、隊上上限 = 參數 271）；3 召喚；8 陷阱（0x10034cb0：參數 286 選 1 猎杀陷阱～5 无情箭阱，自己的 > 7 不放，AP、MAP 照抄）；9 除障（技能等級 − 陷阱等級/5 ≥ 0 就打掉）；11 分身（0x10034140：克隆刺客刀／弓，只能一個）。5 開鎖、6 偷竊、12 清狀態、13 界面技能沒有技能用到，沒做。
- 陷阱的角色在 ObjMdl/BeastieMdl.txt（小動物表，0 名、1 ID、2 图素ID、5/6 速度、12 索敌），一個動畫 3 序列；RoleDef.anim + role_load_anim。原版陷阱是 Poq 物件種類 3，怎麼觸發沒拆，先用技能作用範圍當索敌。
- 打中反應（Poq 0x48c3a0）：JDR = 攻方 B_C_JDR（ex 74）+ 招式技能欄 18；DJHF = 守方 B_C_DJHF（ex 75，怪物表打击恢复率）+ 守方正在放的技能欄 19；欄 18 = −1 不反應。① JDR>100 擊退＋硬直、② 擊退、③ 挨打機率 (200−DJHF)×1000/(DJHF+200)‰。擊退 = 狀態 0x1f 每次一格（0x493cb0），方向 WhatDir8(守, 攻) = 從攻方指向守方。
- 致命一击／会心一击：攻方百分之 ZMYJ（ex 72）→ 0x13b 旗標 −1 物理 ×2；否則百分之 HXYJ（ex 73）→ −2 不算 DP（EasyRPG 0x10033140）。
- 觸發 14 ID 4 = 震畫面 ID2 拍（0x461180，每 5 拍上下，幅度 寬/640×25×剩/總）；15 = 送 0x13b 結算技能（陷阱招式）；10 = 連鎖（0x43a760：+0x434 次數 > 0 時找範圍內下一個敵人重放招式，次數來源沒查，還沒做）；6 只對物件種類 7，沒做。
- 技能目標看招式 wManualID：2 對同隊（0x43acb0 要同 +0x339）、3 地點、4 放東西、5 陷阱、0/1 敵人。
- 怪物拉開距離（0x4911b0）：每 61 拍、招式 Reserve3 ≠ 1、目標在 (範圍−2) 方框內、n = 範圍 − 距離 − 2 ≥ 2 → 往反方向 8 方向走 n 格（OmUtil.dll GetFleePos，用 objdump 看的）。
- 測試網址可加 `prof=2`（力士）、`prof=3`（刺客）。

## 原版函數總表 FUNCS.tsv（2026-11-15 起的主清單）

`python3 tools/functool.py` 解析 decomp/ 三個反編譯檔（Poq.exe 13,777、EasyRPG.dll 17,983、UIWndEx.dll 362 個函數），
每個函數分類並標狀態，寫 `FUNCS.tsv`（只列要處理的：遊戲函數和還沒完成的劇情函數，依位址 = 原版原始碼檔案順序）。
- 分類：`lib` 執行庫／MFC／ATL／STL／跳板／DLL 匯入轉接／例外處理片段（平台層換掉，不翻）；`story` 劇情物件的格子（storytool 產生）；`game` 遊戲邏輯。
- 狀態：`done` = engine/src 或 story/ 的程式碼裡寫了這個原版位址（**移植一個原版函數就在對應的程式碼註解寫上它的位址**，總表自動算進去）；`skip` = `tools/funcs_skip.txt` 列的（附原因，目前是連線版）；`todo` = 其餘。
- class 欄：虛擬函式表（RTTI）的類別；前面有 `~` 是從呼叫者或位址相鄰推出來的。
- 做法：照 FUNCS.tsv 由上往下，一個類別／一段位址做完（翻譯，或確認已有移植就補註記位址，或列進 skip 附原因），重跑 functool.py，提交。
- 2026-11-15 第一次跑：Poq.exe 遊戲函數 5,368（done 61、skip 140）；EasyRPG.dll 遊戲函數 2,030（done 158），劇情 12,684（done 12,662，剩 7 個基底建構、15 個連線版房間 skip）；UIWndEx.dll 58。
  已完成的大多還沒補位址註記，所以 done 偏低；補註記是工作的一部分。
- 總表第一次跑就抓到漏洞：storytool 的劇情區段起點設成 0x10100000，寶箱／水缸等 94 個類別繼承的第 6 格（隨機產生寶物 0x100f9670 等 0x100f9000～0x100fffff 的共用格子）被當成引擎略過沒翻。已改成 0x100f9000 重新產生。

- 另外兩個表：`tools/funcs_done.txt`（原版函數 → 我們對應的程式，用在建構／解構／畫圖這類不方便逐一註記的）、`init` 狀態 = Poq.exe 0x410000～0x433dc0 的全域初值建構（隨用到的函數一起帶入）。Poq.exe 0x401000～0x410000 是跳板表，算執行庫。
- 進度（11-15）：Poq.exe 0x433dc0～0x43f300 —— 平台層（記憶體堆積、MFC 訊息框）列 skip；**招式單位整段照原版重寫成 engine/src/unit.c**（MethodBase 的計時／幀／打中／撞牆／結束觸發、沿路命中、依 iTriID 的命中範圍 0x438a00/0x439fb0、放出點 0x43bf50、扇形放單位；CNormalEffect、CFlyEffect 拋物線、CARROW、CFlyEffectNormal 落地爆炸、CGhostMan、CFlyCycleObj、CFireDeadMan、CSurFireArrow），取代原本簡化的特效與飛行物。順便修正：角色出手的命中 6～10 是以出手的人為中心（之前寫成目標）。
  還沒做（在這段的 todo）：0x433dc0 啟動（游標動畫、record.rec 的片頭影片 PlayIntro／Rec 錄影）、0x434850 游標動畫、MethodMain（0x43aba0～0x43b6a0，角色出手目前在 main.c action_tick）、wEffect 1 把打中的人帶著走的名單、CPhantasmObj（0x43dd90～，type 16 的 0x14/0x16）、CFootStep、CGale 天氣。unit.c 的 shortcut 註解列了細節缺口（混色、透明度、格子地形種類）。
  發現原版有片頭影片 game/movie/logo1.avi、Start.avi、End.avi，要轉成網頁能播的格式。
- 進度（11-15）：Poq.exe 0x439b20～0x444740 **場景格子查詢**整段移植成 engine/src/near.c（unit.c 引入）：螺旋走格（0x442730/0x442a80，n 圈剛好 (2n+1)² 格）、
  從一點找角色（0x442810/0x443a00，方框照原版是 x−r 起邊長 3r）、點選角色（0x442b10 → monster_at）、直線走到擋住（0x442eb0）、直線通不通（0x443790）、
  8 方向最遠（0x443230）、敵對（0x443560，先用 side）、空格放掉落道具（0x444430 → ground_drop，周圍 10 圈）、陷阱格（0x444740 → trap，10 圈）、找陷阱／建築、格子層 0x435c00（Cell.layer = 地圖第 6 byte）。
  hit_find（0x439fb0）和連鎖找下一個敵人（0x43a760）改成照原版螺旋順序。
  shortcut：原版一格只記一個角色，我們同格幾個都算；建築還不是物件（cell_bld 一律沒有）；組關係表 0x452460/0x453210 沒做；格子 +0x13、陷阱 +0x9f 沒對應。
  skip：0x440830 播 AVI（片頭影片之後用網頁播放另做）、0x4406b0／0x442660 沒呼叫者、0x4426b0、0x4426a0 空函數。
  ~~下一步：0x446390（CZhenMng 陣法）起~~
- 進度（11-15）：Poq.exe 0x446390～0x448a70 —— **CZhenMng 整個類別是死碼**（用 game/Poq.exe 掃過：建構 0x446470、初始化 0x4471a0、更新 0x4476c0、畫 0x446f80 的跳板都沒有任何呼叫或位址引用），全部列 skip，內容記在「劇情沒用到」。
  0x447da0～0x448860 是 std::map／std::vector、0x4489b0 輸入法、0x448610 沒呼叫者，列 skip。
  移植了 0x448a70 遊戲速度（main.c `game_speed_set`：速度 0～5 → 31 − 3×速度；節拍仍固定 16ms）。讀它的 0x449c90、0x566920 和調整它的按鍵（0x455d60 的 +／−）還沒做，所以目前沒有效果。
  ~~下一步：0x448ab0（CGameApp）起~~
- 進度（11-15）：Poq.exe 0x448ab0～0x44e730 **CGameApp**。這個類別大半是平台層（顯示模式、DirectDraw、除錯文字、MFC）和轉接到介面伺服器 0x4fxxxx 的一行函數，都列 skip。移植的：
  - **畫面縮放**（main.c `zoom_*`：0x4493f0、0x44ae10、0x44aed0、0x44af30、0x44bf40/0x44bf99、0x44c970）：PageUp／滾輪往上拉近、PageDown／滾輪往下拉遠，每次 0.04、最大 0.24（中間 52% 放大到全畫面），每拍走 0.04；場景畫完才放大，介面不放大；點場景的座標會換回縮放前。招式觸發 12（來源是角色）拉近 0.24／放回。
  - **對話面對面**（0x44caf0 → `talk_face`）：點 NPC 說話時兩人停下互看。
  - **開局時間 23 點**（0x44e6e0），所以一進遊戲是晚上（原版如此）。
  - 照原版保留：縮放「記住的值」記的是當時的現在值不是目標；浮點數比相等，劇情指令設的目標不是 0.04 的倍數時畫面會來回跳。
  - 這段還是 todo 的 33 個（roles.tsv 都寫了作用），要等後面的系統一起做：程式狀態機 0x448ab0 和進各狀態（0x44d340～0x44d470，等主選單 0x4f5xxx）、載入畫面（0x44dbd0/0x44dc20/0x44dda0，等 0x5022e0）、小地圖探索格存讀（0x44c0d0/0x44c830，格子 +0x13）、存檔縮圖 0x449bf0、鏡頭移到角色（0x44e2a0/0x44e380，等 0x461680）、暫停計數 0x44d9e0、大地圖自動存檔 0x44ce00、頭頂座標 0x44cfe0、劇情模式放開左鍵跳過 0x44b840、劇情指令設縮放（0x4725c0 case 2 → `zoom_set`，還沒接）、淡入淡出 0x44bf00 → 0x4a6770。
  - 手機還沒有縮放手勢（手機操控最後做）。
  ~~下一步：0x44eb10（CPlayerGroupAI）起~~
- 進度（11-15）：Poq.exe 0x44eb10～0x44f410 —— CPlayerGroupAI 的建構 0x44ee50 沒人呼叫（**更正：類別不是死碼**，0x450bc0 直接內嵌建它，當 AI 組的敵人名單，見下一段）。其餘是容器樣板建構／放入、SInitBase／SInitCha 建構解構（SInitCha 預設值記進 fields.tsv）、CScnClient 建構 0x44ec70，全部列 skip，沒有移植的程式。
  ~~下一步：0x44f460（CScnClient 場景物件）起~~
- 進度（11-15）：Poq.exe 0x44f460～0x451120 **CObjMng 建物件**。我們沒有統一的物件管理，原版的工廠（AddObj 0x450960、建角色 0x44f580、AddProp 0x450cc0／0x450f00）對到 mon_make／ground_drop／trap（funcs_done.txt），串列操作（CDMList、分串列 0x44f610、名單放入移除）記 base_map.tsv。移植的：
  - **AI 組關係表**（near.c `grps`、`grp_enemy_add` 0x450bc0／0x44f460、`grp_find` 0x452460、`grp_is_enemy` 0x453210）；Actor 加 `grp`（+0x339），敵對 `is_foe` 改照 0x443560（取代原本用 side 的 shortcut）。角色把 NPC 組的人設成目標時，那組記下它的組為敵人（`foe_grp`，CCharactor 設目標的一段）。劇情 G_AIGroup（actors_sync、訊息 0x51c）設 grp；沒有劇情時怪物都是組 10000（shortcut）。
  - 劇情建角色（0x44fd20 種類 0）：入口點隨機挑（0x4cc9c0，跟我們原本一樣）、角色面向入口方向（新加）。
  - 照原版保留：敵對是單向的，A 是 NPC 組時只查 A 那組的名單。
  - skip：0x451120 預先載入角色動畫（平台層）。
  - 這段還是 todo 的（roles.tsv 寫了作用），等對應系統：0x44f4c0 建築物件（等建築變成物件，near.c cell_bld）、0x44f6d0 倒下留下的東西／碎屍 CPartDeadObj（等 CCharactor 倒下 0x486910）、0x450b30 CBirdFast（等 0x453800）、0x450da0 地上痕跡（等 0x459840）、0x450f70 陷阱設主人（等 CTrapBase）。0x44fd20 的道具／建築／陷阱／隨機建築分支也還沒接。
  - 發現：註解裡提到位址就算 done，所以 0x498c40（unit.c 的 shortcut 註解）其實還沒移植；之前 0x452460／0x453210 也是這樣（這次真的做了）。之後寫 shortcut 註解提到未移植的函數不要寫位址。
  ~~下一步：0x451820（SInitBase/SInitCha 起）~~（`functool.py next` 的位址要加 0x，不然會被當成數量）
- 進度（11-15）：Poq.exe 0x451820～0x454720 **CObjMng 剩下的部分**，沒有要移植的遊戲邏輯：
  - 每拍分派 0x451900／0x4536c0／0x4545e0／0x4525e0（角色 +0x70 = 0x488dd0、+0x14、+0x24 = 0x48c050）、清空 0x451890 → funcs_done.txt 對到 main.c 主迴圈／換場景。
  - 依編號／名字找物件 0x451e80／0x451f70／0x451d20／0x451dd0 → base_map.tsv。CObjMng 各串列的偏移與種類範圍記進 fields.tsv。
  - 容器樣板、沒有呼叫者的（0x452050 依種類找、0x453ab0 角色寫回格子、0x454720…）、預先載入單位圖 0x453360 → skip。
  - 還是 todo：0x453800 每拍在畫面外隨機放鳥 CBirdFast，等 CBirdFast 類別（0x4808b0～0x481ab0）一起做。全域冷卻 0x663ee8 的每拍遞減在 0x4545e0，等 0x48a0xx 移植時一起帶入（fields.tsv）。
  ~~下一步：0x454790（CUserInput 起）~~
- 進度（11-15）：Poq.exe 0x454790～0x457360 **CUserInput 滑鼠／鍵盤**。原版輸入都變成「給主角的指令」（0x4589b0 包成 0xf023 訊息，執行端在 CCharactor），我們直接設 hero->foe／ground_want／走去的格子。移植的（main.c「滑鼠」那段）：
  - **按下記位置、放開才下指令**（0x454eb0／0x454f00 → `click_cmd`）：溃散（17）、盅惑人心（14）不收指令；战斗状态（我們的 fight）點誰都打；平常點敵人打、點其他人對話；再來撿道具；其他 = 走到那格（新加 `point_walk`），地上留痕跡動畫 0x1001bd（0x459840 → unit.c `mark_new`）。Alt 按著不下指令。點到的角色改用 0x442b10 不分邊（`input_char`），原本的 npc_at 拿掉。
  - **游標種類**（0x455d60 → `input_cursor`）：游標動畫 0x100011 的 8 個序列 = 種類 0 平常、1 道具、2 對話、5 不能走、6 攻擊（3、4、7 是物件，還沒有物件）。103 无对话時指著人是平常游標。
  - **速度鍵**（0x455d60）：數字鍵盤 +／= 加快、−／- 減慢 → `game_speed_key`。照原版保留：開局 21 按一次加快就跳到最快 16。讀速度的 0x449c90、0x566920 還沒做，所以還是沒效果。
  - 原版旗標 +0x341 = 身上有狀態 n（0 战斗状态、14 盅惑人心、17 溃散、103 无对话），記進 fields.tsv。
  - shortcut：按住空地仍一直走（手機要用；原版按住拖拉是選人框 0x459260）；對話是馬上說（原版指令 0xf 走過去再說）、只有 ONT 擺的 NPC 能說；走去格子是直線（原版尋路在 CCharactor）；格子 +0x13 沒對應；游標在介面上沒換回平常；道具名字提示（0x143／0x145）沒做。
  - skip：建構解構、空函數、連線版目標頭像（0x4548c0、0x454900～0x454a00、0x455b80）、除錯指令（SetFog／GetSpeed、0x454e20）。
  - 還是 todo（roles.tsv 寫了作用），等對應系統：雙擊鏡頭移過去 0x455580（等鏡頭 0x461530）、滑鼠進出介面 0x455630／0x455640、右鍵按住 0x4556b0（轉向 0xd、0x457be0 右鍵出手、Ctrl 指令 5）。0x455d60 裡還沒接的：數字鍵 1～9 配 Ctrl／Shift／Alt 選快捷技能（0x48d6a0，等快捷技能表 +0x4bb）、方向鍵／游標到邊緣捲畫面（0x45b900，我們的方向鍵是走路）、H 鏡頭回主角（0x4618a0）、Alt 顯示道具名字（0x460ad0）、Shift 指令 3、Ctrl 點地點出手（指令 8）、空白鍵 0x46c260、Esc 0x46bc10、拖拉選人框、劇情模式（0x472e60 = 2）Esc／Enter／空白跳過。
  ~~下一步：0x457670（CGameApp 的指令小函數）~~
- 進度（11-15）：Poq.exe 0x457670～0x45a3a0 **CUserInput 剩下的：選人名單與下指令**。大半是「選到的隊友名單」+0x14 和全域選人名單 0x93f9d4、自己這邊名單 0x93fdac 的維護；引擎還沒有隊友角色（劇情 AddMember 有，main.c 沒有），所以只做單一主角能用的：
  - **敵人狀態框改成滑鼠指著的角色**（0x459ce0／0x459fa0 → main.c `status_target`、`actor_title`）：怪物、NPC、自己都會顯示，沒指著就關。手機（沒有游標）仍用攻擊目標。
  - 下指令 0x4589b0 → funcs_done（click_cmd，直接設 hero）；操控角色 0x4582d0 = hero；換左右手技能 0x4599e0／0x459a90 對到快捷技能視窗。
  - 照原版保留：狀態 102 本來要讓敵人血條不打開，但 0x583e40 傳 0 是「不變」、也沒地方關，所以一樣看得到血。
  - shortcut：指著的角色原版加旗標 0x100（描邊？）、指著物件另外標示，沒做。
  - skip：容器、CString 設定表（RoleData）、建構解構。
  - 還是 todo（roles.tsv 都寫了），**等隊伍系統**（隊友變成角色、隊伍面板）：選人名單 0x457a30／0x458fc0／0x458f30／0x45a000／0x45a030／0x45a100／0x45a3a0／0x4590f0、每拍同步 0x4587a0、拖拉選人框 0x459260、隊伍查詢 0x458400／0x4584c0／0x458550／0x4585c0／0x458620／0x458700、全隊指令 3 0x459170、隊伍面板選人 0x459b30。右鍵出手 0x457be0 等技能表 +0x36 目標旗標對上欄位（和右鍵 0x4556b0 一起做）。
  ~~下一步：0x45a470（CViewPort 鏡頭）起~~
- 進度（11-16）：Poq.exe 0x45a470～0x45b2b0 **CViewPort 前段（12 個函數）**。
  - 移植小地圖格座標換算 0x45ad80 → main.c `viewport_grid_point`；保留負奇數餘數為 −1、整數除法向零截斷，skilltest 加正負座標與奇偶列測試。
  - shortcut：換算尚未接入小地圖視窗，原版地圖 +8 與全域 DAT_00941288 改由呼叫端明確提供；等小地圖狀態一起接入，不猜全域初值。
  - skip 共 6 個：建構、解構、串列節點配置、名單尾端去連續重複、髒矩形與其編譯器續接片段；其中串列 2 個也記入 base_map.tsv。SDL 每幀全畫面重畫代替髒矩形，沒有刪除劇情功能。
  - todo 共 5 個（roles.tsv）：0x45aad0 可見物件排序等統一物件矩形與繪圖名單；0x45ac40 鏡頭設位等 0x45b900 捲動與世界限制矩形；0x45ad30 設視窗等 0x45e820 快取；0x45ae70 分層繪圖等物件虛函式／遮擋旗標；0x45b2b0 小地圖探索遮罩等探索可見資料與遮罩圖資。新偏移記入 fields.tsv。
  - 後續必須保留原版怪現象：鏡頭設位用輸入 Y−16 比較邊界，只有越界時改成邊界值，範圍內仍寫原輸入 Y；排序以物件 +0x34 加 +0x38 降序，相等時新物件插在前面。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`。
  ~~下一步：0x45b6b0（CViewPort 鏡頭捲動）起~~
- 進度（11-16）：Poq.exe 0x45b6b0～0x45b900 **CViewPort 鏡頭捲動段（5 個函數）**。
  - 移植 0x45b6b0 → main.c `viewport_center_grid`、0x45b780 → `viewport_clamp`，skilltest 補中心格、正負座標、邊界及世界小於視窗的測試。
  - 照原版保留：中心格 X 加視窗高/32、Y 加寬/64；右下恰好碰界也減1；先上界再下界，最後向零取偶數，因此奇數下界可能在取偶數後被越過。
  - shortcut：兩個換算函數等待鏡頭格座標快取與世界限制矩形初始化後接入，未改目前遊戲鏡頭。
  - skip 0 個；todo 3 個：0x45b720 等可見物件名單與統一矩形；0x45b830 等格子可見旗標與排序系統；0x45b900 等世界限制矩形初始化與座標轉換/快取系統。用途與新偏移已記 roles.tsv／fields.tsv。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；有未接入函數的 unused 警告。
  ~~下一步：0x45bc50（CGameApp）起~~
- 進度（11-16）：Poq.exe 0x45bc50～0x45c390 **索引推定 CGameApp 的鏡頭更新段（2 個函數，this 實際使用 CViewPort 欄位）**。
  - 兩個皆留 todo，skip 0 個：定時平移／震動更新與角色跟隨；用途記入 roles.tsv，鏡頭跟隨開關、參考點、死區、震動計數與平移目標／步長共10個新偏移記入 fields.tsv。
  - 等鏡頭設位、世界限制矩形初始化、世界／螢幕轉換與格座標快取；定時更新另等自動跟隨模式查詢。未新增遊戲邏輯，未加完成位址註記或 shortcut，未刪除劇情功能。
  - 後續必須保留原版怪現象：平移用距離平方直接比較步長，距離平方等於上次全域值也直接到目標；震動只在剩餘 tick 為5的倍數時移動，正負依 tick/5 奇偶交替，沒有可用場景仍遞減。
  - 跟隨先處理 X 再 Y，超死區用差額加2，移動追趕步長為速度減去向零截斷的速度/4；X捲動分支會覆寫原先存方向的 local_28，後續 Y 方向判斷須按分支保留，不能一律當作原方向。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`（本批僅補索引與待辦）。
  ~~下一步：0x45dc00（CViewPort）起~~
- 進度（11-16）：Poq.exe 0x45dc00～0x45e460 **CViewPort 地表／背景快取與格座標換算段（3 個函數）**。
  - 移植 0x45e460 → main.c `viewport_point_grid`：小地圖點轉場景格，沒有場景回 (0,0)；skilltest 補正負奇偶 Y、奇數場景尺寸與無場景測試。
  - 照原版保留：奇數 Y 先加1再除2，X 的差值除2仍向零截斷，Y 結果最後減1；不把這個換算自行改成既有正向換算的數學反函數。
  - shortcut：反向換算等待小地圖與鏡頭格座標快取接入，場景 +8 與全域原點由呼叫端提供。
  - skip 1 個：0x45e020 背景快取表面搬移與露出條帶髒矩形，SDL 每幀全畫面重畫代替。todo 1 個：0x45dc00 交錯掃描地表、底圖／覆圖與除錯疊圖，等世界／格座標轉換、預設圖與圖資虛擬繪圖介面；目前地表迴圈尚未等價，不標完成。用途與新偏移已記 roles.tsv／fields.tsv；本批沒有新增劇情不用的功能。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；有未接入函數的 unused 警告。
  ~~下一步：0x45e4f0（CMapDialog）起~~
- 進度（11-16）：Poq.exe 0x45e4f0～0x45e710 **CMapDialog 小地圖點選／輸入座標轉格段（2 個函數）**。
  - 兩個皆留 todo，skip 0 個：點圖重新定位鏡頭、輸入座標原地轉格；等小地圖視窗狀態、像素轉格查表及快取初始化，前者另等鏡頭設位與小地圖更新。未新增遊戲邏輯，沒有完成位址註記或 shortcut，未刪除劇情功能。
  - 照原版須保留：輸入先加偏移再乘 float 比例、減視窗偏移，最後才向零截斷；像素轉格用向零除64／32，負餘數另補正，菱形邊界相等時不修正。不能以既有小地圖點轉場景格代替此查表。
  - 點圖入口須同時滿足場景識別非 -1 與 +0xe0 非零；設鏡頭後才複製 +4～+0x14 到 +0x104～+0x114，設全域更新旗標、呼叫更新，再清 +0xc4。用途與20個新偏移記入 roles.tsv／fields.tsv；查表依賴僅讀該單函數並記用途，未擴大處理下一批。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`（本批僅補索引與待辦）。
  ~~下一步：0x45e820（CViewPort）起~~
- 進度（11-16）：Poq.exe 0x45e820～0x4607c0 **CViewPort 繪圖總入口、動態遮擋與小地圖初始化／更新段（10 個函數）**。
  - skip 2 個：0x45ef90 解析度事件與 DirectDraw 表面生命週期，由 SDL 取代；0x460470 清空四組繪圖串列，另入 base_map.tsv，用 C 陣列名單代替（總表歸 base）。其餘 8 個留 todo，未將簡化畫面誤標完成。
  - 0x45e820 重設尺寸／跟隨中心與死區等鏡頭格座標快取重算；0x45ec50 局部遮擋、0x45f200 繪圖入口、0x45f820 動態物件繪圖等分層名單、物件虛擬繪圖及格子可見資料。0x45fec0 小地圖標籤／鏡頭框、0x460570 場景初始化、0x4607c0 全圖探索標記等小地圖視窗、原版座標比例與場景世界限制矩形。
  - 0x45f100 是 Rec 鏡頭封包，並非連線同步：僅讀單一依賴 0x473470 確認 fwrite 錄影用途，另記 role，等 Rec 系統；未擴大處理下一批。
  - 後續須保留原版怪現象：遮擋矩形裁切後面積必須小於20001；角色關聯類型2／3時暫時將 X/Y 各加128，繪完還原；小地圖每5次更新僅餘1時畫標籤，比例先向零截斷成整數作開關，全圖標記是 X 外圈、Y 內圈；Rec 鏡頭座標先以640×480中心補正。
  - 新偏移與用途已記 fields.tsv／roles.tsv；+0x38 在本批實為名單表頭，與先前座標推定衝突，保留兩義待後續確認。未新增遊戲邏輯或測試、未加完成位址或 shortcut；沒有新增確認劇情不用的功能。
  - 建置與測試：`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`。
  ~~下一步：0x460ad0（CGameApp 地上道具標籤段）起~~（本輪先回補 CViewPort，後續起點不變）。
- 進度（11-16 回補）：Poq.exe **0x45a470～0x4607c0 CViewPort 鏡頭／可見物件／分層繪圖**整段重新核對；總表本段現為 done 16、skip 6、todo 9（另有 3 個容器已歸 base；todo 含 CMapDialog 2 個）。
  - 新增 `engine/src/viewport.c`，由 main.c 引入：`viewport_set/window/resize/scroll/cache/follow/tick` 接入進場、每16ms更新與畫面尺寸變更，取代每幀把鏡頭置中的簡化版；原先四個 `viewport_*` 換算函數已由投影／捲動快取真正呼叫。MAP +0x26～+0x32 的世界限制矩形、MDL 底／上層部件數及遮擋平面偏移由 res.c 補讀。
  - `viewport_collect/insert/intersects/visible_insert`：按原版帶8格邊界的交錯掃格順序收集建築，當幀圖像矩形裁切，X+Y降序、同深度新項在前／反向繪製。`viewport_layers/draw` 取代 main.c 舊的角色 qsort／場景物件合併迴圈；底層→地上物→中層→上層→光照→動態物件，單位改為單件繪圖回呼。`viewport_occlusion_start/occlude` 接上遮擋平面、格子旗標0x82的建築覆寫與小面積局部重畫；SDL每幀重畫代替DirectDraw快取搬移。
  - 照原版保留：設位Y−16僅用於比較、邊界先減1再取偶數、一般方向捲動入口缺少奇列X+32及負座標base.x−16補正、平移用平方距離直接比步長／距離重複即到目標、震動每5拍才累積交替位移、遮擋裁切面積必須小於20001、CRHOMB邊線相等不修正。**更正前輪跟隨筆記**：`local_28` 只在靜止分支被快取重用，不能據此改寫移動分支的方向；本輪按分支本體翻譯。
  - 外部依賴才留 todo：探索遮罩等 **CScene 探索資料／CResMng 遮罩圖資**；地表交錯主迴圈已接遊戲，但缺格預設圖／除錯格圖等 **CResMng 預設圖初始化**，不標整個地表函數完成；動態角色／單位／type2/3暫加128已接，但完整狀態分流與關聯來源等 **CCharactor／CDynamicObj**，亦不標整個動態入口完成；Rec封包等 **Rec**；小地圖點選、輸入、標籤、初始化及全圖探索標記等 **CMapDialog**。roles.tsv 已刪除這批「等同類別函數」的舊理由。
  - shortcut：目前 CScene 載入把格子可見狀態設2（全部可見）；視窗已依該欄位判斷，探索系統接入後換資料來源。建築名單以擺放格索引，完整占地／格子建築索引、關聯模型類型與特殊動態分類來源等各自類別載入；局部遮擋採SDL裁切直接重畫前景，無DirectDraw暫存／尾端去重名單。沒有把這些尚缺的整個原版函數誤標完成；沒有刪除劇情功能，也沒有新增確認劇情不用的功能。
  - 驗證：skilltest 新增鏡頭狀態／快取／查格邊線、捲動、死區、平方距離平移、交替震動、排序同深度、不可見格、遮擋平面／格子覆寫、軟體renderer分層像素測試；原生建置與 `make skilltest` 通過，最後一行 `skilltest ok`。另以SDL dummy/software實際啟動 xianyang、xy_minjud 並輸出640×480截圖；未驗證人工長時間遊玩。
  ~~下一步：0x460ad0（CGameApp 地上道具標籤段）起~~。
- 進度（11-16）：Poq.exe **0x460ad0 地上道具標籤段**（總表 ~CGameApp 為推測誤判；寬高與 +0x154/+0x158 實為 CViewPort 欄位）。`engine/src/viewport.c` 的 `viewport_ground_labels`／`viewport_ground_labels_draw` 已接進 main.c 畫面迴圈，新增 Alt 切換時的交錯掃格、8格外圈、可見狀態>=2、畫面內原點裁切與道具名單快照；原先沒有地上道具文字簡化版可刪。
  - 照原版保留：僅開關狀態改變才重建，按住時走動／新掉落不刷新；阻擋時連放開也不更新；裁切右／下邊界不包含相等點。新偏移補入 fields.tsv，物件句柄查找工具記入 base_map.tsv。
  - shortcut：同格先取 ground 陣列第一件；以左右視窗作介面阻擋；暫以文字名單顯示。**仍為 todo 1、skip 0**：完整 GroundData 的 ID／ResID、JM70 排版與點選等不同類別介面系統（地面道具介面入口），格子唯一動態物件與世代句柄等 CScene 物件索引。未把整個原版函數標 done，部分實作註解不寫原版位址；沒有新增確認劇情不用的功能。
  - 驗證：skilltest 新增阻擋切換、不可見格、右邊界、交錯順序、同格第一件與按住快照測試；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`。
  ~~下一步：0x460d80（CScnClient 場景探索段）起~~。
- 進度（11-16）：Poq.exe **0x460d80～0x461920 CViewPort 跟隨／置中／探索更新段**（原總表的 CScnClient／CGameDialog 為推測誤判）。新增 `engine/src/viewport.c` 的 `viewport_follow_set/pan/center_at/center_actor/home`，接進場、指定位置啟動與 H 鍵；刪除啟動路徑舊的直接投影置中。共新增 done 5（另完成共用交錯格轉像素工具），原有震動入口 done 1，skip 3（兩個 DirectDraw 平台回呼、一個串列節點配置，容器另記 base_map.tsv），todo 2。
  - 照原版保留：開啟跟隨請求平滑置中；關閉跟隨不取消平移；立即置中也不清平移；平滑步長是畫面距離截斷後除30再加2，距離小於30保持舊目標；設定平移不清上次平方距離；目標採角色整格交錯投影，負奇列以位元判斷。
  - shortcut：物件世代句柄由已持有的 Actor 指標代替，等 CScene 物件索引；SDL 每幀全畫面重畫代替背景快取矩形失效。todo：探索狀態提升／髒矩形／小地圖回呼等 CScene 探索通知、CMapDialog 表面與外部介面刷新；分層名單的一般分支已有繪圖迴圈，特殊建築分支等 CBuilding 特殊模式與 CScene 建築占地索引，不把整個函數標 done。沒有新增確認劇情不用的功能。
  - 驗證：skilltest 加入平移設定、空角色、跟隨開關、立即置中、29／30像素門檻與負奇列投影；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`。FUNCS.tsv 已重產。
  ~~下一步：0x461960（CGameApp）起~~。
- 進度（11-16）：Poq.exe **0x461960～0x4637a0 CGameApp Windows 啟動／事件迴圈段**（中間的 CMainWnd 解構另屬下一類別，未處理）。首批只列三個，本輪沿相鄰啟動區段核對全部 CGameApp 回呼及字串依賴；24 個判定寫入 funcs_skip.txt／roles.tsv，其中總表 **skip 20、base 4、新增 done 0、todo 0**。
  - 平台略過：CWinApp 建構／解構、MFC 主視窗與 CEdit、COM、單一實例互斥鎖、安裝登錄檔與光碟路徑、MMX、螢幕保護設定、Win32 計時器／訊息迴圈、PE 檔案版本及 BugTrap 崩潰回報。旗標存取／常數回呼只服務這套 MFC 生命週期，不另造無呼叫者的 SDL 包裝。std::string 串接／SSO 縮容、版本字串與登錄檔工具補進 base_map.tsv。
  - 零呼叫者入口依 FUNCS.tsv 的 callers=0 列 skip，未為追查舊啟動路徑擴讀本體；此數字是函數呼叫索引，不宣稱已做整個執行檔的間接引用掃描。新讀到的 CGameApp 平台偏移寫入 fields.tsv；+0xd0 只記建構設1，未猜用途。
  - 本輪沒有遊戲邏輯移植、shortcut 或新增外部依賴 todo，沒有新增確認劇情不用的功能；故不加鏡像實作測試。原生建置與既有 skilltest 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x461cd0（CMainWnd）起~~。
- 進度（11-16）：Poq.exe **0x461cd0～0x461cd0 CMainWnd**，核對 FUNCS.tsv 全部 CMainWnd 項目，本類別只有此 deleting destructor；新增 **skip 1、done 0、todo 0**。原版先 DestroyWindow，再解構內嵌 CEdit／CWnd，最後依旗標釋放單一或陣列記憶體；屬 MFC 平台／解構流程，SDL 視窗生命週期代替，原因與用途補入 funcs_skip.txt／roles.tsv，內嵌 CEdit 偏移補入 fields.tsv。
  - 本輪沒有遊戲邏輯、需保留的遊戲怪現象、shortcut、外部依賴 todo 或新增確認劇情不用的功能；不新增鏡像實作測試。原生建置與既有 skilltest 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x463810（CScrMng 前置工具／螢幕管理段）起~~。
- 進度（11-16）：Poq.exe **0x463810～0x464260 CScrMng 與前置平台／字串工具**，首批12個後補齊同類別裝置事件入口，共 **skip 10、base 3、done 0、新增 todo 0**（13個均記 funcs_skip.txt，字串工具另記 base_map.tsv）。CScrMng 建構／解構、顯示矩形初始化、視窗拷貝／全螢幕 Flip 與表面遺失重試均屬 DirectDraw 平台流程，由 `engine/src/main.c` 既有 SDL 視窗／renderer／SDL_RenderPresent 代替；前置 Win32 測試視窗、計時器、MFC 訊息入口與字串 SSO 工具略過，補用途與欄位索引。
  - 完整核對 FUNCS.tsv 的 CScrMng（含推測）項目：表面還原已 skip；另外兩個 `~CScrMng` 實際操作應用程式介面／天氣通知，並非本類別，保留既有 todo：介面管理器重建與雙介面／天氣裝置事件，等對應系統，已在 roles.tsv 註明推測誤判；未把它們誤列平台 skip。
  - 本輪沒有新增遊戲邏輯、遊戲怪現象、shortcut 或確認劇情不用的功能，故不加鏡像實作測試。原生建置與既有 skilltest 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x464370（CGameApp 顯示設定段）起~~。
- 進度（11-16）：Poq.exe **0x464370～0x4656e0 libVLC 播放器包裝與前置基底工具**，首批12個後補齊單例、可播放查詢、播放與停止入口，共 **skip 16、base 11、done 0、新增 todo 0**（16個均記 funcs_skip.txt，基底另記 base_map.tsv）。本體確認 `~CGameApp` 是推測誤判；實際為 shared_ptr 自訂 deleter、CRT 例外與 Windows/libVLC 影片生命週期。下一個建構入口已出現 CClientScreen vftable，故停在類別邊界。
  - 播放入口從路徑建媒體、綁 HWND 再播放；查詢用 will_play，未誤當 is_playing。失敗時原版拋 runtime_error。記錄播放器／媒體 shared_ptr 配置與控制塊偏移；回呼槽只記數量，不猜事件用途。沿用先前影片改網頁播放的工作，不宣稱已移植播放功能，也不列為劇情沒用到。
  - 本輪沒有新增遊戲邏輯、遊戲怪現象、shortcut 或外部依賴 todo；不新增鏡像實作測試。原生建置與既有 skilltest 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x465710（CClientScreen／ScreenObj）起~~。
- 進度（11-16）：Poq.exe **0x465710～0x467880 CClientScreen／ScreenObj**，補齊首批以外的視野重掃、延遲移除、全場同步與名單入口，共 **skip 30（其中 base 11）、done 0、todo 3**。這是伺服端維護每個客戶端可見副本的同步層，非 CViewPort；建立／更新／移除經訊息包發送。單機直接使用場景物件，不另建同步副本；本機進場也會建本類別，未宣稱整類只有多人呼叫。用途與新偏移已記索引。
  - 原版細節：視野2048×1536、離開視野100拍後移除；建築已有節點狀態變成1時不送更新。這些記為同步協定行為，沒有冒充已移植的遊戲邏輯。無呼叫者的六個距離／格子查詢及鏈查詢按索引略過。
  - todo：第一名單加入、每拍倒數與倒數設定會對角色套用／解除狀態102；等待不同類別的角色狀態系統與 CGodClient 模式18/19規則，未將這三個單機可能需要的行為當連線功能略過。無新增 shortcut 或確認劇情不用的功能。沒有新增遊戲邏輯，故不加鏡像測試。原生建置與既有 skilltest 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x467a10（CGodClient 前置容器／場景管理）起~~。
- 進度（11-16）：Poq.exe **0x467a10～0x469d70 CGodClient 與前置容器**，補齊類別在首批以外的入口；本輪 **done 1、skip 14（其中 base 3）、todo 3**。`engine/src/weather.c weather_resume` 移植 0x469a40，接入 `main.c enter_scene`，取代進場未接續粒子的行為；不另留未呼叫入口。建構／解構5個、無呼叫者8個及通用名單查找1個略過；用途、新欄位與容器替代已寫入索引。
  - 照原版保留：接續雨直接設階段2、音量衰減1、雨量18並預推40拍，不經變暗／隨機等待；紅雨旗標照天氣種類，雪片數直接設滿，剩餘時間與亮度不變。沒有新增 shortcut；沿用目前天氣伺服端尚未按房間保存狀態的限制，本輪只處理接續畫面入口。
  - todo 3：建築環境音加入／移除／距離音量入口（0x469bb0、0x469c80、0x469d70），等不同類別 NormalBuilding 的場景物件與音效句柄系統；現有靜態地圖建築尚不能提供原版種類11物件及其音效資料，不自造替代規則。後段聲音包裝雖被推測為 ~CGodClient，實際是全域音效工具，留到音效段處理。沒有新確認劇情不用的功能。
  - `engine/test/skilltest.c` 新增五種雨雪接續與晴天不變測試；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46a0c0（CInitMngMain／初始化管理前置）起~~。
- 進度（11-16）：Poq.exe **0x46a0c0～0x46a140 CInitMngMain**，連同後段共用工具 **0x4ca880～0x4cbf70** 核對，**done 0、skip 12（其中 base 10）、todo 1**。建構／解構略過；四組初始化名單的取值、描述子、配置與清理記入 base_map.tsv，無呼叫者的四個入口亦略過。後段推測為 ~CInitMngMain 的共用名單與 INI 讀取實際屬基底 CInitObjMng，用途與欄位已補索引，未把 INI 載入當容器略過。
  - 原版細節：初始化後只掃第3組，模板ID以 ushort 差值排除1500～2000（含端點）；其他ID配置同種類 SInit、虛擬複製、設已建立旗標後加入場景。不能拿目前劇情 spawn 代替本批入口，沒有補上假的完成位址。
  - todo 1：0x46a140 等不同類別 CInitObjMng 的 INI 記錄載入（0x4cb440／0x4cb640）及 SInit 虛擬複製還原；兩個載入器仍保留原有 todo，隨基底類別處理。沒有新增遊戲邏輯、shortcut 或確認劇情不用的功能，因此不新增鏡像測試；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46a250（CInitObjMng 空入口／後續場景物件初始化段）起~~。
- 進度（11-16）：Poq.exe **0x46a250 CInitObjMng 空入口，連同後段 0x4cab40～0x4cb640 同類別入口核對**；前輪已分類的建構、名單與清理工具維持原判定，本輪 **done 0、新增 skip 2、保留 todo 2**。空虛擬入口與 deleting destructor 記入 funcs_skip.txt；未以空殼實作冒充載入完成，未進入下一個 CNpc 類別。新確認的 SInit 座標、AI、初始態、方向、路徑與跟隨參數偏移已寫 fields.tsv，函數用途與依賴已寫 roles.tsv。
  - 原版細節：開檔失敗不清空舊資料；成功才重設，讀 BldMdl 後依第1／3／2組載入角色／野獸／道具，最後記檔名並呼叫衍生初始化。CIniFile 是從目前游標往後讀取（鍵名忽略大小寫），不能用目前的隨機找鍵代替；初始態同名鍵連讀兩次，路徑按 PathNum 順讀座標。不新增 shortcut，沒有新確認劇情不用的功能。
  - todo 2：0x4cb440／0x4cb640 一起等待不同類別 CIniFile 的順序／解密讀取（0x4bb3d0、0x4bb650）及 SInitChaInit／RandMove／PathMove／Follow、InitBld／Trigger 記錄還原系統；同類別的空入口、解構與共用名單已全數核對，沒有把同類別依賴推給下一輪。CInitMngMain 的建立入口仍等這組載入與虛擬複製接通。
  - 沒有新增可接入的遊戲邏輯，因此未加鏡像測試或未呼叫的新函數；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46a260（CNpc／場景物件初始化段）起~~。
- 進度（11-16）：Poq.exe **0x46a260～0x46a420 初始化名單查找／複製與封包deque，連同 0x46a660～0x46abd0 中同一封包佇列入口核對（不含下一類別 CGameApp 場景入口）**，本輪 **done 0、新增 skip 10（其中 base 5）、todo 3**。`~CNpc` 是推測誤標，前三個入口實際操作 CInitObjMng 第n組 SInit 記錄，第四個是共用已建立旗標設值；後續 deque 擴容、釋放、長度錯誤與全域封包同步／狀態／入列均屬工具、無呼叫者或連線平台層。用途、工具替代及新偏移已寫 roles.tsv／base_map.tsv／fields.tsv，未把場景初始化業務當容器略過。
  - 照原版保留的細節：座標查找接受種類10～16且忽略已建立旗標；名稱查找同樣接受10～16，但只取未建立者、逐byte區分大小寫，兩者均回首個匹配索引，找不到回-1。複製入口檢查索引上下界，以工廠配置同種類後呼叫虛擬格2複製，再將來源旗標設1；旗標設值工具只檢查非負、不檢查上界。工廠實際支援的種類與查找接受範圍不同，不自行修正。
  - todo 3：0x46a260／0x46a2c0 的座標／名稱查找等不同類別 CIniFile 順序讀取及 SInitCha 記錄載入到場景；0x46a370 再等 SInit 各衍生虛擬複製還原。CInitObjMng 本類別的載入入口先前已核對並明確等待不同類別系統，本輪未把同類別依賴推給下一輪；目前劇情 spawn 沒有這組原版初始化記錄與已建立旗標，不能冒充完成。沒有新增 shortcut 或確認劇情不用的功能。
  - 沒有新增可接入的遊戲邏輯，因此未新增未呼叫函數或鏡像測試；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。到此停止，不進入 CGameApp 場景類別。
  ~~下一步：0x46a6b0（CGameApp 場景入口）起~~。
- 進度（11-16）：Poq.exe **0x46a6b0～0x46c260 場景管理入口整段核對**（含同物件的備份／還原、暫停、錄影模式及日期時計；`~CGameApp` 是推測標記，欄位另以 SceneSession 記錄，避免污染真正 CGameApp）。本輪 **done 1、skip 11（其中 base 1）、todo 14**；下一個 +8 CIDTable 的建構及後續大型場景物件不屬本管理物件，到此停止。
  - `engine/src/light.c::clock_tick` 已由 main.c 主迴圈呼叫，補上場景時計原版位址並修正原有簡化版跨年條件：原版月份遞增後 **>11 就跨年，因此只有1～11月**；保留每月31日、0～6／14～20點含端點每拍加120、其餘加1、室內不走。沒有另留未呼叫函數。新增 skilltest 加速端點、11月31日跨年及室內停止測試。
  - shortcut：現有單機未建立 Rec 回放狀態，狀態2阻擋及半週期／半小時日期介面通知，等不同類別錄影／日期介面系統接入時補；天光仍由 weather_tick 更新。沒有新確認劇情不用的功能。
  - todo 14：場景服務拍、配置／進入／釋放、就緒輸入取得、同名備份判斷與還原、錄影模式／檔名、伺服端配置、主拍及暫停通知，分別等不同類別 CScnClient／CScnClientRecord／伺服端場景生命週期、CDynamicObj 狀態還原、角色戰鬥狀態及介面通知；本管理物件同段入口已全部核對，未以現有主迴圈冒充完成。封包deque／鎖與裝置通知列平台skip；無呼叫者及建構解構列skip。用途及新偏移已寫 roles.tsv／fields.tsv／base_map.tsv。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46c340（場景表／大型場景物件初始化段，推測類別待核對）起~~。
- 進度（11-16）：Poq.exe **0x46c340～0x46d420 場景入口對照表／大型場景資源物件整段核對**；`~CGameApp` 推測標記不可靠，欄位以 SceneEntryTable／SceneEntryRow／SceneResources 暫名分開記錄。本輪 **done 0、skip 10（其中 base 6）、todo 5**，到下一個 CScnBase 建構前停止。
  - 建構／解構、表格清空與五個內嵌管理器的全域指標綁定列 skip；容器與綁定工具寫入 base_map.tsv。兩個無呼叫者入口分別是建築初始化重載及地圖重載，列 skip；沒有將有呼叫者的載入業務當作工具略過。
  - 照原版記錄的細節：入口查找逐byte區分大小寫，取到ID後把來源設為0xffff，只能取一次；遇空入口名稱即停止該列，找不到或已消耗都回-1。文字表每列配置容量是總欄數減1，實際填入對數卻是 `(總欄數減1)/2減1`，不自行修正；表清理在第一個入口迴圈內就釋放整個ID陣列，後續靠歸零避免重複釋放。這些是已核對、尚未實作的行為，不算 done。
  - todo 5：人物／道具初始化載入等不同類別 CInitObjMng 的 CIniFile／SInit；入口表載入及一次性查找等不同類別文字表讀取器，之後由 CScnClient 接入；大型場景載入等不同類別場景描述／CScene地圖／建築初始化；清理等不同類別資源串列及 CObjMng 完整生命週期。同段依賴均已核對，沒有把本類別其他入口當成等待理由。現有 `res.c::scene_load`／`main.c::enter_scene` 只覆蓋部分行為，未補位址冒充完成；沒有新增未呼叫函數、shortcut 或確認劇情不用的功能。
  - 用途與16個新偏移寫入 roles.tsv／fields.tsv；沒有新增遊戲邏輯，因此未新增鏡像測試。`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46d470（CScnBase 場景同步基底，含後續同類別入口）起~~。
- 進度（11-16）：Poq.exe **0x46d470～0x46da90 CScnBase 場景同步基底全段**，連同相鄰 **0x46db00／0x46db20 容器解構**核對；本輪 **done 0、skip 15（其中 base 8）、todo 0**。首批之外的字串封裝入口亦已處理，到 CScnClient 建構前停止。
  - 建構／解構、空虛擬入口與字串佇列／封裝工具略過；建築、角色、道具建立及移除均為前輪 CClientScreen 所用的可見物件複製封包，單機共用物件不另建同步副本，不將這些當成尚缺的遊戲規則。容器與工具已記 base_map.tsv，用途及13個新偏移已記 roles.tsv／fields.tsv。
  - 原版細節：移除時先清同步旗標，物件不存在或 +0x62 非零才送移除；種類23道具數值 **大於65535** 才改長封包；角色描述先把空白換成0x0e再入列；字串封包長度含兩個標頭byte及結尾NUL。佇列索引只先擋上界，負索引由 MFC 拋例外。這些是略過的傳輸層行為，未加假的完成位址。
  - 無新增遊戲邏輯、未呼叫函數、shortcut、todo 或新確認劇情不用功能，因此未加鏡像測試。`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x46db60（CScnClient 場景客戶端建構／生命週期）起~~。
- 進度（11-16）：Poq.exe **0x46db60～0x471dd0 CScnClient 場景客戶端全段**核對，包含首批之外的鏡頭／小地圖、查詢、繪圖、同步接收及命令入口；到 CScnClientRecord 建構前停止。本輪 **done 2、skip 21（其中 base 7）、todo 9**（含兩個相鄰500槽容器工具）。
  - `main.c::scene_client_actor`／`scene_client_group` 移植操控角色及組別查詢，場景載入成功後才設就緒，接入點NPC對話面向與 `actors_sync` 的主角組別分邊，取代直接比較的簡化程式。建構／解構、無呼叫者入口、容器／幾何工具、全域場景別名檢查與CRT檔案工具列 skip；同步封包分派與命令發送是可見物件複製／傳輸層，單機直接使用業務物件，不新增副本或封包回送。
  - 照原版保留：未就緒或無操控角色時組別比較回假；死亡本身不使句柄失效；組別比較成功查找後再查一次。已核對但尚未實作的小地圖細節：寬高都大於127才建512×256底圖；先繪8×4地磚及建築，再縮放或裁中、加出口圖示；出口圖示Y超界分支卻改X變數，後續移植不得自行修正。rdr快取以scn修改時間判有效，生成後把修改時間設成scn時間。
  - shortcut：操控角色暫用 actors 索引，等不同類別 CScene 世代句柄容器再升級；單機直接傳場景，無全域管理器別名切換。框選工具只記SDL_Rect替代方案，沒有新增選人／手機操控，完整框選仍缺不同類別CUserInput名單。
  - todo 9：ClientCreate／小地圖生成等不同類別 CMapDialog 探索位元表與 CBuilding 縮圖／出口圖示；繪圖、就緒切換、進場收尾與場景還原等 CUserInput 名單及游標狀態；每拍入口等 CScene 格子探索衰減及 CMapDialog；角色文字等 CCharactor 頭頂文字生命週期；新場景配置等 SceneResources／CInitObjMng／CBuilding 載入。同類別相互依賴均已核對，不以等待本類別入口作為todo理由。索引將格子探索衰減入口推成CScnClient，但本體的寬度、格子指標及21-byte步進屬地圖資料，不拿推測類別當成已完成。無新確認的劇情不用功能。
  - roles.tsv／fields.tsv 已補用途及27個欄位（含修正原有 +0x2c／+0x30／+0x5c）。skilltest 新增場景／就緒／無效索引／缺角色／死亡仍可查／組別比較測試；`cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x471ed0（CScnClientRecord 錄影／回放場景客戶端）起~~。
- 進度（11-16）：Poq.exe **0x471ed0～0x473660 CScnClientRecord 全段**核對，含首批外的錄製、讀包、對話／語音、跟隨及tick入口；到後續場景管理段前停止。新增 **done 0、skip 20（其中 base 6）、todo 0**，另保留既有輸入錄影入口skip 1。
  - 建構／解構、無呼叫者、CRT錄影檔案及同步封包包裝列skip；可選RecordType錄製／回放不屬單機劇情規則，不新增無呼叫的新函數。片頭PlayIntro另有回放用途，沿用既定網頁影片播放工作，不能將片頭說成劇情沒用到。
  - 修正 `main.c::zoom_set` 的未接入Rec分派器註解：移除其位址，避免總表誤算整個分派器done；保留zoom_set自身已移植的正確位址。沒有以縮放單一分支冒充整個回放系統完成。
  - 原版細節已記錄但不實作：同tick只補一次六byte時間包；檔案每包以前置四byte長度儲存；回放只分派首byte為#或@的包；語音等待優先於倒數，歸零後停聲並回-1；無語音文字等待為參數乘3、至少40拍；失效發言者沿用舊對話座標。
  - 用途及8個新偏移寫入roles.tsv／fields.tsv；工具寫base_map.tsv。無新增遊戲邏輯或shortcut，不加鏡像測試。本輪無todo；先前其他類別等待Rec的項目仍保留各自狀態，本輪未冒稱其場景生命週期已完成。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473740（場景管理入口／後續類別待核對）起~~。
- 進度（11-16）：Poq.exe **0x473740～0x473d00 場景管理前置工具／生命週期全段**核對，到 SKillPlayer 前停止；索引的 `~CGameApp` 是推測，實際含 Rec 對話等待、CScnGC 與場景管理容器物件，未將其偏移誤記為 CGameApp。本輪 **done 0、skip 13（其中 base 8）、todo 0**。
  - 無呼叫者的全域雙值設定、容器／字串建構解構、CScnGC 資源配置釋放及九byte傳輸封包均依規則略過；不以生命週期略過冒稱後續 CScnGC 業務完成。Rec 等待控制接續前輪可選錄影分類，片頭仍待既定網頁影片工作。
  - 原版細節：等待值只有等於0才停語音及清旗標，設-1不會停；封包最後16位欄位位於未對齊的+7；CScnGC 解構每刪一物件重新讀鏈表頭；場景配置失敗回傳物件但場景指標為空。這些是略過層的行為記錄，未加假的完成位址。
  - roles.tsv／fields.tsv 補用途及22個新偏移，base_map.tsv 記替代方案。無新增遊戲邏輯、shortcut或todo，不加鏡像測試。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473dc0（SKillPlayer／後續技能與令牌資料類別）起~~。
- 進度（11-16）：Poq.exe **0x473dc0 SKillPlayer 全類別**核對，只有刪除式解構；到下一類別 SLingpai 前停止。本輪 **done 0、skip 1、todo 0**。
  - 解構先清 CHAINITEM，再依旗標 bit0 釋放0x24 byte節點，依生命週期規則列skip；不是容器工具類，不加base_map。無新增遊戲邏輯或shortcut，無需新增測試。
  - 為確認類別邊界，核對索引中引用它的建立／倒數入口：兩者操作持有者的鏈表及技能命令，屬後續管理類別業務，不冒稱完成；roles.tsv記錄用途，fields.tsv記錄SKillPlayer四個資料欄位。
  - 後續管理類別須保留的原版現象：建立節點後立即遞減；原始值100先設-100，但立即變-101，所以仍發到期命令（只有恰為-100才免發）。本輪不提前實作或改正這個行為。既有管理類別todo留待該類別及技能命令系統處理，不屬本輪SKillPlayer解構的阻塞。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473df0（SLingpai）起~~。
- 進度（11-16）：Poq.exe **0x473df0 SLingpai 全類別**核對，只有刪除式解構；到下一類別 SLingpaiPlayer 前停止。本輪 **done 0、skip 1、todo 0**。
  - 解構先清 CHAINITEM，再依旗標 bit0 釋放0x1c byte節點，依生命週期規則列skip；不是容器工具類，不加base_map。roles.tsv補用途；本體沒有新資料偏移，fields.tsv無需新增。
  - 無新增遊戲邏輯、shortcut或todo，無需新增測試；未將後續持有者的令牌管理業務標成完成。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473e20（SLingpaiPlayer）起~~。
- 進度（11-16）：Poq.exe **0x473e20 SLingpaiPlayer 全類別**核對，只有刪除式解構；到後續固定槽表工具前停止。本輪 **done 0、skip 1、todo 0**。
  - 解構先清 CHAINITEM，再依旗標 bit0 釋放0x1c byte節點，依生命週期規則列skip；不是容器工具類，不加base_map。roles.tsv補用途；核對持有者入口後，fields.tsv補節點+0x14／+0x18兩個配對識別值，不猜其業務名稱。
  - 後續持有者的令牌玩家名單入口在param4等於1時新增，其他值會刪除所有兩個識別值都相符的節點；該入口屬後續管理類別，roles.tsv記錄用途，未冒稱本輪完成。
  - 無新增遊戲邏輯、shortcut或todo，無需新增測試；建構時清零的+4～+0x10為CHAINITEM鏈結資料，不另猜節點業務欄位。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473e50（固定槽表工具／後續管理段）起~~。
- 進度（11-16）：Poq.exe **0x473e50～0x473e90 固定槽表工具全段**核對；兩個入口分別為20槽與100槽容器插入，後續0x473ee0已轉為場景物件建立管理。本輪 **done 0、skip 2、todo 0**。
  - 兩者皆無呼叫者，依容器／工具規則記入funcs_skip.txt與base_map.tsv，以C陣列、占用旗標及數量代替；roles.tsv補用途，fields.tsv以位址限定的暫名記錄兩種布局，不猜原版類別名稱。
  - 原版只在占用旗標恰等於1時拒絕插入；否則寫入32位值、設旗標1並將數量加1，沒有索引邊界檢查。無新增遊戲邏輯、shortcut或todo，無需新增測試。
  - `cd engine && make && make skilltest 2>&1 | tail -3` 通過，最後一行 `skilltest ok`；FUNCS.tsv 已重產。
  ~~下一步：0x473ee0（場景物件建立／後續管理段，類別待核對）起~~。
- 進度（11-16）：Poq.exe **0x473ee0～0x476fc0 場景建立／切換管理段**核對40個入口；此段為CScnGC與SceneContainerOwner（RTTI未確認）交錯，不能把連續位址當成單一類別。本輪 **done 0、skip 32（其中22個容器／工具同列base_map）、todo 8**；沒有將CScnGC後續命令業務宣稱完成。
  - 同步廣播沿用CClientScreen共用單機物件的既定處理；無呼叫者入口、字串封包包裝、固定槽讀取／查找／遍歷及配對名單列skip。配對名單param4恰為1才新增，其餘刪除全部雙欄相符項；工具以C陣列／索引代替，不新增無人呼叫的函數。未將場景切換、天氣或倒數說成劇情沒用到。
  - roles.tsv記錄全部入口及8個todo：場景建立／兩種切換入口等SceneResources場景表與初始化載入；劇情建物件的後處理等CCharactor、CDynamicObj及CTriBld；陷阱主人轉接等CDynamicObj主人設定；倒數建立／更新等CScnClient的0x147接收行為及CGameApp光影提示；管理主拍等完整場景子管理器生命週期與命令接收。這些都沒有位址完成註解，不能用現有簡化主迴圈冒稱原版流程已完成。
  - 原版怪現象：固定槽查找有的要求占用恰為1，有的只要求非零；空槽遍歷仍推進游標。SKillPlayer新增後立即更新整鏈（不是只更新新節點），每個節點先減1，恰30才提示；僅遞減後恰-100免送，故初值-100會變-101並送出。0x147數量為兩個32位字，資料是節點+0x18及緊鄰的倒數+0x1c。fields.tsv補新欄位並修正先前只記四byte及將SKill誤稱技能的推測。
  - 無新增已接入遊戲邏輯或shortcut，故無新增測試；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。下一段開始為物件查找／命令入口，不能因本輪容器已分類就連帶標完成。
  ~~下一步：0x476fd0（物件查找／命令入口，類別待核對）起~~。
- 進度（11-16）：Poq.exe **0x476fd0～0x47fed0 物件查找／CScnGC命令與傳輸段**核對53個待辦入口，另有原已done的劇情分派入口；跨過工具及推測CGodServer標記，追完同段CScnGC傳輸到封包解碼尾端，下一入口已轉為CGameApp。本輪 **done 2、skip 46（其中10個工具同列base_map）、todo 5**。
  - `engine/src/main.c`：通用`actor_of`補原版轉接位址；新增`character_of`照原版查找後限定角色類型，接入劇情G_AIGroup更新，取代原本直接接受所有Actor的查找。死亡角色仍可查到，陷阱不當角色；通用位置／死亡訊息仍用物件查找。`engine/test/skilltest.c`新增死亡角色、陷阱、空handle、不存在與已移除角色測試。
  - shortcut：物件編號沿用劇情handle，Actor現階段只有角色／陷阱，因此用陷阱表旗標代替原版種類30～100篩選；等完整CScene索引與其他物件類別接入再升級。沒有新增無呼叫者的遊戲函數。
  - todo 5已在roles.tsv分別記錄：文字命令分派等SceneResources初始化／切換與CCharactor、CDynamicObj、CTriBld業務；兩個客戶端移除入口等CScnClient離場及CClientScreen生命週期；依物件種類移除等CCharactor／CDynamicObj；共用命令區建立物件等SceneResources與三種物件建立後處理。這些同段入口已一起核對，等待的是其他類別；沒有把整個命令分派器當成純連線版略過。
  - skip包含無呼叫者、清場生命週期、連線同步與封包工具；fields.tsv補固定槽移除、封包解碼及出口索引布局，合併更新CClientScreen既有偏移。原版怪現象：跨場景角色查找迴圈未用索引而重查同一編號；槽移除接受任意非零占用；封包參數解碼超過要求數量不截斷、只在不足時補-1；字串解碼先清緩衝但複製至NUL時不檢查容量。這些工具未複製成無人呼叫的C函數。
  - `cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；另以`-DSTORY`做main.c語法檢查通過；FUNCS.tsv已重產。
  ~~下一步：0x47ff20（CGameApp 介面工具）起~~。
- 進度（11-16）：Poq.exe **0x47ff20 CGameApp 音效平台初始化入口**及其直接依賴 **0x57b4a0 MFX配置工具**已核對；本段入口只有一個，下一入口0x47ff60已轉為CGodClient，依規則停在類別邊界。**done 0、skip 2（其中1個工具同列base_map）、todo 0**。
  - 原版入口僅將兩參數加固定0x140／0x578傳給MFX工具；工具設定全域配置、複製路徑與silence.wav後呼叫Windows Mfx_Build，以回傳值非負為成功。現有`engine/src/audio.c audio_init`已接主程式，透過SDL開音訊裝置及載入snd.lib，平台層按規則略過，不新增包裝函數或完成位址註解。
  - 原版怪現象：路徑複製至NUL且不檢查容量；此為平台工具，未搬入SDL引擎。fields.tsv記錄新配置偏移和全域參數，未知MFX欄位語意明列未確認；roles.tsv記錄兩入口用途。無新遊戲邏輯、shortcut、跨系統todo或劇情不用功能，故無新增測試。
  - `cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。
  ~~下一步：0x47ff60（CGodClient 音效管理段）起~~。


- 進度（11-16）：Poq.exe **0x47ff60～0x480650 CGodClient 音效管理段**核對全部5個待辦入口，連同已完成的0x480350、0x4804a0核對依賴；下一入口0x480780為CAirObj建構，不跨入下一類別。另核對直接依賴0x57b830／0x57b9c0。**done 0、skip 5（其中1個平台工具同列base_map）、todo 1**。
  - skip：MFX直接調音、直接播放、錄影／場景別名調音包裝、無呼叫者的世界座標調音，以及MFX位置調音平台工具。SDL現有audio_loop／audio_sfx承接平台播放，不新增無人呼叫的包裝；不把非位置播放宣稱為原版位置音效完成。
  - todo：0x480650隨機環境聲源入口，等不同類別SceneResources的場景種類／環境音觸發資料接入weather主拍，再接位置播放；本類別相依入口已核對，不以同類別函數為等待理由。既有weather尚缺依場景種類觸發環境音，不冒稱已接入。0x57b9c0為後段位置播放工具，roles記錄用途但未標完成。
  - 原版怪現象：隨機模式1是Y=rand()%1400，模式2是Y=rand()%1000+320，X恆0，其他模式原點；不是延遲。位置工具以距離平方直接比較320／1400，不開根號；超上限回失敗且不調整既有聲音，水平聲像按水平差計算。回放模式以全零座標調音。fields補聲像係數並更新遠近界限語意，未猜測MFX單位。
  - 無新增遊戲邏輯、shortcut或劇情不用功能，故無新增測試；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。
  ~~下一步：0x480780（CAirObj 空中物件）起~~。


- 進度（11-16）：Poq.exe **0x480780～0x4809d0 CAirObj 空中物件**全部9個入口已核對（含next首批未列出的5個共用虛擬入口）；下一入口0x480a40為CAirBeastie建構，停在類別邊界。**done 0、skip 4、todo 5**。
  - skip為建構、解構及刪除解構轉接；不是容器／工具，無新增base_map。其餘5個入口逐一在roles.tsv記用途與等待條件：釋放等不同類別CObj場景移除與CSprGroup資源清理；繪製／位置更新等精靈圖片資源與範圍系統；常數更新及條件更新等CObj場景物件狀態／生命週期。依賴的本類別入口均已核對，不以同類別尚未移植為等待理由。
  - 引擎目前沒有空中物件實例及其場景生命週期，不以角色／技能單位繪圖冒稱CAirObj完成，也未新增無呼叫者函數。fields.tsv補本段直接讀到的偏移，幀資料與繪製參數的重疊布局明列待確認。無新增shortcut；無證據證明飛鳥／空中物件不供劇情場景使用，故不列「劇情沒用到」。
  - 原版行為：繪製先呼叫虛擬更新但忽略結果，兩種繪製分支皆回1；更新判定先檢查基底狀態才呼叫虛擬更新，預設更新恆回1；位置由世界轉畫面後計算兩種範圍。本輪保留為待接入規格，未宣稱已在遊戲重現。
  - 無新增遊戲邏輯，故無新增測試；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。
  ~~下一步：0x480a40（CAirBeastie 空中生物）起~~。

- 進度（11-16）：Poq.exe **0x480a40～0x481650 CAirBeastie 空中生物**完整14個入口已核對，含next首批未列出的設目標／逃跑入口；下一入口0x481930為CBirdFast建構（索引暫誤標~CScnClient），停在類別邊界。**done 7、skip 7、todo 0**。
  - `engine/src/air.c`：air_model／air_place綁模型與場景記錄，air_animate依休息／走跑切序列，air_move計算整數拍數與浮點增量，air_tick_one處理閒逛／巡路／聲音與對人反應，air_run選逃跑或追主角路點，air_draw接SDL繪圖。main.c進場、固定主拍及viewport動態繪製均已接入，換場景清掉舊陣列；原引擎對無graphic的小動物直接略過，現在由空中物件陣列承接，沒有留下無呼叫者新函數。
  - 沿用Scene/NpcSpec、Spr/Anim、near_other、dir_of/move_dir及SDL快取，沒有把CObj／CSprGroup基礎依賴留todo。res.c補BeastieMdl移動屬性、顯示方式、對人反應與聲音；ONT補平時邏輯／休息、A_x/A_y/A_cx/A_cy及類型11，網頁預抓新增空中動畫與各序列圖片。陽周城實際載入12隻（普通烏鴉／小鳥9、追主角烏鴉1、蜜蜂2）。
  - skip：4個建構／解構／刪除轉接、3個索引無呼叫者入口（設狀態、獨立閒逛、獨立移動）。base_map補OmUtil::GetFleePos對應；因fn.py漏掉參數，以Poq呼叫點及OmUtil匯出本體機碼核對為WhatDir8後移5格，並非自訂逃跑距離。fields.tsv補34列，roles.tsv補14個用途；無證據表明本類別劇情不用，未列DLC。
  - 原版怪現象：休息值4設10000拍；隨機偏移是−5～+4且含範圍右下端點；移動以截斷距離／速度算拍數，最後不吸附終點、下一拍才回休息；格座標負數除64向零。反應在舊計數mod80==1觸發，路點以+3,+3後的曼哈頓距離選最遠／最近、同距離取先；追主角不支持隨機範圍時仍先設跑態／高速。
  - shortcut：音效沿用SDL非位置播放，等位置播放接入；調色用現有light RGB係數，等逐像素精靈色表支援；零拍除零與空路線加防護，待資源合法性驗證後可還原原版例外行為。既有方向工具的近似沿用，未宣稱還原OmUtil全部幾何。沒有等待其他遊戲系統的todo。
  - skilltest新增移動／負座標、額外一拍休息、10000拍、巡路、逃跑／追逐、動畫切換及真實陽周城12隻建立／巡路／清空測試；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`。FUNCS.tsv已重產。
  ~~下一步：0x481930（CBirdFast 快速飛鳥）起~~。

- 進度（11-16）：Poq.exe **0x481930～0x481ab0 CBirdFast 快速飛鳥**完整6個入口已核對（含建構、解構轉接、刪除解構）；下一入口0x481b30已由本體確認為CBuilding建構，停在類別邊界。**done 3、skip 3、todo 0**。
  - 接續上輪未提交改動：`engine/src/air.c`的bird_init／bird_animate／bird_tick_one對應固定動畫、方向序列、移動與離屏回收；另回補bird_spawn與bird_generate兩個先前入口，main.c固定主拍、viewport空中繪製、進場清空及結束釋放均已接入，網頁預抓包含固定動畫及全部方向圖片。沿用Spr、Scene、viewport與SDL圖片快取承接基底，不留未呼叫的新函數；本批不是容器／工具，無新增base_map。
  - skip為3個建構／解構入口，使用C陣列與共用動畫快取生命週期取代。fields.tsv補9個偏移，roles.tsv補本類別6個用途及兩個生成入口；沒有等待其他遊戲系統的todo，也無證據可列「劇情沒用到」。
  - 原版怪現象：初始方向隨機但序列仍為0，更新才切方向；速度16，離屏舊計數>100才移除（連續離屏第102拍），重新可見歸零；生成倒數先判舊值，重擲<11額外嘗試左側8隻，倒數跨場景保留。shortcut：生成設定旗標與場景管理器條件暫以Scene已載入且寬>96承接，待設定與管理器資料接入還原；移動方向沿用既有工具近似。繪圖沿用現有SDL路徑，尚無原版逐像素色表，待精靈色表支援後升級。
  - Abort原因是新增bird_test在res_init前讀動畫，失敗結果被anim_get快取，導致後續烏鴉動畫斷言失敗；將資源初始化移到測試main入口修復，並加真實飛鳥動畫斷言防回歸。測試涵蓋初值、序列切換、離屏門檻／可見歸零、刪除壓縮後續拍、場景清空、生成倒數與8隻鳥群；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`。FUNCS.tsv已重產。
  ~~下一步：0x481b30（CBuilding 建築）起~~。

- 進度（11-16）：Poq.exe **0x481b30～0x4841f0 CBuilding 建築**完整37個入口已核對，含索引誤標CViewPort／CGameDialog的建築方法；停在NormalBuilding建構邊界。**done 16、skip 15、base 3、todo 3**（funcs_skip另含3個base工具對應，故新增18行）。
  - `engine/src/res.c`的parse_models／scene_load補模型矩形、類型、占地、顯示方式、門點、名稱及同物件共用動畫隨機起始幀；scene_building_cells在進場發布高位占地旗標、建築索引與獨立原點標記，修正先前Cell.building一直缺少來源的情況。模型部件數原先誤讀+0x22類型，已改讀+0x2a，部件上限由8補至40；scene_free釋放占地陣列。Actor／Scene／Anim／Sprite既有基礎直接承接，沒有等待基底系統。
  - `engine/src/viewport.c`的viewport_building_rect／viewport_building_draw／building_part_draw／building_occlude_draw對應模型投影矩形、底中上分層及遮擋重畫，取代舊的直接畫部件迴圈；building_highlight／building_display接main.c滑鼠命中與取消反白，原版遮擋平面X偏移<1優先的分支也回補。`light.c`的light_buildings補光源中心原版位址；BNT名稱及門點快取到Object供後續互動沿用。
  - 原版怪現象：光源格中心乘64加32；同物件所有動畫部件只擲一次rand()%1000再各對幀數取餘；高位占地只看u16低byte，walk開關僅作用低3位==1，原點標記不等於占地；遮擋座標和／分軸相等都不擋，X<1優先（含X負數、Y零）；反白同值不更新且取消恢復模型顯示方式，非動畫底層不反白；門名檢查所有分支最後均直接返回，不憑名稱自造開門效果。
  - skip：建構／解構及部件釋放、空入口、無呼叫者入口、DirectDraw髒區管理與無效果門名檢查。base_map補場景綁定轉接及兩個固定成功工具；fields.tsv補41列並更新3個舊欄位說明，roles.tsv補37個用途。未發現可證明整類別劇情不用的依據，不列DLC。
  - shortcut：SDL全重畫取代髒區與暫存表面；RGBA合併原版SBMP表面旗標分流，反白以加亮混色代替色表，待原始表面旗標／逐像素色表接入還原分流順序、光照及反白；部件動畫沿用共享時鐘加隨機初幀，等NormalBuilding更新改成獨立Spr拍數；拾取先用模型矩形，等CObj逐像素命中；固定40部件表超出時改動態陣列。這些近似均已在程式註記，沒有把未移植函數位址放進shortcut。
  - todo 3：建築縮圖等不同類別CScnClient小地圖生成／CMapDialog消費端；格子walk清除／恢復規則已放scene_building_cells參數分支，但尚未有遊戲呼叫端，等不同類別CTriBld狀態切換接入，未以位址註解冒稱done。門點已快取，互動消費端同等CTriBld；沒有新增未被遊戲呼叫的獨立函數。
  - skilltest新增占地低byte／高位／索引／原點、walk過濾及還原、負座標界限、遮擋等號與負偏移優先、反白恢復與底層例外、隨機初幀／停留拍、模型矩形、滑鼠反白取消、半透明實際像素／未知模式不畫／alpha恢復，以及陽周城真實占地與門點快取檢查。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。
  ~~下一步：0x4843a0（NormalBuilding 一般建築）起~~。


- 進度（11-16）：Poq.exe **0x4843a0～0x484590 NormalBuilding 一般建築**完整6個入口已核對；0x4845c0本體改設CTriBld虛表，停在類別邊界。**done 0、skip 4、todo 2**。
  - 三個建構／解構入口以既有scene_load／scene_free的Object陣列生命週期承接。更新入口逐一檢查模型部件動畫旗標，僅呼叫上輪已核對的CBuilding髒區比較；該本體兩次讀同一幀且未呈現推進，不據此自造動畫更新。SDL全畫面重畫承接這項平台行為，修正viewport.c先前「等NormalBuilding獨立拍數」的shortcut說明，仍保留共享時鐘限制。
  - 載入／卸載的模型、門點、占地及部件生命週期已有對應；兩入口尚需不同類別CGodClient的建築環境音分組／句柄系統，roles.tsv保留todo，並更新先前音效入口的等待原因，避免互相等待。沒有將這兩個未完成入口位址寫入程式註解。
  - fields.tsv補種類、環境音槽、繼承模型欄位與部件動畫旗標共5列；roles.tsv補6個入口並更新3個環境音用途。無新容器／工具，無base_map增項；無劇情不用的證據，不列DLC。本輪無新增遊戲邏輯，故不加實作鏡像測試。
  - 本批完整6入口：done 0、skip 4、todo 2（不同類別CGodClient音效系統）；未新增容器／工具，base_map無變動。FUNCS.tsv已重產；`cd engine && make && make skilltest`通過，最後一行`skilltest ok`（建置仍有5個未使用函數警告）。
  ~~下一步：0x4845c0（CTriBld 可互動建築）起~~。

- 進度（11-16）：Poq.exe **0x4845c0～0x485250 CTriBld 可互動建築**完整12入口已核對，下一個建構0x485270改設CCharactor虛表，停在類別邊界。**done 4、skip 5、todo 3**；另回補前輪CBuilding兩個walk入口done，依賴工具base 2、連線傳輸skip 1。
  - `res.c`的scene_load接載入：模型種類128..256建立互動建築執行期欄位，門第一個空格分詞與原版常數`go`比較後設感應旗標，再切狀態1。scene_building_state保留0/2靜止、1完成、5互動的原版分支、動畫序列0/2/1、音效預設及底／頂部件數；scene_building_cells的walk清除／恢復接到切換呼叫端，解除前輪等待。scene_buildings_tick與building_part_tick接主迴圈動畫完成回1；整個更新入口仍保留todo，沒有用位址註解宣稱已完成感應區。
  - `viewport.c`繪圖、模型矩形fallback、反白及遮擋重畫改讀物件獨立序列、幀與底／頂分層，移除互動建築強制共用序列0的簡化路徑；一般建築沿用原時鐘。`main.c`的input_building照一圈螺旋查旗標0x88、種類128..256非門及狀態0/2，接離地圖游標查詢；地圖內直接取格建築並按原版種類／狀態選游標。`near.c`的cell_bld改讀格子占地索引，刪除一律無建築的舊簡化版。
  - 原版怪現象：狀態0/2反而清walk，1/5恢復walk；模型128在1/5把底層數改全部，0/2恢復；同狀態不再寫格；非門只有0/2能切5，門不限；第一個完成的動畫部件即切回1；最後一幀最後一拍判完成，不多繞一圈；名稱只按空格分詞，`go`門不自動感應。門名檢查沒有可見效果，未自造開門。
  - shortcut：SDL全畫面重畫／單機共享Object承接髒區與同步；音效沿用非位置播放，位置混音接入後補擺放格；互動部件以遊戲拍獨立推進，原版精靈計時器更新來源確認後校準排程；部件仍限MAX_PARTS，原版逐像素拾取仍待CObj拾取接入。均未將未完成入口位址放進shortcut。
  - todo 3：更新的感應區／倒數（空掃設1、命中NPC組10000..60000設1000）與`s_pctstart`的0x13d角色命令，等不同類別CCharactor命令接收端；使用入口的0x8e命令及破壞入口的死亡／恢復walk等CCharactor建築互動／傷害呼叫端。Actor、格子、模型、名稱及動畫基礎已有對應，不以基礎缺失作等待理由；尚未新增只供測試呼叫的完整使用／破壞函數。
  - skip 5為建構、兩個解構、空入口、卸載，由Object陣列生命週期承接；依賴的字串封包發送為連線層skip。base_map補token拷貝及CObj死亡設值工具，fields補16列、roles補本批及依賴用途。沒有足夠證據認定整類別劇情不用，不列DLC。
  - 測試揭露RoleMdl一般角色的beast/air欄位未初始化，堆積殘值會把角色204誤判飛鳥；補零初始化並加回歸檢查。skilltest新增狀態限制、同狀態不重寫、底／頂分層、walk過濾、螺旋順序、旗標／種類／門／狀態排除、格索引、動畫序列與最後一拍完成檢查。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`（原有5個未使用函數警告）；FUNCS.tsv已重產。
  ~~下一步：0x485270（CCharactor 角色）起~~。

- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor 角色，本輪僅部分完成，整類別尚未完成**；確認0x490240建構改設CNpc虛表，未進下一類別。新增 **done 7、skip 28、base 2**；類別範圍合計done 23（含舊16）、skip 28、base 2、**todo 67**。這67個包含同類別尚未翻的實作，並非全都在等其他系統，不可把本輪當成整類別完成。
  - `main.c`新增actor_group：同組完全不動，換組先清目標與待命令再寫組，接怪物建立、召喚、馴服及STORY的actors_sync／0x51c；actor_clear_target及actor_cancel_command分別只清目標／只將命令代號置-1，後三參數保留。待命令四欄是最小Actor基礎，完整命令寫入／消費尚未接通，未標那些入口done。
  - actor_flags／actor_set_flags保留32位遮罩查詢及OR／AND-NOT，取代type8招式旗標的重複設值，接無效目標篩選；actor_seek／actor_chase保留層號大於10各加8，接怪物索敵、追擊及自動選敵，刪除直接取模板的舊路徑。原版怪現象：旗標查詢回遮罩值而非1；同組不取消命令；取消命令不清參數；層號比較不排除255且範圍不夾成正數。
  - shortcut：角色建立仍僅地面層1，多層建立接入時寫原版byte；指標取代物件句柄；單機共用Actor不另送組別同步副本。頭頂文字、獨立附加精靈、技能鏈／選招、完整狀態byte副作用／20項修正／方向更新、主命令及建築互動仍未完成；這些同類別依賴不能寫成等其他系統。前輪CTriBld的3個todo亦尚未解除。沒有新增未被遊戲呼叫的完整功能入口。
  - skip 28：21個索引無呼叫者、5個建構／解構／卸載及2個純狀態封包傳輸；base 2為四欄命令相等與分軸嚴格距離比較。沒有因DAT_00663804非零條件把角色命令主分派誤判連線版。roles.tsv記錄102個入口用途或未完成情況；fields.tsv新增／校正85項CCharactor與9項SkillUnit偏移，修正舊表+0x507與+0x589反寫：前者生命百分比、後者顯示名稱。未有整類別劇情不用的證據，不列DLC。
  - skilltest新增換組／同組、取消目標及命令的獨立效果、保留命令參數、32位高位旗標與非布林遮罩、層0／10／11／255及負範圍邊界。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；另以-DSTORY檢查main.c語法通過，原有5個未使用函數警告；FUNCS.tsv已重產。
  ~~下一步：0x485b80（CCharactor 同類別未完成，先補技能鏈／選招並完成整類別）起~~。

- 進度（11-16）：Poq.exe **0x485b80～0x490220 CCharactor 角色續補，本輪仍未完成整類別**；沒有進入0x490240的CNpc。新增 **done 11、skip 2**，另完成依賴層差工具1、容器base 2；本類別合計 **done 34、skip 30、base 2、todo 54**。未達成本輪要求的整類別完成，這54個含同類別待翻業務，不能描述成全在等別的系統。
  - `main.c`新增ActorSkill有序陣列、獨立外部識別／技能ID、左右手ID、選取索引與射程修正；actor_skill_select／actor_right_skill／actor_left_skill接hero_skill_apply與兩手快捷選單。actor_skill_remove接場景模板與玩家技能重建；actor_skill_enable／actor_skill_update接技能加點，當拍更新最大／最小距離，不只修改UI列。actor_skill_enabled接mon_pick及cast_start，移除怪物一律可用的shortcut。
  - actor_skill_range取代怪物選招、pursue、hero_cast與`unit.c` main_new直接讀技能列射程的路徑；actor_cooling接追打冷卻判斷，actor_can_move接actor_tick統一移動入口，actor_clear_flags接type8觸發。依賴的actor_layer_diff照原版四層分組相減，並未將同類別技能查找標成完成。
  - 原版怪現象：最大距離等於3不加修正，但速度為0時的Enable限制從3開始；距離修正可以變負；層255仍歸最高組；Enable回原整數而非布林；精確ID命中不檢查Enable；跨層近距離或目標高兩級以上清選取，但右手ID照寫；左手不動右手選取。相同外部識別只改／刪第一個；刪除不清目標、不改手ID；更新先重選手ID才寫新ID／距離，保留其順序。
  - shortcut：技能候選暫由現有NPC／玩家技能列建立，尚非RoleMdl執行期模板表；每角色限64項，超限印診斷；未入鏈臨時技能仍用技能列作橋接。fallback的比較暫存反編譯未定義，暫取候選順序第一個啟用項，取得原組語與模板表後校準。單機手選取／修改省略傳輸。上述未完成的建立／查找／重建入口均未加完成位址註解。
  - skip 2為紙娃娃兩套精靈預讀／卸載轉接，交role_load／preload_roles、動畫快取與roles_clear；base_map補清鏈與刪游標節點的陣列對應。fields新增鏈數量偏移並更新16個技能欄位的實際對應，roles更新本批用途；沒有整類別劇情不用的證據，不列DLC。
  - todo 54：移動施法、完整新增技能及模板重建、候選技能觸發、主命令、狀態byte副作用／20修正、角色更新／動畫／繪製／浮字／外觀、死亡與場景占位等仍須續翻同類別；不是等其他系統。前輪CTriBld三項仍等角色建築命令／破壞呼叫端，未解除。
  - skilltest補Enable非布林及速度／距離邊界、層差篩選、負射程、精確ID命中停用技能、兩手互不干擾、重複識別只改第一項、刪除選取與索引搬移、保留目標／手ID、負冷卻及MethodMain存活時禁止移動。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，原有5個未使用函數警告；FUNCS.tsv已重產。
  ~~下一步：0x485b80（CCharactor 尚未完成，續補移動施法／完整技能鏈、命令與狀態；不可跳CNpc）起~~。


- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor 角色續補，本輪僅新增done 2，整類別仍未完成**；範圍合計 **done 36、skip 30、base 2、todo 52**，未進CNpc。尚未達成使用者「整個類別一起做」要求，不能將剩餘同類別工作改列等待其他系統。
  - `main.c`的actor_skill_add對應完整SkillUnit新增，接NPC／玩家技能建立，取代actor_skill_append直接寫陣列：全部欄位先插入，再依hand低兩位選左右手，模板檢查最後進行。原版怪現象：重複ID照樣插入；選手仍精確命中第一個同ID節點（即使停用）；不在模板僅診斷而不回滾；hand高位不影響選手。
  - actor_skill_find精確命中仍不查Enable，fallback改成按獨立模板ID表的優先序選啟用項，不再依技能鏈順序任取第一個；非模板技能不參加fallback。NPC／玩家重建清除並重新建立候選模板表，刪除技能不改模板表。
  - shortcut：RoleMdl執行期原始技能表仍未載入，模板ID表暫由已載入NPC／玩家技能列橋接；fallback的比較暫存反編譯未定義，按模板索引最小者解讀，取得原組語後校準。維持64項容量並在超限診斷，單機省略技能同步；模板缺項以stderr代替原版模態訊息。未將模板重建入口標done。
  - 新增skip 0、base 0；fields補RoleMdl的ushort模板數量及ID陣列偏移，roles更新兩入口。todo 52仍含移動施法、簡單技能新增、原模板重建、主命令、狀態副作用／20修正、更新／動畫／繪製／文字、死亡占位等同類別工作；前輪CTriBld三項仍等角色建築命令／破壞呼叫端。未新增DLC分類。
  - skilltest補模板順序與鏈順序不同、停用／非模板fallback排除、精確命中停用非模板項、完整新增欄位、插入後切手、重複ID與hand高位。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；FUNCS.tsv已重產。
  ~~下一步：0x485b80（CCharactor 尚未完成，續補移動施法／模板重建、命令與狀態；不可跳CNpc）起~~。


- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor 角色續補原模板重建／技能可用性，本輪新增done 3，整類別仍未完成**；遊戲入口合計 **done 39、skip 30、todo 49**，未進CNpc。未達成「整個類別一起做」要求，49個待辦仍含同類別實作，不能列成等待其他系統。
  - `main.c`的actor_skills_rebuild照原模板順序清鏈／重建，接actor_skills_init與hero_skill_apply，取代NPC／玩家技能列代建模板的舊路徑。`res.h`新增RoleDef的ushort數量與100項ID表；`res.c`的roledefs_load載入RoleMdl.rmd，按角色表列序而非角色ID分配（第一組是角色29、第二組角色206）。原版怪現象：保留重複ID、無招式記錄的模板項只保留於模板不進技能鏈、全部模板節點外部識別為0、不改左右手ID，新增min/max取招式預設且Enable/p8/p9均1。
  - actor_can_use_skill／actor_can_use_id接mon_pick與cast_start，還原不能攻擊旗標、MethodMain中斷、空鏈及速度／Enable判斷。`res.c`載入MethodMdl時掃全部iFlag計算interruptible，包括超過現有24項觸發容量的項目；不是拿Reserve4或wType判斷。原版怪現象：目前招式有flag4就可越過不能攻擊與停用；空鏈且無目前招式允許NULL；精確ID命中停用項不轉選其他重複項；Enable保留非布林整數。
  - 接線回歸：原模板key=0會和玩家第0招相撞，玩家技能列改用index+1識別，技能加點同時更新；選手命中模板項時按招式ID接玩家技能結算列，避免模板沒有row指標使主角變空手。模板預設射程與玩家升級射程各自保留。這是引擎識別／結算橋接，未修改原版查找第一個同ID／同key的順序。
  - shortcut：有序技能陣列容量由64增128，容納原模板加玩家／NPC技能；超限仍診斷，需更多時改動態配置。清鏈後明確清陣列選取索引以免指向新節點；玩家外部識別暫用index+1，完整技能物件接入後換句柄。單機執行服務端Enable檢查，網路分流仍待連線支援；fallback反編譯未定義暫存的組語校準仍在。原版主命令／地點施法仍todo，沒有以這三個入口宣稱整條命令／施法已完成。
  - 本類別新增skip 0、base 0；依賴的原版rmd管理物件入口索引無呼叫者，依規則新增skip 1，由引擎最小模板資源讀取承接。fields更新原模板兩偏移、技能鏈來源，補MethodMdl三偏移；roles同步用途。沒有新增DLC分類。49個todo仍含移動施法、簡單技能新增、候選技能觸發、主命令、20修正／狀態副作用、更新／動畫／繪製／文字、外觀、死亡占位等；前輪CTriBld三項仍等角色建築命令／破壞呼叫端。
  - skilltest新增真實rmd列序／數量／頭尾ID、模板重複與無效ID、清鏈及手ID保留、預設距離、非布林Enable、空鏈、速度限制、禁止攻擊與招式中斷優先序、招式598第30項flag4、模板與玩家第0招共存／升級不改模板等回歸。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，原有5個未使用函數警告；FUNCS.tsv已重產。
  ~~下一步：0x485b80（CCharactor 尚未完成，續補移動施法／候選技能觸發、命令與狀態；不可跳CNpc）起~~。


- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor 角色續補移動施法／候選技能，本輪新增done 3（類別內2、格子基礎1），整類別仍未完成**；類別入口合計 **done 41、skip 30、todo 47**，未進CNpc。尚未達成「整個類別一起做」，47項仍有同類別實作，不能列為等待其他系統。
  - `main.c`的actor_move_cast對應移動技：組別的unsigned減法篩選、模板移動技思想等於1、技能鏈順序／Enable／速度／manual3、六圈空格及整數歐氏距離大於2；接戰鬥中地面點選，取代無條件走路。原版怪現象：找到距離合格點後，即使可用性失敗或施法入口拒絕，仍回成功而吞掉走路命令；10000..60000組包含兩端都不嘗試，9999／60001可嘗試。
  - `near.c`的vacant_cell還原格子查找：含中心到指定圈數，逐圈掃上／下／左／右邊，先夾地圖邊界，只檢查層可通行與角色占位；不是既有螺旋掉落空格查找，也不額外排除道具。供移動施法實際呼叫。
  - `main.c`的actor_candidate_cast依四個正ID候選查找，首個有SkillUnit者向目前對象或自身世界座標施法；拒絕不換下一招。`res.c`／`res.h`讀RoleMdl欄37的移動技及欄38星號分隔的四項戰鬥狀態技，actor_setup接入候選。入戰由pursue與strike消費。actor_cast_at共用技能結算並把明確地點／對象／距離交`unit.c`的main_new，地點施法不再沿用舊foe；右鍵改用既有input_char與鏡頭格子換算，刪除monster_at舊換算。場景載入即設定unit_scene，避免首次點選尚無查格場景。
  - shortcut：完整角色主命令及狀態byte分派仍未完成，入戰暫由fight計時0→1橋接；施法入口的記住命令清除／同步與戰鬥狀態副作用仍待主命令接齊，沒有新增其原版位址標done。點選地點暫用格子中心，待像素世界座標逆投影接入；無EasyRPG技能列的模板招式仍可播放及觸發，沒有額外技能數值結算。格子角色占位沿用Actor陣列查找，仍有既有同格多角色／占位順序差異。原有技能容量及fallback組語校準限制不變。
  - 新增skip 0、base 0；fields補RoleMdl移動技思想、四項候選、速度欄位並更新Actor候選對應，roles更新三函數；FUNCS.tsv已重產。todo 47仍含簡單技能新增、主命令、記住命令／排隊、20修正與狀態副作用、更新／動畫／繪製／文字、外觀與死亡占位等本類別工作；不是等別的系統。前輪CTriBld三項仍等角色建築命令／破壞呼叫端；無新增DLC分類。
  - skilltest補真實模板欄位、查格邊序／層／占位／六圈邊界、組別端點、思想精確值、速度／停用、距離取整、可用性失敗吞命令、明確地點不沿舊foe、候選首命中拒絕不換招／冷卻不攔、入戰只觸發一次、自身／目標座標、右鍵地點及同隊技組別限制。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，原有5個警告。
  ~~下一步：0x485e10（CCharactor 尚未完成，續補簡單技能新增／主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor 角色續補名稱／主人識別，本輪新增done 5、skip 1，整類別仍未完成**；類別入口合計 **done 46、skip 31、todo 41**，未進CNpc。尚未達成「整個類別一起做」要求；41項仍含同類別工作，不能列成等待其他系統。
  - `main.c`的actor_init_name／actor_name_set還原SInit種類10..16才可取／寫名稱。核對初始化載入後，確認SInit +0x38讀的是「事件ID」，不是HUD顯示名稱；actor_setup複製現有ONT事件來源，spawn_npcs綁定及點NPC對話改讀角色內記錄，刪除這兩處直接讀NpcSpec事件的舊路徑。`res.h`的NpcSpec.event從32補成原始64 bytes，保留長事件ID及大小寫。
  - actor_name對應獨立的顯示名稱getter，HUD經actor_title橋接原有怪物／劇情文字來源後消費Actor.display_name；改事件ID不會順便改顯示副本。顯示編號與後綴更新仍todo，未以getter宣稱已完成更新入口。
  - actor_owner_set／actor_owner只寫／讀主人識別，以現有Actor指標承接，接summon與join（馴服／分身／放出）、monster_think主人跟隨中心；不額外改組別／選取／目標。
  - skip的簡單技能新增只有已略過的客戶端同步分派呼叫，用來建立零欄位技能副本；單機共用Actor技能鏈，以既有完整新增及模板重建承接，不為它新增無呼叫者的副本函數。本輪base 0，沒有新增DLC分類。
  - shortcut：動態角色沒有ONT來源時以最小SInitCha種類10承接；事件ID超出原始64-byte容量時截斷，遇到超長ID再升級動態字串。顯示名稱仍由原有EasyRPG／怪物來源橋接，完整編號後綴入口未移植；主人識別沿用現有Actor指標，完整物件句柄接入時一併升級。
  - fields修正SInit事件ID／顯示名稱分離與主人識別，roles更新五函數與同步副本skip原因；FUNCS.tsv已重產。todo 41仍含移動狀態／模板初始化、主命令／排隊／記住命令、20修正與狀態副作用、更新／動畫／繪製／文字、外觀及死亡占位；前輪CTriBld三項仍等角色建築命令／破壞呼叫端。
  - skilltest新增種類9..17及負數邊界、63-byte事件ID、空名稱、事件ID／顯示副本互不覆寫、動態記錄、主人讀寫與清空不改組別／選取、HUD顯示來源不覆寫事件ID。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，仍為原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor，本輪僅修正模板候選技能覆寫規則，未完成整類別**；新增done 0、skip 0、base 0，類別入口仍為 **done 46、skip 31、todo 41**，未進CNpc。
  - 依0x4862e0本體修正`main.c`的actor_setup：四個候選各自判斷，模板值65535保留角色舊值，0及-1照值寫入。新角色候選先設-1，與重套模板分開；修正原先將65535直接改寫成-1而丟失既有候選的差異。真實NPC／怪物建立與主角換場景均使用此入口。
  - shortcut：actor_setup仍沒有完整模板入口的狀態103、種類／AI組／層重定位、完整速度及縮放副作用，不在程式註解加該入口位址冒稱done。移動入口、主命令、狀態、繪製與生命週期等41項仍是本類別未完成工作，不能改列為等其他系統；前輪CTriBld三項依賴不變。沒有新增DLC分類。
  - fields補四候選中後三項的角色及模板偏移；roles更新模板入口的已修正部分與未完成部分，FUNCS.tsv已重產。funcs_skip.txt與base_map.tsv無新增項。
  - skilltest直接走actor_setup，檢查新角色候選初值、全部65535保留、混合65535／0／-1／正ID覆寫，以及再次重套仍保留更新後值。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；建置仍有原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor，續補記住命令清除與速度選取；整類別仍未完成，未進CNpc**。新增done 1（清記住命令），skip 0、base 0；另移除舊速度公式註解提及角色屬性分派位址造成的誤判done 1，範圍合計仍 **done 46、skip 31、todo 41**。
  - `main.c`的actor_forget_command照原版僅清記住命令代號；原命令1/2設延遲50，其餘0，保留三參數。接入施法收尾及對話命令；施法失敗也檢查，記住同技能／無目標／非敵對目標時清除，記住不同技能且仍有敵對目標時保留。現有點地移動及指定位置技能入口記錄對應命令；不是完整主命令分派，重送與延遲扣減仍todo。
  - actor_speed改依第1修正、移動跑步、戰鬥／普通速度順序選取，跑步3/2優先於戰鬥速度；門檻>=101才減半（修正舊註解反向）。getter保留原版負值及大於59的結果，只在actor_tick查速度表前夾0..59；刪除monster_think按有無目標覆寫普通速度的簡化路徑。actor_setup保存兩種模板速度。
  - 保留怪現象：速度倍率的非零最小1在加速與減半之前判斷，後續仍可得到0／負數；跑步乘3除2先整數截斷。清記住命令連做兩次，第二次會把50改回0，不是無副作用。
  - shortcut：戰鬥狀態沿用fight，平常／移動狀態由現有移動迴圈橋接，尚非完整CDynamicObj狀態機；第1修正及速度門檻欄位已備妥，但完整20修正表、角色屬性分派與設定入口仍todo。單機施法收尾採模式2；普通左手施法／完整服務端命令及戰鬥副作用未接齊，不以此宣稱整個施法入口完成。沒有新增DLC分類。
  - fields補速度覆寫欄位並修正速度／門檻、記住命令欄位；roles更新用途與未完成限制，FUNCS.tsv重產。41個todo仍含本類別移動／模板、命令、屬性／狀態、繪製與生命週期，不能當成等待其他系統；前輪CTriBld三項依賴不變。
  - skilltest新增普通／戰鬥／跑步優先序、覆寫0速度、100/101門檻、負值及超速度表結果、清命令參數保留／重入、失敗施法仍收尾與敵對目標保留條件。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，建置仍有原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor，續補角色遍歷並替換怪物的最近目標簡化迴圈；整類別仍未完成，未進CNpc**。新增done 1（0x48b9e0）；移除舊AI註解誤標的done 3（0x488a10、0x489730、0x48ba50），範圍合計 **done 44、skip 31、todo 43**；本輪skip／base各0。
  - `near.c`的actor_near_begin保存模式；只有半徑-1取角色索敵範圍及高層+8，其他負值與0回空。接monster_think取代掃全陣列、用Chebyshev距離找最近角色的舊迴圈，改用現有near_begin／near_next格子螺旋順序，取首個符合者，不再額外套圓形距離限制。
  - actor_near_next已接旗標0x20010、第10修正50、自己禁止攻擊／狀態102、同組戰鬥角色轉交目標與模式1／2／3。RoleMdl第27欄程序第一值接RoleDef.program，40允許跨格協助；普通程序僅整數格歐氏距離<1才協助，目標距離嚴格小於索敵範圍。
  - 保留怪現象：半徑不合法時仍先更新模式，但不重設底層遍歷游標；高層加8不影響明確半徑；候選方框仍沿用原版邊長3r；同組轉交的目標不再套候選本身的旗標篩選。
  - shortcut：篩選續入口仍缺模板顯示方式（僅第10修正排除50），戰鬥／狀態沿用fight／Buff，初始化記錄座標由目前整數格承接，物件種類由Actor名單承接，模式2暫用劇情handle；需補同類別模板／狀態與CObj基礎識別，不能列為等待不同遊戲系統。陷阱仍沿用既有技能範圍橋接。頭頂文字本體已核對，未寫新函數或標done。沒有新增DLC分類。
  - fields補模板顯示方式與物件識別，更新程序標識、第10修正及模式欄位；roles記錄上述完成與誤標修正，FUNCS.tsv重產。skilltest新增螺旋首個優先、旗標／狀態篩選、預設與明確半徑、高層、失敗保留游標但改模式、四種模式、同組協助及程序40分支；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，仍有原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-16）：Poq.exe **0x485270～0x490220 CCharactor，續補顯示方式／目標查詢，並補基礎大小setter 0x496530；整類別仍未完成，未進CNpc**。新增done 3（0x4887b0、0x488a10、0x496530），移除誤標done 2（0x48b2f0、0x48ec60）；主範圍合計仍 **done 44、skip 31、todo 43**，另有範圍外大小setter done 1；本輪skip／base各0。
  - `main.c`的actor_display依第10修正非零優先，否則取模板顯示方式；`res.c`按已核對原版讀表器的第29欄載入RoleDef.display，`near.c`改走getter，模板50也會排除，不再只看修正。actor_size_set只存值，actor_setup依模板size<7取1，否則減5；完整模板入口仍todo，不因這一段寫其位址。
  - actor_target以現有Actor場景名單查找，失效才清目標；角色旗標0x10只回空、保留識別，已死仍可查得，陷阱不套角色旗標。已接pursue、monster_think、同組協助、候選施法及無滑鼠HUD目標；刪除怪物把0x10當作直接清目標的舊近似。AI的死亡／追擊範圍取消仍由呼叫端處理。
  - 保留怪現象：顯示修正負數也是有效覆寫；同組協助的目標雖不重套0x20000候選旗標，仍會被目標getter的0x10擋住（修正前輪測試與描述）。大小setter不夾限、不移動角色；模板size=6得1、7得2。
  - shortcut：CObj查找沿用Actor陣列與指標，場景整批重建；若同場景開始重用Actor槽，必須補帶世代識別。完整128狀態重算與20修正表都仍未完成；原有速度子集／讀表註解，以及unit_spawn未做效果的shortcut，分別誤觸done，已移除位址並在roles恢復todo。這些與移動、模板、主命令仍是同類別／基礎工作，不能列為等待其他系統。沒有新增DLC分類。
  - fields補大小與模板顯示方式對應；roles補用途與限制，FUNCS.tsv重產。skilltest新增模板50／非零負數覆寫、getter死亡／失效／0x10保留與恢復／陷阱、同組轉交及模板大小邊界，既有追擊／候選施法／HUD測試改把目標登記入正式場景名單。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；建置仍有原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補目標動畫選擇；本輪仍未完成整類別，未進CNpc**。新增done 1（0x48c190）、skip 0；另記base 3（整數欄位拆分及30槽容器工具），主範圍合計 **done 45、skip 31、todo 42**。
  - `main.c`的actor_animation_select／actor_animation_target取代actor_seq原先單一條件式：依原版優先處理擊退、挨打、跑步、戰鬥、普通動態狀態；actor_tick及action_tick保存目標，actor_draw、出手點及viewport矩形共用純讀取選擇，查矩形不改方向／目標。刪除原先只看moving/fight選四種序列的簡化路徑。
  - `res.c`依已核對讀表器載入第10欄第二個*整數的跑步序列；缺項65535。第11／14欄缺項也還原65535，挨打觸發改以65535判未指定，保留合法序列0。
  - 保留怪現象：指定負序列的方向取C帶符號餘數；戰鬥動態狀態20..30不改目標；擊退／挨打序列未指定時保留舊目標，只有擊退序列超出模型序列數才退至戰鬥休息。跑步先於戰鬥判斷，但門檻>=101、旗標0x20000或0x80000會退至普通走路。
  - shortcut：狀態0／101與第5修正仍以現有fight／Buff／knock橋接，面向經DIR_MAP銜接；完整狀態與動態物件完成時換來源。小動物專用三序列仍沿用原有分支。移動入口、模板、主命令、狀態表副作用、動畫推進與繪製等42個todo仍是本類別工作，不列成等其他系統；前輪CTriBld三項依賴不變，沒有新增DLC分類。
  - fields補目標動畫／原版方向與模板序列偏移，roles更新動畫入口，base_map補工具，FUNCS.tsv重產。skilltest新增所有動畫優先序、20..30邊界、跑步門檻／兩個排除旗標、缺項保留／超界fallback、負序列與唯讀查詢。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，建置僅原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。



- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補角色有效速度套用與走路動畫節奏；本輪仍未完成整類別，未進CNpc**。新增done 1（0x48b2d0），範圍外移除誤標done 1（0x496ae0）；主範圍合計 **done 46、skip 31、todo 41**；本輪skip 0、base 1（0x4c9050速度查表工具）。
  - `main.c`新增actor_speed_update／actor_move_speed，模板建立、一般移動、衝刺及擊退共用A/B/C套用路徑，刪除各處重複查表；衝刺也會夾限索引，不再以未夾限的有效速度讀表。Actor保存A／B／C、A倒數初值與走路延遲調整；actor_frame對普通／戰鬥走路八方向套調整，各角色獨立，不修改共用Role資源。擊退固定速度20亦經此路徑。
  - 已逐項讀Poq.exe唯讀區核對float／double分段常數。保留怪現象：動畫速率只算(A/B)/(普通速度A/B)，完全不含C；比例恰等1才得0，其他分段為7／5／3／1及-1～-5；float的0.7轉double後小於double 0.7，仍得3。普通速度0或2的A為0，NaN／無限大依原版比較次序得到-5，不自行補成1倍。
  - shortcut：完整動態狀態退出虛擬入口仍未接，暫以dynamic_state／moving及腳印欄位承接停止；A倒數已保存，完整動態移動消費尚待補；現有動畫播放器仍將調整後延遲夾到至少1拍，需核對Gfx播放器。底層動態速度函數因舊擊退註解誤標done，已移除位址、在roles恢復todo；沒有把局部套速宣稱完整動態系統。41項主範圍todo與該底層入口仍是本類別／基礎工作，不能列成等其他系統；未新增DLC分類。
  - fields補動態移速A/B/C、A倒數與序列調整；roles／base_map更新並重產FUNCS.tsv。skilltest新增比例全部邊界、float與double比較、NaN／無限大、索引夾限、零速度停止、有效速度接入，以及共用模型兩角色不同動畫速率／不改原幀資料。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，建置僅原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補顯示名稱編號；本輪未完成整類別，未進CNpc**。新增done 2（0x4887a0、0x48e820）、skip 0、base 0；主範圍合計 **done 48、skip 31、todo 39**。
  - `main.c`新增actor_name_number／actor_display_name_set，Actor保存獨立display_number；角色建立初值-1，HUD的actor_title改經原版名稱更新入口，刪除直接覆寫display_name的簡化路徑。每次HUD更新從基底名稱重組，不重複累加後綴；SInit事件ID不變。
  - 已核對原版名稱入口機器碼所引用的唯讀區字串，格式確為`%s(%d)`。保留怪現象：編號不論正負都先保存，只有畫面客戶端且正數才加半形括號；0／負值原樣顯示基底名稱，空基底名稱配正數仍顯示`(n)`；單機省略同步封包。
  - shortcut：原版CString仍以256-byte UTF-8緩衝區承接，超長名稱才需改動態字串；SInit顯示名稱仍以現有EasyRPG／怪物名稱來源橋接，完整SInit載入後改原始來源。39個todo仍是本類別的移動入口、模板、主命令、狀態及繪圖等工作，不能列為等待其他系統；未新增DLC分類。另核對頭頂文字槽配置／繪製與移動修正入口，尚無完整接線，保留todo，不標done。
  - fields／roles更新並重產FUNCS.tsv。skilltest補正負／零／最大正整數、客戶端／非客戶端、空名稱、HUD連續更新及基底改名保留編號。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，建置僅原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，核對移動入口與頭頂文字依賴；本輪未完成整類別，沒有新增已完成遊戲函數**。主範圍仍為 **done 48、skip 31、todo 39**；依賴工具新增skip記錄2、base 3（CGfxRenderDevice建構／解構、Chat.Dlg字串取值）。
  - 核對0x486270／0x4862e0及動態進出狀態：前者會直接寫第6修正的值1／期限-1再更新精靈參數，不能以現有dash／actor_tick代替完整動態入口；後者仍依賴狀態103與動態位置重設，維持todo。沒有把這些同類別／基礎依賴列成等待其他遊戲系統。
  - 頭頂文字的8槽配置、繪圖時倒數、首尾8次淡入淡出、中段上飄每畫面扣兩次已核對並記入fields／roles；普通文字覆寫槽0，上飄先找第一空槽，全滿才換第一個上飄槽。原版全滿且無上飄槽可能索引-1；尚未實作，不自行把猜測標done。
  - 實際接線核對：一般頭頂文字來自訊息佇列form-feed加二進位物件ID，不能用strlen承接；NPC隨機台詞則由模板第40欄四個值與Chat.Dlg提供。本輪整合驗證未在ID1..1999找到有效平時台詞區間，已撤掉臨時NPC接線與文字實作／測試，避免留下無遊戲呼叫者或錯用全域tick的shortcut。這項證據不足以斷定全部劇情不用，沒有新增DLC分類；文字兩入口仍todo，未標done。
  - 新偏移與用途已記錄，FUNCS.tsv已重產；引擎與skilltest程式維持本輪開始版本。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；本輪沒有新增遊戲邏輯，因此沒有留下新邏輯測試。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補快捷索引選取；本輪仍未完成整類別，未進CNpc**。新增done 1（0x48d6a0）、skip 2（0x48d2c0、0x48d740）、base 0；主範圍合計 **done 49、skip 33、todo 36**。
  - `main.c`新增actor_skill_select_index，使用既有Actor.skills有序陣列與actor_layer_diff，接SDL數字鍵及hero_selected_skill_apply實際攻擊資料；抽出hero_skill_apply內原有重複綁定段。按鍵五組依原版為無修飾0、Ctrl 9、Shift 18、Shift+Alt 27、Ctrl+Alt 36，加數字鍵1..9的零起始索引；Alt單按沿用0，Ctrl+Shift沿用18，三鍵同按用36。
  - 保留怪現象：空鏈不改舊選取；鏈內停用項照樣能選；越界清空；跨層且最大距離<3、或目標高出兩級以上才拒選；不改左右手ID。選取經既有目標getter，0x10隱藏目標不參與層限制，失效目標由getter清除。空選取不再以ID -1誤配EasyRPG技能列。
  - shortcut：Actor.atk同時承接播放中的招式與下次攻擊資料，按鍵在動作／施法中改選取時只排待綁定，等動作結束才換資料，避免覆寫目前MethodMain；完整角色技能使用入口接入後拆出播放快照。既有技能鏈來源仍有先前記錄的模板／玩家外部識別shortcut，沒有新增手機操控。
  - 兩個skip均核對總表唯一呼叫者為已略過的客戶端同步分派：一個解碼狀態封包byte、一個接收組合旗標整值。只略過同步副本入口，沒有把完整服務端狀態、禁攻中斷或第12修正標done。其餘36個todo仍多為本類別依賴，不能列為等待外部系統；未新增DLC分類。
  - fields／roles已更新並重產FUNCS.tsv。skilltest新增45格與8種修飾組合、停用項、空鏈、負／過大索引、跨層距離臨界、隱藏／失效目標、左右手ID保留、實際攻擊綁定與動作／施法期間延後套用；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，建置仍為原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補完整八鍵屬性分派；本輪仍未完成整類別，未進CNpc**。新增done 1（0x48f7a0）；主範圍合計 **done 50、skip 33、todo 35**。範圍外新增skip 1（0x496ac0索引無呼叫者），base 1（0x495880反向方向純整數工具）。
  - `main.c`新增actor_property／actor_properties_update，八種鍵名精確比對：MOVE_SPEED、FIGHT_SPEED、SRANGE、VRANGE、ZMYJ、HXYJ、JDR、DJHF；接actor_setup、hero_derive／mon_derive及story_msgs的@屬性訊息，刪除原先忽略該訊息的分支。怪物表vrange也接入原始屬性，技能／裝備改值後同步範圍欄位。索敵以模板值加角色修正表示，避免複製模板基準導致重新載入模板時快取失效。
  - 原版保留：MOVE_SPEED正值以整數v*150/(v+150)換算；FIGHT_SPEED正值以float v*120/(v+120)換算，再取100/(100+v)，分母0只改成1，負倍率不夾限。Actor.ex保留EasyRPG原始值，actor_move_bonus／actor_fight_ratio於消費端換算，避免重複壓縮；attack_speed移除舊整數百分比近似，action_tick更正為NPC組乘倍率、其他組間隔加隨機，組別判斷不依mon指標。VRANGE始終設值+12，唯NPC組10000..60000不加索敵；SRANGE僅原距離>3套用，未知鍵名完全不動。
  - shortcut：冷卻仍於動作結束裝入，完整MethodMain綁定時改回原版裝入時機；NPC物件種類60的免間隔分支仍需物件種類基礎接線。未把完整MethodMain／角色狀態宣稱完成。移動入口依賴重新核對：部件群建立／移除的是殘影名單，並非單純修改精靈參數；用途與動態目的地重試／退出等偏移已更新roles／fields，這些基礎與同類別依賴仍須續做，不是等待其他遊戲系統。未新增DLC分類。
  - skilltest新增八鍵、未知鍵無副作用、NPC組兩端邊界及負值、跨層、長短技能距離、怪物表／技能重算、float攻速與-100／-101極值、實際動畫延遲及冷卻組別分支。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，仍為原有5個未使用函數警告。FUNCS.tsv已重產。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補武器種類byte更新；本輪未完成整類別，未進CNpc**。新增done 1（0x48fd30）、skip 0、base 0；主範圍合計 **done 51、skip 33、todo 34**。
  - `main.c`新增actor_weapon_type_set，保存Actor.weapon_type低8位；由story_msgs的0x52a及單機@W_Type屬性通知呼叫。actor_attribute_changed將通知交回獨立setter或既有八鍵分派，角色屬性重算共用此入口；既有actor_property對未知鍵的行為不變。
  - 原版保留：這個setter只寫byte，負數／大於255均截取低8位，不重選技能、不取消命令／動作、不換模型。shortcut：單機@W_Type直接交給setter，完整場景通知分派接入後由武器更新訊息送入；未把完整文字命令／封包主分派標done。武器種類byte也未猜成EasyRPG技能可用性使用的Hand欄位。
  - 再核對移動入口與狀態鏈，仍缺本類別狀態副作用、20項修正、動態狀態及精靈殘影基礎；這些不能列為等待別的遊戲系統。殘影建立／釋放、採樣間隔及混色欄位已補fields／roles，沒有以空實作或速度替代標done。頭頂文字實際呼叫路徑仍未解決，未新增無呼叫者包裝或DLC分類。
  - skilltest新增武器種類0、255、256、257、負值截位，以及技能選取／左右手／命令／播放中動作完整不變、通知鍵大小寫與尾綴精確比對、八鍵通知轉接測試。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，仍為原有5個未使用函數警告。FUNCS.tsv已重產。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補關係腳圈；本輪仍未完成整類別，未達成整類別完成要求，未進CNpc**。新增done 1（0x4883e0）、skip 0、base 0；主範圍合計 **done 52、skip 33、todo 33**。
  - `main.c`新增actor_relation_anim，使用scene_client_actor與原版is_foe(a,controlled)，按敵對→異組中立→同組目前選取→同組普通選圖，再以RoleDef.size<2分大小。八個資源ID由原版啟動程式的載入參數及this指標核對；已接actor_draw（影子後、角色本體前）及viewport_actor_rect，後者把腳圈納入可見矩形，避免角色本體出畫面時漏畫腳圈。
  - 保留怪現象：敵對判定是角色→操控角色，NPC組的敵人名單不能對調方向；異組中立即使被指著也不用同組選取圖；尺寸0或負值仍用小圖；無操控角色回0，不以死亡過濾取圖。選圖不改序列／幀／角色狀態。
  - actor_hover_set接畫面繪製前的既有滑鼠拾取，更新0x100並保留操控角色的圈，同一畫面供腳圈與狀態框消費。shortcut：引擎目前選人名單僅操控角色，隊伍多選與非角色物件標示等對應介面接入；沒有將完整狀態框分派標done。腳圈沿用共享遊戲tick播放器；角色完整縮放、覆色及合成矩形仍未完成，未把整個角色繪製或位置更新標done。
  - roles／fields已更新；其餘33個todo主要仍為同類別狀態、命令、更新與精靈基礎，不能當成等待外部系統。未新增DLC分類；下一步保留第一個尚未完成入口，不跳CNpc。
  - skilltest新增八種圖選取、size邊界、敵人名單非對稱、死亡／無操控角色、hover切換與旗標保留；軟體renderer逐一載入八種真實資源，檢查actor_draw實際產生像素及可見矩形，取消0x100後矩形不含圈。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；-DSTORY語法檢查通過，仍為原有5個未使用函數警告。FUNCS.tsv已重產。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補關聯識別setter；本輪未完成整類別，未達成整類別完成要求，未進CNpc**。新增done 1（0x48d8f0）、skip 0、base 0；主範圍合計 **done 53、skip 33、todo 32**。
  - `main.c`新增actor_search_link_set，原樣寫入32位Actor.search_link；已接story_msgs的G_NPCActive／0x51b第三參數>0分支，交給既有actor_near_begin模式2索敵消費。核對原版訊息分派後修正舊註解：此訊息不是NPC頭上標記。
  - 保留原版：setter不驗證識別、不修改foe或排隊命令；通知第三參數0／負數不覆寫關聯識別。shortcut：通知的三種分支另有狀態103啟閉，仍待同類別完整狀態入口；只完成獨立setter，沒有將完整訊息分派或狀態入口標done。共用物件識別仍以現有Actor.sh承接，完整句柄基礎接入後升級。
  - 核對移動入口、第6修正與殘影建立／繪製／釋放的依賴，以及生命百分比、第3修正和狀態17連動；未用簡化Buff或空殘影冒充移植。其餘32個todo仍主要依賴同類別狀態、命令、更新及精靈基礎，不能稱為等待別的系統，下一步不前移；本輪無新增DLC分類。
  - fields／roles已更新。skilltest以setter接模式2查找，測試32位識別、未知負值／零及目前目標／命令保持不變；`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`。-DSTORY語法檢查通過，原有5個警告；FUNCS.tsv已重產。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，續補紙娃娃合成縮放與統一命中矩形；本輪仍未完成整類別，未達成整類別完成要求，未進CNpc**。新增done 2（0x48ef40、0x48f200）、skip 0、base 1（矩形數學工具0x497d90）；主範圍合計 **done 55、skip 33、todo 30**。
  - `main.c`新增role_rect／frame_rect_union與role_rect_scale，actor_scaled_draw依PLI順序把整份紙娃娃合成到可重用SDL暫存表面，之後才縮放；接actor_draw的影子與角色本體兩條路徑。暫存繪製保存並還原renderer target、viewport、clip、scale及draw color，換場景時釋放表面。
  - `viewport.c`的viewport_actor_rect照原版先取本體矩形、只改y扣離地高度、合影子、縮放，最後才合不縮放的關係腳圈；角色可見名單及main.c的pick_box共用，刪除舊固定73×111點選方框，點選保留type2/3世界座標各加128。保留怪現象：縮放半邊增量先向零截斷再加倍，奇數尺寸不一定精確倍增；可見矩形的本體與影子先合才縮放，實際繪圖則分開縮放，兩者不可自行改成同一算法。
  - `res.c`讀RoleMdl欄28（+0x124），空欄65535；actor_setup遇65535保留舊比例、0與負數照寫。常數20.0f與0.5f已讀Poq.exe唯讀區核對。actor_scale_tick接固定拍迴圈，只有模板比例65535才每拍追第4修正1步，保留0x200移動中阻擋及0x400例外；完整模板初始化／每拍函數均未標done。
  - shortcut：SDL透明RGBA取代原版16/32位色鍵暫存表面，光照沿用現有整畫面light_draw；逐像素色表／格子光照基礎接入時升級混色。完整角色繪圖的狀態覆色、殘影、三附加精靈及標記仍todo；其餘30個todo主要仍依賴同類別狀態、命令、更新與基礎，不可列為等外部系統；第4修正的完整設置／期限仍待修正表入口。未新增DLC分類。
  - fields／roles／base_map已更新，FUNCS.tsv已重產。skilltest新增合成圖的實際放大像素／部件覆蓋順序、奇數與負比例、零尺寸不繪製、renderer狀態還原、比例追逐及阻擋、點選矩形與關聯座標偏移；真實模板測欄28非空與空欄保留／0覆寫，既有pick_at改用真實角色圖測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查通過，原有5個未使用函數警告。
  ~~下一步：0x486270（CCharactor 尚未完成，續補移動狀態／模板、主命令、狀態與更新；不可跳CNpc）起。~~


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，修復上輪未提交殘影並續補修正表／生命通知／狀態表與目標反應；整類別仍未完成，未進CNpc**。本輪主範圍新增done 6（0x48ec60、0x48e8f0、0x48fae0、0x48eda0、0x48f560、0x48e120），skip 0；合計 **done 61、skip 33、todo 24**。另完成／核對殘影基礎8入口（0x4a1780、0x4a1b40、0x4a1c90、0x4a2280、0x575ae0、0x575bd0、0x575cb0、0x576b90），更新既有附加精靈入口0x48ebd0。
  - `main.c`的actor_modifier_set使用20項共用值／期限表，取代原speed_override、scale_target、display_override及殘影期限各自存值；接狀態表與固定拍。保留期限拒絕縮短、永久-1強制覆寫、輸入0即使遭拒仍標重算、每10拍減10且恰0不解除；第1更新速度、第3擲狀態17、第5通知狀態100並選動畫、第6建立／釋放殘影。舊擊退／硬直的knock/stun仍待完整動態狀態統一，不能標完整角色更新done。
  - actor_health_percent／actor_parameters／actor_health_sync接人物生命百分比與四個行為參數；保留<20／<50／其他為第3修正3／1／0，未夾限百分比，低血值或期限改變才擲潰散、回血不自行解除狀態17。原RoleMdl讀表入口核對第26欄四值中第3值為+0x10c（res.c新增RoleDef.panic），不是取第一值；狀態17／100直接通知byte及效果表，不建立技能Buff。
  - actor_effect依RoleEffectMdl原始20欄的65535空值套旗標、速度、三附加精靈、光環資料、殘影模式、第10～13修正及比例、聲音；指定0仍啟用對應旗標。res.c／res.h保存raw欄位。actor_effect_sprite及status_fx_draw／actor_effects_tick用三份Spr獨立播放，刪除舊逐Buff用全域拍數重畫；Spr型別由unit.c具名供Actor引用，換場景清掉播放器及殘影。
  - actor_target_set接反擊、索敵、點擊與施法，near.c的foe_grp補102／0x4000排除；保留先寫目標、非NPC來源在NPC索敵圓距離內（整數sqrt、含等號）先入戰再互設目標。actor_notice_near接每拍格子名單，保留嚴格索敵方框及高層+8，不改成圓距離；候選技能重入查詢時保存外層名單游標。戰鬥狀態0仍沿actor_enter_fight橋接，完整主命令及狀態byte分派未標done。
  - 殘影修正兩處：各部件幀索引越界回第0幀，不是取餘數；首槽沒有殘影不阻擋其他部件採樣／繪製。保留重開不改參數、重複30筆及非重複每5拍採樣／保留步數筆。shortcut：殘影混色沿SDL alpha；頭上效果額外高度待模型高度；完整狀態重算的末次呼叫反編譯傳-1，目前最後修改狀態優先為明示shortcut，原始組語未核對所以0x48b2f0仍todo。光環資料寫入已翻，每100拍觸發仍屬完整每拍todo；其餘24個todo多為本類別與引擎基礎，不能稱為等外部系統。無新增DLC分類。
  - skilltest新增修正期限邊界／拒絕值0、低血重入亂數與不占Buff槽、通知參數順序、殘影容量／採樣／跨部件幀數、三槽狀態表指定0／解除／光環查鏈、目標圓距離與靠近嚴格方框／高層／敵對登記排除。`cd engine && make && make skilltest`最後一行`skilltest ok`；-DSTORY語法檢查通過；建置6個未使用函數警告。fields／roles已更新，FUNCS.tsv已重產；未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘24個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起。~~


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，核對組語並修正狀態重算；本輪仍未完成整類別，未達整類別完成要求，未進CNpc**。新增done 1（0x48b2f0）、skip 0、base 0；主範圍合計 **done 62、skip 33、todo 23**。
  - `main.c`的actor_effects_recompute完整接固定拍的modifiers_dirty消費端，並保留既有Buff到期／互斥移除的重算呼叫。刪除「最後狀態優先」shortcut，將last_status改為原版byte；buff_add改走共用byte通知，立即套此次狀態，期限仍由Buff名單維護，不能靠重算補首次套表。共用通知仍未涵蓋特殊狀態switch，不宣稱完整狀態主入口done。
  - 保留原版怪現象：0x48b300～0x48b315比較的是state[id]值與last_status ID，不是迴圈ID；127向0重套且不預先清舊效果。0x48b5c9先將ebx減至-1，0x48b5e8確實push ebx，末次套表傳-1（查低byte255，現有表無此列），不替換為last_status。末尾無條件清dirty，包括重套第4修正0時再次設起的dirty；光環重套維持既有範圍及技能游標。
  - `skilltest.c`新增逆序優先、最後狀態不重套、last_status=1略過值1、ID相同但值2仍套、值2略過、Buff名單不能替代byte、光環範圍／游標保留、套0後清dirty、缺列不套、技能狀態加入／到期的byte與效果同步。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查通過；原有6個未使用函數警告。
  - fields／roles已更正，FUNCS.tsv已重產。無新增shortcut／DLC分類。其餘23個todo仍含同類別動態移動、模板、動畫、繪圖、主命令與狀態switch及引擎基礎，不能列為等外部系統；起點不前移。未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘23個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪只修正零速退出漏掉移動殘影收尾，未完成整類別，未進CNpc**。新增done 0、skip 0、base 0；主範圍仍為done 62、skip 33、todo 23。
  - 核對0x486270／0x488280及底層0x4942d0／0x494950／0x496550／0x496ae0：actor_move_speed在速度小於1且舊動態狀態11時，先呼叫actor_trail_exit才改成靜止，修正永久殘影不收尾。未把局部修復標成完整動態入口done；既有動態移動shortcut仍保留。
  - 保留第6修正永久期限-1改為20、期限21以上不縮短、剩0仍有效至下個10拍的怪現象；skilltest新增零速退出、到期邊界、靜止不刷新、模式1不改期限測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`，仍有6個既有未使用函數警告。
  - fields修正+0x73實際對應Actor.dynamic_state，roles補零速退出路徑，FUNCS.tsv已重產。無新增DLC分類。23個todo仍含同類別及引擎基礎，不能列為等外部系統。未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘23個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補特殊狀態共用倒數與解除通知，仍未完成整類別，未進CNpc**。主範圍新增done 0、skip 0、base 0，仍為 **done 62、skip 33、todo 23**；另外補齊依賴的動態物件狀態getter 0x494f50（done 1）。
  - `main.c`的has_status改為只讀原版角色byte並保留值2等非布林值，參數截低byte；刪除Buff名單回退，避免倒數已解除的狀態被仍有效的Buff讀成啟用。shortcut：128..255在原版越過128-byte狀態陣列讀其他物件欄位，目前防越界回0；遊戲資料實際使用此ID時再核對。沒有把不完整主入口的位址寫進程式註解。
  - actor_status_notify接狀態8的隱身旗標、13的反擊技能0/-1、14保存／還原AI組及全域99..1循環組、105／111／114／115共用倒數，以及121令牌旗標；已從Poq.exe資料節確認循環組初值99。+0x579只保存為status_aux並照原版清0，用途未知，不能猜成casting而清除施法。
  - actor_status_countdown接每10拍共用修正相位，無條件減1；優先105→12→111→115→114，每次最多清一個，恰0仍有效。105設9、111／114設8、115設5；重送相同byte不刷新，共存狀態會覆寫同一倒數。114剩5直接寫第10修正2/-1並換身上精靈；解除清修正及精靈。保留狀態12遮蔽較低優先序的行為，但12的陷阱／周遭偵測仍未翻，故0x488090仍todo。
  - actor_second、buff_add互斥替換及buff_drop全部走共用通知解除，取代直接清byte／套表，避免漏掉旗標、換組及附加精靈收尾；新邏輯已接固定拍與技能實際路徑。skilltest新增byte唯一來源／截低byte／值2、零邊界、優先序、共用倒數覆寫、不刷新、每10拍相位、114換色／附加精靈及解除、蠱惑不誤清施法、狀態8／121到期／互斥／明確移除測試。
  - 狀態主分派仍缺戰鬥0、周遭命令15、挨打101、升空110、光源及技能選取／通知等副作用；完整動態移動、模板、動畫、命令與更新等23個todo仍為同類別及引擎基礎，不是等外部系統。未新增DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查通過；仍有6個既有未使用函數警告。fields／roles已更新，FUNCS.tsv已重產；未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘23個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補狀態12的陷阱與周遭隱身偵測，仍未完成整類別，未進CNpc**。主範圍新增done 0、skip 0、base 0，仍為done 62、skip 33、todo 23；補齊依賴的陷阱主人組別查詢與揭露0x49be00／0x49c3a0（done 2）。
  - `main.c`的actor_status_countdown接狀態12：每次先以格子中心周圍5圈找不同AI組尚未被發現的陷阱並揭露，再以世界半徑320查周遭；不同組角色有旗標0x20000即解除偵測者自身狀態8並返回，不要求敵對，也不清對方。12不因倒數負數解除，仍遮蔽111／115／114；105仍優先於12。固定拍既有呼叫直接消費新路徑。
  - `near.c`的near_trap刪除「活著當有效」及owner->side比較；trap_owner_group每次查詢從場景Actor名單核對主人，主人已死但仍存在仍更新AI組，無效時沿用最後組。trap_reveal只清尚未發現旗標，不殺死陷阱、不取消觸發。由陷阱繪製與觸發本體確認+0x9f是尚未發現旗標，並非生命或觸發開關，fields已更正。
  - shortcut：陷阱沿用mon_make建立並預設未被發現，完整CTrapBase建立入口接入時核對初值；主人句柄仍沿用Actor指標／場景名單，同場景重用槽時升級世代識別。格子多物件仍沿用既有陣列查找限制，未宣稱完整陷阱系統。狀態12缺的部分是CGameApp介面自動暫停選項、1501/16/1000通知及場景暫停；沒有猜選項或加任意暫停鍵，0x488090仍todo。其餘同類別移動、模板、動畫、命令、狀態主分派與更新依然待補，不是等外部系統。
  - `skilltest.c`新增不同組同side、已死主人／無效主人快取、發現後不重複命中且不殺陷阱、半徑5邊界、隱身旗標／同組排除、105遮蔽12及12遮蔽低優先倒數測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查通過；6個既有未使用函數警告。roles／fields已更新，FUNCS.tsv已重產；無新增DLC分類，未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘23個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪完成角色動態退出並接旋飛升降；整類別仍未完成，未進CNpc**。主範圍新增done 1（0x488280）、skip 0、base 0，現在done 63、skip 33、todo 22；另完成底層退出0x494950與格子層getter 0x4c7d60，高度setter 0x4961e0由實作承接原有done。
  - `main.c`的actor_trail_exit先照原版處理模式2殘影，期限<21才改1／20並重建，再交actor_dynamic_exit；後者標活動、11／31清反播、22設下降-5／上限0並落入21清空中旗標，有效格且物件層>10才恢復格子層。保留退出不改行為狀態、不清目前高度及格子層10不轉1的行為。現有移動切換及零速退出皆消費此入口；dynamic_active／reverse_playback的完整動態更新與本體反播消費仍待接。
  - actor_status_notify新增110：啟用取消現有MethodMain與待執行命令、設上升2／上限100；解除設下降-10／上限0及第5修正1／30。actor_height_set不清目前高度，招式離地觸發共用它。actor_height_tick非跳躍分支移到每個有效角色固定拍，刪除action_tick內的高度更新與動作結束清零，動作播完仍可繼續下降。MethodMain結束事件沿用既有unit_die並傳目前邏輯拍；其既有連鎖取消限制仍屬招式基礎待補。
  - 修正第5修正／擊退位移混用：旋飛解除只設動畫修正，不能讓角色朝未設定的knock_x/y滑向原點；暫用擊退位移以knock_motion保存，抵達時恢復第5修正值。shortcut：完整狀態31逐格位移、狀態22高度曲線及靜止下降往占格中心插值仍待動態位置基礎；未標完整高度入口done。移除舊高度、擊退與unit_bounce shortcut裡未完成入口的位址，修正0x495010／0x493cb0／0x4942d0／0x496550的誤標done（不影響主範圍22個todo）。未新增DLC分類。
  - `skilltest.c`新增動態退出21／22／31／11、格子層9／10／12／16及越界、移動實際呼叫、旋飛取消MethodMain／待命令、升降與上限／落地、動作結束不清高度、修正不觸發未設定擊退等測試；修正舊mk測試fixture的無效Role指標，旋飛解除選動畫時使用有效空模型。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`，-DSTORY語法檢查通過，仍有6個既有未使用函數警告。fields／roles已更新，FUNCS.tsv已重產；未commit或push。
  - 尚未完成的22個主範圍todo仍包括動態移動／模板、外觀、動畫／繪製、命令、狀態及更新；這些是同類別或引擎基礎，不能稱為等待別的遊戲系統。只有特殊倒數入口的介面自動暫停等通知仍等不同類別CGameApp；下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘22個todo；續補完整動態移動／模板、動畫、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪完成本體動畫逐拍入口；整類別仍未完成，未進CNpc**。主範圍新增done 1（0x487f30）、skip 0、base 0，現在done 64、skip 33、todo 21；另完成反向播放器0x49e5f0，序列指標getter 0x496de0歸base_map（base 1）。
  - `main.c`的actor_animation_tick接固定拍與action_tick：0x200凍結本體，但仍解除101及播放三個附加精靈；第5修正值1時連做三次停末幀step，其他情況按reverse_playback正播或反播。actor_body_step沿迴圈首／末幀跳轉，幀延遲加個別調整後最少1拍；完成判斷是末幀最後一拍。保留反播到末幀也可能觸發完成判斷，以及附加精靈完成後只有非0序列才回0的原版行為。
  - 刪掉actor_tick／pursue的動畫拍數推進、actor_frame按累積拍數重算幀及action_tick私有逐幀程式。繪圖與MethodMain讀同一播放器；遊戲每拍只推進一次，建立角色的既有隨機相位在首次選序列時換成播放器狀態。單次動作先讓MethodMain消費末幀才收招，避免末幀只有1拍時漏掉觸發。
  - shortcut：全域播放器延遲調整仍沿用0，加入該選項時統一接本體／Spr；死亡仍沿用現有Actor屍體計時並停末幀，接原版死亡物件移交時移除橋接。方向漸轉、外觀覆寫及完整狀態／命令／動態位置更新仍待同類別與引擎基礎，不是等待外部系統。只有特殊倒數的介面自動暫停通知仍等不同類別CGameApp；無新增DLC分類。
  - `skilltest.c`新增正反播、迴圈首尾、擊退三次推進及停末幀、空中凍結本體但仍解除101／播放附加精靈、序列切換、隨機相位、個別走路延遲不改共用資源、單次動作及死亡橋接、1拍末幀MethodMain觸發測試；既有測試的手動done旗標改以真實播放器末幀狀態設置。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查通過，仍有6個既有未使用函數警告。roles／fields／base_map已更新，FUNCS.tsv已重產；未commit或push。
  ~~下一步：0x486270（CCharactor 尚餘21個todo；續補完整動態移動／模板、外觀／繪製、命令主分派與128狀態更新，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪接入角色更新的方向漸轉分支；整類別仍未完成，未進CNpc**。本輪新增done 0、skip 0、base 0；主範圍仍done 64、skip 33、todo 21，不把局部實作標為完整入口完成。
  - `main.c`新增actor_direction_tick，actor_animation_tick每拍消費；actor_requested_seq提供目標，actor_seq改讀目前顯示序列，取代原先面向改變便立即換本體序列的簡化。大小<6每4拍、其他每15拍轉一向；目標換組或幀數不同先同步序列組，方向切換保留幀索引、超界歸0。保留差四方向沿原版分支反向轉、空中旗標且動態狀態非1時不轉向的行為。
  - shortcut：物件累積更新拍暫以direction_clock承接，完整物件生命週期接入時統一；完整活動／高度更新門檻、0x400旋轉序列、特殊共用倒數與光環仍待同類別及引擎基礎，不能稱為等待外部系統。角色更新入口仍todo，註解不寫該未完成入口位址。外部依賴仍只有既有CGameApp介面暫停通知；無新增DLC分類。
  - `skilltest.c`新增4／15拍、差四方向、跨0方向、保留幀／超界歸0、換組重設、空中靜止例外測試；兩序列動畫fixture指定轉向更新拍，避免把方向序列變更誤當即時換組。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查、git diff --check通過。fields／roles已更新，FUNCS.tsv已重產；未commit或push。
  ~~下一步：0x486270（CCharactor 仍餘21個todo；完整動態移動／模板、外觀／繪製、命令主分派與128狀態更新未完成，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補角色更新的旋轉與冷卻分支；整類別未完成，未進CNpc**。新增done 0、skip 0、base 0；主範圍仍done 64、skip 33、todo 21，未以局部實作標記完整更新入口。
  - `main.c`的actor_direction_tick接旗標0x400：動畫方向每拍減1、負數回7，以模板+0x4c加方向重選序列，優先於空中靜止例外且不受4／15拍節流；actor_tick不再把旋轉累積方向改回物件面向。更正上輪shortcut提及的「死亡序列起點」：核對模板讀表器後，+0x4c是第7欄休息序列第一值，死亡是+a4。保留換旋轉序列重設幀、解除後按普通漸轉回目標的行為。
  - 新增actor_cooldown_tick，取代固定拍無條件扣冷卻：正數且無存活MethodMain才減1，零／負數保留。shortcut：完整活動／高度門檻仍未接入；累積拍仍沿既有direction_clock橋接。完整動態移動／模板、外觀／繪製、主命令、狀態及100拍光環都是同類別或引擎基礎待補，不能稱為等待別的系統；外部依賴仍只有既有CGameApp介面暫停通知。無新增DLC分類。
  - `skilltest.c`新增非零休息起點與不同死亡起點、完整旋轉一圈、空中／狀態22旋轉、逐拍重設幀、移動入口保留旋轉方向、解除旋轉漸轉，以及MethodMain存在／死亡與冷卻零／負數測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；fields／roles更新、FUNCS.tsv重產。未commit或push。
  ~~下一步：0x486270（CCharactor 仍餘21個todo；完整動態移動／模板、外觀／繪製、命令主分派與128狀態更新未完成，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪完成場景綁定／位置修正與死亡占位移除，並補動態占位基礎；整類別仍未完成，未進CNpc**。新增done 5（主範圍2、基礎3）、skip 0、base 0；主範圍done 66、skip 33、todo 19。
  - `near.c`的actor_bind_scene對應0x488a70：綁Scene、格子／世界位置置中、種類非60時可消費共用重判層旗標、十圈邊掃找空格、保存外部修正或目前格子；actor_destroy對應0x490220，由actor_kill呼叫，先移占位再設object_dead，保留獨立死亡動畫。基礎actor_unoccupy／actor_place／actor_position對應0x495340／0x495420／0x497c90；Cell.actor保存單格物件識別，Actor另存初始化格子與所屬場景，與插值世界座標分開。
  - 接入場景進入、ONT NPC／劇情及一般怪物出生、移動跨格、死亡；刪除兩段出生附近7×7隨機試20次的簡化。換召喚獸／劇情移出場景先移占位；讀檔、測試跳點及既有原地復活補同步。陷阱仍走自己的格子層，不套角色占位／重定位。
  - 照原版保留：大小1拒絕自己已占的目的格，大小>1可覆蓋自身；偶數大小向右下多一格；大物件不走八圈找空格且只對格子層1..9檢通行；先驗證全部占地才清舊格，移除僅清自身識別。綁場景找十圈、單格重定位找八圈；層10不降成1，種類60不消費重判層旗標；外部修正只寫快取，不瞬移至修正座標。
  - shortcut：未綁場景的舊建立資料／fixture仍以世界座標查詢，完整物件建立接齊時移除fallback；存檔／測試入口直接給世界座標，先同步占位，完整位置命令接入後改走該入口。完整動態尋路／狀態轉接、模板、外觀、命令、繪製和角色更新仍是同類別／引擎基礎todo，不能稱為等待其他系統；只有既有CGameApp介面暫停通知等外部依賴。重判層與外部修正旗標的完整上游仍待初始化／位置命令接入；無新增DLC分類。
  - `skilltest.c`新增占地奇偶／邊界、同格／碰撞／失敗保留、物件識別相符才清除、跨場景、重判層／種類60、修正快取、八／十圈差異、實際移動跨格／死亡及讀檔位置橋接測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查、AddressSanitizer（停用既有全域快取的leak檢查）與git diff --check（排除自動產生FUNCS.tsv的空尾欄tab）通過。fields／roles更新、FUNCS.tsv重產；未commit或push。
  ~~下一步：0x486270（CCharactor 仍餘19個todo；完整動態移動／模板、外觀／繪製、命令主分派與128狀態更新未完成，完成整類別才進CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪續補模板重套；整類別未完成，未進CNpc**。新增done 0、skip 0、base 0；主範圍仍done 66、skip 33、todo 19。
  - `main.c`的actor_setup補模板前先啟用狀態103，再直接覆寫模板組別、清旗標、按新大小重定位已綁場景占位；反擊技能與四候選同樣只以65535表示保留舊值，0／負值照寫。`res.c`讀表補欄30組別及欄35反擊技能；怪物出生AI組覆寫移至模板預設組之後。新角色層1只在首次建立設定，重套模板保留現有層。
  - 照原版保留：模板組別直接賦值，不經actor_group清目標；單格重套占位拒絕自身舊格，可找附近格，大物件沿用完整占地檢查。+0x5c是模型類型，並非+0x67物件種類，尚未接入，不將其錯寫成object_kind。
  - shortcut：模板光源仍由每拍light_tick延後建立，模型類型與探索刷新未齊，故模板入口不標done；動態移動、外觀／繪製、主命令、128狀態更新仍是同類別／引擎基礎待補，不能稱為等待外部系統。特殊倒數的CGameApp介面暫停通知仍為既有外部依賴；無新增DLC分類。
  - `skilltest.c`補真實actor_setup路徑：狀態103、模板組別／清旗標、重套保留層、反擊65535／0／-1及重套單格占位重定位。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；fields／roles更新、FUNCS.tsv重產。未commit或push。
  ~~下一步：0x486270（CCharactor 仍餘19個todo；完整動態移動／模板、外觀／繪製、命令主分派與128狀態更新未完成，完成整類別才進CNpc）起~~。



- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪完成模板入口並接入同步光源／格子探索；整類別仍未完成，未進CNpc**。新增done 1（0x4862e0）、skip 0、base 0；主範圍done 67、skip 33、todo 18。
  - `main.c`的actor_setup對應0x4862e0，補模型類型低16位、同步actor_light_init與actor_explore。核對角色模板讀表後，RoleMdl+0x22來自建構的ffff，文字欄4是字串，沒有拿欄4或物件種類代替；`res.c`載入普通RoleMdl保存這個初值。
  - `light.c`的actor_light_init對應0x486480：不用等紙娃娃，不以死亡動畫排除；半徑size×240+400，閃爍另一色e1e1e1。同物件已有光回0，重套模板不換原光。刪除light_tick每拍掃角色補建的簡化，避免淡出刪光後自動重建；隱身狀態8解除亦直接走原版建光入口。
  - `main.c`的actor_explore對應0x489380：同操控角色組別、場景就緒才做，模板套用／場景就緒／角色累積拍餘1時接入。以初始化格子為掃描中心、世界64分格為距離原點；層14半徑加8、16加12、其他>10加4。內圈10，中圈只有種類30..100的占格角色提高到10，其他／外圈1；只增不減，200保留。保留右下界不含端點、內中圈嚴格小於、最外圈距離等於也接受的怪現象。原來只在「尚未做」註解中出現的位址改為真正的實作註解；同組查詢轉接0x443760對應scene_client_group。
  - shortcut：SDL全畫面重繪取代背景髒矩形；CMapDialog表面刷新通知仍等不同類別小地圖系統，未標該通知入口完成。scene_load既有全圖探索初值2及探索衰減仍待CScene／CMapDialog接齊，本輪只補角色探索提升，未宣稱完整迷霧。錄影回放模式2未啟用，之後接回放時補禁止建光條件；未綁場景先用零投影，首次光源更新同步位置。無新增DLC分類。
  - `skilltest.c`走真實模板入口測光源即時建立、重套不替換、淡出後不自動重建、ffff模型類型、探索圈邊界／種類篩選／高層／組別／場景就緒／200不降級。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查及AddressSanitizer（停用既有全域快取leak檢查）通過。修正fields末兩列誤寫字面\t導致fn.py --brief失敗；fields／roles更新，FUNCS.tsv重產。未commit或push。
  - 其餘18個主範圍todo仍含同類別動態移動／外觀／繪製／命令／狀態主入口，不能當成等外部系統；尚未達成整類別完成，不前移到CNpc。
  ~~下一步：0x486270（CCharactor 仍餘18個todo；完整動態移動、外觀／繪製、命令主分派與128狀態更新未完成，完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，接續並核對未提交換裝改動，修正監獄開場誤索敵；整類別仍未完成，未進CNpc**。相對上一段新增done 2（0x48d810、0x48ba50）、主範圍skip 0、base 0；主範圍done 69、skip 33、todo 16。
  - 保留並核對上輪`main.c`的actor_look_set、actor_body_sequence／apply、不可變RoleVariant快取及`skilltest.c`換裝測試。外觀只換指定角色使用的組合，不能原地修改共用模板Role；保留原版最後套目標導致舊序列／幀再次重設的怪現象。動態休息分支已接換裝，完整動態主分派仍未完成，不標該主入口done。
  - **xy_2prison開場獄卒誤入戰鬥是移植錯誤，並非原版開場死亡劇情**：實際場景四名守衛使用模型90，模板組別2；原版守衛腳本覆寫組別100、Invincible=1，主角組10001。spawn_npcs原先只綁劇情handle，留下模型組2和零HP；靠近通知會使守衛與主角互選目標。新增actor_person_init，ONT及入口生成角色建立時讀劇情組別、生命／攻防及無敵等現值；scene_enter在第一拍前恢復主角的劇情組別，避免等資源載完才覆寫。沒有改掉原版0..99組的敵對規則。
  - `actor_status_notify`補狀態0：非NPC組清排隊命令／動態休息、更新速度與動畫，退戰清目標及返回旗標、入戰觸發候選；actor_enter_fight及退戰倒數接入，重複入戰不重做通知。`near.c`的actor_near_next完成同組援敵的狀態byte0、SInit格距離及種類30..100篩選；保留嚴格距離、目標getter及轉交時不重套候選旗標。無劇情物件補高半區32位唯一識別，跨場景不重用，重套模板保留；劇情物件共用既有handle。
  - shortcut：舊AI的退戰時限仍沿用FIGHT_TICKS；完整命令11的選招門檻及其他命令／動態入口未完成。未綁場景fixture的初始化格沿初始世界格讀取；完整遊戲角色使用grid_x/y。SDL每畫面重繪及省略單機同步封包沿用原有限制；沒有新增DLC分類。
  - `skilltest.c`補模型組2會互相入戰的重現、套用劇情組100後600次靠近不入戰、初始HP／Invincible、戰鬥byte0通知／重入／NPC組差異、SInit距離邊界、種類29/100/101及非劇情識別／模板重套測試。原版監獄房間與四守衛腳本另以WebAssembly執行，確認主角10001、四守衛100／無敵／存活（人物表hook固定HP供腳本驗證）；實際模型90組2另讀場景資源核對。
  - `cd engine && make && make skilltest`最後一行`skilltest ok`；-DSTORY語法檢查及AddressSanitizer通過（停用既有全域資源快取leak檢查）。fields／roles更新，FUNCS.tsv重產；未commit或push。
  - 其餘16個todo仍包含同類別動態移動、初始外觀／繪製、角色移除、技能與命令主分派、狀態及更新，並非全部能等待外部系統；只有既有CGameApp介面暫停通知等屬不同類別依賴。**本輪尚未達成整個CCharactor完成的要求，不能前移至CNpc。**
  ~~下一步：0x486270（CCharactor 尚餘16個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，完成外觀初始化並替換出生動畫簡化；整類別仍未完成，未進CNpc**。主範圍新增done 1（0x486520）、skip 0；主範圍done 70、skip 33、todo 15。另完成動態基底面向setter 0x4963e0，字串切分工具0x49f1b0記base 1。
  - `main.c`的actor_appearance_init接進actor_setup，NPC／玩家／生成角色共用；刪掉出生rand()%200的動畫相位與actor_body_select依拍數推算初幀。照原版先載紙娃娃序列0／幀0矩形，保存高度+10，再隨機方向、動態休息、靜態SInit初始態／方向、速度與目標本體；序列長度>2才隨機選幀，兩幀不消費第二次亂數。BeastieMdl單Anim不誤走角色紙娃娃入口；生成type13補對應logic=1。
  - 外觀覆寫以星號切非空token，首項是PLI動作方式，其他項重串款式；載入失敗回模板，使用既有不可變RoleVariant，不修改共用RoleDef。actor_direction_set保存原版有號面向，目標序列按向零除8的組別換方向。靜態SInit的初始序列不再被第一拍簡化AI重選成休息序列。
  - `res.c`補ONT兩個同名「初始态」ushort及RoleMdl初始序列表；照原版欄12五值後再由欄13九值從索引4覆寫，空項65535；保留欄12第五項被覆寫的怪現象。實際監獄模型90的16*24*40及缺項已加入回歸測試。新偏移與用途寫入fields／roles，FUNCS.tsv重產；沒有新增DLC分類。
  - shortcut：沿用96-byte款式上限，超長資料時升級動態字串；初始態超過13項時拒絕原版越界讀模板其他欄位，遇到合法新索引再擴表。網頁圖像下載未完成時暫用可用部件的頭頂高度，每個固定拍重試序列0／幀0，下載完成後固定；SDL全畫面重繪沿用原有限制。單機載完整圖資，不實作回放／伺服端省略圖資模式。
  - `skilltest.c`新增初始方向／序列、第一拍保留、隨機初幀／兩幀不擲、頭部矩形、覆寫非空token／回退、不可變快取與負面向測試；更新舊隨機相位測試。`cd engine && make && make skilltest 2>&1 | tail -3`最後一行`skilltest ok`；-DSTORY語法檢查與AddressSanitizer（停用既有全域快取leak檢查）通過。未commit或push。
  - 尚餘15個todo，含動態移動、移除／繪製、技能／命令主分派、狀態與更新；它們仍主要是同類別與引擎基礎，不能歸為等待外部系統。本輪未達成整類別完成；下一步不前移。既有介面自動暫停通知才等不同類別CGameApp。
  ~~下一步：0x486270（CCharactor 尚餘15個todo；完成整類別才進CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，新增行為重設0x48afc0；整類別仍未完成，未進CNpc**。主範圍新增done 1、skip 0；主範圍done 71、skip 33、todo 14。另完成動態基底重設0x495910。
  - `main.c`的actor_behavior_reset接新建NPC／怪物及enter_scene換場景；刪掉mon_make獨立rand()%100，由actor_dynamic_reset統一更新拍初相位。重設清建築互動／目標、排隊代號、業務輔助與位置快取，按舊記住命令1/2決定50拍延遲，再退戰、初始化光源、動態休息、高度速度-20／上限0，最後解除場景、死亡旗標0及層1。
  - 保留怪現象：排隊及記住命令的三參數不清；連續重設會把50延遲改回0；不直接清離地高度。清MethodMain僅解除綁定，`unit.c`的main_of不再取回舊物件，仍保留owner且不呼叫unit_die／結束觸發。動態基底重設不改世界座標、動態命令三參數及高度。
  - 已核對移動入口與動態進入的路徑／跳躍依賴，未把現有平面移動冒充完整移植。shortcut沿用既有動態休息分支；其他動態命令、同類別狀態及更新仍未完成，不能歸為等待外部系統。令牌名單仍沿用單機Actor身分，沒有建立伺服端配對容器。沒有新增DLC分類。
  - fields／roles已更新，修正進入拍數／尋路結果原先同名的欄位標籤，FUNCS.tsv重產。skilltest新增命令參數保留、延遲1/2及重複重設、MethodMain不結束、單次亂數消耗、動態／高度／場景清理測試；建置與skilltest通過，最後一行`skilltest ok`。未commit／push。
  - 尚餘14個todo：動態移動、移除／繪製、技能／命令主分派、文字、狀態及更新；主要仍是同類別／引擎基礎，尚未達成本輪整類別完成要求。介面自動暫停通知仍等不同類別CGameApp；下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘14個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，新增留血入口0x48f0a0；整類別仍未完成，未進CNpc**。主範圍新增done 1、skip 0；主範圍done 72、skip 33、todo 13。
  - `main.c`的actor_blood接walk_fx每拍與strike受擊；按第3修正讀表，程序標識50完全不擲亂數，受擊機率分母3／2／1／1，分母1仍擲一次。建立單位0x17時使用空來源，不繼承攻擊者、目標、招式、技能，將角色層號保存到`unit.c`的MUnit.layer。攻擊方式4／7先處理，其餘0／1／2／3／8於受擊反應後處理。
  - 原版平常表位於BSS，映像預設全零；shortcut：尚未核對其執行期設定入口，現階段保留零值而不自定流血頻率；接到原版設定入口時更新actor_blood_period。單位繪圖仍沿用既有viewport_units分類，完整層分流屬同類別繪圖待辦。非法修正索引不重現越界讀取，單位表缺失或容量耗盡不重現空指標寫入。
  - fields更新模板程序50例外與招式單位層號，roles記用途及限制，FUNCS.tsv重產。skilltest新增原版機率、零表／模板50不擲、失敗擲骰只耗一次、建構亂數流、空來源、位置／層號與容量耗盡測試；建置與skilltest通過，最後一行`skilltest ok`。未新增DLC分類，未commit／push。
  - 尚餘13個todo：移動、移除／繪圖、技能／命令主分派、頭頂文字、狀態與更新；主要仍是同類別或引擎基礎，不是等待外部系統。特殊倒數的介面自動暫停通知仍等不同類別CGameApp。本輪未達成整類別完成要求，下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，補狀態101挨打分支；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0；仍為done 72、skip 33、todo 13，不把局部分支標成完整入口。
  - `main.c`的actor_status_notify接101：非零先中止目前MethodMain，冷卻<1才補10～19拍；音效不是65535才擲一次rand()%3，結果1播放。即使缺挨打序列仍先執行上述副作用，再恢復舊狀態byte並直接返回，不設dirty、不套效果表。相同byte不重做副作用；解除不擲亂數。
  - hit_react改走101與既有動畫收尾，取代直接播放ACT_HIT且不取消招式的簡化分支，刪strike每次命中無條件音效。保留原命令12的狀態1、狀態2／10／101及擊退值1門檻。shortcut：完整命令12的共同可用性／技能選取檢查仍待同類別命令主分派接齊，未宣稱完整移植命令入口，也未新增其位址註解。
  - fields／roles補音效偏移及副作用順序；修正roles先前「文字槽／繪製已具備」誤述（舊實作已撤回，仍待實作）。未新增skip、base或DLC分類。其餘todo仍主要是同類別移動、移除／繪圖、命令、文字與更新，不是等待外部系統；只有既有介面自動暫停通知等不同類別CGameApp。
  - skilltest新增MethodMain中止、冷卻／音效亂數消耗、缺動畫恢復非零舊byte、同值不重做、解除及受擊／動畫收尾整合測試；建置與skilltest通過，最後一行`skilltest ok`。FUNCS.tsv重產；未commit／push。本輪未達成整類別完成要求。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，補動態基礎的高度／位置插值；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13；依賴入口新增done 3（位置插值初始化／逐拍、完整高度更新）、skip 1（固定回1的基底活動getter），base新增比例取整與IsMinRange映射。
  - `main.c`的actor_interpolate／actor_interpolation_tick接actor_height_tick靜止下降回占格中心，取代只降高度的簡化程式。整數歐氏距離先除float速度，剩餘拍數向零截斷，增量卻除以未截斷拍數；保留最後未必到格中心、沒有末拍補齊的原版行為，觸地才強制回中心。占格與世界座標分開，不以插值移動占位。
  - 高度基底每拍只增加一次direction_clock，方向漸轉不再自行增加；A>8且不是走路時，X或Y離上次落塵至少48世界單位建立單位16，空來源不繼承攻擊者／技能。跳躍上升按剩餘拍扣zmax並把扣量加至z，下降用半比例遞減；零垂直速度不清跳躍高度。基底活動門檻已核對為固定回1，不猜生命／角色活動旗標。
  - 機碼核對反編譯漏掉的sqrt與比例函數EAX回傳：比例先float運算，半數向正方向取整，再從傳址值扣掉；上升同時消費被改的zmax與EAX。shortcut：沒有有效占格的舊初始化角色暫以目前格子中心承接，完整建立流程統一登記占格後移除fallback；回放模式10方向追趕不適用單機，不實作。跳躍命令進入／中點下降切換仍屬完整動態狀態主分派待辦，沒有把局部入口標完成。
  - fields補落塵座標、跳躍半程及四個float插值欄位，roles／base／skip與FUNCS.tsv更新。skilltest新增小數拍數末拍差、負座標截斷、零步長門檻、正負半數取整、跳躍上下曲線、靜止落地插值／觸地回中心、共用時鐘與48單位落塵門檻測試；`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`。未新增DLC分類，未commit／push。
  - 主範圍13個todo仍為移動、移除／繪圖、技能／命令主分派、文字、狀態及更新；主要依賴同類別／引擎基礎，不能稱為等待別的遊戲系統。特殊倒數介面暫停通知才等CGameApp。本輪未達成整類別完成要求，下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。



- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，補招式衝刺／跳躍所需動態基礎；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13；依賴入口新增done 4（落點計算、升層、跳躍半程切換、衝刺／跳躍逐拍插值及退出）。
  - `main.c`的actor_rush_destination對應落點入口，actor_raise_layer處理地面轉高層，actor_jump_phase處理起跳／下降，actor_rush_tick處理插值及強制靜止。owner_trig的type5／7改用MethodMain保存的目標點與招式最大距離，不再依目前foe位置猜落點；刪除dash_x／dash_y／dash旗標及action_tick中的平常速度簡化移動。所有角色固定拍在高度之後接actor_rush_update，不因招式動畫結束而遺失飛行中狀態；一般操控／actor_step不再覆寫21/22。
  - 保留原版二倍／四倍有效速度、先移占格再插值、同一占格不升層、占格失敗仍初始化插值、末拍不補齊世界座標且下拍才退出。地面落點被占、途中又沒有空格時，line_end可越過目的地繼續找空格；near.c修正沿線空格判斷直接讀占位，不再漏掉不在存活Actor名單內的占位。跳躍半程先切下降再做水平插值；觸發7即使命令遭拒絕仍寫指定高度。
  - shortcut：只接已有MethodMain的內部7／10命令分支，共用技能可用性與完整命令／動態主分派仍未齊備，不把角色或基底主入口標完成；無效占格不重現原版下降時的空指標讀取，保留層號待建立流程統一。MethodMain模式2/3自身切換仍未據此宣稱補齊。fields補三參數用途、半程與末拍時序；roles記已完成依賴及主入口剩餘分支，FUNCS.tsv重產。無新增skip／base／DLC分類。
  - skilltest新增二倍／四倍速度、占格／世界座標分離、同格失敗、占位回退與越過目的地、半程下降／末拍後退出、層映射與拒絕命令仍寫起跳高度；舊一般操控中斷跳躍的預期改為原版收尾流程。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`。未commit／push。
  - 尚餘13個主範圍todo：動態主入口11/31/32、角色移除／繪圖、技能與命令主分派、頭頂文字、狀態及完整更新；主要為同類別／引擎基礎，不能稱為等待外部系統。既有特殊倒數的介面自動暫停通知才等CGameApp。本輪未達成整類別完成要求，下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，修正每拍維護的空中門檻與共用時鐘；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13。
  - `main.c`新增並接入`actor_maintenance_tick`，替換固定拍迴圈中散落的修正倒數、縮放、效果重算與冷卻更新。照原版先扣出手冷卻；無0x400且有0x200且動態非1時略過其餘維護，0x400例外仍繼續。移除Actor.modifier_clock，修正每十拍倒數改讀基底已推進的direction_clock。
  - 保留原版空中不扣修正／特殊共用倒數、空中靜止仍維護、旋轉旗標越過空中略過條件、恢復後沿原共用拍數相位且不補扣。新測試涵蓋這些門檻、冷卻優先與恰好0仍有效的期限邊界。
  - shortcut：本輪只完成維護分支整合；角色啟用byte、完整動態11/31/32、動畫／高度先後順序及100拍光環命令接收仍未完成，不能把完整更新或動態進入標done。其餘角色移除／繪圖、技能／命令主分派、頭頂文字與狀態亦仍為同類別／引擎基礎todo，沒有改稱等待外部系統。特殊倒數的介面自動暫停通知才等不同類別CGameApp。
  - fields更新共用拍數與出手冷卻時序；roles更新完整更新的剩餘分支；FUNCS.tsv重產。沒有新增skip、base或已證明劇情未使用的功能分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；有6個既有unused-function警告。未commit／push。本輪未達成整類別完成要求，下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。


- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，替換戰鬥擊退的連續滑動；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13。
  - `main.c`新增並接入`actor_knock_command`／`actor_knock_tick`／`actor_knock_finish`：動態31保存方向、格數、速度，先走角色退出、有效速度與反向面向，第三參數非零才覆寫速度並啟用反播；每拍先移占格再插值，格數逐格扣，遇阻或格數耗盡回靜止。戰鬥`knock_back`改送此入口；所有角色固定拍推進31，玩家操控與NPC平時邏輯不再覆寫它；刪除knock_x／knock_y／knock_motion與react_tick中的滑動程式。
  - 保留原版占格先到下一格而世界位置尚在途中、插值最後一拍不補齊世界位置、下一拍只標活動、再下一拍才移下一格／退出；退出清反播但保留命令三參數。第5修正只控制動畫，不憑它產生擊退位移。
  - shortcut：完整命令5共用技能可用性仍未齊；動態31的counter非零返回目的地，暫交既有目的地行走橋接，仍等同類別動態11完整尋路；未將部分動態進入／設定／更新或31入口標done。新讀到+c3/+c7是尋路目的地，與+97/+9b位置快取分開，fields補列；roles更新31接線與剩餘分支，FUNCS.tsv重產。無新增skip／base／DLC分類。
  - skilltest新增逐格占位、反向面向／反播、速度覆寫、末拍與空拍時序、遇占位／不可走格退出、零格數、狀態拒絕、一般操控不中斷與返回目的地橋接；舊受擊測試改驗證動態31命令。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；6個既有unused-function警告。未commit／push。
  - 尚餘13個主範圍todo，主要為同類別動態11／32、角色移除／繪圖、技能／命令、頭頂文字、狀態與完整更新；只有特殊倒數的介面自動暫停通知等不同類別CGameApp。本輪未達成整類別完成要求，下一步不前移。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，修正固定拍動態／高度先後順序；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13。
  - 核對角色每拍動態入口、動態分派與角色更新的呼叫順序，`main.c`新增並接入`actor_motion_tick`，以動態21/22/31先推進、再跑高度及共用時鐘，取代固定拍原本高度先跑的順序。角色冷卻與修正維護仍接在後面。
  - 保留原版跳躍中點當拍先轉下降，再消費水平插值一拍，最後依剩餘拍數扣高度；水平末拍不退出22，下一拍退出22後立即以-5處理落地並吸回占格中心。skilltest新增整拍測試，驗證中點8降至4、末拍高度歸零、隔拍退出與共用時鐘每拍只加1。
  - shortcut：只整合已存在的動態21/22/31分支；活動byte、完整11/32、動畫虛擬更新及場景活動通知尚未接齊，不以本次局部修正標主入口done。13個主範圍todo仍主要屬同類別／引擎基礎；只有特殊倒數的介面自動暫停通知等不同類別CGameApp。本輪未達成整類別完成要求，下一步不前移。
  - fields補明半程欄位的更新時序，roles更新已接線及剩餘分支，FUNCS.tsv重產；無新增skip／base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`，6個既有unused-function警告。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，整合共用動態命令；整類別仍未完成，未進CNpc**。主範圍仍done 72、skip 33、todo 13；同類別基礎新增done 3：0x494f90、0x496550、0x496ae0，新增skip 0。
  - `main.c`新增`actor_dynamic_command`，取代衝刺／跳躍、擊退、換裝靜止、插值收尾及零速退出各自複製的狀態設定。狀態、三參數及戰鬥旗標快照共同去重；先角色退出再進入，進入失敗依快取退回1。`actor_dynamic_retry`保留NPC組10000..60000的15拍／其他50拍邊界及有號拍差；`actor_move_speed`零速退出改交共用命令。
  - 保留原版怪現象：靜止命令不寫新三參數，所以再次靜止仍可能觸發退出；普通靜止只執行21/22退出副作用而保留狀態，第一參數非零才強制退出。失敗回1不清失敗結果、命令參數及進入時刻。32進入先選八方向序列，外層分派隨後再依狀態重選走路序列。
  - shortcut：`actor_dynamic_enter`的11只交既有目的地行走，尚缺完整尋路進入／步進；32已照本體初始化三倍速、插值和面向，仍缺逐格越界／碰撞更新。不在這些局部入口寫完成位址。13個主範圍todo仍主要屬同類別／引擎基礎；只有既有特殊倒數介面暫停通知等不同類別CGameApp。本輪尚未完成整類別，下一步不前移。
  - skilltest新增命令去重／旗標重入、靜止三參數保留、21/22普通／強制退出、低速進入失敗、NPC組界線、15/50拍正反差與32位回繞、32初始化和序列重選測試。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；6個既有unused-function警告。fields／roles更新，FUNCS.tsv重產；無新增base／DLC分類，未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，補動態32更新消費端；整類別仍未完成，未進CNpc**。主範圍新增done 0、skip 0，仍done 72、skip 33、todo 13。
  - `main.c actor_charge_tick`接入`actor_motion_tick`，越過目前占格中心後先查側向地形，再用正式`actor_place`前移占格，最後推浮點插值；遇阻交共用靜止命令。`actor_tick`與`actor_step`加入32保護，避免一般行走覆寫狀態／三倍速度。
  - 保留原版怪現象：側向方向使用格座標到該格世界中心的混合單位；側向通行只查地形，角色碰撞交正式占位；末拍扣到0仍留32，下一拍退出，不吸回目的地，也不補末端世界座標。
  - shortcut：OmUtil `IsExceedDest`本體不在現有反編譯索引，以面向投影嚴格越過中心近似；取得本體後須核對斜向門檻／相等邊界。因此不在動態32更新入口寫完成位址。動態11、命令／技能、移除／繪圖、文字、狀態及更新仍屬同類別／引擎基礎待補；只有既有介面自動暫停通知等不同類別CGameApp。本輪未完成整類別，下一步不前移。
  - skilltest新增固定拍占格先後、32拒絕一般行走覆寫、地形與角色碰撞、阻擋保留世界位置、末拍與隔拍退出測試。fields／roles更新，FUNCS.tsv重產；無新增skip／base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`，6個既有unused-function警告。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，補同類別動態基底的靜止讓路；整類別仍未完成，未進CNpc**。主範圍仍done 72、skip 33、todo 13；同類別基礎新增done 1：0x494cb0，新增skip 0。
  - `main.c actor_yield_tick`接入`actor_motion_tick`的狀態1分支；按請求者到保存世界位置的距離，少於30且可移動、尚未偏移時保存位置，按請求者面向+2偏移20世界單位。只改世界位置，不改占格，不做地形檢查。`actor_step`跨格碰撞透過`actor_yield_request`向同組、種類30..100的占位角色送請求。
  - 保留原版怪現象：偏移後仍用保存位置量距，不會每拍繼續挪動；距離30..63且已偏移時清快取，未偏移則保留請求；距離>=64或請求者失效時清除。自身物件ID=0時不清請求，但仍可清位置快取。清快取不把世界座標移回原位置。
  - shortcut：完整動態11首個路徑點掃描尚未完成，請求來源先由現有跨格碰撞橋接；逐格尋路完成時須移到首個路徑點，不把路徑掃描或動態主入口標done。13個主範圍todo仍主要屬同類別／引擎基礎；只有既有特殊倒數的介面自動暫停通知等不同類別CGameApp。本輪未達成整類別完成要求，下一步不前移。
  - skilltest新增讓路請求的層／組別／種類門檻、面向回繞、世界位置與占格分離、保存位置量距、30..64邊界、無效ID、零ID和不可移動時保留請求，以及實際跨格碰撞到固定拍的消費路徑測試。fields／roles更新，FUNCS.tsv重產；無新增skip／base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；5個既有unused-function警告。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘13個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補命令接收、狀態15與8槽頭頂文字；整類別仍未完成，未進CNpc**。主範圍done 75、skip 33、todo 10；新增done 3：0x48b6c0、0x48b710、0x48d2f0；前置依賴新增done 1：0x4719a0，新增skip 0。
  - `main.c actor_command`按原版先保存記住命令／選目標，再做一般可用性；處理手別更新、入戰、面向、動態切換、技能8/9和互動15..19排隊及四欄去重。`actor_queue_command`接入同一入口；擊退、衝刺／跳躍、受擊12、玩家點地和地點施法已轉共用接收端，刪原來重複門檻與記住命令直寫。`unit.c MUnit.no_animation`承接命令14與隱藏／特殊旗標時的MethodMain全部幀觸發要求，由`action_tick`實際消費。
  - 保留原版怪現象：101或非1/11動態拒絕命令前仍可能保存記住命令／選手別；命令3不因cancel清排隊命令；cancel先清代號會使相同8/9不再被去重；互動去重失敗前仍先忘記舊命令；命令13早於101／knock一般門檻。互動15直接改第7修正，不另造一份狀態。`actor_status_notify`的15在啟用與解除時都用640世界單位、模式0篩選敵人；檢查的是施加者預設技能可用性，讓對方指向自身後送命令11；隱形、顯示50與相同組篩選沿既有原版遍歷。
  - `actor_say`按原版GBK byte數計時（UTF-8先換算），上限255；`actor_overhead`普通文字覆寫0，上飄找第一空槽、滿時第一上飄槽。`actor_draw`實際繪圖時先扣1，首尾8畫面淡入淡出，中段上飄再扣1；隱藏或未繪到不消耗，歸零的下一畫面才清槽模式。`scene_character_say`按編號掃種類30..100第一筆，包含已死；劇情訊息0x53e的第二token台詞／byte14空白直接接入，單機省略原版字串佇列與二進位物件ID往返同步。
  - shortcut：文字以固定UTF-8緩衝與現有SDL白字取代200×100 SBMP的描邊／調色盤，超長字串截在完整字元；待文字渲染基礎校色及動態字串升級。主命令仍缺非角色種類24的場景物件目標分支，屬CObj索引基礎；8/9完整消費仍待同類別技能主入口。動態11仍由既有行走消費，沒有把命令接收當作尋路完成；這些入口均未寫完成位址。尚餘10個todo主要仍是同類別／引擎基礎（尋路、移除／完整繪圖、技能／命令及更新），只有原有介面暫停／狀態外部通知等不同類別系統；未達成整類別完成要求，下一步不前移。
  - skilltest新增命令副作用先後順序、取消／去重差異、手別／面向／受擊門檻、第7修正、狀態15啟用與解除／隱形篩選、文字滿槽替換、GBK計時、繪圖拍數與短倒數邊界、場景訊息分詞及死亡角色顯示；舊地點施法fixture補原版動態1與模板預設技能。fields／roles更新，FUNCS.tsv重產；無新增skip／base／DLC分類。建置與skilltest通過，最後一行`skilltest ok`；另以`-DSTORY -fsyntax-only`檢查訊息接線。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘10個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補共用技能命令消費與衝刺越界工具；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9；新增done 2：0x488bb0、0x48a700；回復誤標todo 1：0x48ffe0。前置依賴新增done 1：0x4945d0；新增skip 0。
  - `main.c actor_skill_command_tick`接記住命令1/2及8/9重送、動態就緒標記、地點命令8與目標命令9；目標失效／102／旗標0x4000、大小與高層射程、最小射程退避、直線追近距離及成功清佇列均按本體分支順序。`pursue`與`hero_cast_point`改提交共用命令，固定拍實際消費；刪歐氏距離出手、61拍判定／60拍上限退避與輸入端立即施法／丟棄命令。`actor_command_idle`接正式消費端；`actor_command`補種類24陷阱直接選目標，`actor_of`也查無劇情角色的object_id。使用者施法等待冷卻；候選及移動觸發保留略過冷卻。模板招式無SkillRow時可正常結束，不讀空指標。
  - 保留原版怪現象：記住命令重送早於動態就緒門檻；8不可用設旗標1並取消，9不可用保留佇列；最大射程大於3才加修正／高層，再減1並加目標大小的一半。OmUtil的IsRange匯出本體為含邊界方框且至少一軸達最小射程；退避呼叫點的隱藏步長是min+1。IsExceedDest已核對DLL匯出跳表：任一行進軸到達即成立、包含相等，斜向OR而非投影；`actor_charge_tick`因此出生中心就先推占格，保留混合格／世界座標算方向、碰撞不消耗插值、末拍後一拍退出。
  - shortcut：技能命令追近仍由`actor_command_walk_tick`橋接既有行走，動態11完整反向路徑／逐格尋路尚未翻，不標主入口完成；舊建立入口缺初次動態退出，先設就緒標記1，生命週期接齊時交退出回呼。主技能分派的15..19互動仍是同類別待補；角色移除、繪圖、更新及施法入戰順序也不能歸作等待外部系統。只有介面自動暫停、NPC全域選人等通知等不同類別CGameApp。另移除CNpc主AI的局部註解完成位址，回復0x4911b0為todo，未繼續實作CNpc。本輪未達成整類別完成要求，下一步不前移。
  - skilltest新增重送先後順序、就緒門檻、8/9拒絕差異、object_id／陷阱目標、方框與大小邊界、min+1退避、直線追近、失效目標、冷卻等待、實際追打排隊至MethodMain／模板無技能列收尾，以及越界八向／相等／非法方向；舊右鍵測試改驗證先排隊、冷卻後消費。fields／roles／base_map更新，FUNCS.tsv重產；無新增skip或已證明劇情未用的DLC功能。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；6個既有unused-function警告，STORY條件編譯語法檢查通過。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補施法入口先後順序及MethodMain完整綁定／解除；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9；新增done／skip均0（補齊既有0x48d330；施法入口仍有外部通知，不誤標完成）。
  - `main.c actor_cast_at`增加forced參數：一般命令8/9傳0，候選與移動觸發傳1；非強制且冷卻正數直接返回，不選目標也不清記住命令。`actor_cast_try`依原版讀當前移速A而非角色speed，模板speed非0且A<=0時拒絕max>=3；啟用與速度門檻過後，明確角色目標先於MethodMain建立，因此資源不足等失敗仍保留選目標副作用。地點施法占格的同隊檢查不改此順序。成功建立後才經命令11要求入戰，不直接繞過其可用性門檻；模式2的記住命令8/9依原始技能ID、可查詢目標、角色種類與敵對關係收尾，隱藏或非角色目標不能保留記住命令。
  - `main.c actor_method_bound`／`attack_speed`與`unit.c main_new`／`unit_die`接原版綁定生命週期：建立時裝冷卻、活MethodMain期間不遞減、解除只清幀調整而保留冷卻；刪掉`action_tick`結束才擲間隔的簡化版。補NPC組種類60在單機模式2只用補拍、其他組即使模板間隔1仍消耗rand；byte345非0在算完冷卻後才把幀延遲加2。trail_mode1綁定設第6修正1/-1，解除只有期限<21才設1/20，與移動模式2共用殘影基礎。+56d確認是攻速補拍，刪獨立behavior_aux，行為重設與攻速統一使用Actor.extra。
  - 保留怪現象：冷卻早退不做失敗收尾；強制觸發可越過冷卻且重新裝冷卻；MethodMain建立失敗仍可能選目標；成功後命令11仍可能被目前招式門檻拒絕。種類60的免間隔按object_kind而非mon指標判定；第6修正-1期限在解除時可改成20，>=21則不能覆寫。
  - shortcut：單機省去招式同步封包，NPC入戰全域選人提示仍等不同類別CGameApp，故施法主入口不寫完成位址。缺模板／空白測試的間隔0不做mod0，正式資料若出現0需核對原版資料契約；byte345只按已知延遲效果保存，未猜其業務名稱。動態11尋路、移除／完整繪圖、技能互動主分派與更新仍屬同類別／引擎基礎未完成，不能列作等待別的系統；本輪未達整類別完成要求，下一步不前移。
  - skilltest新增施法冷卻早退／強制失敗副作用、move_a與speed獨立、模板零速與近距例外、目標隱藏／非角色收尾、成功後入戰命令門檻、實際MethodMain綁定／解除與冷卻消費、NPC種類60、正負cut補拍、byte345、殘影期限21邊界及rand()%1時序。舊fixture補原版當前A／戰鬥速度與活動場景目標索引；舊結束冷卻測試改驗證建立即裝入且結束不改寫。fields／roles更新、FUNCS.tsv重產；未新增skip／base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；STORY條件編譯語法檢查通過，有既有unused警告。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補動態11目的地校正及點地行走共用消費端；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9；另補引擎動態基礎0x493ad0一個done，未提前處理下一類別業務。
  - `main.c actor_walk_fallback`依原版從目的地往角色占格回掃，找第一個合法層且無占位格；距離小於2直接失敗，抵達目前占格仍找不到時改以原目的地找六圈。`actor_walk_destination`接合法目的地、回掃、原格占位與選取目標相等時的距離拒絕及失敗清索引；`actor_dynamic_enter`在11分支接此入口，失敗交共用動態命令退回靜止，校正結果保存於原目的地欄位。
  - 主角點地已改用`actor_command_walk_tick`，刪獨立`point_walk`；點地與技能追近均用校正後目的地作停止條件，避免新目的地只寫入但輸入仍追原占位格。保留原版相鄰被擋目的格直接失敗、六圈以原目的地為中心、校正失敗仍留校正欄位，以及選取目標NULL亦參與占位相等比較的怪現象。
  - shortcut：反編譯本體遺失Y軸累加變數，回掃以與X軸對稱的整數DDA承接，取得組語後須核對相等門檻；未綁場景的舊fixture保留原目的地，建立入口統一登記後移除。反向byte路徑生成／逐格步進仍缺，因此不把目的地校正誤標成完整動態11或角色主入口完成。其餘移除、完整繪圖、技能互動與更新仍是同類別／引擎基礎todo；不同類別介面自動暫停與NPC全域選人等通知依前輪記錄。本輪**未達成整類別完成要求**，下一步不前移。
  - skilltest新增合法／被占相鄰目的地、橫縱斜線回掃、出界目的地、特殊層、全封閉與六圈找空格、選取目標距離拒絕／range相等、真實動態命令安裝校正目的地及到達校正格停止／失敗回靜止。fields補目的地校正語意、roles更新、FUNCS.tsv重產；新增skip 0，無新base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`，有6個既有unused警告；未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪修正維護與動畫播放的轉向時序；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9，新增done／skip均0。
  - 核對角色維護本體後，`main.c actor_maintenance_tick`在冷卻與空中非靜止略過條件之後、修正倒數之前呼叫`actor_direction_tick`；刪掉`actor_animation_tick`裡的轉向。原版的轉向不應拖到較晚的播放階段，也不能由多次播放補做；固定拍現有維護入口已直接消費此修改。
  - 保留旋轉旗標優先於空中略過、每拍方向減1且負數回7、不受4／15拍節流的怪現象。skilltest新增實際維護→播放次序，驗證旋轉只選一次序列、播放繼續幀內拍數，以及空中非靜止不在動畫階段補做普通漸轉；既有播放器測試明確先走轉向階段。
  - shortcut：本次只校正轉向所屬階段；動態11反向路徑／逐格步進、活動啟用、完整動態→動畫→高度次序、角色移除／繪製與技能互動仍屬同類別／引擎基礎todo，不能算等待外部系統；介面自動暫停與NPC全域選人通知才等不同類別CGameApp。本輪**未達成整類別完成要求**，不新增主入口完成位址，下一步不前移。
  - roles更新維護階段、fields補既有動畫方向欄位時序，沒有新偏移、skip、base或DLC分類；FUNCS.tsv重產。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`；有6個既有unused-function警告。未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪補固定拍活動門檻與首次更新鎖存；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9，新增done／skip均0。
  - `main.c actor_motion_tick`以獨立活動啟用值包住動態及高度／共用時鐘；停用仍將首次更新鎖存0改1，回傳進入時啟用值。動態消費後保存已報告狀態，單機不產生同步副本封包；`actor_maintenance_tick`停用時在冷卻之前早退。保留首次更新鎖存並不重新啟用角色的原版行為，不混用原本的動態路徑就緒欄位。
  - shortcut：活動byte以`!activity_disabled`保存，零初始化沿用引擎現有角色啟用契約；原版完整建立／移除生命週期接齊後核對啟停來源。動畫虛擬更新與高度次序、完整動態11尋路、移除／繪圖及技能互動仍屬同類別／引擎基礎；介面自動暫停、NPC全域選人及令牌通知才等不同類別系統。本輪**未達成整類別完成要求**，不新增主入口完成位址，下一步不前移。
  - skilltest新增停用不推插值／高度／共用時鐘／冷卻、不改已報告狀態、停用仍鎖存、重新啟用恢復推進及再次停用；fields更新兩個已知偏移，roles更新，FUNCS.tsv重產。無新skip／base／DLC分類。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`，有6個既有unused-function警告；未commit／push。
  ~~下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起~~。

- 進度（11-17）：Poq.exe **0x485270～0x490220 CCharactor，本輪接齊固定拍的動態→播放→高度→維護次序；整類別仍未完成，未進CNpc**。主範圍done 76、skip 33、todo 9，新增done／skip均0。
  - 核對角色更新／維護及動態分派本體後，`main.c actor_motion_tick`在動態與狀態快照之後呼叫`actor_animation_tick`，再推高度及共用時鐘，固定拍接著進維護。刪除主迴圈尾端的較晚播放及舊`animation_stepped`旗標；停用角色的單次動作也不再私下推進動畫。
  - `action_tick`以播放前幀／完成值交MethodMain消費，已有固定拍播放時不再播放第二次；`start_act`更新動作序號，避免同拍新動作誤用上一動作完成值。這三項快照與動作序號是現有MethodMain排程的橋接資料，不冒稱原版物件偏移。保留末段跳躍先看空中旗標凍結本體、再將高度降零，下一拍動態退出後才恢復播放的次序；延遲1的末幀即使播放器已繞回，仍消費一次觸發。
  - shortcut：單次動作業務仍由後段`action_tick`消費，所以保存播放前觀測值；待完整場景物件／MethodMain更新次序接齊後核對是否能刪掉橋接快照。動態11反向路徑、移除／完整繪圖、技能15..19互動及100拍光環仍屬同類別／引擎基礎todo；介面自動暫停、NPC全域選人與令牌通知才等不同類別系統。**本輪未達成整類別完成要求**，不新增主入口完成位址，下一步不前移。
  - skilltest新增實際動態→播放→高度→維護→單次動作鏈，驗證落地當拍凍結、下一拍只播放一次、單拍首／末幀消費、新動作拒用舊完成值及停用不播放。roles／fields更新已知欄位時序，無新偏移、skip／base／DLC分類；FUNCS.tsv重產。`cd engine && make && make skilltest 2>&1 | tail -3`通過，最後一行`skilltest ok`，6個既有unused-function警告；未commit／push。
  **下一步：0x486270（CCharactor 尚餘9個todo；完成整類別才進CNpc）起**。

### 移植步驟（2026-11-15 定，嚴格照做，不再每個函數翻大量原檔）

先建索引，之後只查索引、只讀要翻的那一個函數本體：
1. **索引一次建好**（`tools/functool.py` 產生，不進 git 的 decomp/ 之外都提交）：
   - `FUNCS.tsv` 的 `callees` 欄 = 它呼叫的遊戲函數中還沒完成的（只需要讀這些）；`role` 欄 = `tools/roles.tsv` 的一句話用途（分析過就補）。
   - 基底自動認：base_map.tsv 的位址／類別，加上本體有 `"CStaticLinkList::…"` 這類容器除錯字串的、只被基底呼叫的。第一次跑認出 Poq.exe 229、EasyRPG.dll 32 個。
   - 分類多一種 `base`：引擎自己的基底工具（CStaticLinkList／CContainer 名單、OmUtil 數學、字串、記憶體池等），**每種只認一次**，在 `tools/base_map.tsv` 寫「原版位址 → 我們用什麼代替」（例：名單操作 → 一般陣列），之後遇到直接當已處理，不讀本體。
   - `tools/fields.tsv`：類別 + 偏移 → 欄位意思 → 我們的欄位名（例：MethodBase +0x233 = 名單數量 → `hit_n`）。讀到一個新偏移就補一列，之後不再重推。
   - `tools/fn.py ADDR…`：用預先建好的位址索引直接印出那幾個函數本體（不掃整個檔）。
     **讀本體一律加 `--brief`**：刪掉 security cookie、區域變數宣告、GPOINT 解構，CString／std::string 等樣板名縮短，去掉 `thunk_`，
     funcs_done.txt／base_map.tsv 有的函數換成 `⟨我們的名字⟩`，fields.tsv 有的偏移換成 `param_1->⟨欄位⟩`
     （只在 FUNCS.tsv 的 class 確定（沒有 `~`）、函數是 __thiscall/__fastcall、且那個偏移只有一種意思時；fields.tsv 裡同類別同偏移寫了兩種意思的保留原樣）。
     0x442660～0x442b10 這批試用：8,982 → 7,692 字元；Poq.exe 全部 todo 本體 617 萬 → 448 萬字元（−27%）。
   - `tools/functool.py next [ADDR] [N]`：從 FUNCS.tsv 第一個 todo（或 ADDR）起列出下一批（同類別或連續位址，最多 N 個）：
     行數、標記（`解構`、`std::string`、`轉接` = 只呼叫另一個函數、`空`、`CRT:…`）、role，和這批還沒移植的被呼叫函數（行數、類別），最後印出讀本體的指令。
2. **每次移植一段**：`functool.py next` 取下一批 → 標了 解構／空／轉接／std::string 的先判斷是否只要記進 funcs_done.txt 或 base_map.tsv → callees 是 `base`／`lib`／已 done 的不讀 → 用它印出的 `fn.py --brief …` 讀還沒做的本體 → 欄位查 fields.tsv → 翻譯、寫位址註解、補 fields.tsv／role → 重跑 functool.py → 建置測試 → 提交。
3. 不能為了「找完整來龍去脈」去翻別的段落；缺的資訊先查索引，索引沒有才讀那一個函數，讀完寫進索引。

下面 A～K 是 11-14 憑盤點寫的清單，保留當說明；之後以 FUNCS.tsv 為準。

## 全部待辦清單（2026-11-14 盤點；由上往下照順序做，做完打勾，不跳著做）

清單是機械式列出的：劇情檔裡還沒翻的引擎呼叫、BNT 觸發區的種類、原版指令表、DialogInfo.ini 的介面、各節「還沒做」。
重新產生引擎函式清單：`grep -rhoE "(FUN_|ENGINE\(0x)[0-9a-f]{8}" story/room story/npc story/item story/skill story/plot story/_shared.c | sort | uniq -c | sort -rn`，扣掉 story_rt.c 已有的 case 和 _shared.c 的共用函式。

### A. 劇情引擎函式（85 個，括號是劇情檔裡出現幾處）
**2026-11-14 全部翻完**（story_rt.c）。劇情世界現在是房間、出口、人物（怪物）的來源：
- 物件多了種類（0 房間 1 人物 2 道具 5 技能 6 劇情事件）、隊伍帶頭者、房間的出口表；存檔版本 2。
- spawn（0x10052550）照原版產生人物物件（數值走引擎的 story_on_person = 0x100730c0，公式改成原版的 (L/除數+1)×((L−1)×參數+基準)，表上空格 = −1），引擎進場景時 actors_sync 把房間裡 G_PosX = −100 的人物放到入口 G_PosY 附近。打倒時呼叫第 10 格再刪（Boss 開劇情）。
- go 走原版：出口要開著、隨機地下城出口、舊房間第 13 格／新房間第 14 格檢查、進房間後第 16 格（例如進陽周城孔剛加入隊伍）。房間第一次產生後第一次進去不刷新，之後每次進去呼叫房間和裡面物件的第 6 格。
- 原版送給畫面的訊息經 story_on_msg 排隊，story_end 時處理；沒做的會印「劇情訊息 0x… 還沒做」（見 K）。
- storytool 修正：Ghidra 認成 cdecl 的虛擬函式格，參數從 a[1] 起（房間第 13 格的目的房間之前讀錯）；SetSigs 補了 random_exits、0x100528d0、0x10056640、0x10055670、0x1004a940 的簽名。
- 非 STORY 建置（Mac 原生）還是用 res.c 的 world_load。
- [x] 10052550（spawn）
- [x] 1005d8b0（add_exit）
- [x] 1003c6a0（set_attr）
- [x] 10054960（place_npc）
- [x] 100558c0（current_room）
- [x] 1003fd90（find_alias）
- [x] 1004f1a0（start_plot）
- [x] 1003cbf0（set_int_alias）
- [x] 10052a20（random_exits）
- [x] 1003fe30（find_room）
- [x] 1003c550（477 處）
- [x] 100730c0（362 處）
- [x] 10022980（240 處）
- [x] 100757a0（203 處）
- [x] 1003e5d0（140 處）
- [x] 1004ea10（109 處）
- [x] 1003f300（105 處）
- [x] 100226d0（105 處）
- [x] 100528d0（82 處）
- [x] 1004c4f0（57 處）
- [x] 1004abd0（56 處）
- [x] 1004e4c0（54 處）
- [x] 100406a0（54 處）
- [x] 1004b020（51 處）
- [x] 1005f2e0（48 處）
- [x] 1004c6d0（48 處）
- [x] 1003fc80（47 處）
- [x] 10054630（46 處）
- [x] 1004db90（46 處）
- [x] 10037850（46 處）
- [x] 10054f90（42 處）
- [x] 1004e1b0（39 處）
- [x] 1004e0d0（39 處）
- [x] 1003e920（35 處）
- [x] 1003ca70（34 處）
- [x] 1004aac0（28 處）
- [x] 1003d200（27 處）
- [x] 1003d0b0（22 處）
- [x] 1004e200（17 處）
- [x] 1003cd10（16 處）
- [x] 1003ee60（14 處）
- [x] 1003c9f0（14 處）
- [x] 1004e640（13 處）
- [x] 10055ae0（10 處）
- [x] 1004d180（9 處）
- [x] 10021cd0（9 處）
- [x] 100fa310（8 處）
- [x] 10056640（8 處）
- [x] 1003a880（8 處）
- [x] 1003daf0（7 處）
- [x] 10021c60（7 處）
- [x] 1005f390（6 處）
- [x] 10055670（6 處）
- [x] 10053b40（6 處）
- [x] 1004a940（6 處）
- [x] 1003e160（6 處）
- [x] 100f9630（5 處）
- [x] 1004b6c0（5 處）
- [x] 1004ad90（5 處）
- [x] 1003aea0（5 處）
- [x] 100f95f0（4 處）
- [x] 10048790（4 處）
- [x] 1003ce20（4 處）
- [x] 100222f0（4 處）
- [x] 10054810（3 處）
- [x] 1003f2c0（3 處）
- [x] 10022ac0（3 處）
- [x] 10022490（3 處）
- [x] 1020f761（2 處）
- [x] 1020f731（2 處）
- [x] 100fa200（2 處）
- [x] 100f9570（2 處）
- [x] 10058d30（2 處）
- [x] 10053e10（2 處）
- [x] 1003ff70（2 處）
- [x] 1003ed10（2 處）
- [x] 1003c510（2 處）
- [x] 10039b60（2 處）
- [x] 10038f90（2 處）
- [x] 1005e490（1 處）
- [x] 1005d790（1 處）
- [x] 100549d0（1 處）
- [x] 1004d500（1 處）
- [x] 10040760（1 處）
- [x] 1003d380（1 處）

### B. 原版指令表（EasyRPG 0x1006b2e0 登記，ER_Explain 0x1005fa30 分派；單機會用到的）
- [ ] go（出口；現在是 main.c 自己做的，翻完 add_exit 後改走原版 0x10017e30）
- [ ] open、openall（寶箱、門；建築物件 isbuilding 1/2/5/6，0x1004ea10、0x10041d10、0x10040970、0x1004e680）
- [ ] use、abandon、eat（eat 已做）、weaponset（成套武装）
- [ ] s_pctstart、s_pctend（劇情事件）
- [ ] s_savepos、s_saveequip、s_fightstatus、s_delscene、s_scenecmd、s_deltrap、s_testgo、s_testdir、s_quitok、s_testopen、s_time、s_findtrap、s_savetrap、s_needsceneinit、s_sceneinitend
- [ ] c_talkend、c_equipstr、c_changeint、c_fightstatus、c_attributes、c_init_attr、c_itempos、c_itempos2、c_itemreplace、c_skillpos、c_equippos、c_extrause、c_extrause2、c_arrow、c_wname、c_uibuy、c_buychange、c_aigroup、c_priceprod、c_priceiden、c_pricerepa、c_choose、c_tradebegin、c_tradeend、c_ItemInfo、c_readestr、c_log、c_making
- [ ] askquit、asksave、saveexit、chapter
- 連線版、除錯用的（help、look、register、team…、robot_*、kill、summon、goto、clone…）不做

### C. 觸發區（BNT 類型 4，事件文字）
- [x] go <出口>（2729 格）
- [ ] s_pctstart <劇情路徑>（157 格；Poq 0x484a80，格子旗標 0x88、主角在範圍內才送）
- [ ] 建築物件：事件文字 = 房間裡劇情物件的別名（suijiwupinbaoxiang、netsuijiwupinbaoxiang、keysuijiwupinbaoxiang、suijikuangshi、netsuijikuangshi、random_tree、netrandom_tree、boxa_xnyd、box_a~d、kuangshi_a/b、shumu_a/b、iron_ore、各種 guizi/xiangzi/waguan/door…，共約 1900 格）
- [ ] openall <別名>、bingyongzu*（兵俑）
- [ ] c_aigroup <組> <值>
- [ ] 空事件（932 格）：查原版用途

### D. 劇情事件和過場
- [ ] start_plot 0x1004f1a0：第 17 格檢查、0x52e 送客戶端播 Rec/User/<別名>.rec、結束 0x1004f2a0 跑第 18 格
- [ ] .rec 播放器：'@' 00~0c 全部、'#' 0x01~0x12 全部（格式見「劇情事件」一節）、對話框 PTalk_C.dlg、跳過

### E. 介面（DialogInfo.ini；已做：JM50、51、514、515、52~56、62、63、67、73、102）
- [ ] JM011 扉页、JM01 外部主界面、JM02 单人游戏、JM09 创建新主角、JM21 难度选择、JM012 读盘
- [ ] JM31 游戏设置、JM30 功能环境、JM36 游戏帮助、JM105 在线帮助、JM33 制作群、JM35 动画
- [ ] JM108 对话选择（取代現在自己畫的對話框）、JM000 字幕、JM66 场景名、JM69 章节、JM71 结局、JM72 演示
- [ ] JM57 道具介绍、JM58 人物介绍、JM59 技能说明、JM60 地图、JM61 日志、JM65 消息、JM70 地面道具
- [ ] JM103 任务失败、JM104 任务完成、JM513 死亡、JM68 暂停、JM109 扔钱、JM79 学习技能
- [ ] JM100 框架、JM511、JM512（查原版有沒有用到）
- 連線版不做：JM04、05、06、64、74、75、76、77、78、80、101、106、122、JM510（除錯）

### F. 戰鬥、角色
- [ ] 盾牌格擋（Poq 0x48c4e3）
- [ ] 位移（招式 type 7）、混色 bData3、其他 iTriPos（2、3、7、8、11）、iFlag 3 的其他觸發
- [ ] 名字是圖形 ID 的 5 種怪物（蝙蝠、骷髏、蜘蛛…）
- [ ] `颜色` 變色、傷害數字、死亡（主角死亡 JM513、怪物屍體時間照原版）
- [ ] 左手技能施放、精氣消耗、按住加點加速
- [ ] 怪物 AI 照原版（閒逛半徑、跟隨類型 15、閒逛停留時間）
- [ ] 隊友（Friend 2、隊伍面板其他格、經驗分配）
- [ ] 狀態（buff）
- [ ] 速度比調整動畫延遲（加速／減速）

### G. 道具
- [ ] 成套武装（weaponset 0x1001eaa0、JM55 m_pCombo／m_Asunder、UI_Pos 40）
- [ ] 自己用技能打造、五行換算 0x10058900、m_pMakerInfo
- [ ] 原版說明文字 0x100279b0、can_eat、拖放規則、超重、箭的消耗、相生特性、地上道具畫法、系統訊息字串表
- [ ] 寄存道具原版存在哪

### H. 存檔
- [ ] 每格的隊伍頭像（ResID1~5）
- [ ] 地上道具、狀態、各場景怪物的狀態
- [ ] 測：刪除、覆蓋確認、自动存盘、讀回後的劇情對話

### I. 畫面、聲音
- [x] 光影、日夜（light.c：亮度表、時鐘、角色／建築／招式光源、下雨變暗、閃電、色調）
- [ ] 遮擋排序（大型建築）、透明效果設定（Gfx3D 旗標 0x80）
- [ ] 鏡頭停在地圖邊緣（對照原版）
- [ ] 按鈕指著／按下的幀、旗標 0x20000、物品和技能圖示補齊、頭像
- [ ] snd.lib 介面和技能音效
- [ ] Gfx3D.dll（2022 Steam 版 3D 效果）

### K. 劇情送給畫面的訊息（原版 EasyRPG → Poq，Poq 0x47d4e0／0x5174d0 分派；引擎 story_msgs）
- [x] 0x51d 物件進出房間（重對角色）、0x535 Invincible、0x51c G_AIGroup、0x42aa0 HP、0x574e0 劇情讓人死
- [ ] 0x52e 劇情事件過場（現在直接結束；D 做）
- [ ] 0x49b70 加入隊伍、0x51e 離隊（F 隊友）
- [ ] 0x51b NPC 頭上標記（G_NPCActive）、0x519 說話轉向
- [ ] 0x532 人物位置／鏡頭、0x533 畫面淡入淡出／震動、0x53a 章節（JM69）、0x56640 @poqdata
- [ ] 0x52c 出口開關（畫面上的觸發區）、0x605 地圖開圖（JM60）、0x61d 清日誌（JM61）、0x607、0x629、0x60b、0x3f8 廣播
- [ ] 0x522 狀態旗標、0x2d8d0 六屬性重算、0x48e20 UIS_P、0x4ea10 建築狀態（C 寶箱）、0x47870 屬性更新
- [ ] 其餘原版訊息編號（EasyRPG 送出的有 0x515~0x53e、0x5dd~0x63f 共約 110 種）逐一對 Poq 處理：
  0x515 0x516 0x517 0x518 0x51a 0x51f 0x520 0x521 0x523 0x524 0x525 0x526 0x527 0x528 0x529 0x52d 0x530 0x531 0x534 0x536 0x537 0x538 0x539 0x53c 0x53d 0x53e
  0x5dd 0x5de 0x5e1 0x5e2 0x5e4 0x5e5 0x5e6 0x5e7 0x5e9 0x5ea 0x5eb 0x5ee 0x5ef 0x5f0 0x5f1 0x5f2 0x5f7 0x5f8 0x5f9 0x5fa 0x5fb 0x5fc 0x5fd 0x5fe 0x5ff
  0x600 0x601 0x603 0x604 0x606 0x609 0x60a 0x60c 0x60d 0x60e 0x610 0x611 0x612 0x613 0x614 0x615 0x616 0x617 0x618 0x619 0x61a 0x61b 0x61e 0x61f
  0x620 0x621 0x622 0x623 0x624 0x625 0x626 0x627 0x62a 0x62b 0x62c 0x63f

### J. 最後
- [ ] 手機操作重新設計（使用者說等整個遊戲做完）

## 劇情沒用到（以後 DLC 可用）

原版程式有、但單機劇情從來沒觸發的東西。移植時照樣記下（有的已經做好、只是沒人呼叫），以後做 DLC 可以直接拿來用：
- 舊共用命令區解碼入口（0x47cd10～0x47d380、0x47ece0／0x47eec0、0x47f580～0x47fb90）：索引無呼叫者，依本輪規則略過；可供DLC重新接線參考。這只指舊入口，不代表其隊伍、屬性、換景、天氣或稱號業務沒有劇情用途；仍有呼叫者的建立入口另留todo。
- 天氣 10 疾風：EasyRPG 0x10042420 會送 G_WEA_Gale，但 Poq.exe 0x44e5d0 不把它放進機率表，所以不會擲到。weather.c 已做（沙塵、風聲），`weather_set(10, 拍數)` 就能用。
- 天氣 11～16 色調：0x4697b0 讓 11 紅、12 綠、13 藍、14 黃、15 變亮、16 淡紅（色表模式 2～7），light.c 的 `light_mode` 已做。機率表只有 0～9，所以劇情到不了。
- 天氣 6～9（冰雹、紅天、粉天、白天）：房間會設機率，也會擲到，但原版沒有任何畫面效果。DLC 可以補上畫面。
- 色表 17（全部 ×1.5）、18（藍色加強）：0x4a5880 會做出來，但沒有程式把亮度設成 17、18。
- 光影模式 1、3（全域 DAT_00626398）：單機固定是 2，其他模式的緩衝 0x4a6480 有建立但沒用到。
- 遊戲時鐘的設定（0x44e880、0x44e890）：只有連線版和主控台會呼叫。單機時鐘從 0 點開始走，日夜交替會照常發生，但不能指定時刻。
- CZhenMng（0x446390～0x447d90，名字是「陣」但內容是一群光點）：10 組，每組一串「從來源物件飛向目標物件」的光點群。初始化 0x4471a0（來源、目標、點數；範圍 60），
  每拍 0x4476c0：來源點朝目標每拍走 10 像素（距離 > 4 時，一半機率範圍 +1）；每個光點沿路線前後擺動（速度 1～4，超出範圍就反向）、左右上下各在 ±7 內抖動；
  畫 0x446cd0：光點離目標不到 10 像素就消失（範圍 −1），用 DAT_00662d38+400 那張圖畫。原版沒有建構它，要用得整段新寫。
- CGroup／CUserGroup 隊伍群組（0x440e70～0x441e00）：單機沒有建立，列在 tools/funcs_skip.txt。
- 連線版目標頭像（CUserInput 0x4548c0、0x454900～0x454a00、0x455b80）：DAT_00942f28 == 2 時，游標指著 NPC 組（10000～60000）的角色、地圖種類 0x12／0x13，送 0x218 給介面秀它的頭像和名字。
- 可選輸入錄影／回放 CScnClientRecord（0x471ed0～0x473660）：RecordType=0 的單機劇情不啟用record.rec錄製／回放、錄影專用對話及鏡頭控制。日後可供DLC重播使用；PlayIntro片頭另有用途，仍待既定網頁影片播放工作，未列為劇情無用。
- Rec 對話等待設定（0x473760）：沿用上述可選回放分類；單機劇情不啟用，PlayIntro仍由網頁影片工作處理。
- 除錯指令 SetFog（設霧 0x4c8630）、GetSpeed（0x455d60 讀 DAT_00662924，DAT_0094c1b8 == 1 時）。

## 引擎現況

- 場景：地面、疊加層、建築和樹（Res01）、動畫部件（Res00 第 0 序列）都正確，抽驗了 4 張地圖。
- 主角：遊俠的紙娃娃 11 個部件加影子（黑色、半透明），平時的待機和走路，8 個方向都對。
- 移動與動畫：照原版固定 16ms 節拍、速度對照表、每幀停留拍數。碰撞用 MAP 的可走旗標，撞牆時沿軸滑動。
- 繪製：先以 1:1 畫到 vw×vh 的畫布，再整張縮放（避免磁磚接縫出現細線）。物件依 x+y 排序，主角在自己的深度插入繪製。
- 記憶體：封包只把索引放進記憶體，圖片用到才讀；貼圖快取用 hash table。驪山實測約 152MB（含 macOS 開銷），Mac 上每秒 100 幀。
- 輸入：鍵盤、手把（十字鍵、左搖桿，Select + Start 離開）、滑鼠和觸控（按住就往那個方向走）。

### 已知問題
- 網頁版的下載方式（為了手機 4G）：
  - 伺服器 `tools/webserve.py` 用 `[路徑,-1,序號]` 直接回 Lib 第幾項，手機不下載索引表；`[路徑,0,-1]` 回整個檔案；回應用 gzip 壓縮。
  - 開場只等必要的小資料：場景檔、角色 PLI（`res_preload` 一次批次）、動畫定義（`res_fetch_now`）。`EasyRPG.dll` 等第一次踩出口才讀。
  - 圖片不等（`lib_try`）：還沒到就先不畫，排進優先佇列；進場景時其餘的圖依離主角距離排進背景佇列（`lib_want`）。`res_pump` 每幀一次只送一批約 400KB（依各封包平均項目大小估）。
  - 音樂等場景的圖都下載完才開始抓。
  - 模擬 4G（4Mbps、延遲 100ms）實測：約 6.5 秒進入遊戲迴圈，約 10 秒看到完整畫面。
- 遮擋排序是簡化版，大型建築可能疊錯。
- 速度比調整動畫延遲（加速／減速時）還沒做；目前只有平常速度。
- 驪山 LISHAN 有大片空地面，推測是地圖外圍，沒有跟原版對照過。
- `Gfx3D.dll`：2022 年 Steam 版有部分效果改用 3D，還沒研究。

## 下一步（照順序做，不必再問使用者）

1. ~~節拍與移動單位~~（已完成）
2. ~~場景切換、NPC 顯示與平時行為~~（已完成；閒逛的停留時間是自訂的，跟隨類型 15 目前站著不動）。
3. ~~音樂、腳步聲~~（已完成）。`snd.lib`（Mfx sound lib 2.0）還沒拆，可能是介面或技能音效。
4. 戰鬥（進行中）：
   - ~~怪物出現：房間的怪物群、等級、數值表~~（已完成；閒逛半徑 4 格是自訂的）。
   - ~~怪物追打、主角點怪物攻擊、物理傷害公式、挨打和死亡動作、經驗與升級~~（已完成，見「戰鬥」）。
   - ~~主角初始裝備（數值與外觀）~~、~~道具系統~~（已完成，見下面「道具系統」）。
   - **遮擋**：原版 `ini/Config.ini` 的 `透明效果` 預設 0，這時物件照常畫，**主角被樹擋住就是看不到（跟原版一樣）**。設成 1 時 Poq.exe 對物件畫圖加旗標 0x80（0x4356b0、0x435b51、0x4373af），角色另有旗標 9（0x48739b），效果在 Gfx3D.dll 的 Paint 裡還沒拆。之後做成設定選項。
   - ~~命中判定~~（已完成）：Poq.exe 0x48c3a0（CCharactor 虛擬函式第 104 格，對防守方呼叫）。攻擊方 HR 在 +0x518、防守方 SR 在 +0x514；千分比 = HR×1000 / (SR + HR)，夾在 100～900，`rand() % 1000` 小於它就命中，打空就直接結束（沒有傷害、音效、挨打動作）。命中後才送 315 號訊息給 EasyRPG.dll 算傷害。在這之前還有盾牌格擋（0x48c4e3，看防守方 +0x4e7 和攻擊方向），還沒做。
   - ~~招式特效（type 2）~~（已完成）：觸發的 iTriID = `ObjMdl/MethodUnit_Mdl.ini` 的 wAutoID，`dData0` 是 Res00 動畫（例如 241 劈砍命中 = 0x10084B，一個序列 10 幀）。iTriPos 1 = 畫在對方腳底，0 = 自己；其他值（2、3、7、8、11）先當自己。混色（bData3）還沒做。
   - 下一步：位移（type 7）。
   - 之後：招式特效（type 2）和位移（type 7）、技能、掉寶、死亡後的原版處理、名字是圖形 ID 的 5 種怪物、`颜色` 變色、傷害數字。
5. 介面（使用者 2026-11-13：**先把基本遊戲做出來，劇情之後再接**）：
   - 原版介面定義在 `ini/DialogInfo.ini`（XOR）：每個 `[介面]` 一段，元件「名稱=種類,x,y,寬,高,旗標,ID,提示,圖,…」。種類 1 背景、2 按鈕（圖的各幀 = 平常/指著/按下）、6 進度條、9 靜態圖、10 文字。圖是 ResUI 編號，多半是動畫定義（Res00 格式），少數直接是 xbm。
   - 介面位置照 `Poq.exe` 建立介面那段（約 0x5d3xxx）：底部主列 `JM51_状态界面_中间` = 底部置中；隊伍面板 `JM51_状态界面_主控` = 右緣、貼在主列上方；敵人狀態 `JM515_敌人状态界面` = 上方置中、距頂 20。`JM511`、`JM512` 這版沒用到。
   - 進度條（`CProgress` 第 12 格 0x401690）：旗標 0x4000 = 直的由下往上，否則橫的從左往右。
   - **已完成**：底部主列（背景、按鈕、物品格、血條、精氣條）、隊伍面板（主角那格的血和精氣）、敵人狀態（名字用 Data/npc_*.txt 第 1 欄「人物名称」；GBK 經 `gbk.h`（`tools/gbktable.py` 產生）轉 Unicode 給原版字型）。頭頂的臨時血條拿掉了。精氣：遊俠模板的升級表 `Upgrade_MP_MAX = 140 4`（HP 是 `120 28`）→ MP_MAX = 140 + 4×等級；還沒有耗精氣的招式，所以一直是滿的。敵人狀態原版沒有底圖，只有名字和血條（已在瀏覽器確認）。網頁上方的操作說明在第一次觸控／按鍵後收掉，免得擋到敵人名字。
   - **視窗**：主列（和人物屬性視窗裡）的 m_pRole／m_pSkill／m_pProp 開關 JM52 人物属性（中線左邊，x = W/2 − 寬）、JM53 技能、JM54 道具（中線右邊，x = W/2），y = 垂直置中再上移 50（Poq.exe 0x4ff3xx 等）；m_pCClose 關。只畫了底圖和按鈕，裡面的文字（屬性、描述）、道具格、技能表還沒填；按鈕還沒有指著／按下的幀；開視窗時遊戲沒暫停（原版是否暫停還沒查）。
   - **文字表**：原版介面文字在 `Dlg/UI_C.dlg`（加密：每個 u32 依位元組與 0xAD−i 互斥，字串第 i 個 byte 與 長度−i+0xA9 互斥；Poq.exe 0x4c1b10），`tools/dlgtool.py` 可以印出來，引擎用 `res_dlg()` 讀。`[1…]` 是顏色標記，目前直接拿掉。
   - **人物屬性 m_pInfo**：照 Poq.exe 0x54f820 的 25 行（標籤 2105…2125、數值順序 Exp、NextExp、Fame、HP、MP、Str、Con、Dex、Savvy、Charm、Wisdom、SR、HR、AP、MAP1–5、DP、MDP1–5）。用 ini 第 10~13 欄的捲軸圖（軌道 8151、上 389、下 386、滑塊 383）捲動，箭頭能點，位置是推的（框內右緣），還沒對 UIWndEx.dll；聲望、五行攻防先 0。加點面板（m_pPanel/m_pAttrib/m_pOK/m_pBonus）照 0x554370 在沒有剩餘點數時藏起來。
   - **人物基本描述 m_pDescribe**：Poq.exe 0x554600 的 `"%s\n%s%s\n%s%s\n%s%d\n%s%d\n"`：Name、2101+Prof、2102+(2197+Gender)、2103+Age、2104+Level。Prof = 文字 2131 + 職業序號（0x94c2fb，游侠 = 1）。Name 原版來自開新遊戲輸入（0x94c358），那畫面沒做，先寫「扶苏」。字行距 16（框高 84 放 5 行）。
   - **道具 JM54**：m_pMoney 顯示錢數（0x54db42；「金钱：%d」2126 是提示）。開局背包是空的：主角的武器、布衣 UI_Pos 是 32、31 = 裝備欄。
   - **裝備 JM55**：道具視窗的 m_pEquip 打開，位置同人物屬性（0x54ad87）。裝備欄 = UI_Pos − 30（0 头盔、1 盔甲、2 右手、3 左手…）。圖示 = 道具表第 31 欄「道具栏图片ID」（EasyRPG 0x10070360 存成 G_PicID2）。介面元件上限改 128（JM54 有 80 多個）。
   - **元件顯示旗標**：旗標沒有 0x1 也沒有 0x8 的一開始不畫（升級提示、選取框、成套裝備欄、JM55 的 m_pItemBox）。只有 0x8 判斷不行（JM53 背景只有 0x1）。
   - **技能 JM53**：Poq.exe 0x560f30 → 0x55f900。第 p 頁顯示 SkillData 第 4p~4p+3 個：m_pItem[i] 圖示（ResID）、m_pItemInfo[i] 名稱、m_pOK[i] 等級；m_pAdd 只在有剩餘技能點、等級沒滿、CanUpgrade 時可按。名稱在 Dlg/name_C.dlg（技能表名稱ID）。單機遊俠技能 = skill_1.txt 前 20 列（「网络」開頭是連線版），剛好 5 頁。頁籤 m_pPage[1]~[5] = 第 0~4 頁。等級 0 畫灰（原版加 0x40000，當成灰色是推的）。
   - **升級點數**：每升一級 Extra_Exp +10、Extra_Skill +1（EasyRPG.dll 0x10026d30）。人物屬性開著是顯示模式；左邊「升级」m_pUpgrade（Poq.exe 0x5538a0）切到加點模式（0x554370，沒點數就留在顯示），同時右邊開技能視窗的加點模式。加點面板 m_pAttrib[0..5] = 力量、体质、敏捷、悟性、魅力、智慧（「%s[2%d]」），m_pOK[i] 一下 +1（原版按住會加速，400→50 毫秒，沒做），m_pBonusTitle = 2201、m_pBonus = 剩餘點數；用完回顯示模式。技能 m_pAdd[i]：EasyRPG 0x1003b9e0 的條件（有技能點、等級 < 上限、等级需求增量 × 等級 + 等级需求 ≤ 主角等級）；下方 2202 + 剩餘技能點只在加點模式顯示。種類 10 文字欄的圖不畫（4326 是勾勾、331 是別的）。網址 `level=N` 會照原版給 (N−1)×10 屬性點、N−1 技能點，方便測。
   - **左右手技能**：主列 m_pSkill[0] = 右手（CurRSkillID，控制項 1016）、m_pSkill[1] = 左手（CurLSkillID，1017）（Poq.exe 0x504990）。點了開快捷技能視窗：右手 JM514（0x504d90，+0x2e4 = 0）、左手 JM515（0x504b40，+0x2e4 = 1）；0x564360 依序放進 Hand（技能表第 7 欄左右键位）等於這隻手或 2、SpeType == 0、Level > 0 的技能。遊俠技能鍵位只有 1、2、3（3 = 打造、修缮，不會出現）。點敵人用右手技能的 Attack ID 招式（手機對應是自訂的）；左手技能還不能施放。開局兩手都放斩棘披荆是猜的；視窗位置（原版 0x583790）還沒翻，先貼在按鈕上方。
   - **修正**：原版鍵名 SR = 命中率、HR = 闪避率（看 UI 標籤得知）；主角的命中、閃避之前對調了，已改正。原版命中判定用攻擊者 +0x518 對防守者 +0x514，哪個偏移是哪個鍵還沒直接確認。文字（原版字型）現在 Mac 版也有。
   - 還沒做：按鈕按下的反應、精氣（MP）數值、頭像、物品和技能圖示、旗標 0x20000 確切意義（目前當「一開始隱藏」）、敵人狀態在手機上要顯示誰（原版是滑鼠指著的，目前用攻擊目標；還沒實測到畫面）。
   - 之後：人物屬性（JM52）、技能（JM53）、道具（JM54）、裝備（JM55）這些視窗，照同樣方式從 DialogInfo.ini 讀。

其他待查：原版鏡頭可能會停在地圖邊緣，不露出黑邊；要對照原版確認。

待辦（使用者 2026-11-12 列入；**目前先專心復刻遊戲本體**，這些等遊戲內容做得差不多再做）：
- 手機虛擬按鈕：搖桿、攻擊、技能、物品等按鈕，依螢幕邊緣排版。
- 藍牙裝置：藍牙手把、鍵盤（瀏覽器 Gamepad API；SDL 已經吃得到手把，要實測）。
- 直式與橫式畫面：兩種方向都能玩，介面依方向重新排版。

暫停的平台（不要做）：
- R36S：已有 `make r36s` 交叉編譯和 PortMaster 啟動檔，Linux VM 驗證過，沒有實機測過。
- 平板：iOS 和 Android。

## 道具系統（2026-11-14 完成）

- **產生**（res.c `item_new` = EasyRPG 0x10070360）：讀 41 張 item_N.txt + attribT + SLevel_i/t。別名開頭相符或依等級隨機（唯一 Y 不隨機掉）；AP 小在下半段、大在上半段；DP、耐久、五行（46/47 欄兩次機率，第一次中帶相生特性，相生特性先不生效）、固定特性（萬分比累加）、隨機特性（每次 1/3 機會、依抽取几率、不重複，超過 2 個要鑑定）、金色屬性要鑑定、特性影響等級需求 L×4−3、價格修正率。掉寶類型 999/100/101/103/201~207/1001~1119 → `item_type_pick`（0x1007a2f0）。
- **身上**（main.c `inv[41]`，依 UI_Pos）：0~23 背包、24~29 快捷欄、30~40 裝備欄。開局 qingtongwandao1 → 32、buyi1 → 31（劇情的 item_new）。
- **數值**：`hero_derive` 基礎 0 → 裝備固定值 + 技能固定值 → 回復 → 職業倍率（Str/Con/Dex/Wis 含裝備加值）→ 裝備% + 技能%。
- **穿脫**：檢查 0x1004c0e0（等級、力、體、敏、耐久 ≠ 0、已鑑定）；部位 → 欄位；雙手武器空出左手（箭除外）。外觀 `hero_refresh` 依該職業裝備串（33~37 欄）重載快取裡的主角 Role（原地換，位址不變）。
- **耐久**（0x1003c110 → 0x10056730）：物理攻擊方式 1/10 打中後，攻方隨機一件武器、守方依千分比挑盔甲類型，HitPoint 滿 40 扣 1，剩 HP_MAX/8+1 提醒，0 就脫下。
- **掉寶**（0x1006fa50）：怪物表 39/40 件數、41~50 五組（類型、機率×10 千分比），等級欄 = 怪物等級/5+1（≤9）。這裡倒下時才產生（原版出生時），地上道具畫 G_PicID（ResUI 28×28 黑底），點了走過去一格內撿；金钱（類型 60）加到 Money。換場景清空。
- **道具視窗 JM54**：點 = 選（m_pHItem 4412 框 + 左邊說明），再點 = 裝備/使用，選了點空格 = 移動；使用、丢弃（掉腳下）、修理（修缮之术，等級 ≤ 技能×3，錢 = 原價×損耗×4/上限）、鉴定（慧眼识金，(等級+4)×10 精氣）、整理。負重 m_pBurden = 重量/（Str×1.5+30）。JM55 點裝備 = 脫下。
- **吃**（0x10016be0）：要已鑑定；食物 21/292/22/293 回 HP/MP，書 27~33 永久加屬性，吃完消失。
- **買賣（2026-11-14）**：劇情選項呼叫 0x10055990(主角, 商人) → 左邊 JM63 交易介面、右邊道具 JM54。商人的貨 = 劇情物件身上的道具：第 1 格的 add_item／drop_items，或第 6 格「刷新」進貨（原版 0x10052e40 輪流刷新沒有玩家的房間；這裡每次進房間刷新一次）。15 家店共用的進貨 0x101db580 等「共用劇情函式」由 storytool 放在 story/_shared.c，依原位址登記（story_fn_add），繼承的格子和跨類別呼叫都會找到。
  - 頁籤 m_pWeapons 3、m_pShield 2、m_pLeechdom 1、m_pOther 0（Poq 0x558850 起，開店 = 3）；分類照 EasyRPG 0x1003aef0：類型 0~6、49、50、52 → 0，51 → 1，25~34 → 2，7~24 → 3，每頁前 20 件。
  - 價格 0x10029400：魅力 c、隊伍平均魅力 m；賣 = 價×(c+20)(m+20)/(c+50)/(m+50)，買 = 價×(c+50)(m+50)/(c+20)/(m+20)，沒鑑定用 ValueUn 且買價 ×10。錢不夠秀 EasyRPG_C.dlg 1185「(價格/身上)」。賣掉的給商人，可以買回。
  - NPC 修理 = ValueUn×損耗×2/上限（UIS_Repair）、鑑定 = ValueUn（UIS_Identify）；按鈕依商人的 UIS_* 顯示。原版 m_pBuy/m_pSell 是游標模式（7、8），手機改成點選再點一次。
  - Ghidra 把 drop_items 0x100521f0 的參數猜成 4 個，實際 6 個（誰, 類型, 件數, PosY, 等級, 欄）：tools/ghidra/SetSigs.java 在反編譯前設簽名（`analyzeHeadless /tmp/ghproj qin -process EasyRPG.dll -noanalysis -preScript SetSigs.java -postScript DumpC.java <out>`，再跑 decomp_strings.py）。
  - 主角的錢、身上道具跟劇情同步（story_begin/story_end）；開場裝備改由劇情的扶苏（UI_Pos 32、31）給。
- **劇情看得到 inv（2026-11-14）**：Item.sh = 劇情那邊的道具物件。story_begin 把 inv 每件放回主角物件身上（held：物件留著給 find_item／find_in_bag 找別名、move_object 0x1004bcf0 拿走），story_end 把被拿走的從 inv 移除（道具跟著物件走）、新給的收進 inv。item_new 照原版把物件別名設成道具表別名（劇情再 set_alias 會蓋掉；所以萍儿找 "fachai" 找不到改名成 fachaiyaoshi 的发钗，原版就是這樣）。add_exp 0x1003d270 → hero_gain_exp，quest_log 0x1004dd00 → hud_say。storytest 可帶 `give=物件路徑`、`flag=旗標=值`。
- **打造、镶嵌（2026-11-14）**：交易介面的 m_pMake／m_pEnchase 開 JM56_打造界面（Poq CMakePropDialog 0x540230，模式 −1 打造、1 镶嵌）。配方 = `ObjMdl/MakeMdl.txt`（XOR）：类别、物品類型、木矿骨皮筋件數、主材、文字ID（UI_C.dlg）。打造規則 EasyRPG produce 0x10051530（NPC：等級 = UIS_Produce、錢 = 材料 ValueUn 總和；材料特性合併後加到 item_new 的產品上，價格 ×1.2）；镶嵌 addgem 0x10037690（武器去防禦類、防具去攻擊類特性，價格 + 寶石價 ×1.5，SlotMax 上限）、拆除 removegem 0x10053a40（寶石毀掉）。Item 多了 gem_n/gem_icon/gem_value，ItemFx.gem 記是哪顆寶石的。測試網址可加 `item=類型,等級`（可多個）、`money=N`。沒做：自己用技能打造、五行換算 0x10058900、m_pMakerInfo。
- **寄存（2026-11-14）**：商人選項叫 0x1004ec80 → 0x61b → JM73_寄存界面（40 格）在左、道具在右。全國共用一個寄存箱（main.c `store[40]`）。原版寄存道具存在哪還沒查。
- **還沒做**：成套武装（JM55 的 m_pCombo「weaponset #主角 null 各件」、m_Asunder「weaponset #主角 #套裝」；EasyRPG 0x1001eaa0 把身上的裝備移進一件「秦殇/道具/成套武装」，套裝放 UI_Pos 40 = 第 11 個裝備欄，裡面的道具保留原本的 UI_Pos。穿脫套裝 0x1004cc5c 那段把 30~40 都當裝備欄，套裝內的道具怎麼算數值還沒查）。待查：原版說明文字（0x100279b0）、can_eat 設在哪、拖放規則、地上道具畫法、超重效果、箭的消耗、系統訊息字串表。


## 存檔（原版 JM50_内部主界面 → JM62_存取盘界面）

- 主列 m_pSystem 或 Esc 開系統選單（JM50）：保存、读取 → JM62；返回游戏、关闭；退出 → JM102 確認（還沒有標題畫面：網頁重新整理、電腦版關掉）。设置、帮助還沒做。
- JM62（Poq.exe CLoadDialog 0x56c400，畫 0x56ce20，換頁 0x56cce0）：一頁 6 格、最多 16 頁，m_pTitle 第 0 幀保存、第 1 幀读取，m_pScene 的「秦殇」圖是空格、有檔換成縮圖（開系統選單時截圖，原版 0x535ec0 也是先截）。第 0 頁前兩格 = 自动存盘（每次走出口存，原版只在大場景之間）、快速存盘（F9）。覆蓋問 UI_C 1535、刪除問 1534（JM67 底圖 + 借 JM102 是／否）；战斗中不能存 1507、打造視窗開著不能存 1533。
- 檔案是自己的格式（原版是整個 EasyRPG 世界序列化）：SaveHead（場景別名、房間名、時間、等級、128×96 縮圖）+ SaveHero（位置、等級、經驗、HP/MP、六屬性、點數、錢、職業、兩手技能、技能等級）+ inv + store + story_save（所有劇情物件依代號順序：類別路徑、名稱、別名、parent、held、ints/flags/strs、道具整塊）。網頁版放 IndexedDB（IDBFS 掛 /save，存完 syncfs），電腦版 ./save/。
- 讀檔在主迴圈做（load_due）：換掉主角、背包、劇情 → 進存檔的場景 → 放回位置、數值 → 重找房間、重放怪物。
- 沒存的：地上的道具、狀態（buff）、各場景怪物的狀態、每格的隊伍頭像（ResID1~5）。
- 已測（網頁）：保存、重新整理後清單還在、讀回位置、錢、背包；快速存盘。沒測：刪除、覆蓋確認、自动存盘、劇情任務進度讀回後的對話。
