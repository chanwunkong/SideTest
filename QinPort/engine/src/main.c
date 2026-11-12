// 秦殤原生引擎（第 2 階段）：讀原版資源，畫出場景與主角，主角可以走動。
// 用法：qin [遊戲資料夾] [場景別名] [--640] [--shot 檔名.bmp] [--warp N] [--at X,Y]
// --shot：畫面穩定後存一張圖就結束；--warp：一開始站到第 N 個觸發區（測場景切換用）；--at：站到指定格
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "res.h"
#include "audio.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define HERO_GRAPHIC "1201"  // 主角（遊俠）在 ObjMdl/RoleMdl.txt 的圖形 ID；外觀、動作、速度都照角色表
#define TICK_MS 16           // 原版固定節拍：每 16ms 更新一次遊戲邏輯（Poq.exe 0x449f50）

// 原版速度對照表（Poq.exe 0x5f21d0/0x5f23b0/0x5f22c0）：速度 s → 每 B 拍移動 A + C/10 像素
static const uint8_t SPEED_A[60] = {0,1,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15,16,16,16,17,17,17,18,18,18,19,19,19};
static const uint8_t SPEED_B[60] = {90,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static const uint8_t SPEED_C[60] = {0,0,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,0,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7};

// 畫面方向(0=上，順時針每 45°) → 動畫方向編號。動畫方向從「右」開始逆時針排（手機實測校正）
static const int DIR_MAP[8] = {2, 1, 0, 7, 6, 5, 4, 3};

typedef struct { SDL_Texture *tex; int w, h; } Sprite;

static SDL_Renderer *ren;
static int vw = 800, vh = 600;

// ---- 貼圖快取：key → 貼圖（第一次用到才解碼），失敗記成 w = -1 避免每幀重試 ----
typedef struct { uint32_t key; Sprite s; } Slot;
static Slot *slots;
static uint32_t slot_cap, slot_used;

static Sprite *slot_find(uint32_t key) {
    if (slot_used * 2 >= slot_cap) {
        Slot *old = slots;
        uint32_t old_cap = slot_cap;
        slot_cap = slot_cap ? slot_cap * 2 : 4096;
        slots = calloc(slot_cap, sizeof(Slot));
        slot_used = 0;
        for (uint32_t i = 0; i < old_cap; i++)
            if (old[i].key) *slot_find(old[i].key) = old[i].s;
        free(old);
    }
    uint32_t i = (key * 2654435761u) & (slot_cap - 1);
    while (slots[i].key && slots[i].key != key) i = (i + 1) & (slot_cap - 1);
    if (!slots[i].key) { slots[i].key = key; slot_used++; }
    return &slots[i].s;
}

// 換場景時清空：磁磚 key 只有序號，不同場景的磁磚封包會撞號
static void cache_clear(void) {
    for (uint32_t i = 0; i < slot_cap; i++)
        if (slots[i].s.tex) SDL_DestroyTexture(slots[i].s.tex);
    free(slots);
    slots = NULL;
    slot_cap = slot_used = 0;
}

static const Sprite *upload(Sprite *s, uint32_t *px) {
    if (!px) { s->w = -1; return NULL; }
    s->tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, s->w, s->h);
    SDL_UpdateTexture(s->tex, NULL, px, s->w * 4);
    SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
    free(px);
    return s->tex ? s : NULL;
}

// 網頁版圖片還在下載時回 NULL 但不記成失敗，下一幀再試
static const Sprite *upload_or_wait(Sprite *s, uint32_t *px) {
    if (!px && res_waiting) return NULL;
    return upload(s, px);
}

// 全域圖片 ref（高 8 位幀號）。ref 不會是 0xFFxxxxxx，所以磁磚用那段當 key
static const Sprite *image(uint32_t ref) {
    Sprite *s = slot_find(ref);
    if (s->tex) return s;
    if (s->w < 0) return NULL;
    res_waiting = 0;
    return upload_or_wait(s, res_image(ref, &s->w, &s->h));
}

static const Sprite *tile(Lib *tiles, uint16_t i) {
    Sprite *s = slot_find(0xFF000000u | i);
    if (s->tex) return s;
    if (s->w < 0) return NULL;
    uint32_t size;
    res_waiting = 0;
    uint8_t *d = lib_try(tiles, i, &size);
    uint32_t *px = d ? xbm_decode(d, size, &s->w, &s->h) : NULL;
    free(d);
    return upload_or_wait(s, px);
}

#ifdef __EMSCRIPTEN__
// 網頁上方的提示：n < 0 = 正在載入場景；n > 0 = 背景還有 n 張圖在下載
EM_JS(void, js_status, (int n), {
    const el = document.getElementById('status');
    if (el) el.textContent = n < 0 ? '載入場景中…' : n > 0 ? '載入中… 還有 ' + n + ' 張圖' : '按住畫面走路，點怪物攻擊';
});
#else
static void js_status(int n) { (void)n; }
#endif

static void draw(const Sprite *s, int x, int y) {
    if (!s || x >= vw || y >= vh || x + s->w <= 0 || y + s->h <= 0) return;
    SDL_Rect r = {x, y, s->w, s->h};
    SDL_RenderCopy(ren, s->tex, NULL, &r);
}

static void draw_shadow(const Sprite *s, int x, int y) {
    if (!s) return;
    SDL_SetTextureColorMod(s->tex, 0, 0, 0);
    SDL_SetTextureAlphaMod(s->tex, 110);
    draw(s, x, y);
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s->tex, 255);
}

// 動畫的某一幀，腳底對齊 (x, y)
static void draw_frame(const Anim *a, int seq, int frame, int x, int y, int shadow) {
    if (!a || seq >= a->count || !a->seqs[seq].count) return;
    const Frame *f = &a->seqs[seq].frames[frame % a->seqs[seq].count];
    const Sprite *s = image(f->ref);
    if (shadow) draw_shadow(s, x - f->dx, y - f->dy);
    else draw(s, x - f->dx, y - f->dy);
}

// 依每幀停留拍數循環播放：第 t 拍該顯示哪一幀
static int frame_at(const Seq *q, Uint32 t) {
    Uint32 total = 0;
    for (int i = 0; i < q->count; i++) total += q->frames[i].delay > 0 ? q->frames[i].delay : 1;
    if (!total) return 0;
    t %= total;
    for (int i = 0; i < q->count; i++) {
        Uint32 d = q->frames[i].delay > 0 ? q->frames[i].delay : 1;
        if (t < d) return i;
        t -= d;
    }
    return 0;
}

// ---- 角色（主角與 NPC 共用）----
enum { ACT_NONE, ACT_ATTACK, ACT_HIT, ACT_DEAD };  // 單次動作：播一次就回到平常；死亡停在最後一幀
typedef struct Actor {
    float x, y;       // 格子座標（可有小數）
    int dir, moving, speed;
    int idle, walk, fidle, fwalk;  // 動作起點序列（角色表「平時/戰鬥」的休息與走路）
    Uint32 act_tick;  // 目前動作已經過幾拍（換動作時歸零）
    const Role *role;
    const RoleDef *def;
    const NpcSpec *spec;  // NPC 的擺放資料；主角為 NULL
    float tx, ty;     // AI 目標格
    int has_target, path_i, rest;  // rest = 還要休息幾拍
    const MonDef *mon;  // 怪物的數值表列；NPC 與主角為 NULL
    int level, hp, hp_max, ap, ap_max, dp, hr, sr, exp;
    int act, act_seq, last_frame;  // 單次動作與它的序列起點、上一拍的幀（觸發用）
    const Method *atk;             // 普通攻擊的招式
    struct Actor *foe;             // 攻擊目標
    int cool, fight, dead_ticks;   // 攻擊冷卻（拍）、戰鬥姿態剩幾拍、死了幾拍
} Actor;

// 角色某序列的節奏：各部件的幀延遲相同，拿第一個有資料的部件
static const Seq *role_seq(const Role *r, int seq) {
    for (int i = 0; r && i < r->part_count; i++)
        if (r->body[i] && seq < r->body[i]->count && r->body[i]->seqs[seq].count) return &r->body[i]->seqs[seq];
    return NULL;
}

static Uint32 seq_ticks(const Seq *q) {
    Uint32 t = 0;
    for (int i = 0; i < q->count; i++) t += q->frames[i].delay > 0 ? q->frames[i].delay : 1;
    return t;
}

static int actor_seq(const Actor *a) {
    int base = a->act ? a->act_seq : a->moving ? (a->fight ? a->fwalk : a->walk) : (a->fight ? a->fidle : a->idle);
    return base + DIR_MAP[a->dir];
}

// 目前該畫第幾幀：平常循環播放，單次動作播完停在最後一幀
static int actor_frame(const Actor *a, int seq) {
    const Seq *q = role_seq(a->role, seq);
    if (!q) return 0;
    if (a->act && a->act_tick >= seq_ticks(q)) return q->count - 1;
    return frame_at(q, a->act_tick);
}

static void actor_draw(const Actor *a, int sx, int sy) {
    const Role *r = a->role;
    if (!r) return;
    int seq = actor_seq(a);
    if (seq >= r->seq_count || !r->frames[seq]) return;
    int frame = actor_frame(a, seq) % r->frames[seq];
    for (int i = 0; i < r->part_count; i++) draw_frame(r->shadow[i], seq, frame, sx, sy, 1);
    const uint8_t *order = r->order[seq] + frame * r->part_count;
    for (int i = 0; i < r->part_count; i++)
        if (order[i] < r->part_count) draw_frame(r->body[order[i]], seq, frame, sx, sy, 0);
}

// 面向畫面方向 (sdx, sdy)
static void face(Actor *a, float sdx, float sdy) {
    int oct = (int)lroundf(atan2f(sdx, -sdy) / (float)(M_PI / 4));
    a->dir = (oct + 8) % 8;
}

// 往畫面方向 (sdx, sdy) 走 px 個畫面像素。畫面上各方向等速（格子寬 64 高 32，若照格子等速，上下會只有左右一半快）
static void actor_step(Actor *h, const Scene *s, float sdx, float sdy, float px) {
    float len = sqrtf(sdx * sdx + sdy * sdy);
    if (len < 0.01f) return;
    sdx /= len;
    sdy /= len;
    // 畫面位移 → 格子位移：畫面 x = (gx - gy) * 32，畫面 y = (gx + gy) * 16
    float gx = (sdx / 64 + sdy / 32) * px, gy = (-sdx / 64 + sdy / 32) * px;
    // 能走就走；撞牆時試著只沿一個軸滑動
    if (scene_walkable(s, (int)(h->x + gx), (int)(h->y + gy))) { h->x += gx; h->y += gy; }
    else if (scene_walkable(s, (int)(h->x + gx), (int)h->y)) h->x += gx;
    else if (scene_walkable(s, (int)h->x, (int)(h->y + gy))) h->y += gy;
    face(h, sdx, sdy);
    h->moving = 1;
}

// 一拍的角色更新：往畫面方向 (mx, my) 走；原版每 B 拍移動 A + C/10 像素（速度對照表）
static void actor_tick(Actor *a, const Scene *s, float mx, float my, Uint32 tick) {
    int was_moving = a->moving;
    a->moving = 0;
    if (mx * mx + my * my > 0.01f) {
        int sp = a->speed < 0 ? 0 : a->speed > 59 ? 59 : a->speed;
        float px = tick % SPEED_B[sp] ? 0 : SPEED_A[sp] + SPEED_C[sp] / 10.0f;
        actor_step(a, s, mx, my, px);
    }
    a->act_tick = a->moving == was_moving ? a->act_tick + 1 : 0;
}

// NPC 平時邏輯：13 在範圍內隨機走走停停、14 沿路線來回巡邏、其他站著
static void npc_think(Actor *a, const Scene *s, Uint32 tick) {
    const NpcSpec *n = a->spec;
    float mx = 0, my = 0;
    if (a->rest > 0) a->rest--;
    else if (n->type == 13 || (n->type == 14 && n->path_n)) {
        if (!a->has_target) {
            if (n->type == 14) {
                a->path_i = (a->path_i + 1) % n->path_n;
                a->tx = n->path[a->path_i][0] + 0.5f;
                a->ty = n->path[a->path_i][1] + 0.5f;
            } else {  // shortcut: 閒逛的停留時間與選點方式是自訂的，原版邏輯還沒拆
                a->tx = n->rx + rand() % (n->rw > 0 ? n->rw : 1) + 0.5f;
                a->ty = n->ry + rand() % (n->rh > 0 ? n->rh : 1) + 0.5f;
            }
            a->has_target = scene_walkable(s, (int)a->tx, (int)a->ty);
        }
        float gx = a->tx - a->x, gy = a->ty - a->y;
        if (!a->has_target || gx * gx + gy * gy < 0.04f) {
            a->has_target = 0;
            if (n->type == 13) a->rest = 60 + rand() % 180;
        } else {
            mx = (gx - gy) * 32;  // 格子方向 → 畫面方向
            my = (gx + gy) * 16;
        }
    }
    float bx = a->x, by = a->y;
    actor_tick(a, s, mx, my, tick);
    if (a->moving && a->x == bx && a->y == by) { a->has_target = 0; a->rest = 30; }  // 卡住就換目標
}

// 依角色表載入外觀；roles 是這個場景已載入過的外觀快取（同一模板共用）
#define MAX_ROLES 64
typedef struct { int tmpl; Role role; } RoleSlot;
static RoleSlot roles[MAX_ROLES];
static int role_count;

static const Role *role_for(const RoleDef *d) {
    for (int i = 0; i < role_count; i++)
        if (roles[i].tmpl == d->id) return &roles[i].role;
    if (role_count == MAX_ROLES) return NULL;
    RoleSlot *r = &roles[role_count];
    if (!role_load(&r->role, d->graphic, d->pli, d->styles)) return NULL;
    r->tmpl = d->id;
    role_count++;
    return &r->role;
}

static void roles_clear(void) {
    for (int i = 0; i < role_count; i++) role_free(&roles[i].role);
    role_count = 0;
}

// 主角 + 場景 NPC
#define MAX_ACTORS 256
static Actor actors[MAX_ACTORS];
static int actor_count;

static void actor_setup(Actor *a, const RoleDef *d) {
    a->role = role_for(d);
    a->def = d;
    a->idle = d->idle;
    a->walk = d->walk;
    a->fidle = d->fight_idle;
    a->fwalk = d->fight_walk ? d->fight_walk : d->walk;
    a->speed = d->speed;
}

// 編號清單（預抓用）
typedef struct { uint32_t *v; int n, cap; } IdList;
static void id_add(IdList *l, uint32_t v) {
    if (l->n == l->cap) l->v = realloc(l->v, (l->cap = l->cap ? l->cap * 2 : 1024) * sizeof *l->v);
    l->v[l->n++] = v;
}

// 進場景前一次收齊所有角色要的 PLI 與動畫定義（網頁版每個角色各抓一次會來回上百趟）
static void preload_roles(const Scene *s, const RoleDef *hero_def) {
    const RoleDef *defs[MAX_ACTORS];
    int n = 0;
    if (hero_def) defs[n++] = hero_def;
    for (int i = 0; i < s->npc_count && n < MAX_ACTORS; i++) {
        const RoleDef *d = roledef_find(s->npcs[i].tmpl);
        int dup = !d || !atoi(d->graphic);
        for (int k = 0; k < n && !dup; k++) dup = defs[k] == d;
        if (!dup) defs[n++] = d;
    }
    char names[MAX_ACTORS + 1][24];
    const char *list[MAX_ACTORS + 1];
    snprintf(names[0], sizeof names[0], "0000_001.PLI");
    list[0] = names[0];
    for (int i = 0; i < n; i++) {
        snprintf(names[i + 1], sizeof names[i + 1], "%.8s_%03d.PLI", defs[i]->graphic, defs[i]->pli);
        list[i + 1] = names[i + 1];
    }
    res_preload("PLI", list, n + 1);
    IdList ids = {0};
    for (int i = 0; i < n; i++) {
        uint32_t g[MAX_ROLE_PARTS * 2];
        int m = role_anim_ids(defs[i]->graphic, defs[i]->pli, defs[i]->styles, g, MAX_ROLE_PARTS * 2);
        for (int k = 0; k < m; k++) id_add(&ids, g[k]);
    }
    res_fetch_now(ids.v, ids.n);
    free(ids.v);
}

static void spawn_npcs(const Scene *s) {
    actor_count = 1;  // actors[0] 是主角
    for (int i = 0; i < s->npc_count && actor_count < MAX_ACTORS; i++) {
        const NpcSpec *n = &s->npcs[i];
        const RoleDef *d = roledef_find(n->tmpl);
        if (!d || !atoi(d->graphic)) continue;  // shortcut: 圖形 ID 是名字的（怪物）用另一套格式，之後做戰鬥時處理
        Actor *a = &actors[actor_count++];
        memset(a, 0, sizeof *a);
        a->x = n->x + 0.5f;
        a->y = n->y + 0.5f;
        for (int o = 0; o < 8; o++)
            if (DIR_MAP[o] == n->dir) a->dir = o;
        a->spec = n;
        a->act_tick = rand() % 200;  // 錯開動作，不要整排同步
        actor_setup(a, d);
    }
}

// 房間的怪物群（EasyRPG.dll 0x10052550）：每群挑一個「前场景名称 = 區域」的入口點，整群出現在附近
#define MON_WANDER 4  // shortcut: 怪物閒逛半徑（格）是自訂的，原版 AI 還沒拆
static NpcSpec mon_specs[MAX_ACTORS];

static void spawn_monsters(const Scene *s, const World *w, int room) {
    if (room < 0) return;
    const Room *r = &w->rooms[room];
    for (int g = 0; g < r->group_count; g++) {
        const MonGroup *mg = &w->groups[r->first_group + g];
        int n = 0, pick = -1;
        for (int i = 0; i < s->entry_count; i++)
            if (s->entries[i].id == mg->area && rand() % ++n == 0) pick = i;
        if (pick < 0) continue;
        const Entry *en = &s->entries[pick];
        for (int k = 0; k < mg->count && actor_count < MAX_ACTORS; k++) {
            const MonDef *m = mondef_pick(mg->name);
            const RoleDef *d = m ? roledef_find(m->role) : NULL;
            if (!d || !atoi(d->graphic)) continue;  // shortcut: 圖形 ID 是名字的（蝙蝠、骷髏、蜘蛛）用另一套格式，還沒做
            int x = en->x, y = en->y;
            for (int t = 0; t < 20; t++) {  // 在入口點附近找一格能站的
                int tx = en->x + rand() % 7 - 3, ty = en->y + rand() % 7 - 3;
                if (scene_walkable(s, tx, ty)) { x = tx; y = ty; break; }
            }
            Actor *a = &actors[actor_count];
            NpcSpec *sp = &mon_specs[actor_count++];
            *sp = (NpcSpec){.type = 13, .tmpl = d->id, .x = x, .y = y, .rx = en->x - MON_WANDER, .ry = en->y - MON_WANDER,
                            .rw = MON_WANDER * 2 + 1, .rh = MON_WANDER * 2 + 1};
            memset(a, 0, sizeof *a);
            a->x = x + 0.5f;
            a->y = y + 0.5f;
            a->dir = rand() % 8;
            a->spec = sp;
            a->act_tick = rand() % 200;
            a->mon = m;
            int lv = mg->level, tier = mg->tier;
            if (lv < 0) lv = tier * 5 - rand() % 5;
            a->level = lv < 1 ? 1 : lv > 99 ? 99 : lv;
            a->hp = a->hp_max = stat_at(m->hp, a->level);
            a->ap = stat_at(m->ap, a->level);
            a->ap_max = stat_at(m->ap_max, a->level);
            a->dp = stat_at(m->dp, a->level);
            a->hr = stat_at(m->hit, a->level);
            a->sr = stat_at(m->dodge, a->level);
            a->exp = stat_at(m->exp, a->level);
            a->atk = method_find(skill_attack(m->skill));
            actor_setup(a, d);
            printf("怪物 %s Lv%d HP%d 在 (%d,%d)\n", m->name, a->level, a->hp, x, y);
        }
    }
}

// ---- 戰鬥 ----
#define FIGHT_TICKS 300  // shortcut: 打完多久收起戰鬥姿態是自訂的（約 5 秒）
#define CORPSE_TICKS 600 // shortcut: 屍體留多久是自訂的（約 10 秒）
static int exp_table[100], exp_levels;  // Data/experience.txt：第 n 列 = 升到 n+1 級要的經驗

static int alive(const Actor *a) { return a && a->role && a->act != ACT_DEAD; }

static float cell_dist(const Actor *a, const Actor *b) {
    return fmaxf(fabsf(a->x - b->x), fabsf(a->y - b->y));
}

static void start_act(Actor *a, int act, int seq) {
    a->act = act;
    a->act_seq = seq;
    a->act_tick = 0;
    a->last_frame = -1;
    a->moving = 0;
}

// 主角數值。遊俠模板（EasyRPG.dll 0x1013c380）：Str 30、Con 25、Dex 25，HP_MAX = 120 + 28×等級
// 職業倍率（0x1002d510，遊俠 K = 150）：AP、AP_MAX = 基礎 × (Str+K)/K；DP = 基礎 × (Con+K)/K；
// HR = (基礎 + 3×Dex) × (Dex+K)/K；SR = (基礎 + 4×Dex) × (Dex+K)/K
// 基礎 AP 來自武器、DP 來自護甲（模板給的扶苏武器 = item_8 第 1 列青銅彎刀、布衣 = item_26 第 1 列）；HR/SR 基礎 0
// shortcut: 道具的隨機特性、升級加點、其他部位的裝備還沒做
static Item hero_weapon, hero_armor;
static int hero_armor_dp;  // 護甲防禦在「防御力小～大」之間，道具產生時決定

static void hero_stats(Actor *h, int level) {
    const int K = 150, str = 30, con = 25, dex = 25;
    h->level = level;
    h->hp = h->hp_max = 120 + 28 * level;
    h->ap = hero_weapon.ap * (str + K) / K;
    h->ap_max = hero_weapon.ap_max * (str + K) / K;
    h->dp = hero_armor_dp * (con + K) / K;
    h->hr = 3 * dex * (dex + K) / K;
    h->sr = 4 * dex * (dex + K) / K;
}

// 一次命中（EasyRPG.dll 0x10033140 物理傷害）：攻擊 = AP~AP_MAX 隨機，損血 = 攻擊² / (攻擊 + DP/2)，至少 3
// ---- 招式特效（觸發 type 2 → MethodUnit_Mdl.ini 的 Res00 動畫，播一次）----
#define MAX_FX 64
static struct { uint32_t gid; float x, y; Uint32 start; } fx[MAX_FX];
static int fx_count;

// shortcut: pos 只分自己和對方（0、1 以外的 2、3、7、8、11 都先當自己），特效的混色（bData3）還沒做
static void fx_spawn(const Trig *t, const Actor *self, const Actor *foe, Uint32 tick) {
    const Actor *at = t->pos == 1 && foe ? foe : self;
    uint32_t gid = unit_anim(t->id);
    if (!gid || fx_count == MAX_FX) return;
    fx[fx_count].gid = gid;
    fx[fx_count].x = at->x;
    fx[fx_count].y = at->y;
    fx[fx_count++].start = tick;
}

static void strike(Actor *a, Actor *t, Uint32 tick) {
    if (!alive(t)) return;
    t->fight = FIGHT_TICKS;
    if (t->mon && !t->foe) t->foe = a;  // 被打的怪物會反擊
    // 命中（Poq.exe 0x48c600，CCharactor 虛擬函式第 104 格）：千分比 = HR×1000 / (SR + HR)，夾在 100~900；HR 0 當 1
    // shortcut: 之前還有一段盾牌格擋（0x48c4e3，看防守方 +0x4e7 的裝備和攻擊方向），沒有盾的主角用不到，還沒做
    int hr = a->hr ? a->hr : 1, chance = hr * 1000 / (t->sr + hr);
    if (rand() % 1000 >= (chance < 100 ? 100 : chance > 900 ? 900 : chance)) return;
    int dmg = a->ap + (a->ap_max > a->ap ? rand() % (a->ap_max - a->ap + 1) : 0);
    int loss = dmg + t->dp / 2 > 0 ? dmg * dmg / (dmg + t->dp / 2) : 0;
    if (loss < 3) loss = 3;
    t->hp -= loss;
    if (a->atk)  // 打中對方時的音效和特效（招式 iFlag = 1）
        for (int i = 0; i < a->atk->trig_count; i++) {
            const Trig *g = &a->atk->trig[i];
            if (g->flag != 1) continue;
            if (g->type == 1) audio_sfx(g->id);
            else if (g->type == 2) fx_spawn(g, a, t, tick);
        }
    if (t->hp <= 0) {
        t->hp = 0;
        start_act(t, ACT_DEAD, t->def->death);
        if (t->def->snd_death) audio_sfx(t->def->snd_death);
        if (!a->mon && t->mon) {  // 主角拿經驗，夠了就升級
            a->exp += t->exp;
            while (a->level < exp_levels && a->exp >= exp_table[a->level]) {
                hero_stats(a, a->level + 1);
                printf("升到 %d 級\n", a->level);
            }
        }
        printf("%s 倒下\n", t->mon ? "怪物" : "主角");
        return;
    }
    if (t->def->snd_hit) audio_sfx(t->def->snd_hit);
    if (t->def->hit && t->act != ACT_ATTACK) start_act(t, ACT_HIT, t->def->hit);
}

// 單次動作的進度：攻擊依招式各幀觸發音效與命中；播完回到平常（死亡除外）
static void action_tick(Actor *a, Uint32 tick) {
    int seq = actor_seq(a);
    const Seq *q = role_seq(a->role, seq);
    int f = actor_frame(a, seq);
    if (a->act == ACT_ATTACK && a->atk)
        for (int fr = a->last_frame + 1; fr <= f; fr++)
            for (int i = 0; i < a->atk->trig_count; i++) {
                const Trig *t = &a->atk->trig[i];
                if (t->flag || t->frame != fr) continue;
                if (t->type == 1) audio_sfx(t->id);
                else if (t->type == 0) strike(a, a->foe, tick);
                else if (t->type == 2) fx_spawn(t, a, a->foe, tick);
            }
    a->last_frame = f;
    a->act_tick++;
    if (a->act != ACT_DEAD && (!q || a->act_tick >= seq_ticks(q))) {
        if (a->act == ACT_ATTACK) a->cool = a->def->atk_delay;
        a->act = ACT_NONE;
        a->act_tick = 0;
    }
}

// 追打目標：在招式距離內就面向它出手，否則往它走。回傳 0 = 沒有目標
static int pursue(Actor *a, const Scene *s, Uint32 tick) {
    if (!alive(a->foe)) { a->foe = NULL; return 0; }
    Actor *t = a->foe;
    a->fight = FIGHT_TICKS;
    float gx = t->x - a->x, gy = t->y - a->y;
    float sdx = (gx - gy) * 32, sdy = (gx + gy) * 16;  // 格子方向 → 畫面方向
    int range = a->atk && a->atk->range_max > 0 ? a->atk->range_max : 1;
    if (cell_dist(a, t) <= range + 0.5f) {
        face(a, sdx, sdy);
        a->moving = 0;
        a->act_tick++;
        if (a->cool <= 0) start_act(a, ACT_ATTACK, a->atk ? a->atk->seq : a->fidle);
        return 1;
    }
    actor_tick(a, s, sdx, sdy, tick);
    return 1;
}

// 怪物：主角進入索敌范围就追上去打，離出生點超過追击范围就放棄回去閒逛
static void monster_think(Actor *a, Actor *hero, const Scene *s, Uint32 tick) {
    if (a->foe && (!alive(a->foe) || fabsf(a->x - a->spec->rx - MON_WANDER) + fabsf(a->y - a->spec->ry - MON_WANDER) > a->def->chase))
        a->foe = NULL;
    if (!a->foe && alive(hero) && cell_dist(a, hero) <= a->def->seek) a->foe = hero;
    a->speed = a->foe ? a->def->fight_speed : a->def->speed;
    if (!pursue(a, s, tick)) npc_think(a, s, tick);
}

// 畫面上點到哪隻活著的怪物（腳底附近、往上約一個身高）
static Actor *monster_at(int px, int py, int ox, int oy, const Scene *s) {
    Actor *best = NULL;
    int best_d = 1 << 30;
    for (int i = 1; i < actor_count; i++) {
        Actor *a = &actors[i];
        if (!a->mon || !alive(a)) continue;
        int fx = (int)((a->x - a->y + s->h - 1) * TILE_W / 2) - ox, fy = (int)((a->x + a->y) * TILE_H / 2) - oy;
        int dx = px - fx, dy = py - (fy - 30);
        if (abs(dx) > 36 || dy < -70 || dy > 40) continue;
        if (dx * dx + dy * dy < best_d) { best_d = dx * dx + dy * dy; best = a; }
    }
    return best;
}

// 血條：shortcut: 原版介面（ResUI 血球）還沒做，先畫簡單的條
static void hp_bar(const Actor *a, int x, int y, int w) {
    if (!a->hp_max) return;
    SDL_Rect bg = {x, y, w, 4}, fg = {x, y, w * a->hp / a->hp_max, 4};
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawColor(ren, 200, 30, 30, 255);
    SDL_RenderFillRect(ren, &fg);
}

static int cmp_actor_depth(const void *a, const void *b) {
    const Actor *x = *(Actor *const *)a, *y = *(Actor *const *)b;
    float d = (x->x + x->y) - (y->x + y->y);
    return d < 0 ? -1 : d > 0;
}

// 進場景時一次抓齊會用到的圖（網頁版走批次請求；原生版直接讀硬碟，這裡幾乎不花時間）

static void add_seq_frames(IdList *l, const Anim *a, int seq) {
    if (a && seq >= 0 && seq < a->count)
        for (int f = 0; f < a->seqs[seq].count; f++) id_add(l, a->seqs[seq].frames[f].ref & 0xFFFFFF);
}

// 依離主角的距離排序（近的先下載）
typedef struct { float d; uint32_t id; } Near;
static int cmp_near(const void *a, const void *b) {
    float x = ((const Near *)a)->d, y = ((const Near *)b)->d;
    return x < y ? -1 : x > y;
}

static void want_sorted(Near *v, int n, Lib *lib) {
    qsort(v, n, sizeof *v, cmp_near);
    uint32_t *ids = malloc((n ? n : 1) * sizeof *ids);
    for (int i = 0; i < n; i++) ids[i] = v[i].id;
    if (lib) lib_want(lib, ids, n);
    else res_want(ids, n);
    free(ids);
}

static float dist_to(const Actor *a, float x, float y) { return fabsf(x - a->x) + fabsf(y - a->y); }

static void prefetch_scene(Scene *s, const Actor *hero) {
    // 動畫定義是小資料、而且要先有才知道要哪些圖：等它下載完
    IdList anims = {0};
    for (int i = 0; i < s->object_count; i++)
        for (int p = 0; s->objects[i].m && p < s->objects[i].m->part_count; p++)
            if (s->objects[i].m->parts[p].anim) id_add(&anims, s->objects[i].m->parts[p].gid);
    res_fetch_now(anims.v, anims.n);
    free(anims.v);
    // 圖片都排進背景下載，離主角近的先
    Near *v = malloc(((size_t)s->w * s->h * 2 + 1) * sizeof *v);
    int n = 0;
    for (int i = 0; i < s->w * s->h; i++) {
        float d = dist_to(hero, i % s->w + 0.5f, i / s->w + 0.5f);
        if (s->cells[i].ground != 0xFFFF) v[n++] = (Near){d, s->cells[i].ground};
        if (s->cells[i].over != 0xFFFF) v[n++] = (Near){d, s->cells[i].over};
    }
    want_sorted(v, n, &s->tiles);
    n = 0;
    IdList frames = {0};
    for (int i = 0; i < s->object_count; i++) {
        const Object *o = &s->objects[i];
        for (int p = 0; o->m && p < o->m->part_count; p++) {
            frames.n = 0;
            if (o->m->parts[p].anim) add_seq_frames(&frames, anim_get(o->m->parts[p].gid), 0);
            else id_add(&frames, o->m->parts[p].gid);
            for (int k = 0; k < frames.n; k++) {
                if (n % 4096 == 0) v = realloc(v, (n + 4096) * sizeof *v);
                v[n++] = (Near){dist_to(hero, o->x + 0.5f, o->y + 0.5f) + 2, frames.v[k]};  // 建築大，稍微讓地面先
            }
        }
    }
    // 角色：站立與走路的 8 個方向
    for (int i = 0; i < actor_count; i++) {
        const Actor *a = &actors[i];
        frames.n = 0;
        for (int p = 0; a->role && p < a->role->part_count; p++)
            for (int d = 0; d < 8; d++) {
                add_seq_frames(&frames, a->role->body[p], a->idle + d);
                add_seq_frames(&frames, a->role->body[p], a->walk + d);
                add_seq_frames(&frames, a->role->shadow[p], a->idle + d);
                add_seq_frames(&frames, a->role->shadow[p], a->walk + d);
            }
        for (int k = 0; k < frames.n; k++) {
            if (n % 4096 == 0) v = realloc(v, (n + 4096) * sizeof *v);
            v[n++] = (Near){i ? dist_to(hero, a->x, a->y) : 0, frames.v[k]};
        }
    }
    want_sorted(v, n, NULL);
    free(frames.v);
    free(v);
}

// 主角的角色表列，外觀加上身上裝備（道具表的遊俠裝備串）
static RoleDef hero_def;

static void hero_equip(void) {
    const RoleDef *d = roledef_by_graphic(HERO_GRAPHIC);
    if (!d) return;
    hero_def = *d;
    static const char *const items[] = {"item_8.txt", "item_26.txt"};
    res_preload("Data", items, 2);
    if (item_find(8, 1, &hero_weapon)) styles_equip(hero_def.styles, sizeof hero_def.styles, d->graphic, d->pli, hero_weapon.equip[0]);
    if (item_find(26, 1, &hero_armor)) {
        styles_equip(hero_def.styles, sizeof hero_def.styles, d->graphic, d->pli, hero_armor.equip[0]);
        hero_armor_dp = hero_armor.dp_min + rand() % (hero_armor.dp_max - hero_armor.dp_min + 1);
    }
}

// 載入場景（別名 = 場景檔名），主角站到入口 entry_id；失敗時保留原場景
static int enter_scene(Scene *scene, Actor *hero, const char *alias, int entry_id) {
    char file[96];
    snprintf(file, sizeof file, "%.80s.Scn", alias);
    Scene next;
    if (!scene_load(&next, file)) { fprintf(stderr, "載入場景失敗：%s\n", file); return 0; }
    if (scene->cells) scene_free(scene);
    *scene = next;
    cache_clear();
    res_cache_clear();
    // 外觀快取跟著場景換：先留主角的，再重建 NPC
    const RoleDef *hd = hero_def.id ? &hero_def : roledef_by_graphic(HERO_GRAPHIC);
    roles_clear();
    preload_roles(scene, hd);
    if (hd) actor_setup(hero, hd);
    spawn_npcs(scene);
    fx_count = 0;
    const Entry *en = scene_entry(scene, entry_id);
    if (en) {
        hero->x = en->x + 0.5f;
        hero->y = en->y + 0.5f;
        for (int o = 0; o < 8; o++)
            if (DIR_MAP[o] == en->dir) hero->dir = o;  // shortcut: 假設入口方向跟動畫方向同編號，還沒跟原版對照
    }
    hero->moving = 0;
    hero->act_tick = 0;
    prefetch_scene(scene, hero);
    return 1;
}

// 原版加密設定檔裡 key=value 的數值（key 是 GBK）；找不到回 def
static int ini_int(const char *text, const char *key, int def) {
    size_t n = strlen(key);
    for (const char *p = text; p && *p; p = strchr(p, '\n'), p = p ? p + 1 : NULL)
        if (!strncmp(p, key, n)) return atoi(p + n);
    return def;
}

// Data/music.txt：場景別名 → 背景音樂檔名（第 2 欄）
static void scene_music(const char *alias, char *out, size_t n) {
    snprintf(out, n, "normal1");
    size_t size;
    uint8_t *d = res_file("Data", "music.txt", &size);
    if (!d) return;
    res_unxor(d, size);
    for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char *tab = strchr(line, '\t');
        if (!tab || (size_t)(tab - line) != strlen(alias) || strncasecmp(line, alias, tab - line)) continue;
        char *end = strchr(tab + 1, '\t');
        if (end) *end = 0;
        if (tab[1]) snprintf(out, n, "%s", tab + 1);
        break;
    }
    free(d);
}

// 主角是否站在觸發區上（以觸發區所在格為中心 3×3 格）。shortcut: 觸發區實際形狀在 MDL 的佔地格，還沒解析
static const Trigger *trigger_at(const Scene *s, const Actor *h) {
    for (int i = 0; i < s->trigger_count; i++) {
        const Trigger *t = &s->triggers[i];
        if (fabsf(h->x - (t->x + 0.5f)) <= 1.5f && fabsf(h->y - (t->y + 0.5f)) <= 1.5f) return t;
    }
    return NULL;
}

// 第一次需要時才讀房間表（EasyRPG.dll），回傳目前場景的房間索引
static int load_world(World *world, int *tried, const char *alias) {
    *tried = 1;
    js_status(-1);
    if (!world_load(world)) { fprintf(stderr, "讀不到 EasyRPG.dll 的房間資料，不能切換場景\n"); return -1; }
    return world_find(world, alias);
}

int main(int argc, char **argv) {
    const char *game = "game", *scn = "xianyang", *shot = NULL;
    int pos = 0, warp = -1, at_x = -1, at_y = -1, level = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--640")) { vw = 640; vh = 480; }
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--warp") && i + 1 < argc) warp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &at_x, &at_y);
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (pos++ == 0) game = argv[i];
        else scn = argv[i];
    }
    res_init(game);

    js_status(-1);
    // 房間表（EasyRPG.dll，4MB）等第一次踩到出口才讀：網頁版開場不用等它
    World world = {0};
    int world_tried = 0;
    char alias[64];
    snprintf(alias, sizeof alias, "%s", scn);
    char *dot = strrchr(alias, '.');
    if (dot && !strcasecmp(dot, ".Scn")) *dot = 0;
    if (!roledefs_load()) { fprintf(stderr, "讀不到角色表 ObjMdl/RoleMdl.txt\n"); return 1; }
    srand((unsigned)time(NULL));
    hero_equip();
    Scene scene = {0};
    Actor *hero = &actors[0];
    hero->dir = 4;
    if (!enter_scene(&scene, hero, alias, -1)) return 1;
    if (!hero->role) { fprintf(stderr, "載入主角失敗\n"); return 1; }
    hero_stats(hero, level < 1 ? 1 : level > 99 ? 99 : level);
    int room = -1, armed = 0;  // armed：離開觸發區後才會再觸發，避免一進場就被送回去
    int monsters_due = 1;      // 進場景後等圖片都下載好，再讀房間表放怪物（網頁版開場不用等 4MB 的 DLL）
    if (at_x >= 0) { hero->x = at_x + 0.5f; hero->y = at_y + 0.5f; }
    if (warp >= 0 && warp < scene.trigger_count) {
        hero->x = scene.triggers[warp].x + 0.5f;
        hero->y = scene.triggers[warp].y + 0.5f;
        armed = 1;
        printf("站到觸發區 %d：%s\n", warp, scene.triggers[warp].event);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Window *win = SDL_CreateWindow("秦殤", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, vw, vh,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);  // 沒有 GPU 驅動時改用軟體繪製
    if (!win || !ren) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, vw, vh);  // 視窗多大都等比例縮放，畫面內容固定 vw x vh
    // 先以 1:1 畫在畫布上再整張放大；直接放大每塊磁磚會在接縫透出細線
    SDL_Texture *canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);
    // 音量與開關照原版 ini/Config.ini（[音频] 音效、音乐、音效音量、音乐音量）
    size_t cfg_size;
    uint8_t *cfg = res_file("ini", "Config.ini", &cfg_size);
    if (cfg) res_unxor(cfg, cfg_size);
    const char *cfg_text = cfg ? (const char *)cfg : "";
    audio_init(ini_int(cfg_text, "\xd2\xf4\xc0\xd6=", 1) ? ini_int(cfg_text, "\xd2\xf4\xc0\xd6\xd2\xf4\xc1\xbf=", 15) : 0,   // 音乐、音乐音量
               ini_int(cfg_text, "\xd2\xf4\xd0\xa7=", 1) ? ini_int(cfg_text, "\xd2\xf4\xd0\xa7\xd2\xf4\xc1\xbf=", 100) : 0);  // 音效、音效音量
    free(cfg);
    char music[64];
    scene_music(alias, music, sizeof music);
    audio_music(music);
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);

    Uint64 fps_t = SDL_GetPerformanceCounter();
    Uint32 logic_ms = SDL_GetTicks(), tick = 0;
    int frames = 0, running = 1, pointer_down = 0, px = 0, py = 0, ox = 0, oy = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f)
                SDL_SetWindowFullscreen(win, SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            // 滑鼠與觸控（SDL 預設會把觸控也送成滑鼠事件）：按住就一直往指的地方走
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                px = e.button.x;
                py = e.button.y;
                Actor *m = monster_at(px, py, ox, oy, &scene);  // 點怪物 = 攻擊它；點其他地方 = 按住走路
                if (m) hero->foe = m;
                else pointer_down = 1;
            }
            // 空白鍵／手把 A：打最近的怪物
            else if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) ||
                     (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A)) {
                float best = hero->def ? hero->def->seek : 10;
                for (int i = 1; i < actor_count; i++)
                    if (actors[i].mon && alive(&actors[i]) && cell_dist(hero, &actors[i]) <= best) { best = cell_dist(hero, &actors[i]); hero->foe = &actors[i]; }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) pointer_down = 0;
            else if (e.type == SDL_MOUSEMOTION) { px = e.motion.x; py = e.motion.y; }
            else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
        }
        if (pad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
            running = 0;

        Uint64 now = SDL_GetPerformanceCounter();

        // ---- 指令：所有輸入都變成「往畫面某方向走」----
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        float mx = (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) - (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]);
        float my = (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) - (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]);
        if (pad) {
            mx += SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) - SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            my += SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) - SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
            float ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f, ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
            if (ax * ax + ay * ay > 0.1f) { mx += ax; my += ay; }
        }
        if (pointer_down) {  // 主角腳底在畫面中央，指向身體中段附近；太靠近就不動
            mx = px - vw / 2;
            my = py - (vh / 2 - 40);
            if (mx * mx + my * my < 64) mx = my = 0;
        }

        // ---- 固定節拍：跟原版一樣每 16ms 一拍，畫面多快都不影響遊戲速度 ----
        Uint32 now_ms = SDL_GetTicks();
        if (now_ms - logic_ms > 250) logic_ms = now_ms - TICK_MS;  // 卡頓或切到背景後不要一次補太多拍
        while (now_ms - logic_ms >= TICK_MS) {
            logic_ms += TICK_MS;
            ++tick;
            for (int i = 0; i < actor_count; i++) {
                if (actors[i].cool > 0) actors[i].cool--;
                if (actors[i].fight > 0 && !actors[i].foe) actors[i].fight--;
            }
            if (hero->act == ACT_DEAD) {  // shortcut: 原版死亡後的處理（回城、掉經驗）還沒做，先 3 秒後原地復活
                if (++hero->dead_ticks > 180) { hero->act = ACT_NONE; hero->dead_ticks = 0; hero->hp = hero->hp_max; }
                action_tick(hero, tick);
            } else if (hero->act) action_tick(hero, tick);
            else if (mx * mx + my * my > 0.01f) { hero->foe = NULL; actor_tick(hero, &scene, mx, my, tick); }  // 自己走 = 放棄目標
            else if (!pursue(hero, &scene, tick)) actor_tick(hero, &scene, 0, 0, tick);
            // 腳步聲：每 max((15 - A) * 2, 10) 拍一次（Poq.exe 0x48704f），80 號音效
            int sp = hero->speed < 0 ? 0 : hero->speed > 59 ? 59 : hero->speed, step = (15 - SPEED_A[sp]) * 2;
            if (hero->moving && tick % (step < 10 ? 10 : step) == 0) audio_sfx(80);  // shortcut: 原版某些地面改用 81 號，地面類型還沒對應
            for (int i = 1; i < actor_count; i++) {
                Actor *a = &actors[i];
                if (!a->mon) npc_think(a, &scene, tick);
                else if (!a->role) continue;
                else if (a->act == ACT_DEAD) { action_tick(a, tick); if (++a->dead_ticks > CORPSE_TICKS) a->role = NULL; }
                else if (a->act) action_tick(a, tick);
                else monster_think(a, hero, &scene, tick);
            }
            // 踩到觸發區：事件 "go <出口>" 對應目前房間的出口，切到目的房間的入口
            const Trigger *t = trigger_at(&scene, hero);
            if (!t) armed = 1;
            else if (armed && !strncmp(t->event, "go ", 3)) {
                if (!world_tried) room = load_world(&world, &world_tried, alias);
                if (room < 0) { armed = 0; continue; }
                const Room *r = &world.rooms[room];
                for (int i = 0; i < r->exit_count; i++) {
                    const Exit *x = &world.exits[r->first_exit + i];
                    if (strcmp(x->dir, t->event + 3) || x->to < 0) continue;
                    js_status(-1);
                    if (enter_scene(&scene, hero, world.rooms[x->to].alias, x->entry)) {
                        room = x->to;
                        monsters_due = 1;
                        snprintf(alias, sizeof alias, "%s", world.rooms[room].alias);
                        scene_music(world.rooms[room].alias, music, sizeof music);
                        audio_music(music);
                        printf("進入 %s\n", world.rooms[room].alias);
                    }
                    armed = 0;
                    break;
                }
            }
        }

        if (monsters_due && !res_loading() && (world_tried || world_ready())) {
            monsters_due = 0;
            if (!world_tried) room = load_world(&world, &world_tried, alias);
            if (!hero->atk) {  // 戰鬥資料也等開場畫面好了才讀（網頁版不拖慢開場）
                hero->atk = method_find(skill_attack("\xd3\xce\xcf\xc0_\xd5\xb6\xbc\xac\xc5\xfb\xbe\xa3"));  // 游侠_斩棘披荆（1 級技能，原版的普通攻擊）
                size_t n;
                uint8_t *d = res_file("Data", "experience.txt", &n);
                if (d) {
                    res_unxor(d, n);
                    for (char *l = strtok((char *)d, "\r\n"); l && exp_levels < 100; l = strtok(NULL, "\r\n")) exp_table[exp_levels++] = atoi(l);
                    free(d);
                }
            }
            if (mondefs_load()) spawn_monsters(&scene, &world, room);
        }

        // 主角腳底的世界像素；鏡頭跟著主角
        int hx = (int)((hero->x - hero->y + scene.h - 1) * TILE_W / 2), hy = (int)((hero->x + hero->y) * TILE_H / 2);
        ox = hx - vw / 2;
        oy = hy - vh / 2;

        // ---- 畫面 ----
        SDL_SetRenderTarget(ren, canvas);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        for (int y = 0; y < scene.h; y++)
            for (int x = 0; x < scene.w; x++) {
                const Cell *c = &scene.cells[y * scene.w + x];
                int sx = world_x(&scene, x, y) - ox, sy = world_y(&scene, x, y) - oy;
                if (sx >= vw || sy >= vh || sx + TILE_W <= 0 || sy + TILE_H <= 0) continue;
                if (c->ground != 0xFFFF) draw(tile(&scene.tiles, c->ground), sx, sy);
                if (c->over != 0xFFFF) draw(tile(&scene.tiles, c->over), sx, sy);
            }
        // 物件已依深度(x+y)排序；角色也依深度排好，經過自己的深度時插進去畫
        Actor *order[MAX_ACTORS];
        for (int i = 0; i < actor_count; i++) order[i] = &actors[i];
        qsort(order, actor_count, sizeof *order, cmp_actor_depth);
        int next_actor = 0;
        for (int i = 0; i <= scene.object_count; i++) {
            const Object *o = i < scene.object_count ? &scene.objects[i] : NULL;
            while (next_actor < actor_count && (!o || o->x + o->y + 1 > order[next_actor]->x + order[next_actor]->y)) {
                const Actor *a = order[next_actor++];
                actor_draw(a, (int)((a->x - a->y + scene.h - 1) * TILE_W / 2) - ox, (int)((a->x + a->y) * TILE_H / 2) - oy);
            }
            if (!o || !o->m) continue;
            int sx = world_x(&scene, o->x, o->y) + TILE_W / 2 - ox, sy = world_y(&scene, o->x, o->y) + TILE_H / 2 - oy;
            for (int p = 0; p < o->m->part_count; p++) {
                const Part *pt = &o->m->parts[p];
                const Anim *a = pt->anim ? anim_get(pt->gid) : NULL;
                if (pt->anim) { if (a) draw_frame(a, 0, frame_at(&a->seqs[0], tick), sx + pt->dx, sy + pt->dy, 0); }  // Res00 動畫，播第一個序列
                else draw(image(pt->gid), sx + pt->dx, sy + pt->dy);
            }
        }
        for (int i = 0; i < fx_count; i++) {  // 特效畫在最上層，播完就移除
            const Anim *a = anim_get(fx[i].gid);
            if (a && a->count && tick - fx[i].start >= seq_ticks(&a->seqs[0])) { fx[i--] = fx[--fx_count]; continue; }
            if (a && a->count)
                draw_frame(a, 0, frame_at(&a->seqs[0], tick - fx[i].start), (int)((fx[i].x - fx[i].y + scene.h - 1) * TILE_W / 2) - ox,
                           (int)((fx[i].x + fx[i].y) * TILE_H / 2) - oy, 0);
        }
        for (int i = 1; i < actor_count; i++)
            if (actors[i].mon && alive(&actors[i]) && actors[i].hp < actors[i].hp_max)
                hp_bar(&actors[i], (int)((actors[i].x - actors[i].y + scene.h - 1) * TILE_W / 2) - ox - 20, (int)((actors[i].x + actors[i].y) * TILE_H / 2) - oy - 90, 40);
        hp_bar(hero, 10, 10, 160);
        if (shot && tick > 3) {
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, vw, vh, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
            SDL_SaveBMP(surf, shot);
            SDL_FreeSurface(surf);
            running = 0;
        }
        SDL_SetRenderTarget(ren, NULL);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, canvas, NULL, NULL);
        SDL_RenderPresent(ren);
        res_pump();
        audio_update(res_loading() == 0);
        static int shown = -2;  // 只在數字變了才更新網頁文字
        int loading = res_loading();
        if (loading != shown) js_status(loading);
        shown = loading;
#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);  // 交還給瀏覽器（ASYNCIFY），否則網頁會卡住
#endif

        frames++;
        if (now - fps_t > SDL_GetPerformanceFrequency()) {
            char title[64];
            snprintf(title, sizeof title, "秦殤 — %d FPS", frames);
            SDL_SetWindowTitle(win, title);
            printf("%d FPS\n", frames);
            fflush(stdout);
            frames = 0;
            fps_t = now;
        }
    }

    cache_clear();
    audio_quit();
    world_free(&world);
    SDL_DestroyTexture(canvas);
    roles_clear();
    scene_free(&scene);
    SDL_Quit();
    return 0;
}
