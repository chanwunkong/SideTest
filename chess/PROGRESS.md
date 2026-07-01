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
- Commit: (pending)
