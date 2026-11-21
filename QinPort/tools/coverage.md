# 覆蓋矩陣（tools/coverage.py 產生，勿手改）

狀態：完整＝引擎照原版處理；部分＝有但有缺；無＝沒做；?＝無法判斷。

## 總表

| 類別 | 項數 | 完整 | 部分 | 無 | ? |
|---|---|---|---|---|---|
| (a1) 技能 Attack方式（669 列技能） | 13 | 10 | 1 | 2 | 0 |
| (a2) 技能參數 ID（skill_field／0x1039e628 處理表） | 79 | 78 | 0 | 0 | 1 |
| (a3) 技能用到的狀態 ID（RoleEffectMdl 91 個） | 52 | 52 | 0 | 0 | 0 |
| (a4) 狀態表欄位（RoleEffectMdl.txt） | 18 | 16 | 0 | 0 | 2 |
| (a5) 技能死亡方式（欄 12） | 8 | 8 | 0 | 0 | 0 |
| (b1) 道具類型（道具表第 3 欄） | 37 | 37 | 0 | 0 | 0 |
| (b2) 道具特性屬性 ID（attribT 欄 31，604 列特性） | 67 | 66 | 0 | 0 | 1 |
| (c1) 怪物表欄位（499 個怪物列） | 73 | 73 | 0 | 0 | 0 |
| (c2) 劇情物件屬性鍵（set_int／new_int，不含 NET） | 55 | 46 | 0 | 0 | 9 |
| (d1) 劇情用到的 EasyRPG 命令（ENGINE／共用函式） | 93 | 93 | 0 | 0 | 0 |
| (d2) 劇情錄影（Rec/User/*.rec）封包子類（`#` 0x470300、`@` 0x4725c0） | 22 | 19 | 3 | 0 | 0 |
| (e1) 場景 BNT 建築模型種類 | 4 | 4 | 0 | 0 | 0 |
| (e2) BNT 事件文字／物件名稱第一詞 | 242 | 242 | 0 | 0 | 0 |
| (f1) 場景檔 .Scn 欄位 | 6 | 6 | 0 | 0 | 0 |
| (f2) 其他場景資料 | 3 | 3 | 0 | 0 | 0 |

## (a1) 技能 Attack方式（669 列技能）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| Attack方式 空 | 467 | 完整 | 不造成傷害，skill_hit 只加狀態與參數 |
| Attack方式 0 | 166 | 完整 | 不造成傷害，skill_hit 只加狀態與參數 |
| Attack方式 3 | 15 | 完整 | 召喚 summon 0x100355d0 |
| Attack方式 8 | 10 | 完整 | 陷阱 trap 0x10034cb0 |
| Attack方式 10 | 5 | 完整 | 自損物理 strike（287） |
| Attack方式 2 | 2 | 完整 | 馴服 tame 0x10035370 |
| Attack方式 9 | 2 | 完整 | 除障 disarm |
| Attack方式 11 | 2 | 部分 | clone() 放克隆刺客刀／弓；缺 0x1003df00（複製施放者的裝備子物件與技能，技能等級上限 9，經 0x10037d70；原版規則已讀） |
| Attack方式 5 | 0 | 完整 | 開鎖 0x10034900（lockpick_open）；資料 0 次 |
| Attack方式 6 | 0 | 完整 | 偷竊 0x10034010（skill_steal）；資料 0 次 |
| Attack方式 7 | 0 | 完整 | (原版 strike 的第 7 類，strike 同 4 先留血)；資料 0 次 |
| Attack方式 12 | 0 | 無 | 669 列技能全掃資料 0 次，遊戲不會出現；清狀態，見 PORT_QUEUE |
| Attack方式 13 | 0 | 無 | 669 列技能全掃資料 0 次，遊戲不會出現；界面技能，見 PORT_QUEUE |

## (a2) 技能參數 ID（skill_field／0x1039e628 處理表）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 參數 269 | 338 | 完整 | 原版整個 dll 沒有讀這個變數，沒作用（等價） |
| 參數 550 | 221 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 560 | 115 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 558 | 110 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 554 | 106 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 556 | 104 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 552 | 103 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 18 | 91 | 完整 | skill_field／skill_now／stat_ptr（0x10026270 表） |
| 參數 218 | 56 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 210 | 45 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 214 | 44 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 212 | 42 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 216 | 39 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 220 | 33 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 41 | 27 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 42 | 27 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 19 | 23 | 完整 | skill_field／skill_now／stat_ptr（0x10026270 表） |
| 參數 64 | 19 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 10 | 19 | 完整 | skill_field／skill_now／stat_ptr（0x10026270 表） |
| 參數 45 | 17 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 46 | 17 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 53 | 15 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 273 | 15 | 完整 | 馴服／召喚／陷阱參數 |
| 參數 278 | 15 | 完整 | 馴服／召喚／陷阱參數 |
| 參數 274 | 15 | 完整 | 馴服／召喚／陷阱參數 |
| 參數 49 | 14 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 50 | 14 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 65 | 14 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 51 | 13 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 52 | 13 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 43 | 12 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 44 | 12 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 47 | 12 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 48 | 12 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 60 | 12 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 61 | 11 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 76 | 11 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 260 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 261 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 252 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 253 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 286 | 10 | 完整 | 馴服／召喚／陷阱參數 |
| 參數 67 | 10 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 258 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 259 | 10 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 62 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 54 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 55 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 56 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 57 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 58 | 9 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 66 | 8 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 254 | 8 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 255 | 8 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 77 | 7 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 250 | 7 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 251 | 7 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 63 | 6 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 256 | 6 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 257 | 6 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 88 | 5 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 287 | 5 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 75 | 4 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 86 | 4 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 80 | 4 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 82 | 4 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 84 | 4 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 292 | 3 | 完整 | skill_now／status_clear |
| 參數 271 | 2 | 完整 | 馴服／召喚／陷阱參數 |
| 參數 68 | 2 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 223 | 2 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 21 | 2 | 完整 | skill_field／skill_now／stat_ptr（0x10026270 表） |
| 參數 295 | 2 | 完整 | skill_now／status_clear |
| 參數 222 | 2 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 23 | 2 | 完整 | skill_field／skill_now／stat_ptr（0x10026270 表） |
| 參數 512 | 1 | 完整 | strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個 |
| 參數 69 | 1 | ? | ex[69] 沒有讀它的地方，原版是否使用未查 |
| 參數 74 | 1 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |
| 參數 70 | 1 | 完整 | stat_ptr → Actor.ex[id]，strike／derive 讀 |

## (a3) 技能用到的狀態 ID（RoleEffectMdl 91 個）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 狀態 19 破除隐身 | 24 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 97  | 11 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 5 移行换影 | 10 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 18 怒 | 9 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 40 百魔不侵 | 7 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 47 荆棘密布 | 6 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 11 紧身咒 | 6 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 15 诱敌深入(自己） | 6 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 20 御气之术 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 35 噬骨之毒 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 32 点石成俑 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 14 盅惑人心 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 31 铜皮铁骨 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 48 加速 | 5 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 25 雷霆万钧 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 6 燃烧血刃 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 38 凝神养气 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 37 金钟护体 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 46 兴奋剂 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 23 天地混沌 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 98  | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 63 火 | 4 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 7 虎啸龙吟 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 36 乾坤挪移 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 53 变草人 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 13 反戈一击 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 43 降敌速 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 49 加射程 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 50 春满人间 | 3 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 44 降敌防 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 30 失明 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 41 止血回春 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 16 伏虎降龙 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 29 群体防守 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 42 剑阵 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 83 受伤火雾 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 12 金睛火眼 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 8 遁地无形 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 39 反刺猬甲 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 51 (三倍攻击) | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 26 AI变红 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 34 无情冰刃 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 61 木 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 3 捆仙之索 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 70 伙伴春满人间 | 2 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 28 群体攻击 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 52 地牢 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 9 AI放大 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 27 AI噬骨毒 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 62 水 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 64 土 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |
| 狀態 65 攻击 | 1 | 完整 | status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色 |

## (a4) 狀態表欄位（RoleEffectMdl.txt）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| temp | 78 | ? | 欄位引擎有 raw[20] 保存，是否逐欄消費未逐項查 |
| 头上效果 | 43 | 完整 | status_def raw[]／trail_* |
| 声音 | 42 | 完整 | status_def raw[]／trail_* |
| 身上效果 | 25 | 完整 | status_def raw[]／trail_* |
| 速度变化 | 14 | 完整 | status_def raw[]／trail_* |
| 附加状态 | 13 | 完整 | status_def raw[]／trail_* |
| 攻击不能 | 10 | 完整 | status_def raw[]／trail_* |
| 全身变色 | 9 | 完整 | status_def raw[]／trail_* |
| 不发送到Client | 7 | ? | 欄位引擎有 raw[20] 保存，是否逐欄消費未逐項查 |
| 缩放 | 4 | 完整 | status_def raw[]／trail_* |
| 无敌 | 3 | 完整 | status_def raw[]／trail_* |
| 旋转 | 2 | 完整 | status_def raw[]／trail_* |
| 停止动画循环 | 1 | 完整 | status_def raw[]／trail_* |
| 幻影移动 | 1 | 完整 | status_def raw[]／trail_* |
| 武器变色 | 1 | 完整 | status_def raw[]／trail_* |
| 幻影攻击 | 1 | 完整 | status_def raw[]／trail_* |
| 头变色 | 1 | 完整 | status_def raw[]／trail_* |
| 脚下效果 | 1 | 完整 | status_def raw[]／trail_* |

## (a5) 技能死亡方式（欄 12）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 死亡方式 8 | 10 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 7 | 8 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 6 | 6 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 3 | 6 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 4 | 5 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 9 | 2 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 10 | 2 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |
| 死亡方式 5 | 2 | 完整 | 欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證 |

## (b1) 道具類型（道具表第 3 欄）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 類型 50 | 49 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 1001~1119（掉寶類型表／打造配方） | 45 | 完整 | item_type_pick 0x1007a2f0；MakeMdl 配方 |
| 類型 0 | 44 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 51 | 43 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 17 | 36 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 10 | 36 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 26 | 34 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 8 | 30 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 9 | 27 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 34 | 27 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 20 | 26 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 23 | 26 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 29 | 26 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 7 | 26 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 25 | 25 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 16 | 25 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 27 | 24 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 11 | 24 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 31 | 23 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 30 | 22 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 28 | 22 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 15 | 21 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 13 | 21 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 49 | 21 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 32 | 20 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 14 | 20 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 12 | 20 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 33 | 19 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 21 | 18 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 22 | 18 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 19 | 18 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 18 | 18 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 24 | 18 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 2 | 9 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 1 | 9 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 61 | 1 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |
| 類型 60 | 1 | 完整 | item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類 |

## (b2) 道具特性屬性 ID（attribT 欄 31，604 列特性）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 屬性 41 | 23 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 42 | 23 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 24 | 21 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 23 | 17 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 61 | 17 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 53 | 17 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 27 | 16 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 29 | 16 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 28 | 15 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 32 | 15 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 49 | 15 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 50 | 15 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 60 | 14 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 47 | 13 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 48 | 13 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 43 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 44 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 45 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 46 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 51 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 52 | 12 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 86 | 11 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 65 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 80 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 84 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 92 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 95 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 96 | 10 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 57 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 73 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 82 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 88 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 93 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 94 | 9 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 31 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 33 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 54 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 55 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 56 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 58 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 71 | 8 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 64 | 7 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 70 | 7 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 72 | 7 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 74 | 7 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 75 | 7 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 62 | 6 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 66 | 6 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 67 | 6 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 63 | 5 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 76 | 5 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 77 | 4 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 21 | 4 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 292 | 4 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 22 | 4 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 293 | 4 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 97 | 3 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 90 | 3 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 295 | 3 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 294 | 2 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 98 | 1 | 完整 | item_mods → stat_ptr → Ex[]，derive／strike 讀 |
| 屬性 300 | 1 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 302 | 1 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 303 | 1 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 304 | 1 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 301 | 1 | 完整 | skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表） |
| 屬性 40 | 1 | ? | ex[40] 沒有讀它的地方 |

## (c1) 怪物表欄位（499 個怪物列）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 人物类型 | 499 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 人物别名 | 499 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 名称ID | 499 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 人物名称 | 494 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 图形1 | 493 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击力基准值 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击力参数 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击力大基准值 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击力大参数 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 命中率基准值 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 命中率参数 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 魔法攻击基准值 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 魔法攻击参数 | 474 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能1 | 473 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 经验值基准值 | 469 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 经验值参数 | 469 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 视野 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 打击恢复率 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 减少物理伤害 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 眩晕免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 中毒免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 冰冻免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 失明免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 石化免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 溃散免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 其他免疫 | 459 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 减少魔法伤害 | 449 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 生命恢复率 | 448 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 生命值基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 生命值参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 防御力基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 防御力参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 金魔法防御基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 金魔法防御参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 木魔法防御基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 木魔法防御参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 水魔法防御基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 水魔法防御参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 火魔法防御基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 火魔法防御参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 土魔法防御基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 土魔法防御参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 闪避率基准值 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 闪避率参数 | 447 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 最小道具随机数 | 441 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 最大道具随机数 | 441 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 随机道具类型1 | 391 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 类型1概率 | 391 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能2 | 335 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 移动速度 | 334 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 随机道具类型2 | 325 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 类型2概率 | 325 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击频率 | 323 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 随机道具类型3 | 298 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 类型3概率 | 298 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 随机道具类型4 | 279 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 类型4概率 | 279 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 魔法攻击属性 | 237 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能3 | 205 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 随机道具类型5 | 133 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 类型5概率 | 133 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 图形2 | 125 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 移动速度（2－10） | 104 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 攻击频率（1－5） | 104 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 是否陷阱 | 94 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能4 | 94 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 图形3 | 39 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能5 | 27 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 图形4 | 11 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 图形5 | 9 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能6 | 7 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能7 | 6 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |
| 技能8 | 4 | 完整 | res.c mondefs_load 讀入並由 story_on_person 套用 |

## (c2) 劇情物件屬性鍵（set_int／new_int，不含 NET）
含 G_AIGroup、NpcBehavior 等 AI 旗標。

| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| G_NPCActive | 1077 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_AIGroup | 1072 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Invincible | 1051 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_Type | 120 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_Day | 66 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_Night | 64 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Level | 47 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Map | 43 | 完整 | 0x100484c0 記 CurMap／"Map" 位元遮罩（story_rt room_map_note）；地圖視窗 JM60 地點按鈕依 0x51bb60／0x50b1b0 的位元公式只顯示去過的（mapwin.c map_site_valid，2026-11-21）。注意：地點序號 = Map 號是照位元公式推的，沒對照地圖資料；只在網頁版（有劇情）生效，沒在瀏覽器看過 |
| SortInquiry | 41 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| G_InfoID | 39 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| UI_Pos | 37 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Con | 34 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| IsActorNPC | 33 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_PicID | 25 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_Ufo | 24 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_PicID2 | 18 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Friend | 18 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| isbuilding | 17 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| UIS_Produce | 17 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| UIS_Addgem | 17 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| Money | 17 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Gender | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Age | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Str | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Dex | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Savvy | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Wisdom | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Charm | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| B_C_DJHF | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| B_QTIMMU | 16 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| UIS_Identify | 15 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| deletekey | 13 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_WEA_BigRain | 11 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_WEA_SmallRain | 11 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_IsUISkill | 6 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| SR | 6 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| HR | 6 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| istrap | 6 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| HP | 6 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| HP_MAX | 6 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| UI_Cur | 5 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| Amount | 4 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| BD_MAX | 4 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| BD_MAX0 | 4 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| G_WEA_BigSnow | 3 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| G_WEA_SmallSnow | 3 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Value | 3 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| Exp | 3 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| UIS_Repair | 2 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| maketype | 1 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| contain | 1 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| Type | 1 | ? | 2026-11-21 更正：先前寫「只出現 1 次」是錯的，反編譯碼裡有多處以名稱讀寫（SortInquiry 62 個函式、G_InfoID 73、UIS_Addgem 26、G_IsUISkill 18、UI_Cur 16、BD_MAX0 7、maketype 1、Value 17、Type 38，其中多數是房間／NPC 建構式設初值）；引擎有沒有對應規則沒逐一核對 |
| AP | 1 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| AP_MAX | 1 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |
| DP | 1 | 完整 | 引擎 literal 有讀／寫（story_rt／main） |

## (d1) 劇情用到的 EasyRPG 命令（ENGINE／共用函式）


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
|  0x1003c550 | 476 | 完整 | story_rt.c case |
|  0x100730c0 | 331 | 完整 | story_rt.c case |
|  0x10070360 | 175 | 完整 | story_rt.c case |
|  0x1003d270 | 149 | 完整 | story_rt.c case |
|  0x1003e5d0 | 140 | 完整 | story_rt.c case |
|  0x1004bcf0 | 112 | 完整 | story_rt.c case |
|  0x100757a0 | 103 | 完整 | story_rt.c case |
|  0x100226d0 | 101 | 完整 | story_rt.c case |
|  0x1003f300 | 101 | 完整 | story_rt.c case |
|  0x100f9670 | 94 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x100528d0 | 82 | 完整 | story_rt.c case |
|  0x1005f3c0 | 76 | 完整 | story_rt.c case |
|  0x10023410 | 60 | 完整 | story_rt.c case |
|  0x1004c4f0 | 57 | 完整 | story_rt.c case |
|  0x1004b020 | 51 | 完整 | story_rt.c case |
|  0x1004c6d0 | 48 | 完整 | story_rt.c case |
|  0x1003fc80 | 47 | 完整 | story_rt.c case |
|  0x1005f2e0 | 47 | 完整 | story_rt.c case |
|  0x10054630 | 46 | 完整 | story_rt.c case |
|  0x10037850 | 46 | 完整 | story_rt.c case |
|  0x1004e0d0 | 39 | 完整 | story_rt.c case |
|  0x1004e1b0 | 38 | 完整 | story_rt.c case |
|  0x10055990 | 37 | 完整 | story_rt.c case |
|  0x1003ca70 | 34 | 完整 | story_rt.c case |
|  0x1004aac0 | 28 | 完整 | story_rt.c case |
|  0x1003d200 | 27 | 完整 | story_rt.c case |
|  0x1003e920 | 23 | 完整 | story_rt.c case |
|  0x1003d0b0 | 22 | 完整 | story_rt.c case |
|  0x1004e200 | 17 | 完整 | story_rt.c case |
|  0x1003cd10 | 16 | 完整 | story_rt.c case |
|  0x101db580 | 15 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x101daa20 | 15 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1004ec80 | 14 | 完整 | story_rt.c case |
|  0x1003ee60 | 14 | 完整 | story_rt.c case |
|  0x1003c9f0 | 14 | 完整 | story_rt.c case |
|  0x101da190 | 14 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x101da100 | 14 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1004e640 | 13 | 完整 | story_rt.c case |
|  0x100406a0 | 12 | 完整 | story_rt.c case |
|  0x1004e4c0 | 12 | 完整 | story_rt.c case |
|  0x1004db90 | 10 | 完整 | story_rt.c case |
|  0x10055ae0 | 10 | 完整 | story_rt.c case |
|  0x1003da30 | 9 | 完整 | story_rt.c case |
|  0x1004d180 | 9 | 完整 | story_rt.c case |
|  0x1004abd0 | 8 | 完整 | story_rt.c case |
|  0x10056640 | 8 | 完整 | story_rt.c case |
|  0x10021c60 | 7 | 完整 | story_rt.c case |
|  0x1003daf0 | 7 | 完整 | story_rt.c case |
|  0x10053b40 | 6 | 完整 | story_rt.c case |
|  0x1004a940 | 6 | 完整 | story_rt.c case |
|  0x100fa0b0 | 6 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1005f390 | 6 | 完整 | story_rt.c case |
|  0x1004b6c0 | 5 | 完整 | story_rt.c case |
|  0x100f9f60 | 5 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x100f9630 | 5 | 完整 | story_rt.c case |
|  0x1003aea0 | 5 | 完整 | story_rt.c case |
|  0x1003e160 | 5 | 完整 | story_rt.c case |
|  0x10048790 | 4 | 完整 | story_rt.c case |
|  0x1003ce20 | 4 | 完整 | story_rt.c case |
|  0x100f95f0 | 4 | 完整 | story_rt.c case |
|  0x1004ad90 | 4 | 完整 | story_rt.c case |
|  0x10022490 | 3 | 完整 | story_rt.c case |
|  0x10054810 | 3 | 完整 | story_rt.c case |
|  0x101dde30 | 3 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1003ed10 | 2 | 完整 | story_rt.c case |
|  0x1020f731 | 2 | 完整 | story_rt.c case |
|  0x1020f761 | 2 | 完整 | story_rt.c case |
|  0x10053e10 | 2 | 完整 | story_rt.c case |
|  0x10058d30 | 2 | 完整 | story_rt.c case |
|  0x1003ff70 | 2 | 完整 | story_rt.c case |
|  0x100f9570 | 2 | 完整 | story_rt.c case |
|  0x1013c690 | 2 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013da60 | 2 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1003c510 | 2 | 完整 | story_rt.c case |
|  0x1003f2c0 | 1 | 完整 | story_rt.c case |
|  0x1005e490 | 1 | 完整 | story_rt.c case |
|  0x1003d380 | 1 | 完整 | story_rt.c case |
|  0x10022ac0 | 1 | 完整 | story_rt.c case |
|  0x1013dc00 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013cee0 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1005d790 | 1 | 完整 | story_rt.c case |
|  0x1013d8b0 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013d090 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013d570 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013c4d0 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x100549d0 | 1 | 完整 | story_rt.c case |
|  0x1013c850 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013cb90 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013d230 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013cd30 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013c9f0 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013d3d0 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |
|  0x1013d710 | 1 | 完整 | 共用劇情函式，已翻成 C（story/_shared.c） |

## (d2) 劇情錄影（Rec/User/*.rec）封包子類（`#` 0x470300、`@` 0x4725c0）
`$` 在資料中 0 次。

| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| @ 0 | 4051 | 完整 | 時間戳 |
| # 3 | 3666 | 完整 | 0x470300 case 3：子碼 0x01 換外觀 0x48d810、0x02 名字、0x33、0x34、0x35 層號 +0x66、0x36 名字編號 0x48e820、0x65、0x66、0x67 生命百分比 0x48e8f0；0x79（資料 1 次）原版不處理 |
| # 4 | 1561 | 完整 | 位置修正 0x496260／插值 0x4967d0 |
| @ 4 | 1000 | 部分 | 對話框 = 說話者頭頂的文字框（0x4722f0 定位 x = 角色 x、下緣 = y − 頭高 − 10；0x5963e0 建框；超出畫面推回）；\r\n → \n；語音 sound/NNN.wav 播完才收。原版 Rec 對話沒有說話者名／頭像（case 4 只存文字，名字在 PTalk_C 文字裡）。框色與字色（DAT_0064b3b4 16 位元全域）沒查到 |
| @ 6 | 999 | 完整 | case 6：沒有語音才設拍數 n×3、至少 0x28；有語音靠語音播完（0x472e70） |
| # 10 | 932 | 完整 | 字串佇列 strq |
| # 1 | 837 | 部分 | 子類 0（角色）rec_add 完整；子類 1（進場景）：亮度參數（類型 u16 b7、夜 b9、日 b10 → light_table，rec_end 還原）做；b12＋u16(b13) 傳給 vfunc+8（疑似時鐘，資料 538/1370 等超過一個半小時，原版規則沒找到）和原版載另一份場景拷貝沒做 |
| # 5 | 702 | 完整 | actor_status_notify 0x48d2c0 |
| # 7 | 612 | 部分 | 建築狀態 0x484ee0：編號 = 低 14 位 − 2001 = BNT 順序（0x4a2600 配號 2000 起先加 1、0x44f4c0 逐筆建築；rec_net case 7，只動 CTriBld）。資料 612 筆全落在 BNT 範圍內；但同場景不同次進入偏移會差 1（hd_mishi），#6 地上道具也佔編號，原版規則沒找到（記錄檔封包比原版讀的欄位短，見 #6）。2026-11-20 新發現：0x4a2600 是環形配號器（範圍 2000～16381，計數器 +0x17ffa 先加 1、遇到使用中的槽就往後找、滿了回 2000，只在換場景時不重設），所以編號偏移取決於當時已配過的物件數，沒有固定偏移；要完全照原版得模擬播放端配號歷史，資料裡只有 #1 角色編號（3527 起）可旁證，沒查通。原生版沒有劇情層跑不了過場，網頁版逐片實測未做 |
| # 13 | 174 | 完整 | 場景名／命令字串；子類 2 換場景 → rec.foreign（本輪修：3 byte 封包以前被長度檢查擋掉） |
| @ 8 | 151 | 完整 | 0x44ca40(模式, 0)：1／2／3 淡入淡出 = screen_effect；+0x64 只被 #11／#12 讀（資料沒有） |
| @ 5 | 104 | 完整 | 原版單機不建 Rec 封包，只寫標記（等價） |
| @ 1 | 92 | 完整 | 鏡頭定位 |
| @ 9 | 74 | 完整 | 鏡頭平移 |
| # 9 | 47 | 完整 | Rec 封包 0x43acb0(角色, 招式, 點)：rec_net case 9 → rec_cast → actor_cast_try（MethodMain + 招式觸發，招式不在角色技能列就先補進去）；資料 47 次、招式編號 18～598。沒在畫面逐片驗證 |
| @ 7 | 42 | 完整 | 鏡頭跟隨 |
| @ 11 | 34 | 完整 | 播音效 0x4804a0（audio_sfx） |
| # 6 | 24 | 完整 | 地上道具 24 次：4 筆 15 byte 照原版放（AddProp 0x450cc0）；20 筆只有 12 byte，原版照新格式讀 x、y 會讀到封包外殘值 0xcdcd（格子在地圖外），AddObj 失敗什麼都不放，引擎同樣要 n >= 15 才放（等價）；沒在畫面驗證 |
| # 8 | 4 | 完整 | Rec 封包 0x450700(單位編號, x, y, 0, 0)：rec_net case 8 → unit_new(來源 0)；資料 4 次都是單位 23。沒在畫面逐片驗證 |
| # 2 | 4 | 完整 | rec_remove |
| @ 10 | 3 | 完整 | 0x4c8630：全場景探索值 0 → 0xc9、1 → 1、2 → 0 |
| @ 12 | 1 | 完整 | 0x469630＋0x4695f0：設小時並依亮度表重設亮度（clock_hour_set） |

## (e1) 場景 BNT 建築模型種類


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 模型類型 0x1 | 106820 | 完整 | 一般建築：占地格、遮擋 |
| 模型類型 0x81 | 32 | 完整 | 樹／礦：點擊命令 0x12、開箱規則 0x1004e680 |
| 模型類型 0x96 | 5 | 完整 | 門：go 門點擊命令 0x10、非 go 感應區 |
| 模型類型 無模型 | 4 | 完整 | 4 筆 BNT 建築模型編號在 .MDL 裡找不到：原版 0x44f4c0 對空模型指標讀 +0x22（資料錯誤、未定義行為），引擎 o->m = NULL 略過（等價於不存在） |

## (e2) BNT 事件文字／物件名稱第一詞


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| go | 2729 | 完整 | door_go_due／story_exit |
| (空) | 932 | 完整 | 空名稱：只是建築／觸發區，無事件文字（原版 0x482db0 空字串不動作） |
| netsuijiwupinbaoxiang | 330 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| suijikuangshi | 265 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| netsuijikuangshi | 257 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| random_tree | 224 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| netrandom_tree | 218 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| keysuijiwupinbaoxiang | 182 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| s_pctstart | 157 | 完整 | story_pct_start 0x1001c210 |
| suijiwupinbaoxiang | 109 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_xnyd | 98 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| openall | 90 | 完整 | 本輪補：trigger_sense_tick 執行 openall <別名>（0x10019d70 chest_open_all），之前只認 s_pctstart |
| box_a | 23 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| kuangshi_a | 23 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_b | 15 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_d | 13 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shumu_a | 12 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| jiujiwuqi | 11 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| iron_ore | 10 | 完整 | 字串在 Poq.exe／EasyRPG.dll 位元組搜尋 0 次：不是指令、沒有同名物件，原版沒有作用（等價） |
| yicengbaoxiang | 9 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| gaojishipinbaoxiang | 8 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| jiujishipin | 8 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| bingmayong1 | 8 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_c | 7 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu12 | 7 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu14 | 7 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| kuangshi_b | 7 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shumu_b | 7 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_hsh | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| ercengbaoxiang | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xcbox_b | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu24 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu3 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu19 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu4 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu16 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu15 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu13 | 6 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xy_box_a | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzib_xy | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzia_fx | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_hsh | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu1 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu2 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu25 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu40 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu26 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu17 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu5 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu6 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu7 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu11 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu10 | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| superbox | 5 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| c_aigroup | 5 | 完整 | 本輪補：trigger_sense_tick 執行 c_aigroup <原組> <新組>（0x10013270 → story_c_aigroup，storytest aigroup=）；之前誤判為「不是指令」 |
| shuigang_hsh | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuiguan_wch | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_shuaifu | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| waguan_hd | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tanzi_ych | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu36 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu35 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu32 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_julu | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| waguan_px | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu37 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu39 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu38 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu20 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu18 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu21 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu23 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu8 | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiaoguizi_px | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_gk | 4 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| snow1 | 4 | 完整 | 連線版房間入口（EasyRPG.dll 0x1005d8b0 "snow1" → 秦殇/NET/雁门关/ROOM/雪地战场01），所在場景 net_xiongnuyingdi 是連線版場景 |
| table_pch | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_pch | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_bingying | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_chch | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zawugui_xy | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingqijia | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_hjc | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tanzi_yhl | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_prison | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu33 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu34 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu27 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu31 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu30 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu29 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_hz | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_a | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzib_dsh | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_d | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu22 | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| yicengbox_a | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| gaojiwuqibaoxiang | 3 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| shuigang_a | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_xsh | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuijing_hq | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_hz | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_px | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| needkey | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_ych | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_xsh | 3 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizib_xy | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| yanshi_ds_sd | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_ts | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzia_ts | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_chch | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guanzi_a | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_hd | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_hd | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| jiguanshoushiti | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| goodbox_a | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_city | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_hjc | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_hd | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tanzi_pch | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xcbox_a | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| afang_backdoor | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_shanzhai | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_jl | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xy_box_c | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_wch | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu28 | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingqijia_dsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingqijiaa_dsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zawuguia_jj | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shigang_jj | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzia_dsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzia_jj | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_hz | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_f | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingyongzu9 | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuijing_hd | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_e | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_yx | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tanzi_fx | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_a | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuijing_hsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_c | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_ych | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shujia | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| well | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzid_yhl | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_yhl | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| trueboxa | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_yhl | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shoutuiche_dsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| goodbox_b | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_hsh | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| qiangdaobaoxiang | 2 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_hd | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzib_hd | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| well_hjc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| waguan_hjc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuijing_xch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| fangche | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowugui | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| damupen_xy | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| net_xch_junyingdoor | 1 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| xiaozhuozi_xy | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_zhoufu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxes_a | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxes_b | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tree_yc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| netdsjunyingdamen | 1 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| shuijing_pch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| mucai | 1 | 完整 | 字串在 Poq.exe／EasyRPG.dll 位元組搜尋 0 次：不是指令、沒有同名物件，原版沒有作用（等價） |
| shoutuiche_pch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| bingqijiazi_yc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_pch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzia_hz | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| jiugang_jl | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_whc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zhuozi_wch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| net_danghubox_hsj | 1 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| aerolite | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shoutuiche_jj | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_yao | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zhaogaofumen | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| backdoor_of_taizi | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| neveropen | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| hffdoor | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_tianlao | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_d | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| anmen_xc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxb_pch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_pch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| dihuoshiyanshi | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shikeyanbi | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shikeyanbia | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_chengfu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baowuxiangzi_b | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_xnyd | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiaotuiche_hq | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangluyishu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangzi_ts | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_hq | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| yingzhangdamen | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| fengxiangate | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| backdoor_of_shuaifu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_fx | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuijing_chch | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_hq | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zhuozi_a | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| xiangziymgb | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxb_xnyd | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| baoxiang | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| canopen | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_xsh | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door_of_lao | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| jushu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_ych | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| yijia | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| locked | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| jianyubaoxiang | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_lingzhi | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizia_xy | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| wenlinyajingbaoxiang | 1 | 完整 | 只有 NET（連線版）劇情物件用，單機不產生 |
| shuigang1 | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang2 | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_c | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zawugui_jj | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| poche_xsh | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| boxa_whc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| weiliaobaoxiang | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| zawugui_px | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| taishantiesong | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door1_of_xiongnu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| door2_of_xiongnu | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_yc | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| cangshubaoxiang | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| qiangdaoqianxiang | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| tree_a | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| laomen_a | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| box_xy | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| shuigang_fx | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |
| guizi_px | 1 | 完整 | 同名單機劇情物件（object_story 依名稱配對，chest_state_sync） |

## (f1) 場景檔 .Scn 欄位
前場景／x／y／方向為入口點，已處理。

| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 前场景名称 | 6693 | 完整 | scn_value／入口點 |
| Map名称 | 512 | 完整 | scn_value／入口點 |
| IntBld名称 | 512 | 完整 | scn_value／入口點 |
| IntObj名称 | 512 | 完整 | scn_value／入口點 |
| 入口点数 | 512 | 完整 | scn_value／入口點 |
| 雷达地图ID | 512 | 完整 | scn_value／入口點 |

## (f2) 其他場景資料


| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |
|---|---|---|---|
| 天氣（weather.c） | 512 | 完整 | weather.c：雨／雪／疾風（CGale 0x43fda0）／色調 light_mode 已做；剩 shortcut：依場景種類 G_Type 的環境音（0x468c10 後半）、小雨亮度鎖只在開始下雨設 |
| music.txt 場景→音樂 | 118 | 完整 | main.c music |
| 日夜（light_hour） | 1 | 完整 | light.c |
