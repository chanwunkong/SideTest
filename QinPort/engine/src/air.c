// CAirBeastie：場景 ONT 的 BeastieMdl 移動屬性非零小動物。
// 由 main.c 引入；Scene/NpcSpec、Spr 與 SDL 資源快取承接 CObj/CSprGroup。
typedef struct {
    const NpcSpec *spec;
    const RoleDef *def;
    Spr spr;
    int gx, gy, x, y, dir, state, steps, tx, ty, speed, path, wait;
    unsigned pulse;
    float fx, fy, dx, dy;
} AirBeastie;
static AirBeastie *air;
static int air_n;
static void bird_clear(void);

// 0x480b40：綁模型、方向5、休息態、低速；精靈與一般物件初始化不複製ABI。
static void air_model(AirBeastie *a, const RoleDef *d) {
    a->def = d;
    spr_load(&a->spr, d->anim);
    a->dir = 5;
    a->state = 0;
    a->speed = d->speed;
}
// 0x480f40：綁場景擺放記錄，格中心是格×64+32。
static void air_place(AirBeastie *a, const NpcSpec *n) {
    a->spec = n;
    a->gx = n->x; a->gy = n->y;
    a->x = n->x * 64 + 32; a->y = n->y * 64 + 32;
}
static void air_enter(const Scene *s) {
    bird_clear();
    free(air);
    air = calloc((size_t)s->npc_count + 1, sizeof *air);
    air_n = 0;
    if (!air) return;
    for (int i = 0; i < s->npc_count; i++) {
        const NpcSpec *n = &s->npcs[i];
        const RoleDef *d = roledef_find(n->tmpl);
        if (!d || !d->beast || !d->air || d->id > 1499) continue;
        AirBeastie *a = &air[air_n++];
        air_place(a, n);
        air_model(a, d);
    }
}
// 0x481420：格目標轉世界中心，以截斷的歐氏距離/速度算拍數；不吸附終點。
static int air_move(AirBeastie *a, int x, int y) {
    if (a->gx == x && a->gy == y) {
        a->state = 0; a->speed = a->def->speed;
        return 0;
    }
    if (a->state != 1 && a->state != 2) a->state = 1;
    a->tx = x; a->ty = y;
    int wx = x * 64 + 32, wy = y * 64 + 32;
    a->steps = a->speed ? dist_of(a->x, a->y, wx, wy) / a->speed : 0;
    a->dir = dir_of(a->x, a->y, wx, wy, 8);
    a->fx = a->x; a->fy = a->y;
    // shortcut: 原版距離小於速度會除以0；這裡留0拍且不產生NaN，資源合法性驗證後可還原浮點例外行為。
    a->dx = a->steps ? (float)(wx - a->x) / a->steps : 0;
    a->dy = a->steps ? (float)(wy - a->y) / a->steps : 0;
    return 1;
}
// 0x481650：反應1逃離、反應2追主角；路線點以+3,+3的曼哈頓距離選最遠／最近。
static void air_run(AirBeastie *a, const Actor *t) {
    const NpcSpec *n = a->spec;
    a->state = 2; a->speed = a->def->fight_speed;
    int x = 0, y = 0;
    if (a->def->reaction == 1 && n->logic == 1) {
        x = a->gx; y = a->gy;
        // OmUtil GetFleePos = WhatDir8(自身,威脅)後MoveDirForSpeed；呼叫點機碼確認隱藏參數為5。
        move_dir(&x, &y, dir_of((int)t->x, (int)t->y, x, y, 8), 5);
    } else if (a->def->reaction == 1 || a->def->reaction == 2) {
        if (a->def->reaction == 2 && n->logic == 1) return; // 原版只印不支持，已切到跑態／高速
        // shortcut: 損壞或空路線不讀越界指標，保持跑態；載入器驗證路徑後可移除此保護。
        if (!n->path_n) return;
        int best = 0, distance = a->def->reaction == 1 ? 0 : 50000;
        for (int i = 0; i < n->path_n; i++) {
            int d = abs((int)t->x - (n->path[i][0] + 3)) + abs((int)t->y - (n->path[i][1] + 3));
            if ((a->def->reaction == 1 && d > distance) || (a->def->reaction == 2 && d < distance)) { best = i; distance = d; }
        }
        x = n->path[best][0]; y = n->path[best][1];
    }
    if (air_move(a, x, y) && a->def->snd_run != 0xffff && rand() % 3 == 1)
        audio_sfx(a->def->snd_run); // shortcut: SDL現有非位置音效；位置播放接入後傳入a->gx/gy。
}
static void air_animate(AirBeastie *a);
// 0x480f60：更新先經基底恆真與動畫虛擬更新，再處理閒逛、移動與每80拍反應。
static void air_tick_one(AirBeastie *a, const Actor *hero) {
    air_animate(a);
    if (!a->state) {
        if (a->def->snd_idle != 0xffff && rand() % 200 == 1) audio_sfx(a->def->snd_idle);
        const NpcSpec *n = a->spec;
        if (a->def->reaction != 2 && (n->logic == 1 || n->logic == 2)) {
            if (a->wait > 0) a->wait--;
            else if (n->rest == 4) a->wait = 10000;
            else {
                if (n->rest) { int r = n->rest * 200; a->wait = rand() % (r + 10) + r; }
                int x, y;
                if (n->logic == 1) {
                    x = a->gx - 5 + rand() % 10; y = a->gy - 5 + rand() % 10;
                    x = x > n->rx + n->rw ? n->rx + n->rw : x < n->rx ? n->rx : x;
                    y = y > n->ry + n->rh ? n->ry + n->rh : y < n->ry ? n->ry : y;
                } else {
                    if (!n->path_n) goto pulse; // 空路線保護同air_run
                    x = n->path[a->path][0]; y = n->path[a->path][1];
                    if (++a->path >= n->path_n) a->path = 0;
                }
                air_move(a, x, y);
            }
        }
    } else if (a->steps < 1) {
        a->state = 0; a->speed = a->def->speed;
    } else {
        a->fx += a->dx; a->fy += a->dy; a->steps--;
        a->x = (int)a->fx; a->y = (int)a->fy;
        a->gx = a->x / 64; a->gy = a->y / 64; // 負座標向零除法
    }
pulse:
    if (a->pulse++ % 80 == 1) {
        Actor *t = NULL;
        if (a->def->reaction == 1) {
            if (a->state == 2) return;
            t = near_other(a->x, a->y, 512, -1);
        } else if (a->def->reaction == 2) t = (Actor *)hero;
        if (t) air_run(a, t);
    }
}
static void air_tick(const Actor *hero) {
    for (int i = 0; i < air_n; i++) air_tick_one(&air[i], hero);
}
// 0x480cf0：休息序列方向+8，走／跑共用方向序列；每次更新都前進一拍。
static void air_animate(AirBeastie *a) {
    if (a->state == 0) spr_seq(&a->spr, a->dir + 8);
    else if (a->state == 1 || a->state == 2) spr_seq(&a->spr, a->dir);
    spr_next(&a->spr);
}
// 0x480be0：繪製先虛擬更新且忽略回值；普通、調色、半透明皆回成功。
// 0x49ad40（CSurBeastie 畫；CAirBeastie 0x480be0 同）：顯示方式 0 普通、1 調色、其他半透明
static void beast_draw_one(const ViewPort *v, Spr *spr, const RoleDef *def, int wx, int wy) {
    const Frame *f = spr_frame(spr);
    if (!f) return;
    const Sprite *s = image(f->ref);
    if (!s || !s->tex) return;
    SDL_Point p = viewport_world_screen(v, wx, wy);
    int alpha = def->display == 0 || def->display == 1 ? 255 : 128;
    // shortcut: 調色分支以現有light色係數乘RGB，無原版逐像素palette；精靈色表支援後替換。
    if (def->display == 1 && lt.mode >= 0) {
        int b = lt.bright < 0 ? 0 : lt.bright > 18 ? 18 : lt.bright;
        int c[3]; for (int k = 0; k < 3; k++) { c[k] = (int)(lt.f[b][k] * 255); if (c[k] > 255) c[k] = 255; if (c[k] < 0) c[k] = 0; }
        SDL_SetTextureColorMod(s->tex, c[0], c[1], c[2]);
    }
    SDL_SetTextureAlphaMod(s->tex, alpha);
    draw(s, p.x - f->dx, p.y - f->dy);
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s->tex, 255);
}
static void sur_draw(const ViewPort *v);
static void air_draw(const ViewPort *v) {
    for (int i = 0; i < air_n; i++) beast_draw_one(v, &air[i].spr, air[i].def, air[i].x, air[i].y);
    sur_draw(v);
}

// CSurBeastie：地面小動物（老鼠、雞）。照 CAirBeastie 的模式，但走格子（能不能走看格子旗標）、會被人踩死、
// 看到人會逃。狀態 0 休息、1 走、2 逃、3 被踩死（500 拍後移除）。
typedef struct {
    const NpcSpec *spec;
    const RoleDef *def;
    Spr spr;
    int gx, gy, x, y, dir, state, tx, ty, speed, wait, count, path_i;
} SurBeastie;
static SurBeastie *sur;
static int sur_n;
// 0x49ac70：綁模型、方向5、速度 = 低速（0x49b620）。shortcut: 原版 +0x66 層號由場景擺放記錄給，這裡一律地面層 1
static void sur_init(SurBeastie *b, const NpcSpec *n, const RoleDef *d) {
    *b = (SurBeastie){.spec = n, .def = d, .dir = 5, .speed = d->speed, .gx = n->x, .gy = n->y};  // 0x49b140 放進場景
    b->x = n->x * 64 + 32; b->y = n->y * 64 + 32;
    spr_load(&b->spr, d->anim);
}
static void sur_flee(SurBeastie *b, const Actor *t);
// 0x49b620：停下來（逃跑中又有人在 0x200 內就繼續逃），速度改回低速
static void sur_reset(SurBeastie *b) {
    if (b->state == 2) {
        Actor *t = near_other(b->x, b->y, 0x200, -1);
        if (t) { sur_flee(b, t); return; }
    }
    b->state = 0;
    b->speed = b->def->speed;
}
// 0x49b6e0：被人嚇到，改逃跑態（高速）；logic 1 往反方向跑 5 格，logic 2 跑到離威脅最遠的路點
static void sur_flee(SurBeastie *b, const Actor *t) {
    const NpcSpec *n = b->spec;
    b->state = 2;
    b->speed = b->def->fight_speed;
    if (n->logic == 1) {
        int x = b->gx, y = b->gy;
        move_dir(&x, &y, dir_of((int)t->x, (int)t->y, x, y, 8), 5);  // GetFleePos
        b->tx = x; b->ty = y;
        return;
    }
    int best = 0, far = 0;
    for (int i = 0; i < n->path_n; i++) {
        int d = abs((int)t->y - n->path[i][1]) + abs((int)t->x - n->path[i][0]);
        if (d > far) { far = d; best = i; }
    }
    if (!n->path_n) return;  // shortcut: 空路線不讀越界，保持原目標
    b->tx = n->path[best][0]; b->ty = n->path[best][1];
}
// 0x49af00：休息態每拍：1/500 叫一聲；logic 1／2 的休息倒數完就挑下一個目標，轉走態
static void sur_idle(SurBeastie *b) {
    const NpcSpec *n = b->spec;
    if (b->def->snd_idle != 0xffff && rand() % 500 == 10) audio_sfx(b->def->snd_idle);  // shortcut: 非位置音效
    if (n->logic != 1 && n->logic != 2) return;
    if (b->wait > 0) { b->wait--; return; }
    if (n->rest == 4) { b->wait = 10000; return; }
    if (n->rest) b->wait = rand() % (n->rest * 200 + 10);
    int x, y;
    if (n->logic == 1) {
        x = b->gx - 5 + rand() % 10; y = rand() % 10 + b->gy - 5;
        x = x > n->rx + n->rw ? n->rx + n->rw : x < n->rx ? n->rx : x;
        y = y > n->ry + n->rh ? n->ry + n->rh : y < n->ry ? n->ry : y;
    } else {
        if (!n->path_n) return;  // shortcut: 空路線不讀越界
        x = n->path[b->path_i][0]; y = n->path[b->path_i][1];
        if (++b->path_i >= n->path_n) b->path_i = 0;
    }
    if (b->state != 2 && b->state != 1) b->state = 1;
    b->tx = x; b->ty = y;
}
// 0x49b2d0：走態每拍：到了就休息（逃跑中仍有人在旁就再逃）；往目標方向探半格，能走就走 speed，不能走重挑目標（最多 8 次）
static void sur_walk(SurBeastie *b) {
    for (int tries = 0;; tries++) {
        if (b->tx == b->gx && b->ty == b->gy) {
            Actor *t = b->state == 2 ? near_other(b->x, b->y, 0x200, -1) : NULL;
            if (t) sur_flee(b, t);
            else { b->state = 0; b->speed = b->def->speed; }
        }
        b->dir = dir_of(b->gx, b->gy, b->tx, b->ty, 8);
        int px = b->x, py = b->y;
        move_dir(&px, &py, b->dir, 0x20);
        const Cell *c = cell_at(px / 64, py / 64);
        int go;
        if (!c) go = 1;
        else go = c->walk != 0;  // shortcut: 層號固定 1（地面層），只看格子能不能走；層 11／13／15 的水面等判斷等場景給層號再補
        if (go) {
            move_dir(&b->x, &b->y, b->dir, b->speed);
            b->gx = b->x / 64; b->gy = b->y / 64;
            return;
        }
        if (tries > 7) { sur_reset(b); return; }
        int x = b->gx, y = b->gy;
        spread_pt(&x, &y, 5);
        b->tx = x; b->ty = y;
    }
}
// 0x49acc0：換狀態；進入 3（被踩死）叫死亡聲
static void sur_state(SurBeastie *b, int st) {
    if (b->state == st) return;
    b->state = st;
    if (st == 3 && b->def->snd_run != 0xffff) audio_sfx(b->def->snd_run);
}
// 0x49ae60：休息 dir+8、走 dir、逃 dir+16，其餘保持；每拍前進一幀
static void sur_animate(SurBeastie *b) {
    if (b->state == 0) spr_seq(&b->spr, b->dir + 8);
    else if (b->state == 1) spr_seq(&b->spr, b->dir);
    else if (b->state == 2) spr_seq(&b->spr, b->dir + 16);
    spr_next(&b->spr);
}
static int is_npc_grp(const Actor *t) { return (unsigned)(t->grp - 10000) < 50001u; }
// 0x49b160：回 0 = 要移除。被踩死 500 拍後移除；否則走休息／走，每 80 拍看有沒有人靠近（怕人的逃），
// 序列夠多（> 0x18）的，有人在 30 內（每 30 拍一次例外）又不是平常態就被踩死
static int sur_tick_one(SurBeastie *b) {
    sur_animate(b);
    b->count++;
    if (b->state == 3) return b->count <= 500;
    if (b->state == 0) sur_idle(b); else sur_walk(b);
    if (b->def->reaction == 1 && b->state != 2 && b->count % 0x50 == 1)
        for (Actor *t = near_first(b->x, b->y, 0x1c0); t; t = near_next())
            if (is_npc_grp(t)) { sur_flee(b, t); break; }
    if (spr_count(&b->spr) > 0x18 && b->count % 0x1e) {
        Actor *t = near_other(b->x, b->y, 0x1e, -1);
        if (t && is_npc_grp(t) && t->dynamic_state != 1) {
            b->count = 0;
            sur_state(b, 3);
            spr_seq(&b->spr, b->dir + 0x18);
        }
    }
    return 1;
}
static void sur_tick(void) {
    for (int i = 0; i < sur_n;)
        if (sur_tick_one(&sur[i])) i++;
        else sur[i] = sur[--sur_n];
}
static void sur_draw(const ViewPort *v) {
    for (int i = 0; i < sur_n; i++) beast_draw_one(v, &sur[i].spr, sur[i].def, sur[i].x, sur[i].y);
}
static void sur_enter(const Scene *s) {
    free(sur);
    sur = calloc((size_t)s->npc_count + 1, sizeof *sur);
    sur_n = 0;
    for (int i = 0; sur && i < s->npc_count; i++) {
        const NpcSpec *n = &s->npcs[i];
        const RoleDef *d = roledef_find(n->tmpl);
        if (d && d->beast && !d->air && d->id < 1500) sur_init(&sur[sur_n++], n, d);
    }
}

// CBirdFast 使用同一個場景陣列／Spr生命週期，不保留原版CObj的ABI。
typedef struct { Spr spr; int x, y, dir, speed, outside, limit; } FastBird;
static FastBird *birds;
static int bird_n, bird_countdown = -1;
static void bird_clear(void) { free(birds); birds = NULL; bird_n = 0; }
// 0x481a10：固定動畫、隨機方向、速度16、離屏容忍100拍；初始序列仍是0。
static void bird_init(FastBird *b, int gx, int gy) {
    *b = (FastBird){.x = gx * 64 + 32, .y = gy * 64 + 32,
                    .dir = rand() % 8, .speed = 16, .limit = 100};
    spr_load(&b->spr, 0x100001);
    spr_seq(&b->spr, 0);
}
// 0x450b30：建立後加入場景空中物件名單。
static void bird_spawn(int gx, int gy) {
    FastBird *p = realloc(birds, (size_t)(bird_n + 1) * sizeof *birds);
    if (!p) return;
    birds = p; bird_init(&birds[bird_n++], gx, gy);
}
// 0x4819c0：方向序列改變才重設，前進一拍；基底動畫更新恆真。
static void bird_animate(FastBird *b) { spr_seq(&b->spr, b->dir); spr_next(&b->spr); }
static SDL_Rect bird_rect(const FastBird *b, const ViewPort *v) {
    SDL_Point p = viewport_world_screen(v, b->x, b->y);
    return frame_rect(spr_frame(&b->spr), p.x, p.y);  // 0x49d740
}
// 0x481ab0：先用舊範圍判斷；舊離屏計數>100才刪，否則先移動再虛擬動畫更新。
static int bird_tick_one(FastBird *b, int visible) {
    if (visible) b->outside = 0;
    else if (b->outside++ > b->limit) return 0;
    move_dir(&b->x, &b->y, b->dir, b->speed);
    bird_animate(b);
    return 1;
}
// 0x453800：倒數先判舊值，畫面外隨機點；重擲值<11額外從左側放8隻。
// shortcut: 原版設定旗標與場景管理器就緒值尚無對應，以已載入Scene且寬>96啟用；
// 設定／場景管理器資料接入時改回原版兩個條件。週期200已核對Poq.exe資料初值。
static void bird_generate(const Scene *s, const ViewPort *v) {
    if (!s->cells || s->w <= 96) return;
    if (bird_countdown < 0) bird_countdown = rand() % 200;
    if (bird_countdown-- > 0) return;
    bird_countdown = rand() % 200;
    int x = rand() % (v->w + 200) - 100, y = rand() % (v->h + 200) - 100;
    if (x < 0 || x >= v->w || y < 0 || y >= v->h) {
        SDL_Point p = viewport_screen_grid(v, x, y); bird_spawn(p.x, p.y);
    }
    if (bird_countdown < 11) for (int i = 0; i < 8; i++) {
        x = rand() % 100 - 100; y = rand() % (v->h + 200) - 100;
        SDL_Point p = viewport_screen_grid(v, x, y); bird_spawn(p.x, p.y);
    }
}
static void bird_tick(const Scene *s, const ViewPort *v) {
    bird_generate(s, v);
    SDL_Rect screen = {0, 0, v->w, v->h};
    for (int i = 0; i < bird_n;) {
        SDL_Rect r = bird_rect(&birds[i], v);
        if (bird_tick_one(&birds[i], SDL_HasIntersection(&r, &screen))) i++;
        else { memmove(&birds[i], &birds[i + 1], (size_t)(bird_n - i - 1) * sizeof *birds); bird_n--; }
    }
}
static void bird_draw(const ViewPort *v) {
    // shortcut: 沿用SDL普通繪製，未套原版逐像素色表；精靈色表支援接入時補調色分支。
    for (int i = 0; i < bird_n; i++) {
        SDL_Rect r = bird_rect(&birds[i], v);
        const Frame *f = spr_frame(&birds[i].spr);
        if (f) draw(image(f->ref), r.x, r.y);
    }
}
