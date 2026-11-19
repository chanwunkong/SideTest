# QinPort 完整移植佇列（接力用，額度中斷後照這份繼續，不要問使用者）

目標：做出完整可玩的《秦殤》（含非劇情模式）。使用者明確要求：不接受「自行判斷略過」「簡化版當完成」。只有 tools/reach.tsv 證明「只能從連線模式到達」的函數才可以不移植。查不到原版規則時，回報裡寫「原版規則沒找到」並列出函數，不要編造。

## 新策略：資料驅動補齊，函數層級只用來查原版規則（2026-11-19）
逐個函數移植太慢（單機可達 skip 約 1000 個）。改成：`python3 -I tools/coverage.py` 從資料（Data／ObjMdl／scene／Rec／story）統計「遊戲真的會出現的機制」，輸出 tools/coverage.md（各項資料次數＋引擎 完整／部分／無／?）。
只補矩陣裡「無」「部分」，依資料次數由多到少；需要原版規則時才用 `python3 tools/fn.py --brief`／`r2` 查函數。改了引擎就更新 coverage.py 開頭的 S[...] 狀態表並重跑。
**本輪（資料驅動第 1 輪）補了**：對話語音 `audio_voice`（Rec '@' 4，game/sound/NNN.wav，919/935 個鍵有檔）、Rec '@' 11 音效、BNT 觸發區 `openall <別名>`（shhl_neicheng 約 90 處，原本只認 s_pctstart）。
**矩陣裡尚未補的（依優先序；資料驅動第 2 輪 2026-11-19 已劃掉做完的）**：
1. ~~Rec '#' 7 建築狀態~~：做了（編號 − 2001 = BNT 順序，0x4a2600 配號），標「部分」——同場景第 2 次進入偏移差 1、地上道具也佔編號，原版規則沒找到。
2. ~~Rec '@' 4／6 對話框~~：做了（原版就是頭頂文字框 0x4722f0／0x5963e0，沒有說話者名與頭像；語音播完才收；框色字色沒查到）。
3. ~~'#' 1 子類 1、'#' 3 子碼、'@' 8／10／12~~：做了。'#' 6（地上道具）資料封包比原版讀的欄位短，原版規則沒找到；'#' 8／9 要 MethodMain 出手動畫（0x43acb0／0x43af50／0x43c4f0／0x450700 的 CARROW 等），**還沒做**。
4. ~~（2026-11-19 接力第 2 輪做完）~~ Attack 11 分身 0x1003df00（複製施放者裝備子物件與技能，技能等級上限 9 → 0x10037d70；原版規則已讀，缺「分身有自己的技能列」模型）；偷竊／開鎖 0x10034010／0x10034900（Attack 5／6，資料 0 次）；銀行 0x1003ac40；丟錢 0x1003b640／0x10054830；0x1003de10、0x10035540、0x10038240、SetSkill。
5. 章節配音 `sound\C%.3d.wav`（Poq 0x566920／0x566860，c011…）、天氣疾風與色調、CZhenMng、CGroup、錄影回放除錯指令；Rec 多場景（#13 子類 2 換場景）原版載另一份場景拷貝，這裡只標 foreign 擋建築封包，角色封包還是畫在原房間。
6. 矩陣 `?` 還剩 19 項：狀態表 9 欄（temp、全身变色、不发送到Client、缩放、旋转、幻影移动、武器变色、幻影攻击、头变色）是否逐欄消費、技能死亡方式 8 種（actor_kill 固定 mode 1，原版把技能欄 12 傳給 0x486910 的 mode 沒查）、參數 69、屬性 40、地圖視窗 JM60 用 "Map" 遮罩只顯示去過的地點（site 序號 ↔ Map 號對照沒查）。

## 每一批的做法（Sonnet 子代理，model=sonnet，主樹一次只能一個）
1. 派 general-purpose 子代理，prompt 要包含：照 tools/CODEX_TASK.md 的規則；不要 git commit／push；網頁版只在 /tmp 複本編（`rsync -a --exclude game --exclude decomp --exclude extracted ../ /tmp/qbN/qinport/`），不動 qinport/web/ 與 8642／8643 伺服器；驗證 `cd engine && make && make skilltest 2>&1 | tail -3` 印 skilltest ok、`make web` 能編、動到 story 就跑 `make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1`；更新 HANDOFF／roles／fields／FUNCS（`python3 tools/functool.py`）。
2. 子代理回報後我（主對話）重跑 make／skilltest／storytest，通過才 `git add -A ':(icase)qinport' ':(icase,exclude)qinport/tools/decomp_strings.py'` commit（結尾 Co-Authored-By 行照系統提示）並 push origin qinport。
3. 更新給使用者試玩的穩定版：`rsync` 複本 → `cd /tmp/qbuild/qinport/engine && make web` → 把 web/ 的 index.html index.js index.wasm sw.js manifest.webmanifest *.png 複製到 /tmp/qin-web/（8643 伺服器：`python3 tools/webserve.py /tmp/qin-web game 100.83.41.116 8643`，沒在跑就重開）。
4. 立刻派下一批，不等使用者。

## 佇列（依序；做完一項在這裡劃掉並寫 commit）
- [ ] 世界物件互動：砍樹／挖礦／打水缸（BNT 類型 4，原版點擊命令 0x80 打、0x85 開、0x96 門 0x10、0x17→0x11 撿）、點門走到門點（代理 a2c2b57 進行中；回報後驗證 commit）
- [ ] EasyRPG 規則函數：0x10030c90（屬性重算 408 行）、0x10057f50（隊友升級屬性）、0x1003d890（對話選項 Chosen）
- [ ] Poq「介面→本機伺服器訊息層」324 個（約 4 萬行，tools/reach.tsv 的 skip_note 含「介面→本機伺服器」）：逐個與引擎現有實作比對是否等價，不等價照原版改；原先「引擎已有替代」的宣稱都要驗證
- [ ] 0x50c390（2409 行）、0x4774d0（2110 行，`#`／`$` 文字命令接收：GM／除錯／聊天指令，單機可用的要做）
- [ ] tools/reach.tsv 其餘「單機可達（Y 或 ?）」skip（約 1535 個、9.8 萬行，扣掉純解構／平台層／容器後），依 reach_summary.md 的 48 批切法，每批約 45 個
- [ ] 補件：tools/reach.tsv 沒解開的 174 處函式指標呼叫與 Y 偏寬的虛擬呼叫，重跑 `python3 tools/reach.py`
- [ ] 之前列的缺口：技能打造（扣精氣 (等級+10)×5、五行換算 0x10058900、m_pProductFinish、m_pMake／m_pEnchase／m_LookOver）、商店修理／鑑定、聲望與五行攻防、六項屬性加值回算、超重速度懲罰、分身換裝、色表／混色／光影模式、章節配音與音效、設定視窗項目生效、標題畫面、小地圖校對、遮擋排序、遊戲速度、劇情命令分派其餘 case（# 6/7/8/9、@ 5/8/10/11/12）、隊友裝備外觀頭像、死亡介面觸發點、dead_obj 缺口
- [ ] 之前「劇情沒用到」清單：天氣疾風與色調、銀行、丟錢、開鎖偷竊、遊戲時鐘設定、CZhenMng、CGroup、錄影回放、除錯指令
- [ ] 使用者延後：片頭影片卡頓（之後處理）

## 狀態備忘
- 穩定版試玩網址：http://100.83.41.116:8643/（Tailscale）；PWA 完整離線快取需要 https（Tailscale serve），步驟在 HANDOFF。
- 測試瀏覽器：用已裝的 Google Chrome 經 CDP（tools/webdrive_example.mjs、webdrive_cells.mjs、firstmap_check.sh、pwa_check.mjs）；不要安裝任何東西（Playwright 安裝被分類器擋過）。
- 被分類器擋過：npm／playwright 安裝。funcs_skip.txt 的編輯曾被擋，使用者授權後可改。
- 使用者要求每批結果誠實回報：哪些只靠編譯／單元測試、沒在實機驗證。

## 接力第 2 輪（2026-11-19）結果
- 做了：分身技能列（clone_skills）、銀行 bank_op＋B 鍵視窗、丟錢（背包 m_pMoney → JM109）、偷竊／開鎖（Attack 6／5，參數 ID 103／101 是由函式表位置推的）、SetSkill、--weather／--hour／--clock／--cmd（SetFog／GetSpeed）、章節配音 c%03d.wav＋行間隔、地上道具起飛／落地音（PropMdl）、陷阱觸發音 0x3c、Rec #1 子類 1 的 b12＋u16(b13) = 0x4697b0 換天氣（不是時鐘）、#6（15 byte 版）、#8、#9（rec_cast）。
- 證據、沒做：CZhenMng（0x446390～0x447d90）與 CGroup／CUserGroup（0x440e70～0x441e00）：decomp 與 tools/callers.py 都找不到任何建構呼叫，原版沒有觸發條件，沒接。#6 的 12 byte 舊格式（20 筆）：原版照新格式讀 x、y 會讀到 0xcdcd（地圖外），AddObj 失敗不放，所以引擎也不放；PicID < 2000（1503／1512／1514）沒有 15 byte 資料。0x1003de10／0x10035540／0x10038240 原本就在 funcs_skip（story_rt 隊伍與 hero_look 取代）。
- 原版規則沒找到：銀行沒有原版視窗（只有 GM 文字指令 bank 與訊息 0xbf），B 鍵視窗是自訂；偷竊／開鎖沒有資料用到、開鎖目標選法（原版點選物件）是自訂的最近箱子；偷竊的物品 Type 0x31／0x32 意義沒查。
- 沒做：手機沒有銀行入口；網頁 URL 參數 weather／hour／clock／cmd 只加在 /tmp/qb13 的 shell.html，主樹 web/shell.html 沒動。

## 遊玩驗證待修（2026-11-20 第 2 輪後，HANDOFF「實際遊玩驗證 第 2 輪」）
- [x] 背包穿脫／娃娃面板（doll_draw）／拖放／新遊戲道具重複、創角職業名稱按鈕、過場 ptdhcj 與 yzh_yafu 載入、狼 istrap 隱形：已修並用瀏覽器證明
- [x] 第 3 輪已走過並正常：打狼／射箭／技能 1／掉落撿取／經驗／死亡與讀檔／砍樹／技能視窗與升級／商店買賣／換場景來回／存讀檔／孔刚跟隨／手機兩種方向（HANDOFF「第 3 輪」；修了怪物單拍打中數百次、刺客 Hand、箭消耗、存檔名亂碼、地上道具跟場景）
- [ ] 第 3 輪還沒走：水缸點擊、打造真的做出道具、鑑定／修理按鈕、背包滿、技能鍵 2～4 施放、手機放大鏡預覽逐項、宝箱（kuangshi／箱子）實點
- [ ] 原版規則沒找到：JM54 黑框（娃娃）的原版畫法與姿勢；拖放放到地上／互換細節
- [ ] 同格出生的怪物重疊（原版是否錯開沒查）

## 遊玩驗證待修（2026-11-20 第 4 輪後，HANDOFF「實際遊玩驗證 第 4 輪」）
- [x] 存檔含地上道具（GRND 尾巴）、水缸／箱子／撿取點擊、技能鍵 1～3 與瞄準治療、141 場景載入、聲音（AudioContext、sfx）、怪物同格（不是 bug）
- [x] 第 5 輪已做（原版無戰鬥曲，見 HANDOFF 第 5 輪）音樂：music.txt 第 3、4 欄（战斗音乐名）、dead.wav、start.wav 的切換時機（CMusic 0x579df0／0x57a910 狀態機）原版規則沒查清
- [ ] 打造真的做出道具（瀏覽器選配方放材料）、修理／鑑定按鈕、背包滿、受傷狀態動畫（中毒、冰凍）、法術瞄準（除治療外）、手機放大鏡預覽與直式視窗逐項
- [x] 第 5 輪已做 怪物位置／血量存檔（原版 CERWorld::Save 存全部物件）
- [ ] 劇情主線長途走查：逐章 NPC 對話、任務日誌、過場（第 4 輪只做載入、出口、第一屏）
- [ ] 日夜／天氣循環逐時段截圖比對

## 遊玩驗證待修（2026-11-20 第 5 輪後）
- [x] 主線逐章走查：第 6 輪走完第一章整條、第二章開頭（HANDOFF「主線走查」表；`make walktest`）；沒走完的見下「主線走查待續」
- [ ] 日夜／天氣逐時段與原版畫面比對（只有程式依據，沒有原版截圖）

## 主線走查待續（第 7 輪停在這裡；2026-11-20，HANDOFF「主線走查」表）
**走到哪**：第二章整條（寻访异人）、第三章整條（衡山城乌秸買玉佩→衡山村田纲）、第四章字幕、進得了 骊山／合江村／云雾谷，headless 走查通過（`engine/test/walk/ch2full.txt`，`make walktest` 內）。第 7 輪修了：虛擬格 2（狗熊A 死了放萧起）、虛擬格 11（使用道具）、第 10 格被問兩次。
**下一步**（第四、五章，骊山開館驗屍 kaiguanyanshi，咸阳 s16／骊山 s16 見 HANDOFF 表）：
1. 云雾谷 金蟾珠（godtoad_ball）：茵珠（alias yinzhu）ask1→ask2→ask5/ask6…，要 宋季阴谋、茵珠加入、金蟾珠給（第 15 格 journal）；talk 的 `auto` 選最後一個選項，這段要手選（`talk yinzhu ask1 ask2`）。
2. 七把神剑任選一把（ganjiangjian 虎丘、juquejian 匈奴营地、yuchangjian 雍县、zhanlujian 泰山（走查已拿到）、gongbujian 九江、moyejian1／taiebaojian）：骊山第 16 格要「神剑之一 + 金蟾珠」才開 disizhangjieshu；之後 kaiguanyanshi → 开馆验尸 plot → 方仲加入 → 進入皇陵 → 機關之獸 A／B → 始皇墓室。
3. 第六～十章：陈城残兵A s25（diliuzhangjieshu）、丰县 pct_斩将夺城（diqizhangjieshu）、巨鹿 pct_项羽坑卒（dijiuzhangjieshu）、咸阳监狱地下二层（dibazhangjieshu）、阿房宫 pct_赵高造反／胡亥之死（dishizhangjieshu）。誰設誰讀：`python3 -I tools/flagwho.py 旗標…`。
4. explore 的限制：對話樹用「重放」走，改變狀態的選項（例：萧起 ask31→ask32→ask24 之後選項變了）重放不到更深的節點，要用手寫的 talk；explore 第二階段會把人物打倒到不動點，走到仙境山洞（zhc_dongxue3）會被困住，之後的 BFS 找不到路，所以第二、三章用 `enter 房間別名` 直接進。
5. 起點：HANDOFF 第 7 輪的「怎麼重跑」；ch2full 的 explore 後面接手寫步驟，要往下就接在 `enter lishan` 後面。
**瀏覽器還沒驗的**：彭城狗熊已驗（HANDOFF 第 7 輪 8）；其餘是無畫面走查；萧起對話、乌秸買玉佩、田纲的道具進背包與日誌、第 11 格使用《孙膑兵法》要在真引擎走一遍（`?cmd=SetFlag …` 跳過前面）。
**原版規則沒找到**：孔刚 s19／s22 等 110 個沒有呼叫點的格子（`python3 -I tools/story_orphans.py`，只有虛擬表引用）；薛城暗門第 17 格「place_npc 密道口 B 之後又 DelObjectFromRoom 同別名」——民居裡沒有先放過的同別名物件，結果看起來是把剛放的 B 刪掉，原版意圖沒查清（可能是 0x1003e5d0 對房間內建築有別的規則）。
