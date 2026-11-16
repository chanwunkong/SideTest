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

static void viewport_test(void) {
    int old_w = vw, old_h = vh;
    vw = 640; vh = 480;
    Cell *cells = calloc(64 * 64, sizeof *cells);
    for (int i = 0; i < 64 * 64; i++) cells[i].view = 2, cells[i].ground = cells[i].over = 0xffff;
    Scene sc = {.w = 64, .h = 64, .cells = cells, .view_w = 8192, .view_h = 4096};
    ViewPort v = {.world = {0, 0, 8192, 4096}};
    viewport_window(&v, &sc, 100, 200, vw, vh);
    assert(v.area == 307200 && v.ref_x == 320 && v.ref_y == 240 && v.dead_x == 295 && v.dead_y == 215);
    SDL_Point gp = viewport_grid_screen(&v, 20, 20);
    assert(gp.x == world_x(&sc, 20, 20) - 100 && gp.y == world_y(&sc, 20, 20) - 200);
    gp = viewport_world_screen(&v, 20 * 64 + 3, 20 * 64 + 7);
    SDL_Point wp = viewport_project(&sc, 20 * 64 + 3, 20 * 64 + 7);
    assert(gp.x == wp.x - 100 && gp.y == wp.y - 200);
    // 舊換算函數都由鏡頭狀態更新實際使用。
    assert(v.center.x == v.base.x + 15 && v.center.y == v.base.y + 10);
    v.pos = (SDL_Point){100, 210};
    viewport_scroll(&v, &sc, -1);
    gp = viewport_grid_screen(&v, 20, 20);
    viewport_cache(&v, &sc, 1);
    SDL_Point corrected = viewport_grid_screen(&v, 20, 20);
    assert(gp.x == corrected.x + 32 && gp.y == corrected.y); // 原捲動入口沒有奇列X補正
    v.pos = (SDL_Point){0, 0}; viewport_cache(&v, &sc, 1);
    gp = viewport_screen_grid(&v, 32, 0);
    assert(gp.x == -32 && gp.y == 31);
    // CRHOMB 邊線相等留原格；負餘數補正不更改向零商。
    ViewPort cv = {.base = {0, 0}, .rem = {0, 0}};
    gp = viewport_screen_grid(&cv, 30, 0); assert(gp.x == 0 && gp.y == 0);
    gp = viewport_screen_grid(&cv, 29, 0); assert(gp.x == -1 && gp.y == 0);
    gp = viewport_screen_grid(&cv, -1, -1); assert(gp.x == 1 && gp.y == 0);
    viewport_set(&v, &sc, 101, 203); assert(v.pos.x == 100 && v.pos.y == 202);
    v.step = 6;
    viewport_scroll(&v, &sc, 7); assert(v.pos.x == 106);
    viewport_scroll(&v, &sc, 3); assert(v.pos.x == 100);
    viewport_scroll(&v, &sc, 1); assert(v.pos.y == 196);
    viewport_scroll(&v, &sc, 5); assert(v.pos.y == 202);
    // 平移用平方距離，走過頭後距離重複即到目標；震動只在5的倍數捲動。
    Actor a = {.x = 20, .y = 20, .speed = 8};
    v.follow = 0; v.pan = 20; v.target = (SDL_Point){110, 202}; v.last_distance = -1;
    viewport_tick(&v, &sc, &a); assert(v.pos.x == 120 && v.pan == 20);
    viewport_tick(&v, &sc, &a); assert(v.pos.x == 110 && v.pan == 0);
    shake_left = shake_total = 10; int sy = v.pos.y;
    viewport_tick(&v, &sc, &a); assert(v.pos.y == sy - 26 && shake_left == 9);
    sy = v.pos.y; viewport_tick(&v, &sc, &a); assert(v.pos.y == sy && shake_left == 8);
    shake_left = 5; viewport_tick(&v, &sc, &a); assert(v.pos.y == sy + 12);
    shake_left = shake_total = 0;
    // 跟隨有死區，不會每幀重設為主角中心；靜止只修正越界差額+2。
    wp = viewport_project(&sc, 20 * 64, 20 * 64);
    v.last_actor = (SDL_Point){20 * 64, 20 * 64};
    v.pos = (SDL_Point){wp.x - v.ref_x - 100, wp.y - v.ref_y};
    viewport_cache(&v, &sc, 1);
    SDL_Point before = v.pos;
    viewport_follow(&v, &sc, &a); assert(v.pos.x == before.x && v.pos.y == before.y);
    v.pos.x = wp.x - v.ref_x - 300; viewport_cache(&v, &sc, 1);
    viewport_follow(&v, &sc, &a); assert(v.step == 7 && v.pos.x == (wp.x - v.ref_x - 300 + 7) / 2 * 2);
    // 置中使用角色整格的交錯投影，平滑步長則用角色當前畫面位置。
    SDL_Point neg = viewport_point_pixel((SDL_Point){2, -1});
    assert(neg.x == 96 && neg.y == -32);
    v.pan = 17; viewport_follow_set(&v, &sc, &a, 0);
    assert(!v.follow && v.pan == 17); // 關閉跟隨不取消平移
    v.last_distance = 123;
    viewport_pan(&v, 200, 300, 7);
    assert(v.target.x == 200 && v.target.y == 300 && v.pan == 7 && v.last_distance == 123);
    viewport_home(&v, &sc, NULL, 1); assert(v.pan == 7);
    a.x = 20.5f; a.y = 20.5f;
    viewport_home(&v, &sc, &a, 0);
    SDL_Point center = viewport_point_pixel(viewport_grid_point(20, 20, sc.w, sc.h / 2));
    ViewPort expected = v;
    viewport_set(&expected, &sc, center.x - v.w / 2, center.y - v.h / 2);
    assert(v.pos.x == expected.pos.x && v.pos.y == expected.pos.y && v.pan == 7);
    SDL_Point actor_screen = viewport_world_screen(&v, 1312, 1312);
    v.ref_x = actor_screen.x - 29; v.ref_y = actor_screen.y;
    viewport_follow_set(&v, &sc, &a, 1); assert(v.follow && v.pan == 7);
    v.ref_x = actor_screen.x - 30;
    viewport_home(&v, &sc, &a, 1);
    assert(v.pan == 3 && v.target.x == center.x - v.w / 2 && v.target.y == center.y - v.h / 2);
    v.ref_x = v.w / 2; v.ref_y = v.h / 2;
    ViewList l = {0}; SDL_Rect screen = {0, 0, vw, vh};
    viewport_insert(&l, (ViewItem){.index = 1, .depth = 20, .rect = {10, 10, 20, 20}}, screen);
    viewport_insert(&l, (ViewItem){.index = 2, .depth = 20, .rect = {10, 10, 20, 20}}, screen);
    viewport_insert(&l, (ViewItem){.index = 3, .depth = 40, .rect = {10, 10, 20, 20}}, screen);
    viewport_insert(&l, (ViewItem){.index = 4, .depth = 99, .rect = {640, 0, 20, 20}}, screen);
    assert(l.n == 3 && l.items[0].index == 3 && l.items[1].index == 2 && l.items[2].index == 1);
    assert(viewport_intersects(&l, (SDL_Rect){29, 29, 3, 3}));
    assert(!viewport_intersects(&l, (SDL_Rect){30, 30, 3, 3}));
    Model wall[2] = {{.block_y = 2}, {0}};
    Object buildings[2] = {{.x = 0, .y = 0, .m = &wall[0]}, {.x = 1, .y = 1, .m = &wall[1]}};
    Scene blocked = sc; blocked.objects = buildings; blocked.object_count = 2;
    ViewItem walls[2] = {{.index = 1, .depth = 128, .rect = {0, 0, 100, 100}},
                         {.index = 0, .depth = 0, .rect = {0, 0, 100, 100}}};
    ViewList saved_buildings = view_buildings;
    view_buildings = (ViewList){walls, 2, 2};
    ViewItem who = {.depth = 64, .x64 = 32, .y64 = 32, .rect = {0, 0, 100, 100}};
    assert(viewport_occlusion_start(&view_buildings, &blocked, &who, who.rect) == 0);
    who.x64 = -1; who.y64 = 20; who.depth = 19;
    assert(viewport_occlusion_start(&view_buildings, &blocked, &who, who.rect) == 1);
    who = (ViewItem){.depth = 64, .x64 = 32, .y64 = 32, .rect = {0, 0, 100, 100}};
    cells[0].flags = 0x82; cells[0].building = 0;
    assert(viewport_occlusion_start(&view_buildings, &blocked, &who, who.rect) == 1);
    cells[0].flags = 0; cells[0].building = -1;
    view_buildings = saved_buildings;
    cells[0].view = 1;
    viewport_visible_insert(&l, &sc, (ViewItem){.rect = {0, 0, 10, 10}}, screen);
    assert(l.n == 3);
    cells[0].view = 2;
    viewport_visible_insert(&l, &sc, (ViewItem){.rect = {0, 0, 10, 10}}, screen);
    assert(l.n == 4);
    // 軟體 renderer 驗證真實分層：A 的上層要覆過後畫的 B 中層。
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, vw, vh, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf); ren = SDL_CreateSoftwareRenderer(surf); assert(ren);
    const uint32_t keys[3] = {0xeee001, 0xeee002, 0xeee003};
    const Uint8 colors[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    for (int i = 0; i < 3; i++) {
        Sprite *sp = slot_find(keys[i]); sp->w = sp->h = 8;
        uint32_t *px = malloc(64 * sizeof *px);
        for (int j = 0; j < 64; j++) px[j] = SDL_MapRGBA(surf->format, colors[i][0], colors[i][1], colors[i][2], 255);
        assert(upload(sp, px));
    }
    Model models[2] = {0};
    models[0].part_count = 2; models[0].bottom = 1; models[0].top = 1;
    models[0].parts[0].gid = keys[0]; models[0].parts[1].gid = keys[2];
    models[1].part_count = 1; models[1].parts[0].gid = keys[1];
    Object objects[2] = {{.x = 0, .y = 0, .m = &models[0]}, {.x = 1, .y = 0, .m = &models[1]}};
    models[1].parts[0].dx = -32; models[1].parts[0].dy = -16;
    sc.objects = objects; sc.object_count = 2;
    v.base = (SDL_Point){0, 0}; v.rem = (SDL_Point){0, 0};
    view_buildings.n = 0;
    viewport_insert(&view_buildings, (ViewItem){.index = 0, .depth = 0, .rect = {32, 16, 8, 8}}, screen);
    viewport_insert(&view_buildings, (ViewItem){.index = 1, .depth = 64, .rect = {32, 16, 8, 8}}, screen);
    int saved_ground = ground_n, saved_units = unit_n;
    ground_n = unit_n = 0;
    viewport_layers(&v, &sc, 0); SDL_RenderPresent(ren);
    Uint8 r, g, b, alpha;
    SDL_GetRGBA(((Uint32 *)surf->pixels)[18 * (surf->pitch / 4) + 34], surf->format, &r, &g, &b, &alpha);
    assert(r == 0 && g == 0 && b == 255);
    ground_n = saved_ground; unit_n = saved_units;
    cache_clear(); SDL_DestroyRenderer(ren); ren = NULL; SDL_FreeSurface(surf);
    free(l.items); free(cells);
    view_buildings.n = view_dynamic.n = 0;
    vw = old_w; vh = old_h;
}

static void ground_labels_test(void) {
    Cell cells[32 * 32] = {0};
    Scene s = {.w = 32, .h = 32, .cells = cells};
    ViewPort v = {.w = 96, .h = 64, .base = {10, 10}};
    ground_n = 3;
    ground[0].x = 10.5f; ground[0].y = 10.5f;
    ground[1].x = 11.5f; ground[1].y = 9.5f;
    ground[2].x = 10.5f; ground[2].y = 10.5f;
    strcpy(ground[0].it.name, "first");
    strcpy(ground[1].it.name, "edge");
    strcpy(ground[2].it.name, "same cell");
    cells[10 * 32 + 10].view = 1;
    cells[9 * 32 + 11].view = 2;
    memset(&ground_labels, 0, sizeof ground_labels);
    viewport_ground_labels(&v, &s, 1, 0);
    assert(!ground_labels.on);
    viewport_ground_labels(&v, &s, 1, 1);
    assert(ground_labels.on && !ground_labels.n); // 未探索與右邊界相等皆排除
    viewport_ground_labels(&v, &s, 0, 1);
    cells[10 * 32 + 10].view = 2;
    v.w = 97;
    viewport_ground_labels(&v, &s, 1, 1);
    assert(ground_labels.n == 2);
    assert(!strcmp(ground_labels.items[0].name, "first"));
    assert(!strcmp(ground_labels.items[1].name, "edge"));
    ground_n = 0;
    viewport_ground_labels(&v, &s, 1, 1);
    assert(ground_labels.n == 2); // 按住時保留快照
    viewport_ground_labels(&v, &s, 0, 0);
    assert(ground_labels.on); // 介面阻擋時連放開都不處理
    viewport_ground_labels(&v, &s, 0, 1);
    assert(!ground_labels.on && !ground_labels.n);
}

int main(void) {
    viewport_test();
    ground_labels_test();
    // 小地圖換算：正負奇數的有號餘數，以及除法向零截斷。
    SDL_Point gp = viewport_grid_point(3, 2, 10, 1);
    assert(gp.x == 4 && gp.y == 11);
    gp = viewport_grid_point(3, 3, 10, 1);
    assert(gp.x == 3 && gp.y == 12);
    gp = viewport_grid_point(-3, 2, 10, 1);
    assert(gp.x == -1 && gp.y == 5);
    gp = viewport_grid_point(-3, -3, 0, 0);
    assert(gp.x == 2 && gp.y == -4);
    gp = viewport_grid_point(-4, 3, 0, 0);
    assert(gp.x == -2 && gp.y == 1);
    // 反向換算保留原版偏移，不假設與正向換算互為反函數。
    int map_rows = 10;
    gp = viewport_point_grid((SDL_Point){3, 2}, &map_rows, 1);
    assert(gp.x == -1 && gp.y == -2);
    gp = viewport_point_grid((SDL_Point){3, 3}, &map_rows, 1);
    assert(gp.x == -1 && gp.y == -1);
    map_rows = 9;
    gp = viewport_point_grid((SDL_Point){-3, -3}, &map_rows, 0);
    assert(gp.x == -9 && gp.y == 1); // -13/2 = -6，(-3+1)/2 = -1
    gp = viewport_point_grid((SDL_Point){-3, -2}, &map_rows, 0);
    assert(gp.x == -8 && gp.y == 1); // -11/2 = -5
    gp = viewport_point_grid((SDL_Point){3, 3}, NULL, 99);
    assert(gp.x == 0 && gp.y == 0);
    // 中心格用互換的寬高除數，負數除法向零截斷。
    gp = viewport_center_grid((SDL_Point){10, 20}, 641, 479);
    assert(gp.x == 24 && gp.y == 30);
    gp = viewport_center_grid((SDL_Point){0, 0}, -65, -33);
    assert(gp.x == -1 && gp.y == -1);
    SDL_Rect bounds = {0, 0, 1000, 800};
    gp = viewport_clamp((SDL_Point){800, 600}, 200, 200, bounds);
    assert(gp.x == 798 && gp.y == 598); // 恰好碰到右下邊也減 1，再取偶數
    gp = viewport_clamp((SDL_Point){101, 203}, 200, 200, bounds);
    assert(gp.x == 100 && gp.y == 202);
    gp = viewport_clamp((SDL_Point){-9, -11}, 10, 10, (SDL_Rect){-5, -7, 100, 100});
    assert(gp.x == -4 && gp.y == -6); // 取偶數後可超過奇數的下界
    gp = viewport_clamp((SDL_Point){50, 50}, 200, 200, (SDL_Rect){3, 5, 10, 10});
    assert(gp.x == 2 && gp.y == 4); // 世界小於視窗：下界優先，最後才取偶數
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
    // 範圍打擊：type 0 ID 7 = 自己（招式物件位置 = 出手的人）周圍 2 格內全部；ID 0 = 只打目標
    actor_count = 4;
    for (int i = 0; i < 4; i++) mk(&actors[i]);
    actors[0].mon = NULL;
    actors[0].side = 0;
    actors[0].hr = 100000;
    actors[0].x = 4; actors[1].x = 5; actors[2].x = 6; actors[3].x = 12;
    actors[0].foe = &actors[1];
    int h1 = actors[1].hp, h2 = actors[2].hp, h3 = actors[3].hp;
    for (int i = 0; i < 20 && (actors[1].hp == h1 || actors[2].hp == h2); i++) method_hits(&actors[0], &(Trig){.type = 0, .id = 7}, 0);
    assert(actors[1].hp < h1 && actors[2].hp < h2 && actors[3].hp == h3);
    h2 = actors[2].hp;
    method_hits(&actors[0], &(Trig){.type = 0, .id = 0}, 0);
    assert(actors[2].hp == h2);
    // 箭（普通弓箭1，bData3 = 30）：沿路每 64 掃一次（命中方式 5），經過目標時打中、消失
    actor_count = 2;
    mk(&actors[0]); mk(&actors[1]);
    actors[0].mon = NULL;
    actors[0].side = 0;
    actors[0].hr = 100000;
    actors[0].x = actors[0].y = actors[1].y = 0.5f;
    actors[1].x = 5.5f;
    actors[0].foe = &actors[1];
    hp = actors[1].hp;
    static Cell cells32[32 * 32];  // 32×32 全部能走的測試場景
    static Scene test_scene = {.w = 32, .h = 32, .cells = cells32};
    for (int i = 0; i < 32 * 32; i++) cells32[i].walk = 1;
    unit_scene = &test_scene;
    unit_n = 0;
    MUnit src = {.cls = -1, .owner = &actors[0], .target = &actors[1], .x = 32, .y = 32, .tx = 5 * 64 + 32, .ty = 32, .range = 5, .persist = 1};
    int ticks = 0, hit_at = -1;
    for (int tries = 0; hit_at < 0 && tries < 10; tries++) {  // 命中率上限 90%，射到中為止
        assert(unit_new(0, src.x, src.y, &src, 0) && unit_n == 1 && units[0].cls == U_ARROW);
        for (ticks = 0; unit_n && ticks < 100;) {
            units_tick(ticks++);
            if (hit_at < 0 && actors[1].hp < hp) hit_at = ticks;
        }
    }
    const Unit *ad = unit_find(0);
    printf("arrow b0..5 %d %d %d %d %d %d: hit at %d, gone at %d\n", ad->b[0], ad->b[1], ad->b[2], ad->b[3], ad->b[4], ad->b[5], hit_at, ticks);
    assert(hit_at > 0 && hit_at <= 11 && unit_n == 0);
    // 角色出手：wEffect 2 打中加狀態 110 進名單；wQuality 3 連鎖 1 次時放開名單（拿掉 110）重來，第二次結束
    Method mm = {.effect = 2, .quality = 3, .r1 = 1, .range_max = 3};
    actors[0].act = ACT_ATTACK;
    actors[0].atk = &mm;
    unit_n = 0;
    main_new(&actors[0]);
    MUnit *mu = main_of(&actors[0]);
    assert(mu && mu->bounce == 1);
    main_hit(&actors[0], &actors[1], 0);
    assert(mu->hit_n == 1 && has_status(&actors[1], 110));
    assert(unit_bounce(mu) && mu->hit_n == 0 && !has_status(&actors[1], 110));
    assert(!unit_bounce(mu));
    unit_die(mu, 0);
    // 分身：殘影 idx 3 往側邊滑 16 後跟著出手者走；環繞每拍半徑 +2；出手結束後殘影淡出、環繞活滿 攻擊距離×32 拍再淡出
    static Role dummy;
    actors[0].role = &dummy;
    unit_n = 0;
    main_new(&actors[0]);
    phantom_new(main_of(&actors[0]), 1, 3);
    phantom_new(main_of(&actors[0]), 3, 0);
    MUnit *ph = &units[1], *pc = &units[2];
    assert(unit_n == 3 && ph->cls == U_PHANTOM && ph->life2 == -1 && ph->life == 16 && pc->life == mu->range * 32);
    for (int k = 0; k < 16; k++) units_tick(k);
    int px = ph->x, py = ph->y;
    assert(abs(px - 32) == 16 || abs(py - 32) == 16);
    assert(dist_of(pc->x, pc->y, 32, 32) >= 40 - 1 && dist_of(pc->x, pc->y, 32, 32) <= 40);
    actors[0].x += 1;
    units_tick(16);
    assert(ph->x == px + 64 && ph->y == py);
    actors[0].act = ACT_NONE;
    for (int k = 0; k < 200 && unit_n; k++) units_tick(17 + k);
    assert(unit_n == 0);
    actors[0].x -= 1;
    actors[0].role = NULL;
    // 腳印：300 拍停在第一幀，之後換序列 +8 播完消失
    const Anim *fa = anim_get(0x100309);
    printf("footstep seqs %d\n", fa ? fa->count : -1);
    assert(fa && fa->count >= 0x20);
    foot_new(32, 32, 0x13);
    for (int k = 0; k <= 301; k++) units_tick(k);
    assert(unit_n == 1 && units[0].mode == 1 && units[0].spr.seq == 0x1b && units[0].spr.cur == 0);
    for (int k = 0; k < 1000 && unit_n; k++) units_tick(k);
    assert(unit_n == 0);
    // 天氣：表上全是小雨 → 擲到 2，馬上開始下（小雨不等），雨量漸大；換大雪片數逐拍加；疾風粒子全部重生
    for (int i = 0; i < 10; i++) wx.cum[i] = i < 2 ? -1 : 100;
    wx.total = 100;
    for (int k = 0; k < 600; k++) weather_tick();
    printf("rain: idx %d phase %d amount %d drops %d\n", wx.idx, wx.phase, wx.rain_amt, wx.rain_live);
    assert(wx.idx == 2 && wx.phase == 2 && wx.rain_amt >= 2 && wx.rain_live > 0 && wx.left > 4000);
    weather_set(4, 2000);
    for (int k = 0; k < 100; k++) weather_tick();
    assert(wx.idx == 4 && wx.snow_on == 100 && wx.p[0].life > 0);
    weather_set(10, 1000);
    weather_tick();
    assert(wx.p[0].life >= 499 && wx.p[0].dx <= -2 && wx.p[wx.n - 1].life > 0);
    wx.total = 0;
    weather_set(0, 1000);
    // 進場接續：雨不等變暗，保留倒數與亮度，紅雨及雪立刻填滿。
    for (int kind = 1; kind <= 5; kind++) {
        weather_set(kind, 2345);
        int bright = lt.bright;
        weather_resume();
        assert(wx.left == 2345 && lt.bright == bright);
        if (kind <= 3) {
            assert(wx.phase == 2 && wx.vol == 1 && wx.rain_grow == 1);
            assert(wx.rain_amt >= 18 && wx.rain_live > 0 && wx.rain_red == (kind == 3));
        } else assert(wx.snow_on == wx.n);
    }
    weather_set(0, 1000);
    weather_resume();
    assert(wx.idx == 0 && wx.left == 1000);
    // 光影：亮度表 14 格由夜到日、12 格白天、14 格由日到夜、8 格夜晚
    light_table(15, 0, 10, 0);
    assert(lt.table[0] == 0 && lt.table[7] == 7 && lt.table[14] == 15 && lt.table[25] == 15 && lt.table[33] == 8 && lt.table[40] == 0);
    // 時鐘：0~6 點每拍 120（10 拍半小時），7 點起每拍 1
    clk.hour = clk.tick = clk.half = 0;
    for (int k = 0; k < 10; k++) clock_tick();
    assert(clk.half == 1 && clk.tick == 0);
    clk.hour = 7, clk.half = 0;
    for (int k = 0; k < 1199; k++) clock_tick();
    assert(clk.half == 0 && clk.tick == 1199);
    // 場景時計：加速區間含端點；跨年固定每月31日、每年11個月，室內完全不走。
    const int clock_hours[] = {6, 7, 13, 14, 20, 21};
    const int clock_steps[] = {120, 1, 1, 120, 120, 1};
    for (int i = 0; i < 6; i++) {
        clk.hour = clock_hours[i], clk.tick = 0, clk.half = 0;
        clock_tick();
        assert(clk.tick == clock_steps[i] && clk.half == 0);
    }
    clk.year = 9, clk.month = 11, clk.day = 31;
    clk.hour = 23, clk.half = 1, clk.tick = 1199;
    clock_tick();
    assert(clk.year == 10 && clk.month == 1 && clk.day == 1);
    assert(clk.hour == 0 && clk.half == 0 && clk.tick == 0);
    lt.type = 12;
    clock_tick();
    assert(clk.tick == 0 && clk.hour == 0 && clk.day == 1);
    lt.type = 10;
    clk.hour = 10, clk.tick = 0;
    light_table(15, 0, 10, 0);
    assert(lt.bright == 15);
    clk.tick = 1;  // 不在整點（整點會重設要回到的亮度）
    // 大雨：每 30 拍暗一級，暗到 < 7 才下；閃電亮度 16，閃完回去
    weather_set(1, 5000);
    for (int k = 0; k < 400; k++) weather_tick();
    printf("big rain: bright %d phase %d\n", lt.bright, wx.phase);
    assert(lt.bright == 6 && wx.phase >= 1 && lt.mode == 0);
    for (int k = 0; k < 2000 && wx.flash < 1; k++) weather_tick();
    assert(wx.flash > 0 && lt.bright == 16);
    while (wx.flash > 0) weather_tick();
    assert(lt.bright == 6);
    weather_set(3, 5000);
    assert(lt.mode == 2);
    weather_set(0, 1000);
    // 光圈：中間一半是滿的，外圈降到 0
    lt.mw = lt.mh = 100;
    lt.map = calloc(100 * 100, 1);
    light_stamp(400, 15, 200, 200);
    assert(lt.map[50 * 100 + 50] == 15 && lt.map[50 * 100 + 60] == 15 && lt.map[50 * 100 + 90] < 15 && lt.map[50 * 100 + 99] == 0 && lt.map[0] == 0);
    free(lt.map), lt.map = NULL;
    // 人死了光圈每拍縮 10，縮到 60 以下拿掉
    Actor la;
    mk(&la);
    light_clear();
    light_add(&la, 0, 0, 1, 200, 0xffffff, -1);
    assert(light_of(&la) && light_of(&la)->cur == 15);
    la.act = ACT_DEAD;
    Scene ls = {.h = 1};
    light_tick(&ls);
    assert(light_of(&la)->dying);
    for (int k = 0; k < 20; k++) light_tick(&ls);
    assert(!light_of(&la));
    light_clear();
    unit_n = 0;
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
    // ---- 場景格子查詢（near.c，Poq.exe 0x442660～0x444740）----
    {
        // 螺旋：中心、上、左、下下、右右、上上；n = 1 剛好 3×3（邊長 3 那邊走到底那步回 0）
        Spiral sp;
        spiral_init(&sp, 10, 10, 1);
        static const int want[][2] = {{10,10},{10,9},{9,9},{9,10},{9,11},{10,11},{11,11},{11,10},{11,9}};
        int k = 0;
        while (spiral_next(&sp)) { assert(k < 9 && sp.x == want[k][0] && sp.y == want[k][1]); k++; }
        assert(k == 9);
        // 格子層：一般層 1 看能不能走；層 11 過 12 不過 14；層 15 只在 12/14/16；其他要同層；層 16 格子 13 不過
        Cell c = {.walk = 1, .layer = 12};
        assert(cell_pass(&c, 1) && cell_pass(&c, 11) && cell_pass(&c, 15) && !cell_pass(&c, 3));
        c.layer = 14; assert(!cell_pass(&c, 11) && cell_pass(&c, 13));
        c.layer = 16; assert(!cell_pass(&c, 13) && cell_pass(&c, 16));
        assert(obj_breakable(0x80) && obj_breakable(0x100) && !obj_breakable(0x96) && !obj_breakable(0x101));
        // 找角色：方框是 (x−r, y−r) 起邊長 3r（怪現象），所以右下 1.5r 的找得到、左上 1.5r 的找不到
        mk(&actors[0]); mk(&actors[1]); actor_count = 2;
        actors[0].side = 0; actors[1].side = 1; actors[1].grp = 10000;
        actors[0].x = actors[0].y = 10.5f;
        actors[1].x = actors[1].y = 11.9f;   // 761：超過 672 + 64（2r 的方框），在 672 + 128 之內
        assert(near_first(672, 672, 64) == &actors[0] && near_next() == &actors[1] && !near_next());
        actors[1].x = actors[1].y = 9.0f;    // 576 < 672 − 64
        assert(near_first(672, 672, 64) == &actors[0] && !near_next());
        assert(!near_other(672, 672, 64, 0));
        actors[1].x = actors[1].y = 11.2f;
        assert(near_other(672, 672, 64, 0) == &actors[1] && is_foe(&actors[0], &actors[1]) && !is_foe(&actors[0], &actors[0]));
        // AI 組 0x443560／0x450bc0／0x453210：玩家組對別組都敵對；兩個 NPC 組要在敵人名單裡；同組不敵對
        {
            Actor p = {.grp = 3}, x = {.grp = 10001}, y = {.grp = 20000}, z = {.grp = 9500};
            assert(is_foe(&p, &x) && is_foe(&y, &p) && !is_foe(&x, &y) && !is_foe(&z, &x));
            foe_grp(&x, &y);  // 10001 打 20000：20000 記 10001 為敵人
            assert(is_foe(&y, &x) && !is_foe(&x, &y) && !is_foe(&y, &(Actor){.grp = 30000}));  // 怪現象：只查 a 那組的名單，單向
            foe_grp(&z, &x); foe_grp(&p, &x);  // 9000～9999 組、玩家組不記
            assert(!grp_is_enemy(10001, 9500) && !grp_is_enemy(10001, 3));
            for (int k = 0; k < 205; k++) grp_enemy_add(40000, 100 + k);
            assert(grps[grp_find(40000)].n == 200);
            grp_enemy_add(40000, 100); assert(grps[grp_find(40000)].n == 200);
            x.grp = y.grp; assert(!is_foe(&x, &y));
        }
        // 直線：往右 6 格，第 14 格不能走 → 停在 13；mode 0 有角色的格子不算但不擋（mode 0 會擋）
        cells32[10 * 32 + 14].walk = 0;
        int x, y;
        line_end(&x, &y, 10, 10, 20, 10, 6, 0, 1, 1);
        assert(x == 13 && y == 10);
        assert(!line_clear(10, 10, 16, 10, 10, 1) && line_clear(10, 10, 13, 10, 10, 1) && line_clear(5, 5, 5, 5, 1, 0));
        actors[1].x = 12.5f, actors[1].y = 10.5f;
        line_end(&x, &y, 10, 10, 20, 10, 6, 0, 0, 1);
        assert(x == 11 && y == 10);   // mode 0：12 有人就停
        line_end(&x, &y, 10, 10, 20, 10, 6, 0, 1, 1);
        assert(x == 13);               // mode 1：12 有人不擋，但不當成終點
        line_end(&x, &y, 10, 10, 20, 10, 2, 1, 1, 1);
        assert(x == 11);               // 步數 2：走到 12（有人）不算
        // 八方向：四周都是牆只剩右邊（方向 7 = x+1）能走 → 選右邊、停在牆前
        for (int i = 0; i < 32 * 32; i++) cells32[i].walk = 0, cells32[i].layer = 2;
        for (int i = 3; i <= 8; i++) cells32[3 * 32 + i].walk = 1;
        open_dir(&x, &y, 3, 3, 10);
        assert(x == 8 && y == 3);
        cells32[3 * 32 + 4].walk = 0;
        open_dir(&x, &y, 3, 3, 10);
        assert(x == -1 && y == -1);
        for (int i = 0; i < 32 * 32; i++) cells32[i].walk = 1, cells32[i].layer = 0;
        // 掉落空格：中心有人 → 螺旋下一格（上）；那格有道具 → 再下一格（左）
        actors[0].x = actors[0].y = 5.5f; actors[1].x = actors[1].y = 20.5f;
        x = y = 5;
        assert(free_cell(&x, &y, 10) && x == 5 && y == 4);
        ground_n = 1; ground[0].x = 5.5f, ground[0].y = 4.5f;
        x = y = 5;
        assert(free_cell(&x, &y, 10) && x == 4 && y == 4);
        x = y = 5;
        assert(trap_cell(&x, &y, 10) && x == 5 && y == 5);   // 陷阱不管有沒有人
        ground_n = 0;
        spread_dir = 7; x = y = 0;
        spread_pt(&x, &y, 2);
        assert(spread_dir == 0 && x == 2 && y == -2);
        // 點選：兩個人都在方框裡，取離方框中心近的；side 只看那一邊
        actors[0].x = actors[0].y = 5.5f; actors[1].x = 6.5f; actors[1].y = 5.5f;
        pick_s = &test_scene; pick_ox = pick_oy = 0;
        int l, tp, w, h;
        pick_box(&actors[1], &l, &tp, &w, &h);
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 1, -1, pick_box) == &actors[1]);
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 1, 0, pick_box) != &actors[1]);
        actors[1].flags = 0x80;
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 0, 1, pick_box) == NULL);
        actors[1].flags = 0;
        assert(monster_at(l + w / 2, tp + h / 2, 0, 0, &test_scene) == &actors[1]);
    }
    // 遊戲速度 0x448a70：0~5 → 31~16
    assert(game_speed_ms == 21);
    game_speed_set(0); assert(game_speed_ms == 31);
    game_speed_set(5); assert(game_speed_ms == 16);
    game_speed_set(2); assert(game_speed_ms == 25);
    // 畫面縮放 0x44af30／0x44bf99／0x44c970：每拍 0.04，夾在 0~0.24；中心點不動
    vw = 640, vh = 480;
    zoom_add(0.04f); zoom_add(0.04f);
    zoom_tick(); assert(zoom == 0.04f);
    zoom_tick(); zoom_tick(); assert(zoom == zoom_to && zoom_keep == 0);
    for (int k = 0; k < 9; k++) zoom_add(0.04f);
    for (int k = 0; k < 9; k++) zoom_tick();
    assert(zoom == 0.24f);
    { int x = 320, y = 240; zoom_point(&x, &y); assert(abs(x - 320) <= 1 && abs(y - 240) <= 1);
      x = 0, y = 0; zoom_point(&x, &y); assert(x == 153 && y == 115); }
    zoom_back();
    for (int k = 0; k < 9; k++) zoom_tick();
    assert(zoom == zoom_keep);
    zoom_set(0); for (int k = 0; k < 9; k++) zoom_tick();
    assert(zoom == 0);
    // 對話面對面 0x44caf0
    actors[0].x = 5, actors[0].y = 5, actors[1].x = 7, actors[1].y = 5;
    actors[0].moving = 1;
    talk_face(&actors[0], &actors[1]);
    assert(!actors[0].moving && (actors[0].dir + 4) % 8 == actors[1].dir);
    // 滑鼠 CUserInput：點擊指令 0x454f00、游標 0x455d60、速度鍵、地面痕跡 0x459840
    {
        Actor h, c;
        mk(&h); mk(&c);
        h.grp = 0, c.grp = 10000;  // 玩家組對 NPC 組 = 敵對
        assert(click_cmd(&h, &c, -1) == CMD_ATTACK && input_cursor(&h, &c, -1, -1, -1) == CUR_ATTACK);
        c.grp = 0;  // 同組：對話；无对话（103）時游標是平常的
        assert(click_cmd(&h, &c, -1) == CMD_TALK && input_cursor(&h, &c, -1, -1, -1) == CUR_TALK);
        h.fight = 1;  // 战斗状态點誰都打
        assert(click_cmd(&h, &c, -1) == CMD_ATTACK && input_cursor(&h, &c, -1, -1, -1) == CUR_ATTACK);
        h.fight = 0;
        assert(buff_add(&h, NULL, 103, 1, 5, 0) && input_cursor(&h, &c, -1, -1, -1) == CUR_NORMAL);
        assert(click_cmd(&h, &h, 3) == CMD_PICK && input_cursor(&h, &h, 3, -1, -1) == CUR_ITEM);  // 點自己 = 看底下的道具
        assert(click_cmd(&h, NULL, -1) == CMD_MOVE);
        assert(buff_add(&h, NULL, 17, 1, 5, 0) && click_cmd(&h, &c, -1) == CMD_NONE);  // 溃散不收指令
        // 敵人狀態框 0x459ce0：有滑鼠時是指著的（自己也算），沒指著就關；觸控用攻擊目標
        h.foe = &c;
        assert(status_target(&h, &h, 1) == &h && !status_target(&h, NULL, 1) && status_target(&h, NULL, 0) == &c);
        c.act = ACT_DEAD; assert(!status_target(&h, &c, 1));
        game_speed_ms = 21;
        game_speed_key(1); assert(game_speed_ms == 16);  // 怪現象：開局按一次就最快
        game_speed_key(0); assert(game_speed_ms == 19);
        game_speed_ms = 31; game_speed_key(0); assert(game_speed_ms == 31);
        unit_n = 0;
        mark_new(3, 4); mark_new(5, 6);
        assert(units[1].x == 5 * 64 + 32 && units[1].spr.a && units[0].dead && !units[1].dead);
        unit_n = 0;
    }
    puts("skilltest ok");
    return 0;
}
