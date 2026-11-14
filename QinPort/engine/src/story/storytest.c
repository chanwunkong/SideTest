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

int main(int argc, char **argv) {
    if (argc < 2) return 1;
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
