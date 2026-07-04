


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
- [x] TASK-013: Engine — 自由布陣階段（Free setup phase，緬甸象棋）
  - 目標：遊戲開始前讓雙方在己方半場依規則擺放棋子
  - 影響：新增 `phase: 'setup' | 'play'` 在遊戲狀態

#### 8×8 系棋種
- [x] TASK-014: Chaturanga（印度古象棋）
  - 棋盤：8×8 格子
  - 棋子：Raja(王)、Mantri(宰相，一步斜走)、Ratha(車)、Gaja(象，跳兩格斜)、Ashva(馬)、Padati(兵)
  - 升變：兵到底線升為 Mantri
  - 勝負：將死
- [x] TASK-015: Shatranj（波斯象棋）
  - 棋盤：8×8 格子
  - 棋子：Shah(王)、Firzan(宰相，一步斜走)、Rukh(車)、Alfil(象，跳兩格斜)、Faras(馬)、Baidaq(兵)
  - 特殊：stalemate = 對方勝（非平局）；「裸王」(bare king) 視版本可為敗
  - 注意：Alfil 與 Mantri/Firzan 走法相同，可共用 `def`
- [x] TASK-016: Makruk（泰國象棋）
  - 棋盤：8×8 格子
  - 棋子：Khun(王)、Met(后，一步斜走)、Ruea(車)、Khon(象，前一步+斜一步)、Ma(馬)、Bia(兵)
  - 升變：兵到第六橫列(敵方第三行)升為 Met
  - 特殊：終盤計步限制（Counting rules）
- [x] TASK-017: Sittuyin（緬甸象棋）
  - 棋盤：8×8 格子
  - 棋子：Sit-Ke(王)、Thida(后，一步斜走)、Sin(車)、Myin(馬)、Aung-Gway(象，前一步+斜一步)、Nè(兵)
  - **依賴 TASK-013（自由布陣）**：開局雙方在己方半場自由擺放後排棋子
  - 兵開局已在第三行，只能升變在敵方底線
- [x] TASK-018: Ouk Chatrang（柬埔寨象棋）
  - 棋盤：8×8 格子
  - 棋子：與 Makruk 大致相同
  - 特殊：Neang(后)、Trey(象)的首次移動有特殊步法（多走一格）
- [x] TASK-019: Shatar（蒙古象棋）
  - 棋盤：8×8 格子
  - 棋子：Noyon(王)、Bers(后，一步斜或一步正)、Tereg(車)、Mori(馬)、Temee(駱駝，跳3×1)、Khukhuu(兵)
  - 特殊：王不能被將軍（只能被困死）；「Zunn」最後一子規則
- [x] TASK-020: 西藏象棋（Tibetan Chess）
  - 棋盤：8×8（地方版本，以最常見規則為準）
  - 棋子：與 Shatranj/Chaturanga 系相近
  - 注意：規則版本不一，以維基/文獻記載最廣泛版本實作，並在 UI 標注

#### 9×10 系棋種
- [x] TASK-021: 越南象棋（Cờ Tướng）
  - 棋盤：9×10 線交點（與中國象棋完全相同）
  - 棋子：走法與中國象棋相同，僅名稱/外觀不同
  - 差異確認：部分版本象可過河，需查明後決定是否實作分支
- [x] TASK-022: 朝鮮象棋（Janggi）
  - 棋盤：9×10 線交點，**無河界**
  - 將帥：在九宮內可沿斜線走（九宮包含斜線）
  - 象的走法：1步正 + 2步斜（不同於中象的2步斜跳）
  - 馬的走法：1步正 + 1步斜（同中象，但無腿規）—— 需確認
  - 特殊：開局可互換馬象位置；Bikjang（將帥面對面）= 平局提案
  - **無「飛將」，但有「宮車」（車在九宮可走斜線）**

#### 9×9 系棋種
- [x] TASK-023: 將棋（Shogi）
  - **依賴 TASK-012（Drop mechanic）**
  - 棋盤：9×9 格子，p1 在下方（南向）
  - 棋子：玉(王)、飛(飛車)、角(角行)、金(金將)、銀(銀將)、桂(桂馬，前L)、香(香車，直前滑動)、歩(歩兵，前一步)
  - 升變：進入敵方三行（row 0-2 for p2 territory），棋子翻面
    - 飛→龍、角→馬、銀/桂/香/歩→金
  - **棋子外觀**：五角形駒，朝向頂點=自己方向，需特殊繪製
  - Drop 規則：不能打二歩、不能打在最後一行的香/桂、不能打出立即將死

#### UI 擴充
- [x] TASK-024: 選單擴充 — 加入所有新棋種的入口卡片
  - 分組：「古印度/波斯系」「東南亞系」「東亞系」
- [x] TASK-025: 將棋 UI — 五角形駒片渲染（Canvas）
  - 在 `drawPiece` 加入 `isShogi` 分支，繪製五角形並顯示升變狀態
- [x] TASK-026: 手棋面板（Shogi hand display）
  - 在棋盤旁顯示雙方 hand，可點擊 hand 中棋子選擇投入位置

---

### 自訂組軍系統（Draft Engine）— 場地／棋子／布陣自由選配

> 背景：`src/engine`（types/geometry/constraint/interpreter）已是通用引擎，`REGION` 類約束對「場地未定義該區域」預設不限制（見 `constraint.ts` regionFilter），因此棋子與場地的相容性本來就不需要額外擋板，交給玩家自行選配即可。本階段先把「選場地→Ban→Pick/布陣→開局」的骨架與資料結構做出來，棋子點數先用 placeholder，實際數值留待 TASK-033 用模型計算後回填。

- [x] TASK-027: Engine — PieceDef 加入 `cost` 欄位（schema only）
  - 目標：`src/engine/types.ts` 的 `PieceDef` 加 `cost?: number`；`index.html` 現有棋子表比照加上欄位
  - 數值：全部先填 placeholder（如 `1`），不做平衡設計，等 TASK-033
  - 不影響現有棋種的走法邏輯

- [x] TASK-028: 建立跨棋種共用棋子池（Piece Pool Registry）
  - 目標：彙整 `index.html` 內現有全部棋種的棋子定義（國際象棋 + XQ/VN/JG/SHG/CT/ST/MK/SY/OUK/SH/TB）成一份不分棋種的清單
  - 欄位：`name`（顯示名）、`sourceGame`（原棋種，供標註/篩選）、`moves`（沿用既有走法函式或描述）、`cost`（placeholder）、`tags`（如 slider/leaper/royal）
  - 僅做清單彙整，不改變現有各棋種模式下的既有邏輯

- [x] TASK-029: 場地清單資料化（Board Registry）
  - 目標：把現有棋盤幾何（8×8、9×10 有河界/無河界、9×9）與特殊區域（河界、九宮、升變區）整理成獨立 `BoardDef` 清單
  - 供「選場地」畫面使用；場地本身先不綁定任何特定棋種規則，只描述格線與區域
  - 依賴：`src/engine/types.ts` 的 `BoardDef.regions` 結構（已存在，沿用）

- [x] TASK-030: Ban 階段狀態機
  - 目標：新增 `phase: 'ban'`，雙方從 TASK-028 的共用棋子池輪流剔除棋子（次數先寫死，如各 3 次）
  - UI：棋子池網格、已 ban 標記、輪替提示文字
  - 不做智慧 ban 建議或 AI 對手，先支援雙人本機輪流操作

- [x] TASK-031: Pick / 預算布陣階段
  - 目標：新增 `budget: number`（placeholder 值，如 39），玩家在池中選子時扣點數（用 TASK-027 的 placeholder cost），超出預算不可選
  - 布陣沿用 TASK-013 自由布陣的 `setupArmed` / `getSetupHints` 邏輯，僅將「可選清單來源」換成 Pick 階段結果
  - 相容性：允許選擇場地不支援的棋子（例如九宮限定棋子放到無九宮場地）——不擋，維持現有 REGION 約束的寬鬆設計；必要時在 UI 用提示文字標註「此棋子的特殊規則在本場地不生效」

- [x] TASK-032: 組局主流程串接
  - 目標：新增選單入口「自訂組局」，串接 選場地(029) → Ban(030) → Pick/布陣(031) → 開局(既有 render())
  - 驗收：走完整流程可以開始一場雙方陣容/場地皆自訂的對局，並用既有引擎正常判斷合法步、勝負
  - **已知依賴（TASK-031 發現）**：`rawMoves`/`findRuler`/`RULER_DEFS`/`isBareKing` 目前都是用裸棋子名稱（如 `'Ma'`）查表分派，同名棋子在不同棋種代表不同走法（Makruk/Vietnamese/Janggi 的 `Ma` 走法各不相同）。TASK-031 的布陣改用 `PIECE_POOL` 的命名空間 key（如 `'janggi:Ma'`）當 `def` 存放，並已修好 `drawPiece`/`drawShogiPiece`/`updateSetupPhaseUI` 的顯示標籤查找，但走法分派／找王／裸王判定尚未跟進，所以 TASK-031 布陣完成後不會進入可對戰的 `'play'` 狀態。TASK-032 要讓混陣容真正能玩，必須：(1) 讓 `rawMoves` 優先查 `PIECE_POOL_BY_KEY`（一行 early-return，向下相容裸名稱棋種）；(2) 讓 `findRuler`/`isBareKing` 改用 `PIECE_POOL_BY_KEY.get(def)?.tags.includes('royal')` 取代/輔助 `RULER_DEFS` 的裸名稱比對

- [ ] TASK-033: 棋子點數計算模型（**暫緩，優先做 034~038**）
  - 目標：設計並套用一套模型，依據棋子的機動力特徵（可達格數、slide/leap/step、方向數、是否需要特定 region、是否為 royal 等）計算正式點數
  - 輸出：一份 cost 對照表，回填取代 TASK-027/031 的 placeholder
  - 依賴：TASK-027~032 先跑起來，才有實際對局可用來校驗點數是否合理（避免純理論算出來的數字不堪一戰）
  - 註：TASK-034~038 的解鎖價格會直接沿用 `PieceDef.cost` 現有的 placeholder（全部 `1`），等 TASK-033 補上真實數值後兩邊（組局預算、闖關解鎖價）會自動一起變準確，不需要另外改資料結構

### 闖關模式（Campaign / 教學解鎖）

> 背景：讓玩家從最簡單的棋子開始，在縮小版棋盤上一次學一種走法，過關拿點數，點數可以「買」解鎖新棋子——解鎖價格沿用 `PieceDef.cost`，跟組局模式的預算成本共用同一套數值。83 顆 `PIECE_POOL` 棋子裡，很多是同一個走法物件在不同棋種掛不同名字（例如 `MK.Khun`/`SY.SitKe`/`OUK.Sdech`/`SH.Noyon`/`TB.Gyalpo` 全部等於 `CT.Raja`），依「真正走法是否為同一份程式碼／同一份輔助函式」分組後可以收斂成關卡（不會把腿規/過河限制等細節不同的棋子錯誤合併，也不會為完全相同的走法重複出手把關）。解鎖是真的鎖：Pick 畫面預設只顯示已解鎖棋子，但保留「自由模式」開關給不想被鎖住的玩家。
>
> **修正（TASK-038 實作時發現）**：規劃階段估的 37 關是手動心算分組，實際動手把 Shogi 的升變棋子按「呼叫同一個 `shogiGold()` 輔助函式」重新分組後，發現 Kin/Narigin/Narikei/Narikyo/Tokin 這 5 顆其實是同一個動作家族（原本誤以為每個升變棋子要跟它「升變前」的本體配對成一關），Shogi 從估計的 8 關變成 10 關，總數變成 **39 關、6 個世界**。已用程式驗證：39 關的 `unlocks` 陣列聯集起來剛好是 83 顆 `PIECE_POOL` 棋子、無重複無遺漏。

- [x] TASK-034: 闖關資料結構樣板
  - 目標：定義 `CampaignStage`（`id`/`world`/`pieceKeys`（涵蓋的 PIECE_POOL key，同一走法家族全部列入）/縮小版 `board:{cols,rows,regions}`/`setup`/`objective:{type,params}`/`parMoves`/`rewardPoints`）
  - 新增 `CAMPAIGN_SAVE`（`localStorage` 持久化 `unlockedKeys`/`points`/`clearedStageIds`/`bestScoreByStage`），含讀取/寫入/reset 函式
  - 先建 2~3 個代表性關卡（例如 Raja 家族＋Ashva 家族）驗證資料格式可用，其餘 34 關留給 TASK-038 補完（用同格式的最小佔位資料，objective 先全部用「走到指定格」，避免這一步被內容設計卡住）
  - 不做 UI，純資料與存讀檔

- [x] TASK-035: 單人闖關遊玩畫面（`survive` 型態延後到 TASK-038 有實際關卡需要時再做）
  - 目標：新增單人關卡遊玩畫面，沿用既有 `renderBoard`/`drawPiece`/`getLegalMoves`/`rawMoves`/`gameIb` 引擎渲染與走法判斷，但勝負判定換成單人目標型態，不是雙人 `finishTurn`
  - 目標型態（對應 TASK-034 的 `objective.type`）：`reach`（走到指定格）、`captureAll`（N 步內吃光場上棋子）、`survive`（不被任一敵子攻擊範圍威脅地撐過 N 回合／走到安全格）
  - 過關結算：依步數 vs `parMoves` 給分，寫回 `CAMPAIGN_SAVE`
  - 敵子（如果關卡有）不需要真的 AI，先固定不動或照預錄路線移動即可（多數目標型態本來就是單子解謎，只有少數關卡會放靜態敵子讓玩家練習閃避/吃子）

- [x] TASK-036: 世界/關卡地圖選單 + 過關結算畫面
  - 目標：新增闖關地圖畫面，依 `world` 分組動態列出 `CAMPAIGN_STAGES` 的所有關卡卡片（已過關/可挑戰/未解鎖三種狀態；地圖不寫死關卡數，TASK-038 補完後自動變成 39 關），點擊進入 TASK-035 的遊玩畫面
  - 過關結算畫面：顯示本次步數、獲得點數、目前總點數，以及「解鎖棋子」按鈕（消耗點數，對照 `PieceDef.cost`）
  - 選單新增入口「🎓 闖關模式」

- [x] TASK-037: Pick 畫面鎖定狀態 + 自由模式切換
  - 目標：`updatePickUI`（TASK-031）比照 `CAMPAIGN_SAVE.unlockedKeys` 標示鎖定棋子（灰階＋「完成 OO 關解鎖」提示文字，不可點選）
  - 新增「自由模式」開關（例如選場地畫面或 Pick 畫面角落的 checkbox），開啟後 Pick 池全部可選，不受解鎖狀態限制；狀態存 `localStorage`，跨局记住
  - 不改變 Ban 階段：Ban 池維持全部 83 顆棋子可禁用（禁用不等於「選用」，鎖定限制只作用在 Pick 的「加入名單」動作）

- [x] TASK-038: 關卡內容設計（實際 39 關／6 世界，見上方修正說明）
  - 目標：把 TASK-034 佔位的其餘關卡補上實際內容——每關指定 `teachKey`/`unlocks`、縮小棋盤（`board:{cols,rows,regions}`）、`setup`、`objective`、`parMoves`、`rewardPoints`
  - 世界分組（實際）：①8×8 基礎家族 7 關 ②國際象棋 6 關 ③中/越象棋 7 關（含河界/九宮/腿規）④朝鮮象棋 7 關（含宮內斜走/宮車/隔子包） ⑤將棋 10 關（含升變加成、Gold 家族 5 合 1）⑥獨立特例 2 關（Neang/Temee）
  - 部分棋子的特殊規則（palace/own-half 限制）是寫死絕對座標，不是相對棋盤大小算的，所以這幾關的縮小棋盤只能縮窄不能縮矮（例如中象「將」需要完整 rows 0-9 才找得到九宮，只把欄數從 9 砍到 6）
  - 每一關都手動逐步模擬過一次「照設計走完是否真的能過關」，過程中抓到並修正一個真的解不開的關卡設計（見 PROGRESS.md）

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
