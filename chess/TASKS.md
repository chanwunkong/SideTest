


# Task Queue

## Active

### 歷史棋種實作計畫（12 variants）

#### 前置：引擎擴充
- [x] TASK-011: Engine — 通用升變區（generalized promotion zone）
  - 目標：讓任意棋種的任意棋子可在指定區域升變，不限於國際象棋兵
  - 影響檔案：`index.html` 中的 `doMove` / `finishTurn` 流程
- [x] TASK-012: Engine — 棄子（Drop mechanic，将棋專用）
  - 目標：吃到的棋子進入「手棋」(hand)，可在己方回合放回棋盤
  - 新狀態：`hand: {p1: Map<def, count>, p2: Map<def, count>}`
  - 需新增 `DROP` 類動作在合法步生成
- [ ] TASK-013: Engine — 自由布陣階段（Free setup phase，緬甸象棋）
  - 目標：遊戲開始前讓雙方在己方半場依規則擺放棋子
  - 影響：新增 `phase: 'setup' | 'play'` 在遊戲狀態

#### 8×8 系棋種
- [ ] TASK-014: Chaturanga（印度古象棋）
  - 棋盤：8×8 格子
  - 棋子：Raja(王)、Mantri(宰相，一步斜走)、Ratha(車)、Gaja(象，跳兩格斜)、Ashva(馬)、Padati(兵)
  - 升變：兵到底線升為 Mantri
  - 勝負：將死
- [ ] TASK-015: Shatranj（波斯象棋）
  - 棋盤：8×8 格子
  - 棋子：Shah(王)、Firzan(宰相，一步斜走)、Rukh(車)、Alfil(象，跳兩格斜)、Faras(馬)、Baidaq(兵)
  - 特殊：stalemate = 對方勝（非平局）；「裸王」(bare king) 視版本可為敗
  - 注意：Alfil 與 Mantri/Firzan 走法相同，可共用 `def`
- [ ] TASK-016: Makruk（泰國象棋）
  - 棋盤：8×8 格子
  - 棋子：Khun(王)、Met(后，一步斜走)、Ruea(車)、Khon(象，前一步+斜一步)、Ma(馬)、Bia(兵)
  - 升變：兵到第六橫列(敵方第三行)升為 Met
  - 特殊：終盤計步限制（Counting rules）
- [ ] TASK-017: Sittuyin（緬甸象棋）
  - 棋盤：8×8 格子
  - 棋子：Sit-Ke(王)、Thida(后，一步斜走)、Sin(車)、Myin(馬)、Aung-Gway(象，前一步+斜一步)、Nè(兵)
  - **依賴 TASK-013（自由布陣）**：開局雙方在己方半場自由擺放後排棋子
  - 兵開局已在第三行，只能升變在敵方底線
- [ ] TASK-018: Ouk Chatrang（柬埔寨象棋）
  - 棋盤：8×8 格子
  - 棋子：與 Makruk 大致相同
  - 特殊：Neang(后)、Trey(象)的首次移動有特殊步法（多走一格）
- [ ] TASK-019: Shatar（蒙古象棋）
  - 棋盤：8×8 格子
  - 棋子：Noyon(王)、Bers(后，一步斜或一步正)、Tereg(車)、Mori(馬)、Temee(駱駝，跳3×1)、Khukhuu(兵)
  - 特殊：王不能被將軍（只能被困死）；「Zunn」最後一子規則
- [ ] TASK-020: 西藏象棋（Tibetan Chess）
  - 棋盤：8×8（地方版本，以最常見規則為準）
  - 棋子：與 Shatranj/Chaturanga 系相近
  - 注意：規則版本不一，以維基/文獻記載最廣泛版本實作，並在 UI 標注

#### 9×10 系棋種
- [ ] TASK-021: 越南象棋（Cờ Tướng）
  - 棋盤：9×10 線交點（與中國象棋完全相同）
  - 棋子：走法與中國象棋相同，僅名稱/外觀不同
  - 差異確認：部分版本象可過河，需查明後決定是否實作分支
- [ ] TASK-022: 朝鮮象棋（Janggi）
  - 棋盤：9×10 線交點，**無河界**
  - 將帥：在九宮內可沿斜線走（九宮包含斜線）
  - 象的走法：1步正 + 2步斜（不同於中象的2步斜跳）
  - 馬的走法：1步正 + 1步斜（同中象，但無腿規）—— 需確認
  - 特殊：開局可互換馬象位置；Bikjang（將帥面對面）= 平局提案
  - **無「飛將」，但有「宮車」（車在九宮可走斜線）**

#### 9×9 系棋種
- [ ] TASK-023: 將棋（Shogi）
  - **依賴 TASK-012（Drop mechanic）**
  - 棋盤：9×9 格子，p1 在下方（南向）
  - 棋子：玉(王)、飛(飛車)、角(角行)、金(金將)、銀(銀將)、桂(桂馬，前L)、香(香車，直前滑動)、歩(歩兵，前一步)
  - 升變：進入敵方三行（row 0-2 for p2 territory），棋子翻面
    - 飛→龍、角→馬、銀/桂/香/歩→金
  - **棋子外觀**：五角形駒，朝向頂點=自己方向，需特殊繪製
  - Drop 規則：不能打二歩、不能打在最後一行的香/桂、不能打出立即將死

#### UI 擴充
- [ ] TASK-024: 選單擴充 — 加入所有新棋種的入口卡片
  - 分組：「古印度/波斯系」「東南亞系」「東亞系」
- [ ] TASK-025: 將棋 UI — 五角形駒片渲染（Canvas）
  - 在 `drawPiece` 加入 `isShogi` 分支，繪製五角形並顯示升變狀態
- [ ] TASK-026: 手棋面板（Shogi hand display）
  - 在棋盤旁顯示雙方 hand，可點擊 hand 中棋子選擇投入位置

---

## Completed

- [x] TASK-001: Project scaffold (package.json, tsconfig, folder structure)
  - [x] TASK-001a: Write package.json + tsconfig.json
  - [x] TASK-001b: Create src/ folder structure
- [x] TASK-002: Core type system (types.ts)
- [x] TASK-003: Geometry Engine (STEP / SLIDE / LEAP / VECTOR)
- [x] TASK-004: Constraint Engine (REGION / PATH_EMPTY / SCREEN / BLOCK_CELL / STATE)
- [x] TASK-005: Action Engine + Composite Action (MOVE / CAPTURE / DROP / COMPOSITE)
- [x] TASK-006: State Engine + Move Validation Pipeline
- [x] TASK-007: Xiangqi game definition (馬、炮、車、象、士、卒、將)
- [x] TASK-008: International Chess (index.html) — King/Queen/Rook/Bishop/Knight/Pawn + castling + en-passant + promotion
- [x] TASK-009: Unit tests — 12/12 passing
- [x] TASK-010: HTML frontend demo (index.html) — 棋盤渲染、選子移子、勝負判定、Setup 模式、混棋支援
