# 伺服器→客戶端視窗資料訊息（Poq 處理 0x5dd~0x60e，共 41 種）對照引擎

2026-11-21 掃描：DLL 以 `FUN_10060cd0(玩家, 訊息編號, 參數, 個數, 字串)` 送 66 種（0x5dd~0x63f）；Poq 的分派（0x4xxxxx 的 switch）處理其中 41 種。
這些訊息只是把規則結果搬到視窗，引擎的視窗直接讀 Actor／inv／story 物件，所以沒有「接收」程式。下表記每種的內容與引擎對應；
**狀態**：✓ = 引擎有對應畫面／行為（有看過或有測試）；△ = 有對應但沒逐欄核對；— = 單機沒有產生者（只有連線或文字指令會送）。

| 訊息 | Poq 處理函式 | 內容 | 引擎對應 | 狀態 |
|---|---|---|---|---|
| 0x5dd | 0x517130 | 對話（說話者、台詞、選項） | story_dialog／dialog_draw | ✓ 網頁看過 |
| 0x5de | 0x51e160 | 角色資料（名、職業、等級、經驗、名聲、錢、HP／MP…） | hero_info（JM52） | △ 欄位含名聲、屬性；OldLevel、NextExp 顯示未逐欄核對 |
| 0x5e2 | 0x512220 | 交易道具表 | shop_list（JM63） | ✓ |
| 0x5e3 | 0x516620 | 背包／交易道具更新 | bag_draw | ✓ |
| 0x5e4 | 0x51fdd0 | 道具（Enable、Identified、HangData） | bag／item_icon | △ HangData（掛機）沒有對應 |
| 0x5e5、0x5e6 | 0x513a30、0x516860 | 隊伍成員 | party.c | ✓ |
| 0x5e7、0x5e8 | 0x514270、0x516ea0 | 技能表、快捷技能 | 技能視窗／pick | ✓ |
| 0x5e9、0x60c | 0x520350、0x5201b0 | 技能可升級（CanUpgrade） | 技能升級 | △ |
| 0x5ea | 0x51a8e0 | 章節、日誌、任務（ChapterData、LogData、MissionData） | chapter_message、journal（journal.c／logwin.c JM61） | ✓ 日誌＋任務清單（2026-11-23 照 0x51a8e0）；章節視窗 JM69 △ |
| 0x5eb | 0x514ed0 | 裝備 | equip（JM55） | ✓ 網頁看過 |
| 0x5ed | 0x5188b0 | 存檔列表 | sav_d | ✓ |
| 0x5ee、0x5f0、0x5f1、0x5f2 | 0x51d310、0x520910、0x51da20、0x5111a0 | 組隊邀請／接受（TeamAsk、TeamAccept） | — | — 產生者 0x10047270／47440／47520／474b0 只被文字指令與同步轉接呼叫，單機劇情用 add_member 直接入隊 |
| 0x5f7、0x5f8、0x5f9、0x5ff、0x600 | 0x518d60、0x511bf0、0x5163f0、0x5191e0、0x518f00 | 道具資料、金錢（Money）、位置 | bag／hero_money | ✓ |
| 0x5fa、0x5fb、0x5fc、0x606 | 0x514bd0、0x5205a0、0x515a70、0x51df30 | 打造資料（MakeData、SpeType、Level）、UIS_P | make_open／ui_skill_actor | △ 施展者挑選順序沒完全對照 |
| 0x5fd | 0x519e50 | 套裝（SuperEquip） | set_mask（inv[40]） | △ 沒搬進容器 |
| 0x5fe | 0x519870 | 道具需求（LvlReq、StrReq、ConReq…） | item_describe | ✓ |
| 0x601 | 0x51cc50 | 技能說明（Info） | 技能說明 | △ |
| 0x603、0x604 | 0x51bb60、0x51b9a0 | 地圖已去過的地點（MapValid） | mapwin.c map_site_valid | ✓ 網頁看過地圖；地點圖示對應是推的 |
| 0x60d | 0x51c6b0 | 五行（FiveElement） | fe_conv／item_describe | △ |
| 0x605、0x607、0x608、0x609、0x60a、0x60b、0x60e | 多個 | 地圖資料同步（MapData）、視窗開關、確認框 | 地圖／系統選單 | △ 未逐項 |
| 0x5e1 | 0x458010 | 資料表設值 | — | △ 內部資料登錄 |
