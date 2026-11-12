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
#include "gbk.h"
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
    if (!el) return;
    // 操作說明只給還沒碰過畫面的人看，之後收掉，不擋原版敵人狀態（同在畫面上方）
    el.textContent = n < 0 ? '載入場景中…' : n > 0 ? '載入中… 還有 ' + n + ' 張圖' : window.qinTouched ? "" : '按住畫面走路，點怪物攻擊';
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
// 身上的一個狀態（原版 Status_N = "經過秒數 時間 技能"）：帶著施放時的技能等級
typedef struct { const SkillRow *skill; int level, status, elapsed, time; } Buff;

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
    int level, hp, hp_max, mp, mp_max, ap, ap_max, dp, hr, sr, exp;
    int act, act_seq, last_frame;  // 單次動作與它的序列起點、上一拍的幀（觸發用）
    const Method *atk;             // 普通攻擊的招式
    const SkillRow *skill;         // 攻擊用的技能（主角右手）；怪物為 NULL
    const SkillRow *cast;          // 正在放的左手技能（casting 時有效）
    const Method *atk_rh;          // 放左手技能時先存起來的右手招式
    int casting;
    int hp_speed, mp_speed, hp_tmp, mp_tmp;  // 每秒回復（百分之一點）與累積的零頭（EasyRPG 0x10054450）
    Buff buff[8];
    int buff_n;
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

// 主角數值。遊俠模板（EasyRPG.dll 0x1013c380）：Str 30、Con 25、Dex 25；升級表 Upgrade_HP_MAX = 120 28、Upgrade_MP_MAX = 140 4
// → HP_MAX = 120 + 28×等級、MP_MAX（精氣）= 140 + 4×等級
// 職業倍率（0x1002d510，遊俠 K = 150）：AP、AP_MAX = 基礎 × (Str+K)/K；DP = 基礎 × (Con+K)/K；
// HR（閃避）= (基礎 + 3×Dex) × (Dex+K)/K；SR（命中）= (基礎 + 4×Dex) × (Dex+K)/K
// 基礎 AP 來自武器、DP 來自護甲（模板給的扶苏武器 = item_8 第 1 列青銅彎刀、布衣 = item_26 第 1 列）；HR/SR 基礎 0
// shortcut: 道具的隨機特性、升級加點、其他部位的裝備還沒做
static Item hero_weapon, hero_armor;
static int hero_armor_dp;  // 護甲防禦在「防御力小～大」之間，道具產生時決定

// 六項屬性，順序 = 人物屬性加點面板 m_pAttrib[0..5] = 力量、体质、敏捷、悟性、魅力、智慧（Poq.exe 0x554d66 起）。開局值照遊俠扶苏模板
enum { STR, CON, DEX, SAVVY, CHARM, WISDOM };
static int hero_attr[6] = {30, 25, 25, 30, 40, 20};
static int hero_extra_exp, hero_extra_skill;  // 剩餘屬性點、技能點：每升一級 +10、+1（EasyRPG.dll 0x10026d30）
static const int hero_money = 100;  // 同一模板的 Money。shortcut: 還沒有撿錢、買賣，錢不會變
static const int hero_gender = 0, hero_age = 29, hero_prof = 1;  // 同一模板的 Gender、Age；職業序號（0x94c2fb，2131 + 序號 = 游侠）

// 技能參數的值：(技能等級 − 1) × 增量 + 初始值（EasyRPG 0x10036160、0x10024xxx 都是這個式子）
static float skill_value(float init, float incr, int level) { return (level - 1) * incr + init; }

// 狀態帶的技能參數套到身上（0x100300f0 → 0x10036160）。屬性代號 20~99 對名稱表 "Invincible" 起每格 0x40（.data 0x1039b5f0）；
// 原版鍵名 HR = 閃避、SR = 命中，這裡 hr = 命中、sr = 閃避。百分比 = 加上目前值 × v / 100
// shortcut: 只對得上 Actor 有的欄位（HP/MP 上限、攻防、命中閃避、回復）；力量等屬性、移動攻擊速度、對敵的參數還沒做
static void hero_mods(Actor *h, int pct) {
    for (int b = 0; b < h->buff_n; b++)
        for (int i = 0; i < h->buff[b].skill->mod_n; i++) {
            const SkillMod *m = &h->buff[b].skill->mod[i];
            if (m->enemy || m->pct != pct) continue;
            int *f = m->id == 23 ? &h->hp_max : m->id == 24 ? &h->mp_max : m->id == 41 ? &h->ap : m->id == 42 ? &h->ap_max : m->id == 53 ? &h->dp :
                     m->id == 60 ? &h->sr : m->id == 61 ? &h->hr : m->id == 62 ? &h->hp_speed : m->id == 63 ? &h->mp_speed : NULL;
            float v = skill_value(m->init, m->incr, h->buff[b].level);
            if (f) *f += (int)(pct ? *f * v / 100 : v);
        }
}

// 重算數值（0x1002d8d0）：基礎值 → 固定加值 → 回復速度（0x1002fa90）→ 職業倍率（0x1002d510）→ 百分比。加點、升級、狀態變化後呼叫，不回血
static void hero_derive(Actor *h) {
    const int K = 150, str = hero_attr[STR], con = hero_attr[CON], dex = hero_attr[DEX], wis = hero_attr[WISDOM], lv = h->level;
    h->hp_max = 120 + 28 * lv;
    h->mp_max = 140 + 4 * lv;
    h->ap = hero_weapon.ap;
    h->ap_max = hero_weapon.ap_max;
    h->dp = hero_armor_dp;
    h->hr = h->sr = h->hp_speed = h->mp_speed = 0;
    hero_mods(h, 0);
    // 回復速度依職業：HP_SPEED = (Con − a) × HP_MAX × b / 100 / 分母，MP 同理用 Wisdom；分母 0 當 1
    static const int R[6][8] = {{0}, {25, 50, 25, 4, 20, 50, 10, 2}, {30, 60, 15, 2, 10, 40, 5, 2}, {20, 50, 10, 2, 20, 50, 10, 2},
                                {25, 40, 25, 4, 35, 60, 35, 4}, {20, 40, 10, 2, 40, 60, 20, 2}};
    const int *r = R[hero_prof];  // 分母：R[3]=2 → Con + (Lv − c)×2；R[3]=4 → Lv×2 − c + Con（原版兩種寫法）
    int hd = r[3] == 2 ? con + (lv - r[2]) * 2 : lv * 2 - r[2] + con, md = r[7] == 2 ? wis + (lv - r[6]) * 2 : lv * 2 - r[6] + wis;
    h->hp_speed = (con - r[0]) * h->hp_max * r[1] / 100 / (hd ? hd : 1);
    h->mp_speed = (wis - r[4]) * h->mp_max * r[5] / 100 / (md ? md : 1);
    h->ap = h->ap * (str + K) / K;
    h->ap_max = h->ap_max * (str + K) / K;
    h->dp = h->dp * (con + K) / K;
    // 原版鍵名 SR = 命中率、HR = 闪避率（UI_C.dlg 2116/2115 對 Poq.exe 0x54f820 的顯示順序）；這裡 hr = 命中、sr = 閃避（跟怪物表一樣）
    h->hr = (h->hr + 4 * dex) * (dex + K) / K;
    h->sr = (h->sr + 3 * dex) * (dex + K) / K;
    hero_mods(h, 1);
    if (h->hp > h->hp_max) h->hp = h->hp_max;
    if (h->mp > h->mp_max) h->mp = h->mp_max;
}

static void hero_stats(Actor *h, int level) {
    h->level = level;
    hero_derive(h);
    h->hp = h->hp_max;
    h->mp = h->mp_max;
}

// 系統心跳（Poq.exe 註冊的 refresh 指令「每秒一次」→ USER_Refresh）：狀態計時（0x10055e90），再回 HP、MP（0x10054450）
static void hero_second(Actor *h) {
    int changed = 0;
    for (int b = 0; b < h->buff_n; b++)
        if (++h->buff[b].elapsed >= h->buff[b].time && h->buff[b].time >= 0) { h->buff[b--] = h->buff[--h->buff_n]; changed = 1; }
    if (changed) hero_derive(h);
    if (h->act == ACT_DEAD) return;
    int hs = h->hp_speed, ms = h->mp_speed;
    if (!h->fight) {  // 不在戰鬥（FightStatus 0）回得快三倍
        if (hs > 0) hs *= 3;
        if (ms > 0) ms *= 3;
    }
    h->hp += (hs + h->hp_tmp) / 100;
    h->hp_tmp = (hs + h->hp_tmp) % 100;
    h->mp += (ms + h->mp_tmp) / 100;
    h->mp_tmp = (ms + h->mp_tmp) % 100;
    h->hp = h->hp < 0 ? 0 : h->hp > h->hp_max ? h->hp_max : h->hp;  // shortcut: 原版回復是負的（中毒）扣到 0 會死，還沒有負的回復
    h->mp = h->mp < 0 ? 0 : h->mp > h->mp_max ? h->mp_max : h->mp;
}

// 技能的精气消耗（MPU）、作用時間（Time，沒填預設 5 秒）
static int skill_mpu(const SkillRow *r) { return (int)skill_value(r->mpu, r->mpu_inc, r->level); }
static int skill_time(const SkillRow *r) { return r->time < 0 ? 5 : (int)skill_value(r->time, r->time_inc, r->level); }

// 技能打中時（0x1003f590）：有我方状态、身上還沒有，就加上狀態，技能參數跟著生效（0x10037f40、0x1002fe20）
// shortcut: 同類狀態互斥（0x10054d50、0x100588c0）、敌方状态、對敵參數還沒做
static void skill_status(Actor *h, const SkillRow *r) {
    if (r->status < 1 || h->buff_n == 8) return;
    for (int b = 0; b < h->buff_n; b++)
        if (h->buff[b].status == r->status) return;
    h->buff[h->buff_n++] = (Buff){r, r->level, r->status, 0, skill_time(r)};
    hero_derive(h);
    printf("狀態 %d 生效 %d 秒，回復 HP %d MP %d（每秒百分之一點）\n", r->status, skill_time(r), h->hp_speed, h->mp_speed);
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
    // 技能參數（EasyRPG 0x10024xxx 一屬性一個處理）：值 = (技能等級 − 1) × 增量 + 初始值，是否百分比分兩組；
    // 250、251 加到 AP、AP_MAX；ID > 500 拆成 ID−300、ID−299 兩個。攻擊 = (AP + 加值) × (100 + 百分比) / 100
    // shortcut: 只做 250、251（物理攻擊）；252 起的法術攻擊 MAP1~5、269 等還沒做，怪物的技能也還沒算
    float add[2] = {0}, pct[2] = {0};
    const SkillRow *sk = a->casting ? NULL : a->skill;  // shortcut: 左手技能的 250 起參數還沒算
    for (int i = 0; sk && i < sk->mod_n; i++) {
        const SkillMod *m = &sk->mod[i];
        float v = skill_value(m->init, m->incr, sk->level);
        for (int id = m->id > 500 ? m->id - 300 : m->id; id <= (m->id > 500 ? m->id - 299 : m->id); id++)
            if (id == 250 || id == 251) (m->pct ? pct : add)[id - 250] = v;
    }
    int ap = (int)((a->ap + add[0]) * (pct[0] + 100) / 100), ap_max = (int)((a->ap_max + add[1]) * (pct[1] + 100) / 100);
    int dmg = ap + (ap_max > ap ? rand() % (ap_max - ap + 1) : 0);
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
                hero_extra_exp += 10;
                hero_extra_skill += 1;
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
                else if (t->type == 0) {
                    // shortcut: 原版打中處理（0x1003f590）怎麼分「只對自己」的技能還沒查清；先當沒填作用範圍的左手技能不打人
                    if (a->casting) skill_status(a, a->cast);  // 同一狀態已在身上就不會再加
                    if (!a->casting || a->cast->range >= 0) strike(a, a->foe, tick);
                }
                else if (t->type == 2) fx_spawn(t, a, a->foe, tick);
            }
    a->last_frame = f;
    a->act_tick++;
    if (a->act != ACT_DEAD && (!q || a->act_tick >= seq_ticks(q))) {
        if (a->act == ACT_ATTACK) a->cool = a->def->atk_delay;
        if (a->casting) {  // 左手技能放完：招式沒有打中的觸發就在這裡生效，換回右手招式
            skill_status(a, a->cast);
            a->cast = NULL;
            a->atk = a->atk_rh;
            a->casting = 0;
        }
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
        // 主角技能要夠精气才能用（0x1004c2a0），出手時扣（Poq.exe 0x48ffe0 送 0x13a → EasyRPG 0x1003f370）
        if (a->cool <= 0 && (!a->skill || a->mp >= skill_mpu(a->skill))) {
            if (a->skill) a->mp -= skill_mpu(a->skill);
            start_act(a, ACT_ATTACK, a->atk ? a->atk->seq : a->fidle);
        }
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

// ---- 文字：原版字型 font/16Point.fnt：0xB4 起依 Unicode 每字 32 bytes（16 列 × 16 位元，0 = 筆畫）----
static uint8_t *font16;
static SDL_Texture *glyph_tex[65536];
static uint8_t glyph_adv[65536];  // 0 = 還沒量

static SDL_Texture *glyph(uint32_t c) {
    if (c > 0xFFFF) c = '?';
    if (glyph_tex[c]) return glyph_tex[c];
    if (!font16) {
        size_t n;
        font16 = res_file("font", "16Point.fnt", &n);
        if (!font16 || n < 0xB4 + 65536 * 32) { free(font16); font16 = NULL; return NULL; }
    }
    uint32_t px[16 * 16];
    int right = -1;
    for (int y = 0; y < 16; y++) {
        int row = font16[0xB4 + c * 32 + y * 2] << 8 | font16[0xB4 + c * 32 + y * 2 + 1];
        for (int x = 0; x < 16; x++) {
            int ink = !(row >> (15 - x) & 1);
            px[y * 16 + x] = ink ? 0xFFFFFFFFu : 0;
            if (ink && x > right) right = x;
        }
    }
    // 間距：中日韓文字佔滿 16；其他照筆畫寬度 + 2（空白 5）。shortcut: 檔尾每字 2 bytes 的表意義還沒查
    glyph_adv[c] = c >= 0x2E80 ? 16 : right < 0 ? 5 : right + 2;
    glyph_tex[c] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 16, 16);
    SDL_UpdateTexture(glyph_tex[c], NULL, px, 16 * 4);
    SDL_SetTextureBlendMode(glyph_tex[c], SDL_BLENDMODE_BLEND);
    return glyph_tex[c];
}

static uint32_t utf8_next(const char **p) {
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = *s++;
    if (c >= 0xE0 && s[0] && s[1]) { c = (c & 0x0F) << 12 | (s[0] & 0x3F) << 6 | (s[1] & 0x3F); s += 2; }
    else if (c >= 0xC0 && s[0]) { c = (c & 0x1F) << 6 | (s[0] & 0x3F); s += 1; }
    *p = (const char *)s;
    return c;
}

// 原版資料檔的 GBK 文字轉成 UTF-8（字型依 Unicode 排）
static void gbk_utf8(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const uint8_t *p = (const uint8_t *)in; *p && o + 4 < n; p++) {
        uint32_t c = *p;
        if (c >= 0x81 && c <= 0xFE && p[1] >= 0x40 && p[1] <= 0xFE) { c = gbk_uni[(c - 0x81) * 191 + (p[1] - 0x40)]; p++; }
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) { out[o++] = (char)(0xC0 | c >> 6); out[o++] = (char)(0x80 | (c & 0x3F)); }
        else { out[o++] = (char)(0xE0 | c >> 12); out[o++] = (char)(0x80 | (c >> 6 & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[o] = 0;
}

// 一行字的寬度（置中用）
static int text_width(const char *t) {
    int w = 0;
    for (const char *q = t; *q;) { uint32_t c = utf8_next(&q); glyph(c); w += glyph_adv[c <= 0xFFFF ? c : '?']; }
    return w;
}

// 行距 16：原版 m_pDescribe 框高 84 要放 5 行。從 (x, y) 開始寫，超過 maxw 換行；draw = 0 只量高度。回傳用掉的高度
static int draw_text(int x, int y, const char *t, int maxw, Uint32 rgb, int draw) {
    int cx = 0, cy = 0;
    while (*t) {
        uint32_t c = utf8_next(&t);
        if (c == '\n') { cx = 0; cy += 16; continue; }
        SDL_Texture *g = glyph(c);
        if (!g) return 0;
        if (cx + glyph_adv[c] > maxw) { cx = 0; cy += 16; }
        if (draw) {
            SDL_SetTextureColorMod(g, rgb >> 16, rgb >> 8 & 0xFF, rgb & 0xFF);
            SDL_Rect r = {x + cx, y + cy, 16, 16};
            SDL_RenderCopy(ren, g, NULL, &r);
        }
        cx += glyph_adv[c];
    }
    return cy + 16;
}

#ifdef STORY
// ---- 對話（劇情來自原版 EasyRPG.dll，見 story/）：畫面下方的框，說話者、台詞、回答選項 ----
#include "story/story.h"
static int dialog_open;
static int opt_y0[STORY_MAX_OPTIONS], opt_y1[STORY_MAX_OPTIONS];

// shortcut: 原版對話框的介面圖（ResUI）還沒做，先畫半透明的框
static void dialog_draw(void) {
    if (!dialog_open) return;
    const StoryDialog *d = &story_dialog;
    int x = 10, w = vw - 20, tw = w - 20;
    int h = 10 + 18 + draw_text(0, 0, d->text, tw, 0, 0) + 10;
    for (int i = 0; i < d->option_count; i++) h += draw_text(0, 0, d->options[i].text, tw - 20, 0, 0) + 6;
    int y = vh - h - 10;
    SDL_Rect box = {x, y, w, h};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
    SDL_RenderFillRect(ren, &box);
    SDL_SetRenderDrawColor(ren, 160, 130, 80, 255);
    SDL_RenderDrawRect(ren, &box);
    int cy = y + 10;
    cy += draw_text(x + 10, cy, d->speaker, tw, 0xF0D080, 1);
    cy += draw_text(x + 10, cy, d->text, tw, 0xFFFFFF, 1) + 4;
    for (int i = 0; i < d->option_count; i++) {
        opt_y0[i] = cy - 3;
        cy += draw_text(x + 30, cy, d->options[i].text, tw - 20, 0x80E0FF, 1) + 6;
        opt_y1[i] = cy - 3;
        draw_text(x + 10, opt_y0[i] + 3, "\xe2\x96\xb6", 20, 0x80E0FF, 1);  // ▶
    }
}

// 對話中點畫面：點到選項就選；沒有選項時點哪裡都關掉
static void dialog_tap(int py) {
    for (int i = 0; i < story_dialog.option_count; i++)
        if (py >= opt_y0[i] && py < opt_y1[i]) {
            story_choose(i);
            if (!story_dialog.text[0] && !story_dialog.option_count) dialog_open = 0;
            return;
        }
    if (!story_dialog.option_count) dialog_open = 0;
}
#endif

#ifdef STORY
// 點到的 NPC（有事件 ID 的，才能對話）
static Actor *npc_at(int px, int py, int ox, int oy, const Scene *s) {
    for (int i = 1; i < actor_count; i++) {
        Actor *a = &actors[i];
        if (a->mon || !a->spec || !a->spec->event[0]) continue;
        int fx = (int)((a->x - a->y + s->h - 1) * TILE_W / 2) - ox, fy = (int)((a->x + a->y) * TILE_H / 2) - oy;
        int dx = px - fx, dy = py - (fy - 30);
        if (abs(dx) <= 36 && dy >= -70 && dy <= 40) return a;
    }
    return NULL;
}
#endif

// ---- 原版介面：ini/DialogInfo.ini 每個 [介面] 一段；每個元件「名稱=種類,x,y,寬,高,旗標,ID,提示,圖,…」----
// 種類：1 背景、2 按鈕（圖的各幀 = 平常、指著、按下）、6 進度條、9 靜態圖。圖是 ResUI 的編號：
// 多半是動畫定義（Res00 格式，取第 0 序列第 0 幀），少數直接是 xbm
typedef struct { char name[32]; int type, x, y, w, h, flags, img, sb[4]; } UiCtrl;  // sb：文字框捲軸的軌道、上、下、滑塊圖
typedef struct { char name[48]; int w, h, count; UiCtrl c[128]; } UiDialog;

static int ui_load(const char *section, UiDialog *out) {
    size_t n;
    uint8_t *d = res_file("ini", "DialogInfo.ini", &n);
    if (!d) return 0;
    if (n && d[0] == 0x80) n = res_unxor(d, n);
    memset(out, 0, sizeof *out);
    int on = 0;
    for (char *l = strtok((char *)d, "\r\n"); l; l = strtok(NULL, "\r\n")) {
        if (l[0] == '[') { if (on) break; on = !strncmp(l + 1, section, strlen(section)) && l[1 + strlen(section)] == ']'; continue; }
        char *eq = strchr(l, '=');
        if (!on || !eq) continue;
        *eq = 0;
        int f[13] = {0}, k = 0;
        for (char *p = eq + 1; k < 13 && p; p = strchr(p, ','), p = p ? p + 1 : NULL) f[k++] = atoi(p);
        if (!strcmp(l, "Dialog")) { out->w = f[3]; out->h = f[4]; continue; }
        if (out->count == 128) break;
        UiCtrl *c = &out->c[out->count++];
        snprintf(c->name, sizeof c->name, "%s", l);
        c->type = f[0]; c->x = f[1]; c->y = f[2]; c->w = f[3]; c->h = f[4]; c->flags = f[5]; c->img = f[8];
        for (int j = 0; j < 4; j++) c->sb[j] = f[9 + j];
    }
    snprintf(out->name, sizeof out->name, "%s", section);
    free(d);
    return out->count;
}

static const Sprite *ui_image(int img, int frame) {
    const Anim *a = anim_get((uint32_t)img);
    if (!a) return image((uint32_t)img);  // 直接是 xbm
    int i = 0;
    for (int s = 0; s < a->count; s++)
        for (int f = 0; f < a->seqs[s].count; f++, i++)
            if (i == frame) return image(a->seqs[s].frames[f].ref);
    return a->seqs[0].count ? image(a->seqs[0].frames[0].ref) : NULL;
}

static UiCtrl *ui_find(UiDialog *d, const char *name) {
    for (int i = 0; i < d->count; i++)
        if (!strcmp(d->c[i].name, name)) return &d->c[i];
    return NULL;
}

// 進度條（種類 6，原版 Poq.exe CProgress 第 12 格 0x401690）：旗標 0x4000 = 直的、由下往上；否則橫的、從左往右
static void ui_bar(const UiCtrl *c, int ox, int oy, int value, int max) {
    const Sprite *s = ui_image(c->img, 0);
    if (!s || max <= 0) return;
    int v = value < 0 ? 0 : value > max ? max : value;
    SDL_Rect src = {0, 0, s->w, s->h};
    if (c->flags & 0x4000) { src.y = s->h - s->h * v / max; src.h = s->h - src.y; }
    else src.w = s->w * v / max;
    SDL_Rect dst = {ox + c->x + src.x, oy + c->y + src.y, src.w, src.h};
    SDL_RenderCopy(ren, s->tex, &src, &dst);
}

// 畫整個介面（進度條另外畫）；(ox, oy) = 介面左上角
static void ui_draw(UiDialog *d, int ox, int oy) {
    for (int i = 0; i < d->count; i++) {
        const UiCtrl *c = &d->c[i];
        if (c->type == 6 || c->type == 10 || !c->img || !(c->flags & 9)) continue;  // 種類 10（文字欄）的圖不是底圖（4326 是勾勾）。shortcut: 旗標 0x1、0x8 都沒有當成「一開始隱藏」（升級提示、選取框、成套裝備欄都是），還沒對原版
        draw(ui_image(c->img, 0), ox + c->x, oy + c->y);
    }
}

// 主畫面介面，位置照原版（Poq.exe 0x5d3xxx 建立介面那段）：
// 底部主列 JM51_状态界面_中间 = 底部置中；隊伍面板 JM51_状态界面_主控 = 右緣、貼在主列上方
static UiDialog bar, party, enemy;

// 視窗：原版 人物属性（JM52）開在畫面中線左邊（x = W/2 − 寬），技能（JM53）、道具（JM54）開在中線右邊（x = W/2）；
// y 都是垂直置中再往上 50（Poq.exe 0x4ff3xx、0x5004xx、0x4feexx）。左右各一個，可以同時開
typedef struct { UiDialog d; int open; } Win;
static Win win_left, win_right;

#define JM52 "JM52_\xc8\xcb\xce\xef\xca\xf4\xd0\xd4\xbd\xe7\xc3\xe6"  // JM52_人物属性界面（GBK）

// 人物屬性兩種模式（Poq.exe 0x554370）：0 = 加點（m_pPanel、m_pAttrib、m_pOK、m_pBonus、m_pBonusTitle），1 = 顯示 m_pInfo。
// 要 0 但沒有剩餘屬性點時改成 1
static int role_mode = 1;
static void role_set_mode(int mode) {
    if (hero_extra_exp <= 0) mode = 1;
    role_mode = mode;
    for (int i = 0; i < win_left.d.count; i++) {
        UiCtrl *c = &win_left.d.c[i];
        int up = !strcmp(c->name, "m_pPanel") || !strncmp(c->name, "m_pAttrib", 9) || !strncmp(c->name, "m_pOK", 5) || !strncmp(c->name, "m_pBonus", 8);
        if (up || !strcmp(c->name, "m_pInfo")) c->flags = (c->flags & ~9) | ((up == (mode == 0)) ? 9 : 0);
    }
}

// 加點按鈕 m_pOK[i]（Poq.exe 0x553b00）：剩餘點數 −1、送 0x200 訊息讓第 i 項屬性 +1，再照目前模式重整（點數用完就回顯示模式）。
// shortcut: 原版按住會越加越快（間隔 400 → 50 毫秒），這裡一下一點
static void role_add_point(int i) {
    if (hero_extra_exp <= 0 || i < 0 || i > 5) return;
    hero_extra_exp--;
    hero_attr[i]++;
    hero_derive(&actors[0]);
    role_set_mode(role_mode);
}

static void win_toggle(Win *w, const char *section) {
    if (w->open && !strcmp(w->d.name, section)) { w->open = 0; return; }
    w->open = ui_load(section, &w->d) > 0;
    if (w->open && !strcmp(section, JM52)) role_set_mode(1);  // 開人物屬性是顯示模式（Poq.exe 0x554080）
}

static int win_x(const Win *w) { return w == &win_left ? vw / 2 - w->d.w : vw / 2; }
static int win_y(const Win *w) { return (vh - w->d.h) / 2 - 50; }

// 點到哪個按鈕（按鈕在 ini 裡寬高是 0，大小取圖的第 0 幀）
static const UiCtrl *ui_button_at(const UiDialog *d, int ox, int oy, int x, int y) {
    for (int i = 0; i < d->count; i++) {
        const UiCtrl *c = &d->c[i];
        if (c->type != 2 || !c->img || !(c->flags & 9)) continue;
        const Sprite *s = ui_image(c->img, 0);
        if (s && x >= ox + c->x && x < ox + c->x + s->w && y >= oy + c->y && y < oy + c->y + s->h) return c;
    }
    return NULL;
}

#define JM53 "JM53_\xbc\xbc\xc4\xdc\xbd\xe7\xc3\xe6"                      // JM53_技能界面
#define JM54 "JM54_\xb5\xc0\xbe\xdf\xbd\xe7\xc3\xe6"                      // JM54_道具界面
#define JM55 "JM55_\xd7\xb0\xb1\xb8\xbd\xe7\xc3\xe6"                      // JM55_装备界面

static int skill_page;  // 原版 SkillData 的 CurSkillPage
static int skill_mode = 1;  // 技能視窗：0 = 升级打開的加點模式（m_pAdd 可按、顯示技能點），1 = 主列打開的檢視（Poq.exe 0x560950）
static SkillRow hero_skill[20];
static int hero_skill_n = -1;

// 技能升一級（EasyRPG.dll 0x1003b9e0）：有技能點、沒到上限、等级需求增量 × 目前等級 + 等级需求 ≤ 主角等級
static int skill_can_up(const SkillRow *r) {
    return hero_extra_skill > 0 && r->level < r->max_level && r->req_add * r->level + r->req <= actors[0].level;
}

static void skill_add(int i) {
    int k = skill_page * 4 + i;
    if (skill_mode || i < 0 || i > 3 || k >= hero_skill_n || !skill_can_up(&hero_skill[k])) return;
    hero_skill[k].level++;
    hero_extra_skill--;
}

static void skills_load(void) {
    if (hero_skill_n < 0) hero_skill_n = skill_rows(hero_prof, hero_skill, 20);
}

// 主列的右手、左手技能（原版 SkillData 的 CurRSkillID、CurLSkillID）：hero_skill 的第幾個，-1 = 空。
// shortcut: 開局兩手都是斩棘披荆（第 0 個，左右鍵位 2）是猜的，原版開新遊戲怎麼設還沒找到
static int cur_skill[2];

// 點敵人用右手技能的招式（技能表 Attack ID）。shortcut: 左手技能（止血回春等）還不能施放；手機點一下對應哪一手是自訂的
static void hero_skill_apply(Actor *h) {
    skills_load();
    int k = cur_skill[0];
    h->skill = k >= 0 && k < hero_skill_n ? &hero_skill[k] : NULL;
    h->atk = h->skill ? method_find(skill_attack(h->skill->key)) : NULL;
}

// 放左手技能（原版右鍵 → Poq.exe 0x48ffe0 手 = 1）：技能等級 > 0、精气夠，招式開始才扣精气
// shortcut: 武器限制（W_Type）、作用範圍（RangeMin/Max）還沒檢查，原地對目前目標放
static void hero_cast(Actor *h) {
    int k = cur_skill[1];
    if (k < 0 || k >= hero_skill_n || h->act || h->casting) return;
    const SkillRow *r = &hero_skill[k];
    const Method *m = method_find(skill_attack(r->key));
    if (r->level < 1 || h->mp < skill_mpu(r) || !m) return;
    h->mp -= skill_mpu(r);
    h->cast = r;
    h->casting = 1;
    h->atk_rh = h->atk;
    h->atk = m;
    start_act(h, ACT_ATTACK, m->seq);
}

// 快捷技能視窗（Poq.exe 0x504d90 開 JM514 = 右手、0x504b40 開 JM515 = 左手；0x564360 填格子）：
// 依序放進左右鍵位等於這隻手或 2（兩手都行）、等級 > 0 的技能，第 n 個放 m_pItem[n]。
// shortcut: 原版還要 SpeType == 0（存檔欄位，來源沒找到，先當都是 0）；視窗位置原版 0x583790 還沒翻，先貼在該手的按鈕上方
#define JM514 "JM514_\xbf\xec\xbd\xdd\xbc\xbc\xc4\xdc\xbd\xe7\xc3\xe6"
#define JM515 "JM515_\xbf\xec\xbd\xdd\xbc\xbc\xc4\xdc\xbd\xe7\xc3\xe6"
static UiDialog pick;
static int pick_hand = -1, pick_skill[24], pick_n;

static void pick_open(int hand) {
    if (pick_hand == hand) { pick_hand = -1; return; }
    skills_load();
    pick_hand = ui_load(hand ? JM515 : JM514, &pick) > 0 ? hand : -1;
    pick_n = 0;
    for (int k = 0; k < hero_skill_n && pick_n < 24; k++)
        if ((hero_skill[k].hand == hand || hero_skill[k].hand == 2) && hero_skill[k].level > 0) pick_skill[pick_n++] = k;
}

static void pick_pos(int *x, int *y) {
    char key[16];
    snprintf(key, sizeof key, "m_pSkill[%d]", pick_hand);
    const UiCtrl *b = ui_find(&bar, key);
    const Sprite *s = b ? ui_image(b->img, 0) : NULL;
    int bx = (vw - bar.w) / 2 + (b ? b->x : 0);
    *x = pick_hand ? bx : bx + (s ? s->w : 0) - pick.w;
    *y = vh - bar.h - pick.h;
}

// 技能圖示置中畫在格子 c 上
static void skill_icon(const UiCtrl *c, int ox, int oy, int k) {
    const Sprite *bg = ui_image(c->img, 0), *ic = k >= 0 && k < hero_skill_n ? ui_image(hero_skill[k].icon, 0) : NULL;
    if (bg && ic) draw(ic, ox + c->x + (bg->w - ic->w) / 2, oy + c->y + (bg->h - ic->h) / 2);
}

// 按鈕名稱 → 開的視窗（主列和人物屬性視窗裡的 m_pRole／m_pSkill／m_pProp 同名同作用）
static void ui_command(const char *name) {
    if (!strcmp(name, "m_pRole")) win_toggle(&win_left, JM52);
    else if (!strcmp(name, "m_pSkill")) { skill_mode = 1; win_toggle(&win_right, JM53); }
    else if (!strcmp(name, "m_pProp")) win_toggle(&win_right, JM54);
    else if (!strncmp(name, "m_pPage[", 8)) skill_page = atoi(name + 8) - 1;  // 技能頁籤：m_pPage[1]~[5] = 第 0~4 頁（[6] 藏著）
    else if (!strncmp(name, "m_pSkill[", 9)) pick_open(name[9] == '1');  // 主列 m_pSkill[0] 右手、[1] 左手
    else if (!strcmp(name, "m_pEquip")) win_toggle(&win_left, JM55);  // 道具視窗的装备：原版開在中線左邊（Poq.exe 0x54ad87）
}

// 多行文字框（種類 3）的捲軸：ini 第 10~13 欄是軌道、上箭頭、下箭頭、滑塊的圖。字超過框高才有捲軸。
// shortcut: 捲軸位置照常見做法（框內右緣、箭頭在上下兩端），還沒對原版 UIWndEx.dll；只有箭頭能點，拖滑塊還沒做
static int info_top;  // 人物屬性 m_pInfo 捲到第幾行
typedef struct { SDL_Rect track, up, down; } Scroll;
static int text_scroll(const UiCtrl *c, int ox, int oy, Scroll *out) {
    const Sprite *t = ui_image(c->sb[0], 0), *u = ui_image(c->sb[1], 0), *d = ui_image(c->sb[2], 0);
    if (!t || !u || !d) return 0;
    int x = ox + c->x + c->w - t->w, y = oy + c->y;
    *out = (Scroll){{x, y, t->w, c->h}, {x + (t->w - u->w) / 2, y, u->w, u->h}, {x + (t->w - d->w) / 2, y + c->h - d->h, d->w, d->h}};
    return 1;
}

static int in_rect(const SDL_Rect *r, int x, int y) { return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h; }

// 介面吃掉的點擊回 1（不再拿去走路、打怪）。shortcut: 主列只有按鈕吃點擊，主列底圖空白處還是會走路
static int hud_tap(int x, int y) {
    if (pick_hand >= 0) {  // 快捷技能視窗開著：點格子換技能；點哪裡都收起來
        int px, py;
        pick_pos(&px, &py);
        const UiCtrl *c = ui_button_at(&pick, px, py, x, y);
        int i = c && !strncmp(c->name, "m_pItem[", 8) ? atoi(c->name + 8) : -1;
        if (i >= 0 && i < pick_n) {
            cur_skill[pick_hand] = pick_skill[i];
            if (pick_hand == 0) hero_skill_apply(&actors[0]);
        }
        pick_hand = -1;
        return 1;
    }
    Win *ws[2] = {&win_left, &win_right};
    for (int i = 0; i < 2; i++) {
        Win *w = ws[i];
        if (!w->open) continue;
        int ox = win_x(w), oy = win_y(w);
        if (x < ox || x >= ox + w->d.w || y < oy || y >= oy + w->d.h) continue;
        UiCtrl *info = ui_find(&w->d, "m_pInfo");
        Scroll sc;
        if (info && info->flags & 9 && text_scroll(info, ox, oy, &sc)) {
            if (in_rect(&sc.up, x, y)) { info_top--; return 1; }  // 範圍在畫的時候夾住
            if (in_rect(&sc.down, x, y)) { info_top++; return 1; }
        }
        const UiCtrl *c = ui_button_at(&w->d, ox, oy, x, y);
        int role = !strcmp(w->d.name, JM52);
        if (c && !strcmp(c->name, "m_pCClose")) w->open = 0;
        else if (c && role && !strncmp(c->name, "m_pOK[", 6)) role_add_point(atoi(c->name + 6));
        else if (c && !strcmp(w->d.name, JM53) && !strncmp(c->name, "m_pAdd[", 7)) skill_add(atoi(c->name + 7));
        else if (c && role && !strcmp(c->name, "m_pUpgrade")) {  // 升级（Poq.exe 0x5538a0）：切到加點模式，右邊開技能
            role_set_mode(0);
            skill_mode = 0;
            if (!win_right.open || strcmp(win_right.d.name, JM53)) win_toggle(&win_right, JM53);
        } else if (c) ui_command(c->name);
        return 1;
    }
    const UiCtrl *c = bar.count ? ui_button_at(&bar, (vw - bar.w) / 2, vh - bar.h, x, y) : NULL;
    if (!c) return 0;
    ui_command(c->name);
    return 1;
}

// 原版文字表 UI_C.dlg 第 id 筆轉成 UTF-8，拿掉顏色標記「[數字 … ]」。shortcut: 標記裡的顏色還沒照原版上色，都用控制項的字色
static void ui_text(int id, char *out, size_t cap) {
    char g[512], u[1024];
    out[0] = 0;
    if (!res_dlg("UI_C.dlg", id, g, sizeof g)) return;
    gbk_utf8(g, u, sizeof u);
    size_t k = 0;
    for (const char *p = u; *p && k + 1 < cap; p++) {
        if (*p == '[' && p[1] >= '0' && p[1] <= '9') { p++; continue; }
        if (*p == ']') continue;
        out[k++] = *p;
    }
    while (k && out[k - 1] == ' ') k--;
    out[k] = 0;
}

// 人物屬性視窗的 m_pInfo（Poq.exe 0x54f820）：25 行「標籤 數值」，順序和標籤編號照原版
static void hero_info(const Actor *h, char *out, size_t cap) {
    static const int label[25] = {2105, 2106, 2107, 2108, 2109, 2110, 2111, 2112, 2117, 2118, 2119, 2116, 2115,
                                  2113, 2156, 2157, 2158, 2159, 2160, 2114, 2121, 2122, 2123, 2124, 2125};
    int next = h->level < exp_levels ? exp_table[h->level] - h->exp : 0;
    // 數值：經驗、下一級、聲望、生命、精氣、力量、體質、敏捷、悟性、魅力、智慧、命中、閃避、攻擊、五行攻擊 ×5、防禦、五行防禦 ×5
    // shortcut: 聲望、五行攻防還沒有，先 0
    int v[25][2] = {{h->exp}, {next}, {0}, {h->hp, h->hp_max}, {h->mp, h->mp_max}, {hero_attr[STR]}, {hero_attr[CON]}, {hero_attr[DEX]},
                    {hero_attr[SAVVY]}, {hero_attr[CHARM]}, {hero_attr[WISDOM]}, {h->hr}, {h->sr}, {h->ap, h->ap_max},
                    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {h->dp}};
    static const unsigned pair = 1 << 3 | 1 << 4 | 1 << 13 | 1 << 14 | 1 << 15 | 1 << 16 | 1 << 17 | 1 << 18;  // 「%d/%d」的行
    size_t k = 0;
    out[0] = 0;
    for (int i = 0; i < 25 && k < cap; i++) {
        char t[128];
        ui_text(label[i], t, sizeof t);
        k += pair >> i & 1 ? snprintf(out + k, cap - k, "%s%d/%d\n", t, v[i][0], v[i][1])
                           : snprintf(out + k, cap - k, "%s%d\n", t, v[i][0]);
    }
}

// 人物屬性視窗的 m_pDescribe（Poq.exe 0x554600）：名字、职业、性别、年龄、等级
static void hero_describe(const Actor *h, char *out, size_t cap) {
    char l[4][64], prof[64], gender[16];
    for (int i = 0; i < 4; i++) ui_text(2101 + i, l[i], sizeof l[i]);
    ui_text(2131 + hero_prof, prof, sizeof prof);
    ui_text(2197 + hero_gender, gender, sizeof gender);
    // shortcut: 名字原版來自開新遊戲畫面的輸入（0x94c358），那個畫面還沒做，先用劇情主角的名字
    snprintf(out, cap, "%s\n%s%s\n%s%s\n%s%d\n%s%d\n", "扶苏", l[0], prof, l[1], gender, l[2], hero_age, l[3], h->level);
}

// 技能視窗一頁 4 列（Poq.exe 0x560f30 → 0x55f900）：m_pItem[i] 圖示（ResID）、m_pItemInfo[i] 名稱、m_pOK[i] 等級。
// 等級 0 的技能畫成灰的（原版沒學會的那列加 0x40000 旗標）。加點模式下面顯示「技能升级点数：」2202 和剩餘技能點。
// shortcut: 0x40000 當成灰色、m_pAdd 不能按時照畫，還沒對 UIWndEx.dll；左右鍵圖示還沒做
static void skill_draw(UiDialog *d, int wx, int wy) {
    skills_load();
    char key[16], name[128], g[64];
    if (!skill_mode) {
        UiCtrl *bt = ui_find(d, "m_pSkillBonusTitle"), *bn = ui_find(d, "m_pSkillBonus");
        if (bt) { ui_text(2202, name, sizeof name); draw_text(wx + bt->x, wy + bt->y + 2, name, bt->w, 0xBECDCE, 1); }
        if (bn) {
            snprintf(name, sizeof name, "%d", hero_extra_skill);
            draw_text(wx + bn->x + (bn->w - text_width(name)) / 2, wy + bn->y + (bn->h - 16) / 2, name, bn->w, 0xBECDCE, 1);
        }
    }
    for (int i = 0; i < 4 && skill_page * 4 + i < hero_skill_n; i++) {
        const SkillRow *r = &hero_skill[skill_page * 4 + i];
        snprintf(key, sizeof key, "m_pItem[%d]", i);
        UiCtrl *slot = ui_find(d, key);
        const Sprite *bg = slot ? ui_image(slot->img, 0) : NULL, *ic = ui_image(r->icon, 0);
        if (slot && bg && ic) {
            int grey = r->level ? 255 : 110;
            SDL_SetTextureColorMod(ic->tex, grey, grey, grey);
            draw(ic, wx + slot->x + (bg->w - ic->w) / 2, wy + slot->y + (bg->h - ic->h) / 2);
            SDL_SetTextureColorMod(ic->tex, 255, 255, 255);
        }
        snprintf(key, sizeof key, "m_pItemInfo[%d]", i);
        UiCtrl *info = ui_find(d, key);
        if (info && res_dlg("name_C.dlg", r->name_id, g, sizeof g)) {
            gbk_utf8(g, name, sizeof name);
            draw_text(wx + info->x + (info->w - text_width(name)) / 2, wy + info->y + (info->h - 16) / 2, name, info->w, 0xBECDCE, 1);
        }
        snprintf(key, sizeof key, "m_pOK[%d]", i);
        UiCtrl *lv = ui_find(d, key);
        if (lv) {
            snprintf(name, sizeof name, "%d", r->level);
            draw_text(wx + lv->x + (lv->w - text_width(name)) / 2, wy + lv->y, name, lv->w, 0xD5D5D5, 1);
        }
    }
}

static void hud_draw(const Actor *hero) {
    static int tried;
    if (!tried) {
        tried = 1;
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xd0\xbc\xe4", &bar);      // JM51_状态界面_中间（GBK）
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xf7\xbf\xd8", &party);    // JM51_状态界面_主控
        ui_load("JM515_\xb5\xd0\xc8\xcb\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6", &enemy);    // JM515_敌人状态界面
    }
    // 敵人狀態：上方置中、距頂 20（原版位置）；顯示目前打的怪物名字和血。shortcut: 原版是滑鼠指著的對象，手機沒有指著，先用攻擊目標
    const Actor *t = hero->foe;
    if (enemy.count && t && t->mon && t->act != ACT_DEAD) {
        int ex = (vw - enemy.w) / 2, ey = 20;
        UiCtrl *nm = ui_find(&enemy, "m_pName"), *bl = ui_find(&enemy, "m_pBlood");
        if (nm) {
            char u[96];
            gbk_utf8(t->mon->title, u, sizeof u);
            draw_text(ex + nm->x + (nm->w - text_width(u)) / 2, ey + nm->y + 3, u, nm->w, 0xBECDCE, 1);
        }
        if (bl) ui_bar(bl, ex, ey, t->hp, t->hp_max);
    }
    if (!bar.count) return;
    int ox = (vw - bar.w) / 2, oy = vh - bar.h;
    ui_draw(&bar, ox, oy);
    UiCtrl *red = ui_find(&bar, "m_pRed"), *blue = ui_find(&bar, "m_pBlue");
    if (red) ui_bar(red, ox, oy, hero->hp, hero->hp_max);
    if (blue) ui_bar(blue, ox, oy, hero->mp, hero->mp_max);
    for (int i = 0; i < 2; i++) {
        char key[16];
        snprintf(key, sizeof key, "m_pSkill[%d]", i);
        const UiCtrl *c = ui_find(&bar, key);
        if (c) skill_icon(c, ox, oy, cur_skill[i]);
    }
    if (party.count) {
        int px = vw - party.w, py = oy - party.h;
        for (int i = 0; i < party.count; i++) {  // 只有主角：畫背景和第 0 格。shortcut: 頭像（G_PicID）、隊友還沒做
            const UiCtrl *c = &party.c[i];
            if (c->type == 6 || !c->img || !(c->flags & 9) || (strchr(c->name, '[') && !strstr(c->name, "[0]"))) continue;
            // 升級提示、狀態圖示：有事才顯示（原版 ini 的狀態圖示五格都叫 m_pRoleStatus[0][x]，不能靠 [0] 分）
            if (!strncmp(c->name, "m_pUpgrade", 10) || !strncmp(c->name, "m_pRoleStatus", 13)) continue;
            draw(ui_image(c->img, 0), px + c->x, py + c->y);
        }
        UiCtrl *hp = ui_find(&party, "m_pRoleHP[0]"), *mp = ui_find(&party, "m_pRoleMP[0]");
        if (hp) ui_bar(hp, px, py, hero->hp, hero->hp_max);
        if (mp) ui_bar(mp, px, py, hero->mp, hero->mp_max);
    }
    if (win_left.open) {
        int wx = win_x(&win_left), wy = win_y(&win_left);
        ui_draw(&win_left.d, wx, wy);
        UiCtrl *info = ui_find(&win_left.d, "m_pInfo");
        if (info && info->flags & 9) {
            char t[2048];
            hero_info(hero, t, sizeof t);
            SDL_Rect clip = {wx + info->x, wy + info->y, info->w, info->h};
            Scroll sc;
            int bar = text_scroll(info, wx, wy, &sc), tw = info->w - 4 - (bar ? sc.track.w : 0);
            int rows = (draw_text(0, 0, t, tw, 0, 0) - 1) / 16, fit = (info->h - 4) / 16;  // draw_text 量到的高度多算一行
            if (info_top > rows - fit) info_top = rows - fit;
            if (info_top < 0) info_top = 0;
            SDL_RenderSetClipRect(ren, &clip);
            draw_text(clip.x + 2, clip.y + 2 - info_top * 16, t, tw, 0xBECDCE, 1);
            if (bar && rows > fit) {
                const Sprite *th = ui_image(info->sb[3], 0);
                draw(ui_image(info->sb[0], 0), sc.track.x, sc.track.y);
                draw(ui_image(info->sb[1], 0), sc.up.x, sc.up.y);
                draw(ui_image(info->sb[2], 0), sc.down.x, sc.down.y);
                int span = sc.down.y - (sc.up.y + sc.up.h) - (th ? th->h : 0);
                if (th) draw(th, sc.track.x + (sc.track.w - th->w) / 2, sc.up.y + sc.up.h + span * info_top / (rows - fit));
            }
            SDL_RenderSetClipRect(ren, NULL);
        }
        // 裝備：主角開局的武器、布衣，圖示畫在 m_pItem[UI_Pos − 30]（劇情檔 UI_Pos：武器 32 右手、布衣 31 盔甲）
        const struct { const Item *it; int slot; } eq[] = {{&hero_weapon, 2}, {&hero_armor, 1}};
        for (int i = 0; !strcmp(win_left.d.name, JM55) && i < 2; i++) {
            char slot[16];
            snprintf(slot, sizeof slot, "m_pItem[%d]", eq[i].slot);
            UiCtrl *c = ui_find(&win_left.d, slot);
            const Sprite *bg = c ? ui_image(c->img, 0) : NULL, *s = eq[i].it->icon ? ui_image(eq[i].it->icon, 0) : NULL;
            if (c && s) draw(s, wx + c->x + ((bg ? bg->w : s->w) - s->w) / 2, wy + c->y + ((bg ? bg->h : s->h) - s->h) / 2);
        }
        if (!strcmp(win_left.d.name, JM52) && role_mode == 0) {  // 加點面板：m_pAttrib[i] =「%s[2%d]」標籤 + 數值（Poq.exe 0x554d66 起）
            static const int label[6] = {2110, 2111, 2112, 2117, 2118, 2119};
            char key[16], l[64], t[96];
            for (int i = 0; i < 6; i++) {
                snprintf(key, sizeof key, "m_pAttrib[%d]", i);
                UiCtrl *a = ui_find(&win_left.d, key);
                if (!a) continue;
                ui_text(label[i], l, sizeof l);
                snprintf(t, sizeof t, "%s%d", l, hero_attr[i]);  // shortcut: [2 …] 顏色標記先用控制項字色
                draw_text(wx + a->x, wy + a->y + (a->h - 16) / 2, t, a->w, 0xBECDCE, 1);
            }
            UiCtrl *bt = ui_find(&win_left.d, "m_pBonusTitle"), *bn = ui_find(&win_left.d, "m_pBonus");
            if (bt) { ui_text(2201, l, sizeof l); draw_text(wx + bt->x, wy + bt->y + 2, l, bt->w, 0xBECDCE, 1); }  // 属性升级点数：
            if (bn) {
                snprintf(t, sizeof t, "%d", hero_extra_exp);
                draw_text(wx + bn->x + (bn->w - text_width(t)) / 2, wy + bn->y + (bn->h - 16) / 2, t, bn->w, 0xBECDCE, 1);
            }
        }
        UiCtrl *desc = ui_find(&win_left.d, "m_pDescribe");
        if (desc) {
            char t[512];
            hero_describe(hero, t, sizeof t);
            draw_text(wx + desc->x + 2, wy + desc->y + 2, t, desc->w - 4, 0xBECDCE, 1);
        }
    }
    if (win_right.open) {
        int wx = win_x(&win_right), wy = win_y(&win_right);
        ui_draw(&win_right.d, wx, wy);
        if (!strcmp(win_right.d.name, JM53)) skill_draw(&win_right.d, wx, wy);
        UiCtrl *money = ui_find(&win_right.d, "m_pMoney");  // 道具：原版把錢數交給 m_pMoney 顯示（Poq.exe 0x54db42），「金钱：%d」是提示文字
        if (money) {
            char t[16];
            snprintf(t, sizeof t, "%d", hero_money);
            draw_text(wx + money->x + (money->w - text_width(t)) / 2, wy + money->y + (money->h - 16) / 2, t, money->w, 0xBECDCE, 1);
        }
    }
    if (pick_hand >= 0) {
        int px, py;
        pick_pos(&px, &py);
        ui_draw(&pick, px, py);
        for (int i = 0; i < pick_n; i++) {
            char key[16];
            snprintf(key, sizeof key, "m_pItem[%d]", i);
            const UiCtrl *c = ui_find(&pick, key);
            if (c) skill_icon(c, px, py, pick_skill[i]);
        }
    }
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
#ifdef STORY
    story_register_all();
    if (pos < 2 && story_start_room()) scn = story_start_room();  // 沒指定場景 = 新遊戲，從原版的開場房間開始
#endif

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
    hero_extra_exp = 10 * (hero->level - 1), hero_extra_skill = hero->level - 1;  // 測試用 --level：當成從 1 級升上來，點數照原版給
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
#ifdef STORY
                if (dialog_open) { dialog_tap(py); continue; }  // 對話中不走路
#endif
                if (hud_tap(px, py)) continue;
                // 手機沒有右鍵：點主角自己 = 右鍵放左手技能（左手技能多是對自己的增益）
                if (abs(px - vw / 2) <= 36 && py - (vh / 2 - 30) >= -70 && py - (vh / 2 - 30) <= 40) { hero_cast(hero); continue; }
                Actor *m = monster_at(px, py, ox, oy, &scene);  // 點怪物 = 攻擊它；點 NPC = 說話；點其他地方 = 按住走路
                if (m) hero->foe = m;
#ifdef STORY
                else if ((m = npc_at(px, py, ox, oy, &scene))) {
                    int npc = story_find(story_room(alias), m->spec->event);
                    if (npc) { story_talk(npc); dialog_open = 1; }
                    else printf("劇情裡找不到 NPC %s\n", m->spec->event);
                }
#endif
                else pointer_down = 1;
            }
            // 空白鍵／手把 A：打最近的怪物
            else if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) ||
                     (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A)) {
                float best = hero->def ? hero->def->seek : 10;
                for (int i = 1; i < actor_count; i++)
                    if (actors[i].mon && alive(&actors[i]) && cell_dist(hero, &actors[i]) <= best) { best = cell_dist(hero, &actors[i]); hero->foe = &actors[i]; }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {  // 右鍵：對點到的怪物放左手技能
                Actor *m = monster_at(e.button.x, e.button.y, ox, oy, &scene);
                if (m) hero->foe = m;
                hero_cast(hero);
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
            if (tick * TICK_MS / 1000 != (tick - 1) * TICK_MS / 1000) hero_second(hero);
            for (int i = 0; i < actor_count; i++) {
                if (actors[i].cool > 0) actors[i].cool--;
                if (actors[i].fight > 0 && !actors[i].foe) actors[i].fight--;
            }
            if (hero->act == ACT_DEAD) {  // shortcut: 原版死亡後的處理（回城、掉經驗）還沒做，先 3 秒後原地復活
                if (++hero->dead_ticks > 180) { hero->act = ACT_NONE; hero->dead_ticks = 0; hero->hp = hero->hp_max; hero->mp = hero->mp_max; }
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
            static int fight_loaded;
            if (!fight_loaded) {  // 戰鬥資料也等開場畫面好了才讀（網頁版不拖慢開場）
                fight_loaded = 1;
                hero_skill_apply(hero);
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
        hud_draw(hero);
#ifdef STORY
        dialog_draw();
#endif
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
