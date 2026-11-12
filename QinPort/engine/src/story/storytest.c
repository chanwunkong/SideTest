// 不用畫面測劇情：storytest <物件路徑> [選項序號...]，例如 storytest 秦殇/咸阳/NPC/绣女 0 1
#include "story.h"
#include <stdio.h>
#include <stdlib.h>

static void show(void) {
    printf("【%s】%s\n", story_dialog.speaker, story_dialog.text);
    for (int i = 0; i < story_dialog.option_count; i++) printf("  %d. %s（%s）\n", i, story_dialog.options[i].text, story_dialog.options[i].key);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    story_register_all();
    int npc = story_new(argv[1], 0);
    story_talk(npc);
    show();
    for (int i = 2; i < argc; i++) {
        printf("> 選 %s\n", argv[i]);
        story_choose(atoi(argv[i]));
        show();
    }
    return 0;
}
