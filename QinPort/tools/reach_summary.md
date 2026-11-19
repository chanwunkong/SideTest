# 可達性分析（skip 函數是否在單機會執行到）

由 `tools/reach.py` 產生；輸出 `tools/reach.tsv`。等級：**Y** 嚴格可達（直接呼叫／匯入／被建構類別的已被虛擬呼叫格）；**?** 只靠寬鬆邊（類別已建構但該格沒看到虛擬呼叫、資料表函式指標、DLL 未被 Poq 匯入的匯出）——依規則當成單機可達；**N-net** 只有連線程式碼（網路類別、WinSock 函數、`DAT_00942f28==2` 區塊）走得到；**N-dead** 圖上完全沒路。

## 統計

| 等級 | 函數數 | 行數 |
|---|---|---|
| Y | 4202 | 185172 |
| ? | 750 | 39578 |
| N-net | 246 | 10795 |
| N-dead | 754 | 25889 |
| 合計 | 5952 | 261434 |

單機可達（Y+?）：**4952 個、224750 行**；不可達（N-net+N-dead）：1000 個、36684 行。

根：嚴格 264、寬鬆 6583；資料區函式指標（嚴格=連續初始化陣列／寬鬆=其他）：{'Poq.exe': (30, 3307), 'EasyRPG.dll': (52, 2979), 'UIWndEx.dll': (174, 41)}；網路節點 {'Poq.exe': 205, 'EasyRPG.dll': 0, 'UIWndEx.dll': 0}。

圖的已知缺口：函式指標呼叫 `(*(code *)x)()` 共 174 處沒解開（只靠「FUN_x 被引用」和資料表補）；Ghidra 對 WS2_32／BNNet 匯入只顯示 `Ordinal_N`，網路函數辨識只用類別名與具名 WinSock 呼叫；MFC 訊息表只能靠資料區指標掃描（寬鬆根）；虛擬呼叫以「格號＋已建構類別」近似（沒有型別資訊）。

## 依類別／位址段的可達 skip（每組前 20）

### 類別（行數前 40 組）

#### Poq.exe (無類別)：1970 個 / 101229 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0050c390 | FUN_0050c390 | 2409 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 004074ff -thunk-> 0050c390 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 004774d0 | FUN_004774d0 | 2110 | Y | 004023a1 --> … -call-> 004039db -call-> 004017f8 -call-> 0046c030 -call-> 00408652 -thunk-> 004774d0 | " |
| 0052f020 | FUN_0052f020 | 871 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 004071b7 -thunk-> 0052f020 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 005213c0 | FUN_005213c0 | 859 | Y | 004023a1 --> … -call-> 00408fd0 -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -thunk-> 005213c0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0051e160 | FUN_0051e160 | 832 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 005174d0 -call-> 004026b2 -thunk-> 0051e160 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00522f00 | FUN_00522f00 | 720 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 00405123 -thunk-> 00522f00 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00512220 | FUN_00512220 | 692 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 005174d0 -call-> 00405b73 -thunk-> 00512220 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0051a8e0 | FUN_0051a8e0 | 585 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 005174d0 -call-> 0040169f -thunk-> 0051a8e0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 004c3af0 | FUN_004c3af0 | 533 | Y | 005a7b99 -call-> 0040584e -call-> 004c3af0 | 工具函式庫區（記憶體鏈 CMemChain、CDMList 容器、CIniFile 寫入、CDlgFile/CDlgTe |
| 004fb0d0 | FUN_004fb0d0 | 502 | Y | 00402522 --> … -virt#1-> 00403c92 -call-> 00500720 -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 | 遊戲外殼層：開啟各 JM 視窗的輔助（建立對話框物件、擺位置、記遠端同步資料 GroundData／TradeData／ |
| 004ba310 | FUN_004ba310 | 448 | Y | 004029a0 --> … -call-> 004de570 -call-> 00404efd -call-> 00499f10 -vt:CPropObj#1-> 00405010 -call-> 004ba310 | 工具函式庫區（記憶體鏈 CMemChain、CDMList 容器、CIniFile 寫入、CDlgFile/CDlgTe |
| 00554570 | FUN_00554570 | 448 | Y | 004023a1 --> … -call-> 0055f120 -call-> 00555270 -call-> 00554370 -call-> 0040858a -thunk-> 00554570 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0052b600 | FUN_0052b600 | 446 | Y | 004023a1 --> … -call-> 00408fd0 -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 0052b600 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00561720 | FUN_00561720 | 423 | Y | 004023a1 -call-> 00547720 -call-> 00402e87 -call-> 0055f120 -call-> 004091d8 -call-> 00561720 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 004f6ca0 | FUN_004f6ca0 | 401 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00403ce7 -call-> 0051c450 -call-> 004f6ca0 | 遊戲外殼層：開啟各 JM 視窗的輔助（建立對話框物件、擺位置、記遠端同步資料 GroundData／TradeData／ |
| 00543f30 | FUN_00543f30 | 393 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 00403f3a -call-> 00543f30 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00551c70 | FUN_00551c70 | 376 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00460e10 -call-> 00404cff -call-> 00551c70 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 004f9750 | FUN_004f9750 | 366 | Y | 00402522 --> … -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 00403503 -thunk-> 004f9750 | 遊戲外殼層：開啟各 JM 視窗的輔助（建立對話框物件、擺位置、記遠端同步資料 GroundData／TradeData／ |
| 004f3200 | FUN_004f3200 | 348 | Y | 004029a0 --> … -vt:CResMng#0-> 00401307 -call-> 004f80f0 -call-> 00548550 -vt:CMessageDialog#0-> 004018c0 -call-> 004f3200 | 連線版／平台層：CNetGame、CNetItem（socket、執行緒、封包收送）、CUserInfo 伺服器使用者權 |
| 00510310 | FUN_00510310 | 343 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 004074ff -call-> 00510310 | 客戶端顯示資料表／提示文字／字串清單合併：規則在 main.c 的 item／hero 計算，引擎文字另組 |

#### EasyRPG.dll (無類別)：858 個 / 49046 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 1007c6c0 | FUN_1007c6c0 | 460 | Y | 0040499e --> … -call-> 00401555 -import-> Easy:1005ff00 -call-> Easy:1007c5f0 -call-> Easy:1007c3c0 -call-> Easy:1007c6c0 | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日 |
| 10030c90 | FUN_10030c90 | 408 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:1002e040 -call-> Easy:1002e060 -call-> Easy:1002d8d0 -call-> Easy:10030c90 | 屬性變動通知／相依順序重算（Str Con→BD_MAX HP_SPEED、Dex→HR SR、Wisdom→MP_SP |
| 1007b490 | FUN_1007b490 | 388 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10003dc0 -call-> Easy:1007b490 | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日 |
| 1007d3c0 | FUN_1007d3c0 | 325 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:1007d3c0 | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日 |
| 100274a0 | FUN_100274a0 | 283 | Y | 004023a1 --> … -call-> Easy:100032c0 -call-> Easy:1004fd90 -call-> Easy:100449f0 -call-> Easy:10046d00 -call-> Easy:100274a0 | 技能名／職業技能表的說明文字輔助（G_Info 組字用），隨說明文字 0x100279b0 一起，引擎沒有滑鼠停留說明 |
| 1004fd90 | FUN_1004fd90 | 263 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1004fd90 | 連線版屬性起始（IsActorNPC 隊友 Str0 Con0… 隨機加點，NetType > 1 才呼叫 0x1004 |
| 10040970 | FUN_10040970 | 252 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:100538d0 -call-> Easy:1004c980 -call-> Easy:1004df20 -call-> Easy:1004a450 -call-> Easy:10040970 | 客戶端同步送出器（人物、道具、狀態、日誌、UI_Pos 通知，封包 0x…）：單機共用物件、引擎直接讀 |
| 10005290 | USER_Save | 234 | Y | 004023a1 --> … -call-> 004048db -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004d00 -call-> Easy:10005290 | 用戶帳號／存檔檔案／幫派／對話編輯器／字串檔案工具（連線版或原版工具程式，單機沒有；存檔走 engine 自己的格式） |
| 10003dc0 | USER_Load | 227 | Y | 004023a1 --> … -call-> 004048db -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10003dc0 | 用戶帳號／存檔檔案／幫派／對話編輯器／字串檔案工具（連線版或原版工具程式，單機沒有；存檔走 engine 自己的格式） |
| 10036520 | FUN_10036520 | 226 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:10055d50 -call-> Easy:100263f0 -call-> Easy:100279b0 -call-> Easy:10036520 | EasyRPG.dll 只被死碼（沒有參照的函數）呼叫，也是死碼，不移植 |
| 10043770 | FUN_10043770 | 217 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:100538d0 -call-> Easy:1004c980 -call-> Easy:1004df20 -call-> Easy:1004a450 -call-> Easy:10043770 | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0 |
| 100488d0 | FUN_100488d0 | 216 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:10049e30 -call-> Easy:100488d0 | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0 |
| 10049e30 | FUN_10049e30 | 212 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:10049e30 | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0 |
| 10031f80 | FUN_10031f80 | 201 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:10055d50 -call-> Easy:100263f0 -call-> Easy:100279b0 -call-> Easy:10031f80 | 技能參數字串化／狀態條目 Status_N 字串（"%d %d %d"）的寫入：引擎 Actor.buff[] 取代 |
| 1004a450 | FUN_1004a450 | 196 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:100538d0 -call-> Easy:1004c980 -call-> Easy:1004df20 -call-> Easy:1004a450 | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0 |
| 100032c0 | FUN_100032c0 | 172 | Y | 004023a1 --> … -call-> 004048db -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 | 用戶帳號／存檔檔案／幫派／對話編輯器／字串檔案工具（連線版或原版工具程式，單機沒有；存檔走 engine 自己的格式） |
| 10043480 | FUN_10043480 | 172 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:100538d0 -call-> Easy:1004c980 -call-> Easy:1004df20 -call-> Easy:1004a450 -call-> Easy:10043480 | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0 |
| 1005a120 | FUN_1005a120 | 169 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:10055b50 -call-> Easy:1003e920 -call-> Easy:10057230 -call-> Easy:1005a120 | 物件表／容器鏈／幫手（DAT_104797f0 物件表、新增刪除、鏈結串列節點）：story_rt.c objs[] 取 |
| 1004d790 | FUN_1004d790 | 168 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10003dc0 -call-> Easy:1007b490 -call-> Easy:1007be30 -call-> Easy:1004d790 | 客戶端同步送出器（人物、道具、狀態、日誌、UI_Pos 通知，封包 0x…）：單機共用物件、引擎直接讀 |
| 100058d0 | USER_TestCanBeWizard | 162 | Y | 0041bd90 --> … -call-> 0046de80 -vt:CScnClient#3-> 004055d3 -call-> 004046b5 -call-> 004edff0 -import-> Easy:100058d0 | 用戶帳號／存檔檔案／幫派／對話編輯器／字串檔案工具（連線版或原版工具程式，單機沒有；存檔走 engine 自己的格式） |

#### Poq.exe CGameApp：550 個 / 27076 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 004add10 | FUN_004add10 | 623 | Y | 004023a1 --> … -call-> 00404e35 -call-> 004019bf -call-> 0040495d -call-> 00402171 -call-> 004add10 | STL vector／variant（JSON 式 9 位元組值）樣板函式：STL／連線版 JSON 容器，不移植 |
| 004b0d60 | FUN_004b0d60 | 460 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00404e35 -call-> 004019bf -thunk-> 004b0d60 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004acfa0 | FUN_004acfa0 | 450 | Y | 004023a1 --> … -call-> 004019bf -call-> 0040495d -call-> 00402171 -call-> 0040230b -thunk-> 004acfa0 | STL vector／variant（JSON 式 9 位元組值）樣板函式：STL／連線版 JSON 容器，不移植 |
| 004b6e10 | FUN_004b6e10 | 435 | Y | 004023a1 --> … -call-> 0040495d -call-> 004af210 -call-> 004b6440 -call-> 00404d86 -thunk-> 004b6e10 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式：不移植 |
| 004b6800 | FUN_004b6800 | 310 | Y | 004023a1 --> … -call-> 0040495d -call-> 004af210 -call-> 004b6440 -call-> 0040755e -thunk-> 004b6800 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式：不移植 |
| 004b3390 | FUN_004b3390 | 302 | Y | 004023a1 --> … -call-> 00404e35 -call-> 004019bf -call-> 0040495d -call-> 00402171 -call-> 004b3390 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式：不移植 |
| 004b4530 | FUN_004b4530 | 294 | Y | 004023a1 --> … -call-> 004019bf -call-> 0040495d -call-> 00402171 -call-> 00404485 -thunk-> 004b4530 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式：不移植 |
| 005a2630 | FUN_005a2630 | 281 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00578cb0 -call-> 00405e70 -call-> 00407b03 -thunk-> 005a2630 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004646b0 | FUN_004646b0 | 240 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00408ae4 -call-> 00402b53 -thunk-> 004646b0 | 建構／平台：libVLC 播放器包裝初始化 instance、player 與回呼 shared_ptr；失敗拋 run |
| 004f8a30 | FUN_004f8a30 | 228 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00404a89 -call-> 0044d470 -call-> 004f8a30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004629c0 | FUN_004629c0 | 224 | Y | 004023a1 --> … -call-> 00408b89 -vt:CAirBeastie#9-> 00406b1d -call-> 00461bd0 -vt:CGameApp#22-> 00403134 -thunk-> 004629c0 | 平台：MFC InitInstance（記憶體／COM／命令列啟動模式／互斥鎖／安裝登錄檔／MMX／Win32計時器）； |
| 004494e0 | FUN_004494e0 | 212 | Y | 004029a0 -call-> 0044e060 -call-> 004fe8a0 -virt#7-> 00402a3b -call-> 0044a7a0 -call-> 0040371f -thunk-> 004494e0 | 平台：列舉 Direct3D 顯示模式（std::vector／std::map） |
| 00448d20 | FUN_00448d20 | 211 | Y | 0041bd90 --> … -vt:CCreateConfraternityDialog#55-> 004073ab -thunk-> 00546c10 -call-> 005326a0 -call-> 004493a0 -call-> 00448d20 | 平台：切換顯示模式（解析度表 0=640x480x8 … 6=1024x768、7=1280x720、8=1920x10 |
| 004b5a90 | FUN_004b5a90 | 211 | Y | 004023a1 --> … -call-> 00404e35 -call-> 004019bf -call-> 0040495d -call-> 00402171 -thunk-> 004b5a90 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式：不移植 |
| 00461fb0 | FUN_00461fb0 | 209 | Y | 004029a0 --> … -call-> 004750e0 -call-> 004010a5 -call-> 00407036 -call-> 004069bf -call-> 00461fb0 | 沒有呼叫者（FUNCS.tsv callers=0）；不移植此舊啟動路徑 |
| 00466f30 | FUN_00466f30 | 205 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00405178 -call-> 00476770 -call-> 00466e30 -call-> 00466f30 | 客戶端同步層：依角色與第二名單視野聯集重掃場景格，建立客戶端可見物件；原版伺服端到客戶端的可見物件複製，單機共用物件不另 |
| 0057e5c0 | FUN_0057e5c0 | 203 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00405178 -call-> 00404511 -call-> 0057e5c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004769c0 | FUN_004769c0 | 200 | Y | 004023a1 --> … -call-> 0046b320 -call-> 004043c7 -call-> 004df020 -call-> 0046a6b0 -call-> 004769c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059fa30 | FUN_0059fa30 | 189 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -call-> 00408ad5 -call-> 004067c6 -thunk-> 0059fa30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0046b320 | FUN_0046b320 | 185 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00449f50 -call-> 004038fa -call-> 0046b320 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CEditBox：41 個 / 3161 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00590f80 | FUN_00590f80 | 639 | Y | 004023a1 --> … -call-> 00406451 -call-> 0058c190 -vt:CEditBox#5-> 004086d4 -virt#52-> 0040594d -thunk-> 00590f80 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058f640 | FUN_0058f640 | 211 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#12-> 004023c9 -thunk-> 0058f640 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00592070 | FUN_00592070 | 155 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#5-> 004086d4 -thunk-> 00592070 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00590940 | FUN_00590940 | 144 | Y | 004023a1 --> … -call-> 00406451 -call-> 0058c190 -vt:CEditBox#5-> 004086d4 -virt#52-> 0040594d -call-> 00590940 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00590190 | FUN_00590190 | 116 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#21-> 0040271b -thunk-> 00590190 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058e260 | FUN_0058e260 | 111 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#11-> 004017c6 -thunk-> 0058e260 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058fcb0 | FUN_0058fcb0 | 90 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#18-> 00407261 -thunk-> 0058fcb0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058e080 | FUN_0058e080 | 75 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#10-> 00406f4b -thunk-> 0058e080 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058ff60 | FUN_0058ff60 | 67 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#24-> 00403080 -thunk-> 0058ff60 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00590e70 | FUN_00590e70 | 45 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004053a3 -thunk-> 00590e70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058f550 | FUN_0058f550 | 39 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#32-> 00404f7f -thunk-> 0058f550 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00590510 | FUN_00590510 | 38 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#14-> 00404782 -thunk-> 00590510 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00590dc0 | FUN_00590dc0 | 34 | Y | 004029a0 --> … -vt:CEquipDialog#55-> 00405a1f -call-> 00444e10 -vt:CRAIN#4-> 00401c3a -call-> 00406735 -call-> 00590dc0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058c4b0 | FUN_0058c4b0 | 33 | Y | 004029a0 --> … -call-> 00406aa5 -vt:CEquipDialog#55-> 00405a1f -call-> 00444e10 -vt:CRAIN#4-> 00401c3a -call-> 0058c4b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058e5c0 | FUN_0058e5c0 | 30 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#22-> 0040402a -thunk-> 0058e5c0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058c580 | FUN_0058c580 | 26 | Y | 004023a1 -call-> 005585e0 -call-> 00511a80 -call-> 004fa340 -call-> 00407982 -call-> 0058c580 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058f4c0 | FUN_0058f4c0 | 25 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#37-> 00404b42 -thunk-> 0058f4c0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00590d60 | FUN_00590d60 | 24 | Y | 004023a1 --> … -call-> 00408fd0 -call-> 00404e35 -call-> 004045e3 -call-> 00408b66 -call-> 00590d60 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058c3f0 | FUN_0058c3f0 | 23 | Y | 004023a1 -call-> 005585e0 -call-> 00511a80 -call-> 004fa340 -call-> 00407982 -call-> 0058c580 -call-> 0058c3f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058c440 | FUN_0058c440 | 23 | Y | 004023a1 -call-> 005585e0 -call-> 00511a80 -call-> 004fa340 -call-> 00407982 -call-> 0058c580 -call-> 0058c440 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### UIWndEx.dll (無類別)：58 個 / 2236 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 10001bb0 | FUN_10001bb0 | 38 | Y | UIWn:10001bb0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001cb0 | FUN_10001cb0 | 31 | Y | UIWn:10001cb0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10002af0 | FUN_10002af0 | 26 | Y | UIWn:10002af0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 100032a0 | FUN_100032a0 | 24 | Y | UIWn:100032a0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 100033d0 | FUN_100033d0 | 22 | Y | UIWn:100032a0 -call-> UIWn:100033d0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10002ab0 | FUN_10002ab0 | 17 | Y | UIWn:10002ab0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10003350 | FUN_10003350 | 17 | Y | UIWn:10003350 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001ac0 | FUN_10001ac0 | 16 | Y | UIWn:10001ac0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10003390 | FUN_10003390 | 16 | Y | UIWn:10003390 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10003bf0 | FUN_10003bf0 | 16 | Y | UIWn:10003bf0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b00 | FUN_10001b00 | 9 | Y | UIWn:10001b00 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10003330 | FUN_10003330 | 9 | Y | UIWn:10003330 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10003340 | FUN_10003340 | 9 | Y | UIWn:10003340 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001ab0 | FUN_10001ab0 | 8 | Y | UIWn:10001ab0 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b10 | FUN_10001b10 | 8 | Y | UIWn:10001b10 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b20 | FUN_10001b20 | 8 | Y | UIWn:10001b20 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b30 | FUN_10001b30 | 8 | Y | UIWn:10001b30 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b40 | FUN_10001b40 | 8 | Y | UIWn:10001b40 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b50 | FUN_10001b50 | 8 | Y | UIWn:10001b50 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |
| 10001b60 | FUN_10001b60 | 8 | Y | UIWn:10001b60 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWND 對話框，開線上頁 |

#### Poq.exe CCharactor：39 個 / 2119 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00575f10 | FUN_00575f10 | 436 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0043e100 -call-> 00404a8e -call-> 00575f10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00577580 | FUN_00577580 | 333 | Y | 0040499e --> … -call-> 00403f3a -call-> 005508f0 -call-> 0040688e -call-> 004016bd -thunk-> 00577580 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00576e70 | FUN_00576e70 | 122 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0043e100 -call-> 00404a52 -call-> 00576e70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00487d30 | FUN_00487d30 | 89 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#19-> 00401735 -call-> 00487d30 | 索引無呼叫者；本輪依規則略過，保留原版入口供後續接線參考 |
| 00577170 | FUN_00577170 | 80 | Y | 0040499e --> … -call-> 00405178 -call-> 00404c23 -call-> 004019b0 -vt:CFlyPartDeadObj#13-> 0040329c -call-> 00577170 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0048f630 | FUN_0048f630 | 76 | Y | 005a7b99 --> … -call-> 0053eee0 -call-> 00543810 -call-> 00598c10 -call-> 00408daa -call-> 0048f630 | 索引無呼叫者；本輪依規則略過，保留原版入口供後續接線參考 |
| 0048fde0 | FUN_0048fde0 | 63 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#0-> 004017da -call-> 0048fde0 | 索引無呼叫者；本輪依規則略過，保留原版入口供後續接線參考 |
| 004859d0 | FUN_004859d0 | 61 | Y | 0040499e --> … -call-> 00493430 -call-> 004902c0 -call-> 004856e0 -vt:CCharactor#0-> 00401fe6 -thunk-> 004859d0 | CCharactor刪除解構；同一般解構加operator delete，以Actor陣列生命週期取代 |
| 00576c70 | FUN_00576c70 | 61 | Y | 00402522 --> … -call-> 004066bd -call-> 004f1ee0 -call-> 004ed440 -call-> 00401b3b -call-> 00576c70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00576d70 | FUN_00576d70 | 61 | Y | 004023a1 --> … -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -virt#51-> 004072ca -call-> 00576d70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00577440 | FUN_00577440 | 61 | Y | 004023a1 --> … -call-> 00402171 -call-> 0040230b -call-> 004015d2 -call-> 00407333 -call-> 00577440 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0048bca0 | FUN_0048bca0 | 58 | Y | 0040499e --> … -call-> 00466f30 -call-> 00465fb0 -call-> 00465c60 -call-> 0046d580 -call-> 0048bca0 | 純傳輸：逐一將狀態byte編成0x523封包廣播或發給指定接收者；單機共用Actor，不建立同步副本 |
| 00575d40 | FUN_00575d40 | 55 | Y | 004023a1 --> … -call-> 0040634d -call-> 0055ea10 -call-> 0040345e -call-> 00403e0e -call-> 00575d40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00486d30 | FUN_00486d30 | 48 | Y | 0040499e --> … -call-> 00407f59 -call-> 0040702c -call-> 00493430 -call-> 004902c0 -call-> 00486d30 | CCharactor卸載清理：清字串、三個附加精靈、技能鏈與浮字陣列；以場景／Actor陣列生命週期承接 |
| 00577390 | FUN_00577390 | 46 | Y | 004023a1 --> … -vt:CLoadDialog#9-> 004038f5 -virt#6-> 00401e56 -call-> 00406221 -call-> 00402586 -thunk-> 00577390 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0048bdd0 | FUN_0048bdd0 | 44 | Y | 0040499e --> … -call-> 00465fb0 -call-> 00465c60 -call-> 0046d580 -call-> 00402ccf -thunk-> 0048bdd0 | 純傳輸：逐一將狀態byte編成0x523封包送本機客戶端；單機共用Actor，不另複製狀態 |
| 00576a50 | FUN_00576a50 | 41 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0043e100 -call-> 00578a00 -call-> 00576a50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00576c00 | FUN_00576c00 | 40 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0043e100 -call-> 00404a52 -call-> 00404651 -thunk-> 00576c00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0048f130 | FUN_0048f130 | 39 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00405178 -call-> 00404c23 -call-> 004069ab -call-> 0048f130 | 索引無呼叫者；本輪依規則略過，保留原版入口供後續接線參考 |
| 00575e50 | FUN_00575e50 | 35 | Y | 004029a0 -call-> 00402b21 -call-> 00558bc0 -call-> 0054f6c0 -call-> 004029e6 -call-> 00403bfc -thunk-> 00575e50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### EasyRPG.dll CERObject：71 個 / 1978 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 10022db0 | FUN_10022db0 | 172 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#3-> Easy:10022db0 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 10023950 | FUN_10023950 | 128 | Y | 004023a1 --> … -import-> Easy:10004d00 -call-> Easy:10005290 -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:10023950 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 10020850 | FUN_10020850 | 110 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10006030 -call-> Easy:10050630 -virt#9-> Easy:10021de0 -call-> Easy:10020850 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10021de0 | FUN_10021de0 | 95 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10006030 -call-> Easy:10050630 -virt#9-> Easy:10021de0 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 1000da00 | FUN_1000da00 | 93 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:10055d50 -call-> Easy:100263f0 -call-> Easy:100279b0 -call-> Easy:1000da00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10020610 | FUN_10020610 | 76 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#0-> Easy:10020610 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 1000d6e0 | FUN_1000d6e0 | 63 | Y | 004023a1 --> … -call-> Easy:10003dc0 -call-> Easy:1007b490 -call-> Easy:1007be30 -call-> Easy:1004d790 -call-> Easy:1000d6e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1000dc50 | FUN_1000dc50 | 61 | Y | 004023a1 --> … -call-> Easy:10005290 -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:10023950 -call-> Easy:1000dc50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10020c20 | FUN_10020c20 | 58 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:1002e040 -call-> Easy:1002e060 -call-> Easy:1003f910 -call-> Easy:10020c20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 100229e0 | FUN_100229e0 | 55 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#5-> Easy:100229e0 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 10020b40 | FUN_10020b40 | 49 | Y | Easy:10025260 -call-> Easy:10023410 -call-> Easy:10020b40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10022080 | FUN_10022080 | 47 | Y | Easy:10024e40 -call-> Easy:10022080 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1000d5e0 | FUN_1000d5e0 | 41 | Y | Easy:10025260 -call-> Easy:10047870 -call-> Easy:1000d5e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 100209f0 | FUN_100209f0 | 34 | Y | Easy:10024e40 --> … -call-> Easy:1004c980 -call-> Easy:1004df20 -call-> Easy:10059020 -call-> Easy:1005b5a0 -call-> Easy:100209f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10023ef0 | FUN_10023ef0 | 31 | Y | 004023a1 --> … -call-> Easy:10006180 -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#3-> Easy:10022db0 -call-> Easy:10023ef0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10023f90 | FUN_10023f90 | 31 | Y | 004023a1 --> … -call-> Easy:10005290 -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:10023950 -call-> Easy:10023f90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1000d7c0 | FUN_1000d7c0 | 24 | Y | 004023a1 --> … -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#3-> Easy:10022db0 -call-> Easy:10023ef0 -call-> Easy:1000d7c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1000d800 | FUN_1000d800 | 24 | Y | 004023a1 --> … -call-> Easy:10003dc0 -call-> Easy:1007b490 -call-> Easy:1007be30 -call-> Easy:1004d790 -call-> Easy:1000d800 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10021fa0 | FUN_10021fa0 | 24 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -call-> Easy:1001fee0 -vt:CERObject#1-> Easy:10021fa0 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 0x10023550 回 0、 |
| 1000dd70 | FUN_1000dd70 | 20 | Y | 004023a1 --> … -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:10023950 -call-> Easy:10023f90 -call-> Easy:1000dd70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CGameDialog：40 個 / 1904 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00548a40 | FUN_00548a40 | 630 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00548a40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00459260 | FUN_00459260 | 214 | Y | 004023a1 --> … -vt:CGameDialog#28-> 004020e5 -call-> 00532800 -call-> 00454f00 -call-> 00401645 -thunk-> 00459260 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005384f0 | FUN_005384f0 | 165 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#37-> 00402dce -thunk-> 005384f0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0053c1c0 | FUN_0053c1c0 | 84 | Y | 004023a1 --> … -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -thunk-> 0053c1c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00504fc0 | FUN_00504fc0 | 79 | Y | 004023a1 --> … -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -thunk-> 00504fc0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00581a40 | FUN_00581a40 | 71 | Y | 004023a1 -call-> 00504d90 -call-> 005212f0 -virt#30-> 00402e5a -call-> 00581a40 | 連線版／除錯用介面 |
| 005498f0 | FUN_005498f0 | 58 | Y | 004023a1 --> … -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 004077f7 -thunk-> 005498f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005378c0 | FUN_005378c0 | 55 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00404a89 -call-> 0040792d -call-> 005378c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004556b0 | FUN_004556b0 | 42 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -call-> 00532800 -call-> 0040926e -thunk-> 004556b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00532800 | FUN_00532800 | 41 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -call-> 00532800 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00538eb0 | FUN_00538eb0 | 30 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#15-> 00402e9b -thunk-> 00538eb0 | CGameDialog 的關閉清理、顯示／隱藏全部 HUD、鍵盤事件轉發、連線版聊天輸入／輸出視窗焦點與格式化送字；單機 |
| 004584c0 | FUN_004584c0 | 28 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -call-> 00532800 -call-> 00408828 -call-> 004584c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00538df0 | FUN_00538df0 | 28 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#20-> 00404d95 -thunk-> 00538df0 | CGameDialog 的關閉清理、顯示／隱藏全部 HUD、鍵盤事件轉發、連線版聊天輸入／輸出視窗焦點與格式化送字；單機 |
| 00455580 | FUN_00455580 | 27 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -call-> 00532800 -call-> 00408828 -thunk-> 00455580 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00537ab0 | FUN_00537ab0 | 26 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#11-> 00404b47 -thunk-> 00537ab0 | CGameDialog 的關閉清理、顯示／隱藏全部 HUD、鍵盤事件轉發、連線版聊天輸入／輸出視窗焦點與格式化送字；單機 |
| 00502b30 | FUN_00502b30 | 25 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00537b40 | FUN_00537b40 | 21 | Y | 004023a1 --> … -call-> 004028c9 -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -thunk-> 00537b40 | CGameDialog 的關閉清理、顯示／隱藏全部 HUD、鍵盤事件轉發、連線版聊天輸入／輸出視窗焦點與格式化送字；單機 |
| 00538970 | FUN_00538970 | 21 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#42-> 00401f00 -thunk-> 00538970 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 005498b0 | FUN_005498b0 | 18 | Y | 004023a1 --> … -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#11-> 00404b47 -call-> 00404f75 -thunk-> 005498b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00455640 | FUN_00455640 | 17 | Y | 004023a1 --> … -call-> 004f56a0 -call-> 00401929 -vt:CGameDialog#28-> 004020e5 -call-> 00532800 -call-> 00455640 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CViewPort：33 個 / 1777 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0045f820 | FUN_0045f820 | 243 | Y | 0040499e --> … -vt:CDD#0-> 004040e8 -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 0045f820 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0045b2b0 | FUN_0045b2b0 | 160 | Y | 0040499e --> … -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 00409151 -thunk-> 0045b2b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0045dc00 | FUN_0045dc00 | 148 | Y | 0040499e --> … -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 00401ff0 -thunk-> 0045dc00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0045fec0 | FUN_0045fec0 | 148 | Y | 0040499e --> … -vt:CDD#0-> 004040e8 -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 0045fec0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0045e020 | FUN_0045e020 | 146 | Y | 0040499e --> … -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 004019ab -thunk-> 0045e020 | 平台：背景表面快取搬移與新增露出區域的髒矩形，SDL 每幀全畫面重畫代替 |
| 004c7a00 | FUN_004c7a00 | 140 | Y | 0040499e --> … -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 00409151 -call-> 00408deb -thunk-> 004c7a00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00461230 | FUN_00461230 | 114 | Y | 0040499e --> … -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 004039c2 -thunk-> 00461230 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004837d0 | FUN_004837d0 | 76 | Y | 004023a1 --> … -call-> 00404e35 -call-> 004019bf -call-> 004af940 -call-> 00402bcb -call-> 004837d0 | CBuilding底層以上全部部件裁切繪製；索引無呼叫者 |
| 0045ef90 | FUN_0045ef90 | 69 | Y | 0040499e --> … -vt:CDD#0-> 004040e8 -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#3-> 00408710 -thunk-> 0045ef90 | 平台：解析度切換與 DirectDraw 表面釋放／重建，SDL 視窗及 renderer 生命週期取代 |
| 0053c540 | FUN_0053c540 | 60 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00457ad0 -call-> 00457a30 -call-> 00405821 -call-> 0053c540 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004831c0 | FUN_004831c0 | 51 | Y | 0040499e --> … -call-> 00405178 -virt#15-> 00404881 -call-> 0047ece0 -call-> 00484ee0 -call-> 004831c0 | CBuilding類型150時比較門名但所有分支均直接返回；保留無效果，不自造開門邏輯 |
| 004fa3e0 | FUN_004fa3e0 | 47 | Y | 0040499e --> … -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 0045fec0 -call-> 00407cde -thunk-> 004fa3e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004832d0 | FUN_004832d0 | 42 | Y | 004029a0 --> … -call-> 0044f4c0 -call-> 004843a0 -vt:NormalBuilding#9-> 00402572 -call-> 004051be -thunk-> 004832d0 | CBuilding單部件動畫前後矩形加入DirectDraw髒區；SDL每幀重畫，原反編譯未呈現幀推進呼叫，不將此入口冒 |
| 004fa5d0 | FUN_004fa5d0 | 34 | Y | 004023a1 --> … -call-> 0057c450 -call-> 00408986 -call-> 0056ef30 -call-> 00402b76 -thunk-> 004fa5d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00482750 | FUN_00482750 | 27 | Y | 004023a1 --> … -call-> 0046b320 -call-> 00401c03 -call-> 005353c0 -call-> 00402b08 -call-> 00482750 | CBuilding全模型特殊部件繪製；索引無呼叫者，現用分層繪製路徑 |
| 004fa6d0 | FUN_004fa6d0 | 25 | Y | 0040499e --> … -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 00409151 -call-> 005722c0 -call-> 004fa6d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053dc60 | FUN_0053dc60 | 24 | Y | 0040499e --> … -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 0045fec0 -call-> 00407cde -call-> 0053dc60 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004611b0 | FUN_004611b0 | 22 | Y | 0040499e --> … -call-> 0045f820 -call-> 0045ec50 -call-> 00483b80 -call-> 00409228 -thunk-> 004611b0 | DirectDraw 背景表面延遲初始化／像素格式色鍵；SDL renderer 每幀重畫取代 |
| 0045ae10 | FUN_0045ae10 | 20 | Y | 0041bd90 -call-> 004059b1 -call-> 00405ccc -call-> 0043f350 -vt:CFootStep#12-> 0040736f -call-> 00498240 -call-> 0045ae10 | 平台：標記髒矩形／全畫面重畫，SDL 每幀全畫面重畫代替 |
| 004fa500 | FUN_004fa500 | 20 | Y | 0041bd90 --> … -call-> 0046de80 -vt:CScnClient#3-> 004055d3 -call-> 00402153 -call-> 00460570 -call-> 004fa500 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CLoadingDialog：72 個 / 1744 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00582360 | FUN_00582360 | 254 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#2-> 004062fd -virt#14-> 00407428 -thunk-> 00582360 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00589f00 | FUN_00589f00 | 115 | Y | 0041bd90 --> … -call-> 0044dda0 -call-> 0040305d -call-> 005855a0 -virt#54-> 0040426e -thunk-> 00589f00 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00582d00 | FUN_00582d00 | 104 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0044ca40 -call-> 004a6480 -virt#3-> 004085b7 -thunk-> 00582d00 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 005848e0 | FUN_005848e0 | 91 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#2-> 004062fd -thunk-> 005848e0 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00589b60 | FUN_00589b60 | 80 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#12-> 00406163 -thunk-> 00589b60 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 005459c0 | FUN_005459c0 | 62 | Y | 004029a0 --> … -call-> 0044dbd0 -call-> 0040902f -call-> 005021c0 -vt:CLoadingDialog#55-> 004026f3 -call-> 005459c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005898d0 | FUN_005898d0 | 53 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -thunk-> 005898d0 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00582180 | FUN_00582180 | 51 | Y | 004023a1 --> … -virt#6-> 00402f27 -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -thunk-> 00582180 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00589e10 | FUN_00589e10 | 45 | Y | 0041bd90 --> … -call-> 005852e0 -call-> 004072c5 -vt:CUIManager#6-> 00403cf6 -virt#49-> 00408c4c -thunk-> 00589e10 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00588a70 | FUN_00588a70 | 43 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 0040483b -thunk-> 00588a70 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00589d00 | FUN_00589d00 | 36 | Y | 004023a1 -call-> 005585e0 -call-> 00406465 -virt#20-> 004016b8 -thunk-> 00589d00 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 005a09f0 | FUN_005a09f0 | 32 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 005a09f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00589830 | FUN_00589830 | 31 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00401ce9 -virt#26-> 0040823d -thunk-> 00589830 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00585310 | FUN_00585310 | 30 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00585310 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00520f60 | FUN_00520f60 | 29 | Y | 004023a1 -call-> 00504d90 -call-> 005212f0 -virt#30-> 00402e5a -thunk-> 00520f60 | CGameDialog 基底殼：視窗旗標位置設定、顯示／隱藏、封包欄位取出、清單釋放；引擎 JM 視窗取代 |
| 00581cc0 | FUN_00581cc0 | 29 | Y | 004023a1 --> … -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -virt#47-> 00407c43 -thunk-> 00581cc0 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00502370 | FUN_00502370 | 28 | Y | 004029a0 --> … -call-> 0044dbd0 -call-> 0040902f -call-> 005021c0 -vt:CLoadingDialog#11-> 00406898 -thunk-> 00502370 | CLoadingDialog／CGameInfoDialog 殼：載入視窗轉接、清除、取封包欄位、視窗開關旗標；無遊戲規 |
| 005814c0 | FUN_005814c0 | 25 | Y | 004023a1 -call-> 00504d90 -call-> 004041a6 -thunk-> 005814c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005820f0 | FUN_005820f0 | 23 | Y | 004023a1 -call-> 00504d90 -call-> 00583790 -virt#25-> 00408dd7 -thunk-> 005820f0 | UI 工具箱／平台層：CInterfaceBase 與控制項基底、JM 介面腳本解析（ |
| 00593670 | FUN_00593670 | 23 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CPushbutton：30 個 / 1303 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0058ae00 | FUN_0058ae00 | 254 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#37-> 00403670 -thunk-> 0058ae00 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058b4c0 | FUN_0058b4c0 | 115 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#12-> 00403b34 -thunk-> 0058b4c0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058bca0 | FUN_0058bca0 | 74 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#14-> 00408887 -thunk-> 0058bca0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058b390 | FUN_0058b390 | 64 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#43-> 004058c1 -thunk-> 0058b390 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058baa0 | FUN_0058baa0 | 61 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#1-> 0040479b -thunk-> 0058baa0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058a380 | FUN_0058a380 | 49 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058a380 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058b830 | FUN_0058b830 | 39 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#39-> 004050ab -thunk-> 0058b830 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058ad30 | FUN_0058ad30 | 38 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#35-> 00406a00 -thunk-> 0058ad30 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058b910 | FUN_0058b910 | 37 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#42-> 004028e2 -thunk-> 0058b910 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058ba00 | FUN_0058ba00 | 37 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#20-> 0040349f -thunk-> 0058ba00 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058a4c0 | FUN_0058a4c0 | 26 | Y | 004023a1 -call-> 00504d90 -call-> 0040135c -call-> 00403693 -thunk-> 0058a4c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058ac90 | FUN_0058ac90 | 23 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#36-> 0040249b -thunk-> 0058ac90 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058bbf0 | FUN_0058bbf0 | 23 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#21-> 004079f5 -thunk-> 0058bbf0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058a640 | FUN_0058a640 | 22 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#28-> 00403acb -thunk-> 0058a640 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058b7c0 | FUN_0058b7c0 | 20 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#40-> 004032ec -thunk-> 0058b7c0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058a5a0 | FUN_0058a5a0 | 17 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#27-> 00403427 -thunk-> 0058a5a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058ac20 | FUN_0058ac20 | 16 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#19-> 00404868 -thunk-> 0058ac20 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058ac50 | FUN_0058ac50 | 16 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#38-> 004085ee -thunk-> 0058ac50 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0058b780 | FUN_0058b780 | 16 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#41-> 00404e53 -thunk-> 0058b780 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059d770 | FUN_0059d770 | 16 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004045de -vt:CPushbutton#14-> 00408887 -call-> 0059d770 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CMapDialog：31 個 / 1232 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0053cf80 | FUN_0053cf80 | 299 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#13-> 00404afc -thunk-> 0053cf80 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0053d830 | FUN_0053d830 | 194 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#40-> 00404c5f -call-> 0053d830 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053d810 | FUN_0053d810 | 108 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#3-> 00408922 -thunk-> 0053d810 | CMapDialog 重畫：依髒矩形（GRECT 串列）逐一重畫子精靈，純 DirectDraw 局部重畫；引擎每幀整個 |
| 0053dd20 | FUN_0053dd20 | 86 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#36-> 00401064 -call-> 0053dd20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053c7a0 | FUN_0053c7a0 | 67 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#34-> 00403035 -thunk-> 0053c7a0 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 0045e4f0 | FUN_0045e4f0 | 61 | Y | 004023a1 --> … -vt:CMapDialog#36-> 00401064 -call-> 005325b0 -call-> 0044da50 -call-> 00403729 -thunk-> 0045e4f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053c9f0 | FUN_0053c9f0 | 51 | Y | 0040499e --> … -call-> 00403819 -call-> 00503d70 -call-> 004fe730 -call-> 00403a3f -thunk-> 0053c9f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00549780 | FUN_00549780 | 49 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#13-> 00404afc -call-> 00403d6e -thunk-> 00549780 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053cde0 | FUN_0053cde0 | 44 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#40-> 00404c5f -thunk-> 0053cde0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0045e710 | FUN_0045e710 | 32 | Y | 004023a1 --> … -call-> 00409160 -vt:CMapDialog#36-> 00401064 -call-> 00406fbe -call-> 00406e74 -call-> 0045e710 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053d750 | FUN_0053d750 | 27 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#15-> 00402a6d -thunk-> 0053d750 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0053c490 | FUN_0053c490 | 26 | Y | 00402522 --> … -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 004fa260 -call-> 004047eb -call-> 0053c490 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00532470 | FUN_00532470 | 14 | Y | 004023a1 --> … -call-> 004f7aa0 -call-> 00406e4c -call-> 0040760d -call-> 00539fa0 -call-> 00532470 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00449bd0 | FUN_00449bd0 | 13 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#13-> 00404afc -call-> 004078ce -call-> 00449bd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0044da50 | FUN_0044da50 | 13 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#36-> 00401064 -call-> 005325b0 -call-> 0044da50 | 轉接到 0x45e4f0（移植那邊時直接接） |
| 0044c800 | FUN_0044c800 | 12 | Y | 004023a1 --> … -call-> 00409160 -vt:CMapDialog#36-> 00401064 -call-> 00406fbe -call-> 00406e74 -thunk-> 0044c800 | 轉接到 0x45e710（移植那邊時直接接） |
| 005325f0 | FUN_005325f0 | 12 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#36-> 00401064 -call-> 0040774d -thunk-> 005325f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0053cb10 | FUN_0053cb10 | 10 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#11-> 004092ff -thunk-> 0053cb10 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00532460 | FUN_00532460 | 9 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#13-> 00404afc -call-> 004078ce -thunk-> 00532460 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005324b0 | FUN_005324b0 | 9 | Y | 00402522 --> … -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 00403503 -call-> 005324b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CToolTip：20 個 / 1169 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00594890 | FUN_00594890 | 236 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -call-> 0040369d -thunk-> 00594890 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00594060 | FUN_00594060 | 230 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -call-> 00594060 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005950a0 | FUN_005950a0 | 200 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -call-> 00403166 -thunk-> 005950a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00581d60 | FUN_00581d60 | 109 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -call-> 00407522 -vt:CGameBaseDialog#11-> 004042dc -call-> 004040b1 -thunk-> 00581d60 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00595980 | FUN_00595980 | 78 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#1-> 00403ffd -call-> 00595980 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00584a80 | FUN_00584a80 | 48 | Y | 004023a1 --> … -call-> 00406451 -virt#53-> 00402da1 -call-> 00594060 -call-> 00406870 -thunk-> 00584a80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00595890 | FUN_00595890 | 38 | Y | 004023a1 --> … -call-> 00406a1e -call-> 004071d5 -call-> 0053c120 -call-> 0040203b -thunk-> 00595890 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005957a0 | FUN_005957a0 | 36 | Y | 004023a1 --> … -call-> 00406451 -call-> 0058c190 -vt:CEditBox#5-> 004086d4 -virt#52-> 00406127 -thunk-> 005957a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00593fa0 | FUN_00593fa0 | 35 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00401ce9 -virt#26-> 0040823d -thunk-> 00589830 -call-> 00593fa0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593870 | FUN_00593870 | 27 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#1-> 00403ffd -thunk-> 00593870 | 介面類別 deleting destructor（vftable 還原、成員釋放；C 版 free／引擎生命週期） |
| 00595b50 | FUN_00595b50 | 23 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -call-> 00403166 -call-> 00595b50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00595930 | FUN_00595930 | 21 | Y | 004023a1 --> … -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#1-> 00403ffd -call-> 004026da -thunk-> 00595930 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593f30 | FUN_00593f30 | 20 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -virt#1-> 00407856 -call-> 00593f30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593ee0 | FUN_00593ee0 | 12 | Y | 004023a1 --> … -call-> 004038fa -call-> 0046b320 -call-> 00407dc4 -call-> 0040220c -call-> 00593ee0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593f00 | FUN_00593f00 | 12 | Y | 005a7b99 -call-> 005a776a -call-> 00408d46 -call-> 00593f00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00594020 | FUN_00594020 | 11 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -thunk-> 00594020 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00593eb0 | FUN_00593eb0 | 9 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 005174d0 -call-> 00405e9d -call-> 00593eb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00582320 | FUN_00582320 | 8 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#12-> 004013fc -thunk-> 00582320 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00582330 | FUN_00582330 | 8 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#20-> 00404566 -thunk-> 00582330 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00593e90 | FUN_00593e90 | 8 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#0-> 004048a9 -thunk-> 00593e90 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CEnumBox：23 個 / 1107 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0059a7f0 | FUN_0059a7f0 | 214 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#12-> 00404b33 -thunk-> 0059a7f0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00598cf0 | FUN_00598cf0 | 119 | Y | 00402522 --> … -call-> 004f1ee0 -call-> 004ed440 -call-> 00401b3b -call-> 005a13f0 -call-> 00598cf0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059aec0 | FUN_0059aec0 | 71 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#14-> 00406a32 -thunk-> 0059aec0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059a6a0 | FUN_0059a6a0 | 61 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#43-> 00405b64 -thunk-> 0059a6a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059a460 | FUN_0059a460 | 58 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#36-> 00404d13 -thunk-> 0059a460 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059a090 | FUN_0059a090 | 57 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#11-> 004088ff -thunk-> 0059a090 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00599f90 | FUN_00599f90 | 48 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#10-> 0040100f -thunk-> 00599f90 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059ad80 | FUN_0059ad80 | 47 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#24-> 00406c53 -thunk-> 0059ad80 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059a590 | FUN_0059a590 | 43 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#37-> 0040759a -thunk-> 0059a590 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059a260 | FUN_0059a260 | 38 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#22-> 004048c7 -thunk-> 0059a260 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00598b80 | FUN_00598b80 | 32 | Y | 005a7b99 --> … -call-> 00543810 -call-> 00598c10 -call-> 00408daa -call-> 00402ebe -thunk-> 00598b80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598c30 | FUN_00598c30 | 32 | Y | 004023a1 --> … -call-> 004039db -call-> 004017f8 -call-> 0049ec90 -call-> 004020c2 -call-> 00598c30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059b080 | FUN_0059b080 | 25 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040218a -vt:CEnumBox#17-> 00407c98 -thunk-> 0059b080 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00598af0 | FUN_00598af0 | 23 | Y | 004023a1 --> … -vt:CSurFireArrow#6-> 004075c7 -call-> 00408332 -call-> 00598b40 -call-> 00403549 -thunk-> 00598af0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598f80 | FUN_00598f80 | 23 | Y | 004029a0 --> … -call-> 0056b9c0 -call-> 00408832 -call-> 004548c0 -call-> 00402db0 -call-> 00598f80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598b40 | FUN_00598b40 | 13 | Y | 004023a1 -call-> 00504d90 -call-> 00401a1e -call-> 0043efd0 -vt:CSurFireArrow#6-> 004075c7 -call-> 00408332 -call-> 00598b40 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00599100 | FUN_00599100 | 10 | Y | 00402522 --> … -virt#1-> 00403c92 -call-> 00558940 -call-> 0055a720 -call-> 004011cc -call-> 00599100 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598c10 | FUN_00598c10 | 9 | Y | 005a7b99 -call-> 005a88aa -call-> 00403251 -call-> 0053eee0 -call-> 00543810 -call-> 00598c10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598cc0 | FUN_00598cc0 | 9 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00404eb7 -thunk-> 00598cc0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00599120 | FUN_00599120 | 8 | Y | 004029a0 --> … -vt:CEquipDialog#55-> 00405a1f -call-> 00444e10 -vt:CRAIN#4-> 00401c3a -call-> 00406735 -call-> 00599120 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CPropDialog：19 個 / 1093 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0054dd50 | FUN_0054dd50 | 228 | Y | 004029a0 -call-> 0044e060 -call-> 004fe8a0 -virt#7-> 00402a3b -call-> 0044a7a0 -virt#37-> 00408fa8 -thunk-> 0054dd50 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0054d940 | FUN_0054d940 | 125 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#55-> 00402883 -thunk-> 0054d940 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0054d2b0 | FUN_0054d2b0 | 120 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 00405eb1 -call-> 00402c89 -thunk-> 0054d2b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054e430 | FUN_0054e430 | 103 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#15-> 00405457 -thunk-> 0054e430 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0054d0d0 | FUN_0054d0d0 | 87 | Y | 00402522 --> … -call-> 00405c59 -virt#1-> 00403c92 -call-> 00558940 -call-> 0055a720 -call-> 0054d0d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054d6c0 | FUN_0054d6c0 | 65 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 00405eb1 -call-> 0054d6c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054cf40 | FUN_0054cf40 | 62 | Y | 005a7b99 -call-> 005a88aa -call-> 00403251 -call-> 0053eee0 -call-> 005848b0 -virt#34-> 004085ad -thunk-> 0054cf40 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 0054d560 | FUN_0054d560 | 58 | Y | 00402522 --> … -virt#11-> 0040268f -call-> 005554c0 -call-> 005562b0 -call-> 004023fb -thunk-> 0054d560 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00595c80 | FUN_00595c80 | 43 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#15-> 00405457 -call-> 00595c80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054e3d0 | FUN_0054e3d0 | 32 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#13-> 00401b18 -thunk-> 0054e3d0 | CSprGroup 精靈組（部件群）的載入／複製／取欄位／繪製：引擎 Role／Anim／Sprite 載入與 SDL  |
| 0054cea0 | FUN_0054cea0 | 29 | Y | 00402522 --> … -virt#1-> 00403c92 -call-> 00500720 -call-> 004fd270 -call-> 004070a4 -thunk-> 0054cea0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054d8a0 | FUN_0054d8a0 | 28 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#11-> 0040244b -thunk-> 0054d8a0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0054ccb0 | FUN_0054ccb0 | 25 | Y | 004029a0 -call-> 00402b21 -call-> 00558bc0 -call-> 0054f6c0 -call-> 004029e6 -call-> 00401b63 -thunk-> 0054ccb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054ce00 | FUN_0054ce00 | 24 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#2-> 0040579a -call-> 0054ce00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054cc50 | FUN_0054cc50 | 19 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#1-> 00404089 -thunk-> 0054cc50 | 解構函數 |
| 00577d90 | FUN_00577d90 | 15 | Y | 004029a0 --> … -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#15-> 00405457 -call-> 00406e15 -thunk-> 00577d90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0054d0b0 | FUN_0054d0b0 | 8 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#10-> 00402bee -call-> 0054b190 -vt:CPropDialog#0-> 0040195b -thunk-> 0054d0b0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0054d0c0 | FUN_0054d0c0 | 8 | Y | 004023a1 --> … -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -virt#51-> 00403805 -thunk-> 0054d0c0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0054cdb0 | FUN_0054cdb0 | 14 | ? | 005b9eff -call-> 0040912e -call-> 0054cdb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CCIme：31 個 / 1076 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00532ac0 | FUN_00532ac0 | 234 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#12-> 00403e18 -thunk-> 00532ac0 | 平台層：Windows IME 輸入法（網頁用瀏覽器輸入） |
| 00570190 | FUN_00570190 | 129 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#49-> 004022fc -call-> 00570190 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056fd10 | FUN_0056fd10 | 107 | Y | 004023a1 --> … -call-> 00402d47 -call-> 00532950 -vt:CCIme#49-> 004022fc -call-> 00570190 -call-> 0056fd10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056fb30 | FUN_0056fb30 | 78 | Y | 004023a1 --> … -call-> 00532950 -vt:CCIme#49-> 004022fc -call-> 00570190 -call-> 0040849a -thunk-> 0056fb30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056f500 | FUN_0056f500 | 67 | Y | 004023a1 --> … -vt:CEditBox#11-> 004017c6 -call-> 005329e0 -call-> 0056f3e0 -vt:LIme#2-> 00407635 -thunk-> 0056f500 | 平台層：Windows IME 輸入法（網頁用瀏覽器輸入） |
| 0056f9a0 | FUN_0056f9a0 | 58 | Y | 004029a0 --> … -vt:CFlyEffect#12-> 00401fc3 -call-> 00403f44 -call-> 00406929 -call-> 00570610 -call-> 0056f9a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00570000 | FUN_00570000 | 42 | Y | 004029a0 --> … -call-> 00406393 -call-> 004750e0 -call-> 004010a5 -call-> 00404219 -call-> 00570000 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00570610 | FUN_00570610 | 39 | Y | 004029a0 -call-> 00406e29 -call-> 004367e0 -vt:CFlyEffect#12-> 00401fc3 -call-> 00403f44 -call-> 00406929 -call-> 00570610 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00570110 | FUN_00570110 | 30 | Y | 0041bd90 --> … -call-> 0044dad0 -call-> 00403ed1 -call-> 00402888 -vt:CChapterDialog#51-> 00407009 -call-> 00570110 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056f880 | FUN_0056f880 | 28 | Y | 004023a1 --> … -vt:CCIme#49-> 004022fc -call-> 00570190 -call-> 0056fd10 -call-> 00402e69 -thunk-> 0056f880 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056f3e0 | FUN_0056f3e0 | 21 | Y | 004023a1 --> … -call-> 00406451 -call-> 0058c190 -vt:CEditBox#11-> 004017c6 -call-> 005329e0 -call-> 0056f3e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056fab0 | FUN_0056fab0 | 21 | Y | 005a7b99 -call-> 005a88aa -call-> 00403251 -call-> 0053eee0 -call-> 00408e9a -call-> 0056fab0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005330d0 | FUN_005330d0 | 20 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#1-> 00406fdc -thunk-> 005330d0 | 平台層：Windows IME 輸入法（網頁用瀏覽器輸入） |
| 0056f670 | FUN_0056f670 | 20 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#1-> 00406fdc -call-> 0056f670 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00533050 | FUN_00533050 | 17 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#49-> 004022fc -thunk-> 00533050 | 平台層：Windows IME 輸入法（網頁用瀏覽器輸入） |
| 00570710 | FUN_00570710 | 14 | Y | 004023a1 --> … -call-> 00404e35 -call-> 00402d47 -call-> 00532950 -vt:CCIme#11-> 004016cc -call-> 00570710 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005329e0 | FUN_005329e0 | 13 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 -vt:CEditBox#11-> 004017c6 -call-> 005329e0 | 平台層：Windows IME 輸入法（網頁用瀏覽器輸入） |
| 005330a0 | FUN_005330a0 | 12 | Y | 0041bd90 --> … -call-> 0054b990 -call-> 0040394a -vt:CDropMoneyDialog#11-> 00407833 -call-> 00534410 -call-> 005330a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056f820 | FUN_0056f820 | 12 | Y | 0040499e --> … -call-> 00407f59 -call-> 0040702c -call-> 00493430 -vt:CProtagonist#27-> 00408ca6 -call-> 0056f820 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056f850 | FUN_0056f850 | 12 | Y | 004029a0 --> … -call-> 004081bb -call-> 004c6a90 -call-> 004ce230 -call-> 004027de -call-> 0056f850 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CStatusDialogA：23 個 / 1042 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00506480 | FUN_00506480 | 218 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#15-> 00402dec -thunk-> 00506480 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00505bc0 | FUN_00505bc0 | 168 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -thunk-> 00505bc0 | CStatusDialogA（狀態列）殼：開關、擺位排版、取控制項、隊伍索引；引擎 hud_draw 自己排 |
| 00505580 | FUN_00505580 | 141 | Y | 00402522 --> … -virt#1-> 00403c92 -call-> 00500720 -call-> 00562ac0 -vt:CTeamDialog#0-> 00403abc -call-> 00505580 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005060a0 | FUN_005060a0 | 82 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#31-> 004049e9 -thunk-> 005060a0 | CStatusDialogA（狀態列）殼：開關、擺位排版、取控制項、隊伍索引；引擎 hud_draw 自己排 |
| 005058b0 | FUN_005058b0 | 70 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#34-> 00406230 -thunk-> 005058b0 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 005834f0 | FUN_005834f0 | 51 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 005834f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00506340 | FUN_00506340 | 48 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#13-> 004029b9 -thunk-> 00506340 | CStatusDialogA（狀態列）殼：開關、擺位排版、取控制項、隊伍索引；引擎 hud_draw 自己排 |
| 0056de50 | FUN_0056de50 | 45 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -thunk-> 0056de50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00505300 | FUN_00505300 | 27 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 0040760d -call-> 00505300 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005053b0 | FUN_005053b0 | 24 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00403f3a -call-> 00407e1e -call-> 00407108 -thunk-> 005053b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005054e0 | FUN_005054e0 | 21 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 00402f3b -call-> 005054e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00505ab0 | FUN_00505ab0 | 20 | Y | 00402522 --> … -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 00409395 -thunk-> 00505ab0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00505450 | FUN_00505450 | 19 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 00403f3a -call-> 00405506 -call-> 00505450 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00505b70 | FUN_00505b70 | 17 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#28-> 004053f8 -thunk-> 00505b70 | CStatusDialogA（狀態列）殼：開關、擺位排版、取控制項、隊伍索引；引擎 hud_draw 自己排 |
| 005052b0 | FUN_005052b0 | 15 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#1-> 00408d5f -thunk-> 005052b0 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00505b30 | FUN_00505b30 | 14 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#11-> 00408df5 -thunk-> 00505b30 | CStatusDialogA（狀態列）殼：開關、擺位排版、取控制項、隊伍索引；引擎 hud_draw 自己排 |
| 00538ff0 | FUN_00538ff0 | 11 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 004030cb -thunk-> 00538ff0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00546490 | FUN_00546490 | 11 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 0040684d -thunk-> 00546490 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00505a90 | FUN_00505a90 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -vt:CStatusDialogA#0-> 00405475 -thunk-> 00505a90 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00505aa0 | FUN_00505aa0 | 8 | Y | 00402522 --> … -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 004fa260 -call-> 00401172 -thunk-> 00505aa0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CGfxRenderDevice：29 個 / 991 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00587260 | FUN_00587260 | 247 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#23-> 004058f3 -thunk-> 00587260 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00586d30 | FUN_00586d30 | 205 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#21-> 00407202 -thunk-> 00586d30 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 005879b0 | FUN_005879b0 | 191 | Y | 004023a1 --> … -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#21-> 00407202 -call-> 00402068 -thunk-> 005879b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00586ca0 | FUN_00586ca0 | 27 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#22-> 00408f3a -thunk-> 00586ca0 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00586c20 | FUN_00586c20 | 23 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#18-> 00401cad -thunk-> 00586c20 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00585cc0 | FUN_00585cc0 | 22 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#1-> 00407cf7 -thunk-> 00585cc0 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00587850 | FUN_00587850 | 21 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#24-> 00407bad -thunk-> 00587850 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00587f80 | FUN_00587f80 | 20 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#19-> 00406f00 -thunk-> 00587f80 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 005880c0 | FUN_005880c0 | 19 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#14-> 0040188e -thunk-> 005880c0 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00588100 | FUN_00588100 | 19 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#11-> 004034ea -thunk-> 00588100 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00587210 | FUN_00587210 | 18 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#10-> 00407e41 -thunk-> 00587210 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00585d50 | FUN_00585d50 | 14 | Y | 004029a0 --> … -vt:CFlyEffect#12-> 00401fc3 -call-> 00403f44 -call-> 004b0a50 -call-> 00407c52 -call-> 00585d50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005871f0 | FUN_005871f0 | 14 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#20-> 00401d20 -thunk-> 005871f0 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00587fd0 | FUN_00587fd0 | 14 | Y | 00402522 --> … -call-> 00480d60 -call-> 004066bd -call-> 004f1ee0 -call-> 00406672 -call-> 00587fd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00585f30 | FUN_00585f30 | 12 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00403f3a -call-> 00405506 -call-> 004028ce -call-> 00585f30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00585ee0 | FUN_00585ee0 | 11 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#7-> 0040745f -thunk-> 00585ee0 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 0058d130 | FUN_0058d130 | 11 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#21-> 00407202 -call-> 0058d130 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a0fa0 | FUN_005a0fa0 | 10 | Y | 004023a1 --> … -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#1-> 00407cf7 -call-> 00403a30 -thunk-> 005a0fa0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00588010 | FUN_00588010 | 9 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#16-> 00406cb2 -thunk-> 00588010 | 平台層：Direct3D 繪圖裝置，換成 SDL |
| 00588030 | FUN_00588030 | 9 | Y | 004023a1 --> … -virt#24-> 00406474 -call-> 00406eb0 -call-> 005859b0 -vt:CGfxRenderDevice#27-> 0040190b -thunk-> 00588030 | 平台層：Direct3D 繪圖裝置，換成 SDL |

#### Poq.exe CDialogBox：22 個 / 940 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00588f90 | FUN_00588f90 | 289 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -thunk-> 00588f90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059d3b0 | FUN_0059d3b0 | 93 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0059d3b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593b50 | FUN_00593b50 | 76 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00593b50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058c190 | FUN_0058c190 | 68 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058c190 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00599c10 | FUN_00599c10 | 48 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004073a6 -thunk-> 00599c10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058d510 | FUN_0058d510 | 43 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058d510 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059be50 | FUN_0059be50 | 39 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00403ddc -thunk-> 0059be50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005988a0 | FUN_005988a0 | 34 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00406ad7 -thunk-> 005988a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059b700 | FUN_0059b700 | 34 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0059b700 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a1810 | FUN_005a1810 | 26 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 005a1810 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a14f0 | FUN_005a14f0 | 21 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 005a14f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00598380 | FUN_00598380 | 18 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 004035d0 -thunk-> 00598380 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00589a70 | FUN_00589a70 | 16 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -thunk-> 00589a70 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00596db0 | FUN_00596db0 | 16 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00588960 | FUN_00588960 | 12 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -call-> 00407522 -call-> 00407801 -vt:CDialogBox#1-> 00401b04 -thunk-> 00588960 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00596ee0 | FUN_00596ee0 | 12 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596ee0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a1200 | FUN_005a1200 | 12 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 005a1200 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0058ce60 | FUN_0058ce60 | 9 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0058ce60 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00596e50 | FUN_00596e50 | 9 | Y | 004023a1 --> … -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0040911a -thunk-> 00596e50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a13c0 | FUN_005a13c0 | 9 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 005a13c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### EasyRPG.dll CERPerson：84 個 / 845 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 1005e580 | FUN_1005e580 | 76 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:10003dc0 -call-> Easy:1007b490 -call-> Easy:1007be30 -virt#3-> Easy:1005e580 | 物件存取幫手（Title／Channel／Deal*／Job 等欄位、鏈結節點）：story_rt.c 取代；Deal* |
| 1005ef30 | FUN_1005ef30 | 42 | Y | 004023a1 --> … -import-> Easy:10004d00 -call-> Easy:10005290 -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:1005ef30 | 物件存取幫手（Title／Channel／Deal*／Job 等欄位、鏈結節點）：story_rt.c 取代；Deal* |
| 1005e350 | FUN_1005e350 | 41 | Y | 0040499e --> … -call-> Easy:1007c5f0 -call-> Easy:1007c3c0 -call-> Easy:1007c6c0 -call-> Easy:1007ae80 -virt#5-> Easy:1005e350 | 物件存取幫手（Title／Channel／Deal*／Job 等欄位、鏈結節點）：story_rt.c 取代；Deal* |
| 100229a0 | FUN_100229a0 | 13 | Y | 0040499e --> … -call-> Easy:1007c3c0 -call-> Easy:1007c6c0 -call-> Easy:1007ae80 -virt#5-> Easy:1005e350 -call-> Easy:100229a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 10022d30 | FUN_10022d30 | 13 | Y | 004023a1 --> … -call-> Easy:10003dc0 -call-> Easy:1007b490 -call-> Easy:1007be30 -virt#3-> Easy:1005e580 -call-> Easy:10022d30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 100238e0 | FUN_100238e0 | 13 | Y | 004023a1 --> … -call-> Easy:10005290 -call-> Easy:1007d050 -call-> Easy:1007d2e0 -virt#4-> Easy:1005ef30 -call-> Easy:100238e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005c4e0 | FUN_1005c4e0 | 12 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -vt:CERPerson#0-> Easy:1005c4e0 | 物件表／容器鏈／幫手（DAT_104797f0 物件表、新增刪除、鏈結串列節點）：story_rt.c objs[] 取 |
| 1005e210 | FUN_1005e210 | 12 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:10049e30 -call-> Easy:1005e210 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e4b0 | FUN_1005e4b0 | 9 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10006180 -call-> Easy:1005bce0 -vt:CERPerson#6-> Easy:1005e4b0 | 物件存取幫手（Title／Channel／Deal*／Job 等欄位、鏈結節點）：story_rt.c 取代；Deal* |
| 1005e4c0 | FUN_1005e4c0 | 9 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10003dc0 -call-> Easy:1005e4c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005df10 | FUN_1005df10 | 8 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:100538d0 -call-> Easy:1004c980 -call-> Easy:1005df10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005dfe0 | FUN_1005dfe0 | 8 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:10049e30 -call-> Easy:1005dfe0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e300 | FUN_1005e300 | 8 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004d00 -call-> Easy:10005290 -call-> Easy:1005e300 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e320 | FUN_1005e320 | 8 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004d00 -call-> Easy:10005290 -call-> Easy:1005e320 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e4d0 | FUN_1005e4d0 | 8 | Y | 004023a1 --> … -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003d470 -call-> Easy:10049e30 -call-> Easy:1005e4d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e4e0 | FUN_1005e4e0 | 8 | Y | 004023a1 --> … -call-> 004ecf10 -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1005e4e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e4f0 | FUN_1005e4f0 | 8 | Y | Easy:10024e40 -call-> Easy:10053b40 -call-> Easy:1005e4f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e500 | FUN_1005e500 | 8 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:100032c0 -call-> Easy:1003f7d0 -call-> Easy:1005e500 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e510 | FUN_1005e510 | 8 | Y | 004023a1 --> … -call-> 00401a3c -import-> Easy:10004dd0 -call-> Easy:10006030 -call-> Easy:10050630 -call-> Easy:1005e510 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 1005e1d0 | FUN_1005e1d0 | 12 | ? | Easy:1005fa30 -call-> Easy:10060dd0 -call-> Easy:10065060 -call-> Easy:100569f0 -call-> Easy:1005e1d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CUIServer：27 個 / 826 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0044a7a0 | FUN_0044a7a0 | 194 | Y | 004029a0 -call-> 0044e060 -call-> 004fe8a0 -virt#7-> 00402a3b -call-> 0044a7a0 | 平台：依 +4 的模式重建顯示（同 0x448d20） |
| 0057f7d0 | FUN_0057f7d0 | 168 | Y | 0041bd90 -call-> 004059b1 -call-> 00405ccc -call-> 0043f350 -vt:CFootStep#10-> 00405ed4 -virt#4-> 00406a69 -thunk-> 0057f7d0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 004f4c20 | FUN_004f4c20 | 64 | Y | 004029a0 -call-> 0044e060 -call-> 004fe8a0 -virt#7-> 00402a3b -call-> 004f4c20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0057f6a0 | FUN_0057f6a0 | 51 | Y | 004029a0 -call-> 0044e060 -call-> 004fe8a0 -virt#7-> 00402a3b -thunk-> 0057f6a0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f320 | FUN_0057f320 | 41 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00401ce9 -virt#26-> 00408841 -thunk-> 0057f320 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f4b0 | FUN_0057f4b0 | 36 | Y | 004023a1 -call-> 00504d90 -call-> 005212f0 -call-> 00406a1e -virt#16-> 004055e7 -thunk-> 0057f4b0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f5b0 | FUN_0057f5b0 | 36 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00401ce9 -virt#17-> 0040901b -thunk-> 0057f5b0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f2c0 | FUN_0057f2c0 | 18 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -virt#11-> 004066ef -thunk-> 0057f2c0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f2f0 | FUN_0057f2f0 | 18 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#2-> 004062fd -virt#14-> 004077c0 -thunk-> 0057f2f0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f200 | FUN_0057f200 | 16 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f410 | FUN_0057f410 | 16 | Y | 004023a1 -call-> 00547720 -call-> 00580df0 -virt#28-> 004045bb -thunk-> 0057f410 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f460 | FUN_0057f460 | 15 | Y | 004023a1 -call-> 00504d90 -call-> 005832a0 -virt#27-> 00406604 -thunk-> 0057f460 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057fbc0 | FUN_0057fbc0 | 15 | Y | 004023a1 -call-> 005585e0 -call-> 00406465 -virt#20-> 00404719 -thunk-> 0057fbc0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057fc00 | FUN_0057fc00 | 14 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#12-> 00406163 -virt#21-> 00406df2 -thunk-> 0057fc00 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f570 | FUN_0057f570 | 12 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#12-> 00403459 -thunk-> 0057f570 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f670 | FUN_0057f670 | 12 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00405178 -virt#15-> 00401924 -thunk-> 0057f670 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057fba0 | FUN_0057fba0 | 12 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#8-> 004037fb -virt#5-> 004036ac -thunk-> 0057fba0 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057f280 | FUN_0057f280 | 10 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004056dc -call-> 004a5310 -virt#18-> 0040826f -thunk-> 0057f280 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057fc30 | FUN_0057fc30 | 10 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 0044ca40 -call-> 004a6480 -virt#3-> 00401c0d -thunk-> 0057fc30 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 0057fc50 | FUN_0057fc50 | 10 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#2-> 004040b6 -thunk-> 0057fc50 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |

#### Poq.exe CMakePropDialog：19 個 / 780 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00540a20 | FUN_00540a20 | 116 | Y | 005a7b99 -call-> 005a88aa -call-> 00403251 -call-> 0053eee0 -call-> 00543120 -call-> 00540a20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005420d0 | FUN_005420d0 | 101 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00403f3a -call-> 00543f30 -call-> 00406c12 -thunk-> 005420d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00540cb0 | FUN_00540cb0 | 95 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#34-> 0040192e -thunk-> 00540cb0 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 00541c50 | FUN_00541c50 | 86 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#11-> 00406771 -thunk-> 00541c50 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00541e70 | FUN_00541e70 | 68 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#55-> 00408a4e -thunk-> 00541e70 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00542380 | FUN_00542380 | 45 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#15-> 00405925 -thunk-> 00542380 | 按鈕啟用／停用幀切換（FUN_00583e40 設按鈕狀態、FUN_00580df0 顯示）：引擎以 ui 旗標 0x1 |
| 00542000 | FUN_00542000 | 42 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#37-> 00405bfa -thunk-> 00542000 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00574540 | FUN_00574540 | 35 | Y | 004029a0 -call-> 00402b21 -call-> 00558bc0 -call-> 00406aa5 -vt:CEquipDialog#30-> 0040371a -thunk-> 00574540 | 視窗基底每幀：隱藏／顯示邏輯與選取框（MFC 式 CDialog 狀態機）；引擎 JM 視窗直接用 open 旗標 |
| 00540f10 | FUN_00540f10 | 31 | Y | 004023a1 --> … -call-> 00405afb -call-> 00406456 -vt:CCreditsDialog#55-> 0040835a -call-> 0051bf20 -call-> 00540f10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005410d0 | FUN_005410d0 | 30 | Y | 005a7b99 --> … -vt:MyDefine#0-> 00403ee5 -call-> 0046b190 -call-> 00458620 -call-> 004089a4 -call-> 005410d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00541070 | FUN_00541070 | 24 | Y | 005a7b99 -call-> 005a88aa -call-> 00403251 -call-> 0053eee0 -call-> 00543810 -call-> 00541070 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005406d0 | FUN_005406d0 | 21 | Y | 004023a1 -call-> 00547720 -call-> 004fe7a0 -call-> 00588990 -call-> 005810e0 -virt#31-> 004011a4 -call-> 005406d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00540780 | FUN_00540780 | 20 | Y | 004023a1 --> … -call-> 0040390e -call-> 004d21d0 -call-> 0040696a -call-> 004057fe -call-> 00540780 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00540830 | FUN_00540830 | 16 | Y | 00402522 --> … -call-> 004fb0d0 -call-> 0040277a -call-> 00402f0e -call-> 00404df9 -call-> 00540830 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00540730 | FUN_00540730 | 14 | Y | 004023a1 --> … -vt:CCIme#49-> 004022fc -call-> 00570190 -call-> 0056fd10 -call-> 00402e69 -call-> 00540730 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00541170 | FUN_00541170 | 11 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 00403a80 -call-> 00541170 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005408a0 | FUN_005408a0 | 9 | Y | 0041bd90 --> … -call-> 00471fd0 -call-> 0046de80 -vt:CScnClient#3-> 004055d3 -call-> 00401a46 -call-> 005408a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005411a0 | FUN_005411a0 | 8 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#0-> 00407b8a -thunk-> 005411a0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 005411c0 | FUN_005411c0 | 8 | Y | 0040499e --> … -call-> 00408814 -call-> 00558f40 -call-> 00540230 -vt:CMakePropDialog#51-> 004038aa -thunk-> 005411c0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CScroll：21 個 / 772 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0059c5e0 | FUN_0059c5e0 | 335 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#14-> 004081ed -thunk-> 0059c5e0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059bf20 | FUN_0059bf20 | 114 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#10-> 00408378 -thunk-> 0059bf20 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059c3e0 | FUN_0059c3e0 | 36 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#12-> 0040387d -thunk-> 0059c3e0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059bb80 | FUN_0059bb80 | 34 | Y | 005a7b99 --> … -call-> 00543810 -call-> 0059b220 -call-> 004037bf -call-> 00408a67 -thunk-> 0059bb80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059bc90 | FUN_0059bc90 | 33 | Y | 004023a1 --> … -call-> 00406451 -call-> 00401c99 -vt:CScroll#14-> 004081ed -call-> 00406b31 -thunk-> 0059bc90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059c280 | FUN_0059c280 | 29 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#11-> 00401d1b -thunk-> 0059c280 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059c540 | FUN_0059c540 | 26 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#24-> 00404287 -thunk-> 0059c540 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059c330 | FUN_0059c330 | 23 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#37-> 00408779 -thunk-> 0059c330 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059bd50 | FUN_0059bd50 | 22 | Y | 005a7b99 --> … -call-> 00543810 -call-> 00598c10 -call-> 00408daa -call-> 0040723e -call-> 0059bd50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059c3a0 | FUN_0059c3a0 | 15 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#25-> 00401f69 -thunk-> 0059c3a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059bde0 | FUN_0059bde0 | 13 | Y | 004029a0 --> … -vt:CGodServer#2-> 00404ead -call-> 005a17f0 -call-> 0059b6a0 -call-> 004086d9 -call-> 0059bde0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059bb10 | FUN_0059bb10 | 12 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#1-> 004025ae -thunk-> 0059bb10 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 0059be10 | FUN_0059be10 | 12 | Y | 004023a1 --> … -call-> 00406451 -call-> 00401c99 -vt:CScroll#14-> 004081ed -call-> 00408c7e -thunk-> 0059be10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059c4f0 | FUN_0059c4f0 | 11 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#20-> 0040254f -thunk-> 0059c4f0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0059bb50 | FUN_0059bb50 | 9 | Y | 004023a1 --> … -call-> 00407522 -call-> 00407801 -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 0059bb50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059bb40 | FUN_0059bb40 | 8 | Y | 004023a1 --> … -call-> 00532800 -call-> 00454f00 -call-> 00459840 -call-> 00407fa4 -call-> 0059bb40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059bc70 | FUN_0059bc70 | 8 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#0-> 004017da -thunk-> 0059bc70 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0059bc80 | FUN_0059bc80 | 8 | Y | 004023a1 --> … -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -virt#51-> 004042af -thunk-> 0059bc80 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0059bdc0 | FUN_0059bdc0 | 8 | Y | 0040499e --> … -vt:CNpc#9-> 00403193 -call-> 00404688 -call-> 005813c0 -call-> 0040322e -thunk-> 0059bdc0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059bdd0 | FUN_0059bdd0 | 8 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00405cf9 -call-> 005843f0 -call-> 0059bdd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CUIWinManager：13 個 / 760 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0059db60 | FUN_0059db60 | 460 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -thunk-> 0059db60 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059d8d0 | FUN_0059d8d0 | 66 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#3-> 00404836 -thunk-> 0059d8d0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0059da00 | FUN_0059da00 | 53 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#4-> 004045fc -thunk-> 0059da00 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00584760 | FUN_00584760 | 48 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -call-> 00584760 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059d840 | FUN_0059d840 | 32 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -call-> 0040552e -thunk-> 0059d840 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059dac0 | FUN_0059dac0 | 20 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004073a1 -call-> 00497f20 -vt:CSurfaceObj#0-> 004025ea -call-> 0059dac0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580f20 | FUN_00580f20 | 15 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -call-> 00580f20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00588f50 | FUN_00588f50 | 12 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#4-> 004045fc -call-> 00588f50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00592710 | FUN_00592710 | 12 | Y | 004023a1 --> … -call-> 0044fd20 -call-> 00405d08 -call-> 004068d4 -call-> 00401217 -call-> 00592710 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 0059db40 | FUN_0059db40 | 12 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004056dc -call-> 0059db40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005a0c10 | FUN_005a0c10 | 12 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#4-> 004045fc -call-> 005a0c10 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059db00 | FUN_0059db00 | 10 | Y | 004029a0 --> … -call-> 0049e750 -call-> 00406393 -call-> 004a7340 -vt:char_traits<char>_>#14-> 004030ad -call-> 0059db00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0059db20 | FUN_0059db20 | 8 | Y | 004023a1 --> … -call-> 0044d080 -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#0-> 00408d50 -thunk-> 0059db20 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CTalkDialog：13 個 / 582 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00502cf0 | FUN_00502cf0 | 319 | Y | 00402522 --> … -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 00407ff9 -thunk-> 00502cf0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00503640 | FUN_00503640 | 48 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#31-> 00405e7a -thunk-> 00503640 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |
| 00503840 | FUN_00503840 | 32 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#54-> 004040ca -thunk-> 00503840 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |
| 00503790 | FUN_00503790 | 25 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#15-> 00403292 -thunk-> 00503790 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |
| 00502cb0 | FUN_00502cb0 | 19 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#12-> 00406163 -call-> 00502cb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00502c30 | FUN_00502c30 | 18 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#34-> 0040401b -thunk-> 00502c30 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 00502bf0 | FUN_00502bf0 | 13 | Y | 0041bd90 --> … -call-> 0043f350 -vt:CFootStep#0-> 0040920f -call-> 00471fd0 -call-> 00405c22 -call-> 00502bf0 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 005034c0 | FUN_005034c0 | 12 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#11-> 00407be4 -thunk-> 005034c0 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |
| 0055d350 | FUN_0055d350 | 11 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#15-> 00403292 -call-> 0055d350 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00503500 | FUN_00503500 | 10 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#55-> 0040536c -thunk-> 00503500 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |
| 00502c90 | FUN_00502c90 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#0-> 004077d9 -thunk-> 00502c90 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00502ca0 | FUN_00502ca0 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#51-> 00407199 -thunk-> 00502ca0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00503530 | FUN_00503530 | 59 | ? | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00502b30 -vt:CTalkDialog#45-> 004078c4 -thunk-> 00503530 | 連線版：聊天輸入列歷史、輸入／輸出視窗位置與焦點，單機沒有聊天 |

#### Poq.exe CExchangeDialog：17 個 / 564 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0055caa0 | FUN_0055caa0 | 137 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#11-> 00401343 -call-> 0055caa0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d0b0 | FUN_0055d0b0 | 74 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#34-> 00401910 -thunk-> 0055d0b0 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 0055ce80 | FUN_0055ce80 | 66 | Y | 00402522 --> … -call-> 0055c730 -vt:CExchangeDialog#15-> 004079cd -call-> 00516060 -call-> 00403b39 -thunk-> 0055ce80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d400 | FUN_0055d400 | 36 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#55-> 00407fea -thunk-> 0055d400 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0055d620 | FUN_0055d620 | 34 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#15-> 004079cd -thunk-> 0055d620 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 0055d5a0 | FUN_0055d5a0 | 32 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#13-> 00402383 -thunk-> 0055d5a0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 0055c8c0 | FUN_0055c8c0 | 26 | Y | 0040499e --> … -call-> 00403f3a -call-> 00405c5e -call-> 0055dd80 -call-> 00406555 -thunk-> 0055c8c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055c9b0 | FUN_0055c9b0 | 26 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00403f3a -call-> 00405c5e -call-> 004064d3 -call-> 0055c9b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055cd90 | FUN_0055cd90 | 26 | Y | 0040499e --> … -call-> 0047ece0 -call-> 00484ee0 -call-> 00404d4a -call-> 00515e70 -call-> 0055cd90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d500 | FUN_0055d500 | 24 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#31-> 00408c1a -thunk-> 0055d500 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 0055d370 | FUN_0055d370 | 19 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#11-> 00401343 -thunk-> 0055d370 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0055c870 | FUN_0055c870 | 18 | Y | 004023a1 --> … -call-> 00407072 -call-> 0057a390 -call-> 0057d0b0 -call-> 00408ce2 -call-> 0055c870 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 0055d2b0 | FUN_0055d2b0 | 11 | Y | 00402522 --> … -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#15-> 004079cd -call-> 00516060 -call-> 0055d2b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d2f0 | FUN_0055d2f0 | 10 | Y | 00402522 --> … -call-> 0055c730 -vt:CExchangeDialog#15-> 004079cd -call-> 00516060 -call-> 0040615e -thunk-> 0055d2f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d310 | FUN_0055d310 | 9 | Y | 00402522 --> … -call-> 0055c730 -vt:CExchangeDialog#15-> 004079cd -call-> 00516060 -call-> 00408152 -thunk-> 0055d310 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055d330 | FUN_0055d330 | 8 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#0-> 00404d63 -thunk-> 0055d330 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0055d340 | FUN_0055d340 | 8 | Y | 00402522 --> … -call-> 005162d0 -call-> 004082bf -call-> 0055c730 -vt:CExchangeDialog#51-> 00401591 -thunk-> 0055d340 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CTradeDialog：13 個 / 557 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00559a80 | FUN_00559a80 | 115 | Y | 0040499e -call-> 004020d1 -call-> 00406307 -call-> 0054bb30 -call-> 00405eb1 -call-> 00559a80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00559ec0 | FUN_00559ec0 | 90 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#34-> 00405736 -thunk-> 00559ec0 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 0055a1c0 | FUN_0055a1c0 | 69 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#11-> 00402220 -thunk-> 0055a1c0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0055a380 | FUN_0055a380 | 54 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#55-> 004033cd -thunk-> 0055a380 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00559940 | FUN_00559940 | 52 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -virt#53-> 00402da1 -call-> 00558160 -call-> 00559940 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00559dd0 | FUN_00559dd0 | 45 | Y | 00402522 --> … -call-> 00405c59 -virt#1-> 00403c92 -call-> 00558940 -call-> 0055a720 -call-> 00559dd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055a4f0 | FUN_0055a4f0 | 42 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#37-> 00408cb5 -thunk-> 0055a4f0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00559850 | FUN_00559850 | 26 | Y | 0040499e --> … -call-> 0054bb30 -call-> 00403f3a -call-> 0040456b -call-> 004031c5 -thunk-> 00559850 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055a6c0 | FUN_0055a6c0 | 22 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#15-> 00405619 -thunk-> 0055a6c0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00559800 | FUN_00559800 | 18 | Y | 0041bd90 --> … -call-> 0043f350 -vt:CFootStep#0-> 0040920f -call-> 00471fd0 -vt:CScnClientRecord#3-> 00403f62 -call-> 00559800 | 解構函數 |
| 0055a140 | FUN_0055a140 | 8 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#0-> 0040638e -thunk-> 0055a140 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0055a150 | FUN_0055a150 | 8 | Y | 004023a1 --> … -call-> 00408d05 -call-> 00403ea4 -call-> 00407572 -vt:CTradeDialog#51-> 00401f82 -thunk-> 0055a150 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0055a180 | FUN_0055a180 | 8 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 005299a0 -call-> 0055a180 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CSingleItem：16 個 / 541 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 004dd2a0 | FUN_004dd2a0 | 134 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#8-> 00405b0a -thunk-> 004dd2a0 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dd8c0 | FUN_004dd8c0 | 72 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#11-> 00408c92 -thunk-> 004dd8c0 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dda90 | FUN_004dda90 | 65 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#10-> 0040783d -thunk-> 004dda90 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dd630 | FUN_004dd630 | 51 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#6-> 004087dd -thunk-> 004dd630 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dd010 | FUN_004dd010 | 40 | Y | 0040499e --> … -vt:CSurfaceObj#0-> 004025ea -call-> 004d27d0 -call-> 00404e9e -call-> 004026b7 -thunk-> 004dd010 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004dd150 | FUN_004dd150 | 27 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -call-> 004081de -thunk-> 004dd150 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004dd800 | FUN_004dd800 | 24 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#5-> 0040334b -thunk-> 004dd800 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dd210 | FUN_004dd210 | 20 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#0-> 00407f3b -thunk-> 004dd210 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004ddcb0 | FUN_004ddcb0 | 18 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#1-> 00406b63 -thunk-> 004ddcb0 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004ddd10 | FUN_004ddd10 | 18 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#2-> 00401262 -thunk-> 004ddd10 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dd1b0 | FUN_004dd1b0 | 17 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004dd880 | FUN_004dd880 | 13 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#7-> 00403a49 -thunk-> 004dd880 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004dcfd0 | FUN_004dcfd0 | 12 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#10-> 0040783d -call-> 004dcfd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004dd7c0 | FUN_004dd7c0 | 11 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#9-> 00409147 -thunk-> 004dd7c0 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004ddc90 | FUN_004ddc90 | 10 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#3-> 00408314 -thunk-> 004ddc90 | 連線版：socket／網路物件（含鎖與事件），單機不做 |
| 004ddc70 | FUN_004ddc70 | 9 | Y | 0040499e -call-> 00408e63 -call-> 0057f200 -vt:CUIServer#10-> 00402f77 -call-> 004dd1b0 -vt:CSingleItem#4-> 004020f4 -thunk-> 004ddc70 | 連線版：socket／網路物件（含鎖與事件），單機不做 |

#### Poq.exe CLogDialog：13 個 / 512 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 005679d0 | FUN_005679d0 | 103 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#55-> 00401be0 -thunk-> 005679d0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00566fd0 | FUN_00566fd0 | 88 | Y | 0040499e --> … -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#55-> 00401be0 -call-> 004051ff -thunk-> 00566fd0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00567580 | FUN_00567580 | 65 | Y | 004023a1 --> … -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#1-> 00408afd -call-> 00402077 -call-> 00567580 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00567410 | FUN_00567410 | 55 | Y | 004029a0 --> … -call-> 004750e0 -call-> 004010a5 -call-> 00407036 -call-> 00403db4 -call-> 00567410 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00567840 | FUN_00567840 | 46 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#34-> 004033c8 -thunk-> 00567840 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 00567770 | FUN_00567770 | 20 | Y | 0040499e --> … -call-> 00552740 -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -thunk-> 00567770 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00567740 | FUN_00567740 | 13 | Y | 004029a0 --> … -call-> 00500d90 -call-> 0040193d -call-> 004346f0 -call-> 0040875b -call-> 00567740 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00567810 | FUN_00567810 | 12 | Y | 004023a1 --> … -call-> 00402e00 -call-> 00405afb -call-> 00406456 -vt:CCreditsDialog#1-> 00405f74 -call-> 00567810 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 005679b0 | FUN_005679b0 | 10 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#11-> 00401a8c -thunk-> 005679b0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00567ce0 | FUN_00567ce0 | 9 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#31-> 00404dbd -thunk-> 00567ce0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00567990 | FUN_00567990 | 8 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#0-> 004071e4 -thunk-> 00567990 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 005679a0 | FUN_005679a0 | 8 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#51-> 00406cd5 -thunk-> 005679a0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 005672a0 | FUN_005672a0 | 75 | ? | 005672a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CMessagePipeDialog：19 個 / 496 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00545eb0 | FUN_00545eb0 | 119 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#15-> 00405ede -thunk-> 00545eb0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 00545c50 | FUN_00545c50 | 98 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#11-> 00407e73 -thunk-> 00545c50 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 005454c0 | FUN_005454c0 | 32 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#15-> 00405ede -call-> 005454c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005461b0 | FUN_005461b0 | 31 | Y | 0040499e --> … -vt:CStatusDialogC#1-> 004091ab -call-> 004fe510 -call-> 0040214e -vt:CLogDialog#51-> 00406cd5 -call-> 005461b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005453f0 | FUN_005453f0 | 29 | Y | 0040499e --> … -call-> 00457ad0 -call-> 00457a30 -call-> 00405821 -call-> 00408689 -call-> 005453f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00546340 | FUN_00546340 | 24 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#15-> 00405ede -call-> 00546340 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005453a0 | FUN_005453a0 | 23 | Y | 004023a1 --> … -call-> 00404e35 -call-> 004048f4 -call-> 00545470 -call-> 00403afd -thunk-> 005453a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005462d0 | FUN_005462d0 | 23 | Y | 0040499e --> … -call-> 00403c83 -call-> 00450700 -call-> 004355e0 -vt:CARROW#6-> 00404ccd -call-> 005462d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545be0 | FUN_00545be0 | 20 | Y | 004023a1 --> … -call-> 005859b0 -vt:CGfxRenderDevice#16-> 00406cb2 -call-> 004c1110 -call-> 0040884b -call-> 00545be0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545470 | FUN_00545470 | 14 | Y | 004023a1 --> … -vt:CGameApp#23-> 00407dd3 -call-> 00408fd0 -call-> 00404e35 -call-> 004048f4 -call-> 00545470 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00545b90 | FUN_00545b90 | 12 | Y | 00402522 --> … -call-> 00562ac0 -vt:CTeamDialog#11-> 004045ed -call-> 004fb0d0 -call-> 004032b5 -call-> 00545b90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545bb0 | FUN_00545bb0 | 12 | Y | 0040499e -call-> 004036a7 -call-> 004029c3 -call-> 00545bb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545b20 | FUN_00545b20 | 10 | Y | 004023a1 --> … -call-> 00406456 -vt:CCreditsDialog#55-> 0040835a -call-> 004066a4 -call-> 00405326 -call-> 00545b20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545e90 | FUN_00545e90 | 9 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#55-> 00401960 -thunk-> 00545e90 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00545b10 | FUN_00545b10 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#34-> 00404d90 -thunk-> 00545b10 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00545b40 | FUN_00545b40 | 8 | Y | 004029a0 --> … -call-> 004c3640 -call-> 004084b3 -call-> 00441060 -vt:CGroup#1-> 00407fc2 -call-> 00545b40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00545b50 | FUN_00545b50 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#0-> 00405b3c -thunk-> 00545b50 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00545b80 | FUN_00545b80 | 8 | Y | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#51-> 00407a72 -thunk-> 00545b80 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00545ea0 | FUN_00545ea0 | 8 | ? | 004023a1 --> … -call-> 00401929 -vt:CGameDialog#55-> 00405ce5 -call-> 00545070 -vt:CMessagePipeDialog#45-> 00404e8a -thunk-> 00545ea0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CDeadDialog：9 個 / 492 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00547b00 | FUN_00547b00 | 338 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#55-> 00404421 -thunk-> 00547b00 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 004f9390 | FUN_004f9390 | 51 | Y | 004023a1 --> … -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#15-> 004081fc -call-> 004011fe -thunk-> 004f9390 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00548270 | FUN_00548270 | 33 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#15-> 004081fc -thunk-> 00548270 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 00547a30 | FUN_00547a30 | 24 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#1-> 00404124 -thunk-> 00547a30 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 00547ad0 | FUN_00547ad0 | 13 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#11-> 00406a91 -thunk-> 00547ad0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 00548260 | FUN_00548260 | 9 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#31-> 00402c2f -thunk-> 00548260 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00547aa0 | FUN_00547aa0 | 8 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#34-> 004046a1 -thunk-> 00547aa0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00547ab0 | FUN_00547ab0 | 8 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#0-> 004058ad -thunk-> 00547ab0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00547ac0 | FUN_00547ac0 | 8 | Y | 004023a1 --> … -call-> 00407923 -call-> 00408a80 -call-> 004053ad -vt:CDeadDialog#51-> 004043d1 -thunk-> 00547ac0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CInterfaceBase：19 個 / 477 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00580a40 | FUN_00580a40 | 140 | Y | 004023a1 -call-> 00547720 -call-> 004fe7a0 -call-> 00588b70 -call-> 00580a40 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005805b0 | FUN_005805b0 | 85 | Y | 004023a1 -call-> 00504d90 -call-> 005212f0 -call-> 00406a1e -thunk-> 005805b0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580520 | FUN_00580520 | 32 | Y | 004029a0 -call-> 00406e29 -call-> 00403fb7 -call-> 004de570 -call-> 004072fc -call-> 00580520 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580980 | FUN_00580980 | 30 | Y | 004023a1 --> … -call-> 00449f50 -call-> 004f7aa0 -call-> 00406e4c -call-> 00403a80 -call-> 00580980 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005808f0 | FUN_005808f0 | 20 | Y | 004023a1 -call-> 005585e0 -call-> 00402c43 -call-> 00404381 -call-> 00406d43 -thunk-> 005808f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580e20 | FUN_00580e20 | 20 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 00403c83 -call-> 004065cd -call-> 00580e20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580da0 | FUN_00580da0 | 18 | Y | 005a7b99 --> … -call-> 0053eee0 -call-> 00543810 -call-> 0059b220 -call-> 004037bf -thunk-> 00580da0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005808a0 | FUN_005808a0 | 16 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00401c99 -vt:CScroll#14-> 004081ed -call-> 005808a0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580df0 | FUN_00580df0 | 15 | Y | 004023a1 -call-> 00547720 -call-> 00580df0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005804e0 | FUN_005804e0 | 14 | Y | 004023a1 --> … -call-> 00409043 -virt#22-> 00408ab2 -call-> 004658b0 -vt:CClientScreen#0-> 004074f0 -call-> 005804e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005804b0 | FUN_005804b0 | 12 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -virt#1-> 00405af1 -thunk-> 005804b0 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00580e80 | FUN_00580e80 | 12 | Y | 0040499e --> … -call-> 0046e7f0 -call-> 004051f5 -vt:CViewPort#2-> 00401e42 -call-> 0040321f -call-> 00580e80 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580810 | FUN_00580810 | 10 | Y | 004029a0 --> … -call-> 004010a5 -call-> 00407036 -call-> 0044dbd0 -call-> 00406578 -call-> 00580810 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580e60 | FUN_00580e60 | 10 | Y | 0040499e --> … -call-> 00405178 -call-> 00476770 -call-> 00476560 -call-> 00407de7 -call-> 00580e60 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005807f0 | FUN_005807f0 | 9 | Y | 004023a1 --> … -call-> 004017f8 -call-> 0046c030 -call-> 00408652 -call-> 00408486 -call-> 005807f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580eb0 | FUN_00580eb0 | 9 | Y | 004023a1 -call-> 005585e0 -call-> 00406465 -virt#20-> 004016b8 -call-> 00580eb0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00580ec0 | FUN_00580ec0 | 9 | Y | 004023a1 --> … -call-> 00406451 -call-> 004045de -vt:CPushbutton#20-> 0040349f -call-> 00408008 -thunk-> 00580ec0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056e1d0 | FUN_0056e1d0 | 8 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -vt:CInterfaceBase#10-> 004045d4 -thunk-> 0056e1d0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00580ed0 | FUN_00580ed0 | 8 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -vt:CInterfaceBase#0-> 004031cf -thunk-> 00580ed0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CSinglePlayerDialog：8 個 / 472 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 00571160 | FUN_00571160 | 302 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#55-> 004061f9 -thunk-> 00571160 | 讀／存設定檔 ini\user.ini 的 FileData／SaveFolder 設定表（登入帳號清單）：單機設定存檔 |
| 00570f70 | FUN_00570f70 | 58 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#34-> 004013a7 -thunk-> 00570f70 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 004f5e70 | FUN_004f5e70 | 53 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#55-> 004061f9 -call-> 004f5e70 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00570f00 | FUN_00570f00 | 24 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#1-> 00404692 -thunk-> 00570f00 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00571140 | FUN_00571140 | 10 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#11-> 00402ed2 -thunk-> 00571140 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00571780 | FUN_00571780 | 9 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#31-> 004066f4 -thunk-> 00571780 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00571120 | FUN_00571120 | 8 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#0-> 00401d0c -thunk-> 00571120 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00571130 | FUN_00571130 | 8 | Y | 004023a1 --> … -call-> 004010ff -call-> 005350c0 -call-> 00404ecb -vt:CSinglePlayerDialog#51-> 0040529f -thunk-> 00571130 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CSkillDialog：12 個 / 470 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0055fd50 | FUN_0055fd50 | 99 | Y | 004023a1 --> … -call-> 00406e4c -call-> 0040634d -call-> 0055ea10 -call-> 0040345e -thunk-> 0055fd50 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00560170 | FUN_00560170 | 74 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 00409160 -vt:CMapDialog#13-> 00404afc -call-> 0040654b -call-> 00560170 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0055fb10 | FUN_0055fb10 | 72 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#34-> 004066db -thunk-> 0055fb10 | 依 ID 取控制項的轉接，無規則 |
| 00560410 | FUN_00560410 | 59 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#15-> 00404304 -thunk-> 00560410 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00560020 | FUN_00560020 | 51 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#55-> 004076ee -thunk-> 00560020 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0055fca0 | FUN_0055fca0 | 34 | Y | 004023a1 -call-> 00547720 -call-> 00402e87 -call-> 0055f120 -call-> 004091d8 -call-> 00561720 -call-> 0055fca0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00560350 | FUN_00560350 | 18 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#42-> 00405f6f -thunk-> 00560350 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 0055f8b0 | FUN_0055f8b0 | 15 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#1-> 00404ff2 -thunk-> 0055f8b0 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00560000 | FUN_00560000 | 10 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#11-> 0040632a -thunk-> 00560000 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0055fd30 | FUN_0055fd30 | 8 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#0-> 00402621 -thunk-> 0055fd30 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0055fd40 | FUN_0055fd40 | 8 | Y | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#51-> 00403067 -thunk-> 0055fd40 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00560110 | FUN_00560110 | 22 | ? | 004029a0 --> … -call-> 00553640 -call-> 00500360 -call-> 00406c99 -vt:CSkillDialog#45-> 00406253 -thunk-> 00560110 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |

#### Poq.exe XPathSearch：15 個 / 437 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 004d04d0 | FUN_004d04d0 | 46 | Y | 0041bd90 --> … -call-> 0043f350 -vt:CFootStep#0-> 0040920f -call-> 00471fd0 -vt:CScnClientRecord#3-> 00403f62 -call-> 004d04d0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004d0370 | FUN_004d0370 | 39 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#2-> 00409269 -thunk-> 004d0370 | XPathSearch 取方向 byte 路徑（呼叫端只用 0x4d09e0 的座標路徑，無呼叫者） |
| 004d0400 | FUN_004d0400 | 39 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#3-> 00402310 -thunk-> 004d0400 | XPathSearch 取節點指標串列（無呼叫者） |
| 004d0580 | FUN_004d0580 | 39 | Y | 004023a1 --> … -call-> 00593670 -vt:CToolTip#12-> 004013fc -call-> 00494e80 -call-> 004014c4 -call-> 004d0580 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004cfd30 | FUN_004cfd30 | 38 | Y | 004029a0 -call-> 00402b21 -call-> 00558bc0 -call-> 00406aa5 -vt:CEquipDialog#30-> 0040371a -call-> 004027fc -call-> 004cfd30 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004cfe90 | FUN_004cfe90 | 34 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004083aa -call-> 004cfe90 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004cfe20 | FUN_004cfe20 | 30 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#7-> 00401820 -thunk-> 004cfe20 | XPathSearch 清空節點表／堆（C 版每次尋路重配陣列） |
| 004d0290 | FUN_004d0290 | 26 | Y | 0040499e --> … -call-> 005326e0 -call-> 00403da5 -call-> 005a1ab0 -call-> 004062cb -call-> 004d0290 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004d02e0 | FUN_004d02e0 | 26 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#6-> 00403aad -thunk-> 004d02e0 | XPathSearch 重設節點表（同上） |
| 004d0480 | FUN_004d0480 | 26 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#5-> 004054f2 -thunk-> 004d0480 | XPathSearch 重設節點表（同上） |
| 004cfdc0 | FUN_004cfdc0 | 21 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#0-> 004080c6 -call-> 004cfdc0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004d0330 | FUN_004d0330 | 19 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004015aa -call-> 004d0330 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 004cfce0 | FUN_004cfce0 | 15 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#0-> 004080c6 -thunk-> 004cfce0 | XPathSearch 解構（C 版 free 陣列） |
| 004d03f0 | FUN_004d03f0 | 8 | Y | 004023a1 --> … -vt:CLoadDialog#8-> 004037fb -virt#5-> 00403eb8 -call-> 004cfc00 -vt:XPathSearch#4-> 00407eb9 -thunk-> 004d03f0 | XPathSearch 虛擬空函式回 0 |
| 004d0680 | FUN_004d0680 | 31 | ? | 00402982 -thunk-> 004d0680 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |

#### Poq.exe CPortraitPanel：15 個 / 402 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 0056e450 | FUN_0056e450 | 83 | Y | 004023a1 --> … -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#14-> 00401573 -thunk-> 0056e450 | CPortraitPanel 每幀轉盤動畫：同上 |
| 0056e020 | FUN_0056e020 | 54 | Y | 004023a1 --> … -call-> 005174d0 -call-> 00506e20 -call-> 0056e830 -call-> 0040618b -thunk-> 0056e020 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056e1e0 | FUN_0056e1e0 | 46 | Y | 004023a1 --> … -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#10-> 00404bce -thunk-> 0056e1e0 | CPortraitPanel（頭像轉盤，只掛在沒用到的 JM511）載入 5 個頭像精靈 |
| 0049e490 | FUN_0049e490 | 45 | Y | 004023a1 --> … -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#14-> 00401573 -call-> 00403cce -thunk-> 0049e490 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00581990 | FUN_00581990 | 37 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#26-> 00404db3 -thunk-> 00581990 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0056e360 | FUN_0056e360 | 35 | Y | 004023a1 --> … -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#12-> 00406325 -thunk-> 0056e360 | CPortraitPanel 繪製：同上，JM511 沒用到 |
| 0056e2f0 | FUN_0056e2f0 | 26 | Y | 004023a1 --> … -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#11-> 004062f3 -thunk-> 0056e2f0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 0056dfd0 | FUN_0056dfd0 | 12 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -call-> 004084e5 -vt:CUIStateServer#0-> 00402356 -call-> 0056dfd0 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00582340 | FUN_00582340 | 11 | Y | 004023a1 -call-> 00504d90 -call-> 00583790 -virt#24-> 00406474 -call-> 00407612 -thunk-> 00582340 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00582c90 | FUN_00582c90 | 11 | Y | 0041bd90 --> … -call-> 005852e0 -call-> 004072c5 -vt:CUIManager#6-> 00403cf6 -virt#49-> 00402833 -thunk-> 00582c90 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0056e000 | FUN_0056e000 | 9 | Y | 004023a1 --> … -call-> 004f7aa0 -call-> 00406e4c -call-> 00528030 -call-> 005069d0 -call-> 0056e000 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00593000 | FUN_00593000 | 9 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 005981c0 -vt:CStaticText#20-> 00404110 -thunk-> 00593000 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0056e1c0 | FUN_0056e1c0 | 8 | Y | 004023a1 --> … -call-> 0040764e -vt:CStatusDialogA#55-> 004010d2 -call-> 00402d79 -vt:CPortraitPanel#0-> 004061d6 -thunk-> 0056e1c0 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00581110 | FUN_00581110 | 8 | Y | 004023a1 --> … -call-> 00597ad0 -call-> 00407f90 -virt#50-> 00402603 -virt#51-> 00408b52 -thunk-> 00581110 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 0056e350 | FUN_0056e350 | 8 | ? | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 00593670 -vt:CToolTip#45-> 00401c76 -thunk-> 0056e350 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CGodClient：11 個 / 357 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 004800a0 | FUN_004800a0 | 102 | Y | 00402522 -call-> 004052c2 -call-> 00592c50 -call-> 00405c59 -virt#1-> 00407856 -call-> 004800a0 | 世界座標轉畫面後依畫面中心調整音效句柄；索引無呼叫者，略過未使用入口 |
| 00467fa0 | FUN_00467fa0 | 50 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#12-> 00406163 -virt#21-> 00406df2 -call-> 00467fa0 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |
| 0047ff90 | FUN_0047ff90 | 49 | Y | 00402522 --> … -call-> 004ed440 -call-> 00406b86 -call-> 00467c40 -vt:CGodClient#1-> 004078d3 -call-> 0047ff90 | 錄影模式／當前場景別名檢查後轉接MFX位置調音；平台與錄影包裝，SDL直接管理音訊 |
| 00467da0 | FUN_00467da0 | 43 | Y | 00402522 --> … -call-> 004ed440 -call-> 00406b86 -call-> 00467c40 -vt:CGodClient#0-> 004056b9 -thunk-> 00467da0 | CGodClient deleting destructor：同解構並依旗標釋放0x5fdc bytes；容器／建構解構 |
| 00480650 | FUN_00480650 | 40 | Y | 00402522 --> … -call-> 004ed440 -call-> 00406b86 -call-> 00467c40 -vt:CGodClient#1-> 004078d3 -call-> 00480650 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00468620 | FUN_00468620 | 17 | Y | 004023a1 --> … -vt:CGameDialog#55-> 00405ce5 -call-> 0040764e -call-> 0056dd60 -vt:CGameStatusDialog#1-> 00409075 -call-> 00468620 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |
| 00467f30 | FUN_00467f30 | 14 | Y | 0040499e -call-> 0040141a -call-> 00467f30 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |
| 00468660 | FUN_00468660 | 12 | Y | 004023a1 -call-> 00547720 -call-> 0056c400 -vt:CLoadDialog#10-> 004078a1 -call-> 004b89b0 -vt:PartPic#2-> 00407130 -call-> 00468660 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |
| 004685e0 | FUN_004685e0 | 11 | Y | 004023a1 --> … -call-> 0040495d -call-> 00402171 -call-> 0040230b -call-> 00405a15 -call-> 004685e0 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |
| 00480460 | FUN_00480460 | 11 | Y | 00402522 --> … -call-> 00467c40 -vt:CGodClient#1-> 004078d3 -call-> 00469d70 -call-> 00407dc9 -thunk-> 00480460 | Mfx_SND_PlayEx平台轉接，回傳值非負為成功；SDL音訊播放取代Windows MFX |
| 00468aa0 | FUN_00468aa0 | 8 | Y | 0040499e -call-> 004020ae -call-> 004014ab -call-> 004056dc -call-> 00578f40 -call-> 0040462e -call-> 00468aa0 | 無呼叫者：CGodClient相鄰入口：呼叫索引callers=0，依規則略過，未擴讀本體 |

#### Poq.exe CPlayerDialog：13 個 / 347 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 004f85e0 | FUN_004f85e0 | 101 | Y | 004023a1 --> … -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#11-> 00402c9d -call-> 00404106 -thunk-> 004f85e0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00565e20 | FUN_00565e20 | 73 | Y | 0040499e --> … -call-> 00484ee0 -call-> 00404d4a -call-> 00515e70 -call-> 00401e88 -thunk-> 00565e20 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00566010 | FUN_00566010 | 44 | Y | 004023a1 -call-> 00504d90 -call-> 005212f0 -virt#30-> 00402e5a -call-> 00581a40 -virt#33-> 004084c2 -call-> 00566010 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 00565dd0 | FUN_00565dd0 | 18 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#1-> 00407207 -thunk-> 00565dd0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 005662e0 | FUN_005662e0 | 18 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#15-> 004039cc -thunk-> 005662e0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 005661a0 | FUN_005661a0 | 15 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#55-> 0040836e -thunk-> 005661a0 | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData |
| 00566110 | FUN_00566110 | 14 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#34-> 00403cc9 -thunk-> 00566110 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_00582cb0 依編號取）綁到成員；引擎 ui_load／ui_ |
| 005660c0 | FUN_005660c0 | 13 | Y | 00402522 --> … -call-> 004cab40 -vt:CInitObjMng#2-> 00402a45 -call-> 00480d60 -call-> 004066bd -call-> 005660c0 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 005661f0 | FUN_005661f0 | 12 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#37-> 004026e9 -thunk-> 005661f0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 005662b0 | FUN_005662b0 | 12 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#42-> 0040133e -thunk-> 005662b0 | 連線版介面：死亡後等伺服器復活（JM106 交談輸出、計時重連）、玩家聊天管道、玩家對玩家交換、玩家列表；單機沒有 |
| 00566170 | FUN_00566170 | 11 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#11-> 00402c9d -thunk-> 00566170 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00566140 | FUN_00566140 | 8 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#0-> 00407757 -thunk-> 00566140 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00566160 | FUN_00566160 | 8 | Y | 004023a1 --> … -call-> 00533500 -call-> 004fe900 -call-> 00402be4 -vt:CPlayerDialog#51-> 0040797d -thunk-> 00566160 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CSprSerialPicture：27 個 / 342 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 005980d0 | FUN_005980d0 | 24 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#13-> 00406c58 -thunk-> 005980d0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597ef0 | FUN_00597ef0 | 22 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#20-> 00402e37 -thunk-> 00597ef0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 0049e200 | FUN_0049e200 | 20 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#21-> 00403fe4 -call-> 0049e200 | 精靈群畫目前幀（偏移＋色表參數）：engine draw() 套 −dx −dy 承接（同 0x49dae0） |
| 0049e270 | FUN_0049e270 | 19 | Y | 004023a1 --> … -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#22-> 00405033 -call-> 00403f85 -thunk-> 0049e270 | 精靈群畫目前幀（偏移，參數少一版）：engine draw() 承接 |
| 00597cd0 | FUN_00597cd0 | 17 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#1-> 004087a1 -thunk-> 00597cd0 | 介面類別 deleting destructor（vftable 還原、成員釋放；C 版 free／引擎生命週期） |
| 00598070 | FUN_00598070 | 16 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#15-> 00401dfc -thunk-> 00598070 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597f70 | FUN_00597f70 | 15 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#21-> 00403fe4 -thunk-> 00597f70 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00598160 | FUN_00598160 | 15 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#8-> 0040443f -thunk-> 00598160 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597d50 | FUN_00597d50 | 13 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#19-> 004061a9 -thunk-> 00597d50 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597dc0 | FUN_00597dc0 | 13 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#16-> 004085fd -thunk-> 00597dc0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597e00 | FUN_00597e00 | 13 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#14-> 004060f0 -thunk-> 00597e00 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597fc0 | FUN_00597fc0 | 13 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#22-> 00405033 -thunk-> 00597fc0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00598010 | FUN_00598010 | 13 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#23-> 00409142 -thunk-> 00598010 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 004b18f0 | FUN_004b18f0 | 12 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#13-> 00406c58 -call-> 004b18f0 | 設全域 DAT_00639460（只寫不讀）：沒有作用，不移植 |
| 005981a0 | FUN_005981a0 | 11 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#12-> 00409093 -thunk-> 005981a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597e50 | FUN_00597e50 | 10 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#11-> 00402900 -thunk-> 00597e50 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597d20 | FUN_00597d20 | 9 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#17-> 00408ea4 -thunk-> 00597d20 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00597d40 | FUN_00597d40 | 9 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#2-> 0040614a -thunk-> 00597d40 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00597d90 | FUN_00597d90 | 9 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#3-> 00402e91 -thunk-> 00597d90 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |
| 00597e70 | FUN_00597e70 | 9 | Y | 004023a1 --> … -call-> 00403693 -call-> 00593100 -call-> 00597bf0 -vt:CSprSerialPicture#4-> 0040541b -thunk-> 00597e70 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

#### Poq.exe CProgress：7 個 / 335 行

| 位址 | 名稱 | 行 | 等級 | 路徑 | skip 原因 |
|---|---|---|---|---|---|
| 005972f0 | FUN_005972f0 | 171 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 -vt:CProgress#12-> 00401690 -thunk-> 005972f0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 005971b0 | FUN_005971b0 | 48 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#43-> 00408f71 -thunk-> 005971b0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597080 | FUN_00597080 | 45 | Y | 004023a1 --> … -call-> 00500600 -call-> 00592740 -vt:CUIWinManager#5-> 004072de -virt#36-> 004086f7 -thunk-> 00597080 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 005978a0 | FUN_005978a0 | 38 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 -vt:CProgress#14-> 00401b68 -thunk-> 005978a0 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00597040 | FUN_00597040 | 13 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 -vt:CProgress#11-> 00406fe6 -thunk-> 00597040 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎的 JM 視窗在 m |
| 00596e20 | FUN_00596e20 | 12 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 -vt:CProgress#1-> 00401ebf -thunk-> 00596e20 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生命週期） |
| 00596e80 | FUN_00596e80 | 8 | Y | 004023a1 --> … -vt:CDialogBox#55-> 00402388 -call-> 00406451 -call-> 00596db0 -vt:CProgress#0-> 00405515 -thunk-> 00596e80 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經 MFC 式物件，無遊 |

### 位址段（每 64KB 一段，只列個數與行數）

| bin | 位址段 | 個數 | 行數 |
|---|---|---|---|
| Poq.exe | 004b0000.. | 268 | 11296 |
| Poq.exe | 00590000.. | 253 | 10657 |
| Poq.exe | 00570000.. | 280 | 10545 |
| Poq.exe | 00500000.. | 182 | 10127 |
| Poq.exe | 00580000.. | 258 | 9783 |
| EasyRPG.dll | 10010000.. | 170 | 9635 |
| Poq.exe | 004f0000.. | 177 | 9407 |
| Poq.exe | 00550000.. | 162 | 9050 |
| Poq.exe | 00540000.. | 182 | 8995 |
| Poq.exe | 00520000.. | 83 | 8592 |
| Poq.exe | 00560000.. | 214 | 8546 |
| Poq.exe | 004d0000.. | 226 | 8201 |
| Poq.exe | 00530000.. | 255 | 8191 |
| Poq.exe | 00510000.. | 63 | 8147 |
| Poq.exe | 004a0000.. | 154 | 7465 |
| Poq.exe | 004c0000.. | 180 | 7324 |
| EasyRPG.dll | 10000000.. | 176 | 6902 |
| EasyRPG.dll | 10040000.. | 120 | 6574 |
| Poq.exe | 00460000.. | 128 | 5937 |
| Poq.exe | 00470000.. | 84 | 5664 |
| EasyRPG.dll | 10060000.. | 57 | 5664 |
| EasyRPG.dll | 10050000.. | 217 | 5557 |
| EasyRPG.dll | 10070000.. | 73 | 5351 |
| Poq.exe | 00440000.. | 171 | 4940 |
| EasyRPG.dll | 10030000.. | 65 | 4879 |
| EasyRPG.dll | 10020000.. | 129 | 4619 |
| Poq.exe | 004e0000.. | 65 | 4314 |
| Poq.exe | 005a0000.. | 123 | 3382 |
| Poq.exe | 00490000.. | 159 | 3329 |
| Poq.exe | 00450000.. | 85 | 3188 |
| Poq.exe | 00480000.. | 71 | 2269 |
| UIWndEx.dll | 10000000.. | 58 | 2236 |
| EasyRPG.dll | 100b0000.. | 2 | 535 |
| Poq.exe | 00430000.. | 25 | 510 |
| EasyRPG.dll | 100f0000.. | 2 | 435 |
| EasyRPG.dll | 10200000.. | 14 | 409 |
| EasyRPG.dll | 100a0000.. | 1 | 346 |
| EasyRPG.dll | 100c0000.. | 1 | 292 |
| EasyRPG.dll | 10110000.. | 9 | 289 |
| EasyRPG.dll | 100e0000.. | 1 | 286 |

### 可達 skip 的原始 skip 原因（前 25 種）

| 個數 | 行數 | 原因 |
|---|---|---|
| 747 | 29216 | 解構函數（物件清理；C 版由 free／引擎生命週期取代）：不移植 |
| 388 | 4358 | 介面類別樣板：虛擬函式表轉接／空函式／訊息表取址／建構時只設 vftable（引擎 JM 視窗不經  |
| 324 | 39787 | 介面→本機伺服器訊息層：界面動作經 FUN_N 送封包、讀伺服器資料表（RoleData／PropD |
| 294 | 13484 | 連線版／平台層：CNetGame、CNetItem（socket、執行緒、封包收送）、CUserIn |
| 266 | 9770 | UI 工具箱／平台層：CInterNBase 與控制項基底、JM 介面腳本解析（ |
| 207 | 8543 | 工具函式庫區（記憶體鏈 CMemChain、CDMList 容器、CIniFile 寫入、CDlgF |
| 180 | 6980 | 用戶帳號／存檔檔案／幫派／對話編輯器／字串檔案工具（連線版或原版工具程式，單機沒有；存檔走 engi |
| 103 | 6968 | 介面控制項工具箱（按鈕、輸入框、下拉、列舉框、捲軸、進度、靜態字、提示、視窗管理、動畫控制項）：引擎 |
| 90 | 5710 | 遊戲外殼層：開啟各 JM 視窗的輔助（建立對話框物件、擺位置、記遠端同步資料 GroundData／ |
| 87 | 696 | 建構／解構／STL 容器轉接／欄位存取器（8 行，無字串、不呼叫遊戲函數；引擎用 Actor／Ite |
| 78 | 4304 | 伺服器→客戶端同步封包送出器（N／N 組封包 N／N／N／N…，條件 Friend／本機玩家）：單機 |
| 69 | 4727 | nlohmann::json 函式庫／STL 容器（vector、紅黑樹、迭代器、詞法分析）內部函式 |
| 58 | 2236 | UIWndEx.dll 是 MFC 內嵌網頁視窗外掛（AfxParseURL／CString／HWN |
| 55 | 1649 | 場景初始化物件區的容器／生命週期：SInit* 與 InitBld* 的建構解構、虛擬複製（NpcS |
| 52 | 1840 | 物件表／容器鏈／幫手（DAT_N 物件表、新增刪除、鏈結串列節點）：story_rt.c objs[ |
| 47 | 593 | 介面類別 deleting destructor（呼叫基底解構後 free；C 版 free／引擎生 |
| 40 | 845 | 平台層：DirectInput／輸入伺服器，換成 SDL 事件 |
| 39 | 3575 | STL vector／variant（JSON 式 9 位元組值）樣板函式：STL／連線版 JSON |
| 35 | 823 | EasyRPG.dll 沒有呼叫者（FUN 呼叫、.data 函式表、.text 位址立即值都沒有參 |
| 31 | 2843 | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑） |
| 31 | 1114 | CSprGroup 精靈組（部件群）的載入／複製／取欄位／繪製：引擎 Role／Anim／Sprit |
| 31 | 1273 | 介面取控制項：把視窗的 m_pXxx 控制項（FUN_N 依編號取）綁到成員；引擎 ui_load／ |
| 30 | 1533 | 客戶端同步送出器（人物、道具、狀態、日誌、UI_Pos 通知，封包 0x…）：單機共用物件、引擎直接 |
| 30 | 1241 | CERObject 基底物件方法（屬性表讀寫、子物件鏈、虛擬函式預設值如 slot 11 N 回 0 |
| 26 | 1037 | 資料表／列表物件的建構、清除、複製（STL 式容器轉接），引擎用 C 陣列 |

## 單機可達且「不是明顯平台／解構／容器」的 skip（需要逐一判斷是否有遊戲規則）

共 1535 個、98385 行（可達 skip 的其餘 3417 個多為解構、平台層、容器／STL、UI 樣板，仍可達但多半不含規則，解構要看有沒有副作用）。

已讀本體（`tools/fn.py --brief`）確認：
- Poq 0x4774d0（2110 行）：`#`／`$` 文字命令接收（#ObjSpeed 等），`$` 轉給已 done 的 0x47d4e0；可達（聊天輸入），屬開發／GM 命令，不是劇情分派（劇情分派在別處）。
- EasyRPG 0x10030c90（408 行）：屬性變動重算，Str／Con／Dex／Wisdom 的相依順序，`DAT_10478d40==0` 時走裝備特性清單 → 明顯遊戲規則，單機可達（Y，直接呼叫）。
- EasyRPG 0x10057f50／0x10056120：隊友升級後 Upgrade_ 屬性重算（HP_MAX／MP_MAX 隨等級），解析 "x y z" 字串三個浮點 → 遊戲規則，Y。
- EasyRPG 0x1003d890：選項 `Chosen` 紀錄與迴圈 → 對話選項規則，Y；0x1003f7d0：依房間編號找房名（開場房間查找），Y，原註記「引擎有 story_start_room」，需確認是否等價。
- 其餘「同步封包送出器」「說明文字」「物件表／容器」等依註記判斷屬單機直接讀共用物件或顯示，但**註記是宣稱，不是證據**；可達性分析證明它們在原版單機會被執行，是否等價需逐批對照。

### 可達候選前 60（按行數）

| 位址 | bin | 行 | 等級 | via | 類別 | skip 原因 |
|---|---|---|---|---|---|---|
| 0050c390 | Poq. | 2409 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 004774d0 | Poq. | 2110 | Y | direct |  | " |
| 0052f020 | Poq. | 871 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 005213c0 | Poq. | 859 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0051e160 | Poq. | 832 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00522f00 | Poq. | 720 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00512220 | Poq. | 692 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0051a8e0 | Poq. | 585 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 1007c6c0 | Easy | 460 | Y | direct |  | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日誌 |
| 00554570 | Poq. | 448 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0052b600 | Poq. | 446 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00561720 | Poq. | 423 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 10030c90 | Easy | 408 | Y | direct |  | 屬性變動通知／相依順序重算（Str Con→BD_MAX HP_SPEED、Dex→HR SR、Wisdom→MP_SPEED）後送客戶端： |
| 00543f30 | Poq. | 393 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 1007b490 | Easy | 388 | Y | direct |  | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日誌 |
| 00551c70 | Poq. | 376 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00510310 | Poq. | 343 | Y | direct |  | 客戶端顯示資料表／提示文字／字串清單合併：規則在 main.c 的 item／hero 計算，引擎文字另組 |
| 00550120 | Poq. | 328 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0056d130 | Poq. | 326 | Y | virt |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 1007d3c0 | Easy | 325 | Y | direct |  | 存檔（.cur／.use／.gam）、檔案路徑、日誌寫入工具：引擎有自己的存檔（save.c 路徑）與 printf 日誌 |
| 0054bb30 | Poq. | 320 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0055bb30 | Poq. | 315 | Y | virt |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00514270 | Poq. | 311 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00556f60 | Poq. | 310 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00571160 | Poq. | 302 | Y | virt | CSinglePlayerDia | 讀／存設定檔 ini\user.ini 的 FileData／SaveFolder 設定表（登入帳號清單）：單機設定存檔用 save/ 目錄 |
| 0053cf80 | Poq. | 299 | Y | virt | CMapDialog | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 100274a0 | Easy | 283 | Y | direct |  | 技能名／職業技能表的說明文字輔助（G_Info 組字用），隨說明文字 0x100279b0 一起，引擎沒有滑鼠停留說明 |
| 005252f0 | Poq. | 282 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 005722c0 | Poq. | 278 | Y | virt |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00513a30 | Poq. | 260 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 10040970 | Easy | 252 | Y | direct |  | 客戶端同步送出器（人物、道具、狀態、日誌、UI_Pos 通知，封包 0x…）：單機共用物件、引擎直接讀 |
| 00503f10 | Poq. | 251 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0052ada0 | Poq. | 249 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0051a0f0 | Poq. | 248 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00524a60 | Poq. | 246 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00550980 | Poq. | 243 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 005279b0 | Poq. | 238 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0054dd50 | Poq. | 228 | Y | virt | CPropDialog | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 004629c0 | Poq. | 224 | Y | virt | CGameApp | 平台：MFC InitInstance（記憶體／COM／命令列啟動模式／互斥鎖／安裝登錄檔／MMX／Win32計時器）；SDL 啟動流程代替 |
| 00528030 | Poq. | 220 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00506480 | Poq. | 218 | Y | virt | CStatusDialogA | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0050a3c0 | Poq. | 218 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00516860 | Poq. | 217 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00574e50 | Poq. | 217 | Y | virt |  | CSprGroup 精靈組（部件群）的載入／複製／取欄位／繪製：引擎 Role／Anim／Sprite 載入與 SDL 貼圖（混合模式、顏色 |
| 10043770 | Easy | 217 | Y | direct |  | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0x5fe…，條件 F |
| 100488d0 | Easy | 216 | Y | direct |  | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0x5fe…，條件 F |
| 0054f190 | Poq. | 214 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 004494e0 | Poq. | 212 | Y | virt | ~CGameApp | 平台：列舉 Direct3D 顯示模式（std::vector／std::map） |
| 10049e30 | Easy | 212 | Y | direct |  | 伺服器→客戶端同步封包送出器（0x100646b0／0x10060cd0 組封包 0x51b／0x528／0x5eb／0x5fe…，條件 F |
| 00448d20 | Poq. | 211 | Y | virt | ~CGameApp | 平台：切換顯示模式（解析度表 0=640x480x8 … 6=1024x768、7=1280x720、8=1920x1080；DirectD |
| 0054a0a0 | Poq. | 210 | Y | virt |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 005749e0 | Poq. | 210 | Y | direct |  | CSprGroup 精靈組（部件群）的載入／複製／取欄位／繪製：引擎 Role／Anim／Sprite 載入與 SDL 貼圖（混合模式、顏色 |
| 00461fb0 | Poq. | 209 | Y | direct | ~CGameApp | 沒有呼叫者（FUNCS.tsv callers=0）；不移植此舊啟動路徑 |
| 00510d00 | Poq. | 209 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 005394f0 | Poq. | 208 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 0053a8d0 | Poq. | 207 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00466f30 | Poq. | 205 | Y | direct | ~CGameApp | 客戶端同步層：依角色與第二名單視野聯集重掃場景格，建立客戶端可見物件；原版伺服端到客戶端的可見物件複製，單機共用物件不另建同步副本 |
| 0053dff0 | Poq. | 201 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 00542bb0 | Poq. | 201 | Y | direct |  | 介面→本機伺服器訊息層：界面動作經 FUN_00511a80 送封包、讀伺服器資料表（RoleData／PropData／TradeData |
| 10031f80 | Easy | 201 | Y | direct |  | 技能參數字串化／狀態條目 Status_N 字串（"%d %d %d"）的寫入：引擎 Actor.buff[] 取代 |

## 建議移植批次（候選依 bin、位址排序切段，每批約 45 個，位址相近＝同檔／同類別）

| 批 | bin | 位址範圍 | 個數 | 行數 | 主要註記 |
|---|---|---|---|---|---|
| 1 | Poq.exe | 004346f0–0044afa0 | 45 | 1791 | 劇情沒用到：CGroup／CUserGrou |
| 2 | Poq.exe | 0044b890–0044e120 | 45 | 970 | 轉接到 N（移植那邊時直接接） |
| 3 | Poq.exe | 0044e140–004559d0 | 45 | 727 | 容器：CStaticLinkList<*,N |
| 4 | Poq.exe | 004578b0–00464170 | 45 | 1987 | CString 設定表清除 |
| 5 | Poq.exe | 004641c0–0046c8f0 | 45 | 1685 | 無呼叫者：CGodClient相鄰入口：呼叫 |
| 6 | Poq.exe | 0046ce90–00477430 | 39 | 2071 | 可選Rec錄製／回放專用，單機劇情Recor |
| 7 | Poq.exe | 004774d0–004774d0 | 1 | 2110 | " |
| 8 | Poq.exe | 0047b530–00482750 | 45 | 1641 | 清空CScnGC的全部客戶端節點與三條配對鏈 |
| 9 | Poq.exe | 00482c20–00494b30 | 45 | 1377 | 索引無呼叫者；本輪依規則略過，保留原版入口供 |
| 10 | Poq.exe | 00494b90–0049aef0 | 45 | 533 | 虛擬槽空函數 |
| 11 | Poq.exe | 0049b690–0049ff60 | 45 | 1211 | delete[] 指標包裝：free 取代 |
| 12 | Poq.exe | 004a0050–004a35a0 | 45 | 1048 | 記憶池建立單一池：malloc／free 承 |
| 13 | Poq.exe | 004a6610–00506d20 | 45 | 2183 | CLoadingDialog／CGameIn |
| 14 | Poq.exe | 00506e20–0050bb30 | 45 | 2977 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 15 | Poq.exe | 0050bc30–0050c340 | 11 | 380 | CDMList／連結串列容器節點操作：引擎用 |
| 16 | Poq.exe | 0050c390–0050c390 | 1 | 2409 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 17 | Poq.exe | 00510310–00511bf0 | 8 | 1418 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 18 | Poq.exe | 00512220–00512220 | 1 | 692 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 19 | Poq.exe | 00513a30–0051d270 | 45 | 4539 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 20 | Poq.exe | 0051d310–0051df30 | 6 | 493 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 21 | Poq.exe | 0051e160–0051e160 | 1 | 832 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 22 | Poq.exe | 0051fc40–005212f0 | 18 | 838 | CGameDialog 基底殼：視窗旗標位置 |
| 23 | Poq.exe | 005213c0–005213c0 | 1 | 859 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 24 | Poq.exe | 005229c0–005229c0 | 1 | 146 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 25 | Poq.exe | 00522f00–00522f00 | 1 | 720 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 26 | Poq.exe | 00524a60–0052d1b0 | 45 | 4335 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 27 | Poq.exe | 0052d450–0052ed50 | 12 | 933 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 28 | Poq.exe | 0052f020–0052f020 | 1 | 871 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 29 | Poq.exe | 00531350–0053b5e0 | 45 | 2486 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 30 | Poq.exe | 0053b7a0–005416c0 | 45 | 2709 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 31 | Poq.exe | 00541b50–0054ec50 | 45 | 4238 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 32 | Poq.exe | 0054ed10–005591a0 | 45 | 4558 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 33 | Poq.exe | 00559ec0–005620b0 | 45 | 3642 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 34 | Poq.exe | 00562150–0056aa90 | 45 | 2921 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 35 | Poq.exe | 0056ac30–005749e0 | 45 | 3111 | 介面→本機伺服器訊息層：界面動作經 FUN_ |
| 36 | Poq.exe | 00574cd0–005a25e0 | 34 | 993 | CSprGroup 精靈組（部件群）的載入／ |
| 37 | EasyRPG.dll | 10011080–1001c5b0 | 45 | 2219 | 文字指令（讀過本體）：伺服器→客戶端的場景／ |
| 38 | EasyRPG.dll | 1001c680–10028a20 | 45 | 2450 | CERObject 基底物件方法（屬性表讀寫 |
| 39 | EasyRPG.dll | 10028cc0–1003b910 | 45 | 4963 | BAPA／Job 是幫派（門派）資料：任命、 |
| 40 | EasyRPG.dll | 1003be30–100426b0 | 45 | 2019 | 伺服器→客戶端同步封包送出器（N／N 組封包 |
| 41 | EasyRPG.dll | 10042750–100470c0 | 45 | 2510 | 伺服器→客戶端同步封包送出器（N／N 組封包 |
| 42 | EasyRPG.dll | 10047270–1004fcc0 | 45 | 2477 | 伺服器→客戶端同步封包送出器（N／N 組封包 |
| 43 | EasyRPG.dll | 10050540–10059ed1 | 45 | 2273 | 物件表／容器鏈／幫手（DAT_N 物件表、新 |
| 44 | EasyRPG.dll | 10059f42–1005fe20 | 45 | 1533 | 物件表／容器鏈／幫手（DAT_N 物件表、新 |
| 45 | EasyRPG.dll | 1005ff00–10060990 | 14 | 493 | 封包組裝與文字系統幫手（N 取 dlg 文字 |
| 46 | EasyRPG.dll | 10060dd0–10060dd0 | 1 | 2487 | ER_Explain 的 $ 封包指令分派（ |
| 47 | EasyRPG.dll | 10065060–1007c3c0 | 45 | 3966 | 文字指令表登記與查找（/? /h /help |
| 48 | EasyRPG.dll | 1007c5f0–100f8900 | 34 | 4561 | 存檔（.cur／.use／.gam）、檔案路 |

（單獨成批者為超過 600 行的大函數。批內相依以位址鄰近近似，沒有再做呼叫圖聚類。）

## 驗證

已 done 抽樣 20 個（seed=1）：判成可達 20/20（其中嚴格 Y 15）。
全部 done（1134 個）判成可達 1127 個（99.4%）。

連線類別（CNet*／CServerSocket／CClientSocket／CSocketItem，共 147 個）抽樣 20 個：判成不可達 20。
其中在「不擋連線」的全圖裡有路可達的：147/147（代表圖本身連得到它們，被擋是因為只經過連線節點）。注意：連線類別本來就是封鎖種子，這項只驗證封鎖有生效、沒有從別的路漏進單機，不是獨立證據。
全部連線類別函數判成不可達 147/147。

誠實說明：done 全部都被判成可達的比例就是覆蓋率下限的估計；漏判的 done 函數代表圖有缺口（虛擬呼叫、MFC 訊息表等），skip 的 N 結果因此只能視為「圖上沒路」而非證明。N-dead 的函數須再用 `tools/fn.py` 看本體或查資料區才能放心略過。
