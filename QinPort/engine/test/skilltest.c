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

static void scene_client_test(void) {
    Actor saved = actors[0];
    int count = actor_count;
    Scene sc = {0};
    SceneClient c = {.scene = &sc, .ready = 1, .controlled = 0};
    actor_count = 1;
    actors[0] = (Actor){.role = (const Role *)1, .grp = 12345, .act = ACT_DEAD};
    // 已死不等於句柄不存在；查詢不以 alive() 過濾。
    assert(scene_client_actor(&c) == &actors[0]);
    assert(scene_client_group(&c, 12345));
    assert(!scene_client_group(&c, 0));
    c.ready = 0;
    assert(!scene_client_actor(&c) && !scene_client_group(&c, 12345));
    c.ready = 1; c.scene = NULL;
    assert(!scene_client_actor(&c));
    c.scene = &sc; c.controlled = -1;
    assert(!scene_client_actor(&c));
    c.controlled = 1;
    assert(!scene_client_actor(&c));
    c.controlled = 0; actors[0].role = NULL;
    assert(!scene_client_group(&c, 12345));
    actors[0] = saved; actor_count = count;
}

static void character_lookup_test(void) {
    Actor saved[2] = {actors[0], actors[1]};
    int count = actor_count;
    MonDef trap = {.istrap = 1};
    actor_count = 2;
    actors[0] = (Actor){.role = (const Role *)1, .sh = 123, .act = ACT_DEAD};
    actors[1] = (Actor){.role = (const Role *)1, .sh = 456, .mon = &trap};
    assert(character_of(123) == &actors[0]); // 死亡不影響查找
    assert(actor_of(456) == &actors[1] && !character_of(456));
    assert(!character_of(0) && !character_of(999));
    actors[0].role = NULL;
    assert(!character_of(123));
    actors[0] = saved[0]; actors[1] = saved[1]; actor_count = count;
}

static void character_relation_test(void) {
    Actor saved[2] = {actors[0], actors[1]};
    SceneClient saved_client = scene_client;
    Actor *saved_hover = hover_actor;
    int saved_mouse = hover_mouse, count = actor_count, groups = grp_n;
    void *saved_groups = malloc(sizeof grps); assert(saved_groups);
    memcpy(saved_groups, grps, sizeof grps);
    grp_n = 0;
    Scene sc = {0};
    RoleDef d = {.size = 1, .hit = 65535, .death = 65535};
    int frames[] = {1};
    uint8_t *order[] = {NULL};
    Role role = {.seq_count = 1, .frames = frames, .order = order};
    actor_count = 2;
    actors[0] = (Actor){.role = &role, .def = &d, .grp = 20000};
    actors[1] = (Actor){.role = &role, .def = &d, .grp = 20000, .flags = 0x4000};
    Actor *a = &actors[1];
    scene_client = (SceneClient){.scene = &sc, .ready = 1, .controlled = 0};
    hover_actor = NULL;
    // 同組普通、同組選取、中立、敵對，各自兩種尺寸。
    const uint32_t ids[4][2] = {
        {0x1002ef, 0x1002f0}, {0x100019, 0x100018},
        {0x100017, 0x100016}, {0x100013, 0x100012}
    };
    for (int kind = 0; kind < 4; kind++) {
        a->grp = kind < 2 ? 20000 : kind == 2 ? 70000 : 0;
        hover_actor = kind == 0 ? NULL : a;
        for (int size = 0; size < 2; size++) {
            d.size = size + 1;
            assert(actor_relation_anim(a) == ids[kind][size]);
        }
    }
    // 實際使用 a→操控角色方向的敵人名單；不可對調參數或改用side。
    a->grp = 60000; actors[0].grp = 10000;
    grp_enemy_add(10000, 60000);
    assert(actor_relation_anim(a) == 0x100016);
    grp_enemy_add(60000, 10000);
    assert(actor_relation_anim(a) == 0x100012);
    // 小型的分界包含0與負值；死亡角色仍能取圖，沒有操控角色才回0。
    d.size = -1; a->act = ACT_DEAD;
    assert(actor_relation_anim(a) == 0x100013);
    scene_client.ready = 0; assert(!actor_relation_anim(a));
    scene_client.ready = 1;
    scene_client.controlled = -1; assert(!actor_relation_anim(a));
    scene_client.controlled = 0;
    a->grp = actors[0].grp; a->act = ACT_NONE; a->dir = 2; d.size = 1;
    actor_hover_set(a, 1);
    assert(hover_actor == a && hover_mouse);
    assert((a->flags & 0x4100) == 0x4100 && (actors[0].flags & 0x100));
    actor_hover_set(NULL, 0);
    assert(!hover_actor && !hover_mouse);
    assert(a->flags == 0x4000 && (actors[0].flags & 0x100));
    // 軟體renderer走真正actor_draw及可見矩形，確保腳圈不是只有測試呼叫的選圖。
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf); ren = SDL_CreateSoftwareRenderer(surf); assert(ren);
    for (int kind = 0; kind < 4; kind++) {
        a->grp = kind < 2 ? actors[0].grp : kind == 2 ? 70000 : 0;
        hover_actor = kind == 0 ? NULL : a;
        a->flags |= 0x100;
        for (int size = 0; size < 2; size++) {
            d.size = size + 1;
            const Anim *ring = anim_get(ids[kind][size]);
            assert(ring && ring->count && ring->seqs[0].count);
            const Frame *f = &ring->seqs[0].frames[0];
            const Sprite *sp = image(f->ref); assert(sp);
            SDL_Rect rect = viewport_actor_rect(a, 128, 128, 0);
            assert(rect.x == 128-f->dx && rect.y == 128-f->dy);
            assert(rect.w == sp->w && rect.h == sp->h);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
            actor_draw(a, 128, 128, 0); SDL_RenderPresent(ren);
            int ink = 0;
            for (int y = 0; y < surf->h; y++) for (int x = 0; x < surf->w; x++) {
                Uint8 r, g, b, alpha;
                SDL_GetRGBA(((Uint32 *)surf->pixels)[y*(surf->pitch/4)+x], surf->format, &r, &g, &b, &alpha);
                ink += !!(r || g || b);
            }
            assert(ink);
            a->flags &= ~0x100;
            rect = viewport_actor_rect(a, 128, 128, 0);
            assert(!rect.w && !rect.h);
            a->flags |= 0x100;
        }
    }
    cache_clear(); SDL_DestroyRenderer(ren); ren = NULL; SDL_FreeSurface(surf);
    actors[0] = saved[0]; actors[1] = saved[1]; actor_count = count;
    scene_client = saved_client; hover_actor = saved_hover; hover_mouse = saved_mouse; grp_n = groups;
    memcpy(grps, saved_groups, sizeof grps); free(saved_groups);
}

static void character_scale_test(void) {
    SDL_Rect r = {10, 20, 7, 9};
    role_rect_scale(&r, 20, 0);
    assert(r.x == 7 && r.y == 12 && r.w == 13 && r.h == 17);
    r = (SDL_Rect){10, 20, 7, 9}; role_rect_scale(&r, -10, 0);
    assert(r.x == 11 && r.y == 24 && r.w == 5 && r.h == 5);
    r = (SDL_Rect){10, 20, 7, 9}; role_rect_scale(&r, 20, 1);
    assert(r.x == 7 && r.y == 16 && r.w == 13 && r.h == 17);
    r = (SDL_Rect){10, 20, 7, 9}; role_rect_scale(&r, 1, 0);
    assert(r.x == 10 && r.y == 20 && r.w == 7 && r.h == 9);

    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf); ren = SDL_CreateSoftwareRenderer(surf); assert(ren);
    const uint32_t refs[] = {0xf1000001, 0xf1000002, 0xf1000003};
    const int widths[] = {6, 4, 10}, heights[] = {8, 6, 2};
    for (int i = 0; i < 3; i++) {
        SDL_Surface *pixels = SDL_CreateRGBSurfaceWithFormat(0, widths[i], heights[i], 32, SDL_PIXELFORMAT_RGBA32);
        assert(pixels);
        SDL_FillRect(pixels, NULL, SDL_MapRGBA(pixels->format, i == 0 ? 255 : 0, i == 1 ? 255 : 0, 0, 255));
        Sprite *sp = slot_find(refs[i]);
        sp->w = widths[i]; sp->h = heights[i];
        sp->tex = SDL_CreateTextureFromSurface(ren, pixels); assert(sp->tex);
        SDL_FreeSurface(pixels);
    }
    Frame frames[3] = {{.ref = refs[0], .dx = 3, .dy = 8, .delay = 1},
                       {.ref = refs[1], .dx = 1, .dy = 6, .delay = 1},
                       {.ref = refs[2], .dx = 5, .dy = 1, .delay = 1}};
    Seq seqs[3] = {{.count = 1, .frames = frames}, {.count = 1, .frames = frames+1}, {.count = 1, .frames = frames+2}};
    Anim anim[3] = {{.count = 1, .seqs = seqs}, {.count = 1, .seqs = seqs+1}, {.count = 1, .seqs = seqs+2}};
    int count[] = {1}; uint8_t parts[] = {0, 1}; uint8_t *order[] = {parts};
    Role role = {.part_count = 2, .seq_count = 1, .body = {anim, anim+1},
                 .shadow = {anim+2}, .frames = count, .order = order};
    RoleDef d = {.scale = 65535, .hit = 65535, .death = 65535};
    Actor a = {.role = &role, .def = &d, .dir = 2, .scale = 20, .z = 3};
    r = role_rect(&role, 0, 0, 100, 100, 0);
    assert(r.x == 97 && r.y == 92 && r.w == 6 && r.h == 8);
    // 原版只上移角色矩形的y，不增高；加影子後才縮放，與分別縮放兩份不同。
    r = viewport_actor_rect(&a, 100, 100, 0);
    assert(r.x == 90 && r.y == 77 && r.w == 20 && r.h == 24);
    const Scene *saved_pick_s = pick_s;
    int saved_pick_ox = pick_ox, saved_pick_oy = pick_oy;
    Scene sc = {.w = 10, .h = 10};
    pick_s = &sc; pick_ox = 220; pick_oy = -100;
    int left, top, width, height;
    pick_box(&a, &left, &top, &width, &height);
    assert(left == 90 && top == 77 && width == 20 && height == 24);
    a.view_link_type = 2;
    pick_box(&a, &left, &top, &width, &height);
    assert(left == 90 && top == 141 && width == 20 && height == 24);
    a.view_link_type = 0;
    pick_s = saved_pick_s; pick_ox = saved_pick_ox; pick_oy = saved_pick_oy;
    a.flags = 0; a.z = 0;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
    actor_draw(&a, 100, 100, 0); SDL_RenderPresent(ren);
    for (int y = 84; y < 100; y++) for (int x = 94; x < 106; x++) {
        Uint8 red, green, blue, alpha;
        SDL_GetRGBA(((Uint32 *)surf->pixels)[y * (surf->pitch / 4) + x], surf->format, &red, &green, &blue, &alpha);
        int top = x >= 98 && y >= 88; // 第二部件必須覆蓋第一部件。
        assert(red == (top ? 0 : 255) && green == (top ? 255 : 0) && blue == 0);
    }
    // 已縮小至0／負尺寸時不畫；不能把負寬高交給SDL當成整個表面。
    a.scale = -20;
    SDL_SetRenderDrawColor(ren, 7, 9, 11, 255); SDL_RenderClear(ren);
    actor_draw(&a, 100, 100, 0); SDL_RenderPresent(ren);
    Uint8 red, green, blue, alpha;
    SDL_GetRGBA(((Uint32 *)surf->pixels)[92 * (surf->pitch / 4) + 97], surf->format, &red, &green, &blue, &alpha);
    assert(red == 7 && green == 9 && blue == 11);
    // 合成時不得沿用主畫面clip或scale，也不能污染之後的物件。
    a.scale = 10;
    SDL_Rect view = {2, 3, 200, 190}, clip = {4, 5, 170, 160}, actual;
    SDL_RenderSetScale(ren, 1.25f, 1.25f);
    SDL_RenderSetViewport(ren, &view); SDL_RenderSetClipRect(ren, &clip);
    SDL_SetRenderDrawColor(ren, 21, 22, 23, 24);
    actor_draw(&a, 100, 100, 0);
    SDL_RenderGetViewport(ren, &actual); assert(!memcmp(&view, &actual, sizeof view));
    SDL_RenderGetClipRect(ren, &actual); assert(!memcmp(&clip, &actual, sizeof clip));
    float sx, sy; SDL_RenderGetScale(ren, &sx, &sy); assert(sx == 1.25f && sy == 1.25f);
    SDL_GetRenderDrawColor(ren, &red, &green, &blue, &alpha);
    assert(red == 21 && green == 22 && blue == 23 && alpha == 24);
    assert(SDL_GetRenderTarget(ren) == NULL);

    a.scale = 0; a.scale_target = 2;
    actor_scale_tick(&a); assert(a.scale == 1);
    actor_scale_tick(&a); actor_scale_tick(&a); assert(a.scale == 2);
    a.scale_target = -1; actor_scale_tick(&a); assert(a.scale == 1);
    d.scale = 0; actor_scale_tick(&a); assert(a.scale == 1); // 模板0也算指定比例。
    d.scale = 65535; a.flags = 0x200; a.dynamic_state = 11;
    actor_scale_tick(&a); assert(a.scale == 1);
    a.flags |= 0x400; actor_scale_tick(&a); assert(a.scale == 0);
    cache_clear(); SDL_DestroyRenderer(ren); ren = NULL; SDL_FreeSurface(surf);
}

static void character_weapon_type_test(void) {
    Actor a = {.flags = 0x10400, .selected = 3, .right_id = 27, .left_id = 42,
               .act = ACT_ATTACK, .act_frame = 7, .weapon_type = 9};
    a.command[0] = 8;
    a.remembered[0] = 2;
    Actor expected = a;
    const int values[] = {0, 1, 255, 256, 257, -1, -256, -257};
    for (unsigned i = 0; i < sizeof values / sizeof *values; i++) {
        actor_weapon_type_set(&a, values[i]);
        expected.weapon_type = (unsigned char)values[i];
        assert(!memcmp(&a, &expected, sizeof a));
    }
    actor_attribute_changed(&a, "W_Type", 258);
    expected.weapon_type = 2;
    assert(!memcmp(&a, &expected, sizeof a));
    actor_attribute_changed(&a, "W_TYPE", 10);
    actor_attribute_changed(&a, "W_Type_extra", 10);
    // 原八鍵入口不擴充；單機通知層才把武器種類交給獨立setter。
    actor_property(&a, "W_Type", 10);
    assert(!memcmp(&a, &expected, sizeof a));
    actor_attribute_changed(&a, "B_C_MOVE_SPEED", 150);
    expected.ex[64] = 150;
    assert(!memcmp(&a, &expected, sizeof a));
}

static void character_core_test(void) {
    RoleDef d = {.seek = 7, .chase = 15};
    Actor target = {0};
    Actor a = {.def = &d, .grp = 10000, .foe = &target,
               .command = {9, 123, 456, 789}, .layer = 10,
               .act = ACT_ATTACK, .moving = 1, .flags = 0x4010};
    // 同組不能取消目標或排隊命令；換組只取消命令代號，參數保留。
    actor_group(&a, 10000);
    assert(a.foe == &target && a.command[0] == 9);
    actor_group(&a, 60000);
    assert(a.grp == 60000 && !a.foe && a.command[0] == -1);
    assert(a.command[1] == 123 && a.command[2] == 456 && a.command[3] == 789);
    assert(a.act == ACT_ATTACK && a.moving && a.flags == 0x4010);
    // 取消目標與取消命令是兩個入口，不能互相擴大效果。
    a.foe = &target; a.command[0] = 8;
    actor_clear_target(&a);
    assert(!a.foe && a.command[0] == 8);
    a.foe = &target;
    actor_cancel_command(&a);
    assert(a.foe == &target && a.command[0] == -1);
    assert(actor_flags(&a, 0x4000) == 0x4000); // 遮罩值，不是1。
    actor_set_flags(&a, 0x80000200u, 7);
    assert(actor_flags(&a, 0x80000200u) == 0x80000200u);
    actor_clear_flags(&a, 0x201u);
    assert(actor_flags(&a, 0x80004210u) == 0x80004010u);
    assert(actor_seek(&a) == 7 && actor_chase(&a) == 15);
    a.layer = 11;
    assert(actor_seek(&a) == 15 && actor_chase(&a) == 23);
    a.layer = 255;
    d.seek = -8; d.chase = 0;
    assert(actor_seek(&a) == 0 && actor_chase(&a) == 8); // 原版不夾成正數。
    a.layer = 0;
    assert(actor_seek(&a) == -8 && actor_chase(&a) == 0);
}

static void character_skills_test(void) {
    RoleDef d = {.speed = 5};
    Actor target = {.layer = 1};
    Actor a = {.def = &d, .speed = 5, .layer = 1, .selected = -1,
        .template_skill_n = 3, .template_skill = {10, 20, 30},
        .skill_n = 3, .skills = {{.key = 7, .id = 10, .max = 2, .enabled = 1},
            {.key = 7, .id = 20, .max = 3, .enabled = 7},
            {.key = 9, .id = 30, .max = 4, .enabled = 0}}};
    assert(actor_skill_find(&a, 30) == 2); // 精確命中不過濾Enable。
    assert(actor_skill_find(&a, -1) == 0);
    a.range_adjust = -10; a.layer = 14;
    assert(actor_skill_range(&a, &a.skills[1]) == 3);
    assert(actor_skill_range(&a, &a.skills[2]) == -3); // 不夾範圍，按原始4先加修正再加層。
    a.range_adjust = 2;
    assert(actor_skill_range(&a, &a.skills[2]) == 9);
    a.layer = 16; assert(actor_skill_range(&a, &a.skills[2]) == 10);
    a.layer = 255; assert(actor_skill_range(&a, &a.skills[2]) == 8);
    a.speed = 0;
    assert(actor_skill_enabled(&a, &a.skills[0]) == 1);
    assert(actor_skill_enabled(&a, &a.skills[1]) == 0); // 3是Enable速度限制邊界。
    d.speed = 0;
    assert(actor_skill_enabled(&a, &a.skills[1]) == 7); // 非布林Enable。
    d.speed = 5; a.speed = 1;
    assert(actor_skill_enabled(&a, &a.skills[1]) == 7);
    a.foe = &target; a.layer = 11;
    actor_right_skill(&a, 10);
    assert(a.right_id == 10 && a.selected == -1);
    actor_right_skill(&a, 20); assert(a.selected == 1);
    a.layer = 1; target.layer = 15;
    actor_right_skill(&a, 20); assert(a.selected == -1);
    target.layer = 11;
    actor_right_skill(&a, 20); assert(a.selected == 1); // 高一級的遠程可以。
    actor_left_skill(&a, 30); assert(a.left_id == 30 && a.selected == 1);
    actor_skill_enable(&a, 7, 5, 3);
    assert(a.skills[0].enabled == 5 && a.skills[1].enabled == 7);
    assert(a.right_id == 10 && a.left_id == 10 && a.selected == -1);
    a.foe = NULL;
    actor_right_skill(&a, 20);
    actor_left_skill(&a, 20);
    actor_skill_update(&a, 7, 40, 8, 9, 6, 7); // 第一個key=7，其ID10未選中。
    assert(a.right_id == 20 && a.left_id == 20);
    assert(a.skills[0].id == 40 && a.skills[0].min == 8 && a.skills[0].p7 == 7);
    actor_skill_remove(&a, 7);
    assert(a.skill_n == 2 && a.skills[0].id == 20 && a.selected == 0);
    a.foe = &target;
    actor_skill_remove(&a, 7);
    assert(a.skill_n == 1 && a.selected == -1 && a.foe == &target);
    assert(a.right_id == 20); // 刪除不改保存的手ID。
    actor_skill_remove(&a, 999); assert(a.skill_n == 1);
    a.skills[0].enabled = 0;
    assert(actor_skill_find(&a, -1) == -1);
    // 模板順序與鏈順序不同：fallback必須遵守模板優先序。
    a.skill_n = 2; a.skills[0] = (ActorSkill){.id = 20, .enabled = 2};
    a.skills[1] = (ActorSkill){.id = 10, .enabled = 3};
    assert(actor_skill_find(&a, -1) == 1);
    a.skills[1].enabled = 0; assert(actor_skill_find(&a, -1) == 0);
    a.template_skill_n = 0; assert(actor_skill_find(&a, -1) == -1);
    assert(actor_skill_find(&a, 10) == 1); // 不在模板也能精確命中停用項。
    a.foe = NULL; a.layer = 1;
    int added = actor_skill_add(&a, (ActorSkill){.key = 42, .id = 99,
        .min = -2, .max = 7, .p6 = 5, .p7 = 6, .p8 = 7, .p9 = 8, .enabled = 0}, 7);
    assert(added == 2 && a.skill_n == 3 && a.selected == 2);
    assert(a.right_id == 99 && a.left_id == 99);
    assert(a.skills[2].min == -2 && a.skills[2].p6 == 5 && a.skills[2].p9 == 8);
    // 重複ID允許插入，但選手仍找第一個；高位hand旗標不切手。
    assert(actor_skill_add(&a, (ActorSkill){.id = 99, .enabled = 1}, 1) == 3);
    assert(a.selected == 2);
    actor_skill_add(&a, (ActorSkill){.id = 100}, 4);
    assert(a.right_id == 99 && a.left_id == 99);
    int saved_n = unit_n;
    MUnit saved_u = units[0];
    unit_n = 0; a.speed = 0; assert(!actor_can_move(&a));
    a.speed = -1; assert(actor_can_move(&a));
    units[0] = (MUnit){.cls = U_MAIN, .owner = &a}; unit_n = 1;
    assert(!actor_can_move(&a));
    units[0].dead = 1; assert(actor_can_move(&a));
    units[0] = saved_u; unit_n = saved_n;
    for (int i = -1; i <= 1; i++) { a.cool = i; assert(actor_cooling(&a) == (i > 0)); }
}

static void character_move_cast_test(void) {
    const Scene *saved_scene = unit_scene;
    int saved_count = actor_count, saved_units = unit_n;
    Actor saved[2] = {actors[0], actors[1]};
    MUnit old_units[3] = {units[0], units[1], units[2]};
    Cell cells[20 * 20] = {0};
    Scene sc = {.w = 20, .h = 20, .cells = cells};
    unit_scene = &sc; actor_count = unit_n = 0;
    // 空格搜尋的四邊順序、層、占位與六圈的最後一圈。
    int x = 4, y = 4;
    cells[3 * 20 + 3].walk = cells[5 * 20 + 3].walk = 1;
    assert(vacant_cell(&x, &y, 1, 1) && x == 3 && y == 3);
    cells[3 * 20 + 3].walk = 0; x = y = 4;
    assert(vacant_cell(&x, &y, 1, 1) && x == 3 && y == 5);
    memset(cells, 0, sizeof cells); x = y = 4;
    cells[4 * 20 + 4] = (Cell){.layer = 16};
    assert(vacant_cell(&x, &y, 15, 0) && x == 4 && y == 4);
    assert(!vacant_cell(&x, &y, 1, 0));
    cells[4 * 20 + 4].walk = 1;
    actors[0] = (Actor){.role = (const Role *)1, .x = 4.5f, .y = 4.5f}; actor_count = 1;
    assert(!vacant_cell(&x, &y, 1, 0));
    actor_count = 0; cells[4 * 20 + 4].walk = 0;
    cells[10 * 20 + 4].walk = 1;
    assert(!vacant_cell(&x, &y, 1, 5) && x == 4 && y == 4);
    assert(vacant_cell(&x, &y, 1, 6) && x == 4 && y == 10);
    memset(cells, 0, sizeof cells); cells[0].walk = 1; x = y = -2;
    assert(vacant_cell(&x, &y, 1, 0) && x == 0 && y == 0);
    // 真實模板欄：移動技及戰鬥狀態技（星號多值）。
    assert(roledef_find(346)->move_cast == 1);
    assert(roledef_find(82)->candidate[0] == 276 && roledef_find(82)->candidate[1] == 277);
    assert(roledef_find(82)->candidate[2] == -1);
    int id = -1;
    for (int k = 1; k < 65535; k++) {
        const Method *m = method_find(k);
        if (m && m->manual == 3) { id = k; break; }
    }
    assert(id > 0);
    Role role = {0};
    RoleDef d = {.speed = 5, .move_cast = 1};
    Actor a = {.def = &d, .role = &role, .x = 1.5f, .y = 1.5f, .grp = 0, .speed = 5,
        .layer = 1, .skill_n = 1, .skills = {{.id = id, .enabled = 1, .max = 7}},
        .atk = method_find(1)};
    memset(cells, 0, sizeof cells); cells[4 * 20 + 8].walk = 1;
    assert(actor_move_cast(&a, 8, 4) && unit_n == 1 && a.act == ACT_ATTACK);
    assert(a.aim_x == 8 * 64 + 32 && a.aim_y == 4 * 64 + 32);
    assert(units[0].target == NULL && units[0].range == 7);
    // 地點施法不受舊索敵對象影響，MethodMain必須朝新地點。
    Actor stale = {.role = &role, .x = 1.5f, .y = 12.5f};
    unit_n = 0; a.act = 0; a.casting = 0; a.foe = &stale;
    assert(actor_move_cast(&a, 8, 4) && units[0].target == NULL);
    assert(units[0].tx > units[0].x && a.aim_x == 8 * 64 + 32);
    // NPC組邊界含60000，60001及9999可使用；思想不是1不使用。
    unit_n = 0; a.act = 0; a.casting = 0;
    a.grp = 10000; assert(!actor_move_cast(&a, 8, 4));
    a.grp = 60000; assert(!actor_move_cast(&a, 8, 4));
    a.grp = 60001; a.flags = 0x800;
    assert(actor_move_cast(&a, 8, 4) && unit_n == 0); // 有技能但不可出手仍吞命令。
    a.grp = 9999; assert(actor_move_cast(&a, 8, 4));
    d.move_cast = 2; assert(!actor_move_cast(&a, 8, 4)); d.move_cast = 1;
    a.flags = 0; a.speed = 0; assert(!actor_move_cast(&a, 8, 4)); a.speed = 5;
    a.skills[0].enabled = 0; assert(!actor_move_cast(&a, 8, 4)); a.skills[0].enabled = 1;
    // 距離取整：sqrt(8)=2，不施放；sqrt(10)=3，施放。
    memset(cells, 0, sizeof cells); cells[3 * 20 + 3].walk = 1;
    assert(!actor_move_cast(&a, 3, 3));
    cells[3 * 20 + 3].walk = 0; cells[2 * 20 + 4].walk = 1;
    assert(actor_move_cast(&a, 4, 2) && unit_n == 1);
    // 候選命中停用節點即停止，不改試後面的；不因冷卻而拒絕觸發。
    unit_n = 0; a.act = 0; a.casting = 0; a.cool = 99;
    a.skill_n = 2; a.skills[0] = (ActorSkill){.id = id, .enabled = 0, .max = 7};
    a.skills[1] = (ActorSkill){.id = 1, .enabled = 1, .max = 1};
    a.candidate[0] = 0; a.candidate[1] = id; a.candidate[2] = 1; a.candidate[3] = -1;
    actor_candidate_cast(&a); assert(unit_n == 0);
    a.skills[0].enabled = 1;
    Actor target = {.role = &role, .x = 7.5f, .y = 8.5f};
    Actor saved_candidate_target = actors[0]; int saved_candidate_count = actor_count;
    actors[0] = target; actor_count = 1;
    a.foe = &actors[0];
    actor_enter_fight(&a);
    assert(unit_n == 1 && units[0].target == &actors[0] && a.aim_x == ax64(&actors[0]));
    actor_enter_fight(&a); assert(unit_n == 1); // 同一次入戰只觸發一次。
    unit_n = 0; a.act = 0; a.casting = 0; a.foe = NULL;
    actor_candidate_cast(&a);
    assert(unit_n == 1 && a.aim_x == ax64(&a) && a.aim_y == ay64(&a));
    actors[0] = saved_candidate_target; actor_count = saved_candidate_count;
    // 右鍵共用地點施法；同隊技不接受敵對組，也不能把點到的友軍改成自己。
    SkillRow old_skill = hero_skill[0]; int old_n = hero_skill_n, old_left = cur_skill[1];
    hero_skill_n = 1; cur_skill[1] = 0;
    hero_skill[0] = (SkillRow){.level = 1, .wtype = -1, .mod_n = 1, .mod = {{.id = 10, .init = id}}};
    a.act = a.casting = 0; unit_n = 0;
    a.skills[0].row = &hero_skill[0];
    hero_cast_point(&a, 10 * 64 + 32, 2 * 64 + 32, NULL);
    assert(unit_n == 1 && !units[0].target && a.aim_x == 10 * 64 + 32);
    hero_skill[0] = old_skill; hero_skill_n = old_n; cur_skill[1] = old_left;
    int ally_id = -1;
    for (int k = 1; k < 65535; k++) {
        const Method *m = method_find(k);
        if (m && m->manual == 2) { ally_id = k; break; }
    }
    assert(ally_id > 0);
    unit_n = 0; a.act = a.casting = 0;
    a.skills[0] = (ActorSkill){.id = ally_id, .enabled = 1, .max = 7};
    target.grp = a.grp + 1;
    assert(!actor_cast_at(&a, ally_id, ax64(&target), ay64(&target), &target));
    target.grp = a.grp;
    assert(actor_cast_at(&a, ally_id, ax64(&target), ay64(&target), &target));
    assert(units[0].target == &target);
    units[0] = old_units[0]; units[1] = old_units[1]; units[2] = old_units[2];
    actors[0] = saved[0]; actors[1] = saved[1];
    unit_scene = saved_scene; actor_count = saved_count; unit_n = saved_units;
}

static void character_names_owner_test(void) {
    NpcSpec spec = {.type = 10};
    Actor a = {.spec = &spec, .grp = 12345, .selected = 7};
    Actor owner = {.grp = 456};
    // SInit的事件ID與顯示字串是兩份資料；改事件ID不能改顯示副本。
    snprintf(a.display_name, sizeof a.display_name, "%s", "display");
    for (int type = 9; type <= 17; type++) {
        spec.type = type;
        snprintf(a.init_name, sizeof a.init_name, "%s", "old");
        actor_name_set(&a, "EventID");
        if (type >= 10 && type <= 16) {
            assert(!strcmp(actor_init_name(&a), "EventID"));
        } else {
            assert(!actor_init_name(&a) && !strcmp(a.init_name, "old"));
        }
        assert(!strcmp(actor_name(&a), "display"));
    }
    spec.type = -1;
    assert(!actor_init_name(&a));
    spec.type = 12;
    char event[64];
    memset(event, 'A', sizeof event - 1); event[63] = 0;
    actor_name_set(&a, event);
    assert(strlen(actor_init_name(&a)) == 63 && !strcmp(actor_init_name(&a), event));
    actor_name_set(&a, ""); assert(!*actor_init_name(&a));
    // 動態建立角色用最小SInitCha（種類10），不需要ONT指標。
    a.spec = NULL;
    actor_name_set(&a, "dynamic"); assert(!strcmp(actor_init_name(&a), "dynamic"));
    actor_owner_set(&a, &owner);
    assert(actor_owner(&a) == &owner && a.grp == 12345 && a.selected == 7);
    actor_owner_set(&a, NULL);
    assert(!actor_owner(&a) && a.grp == 12345 && a.selected == 7);
    // HUD從顯示來源更新，不得把怪物顯示名稱寫入事件ID。
    MonDef mon = {0};
    snprintf(mon.title, sizeof mon.title, "%s", "Monster");
    a.mon = &mon;
    char title[256];
    actor_title(&a, title, sizeof title);
    assert(!strcmp(title, "Monster") && !strcmp(actor_name(&a), "Monster"));
    assert(!strcmp(actor_init_name(&a), "dynamic"));
    // 編號是獨立欄位；正數只在客戶端加半形括號，0與負數皆保留基底名稱。
    const int numbers[] = {-1, -123, 0, 1, 255, 2147483647};
    for (unsigned i = 0; i < sizeof numbers / sizeof *numbers; i++) {
        int number = numbers[i];
        actor_display_name_set(&a, "Monster", number, 0);
        assert(actor_name_number(&a) == number && !strcmp(actor_name(&a), "Monster"));
        actor_display_name_set(&a, "Monster", number, 1);
        char expected[256];
        if (number > 0) snprintf(expected, sizeof expected, "Monster(%d)", number);
        else snprintf(expected, sizeof expected, "Monster");
        assert(!strcmp(actor_name(&a), expected));
        assert(!strcmp(actor_init_name(&a), "dynamic"));
    }
    actor_display_name_set(&a, "", 2, 1);
    assert(!strcmp(actor_name(&a), "(2)"));
    // HUD持續刷新必須保留編號，不能累加後綴；來源改名仍重新套用同一編號。
    actor_display_name_set(&a, "old", 7, 1);
    actor_title(&a, title, sizeof title);
    actor_title(&a, title, sizeof title);
    assert(!strcmp(title, "Monster(7)"));
    snprintf(mon.title, sizeof mon.title, "%s", "Renamed");
    actor_title(&a, title, sizeof title);
    assert(!strcmp(title, "Renamed(7)") && actor_name_number(&a) == 7);
    actor_display_name_set(&a, "Renamed", 0, 1);
    actor_title(&a, title, sizeof title);
    assert(!strcmp(title, "Renamed"));
}

static void character_near_test(void) {
    RoleDef d = {.seek = 2};
    for (int i = 0; i < 4; i++) {
        mk(&actors[i]); actors[i].def = &d;
        actors[i].x = actors[i].y = 10.5f;
        actors[i].grp = i ? 10000 : 0;
    }
    actor_count = 3;
    Actor *a = &actors[0], *first = &actors[1], *closer = &actors[2];
    first->y = 9.1f; closer->x = 11.1f;
    // 螺旋先上方再右方，選第一個，不選距離較近者。
    assert(actor_near_begin(a, 0, -1) == first);
    assert(actor_near_next(a) == closer && !actor_near_next(a));
    first->flags = 0x10;
    assert(actor_near_begin(a, 0, -1) == closer);
    closer->flags = 0x20000;
    assert(!actor_near_begin(a, 0, -1));
    first->flags = closer->flags = 0;
    first->display_override = 50;
    assert(actor_near_begin(a, 0, -1) == closer);
    first->display_override = 0;
    RoleDef hidden = d;
    hidden.display = 50;
    first->def = &hidden;
    assert(actor_near_begin(a, 0, -1) == closer);
    first->display_override = -1; // 非零覆寫，不能把負值當成未設定。
    assert(actor_near_begin(a, 0, -1) == first);
    first->display_override = 0; first->def = &d;
    a->flags = 0x4000; assert(!actor_near_begin(a, 0, -1));
    a->flags = 0; a->buff_n = 1; a->buff[0].status = 102;
    assert(!actor_near_begin(a, 0, -1)); a->buff_n = 0;
    // 只有-1用預設範圍；零及其他負值都失敗，而且不重設底層游標。
    assert(actor_near_begin(a, 0, 2) == first);
    assert(!actor_near_begin(a, 0, 0));
    assert(actor_near_next(a) == closer);
    assert(!actor_near_begin(a, 0, -2));
    first->y = 5.5f; closer->x = 15.5f;
    assert(!actor_near_begin(a, 0, -1));
    a->layer = 11;
    assert(actor_near_begin(a, 0, -1));
    assert(!actor_near_begin(a, 0, 2)); a->layer = 1;
    first->x = first->y = closer->x = closer->y = 10.5f;
    first->grp = a->grp; first->sh = 71;
    assert(actor_near_begin(a, 1, 2) == first);
    actor_search_link_set(a, 71); assert(actor_near_begin(a, 2, 2) == first);
    // setter不預先驗證識別；模式2查找才決定是否有相符角色。
    // 不能截成byte或short，亦不能改目前目標／排隊命令。
    first->sh = 0x12345678;
    a->foe = closer; a->command[0] = 9; a->command[1] = 71;
    actor_search_link_set(a, 0x12345678);
    assert(actor_near_begin(a, 2, 2) == first);
    assert(a->foe == closer && a->command[0] == 9 && a->command[1] == 71);
    actor_search_link_set(a, -1); assert(!actor_near_begin(a, 2, 2));
    actor_search_link_set(a, 0); assert(actor_near_begin(a, 2, 2) == closer);
    first->sh = 71;
    actor_search_link_set(a, 71);
    a->foe = NULL;
    assert(actor_near_begin(a, 3, 2) == closer);
    closer->grp = 60001; assert(!actor_near_begin(a, 3, 2));
    closer->grp = 60000; assert(actor_near_begin(a, 3, 2) == closer);
    // 同組戰鬥角色可轉交其目標；以整數格歐氏距離嚴格小於索敵範圍。
    actor_count = 4; Actor *foe = &actors[3];
    foe->x = 11.5f; foe->y = 10.5f; foe->grp = 10000;
    first->fight = 1; first->foe = foe;
    assert(actor_near_begin(a, 0, 2) == foe);
    foe->x = 12.5f;
    assert(actor_near_begin(a, 0, 2) == closer);
    first->fight = 0;
    assert(actor_near_begin(a, 0, 2) == closer);
    // 程序40可在不同格協助；一般程序只在整數格距離0時協助。
    first->fight = 1; first->y = 9.5f; foe->x = 11.5f;
    closer->flags = foe->flags = 0x10;
    assert(!actor_near_begin(a, 0, 2));
    d.program = 40;
    assert(!actor_near_begin(a, 0, 2)); // 目標getter的0x10也會阻止同組轉交。
    assert(first->foe == foe); // 僅隱藏查詢，不丟失識別。
    foe->flags = 0x20000; // 轉交目標不重套候選篩選；getter只排除0x10。
    assert(actor_near_begin(a, 0, 2) == foe);
    d.program = 0; closer->flags = foe->flags = 0;
    a->flags = 0x4000;
    assert(actor_near_begin(a, 1, 2) == first); // 自身禁止攻擊不阻擋模式1。
    a->flags = 0;
    assert(actor_near_begin(a, 0, 2) == closer);
    assert(!actor_near_begin(a, 1, 0)); // 半徑失敗仍先更新模式。
    assert(actor_near_next(a) == first);
    actor_count = 0;
}

static void character_target_display_test(void) {
    Actor saved = actors[0];
    int saved_count = actor_count;
    RoleDef d = {.display = 50};
    Actor a = {.def = &d};
    assert(actor_display(&a) == 50);
    a.display_override = 1; assert(actor_display(&a) == 1);
    a.display_override = -1; assert(actor_display(&a) == -1);
    a.display_override = 0; d.display = 0; assert(actor_display(&a) == 0);
    actor_size_set(&a, -3); assert(a.body_size == -3);
    actor_count = 1;
    actors[0] = (Actor){.role = (const Role *)1, .act = ACT_DEAD};
    a.foe = &actors[0];
    assert(actor_target(&a) == &actors[0]); // 死亡仍存在。
    actors[0].flags = 0x10;
    assert(!actor_target(&a) && a.foe == &actors[0]);
    Scene sc = {0};
    assert(!pursue(&a, &sc, 0) && a.foe == &actors[0]);
    actors[0].flags = 0;
    assert(actor_target(&a) == &actors[0]); // 移除旗標即可重現，不需重設目標。
    MonDef trap = {.istrap = 1};
    actors[0].mon = &trap; actors[0].flags = 0x10;
    assert(actor_target(&a) == &actors[0]); // 陷阱不是種類30..100，不套角色旗標檢查。
    actors[0].role = NULL;
    assert(!actor_target(&a) && !a.foe);
    a.foe = &saved;
    assert(!actor_target(&a) && !a.foe); // 陣列外的失效識別。
    a.foe = &actors[0]; actors[0].role = (const Role *)1; actor_count = 0;
    assert(!actor_target(&a) && !a.foe);
    actors[0] = saved; actor_count = saved_count;
}

static void character_animation_select_test(void) {
    RoleDef d = {.idle = 8, .walk = 16, .fight_idle = 24, .fight_walk = 32,
        .run_seq = 40, .hit = 48, .death = 56};
    Role r = {.seq_count = 64};
    Actor a = {.def = &d, .role = &r, .animation_dir = 3, .dynamic_state = 1};
    actor_animation_select(&a, -1); assert(a.animation_target == 11);
    a.fight = 1; actor_animation_select(&a, -1); assert(a.animation_target == 27);
    a.dynamic_state = 11; actor_animation_select(&a, -1); assert(a.animation_target == 35);
    for (int state = 20; state <= 30; state++) {
        a.dynamic_state = state; a.animation_target = 77;
        actor_animation_select(&a, -1); assert(a.animation_target == 77);
    }
    a.dynamic_state = 19; actor_animation_select(&a, -1); assert(a.animation_target == 35);
    a.dynamic_state = 31; actor_animation_select(&a, -1); assert(a.animation_target == 35);
    a.flags = 0x10000; actor_animation_select(&a, -1); assert(a.animation_target == 43);
    a.speed_threshold = 100; actor_animation_select(&a, -1); assert(a.animation_target == 43);
    a.speed_threshold = 101; actor_animation_select(&a, -1); assert(a.animation_target == 19);
    a.speed_threshold = 0;
    for (int bit = 0x20000; bit <= 0x80000; bit *= 4) {
        a.flags = 0x10000 | bit; actor_animation_select(&a, -1); assert(a.animation_target == 19);
    }
    a.flags = 0x10000; d.run_seq = 65535;
    actor_animation_select(&a, -1); assert(a.animation_target == 35);
    a.dynamic_state = 1; d.run_seq = 40;
    actor_animation_select(&a, -1); assert(a.animation_target == 27);
    a.buff_n = 1; a.buff[0].status = 101;
    actor_animation_select(&a, -1); assert(a.animation_target == 51);
    d.hit = 65535; a.animation_target = 78;
    actor_animation_select(&a, -1); assert(a.animation_target == 78);
    a.knock = 1; actor_animation_select(&a, -1); assert(a.animation_target == 59);
    r.seq_count = 59; actor_animation_select(&a, -1); assert(a.animation_target == 27);
    d.death = 65535; a.animation_target = 79;
    actor_animation_select(&a, -1); assert(a.animation_target == 79);
    actor_animation_select(&a, 83); assert(a.animation_target == 83 && a.animation_dir == 3);
    actor_animation_select(&a, -2); assert(a.animation_target == -2 && a.animation_dir == -2);
    // 正式選擇路徑供繪製及畫面矩形使用；讀取不改目標／方向。
    a = (Actor){.def = &d, .role = &r, .dynamic_state = 1, .animation_target = 99, .animation_dir = 6};
    assert(actor_seq(&a) == d.idle + DIR_MAP[0]);
    assert(a.animation_target == 99 && a.animation_dir == 6);
    a.act = ACT_ATTACK; a.act_seq = 64;
    assert(actor_seq(&a) == 64 + DIR_MAP[0]);
}

static void character_movement_speed_test(void) {
    // 原版浮點常數直接從Poq的唯讀區核對；1.0是唯一回0的比值。
    static const struct { float ratio; int delay; } cases[] = {
        {0,7}, {.299f,7}, {.3f,5}, {.499f,5}, {.5f,3}, {.699f,3}, {.7f,3},
        {.701f,1}, {.999f,1}, {1,0}, {1.001f,1}, {1.5f,1}, {1.501f,-1},
        {2,-1}, {2.001f,-2}, {3,-2}, {3.001f,-3}, {4,-3}, {4.001f,-4},
        {5,-4}, {5.001f,-5}, {INFINITY,-5}, {NAN,-5}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++)
        assert(movement_delay_adjust(cases[i].ratio) == cases[i].delay);
    Actor a = {.speed = 5, .fight_speed = 11, .dynamic_state = 11, .moving = 1};
    actor_speed_update(&a);
    assert(a.move_a == 1 && a.move_b == 1 && a.move_c == 7 && a.move_remaining == 1);
    assert(a.walk_delay_adjust == 0 && a.dynamic_state == 11);
    a.fight = 1; actor_speed_update(&a);
    assert(a.move_a == 3 && a.walk_delay_adjust == -2);
    a.speed_override = 1; actor_speed_update(&a);
    assert(a.move_a == 0 && a.move_b == 90 && a.walk_delay_adjust == 7);
    assert(a.dynamic_state == 1 && !a.moving && a.print_x == -100 && a.print_y == -100);
    // 查表可夾限，getter本身保留範圍外值。
    a.speed_override = 101; actor_speed_update(&a);
    assert(actor_speed(&a) == 100 && a.move_a == 19 && a.move_c == 7);
    actor_move_speed(&a, -10); assert(a.move_a == 0 && a.move_b == 90);
    a.speed = 0; actor_move_speed(&a, 0); assert(a.walk_delay_adjust == -5); // 0/0的NaN走原版末分支。
    actor_move_speed(&a, 5); assert(a.walk_delay_adjust == -5); // 基準0，非零/0。
    a.speed = 2; actor_move_speed(&a, 2); assert(a.walk_delay_adjust == -5); // 小數C不參與比例，仍為0/0。
    // 同一Role的兩個角色可有不同延遲，不能改共用幀資料。
    Frame frames[] = {{.delay = 5}, {.delay = 5}};
    Seq seq[] = {{.count = 2, .frames = frames}, {.count = 2, .frames = frames}};
    Anim anim = {.count = 2, .seqs = seq};
    Role role = {.part_count = 1, .seq_count = 2, .body = {&anim}};
    RoleDef d = {.walk = 1, .fight_walk = 65535};
    Actor slow = {.role = &role, .def = &d, .act_tick = 4, .walk_delay_adjust = 3};
    Actor fast = slow; fast.walk_delay_adjust = -2;
    assert(actor_frame(&slow, 1) == 0 && actor_frame(&fast, 1) == 1);
    assert(actor_frame(&fast, 0) == 0 && frames[0].delay == 5);
    fast.walk_delay_adjust = -5; assert(actor_frame(&fast, 1) == 0); // 4 mod 2。
}

static void character_properties_test(void) {
    RoleDef d = {.seek = 7, .chase = 12};
    Actor a = {.def = &d, .speed = 20, .fight_speed = 30, .dynamic_state = 1};
    actor_property(&a, "B_C_MOVE_SPEED", 150);
    assert(a.ex[64] == 150 && actor_move_bonus(&a) == 75 && actor_speed(&a) == 35);
    actor_property(&a, "B_C_MOVE_SPEED", -101);
    assert(actor_move_bonus(&a) == -101 && actor_speed(&a) == 0);
    actor_property(&a, "B_C_FIGHT_SPEED", 1);
    assert(fabsf(actor_fight_ratio(&a) - 100.0f / (100.0f + 120.0f / 121.0f)) < .000001f);
    actor_property(&a, "B_C_FIGHT_SPEED", -100);
    assert(actor_fight_ratio(&a) == 100.0f); // 原版零分母改1，不能改成無加速。
    actor_property(&a, "B_C_FIGHT_SPEED", -101);
    assert(actor_fight_ratio(&a) == -100.0f); // 原版未夾限負倍率。
    static const int groups[] = {-1, 0, 99, 100, 9999, 10000, 60000, 60001};
    for (unsigned i = 0; i < sizeof groups / sizeof *groups; i++) {
        a.grp = groups[i];
        actor_property(&a, "B_C_VRANGE", -3);
        assert(a.view_range == 9 && actor_seek(&a) == (grp_npc(a.grp) ? 7 : 4));
        a.layer = 11;
        assert(actor_seek(&a) == (grp_npc(a.grp) ? 15 : 12));
        assert(actor_chase(&a) == 20); // 視野屬性不改追擊距離。
        a.layer = 1;
    }
    ActorSkill short_skill = {.max = 3}, long_skill = {.max = 4};
    actor_property(&a, "B_C_SRANGE", -2);
    assert(actor_skill_range(&a, &short_skill) == 3 && actor_skill_range(&a, &long_skill) == 2);
    const char *names[] = {"B_C_ZMYJ", "B_C_HXYJ", "B_C_JDR", "B_C_DJHF"};
    for (int i = 0; i < 4; i++) { actor_property(&a, names[i], -i - 1); assert(a.ex[72+i] == -i-1); }
    Actor saved = a;
    actor_property(&a, "b_c_vrange", 999);
    actor_property(&a, "B_C_SRANGE_SUFFIX", 999);
    actor_property(&a, "", 999);
    assert(!memcmp(&a, &saved, sizeof a));
    // 整合：怪物表、技能加值與重算必須真正到達角色範圍欄位。
    MonDef mon = {.vrange = 5, .mspeed = 150, .mfight = 1};
    SkillRow row = {.mod_n = 2, .mod = {{.id = 66, .init = 2}, {.id = 67, .init = 3}}};
    a = (Actor){.def = &d, .mon = &mon, .level = 1, .buff_n = 1,
                .buff = {{.skill = &row, .level = 1}}, .grp = 0};
    mon_derive(&a);
    assert(a.view_range == 19 && actor_seek(&a) == 14 && a.range_adjust == 3);
    assert(actor_move_bonus(&a) == 75 && a.ex[65] == 1);
    a.grp = 10000; a.buff_n = 0; mon_derive(&a);
    assert(a.view_range == 17 && actor_seek(&a) == 7 && !a.range_adjust);
    // 動畫消費端保留float比例，不能先把正攻速截為整數百分比。
    Frame frames[3] = {{.delay = 10}, {.delay = 10}, {.delay = 10}};
    Seq seq = {.count = 3, .frames = frames};
    Anim anim = {.count = 1, .seqs = &seq};
    Role role = {.part_count = 1, .seq_count = 1, .body = {&anim}};
    a = (Actor){.role = &role, .def = &d, .act = ACT_ATTACK, .dir = 2};
    actor_property(&a, "B_C_FIGHT_SPEED", 1);
    attack_speed(&a);
    assert(a.adj == -1 && a.extra == 2); // cut=30-trunc(30*ratio)=1。
    actor_property(&a, "B_C_FIGHT_SPEED", -100);
    a.adj = a.extra = 0; attack_speed(&a);
    assert(a.adj == 990 && !a.extra); // cut=30-3000=-2970。
    d.atk_delay = 30;
    Scene sc = {0};
    a = (Actor){.role = &role, .def = &d, .dir = 2, .grp = 10000,
                .act = ACT_ATTACK, .act_done = 1, .extra = 2};
    actor_property(&a, "B_C_FIGHT_SPEED", 1);
    action_tick(&a, &sc, 0);
    assert(a.cool == 31 && !a.act); // trunc(30*ratio)+2=31，與mon指標無關。
    a.act = ACT_ATTACK; a.act_done = 1; a.extra = 0; a.grp = 0; a.mon = &mon;
    srand(123); int expected = 30 + rand() % 30;
    srand(123); action_tick(&a, &sc, 0);
    assert(a.cool == expected && !a.act);
}

static void character_speed_memory_test(void) {
    Actor a = {.speed = 7, .fight_speed = 11, .dynamic_state = 1};
    assert(actor_speed(&a) == 7);
    a.fight = 1; assert(actor_speed(&a) == 11);
    a.flags = 0x10000; assert(actor_speed(&a) == 11); // 跑步只在移動狀態生效。
    a.dynamic_state = 11; assert(actor_speed(&a) == 10); // 7*3/2，先截斷。
    a.speed_override = 1; assert(actor_speed(&a) == 0);
    a.speed_override = 10; assert(actor_speed(&a) == 9);
    a.speed_threshold = 100; assert(actor_speed(&a) == 9);
    a.speed_threshold = 101; assert(actor_speed(&a) == 4);
    a.speed_threshold = 102; assert(actor_speed(&a) == 4);
    a.speed_threshold = 0; a.speed_override = 101;
    assert(actor_speed(&a) == 100); // getter不夾到59。
    a.ex[64] = -101; assert(actor_speed(&a) == -1); // 加速後不再夾到0。
    a.ex[64] = 150; assert(actor_speed(&a) == 175);
    for (int cmd = -1; cmd <= 9; cmd++) {
        a.remembered[0] = cmd; a.remembered[1] = 12;
        a.remembered[2] = 34; a.remembered[3] = 56;
        a.remember_delay = 99;
        actor_forget_command(&a);
        assert(a.remembered[0] == -1);
        assert(a.remember_delay == (cmd == 1 || cmd == 2 ? 50 : 0));
        assert(a.remembered[1] == 12 && a.remembered[2] == 34 && a.remembered[3] == 56);
        actor_forget_command(&a); assert(a.remember_delay == 0);
    }
    // 失敗施法仍做收尾；記住不同技能且有敵對目標時保留。
    RoleDef d = {0}; Actor enemy = {.grp = 10000};
    a = (Actor){.def = &d, .grp = 0, .foe = &enemy, .remembered = {8, 123, 34, 56}};
    assert(!actor_cast_at(&a, 999, 0, 0, NULL) && a.remembered[0] == 8);
    assert(!actor_cast_at(&a, 123, 0, 0, NULL) && a.remembered[0] == -1);
    assert(a.remembered[1] == 123 && a.remembered[2] == 34 && a.remembered[3] == 56);
    a.remembered[0] = 9; a.foe = NULL;
    assert(!actor_cast_at(&a, 999, 0, 0, NULL) && a.remembered[0] == -1);
    a.remembered[0] = 9; a.foe = &enemy; enemy.grp = 0;
    assert(!actor_cast_at(&a, 999, 0, 0, NULL) && a.remembered[0] == -1);
}

static void character_template_candidates_test(void) {
    // 假模型快取讓測試走真實actor_setup，不需建立SDL視窗或載入紙娃娃。
    RoleSlot saved = roles[0];
    int saved_count = role_count;
    RoleDef d = {.id = 1234567, .candidate = {65535, 65535, 65535, 65535}};
    roles[0] = (RoleSlot){.tmpl = d.id};
    role_count = 1;
    Actor fresh = {0};
    actor_setup(&fresh, &d);
    assert(fresh.body_size == 1);
    for (int i = 0; i < 4; i++) assert(fresh.candidate[i] == -1);
    Actor a = {.def = &d, .candidate = {276, 277, 278, 279}};
    actor_setup(&a, &d);
    for (int i = 0; i < 4; i++) assert(a.candidate[i] == 276 + i);
    d.candidate[1] = 0; d.candidate[2] = -1; d.candidate[3] = 280;
    actor_setup(&a, &d);
    assert(a.candidate[0] == 276 && a.candidate[1] == 0);
    assert(a.candidate[2] == -1 && a.candidate[3] == 280);
    d.size = 6; actor_setup(&a, &d); assert(a.body_size == 1);
    d.size = 7; actor_setup(&a, &d); assert(a.body_size == 2);
    d.size = 100; actor_setup(&a, &d); assert(a.body_size == 95);
    d.candidate[1] = d.candidate[2] = d.candidate[3] = 65535;
    actor_setup(&a, &d);
    assert(a.candidate[0] == 276 && a.candidate[1] == 0);
    assert(a.candidate[2] == -1 && a.candidate[3] == 280);
    roles[0] = saved;
    role_count = saved_count;
}

static void character_skill_runtime_test(void) {
    RoleDef d = {.speed = 5, .skill_count = 3, .skill_ids = {65535, 1, 1}};
    Actor a = {.def = &d, .speed = 5, .right_id = 77, .left_id = 88,
        .skill_n = 1, .selected = 0, .skills = {{.id = 999}}};
    actor_skills_rebuild(&a);
    assert(a.template_skill_n == 3 && a.template_skill[0] == 65535);
    assert(a.skill_n == 2 && a.skills[0].id == 1 && a.skills[1].id == 1);
    assert(a.skills[0].max == method_find(1)->range_max && a.skills[0].min == method_find(1)->range_min);
    assert(a.skills[0].enabled == 1 && a.skills[0].p8 == 1 && a.skills[0].p9 == 1);
    assert(!a.skills[0].key && !a.skills[0].p6 && !a.skills[0].p7);
    assert(a.right_id == 77 && a.left_id == 88 && a.selected == -1);
    int saved_n = unit_n;
    MUnit saved_u = units[0];
    unit_n = 0;
    a.skills[0].enabled = 7;
    assert(actor_can_use_id(&a, 1) == 7);
    a.skills[0].enabled = 0;
    assert(!actor_can_use_id(&a, 1)); // 精確命中不會偷偷換成後面的啟用重複項。
    assert(actor_can_use_id(&a, -1) == 1); // fallback可取第二個啟用項。
    a.flags = 0x800;
    assert(!actor_can_use_id(&a, -1));
    Method m = {.interruptible = 1};
    units[0] = (MUnit){.cls = U_MAIN, .owner = &a, .m = &m}; unit_n = 1;
    assert(actor_can_use_id(&a, 1) == 1); // 中斷條件優先，無視禁止攻擊及停用。
    m.interruptible = 0;
    assert(!actor_can_use_id(&a, 1));
    units[0].dead = 1; a.flags = 0;
    a.skill_n = 0;
    assert(actor_can_use_id(&a, 65535) == 1); // 空鏈且沒有MethodMain允許NULL。
    a.skill_n = 1; a.skills[0].max = 3; a.skills[0].enabled = 9; a.speed = 0;
    assert(!actor_can_use_id(&a, 1));
    d.speed = 0; assert(actor_can_use_id(&a, 1) == 9);
    units[0] = saved_u; unit_n = saved_n;
    // 中斷欄位來自任一iFlag=4，而非wType、Reserve4或觸發種類。
    assert(method_find(31)->interruptible && method_find(255)->interruptible);
    assert(!method_find(1)->interruptible);
    // 招式598的flag4在第24個觸發之後，仍必須判為可中斷。
    int visible = 0;
    for (int i = 0; i < method_find(598)->trig_count; i++) visible |= method_find(598)->trig[i].flag == 4;
    assert(!visible && method_find(598)->interruptible);
    // 模板項與玩家第0招共存：右手仍能接到技能列，升級不能修改模板的key=0項。
    SkillRow saved_skill = hero_skill[0];
    int saved_skill_n = hero_skill_n, saved_right = cur_skill[0], saved_left = cur_skill[1];
    hero_skill_n = 1; cur_skill[0] = cur_skill[1] = 0;
    hero_skill[0] = (SkillRow){.level = 1, .mod_n = 1, .mod = {{.id = 10, .init = 1}}};
    a.def = &d; a.foe = NULL;
    hero_skill_apply(&a);
    assert(a.selected == 0 && a.skills[0].key == 0 && a.skills[2].key == 1);
    assert(a.skill == &hero_skill[0] && a.atk == method_find(1));
    int raw_range = a.skills[0].max;
    actor_skill_update(&a, 1, 1, 3, 8, 0, 0);
    assert(a.skills[0].max == raw_range && a.skills[2].max == 8);
    hero_skill[0] = saved_skill; hero_skill_n = saved_skill_n;
    cur_skill[0] = saved_right; cur_skill[1] = saved_left;
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
    // CBuilding 半透明只在特殊路徑；未知模式不畫；畫後恢復全域 alpha。
    Object *test = &objects[0]; models[0].parts[0].anim = 0; models[0].display = 5;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
    building_part_draw(test, 0, 32, 16, 0, 1); SDL_RenderPresent(ren);
    SDL_GetRGBA(((Uint32 *)surf->pixels)[18 * (surf->pitch / 4) + 34], surf->format, &r, &g, &b, &alpha);
    assert(r >= 127 && r <= 128 && g == 0 && b == 0 && draw_alpha == 255);
    models[0].display = 3;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
    building_part_draw(test, 0, 32, 16, 0, 1); SDL_RenderPresent(ren);
    SDL_GetRGBA(((Uint32 *)surf->pixels)[18 * (surf->pitch / 4) + 34], surf->format, &r, &g, &b, &alpha);
    assert(!r && !g && !b);
    ground_n = saved_ground; unit_n = saved_units;
    cache_clear(); SDL_DestroyRenderer(ren); ren = NULL; SDL_FreeSurface(surf);
    free(l.items); free(cells);
    view_buildings.n = view_dynamic.n = 0;
    vw = old_w; vh = old_h;
}

static void tribuilding_test(void) {
    Cell cells[9] = {0}; uint16_t footprint[] = {0x81, 0x82};
    Model m = {.type = 0x80, .part_count = 4, .bottom = 1, .top = 1,
               .grid_w = 2, .grid_h = 1, .footprint = footprint};
    Object o = {.m = &m, .x = 1, .y = 1, .tri = 1, .state = -1};
    Scene s = {.w = 3, .h = 3, .cells = cells, .objects = &o, .object_count = 1};
    scene_building_cells(&s, 0, -1);
    cells[5].walk = 7;
    scene_building_state(&s, 0, 1);
    assert(o.state == 1 && o.bottom == 4 && cells[4].walk == 1 && cells[5].walk == 7);
    scene_building_state(&s, 0, 5); // 非門且不是0/2，不准啟動
    assert(o.state == 1);
    scene_building_state(&s, 0, 2);
    assert(building_available(&o) && o.bottom == 1 && o.top == 1 && !cells[4].walk);
    cells[4].walk = 9; scene_building_state(&s, 0, 2);
    assert(cells[4].walk == 9); // 同狀態不重寫walk
    const Scene *saved = unit_scene; unit_scene = &s;
    cells[4].flags = 0x88;
    assert(input_building(&s, 1, 1) == &o);
    assert(input_building(&s, 1, 2) == &o); // 下一格向y-1
    m.type = 0x96; assert(!input_building(&s, 1, 1));
    m.type = 0x100; assert(input_building(&s, 1, 1) == &o);
    m.type = 0x101; assert(!input_building(&s, 1, 1));
    m.type = 0x81; o.state = 1; assert(!input_building(&s, 1, 1));
    assert(cell_bld(1, 1) == 0 && cell_bld(-1, 1) == -1);
    o.state = 0; cells[4].flags = 0x80; assert(!input_building(&s, 1, 1));
    unit_scene = saved;
    // 用記憶體動畫測獨立序列選擇與幀號，無需建立鏡像播放器。
    Frame f[2] = {{.delay = 2}, {.delay = 3}};
    Seq seq[3] = {{2, f}, {2, f}, {2, f}}; Anim a = {.count = 3, .seqs = seq};
    o.seq[0] = 2; o.frame[0] = 1;
    assert(building_sequence(&o, 0, &a) == &seq[2]);
    assert(building_part_frame(&o, 0, &seq[2], 999) == 1);
    o.state = 5; o.seq[0] = 1; o.frame[0] = o.elapsed[0] = 0;
    for (int t = 0; t < 4; t++) assert(!building_part_tick(&o, 0, &a));
    assert(o.frame[0] == 1 && o.elapsed[0] == 2);
    assert(building_part_tick(&o, 0, &a)); // 最後一幀最後一拍，不多播一圈
    o.state = 1;
    assert(!building_part_tick(&o, 0, &a) && !o.frame[0] && !o.elapsed[0]);

}

static void building_test(void) {
    Cell cells[16] = {0};
    uint16_t footprint[] = {0x81, 0x182, 0x7f, 0x88, 0x89, 0x82};
    Model m = {.grid_x = -1, .grid_y = -1, .grid_w = 3, .grid_h = 2,
               .anchor_x = 1, .anchor_y = 1, .footprint = footprint, .part_count = 3, .bottom = 1};
    Object o = {.x = 2, .y = 2, .m = &m};
    Scene s = {.w = 4, .h = 4, .cells = cells, .objects = &o, .object_count = 1};
    scene_building_cells(&s, 0, -1);
    assert(cells[0].flags == 0x81 && cells[0].building == 0);
    assert(cells[1].flags == 0x82 && !cells[2].flags);
    assert(cells[4].flags == 0x88 && cells[5].flags == 0x89 && cells[6].flags == 0x82);
    assert(cells[10].origin == 1); // 原點標記和占地分開
    cells[1].walk = 7; cells[4].walk = 7;
    scene_building_cells(&s, 0, 1);
    assert(cells[0].walk == 1 && cells[5].walk == 1 && cells[1].walk == 7 && cells[4].walk == 7);
    scene_building_cells(&s, 0, 0);
    assert(!cells[0].walk && !cells[5].walk && cells[1].walk == 7);
    o.x = o.y = 0; scene_building_cells(&s, 0, -1); // 負占地和越界不存取格子
    m.block_x = -1; m.block_y = 0;
    ViewItem who = {.depth = -65, .x64 = -65, .y64 = 0};
    assert(viewport_blocks(&o, &who)); // 原版先判 X<1，即使 Y=0
    who.x64 = -64; assert(!viewport_blocks(&o, &who));
    who.x64 = -65; who.depth = -64; assert(!viewport_blocks(&o, &who)); // 和相等不擋
    m.display = 5;
    assert(building_display(&o) == 5 && !building_special(&o, 1));
    building_highlight(&o, 1);
    assert(building_display(&o) == 15 && !building_special(&o, 0) && building_special(&o, 1));
    building_highlight(&o, 0); assert(building_display(&o) == 5);
    m.parts[0].anim = 1; assert(building_special(&o, 0));
    Frame frames[3] = {{.delay = 2}, {.delay = 5}, {.delay = 3}};
    Seq q = {3, frames}; o.phase = 1001 % 1000;
    assert(building_frame(&o, &q, 0) == 1 && building_frame(&o, &q, 4) == 1);
    assert(building_frame(&o, &q, 5) == 2 && building_frame(&o, &q, 8) == 0);
    m.rect_x = -10; m.rect_y = -20; m.rect_w = 100; m.rect_h = 80;
    SDL_Rect rect = viewport_building_rect(&o, 50, 60, 0);
    assert(rect.x == 40 && rect.y == 40 && rect.w == 100 && rect.h == 80);
    ViewPort v = {0}; SDL_Point p = viewport_grid_screen(&v, o.x, o.y);
    viewport_building_hover(&v, &s, p.x + 32, p.y + 16, 1, 0); assert(o.highlight);
    viewport_building_hover(&v, &s, p.x + 32, p.y + 16, 0, 0); assert(!o.highlight);
    Scene real = {0}; assert(scene_load(&real, "yangzhou.Scn"));
    int marked = 0, large = 0;
    for (int i = 0; i < real.w * real.h; i++) marked += !!(real.cells[i].flags & 128);
    for (int i = 0; i < real.model_count; i++) large += real.models[i].part_count > 8;
    assert(marked > 0 && real.object_count > 0);
    (void)large; // 實際模型部件不再截斷到8；不同場景不強求有大型模型。
    for (int i = 0; i < real.object_count; i++) {
        const Object *o = &real.objects[i]; const Model *model = o->m;
        if (!model) continue;
        assert(o->door_x == (model->door_x || model->door_y ? o->x - model->anchor_x + model->door_x : o->x));
        assert(o->door_y == (model->door_x || model->door_y ? o->y - model->anchor_y + model->door_y : o->y));
    }
    scene_free(&real);
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

static void air_test(void) {
    RoleDef d = {.speed = 10, .fight_speed = 20, .snd_idle = 0xffff, .snd_run = 0xffff};
    NpcSpec n = {.x = -2, .y = -2, .logic = 0};
    AirBeastie a = {.def = &d, .speed = 10};
    air_place(&a, &n);
    assert(a.x == -96 && a.gx == -2);
    assert(!air_move(&a, -2, -2) && a.state == 0);
    assert(air_move(&a, 0, -2) && a.steps == 12 && a.state == 1);
    for (int i = 0; i < 12; i++) air_tick_one(&a, NULL);
    assert(a.steps == 0 && a.state == 1 && a.gx == 0 && a.gy == -1); // -96/64向零
    air_tick_one(&a, NULL);
    assert(a.state == 0 && a.speed == 10); // 多一拍才回休息
    n = (NpcSpec){.logic = 1, .rest = 4};
    a = (AirBeastie){.spec = &n, .def = &d};
    air_tick_one(&a, NULL);
    assert(a.wait == 10000 && !a.state);
    air_tick_one(&a, NULL); assert(a.wait == 9999);
    n = (NpcSpec){.logic = 2, .path_n = 3, .path = {{2, 0}, {4, 0}, {0, 0}}};
    a = (AirBeastie){.spec = &n, .def = &d, .speed = 10, .x = 32, .y = 32};
    air_tick_one(&a, NULL);
    assert(a.path == 1 && a.tx == 2 && a.state == 1);
    Actor t = {.x = 3, .y = 3};
    d.reaction = 1;
    air_run(&a, &t);
    assert(a.state == 2 && a.tx == 4 && a.speed == 20); // +3後最遠路線點
    d.reaction = 2;
    t.x = 7;
    air_run(&a, &t);
    assert(a.tx == 4); // +3後最近，不是直接追到角色格
    n.logic = 1; a.state = 0; a.steps = 19;
    air_run(&a, &t);
    assert(a.state == 2 && a.speed == 20 && a.steps == 19); // 不支持仍先改跑態
    d.reaction = 1; a.gx = a.gy = 0; a.x = a.y = 32; t.x = t.y = -10;
    air_run(&a, &t);
    assert(a.tx == 5 && a.ty == 5); // OmUtil呼叫點的5格逃跑
    // 動畫只由邏輯更新推進；休息+8、走跑共用方向序列，切換從第0幀再前進。
    Frame f[2] = {{.delay = 1}, {.delay = 1}};
    Seq q[16]; for (int i = 0; i < 16; i++) q[i] = (Seq){2, f};
    Anim anim = {.count = 16, .seqs = q};
    a.spr = (Spr){.a = &anim}; a.state = 0; a.dir = 5;
    air_animate(&a); assert(a.spr.seq == 13 && a.spr.cur == 1);
    a.state = 2; air_animate(&a); assert(a.spr.seq == 5 && a.spr.cur == 1);
    a.state = 1; air_animate(&a); assert(a.spr.seq == 5 && a.spr.cur == 0);
}

static void bird_test(void) {
    FastBird b;
    bird_init(&b, -2, 3);
    assert(b.spr.a && b.spr.a->count >= 8); // 真實固定動畫，亦防止資源初始化順序回歸。
    assert(b.x == -96 && b.y == 224 && b.dir >= 0 && b.dir < 8);
    assert(b.speed == 16 && b.limit == 100 && b.outside == 0 && b.spr.seq == 0);
    b.dir = 0;
    int x = b.x, y = b.y;
    for (int i = 0; i < 101; i++) assert(bird_tick_one(&b, 0));
    assert(b.outside == 101 && b.x == x + 101 * 16 && b.y == y - 101 * 16);
    assert(!bird_tick_one(&b, 0) && b.x == x + 101 * 16);
    assert(bird_tick_one(&b, 1) && b.outside == 0);
    Frame f[2] = {{.delay = 1}, {.delay = 1}};
    Seq seq[8]; for (int i = 0; i < 8; i++) seq[i] = (Seq){2, f};
    Anim a = {.count = 8, .seqs = seq};
    b.spr = (Spr){.a = &a}; b.dir = 3;
    bird_animate(&b); assert(b.spr.seq == 3 && b.spr.cur == 1);
    bird_animate(&b); assert(b.spr.cur == 0);
    bird_clear(); bird_spawn(2, 3); bird_spawn(4, 5);
    assert(bird_n == 2 && birds[1].x == 288);
    Scene empty = {0};
    ViewPort v = {.w = 640, .h = 480};
    // 空幀代表不在畫面內；刪第一隻後仍須更新搬到槽0的第二隻。
    birds[0].spr.a = birds[1].spr.a = NULL;
    birds[0].outside = 101; birds[1].dir = 7;
    bird_tick(&empty, &v);
    assert(bird_n == 1 && birds[0].outside == 1 && birds[0].x == 304);
    air_enter(&empty); assert(!bird_n && !birds);
    Cell cell = {0}; Scene scene = {.cells = &cell, .w = 100};
    bird_countdown = 2; bird_generate(&scene, &v); assert(bird_countdown == 1 && bird_n == 0);
    bird_generate(&scene, &v); assert(bird_countdown == 0 && bird_n == 0);
    // 重擲落在<11時必定追加左側八隻；另一次隨機點可能也在畫面外。
    int flock_tested = 0;
    for (int seed = 0; seed < 1000; seed++) {
        srand(seed); int next = rand() % 200;
        if (next >= 11) continue;
        srand(seed); bird_countdown = 0; bird_generate(&scene, &v);
        assert(bird_countdown == next && bird_n >= 8 && bird_n <= 9);
        flock_tested = 1; break;
    }
    assert(flock_tested);
    bird_clear(); bird_countdown = -1;
}

static void character_skill_index_test(void) {
    Actor saved[2] = {actors[0], actors[1]};
    int saved_count = actor_count;
    actor_count = 2;
    Actor *a = &actors[0], *target = &actors[1];
    *a = (Actor){.selected = 7, .right_id = 321, .left_id = 654, .layer = 1};
    *target = (Actor){.layer = 11, .role = (const Role *)1};
    actor_skill_select_index(a, 0);
    assert(a->selected == 7); // 空鏈保留舊選取，連越界也不清。
    a->skill_n = 45;
    for (int i = 0; i < a->skill_n; i++)
        a->skills[i] = (ActorSkill){.id = 100 + i, .max = 3, .enabled = 0};
    for (int mod = 0; mod < 8; mod++) {
        static const int offsets[8] = {0, 9, 18, 18, 0, 36, 27, 36};
        SDL_Keymod m = (SDL_Keymod)((mod & 1 ? KMOD_CTRL : 0) |
            (mod & 2 ? KMOD_SHIFT : 0) | (mod & 4 ? KMOD_ALT : 0));
        for (SDL_Keycode key = SDLK_1; key <= SDLK_9; key++) {
            int index = skill_key_index(key, m);
            assert(index == offsets[mod] + key - SDLK_1);
            actor_skill_select_index(a, index);
            assert(a->selected == index); // 停用項仍可選，不呼叫Enable檢查。
        }
    }
    assert(skill_key_index(SDLK_0, KMOD_NONE) == -1);
    assert(skill_key_index(SDLK_KP_1, KMOD_NONE) == -1);
    actor_skill_select_index(a, -1); assert(a->selected == -1);
    actor_skill_select_index(a, 45); assert(a->selected == -1);
    a->foe = target;
    a->skills[0].max = 2;
    actor_skill_select_index(a, 0); assert(a->selected == -1);
    a->skills[0].max = 3;
    actor_skill_select_index(a, 0); assert(a->selected == 0); // 高一級可遠攻。
    target->layer = 13;
    actor_skill_select_index(a, 0); assert(a->selected == -1); // 高兩級不可。
    target->flags = 0x10;
    actor_skill_select_index(a, 0); assert(a->selected == 0); // 隱藏目標getter回空。
    assert(a->foe == target && a->right_id == 321 && a->left_id == 654);
    target->flags = 0;
    actor_count = 1;
    actor_skill_select_index(a, 0);
    assert(a->selected == 0 && !a->foe); // 失效目標由getter清掉。
    // 鍵盤消費端用選中項更新攻擊資料，不能重建鏈後選回右手ID。
    skills_load();
    assert(hero_skill_n > 0);
    a->skills[0].id = skill_attack_id(&hero_skill[0]);
    hero_selected_skill_apply(a);
    assert(a->skill == &hero_skill[0] && a->atk == method_find(a->skills[0].id));
    assert(a->skill_n == 45 && a->right_id == 321 && a->left_id == 654);
    const Method *playing = a->atk;
    const SkillRow *playing_skill = a->skill;
    a->act = ACT_ATTACK;
    a->selected = -1;
    hero_selected_skill_apply(a);
    assert(a->selection_pending && a->atk == playing && a->skill == playing_skill);
    a->act = ACT_NONE; a->casting = 1;
    hero_selected_skill_apply(a);
    assert(a->selection_pending && a->atk == playing && a->skill == playing_skill);
    a->casting = 0;
    hero_selected_skill_apply(a);
    assert(!a->selection_pending);
    assert(!a->skill && !a->atk);
    actors[0] = saved[0]; actors[1] = saved[1]; actor_count = saved_count;
}

int main(void) {
    // 資源測試必須先設定路徑；anim_get會快取不存在的動畫，晚設路徑無法補救。
    res_init("../../game");
    air_test();
    bird_test();
    character_lookup_test();
    character_core_test();
    character_weapon_type_test();
    character_relation_test();
    character_scale_test();
    character_skills_test();
    character_skill_runtime_test();
    character_skill_index_test();
    character_template_candidates_test();
    character_movement_speed_test();
    character_speed_memory_test();
    character_properties_test();
    character_animation_select_test();
    character_near_test();
    character_target_display_test();
    character_names_owner_test();
    scene_client_test();
    viewport_test();
    building_test();
    tribuilding_test();
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
    assert(roledef_find(29)->scale == 65535);
    assert(roledef_find(327)->scale == 4 && roledef_find(320)->scale == 14);
    {
        Actor sized = {0};
        RoleDef sized_def = *roledef_find(327);
        actor_setup(&sized, &sized_def); assert(sized.scale == 4);
        sized_def.scale = 65535;
        actor_setup(&sized, &sized_def); assert(sized.scale == 4);
        sized_def.scale = 0;
        actor_setup(&sized, &sized_def); assert(sized.scale == 0);
    }
    character_move_cast_test();
    assert(roledef_find(204) && !roledef_find(204)->beast && !roledef_find(204)->air);
    // RoleMdl.rmd的分組與角色ID無關，第一列ID29必須取得第一組57招。
    const RoleDef *template = roledef_find(29);
    assert(template && template->skill_count == 57 && template->skill_ids[0] == 22 && template->skill_ids[56] == 674);
    assert(roledef_find(206)->skill_count == 34 && roledef_find(206)->skill_ids[0] == 167);
    const RoleDef *crow = roledef_find(1003);
    assert(crow && crow->beast && crow->air == 1 && crow->reaction == 0 && crow->snd_idle == 26);
    NpcSpec air_specs[2] = {{.tmpl = 1003, .x = 8, .y = 9}, {.tmpl = 1005}};
    Scene air_scene = {.npcs = air_specs, .npc_count = 2};
    air_enter(&air_scene);
    assert(air_n == 1 && air[0].gx == 8 && air[0].x == 544 && air[0].spr.a && air[0].dir == 5);
    air_scene.npc_count = 0; air_enter(&air_scene); assert(!air_n);
    Scene birds = {0};
    assert(scene_load(&birds, "yangzhou.Scn"));
    air_enter(&birds);
    assert(air_n == 12); // 追主角烏鴉1、普通烏鴉/小鳥9、蜜蜂2
    int crow_i = 0; while (crow_i < air_n && air[crow_i].def->id != 1003) crow_i++;
    assert(crow_i < air_n);
    AirBeastie *placed = &air[crow_i];
    assert(placed->spec->logic == 2 && placed->spec->path_n == 6);
    assert(placed->gx == 50 && placed->spec->path[0][0] == 48);
    air_tick(NULL);
    assert(placed->state == 1 && placed->tx == 48 && placed->ty == 90);
    Scene empty = {0}; air_enter(&empty); assert(!air_n);
    scene_free(&birds);
    free(air); air = NULL;
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
    // 目標須在場景物件名單裡，走正式的查找而非未登記的區域指標。
    Actor saved_target = actors[0]; int saved_actor_count = actor_count;
    actors[0] = t; actor_count = 1;
    a.foe = &actors[0]; a.skill = &far; a.ai_tick = 0; a.cool = 100000;  // 不出手（測試角色沒有動畫）
    for (int i = 0; i < 61 && !a.flee; i++) { a.atk = &fm2; a.skill = &far; pursue(&a, &sc, 0); }
    assert(a.flee && (int)a.tx == 15 && (int)a.ty == 20);
    actors[0] = saved_target; actor_count = saved_actor_count;
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
        SDL_Surface *pick_surf = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
        assert(pick_surf); ren = SDL_CreateSoftwareRenderer(pick_surf); assert(ren);
        actors[0].def = actors[1].def = roledef_find(29);
        actors[0].role = actors[1].role = role_for(actors[0].def);
        assert(actors[0].role);
        actors[0].dir = actors[1].dir = 2;
        actors[0].act = actors[1].act = ACT_NONE;
        pick_s = &test_scene; pick_ox = pick_oy = 0;
        int l, tp, w, h;
        pick_box(&actors[1], &l, &tp, &w, &h);
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 1, -1, pick_box) == &actors[1]);
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 1, 0, pick_box) != &actors[1]);
        actors[1].flags = 0x80;
        assert(pick_at(l + w / 2, tp + h / 2, 6, 5, 0, 1, pick_box) == NULL);
        actors[1].flags = 0;
        cache_clear(); SDL_DestroyRenderer(ren); ren = NULL; SDL_FreeSurface(pick_surf);
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
        Actor saved_hover_target = actors[0]; int saved_hover_count = actor_count;
        actors[0] = c; actor_count = 1; h.foe = &actors[0];
        assert(status_target(&h, &h, 1) == &h && !status_target(&h, NULL, 1) && status_target(&h, NULL, 0) == &actors[0]);
        actors[0] = saved_hover_target; actor_count = saved_hover_count;
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
