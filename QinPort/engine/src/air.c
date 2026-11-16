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
static void air_draw(const ViewPort *v) {
    for (int i = 0; i < air_n; i++) {
        AirBeastie *a = &air[i];
        const Frame *f = spr_frame(&a->spr);
        if (!f) continue;
        const Sprite *s = image(f->ref);
        if (!s || !s->tex) continue;
        SDL_Point p = viewport_world_screen(v, a->x, a->y);
        int alpha = a->def->display == 0 || a->def->display == 1 ? 255 : 128;
        // shortcut: 調色分支以現有light色係數乘RGB，無原版逐像素palette；精靈色表支援後替換。
        if (a->def->display == 1 && lt.mode >= 0) {
            int b = lt.bright < 0 ? 0 : lt.bright > 18 ? 18 : lt.bright;
            int c[3]; for (int k = 0; k < 3; k++) { c[k] = (int)(lt.f[b][k] * 255); if (c[k] > 255) c[k] = 255; if (c[k] < 0) c[k] = 0; }
            SDL_SetTextureColorMod(s->tex, c[0], c[1], c[2]);
        }
        SDL_SetTextureAlphaMod(s->tex, alpha);
        draw(s, p.x - f->dx, p.y - f->dy);
        SDL_SetTextureColorMod(s->tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(s->tex, 255);
    }
}
