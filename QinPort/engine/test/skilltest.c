// 技能戰鬥規則的自我檢查：直接呼叫 main.c 的 strike／buff_add 等，不開視窗。用法：make skilltest（要有 QinPort/game）
#define main qin_main
#include "../src/main.c"
#undef main
#include <assert.h>

static RoleDef def;
static MonDef md;

static void mk(Actor *a) {
    memset(a, 0, sizeof *a);
    a->role = (const Role *)1;  // alive() 只看有沒有角色
    a->def = &def;
    a->mon = &md;
    a->level = 1;
    mon_derive(a);
    a->hp = a->hp_max;
}

int main(void) {
    res_init("../../game");
    md.hp = (Stat){100000, 0};
    md.ap = md.ap_max = (Stat){10, 0};
    Actor a, t;
    // 中毒：MAP2 50、中毒機率 1000 → 成功率 90%；中毒 = 狀態 1，HP_SPEED = -8 × 50
    mk(&a); mk(&t);
    a.ex[45] = a.ex[46] = 50;
    a.ex[82] = 1000;
    a.hr = 100000;
    for (int i = 0; i < 20 && !has_status(&t, 1); i++) strike(&a, &t, NULL, 0);
    assert(has_status(&t, 1));
    assert(t.hp_speed == -400);
    int hp = t.hp;
    actor_second(&t);
    assert(t.hp == hp - 4);
    // 同一個狀態加不上第二次；眩暈（10）不能攻擊、不能動
    assert(!buff_add(&t, NULL, 1, 1, 5, 0));
    assert(buff_add(&t, NULL, 10, 1, 5, 0) && status_noattack(&t) && status_speed(&t) == 0);
    // 冰凍（4）速度 50%；時間到移除
    mk(&t);
    assert(buff_add(&t, NULL, 4, 1, 2, 0) && status_speed(&t) == 50);
    actor_second(&t); actor_second(&t);
    assert(!has_status(&t, 4) && status_speed(&t) == 100);
    // 石化（2）DP 加倍；減物理傷害 75% 封頂
    mk(&t);
    int dp = t.dp = 10;
    md.dp = (Stat){10, 0};
    mon_derive(&t);
    dp = t.dp;
    buff_add(&t, NULL, 2, 1, 5, 0);
    assert(t.dp == dp * 2);
    // 吸血：B_XQSM 100% → 打多少補多少
    mk(&a); mk(&t);
    a.hr = 100000;
    a.ex[70] = 100;
    a.hp = 10;
    strike(&a, &t, NULL, 0);
    assert(a.hp > 10);
    // 中毒扣到 0 會死（不是無敌）
    mk(&t);
    buff_add(&t, NULL, 1, 1, 50, 100000);
    t.hp = 1;
    actor_second(&t);
    assert(t.act == ACT_DEAD);
    // 召喚：匈奴法师_召唤（273 = 10 → 朱厌，最多 2 隻）
    roledefs_load();
    mondefs_load();
    const SkillRow *sm = npc_skill("\xd0\xd9\xc5\xab\xb7\xa8\xca\xa6_\xd5\xd9\xbb\xbd");
    assert(sm && sm->attack == 3);
    actor_count = 1;
    mk(&actors[0]);
    for (int i = 0; i < 5; i++) skill_hit(&actors[0], NULL, sm, 0);
    assert(actor_count == 3 && actors[1].owner == &actors[0]);
    // 改召狼（前 6 種）：先移掉別種（朱厌）
    SkillRow wolf = *sm;
    for (int i = 0; i < wolf.mod_n; i++) if (wolf.mod[i].id == 273) wolf.mod[i].init = 1, wolf.mod[i].incr = 0;
    skill_hit(&actors[0], NULL, &wolf, 0);
    assert(!actors[1].role && !actors[2].role && actor_count == 4 && actors[3].role);
    // 範圍打擊：type 0 ID 7 = 目標周圍 2 格內全部；ID 0 = 只打目標
    actor_count = 4;
    for (int i = 0; i < 4; i++) mk(&actors[i]);
    actors[0].mon = NULL;
    actors[0].hr = 100000;
    actors[1].x = 5; actors[2].x = 6.5f; actors[3].x = 12;
    actors[0].foe = &actors[1];
    int h1 = actors[1].hp, h2 = actors[2].hp, h3 = actors[3].hp;
    method_hits(&actors[0], &(Trig){.type = 0, .id = 7}, 0);
    assert(actors[1].hp < h1 && actors[2].hp < h2 && actors[3].hp == h3);
    h2 = actors[2].hp;
    method_hits(&actors[0], &(Trig){.type = 0, .id = 0}, 0);
    assert(actors[2].hp == h2);
    // 箭（普通弓箭1，bData3 = 30）：5 格 = 320 像素 → 第 10 拍才打中
    mk(&a); mk(&t);
    t.x = 5;
    a.foe = &t;
    a.hr = 100000;
    hp = t.hp;
    missile_launch(&a, unit_find(0), 0);
    for (int i = 0; i < 9; i++) missile_tick(0);
    assert(t.hp == hp && missile_count == 1);
    missile_tick(0);
    assert(t.hp < hp && missile_count == 0);
    const Anim *arrow = anim_get(unit_find(0)->anim);
    printf("arrow seqs %d\n", arrow ? arrow->count : -1);
    // 怪物選招：老虎（抓、扑、冲）三招都會被選到
    MonDef tiger = md;
    strcpy(tiger.skill[0], "\xc0\xcf\xbb\xa2_\xd7\xa5");
    strcpy(tiger.skill[1], "\xc0\xcf\xbb\xa2_\xc6\xcb");
    strcpy(tiger.skill[2], "\xc0\xcf\xbb\xa2_\xb3\xe5");
    mk(&a); mk(&t);
    a.mon = &tiger;
    a.foe = &t;
    t.x = 1;
    int seen[3] = {0};
    for (int i = 0; i < 600; i++) {
        const SkillRow *r = mon_pick(&a);
        for (int k = 0; r && k < 3; k++) seen[k] += r == npc_skill(tiger.skill[k]);
        if (r) a.last_skill = r;
    }
    printf("tiger picks %d %d %d\n", seen[0], seen[1], seen[2]);
    for (int k = 0; k < 3; k++) {
        const SkillRow *r = npc_skill(tiger.skill[k]);
        const Method *m = r ? method_find(skill_attack_id(r)) : NULL;
        printf("  skill %d: manual %d ai %d range %d\n", k, m ? m->manual : -1, m ? m->ai : -1, r ? skill_range_max(r) : -1);
    }
    assert(seen[0] && seen[1]);
    // 招式旗標：動作中設 0x10，動作結束（iFlag 3）清掉
    Method fm = {.trig_count = 2, .trig = {{.flag = 0, .type = 8, .id = 16, .id2 = 1}, {.flag = 3, .type = 8, .id = 16, .id2 = 0}}};
    mk(&a);
    act_flags(&a, &fm.trig[0]);
    assert(a.flags == 16);
    a.act = ACT_ATTACK;
    a.atk = &fm;
    act_end(&a);
    assert(a.flags == 0);
    puts("skilltest ok");
    return 0;
}
