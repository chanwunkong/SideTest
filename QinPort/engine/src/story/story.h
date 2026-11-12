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
} StoryDialog;
extern StoryDialog story_dialog;  // 目前畫面上的對話

int story_new(const char *path, int parent);  // 產生物件，回傳代號
int story_player(void);
const char *story_name(int h);
void story_talk(int npc);    // 點 NPC 說話
void story_choose(int i);    // 選第 i 個回答
