// CViewPort：SDL 每幀全重畫取代 DirectDraw 背景搬移／髒矩形。
// 此檔由 main.c 引入，所有繪圖仍使用同一資源快取與 renderer。
typedef struct {
    SDL_Point pos, base, rem, center, last_actor, target;
    SDL_Rect world;
    int w, h, area, ref_x, ref_y, dead_x, dead_y, step, pan, last_distance, follow;
} ViewPort;
static ViewPort viewport;
static SDL_Point viewport_screen_grid(const ViewPort *v, int x, int y);
static SDL_Point viewport_grid_screen(const ViewPort *v, int x, int y);
// 0x4a1250：世界64分格位置轉畫面像素。
static SDL_Point viewport_world_screen(const ViewPort *v, int x, int y) {
    SDL_Point p = viewport_grid_screen(v, x / 64, y / 64);
    return (SDL_Point){p.x + 32 + (x % 64 - y % 64) / 2, p.y + (x % 64 + y % 64) / 4};
}


// 像素 → 交錯格：原版邊線相等留在原格，除法向零（座標快取的依賴）。
// 0x4ced00（共用座標工具依賴）。
static SDL_Point viewport_stagger(int x, int y) {
    int cx = x / 32, cy = y / 16, rx = x % 32, ry = (y % 16) * 2;
    int row;
    if (cx % 2 == 0) row = cy + (ry > ((cy & 1) ? 32 - rx : rx));
    else row = cy + (ry > ((cy & 1) ? rx : 32 - rx));
    return (SDL_Point){cx / 2 + ((cx & 1) && (row & 1)), row};
}

static void viewport_cache(ViewPort *v, const Scene *s, int odd_shift) {
    int x = v->pos.x, y = v->pos.y, negative = x < 0 || y < 0;
    if (negative) { x += 512; y += 256; }
    SDL_Point p = viewport_stagger(x, y);
    v->base = viewport_point_grid(p, &s->w, s->h / 2);
    v->rem = (SDL_Point){x - p.x * 64, y - (p.y / 2) * 32};
    if (!(p.y & 1)) v->rem.y += 16;
    else if (odd_shift) v->rem.x += 32;
    if (negative) v->base.x -= 16;
    v->center = viewport_center_grid(v->base, v->w, v->h);
}

// 0x45b900：方向只接受 1/3/5/7；-1 只限制邊界與重算快取。
static void viewport_scroll(ViewPort *v, const Scene *s, int direction) {
    if (!s || !s->cells) return;
    switch (direction) {
    case 1: v->pos.y -= v->step; break;
    case 3: v->pos.x -= v->step; break;
    case 5: v->pos.y += v->step; break;
    case 7: v->pos.x += v->step; break;
    }
    v->pos = viewport_clamp(v->pos, v->w, v->h, v->world);
    // 原版這個入口沒有奇數列的 X+32／負座標 base.x-16 補正。
    int x = v->pos.x, y = v->pos.y;
    if (x < 0 || y < 0) { x += 512; y += 256; }
    SDL_Point p = viewport_stagger(x, y);
    v->base = viewport_point_grid(p, &s->w, s->h / 2);
    v->rem = (SDL_Point){x - p.x * 64, y - (p.y / 2) * 32 + (!(p.y & 1) ? 16 : 0)};
    v->center = viewport_center_grid(v->base, v->w, v->h);
}

// 0x45ac40：Y-16 只用於比較；範圍內仍保留傳入 Y。
static void viewport_set(ViewPort *v, const Scene *s, int x, int y) {
    v->pos = (SDL_Point){x, y};
    if (x < v->world.x) v->pos.x = v->world.x;
    else if (x > v->world.x + v->world.w - v->w) v->pos.x = v->world.x + v->world.w - v->w;
    if (y - 16 < v->world.y) v->pos.y = v->world.y;
    else if (y - 16 > v->world.y + v->world.h - v->h) v->pos.y = v->world.y + v->world.h - v->h;
    viewport_scroll(v, s, -1);
}

// 0x45e820：表面由 main.c canvas 管理，重設中心、死區與像素限制。
static void viewport_resize(ViewPort *v, const Scene *s, int w, int h) {
    v->w = w; v->h = h; v->area = w * h;
    v->ref_x = w / 2; v->ref_y = h / 2;
    v->dead_x = (w - 50) / 2; v->dead_y = (h - 50) / 2;
    v->pos = viewport_clamp(v->pos, w, h, v->world);
    if (s && s->cells) viewport_cache(v, s, 1);
}

// 0x45ad30：視窗設位／尺寸後重建快取。
static void viewport_window(ViewPort *v, const Scene *s, int x, int y, int w, int h) {
    v->pos = (SDL_Point){x, y};
    viewport_resize(v, s, w, h);
}

// 原版格／世界點的共用投影；透過已接入的兩個小地圖換算計算原點。
static SDL_Point viewport_project(const Scene *s, int x64, int y64) {
    SDL_Point zero = viewport_grid_point(0, 0, s->w, s->h / 2);
    // 正向換算的 Y+2 對應磁磚的下緣；世界腳底原點在上緣。
    return (SDL_Point){(x64 / 64 - y64 / 64) * 32 + 32 + (x64 % 64 - y64 % 64) / 2 + (s->w / 2 + s->h / 2 - 1) * 32,
                       (x64 / 64 + y64 / 64) * 16 + (x64 % 64 + y64 % 64) / 4 + (zero.y - 2) * 16};
}

static void viewport_grid_clear(void);

// 進場的鏡頭部分；小地圖視窗／比例與載入仍由另一類別負責。
static void viewport_enter(ViewPort *v, const Scene *s, const Actor *hero) {
    viewport_grid_clear();
    memset(v, 0, sizeof *v);
    v->world = (SDL_Rect){s->view_x, s->view_y, s->view_w, s->view_h};
    if (s->w < 33) { v->world.x -= 200; v->world.y -= 200; v->world.w += 200; v->world.h += 200; }
    int half = s->w / 2 + s->h / 2;
    if (s->w != 64 && s->view_x == half * 16 && s->view_y == half * 8 &&
        s->view_w == half * 32 && s->view_h == half * 16) {
        v->world.x += 128; v->world.w -= 224;
    }
    v->follow = 1; v->last_actor = (SDL_Point){-1, -1}; v->last_distance = -1;
    viewport_window(v, s, v->pos.x, v->pos.y, vw, vh);
    (void)hero;
    viewport_set(v, s, v->world.x + 1400, v->world.y + 500);
}

// 0x45c390：靜止超死區補2；移動依原方向及速度的3/4追趕。
// local_28 只在「靜止」分支被快取重用，不得帶入「移動」分支改寫方向。
static void viewport_follow(ViewPort *v, const Scene *s, const Actor *a) {
    SDL_Point actor = {(int)(a->x * 64), (int)(a->y * 64)};
    SDL_Point p = viewport_world_screen(v, actor.x, actor.y);
    int dx = p.x - v->ref_x, dy = p.y - v->ref_y;
    int x = abs(dx), y = abs(dy), direction = a->dir;
    if (actor.x == v->last_actor.x && actor.y == v->last_actor.y) {
        if (x && x > v->dead_x) {
            v->step = x - v->dead_x + 2;
            v->pos.x += dx < 0 ? -v->step : v->step;
            viewport_scroll(v, s, -1); viewport_cache(v, s, 1);
        }
        if (y && y > v->dead_y) {
            v->step = y - v->dead_y + 2;
            v->pos.y += dy < 0 ? -v->step : v->step;
            viewport_scroll(v, s, -1); viewport_cache(v, s, 1);
        }
        return;
    }
    int speed = actor_speed(a); speed -= speed / 4;
    if (x) {
        if (x > v->dead_x) x = x - v->dead_x + 2;
        else if (x >= speed) {
            x = speed;
            if (((unsigned)(direction - 3) < 3) ? dx >= 0 : dx < 1) x = 0;
        }
        v->step = x; v->pos.x += dx < 0 ? -x : x;
        viewport_scroll(v, s, -1); viewport_cache(v, s, 1);

    }
    if (y) {
        if (y > v->dead_y) y = y - v->dead_y + 2;
        else if (y >= speed) {
            y = speed;
            if (((unsigned)(direction - 1) < 3) ? dy >= 0 : dy < 1) y = 0;
        }
        v->step = y; v->pos.y += dy < 0 ? -y : y;
        viewport_scroll(v, s, -1); viewport_cache(v, s, 1);
    }
    v->last_actor = actor;
}

// 0x45bc50：平方距離直接比較步長；震動是累積捲動，每5拍才移動。
static void viewport_tick(ViewPort *v, const Scene *s, const Actor *a) {
    if (v->pan) {
        int dx = v->target.x - v->pos.x, dy = v->target.y - v->pos.y;
        int d = dx * dx + dy * dy;
        SDL_Point p = v->target;
        if (d < v->pan || d == v->last_distance) v->pan = 0;
        else {
            v->last_distance = d;
            float length = sqrtf((float)d);
            p = (SDL_Point){(int)(v->pos.x + dx * v->pan / length), (int)(v->pos.y + dy * v->pan / length)};
        }
        viewport_set(v, s, p.x, p.y);
    }
    if (shake_left > 0) {
        if (shake_left % 5 == 0) {
            v->step = (int)((v->w / 640.0f) * 25 * shake_left / shake_total);
            viewport_scroll(v, s, (shake_left / 5 & 1) ? 5 : 1);
            viewport_cache(v, s, 1);
        }
        shake_left--;
    } else if (v->follow) viewport_follow(v, s, a);
}

typedef struct { int kind, index, depth, x64, y64; SDL_Rect rect; } ViewItem;
typedef struct { ViewItem *items; int n, cap; } ViewList;
static ViewList view_buildings, view_dynamic;

// 0x45aad0：降序，相等時新項在前；繪圖反向走，所以相等仍按加入順序畫。
static void viewport_insert(ViewList *l, ViewItem item, SDL_Rect screen) {
    if (!SDL_HasIntersection(&screen, &item.rect)) return;
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 64;
        ViewItem *p = realloc(l->items, (size_t)cap * sizeof *p);
        if (!p) { fprintf(stderr, "viewport list allocation failed\n"); exit(1); }
        l->items = p; l->cap = cap;
    }
    int i = 0;
    while (i < l->n && l->items[i].depth > item.depth) i++;
    memmove(l->items + i + 1, l->items + i, (size_t)(l->n - i) * sizeof item);
    l->items[i] = item; l->n++;
}

// 0x45b720：可見物件矩形相交查詢。
static int viewport_intersects(const ViewList *l, SDL_Rect r) {
    for (int i = l->n - 1; i >= 0; i--) if (SDL_HasIntersection(&l->items[i].rect, &r)) return 1;
    return 0;
}

static void viewport_union_frame(SDL_Rect *rect, const Anim *a, int seq, int frame, int x, int y) {
    if (!a || seq < 0 || seq >= a->count || !a->seqs[seq].count) return;
    const Frame *f = &a->seqs[seq].frames[frame % a->seqs[seq].count];
    const Sprite *im = image(f->ref);
    if (!im) return;
    SDL_Rect r = {x - f->dx, y - f->dy, im->w, im->h};
    if (rect->w) SDL_UnionRect(rect, &r, rect); else *rect = r;
}

static SDL_Rect viewport_actor_rect(const Actor *a, int sx, int sy) {
    SDL_Rect r = {0};
    int seq = actor_seq(a), frame = actor_frame(a, seq);
    for (int i = 0; a->role && i < a->role->part_count; i++) {
        viewport_union_frame(&r, a->role->body[i], seq, frame, sx, sy - a->z);
        viewport_union_frame(&r, a->role->shadow[i], seq, frame, sx, sy);
    }
    return r;
}

static SDL_Rect viewport_building_rect(const Object *o, int sx, int sy, Uint32 tick) {
    SDL_Rect r = {0};
    for (int p = 0; o->m && p < o->m->part_count; p++) {
        const Part *pt = &o->m->parts[p];
        const Anim *a = pt->anim ? anim_get(pt->gid) : NULL;
        if (a && a->count) viewport_union_frame(&r, a, 0, frame_at(&a->seqs[0], tick), sx + pt->dx, sy + pt->dy);
        else if (!pt->anim) {
            const Sprite *im = image(pt->gid);
            if (im) {
                SDL_Rect part = {sx + pt->dx, sy + pt->dy, im->w, im->h};
                if (r.w) SDL_UnionRect(&r, &part, &r); else r = part;
            }
        }
    }
    return r;
}

static void viewport_building_draw(const Object *o, int sx, int sy, int pass, Uint32 tick) {
    const Model *m = o->m;
    if (!m) return;
    int start = pass == 0 ? 0 : pass == 1 ? m->bottom : m->part_count - m->top;
    int end = pass == 0 ? m->bottom : pass == 1 ? m->part_count - m->top : m->part_count;
    for (int p = start; p < end && p < m->part_count; p++) {
        if (p < 0) continue;
        const Part *pt = &m->parts[p];
        const Anim *a = pt->anim ? anim_get(pt->gid) : NULL;
        if (a && a->count) draw_frame(a, 0, frame_at(&a->seqs[0], tick), sx + pt->dx, sy + pt->dy, 0);
        else if (!pt->anim) draw(image(pt->gid), sx + pt->dx, sy + pt->dy);
    }
}

// 0x45b830：只將存在且未被隱藏的格子物件加入可見排序。
// view 是原版 CScene 格子 +0x13；初始化來源的限制記於 scene_load。
static int viewport_cell_visible(const Scene *s, int x64, int y64) {
    int x = x64 / 64, y = y64 / 64;
    return s->cells && x >= 0 && y >= 0 && x < s->w && y < s->h && s->cells[y * s->w + x].view >= 2;
}
static void viewport_visible_insert(ViewList *l, const Scene *s, ViewItem item, SDL_Rect screen) {
    if (viewport_cell_visible(s, item.x64, item.y64)) viewport_insert(l, item, screen);
}

// 0x4a1090；CRHOMB 像素查格：查表邊線相等不修正，負餘數補正後商仍向零截斷。
static SDL_Point viewport_screen_grid(const ViewPort *v, int x, int y) {
    x += v->rem.x; y += v->rem.y;
    int rx = x % 64, ry = y % 32;
    if (rx < 0) rx += 64;
    if (ry < 0) ry += 32;
    SDL_Point p = {v->base.x + y / 32 + x / 64, v->base.y - x / 64 + y / 32};
    int edge = ry < 16 ? (ry + 1) * 2 : (32 - ry) * 2;
    if (edge < abs(rx - 32)) {
        if (rx < 33) { if (ry < 17) p.x--; else p.y++; }
        else { if (ry < 17) p.y--; else p.x++; }
    }
    return p;
}
// 0x4a1030：格點轉畫面像素。
static SDL_Point viewport_grid_screen(const ViewPort *v, int x, int y) {
    x -= v->base.x; y -= v->base.y;
    return (SDL_Point){(x - y) * 32 - v->rem.x, (x + y) * 16 - v->rem.y};
}

// 地表主迴圈：視窗起格 X-1，兩列交錯，各列寬/64+3，高/32+3。
// shortcut: 缺格預設圖與除錯格圖等 CResMng 預設圖資初始化，現在保留清屏底色。
static void viewport_terrain(const ViewPort *v, Scene *s) {
    SDL_Point start = viewport_screen_grid(v, 0, 0);
    start.x--;
    SDL_Point pixel = viewport_grid_screen(v, start.x, start.y);
    for (int row = 0; row < v->h / 32 + 3; row++)
        for (int odd = 0; odd < 2; odd++)
            for (int col = 0; col < v->w / 64 + 3; col++) {
                int x = start.x + row + odd + col, y = start.y + row - col;
                int sx = pixel.x + odd * 32 + col * 64, sy = pixel.y + row * 32 + odd * 16;
                if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
                const Cell *c = &s->cells[y * s->w + x];
                if (c->ground != 0xffff) draw(tile(&s->tiles, c->ground), sx, sy);
                if (c->over != 0xffff) draw(tile(&s->tiles, c->over), sx, sy);
            }
}

// 建築遮擋平面；物件矩形與位置均是原版的當幀矩形。
static int viewport_blocks(const Object *o, const ViewItem *item) {
    const Model *m = o->m;
    int bx = o->x * 64 + m->block_x * 64, by = o->y * 64 + m->block_y * 64;
    if (m->block_x < 1 && m->block_y) return item->depth < bx + by && item->x64 < bx;
    if (m->block_y < 1 && m->block_x) return item->depth < bx + by && item->y64 < by;
    return 0;
}
static int viewport_occlusion_start(const ViewList *l, const Scene *s, const ViewItem *item, SDL_Rect clip) {
    int i;
    int found = 0;
    for (i = l->n - 1; i >= 0; i--) {
        const ViewItem *b = &l->items[i];
        if (item->depth < b->depth) break;
        if (viewport_blocks(&s->objects[b->index], item) && SDL_HasIntersection(&b->rect, &clip)) {
            found = 1; break;
        }
    }
    int x = item->x64 / 64, y = item->y64 / 64;
    if (x >= 0 && y >= 0 && x < s->w && y < s->h) {
        const Cell *c = &s->cells[y * s->w + x];
        if ((c->flags & 0x80) && (c->flags & 7) == 2 && c->building >= 0 && c->building < s->object_count) {
            const Object *o = &s->objects[c->building];
            int depth = (o->x + o->y) * 64;
            if ((found && l->items[i].depth <= depth) || depth <= item->depth) {
                for (i = l->n - 1; i >= 0 && l->items[i].index != c->building; i--) {}
            }
        }
    }
    return i;
}
// 0x45ec50：裁切後面積小於20001；從遮擋起點起依序重畫所有較近的前景。
// SDL 直接在裁切區重畫，取代透明暫存表面與尾端連續去重名單。
static void viewport_occlude(const ViewPort *v, const Scene *s, const ViewItem *item, Uint32 tick) {
    SDL_Rect clip, old_clip, screen = {0, 0, v->w, v->h};
    if (!SDL_IntersectRect(&item->rect, &screen, &clip) || (int64_t)clip.w * clip.h >= 20001) return;
    if (!viewport_intersects(&view_buildings, clip)) return;
    int first = viewport_occlusion_start(&view_buildings, s, item, clip);
    SDL_bool clipped = SDL_RenderIsClipEnabled(ren);
    SDL_RenderGetClipRect(ren, &old_clip);
    SDL_RenderSetClipRect(ren, &clip);
    for (int i = first; i >= 0; i--) {
        const ViewItem *b = &view_buildings.items[i];
        const Object *o = &s->objects[b->index];
        SDL_Point p = viewport_grid_screen(v, o->x, o->y);
        for (int pass = 1; pass < 3; pass++) viewport_building_draw(o, p.x + 32, p.y + 16, pass, tick);
    }
    SDL_RenderSetClipRect(ren, clipped ? &old_clip : NULL);
}

static void viewport_units(const ViewPort *v, const Scene *s, int ground_pass, Uint32 tick) {
    for (int i = 0; i < unit_n; i++) {
        const MUnit *u = &units[i];
        if (u->dead || (u->cls == U_FOOT || u->cls == U_SPLASH) != ground_pass ||
            !viewport_cell_visible(s, u->x, u->y)) continue;
        SDL_Point p = viewport_world_screen(v, u->x, u->y);
        unit_draw(u, p.x, p.y);
        if (!ground_pass && u->spr.a && u->spr.seq >= 0 && u->spr.seq < u->spr.a->count) {
            SDL_Rect rect = {0};
            viewport_union_frame(&rect, u->spr.a, u->spr.seq, u->spr.cur, p.x, p.y - u->h);
            ViewItem item = {2, i, u->x + u->y, u->x, u->y, rect};
            viewport_occlude(v, s, &item, tick);
        }
    }
}

// 0x45ae70：底層 → 一般靜態物件 → 中層 → 上層 → 特殊靜態物件。
static void viewport_layers(const ViewPort *v, const Scene *s, Uint32 tick) {
    for (int pass = 0; pass < 3; pass++) {
        for (int i = view_buildings.n - 1; i >= 0; i--) {
            const Object *o = &s->objects[view_buildings.items[i].index];
            SDL_Point p = viewport_grid_screen(v, o->x, o->y);
            viewport_building_draw(o, p.x + 32, p.y + 16, pass, tick);
        }
        if (!pass) {
            for (int i = 0; i < ground_n; i++) {
                if (!viewport_cell_visible(s, (int)(ground[i].x * 64), (int)(ground[i].y * 64))) continue;
                const Sprite *g = ground_image(ground[i].it.ground);
                SDL_Point p = viewport_world_screen(v, (int)(ground[i].x * 64), (int)(ground[i].y * 64));
                if (g) draw(g, p.x - g->w / 2, p.y - g->h / 2);
            }
            viewport_units(v, s, 1, tick);
        }
    }
}

// 動態角色重建矩形、排序後畫，套用局部遮擋；飛行物/其他單位最後畫。
// 關聯模型 type2/3 暫加128；只改當幀繪圖座標，角色世界座標自然保留。
// shortcut: 關聯模型欄位來源等 CCharactor 模型關聯載入，目前一般角色為0。
static void viewport_dynamic(const ViewPort *v, const Scene *s, Uint32 tick) {
    SDL_Rect screen = {0, 0, v->w, v->h};
    view_dynamic.n = 0;
    for (int i = 0; i < actor_count; i++) {
        const Actor *a = &actors[i];
        int shift = a->view_link_type == 2 || a->view_link_type == 3 ? 128 : 0;
        int x = (int)(a->x * 64) + shift, y = (int)(a->y * 64) + shift;
        SDL_Point p = viewport_world_screen(v, x, y);
        SDL_Rect rect = viewport_actor_rect(a, p.x, p.y);
        viewport_visible_insert(&view_dynamic, s, (ViewItem){1, i, x + y, x, y, rect}, screen);
    }
    for (int i = view_dynamic.n - 1; i >= 0; i--) {
        const ViewItem *item = &view_dynamic.items[i];
        SDL_Point p = viewport_world_screen(v, item->x64, item->y64);
        actor_draw(&actors[item->index], p.x, p.y, tick);
        viewport_occlude(v, s, item, tick);
    }
    viewport_units(v, s, 0, tick);
}

typedef struct { const Object *objects; int count, w, h; int *heads, *next; } ViewGrid;
static ViewGrid view_grid;
static void viewport_grid_clear(void) {
    free(view_grid.heads); free(view_grid.next);
    memset(&view_grid, 0, sizeof view_grid);
}
static void viewport_collect(ViewPort *v, const Scene *s, Uint32 tick) {
    if (view_grid.objects != s->objects || view_grid.count != s->object_count || view_grid.w != s->w || view_grid.h != s->h) {
        viewport_grid_clear();
        view_grid.objects = s->objects; view_grid.count = s->object_count;
        view_grid.w = s->w; view_grid.h = s->h;
        view_grid.heads = malloc((size_t)s->w * s->h * sizeof(int));
        view_grid.next = malloc((size_t)(s->object_count + 1) * sizeof(int));
        if (!view_grid.heads || !view_grid.next) { fprintf(stderr, "viewport grid allocation failed\n"); exit(1); }
        for (int i = 0; i < s->w * s->h; i++) view_grid.heads[i] = -1;
        for (int i = s->object_count - 1; i >= 0; i--) {
            const Object *o = &s->objects[i]; view_grid.next[i] = -1;
            if (o->x < 0 || o->y < 0 || o->x >= s->w || o->y >= s->h) continue;
            int k = o->y * s->w + o->x;
            view_grid.next[i] = view_grid.heads[k]; view_grid.heads[k] = i;
        }
    }
    view_buildings.n = 0;
    SDL_Rect screen = {0, 0, v->w, v->h};
    // 原版掃格順序決定同深度物件的加入順序；SDL 每幀重建，不保留背景世代。
    for (int row = -8; row < v->h / 32 + 8; row++)
        for (int odd = 0; odd < 2; odd++)
            for (int col = -8; col < v->w / 64 + 8; col++) {
                int x = v->base.x + col + odd + row, y = v->base.y - col + row;
                if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
                for (int i = view_grid.heads[y * s->w + x]; i >= 0; i = view_grid.next[i]) {
                    const Object *o = &s->objects[i];
                    SDL_Point p = viewport_grid_screen(v, o->x, o->y);
                    SDL_Rect rect = viewport_building_rect(o, p.x + 32, p.y + 16, tick);
                    viewport_insert(&view_buildings, (ViewItem){0, i, (o->x + o->y) * 64, o->x * 64, o->y * 64, rect}, screen);
                }
            }
}

// 0x45f200：重建當幀可見名單，地表、分層背景、光照、動態物件；小地圖另等 CMapDialog。
static void viewport_draw(ViewPort *v, Scene *s, Uint32 tick) {
    if (v->w != vw || v->h != vh) viewport_resize(v, s, vw, vh);
    viewport_collect(v, s, tick);
    viewport_terrain(v, s);
    viewport_layers(v, s, tick);
    SDL_Point origin = viewport_grid_screen(v, 0, 0);
    light_draw(world_x(s, 0, 0) - origin.x, world_y(s, 0, 0) - origin.y);
    viewport_dynamic(v, s, tick);
    weather_draw(v->pos.x, v->pos.y);
}
