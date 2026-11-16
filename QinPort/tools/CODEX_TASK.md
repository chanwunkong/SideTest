# QinPort 移植任務（給 Codex，每輪一個系統）

你在 QinPort/ 裡工作：《秦殤》的忠實重製引擎（C + SDL2，主要目標是手機網頁版）。
先讀 HANDOFF.md 的「原版函數總表 FUNCS.tsv」一節最後的 **下一步** 和「移植步驟」，其他大檔不要整檔讀。

## 這一輪要做
1. `python3 tools/functool.py next 0x<下一步位址>` 看這批函數（同一個類別或連續位址）。
2. 平台層／容器／建構解構／std::string／CRT／連線版／沒有呼叫者的 → 寫進 tools/funcs_skip.txt 附原因；
   容器或工具類 → tools/base_map.tsv。
3. 其餘用 `python3 tools/fn.py --brief 位址…` 讀本體（只讀要翻的），照原版翻進 engine/src/。
   - 已有對應程式的，在那段程式的註解寫上原版位址（「已完成」靠註解裡的位址自動判斷，位址不能寫錯；
     shortcut 註解提到還沒移植的函數 **不要寫位址**）。
   - **整個類別一起做**：依賴同一個類別（或同一段位址）裡其他函數的，不算「等別的系統」，一起翻。
   - 翻好的函數要**接進遊戲**：引擎裡已有簡化版的（例如鏡頭、小地圖、繪圖順序），用原版的取代它，
     舊的簡化程式刪掉。不要留沒人呼叫的新函數。
   - 依賴的是**引擎基礎**（CObj 物件生命週期、CSprGroup 精靈、資源、場景物件名單、格子資料等）時不要等：
     用引擎現有的對應（Actor、Anim/Sprite、Scene、Cell…，grep engine/src 找），沒有就補最小的一份，然後照翻。
   - 只有依賴**別的、還沒做的遊戲系統**（例如隊伍、任務、商店介面）（不同類別、位址在很後面）才留 todo，在 tools/roles.tsv 寫一句用途和等哪個系統。
   - 劇情沒用到的功能不做，記到 HANDOFF.md「劇情沒用到（以後 DLC 可用）」。
4. 讀到的新偏移寫進 tools/fields.tsv，函數用途寫進 tools/roles.tsv。
5. 新的遊戲邏輯在 engine/test/skilltest.c 加測試。`cd engine && make && make skilltest 2>&1 | tail -3` 要過（印 "skilltest ok"）。
6. `python3 tools/functool.py` 重新產生 FUNCS.tsv。
7. HANDOFF.md 該節照前面的格式加一段「進度」（做了什麼、照原版保留的怪現象、shortcut、還是 todo 的等什麼），
   把舊的 **下一步** 加刪除線，寫新的 **下一步：0x…（類別）起**。
8. 做完一整個類別（`functool.py next` 可能只給前幾個，要把同類別的都做完）就停，不要繼續下一個類別。

## 規則
- 照原版翻，不要自己設計；技術選擇自己決定，不要問問題。
- 猜的地方加 `shortcut: <限制>, <何時升級>` 註解。
- 不要 git commit、不要 push、不要碰 decomp/、story/、game/、tools/decomp_strings.py。
- 不要大量讀反編譯檔，不要整檔讀 engine 的大檔（用 grep / sed -n 讀需要的行段）。
- 手機操控最後做，這一輪不要做。

## 最後的回覆（中文、15 行內）
- 這批範圍（位址起訖、類別）
- 移植了什麼（對到哪個檔的哪個函數）
- skip / todo 各幾個、等什麼
- 測試結果最後一行
- 新的下一步位址
