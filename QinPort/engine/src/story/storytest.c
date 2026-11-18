// 不用畫面測劇情：storytest <物件路徑> [選項序號...]，例如 storytest 秦殇/咸阳/NPC/绣女 0 1
// 參數 give=物件路徑 先放到主角身上，flag=旗標=值 先設主角的旗標（放在選項前面）；最後印主角身上的東西
#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qinstory.h"

static void show(void) {
    printf("【%s】%s\n", story_dialog.speaker, story_dialog.text);
    for (int i = 0; i < story_dialog.option_count; i++) printf("  %d. %s（%s）\n", i, story_dialog.options[i].text, story_dialog.options[i].key);
}

static int skill_stub(int h, const char *key, int level) { story_set_alias(h, key); story_set_int(h, "Level_Max", 10); story_set_int(h, "Level_Req", 1); story_set_int(h, "Level", level); return 1; }
static int exp_for(int lv) { return lv * lv * 10; }  // 測試用的經驗表（升到 lv+1 要 lv²×10）

// storytest team=<隊友路徑> [exp=主角經驗]：隊友入隊（0x10037850 → 自動加點 0x100295b0），印等級、經驗、點數、屬性和技能等級
static int team_test(const char *path, int exp) {
    int pl = story_player();
    story_exp_for = exp_for, story_exp_max = 40, story_on_skill = skill_stub;
    story_set_int(pl, "Exp", exp);
    int m = story_new(path, 0);
    printf("入隊前 Level %d Exp %d Extra_Exp %d Wisdom %d Wisdom0 %d pic2 %d Savvy %d 主角 Savvy %d\n", story_get_int(m, "Level"), story_get_int(m, "Exp"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Wisdom"), story_get_int(m, "Wisdom0"), story_get_int(m, "G_PicID2"), story_get_int(m, "Savvy"), story_get_int(pl, "Savvy"));
    engine_call(0x10037850, (intptr_t[8]){pl, m});
    printf("隊友 %s Level %d Exp %d NextExp %d Extra_Exp %d Extra_Skill %d\n", story_name(m), story_get_int(m, "Level"), story_get_int(m, "Exp"),
           story_get_int(m, "NextExp"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Extra_Skill"));
    printf("屬性 Str %d Con %d Dex %d Savvy %d Charm %d Wisdom %d\n", story_get_int(m, "Str"), story_get_int(m, "Con"), story_get_int(m, "Dex"),
           story_get_int(m, "Savvy"), story_get_int(m, "Charm"), story_get_int(m, "Wisdom"));
    for (int h = story_next(m, 0); h; h = story_next(m, h)) printf(" %s=%d", story_alias(h), story_get_int(h, "Level"));
    printf("\n");
    // 檢查：術士方仲（G_PicID2 18，表：Wisdom 基礎 35、每級 3 點）優先加 Wisdom；技能點花掉一部分（剩下 < 升的級數）
    int lv = story_get_int(m, "Level"), wis = 35 + (int)(3.0f * (lv - 1)), skills = 0, learned = 0;
    for (int h = story_next(m, 0); h; h = story_next(m, h)) if (story_get_int(h, "Level") > 0) skills += story_get_int(h, "Level"), learned++;
    if (story_get_int(m, "Wisdom") != wis || story_get_int(m, "Extra_Skill") >= lv || !learned) { printf("FAIL team_autoadd wis %d/%d skills %d\n", story_get_int(m, "Wisdom"), wis, skills); return 1; }
    story_gain_exp(m, 1000);
    if (story_get_int(m, "Level") <= lv || story_get_int(m, "Extra_Skill") <= 0) { printf("FAIL story_gain_exp\n"); return 1; }
    printf("team ok\n");  // 再加經驗連升級：每級 Extra_Exp +10、Extra_Skill +1
    printf("加 1000 經驗後 Level %d Extra_Exp %d Extra_Skill %d\n", story_get_int(m, "Level"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Extra_Skill"));
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    if (!strncmp(argv[1], "team=", 5)) { story_register_all(); return team_test(argv[1] + 5, argc > 2 ? atoi(argv[2]) : 3000); }
    story_register_all();
    int pl = story_player();
    int a = 2;
    for (; a < argc && strchr(argv[a], '='); a++) {
        char *eq = strchr(argv[a], '=');
        *eq = 0;
        if (!strcmp(argv[a], "give")) story_new(eq + 1, pl);
        char *v = strchr(eq + 1, '=');
        if (!strcmp(argv[a], "flag") && v) { *v = 0; engine_call(0x1005e850, (intptr_t[8]){pl, (intptr_t)(eq + 1), atoi(v + 1)}); }
    }
    int npc = story_new(argv[1], 0);
    story_talk(npc);
    show();
    for (int i = a; i < argc; i++) {
        printf("> 選 %s\n", argv[i]);
        story_choose(atoi(argv[i]));
        show();
    }
    printf("主角身上：");
    for (int h = story_next(pl, 0); h; h = story_next(pl, h)) printf(" %s", story_name(h));
    printf("\n");
    return 0;
}
