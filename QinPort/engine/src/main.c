// 秦殤原生引擎（第 2 階段）：讀原版資源，畫出場景與主角，主角可以走動。
// 用法：qin [遊戲資料夾] [場景別名] [--640] [--shot 檔名.bmp] [--warp N] [--at X,Y]
// --shot：畫面穩定後存一張圖就結束；--warp：一開始站到第 N 個觸發區（測場景切換用）；--at：站到指定格
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

// 全域圖片 ref（高 8 位幀號）。ref 不會是 0xFFxxxxxx，所以磁磚用那段當 key
static const Sprite *image(uint32_t ref) {
    Sprite *s = slot_find(ref);
    if (s->tex) return s;
    return s->w < 0 ? NULL : upload(s, res_image(ref, &s->w, &s->h));
}

static const Sprite *tile(Lib *tiles, uint16_t i) {
    Sprite *s = slot_find(0xFF000000u | i);
    if (s->tex) return s;
    if (s->w < 0) return NULL;
    uint32_t size;
    uint8_t *d = lib_read(tiles, i, &size);
    uint32_t *px = d ? xbm_decode(d, size, &s->w, &s->h) : NULL;
    free(d);
    return upload(s, px);
}

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
typedef struct {
    float x, y;       // 格子座標（可有小數）
    int dir, moving, speed;
    int idle, walk;   // 動作起點序列（角色表「平時休息/平時走路」）
    Uint32 act_tick;  // 目前動作已經過幾拍（換動作時歸零）
    const Role *role;
    const NpcSpec *spec;  // NPC 的擺放資料；主角為 NULL
    float tx, ty;     // AI 目標格
    int has_target, path_i, rest;  // rest = 還要休息幾拍
} Actor;

static void actor_draw(const Actor *a, int sx, int sy) {
    const Role *r = a->role;
    if (!r) return;
    int seq = (a->moving ? a->walk : a->idle) + DIR_MAP[a->dir];
    if (seq >= r->seq_count || !r->frames[seq]) return;
    // 各部件的幀延遲相同，拿第一個有資料的部件當節奏
    int frame = 0;
    for (int i = 0; i < r->part_count; i++)
        if (r->body[i] && seq < r->body[i]->count) { frame = frame_at(&r->body[i]->seqs[seq], a->act_tick); break; }
    frame %= r->frames[seq];
    for (int i = 0; i < r->part_count; i++) draw_frame(r->shadow[i], seq, frame, sx, sy, 1);
    const uint8_t *order = r->order[seq] + frame * r->part_count;
    for (int i = 0; i < r->part_count; i++)
        if (order[i] < r->part_count) draw_frame(r->body[order[i]], seq, frame, sx, sy, 0);
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
    int oct = (int)lroundf(atan2f(sdx, -sdy) / (float)(M_PI / 4));
    h->dir = (oct + 8) % 8;
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
    a->idle = d->idle;
    a->walk = d->walk;
    a->speed = d->speed;
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

static int cmp_actor_depth(const void *a, const void *b) {
    const Actor *x = *(Actor *const *)a, *y = *(Actor *const *)b;
    float d = (x->x + x->y) - (y->x + y->y);
    return d < 0 ? -1 : d > 0;
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
    // 外觀快取跟著場景換：先留主角的，再重建 NPC
    const RoleDef *hd = roledef_by_graphic(HERO_GRAPHIC);
    roles_clear();
    if (hd) actor_setup(hero, hd);
    spawn_npcs(scene);
    const Entry *en = scene_entry(scene, entry_id);
    if (en) {
        hero->x = en->x + 0.5f;
        hero->y = en->y + 0.5f;
        for (int o = 0; o < 8; o++)
            if (DIR_MAP[o] == en->dir) hero->dir = o;  // shortcut: 假設入口方向跟動畫方向同編號，還沒跟原版對照
    }
    hero->moving = 0;
    hero->act_tick = 0;
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

int main(int argc, char **argv) {
    const char *game = "game", *scn = "xianyang", *shot = NULL;
    int pos = 0, warp = -1, at_x = -1, at_y = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--640")) { vw = 640; vh = 480; }
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--warp") && i + 1 < argc) warp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &at_x, &at_y);
        else if (pos++ == 0) game = argv[i];
        else scn = argv[i];
    }
    res_init(game);

    World world;
    if (!world_load(&world)) fprintf(stderr, "讀不到 EasyRPG.dll 的房間資料，不能切換場景\n");
    char alias[64];
    snprintf(alias, sizeof alias, "%s", scn);
    char *dot = strrchr(alias, '.');
    if (dot && !strcasecmp(dot, ".Scn")) *dot = 0;
    if (!roledefs_load()) { fprintf(stderr, "讀不到角色表 ObjMdl/RoleMdl.txt\n"); return 1; }
    Scene scene = {0};
    Actor *hero = &actors[0];
    hero->dir = 4;
    if (!enter_scene(&scene, hero, alias, -1)) return 1;
    if (!hero->role) { fprintf(stderr, "載入主角失敗\n"); return 1; }
    int room = world_find(&world, alias), armed = 0;  // armed：離開觸發區後才會再觸發，避免一進場就被送回去
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
    int frames = 0, running = 1, pointer_down = 0, px = 0, py = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f)
                SDL_SetWindowFullscreen(win, SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            // 滑鼠與觸控（SDL 預設會把觸控也送成滑鼠事件）：按住就一直往指的地方走
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { pointer_down = 1; px = e.button.x; py = e.button.y; }
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
            actor_tick(hero, &scene, mx, my, ++tick);
            // 腳步聲：每 max((15 - A) * 2, 10) 拍一次（Poq.exe 0x48704f），80 號音效
            int sp = hero->speed < 0 ? 0 : hero->speed > 59 ? 59 : hero->speed, step = (15 - SPEED_A[sp]) * 2;
            if (hero->moving && tick % (step < 10 ? 10 : step) == 0) audio_sfx(80);  // shortcut: 原版某些地面改用 81 號，地面類型還沒對應
            for (int i = 1; i < actor_count; i++) npc_think(&actors[i], &scene, tick);
            // 踩到觸發區：事件 "go <出口>" 對應目前房間的出口，切到目的房間的入口
            const Trigger *t = trigger_at(&scene, hero);
            if (!t) armed = 1;
            else if (armed && room >= 0 && !strncmp(t->event, "go ", 3)) {
                const Room *r = &world.rooms[room];
                for (int i = 0; i < r->exit_count; i++) {
                    const Exit *x = &world.exits[r->first_exit + i];
                    if (strcmp(x->dir, t->event + 3) || x->to < 0) continue;
                    if (enter_scene(&scene, hero, world.rooms[x->to].alias, x->entry)) {
                        room = x->to;
                        scene_music(world.rooms[room].alias, music, sizeof music);
                        audio_music(music);
                        printf("進入 %s\n", world.rooms[room].alias);
                    }
                    armed = 0;
                    break;
                }
            }
        }

        // 主角腳底的世界像素；鏡頭跟著主角
        int hx = (int)((hero->x - hero->y + scene.h - 1) * TILE_W / 2), hy = (int)((hero->x + hero->y) * TILE_H / 2);
        int ox = hx - vw / 2, oy = hy - vh / 2;

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
