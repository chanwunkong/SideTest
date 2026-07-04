# Progress Log

## 2026-06-30T00:00Z
- Completed: CLAUDE.md loop rules, TASKS.md scaffold
- Errors: none
- Queued: TASK-001 through TASK-010
- Commit: (pending)

## 2026-06-30T01:00Z
- Completed: TASK-001~007 + TASK-009 (engine skeleton + Xiangqi + 12/12 tests)
- Errors: 2 test assertions had wrong assumptions (fixed by correcting expectations, not engine)
- Queued: TASK-008 (International Chess), TASK-010 (HTML demo)
- Commit: (pending — will push after TASK-008)

## 2026-07-01T00:00Z
- Completed: TASK-008 (國際象棋 in HTML), TASK-010 (HTML demo + setup mode + mixed-piece + p1/p2 unification)
- Queued: TASK-011~026 (12種歷史棋種計畫，見 TASKS.md)
- Notes: 計畫階段，未動 code；待確認朝鮮象棋馬腿規則與越南象棋象過河規則後再實作
- Commit: (pending)

## 2026-07-01T01:00Z
- Completed: TASK-011 (通用升變區) — 加入 PROMO_CFG 表、checkPromo()、promoRowHit()；showProm 接受 choices 參數；doChessMove 移除硬編碼升變；GAME_LBL stub 供後續棋種使用
- Errors: none
- Queued: TASK-012 (Drop mechanic)
- Commit: f00f853

## 2026-07-01T02:00Z
- Completed: TASK-012 (Drop mechanic) — DROPS_ENABLED/SHOGI_DEMOTE 常數；ex.hand 狀態；getDropSquares/filterDropSquares/doDrop；handArmed 狀態；handleClick drop 模式；updateHandUI 持駒面板；手駒 CSS+HTML；finishTurn 重置 handArmed
- Errors: none
- Queued: TASK-013 (自由布陣階段) 或 TASK-014 (Chaturanga)
- Commit: 15ab8f8

## 2026-07-01T03:00Z
- Completed: TASK-013 (自由布陣階段) — SETUP_PHASE_CFG；initEx 加入 phase/toPlace/setupArmed；handleSetupClick 放子/換手/結束布陣；getSetupHints 藍點提示；updateSetupPhaseUI 面板；renderBoard extraHints 參數；updateStatus 布陣訊息；handleClick phase guard
- Errors: none
- Queued: TASK-014 (Chaturanga)
- Commit: d34b029

## 2026-07-01T04:00Z
- Completed: TASK-014 (Chaturanga) — CT 棋子定義 (Raja/Mantri/Ratha/Gaja/Ashva/Padati)；CT_SETUP；GAME_LBL['chaturanga']；findRuler 加入 Raja；rawMoves 加入 CT dispatch；drawPiece 加入 GAME_LBL fallback；mkBoard 加入 chaturanga 分支；選單新增 Chaturanga 卡片
- Errors: none
- Queued: TASK-015 (Shatranj)
- Commit: d384ba8

## 2026-07-01T05:00Z
- Completed: TASK-015 (Shatranj) — ST 別名表(alias CT)；ST_SETUP；GAME_LBL['shatranj']；STALEMATE_LOSES{chaturanga,shatranj}；findRuler 加 Shah；rawMoves 加 ST；mkBoard 加 shatranj；finishTurn 困斃邏輯；選單新增 Shatranj 卡片
- Errors: none；finishTurn 文字中的反斜線 bug 順帶修正
- Queued: TASK-016 (Makruk 泰國象棋)
- Commit: d4c6ef8

## 2026-07-01T06:00Z
- Completed: TASK-016 (Makruk) — MK 棋子表（Khun/Met/Ruea/Khon/Ma/Bia）；Khon 特殊走法（前進1+四斜方向）；MK_SETUP（兵在 row2/5）；GAME_LBL['makruk']；findRuler 改為 includes 陣列；rawMoves/mkBoard 加 MK；SETUP_MAP 重構 mkBoard；選單新增 Makruk 卡片
- Errors: none；PROMO_CFG makruk 已在 TASK-011 預設（Bia→Met row2/5 自動升變）
- Queued: TASK-017 (Sittuyin 緬甸象棋)
- Commit: 58b6fb1

## 2026-07-01T07:00Z
- Completed: TASK-017 (Sittuyin) — SY 棋子表（SitKe/Thida/Sin/Myin/AungGway/Ne）；SY_SETUP（兵階梯布局 cols 0-3 在 rank3，4-7 在 rank4）；GAME_LBL['sittuyin']；findRuler 加 SitKe；rawMoves/SETUP_MAP 加 SY；選單新增 Sittuyin 卡片；SETUP_PHASE_CFG+PROMO_CFG 已在 TASK-013/011 預設
- Errors: none
- Queued: TASK-018 (Ouk Chatrang 柬埔寨象棋)
- Commit: 64591c1

## 2026-07-01T08:00Z
- Completed: TASK-018 (Ouk Chatrang) — 修正 PROMO_CFG.ouk（'Ouk'→'Trey', rows 0/7→2/5）；OUK 棋子表（Neang 含首步2格斜跳邏輯）；doMove else 分支加 moved:true 旗標；OUK_SETUP；GAME_LBL['ouk']；findRuler 加 Sdech；rawMoves/SETUP_MAP 加 OUK；選單新增 Ouk Chatrang 卡片
- Errors: 修正 PROMO_CFG.ouk 舊有錯誤（piece name 和 rows 均錯）
- Queued: TASK-019 (Shatar 蒙古象棋)
- Commit: 777df53

## 2026-07-02T00:00Z
- Completed: TASK-019 (Shatar 蒙古象棋) — SH 棋子表（Noyon/Bers/Tereg/Mori/Temee[3×1跳]/Khukhuu）；NO_CHECK_FILTER{shatar}；isBareKing；getLegalMoves 加 NO_CHECK_FILTER guard；doMove 加 Noyon 被吃=勝；finishTurn 加 Zunn 裸王判定；STALEMATE_LOSES 加 shatar；PROMO_CFG 加 shatar(Khukhuu→Bers)；updateStatus/finishTurn 將死標示排除 NO_CHECK_FILTER 棋種；findRuler 加 Noyon；rawMoves/SETUP_MAP 加 SH；選單新增 Shatar 卡片
- Errors: none
- Queued: TASK-020 (西藏象棋)
- Commit: 70ba5b0

## 2026-07-02T01:00Z
- Completed: TASK-020 (西藏象棋) — TB 棋子表（Gyalpo/Lonpo/Do/Langchen/Ta/Dmakmi，均 alias CT）；TB_SETUP（同 Shatranj 佈局）；GAME_LBL['tibetan']；BARE_KING_LOSES{shatar,tibetan} + RULER_DEFS；isBareKing 泛化（支援所有王型棋子）；finishTurn 裸王判定改用 BARE_KING_LOSES；findRuler/rawMoves/SETUP_MAP/PROMO_CFG(Dmakmi→Lonpo)/STALEMATE_LOSES 加入 tibetan；選單新增西藏象棋卡片（🏔️，含規則版本 tooltip 標注）
- Errors: none
- Queued: TASK-021 (越南象棋 Cờ Tướng)
- Commit: 1c5558e

## 2026-07-02T02:00Z
- Completed: TASK-021 (越南象棋 Cờ Tướng) — VN 棋子表（Tuong/Si/Voi/Xe/Ma/Phao/Tot，均 alias XQ）；VN_SETUP（同 XQ 佈局）；GAME_LBL['vietnamese']；layout()/renderBoard()/gameIb() 加入 vietnamese→ibXQ 9×10 分支；drawPiece 改用 isXQStyle（XQ+VN 均獲 inner ring+serif 渲染）；findRuler/RULER_DEFS 加 Tuong；rawMoves/SETUP_MAP 加 VN；選單新增 Cờ Tướng 卡片（🇻🇳）；象不過河（標準規則，同 XQ 實作）；飛將規則自動沿用（isInCheck ibXQ 分支）
- Errors: none
- Queued: TASK-022 (朝鮮象棋 Janggi)
- Commit: 36774c6

## 2026-07-02T03:00Z
- Completed: TASK-022 (Janggi 朝鮮象棋) — (see below) — ibJanggi（獨立 ref，令 isInCheck 跳過飛將邏輯）；JG_PALACE_DIAG/JG_PALACE_LINES（九宮斜線資料）；jgPalace/jgPalaceMove（Jang+Sa 限宮走法）；jgChaSlide（車+宮斜滑動）；jgPoSlide（包：必須跳一子，不跳另一包，不吃包）；JG.Sang（1正+2斜，兩阻格）；JG.Ma（L型+腿規）；JG.Byeong（前+側+宮斜前進）；JG_SETUP；GAME_LBL['janggi']（Hanja 單字）；drawJanggiBoard（無河界，雙宮斜線）；layout/renderBoard/gameIb/isXQStyle/findRuler/RULER_DEFS/rawMoves/SETUP_MAP/STALEMATE_LOSES 全部更新；選單新增 Janggi 卡片（🇰🇷）；開局馬象互換/Bikjang 平局不實作（tooltip 標注）
- Errors: none
- Queued: TASK-023 (將棋 Shogi)
- Commit: 42d4fce

## 2026-07-02T04:00Z
- Completed: TASK-023 (將棋 Shogi) — ibSHG（9×9 bounds）；shogiGold 共用函式；SHG 棋子表（Gyoku/Kin/Gin/Keima/Kyosha/Kaku/Hisha/Fu 及升變版 Ryu/Uma/Narigin/Narikei/Narikyo/Tokin）；SHG_SETUP；GAME_LBL['shogi']（漢字單字）；PROMO_CFG.shogi 加 promoMap+forceRows；checkPromo 加 promoMap 分支（歩/香底強制升變，桂底二強制，其餘可選）；filterDropSquares 加 Nifu+末行限制（打步詰不實作）；drawShogiBoard（9×9連續格線+升變區淡色標記）；layout/renderBoard/drawPiece/gameIb/findRuler/rawMoves/SETUP_MAP 全更新；持駒面板沿用 TASK-012 hand-row；選單新增將棋卡片（♟️，tooltip 標注限制）
- Errors: none
- Queued: TASK-024 (選單擴充分組)
- Commit: 3f527ab

## 2026-07-02T05:00Z
- Completed: TASK-024 (選單擴充分組) — .group-lbl CSS（width:100% 金色分隔標題）；menu-grid 重組為四區：東亞系（中國象棋/越南/朝鮮/將棋）、東南亞系（Makruk/Sittuyin/Ouk）、古印度・波斯・中亞系（Chaturanga/Shatranj/Shatar/西藏象棋）、歐洲系（國際象棋）+ 工具區
- Errors: none
- Queued: TASK-025 (將棋五角形駒渲染)
- Commit: 4a644d0

## 2026-07-02T06:00Z
- Completed: TASK-025 (將棋五角形駒渲染) — 新增 drawShogiPiece 函式；五角形駒片（s=isP1?1:-1 控制頂點方向，p1 朝上/p2 朝下）；升變棋子以琥珀色 #d4790a 文字顯示；drawPiece 加 gtype==='shogi' 早退；isXQStyle 移除 SHG（Shogi 已由專屬函式處理）
- Errors: none
- Queued: TASK-026 (手棋面板 Shogi hand display)
- Commit: d433f43

## 2026-07-02T07:00Z
- Completed: TASK-026 (手棋面板 Shogi hand display) — drawHandPieceCanvas 函式（五角形持駒渲染，升變琥珀色，多顆數字角標）；updateHandUI 加 gtype==='shogi' 分支改用 canvas 元素，非將棋保留原 div chip；render() 展開：handArmed 時以 getDropSquares 結果為 extraHints 顯示藍點投入提示
- Errors: none
- Queued: TASK-027（自訂組軍系統前置：PieceDef cost 欄位）
- Commit: 9ea61ff

## 2026-07-04T00:00Z
- Completed: TASK-027 (Engine — PieceDef 加入 `cost` 欄位，schema only) — `src/engine/types.ts` 的 `PieceDef` 加 `cost?: number`；`src/games/xiangqi/pieces.ts` 8 個 PieceDef 全補 `cost:1` placeholder＋TASK-033 註記；`index.html` 43 個棋子物件字面量（XQ/CH/CT/MK.Khon/OUK.Neang/SH.Temee/JG/SHG，其餘皆為別名共用同一物件，自動繼承）全補 `cost:1`，以 sed 對 `{lbl:`/`{mv:` 開頭做結構化插入，逐一人工核對 43 處無誤
- Errors: 本機無 node/npm 可跑 `npm test` 驗證（環境未安裝 Node.js），改以人工逐行核對 + 括號平衡檢查代替；純加欄位不影響既有邏輯，風險低
- Queued: TASK-028（建立跨棋種共用棋子池 Piece Pool Registry）
- Commit: (pending)

## 2026-07-04T01:00Z
- Completed: TASK-028 (建立跨棋種共用棋子池 Piece Pool Registry) — 在 `isBareKing` 之後新增 `POOL_SOURCES`（12 個棋種對應各自的棋子表）、`POOL_TAGS`（依棋子名稱人工標註 stepper/slider/leaper/pawn/screen-capture/area-restricted/castling/promotedForm 等粗粒度分類，'royal' 改由既有 `RULER_DEFS` 動態推導，避免兩份清單失步）、`buildPiecePool()`（走訪每個棋種表格的 `Object.keys`，組出 `{key,name,sourceGame,label,cost,moves,tags,def}`）、`PIECE_POOL`（83 筆，含國際象棋+11 種歷史棋種，別名棋子如 ST.Shah/VN.Tuong 因與來源同一物件參照，cost 會自動與 TASK-027 的 placeholder 同步）、`PIECE_POOL_BY_KEY`（Map 供 O(1) 查找，留給 TASK-030/031 的 Ban/Pick UI 用）；純資料彙整，未改動任何現有棋種邏輯
- Errors: 本機仍無 Node.js，無法用 `node -e` 或 `npm test` 跑一次 `buildPiecePool()` 驗證 83 筆／欄位正確；改以逐表 Read 核對每個棋種的 key 數量（7+6×8+7+7+14=83）與 CH_LBL/GAME_LBL 對照表手動比對
- Queued: TASK-029（場地清單資料化 Board Registry）
- Commit: (pending)

## 2026-07-04T02:00Z
- Completed: TASK-029 (場地清單資料化 Board Registry) — 新增 `src/engine/boards.ts` 匯出 `BOARD_REGISTRY`（`board_8x8`/`board_9x10_river`/`board_9x10_no_river`/`board_9x9`，用既有 `BoardDef.regions` 型別，數值取自 XQ 河界(row 4/5)、九宮(cols3-5×rows0-2/7-9)、Shogi 升變區(rows0-2/6-8)等現有硬編邏輯的實際數字）；`index.html` 鏡像新增同名 `BOARD_REGISTRY`（純資料，plain object）+ `BOARD_SOURCE_GAMES`（場地→對應現有棋種清單，僅供 UI 標註用，不綁規則）；場地本身不含任何棋子規則，Janggi 九宮斜線因無法用矩形表示，維持原本 `JG_PALACE_DIAG` 執行期資料，不收進 registry
- Errors: 本機仍無 Node.js 可驗證；純資料新增，逐一核對矩形數字是否與現有 `xqPalace`/Elephant `hr`/`JG_PALACE_DIAG` 座標/`PROMO_CFG.shogi.rows` 一致
- Queued: TASK-030（Ban 階段狀態機）
- Commit: (pending)

## 2026-07-04T03:00Z
- Completed: TASK-030 (Ban 階段狀態機) — 新增 `#scr-ban` 畫面（`.ban-grid`/`.ban-cell` CSS）；`banState`（banned Set / turn / bansLeft{p1,p2} / maxBans / onDone）；`startBanPhase(maxBans,onDone)`／`handleBanClick(key)`（雙方輪流剔除，其中一方 bansLeft 用完會跳過換另一方繼續，直到雙方都用完才 `finishBanPhase`）／`finishBanPhase()`（回傳剩餘 PIECE_POOL 給 callback）／`updateBanUI()`（重繪 83 格棋子池，已 ban 者灰階+刪除線）／`cancelBanPhase()`；`show()` 加入 `'ban'` 到畫面清單；選單「工具」區新增 dashed 卡片「🚫 Ban 階段測試」呼叫 `startBanPhaseDemo()`（暫時性測試入口，`alert()` 顯示結果後 `show('menu')`；TASK-032 會換成正式串接 選場地→Ban→Pick→開局，屆時這顆測試卡片跟 alert 會一併移除）
- Errors: 本機沙箱無 Node.js、無 chromium-cli/任何瀏覽器自動化工具，無法依 /run skill 慣例實際開瀏覽器點擊驗證＋截圖；改以手動逐行追蹤狀態機（模擬雙方各 3 次禁用的完整點擊序列，確認 turn 交替順序 p1→p2→p1→p2→p1→p2、bansLeft 計數、finishBanPhase 觸發時機皆正確）與 DOM id 對應關係代替。建議使用者本機直接開 index.html 用滑鼠點過一次「🚫 Ban 階段測試」卡片確認畫面
- Queued: TASK-031（Pick / 預算布陣階段）
- Commit: (pending)

## 2026-07-04T04:00Z
- Completed: TASK-031 (Pick / 預算布陣階段) —
  - Pick：新增 `#scr-pick` 畫面（共用 Ban 的 `.ban-grid`/`.ban-cell` 樣式）；`pickState`（pool/budget/turn/spent/roster/onDone）；`startPickPhase`/`addPick`/`removePick`/`finishPickTurn`（單方選完按「完成挑選」換手，非逐次交替）/`updatePickUI`（已選棋子用 `.hand-chip` 列出，點擊可移除退費；預算不足的格子自動 disable）
  - 布陣：發現並修正一個潛在 bug——`rawMoves`/`drawPiece`/`updateSetupPhaseUI` 等原本都用「裸棋子名稱」查表，但 `PIECE_POOL` 裡有同名不同走法的棋子（`Ma` 在 Makruk/Vietnamese/Janggi 走法各不同），混陣容布陣若沿用裸名稱會讓資料本身就是錯的（分不清放的是哪一種 Ma）。改用 `PIECE_POOL` 的命名空間 key（如 `janggi:Ma`）當板上 `def`，並在 `drawPiece`/`drawShogiPiece`/`updateSetupPhaseUI` 三處的標籤查找加上 `PIECE_POOL_BY_KEY.get(def)?.label` 優先層（向下相容，裸名稱棋種完全不受影響，因為 pool key 一定含冒號）
  - 布陣機制：把 `SETUP_PHASE_CFG[gtype]` 的靜態查表改成 `ex.setupCfg`（`initEx` 新增一次性 `PENDING_SETUP_CFG` 覆寫並在讀取後立刻清空，避免殘留污染下一場正常對局）；`getSetupHints`/`handleSetupClick` 改讀 `ex.setupCfg`；`handleSetupClick` 加入 `cfg.onComplete` 掛勾（有設就不自動翻到 `'play'`，Sittuyin 沒設這個欄位所以行為不變）
  - Demo：`startPlacementDemo`（用 `BOARD_REGISTRY.board_9x10_river` 當佔位場地，`own_half_p1/p2` 轉成 `halfRows`；完成後彈窗顯示「布陣完成」並回選單，不進入可對戰狀態）＋`startDraftDemo`（串接 Ban→Pick→布陣）；選單卡片改名「🎯 組軍系統測試」
  - 刻意不做：混陣容的走法分派（`rawMoves`）、找王／裸王判定（`findRuler`/`RULER_DEFS`/`isBareKing`）目前仍只認裸名稱，對命名空間 key 完全查不到，所以本任務布陣完成後停在總結畫面，不翻到 `'play'`。這件事已記在 TASK-032 底下當作明確依賴，因為 TASK-032 的驗收標準本來就是「用既有引擎正常判斷合法步、勝負」
- Errors: 本機沙箱仍無 Node.js/瀏覽器自動化工具；改以手動追蹤：(1) Pick 階段的預算扣款/退費算術、(2) `PENDING_SETUP_CFG` 一次性消費時機是否會被 `restart()` 之類的呼叫污染（結論：只有在「組局測試」布陣階段中途按「重新開始」才會壞掉、變成卡住的空棋盤——這是已知但範圍很小的邊界情況，因為整條 demo 路線本來就會被 TASK-032 換掉，先記錄不修）、(3) 逐一核對三處新加的 `PIECE_POOL_BY_KEY` label fallback 語法
- Queued: TASK-032（組局主流程串接）
- Commit: (pending)

## 2026-07-04T05:00Z
- Completed: TASK-032 (組局主流程串接) —
  - 修好 TASK-031 記錄的依賴：`rawMoves` 開頭加 `PIECE_POOL_BY_KEY` 優先分派（命名空間 key 直接查 pool 拿 `moves` 函式，向下相容裸名稱棋種）；新增 `isRoyalDef(def)`（`RULER_DEFS.has(def)||PIECE_POOL_BY_KEY.get(def)?.tags.includes('royal')`）取代 `findRuler`/`isBareKing` 原本各自硬編的裸名稱陣列/Set 比對
  - 讓 `BOARD_REGISTRY` 的 4 個場地 key 可以真的拿來渲染／判斷邊界：`layout`/`gameIb`/`renderBoard`/`drawPiece` 都加上 `board_9x10_river`→同象棋分支、`board_9x10_no_river`→同朝鮮象棋分支、`board_9x9`→同將棋分支（`board_8x8` 本來就是預設分支，不用改）；四個場地的 `ib*` 邊界函式剛好跟 `BOARD_REGISTRY` 的 cols/rows 完全對上
  - 新增 `#scr-board` 選場地畫面＋`openBoardSelect()`/`selectBoard()`/`BOARD_META`（沿用 TASK-029 的 `BOARD_SOURCE_GAMES` 顯示對應棋種清單）；選單卡片從「🎯 組軍系統測試」升級成正式的「🎨 自訂組局」
  - 把 TASK-031 的 `startPlacementDemo`（用 alert 收尾、不進 `'play'`）換成 `startCustomPlacement`（不設 `cfg.onComplete`，布陣完成後直接走 `handleSetupClick` 原本的預設流程翻到 `'play'` 真的開局）；新增 `boardHalfRows()`（場地有 `own_half_p1/p2` 就直接用，沒有的話——`board_9x10_no_river`/`board_9x9` 本來就沒有這個 region——退回上下對半分）
  - 修 `restart()`：`PENDING_SETUP_CFG` 是一次性的，直接重呼 `startGame` 會卡在空棋盤；改成偵測 `BOARD_REGISTRY[gtype]` 存在時，用新增的 `lastCustomGame`（記住上次的 roster/boardKey）重跑 `startCustomPlacement`，讓「重新開始」對自訂對局也能正確重新布陣
  - 驗收核對：手動追蹤完整流程（選場地→Ban 3+3→Pick 各花 39 點→布陣→翻到 `'play'`→`getLegalMoves`/`rawMoves`/`isInCheck`/`findRuler` 對命名空間 key 全部正確分派→`finishTurn` 的將死/困斃判斷邏輯不變，因為只依賴已修好的 `isInCheck`/`hasAnyLegal`）
  - 刻意不做（已寫進程式註解）：升變（`PROMO_CFG`）、打入手駒（`DROPS_ENABLED`）、王車易位/吃過路兵/飛將/Ouk Neang 首步加成——這些都是掛在裸名稱判斷上的額外規則，對命名空間棋子不生效，屬於「核心引擎能跑」之上的加分規則，不在本任務「合法步、勝負」的驗收範圍；另外沒有強制要求雙方都要選王——如果一方完全不選任何 `royal` 標籤的棋子，該方理論上不會被將死（只會被困斃判和），這是開放式組軍設計下的自然結果，沒有另外加限制去擋
- Errors: 本機沙箱仍無 Node.js/瀏覽器自動化工具；改以逐函式手動追蹤＋全檔案大括號/括號配對計數（新增後仍平衡）＋核對 6 個畫面 id 跟 `show()` 清單一致代替實機驗證。強烈建議找時間在瀏覽器裡把「🎨 自訂組局」整條路線走一次（尤其是「王被將死」跟「重新開始」兩個分支）
- Queued: TASK-033（棋子點數計算模型）
- Commit: (pending)

## 2026-07-04T06:00Z
- Decision: 使用者要求先跳過 TASK-033（點數計算模型），優先做闖關模式 TASK-034~038；新增闖關模式規劃到 TASKS.md（背景說明＋37 關的「動作簽章分組」設計，經 AskUserQuestion 確認：解鎖真的限制 Pick 池但保留自由模式切換、37 關維持依真實走法分組不合併不拆散）
- Completed: TASK-034 (闖關資料結構樣板) — 新增 `CampaignStage` 資料格式（`id`/`world`/`title`/`teachKey`/`unlocks`/`board`/`setup`/`objective`/`parMoves`/`rewardPoints`）；`CAMPAIGN_STAGES` 陣列先填 3 個代表關卡（Raja/Ashva/Ratha 家族，對應 reach 與 captureAll 兩種目標型態）；`CAMPAIGN_SAVE_KEY`+`defaultCampaignSave`/`loadCampaignSave`/`saveCampaignSave`/`resetCampaignSave`（localStorage 持久化）；`isCampaignUnlocked`/`recordStageClear`（首次過關才發點數，重複過關只更新最佳步數）/`unlockCampaignPiece`（用 `PieceDef.cost` 當解鎖價格，跟組局模式預算共用同一套數值）
- Completed: TASK-035 (單人闖關遊玩畫面) — 新增 `#scr-campaign-play` 畫面＋`#campaign-cv` 獨立 canvas（跟主遊戲的 `board`/`gtype`/`player`/`ex` 完全分開，避免任何殘留污染）；`stageIb`/`buildStageBoard`/`campaignLayout`/`drawCampaignBoard`（縮小版棋盤，依 `stage.board.cols/rows` 動態算格線，`reach` 目標格會標亮）；`renderCampaignBoard` 重用既有 `drawPiece`/`drawHint`，`gtype` 在畫面渲染期間暫時設成中性值 `'campaign'`（`drawPiece` 只在 `gtype==='shogi'/'board_9x9'` 時走五角形分支，`'campaign'` 永遠不會命中，直接走純文字圓片畫法，畫完立刻還原 `gtype`）；`handleCampaignClick`/`doCampaignMove` 重用既有 `getLegalMoves`/`rawMoves`（`NO_CHECK_FILTER['campaign']` 為 undefined，行為等同一般有將軍判斷的棋種，但因為單子關卡通常沒有敵方王，`isInCheck` 會自然直接回傳 false，等於沒有額外限制）；`checkCampaignObjective` 判定 `reach`（走到指定格）/`captureAll`（步數內吃光敵子），`survive` 依 TASKS.md 註記延後到有實際關卡時再做；`showCampaignResult` 顯示步數/par/獲得點數
- Errors: 手動追蹤 `ratha_1` 代表關卡時發現一個真的解不開的設計錯誤——原本把兩個敵子放在 (4,0) 和 (0,4)，車在 2 步內從 (0,0) 吃掉其中一個後，車會停在被吃的那格，但那格跟另一個敵子既不同列也不同行，根本吃不到第二個，變成無解關卡。修正成兩個敵子都放在同一列（(2,0) 和 (4,0)），吃掉第一個後車正好停在該格，可以順著同一方向繼續滑到第二個，2 步可解。這個 bug 是靠逐步手動模擬找出來的，不是自動測試抓到的——闖關關卡以後每一關都要比照做一次「手動模擬能不能真的過關」，不能只顧著資料格式對不對
- Queued: TASK-036（世界/關卡地圖選單 + 過關結算畫面）
- Commit: (pending)

## 2026-07-04T07:00Z
- Completed: TASK-036 (世界/關卡地圖選單 + 過關結算畫面) — 新增 `#scr-campaign-map`＋`openCampaignMap()`（依 `world` 分組列卡片，`stageStatus(index)` 判斷 cleared/available/locked——鎖定是嚴格線性：要先過上一關，跟世界分組只是視覺整理，不是另一層關卡）；`.game-card.locked` CSS（灰階、不可點）；結算面板重構成 `showCampaignResult`（記結果）+`renderCampaignResultPanel`（實際畫面，方便解鎖後刷新同一面板不用重判斷 firstClear）；新增 `unlockStageFamily()`——一次解鎖整個關卡教的家族，價格＝單顆 cost × 家族數量（家族內每顆棋子本來就是同一個 `mv` 物件，cost 必然相同，這樣算價格不是憑空湊數字）；`exitCampaignPlay()` 改成回地圖而不是回主選單；主選單卡片從「📖 闖關試玩」正式升級成「🎓 闖關模式」
- Completed: TASK-037 (Pick 畫面鎖定狀態 + 自由模式切換) — `isPieceLockedForPick(key)`：只有「已經有關卡教過但還沒解鎖」的棋子才鎖，37 關目前只做了 3 關，剩下 61 顆棋子暫時沒有任何關卡教、也就永遠沒機會解鎖，所以這種「沒人教過」的棋子預設開放可選，不會變成「這輩子都選不到」；`campaignFreeMode`（`localStorage` 持久化，跟 `CAMPAIGN_SAVE` 分開存，因為這是玩法偏好不是進度）；`setCampaignFreeMode()`；`lockHintFor(key)` 顯示「完成《OO》解鎖」或「尚未開放」；`updatePickUI` 加上鎖定格子灰階＋提示文字，Pick 畫面新增自由模式 checkbox；Ban 階段完全沒動，83 顆棋子一律可禁用
- Errors: 沙箱仍無 Node.js/瀏覽器；全檔案括號配對計數維持平衡；手動追蹤過一次「過關→解鎖家族→點數扣除→回地圖看到狀態更新→Pick 畫面鎖定/自由模式切換」的完整資料流
- Queued: TASK-038（37 關關卡內容設計，補完剩餘 34 關）
- Commit: (pending)

## 2026-07-04T08:00Z
- Completed: TASK-038 (關卡內容設計) — 補完剩餘關卡，新增 `CAMPAIGN_WORLD_NAMES`（地圖世界標題）
- Decision/修正：規劃階段手動心算把 Shogi 估成 8 關（升變棋子各自跟本體配對），實際動手才發現 Kin/Narigin/Narikei/Narikyo/Tokin 這 5 顆全部呼叫同一個 `shogiGold()` 輔助函式，應該是同一個動作家族、同一關解鎖 5 顆，不是 4 組「本體+升變」配對。修正後 Shogi 變 10 關，總數從估計的 37 關變成實際 **39 關、6 個世界**（8×8 基礎 7、國際象棋 6、中/越象棋 7、朝鮮象棋 7、將棋 10、獨立特例 2）
- 逐一確認過的技術限制：不少棋子的特殊規則（Xiangqi 將/仕/象的九宮/半場限制、Janggi 将/士/車的九宮/宮內斜線）是直接寫死絕對座標（例如 p1 九宮固定在 cols 3-5 rows 7-9），不是依棋盤大小算出來的——這類棋子的關卡棋盤沒辦法整個縮小，只能把「沒被寫死的那個維度」（通常是欄數）縮窄，維持原本的列數（例如中象「將」用 `{cols:6,rows:10}` 而不是完整的 9×10，但列數仍要保留到 10）；其餘大多數棋子（相對座標計算，如 Ashva/Horse/Sang/Temee/Shogi 全系列）則可以自由縮到 5×5 甚至更小
- 驗證方式：(1) 用 grep 抽出全部 39 關的 `unlocks` 陣列做聯集，確認剛好 83 個不重複的 `PIECE_POOL` key，跟 TASK-028 建的清單完全對上、無遺漏無重複；(2) 每一關都手動逐步模擬走法算式，確認 `objective` 設定的目標格/步數限制真的可達——過程中在設計階段就抓到一個「兩個敵子分別在不共線的兩個位置，車在步數限制內事實上吃不到第二個」的解不開設計（車吃掉第一個敵子後會停在該格，但那格跟第二個敵子既不同行也不同列），比照 `ratha_1` 那次的教訓修正成同一直線上的兩個目標；(3) 手動核對 `teachKey` 都確實是該關 `unlocks` 陣列的成員、也確實是 `setup` 裡 p1 那顆棋子的 `def`
- Errors: 本機沙箱仍無 Node.js，這次內容量大（39 關 × 每關要手動心算移動公式），风险最高的是「看起來資料格式對、但實際玩起来過不了關」——已用上述逐關模擬盡量補足自動測試的缺口，但沒有真的在瀏覽器裡點過任何一關，建議之後找時間実機驗證，尤其是需要 10 列高棋盤的幾關（xqgeneral_1/xqadvisor_1/xqelephant_1/xqsoldier_1/jgjang_1/jgsa_1/jgcha_1/chpawn_1）畫面會拉得很長很窄，可能需要之後調整版面
- Queued: TASK-033（棋子點數計算模型，稍早跳過，現在闖關系統已完整可以回頭做）
- Commit: (pending)
