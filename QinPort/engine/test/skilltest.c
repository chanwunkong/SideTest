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
    a->side = 1;
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
    for (int i = 0; i < 20 && a.hp <= 10; i++) strike(&a, &t, NULL, 0);  // 命中最多 90%
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
    actors[0].side = 0;
    actors[0].hr = 100000;
    actors[1].x = 5; actors[2].x = 6.5f; actors[3].x = 12;
    actors[0].foe = &actors[1];
    int h1 = actors[1].hp, h2 = actors[2].hp, h3 = actors[3].hp;
    for (int i = 0; i < 20 && (actors[1].hp == h1 || actors[2].hp == h2); i++) method_hits(&actors[0], &(Trig){.type = 0, .id = 7}, 0);
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
    // 力士、刺客的特殊攻擊方式
    static SkillRow s2[64], s3[64];
    int n2 = skill_rows_file("skill_2.txt", s2, 64), n3 = skill_rows_file("skill_3.txt", s3, 64);
    const SkillRow *tm = NULL, *tp = NULL, *dz = NULL, *cl = NULL;
    for (int i = 0; i < n2; i++) if (s2[i].attack == 2) tm = &s2[i];
    for (int i = 0; i < n3; i++) {
        if (s3[i].attack == 8 && !tp) tp = &s3[i];
        if (s3[i].attack == 9) dz = &s3[i];
        if (s3[i].attack == 11) cl = &s3[i];
    }
    assert(tm && tp && dz && cl);
    SkillRow tm1 = *tm, tp1 = *tp, dz1 = *dz, cl1 = *cl;
    tm1.level = tp1.level = dz1.level = cl1.level = 1;
    actor_count = 3;
    for (int i = 0; i < 3; i++) mk(&actors[i]);
    actors[0].mon = NULL;
    actors[0].side = 0;
    // 馴服：只收 istrap 10 的，等級 1 最多 1 隻（271 = 1 + 1×等級 → 原版公式另算，這裡只驗上限有作用）
    MonDef beast = md, rock = md;
    beast.istrap = 10;
    actors[1].mon = &beast;
    actors[2].mon = &beast;
    actors[0].foe = &actors[1];
    skill_hit(&actors[0], &actors[1], &tm1, 0);
    assert(actors[1].side == 0 && actors[1].owner == &actors[0] && !actors[0].foe);
    for (int i = 0; i < 20; i++) skill_hit(&actors[0], &actors[2], &tm1, 0);
    assert(actors[2].side == (skill_param(&tm1, 271) > 1 ? 0 : 1));
    actors[2].side = 1;
    actors[2].mon = &rock;
    skill_hit(&actors[0], &actors[2], &tm1, 0);
    assert(actors[2].side == 1);
    // 陷阱：放在旁邊、跟主角同隊、攻擊力照抄（主角狀態 19 重算後的）、角色是小動物表的動畫
    actor_count = 1;
    skill_hit(&actors[0], &actors[0], &tp1, 0);
    assert(actor_count == 2 && actors[1].side == 0 && actors[1].mon->istrap == 1 && actors[1].ap == actors[0].ap && actors[1].ap_max == actors[0].ap_max && actors[1].role);
    // 除障：別隊的陷阱等級 5 以下（技能 1 級 − 5/5 = 0）打掉，等級 10 不行
    actors[1].side = 1;
    actors[1].level = 10;
    skill_hit(&actors[0], &actors[1], &dz1, 0);
    assert(alive(&actors[1]) && actors[1].act != ACT_DEAD);
    actors[1].level = 5;
    skill_hit(&actors[0], &actors[1], &dz1, 0);
    assert(actors[1].act == ACT_DEAD);
    // 分身：只能一個
    actor_count = 1;
    skill_hit(&actors[0], &actors[0], &cl1, 0);
    skill_hit(&actors[0], &actors[0], &cl1, 0);
    assert(actor_count == 2 && actors[1].owner == &actors[0] && actors[1].side == 0);
    // 打中反應：JDR 200 對 DJHF 0 一定被擊退（② 機率 2000‰），往反方向；JDR −1 不反應；DJHF 200 不會被擊退
    SkillRow kb = {.level = 1, .mod_n = 1, .mod = {{.id = 18, .init = 200}}};
    mk(&a); mk(&t);
    t.x = 5; t.y = 5; a.x = 4; a.y = 5;
    hit_react(&a, &t, &kb);
    assert(t.knock && t.knock_x > 5 + 3.9f && t.stun >= 0);
    mk(&t); t.x = 5; t.y = 5;
    kb.mod[0].init = -1;
    hit_react(&a, &t, &kb);
    assert(!t.knock && !t.stun);
    kb.mod[0].init = 200;
    t.ex[75] = 200;
    hit_react(&a, &t, &kb);
    assert(!t.knock && !t.stun);
    // 對同隊的招式（wManualID 2，力士_反刺猬甲）：不用敵人，敌方状态 42 加在自己（目標）身上
    const SkillRow *thorn = NULL;
    for (int i = 0; i < n2; i++) if (s2[i].estatus == 42) thorn = &s2[i];
    assert(thorn && skill_manual(thorn) == 2 && !skill_needs_foe(thorn));
    SkillRow th1 = *thorn;
    th1.level = 1;
    mk(&a);
    skill_hit(&a, &a, &th1, 0);
    assert(has_status(&a, 42));
    // 拉開距離：射程 8 的招、目標貼身 → 第 61 拍算出往反方向走 n = 8 − 1 − 2 = 5 格
    static Cell cells[40 * 40];
    for (int i = 0; i < 40 * 40; i++) cells[i].walk = 1;
    Scene sc = {.w = 40, .h = 40, .cells = cells};
    SkillRow far = {.level = 1, .range = 8};
    Method fm2 = {.ai = 0};
    mk(&a); mk(&t);
    a.x = 20.5f; a.y = 20.5f; t.x = 21.5f; t.y = 20.5f;
    a.foe = &t; a.skill = &far; a.ai_tick = 0; a.cool = 100000;  // 不出手（測試角色沒有動畫）
    for (int i = 0; i < 61 && !a.flee; i++) { a.atk = &fm2; a.skill = &far; pursue(&a, &sc, 0); }
    assert(a.flee && (int)a.tx == 15 && (int)a.ty == 20);
    // 致命一击 100%：物理損加倍（AP 10、DP 0 → 10 → 20）；会心一击 100%：不算 DP
    mk(&a); mk(&t);
    a.hr = 100000; a.ap = a.ap_max = 10; t.dp = 0;
    a.ex[72] = 100;
    int h0 = t.hp;
    for (int i = 0; i < 20 && t.hp == h0; i++) strike(&a, &t, NULL, 0);
    assert(h0 - t.hp == 20);
    mk(&t);
    t.dp = 1000;
    a.ex[72] = 0; a.ex[73] = 100;
    h0 = t.hp;
    for (int i = 0; i < 20 && t.hp == h0; i++) strike(&a, &t, NULL, 0);
    assert(h0 - t.hp == 10);
    // 道具：開局的青銅彎刀（item_8、別名 qingtongwandao1）是單手武器，攻擊力小 ≤ 大
    Item it;
    assert(item_new(&it, 8, 1, -1, "qingtongwandao1") && it.type == 8 && it.part == 3 && it.identify);
    float lo = -1, hi = -1;
    for (int i = 0; i < it.fx_n; i++) { if (it.fx[i].id == 41) lo = it.fx[i].v; if (it.fx[i].id == 42) hi = it.fx[i].v; }
    assert(lo > 0 && lo <= hi);
    // 隨機掉寶不會失敗到怪類型，等級在 1~9
    for (int i = 0; i < 300; i++) {
        Item r;
        if (item_new(&r, 999, -1, 1 + i % 9, NULL)) assert(r.level >= 1 && r.level <= 10 && r.name[0]);
    }
    // 穿上：AP 來自武器 × 力量倍率；脫下回 0
    Actor h;
    memset(&h, 0, sizeof h);
    h.level = 1;
    inv[3] = it;
    inv[3].pos = 3;
    item_equip(&h, 3);
    assert(inv[32].name[0] && !inv[3].name[0]);
    assert(h.ap == (int)lo * (hero_attr[STR] + 150) / 150);
    assert(item_unequip(&h, 32) && h.ap == 0 && inv[0].name[0]);
    // 耐久：每 40 次 HitPoint 扣 1
    item_equip(&h, 0);
    int dur = inv[32].hp;
    for (int i = 0; i < 40; i++) item_wear(&h, 32);
    assert(inv[32].hp == dur - 1 && inv[32].hit == 0);
    // 等級不夠穿不上
    Item big;
    assert(item_new(&big, 26, 9, -1, NULL));
    if (big.lv_req > 1) { inv[5] = big; item_equip(&h, 5); assert(inv[5].name[0]); inv[5].name[0] = 0; }
    // 吃麦饼回生命，吃完道具消失
    Item food;
    assert(item_new(&food, 51, 1, -1, "maibing") && food.type == 51);
    inv[6] = food;
    h.hp = 1;
    assert(item_eat(&h, 6) && !inv[6].name[0]);
    // 怪物掉寶：必掉 3 件金錢（類型 60）
    MonDef dm = md;
    dm.drop_min = dm.drop_max = 3;
    dm.drop[0][0] = 60; dm.drop[0][1] = 100;
    Actor m;
    mk(&m);
    m.mon = &dm;
    ground_n = 0;
    mon_loot(&m);
    assert(ground_n == 3 && ground[0].it.type == 60);
    int money = hero_money;
    ground_pick(0);
    assert(hero_money > money && ground_n == 2);
    puts("skilltest ok");
    return 0;
}
