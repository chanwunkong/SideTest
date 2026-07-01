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
- Commit: (pending)
