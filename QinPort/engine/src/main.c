// 秦殤原生引擎（第 2 階段）：讀原版資源，畫出場景與主角，主角可以走動。
// 用法：qin [遊戲資料夾] [場景.Scn] [--640] [--shot 檔名.bmp（畫一幀存檔後結束，給測試用）]
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "res.h"

#define HERO_ROLE 1201       // 主角（遊俠），ObjMdl/RoleMdl.txt 的圖形 ID
#define ACT_IDLE 0           // 動作起點（序列 = 動作 + 方向），見 RoleMdl.txt「平時休息/平時走路」
#define ACT_WALK 8
#define FRAME_MS 70          // shortcut: 幀延遲先用固定值，動畫檔裡的延遲欄位還沒確認
#define HERO_SPEED 4.0f      // 格/秒

// 畫面方向(0=上，順時針每 45°) → 動畫方向編號。shortcut: 從靜態圖推測的，實際走動後再校正
static const int DIR_MAP[8] = {0, 1, 2, 3, 6, 5, 4, 7};

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

// ---- 主角 ----
typedef struct {
    float x, y;  // 格子座標（可有小數）
    int dir, moving;
    Uint32 anim_start;
} Hero;

static void hero_draw(const Role *r, const Hero *h, int sx, int sy, Uint32 now) {
    int seq = (h->moving ? ACT_WALK : ACT_IDLE) + DIR_MAP[h->dir];
    if (seq >= r->seq_count || !r->frames[seq]) return;
    int frame = (now - h->anim_start) / FRAME_MS % r->frames[seq];
    for (int i = 0; i < r->part_count; i++) draw_frame(r->shadow[i], seq, frame, sx, sy, 1);
    const uint8_t *order = r->order[seq] + frame * r->part_count;
    for (int i = 0; i < r->part_count; i++)
        if (order[i] < r->part_count) draw_frame(r->body[order[i]], seq, frame, sx, sy, 0);
}

// 往畫面方向 (sdx, sdy) 走 dist 格；格子座標：畫面右 = (+x, -y)，畫面下 = (+x, +y)
static void hero_step(Hero *h, const Scene *s, float sdx, float sdy, float dist) {
    float len = sqrtf(sdx * sdx + sdy * sdy);
    if (len < 0.01f) return;
    sdx /= len;
    sdy /= len;
    // 畫面上 1 格寬 64、高 32，換算回格子方向
    float gx = sdx / 2 + sdy, gy = -sdx / 2 + sdy, glen = sqrtf(gx * gx + gy * gy);
    gx = gx / glen * dist;
    gy = gy / glen * dist;
    // 能走就走；撞牆時試著只沿一個軸滑動
    if (scene_walkable(s, (int)(h->x + gx), (int)(h->y + gy))) { h->x += gx; h->y += gy; }
    else if (scene_walkable(s, (int)(h->x + gx), (int)h->y)) h->x += gx;
    else if (scene_walkable(s, (int)h->x, (int)(h->y + gy))) h->y += gy;
    int oct = (int)lroundf(atan2f(sdx, -sdy) / (float)(M_PI / 4));
    h->dir = (oct + 8) % 8;
    h->moving = 1;
}

int main(int argc, char **argv) {
    const char *game = "game", *scn = "CHENG001.Scn", *shot = NULL;
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--640")) { vw = 640; vh = 480; }
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (pos++ == 0) game = argv[i];
        else scn = argv[i];
    }
    res_init(game);

    Scene scene;
    if (!scene_load(&scene, scn)) { fprintf(stderr, "載入場景失敗：%s/scene/%s\n", game, scn); return 1; }
    Role role;
    if (!role_load(&role, HERO_ROLE, 1)) { fprintf(stderr, "載入主角失敗\n"); return 1; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Window *win = SDL_CreateWindow("秦殤", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, vw, vh,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, vw, vh);  // 視窗多大都等比例縮放，畫面內容固定 vw x vh
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);

    Hero hero = {scene.start_x + 0.5f, scene.start_y + 0.5f, 4, 0, 0};
    Uint64 last = SDL_GetPerformanceCounter(), fps_t = last;
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
        float dt = (float)(now - last) / SDL_GetPerformanceFrequency();
        if (dt > 0.1f) dt = 0.1f;
        last = now;
        Uint32 ticks = SDL_GetTicks();

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
        if (pointer_down) { mx = px - vw / 2; my = py - (vh / 2 - 40); }  // 主角腳底在畫面中央，指向身體中段附近
        int was_moving = hero.moving;
        hero.moving = 0;
        if (mx * mx + my * my > (pointer_down ? 64 : 0.01f)) hero_step(&hero, &scene, mx, my, HERO_SPEED * dt);
        if (hero.moving != was_moving) hero.anim_start = ticks;

        // 主角腳底的世界像素；鏡頭跟著主角
        int hx = (int)((hero.x - hero.y + scene.h - 1) * TILE_W / 2), hy = (int)((hero.x + hero.y) * TILE_H / 2);
        int ox = hx - vw / 2, oy = hy - vh / 2;

        // ---- 畫面 ----
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
        // 物件已依深度(x+y)排序；主角在經過自己的深度時插進去畫
        float hero_depth = hero.x + hero.y;
        int hero_drawn = 0;
        for (int i = 0; i <= scene.object_count; i++) {
            const Object *o = i < scene.object_count ? &scene.objects[i] : NULL;
            if (!hero_drawn && (!o || o->x + o->y + 1 > hero_depth)) {
                hero_draw(&role, &hero, hx - ox, hy - oy, ticks);
                hero_drawn = 1;
            }
            if (!o || !o->m) continue;
            int sx = world_x(&scene, o->x, o->y) + TILE_W / 2 - ox, sy = world_y(&scene, o->x, o->y) + TILE_H / 2 - oy;
            for (int p = 0; p < o->m->part_count; p++) {
                const Part *pt = &o->m->parts[p];
                if (pt->anim) draw_frame(anim_get(pt->gid), 0, ticks / FRAME_MS, sx + pt->dx, sy + pt->dy, 0);  // Res00 動畫，播第一個序列
                else draw(image(pt->gid), sx + pt->dx, sy + pt->dy);
            }
        }
        if (shot) {
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, vw, vh, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
            SDL_SaveBMP(surf, shot);
            SDL_FreeSurface(surf);
            running = 0;
        }
        SDL_RenderPresent(ren);

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

    for (uint32_t i = 0; i < slot_cap; i++)
        if (slots[i].s.tex) SDL_DestroyTexture(slots[i].s.tex);
    role_free(&role);
    scene_free(&scene);
    SDL_Quit();
    return 0;
}
