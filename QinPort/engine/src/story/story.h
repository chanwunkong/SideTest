// 劇情執行環境給引擎用的介面（實作在 story_rt.c；劇情檔用的是 qinstory.h）
#pragma once
#include "qinstory.h"
#include <stdio.h>

#define STORY_MAX_OPTIONS 12
typedef struct {
    char key[16], text[256];
    intptr_t (*fn)(intptr_t *a);  // 劇情檔的方法（包裝）
    uint32_t addr;                // 或原版基底的位址（還沒翻）
} StoryOption;
typedef struct {
    int npc;
    char speaker[64], text[4096];
    int option_count;
    StoryOption options[STORY_MAX_OPTIONS];
    int shop;
    int store;  // 劇情叫出寄存介面（0x1004ec80 → 0x61b 訊息）  // 劇情叫出了買賣介面（原版 0x10055990 送 0x5fc）：商人的代號，引擎看到後打開、清 0
} StoryDialog;
extern StoryDialog story_dialog;  // 目前畫面上的對話

// 劇情事件（CERPCT）：start_plot 送 0x52e，引擎播 Rec/User/<alias>.rec，播完呼叫 story_plot_end（第 18 格）
typedef struct { int h; char alias[64]; } StoryPlot;
extern StoryPlot story_plot;
void story_plot_end(void);
void story_pct_start(const char *path);  // 感應區文字指令 s_pctstart（0x1001c210 → 0x1004f1a0 start_plot）

int story_new(const char *path, int parent);  // 產生物件，回傳代號
int story_player(void);
const char *story_name(int h);
int story_room(const char *alias);             // 進房間（依別名 = 場景檔名；同一房間只產生一次）
int story_find(int room, const char *alias);   // 房間裡的物件
void story_refresh(int room);                  // 房間刷新：裡面的物件呼叫第 6 格（商店進貨）
const char *story_start_room(void);            // 新遊戲的開場房間別名
void story_talk(int npc);    // 點 NPC 說話
void story_choose(int i);    // 選第 i 個回答

// 道具：原版劇情的道具物件在引擎這邊是一個 Item（res.h）。引擎設 story_item_make 照 item_new 產生（malloc 的 Item）；
// 沒設（storytest）就只有物件、沒有道具
extern void *(*story_item_make)(int h, int type, int level, int col, const char *alias);  // h = 道具物件（設別名用）
int story_next(int owner, int after);       // owner 身上 after 之後的下一個物件（after = 0 從頭），沒有回 0
void *story_item(int h);                    // 物件的道具（不是道具回 NULL）
void story_take(int h);                     // 從原本的持有者身上拿走（之後由引擎管，道具不會被釋放）
int story_put(int owner, void *item);       // 把引擎的道具（malloc 的）放到 owner 身上
// 引擎接的事件：加經驗（0x1003d270）、系統訊息（0x1004dd00，kind 7 得到、8 失去、13 經驗…）
extern void (*story_on_exp)(int h, int exp);
extern int (*story_exp_for)(int level);  // 升到 level+1 要的累計經驗（引擎的經驗表），沒設 = 隊友不升級
extern int story_exp_max;                // 最高等級
void story_gain_exp(int h, int exp);     // 隊友（非玩家人物）加經驗，連升級（0x10026d30）
extern void (*story_on_log)(int h, int kind, const char *text);
// 主角身上的道具在引擎的 inv：物件留著（別名、屬性給劇情找），item 交給引擎（held）
void story_hold(int h);
void story_set_item(int h, void *item);     // 道具回到物件身上（不再 held）
void story_unhold(int owner);               // owner 身上 held 的物件先拿下來（引擎再依 inv 放回）
int story_parent(int h);
void story_move(int h, int parent);
void story_set_alias(int h, const char *s);
int story_get_int(int h, const char *key);
void story_set_int(int h, const char *key, int v);
// 存檔：整個劇情世界（物件、屬性、旗標、道具）寫進 f／讀回；item_size = 引擎的 sizeof(Item)
void story_save(FILE *f, size_t item_size);
int story_load(FILE *f, size_t item_size);

// ---- 世界（房間、出口、人物）----
const char *story_alias(int h);
const char *story_path(int h);
int story_kind(int h);                          // 0 房間 1 人物 2 道具 5 技能 6 劇情事件；−1 不存在
int story_has_int(int h, const char *key);
const char *story_get_str(int h, const char *key);
void story_set_str(int h, const char *key, const char *s);
void story_set_name(int h, const char *s);
int story_room_by_path(const char *path);        // 依路徑取房間（還沒產生就產生）
const char *story_room_alias(int room);          // 房間別名 = 場景檔名
int story_enter(int room);                       // 主角走進房間（查出入、隊員跟著、第 16 格）；不讓進回 0
void story_place(int room);                      // 主角直接放進房間（新遊戲、讀檔）
int story_exit(int room, const char *dir, char *path, int cap, int *entry);  // 出口（開著才回 1）
int story_exit_on(int room, const char *dir);    // 出口開關；沒有這個出口回 −1
int story_random_exit(int room, const char *dir, char *path, int cap, int *entry);  // 隨機地下城出口（有換回 1）
int story_die(int h);                            // 被打倒：第 10 格同意就刪掉，回 1
void story_delete(int h);
int story_team_next(int leader, int after);
// 引擎提供：依怪物表設人物數值（0x100730c0；h 已是人物物件）、依技能表設技能物件（0x100757a0）、
// 給畫面的訊息（id = 原版訊息編號；原版沒有編號、直接呼叫函式的用該函式位址的低位，例如 0x47870 = 屬性更新）
extern int (*story_on_person)(int h, const char *name, int level, int tier);
extern int (*story_on_skill)(int h, const char *key, int level);
extern void (*story_on_msg)(int id, const intptr_t *v, int n, const char *s);
extern const char *(*story_on_text)(int id);  // 系統訊息表 EasyRPG_C.dlg 第 id 筆（UTF-8），主程式提供
