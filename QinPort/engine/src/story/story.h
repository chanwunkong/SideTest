// 劇情執行環境給引擎用的介面（實作在 story_rt.c；劇情檔用的是 qinstory.h）
#pragma once
#include "qinstory.h"

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
    int shop;  // 劇情叫出了買賣介面（原版 0x10055990 送 0x5fc）：商人的代號，引擎看到後打開、清 0
} StoryDialog;
extern StoryDialog story_dialog;  // 目前畫面上的對話

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
extern void *(*story_item_make)(int type, int level, int col, const char *alias);
int story_next(int owner, int after);       // owner 身上 after 之後的下一個物件（after = 0 從頭），沒有回 0
void *story_item(int h);                    // 物件的道具（不是道具回 NULL）
void story_take(int h);                     // 從原本的持有者身上拿走（之後由引擎管，道具不會被釋放）
int story_put(int owner, void *item);       // 把引擎的道具（malloc 的）放到 owner 身上
int story_get_int(int h, const char *key);
void story_set_int(int h, const char *key, int v);
