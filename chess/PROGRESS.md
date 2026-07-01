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
- Commit: (pending)
