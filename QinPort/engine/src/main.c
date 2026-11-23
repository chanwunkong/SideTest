#include <limits.h>
// 秦殤原生引擎（第 2 階段）：讀原版資源，畫出場景與主角，主角可以走動。
// 用法：qin [遊戲資料夾] [場景別名] [--640] [--shot 檔名.bmp] [--warp N] [--at X,Y]
// --shot：畫面穩定後存一張圖就結束；--warp：一開始站到第 N 個觸發區（測場景切換用）；--at：站到指定格
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "res.h"
#include "audio.h"
#include "gbk.h"
extern int debug_cells;  // viewport.c
#ifdef STORY
#include "story/story.h"
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define HERO_GRAPHIC "1201"  // 主角（遊俠）在 ObjMdl/RoleMdl.txt 的圖形 ID；外觀、動作、速度都照角色表
#define TICK_MS 16           // 原版固定節拍：每 16ms 更新一次遊戲邏輯（Poq.exe 0x449f50）

// 遊戲速度（Poq.exe 0x448a70）：設定的速度 0~5 → 31 − 3×速度（開局 21，Config 預設速度 2 → 25）。
// 節拍固定 16ms 不受影響；原版只有 0x449c90（每 5000/值 拍）和 0x566920（字幕捲動）讀它。
// 原版在連線客戶端（DAT_00942f28 == 2）不改，單機沒有這情況。
// DAT_00639f6c：單機發現陷阱自動暫停選項。
// shortcut: 設定介面未接，暫由--pause-traps提供；JM31完成後改接原版設定值。
static int trap_auto_pause, scene_paused;
// Rec模式0=關、2=回放（過場播放器 rec_begin／rec_end 設）。
static int scene_record_mode;
static int death_auto_pause, death_keep_mode = 1;
static int rec_on;  // 過場播放中（原版 Rec 模式 2，Poq.exe 0x472e60）：其他角色凍結不畫、不收輸入
static int scene_objects_dirty;

static void trap_detected_message(void);
static void character_death_message(void);
static void ui_text(int id, char *out, size_t cap);
static int game_speed_ms = 21;
static void game_speed_set(int level) { game_speed_ms = 31 - 3 * level; }

// 小地圖格座標換算（Poq.exe 0x45ad80）。map_rows = 原版地圖 +8，
// origin = 原版全域 DAT_00941288；由小地圖呼叫端提供，不假設它的設定值。
// 同時供鏡頭投影原點使用。
static inline SDL_Point viewport_grid_point(int x, int y, int map_rows, int origin) {
    int odd = x % 2;  // 原版是有號餘數，負奇數為 -1
    int half = map_rows / 2;
    int px = (half - y + origin + x) / 2;
    return (SDL_Point){y % 2 == 0 ? px + odd : px - odd + 1,
                       y + 2 + half - origin + x};
}

// 小地圖點轉場景格（Poq.exe 0x45e460）：奇數 Y 先加1再除2，差值仍向零截斷。
// 鏡頭捲動用此換算重算快取；map_rows 為 NULL 表示沒有場景。
static inline SDL_Point viewport_point_grid(SDL_Point p, const int *map_rows, int origin) {
    if (!map_rows) return (SDL_Point){0, 0};
    int half, delta;
    if (p.y % 2 == 0) {
        half = p.y / 2 - p.x;
        delta = p.y - *map_rows;
    } else {
        half = (p.y + 1) / 2 - p.x;
        delta = p.y - *map_rows - 1;
    }
    return (SDL_Point){delta / 2 + p.x, origin + half - 1};
}

// 鏡頭中心格（Poq.exe 0x45b6b0）：原版刻意用高/32 加 X、寬/64 加 Y。
// base 是鏡頭捲動更新的格座標快取。
static inline SDL_Point viewport_center_grid(SDL_Point base, int width, int height) {
    return (SDL_Point){base.x + height / 32, base.y + width / 64};
}

// 鏡頭像素邊界（Poq.exe 0x45b780）：先上界、再下界，最後向零取偶數。
// 設位、方向捲動、跟隨與解析度改變共用。
static inline SDL_Point viewport_clamp(SDL_Point p, int width, int height, SDL_Rect world) {
    if (p.y + height >= world.y + world.h) p.y = world.y + world.h - 1 - height;
    if (p.y < world.y) p.y = world.y;
    if (p.x + width >= world.x + world.w) p.x = world.x + world.w - 1 - width;
    if (p.x < world.x) p.x = world.x;
    p.x -= p.x % 2;
    p.y -= p.y % 2;
    return p;
}

// 原版速度對照表（Poq.exe 0x5f21d0/0x5f23b0/0x5f22c0）：速度 s → 每 B 拍移動 A + C/10 像素
static const uint8_t SPEED_A[60] = {0,1,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15,16,16,16,17,17,17,18,18,18,19,19,19};
static const uint8_t SPEED_B[60] = {90,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static const uint8_t SPEED_C[60] = {0,0,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,0,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7,0,3,7};

// 畫面方向(0=上，順時針每 45°) → 動畫方向編號。動畫方向從「右」開始逆時針排（手機實測校正）
static const int DIR_MAP[8] = {2, 1, 0, 7, 6, 5, 4, 3};

typedef struct { SDL_Texture *tex; int w, h; } Sprite;

static SDL_Renderer *ren;
static int vw = 800, vh = 600;
static SDL_Texture *role_surface;
static int role_surface_w, role_surface_h;

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

// 貼圖留著跨場景（回到剛離開的場景不用重新解碼）：一般圖的 key 是全域資源編號，不會撞；磁磚 key = 0xFF000000 | 封包標記（tile_tag，依封包路徑）<< 16 | 序號，
// 各場景的磁磚封包不會撞號。貼圖總量超過 TEX_CAP 才整批清（換場景時檢查，enter_scene）
static size_t tex_bytes;
#define TEX_CAP ((size_t)160 << 20)
static char tile_tags[255][256];
static int tile_tag_n;
static int tile_tag(const char *path) {
    for (int i = 0; i < 255 && tile_tags[i][0]; i++) if (!strcmp(tile_tags[i], path)) return i + 1;
    for (int i = 0; i < 255; i++) if (!tile_tags[i][0]) { snprintf(tile_tags[i], sizeof tile_tags[i], "%s", path); tile_tag_n = i + 1; return i + 1; }
    return 0;  // 標記表滿了：呼叫端要整批清貼圖
}
static void role_surface_free(void) {
    SDL_DestroyTexture(role_surface);
    role_surface = NULL;
    role_surface_w = role_surface_h = 0;
}
static void cache_clear(void) {
    role_surface_free();
    for (uint32_t i = 0; i < slot_cap; i++)
        if (slots[i].s.tex) SDL_DestroyTexture(slots[i].s.tex);
    free(slots);
    slots = NULL;
    slot_cap = slot_used = 0;
    tex_bytes = 0;
    memset(tile_tags, 0, sizeof tile_tags);
    tile_tag_n = 0;
}

static const Sprite *upload(Sprite *s, uint32_t *px) {
    if (!px) { s->w = -1; return NULL; }
    s->tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, s->w, s->h);
    tex_bytes += (size_t)s->w * s->h * 4;
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

// 全域圖片 ref（高 8 位幀號、低 18 位資源號）= CResMng 取圖 0x4b0ba0（懶載入、快取）。ref 不會是 0xFFxxxxxx，所以磁磚用那段當 key
static const Sprite *image(uint32_t ref) {
    Sprite *s = slot_find(ref);
    if (s->tex) return s;
    if (s->w < 0) return NULL;
    res_waiting = 0;
    return upload_or_wait(s, res_image(ref, &s->w, &s->h));
}

// 地上道具圖（ResUI 28×28 不透明、黑底）：黑色當透明。shortcut: 原版地上道具怎麼畫還沒查，看到原版畫法再改
static const Sprite *ground_image(uint32_t ref) {
    Sprite *s = slot_find(0xFE000000u | ref);
    if (s->tex) return s;
    if (s->w < 0) return NULL;
    res_waiting = 0;
    uint32_t *px = res_image(ref, &s->w, &s->h);
    for (int i = 0; px && i < s->w * s->h; i++)
        if (!(px[i] & 0xFFFFFF)) px[i] = 0;
    return upload_or_wait(s, px);
}

static const Sprite *tile(Lib *tiles, uint16_t i) {
    if (!tiles->tag) tiles->tag = tile_tag(tiles->path);
    Sprite *s = slot_find(0xFF000000u | (uint32_t)(tiles->tag & 0xFF) << 16 | i);
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
// 網頁載入畫面（web/shell.html 的 qinProgress）：n < 0 = 正在載入場景（蓋上載入畫面）；n > 0 = 還有 n 張圖在下載（畫面顯示已下載幾張、剩幾張）；n == 0 = 好了，載入畫面消失
EM_JS(void, js_status_raw, (int n), { if (window.qinProgress) window.qinProgress(n); });
#else
static void js_status_raw(int n) { (void)n; }
#endif
static int status_shown = -2;  // 最近一次送出的數字；-1（換場景）之後歸 -2，讓主迴圈下一幀一定再送一次（否則場景圖都在快取時載入畫面不會消失）
static void js_status(int n) { status_shown = n < 0 ? -2 : n; js_status_raw(n); }
// 觸控確認震動（iOS 沒有 navigator.vibrate 就略過）
#ifdef __EMSCRIPTEN__
EM_JS(void, js_vibrate, (int ms), { if (navigator.vibrate) navigator.vibrate(ms); });
#else
static void js_vibrate(int ms) { (void)ms; }
#endif

// 片頭／片尾影片（原版 Poq.exe 0x440830 播 movie/<名>.avi；狀態機 0x449f50：Logo1~3 → Start（PlayIntro）→ 主選單、End 在片尾）。
// 網頁：蓋一個 <video>（webserve.py 把 game/movie/*.avi 轉成 mp4：Start/logo 的 H.264 直接搬、End 的 Indeo5 重新編碼），
// 播完或點一下畫面（原版按鍵跳過）才回來。原生版沒有影片解碼，直接略過。
// shortcut: 原生版不播；原版 Logo2／Logo3 檔案不在遊戲資料夾，略過。
#ifdef __EMSCRIPTEN__
EM_ASYNC_JS(void, movie_play, (const char *name), {
    const n = UTF8ToString(name);
    await new Promise((done) => {
        const v = document.createElement('video');
        v.src = '/movie/' + n + '.mp4';
        v.playsInline = true;
        v.autoplay = true;
        v.style.cssText = 'position:fixed;inset:0;width:100%;height:100%;background:#000;object-fit:contain;z-index:9';
        const end = () => { v.remove(); done(); };
        v.onended = end;
        v.onerror = end;  // 沒有這支影片或轉檔失敗：直接進遊戲
        v.onclick = end;
        v.play().catch(() => { v.muted = true; v.play().catch(end); });  // 瀏覽器擋有聲自動播放時改靜音
        document.body.appendChild(v);
    });
});
// PlayIntro（POQ.ini）：第一次玩才播 Start，播過記在 localStorage（原版播完把 ini 寫 0）；網址 ?intro=1 強制再看
EM_JS(int, intro_wanted, (void), {
    const f = new URLSearchParams(location.search).get('intro') === '1';
    if (!f && localStorage.getItem('qinIntro')) return 0;
    localStorage.setItem('qinIntro', '1');
    return 1;
});
#else
static void movie_play(const char *name) { (void)name; }
static int intro_wanted(void) { return 0; }
#endif

static Uint8 draw_alpha = 255;  // 不是 255 時半透明（分身）

static void draw(const Sprite *s, int x, int y) {
    if (!s || x >= vw || y >= vh || x + s->w <= 0 || y + s->h <= 0) return;
    SDL_Rect r = {x, y, s->w, s->h};
    if (draw_alpha != 255) SDL_SetTextureAlphaMod(s->tex, draw_alpha);
    SDL_RenderCopy(ren, s->tex, NULL, &r);
    if (draw_alpha != 255) SDL_SetTextureAlphaMod(s->tex, 255);
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
typedef struct { const SkillRow *skill; int level, status, elapsed, time, side, patch; } Buff;  // side 2 = 光環接收者兼參數目標；side 1 = 別人放在身上的（套「是否对敌」的參數）；
// 打中觸發的狀態沒有技能參數（skill = NULL）；patch = 中毒每秒扣的量（Patch_62）

typedef struct {
    int key, id, min, max, p6, p7, enabled, p8, p9;
    const SkillRow *row;
} ActorSkill;
#define MAX_ACTOR_SKILLS 128

// 每個精靈部件各有30筆歷史；位置是世界64分格，Frame保存採樣當時的幀。
typedef struct { int x, y; Frame frame; } IllusionUnit;
typedef struct {
    IllusionUnit unit[30]; // 最新在0，最舊在count-1，取代原版雙向鏈
    int count, start, steps, delta, ticks, repeat, interval;
} Illusion;

// Poq.exe 0x4a1780：只在首次建立時設定參數；重複開啟不清歷史或改參數。
static void illusion_create(Illusion **out, int start, int end, int delta, int repeat) {
    if (*out) return;
    // shortcut: 原版除0會崩潰；拒絕無效配置，若遊戲資料出現此配置再核對呼叫契約。
    if (!delta || (repeat && !(end - start))) return;
    int steps = (end - start) / delta;
    if (steps < 1 || steps > 30) return;
    Illusion *v = calloc(1, sizeof *v);
    if (!v) return;
    v->start = start; v->steps = steps; v->delta = delta; v->repeat = repeat;
    v->interval = repeat ? 30 / steps + 1 : 1;
    *out = v;
}

// Poq.exe 0x4a2280：不重複時每5次才採樣，容量為步數；重複時每次採樣、容量30。
static void illusion_sample(Illusion *v, int x, int y, const Frame *f) {
    if (!v->repeat && ++v->ticks % 5) return;
    int limit = v->repeat ? 30 : v->steps;
    if (v->count < limit) v->count++;
    memmove(v->unit + 1, v->unit, (size_t)(v->count - 1) * sizeof *v->unit);
    v->unit[0] = (IllusionUnit){x, y, *f};
}

// Poq.exe 0x4a1c90：清歷史、釋放容器、清指標；靜態Anim資源仍由共用快取持有。
static void illusion_clear(Illusion **v) { free(*v); *v = NULL; }

typedef struct Spr Spr;
static const Frame *spr_frame(const Spr *s);

typedef struct Actor {
    float x, y;       // 格子座標（可有小數）
    int dir, facing, moving, speed; // dir為畫面方向，facing保存原版有號面向值
    int fight_speed, speed_threshold, dynamic_state;
    int dynamic_active, reverse_playback;
    // shortcut: 啟用byte反值保存使零初始化沿用現有啟用契約；完整建立／移除接齊後核對啟停來源。
    int activity_disabled, activity_latched; // 首次更新鎖存與路徑就緒分開。
    int dynamic_command[3], dynamic_flag_snapshot;
    int dynamic_enter_tick, interpolation_left, dynamic_reported_state;
    float interpolation_x, interpolation_y, interpolation_dx, interpolation_dy;
    int jump_half, dust_x, dust_y;
    int dynamic_cache_x, dynamic_cache_y, dynamic_counter;
    int dynamic_destination_x, dynamic_destination_y;
    int dynamic_pending, dynamic_path, dynamic_aux, dynamic_path_result;
    int dynamic_new_path;           // +0x3d5：新路徑旗標
    int path_walk;                  // 動態11走逐格路徑（actor_walk_step）；鍵盤像素行走不設
    int walk_wait, walk_block;      // +0x3f5等待拍、+0x3f6同組阻擋重試
    int npc_timer, follow_id, npc_rest_stop; // CNpc閒置：記錄+0x20計時、跟隨目標句柄(+0x78)、+0x1c被改成4
    int npc_home_x, npc_home_y, npc_wait, npc_idle; // CNpc +0x5e2／+0x5e6（-1未設、-2停用）、+0x5ea攻擊間隔、+0x5ee閒置拍
    unsigned char path_buf[500];    // +0xcb：250筆(x,y)反向路徑，[0]=終點、[count-1]=起點
    const Scene *bound_scene;
    int grid_x, grid_y, grid_valid, occupancy_removed, object_dead, object_kind;
    unsigned char auto_talked;      // 走近自動說話已觸發過：離開 AUTO_TALK_LEAVE 格才清（auto_talk_step）
    int walk_blocked;               // 按住走路（actor_step）上一次嘗試完全走不動：站著不播走路動作
    unsigned char removal_mode;
    int corpse_visible;
    uint16_t model_type;
    int corrected_x, corrected_y;
    int animation_target, animation_dir;
    int shown_seq, shown_ready, previous_target;
    uint16_t head_height;
    int head_height_pending;
    Uint32 direction_clock;
    int move_a, move_b, move_c, move_remaining, walk_delay_adjust;
    int scale; // 目前比例與第4修正；模板指定比例時不追逐修正
    struct { int value, time; } modifier[20];
    int modifiers_dirty, trail_mode;
    unsigned char state[128]; // 原版角色狀態byte；特殊主分派仍待補齊
    int status_countdown, original_group, counter_skill, status_aux;
    void *interaction;
    int behavior_unused, debug_rectangle;
    int command_world_x, command_world_y;
    int hp_percent, behavior[4];
    int battle_return;
    Spr *effect[3];
    int effect_speed, effect_applied;
    unsigned char last_status; // 最後修改的狀態ID；重算原版卻拿它與狀態值比較
    int aura_status, aura_range, aura_skill;
    Illusion *illusion[MAX_ROLE_PARTS];
    int view_link_type; // 關聯模型 +0x2c；type2/3 繪圖時世界 X/Y 暫加128
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
    int body_seq, body_start, body_end, body_ready;
    // shortcut: MethodMain仍在播放後消費，保存播放前觀測值；完整場景更新次序接齊後核對並移除。
    unsigned action_serial, sampled_action_serial;
    int animation_sample_pending, sampled_frame, sampled_finished;
    int act_frame, frame_tick, act_done;  // 單次動作播到第幾幀、這幀停了幾拍、播完了沒
    unsigned char sequence_delay_bias; // 原版byte +0x345，非0時每幀延遲另加2
    int adj, extra;                // 這次動作每幀延遲的調整（序列 +0x14）、攻速多出來要補到間隔的拍數
    int flags;                     // 招式設的旗標（type 8）：0x10 = 怪物不選它當目標、0x200 = 在空中、0x4000 = 打它的會停手
    int z, vz, zmax;               // 離地高度、每拍升降、上限（+0x2d7、+0x2db、+0x2df）
    const Method *atk;             // 普通攻擊的招式
    const SkillRow *skill;         // 攻擊用的技能（主角右手；怪物是這一招選的，打完清掉）
    const SkillRow *last_skill;    // 怪物上一招
    const SkillRow *cast;          // 正在放的左手技能（casting 時有效；1 = 還沒打中，2 = 打中觸發過了）
    const SkillRow *want;          // 要放、正在走過去的左手技能
    const Method *atk_rh;          // 放左手技能時先存起來的右手招式
    int casting;
    int hp_speed, mp_speed, hp_tmp, mp_tmp;  // 每秒回復（百分之一點）與累積的零頭（EasyRPG 0x10054450）
    Buff buff[8];
    int buff_n;
    int ex[100];  // 沒有獨立欄位的屬性，依屬性代號（Invincible 20、MAP1~5 43~52、MDP1~5 54~58、B_* 64~98）
    struct Actor *foe;             // 攻擊目標
    struct Actor *owner;           // 召喚出來的：召喚者
    int side;                      // 隊伍：0 = 主角這邊（主角、他的召喚兽、馴服的、陷阱、分身），1 = 怪物
    int search_link; // 關聯識別與第10修正（索敵篩選）
    uint32_t object_id; // 無劇情handle的場景物件也有獨立的32位識別。
    int body_size;                 // 動態物件占位大小（模板大小<7取1，否則減5）
    int grp;                       // AI 組（+0x339）：0～99 玩家組、10000～60000 NPC 組；敵對看它（near.c is_foe）
    int seek_adjust, view_range;
    int trap_revealed, trap_group; // 陷阱發現旗標（原版+0x9f的反值）及最後主人組別
    int trap_state, trap_timer, trap_fired, trap_trig, trap_uses;  // CTrapBase +0x97 狀態0待命1觸發動畫2冷卻、+0x77 計數、+0xa7 已放招、+0xbb 偵測到敵人、+0xb7 剩餘次數
    int copied, copy[12];
    int cloned;                    // 分身（IsCloned = 1）：技能列複製自施放者（0x1003df00），AI 像隊友一樣挑技能
    int knock, stun;               // 擊退中、硬直剩幾拍（+0x42f、+0x433：不能動、不能出手）
    int print_x, print_y, foot;    // 上一個腳印的位置（+0x3c9）、這次踩哪隻腳（+0x3c5）
    int ai_tick;                  // 怪物 AI 計數（+0xbb，起始隨機 0~99）
    int cool, fight, dead_ticks, dead_phase;   // 攻擊冷卻（拍）、戰鬥姿態剩幾拍、死了幾拍、屍體階段（CDeadObj +0xa3 - 1）
    int part_kind;                 // 屍體物件種類：0 一般角色、1 CPartDeadObj、2 CFlyPartDeadObj
    unsigned part_hide;            // 部件群（Role 部件）不畫的遮罩：bit i = 第 i 個部件（原版 CSprGroup 部件樣式 0）
    int part_wait;                 // CDeadObj +0x9f：階段0的倒數（起始 25／30 停住不播），飛散部件靠它數 20 拍
    int fly_speed;                 // CFlyPartDeadObj +0x101 飛散速度（10~14；+0xfd 播到一半旗標全映像沒人讀，不留）
    int blood_id, blood_x, blood_y; // CPartDeadObj +0x93 血跡動畫、+0x7b／+0x7f 疊圖偏移
    Uint32 blood_start;            // 血跡動畫起始拍
    int sh;                        // 劇情物件（原版 EasyRPG 的人物；0 = 沒有）
    int remembered[4], remember_delay; // 原版記住命令與取消後延遲
    int command[4];                // 待執行命令，第一格 -1 表示沒有（原版 +0x5b6）
    char init_name[64], display_name[256]; // SInit事件ID為原始bytes；顯示名稱為UTF-8
    struct { unsigned char left, total, floating; char text[1024]; } overhead[8];
    int display_number;           // 顯示名稱附加編號；不屬於SInit事件ID
    ActorSkill skills[MAX_ACTOR_SKILLS];
    int sel;  // 在選人名單裡（原版旗標 0x100）
    int arrow_kind;  // +0x52d：箭種（原版預設 1；0 當 1）；技能參數 291 設、msg 0x531 設，unit_spawn 依它換飛行物
    int rec;   // 過場播放器建的角色（rec_on 時只有它們動、被畫）
    int team;  // 隊友（劇情的 Friend 2 隊員）：1 起的槽號，0 = 不是；屬性、職業供 team_derive 用（劇情人物的 Str／Con／Dex／Savvy／Charm／Wisdom、IsActorNPC）
    int team_attr[6], team_prof;
    const void *good_role; int good_seq, good_frame;  // 最後一次整幀圖都齊的（序列、幀）：圖還在下載時沿用它，人不會消失
    float pvx, pvy;  // 上一拍的位置（畫面補間用，見主迴圈 ‘畫面補間’）
    void *eq;  // 隊友身上的裝備（Item[INV_N]，story 子物件 UI_Pos 30~40 的道具；NULL = 沒有裝備資料）
    int clone_equip;  // 分身（0x1003df00 複製施放者的裝備與技能）：1 = 施放者是主角，數值照主角裝備算；2 = 隊友施放（沒有裝備模型）
    int skill_n, selected, right_id, left_id, range_adjust, selection_pending;
    int template_skill[MAX_ACTOR_SKILLS], template_skill_n;
    int candidate[4];
    int aim_valid, aim_x, aim_y, aim_range;
    struct Actor *aim_actor; // 本次施法的明確對象；地點施法不能沿用上一次foe
    unsigned char layer;           // +0x66；目前角色建立入口使用地面層1
    unsigned char weapon_type;     // +0x513；更新入口只保存byte，不改技能或外觀
} Actor;

// Poq.exe 0x575cb0：有資源且有序列的每個本體部件都各自建立，不含影子。
static void actor_illusion_begin(Actor *a) {
    const Role *r = a->role;
    for (int i = 0; r && i < r->part_count; i++)
        if (r->body[i] && r->body[i]->count > 0)
            illusion_create(&a->illusion[i], 2, 12, 2, 1);
}

// Poq.exe 0x576b90：對有效部件逐一清殘影；場景釋放另清所有槽，避免換外觀後遺留。
static void actor_illusion_end(Actor *a) {
    const Role *r = a->role;
    for (int i = 0; r && i < r->part_count; i++)
        if (r->body[i] && r->body[i]->count > 0) illusion_clear(&a->illusion[i]);
}
static const Scene *unit_scene;
static int actor_bind_scene(Actor *a, const Scene *s);
static int actor_place(Actor *a, int x, int y, int relocate);
static int actor_destroy(Actor *a);
static void actor_unoccupy(Actor *a);
static void actors_illusion_clear(void);
static void actor_illusion_draw(const Actor *a);

static void actor_modifier_changed(Actor *a, int index, int value);
static void actor_effect(Actor *a, int status, int on, int skill);
static void actor_status_notify(Actor *a, int status, int on, int skill);
static void actor_status_countdown(Actor *a);
static void actor_dynamic_exit(Actor *a);
static void actor_dynamic_rest(Actor *a);
static void actor_dynamic_command(Actor *a, int state, int p0, int p1, int p2);
static int actor_command(Actor *a, int command, int p0, int p1, int p2, int cancel, int remember);
static int input_party_targets(const Actor *a);
static int input_sel_remove(Actor *a);
static int input_sel_has(const Actor *a);
static int input_sel_add(Actor *a);
static int input_sel_count(void);
static int input_party_fighting(void);
static int input_side_command(int p0);
static void scene_ready_set(int on);
static void actor_direction_set(Actor *a, int dir);
static void actor_body_apply(Actor *a, int seq);
// Poq.exe 0x48ec60：期限-1可覆寫任何值；其他期限只接受不短於現有期限。
// 回傳暫存器原版混合了被呼叫函數結果與輸入索引；遊戲呼叫端均不消費，以void承接。
static void actor_modifier_set(Actor *a, int index, int value, int time) {
    if ((time == -1 || a->modifier[index].time <= time) &&
        (a->modifier[index].value != value || a->modifier[index].time != time)) {
        a->modifier[index].value = value;
        a->modifier[index].time = time;
        actor_modifier_changed(a, index, value);
    }
    // 原版即使期限拒絕寫入，輸入值0仍標記重算。
    if (!value) a->modifiers_dirty = 1;
}

// 移動進入的第6修正分支；底層完整動態進入仍待移植。
static void actor_trail_enter(Actor *a) {
    if (a->dynamic_state == 11 && a->trail_mode == 2 && (a->modifier[6].value != 1 || a->modifier[6].time != -1)) {
        a->modifier[6].value = 1; a->modifier[6].time = -1;
        actor_illusion_begin(a);
    }
}

// Poq.exe 0x488280：角色退出先處理移動殘影，再交動態物件退出。
// 期限>=21不覆寫，-1也會改成20；退出函數不改寫dynamic_state。
static void actor_trail_exit(Actor *a) {
    if (a->dynamic_state == 11 && a->trail_mode == 2 && a->modifier[6].time < 21 &&
        (a->modifier[6].value != 1 || a->modifier[6].time != 20)) {
        a->modifier[6].value = 1; a->modifier[6].time = 20;
        actor_illusion_begin(a);
    }
    actor_dynamic_exit(a);
}

// 角色每拍中的20項期限分支；與原版共用角色拍數，不因重新設置期限而重置相位。
// 拍數由動態基底統一推進；空中略過後不補扣，回地面仍沿原相位。
// 活動門檻及後段光環由actor_maintenance_tick統一處理。
static void actor_modifiers_tick(Actor *a) {
    if (a->direction_clock % 10) return;
    actor_status_countdown(a);
    for (int i = 0; i < 20; i++) {
        if (a->modifier[i].time < 0) continue;
        a->modifier[i].time -= 10;
        if (a->modifier[i].time < 0) {
            if (a->modifier[i].value != 0 || a->modifier[i].time != -1) {
                a->modifier[i].value = 0;
                a->modifier[i].time = -1;
                // 到期分支只有1/5/6具有副作用；不重新擲第3修正的潰散。
                if (i == 1 || i == 5 || i == 6) actor_modifier_changed(a, i, 0);
            }
            // +0x4ba在負期限分支無條件設置，含0值恰好減至-1。
            a->modifiers_dirty = 1;
        }
    }
}

// RoleEffectMdl欄11/12的設置／解除分支；完整狀態表的其他副作用另待移植。
static void actor_trail_effect(Actor *a, const StatusDef *d, int on) {
    if (!d || (d->trail_move == 65535 && d->trail_always == 65535)) return;
    if (on) {
        if (d->trail_move != 65535) a->trail_mode = 2;
        if (d->trail_always != 65535) a->trail_mode = 1;
    } else {
        a->trail_mode = 0;
        actor_modifier_set(a, 6, 0, -1);
    }
}

// Poq.exe 0x48b2f0：逆序重套非零byte；不先清除既有表格效果。
// 原版怪現象（組語已核對）：比較state[id]與last_status而不是id，
// 末次傳的是迴圈結束的-1（低byte=255），不是最後狀態ID。不能修成最後狀態優先。
static int has_status(const Actor *a, int st);
static void actor_effects_recompute(Actor *a) {
    for (int id = 127; id >= 0; id--)
        if (a->state[id] && a->state[id] != a->last_status) actor_effect(a, id, 1, -1);
    if (a->last_status < 128 && a->state[a->last_status]) actor_effect(a, -1, 1, -1);
    a->modifiers_dirty = 0;
}

static uint32_t actor_relation_anim(const Actor *a);
// Poq.exe 0x48d8f0：只保存32位關聯識別，不查找目標、不修改目前命令。
static inline void actor_search_link_set(Actor *a, int link) { a->search_link = link; }
static Actor *hover_actor;
static int hover_mouse;

// Poq.exe 0x48fd30：保留低8位；不重選招式、不取消動作，也不重載紙娃娃。
static void actor_weapon_type_set(Actor *a, int value) { a->weapon_type = (unsigned char)value; }

// Poq.exe 0x48e8d0：只清目標，不能順便取消移動、技能或目前動作。
static void actor_clear_target(Actor *a) { a->foe = NULL; }
static void actor_target_set(Actor *a, Actor *t);

// Poq.exe 0x486e70：取消排隊命令，保留其餘三個參數。
static void actor_cancel_command(Actor *a) { a->command[0] = -1; }

// Poq.exe 0x488bb0：只查命令代號，不查動畫、冷卻或記住命令。
static int actor_command_idle(const Actor *a) { return a->command[0] == -1; }

// Poq.exe 0x486e80：只清代號，保留三參數；再次清除會把延遲改回0。
static void actor_forget_command(Actor *a) {
    a->remember_delay = a->remembered[0] == 1 || a->remembered[0] == 2 ? 50 : 0;
    a->remembered[0] = -1;
}


// Poq.exe 0x488360：只有SInit種類10..16有角色名稱，不能拿其他種類當字串。
static const char *actor_init_name(const Actor *a) {
    // shortcut: 動態角色以最小SInitCha（種類10）承接；完整初始化記錄接入時保留其種類。
    unsigned type = a->spec ? (unsigned)a->spec->type : 10;
    return type - 10u < 7u ? a->init_name : NULL;
}

// Poq.exe 0x48c150：改原始名稱不會順便更新顯示副本。
// shortcut: 超過SInit原始64-byte容量時截斷，遇到超長事件ID時改動態字串。
static void actor_name_set(Actor *a, const char *name) {
    if (actor_init_name(a) && name != a->init_name)
        snprintf(a->init_name, sizeof a->init_name, "%s", name);
}

// Poq.exe 0x488840：回傳顯示副本，不能直接回傳SInit名稱。
static const char *actor_name(const Actor *a) { return a->display_name; }

// Poq.exe 0x4887a0：讀取顯示名稱編號，不從字串反推。
static int actor_name_number(const Actor *a) { return a->display_number; }

// Poq.exe 0x48e820：先保存編號，只有畫面客戶端且正數才加後綴。
// 格式字串已核對原版唯讀區：%s(%d)。單機共用Actor，不另送名稱同步封包。
// shortcut: 原版CString改用固定256-byte UTF-8顯示緩衝區；需要超長名稱時改動態字串。
static void actor_display_name_set(Actor *a, const char *base, int number, int client) {
    a->display_number = number;
    if (client && number > 0)
        snprintf(a->display_name, sizeof a->display_name, "%s(%d)", base, number);
    else
        snprintf(a->display_name, sizeof a->display_name, "%s", base);
}

// Poq.exe 0x4887b0：任何非零修正均優先，包括負數。
static int actor_display(const Actor *a) {
    return a->modifier[10].value ? a->modifier[10].value : a->def ? a->def->display : 0;
}

// Poq.exe 0x496530：只保存大小，不改位置或重建模型。
static void actor_size_set(Actor *a, int size) { a->body_size = size; }

// Poq.exe 0x48e6c0 / 0x488750：只寫／讀主人識別；引擎以Actor指標承接物件識別。
static void actor_owner_set(Actor *a, Actor *owner) { a->owner = owner; }
static Actor *actor_owner(const Actor *a) { return a->owner; }

// Poq.exe 0x48e6e0：同組完全不動；換組先清目標和待執行命令。
// shortcut: 單機以共用Actor取代角色組別同步封包，連線支援時補傳輸。
static void actor_group(Actor *a, int group) {
    if (a->grp == group) return;
    actor_clear_target(a);
    actor_cancel_command(a);
    a->grp = group;
}

// Poq.exe 0x488c50 / 0x48d910：旗標讀取回遮罩原值，而非布林值。
static unsigned actor_flags(const Actor *a, unsigned mask) { return (unsigned)a->flags & mask; }
static void actor_set_flags(Actor *a, unsigned mask, int on) {
    a->flags = (int)(on ? (unsigned)a->flags | mask : (unsigned)a->flags & ~mask);
}

// Poq.exe 0x48b160：交虛擬旗標設置入口清除，不把參數當整值覆寫。
static void actor_clear_flags(Actor *a, unsigned mask) { actor_set_flags(a, mask, 0); }

// Poq.exe 0x488800 / 0x488820：層號大於10時，索敵及追擊範圍各加8。
// shortcut: 現有角色建立入口僅載入地面層1；多層角色建立接入時寫入原版層號。
static int actor_seek(const Actor *a) { return a->def->seek + a->seek_adjust + (a->layer > 10 ? 8 : 0); }
static int actor_chase(const Actor *a) { return a->def->chase + (a->layer > 10 ? 8 : 0); }

// 原版將兩個速度屬性換算後存入角色；Actor.ex保存EasyRPG原始值，消費端換算。
static int actor_move_bonus(const Actor *a) {
    int v = a->ex[64];
    return v > 0 ? v * 150 / (v + 150) : v;
}

static float actor_fight_ratio(const Actor *a) {
    float v = (float)a->ex[65];
    if (v > 0) v = v * 120.0f / (v + 120.0f);
    float divisor = v + 100.0f;
    if (divisor == 0.0f) divisor = 1.0f;
    return 100.0f / divisor;
}

// Poq.exe 0x48f7a0：八種名稱逐一精確比對，未知名稱完全不動。
// +0x541/+0x545採延後換算的等價表示，避免把已換算值再當成EasyRPG原始屬性。
static void actor_property(Actor *a, const char *name, int value) {
    static const struct { const char *name; int id; } props[] = {
        {"B_C_MOVE_SPEED", 64}, {"B_C_FIGHT_SPEED", 65},
        {"B_C_SRANGE", 67}, {"B_C_VRANGE", 66},
        {"B_C_ZMYJ", 72}, {"B_C_HXYJ", 73}, {"B_C_JDR", 74}, {"B_C_DJHF", 75}
    };
    for (unsigned i = 0; i < sizeof props / sizeof *props; i++) {
        if (strcmp(name, props[i].name)) continue;
        a->ex[props[i].id] = value;
        if (props[i].id == 67) a->range_adjust = value;
        if (props[i].id == 66) {
            a->view_range = value + 12;
            a->seek_adjust = (unsigned)a->grp - 10000u > 50000u ? value : 0;
        }
        return;
    }
}

// 單機EasyRPG的@屬性通知交回各個原版入口，八鍵屬性分派仍保持精確比對。
// shortcut: @W_Type直接交給byte setter；完整場景通知分派接入後改由武器更新訊息送入。
static inline void actor_attribute_changed(Actor *a, const char *name, int value) {
    if (!strcmp(name, "W_Type")) actor_weapon_type_set(a, value);
    else actor_property(a, name, value);
}

// EasyRPG重算完的原始值交回角色屬性入口；裝備、技能、出生共用這條路徑。
static void actor_properties_update(Actor *a) {
    actor_attribute_changed(a, "B_C_MOVE_SPEED", a->ex[64]);
    actor_attribute_changed(a, "B_C_FIGHT_SPEED", a->ex[65]);
    actor_attribute_changed(a, "B_C_SRANGE", a->ex[67]);
    actor_attribute_changed(a, "B_C_VRANGE", a->ex[66]);
    actor_attribute_changed(a, "B_C_ZMYJ", a->ex[72]);
    actor_attribute_changed(a, "B_C_HXYJ", a->ex[73]);
    actor_attribute_changed(a, "B_C_JDR", a->ex[74]);
    actor_attribute_changed(a, "B_C_DJHF", a->ex[75]);
}

// 角色某序列的節奏：各部件的幀延遲相同，拿第一個有資料的部件
static const Seq *role_seq(const Role *r, int seq) {
    if (seq < 0) return NULL;
    for (int i = 0; r && i < r->part_count; i++)
        if (r->body[i] && seq < r->body[i]->count && r->body[i]->seqs[seq].count) return &r->body[i]->seqs[seq];
    return NULL;
}

// Poq.exe 0x496d80：指定序列描述取目前部件群首個精靈，不跳過零幀序列。
// shortcut: 原版無部件群會解參考0x14；缺資源或越界回NULL，資源完整性驗證接齊時升級為載入錯誤。
static const Seq *actor_sequence(const Actor *a, int seq) {
    const Role *r = a->role;
    const Anim *first = r && r->part_count > 0 ? r->body[0] : NULL;
    return first && seq >= 0 && seq < first->count ? &first->seqs[seq] : NULL;
}

static Uint32 seq_ticks(const Seq *q) {  // 0x49d6c0：序列總拍數（原版直接加幀延遲；這裡 0 當 1，與 frame_at 一致）
    Uint32 t = 0;
    for (int i = 0; i < q->count; i++) t += q->frames[i].delay > 0 ? q->frames[i].delay : 1;
    return t;
}

static int has_status(const Actor *a, int st);

// 選擇運算與寫入分開，繪製矩形查詢不可改角色的方向或目標序列。
static int actor_animation_target(const Actor *a, int dir) {
    const RoleDef *d = a->def;
    if (!d) return a->animation_target;
    if (a->knock == 1) {
        if (d->death == 65535) return a->animation_target;
        int seq = d->death + dir;
        return !a->role || seq >= a->role->seq_count ? d->fight_idle + dir : seq;
    }
    if (has_status(a, 101)) return d->hit == 65535 ? a->animation_target : d->hit + dir;
    if (!(a->flags & 0x10000) || a->dynamic_state == 1 || d->run_seq == 65535) {
        if (a->fight) {
            if (a->dynamic_state == 1) return d->fight_idle + dir;
            if ((unsigned)(a->dynamic_state - 20) < 11u) return a->animation_target;
            return d->fight_walk + dir;
        }
        if (a->dynamic_state == 1) return d->idle + dir;
    } else if (a->speed_threshold < 101 && !(a->flags & (0x20000 | 0x80000))) {
        return d->run_seq + dir;
    }
    return d->walk + dir;
}

// Poq.exe 0x48c190：選目標序列；未指定的擊退／挨打分支保留舊值。
// shortcut: 狀態0/101與第5修正由fight/Buff/knock橋接；完整狀態表接入後替換資料來源。
static void actor_animation_select(Actor *a, int seq) {
    if (seq != -1) {
        a->animation_target = seq;
        a->animation_dir = seq % 8; // 原版帶符號餘數，負數也不能改成0..7。
    } else a->animation_target = actor_animation_target(a, a->animation_dir);
}

static int actor_requested_seq(const Actor *a) {
    if (a->def && a->def->anim) return a->object_kind == 24 ? a->trap_state : 0;  // 陷阱三序列 = 狀態 0／1／2（0x49c410）；shortcut: 其他小動物的狀態切換待還原。
    // 靜態SInit的初始態是明確選定的目標序列，不能每拍用休息序列覆蓋。
    if (!a->act && a->spec && !a->spec->logic && a->dynamic_state == 1)
        return a->animation_target;
    // 回放（Poq.exe 0x4963e0：DAT_00663804 == 0 的客戶端不自己選序列）：身體序列就是錄影 #3 子碼 0x66 送來的目標序列（0x48c190 存的 +0x8b），
    // 方向跟著錄影、不跟 a->dir（錄影角色的 a->dir 從建立後沒人改，走路一直面向同一邊）
    if (a->rec && !a->act) return a->animation_target;
    return a->act ? a->act_seq + DIR_MAP[a->dir] : actor_animation_target(a, DIR_MAP[a->dir]);
}

static int actor_seq(const Actor *a) {
    return a->shown_ready ? a->shown_seq : actor_requested_seq(a);
}

// 本體播放器由固定拍推進；繪製只讀，不能以畫面頻率或總拍數重算。
static int actor_frame(const Actor *a, int seq) {
    const Seq *q = role_seq(a->role, seq);
    if (!q || !q->count || !a->body_ready || a->body_seq != seq) return 0;
    return a->act_frame < q->count ? a->act_frame : q->count - 1;
}

static int actor_body_delay(const Actor *a, const Seq *q);

static void actor_body_select(Actor *a) {
    int seq = actor_seq(a);
    if (!a->body_ready || a->body_seq != seq) {
        a->body_seq = seq;
        a->body_ready = 1;
        a->act_frame = a->frame_tick = a->act_done = 0;
        const Seq *q = role_seq(a->role, seq);
        a->body_start = 0;
        a->body_end = q && q->count ? q->count - 1 : 0;
    }
}

static int actor_body_delay(const Actor *a, const Seq *q) {
    int adjust = a->act ? a->adj : 0;
    int seq = a->body_seq;
    if (!a->act && a->def &&
        ((a->def->walk != 65535 && seq >= a->def->walk && seq < a->def->walk + 8) ||
         (a->def->fight_walk != 65535 && seq >= a->def->fight_walk && seq < a->def->fight_walk + 8)))
        adjust = a->walk_delay_adjust;
    int delay = q->frames[a->act_frame].delay + adjust;
    return delay < 1 ? 1 : delay;
}

// 本體與附加精靈共用原版播放器的完成判斷：最後幀的最後一拍。
static int actor_body_done(const Actor *a, const Seq *q) {
    return q && q->count && a->act_frame >= a->body_end &&
           a->frame_tick == actor_body_delay(a, q) - 1;
}

// 反播 Poq.exe 0x49e5f0；正播／停末幀沿用同一份幀與延遲資料。
// shortcut: 全域播放器延遲調整仍為0；引擎加入該全域選項時統一接入本體與Spr。
static void actor_body_step(Actor *a, const Seq *q, int stop) {
    if (stop && actor_body_done(a, q)) return;
    if (++a->frame_tick < actor_body_delay(a, q)) return;
    a->frame_tick = 0;
    if (stop) {
        if (a->act_frame != a->body_end) ++a->act_frame;
    } else if (a->reverse_playback) {
        a->act_frame = a->act_frame == a->body_start ? a->body_end : a->act_frame - 1;
    } else a->act_frame = a->act_frame == a->body_end ? a->body_start : a->act_frame + 1;
    if (a->act_frame < 0) a->act_frame = q->count - 1;
    else if (a->act_frame >= q->count) a->act_frame = 0;
}

static void actor_animation_tick(Actor *a);
static void actor_yield_request(const Actor *a, int x, int y);

// 角色繪圖的三附加精靈：腳部留在地面，身部隨高度，頭部再扣模型高度。
// 三槽與本體比例無關；各自使用播放器保存的當前幀。
static void status_fx_draw(const Actor *a, int sx, int sy, Uint32 tick, int foot) {
    (void)tick;
    for (int i = 0; i < (foot ? 1 : 2); ++i) {
        int k = foot ? 2 : 1 - i; // 身上在頭上之前；即使兩者重疊也由頭上覆蓋。
        if (!a->effect[k]) continue;
        const Frame *f = spr_frame(a->effect[k]);
        if (f) draw(image(f->ref), sx - f->dx, sy - f->dy - (k == 0 ? a->head_height : 0));
    }
}

// 0x49d740：幀的畫面矩形 = 原點偏移 (−dx, −dy) + 圖寬高；shortcut: 原版還加幀 +0xe/+0x10（ex, ey），這裡 ex, ey 只當出手點（繪圖也沒加），查到資料再接。
static SDL_Rect frame_rect(const Frame *f, int x, int y) {
    const Sprite *im = f ? image(f->ref) : NULL;
    return (SDL_Rect){x - (f ? f->dx : 0), y - (f ? f->dy : 0), im ? im->w : 0, im ? im->h : 0};
}
// CSprGroup矩形基礎：與繪製共用同一序列／幀；沒載到的圖不加入聯集。
static void frame_rect_union(SDL_Rect *rect, const Anim *a, int seq, int frame, int x, int y) {
    if (!a || seq < 0 || seq >= a->count || frame < 0 || !a->seqs[seq].count) return;
    const Frame *f = &a->seqs[seq].frames[frame % a->seqs[seq].count];
    SDL_Rect r = frame_rect(f, x, y);
    if (r.w <= 0 || r.h <= 0) return;
    if (rect->w) SDL_UnionRect(rect, &r, rect); else *rect = r;
}

// 0x49de30：精靈群目前幀矩形聯集（逐部件 0x49d740 後 GRECT +=）
static SDL_Rect role_rect(const Role *r, int seq, int frame, int x, int y, int shadow) {
    SDL_Rect rect = {0};
    if (!r || seq < 0 || seq >= r->seq_count || !r->frames || !r->frames[seq]) return rect;
    for (int i = 0; i < r->part_count; i++)
        frame_rect_union(&rect, shadow ? r->shadow[i] : r->body[i], seq, frame, x, y);
    return rect;
}

// shortcut: 網頁圖像非同步下載時暫用已取得部件的高度，資源就緒後在固定拍補齊；
// 始終讀新模型的序列0／幀0，不能拿角色稍後的動畫幀重新定義頭頂高度。
static void actor_head_height(Actor *a) {
    const Role *r = a->role;
    SDL_Rect rect = {0};
    a->head_height_pending = 0;
    for (int i = 0; r && i < r->part_count; i++) {
        const Anim *an = r->body[i];
        if (!an || !an->count || !an->seqs[0].count) continue;
        const Frame *f = &an->seqs[0].frames[0];
        const Sprite *im = image(f->ref);
        if (!im) { if (res_waiting) a->head_height_pending = 1; continue; }
        if (im->w <= 0 || im->h <= 0) continue;
        SDL_Rect part = {-f->dx, -f->dy, im->w, im->h};
        if (rect.w) SDL_UnionRect(&rect, &part, &rect); else rect = part;
    }
    a->head_height = (uint16_t)(rect.h + 10);
}

// Poq.exe 0x497d90：先各自向零截斷半邊增量，再加倍；底邊固定。
// 常數已核對Poq.exe唯讀區：除數20.0f、半邊係數0.5f。
static void role_rect_scale(SDL_Rect *r, int scale, int center) {
    if (!scale) return;
    float factor = (float)scale / 20.0f;
    int dx = (int)((float)r->w * factor * 0.5f);
    int dy = (int)((float)r->h * factor * 0.5f);
    r->x -= dx;
    r->y -= center ? dy : dy * 2;
    r->w += dx * 2;
    r->h += dy * 2;
}

// 紙娃娃的一幀：各部件照這幀的疊圖順序畫
static unsigned role_hide;  // CPartDeadObj 分件：畫之前設成 Actor.part_hide，畫完清 0
static void role_draw(const Role *r, int seq, int frame, int sx, int sy) {
    if (!r || seq < 0 || seq >= r->seq_count || !r->frames[seq]) return;
    frame %= r->frames[seq];
    const uint8_t *order = r->order[seq] + frame * r->part_count;
    for (int i = 0; i < r->part_count; i++)
        if (order[i] < r->part_count && !(role_hide >> order[i] & 1)) draw_frame(r->body[order[i]], seq, frame, sx, sy, 0);
}

// Poq.exe 0x48f200：整份紙娃娃先合成到暫存表面，再按合成矩形縮放拷貝。
// shortcut: SDL透明RGBA取代原版16/32位色鍵表面，光照由現有整畫面light_draw承接；
// 原版逐像素色表／格子光照接入時升級表面混色，不改此處合成後才縮放的順序。
static void actor_scaled_draw(const Actor *a, int seq, int frame, int sx, int sy, int shadow) {
    SDL_Rect src = role_rect(a->role, seq, frame, sx, sy, shadow), dst = src;
    if (src.w <= 0 || src.h <= 0) return;
    role_rect_scale(&dst, a->scale, 0);
    if (dst.w <= 0 || dst.h <= 0) return;
    if (dst.x >= vw || dst.y >= vh || dst.x + dst.w <= 0 || dst.y + dst.h <= 0) return;
    if (!role_surface || src.w > role_surface_w || src.h > role_surface_h) {
        int w = src.w > role_surface_w ? src.w : role_surface_w;
        int h = src.h > role_surface_h ? src.h : role_surface_h;
        SDL_Texture *surface = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!surface) { fprintf(stderr, "role surface: %s\n", SDL_GetError()); return; }
        SDL_SetTextureBlendMode(surface, SDL_BLENDMODE_BLEND);
        SDL_DestroyTexture(role_surface);
        role_surface = surface; role_surface_w = w; role_surface_h = h;
    }
    SDL_Texture *target = SDL_GetRenderTarget(ren);
    SDL_Rect view, clip;
    SDL_RenderGetViewport(ren, &view);
    SDL_RenderGetClipRect(ren, &clip);
    SDL_bool clipped = SDL_RenderIsClipEnabled(ren);
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(ren, &red, &green, &blue, &alpha);
    float scale_x, scale_y;
    SDL_RenderGetScale(ren, &scale_x, &scale_y);
    if (SDL_SetRenderTarget(ren, role_surface) < 0) return;
    SDL_RenderSetScale(ren, 1, 1);
    SDL_RenderSetViewport(ren, NULL);
    SDL_RenderSetClipRect(ren, NULL);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
    SDL_RenderClear(ren);
    // draw()的畫面裁切改為暫存表面尺寸；場景邊緣不能截掉合成來源。
    int saved_w = vw, saved_h = vh;
    Uint8 saved_alpha = draw_alpha;
    vw = role_surface_w; vh = role_surface_h; draw_alpha = 255;
    if (shadow) {
        for (int i = 0; i < a->role->part_count; i++)
            if (!(role_hide >> i & 1)) draw_frame(a->role->shadow[i], seq, frame, sx - src.x, sy - src.y, 1);
    } else role_draw(a->role, seq, frame, sx - src.x, sy - src.y);
    vw = saved_w; vh = saved_h; draw_alpha = saved_alpha;
    SDL_SetRenderTarget(ren, target);
    SDL_RenderSetScale(ren, scale_x, scale_y);
    SDL_RenderSetViewport(ren, &view);
    SDL_RenderSetClipRect(ren, clipped ? &clip : NULL);
    SDL_SetRenderDrawColor(ren, red, green, blue, alpha);
    SDL_SetTextureAlphaMod(role_surface, saved_alpha);
    SDL_Rect source = {0, 0, src.w, src.h};
    SDL_RenderCopy(ren, role_surface, &source, &dst);
}

// 角色每拍更新的比例分支；完整角色狀態／命令心跳尚未完成。
static void actor_scale_tick(Actor *a) {
    if (!a->role || !a->def || a->def->scale != 65535) return;
    if (!(a->flags & 0x400) && (a->flags & 0x200) && a->dynamic_state != 1) return;
    if (a->scale != a->modifier[4].value) a->scale += a->modifier[4].value < a->scale ? -1 : 1;
}

static void actor_overhead_draw(Actor *a, int sx, int sy);
static void rec_dialog_note(const Actor *a, int sx, int sy);

static int actor_draw_visible(const Actor *a);
static int trap_owner_group(Actor *t);  // near.c
static int trap_viewer_sees(Actor *a);
static int actor_has_method(const Actor *a);

// Poq.exe 0x577d60：未選部件群時false，否則只檢查首部件的殘影容器。
// Role直接保存目前群；容器即使尚無採樣，也算已啟用。
static int actor_illusion_enabled(const Actor *a) {
    return a->role && a->role->part_count > 0 && a->illusion[0] != NULL;
}

// Poq.exe 0x578000／0x575700／0x577360：部件調色盤選擇。part_color[i] = 該部件的色表編號，
// 全部件設值（部件名NULL）或只設名字相符且該部件存在者。
// shortcut: 引擎沒有原版色表（逐像素調色盤），只記下選擇、不改像素；色表資源接入時由draw_frame讀part_color。
static struct { int part_color[MAX_ROLE_PARTS]; } role_palette;
static int role_part_find(const Role *r, const char *name) {
    for (int i = 0; i < r->part_count; i++) if (r->body[i] && !strcmp(r->name[i], name)) return i;
    return -1;
}
static void role_palette_set(const Role *r, const char *name, int color) {
    if (!name) { for (int i = 0; i < r->part_count; i++) if (r->body[i]) role_palette.part_color[i] = color; return; }
    int i = role_part_find(r, name);
    if (i >= 0) role_palette.part_color[i] = color;
}

// 頭上標記（+0x44f）：<500 取第id-1張、其餘取id-500／id-1000；第二行底圖 = id%3。
// shortcut: 標記圖表（0x65c954／0x65c99c）在原版映像裡沒有初始化來源、資源位置未知，只算索引不畫；找到資源再補。
static int actor_marker_index(int id) { return id < 500 ? id - 1 : id < 1000 ? id - 500 : id - 1000; }

static SDL_Rect viewport_actor_rect(const Actor *a, int sx, int sy, Uint32 tick);

// Poq.exe 0x4872e0 CCharactor繪圖：隱身／顯示50早退、影子、腳圈、腳／身／頭附加精靈、殘影、
// 部件覆色（+0x45f RH+LH、+0x467 SD、+0x46f HD）、狀態60八副本、頭上標記、8槽浮字、診斷矩形（+0x57d）。
// 這一幀（所有部件的本體與影子圖）是不是都能畫。網頁版圖片邊畫邊下載，沒好的整個人會不見（使用者手機「缺」數字一直增加）
static int anim_frame_ready(const Anim *an, int seq, int frame) {
    if (!an) return 1;
    if (seq < 0 || seq >= an->count || !an->seqs[seq].count) return 1;  // 沒有這個序列：本來就不畫，不算缺
    const Frame *f = &an->seqs[seq].frames[frame % an->seqs[seq].count];
    return image(f->ref) != NULL;
}
static int role_frame_ready(const Role *r, int seq, int frame) {
    if (!r || seq < 0 || seq >= r->seq_count || !r->frames || !r->frames[seq]) return 1;
    for (int i = 0; i < r->part_count; i++)
        if (!anim_frame_ready(r->body[i], seq, frame) || !anim_frame_ready(r->shadow[i], seq, frame)) return 0;
    return 1;
}
// 角色目前該畫的（序列、幀）：新的一幀圖還沒齊就沿用最後一個齊的；畫與算畫面矩形（viewport.c）共用，不然矩形是空的會被整個剔除
static void actor_visual(const Actor *ca, int *seq, int *frame) {
    Actor *a = (Actor *)ca;
    *seq = actor_seq(a);
    *frame = actor_frame(a, *seq);
    const Role *r = a->role;
    if (!r || a->object_kind == 24) return;
    if (a->good_role != (const void *)r) { a->good_role = r; a->good_seq = -1; }
    if (*seq >= 0 && *seq < r->seq_count && r->frames && r->frames[*seq]) {
        int fr = *frame % r->frames[*seq];
        if (role_frame_ready(r, *seq, fr)) { a->good_seq = *seq; a->good_frame = fr; }
        else if (a->good_seq >= 0 && a->good_seq < r->seq_count && r->frames[a->good_seq]) { *seq = a->good_seq; *frame = a->good_frame; }
    }
}
static void actor_draw(Actor *a, int sx, int sy, Uint32 tick) {
    const Role *r = a->role;
    if (!r || (a->object_dead && !a->corpse_visible) || !actor_draw_visible(a)) return;
    if (a->object_kind == 24) {  // CTrapBase 畫（0x49bc80）：只畫當前幀；已發現 = 待命 192／其他不透明，沒發現 = 只有主人那組看得到、半透明 128
        int seq = actor_seq(a);
        if (seq < 0 || seq >= r->seq_count || !r->frames[seq]) return;
        if (!a->trap_revealed && !trap_viewer_sees(a)) return;
        Uint8 saved = draw_alpha;  // shortcut: 待命時原版另疊色 0x7c00／0xf800（紅），沒做
        draw_alpha = a->trap_revealed ? (a->trap_state == 0 ? 192 : 255) : 128;
        role_draw(r, seq, actor_frame(a, seq) % r->frames[seq], sx, sy);
        draw_alpha = saved;
        return;
    }
    int seq, frame;
    actor_visual(a, &seq, &frame);
    if (seq < 0 || seq >= r->seq_count || !r->frames[seq]) return;
    frame %= r->frames[seq];
    role_hide = a->part_hide;
    // 影子：光照模式單機固定2，大小≤3且全域0x639f38（選項，單機未設）才用色表9；其餘0
    role_palette_set(r, NULL, 0);
    if (a->scale) actor_scaled_draw(a, seq, frame, sx, sy, 1);
    else for (int i = 0; i < r->part_count; i++) if (!(role_hide >> i & 1)) draw_frame(r->shadow[i], seq, frame, sx, sy, 1);
    if (a->flags & 0x100) {
        uint32_t id = actor_relation_anim(a);
        const Anim *ring = id ? anim_get(id) : NULL;
        if (ring && ring->count) draw_frame(ring, 0, frame_at(&ring->seqs[0], tick), sx, sy, 0);
    }
    status_fx_draw(a, sx, sy, tick, 1);
    if (actor_illusion_enabled(a)) actor_illusion_draw(a);
    {   // 本體色表：顯示方式0xffff（模板、無覆蓋）→10，其餘用顯示方式本身
        int mode = actor_display(a);
        role_palette_set(r, NULL, mode == 0xffff && !a->modifier[10].value ? 10 : mode);
        if (a->modifier[11].value) { role_palette_set(r, "RH", a->modifier[11].value); role_palette_set(r, "LH", a->modifier[11].value); }
        if (a->modifier[12].value) role_palette_set(r, "SD", a->modifier[12].value);
        if (a->modifier[13].value) role_palette_set(r, "HD", a->modifier[13].value);
    }
    if (a->scale) actor_scaled_draw(a, seq, frame, sx, sy - a->z, 0);
    else role_draw(r, seq, frame, sx, sy - a->z);  // 影子留在地上
    role_hide = 0;
    if (a->part_kind) {  // 屍體物件 0x499270／0x499540（帶裁切版，裁切 = viewport 矩形相交）、縮放繪製 0x499b90 = actor_scaled_draw：影子、本體、血跡疊圖，沒有狀態特效和頭上標記
        const Anim *blood = a->blood_id ? anim_get(a->blood_id) : NULL;
        if (blood && blood->count)
            draw_frame(blood, 0, frame_at(&blood->seqs[0], tick - a->blood_start), sx + a->blood_x, sy + a->blood_y, 0);
        return;
    }
    // 狀態60僅在未縮放且有MethodMain時，以首部件畫八個固定偏移副本。
    // shortcut: SBMP的0x80合成旗標暫由SDL半透明承接，調色盤渲染器接齊後核對。
    if (!a->scale && has_status(a, 60) && actor_has_method(a)) {
        static const int offset[8][2] = {{15,0},{-15,0},{10,10},{-10,10},
                                        {-10,-10},{10,-10},{0,-15},{0,15}};
        Uint8 saved = draw_alpha;
        draw_alpha = 128;
        for (int i = 0; i < 8; ++i)
            draw_frame(r->body[0], seq, frame, sx + offset[i][0], sy - a->z + offset[i][1], 0);
        draw_alpha = saved;
    }
    status_fx_draw(a, sx, sy - a->z, tick, 0);
    if (a->modifier[9].value) (void)actor_marker_index(a->modifier[9].value);
    actor_overhead_draw(a, sx, sy - a->z);
    rec_dialog_note(a, sx, sy - a->z - a->head_height - 10);
    if (a->debug_rectangle) {
        SDL_Rect box = viewport_actor_rect(a, sx, sy, tick);
        SDL_SetRenderDrawColor(ren, 0, 0, 255, 255);
        SDL_RenderDrawRect(ren, &box);
    }
}

// 面向畫面方向 (sdx, sdy)
static void face(Actor *a, float sdx, float sdy) {
    int oct = (int)lroundf(atan2f(sdx, -sdy) / (float)(M_PI / 4));
    a->dir = (oct + 8) % 8;
    a->facing = DIR_MAP[a->dir];
}

// 按住走路遇到牆時的貼牆滑動：只沿「畫面上朝向手指」的格子軸（螢幕向量與想去方向夾角 < 約 73 度）走，距離是想去方向在該軸上的投影。
// 舊版直接用格子座標分量滑動，常常滑到手指的反方向（往右下的軸分量被當成「往下」的一部分），畫面就左右抖。
// 沒有可滑的軸 = 完全擋住（walk_blocked）。軸 i 的畫面向量 = (±32, ±16)，長度 35.78 像素／格
static int wall_slide(Actor *h, const Scene *s, float sdx, float sdy, float px) {
    static const int AX[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    float dot[4]; int order[4] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i++) dot[i] = (sdx * 32 * (AX[i][0] - AX[i][1]) + sdy * 16 * (AX[i][0] + AX[i][1])) / 35.78f;  // sdx, sdy 已正規化
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 4; j++) if (dot[order[j]] > dot[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int k = 0; k < 4; k++) {
        int i = order[k];
        if (dot[i] < 0.3f) break;
        float cells = px * dot[i] / 35.78f, nx = h->x + AX[i][0] * cells, ny = h->y + AX[i][1] * cells;
        if (!scene_walkable(s, (int)nx, (int)ny)) continue;
        h->x = nx; h->y = ny;
        return 1;
    }
    return 0;
}

// 往畫面方向 (sdx, sdy) 走 px 個畫面像素。畫面上各方向等速（格子寬 64 高 32，若照格子等速，上下會只有左右一半快）
static void actor_step(Actor *h, const Scene *s, float sdx, float sdy, float px) {
    if (h->dynamic_state == 21 || h->dynamic_state == 22 || h->dynamic_state == 31 || h->dynamic_state == 32) return;
    float len = sqrtf(sdx * sdx + sdy * sdy);
    if (len < 0.01f) return;
    sdx /= len;
    sdy /= len;
    // 畫面位移 → 格子位移：畫面 x = (gx - gy) * 32，畫面 y = (gx + gy) * 16
    float gx = (sdx / 64 + sdy / 32) * px, gy = (-sdx / 64 + sdy / 32) * px;
    float previous_x = h->x, previous_y = h->y;
    int moved = 1;
    // 能走就走；撞牆時貼牆滑動（只往朝向手指的軸）
    if (scene_walkable(s, (int)(h->x + gx), (int)(h->y + gy))) { h->x += gx; h->y += gy; }
    else moved = wall_slide(h, s, sdx, sdy, px);
    if (moved && h->grid_valid && ((int)h->x != h->grid_x || (int)h->y != h->grid_y) && !actor_place(h, (int)h->x, (int)h->y, 0)) {
        actor_yield_request(h, (int)h->x, (int)h->y);
        h->x = previous_x; h->y = previous_y;
        moved = 0;
    }
    if (px > 0) h->walk_blocked = !moved;  // px = 0 的拍（速度表隔拍才走）沒有資訊，保留上次
    face(h, sdx, sdy);
    h->moving = !h->walk_blocked;
}

static int status_speed(const Actor *a);

// Poq.exe 0x488540：第1修正非0時以值減1覆蓋；跑步優先於戰鬥速度。
// 門檻>=101才減半，最後不夾限；速度表的範圍在移動消費端處理。
// shortcut: 戰鬥狀態仍由現有fight計時承接，完整狀態表接入後改讀狀態0。
static int actor_speed(const Actor *a) {
    int s = a->modifier[1].value ? a->modifier[1].value - 1 :
        (a->flags & 0x10000) && a->dynamic_state == 11 ? a->speed * 3 / 2 :
        a->fight ? a->fight_speed : a->speed;
    int st = status_speed(a), v = s * st / 100;
    if (s && st && v < 1) v = 1;
    v = (actor_move_bonus(a) + 100) * v / 100;
    return a->speed_threshold < 101 ? v : v / 2;
}

// 只在查表端夾限，不改速度getter的結果；三參數表共用原版資料。
static int speed_index(int speed) { return speed < 0 ? 0 : speed > 59 ? 59 : speed; }

static int movement_delay_adjust(float ratio) {
    if (ratio == 1.0f) return 0;
    if (!(ratio <= 5.0f)) return -5;
    if (ratio > 4.0f) return -4;
    if (ratio > 3.0f) return -3;
    if (ratio > 2.0f) return -2;
    if (ratio > 1.5f) return -1;
    if ((double)ratio < 0.3) return 7;
    if (ratio < 0.5f) return 5;
    return (double)ratio < 0.7 ? 3 : 1;
}

// Poq.exe 0x496ae0：基準比值只用A/B，不把C的小數移動量算進動畫速率。
// 每個Actor保存調整量，避免修改共用Role資源而讓同模型的其他角色一起加速。
static void actor_move_speed(Actor *a, int speed) {
    if (speed < 1 && a->dynamic_state == 11) {
        actor_dynamic_rest(a);
    }
    int base = speed_index(a->speed), index = speed_index(speed);
    a->move_a = SPEED_A[index];
    a->move_b = SPEED_B[index];
    a->move_c = SPEED_C[index];
    float ratio = ((float)a->move_a / a->move_b) / ((float)SPEED_A[base] / SPEED_B[base]);
    a->walk_delay_adjust = movement_delay_adjust(ratio);
    a->move_remaining = a->move_a;
}

// Poq.exe 0x48b2d0：角色有效速度getter交給動態物件套用。
static void actor_speed_update(Actor *a) { actor_move_speed(a, actor_speed(a)); }

static int actor_can_move(const Actor *a);

// 一拍的角色更新：往畫面方向 (mx, my) 走；原版每 B 拍移動 A + C/10 像素（速度對照表）
static void actor_tick(Actor *a, const Scene *s, float mx, float my, Uint32 tick) {
    if (a->dynamic_state == 21 || a->dynamic_state == 22 || a->dynamic_state == 31 || a->dynamic_state == 32) return;
    a->moving = 0;
    a->path_walk = 0;
    // shortcut: 現有移動迴圈承接平常／移動狀態，跳躍等完整動態狀態仍待移植。
    int state = mx * mx + my * my > 0.01f && !a->walk_blocked ? 11 : 1;
    int changed = state != a->dynamic_state;
    if (state != a->dynamic_state) {
        actor_trail_exit(a);
        a->dynamic_state = state;
        actor_trail_enter(a);
    }
    actor_speed_update(a);
    if (mx * mx + my * my <= 0.01f) a->walk_blocked = 0;
    if (mx * mx + my * my > 0.01f && actor_can_move(a)) {
        float px = tick % a->move_b ? 0 : a->move_a + a->move_c / 10.0f;
        actor_step(a, s, mx, my, px);
    }
    if (!(a->flags & 0x400)) a->animation_dir = DIR_MAP[a->dir];
    if (changed || state == 11 || !a->spec || a->spec->logic)
        actor_animation_select(a, -1);
}

// 圖形 ID（RoleMdl 欄 2）：數字 = 紙娃娃 PLI（0001_001.PLI…）；蝙蝠 BIANFU1／2、骷髏 SKULLBOW／SKULLDAO、蜘蛛 ZHIZHU 是名字，
// 同一套 PLI 格式（BIANFU1_001.PLI，部件 TK＋影子 TKS，資源 SPR_ROLE_<名字>_TK_001_001），role_load 的 %.8s 本來就吃得下；
// 小動物（BeastieMdl）則是單一動畫。兩種都沒有的角色不畫。
static int role_drawable(const RoleDef *d) { return d && (d->anim || d->graphic[0]); }

// 依角色表載入外觀；roles 是這個場景已載入過的外觀快取（同一模板共用）
#define MAX_ROLES 64
typedef struct {
    int tmpl;
    Role role;
    int pli;
    uint32_t anim;
    char graphic[16], styles[96];
} RoleSlot;
static RoleSlot roles[MAX_ROLES];
static int role_count;

// 紙娃娃資源可共用，換裝後的組合不可原地改寫其他角色正在使用的Role。
typedef struct RoleVariant {
    struct RoleVariant *next;
    int pli;
    uint32_t anim;
    char graphic[16], styles[96];
    Role role;
} RoleVariant;
static RoleVariant *role_variants;

static const Role *role_variant_for(const RoleDef *d) {
    for (int i = 0; i < role_count; i++) {
        const RoleSlot *r = &roles[i];
        if (r->pli == d->pli && r->anim == d->anim &&
            !strcmp(r->graphic, d->graphic) && !strcmp(r->styles, d->styles)) return &r->role;
    }
    for (RoleVariant *v = role_variants; v; v = v->next)
        if (v->pli == d->pli && v->anim == d->anim &&
            !strcmp(v->graphic, d->graphic) && !strcmp(v->styles, d->styles)) return &v->role;
    RoleVariant *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    if (!(d->anim ? role_load_anim(&v->role, d->anim) : role_load(&v->role, d->graphic, d->pli, d->styles))) {
        role_free(&v->role); free(v); return NULL;
    }
    v->pli = d->pli; v->anim = d->anim;
    snprintf(v->graphic, sizeof v->graphic, "%s", d->graphic);
    snprintf(v->styles, sizeof v->styles, "%s", d->styles);
    v->next = role_variants; role_variants = v;
    return &v->role;
}

static const Role *role_for(const RoleDef *d) {
    for (int i = 0; i < role_count; i++)
        if (roles[i].tmpl == d->id) return &roles[i].role;
    if (role_count == MAX_ROLES) return NULL;
    RoleSlot *r = &roles[role_count];
    if (!(d->anim ? role_load_anim(&r->role, d->anim) : role_load(&r->role, d->graphic, d->pli, d->styles))) return NULL;
    r->tmpl = d->id;
    r->pli = d->pli; r->anim = d->anim;
    snprintf(r->graphic, sizeof r->graphic, "%s", d->graphic);
    snprintf(r->styles, sizeof r->styles, "%s", d->styles);
    role_count++;
    return &r->role;
}

static void roles_clear(void) {
    actors_illusion_clear();
    for (int i = 0; i < role_count; i++) role_free(&roles[i].role);
    role_count = 0;
    while (role_variants) {
        RoleVariant *v = role_variants;
        role_variants = v->next;
        role_free(&v->role); free(v);
    }
}

// 主角 + 場景 NPC
#define MAX_ACTORS 256
static Actor actors[MAX_ACTORS];
static int actor_count;

// Actor私有的殘影必須先於Role快取及Actor陣列重建釋放。
static void actors_illusion_clear(void) {
    for (int i = 0; i < actor_count; i++)
        {
            for (int k = 0; k < MAX_ROLE_PARTS; k++) illusion_clear(&actors[i].illusion[k]);
            for (int k = 0; k < 3; k++) { free(actors[i].effect[k]); actors[i].effect[k] = NULL; }
        }
}

// Poq.exe 0x488a10：失效才清識別；角色旗標0x10只隱藏本次查詢，死亡不等於失效。
// shortcut: CObj句柄查找沿用Actor陣列和指標；目前場景整批重建，
// 若開始在同場景重用Actor槽，須改帶世代的識別以排除舊指標。
static Actor *actor_target(Actor *a) {
    Actor *t = a->foe;
    for (int i = 0; t && i < actor_count; i++) {
        if (t != &actors[i] || !t->role) continue;
        if (!(t->mon && t->mon->istrap == 1) && (t->flags & 0x10)) return NULL;
        return t;
    }
    a->foe = NULL;
    return NULL;
}

// Poq.exe 0x4770a0：依物件編號查找，包含已死物件。
// shortcut: 劇情handle與無劇情物件object_id共用Actor索引；場景加入非Actor物件時擴充共用索引。
static inline Actor *actor_of(int h) {
    for (int i = 0; h && i < actor_count; i++)
        if ((actors[i].sh == h || actors[i].object_id == (uint32_t)h) && actors[i].role) return &actors[i];
    return NULL;
}

// Poq.exe 0x476fd0：查找後只接受種類 30～100，不檢查是否死亡。
// shortcut: actors 只承載角色及陷阱，以 istrap 排除陷阱；加入其他物件類別時改讀原版種類。
static inline Actor *character_of(int h) {
    Actor *a = actor_of(h);
    return a && !(a->mon && a->mon->istrap == 1) ? a : NULL;
}

// 場景客戶端保留單機需要的查詢狀態。
typedef struct {
    const Scene *scene;
    int ready;
    int controlled;
} SceneClient;
static SceneClient scene_client;

// 暫停要求對照場景管理器：回放、無場景、未就緒一律清零。
static void scene_pause(int pause) {
    if (scene_record_mode == 2 || !scene_client.scene || !scene_client.ready) pause = 0;
    scene_paused = pause;
}
// 暫停計數（Poq.exe 0x44d9e0，包裝 0x532570）：要暫停時還沒暫停就暫停、已暫停就計數 +1；解除時計數 < 1 才真的解除，否則 −1。
static int scene_pause_n;
static void scene_pause_count(int pause) {
    if (!pause) {
        if (scene_pause_n < 1) { scene_pause_n = 0; scene_pause(0); }
        else scene_pause_n--;
        return;
    }
    if (!scene_paused) scene_pause(1);
    else scene_pause_n++;
}

// Poq.exe 0x46eb40：沒有場景或場景未就緒時，不查操控角色。
// shortcut: 原版以帶世代的物件句柄查找；目前 controlled 是 actors 索引，
// 待 CScene 完整物件索引接入後改用其句柄，單機不切換全域管理器別名。
static Actor *scene_client_actor(const SceneClient *c) {
    if (!c->scene || !c->ready) return NULL;
    if (c->controlled < 0 || c->controlled >= actor_count) return NULL;
    return actors[c->controlled].role ? &actors[c->controlled] : NULL;
}

// Poq.exe 0x46ebd0／0x443760：比較操控角色的 +0x339 組別；沒有角色時回假。
// 原版成功查找後再查一次；單機陣列查詢沒有副作用，沿用同樣順序。
static inline int scene_client_group(const SceneClient *c, int group) {
    if (!scene_client_actor(c)) return 0;
    return scene_client_actor(c)->grp == group;
}

static int trap_viewer_sees(Actor *a) { return scene_client_group(&scene_client, trap_owner_group(a)); }

// 繪圖本體的早退：隱身僅操控角色同組能看見，顯示50連影子／浮字也不處理。
// 不在矩形getter隱藏，原版矩形計算與繪圖可見性是兩個入口。
static int actor_draw_visible(const Actor *a) {
    if (rec_on && !a->rec) return 0;
    if ((a->flags & 0x20000) && !scene_client_group(&scene_client, a->grp)) return 0;
    return actor_display(a) != 50;
}

// Poq.exe 0x489380：只揭開操控角色同組；上層擴大半徑，迴圈右／下界不含端點。
// 探索提升（Poq.exe 0x460e10 的格子部分：只增不減；背景髒矩形與小地圖刷新由每幀重畫／mapwin.c 取代）
static void actor_explore(Actor *a) {
    const Scene *s = a->bound_scene;
    if (!scene_client.ready || !scene_client.scene || !s || !s->cells ||
        s != scene_client.scene || !scene_client_group(&scene_client, a->grp)) return;
    int r = a->view_range;
    if (a->layer > 10) r += a->layer == 14 ? 8 : a->layer == 16 ? 12 : 4;
    int gx = a->grid_valid ? a->grid_x : (int)a->x;
    int gy = a->grid_valid ? a->grid_y : (int)a->y;
    int wx = (int)(a->x * 64), wy = (int)(a->y * 64);
    for (int x = gx - r; x < gx + r; x++) for (int y = gy - r; y < gy + r; y++) {
        if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
        double dx = (double)x * 64 - wx, dy = (double)y * 64 - wy;
        int distance = (int)sqrt(dx * dx + dy * dy);
        if (distance > r * 64) continue;
        Cell *c = &s->cells[y * s->w + x];
        int level = 1;
        if (distance < r * 64 - 256) level = 10;
        else if (distance < r * 64 - 128 && c->actor &&
                 c->actor->object_kind >= 30 && c->actor->object_kind <= 100) level = 10;
        // shortcut: SDL每畫面重畫，直接提高格子探索值；CMapDialog表面／探索通知接齊時補通知，
        // 不用原版背景髒矩形，也不把尚未移植的小地圖刷新入口標完成。
        if (c->view < level) c->view = level;
    }
}

static void actor_skills_init(Actor *a);
static int actor_light_init(const Actor *a);
static void actor_appearance_init(Actor *a, const char *override);
static void actor_behavior_reset(Actor *a);
static void npc_behavior_reset(Actor *a);
// 劇情handle為正整數；無劇情物件使用高半區，跨場景不重用，避免關聯命中另一個角色。
static uint32_t next_actor_object_id = 0x80000000u;

// Poq.exe 0x4862e0：套用角色模板；狀態103先啟用，再寫模型種類、組別、光源、占位與探索。
// 新角色一建立就把它的圖叫進來（怪物群、隊友、劇情中途出現的人以前沒有預載，網頁版第一次看到才開始下載，下載好之前整個人不見）。
// 同一個角色表列只叫一次；場景進入時的 preload_roles 會一次叫齊場景裡的人，這裡補其他途徑建立的
static void role_prefetch(const RoleDef *d) {
    static const RoleDef *done[1024];
    static int n;
    if (!d) return;
    for (int i = 0; i < n; i++) if (done[i] == d) return;
    if (n < 1024) done[n++] = d;
    uint32_t g[MAX_ROLE_PARTS * 2 + 1];
    int m = 0;
    if (d->anim) g[m++] = d->anim;
    else if (d->graphic[0]) {
        char name[24];
        const char *list[1] = {name};
        snprintf(name, sizeof name, "%.8s_%03d.PLI", d->graphic, d->pli);
        res_preload("PLI", list, 1);
        m = role_anim_ids(d->graphic, d->pli, d->styles, g, MAX_ROLE_PARTS * 2);
    }
    if (m > 0) res_fetch_now(g, m);
}
static void actor_setup(Actor *a, const RoleDef *d) {
    if (!a->def) role_prefetch(d);
    // 新角色的候選初值屬於建構；重套模板不可先清掉既有候選。
    if (!a->def) {
        if (!a->object_id) a->object_id = next_actor_object_id++;
        for (int i = 0; i < 4; i++) a->candidate[i] = -1;
        a->remembered[0] = -1;
        a->dynamic_state = 1;
        // shortcut: 舊建立入口缺初次動態退出，出生先允許命令；生命週期接齊後交退出回呼設置。
        a->dynamic_active = 1;
        a->layer = 1;
        if (!a->object_kind) a->object_kind = 31;
        for (int i = 0; i < 20; i++) a->modifier[i].time = -1;
        a->hp_percent = 100;
        a->battle_return = 1;  // 0x485270 建構：[0x148] = 1，npc_pick_target 沒候選時才退戰
        a->effect_speed = 100;
        a->display_number = -1;
        a->npc_home_x = a->npc_home_y = -1;
        a->counter_skill = -1;
    }
    actor_cancel_command(a);
    actor_name_set(a, a->spec ? a->spec->event : "");
    a->display_name[0] = 0;
    actor_status_notify(a, 103, 1, -1);
    a->grp = d->group; // 模板直接賦值，不走換組清目標的副作用。
    a->flags = 0;
    a->def = d;
    a->model_type = d->model_type;
    actor_light_init(a);
    a->seek_adjust = 0;
    a->view_range = 12;
    actor_size_set(a, d->size < 7 ? 1 : d->size - 5);
    if (a->bound_scene == unit_scene && a->grid_valid)
        actor_place(a, a->grid_x, a->grid_y, 1);
    actor_explore(a);
    a->idle = d->idle;
    a->walk = d->walk;
    a->fidle = d->fight_idle;
    a->fwalk = d->fight_walk ? d->fight_walk : d->walk;
    a->speed = d->speed;
    a->fight_speed = d->fight_speed;
    if (d->scale != 65535) a->scale = d->scale;
    actor_speed_update(a);
    // 模板的ffff表示不覆寫，並非清除；-1及0則照值寫入。
    // shortcut: 探索的外部小地圖通知仍待CMapDialog，角色格子探索已同步更新。
    if (d->counter_skill != 65535) a->counter_skill = d->counter_skill;
    for (int i = 0; i < 4; i++)
        if (d->candidate[i] != 65535) a->candidate[i] = d->candidate[i];
    actor_skills_init(a);
    actor_properties_update(a);
    if (d->anim) {
        // BeastieMdl／陷阱沿用單一Anim播放器，不走CCharactor的紙娃娃初始化。
        a->role = role_for(d);
        a->body_ready = a->shown_ready = 0;
    } else actor_appearance_init(a, NULL);
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
    IdList ids = {0};
    // 有 anim 的角色用 role_load_anim（單一動畫），沒有 PLI 檔；graphic 空的是這種，不能要 _000.PLI（創角 404）
    if (hero_def && hero_def->anim) id_add(&ids, hero_def->anim);
    else if (hero_def && role_drawable(hero_def)) defs[n++] = hero_def;
    for (int i = 0; i < s->npc_count && n < MAX_ACTORS; i++) {
        const RoleDef *d = roledef_find(s->npcs[i].tmpl);
        int dup = !d || !role_drawable(d);
        if (d && d->anim) { id_add(&ids, d->anim); dup = 1; }
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
    for (int i = 0; i < n; i++) {
        uint32_t g[MAX_ROLE_PARTS * 2];
        int m = role_anim_ids(defs[i]->graphic, defs[i]->pli, defs[i]->styles, g, MAX_ROLE_PARTS * 2);
        for (int k = 0; k < m; k++) id_add(&ids, g[k]);
    }
    res_fetch_now(ids.v, ids.n);
    free(ids.v);
}

int difficulty = 2;  // 原版 0x1039b000：1 容易、2 普通、3 困難（開新遊戲 JM21 選）
#ifdef STORY
static int sroom;    // 目前場景的劇情房間
static void story_killed(int h);
#endif

// 建立時複製劇情人物的現值：SetInt初值不發屬性通知，不能只等story_msgs。
// ONT角色與入口生成角色共用；僅建立時套用，避免每拍重設受傷、換組及狀態。
static void actor_person_init(Actor *a, int h, int (*get)(int, const char *)) {
    actor_group(a, get(h, "G_AIGroup"));
    a->hp = get(h, "HP"); a->hp_max = get(h, "HP_MAX");
    a->mp = get(h, "MP"); a->mp_max = get(h, "MP_MAX");
    a->level = get(h, "Level");
    a->ap = get(h, "AP"); a->ap_max = get(h, "AP_MAX");
    a->dp = get(h, "DP"); a->hr = get(h, "SR"); a->sr = get(h, "HR");
    a->ex[20] = get(h, "Invincible");
    a->ex[64] = get(h, "B_C_MOVE_SPEED");
    a->ex[65] = get(h, "B_C_FIGHT_SPEED");
    a->ex[66] = get(h, "B_C_VRANGE");
    actor_status_notify(a, 103, get(h, "G_NPCActive") != 0, -1);  // 0x51b 的起始值：G_NPCActive 0 = 能對話（其餘狀態 103 無对话）
    actor_properties_update(a);
}

static void spawn_npcs(const Scene *s) {
    actor_count = 1;  // actors[0] 是主角
    for (int i = 0; i < s->npc_count && actor_count < MAX_ACTORS; i++) {
        const NpcSpec *n = &s->npcs[i];
        const RoleDef *d = roledef_find(n->tmpl);
        if (!d || (d->beast && d->air) || !role_drawable(d)) continue;
        Actor *a = &actors[actor_count++];
        memset(a, 0, sizeof *a);
        npc_behavior_reset(a);
        a->x = n->x + 0.5f;
        a->y = n->y + 0.5f;
        a->spec = n;
        a->object_kind = 40;
        actor_setup(a, d);
        if (!actor_bind_scene(a, s) || !actor_place(a, (int)a->x, (int)a->y, 1)) a->role = NULL;
#ifdef STORY
        const char *event = actor_init_name(a);
        // 同名的 ONT 入口（彭城 3 隻 bear_a）各配一個還沒有角色的劇情物件：以前每個都配到第一個，其餘兩隻劇情狗熊永遠沒有身體、打不死，
        // 第 2 格（最後一隻死了放萧起）永遠等不到。走查（網頁 KillAlias bear_a）發現
        a->sh = 0;
        for (int h = story_next(sroom, 0); event && *event && h && !a->sh; h = story_next(sroom, h)) {
            if (story_kind(h) != 1 || strcmp(story_alias(h), event)) continue;
            int used = 0;
            for (int j = 1; j < actor_count - 1; j++) used |= actors[j].sh == h;
            if (!used) a->sh = h;
        }
        if (!a->sh && event && *event) a->sh = story_find(sroom, event);  // 入口比物件多：多的還是配到第一個（原行為）
        if (a->sh) actor_person_init(a, a->sh, story_get_int);
#endif
    }
}

// 房間的怪物群（EasyRPG.dll 0x10052550）：每群挑一個「前场景名称 = 區域」的入口點，整群出現在附近
static NpcSpec mon_specs[MAX_ACTORS];

static void mon_derive(Actor *a);
static int skill_attack_id(const SkillRow *r);
static void arrow_spend(Actor *h, const SkillRow *r);

// 怪物的技能1（Data/skill_npc.txt，0x100529a0 → 0x10059b20 依索引名找）：技能等級用表上的初始技能等级，沒填 0
static const SkillRow *npc_skill(const char *key) {
    static SkillRow rows[512];
    static int n = -1;
    if (n < 0) n = skill_rows_file("skill_npc.txt", rows, 512);
    for (int i = 0; i < n; i++)
        if (!strcmp(rows[i].key, key)) return &rows[i];
    return NULL;
}

// 放一隻怪物在 (x, y)。原版 0x44f580 建 SInitCha（0x44eec0 初值：行走休息 0、範圍 0）後只把平時邏輯設 10：
// 不閒逛，只會在偏離出生格時走回去（0x492730 的「其他」分支）；原版沒有固定閒逛半徑。
static Actor *mon_make(const MonDef *m, const RoleDef *d, int x, int y, int lv) {
    Actor *a = &actors[actor_count];
    NpcSpec *sp = &mon_specs[actor_count++];
    *sp = (NpcSpec){.type = 13, .logic = 10, .tmpl = d->id, .x = x, .y = y};
    memset(a, 0, sizeof *a);
    npc_behavior_reset(a);
    a->x = x + 0.5f;
    a->y = y + 0.5f;
    a->spec = sp;
    a->mon = m;
    a->object_kind = m->istrap == 1 ? 24 : 40;  // istrap：1 = 陷阱（CTrapBase），10 = 可馴服的動物、20 = 召喚獸，都是一般角色（以前非 0 全當陷阱，狼群看不見、打不到）
    // 陷阱建構（0x49b8d0）：狀態0、未被發現、計數0；0x49ba30 設模型時的次數（+0xb7）沒找到來源，一律 1。
    if (m->istrap == 1) a->trap_uses = 1;
    a->level = lv < 1 ? 1 : lv > 99 ? 99 : lv;
    a->side = 1;
    actor_group(a, 10000);  // shortcut: 沒有劇情時怪物都算同一個 NPC 組；有劇情時 actors_sync 換成 G_AIGroup
    mon_derive(a);
    a->hp = a->hp_max;
    a->exp = mon_stat(m->exp, a->level, 0);
    a->atk = method_find(skill_attack(m->skill[0]));  // 技能表沒有這招時的招式；有的話每次出手前由 mon_pick 選
    actor_setup(a, d);
    actor_group(a, 10000); // 出生AI組覆寫在模板預設組之後。
    a->trap_group = a->grp;
    if (unit_scene && m->istrap != 1 && (!actor_bind_scene(a, unit_scene) ||
        !actor_place(a, (int)a->x, (int)a->y, 1))) a->role = NULL;
    else if (a->grid_valid) a->x = a->grid_x + 0.5f, a->y = a->grid_y + 0.5f;  // actor_place 換了空格就跟著搬
    return a;
}

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
            if (!d || !role_drawable(d)) continue;
            int x = en->x, y = en->y;
            int lv = mg->level, tier = mg->tier;
            if (lv < 0) lv = tier * 5 - rand() % 5;
            Actor *a = mon_make(m, d, x, y, lv);
            printf("怪物 %s Lv%d HP%d 在 (%d,%d)\n", m->name, a->level, a->hp, x, y);
        }
    }
}

// ---- 戰鬥 ----
#define FIGHT_TICKS 300  // shortcut: 只給玩家組（主角／隊友）：原版其退戰由伺服端送狀態0（Poq 0x522，伺服端程式不在 Poq／EasyRPG.dll），時長查不到；NPC組走 npc_pick_target（0x490430）
static int exp_table[100], exp_levels;  // Data/experience.txt：第 n 列 = 升到 n+1 級要的經驗

static int alive(const Actor *a) { return a && a->role && !a->object_dead && a->act != ACT_DEAD; }

static float cell_dist(const Actor *a, const Actor *b) {
    return fmaxf(fabsf(a->x - b->x), fabsf(a->y - b->y));
}

// 攻速（MethodMain綁定時）：縮短 = 總拍數 − trunc(float(總拍數)×攻速倍率)；每幀延遲調整 = −縮短 / 幀數。
// 有餘數時：縮短 > 0 調整再 −1、要補 幀數 − 餘數 拍，否則補 −餘數 拍；調整 < −1 時補的減半。補的（> 0 才算）加到出手間隔
static void attack_speed(Actor *a) {
    const Seq *q = role_seq(a->role, actor_seq(a));
    if (!q || !q->count) return;
    int total = (int)seq_ticks(q), cut = total - (int)((float)total * actor_fight_ratio(a)), rem = cut % q->count;
    a->adj = -(cut / q->count);
    a->extra = 0;
    if (rem) {
        if (cut < 1) a->extra = -rem;
        else {
            a->adj--;
            a->extra = q->count - rem;
        }
        if (a->adj < -1) a->extra /= 2;
    }
}

static int grp_npc(int grp);

// Poq.exe 0x48d330：MethodMain綁定／解除；冷卻在招式建立時裝入，活招式存在期間不遞減。
// 單機模式2的NPC種類60只用餘拍，不夾負值；其他種類先夾餘拍再加間隔。
static void actor_method_bound(Actor *a, int on) {
    if (on) {
        attack_speed(a);
        int delay = a->def ? a->def->atk_delay : 0;
        int cool = a->extra;
        if (!grp_npc(a->grp) || a->object_kind != 60) {
            // shortcut: 空白測試／缺模板的間隔0不做mod0；正式模板若有0須核對資料契約。
            int interval = grp_npc(a->grp) ? (int)((float)delay * actor_fight_ratio(a)) : delay + (delay ? rand() % delay : 0);
            cool = interval + (cool < 0 ? 0 : cool);
        }
        a->cool = cool;
        if (a->sequence_delay_bias) a->adj += 2;
    } else a->adj = 0;
    if (a->trail_mode == 1 && (on || a->modifier[6].time < 21)) {
        int time = on ? -1 : 20;
        if (a->modifier[6].value != 1 || a->modifier[6].time != time) {
            a->modifier[6].value = 1; a->modifier[6].time = time;
            actor_illusion_begin(a);
        }
    }
}

// 招式觸發 type 8：ID2 非 0 設旗標、0 清掉（角色虛擬函式 +0x48）
static void act_flags(Actor *a, const Trig *t) {
    if (t->id2) actor_set_flags(a, (unsigned)t->id, t->id2);
    else actor_clear_flags(a, (unsigned)t->id);
}

// 動作結束（播完或被換掉）：招式 iFlag 3 的旗標觸發（多半是把動作中設的旗標清掉）
// shortcut: iFlag 3 當成「動作結束時」是從成對的設定／清除推的；其他種類的 iFlag 3 觸發沒做
static void act_end(Actor *a) {
    for (int i = 0; a->act == ACT_ATTACK && a->atk && i < a->atk->trig_count; i++)
        if (a->atk->trig[i].flag == 3 && a->atk->trig[i].type == 8) act_flags(a, &a->atk->trig[i]);
}

static void start_act(Actor *a, int act, int seq) {
    act_end(a);
    ++a->action_serial;
    a->aim_valid = 0;
    a->act = act;
    a->act_seq = seq;
    a->act_tick = a->act_frame = a->frame_tick = a->act_done = a->adj = a->extra = 0;
    a->last_frame = -1;
    a->body_ready = 0;
    a->moving = 0;
    if (act == ACT_ATTACK) attack_speed(a);
}

// 主角數值。遊俠模板（EasyRPG.dll 0x1013c380）：Str 30、Con 25、Dex 25；升級表 Upgrade_HP_MAX = 120 28、Upgrade_MP_MAX = 140 4
// → HP_MAX = 120 + 28×等級、MP_MAX（精氣）= 140 + 4×等級
// 職業倍率（0x1002d510，遊俠 K = 150）：AP、AP_MAX = 基礎 × (Str+K)/K；DP = 基礎 × (Con+K)/K；
// HR（閃避）= (基礎 + 3×Dex) × (Dex+K)/K；SR（命中）= (基礎 + 4×Dex) × (Dex+K)/K
// 基礎 AP、DP、HR、SR 都是 0，全靠身上裝備的特性（劇情開場給扶苏 青銅彎刀 qingtongwandao1、布衣 buyi1）
// 主角身上的道具，依 UI_Pos：0~23 背包 m_pItem[0..23]、24~29 快捷欄 m_pShortcutItem[0..5]、30~40 裝備欄 JM55 m_pItem[UI_Pos − 30]
// （頭盔、盔甲、右手、左手、護腕、靴子、右戒、左戒、項鍊、腰帶、成套）。name[0] == 0 = 空格
// shortcut: 背包、快捷欄的 UI_Pos 是照控制項順序推的，原版 Poq.exe 的格子對應還沒查
#define INV_N 41
#define INV_BAG 30
static Item inv[INV_N];

// 六項屬性，順序 = 人物屬性加點面板 m_pAttrib[0..5] = 力量、体质、敏捷、悟性、魅力、智慧（Poq.exe 0x554d66 起）。開局值照遊俠扶苏模板
enum { STR, CON, DEX, SAVVY, CHARM, WISDOM };
static int hero_attr[6] = {30, 25, 25, 30, 40, 20};
static int hero_extra_exp, hero_extra_skill;  // 剩餘屬性點、技能點：每升一級 +10、+1（EasyRPG.dll 0x10026d30）
static int hero_money = 100;  // 同一模板的 Money
static int hero_bank;  // 人物屬性 Bank（存在錢庄的錢，money.c）；劇情版由 story_begin／story_end 與劇情物件同步，存檔跟著劇情存
static int hero_gender = 0, hero_age = 29;  // 同一模板的 Gender、Age
// 創角選的主角範本（秦殇/主角/<職業人名>）的外觀：G_PicID = 角色表列（29 遊俠、379 力士、207 刺客、208 術士、209 巫師）、
// G_EquipStr<職業> = 外觀字串（動作方式*各部件款式），之後身上裝備再蓋上去（hero_look）。沒有劇情時是遊俠扶蘇
static int hero_pic = 29;
static char hero_look_str[96] = "001*009*009*009*009*009*009*009*009*009*000*009";
static int hero_prof = 1;  // 職業序號（0x94c2fb，2131 + 序號 = 1 游侠、2 力士、3 刺客…）；測試用 --prof 換
// shortcut: 開新遊戲選職業的畫面還沒做

// 技能參數的值：(技能等級 − 1) × 增量 + 初始值（EasyRPG 0x10036160、0x10024xxx 都是這個式子）
static float skill_value(float init, float incr, int level) { return (level - 1) * incr + init; }

// 狀態帶的技能參數套到身上（0x100300f0 → 0x10036160）。屬性代號 20~99 對名稱表 "Invincible" 起每格 0x40（.data 0x1039b5f0）；
// 原版鍵名 HR = 閃避、SR = 命中，這裡 hr = 命中、sr = 閃避。百分比 = 加上目前值 × v / 100
// 參數「是否对敌」的套在對方身上（0x1002fe20），其他的套在放技能的人身上
// shortcut: 力量等六項屬性的加值沒有回算到攻防（遊俠技能表沒有用到）
static int *stat_ptr(Actor *h, int id) {
    return id == 23 ? &h->hp_max : id == 24 ? &h->mp_max : id == 41 ? &h->ap : id == 42 ? &h->ap_max : id == 53 ? &h->dp :
           id == 60 ? &h->sr : id == 61 ? &h->hr : id == 62 ? &h->hp_speed : id == 63 ? &h->mp_speed :
           id >= 20 && id < 100 ? &h->ex[id] : NULL;
}
static void actor_mods(Actor *h, int pct) {
    for (int b = 0; b < h->buff_n; b++)
        for (int i = 0; h->buff[b].skill && i < h->buff[b].skill->mod_n; i++) {
            const SkillMod *m = &h->buff[b].skill->mod[i];
            if ((h->buff[b].side != 2 && (m->enemy > 0) != h->buff[b].side) || m->pct != pct) continue;
            int *f = stat_ptr(h, m->id);
            float v = skill_value(m->init, m->incr, h->buff[b].level);
            if (f) *f += (int)(pct ? *f * v / 100 : v);
        }
}

// 身上裝備的特性（穿上時 0x1002fe20 複製到人身上，重算 0x100300f0 只算生效的）：固定值、百分比分兩輪，跟技能參數同一套屬性
static void item_mods(Actor *h, int pct) {
    const Item *list = h != &actors[0] && h->eq ? (const Item *)h->eq : inv;  // 隊友用自己的裝備表
    for (int s = INV_BAG; s < INV_N; s++)
        for (int i = 0; list[s].name[0] && i < list[s].fx_n; i++) {
            const ItemFx *f = &list[s].fx[i];
            int *p = f->on && (f->pct > 0) == pct ? stat_ptr(h, f->id) : NULL;
            if (p) *p += (int)(pct ? *p * f->v / 100 : f->v);
        }
}

// 打中觸發的狀態改的數值（Patch_N，0x10031d70 → 0x10034620）：石化 2 → DP×2、失明 33 → 命中閃避各減半、中毒 1 → HP_SPEED = −8 × 毒傷
static void status_patch(Actor *a) {
    for (int b = 0; b < a->buff_n; b++)
        if (a->buff[b].status == 2) a->dp *= 2;
        else if (a->buff[b].status == 33) { a->hr /= 2; a->sr /= 2; }
        else if (a->buff[b].status == 1) a->hp_speed = -8 * a->buff[b].patch;
}

// 重算數值（0x1002d8d0）：基礎值 → 固定加值 → 回復速度（0x1002fa90）→ 職業倍率（0x1002d510）→ 百分比。加點、升級、狀態變化後呼叫，不回血
// attr 六項屬性、prof 職業序號（R 表）；equip = 主角才有的裝備特性（隊友沒有裝備模型，見 team_derive）
// 隊友升級（原版 0x10056120 對每個 Upgrade_<屬性> 列呼叫 0x10057f50：值 = (等級 + c) × 每級 + 基礎，再 0x1002daa0 存 *0 基底）：這裡 team_sync → 本函數
// 用同一組升級表（hp_max = 基礎 + 每級 × 等級）一次重算；0x10030c90 相依通知（Str→BD_MAX、Con→HP_SPEED、Dex→HR／SR、Wisdom→MP_SPEED）也由下面的順序涵蓋
static void person_derive(Actor *h, const int *attr, int prof, int equip) {
    const int K = 150, lv = h->level;
    // 升級表 Upgrade_HP_MAX／MP_MAX = 基礎 每級（秦殇/主角 範本裡 0x1005f3c0 的前兩個浮點，依職業 IsActorNPC 1~5；
    // 遊俠 120 28／140 4、力士 150 38／120 3、刺客 105 24／130 4、術士 110 22／180 5、巫師 100 20／200 6）
    static const int UP[6][4] = {{120, 28, 140, 4}, {120, 28, 140, 4}, {150, 38, 120, 3}, {105, 24, 130, 4}, {110, 22, 180, 5}, {100, 20, 200, 6}};
    const int *up = UP[prof < 1 || prof > 5 ? 0 : prof];
    h->hp_max = up[0] + up[1] * lv;
    h->mp_max = up[2] + up[3] * lv;
    h->ap = h->ap_max = h->dp = h->hr = h->sr = h->hp_speed = h->mp_speed = 0;
    memset(h->ex, 0, sizeof h->ex);
    if (equip) item_mods(h, 0);
    actor_mods(h, 0);
    // 裝備、技能加的力量 27、体质 28、敏捷 29、智慧 32 也算進倍率和回復
    const int str = attr[STR] + h->ex[27], con = attr[CON] + h->ex[28], dex = attr[DEX] + h->ex[29], wis = attr[WISDOM] + h->ex[32];
    // 回復速度依職業：HP_SPEED = (Con − a) × HP_MAX × b / 100 / 分母，MP 同理用 Wisdom；分母 0 當 1
    static const int R[6][8] = {{0}, {25, 50, 25, 4, 20, 50, 10, 2}, {30, 60, 15, 2, 10, 40, 5, 2}, {20, 50, 10, 2, 20, 50, 10, 2},
                                {25, 40, 25, 4, 35, 60, 35, 4}, {20, 40, 10, 2, 40, 60, 20, 2}};
    const int *r = R[prof < 1 || prof > 5 ? 1 : prof];  // 分母：R[3]=2 → Con + (Lv − c)×2；R[3]=4 → Lv×2 − c + Con（原版兩種寫法）
    int hd = r[3] == 2 ? con + (lv - r[2]) * 2 : lv * 2 - r[2] + con, md = r[7] == 2 ? wis + (lv - r[6]) * 2 : lv * 2 - r[6] + wis;
    h->hp_speed = (con - r[0]) * h->hp_max * r[1] / 100 / (hd ? hd : 1);
    h->mp_speed = (wis - r[4]) * h->mp_max * r[5] / 100 / (md ? md : 1);
    // 隊友的裝備由 team_equip_load 從劇情人物身上的道具物件取（equip = 1）；沒有任何裝備資料才用等級估：2×等級 + 8（2026-11-22 更新，以前一律估）
    if (!equip) h->ap = h->ap_max = 2 * lv + 8;
    h->ap = h->ap * (str + K) / K;
    h->ap_max = h->ap_max * (str + K) / K;
    h->dp = h->dp * (con + K) / K;
    // 原版鍵名 SR = 命中率、HR = 闪避率（UI_C.dlg 2116/2115 對 Poq.exe 0x54f820 的顯示順序）；這裡 hr = 命中、sr = 閃避（跟怪物表一樣）
    h->hr = (h->hr + 4 * dex) * (dex + K) / K;
    h->sr = (h->sr + 3 * dex) * (dex + K) / K;
    if (equip) item_mods(h, 1);
    actor_mods(h, 1);
    status_patch(h);
    actor_properties_update(h);
    if (h->hp > h->hp_max) h->hp = h->hp_max;
    if (h->mp > h->mp_max) h->mp = h->mp_max;
}

static void hero_derive(Actor *h) { person_derive(h, hero_attr, hero_prof, 1); }
// 隊友同一個公式（0x1002d8d0 對所有人物）
static void team_derive(Actor *h) { person_derive(h, h->team_attr, h->team_prof, h->eq != NULL); }

// 怪物數值：表上的基礎值 → 固定加值 → 百分比（同 0x1002d8d0，沒有職業倍率）
static void mon_derive(Actor *a) {
    const MonDef *m = a->mon;
    a->hp_max = mon_stat(m->hp, a->level, 1);
    a->ap = mon_stat(m->ap, a->level, 2);
    a->ap_max = mon_stat(m->ap_max, a->level, 2);
    a->dp = mon_stat(m->dp, a->level, 3);
    a->hr = mon_stat(m->hit, a->level, 3);
    a->sr = mon_stat(m->dodge, a->level, 3);
    // 怪物表讀進來時（0x1004xxxx 讀 npc 表）：HP_SPEED = 生命恢复率 × HP_MAX / 10；打击恢复率、減傷、免疫放進對應屬性
    a->hp_speed = m->regen * a->hp_max / 10;
    a->mp_speed = 0;
    memset(a->ex, 0, sizeof a->ex);
    a->ex[64] = m->mspeed;
    a->ex[65] = m->mfight;
    a->ex[66] = m->vrange;
    a->ex[75] = m->djhf;
    a->ex[76] = m->jswl;
    a->ex[77] = m->jsmf;
    for (int k = 0; k < 7; k++) a->ex[92 + k] = m->immu[k];
    if (m->melem >= 1 && m->melem <= 5) {
        a->ex[41 + 2 * m->melem] = mon_stat(m->map, a->level, 2);
        a->ex[42 + 2 * m->melem] = mon_stat(m->map_max, a->level, 2);
    }
    for (int k = 0; k < 5; k++) a->ex[54 + k] = mon_stat(m->mdp[k], a->level, 3);
    if (a->copied) {
        a->ap = a->copy[0];
        a->ap_max = a->copy[1];
        memcpy(&a->ex[43], &a->copy[2], 10 * sizeof(int));
    }
    actor_mods(a, 0);
    actor_mods(a, 1);
    status_patch(a);
    actor_properties_update(a);
    if (a->hp > a->hp_max) a->hp = a->hp_max;
}

static void hero_derive(Actor *h);
static void actor_derive(Actor *a) {
    if (a->team) team_derive(a);
    else if (a->mon && a->clone_equip) {  // 分身：人物公式 + 施放者的裝備；移動／出手速度、視距、免疫是怪物表的（人物公式會清 ex），算完還回去
        int keep[100];
        memcpy(keep, a->ex, sizeof keep);
        person_derive(a, a->team_attr, a->team_prof, a->clone_equip == 1);
        for (int k = 64; k <= 66; k++) a->ex[k] = keep[k];
        for (int k = 92; k < 99; k++) a->ex[k] = keep[k];
    }
    else if (a->mon) mon_derive(a);
    else hero_derive(a);
}

static void hero_stats(Actor *h, int level) {
    h->level = level;
    hero_derive(h);
    h->hp = h->hp_max;
    h->mp = h->mp_max;
}

static void actor_kill(Actor *t, Actor *by);
static void actor_kill_mode(Actor *t, Actor *by, int mode);
static void mon_loot(const Actor *m);
static void hit_wear(Actor *a, Actor *t, const SkillRow *sk);

// 系統心跳（Poq.exe 註冊的 refresh 指令「每秒一次」→ USER_Refresh）：狀態計時（0x10055e90），再回 HP、MP（0x10054450）
static void actor_second(Actor *h) {
    int changed = 0;
    for (int b = 0; b < h->buff_n; b++)
        if (++h->buff[b].elapsed >= h->buff[b].time && h->buff[b].time >= 0) {
            actor_status_notify(h, h->buff[b].status, 0, -1);
            h->buff[b--] = h->buff[--h->buff_n]; changed = 1;
        }
    if (changed) { actor_derive(h); actor_effects_recompute(h); }
    if (h->act == ACT_DEAD) return;
    int hs = h->hp_speed, ms = h->mp_speed;
    if (!h->fight) {  // 不在戰鬥（FightStatus 0）回得快三倍
        if (hs > 0) hs *= 3;
        if (ms > 0) ms *= 3;
    }
    int dh = (hs + h->hp_tmp) / 100;
    h->hp += dh;
    h->hp_tmp = (hs + h->hp_tmp) % 100;
    h->mp += (ms + h->mp_tmp) / 100;
    h->mp_tmp = (ms + h->mp_tmp) % 100;
    h->hp = h->hp < 0 ? 0 : h->hp > h->hp_max ? h->hp_max : h->hp;
    h->mp = h->mp < 0 ? 0 : h->mp > h->mp_max ? h->mp_max : h->mp;
    if (dh < 0 && h->hp < 1 && h->ex[20] < 1) actor_kill(h, NULL);  // 回復是負的（中毒）扣到 0、不是無敌就死（0x100574e0）
}

// 技能參數裡屬性代號 < 20 的改技能自己的欄位（0x10026270 → 函式表 .data 0x1039e8f4）：10 AttackID、11 SUR、12 RangeMin、13 RangeMax、
// 14 Time、15 MPU、16 Speed、17 EDR、18 B_C_JDR、19 B_C_DJHF。值 = (技能等級 − 1) × 增量 + 初始值，等級超過 10 用 9（原版如此）
static float skill_field(const SkillRow *r, int id, float def) {
    int lv = r->level > 10 ? 9 : r->level;
    for (int i = 0; i < r->mod_n; i++)
        if (r->mod[i].id == id) return (int)(lv ? skill_value(r->mod[i].init, r->mod[i].incr, lv) : r->mod[i].init);
    return def;
}

// 技能的精气消耗（MPU）、作用時間（Time，沒填預設 5 秒）、作用範圍（RangeMin 預設 0、RangeMax 預設 1，技能基底 0x1005d4a0）、招式（AttackID）
static int skill_mpu(const SkillRow *r) { return (int)skill_field(r, 15, (int)skill_value(r->mpu, r->mpu_inc, r->level)); }
static int skill_time(const SkillRow *r) { return (int)skill_field(r, 14, r->time < 0 ? 5 : (int)skill_value(r->time, r->time_inc, r->level)); }
static int skill_range_max(const SkillRow *r) { return (int)skill_field(r, 13, r->range < 0 ? 1 : (int)skill_value(r->range, r->range_inc, r->level)); }
static int skill_attack_id(const SkillRow *r) { return (int)skill_field(r, 10, skill_attack(r->key)); }

// Poq.exe 0x488850：精確ID包含停用項；未命中時按模板優先序挑啟用項。
// shortcut: fallback的比較暫存反編譯未定義，按模板索引最小者解讀，取得組語後校準。
static int actor_skill_find(const Actor *a, int id) {
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].id == id) return i;
    int best = -1, rank = a->template_skill_n;
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].enabled) {
        for (int j = 0; j < a->template_skill_n; j++) if (a->skills[i].id == a->template_skill[j]) {
            if (j < rank) { best = i; rank = j; }
            break;
        }
    }
    return best;
}

// Poq.exe 0x497ab0：以呼叫者層號減目標層號的分組值；保留255歸入最高組。
static int actor_layer_group(unsigned char layer) {
    return layer < 11 ? 0 : layer < 13 ? 1 : layer < 15 ? 2 : 3;
}
static int actor_layer_diff(const Actor *a, const Actor *target) {
    return actor_layer_group(a->layer) - actor_layer_group(target->layer);
}

// Poq.exe 0x48d630：跨層且近距離，或目標高出兩級以上，清選取。
static void actor_skill_select(Actor *a, int i) {
    if (i >= 0 && i < a->skill_n && a->foe) {
        int diff = actor_layer_diff(a, a->foe);
        if (diff && (a->skills[i].max < 3 || diff < -1)) i = -1;
    }
    a->selected = i;
}

// Poq.exe 0x48d6a0：按SkillUnit鏈的零起始順序選取；空鏈不改舊選取。
// MoveTo越界回空，且不檢查Enable、不改左右手ID。
static void actor_skill_select_index(Actor *a, int index) {
    if (a->skill_n <= 0) return;
    Actor *target = actor_target(a);
    int i = index >= 0 && index < a->skill_n ? index : -1;
    if (target && i >= 0) {
        int diff = actor_layer_diff(a, target);
        if (diff && (a->skills[i].max < 3 || diff < -1)) i = -1;
    }
    a->selected = i;
}

// Poq.exe 0x48e7a0：先寫右手ID，查找後依目標層篩選。
static void actor_right_skill(Actor *a, int id) {
    a->right_id = id;
    actor_skill_select(a, actor_skill_find(a, id));
}

// Poq.exe 0x48eab0：左手只寫ID，不改右手選取。
// shortcut: 單機共享Actor省略指定客戶端同步，連線支援時補傳輸。
static void actor_left_skill(Actor *a, int id) { a->left_id = id; }

// Poq.exe 0x488970：原距離大於3才修正，不能在修正後重新判斷。
static int actor_skill_range(const Actor *a, const ActorSkill *s) {
    int range = s->max;
    if (range > 3) {
        range += a->range_adjust;
        if (a->layer > 10) range += a->layer == 14 ? 3 : a->layer == 16 ? 4 : 2;
    }
    return range;
}

// Poq.exe 0x488d50：回傳Enable原值；遠程技能在可移動模板速度為0時不可用。
static int actor_skill_enabled(const Actor *a, const ActorSkill *s) {
    if (!a->def->speed || a->speed > 0 || s->max < 3) return s->enabled;
    return 0;
}

// Poq.exe 0x488ba0：冷卻查詢只判正數，不把負數視為冷卻中。
static int actor_cooling(const Actor *a) { return a->cool > 0; }

static int actor_row_slot(const Actor *a, const SkillRow *r) {
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].row == r) return i;
    return -1;
}
static int actor_row_range(const Actor *a, const SkillRow *r) {
    int i = actor_row_slot(a, r);
    // shortcut: 未進入角色鏈的臨時技能仍讀技能列，劇情技能同步接入後移除此分支。
    ActorSkill temporary = {.max = skill_range_max(r)};
    return actor_skill_range(a, i < 0 ? &temporary : &a->skills[i]);
}
static int actor_row_enabled(const Actor *a, const SkillRow *r) {
    int i = actor_row_slot(a, r);
    ActorSkill temporary = {.max = skill_range_max(r), .enabled = 1};
    return actor_skill_enabled(a, i < 0 ? &temporary : &a->skills[i]);
}

// Poq.exe 0x48af30：按外部識別刪第一個；選中的節點清空，後面索引跟隨搬移。
static void actor_skill_remove(Actor *a, int key) {
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].key == key) {
        // 陣列表示法補償（原版此入口不寫光環欄位）：保持來源節點身份，
        // 刪除來源後以0表示原版有效性查詢失敗，不能誤指搬上來的下一項。
        if (a->aura_skill == i + 1) a->aura_skill = 0;
        else if (a->aura_skill > i + 1) --a->aura_skill;
        if (a->selected == i) a->selected = -1;
        else if (a->selected > i) a->selected--;
        memmove(a->skills + i, a->skills + i + 1, (a->skill_n - i - 1) * sizeof *a->skills);
        a->skill_n--;
        return;
    }
}

// Poq.exe 0x48fb90：先改Enable，然後按低兩位切手；只改第一個識別相符項。
static void actor_skill_enable(Actor *a, int key, int enabled, unsigned hand) {
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].key == key) {
        ActorSkill *s = &a->skills[i];
        s->enabled = enabled;
        if (hand & 1) actor_right_skill(a, s->id);
        if (hand & 2) actor_left_skill(a, s->id);
        return;
    }
}

// Poq.exe 0x48fc20：重選左右手在寫新ID之前；保留因此可能選到舊節點的順序。
// shortcut: 單機共用資料省略技能替換通知，連線支援時補傳輸。
static void actor_skill_update(Actor *a, int key, int id, int min, int max, int p6, int p7) {
    for (int i = 0; i < a->skill_n; i++) if (a->skills[i].key == key) {
        ActorSkill *s = &a->skills[i];
        if (a->right_id == s->id) actor_right_skill(a, id);
        if (a->left_id == s->id) actor_left_skill(a, id);
        s->id = id; s->min = min; s->max = max; s->p6 = p6; s->p7 = p7;
        return;
    }
}

// Poq.exe 0x485f80：先插入完整節點，再按低兩位選手；模板檢查僅診斷，不拒絕新增。
// shortcut: 有序陣列限128項，達上限印診斷；需要更多技能時改動態配置。
// shortcut: 單機省略技能同步封包，連線支援時補傳輸。
static int actor_skill_add(Actor *a, ActorSkill skill, unsigned hand) {
    if (a->skill_n == MAX_ACTOR_SKILLS) { fprintf(stderr, "角色技能槽已滿\n"); return -1; }
    int slot = a->skill_n++;
    a->skills[slot] = skill;
    if (hand & 1) actor_right_skill(a, skill.id);
    if (hand & 2) actor_left_skill(a, skill.id);
    if (skill.id < 0) return slot;  // 被動／光環技能沒有 Attack方式（招式 ID −1）：靠 row 辨識，不是模板招式，模板檢查（0x485f80 的 AfxMessageBox）不適用
    for (int i = 0; i < a->template_skill_n; i++)
        if (a->template_skill[i] == skill.id) return slot;
    fprintf(stderr, "角色%d技能%d不在模板中\n", a->def ? a->def->id : -1, skill.id);
    return slot;
}

// 劇情／NPC技能列追加外部識別與等級距離，不再改寫RoleMdl模板表。
static void actor_skill_append(Actor *a, const SkillRow *r, int key) {
    if (!r) return;
    int id = skill_attack_id(r);
    actor_skill_add(a, (ActorSkill){.key = key, .id = id,
        .min = (int)skill_field(r, 12, skill_value(r->range_min, r->range_min_inc, r->level)),
        .max = skill_range_max(r), .p8 = 1, .p9 = 1, .enabled = 1, .row = r}, 0);
}
// Poq.exe 0x48f460：只清鏈，按原模板順序加入存在的招式；不重設左右手ID。
static void actor_skills_rebuild(Actor *a) {
    while (a->skill_n) actor_skill_remove(a, a->skills[0].key);
    // shortcut: 原版清鏈後的節點指標不能保留為陣列索引；選取清空，呼叫端重選手時恢復。
    a->selected = -1;
    a->template_skill_n = a->def->skill_count;
    for (int i = 0; i < a->template_skill_n; i++) a->template_skill[i] = a->def->skill_ids[i];
    for (int i = 0; i < a->template_skill_n; i++) {
        int id = a->template_skill[i];
        const Method *m = method_find(id);
        if (m) actor_skill_add(a, (ActorSkill){.id = id, .min = m->range_min,
            .max = m->range_max, .enabled = 1, .p8 = 1, .p9 = 1}, 0);
    }
}
static void actor_skills_init(Actor *a) {
    actor_skills_rebuild(a);
    a->right_id = a->left_id = -1;
    if (a->mon) for (int i = 0; i < 8; i++) actor_skill_append(a, npc_skill(a->mon->skill[i]), 0);
}

// 招式的 wManualID：2 = 對同隊的（0x43acb0 要同一隊 +0x339，反刺猬甲、妙手回春），3 = 對地點（凌空虚渡），4 = 放東西（召喚、分身），5 = 對陷阱（巧手除障）
static int skill_manual(const SkillRow *r) {
    const Method *m = method_find(skill_attack_id(r));
    return m ? m->manual : 0;
}

// 技能要不要對著敵人放：會造成傷害（Attack方式 ≠ 0）、有敌方状态、或有對敵的屬性參數
// 對同隊／地點的技能由hero_cast_point傳遞點選對象與地點，不要求敵人。
static int skill_needs_foe(const SkillRow *r) {
    int man = skill_manual(r);
    if (man == 2 || man == 3 || man == 4) return 0;
    if (man == 5) return 1;
    if (r->attack == 3 || r->attack == 8 || r->attack == 11) return 0;  // 召喚、陷阱、分身放在自己旁邊
    if (r->attack != 0 || r->estatus > 0) return 1;
    for (int i = 0; i < r->mod_n; i++)
        if (r->mod[i].enemy > 0 && r->mod[i].id >= 20 && r->mod[i].id < 100) return 1;
    return 0;
}

// 手持類（Hand，EasyRPG.dll 0x1003b760）：npc = IsActorNPC（1 人形主角、2、3）；rt／lt = 右手／左手道具類型（文字指令 c_equippos 傳來）。
// 類 7、9 → 3；8、10、22、27 → 0（刀劍類）；11、12 = 弓；13、14 = 箭；18~20 → 6；21、23、24 → 7。
// 有弓：有箭 人形 2／npc3 5，沒箭 人形 1／npc3 4；沒弓：右手類優先，右手沒有取左手類，都沒有依 npc 1→0、2→7、3→3。
static int hand_calc(int npc, int rt, int lt) {
    int c[2] = {rt, lt}, v[2] = {-1, -1}, bow = 0, arrow = 0;
    if (npc < 1 || npc > 3) return -1;  // 原版 2 < IsActorNPC − 1U 直接 return，不改 Hand
    for (int i = 0; i < 2; i++)
        switch (c[i]) {
        case 7: case 9: v[i] = 3; break;
        case 8: case 10: case 22: case 27: v[i] = 0; break;
        case 11: case 12: bow = 1; break;
        case 13: case 14: arrow = 1; break;
        case 18: case 19: case 20: v[i] = 6; break;
        case 21: case 23: case 24: v[i] = 7; break;
        }
    if (bow) return arrow ? (npc == 1 ? 2 : npc == 3 ? 5 : -1) : (npc == 1 ? 1 : npc == 3 ? 4 : -1);
    if (v[0] != -1) return v[0];
    if (v[1] != -1) return v[1];
    return npc == 1 ? 0 : npc == 2 ? 7 : 3;
}
// 主角目前的手持類（IsActorNPC = 職業序號 hero_prof：1 游俠、2 力士、3 刺客；以前固定傳 1，刺客裝弓箭算成游俠的 2，刺客的招式 W_Type 5 永遠用不了）：由右手（UI_Pos 32）、左手（33）道具類型算；裝備變動（hero_refresh）與技能重建時更新
static int hero_hand;
static void hero_hand_update(void) {
    int h = hand_calc(hero_prof, inv[32].name[0] ? inv[32].type : 0, inv[33].name[0] ? inv[33].type : 0);
    if (h != -1) hero_hand = h;
}

// 技能能不能用（0x1004c2a0）：等級 > 0、精气夠、武器合 W_Type（-1 不限；相同可以；0 也接受 Hand < 0、1 接受 2、4 接受 5）
static int skill_usable(const Actor *h, const SkillRow *r) {
    int w = r->wtype;
    if (r->level < 1 || h->mp < skill_mpu(r)) return 0;
    if (h->team) return 1;  // 隊友不查手持類與箭（Enable 規則 0x1004c2a0 用的 Hand／箭袋是主角的欄位，隊友的手持類沒有對應資料）
    // Enable 規則已對照 0x1004c2a0（等級 > 0、精氣 ≥ MPU、W_Type 與 Hand 的對應；下面 return 那行逐項相同）；原版沒箭靠 Enable 旗標（箭射完關 0x1003f370、裝箭開 0x10055b50），這裡直接看左手箭袋，效果等價
    if (!h->mon && r->attack == 1 && (w == 1 || w == 3) && (!inv[33].name[0] || (inv[33].type != 13 && inv[33].type != 14))) return 0;
    return w < 0 || w == hero_hand || (w == 0 ? hero_hand < 0 : w == 1 ? hero_hand == 2 : w == 4 && hero_hand == 5);
}

// 狀態分類（.data 0x1039f5c8，11 類各 13 格；0x100588c0）：不在表上回 -1
static int status_class(int st) {
    static const int T[11][13] = {{13, 42, 41, 44}, {76, 77, 82, 79, 80, 81, 78}, {5, 12, 20, 25, 39, 40, 49},
                                  {36, 38, 27, 37, 6, 35, 31, 32, 43, 51, 52, 53, 54}, {71, 72, 73, 74, 75}, {14, 23, 46, 47, 48, 63, 64},
                                  {65}, {11, 62}, {10, 1, 4, 33, 2, 17}, {8}, {19, 121}};
    for (int r = 0; r < 11; r++)
        for (int j = 0; j < 13 && T[r][j]; j++)
            if (T[r][j] == st) return r + 1;
    return -1;
}

// Poq.exe 0x494f50：讀角色byte，技能Buff名單不是第二份狀態來源。
// shortcut: 128..255原版會讀越過狀態陣列的物件欄位；拒絕越界，若資料有此ID再核對用途。
static int has_status(const Actor *a, int st) {
    unsigned id = (unsigned char)st;
    return id < 128u ? a->state[id] : 0;
}

// 加不上的狀態（0x10058410）：身上已有同一個；或已有第 4 類時加第 5、2 類，第 7 類時加第 6 類，第 11 類時加第 10 類，第 2、5 類各自不能再加同類
static int status_blocked(const Actor *a, int st) {
    int c = status_class(st);
    for (int b = 0; b < a->buff_n; b++) {
        int o = status_class(a->buff[b].status);
        if (a->buff[b].status == st || (o == 4 && (c == 5 || c == 2)) || (o == 7 && c == 6) || (o == 11 && c == 10) ||
            (o == 2 && c == 2) || (o == 5 && c == 5))
            return 1;
    }
    return 0;
}

// 速度與攻擊不能由狀態表入口寫入；刪除舊逐Buff猜優先序的簡化計算。
static int status_speed(const Actor *a) { return a->effect_applied ? a->effect_speed : 100; }
static int status_noattack(const Actor *a) { return (a->flags & 0x800) != 0; }

// 加狀態（0x10037f40）：加不上就回 0；先移掉互斥的（0x10054d50：同為第 1、3、4、6 類互斥；3 蓋 4、4 蓋 2/3/5、7 蓋 6、11 蓋 10），
// 技能參數跟著生效，播狀態表的聲音。r = NULL 是打中觸發的狀態（沒有參數）
static int buff_add_source(Actor *a, const SkillRow *r, int status, int side, int time, int patch, int source) {
    if (status > 0 && status_blocked(a, status)) return 0;
    int c = status_class(status);
    for (int b = 0; c > 0 && b < a->buff_n; b++) {
        int o = status_class(a->buff[b].status);
        if (o < 0) continue;
        if ((o == c && (c == 1 || c == 3 || c == 4 || c == 6)) || (c == 3 && o == 4) || (c == 4 && (o == 2 || o == 3 || o == 5)) ||
            (c == 7 && o == 6) || (c == 11 && o == 10))
            { actor_status_notify(a, a->buff[b].status, 0, -1); a->buff[b--] = a->buff[--a->buff_n]; }
    }
    if (a->buff_n == 8) return 0;
    a->buff[a->buff_n++] = (Buff){r, r ? r->level : 0, status, 0, time, side, patch};
    actor_derive(a);
    // 技能Buff名單負責期限；角色狀態byte與立即表格副作用由通知入口承接。
    // 重算留給固定拍，不能靠重算「最後狀態優先」冒充此次設置。
    actor_status_notify(a, status, 1, source);
    printf("狀態 %d 生效 %d 秒（%s）\n", status, time, side ? "對方" : "自己");
    return 1;
}

static int buff_add(Actor *a, const SkillRow *r, int status, int side, int time, int patch) {
    return buff_add_source(a, r, status, side, time, patch, -1);
}

// 修正入口的副作用；狀態17／100是原版byte狀態，不能用帶時間的技能Buff取代。
static void actor_modifier_changed(Actor *a, int index, int value) {
    if (index == 1) actor_speed_update(a);
    else if (index == 6) {
        if (value) actor_illusion_begin(a);
        else { actor_illusion_end(a); a->modifiers_dirty = 1; }
    } else if (index == 5) {
        if (unit_scene && scene_record_mode != 2)
            actor_status_notify(a, 100, value != 0, -1);
        // 第5修正的動畫分支原本由knock承接，現在兩者共用入口。
        a->knock = value;
        actor_animation_select(a, -1);
    } else if (index == 3 && value == 3 && a->def && a->def->panic > 0) {
        if (rand() % a->def->panic == 0) actor_status_notify(a, 17, 1, -1);
    }
}

// Poq.exe 0x48e8f0：保存未夾限的生命百分比；只在修正值或永久期限改變時擲潰散。
static void actor_health_percent(Actor *a, int percent) {
    a->hp_percent = percent;
    if (percent < 20) actor_modifier_set(a, 3, 3, -1);
    else if (percent < 50) actor_modifier_set(a, 3, 1, -1);
    else {
        actor_modifier_set(a, 3, 0, -1);
        a->modifiers_dirty = 1;
    }
}

// Poq.exe 0x48fae0：生命百分比及動畫門檻只在變更時處理，行為參數每次照寫。
// 單機省略0x323客戶端同步封包。
static void actor_parameters(Actor *a, int hp, int threshold, int p4, int p5, int p6, int p7) {
    if (a->hp_percent != hp) actor_health_percent(a, hp);
    if (a->speed_threshold != threshold) {
        a->speed_threshold = threshold;
        actor_animation_select(a, -1);
    }
    a->behavior[0] = p4; a->behavior[1] = p5;
    a->behavior[2] = p7; a->behavior[3] = p6;
}

// EasyRPG人物數值與客戶端角色共用Actor，以固定拍通知代替封包複製。
static int hero_burden(void);
static int hero_burden_max(const Actor *h);
// 負重百分比（EasyRPG 0x10042c30 送給客戶端的 BD×100/BD_MAX，超過再 +1 → 超重 ≥ 101）：Poq 0x488540 走路速度減半、0x48c050 動畫改走路序列。
// 以前主角的門檻從沒被設過（恆 0），所以超重沒有懲罰
static int burden_threshold(const Actor *a) {
    if (a != &actors[0]) return a->speed_threshold;
    int bd = hero_burden(), mx = hero_burden_max(a), t = mx > 0 ? bd * 100 / mx : 0;
    return mx > 0 && mx < bd ? t + 1 : t;
}
static void actor_health_sync(Actor *a) {
    if (!a->def || a->hp_max <= 0) return;
    int old_thr = a->speed_threshold;
    actor_parameters(a, (int)((int64_t)a->hp * 100 / a->hp_max), burden_threshold(a),
        a->behavior[0], a->behavior[1], a->behavior[3], a->behavior[2]);
    if ((old_thr < 101) != (a->speed_threshold < 101)) actor_speed_update(a);  // 跨過超重門檻：重算走路速度
}

static void strike(Actor *a, Actor *t, const SkillRow *sk, Uint32 tick);

// 清狀態（0x1003f130）：身上第 6~9 類狀態（status_class）的已過時間加 v（原版改 "Status_N" 屬性的第一個數字 = 已過秒數；v 取 char），
// 之後 actor_second 的計時就提早到期。技能參數 295（圣手祛魔）、attribT 的 clearstatus10／20／30
static void status_clear(Actor *x, int v) {
    v = (signed char)v;
    for (int b = 0; b < x->buff_n; b++) {
        int c = status_class(x->buff[b].status);
        if (c > 5 && c < 10) x->buff[b].elapsed += v;
    }
}

// 寶石特性 300~304（0x10025260／0x10025370／0x10025450／0x10025530／0x10025610，加上寶石 sign = 1、拆下 −1）：耐久上限（HP_MAX，之後耐久不得超過上限）、
// 等級、力量、體質、敏捷需求各加 v × sign。shortcut: 原版不夾下限（需求可能變負，讀的地方都用 ≥ 比較）；G_Info 說明字串引擎每次畫時現算
static void gem_fx(Item *w, const ItemFx *f, int sign) {
    int d = (int)f->v * sign;
    switch (f->id) {
    case 300: w->hp_max += d; if (w->hp > w->hp_max) w->hp = w->hp_max; break;
    case 301: w->lv_req += d; break;
    case 302: w->str_req += d; break;
    case 303: w->con_req += d; break;
    case 304: w->dex_req += d; break;
    }
}

// 立即生效的參數（0x10035fd0 屬性分類 1；HP／MP 的 % 版 0x10035c90／0x10035df0 = 292／293 那兩格；屬性分類函式 0x100364a0：< 20 技能自己的欄位、1 立即、2 暫存、3 道具生成，也是 0x10025160／0x100251c0／0x10025220 的施放時版本）：21 HP、22 MP 加值（百分比 = 目前值的 %），
// 292、293 = HP_MAX、MP_MAX 的 % 加到 HP、MP；295 = 清狀態（0x1003f130）
static int skill_instant(int id) { return id == 21 || id == 22 || id == 291 || id == 292 || id == 293 || id == 295; }
static void skill_now(Actor *x, const SkillMod *m, int level) {
    float v = skill_value(m->init, m->incr, level);
    if (m->id == 295) { status_clear(x, (int)v); return; }
    // 291（0x10025120）：五行 FE = 值（施放時；DAT_1039b5dc ≠ 1 的拿掉版寫 0，這裡沒有拿掉的呼叫）並通知客戶端 0x531 → 箭種 +0x52d。
    // shortcut: 人物的 FE 屬性沒寫（劇情人物 FE 是怪物表的五行，主角沒有）；單機技能表、attribT 沒有用到 291，DLC 才會用
    if (m->id == 291) { x->arrow_kind = (int)v; return; }
    int *f = m->id == 21 || m->id == 292 ? &x->hp : m->id == 22 || m->id == 293 ? &x->mp : NULL;
    if (!f) return;
    int max = f == &x->hp ? x->hp_max : x->mp_max;
    *f += (int)(m->id > 100 ? v * max / 100 : m->pct ? v * *f / 100 : v);
    *f = *f < 0 ? 0 : *f > max ? max : *f;
}

// 原版 0x10052a00：n > 1 時 rand() % n，否則 0
static int rnd(int n) { return n > 1 ? rand() % n : 0; }

// 召喚（Attack方式 3，0x100355d0）：參數 273 選召喚兽（1 狼、2 老虎、3 熊、4 鸟、5 白狼、6 木偶、7 狗、8 虎蛟、9 狍鸮、10 朱厌；
// 原版表上第 11 個善翼超出檢查範圍，不會召出來），278 = 最多幾隻；選狼時 rand()%100 是 3 的倍數就換白狼。等級同召喚者（FUN_10056120），
// 放在攻擊位置（G_PosX/Y，角色出手的結算點 = 出手者所在格，被佔了由 actor_place 找附近空格），跟召喚者同一邊。
// 數量與移除看的是召喚者整隊（FUN_1004af10）istrap 20 的成員，但只有前 6 種（k < 6）會被加進隊伍（FUN_10037850）：
// 同種的（選狼時狼和白狼一起算）已有 278 隻就不召；召前 6 種時先移掉別種的召喚兽（狼、白狼互不移）；後 4 種不是成員，不被數也不被移。
static const char *const SUMMON_N[10] = {"\xd5\xd9\xbb\xbd\xca\xde\xc0\xc7", "\xd5\xd9\xbb\xbd\xca\xde\xc0\xcf\xbb\xa2", "\xd5\xd9\xbb\xbd\xca\xde\xd0\xdc", "\xd5\xd9\xbb\xbd\xca\xde\xc4\xf1", "\xd5\xd9\xbb\xbd\xca\xde\xb0\xd7\xc0\xc7", "\xd5\xd9\xbb\xbd\xca\xde\xc4\xbe\xc5\xbc", "\xd5\xd9\xbb\xbd\xca\xde\xb9\xb7", "\xd5\xd9\xbb\xbd\xca\xde\xbb\xa2\xf2\xd4", "\xd5\xd9\xbb\xbd\xca\xde\xe1\xf3\xfb\x5e", "\xd5\xd9\xbb\xbd\xca\xde\xd6\xec\xd1\xe1"};
static int summon_member(const Actor *a, const Actor *e) {  // 召喚者隊伍裡的召喚兽成員（種類序號 0~5）；回傳序號，不是成員 -1
    if (!alive(e) || e->side != a->side || !e->mon || e->mon->istrap != 20) return -1;
    for (int j = 0; j < 6; j++) if (!strcmp(e->mon->name, SUMMON_N[j])) return j;
    return -1;
}
static void summon(Actor *a, const SkillRow *r) {
    int k = -1, max = 0;
    for (int i = 0; i < r->mod_n; i++) {
        int v = (int)skill_value(r->mod[i].init, r->mod[i].incr, r->level);
        if (r->mod[i].id == 273) k = v - 1;
        else if (r->mod[i].id == 278) max = v;
    }
    if (k < 0 || k > 9 || max > 9) return;
    int n = 0;
    for (int i = 0; i < actor_count; i++) {
        int j = summon_member(a, &actors[i]);
        n += j >= 0 && (j == k || (k == 0 && j == 4));
    }
    if (n >= max) return;
    for (int i = 0; k < 6 && i < actor_count; i++) {
        Actor *e = &actors[i];
        int j = summon_member(a, e);
        if (j >= 0 && j != k && !(k == 0 && j == 4)) { actor_unoccupy(e); e->role = NULL; }
    }
    if (k == 0 && rnd(100) % 3 == 0) k = 4;
    const MonDef *m = mondef_pick(SUMMON_N[k]);
    const RoleDef *d = m ? roledef_find(m->role) : NULL;
    if (!d || actor_count >= MAX_ACTORS) return;
    Actor *s = mon_make(m, d, (int)a->x, (int)a->y, a->level);
    actor_owner_set(s, a);
    s->side = a->side;
    actor_group(s, a->grp);
    s->foe = a->foe;
}

// 技能參數 id 的值（沒有 = -1）
static int skill_param(const SkillRow *r, int id) {
    for (int i = 0; i < r->mod_n; i++)
        if (r->mod[i].id == id) return (int)skill_value(r->mod[i].init, r->mod[i].incr, r->level);
    return -1;
}

// 加入 a 的隊伍：同隊打它的都停手
static void join(Actor *t, Actor *a) {
    t->side = a->side;
    actor_group(t, a->grp);
    actor_owner_set(t, a);
    t->foe = NULL;
    for (int i = 0; i < actor_count; i++)
        if (actors[i].foe == t && actors[i].side == t->side) actors[i].foe = NULL;
}

// 在 a 的位置放一隻怪物表上叫 name 的，加入 a 的隊伍
// 原版放在攻擊位置（G_PosX/Y，角色出手的結算點 = 出手者所在格；被佔了由 actor_place 找附近空格）；
// shortcut: 等級用放的人的（原版 FUN_10059870 不設等級，陷阱／分身的初始等級來源沒查到）
static Actor *make_beside(Actor *a, const char *name) {
    const MonDef *m = mondef_find(name);
    const RoleDef *d = m ? roledef_find(m->role) : NULL;
    if (!d || actor_count >= MAX_ACTORS) return NULL;
    Actor *s = mon_make(m, d, (int)a->x, (int)a->y, a->level);
    join(s, a);
    s->exp = 0;
    return s;
}

// 馴服（Attack方式 2，0x10035370）：目標 Friend < 4（同隊的）→ 訊息 5000；不是 istrap 10 的動物 → 0x1400；
// 隊上 istrap 10 的已有參數 271 隻 → 0x1389（5001）；目標已經屬於某個隊伍（FUN_1004aed0）→ 0x138a；成功 → Friend 1、加入隊伍。
// 訊息只給玩家看（原版單機直接顯示，連線版送給施放的玩家）。Friend ≥ 4 在這裡 = 不同邊（side）
static void hud_msg(int id, ...);
static void tame(Actor *a, Actor *t, const SkillRow *r) {
    int say = a == &actors[0];
    if (!t || !alive(t) || !t->mon) return;
    if (t->side == a->side) { if (say) hud_msg(5000); return; }
    if (t->mon->istrap != 10) { if (say) hud_msg(0x1400); return; }
    int n = 0;
    for (int i = 0; i < actor_count; i++) n += alive(&actors[i]) && actors[i].side == a->side && actors[i].mon && actors[i].mon->istrap == 10;
    if (n >= skill_param(r, 271)) { if (say) hud_msg(0x1389); return; }
    if (t->owner) { if (say) hud_msg(0x138a); return; }
    join(t, a);
}

// 陷阱（Attack方式 8，0x10034cb0）：參數 286 選 1 猎杀陷阱、2 引蜂术、3 捆仙之索、4 陷地毒签、5 无情箭阱；自己的陷阱超過 7 個不放。
// 攻擊力、法術攻擊照抄放的人，不給經驗
static int trap_cell(int *x, int *y, int n);  // near.c
// 0x450f70（0x474740 是連線轉接，單機同一個函數）：物件是陷阱（種類 0x15～0x1d）才設主人（0x49c340：記主人句柄、組別；owner 由 join 設）
static void trap_owner_set(Actor *s, const Actor *owner) {
    if (!s || !owner || !((s->mon && s->mon->istrap == 1) || (unsigned)(s->object_kind - 0x15) < 9)) return;
    s->trap_group = owner->grp;
}
static void trap(Actor *a, const SkillRow *r) {
    static const char *N[6] = {"", "\xc1\xd4\xc9\xb1\xcf\xdd\xda\xe5", "\xd2\xfd\xb7\xe4\xca\xf5", "\xc0\xa6\xcf\xc9\xd6\xae\xcb\xf7", "\xcf\xdd\xb5\xd8\xb6\xbe\xc7\xa9", "\xce\xde\xc7\xe9\xbc\xfd\xda\xe5"};
    int k = skill_param(r, 286), n = 0;
    for (int i = 0; i < actor_count; i++) n += alive(&actors[i]) && actors[i].owner == a && actors[i].mon && actors[i].mon->istrap == 1;
    if (k < 1 || k > 5 || n > 7) return;
    Actor *s = make_beside(a, N[k]);
    if (!s) return;
    int x = (int)a->x, y = (int)a->y;
    if (!trap_cell(&x, &y, 10)) { s->role = NULL; return; }  // CTrapBase 放進場景（0x49be40）：周圍 10 圈第一個能放的格子
    s->x = x + 0.5f, s->y = y + 0.5f;
    trap_owner_set(s, a);  // 設主人（0x450f70）；攻擊力照抄在下面
    s->copied = 1;
    s->copy[0] = a->ap;
    s->copy[1] = a->ap_max;
    memcpy(&s->copy[2], &a->ex[43], 10 * sizeof(int));
    mon_derive(s);
}

// 除障（Attack方式 9）：對 istrap 1 的陷阱，技能等級 − 陷阱等級/5 ≥ 0 就讓它 HP 歸零
static void disarm(Actor *a, Actor *t, const SkillRow *r) {
    (void)a;
    if (t && alive(t) && t->mon && t->mon->istrap == 1 && r->level - t->level / 5 >= 0) actor_kill(t, NULL);
}

// 分身（Attack方式 11，0x10034140）：施放者整隊（FUN_1004af10）已有 IsCloned 的就不放（訊息 0x1394，分身自己放也一樣被擋）；
// 拿弓（Hand 2、5）放克隆刺客弓，否則克隆刺客刀；放在攻擊位置、Friend 1、加入隊伍（不設 G_AIGroup、不設主人）
// shortcut: 原版把放的人的裝備（G_EquipStr3 → 分身的 G_EquipStr1）給分身，這裡用 clone_equip 照施放者裝備算數值；Hand 只有主角有（hero_hand），隊友施放當刀
// 0x1003df00：放完後把施放者的每個技能（等級 > 0）複製給分身，等級上限 9（0x10037d70 加進技能列）
#define CLONE_MAX 4
static SkillRow clone_row[CLONE_MAX][MAX_ACTOR_SKILLS];
static void clone_skills(Actor *c, const Actor *src) {
    int slot = 0;
    for (; slot < CLONE_MAX - 1; slot++) {  // 空槽 = 沒有活著的分身用它（最後一槽共用）
        int used = 0;
        for (int i = 0; i < actor_count; i++) used |= actors[i].cloned == slot + 1 && actors[i].role && &actors[i] != c;
        if (!used) break;
    }
    SkillRow *rows = clone_row[slot];
    int n = 0;
    c->cloned = slot + 1;
    actor_skills_rebuild(c);
    for (int i = 0; i < src->skill_n && n < MAX_ACTOR_SKILLS; i++) {
        const SkillRow *r = src->skills[i].row;
        if (!r || r->level < 1) continue;
        rows[n] = *r;
        if (rows[n].level > 9) rows[n].level = 9;
        actor_skill_append(c, &rows[n], n + 1);
        n++;
    }
    actor_right_skill(c, -1);
    actor_left_skill(c, -1);
    c->mp = c->mp_max;
}
static void clone(Actor *a) {
    for (int i = 0; i < actor_count; i++)
        if (alive(&actors[i]) && actors[i].side == a->side && actors[i].cloned) { if (a == &actors[0]) hud_msg(0x1394); return; }
    Actor *c = make_beside(a, a == &actors[0] && (hero_hand == 2 || hero_hand == 5) ? "\xbf\xcb\xc2\xa1\xb4\xcc\xbf\xcd\xb9\xad" : "\xbf\xcb\xc2\xa1\xb4\xcc\xbf\xcd\xb5\xb6");
    if (!c) return;
    clone_skills(c, a);
    // 複製施放者的裝備（0x1003df00 第一段）：分身改用人物公式，主角施放時帶主角全身裝備；隊友沒有裝備模型，用隊友屬性與職業
    c->clone_equip = a == &actors[0] ? 1 : 2;
    memcpy(c->team_attr, a == &actors[0] ? hero_attr : a->team_attr, sizeof c->team_attr);
    c->team_prof = a == &actors[0] ? hero_prof : a->team_prof;
    actor_derive(c);
    c->hp = c->hp_max;
    c->mp = c->mp_max;
}

static void skill_open_lock(Actor *a, const SkillRow *r);
static void skill_steal(Actor *a, Actor *t, const SkillRow *r);

// 技能打中（0x1003f590）：我方状态、敌方状态都沒填就不加；我方状态已在自己身上、敌方状态已在對方身上也不加。
// 否則自己加我方状态、對方加敌方状态（0x1002fe20 → 0x10037f40），時間 = 作用時間。
// 再算攻擊（0x10025b70）：立即生效的參數照「是否对敌」給對方或自己；Attack方式 = 0 不造成傷害
// Attack方式：1、4、10 物理，2 馴服，3 召喚，8 陷阱，9 除障，11 分身；5 開鎖、6 偷竊、12 清狀態、13 界面技能沒有技能用到，沒做
// shortcut: 沒有敌方状态但有對敵參數的（點石成俑）也當成狀態加在對方身上
static void skill_hit(Actor *a, Actor *t, const SkillRow *r, Uint32 tick) {
    // 其他免疫 B_QTIMMU（ex[98]）≥ 100：自己免疫就不加我方状态、對方免疫就不加敌方状态——任何一邊擋住，兩個状态都不加（0x1003f590、0x10056d50）
    int immune = (r && r->status > 0 && a->ex[98] >= 100) || (r && r->estatus > 0 && t && t->ex[98] >= 100);
    if (r && !immune && (r->status > 0 || r->estatus > 0) && !(r->status > 0 && has_status(a, r->status)) && !(t && r->estatus > 0 && has_status(t, r->estatus))) {
        if (r->status > 0) buff_add(a, r, r->status, 0, skill_time(r), 0);
        int enemy_mod = 0;
        for (int i = 0; i < r->mod_n; i++) enemy_mod |= r->mod[i].enemy > 0 && r->mod[i].id >= 20 && r->mod[i].id < 100;
        if (t && (t != a || skill_manual(r) == 2) && alive(t) && (r->estatus > 0 || enemy_mod)) buff_add(t, r, r->estatus, 1, skill_time(r), 0);
    }
    for (int i = 0; r && i < r->mod_n; i++) {
        const SkillMod *m = &r->mod[i];
        if (skill_instant(m->id)) skill_now(m->enemy > 0 && t ? t : a, m, r->level);
    }
    int k = r ? r->attack : 1;
    if (k == 2) tame(a, t, r);
    else if (k == 3) summon(a, r);
    else if (k == 8) trap(a, r);
    else if (k == 9) disarm(a, t, r);
    else if (k == 11) clone(a);
    else if (k == 5) skill_open_lock(a, r);
    else if (k == 6) skill_steal(a, t, r);
    else if (t && t != a && (k == 1 || k == 4 || k == 10)) strike(a, t, r, tick);
}


// 加經驗（0x10026d30），夠了就升級
static void hero_gain_exp(Actor *h, int exp) {
    h->exp += exp;
    while (h->level < exp_levels && h->exp >= exp_table[h->level]) {
        hero_stats(h, h->level + 1);
        hero_extra_exp += 10;
        hero_extra_skill += 1;
        printf("升到 %d 級\n", h->level);
    }
}

// 怪物倒下的經驗分配（0x100574e0 後段）：怪物的 Exp × (殺手悟性 + 150) / 150；殺手等級比怪物高 2 級以上，每多 1 級少 10%（最多多 9 級）。
// 單機沒有別的玩家（Friend 3），人數 n = 1，總量 = n × 經驗 × 2 / (n + 1) = 經驗。隊伍（Friend 2 的隊友）依權重分：殺手 1.2，
// 其他隊員 1 − 0.05 × |等級差|（至少 0.05）；殺手以外的帶頭者（沒有）。每人 = 權重 / 總權重 × 總量
static int kill_exp_total(const Actor *t, const Actor *by) {
    int e = t->exp * ((by->team ? by->team_attr[SAVVY] : hero_attr[SAVVY]) + by->ex[31] + 150) / 150;
    if (t->level + 1 < by->level) {
        int d = by->level - t->level - 1;
        e = (10 - (d > 9 ? 9 : d)) * e * 10 / 100;
    }
    return e;
}
#ifdef STORY
static void team_sync(Actor *a);
#endif
#ifdef STORY
static float kill_weight(int lv, int mon_lv) {  // 非殺手的隊員權重
    int d = lv - mon_lv;
    float x = 1.0f - (d < 0 ? -d : d) * 0.05f;
    return x < 0.05f ? 0.05f : x;
}
#endif
static void kill_exp(const Actor *t, Actor *by) {
    int total = kill_exp_total(t, by);
#ifdef STORY
    // 殺手可以是隊友（by->team）：殺手 1.2，其他人（主角也是）依等級差
    int killer = by->team ? by->sh : 0;  // 0 = 主角
    float w_hero = killer ? kill_weight(actors[0].level, t->level) : 1.2f, sum = w_hero, w[16];
    int mem[16], n = 0;
    for (int m = story_team_next(story_player(), 0); m && n < 16; m = story_team_next(story_player(), m)) {
        if (story_get_int(m, "Friend") < 2) continue;
        w[n] = m == killer ? 1.2f : kill_weight(story_get_int(m, "Level"), t->level);  // 原版這裡用的是怪物等級（local_1c24）
        sum += w[n];
        mem[n++] = m;
    }
    for (int i = 0; i < n; i++) {
        story_gain_exp(mem[i], (int)(w[i] / sum * total));
        Actor *m = actor_of(mem[i]);
        if (m && m->team) team_sync(m);  // 升級後數值重算
    }
    hero_gain_exp(&actors[0], (int)(w_hero / sum * total));
#else
    hero_gain_exp(&actors[0], total);
#endif
}

// 倒下（0x100574e0）：播死亡動作，被主角打死的怪物給經驗，夠了就升級
static void actor_death_fear(Actor *a);
static void actor_remove(Actor *a, int mode);
// 0x1004f3a0 玩家死亡（單機的部分）：身上類型 61 的道具放回腳下；扣錢扣經驗與回復點是連線版（ER_GetNetType > 1），單機沒有。
// 之後的畫面是 JM513 死亡界面（見 dead_show）：「游戏结束」＋读取／退出。
static void hero_dies(void);
// 0x100574e0 的死亡方式：呼叫端傳技能的 DieMode（欄 12，預設 1）；身上有狀態 4（冰凍）→ 9，有狀態 2（石化）→ 10（後者蓋過前者）；
// 隊友（IsActorNPC > 0）的 6／8／9（碎裂）改成 1。原版 Status_N 屬性對應引擎 state[N]
static void actor_kill_mode(Actor *t, Actor *by, int mode) {
    if (t->state[4]) mode = 9;
    if (t->state[2]) mode = 10;
    if ((mode == 6 || mode == 8 || mode == 9) && t->team) mode = 1;
    t->hp = 0;
    actor_remove(t, mode);
    if (t->mon) mon_loot(t);
    if (by && !by->mon && t->mon && (by == &actors[0] || by->team)) kill_exp(t, by);
    printf("%s 倒下 死亡方式=%d\n", t->mon ? "怪物" : "主角", t->removal_mode);
    if (t == &actors[0]) hero_dies();
#ifdef STORY
    if (t->sh) story_killed(t->sh);  // 原版 0x100574e0 最後刪掉人物；Boss 的第 10 格會開劇情
#endif
}
static void actor_kill(Actor *t, Actor *by) { actor_kill_mode(t, by, 1); }

static int beast_trampled(const Actor *a, const Actor *t, unsigned tick);
// 小動物被踩死（原版 0x49b160 後半）：圖形序列數 > 0x18（有死亡序列）、每 30 拍的餘數 ≠ 0、NPC 組角色 t 靠到 0x1e（30 像素）內且 t 不是站著（動態狀態 ≠ 1）

// ---- 招式單位（觸發 type 2 → MethodUnit_Mdl.ini）：原版的特效與飛行物件，見 unit.c ----
static int shake_left, shake_total;

// ---- 畫面效果（Poq.exe 0x44ca40 入口；招式觸發 type 14：ID 1~3 淡入淡出、4 震畫面（param = 長度）、其他 = 重設）----
// 淡入淡出物件（0x4a6480 啟動、0x4a66e0 重設、0x4a6720 每拍、0x4a6770 畫，0x44bf00 畫完場景後呼叫）：計數 23 每拍 −1，等級 = 計數 / 3（7 → 0）；
// 模式 1 = 黑幕等級 / 8 的透明度，由黑漸亮；模式 2 = 7 − 等級，漸暗，到 7 整個畫面清成黑，計數用完模式不重設（黑著等下一個效果）；
// 模式 3 = 啟動時把整個畫面複製一份，用等級 / 8 的透明度疊在新畫面上（場景切換的交叉淡出）
// shortcut: 0x44ca40 模式 1、2 先呼叫的那個小函數沒翻（還沒查它設什麼）；原版只在放招的人在畫面內才觸發，這裡由 unit.c 用距離判斷
static int fade_mode, fade_cnt, fade_hold;  // fade_hold = 0x44ca40 存的 +0x25c：黑著等幾拍再淡入（模式 2 的參數，劇情畫面效果 1 用 0x50）
static SDL_Texture *fade_tex, *fade_canvas;  // fade_canvas = 主畫布（模式 3 啟動時複製它）
static void fade_reset(void) { fade_mode = fade_cnt = 0; }  // 0x4a66e0
static void fade_start(int mode) {  // 0x4a6480（單機光影固定模式，這裡只管淡入淡出）
    fade_reset();
    if (mode < 1 || mode > 3) return;
    fade_mode = mode;
    fade_cnt = 0x17;
    if (mode == 3 && fade_canvas) {
        int w, h;
        SDL_QueryTexture(fade_canvas, NULL, NULL, &w, &h);
        if (!fade_tex) fade_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
        SDL_Texture *old = SDL_GetRenderTarget(ren);
        SDL_SetRenderTarget(ren, fade_tex);
        SDL_RenderCopy(ren, fade_canvas, NULL, NULL);
        SDL_SetRenderTarget(ren, old);
        SDL_SetTextureBlendMode(fade_tex, SDL_BLENDMODE_BLEND);
    }
}
static void screen_effect(int mode, int param) {  // 0x44ca40
    if (mode == 4) { if (shake_left < 1) shake_left = shake_total = param; return; }  // 震畫面（0x461180）
    fade_hold = param;
    fade_start(mode);
}
// 每個畫面：先畫（0x4a6770）再減計數（0x4a6720：計數用完且模式不是 2 就重設）
static void fade_draw(int w, int h) {
    if (!fade_mode) return;
    int lv = fade_cnt / 3;
    if (fade_mode == 3 && fade_tex) {
        SDL_SetTextureAlphaMod(fade_tex, (Uint8)(lv * 255 / 8));
        SDL_RenderCopy(ren, fade_tex, NULL, NULL);
    } else if (fade_mode != 3) {
        int a = fade_mode == 2 ? 7 - lv : lv;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, a >= 7 && fade_mode == 2 ? 255 : a * 255 / 8);
        SDL_Rect r = {0, 0, w, h};
        SDL_RenderFillRect(ren, &r);
    }
    if (fade_cnt > 0) fade_cnt--;
    else if (fade_mode != 2) fade_reset();
    // 黑著的等待（Poq.exe 0x43cdxx 那段：+0x25c > 0 倒數，到 1 且模式 2 就 0x44ca40(1, 0) 淡入）。shortcut: 倒數放在畫面迴圈，原版放哪一拍沒查
    if (fade_hold > 0 && --fade_hold == 0 && fade_mode == 2) fade_start(1);
}
// ---- 畫面縮放（CGameApp +0x234 現在值、+0x238 目標、+0x23c 記住的值）：0 = 原尺寸，最大 0.24（中間 52% 放大到整個畫面）----
// 原版 PageUp／滾輪往上 +0.04、PageDown／滾輪往下 −0.04；招式觸發 12（來源是角色）ID 非 0 = +0.24、0 = 回記住的值。
// 照原版保留：記住的是「現在值」而不是目標；用浮點數比相等，目標不是 0.04 的倍數（劇情指令）時會在兩個值之間來回跳。
static float zoom, zoom_to, zoom_keep;
static void zoom_set(float f) { zoom_to = f; }        // 0x4493f0（Rec 控制包的縮放分支尚未接入）
static void zoom_back(void) { zoom_to = zoom_keep; }  // 0x44ae10
static float zoom_clamp(float f) { return f < 0 ? 0 : f > 0.24f ? 0.24f : f; }
static void zoom_now(float d) { zoom = zoom_clamp(zoom + d); }  // 0x44aed0
static void zoom_add(float d) { zoom_keep = zoom; zoom_to = zoom_clamp(zoom_to + d); }  // 0x44af30
static void zoom_tick(void) { if (zoom != zoom_to) zoom_now(zoom_to <= zoom ? -0.04f : 0.04f); }  // 0x44bf40 → 0x44bf99：每拍往目標走 0.04
// 畫面上的點 → 縮放前的點（0x44c970）
static void zoom_point(int *x, int *y) {
    if (zoom == 0) return;
    *x = (int)(vw * zoom) - (int)((zoom + zoom - 1.0f) * *x);
    *y = (int)(vh * zoom) - (int)((zoom + zoom - 1.0f) * *y);
}
static Uint32 actor_logic_tick;
// 移掉某個狀態（打中名單放開時拿掉 110）
static void buff_drop(Actor *a, int st) {
    int n = a->buff_n;
    for (int b = 0; b < a->buff_n; b++)
        if (a->buff[b].status == st) { actor_status_notify(a, st, 0, -1); a->buff[b--] = a->buff[--a->buff_n]; }
    if (n != a->buff_n) { actor_derive(a); actor_effects_recompute(a); }
}
static int hostile(const Actor *a, const Actor *e);
struct MUnit;
static int owner_trig(struct MUnit *u, const Trig *t, Uint32 tick);
// 地上的道具
#define MAX_GROUND 128
// x, y = 落點格（邏輯用）；dx, dy = 畫的位置（像素，64/格，0x49a550 拋出動畫每拍累加）；t = 已飛拍數（+0x7d）、z = 高度（+0x81）
static struct { Item it; float x, y; int id; float dx, dy, vx, vy; int t, z; } ground[MAX_GROUND];  // id：命令0x11用的物件編號（原版CPropObj句柄）
static int ground_n, ground_next_id = 1;
// 離開場景時各場景留下的地上道具（連同存檔：原版 CERWorld::Save 0x1000a8b0 存世界全部物件，丟在房間的道具也在內）
static char ground_alias[64];
static struct { char alias[64]; int n; __typeof__(ground[0]) g[MAX_GROUND]; } gstore[8];
typedef struct { int sh, hp, mp, dir; float x, y; } MonSave;  // 存檔：怪物／NPC 的位置與血量（原版 CERWorld::Save 存全部世界物件）
static MonSave pending_mons[256];
static int pending_mons_n;
static int ground_pending;  // 讀檔讀到的地上道具，進場景後套用
static char pending_alias[64];
static __typeof__(gstore) pending_gstore;
static int pending_n;
static __typeof__(ground) pending_ground;
enum { PROP_TICKS = 30, PROP_STEP = 7, PROP_SPLIT = 0 };  // DAT_0062a2e8、DAT_0062a2ec、DAT_00663f24
static int walk_cx = -1, walk_cy;       // 點地面要走去的格子（-1 = 沒有）
#include "light.c"
#define scene (*unit_scene)
#include "unit.c"
static int beast_trampled(const Actor *a, const Actor *t, unsigned tick) {
    if (!a->def || !a->def->beast || !a->def->anim || !grp_npc(t->grp) || t->dynamic_state == 1 || tick % 30 == 0) return 0;
    const Anim *an = anim_get(a->def->anim);
    long dx = ax64(a) - ax64(t), dy = ay64(a) - ay64(t);
    return an && an->count > 0x18 && dx * dx + dy * dy <= 30L * 30;
}

static int actor_has_method(const Actor *a) { return main_of(a) != NULL; }
#undef scene
#include "weather.c"

// Poq.exe 0x48f0a0：第3修正索引機率表；程序標識50完全不消耗亂數。
// 靜止／移動共用的表在原版映像是BSS零值；受擊表為3、2、1、1。
// shortcut: 尚未接原版對BSS機率表的執行期設定，接到其設定入口時更新此表。
static int actor_blood_period[4];
static MUnit *actor_blood(Actor *a, int hit) {
    static const int hit_period[4] = {3, 2, 1, 1};
    int level = a->modifier[3].value;
    // shortcut: 非法修正索引不重現原版越界讀取，出現合法擴充值時再擴表。
    if (!a->def || a->def->program == 50 || level < 0 || level >= 4) return NULL;
    int period = (hit ? hit_period : actor_blood_period)[level];
    if (!period || rand() % period) return NULL;
    MUnit source = {0}; // 原版建立入口傳空來源，不繼承攻擊者／目標／技能。
    MUnit *u = unit_new(0x17, ax64(a), ay64(a), &source, actor_logic_tick);
    // shortcut: 容量耗盡／資料缺失時略過，不重現原版空指標寫入。
    if (u) u->layer = a->layer;
    return u;
}

// 角色更新的出手冷卻分支；有MethodMain時保留倒數，零與負數不再遞減。
// 活動門檻由角色維護入口檢查；動態、播放及高度先由actor_motion_tick推進。
static void actor_cooldown_tick(Actor *a) {
    if (a->cool > 0 && !main_of(a)) --a->cool;
}

// Poq.exe 0x4c7d60：格子層<10才轉地面1，層10照原值保留。
static unsigned char actor_ground_layer(const Cell *c) {
    return c->layer < 10 ? 1 : c->layer;
}

// Poq.exe 0x494950：退出只標活動並收尾舊狀態，不把21/22直接改成靜止。
static void actor_dynamic_exit(Actor *a) {
    a->dynamic_active = 1;
    switch (a->dynamic_state) {
    case 11:
    case 31:
        a->reverse_playback = 0;
        break;
    case 22:
        a->vz = -5;
        a->zmax = 0;
        // fall through
    case 21: {
        actor_set_flags(a, 0x200, 0);
        const Cell *c = unit_scene && unit_scene->cells ? cell_at((int)a->x, (int)a->y) : NULL;
        if (c && a->layer > 10) a->layer = actor_ground_layer(c);
        break;
    }
    }
}

// Poq.exe 0x4961e0：高度速度與上限照參數寫入；本函數不重設目前高度。
static void actor_height_set(Actor *a, int speed, int limit) {
    a->vz = speed;
    a->zmax = limit;
}

// Poq.exe 0x4967d0：單機共用服務端的插值初始化；-1取占格／A+C/10。
// 步數截斷，但增量除以未截斷的float步數，末拍不補齊目的地。
// 回放模式10的方向追趕修正不適用單機；不複製同步封包。
// shortcut: 舊初始化角色尚未登記占格時，以目前整數格承接；建立流程統一登記後移除fallback。
static int actor_interpolate(Actor *a, int cx, int cy, int speed) {
    if (cx == -1) {
        cx = a->grid_valid ? a->grid_x : (int)a->x;
        cy = a->grid_valid ? a->grid_y : (int)a->y;
    }
    float amount = speed == -1 ? a->move_a + a->move_c / 10.0f : (float)speed;
    int gate = speed == -1 ? a->move_a : speed;
    int x = ax64(a), y = ay64(a), tx = cx * 64 + 32, ty = cy * 64 + 32;
    int distance = dist_of(x, y, tx, ty);
    if (distance < 1) { a->interpolation_left = 0; return 1; }
    // shortcut: 原版 0x4967d0 在 A（速度表每拍整數步）= 0 時不插值（速度 2 的小雞 A=0、只有 C=7 的 0.7 像素／拍，結果永遠站在原地），
    // 但這些小雞的資料是閒逛（邏輯 1、矩形 4×4）、單機看起來是 9 隻釘在地上，使用者回報不合理，所以 A=0 但 C>0 時照 C 的小數步走
    int slow = speed == -1 && a->move_c > 0;
    if ((gate < 1 && !slow) || (!a->move_remaining && !slow)) return 1;
    float steps = (float)distance / amount;
    a->interpolation_left = (int)steps;
    if (a->interpolation_left < 1) return 1;
    a->interpolation_x = (float)x;
    a->interpolation_y = (float)y;
    a->interpolation_dx = (float)(tx - x) / steps;
    a->interpolation_dy = (float)(ty - y) / steps;
    return 0;
}

// Poq.exe 0x4952d0：浮點累加保留小數，世界位置每拍向零截斷，不改格子占位。
static int actor_interpolation_tick(Actor *a) {
    if (a->interpolation_left < 1) return 1;
    a->interpolation_x += a->interpolation_dx;
    a->interpolation_y += a->interpolation_dy;
    a->x = (int)a->interpolation_x / 64.0f;
    a->y = (int)a->interpolation_y / 64.0f;
    --a->interpolation_left;
    return 0;
}

// Poq.exe 0x497cd0：由地面層升至對應的可移動高層，不改其他層。
static void actor_raise_layer(Actor *a) {
    if (a->layer < 11) a->layer = 11;
    else if (a->layer == 12) a->layer = 13;
    else if (a->layer == 14 || a->layer == 16) a->layer = 15;
}

// Poq.exe 0x494070：目的格相同不做任何事；先轉跳躍層再找落點。
// 占格先到目的地，世界座標留給逐拍插值；即使占格失敗也照原版初始化插值。
static int actor_rush_destination(Actor *a) {
    int x = a->dynamic_command[0], y = a->dynamic_command[1];
    if (a->grid_x == x && a->grid_y == y) return 0;
    const Cell *c = cell_at(x, y);
    int mode = 1;
    if (a->dynamic_state == 22) {
        actor_raise_layer(a);
        if (a->layer == 11 && c && c->layer != 12) a->layer = 1;
        mode = 4;
    }
    if (a->layer < 11 || !c || !cell_pass(c, a->layer) || cell_occupant(x, y))
        line_end(&x, &y, a->grid_x, a->grid_y, x, y,
                 a->dynamic_command[2], 1, mode, a->layer);
    if (x == a->grid_x && y == a->grid_y) return 0;
    if (a->dynamic_state == 22) actor_move_speed(a, actor_speed(a) * 4);
    else if (a->dynamic_state == 21) actor_move_speed(a, actor_speed(a) * 2);
    actor_place(a, x, y, 0);
    actor_interpolate(a, -1, -1, -1);
    return 1;
}

// Poq.exe 0x496460：跳躍半程改下降，或以剩餘水平拍數重新起跳。
static void actor_jump_phase(Actor *a, int rising) {
    if (!rising) {
        const Cell *c = cell_at(a->grid_x, a->grid_y);
        // shortcut: 原版無效占格會解參照空指標；舊建立資料未登記時保留層號，登記流程統一後移除。
        if (a->layer > 10 && c) a->layer = actor_ground_layer(c);
        a->jump_half = 0; a->vz = -4; a->zmax = 0;
    } else {
        a->z = 0;
        a->jump_half = a->interpolation_left / 2;
        a->vz = 4; a->zmax = a->jump_half * 4;
    }
}

// 動態進入的21/22分支（0x4942d0 case 0x15/0x16）。
static void actor_rush_enter(Actor *a) {
    if (a->move_a < 2 || !actor_rush_destination(a)) {
        a->dynamic_path_result = 0;
        return;
    }
    actor_set_flags(a, 0x200, 1);
    if (a->dynamic_state == 22) actor_jump_phase(a, 1);
}

// Poq.exe 0x494f90：NPC組10000..60000快取15拍，其他50拍；取有號拍差絕對值。
static int actor_dynamic_retry(const Actor *a) {
    int32_t delta = (int32_t)(actor_logic_tick - (Uint32)a->dynamic_enter_tick);
    uint32_t elapsed = delta < 0 ? 0u - (uint32_t)delta : (uint32_t)delta;
    int limit = ((uint32_t)a->grp - 10000u) <= 50000u ? 15 : 50;
    return (int32_t)elapsed < limit ? a->dynamic_path_result : 1;
}

// Poq.exe 0x493ad0：從目的格往目前占格掃第一個空格；相鄰不掃，
// 抵達目前占格仍無空格時，反而以原目的地為中心找六圈。
// shortcut: 反編譯遺失Y軸累加變數，以與X軸對稱的整數DDA承接；取得組語後核對相等門檻。
static int actor_walk_fallback(const Actor *a, int *x, int *y, int end_x, int end_y) {
    int dx = abs(end_x - *x), dy = abs(end_y - *y);
    int n = dx > dy ? dx : dy;
    if (n < 2) return 0;
    int px = *x, py = *y, ex = 0, ey = 0;
    for (;;) {
        if (px == end_x && py == end_y) return vacant_cell(x, y, a->layer, 6);
        const Cell *c = cell_at(px, py);
        if (c && cell_pass(c, a->layer) && !cell_occupant(px, py)) {
            *x = px; *y = py;
            return 1;
        }
        ex += dx;
        if (ex >= n) { px += end_x < *x ? -1 : 1; ex -= n; }
        ey += dy;
        if (ey >= n) { py += end_y < *y ? -1 : 1; ey -= n; }
    }
}

// Poq.exe 0x4d0610（插入 0x4d0700）：堆上浮（1 起算位置 pos；f = g + h）。
static void sift_up(int *heap, int *hp, const int *g, const int *hh, int pos) {
    while (pos > 1) {
        int up = pos >> 1, a = heap[up - 1], b = heap[pos - 1];
        if (g[a] + hh[a] <= g[b] + hh[b]) return;
        heap[up - 1] = b; heap[pos - 1] = a; hp[b] = up; hp[a] = pos; pos = up;
    }
}
// XPS_Control（0x4d0b10 展開／0x4d0890 估價／0x4d0af0 終點判斷）用的格子狀態（0x4c7eb0）：
// 1 空、2 同組角色（可穿，加權）、3 擋住。目的格原版暫蓋自身ID，所以 occ 在目的格 = 自己。
typedef struct { const Actor *a; int dx, dy; } XpsCtl;
static const Actor *xps_occ(const XpsCtl *c, int x, int y) {
    return x == c->dx && y == c->dy ? c->a : cell_occupant(x, y);
}
static int xps_state(const XpsCtl *c, int x, int y) {
    const Cell *cell = cell_at(x, y);
    if (!cell || !cell_pass(cell, c->a->layer)) return 3;
    const Actor *o = xps_occ(c, x, y);
    if (!o) return 1;
    // shortcut: 自己一律算同組（object_kind 許多角色還沒設，建立入口補齊後可移除 o == a）。
    return o == c->a || (o->object_kind >= 0x1e && o->object_kind <= 0x64 && o->grp == c->a->grp) ? 2 : 3;
}
// Poq.exe 0x4d0b10：dir 0..7（上起順時針，奇數＝斜）；回傳 0 = 不能走，*cost = 5（+3 同組占格／+3 斜走）。
static int xps_expand(const XpsCtl *c, int x, int y, int dir, int *nx, int *ny, int *cost) {
    static const int DX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    *nx = x + DX[dir]; *ny = y + DY[dir];
    int st = xps_state(c, *nx, *ny);
    if (st == 3) return 0;
    *cost = 5;
    if (st == 2) {
        // 兩個不同的同組占格之間不能直接走
        if (cell_at(x, y) && xps_occ(c, x, y) != xps_occ(c, *nx, *ny) && xps_state(c, x, y) == 2) return 0;
        *cost += 3;
    }
    if (dir & 1) {
        if (xps_state(c, *nx, y) == 3 && xps_state(c, x, *ny) == 3) return 0;
        *cost += 3;
    }
    return 1;
}
// Poq.exe 0x4d0890（終點判斷 0x4d0af0 = 座標相等）：估價（八方向，直 10 斜 4 的加權；比 g 的 5 大，所以偏貪婪，照原版）。
static int xps_heur(const XpsCtl *c, int x, int y) {
    int ax = abs(c->dx - x), ay = abs(c->dy - y);
    return ay < ax ? ax * 10 + ay * 4 : ax * 4 + ay * 10;
}

// Poq.exe 0x4973b0（CCharactor虛擬格20）：從目前占格到動態目的地(+0xc3,+0xc7)生成反向byte路徑。
// XPathSearch（Poq.exe 0x4cff00，A*：開表＝二元堆，f = g(+0xc) + h(+0x10)；0x4d0610 上浮）＋XPS_Control（上面三個）＋Poq.exe 0x4d09e0 取路徑。
// 開表空了沒到終點 → 取 h 最小（先到先贏）的節點當終點（走到最近處）。
// shortcut: 最大展開數參數（0x4cff00 param_4）傳 0＝不限；節點表用整張地圖陣列取代雜湊桶＋分塊鏈。
static int actor_path_build(Actor *a) {
    if (!unit_scene) return 0;
    int w = unit_scene->w, h = unit_scene->h, dx = a->dynamic_destination_x, dy = a->dynamic_destination_y;
    int sx = actor_init_x(a), sy = actor_init_y(a);
    a->dynamic_pending = 0;
    if (!cell_at(sx, sy) || !cell_at(dx, dy)) return 0;
    XpsCtl ctl = { a, dx, dy };
    int n = w * h;
    int *par = malloc(sizeof(int) * n * 5);  // par, g, h, 在開表的堆位置(1起,0=不在), 已建節點(1)
    int *heap = malloc(sizeof(int) * n), *order = malloc(sizeof(int) * n);
    if (!par || !heap || !order) { free(par); free(heap); free(order); return 0; }
    int *g = par + n, *hh = par + 2 * n, *hp = par + 3 * n, *made = par + 4 * n;
    memset(par, 0, sizeof(int) * n * 5);
    int hn = 0, on = 0, found = -1;
#define F(i) (g[i] + hh[i])
#define NODE(i, p, gg, hv) (par[i] = (p), g[i] = (gg), hh[i] = (hv), made[i] = 1, order[on++] = (i))
#define PUSH(i) do { heap[hn++] = (i); hp[i] = hn; sift_up(heap, hp, g, hh, hn); } while (0)
    int s = sy * w + sx;
    if (sx == dx && sy == dy) found = s, NODE(s, -1, 0, 0);
    else { NODE(s, -1, 0, xps_heur(&ctl, sx, sy)); PUSH(s); }
    while (found < 0 && hn) {
        int cur = heap[0];
        heap[0] = heap[--hn]; hp[cur] = 0;
        if (hn) hp[heap[0]] = 1;
        for (int i = 1; 2 * i <= hn;) {  // 0x4cff00 的下沉（右子較小才換右）
            int c = 2 * i;
            if (c < hn && F(heap[c]) < F(heap[c - 1])) c++;
            if (F(heap[i - 1]) <= F(heap[c - 1])) break;
            int t = heap[i - 1]; heap[i - 1] = heap[c - 1]; heap[c - 1] = t;
            hp[heap[i - 1]] = i; hp[heap[c - 1]] = c; i = c;
        }
        for (int dir = 0; dir < 8 && found < 0; dir++) {
            int nx, ny, cost;
            if (!xps_expand(&ctl, cur % w, cur / w, dir, &nx, &ny, &cost)) continue;
            int ni = ny * w + nx, g2 = g[cur] + cost;
            if (nx == dx && ny == dy) { NODE(ni, cur, g2, 0); found = ni; break; }
            if (made[ni]) {
                if (g[ni] > g2 && hp[ni]) {  // 還在開表才更新
                    par[ni] = cur; g[ni] = g2;
                    sift_up(heap, hp, g, hh, hp[ni]);
                }
            } else { NODE(ni, cur, g2, xps_heur(&ctl, nx, ny)); PUSH(ni); }
        }
    }
    if (found < 0) {
        for (int k = 0; k < on; k++) if (found < 0 || hh[order[k]] < hh[found]) found = order[k];
    }
#undef F
#undef NODE
#undef PUSH
    // 0x4d09e0：終點格空（沒被別人占）才算一筆；然後沿父節點到起點，最多250筆。
    int cnt = 0;
    const Cell *fc = cell_at(found % w, found / w);
    if (fc && cell_pass(fc, a->layer) && !cell_occupant(found % w, found / w) && found != -1) {
        a->path_buf[0] = (unsigned char)(found % w); a->path_buf[1] = (unsigned char)(found / w);
        cnt = 1;
    }
    for (int p = par[found]; p != -1 && cnt < 250; p = par[p], cnt++) {
        a->path_buf[cnt * 2] = (unsigned char)(p % w);
        a->path_buf[cnt * 2 + 1] = (unsigned char)(p / w);
    }
    free(par); free(heap); free(order);
    a->dynamic_pending = cnt;
    a->dynamic_path = cnt - 2;
    return a->dynamic_path >= 0 &&
        (a->dynamic_path != 0 || dx == a->path_buf[0] || dy == a->path_buf[1]);
}

// Poq.exe 0x496f20：動態11目的地校正＋反向路徑生成（成功設新路徑旗標）。
static int actor_walk_destination(Actor *a, int x, int y) {
    int original_x = x, original_y = y;
    a->dynamic_destination_x = x;
    a->dynamic_destination_y = y;
    const Cell *original = cell_at(x, y);
    int sx = a->grid_valid ? a->grid_x : (int)a->x;
    int sy = a->grid_valid ? a->grid_y : (int)a->y;
    if (!original || !cell_pass(original, a->layer) || cell_occupant(x, y)) {
        if (!actor_walk_fallback(a, &x, &y, sx, sy)) {
            a->dynamic_path = 0;
            return 0;
        }
        a->dynamic_destination_x = x;
        a->dynamic_destination_y = y;
        // NULL目標與空占位也相等，保留原版的指標相等判斷。
        if (original && cell_occupant(original_x, original_y) == a->foe) {
            int dx = abs(x - original_x), dy = abs(y - original_y);
            int corrected = dx > dy ? dx : dy;
            dx = abs(sx - original_x); dy = abs(sy - original_y);
            int current = dx > dy ? dx : dy;
            if (a->dynamic_command[2] < corrected && current <= corrected) {
                a->dynamic_path = 0;
                return 0;
            }
        }
    }
    if (actor_path_build(a)) { a->dynamic_new_path = 1; return 1; }
    a->dynamic_path = 0;
    return 0;
}

// Poq.exe 0x4942d0：動態基底進入。共用拍數與速度在各分支之前；
// 11失敗寫結果0、21/22落點／跳躍、31反向／速度／反播、32三倍速／插值／面向。
static void actor_dynamic_base_enter(Actor *a) {
    a->dynamic_enter_tick = (int)actor_logic_tick;
    if (a->dynamic_state != 1) actor_speed_update(a);
    switch (a->dynamic_state) {
    case 11:
        if (!actor_dynamic_retry(a)) { a->dynamic_path_result = 0; break; }
        // shortcut: 未綁場景的舊fixture保留原目的地；建立入口統一登記後移除。
        if (unit_scene && unit_scene->cells) {
            if (!actor_walk_destination(a, a->dynamic_command[0], a->dynamic_command[1])) {
                a->dynamic_path_result = 0;
                break;
            }
        } else {
            a->dynamic_destination_x = a->dynamic_command[0];
            a->dynamic_destination_y = a->dynamic_command[1];
        }
        a->path_walk = unit_scene && unit_scene->cells;
        break;
    case 21: case 22:
        actor_rush_enter(a);
        break;
    case 31: {
        int dir = a->dynamic_command[0];
        actor_direction_set(a, dir >= 0 && dir < 8 ? (dir + 4) % 8 : dir);
        if (a->dynamic_command[2]) actor_move_speed(a, a->dynamic_command[2]);
        a->reverse_playback = 1;
        break;
    }
    case 32:
        if (a->move_a < 2) { a->dynamic_path_result = 0; break; }
        actor_move_speed(a, actor_speed(a) * 3);
        actor_interpolate(a, a->dynamic_command[0], a->dynamic_command[1], -1);
        a->facing = dir_of(a->grid_x, a->grid_y, a->dynamic_command[0], a->dynamic_command[1], 8);
        actor_animation_select(a, a->animation_target / 8 * 8 + a->facing);
        break;
    }
}

// Poq.exe 0x486270：角色層先啟用移動殘影，再無條件交給動態基底。
// 基底動態11的路徑限制記在actor_dynamic_base_enter，不省略角色入口任何分支。
static void actor_dynamic_enter(Actor *a) {
    actor_trail_enter(a);
    actor_dynamic_base_enter(a);
}

// Poq.exe 0x496550：單機也走原版服務端分支；比較狀態、三參數和旗標快照。
// 靜止不寫三參數；21/22只在p0非零時強制打斷。失敗後保留進入時刻與結果。
static void actor_dynamic_command(Actor *a, int state, int p0, int p1, int p2) {
    int flag = actor_flags(a, 0x10000);
    if (scene_record_mode == 2) {  // 回放（DAT_00663804 == 0 的客戶端分支）：只換狀態；不存參數、不尋路、不選序列（序列由錄影的 #3 0x66 給）
        if (a->dynamic_state != state || a->dynamic_command[0] != p0 || a->dynamic_command[1] != p1 ||
            a->dynamic_command[2] != p2 || a->dynamic_flag_snapshot != flag) {
            actor_trail_exit(a);
            a->dynamic_state = state;
            if (state == 22) { actor_set_flags(a, 0x200, 1); actor_jump_phase(a, 1); }  // 跳躍：離地旗標、半程與垂直速度（shortcut: 層號改 1 的檢查要用命令座標，客戶端沒存，略）
        }
        a->dynamic_flag_snapshot = flag;
        return;
    }
    if (a->dynamic_state != state || a->dynamic_command[0] != p0 ||
        a->dynamic_command[1] != p1 || a->dynamic_command[2] != p2 ||
        a->dynamic_flag_snapshot != flag) {
        actor_trail_exit(a);
        int old = a->dynamic_state;
        if (state == 1) {
            a->print_x = a->print_y = -100;
            if (p0 || (old != 21 && old != 22)) {
                a->dynamic_state = 1;
                a->path_walk = 0;
                a->moving = 0;
            }
        } else {
            a->dynamic_path_result = 1;
            a->path_walk = 0;
            a->dynamic_state = state;
            a->dynamic_command[0] = p0;
            a->dynamic_command[1] = p1;
            a->dynamic_command[2] = p2;
            actor_dynamic_enter(a);
            if (!actor_dynamic_retry(a)) {
                actor_trail_exit(a);
                a->dynamic_state = 1;
                a->path_walk = 0;
                a->moving = 0;
            }
        }
        if ((unsigned)(a->dynamic_state - 20) > 10u && old != 21 && old != 22)
            actor_animation_select(a, -1);
    }
    a->dynamic_flag_snapshot = flag;
}

// 命令7/10由共用角色命令入口檢查可用性。
static int actor_rush_command(Actor *a, int state, int x, int y, int distance) {
    return actor_command(a, state == 22 ? 7 : 10, x, y, distance, 0, 0);
}

// Poq.exe 0x494a20：最後一拍只插值；下一拍才退出。平常命令(1,1,0,0)強制離開21/22。
static void actor_rush_tick(Actor *a) {
    if (a->interpolation_left > 0) { actor_interpolation_tick(a); return; }
    actor_dynamic_command(a, 1, 1, 0, 0);
}

// 動態每拍的21/22分支；中點下降先於當拍水平插值，與高度更新分開。
static void actor_rush_update(Actor *a) {
    if (a->dynamic_state != 21 && a->dynamic_state != 22) return;
    if (a->dynamic_state == 22 && a->interpolation_left == a->jump_half) actor_jump_phase(a, 0);
    actor_rush_tick(a);
}

// Poq.exe 0x4b7d60：扣除四捨五入的比例，EAX仍回傳扣掉的整數。
// 上升呼叫端同時消費回傳值與被扣過的上限；負半數往正方向取整。
static int actor_height_fraction(int *value, int steps, int rising) {
    float part = (float)*value;
    if (steps > 0) part = rising ? (part + part) / steps : part / steps * 0.5f;
    float rounded = floorf(part);
    if (part - rounded >= 0.5f) rounded = ceilf(part);
    int delta = (int)rounded;
    *value -= delta;
    return delta;
}

// Poq.exe 0x495010：基底活動getter固定回1；共用拍數、落塵、跳躍及靜止落地。
static void actor_height_tick(Actor *a) {
    ++a->direction_clock;
    if (!a->z) {
        if (a->move_a > 8 && a->dynamic_state != 11 &&
            (abs(a->dust_x - ax64(a)) >= 48 || abs(a->dust_y - ay64(a)) >= 48)) {
            MUnit source = {0};
            unit_new(0x10, ax64(a), ay64(a), &source, actor_logic_tick);
            a->dust_x = ax64(a); a->dust_y = ay64(a);
        }
    } else if (a->z > 0 && !a->vz && a->dynamic_state != 22) a->z = 0;
    if (a->vz) {
        if (a->dynamic_state != 22) a->z += a->vz;
        if (a->vz >= 0) {
            if (a->dynamic_state == 22)
                a->z += actor_height_fraction(&a->zmax, a->interpolation_left - a->jump_half, 1);
            else if (a->z > a->zmax) a->z = a->zmax;
            return;
        }
        if (a->dynamic_state == 1) {
            int cx = a->grid_valid ? a->grid_x : (int)a->x;
            int cy = a->grid_valid ? a->grid_y : (int)a->y;
            if (a->z < 1) {
                a->x = cx + 0.5f; a->y = cy + 0.5f;
            } else {
                int distance = dist_of(ax64(a), ay64(a), cx * 64 + 32, cy * 64 + 32);
                int steps = abs(a->z / a->vz);
                if (steps < 1) steps = 1;
                actor_interpolate(a, cx, cy, distance / steps);
                actor_interpolation_tick(a);
            }
        }
        if (a->dynamic_state == 22) actor_height_fraction(&a->z, a->interpolation_left, 0);
        if (a->z > 0) return;
        a->vz = 0;
    }
    if (a->z < 0) a->z = 0;
}

// Poq.exe 0x48ebd0：每槽先刪舊播放器；-1只清除，其他值（包括0）照載入。
static void actor_effect_sprite(Actor *a, int slot, int graphic) {
    free(a->effect[slot]); a->effect[slot] = NULL;
    if (graphic == -1) { a->modifiers_dirty = 1; return; }
    Spr *p = calloc(1, sizeof *p);
    if (p) { spr_load(p, (uint32_t)graphic); a->effect[slot] = p; }
}

// Poq.exe 0x48e120：只以65535判斷未指定，指定0也必須觸發對應副作用。
static void actor_effect(Actor *a, int status, int on, int skill) {
    const StatusDef *d = status_def(status & 255);
    if (!d || d->raw[1] == 65535) return;
    const int *v = d->raw;
    if (v[2] != 65535) actor_set_flags(a, 0x200, on);
    if (v[3] != 65535) {
        a->effect_speed = on ? v[3] : 100;
        a->effect_applied = 1;
        actor_speed_update(a);
    }
    if (v[4] != 65535) actor_set_flags(a, 0x800, on);
    if (v[5] != 65535) actor_set_flags(a, 0x4000, on);
    for (int k = 0; k < 3; k++) if (v[k + 6] != 65535) {
        if (on) actor_effect_sprite(a, k, v[k + 6]);
        else if (a->effect[k]) { free(a->effect[k]); a->effect[k] = NULL; a->modifiers_dirty = 1; }
        else a->modifiers_dirty = 1;
    }
    if (v[9] != 65535) {
        int range = a->aura_range;
        if (skill != -1) {
            range = 8; a->aura_skill = 0;
            for (int i = 0; i < a->skill_n; i++) if (a->skills[i].key == skill) {
                range = a->skills[i].max; a->aura_skill = i + 1; break;
            }
        }
        if (on) { a->aura_status = v[9]; a->aura_range = range; }
        else a->aura_status = a->aura_range = a->aura_skill = 0;
    }
    if (v[10] != 65535) actor_set_flags(a, 0x400, on);
    actor_trail_effect(a, d, on);
    for (int k = 13; k <= 17; k++) if (v[k] != 65535)
        actor_modifier_set(a, k == 17 ? 4 : k - 3, on ? v[k] : 0, -1);
    if (on && v[19] != 65535) audio_sfx(v[19]);
}

// Poq.exe 0x48d950：共用byte通知及完整角色分派，供修正17／100與技能Buff使用。
// shortcut: 單機共用Actor，狀態廣播與令牌配對容器由本地state／flags承接；
// 隊伍狀態圖示通知等隊伍介面接入，不能在這裡重造該介面的八槽圖示表。
static void actor_dynamic_rest(Actor *a);
static void actor_candidate_cast(Actor *a);
static int actor_can_use_skill(const Actor *a, const ActorSkill *skill);
static void actor_status_notify(Actor *a, int status, int on, int skill) {
    if ((unsigned)status >= 128u || a->state[status] == on) return;
    unsigned char previous = a->state[status];
    a->state[status] = (unsigned char)on;
    int server = unit_scene && scene_record_mode != 2;
    switch (status) {
    case 0:
        if (!server) break;
        // 原版服務端的戰鬥byte是唯一狀態來源；fight僅保留舊AI的退戰計時。
        a->fight = on ? FIGHT_TICKS : 0;
        if (!grp_npc(a->grp)) {
            actor_cancel_command(a);
            actor_dynamic_rest(a);
        }
        actor_speed_update(a);
        actor_animation_select(a, -1);
        if (!on) {
            a->battle_return = 1;
            actor_clear_target(a);
        } else actor_candidate_cast(a);
        break;
    case 8:
        actor_set_flags(a, 0x20000, on != 0);
        if (!on) actor_light_init(a);
        break;
    case 13:
        a->counter_skill = on ? 0 : -1;
        break;
    case 14:
        if (!server) break;
        if (on) {
            static int group = 99; // 原版共用分配器，99..1循環；不得依角色另起一份。
            a->original_group = a->grp;
            actor_group(a, group);
            if (--group < 1) group = 99;
        } else {
            actor_group(a, a->original_group);
            // +0x579用途尚未確定，不能猜成casting而中斷當前施法。
            a->status_aux = 0;
        }
        break;
    case 15: {
        if (!server) break;
        // 原版狀態15在設與清時都通知；640世界單位、模式0的敵對／援敵篩選。
        actor_near_mode = 0;
        near_begin(ax64(a), ay64(a), 640);
        Actor *target;
        while ((target = actor_near_next(a))) {
            __typeof__(nr) cursor = nr;
            int i = actor_skill_find(a, -1); // 原版檢查的是施加者，而不是被掃到的角色。
            if (actor_can_use_skill(a, i >= 0 ? &a->skills[i] : NULL)) {
                actor_target_set(target, a);
                actor_command(target, 11, 1, 0, 0, 0, 0);
            }
            nr = cursor;
            actor_near_mode = 0;
        }
        break;
    }
    case 100:
        // 服務場景由狀態表控制；只有客戶端／回放額外切第5修正。
        if (!server) actor_modifier_set(a, 5, on != 0, -1);
        break;
    case 101:
        if (!on) break;
        {
            MUnit *u = main_of(a);
            if (u) {
                if (a->cool < 1) a->cool = rand() % 10 + 10;
                unit_die(u, actor_logic_tick);
            }
            // 狀態主分派的挨打分支：沒有挨打序列也先中止招式、擲音效骰。
            if (a->def && a->def->snd_hit != 65535 && rand() % 3 == 1)
                audio_sfx(a->def->snd_hit);
            if (!a->def || a->def->hit == 65535) {
                a->state[status] = previous;
                return;
            }
        }
        break;
    case 105:
        if (on) a->status_countdown = 9;
        break;
    case 110:
        if (on) {
            MUnit *u = main_of(a);
            if (u) unit_die(u, actor_logic_tick);
            actor_cancel_command(a);
            actor_height_set(a, 2, 100);
        } else {
            actor_height_set(a, -10, 0);
            actor_modifier_set(a, 5, 1, 30);
        }
        break;
    case 111:
    case 114:
        if (on) a->status_countdown = 8;
        else if (status == 114) {
            actor_modifier_set(a, 10, 0, -1);
            if (a->effect[1]) {
                free(a->effect[1]); a->effect[1] = NULL;
            }
        }
        break;
    case 115:
        if (on) a->status_countdown = 5;
        break;
    case 121:
        actor_set_flags(a, 0x80000, on != 0);
        break;
    }
    a->modifiers_dirty = 1;
    actor_effect(a, status, on, skill);
    const StatusDef *d = status_def(status);
    if (d && d->raw[1] != 65535) a->last_status = status;
}

// 角色移除中的同組潰散分支；與移除主入口共用原版範圍及near游標。
static void actor_death_fear(Actor *a) {
    for (Actor *t = actor_near_begin(a, 1, -1); t; t = actor_near_next(a)) {
        int fear = t->def->death_fear;
        // 原版只在1／2擲隨機：0（以及負值）必定潰散，>=3完全不擲。
        if (fear < 1 || (fear < 3 && rand() % (fear + 1) == 0)) {
            __typeof__(nr) cursor = nr;
            actor_status_notify(t, 17, 1, -1);
            nr = cursor;
            actor_near_mode = 1;
        }
    }
}

// CPartDeadObj 設類型（Poq.exe 0x4998a0，0x498b70 空函數）：1／4 載入血跡動畫 0x1000b1／0x10011b 掛在部件 "HD" 這幀矩形的頂邊中央，
// 2 載入 0x100106 掛在部件 "CA"。偏移 = 矩形 x + w/2、y（+0x7b／+0x7f）；沒有該部件就不加。0x44f6d0 只用 1／2／4。
// shortcut: 類型 3（取當前幀偏移）沒有呼叫者，沒做；網頁圖像尚未載入時取不到矩形，血跡就不加。
static void part_blood_set(Actor *a, int type) {
    static const struct { int id; const char *part; } T[5] = {{0, 0}, {0x1000b1, "HD"}, {0x100106, "CA"}, {0, 0}, {0x10011b, "HD"}};
    const Role *r = a->role;
    int p = T[type].part && r ? role_part_find(r, T[type].part) : -1;
    if (p < 0) return;
    SDL_Rect rc = {0};
    frame_rect_union(&rc, r->body[p], a->def->death + DIR_MAP[a->dir], 0, 0, 0);
    if (!rc.w) return;
    a->blood_id = T[type].id; a->blood_x = rc.x + rc.w / 2; a->blood_y = rc.y; a->blood_start = actor_logic_tick;
}

// 6／7／8 的分件（Poq.exe 0x44f6d0 的 6／7／8 分支；部件群樣式 0x578ac0／0x5780b0 = part_hide 遮罩；CFlyPartDeadObj 建構 0x498ed0）：模式6 部件 "HD"、7 九個 HD TK LE RE LA LH RA RH SD、8 只量 "HD"。
// 屍體先停 25（8 = 30）拍；部件群要 > 1 個部件，有被點名的部件才拿掉（身體不畫它）、掛血跡 1／2／4；
// 6／7 再建一個 CFlyPartDeadObj（0x498ed0：速度 rand%5+10，同一份角色，只畫被點名的部件，往身後飛）。
// shortcut: 模式6 的名單 DAT_006628cc 在原版映像沒有初始化來源，當 "HD"；部件群 > 1 的判斷（+0xc8）當 Role 部件數 > 1；
// 飛散件沿用倒下角色的位置（原版放格子中央）和朝向（+0xa7 當 facing）。
static void part_dead_split(Actor *a, int mode) {
    static const char *const names[9] = {"HD", "TK", "LE", "RE", "LA", "LH", "RA", "RH", "SD"};
    const Role *r = a->role;
    a->part_wait = mode == 8 ? 30 : 25;
    if (!r || r->part_count < 2) return;
    unsigned hit = 0;
    for (int i = 0; i < (mode == 7 ? 9 : 1); i++) {
        int p = role_part_find(r, names[i]);
        if (p >= 0) hit |= 1u << p;
    }
    if (!hit) return;
    part_blood_set(a, mode == 6 ? 1 : mode == 7 ? 2 : 4);
    if (mode == 8) return;
    a->part_hide = hit;
    if (actor_count >= MAX_ACTORS) return;
    Actor *f = &actors[actor_count++];
    *f = *a;
    memset(f->effect, 0, sizeof f->effect);
    memset(f->illusion, 0, sizeof f->illusion);
    f->part_kind = 2;
    f->part_hide = ~hit;
    f->part_wait = f->blood_id = 0;
    f->fly_speed = rand() % 5 + 10;
    start_act(f, ACT_DEAD, a->def->death);
}

// 單機殘留物承接倒下物件工廠 0x44f6d0（原版回傳新物件；這裡倒下的 Actor 自己就是屍體物件；0x499120 初始化、0x499200 建立）：
// 3／5／9／10 單位1×腿2 放招式單位，4 放腳印，其他走 CPartDeadObj（建構 0x498f70、初始化 0x499120：
// 序列 = 模板死亡序列 +0xa4 + 方向，由 actor_remove 的 start_act 設）；6／7／8 進 part_dead_split。
// shortcut: 原版 0 和 2、>10 的模式不建物件，這裡仍建一般屍體（舊行為，等選項介面核對）。
static void actor_death_remnant(Actor *a, int mode) {
    if (!unit_scene || scene_client.scene != unit_scene) return;
    int unit = mode == 3 ? 10 : mode == 5 ? 0x73 : mode == 9 ? 0xeb : mode == 10 ? 0xec : 0;
    MUnit source = {0};
    if (unit && a->def->size == 1 && a->def->legs == 2) {
        unit_new(unit, ax64(a), ay64(a), &source, actor_logic_tick);
        return;
    }
    if (mode == 4) {
        if (unit_n < MAX_UNIT) {
            MUnit *u = &units[unit_n++];
            *u = (MUnit){.cls = U_FOOT, .x = ax64(a), .y = ay64(a), .mode = 1};
            spr_load(&u->spr, a->def->legs == 2 ? 0x100581 : 0x10000b);
        }
        return;
    }
    if (mode != 6 && mode != 7 && mode != 8) actor_blood(a, 1);
    a->corpse_visible = 1;
    a->part_kind = 1;
    a->part_hide = 0;
    a->blood_id = 0;
    if (mode >= 6 && mode <= 8) part_dead_split(a, mode);
}

// Poq.exe 0x486910（屍體物件建立 CDeadObj 0x4987d0）：移除→單機選項→模式正規化→殘留物→同組潰散→模式音效。
// shortcut: 共用Actor代替伺服／客戶端副本，略過傳輸和令牌配對容器；
// 單機無回放入口。完整選項介面接齊時接death_auto_pause／death_keep_mode。
static void actor_remove(Actor *a, int mode) {
    actor_destroy(a);
    input_sel_remove(a);  // 0x458fc0：死的人離開選人名單
    int has_scene = scene_client.scene && unit_scene == scene_client.scene;
    if (scene_client.scene && !death_keep_mode && mode != 0 && mode != 1) mode = 1;
    if (mode) {
        if (death_auto_pause && scene_client.scene) {
            // 0x458700：本機隊伍有人以它為目標；shortcut: 連原本的「死的是操控角色」也算（原版 vf+0x44 回什麼沒確認）
            Actor *selected = scene_client_actor(&scene_client);
            if (selected == a || input_party_targets(a)) { character_death_message(); scene_pause_count(1); }
        }
        actor_health_percent(a, 0);
    }
    if (a->def->force_death_mode == 1) mode = 1;
    if (a->def->program == 50 && mode != 0) {
        mode = 0;
        MUnit source = {0};
        MUnit *u = unit_new(0x65, (uint16_t)ax64(a), (uint16_t)ay64(a), &source, actor_logic_tick);
        if (u) u->layer = a->layer;
    }
    a->removal_mode = (unsigned char)mode;
    a->corpse_visible = 0;
    a->dead_phase = a->dead_ticks = 0;
    if (mode && has_scene) actor_death_remnant(a, mode);
    start_act(a, ACT_DEAD, a->def->death);
    scene_objects_dirty = 1;
    actor_death_fear(a);
    int sound = -1;
    switch (mode) {
    case 0: case 3: case 100: return;
    case 4: sound = 0x4a; break;
    case 5: sound = 0x4d; break;
    case 6: sound = 0x48; break;
    case 7: sound = 0x47; break;
    case 8: audio_sfx(0x49); return;
    case 9: sound = 0x4c; break;
    case 10: sound = 0x4b; break;
    }
    if (sound >= 0) audio_sfx(sound);
    if (a->def->snd_death != 65535) audio_sfx(a->def->snd_death);
}

// Poq.exe 0x488090：共用倒數、嚴格優先序與陷阱／隱身偵測。
static void actor_status_countdown(Actor *a) {
    --a->status_countdown;
    int status = a->state[105] ? 105 : a->state[12] ? 12 :
                 a->state[111] ? 111 : a->state[115] ? 115 :
                 a->state[114] ? 114 : -1;
    if (status == 12) {
        Actor *t = near_trap((int)a->x, (int)a->y, 5, a->grp);
        if (t) {
            if (trap_auto_pause) {
                trap_detected_message();
                scene_pause_count(1);
            }
            trap_reveal(t);
        }
        for (t = near_first(ax64(a), ay64(a), 0x140); t; t = near_next()) {
            if (a->grp != t->grp && actor_flags(t, 0x20000)) {
                actor_status_notify(a, 8, 0, -1);
                return;
            }
        }
        return; // 不因共用倒數已負數而清12，也不落入較低優先狀態。
    }
    if (status == 114 && a->status_countdown == 5) {
        // 原版直接覆寫，不經修正setter，亦不重設倒數。
        a->modifier[10].value = 2;
        a->modifier[10].time = -1;
        actor_effect_sprite(a, 1, 0x100014);
    }
    if (status >= 0 && status != 12 && a->status_countdown < 0)
        actor_status_notify(a, status, 0, -1);
}

// 角色更新的方向漸轉分支；完整角色更新入口仍有活動門檻、光環等未接。
static void actor_direction_tick(Actor *a) {
    int target = actor_requested_seq(a);
    Uint32 clock = a->direction_clock;
    if (a->flags & 0x400) {
        // 模板+0x4c是休息序列第一值；每拍遞減動畫方向，不改物件面向或目標序列。
        // 此分支優先於空中靜止判定，也不受普通漸轉的4／15拍節流。
        if (--a->animation_dir < 0) a->animation_dir = 7;
        a->shown_seq = (a->def ? a->def->idle : 0) + a->animation_dir;
        a->shown_ready = 1;
        actor_body_select(a);
        return;
    }
    if (!a->shown_ready) {
        a->shown_seq = a->previous_target = target;
        a->shown_ready = 1;
        return;
    }
    if ((a->flags & 0x200) && a->dynamic_state != 1) return;
    const Seq *old = actor_sequence(a, a->shown_seq);
    const Seq *next = actor_sequence(a, target);
    if (target != a->previous_target) {
        if (!old || !next || old->count != next->count || a->shown_seq / 8 != target / 8) {
            a->shown_seq = target / 8 * 8 + a->shown_seq % 8;
            actor_body_select(a);
        }
        a->previous_target = target;
    }
    if (a->shown_seq == target || clock % (a->body_size < 6 ? 4 : 15)) return;
    int from = a->shown_seq % 8, to = target % 8;
    if (a->shown_seq / 8 != target / 8) {
        a->shown_seq = target;
        actor_body_select(a);
    }
    // 原版正好相差四方向時沿較長分支的方向轉，不可改成一般最短路徑tie-break。
    if (from != to) {
        if (abs(from - to) < 4) from += to < from ? -1 : 1;
        else from += from < to ? -1 : 1;
        if (from < 0) from = 7;
        if (from > 7) from = 0;
    }
    int frame = a->act_frame;
    a->shown_seq = target / 8 * 8 + from;
    actor_body_select(a);
    next = actor_sequence(a, a->shown_seq);
    // 原版只保存幀索引；換方向重新選序列後的幀內拍數為零。
    a->act_frame = next && frame < next->count ? frame : 0;
}

// Poq.exe 0x487f30：空中不推本體，但仍清挨打、播放三個附加精靈。
static void actor_animation_tick(Actor *a) {
    if (a->head_height_pending) actor_head_height(a);
    actor_body_select(a);
    const Seq *q = role_seq(a->role, a->body_seq);
    if (!(a->flags & 0x200) && q && q->count) {
        if (a->knock == 1) {
            for (int i = 0; i < 3; i++) actor_body_step(a, q, 1);
        } else {
            // 死亡停末幀；屍體物件階段0 +0x9f>0 時不播（0x4988f0，部件屍體起始停 25／30 拍）。
            if (!(a->act == ACT_DEAD && a->dead_phase == 0 && a->part_wait > 0)) actor_body_step(a, q, a->act == ACT_DEAD);
        }
        ++a->act_tick;
    }
    a->act_done = actor_body_done(a, q);
    if (((a->flags & 0x200) || a->act_done) && has_status(a, 101)) {
        actor_status_notify(a, 101, 0, -1);
        actor_animation_select(a, -1);
    }
    for (int k = 0; k < 3; k++) if (a->effect[k]) {
        spr_next(a->effect[k]);
        if (spr_count(a->effect[k]) > 1 && spr_done(a->effect[k]) && a->effect[k]->seq)
            spr_set(a->effect[k], 0, 0);
    }
}

// 光環0x139的五參數業務接收：狀態、非零啟用值8、技能、接收角色。
// 對照DLL命令分派的光環分支；接收者亦為參數目標，兩種enemy參數都套同一人。
// shortcut: 技能物件以ActorSkill.row承接；完整劇情技能實例接齊後改用其即時參數。
static void actor_aura_receive(Actor *target, const ActorSkill *skill, int status) {
    // 同一封包狀態0走FightStatus分支，8原值必須保留，不建立技能Buff。
    if (status == 0) { actor_status_notify(target, 0, 8, -1); return; }
    const SkillRow *r = skill->row;
    if (!r || status < 0 || status_blocked(target, status)) return;
    if ((status == 78 || status == 83) &&
        (!target->mon || (target->mon->istrap != 10 && target->mon->istrap != 20))) return;
    // 原版狀態阻擋檢查會拒絕同狀態，光環不刷新既有期限。
    if (!buff_add_source(target, r, status, 2, skill_time(r), 0, skill->key)) return;
    for (int i = 0; i < r->mod_n; i++) {
        const SkillMod *m = &r->mod[i];
        if (skill_instant(m->id))
            skill_now(target, m, r->level);
    }
}

// Poq.exe 0x488dd0：活動→動態／播放／高度由actor_motion_tick前置執行；
// 本段接續冷卻、轉向、十拍修正、比例、重算，最後一百拍光環。
static void actor_maintenance_tick(Actor *a) {
    if (a->activity_disabled) return;
    actor_cooldown_tick(a);
    if (!(a->flags & 0x400) && (a->flags & 0x200) && a->dynamic_state != 1) return;
    // 原版維護階段先轉向再扣修正；播放本體時不能再轉一次。
    actor_direction_tick(a);
    actor_modifiers_tick(a);
    actor_scale_tick(a);
    if (a->modifiers_dirty) actor_effects_recompute(a);
    if (a->aura_range && a->direction_clock % 100 == 0 &&
        a->aura_skill > 0 && a->aura_skill <= a->skill_n) {
        ActorSkill source = a->skills[a->aura_skill - 1];
        if (!source.row) return; // 對應來源技能CIDTable有效性檢查。
        for (Actor *t = actor_near_begin(a, 1, a->aura_range); t; t = actor_near_next(a)) {
            // 原版檢查施加者自己的狀態，而非接收者；不得改成一般Buff去重。
            if (t != a && !has_status(a, a->aura_status)) {
                __typeof__(nr) cursor = nr;
                actor_aura_receive(t, &source, a->aura_status);
                nr = cursor;
                actor_near_mode = 1;
            }
        }
    }
}

// Poq.exe 0x496380：直接切本體序列，與目標序列分開；同序列不重設幀。
static void actor_body_sequence(Actor *a, int seq) {
    if (a->body_ready && a->body_seq == seq) return;
    a->shown_seq = seq;
    a->shown_ready = 1;
    a->body_ready = 0;
    a->act_tick = 0;
    actor_body_select(a);
}

// Poq.exe 0x494ec0：-1使用既有目標，其他值先保存為目標再切本體。
static void actor_body_apply(Actor *a, int seq) {
    if (seq == -1) seq = a->animation_target;
    else a->animation_target = seq;
    actor_body_sequence(a, seq);
}

// 換裝／一般靜止共用原版動態命令，不強制離開21/22。
static void actor_dynamic_rest(Actor *a) { actor_dynamic_command(a, 1, 0, 0, 0); }

// Poq.exe 0x495910：重設動態物件並解除場景；不改世界位置、命令三參數或高度。
static void actor_dynamic_reset(Actor *a) {
    a->dynamic_pending = 0;
    a->direction_clock = a->ai_tick = rand() % 100;
    a->dynamic_path = 0;
    a->dynamic_state = 1;
    a->previous_target = -1;
    a->interpolation_left = -1;
    a->dynamic_enter_tick = -1;
    a->dynamic_counter = 0;
    a->dynamic_aux = 0;
    a->dynamic_cache_x = a->dynamic_cache_y = -1;
    a->dynamic_reported_state = 1;
    // CObj解除場景／復原初值；初始化記錄由Actor陣列直接持有，沒有重複建立旗標副本。
    a->bound_scene = NULL;
    a->object_dead = 0;
    a->layer = 1;
}

// Poq.exe 0x48afc0：先清業務關聯，再退戰／重設光、高度及動態基底。
static void actor_behavior_reset(Actor *a) {
    a->interaction = NULL;
    actor_clear_target(a);
    // 原版只清MethodMain指標，不呼叫結束觸發，也不刪掉該招式物件。
    MUnit *u = main_of(a);
    if (u) u->main_detached = 1;
    a->animation_target = 0;
    actor_cancel_command(a);
    actor_forget_command(a);
    a->command_world_x = a->command_world_y = -100;
    a->status_aux = a->extra = a->cool = a->behavior_unused = a->debug_rectangle = 0;
    a->corrected_x = a->corrected_y = 0;
    actor_status_notify(a, 0, 0, -1);
    actor_light_init(a);
    actor_dynamic_rest(a);
    actor_height_set(a, -20, 0);
    actor_dynamic_reset(a);
    // 單機令牌配對沿用Actor身分；此處不複製伺服端名單節點。
}

// Poq.exe 0x492e70（CNpc虛擬重設）：NPC敵對入戰提示呼叫0x579dd0為空函數；清攻擊間隔與閒置拍，再走基底重設。
static void npc_behavior_reset(Actor *a) {
    a->npc_wait = a->npc_idle = 0;
    actor_behavior_reset(a);
}

// Poq.exe 0x4933a0（CProtagonist建構）：CNpc建構後物件種類改60（不消費重判層旗標、單機攻擊間隔只用餘拍）。
// +0x71e/+0x722/+0x726（點、-1）和+0x2c7=5000在整個映像只有建構與重設寫、沒人讀，不留欄位。
static void protagonist_new(Actor *a) {
    a->object_kind = 60;
}

// Poq.exe 0x4963e0：面向與目標序列的八方向分組一起更新。
// Actor.dir使用畫面八方向，facing保留原版方向值（含負值）。
static void actor_direction_set(Actor *a, int dir) {
    a->facing = dir;
    for (int i = 0; i < 8; i++) if (DIR_MAP[i] == dir) a->dir = i;
    int group = a->animation_target / 8; // 原版帶符號除法，向零截斷。
    actor_animation_select(a, group * 8 + dir);
}

// Poq.exe 0x486520：載紙娃娃，初始化方向／序列，長於兩幀才隨機初幀。
// 單機直接載完整Role；不需要原版模式2的伺服端省略圖資／矩形分支。
// 外觀字串「動作方式*款式*款式…」（G_EquipStr、原版封包的外觀）蓋在角色表列上。回 0 = 款式太長
static int look_override(RoleDef *look, const char *override) {
    // 原版星號分詞略過空token：首值是動作方式（PLI），餘值重新以*串接。
    const char *p = override + strspn(override, "*");
    size_t n = strcspn(p, "*");
    look->pli = atoi(p);
    look->styles[0] = 0;
    p += n;
    size_t used = 0;
    while (*(p += strspn(p, "*"))) {
        n = strcspn(p, "*");
        // shortcut: 沿用RoleDef的96-byte款式上限，遇到更長外觀資料時改動態字串。
        if (used + n + (used != 0) >= sizeof look->styles) return 0;
        if (used) look->styles[used++] = '*';
        memcpy(look->styles + used, p, n); used += n;
        look->styles[used] = 0;
        p += n;
    }
    return 1;
}
static void actor_appearance_init(Actor *a, const char *override) {
    const RoleDef *d = a->def;
    const Role *r = NULL;
    if (override && *override && strcasecmp(override, "NULL")) {
        RoleDef look = *d;
        if (look_override(&look, override)) r = role_variant_for(&look);
    }
    if (!r) r = role_for(d); // 覆寫載入失敗仍退回模板紙娃娃。
    a->role = r;
    a->body_ready = 0; a->shown_ready = 1; a->shown_seq = 0;
    a->act_tick = 0;
    // 新播放器的序列／幀皆0；矩形在挑方向之前求，不能改用最終休息序列。
    actor_head_height(a);
    actor_direction_set(a, rand() % 8);
    actor_dynamic_rest(a);
    if (a->spec && a->spec->logic == 0) {
        unsigned state = a->spec->initial_state[0];
        actor_direction_set(a, a->spec->dir);
        // shortcut: 原版越界索引會讀模板其他欄位；拒絕損毀的初始態，遇到合法新索引再擴表。
        int seq = state < 13 ? (uint16_t)d->initial_seq[state] : 65535;
        actor_animation_select(a, seq + a->spec->dir);
    } else actor_animation_select(a, -1);
    actor_speed_update(a);
    actor_body_apply(a, -1);
    const Seq *q = role_seq(r, a->body_seq);
    if (q && q->count > 2) a->act_frame = rand() % q->count;
}

// Poq.exe 0x48d810：先記本體序列／幀，回平常、換紙娃娃、重選目標，再嘗試復原。
// 底層輸入由既有styles_equip解析成RoleDef；單機共用Actor，不發外觀同步封包。
// SDL繪製／點選每次依現有Role算矩形，不需另保存過期的矩形快取。
static void actor_look_set(Actor *a, const RoleDef *look) {
    int old_seq = a->body_ready ? a->body_seq : actor_seq(a);
    int old_frame = a->act_frame;
    actor_dynamic_rest(a);
    const Role *r = role_variant_for(look);
    if (r && r != a->role) {
        a->role = r;
        // 新紙娃娃的播放器從0開始，即使前後的序列編號相同也不能沿用舊播放器。
        a->shown_seq = 0; a->shown_ready = 1;
        a->body_ready = 0; a->act_tick = 0;
        actor_body_select(a);
    }
    actor_animation_select(a, -1);
    if (a->role && old_seq < a->role->seq_count && old_seq >= 0) {
        actor_body_sequence(a, old_seq);
        const Seq *q = role_seq(a->role, old_seq);
        if (q && old_frame < q->count) a->act_frame = old_frame;
    }
    // 保留原版怪現象：最後仍套目標；若目標不同，剛復原的舊序列／幀會再被重設。
    actor_body_apply(a, -1);
}

static void actor_enter_fight(Actor *a);
// Poq.exe 0x48eda0：先寫目標識別，再登記被指向NPC組的敵對關係。
// 入戰經命令11檢查預設選招可用性；原版先互設目標才要求對方入戰。
static void actor_target_set(Actor *a, Actor *t) {
    if (!t) { actor_clear_target(a); return; }
    a->foe = t;
    if (!grp_npc(t->grp)) return;
    foe_grp(a, t);
    if (grp_npc(a->grp)) return;
    int dx = (int)a->x - (int)t->x, dy = (int)a->y - (int)t->y;
    if ((int)sqrt((double)dx * dx + (double)dy * dy) <= actor_seek(t) && !t->fight) {
        // 原版先轉入戰鬥，再互設目標；順序可避免重入時再次觸發候選招式。
        actor_command(t, 11, 1, 0, 0, 0, 0);
        actor_target_set(t, a);
    }
}

// Poq.exe 0x48f560：非NPC組且未選目標，敵對的可見角色進入嚴格方框才反應。
static void actor_notice_near(Actor *a, Actor *t) {
    if ((t->flags & 0x20000) || grp_npc(a->grp) || !is_foe(a, t) || actor_target(a)) return;
    int range = actor_seek(a);
    if (abs((int)a->x - (int)t->x) >= range || abs((int)a->y - (int)t->y) >= range) return;
    actor_target_set(a, t);
    if (!a->fight) actor_enter_fight(a);
}

// 接回角色每拍的靠近通知，沿用既有場景格子查詢，保留原版方框而非圓形。
static void actor_near_reactions(Actor *a) {
    if (!alive(a) || grp_npc(a->grp) || actor_target(a) || actor_seek(a) < 1) return;
    near_begin(ax64(a), ay64(a), actor_seek(a) * 64);
    Actor *t;
    while ((t = near_next())) {
        __typeof__(nr) cursor = nr;
        actor_notice_near(a, t);
        nr = cursor;
        if (actor_target(a)) break;
    }
}

// Poq.exe 0x4883e0：敵對優先；不同組的中立角色不會因選取而改用同組選取圖。
// 原版全域Spr的資源ID由啟動程式的this指標與載入參數逐一核對。
static uint32_t actor_relation_anim(const Actor *a) {
    const Actor *controlled = scene_client_actor(&scene_client);
    if (!controlled) return 0;
    int small = a->def->size < 2;
    if (is_foe(a, controlled)) return small ? 0x100013 : 0x100012;
    if (a->grp != controlled->grp) return small ? 0x100017 : 0x100016;
    if (a == hover_actor) return small ? 0x100019 : 0x100018;
    return small ? 0x1002ef : 0x1002f0;
}

// shortcut: 單機選人名單目前只有操控角色；隊伍多選介面接入後，保留名單內所有角色的腳圈。
// 其餘角色以游標命中切換0x100，直接供角色繪製與可見矩形消費。
static void actor_hover_set(Actor *a, int mouse) {
    Actor *controlled = scene_client_actor(&scene_client);
    for (int i = 0; i < actor_count; i++) {
        if (&actors[i] == a || &actors[i] == controlled) actor_set_flags(&actors[i], 0x100, 1);
        else actor_set_flags(&actors[i], 0x100, 0);
    }
    hover_actor = a;
    hover_mouse = mouse;
}

// Poq.exe 0x488d20：速度只比較是否為0；有MethodMain時不能移動。
static int actor_can_move(const Actor *a) { return a->speed != 0 && !main_of(a); }

// 動態11的同組讓路請求消費端；只查可通行層及角色占位，不排除死亡旗標。
// shortcut: 完整路徑掃描尚未接入，暫由跨格碰撞送出請求；移植逐格掃描後移至其首個路徑點。
static void actor_yield_request(const Actor *a, int x, int y) {
    const Cell *c = cell_at(x, y);
    if (!c || !cell_pass(c, a->layer)) return;
    Actor *other = cell_occupant(x, y);
    if (other && other->sh != a->sh && other->grp == a->grp &&
        (unsigned)(other->object_kind - 30) <= 70u)
        other->dynamic_aux = a->sh;
}

// Poq.exe 0x494cb0：以保存的位置量距；偏移只改世界座標，不移占格、不回吸保存位置。
static void actor_yield_tick(Actor *a) {
    if (!a->dynamic_aux) return;
    Actor *other = character_of(a->dynamic_aux);
    if (other) {
        int x = a->dynamic_cache_x == -1 ? ax64(a) : a->dynamic_cache_x;
        int y = a->dynamic_cache_x == -1 ? ay64(a) : a->dynamic_cache_y;
        int distance = dist_of(x, y, ax64(other), ay64(other));
        if (distance < 64) {
            if (distance < 30) {
                if (!actor_can_move(a) || a->dynamic_cache_x != -1) return;
                a->dynamic_cache_x = ax64(a);
                a->dynamic_cache_y = ay64(a);
                int dir = other->facing + 2;
                if (dir < 0) dir = other->facing + 10;
                else if (dir > 7) dir = other->facing - 6;
                x = ax64(a); y = ay64(a);
                move_dir(&x, &y, dir, 20);
                a->x = x / 64.0f; a->y = y / 64.0f;
                return;
            }
            if (a->dynamic_cache_x != -1) {
                if (a->sh) a->dynamic_aux = 0;
                a->dynamic_cache_x = a->dynamic_cache_y = -1;
            }
            return;
        }
    }
    if (a->sh) a->dynamic_aux = 0;
    if (a->dynamic_cache_x != -1) {
        if (a->sh) a->dynamic_aux = 0;
        a->dynamic_cache_x = a->dynamic_cache_y = -1;
    }
}

// Poq.exe 0x488ca0：目前招式有flag4觸發時，即使禁止攻擊也允許中斷。
// shortcut: 單機同時執行原版服務端的技能Enable分支；拆出網路客戶端時按場景角色分流。
static int actor_can_use_skill(const Actor *a, const ActorSkill *skill) {
    MUnit *u = main_of(a);
    if (!actor_flags(a, 0x800) && !u) {
        if (!a->skill_n) return 1;
        if (skill) return actor_skill_enabled(a, skill);
    } else if (u && u->m && u->m->interruptible) return 1;
    return 0;
}

// Poq.exe 0x488c70：包含精確命中停用項及模板fallback，不只查當前手。
static int actor_can_use_id(const Actor *a, int id) {
    int i = actor_skill_find(a, id);
    return actor_can_use_skill(a, i >= 0 ? &a->skills[i] : NULL);
}

static void actor_enter_fight(Actor *a);

// 命令5沿共用角色門檻接受；動態31的面向與位移方向相反。
static int actor_knock_command(Actor *a, int direction, int cells, int speed) {
    return actor_command(a, 5, direction, cells, speed, 0, 0);
}

static void actor_knock_finish(Actor *a) {
    actor_dynamic_rest(a);
    if (a->dynamic_counter) {
        a->dynamic_counter = 0;
        // shortcut: 原版送回動態11的快取目的地，到達範圍參數沒查到，先用0。
        actor_dynamic_command(a, 11, a->dynamic_destination_x, a->dynamic_destination_y, 0);
        if (a == actors) { walk_cx = a->dynamic_destination_x; walk_cy = a->dynamic_destination_y; }
    }
}

// Poq.exe 0x493cb0：動態31逐格更新；原版先移占格再插值，插值結束的下一拍才標記可走下一格。
// 命令第二參數逐格減1；遇阻或走完回動態1，counter返回動態11目的地（到達範圍0）。
static void actor_knock_tick(Actor *a) {
    if (a->dynamic_state != 31) return;
    if (a->dynamic_active) {
        if (a->dynamic_command[1] < 1) { actor_knock_finish(a); return; }
        int x = a->grid_x, y = a->grid_y;
        move_dir(&x, &y, a->dynamic_command[0], 1);
        if (!actor_place(a, x, y, 0)) { actor_knock_finish(a); return; }
        actor_interpolate(a, -1, -1, -1);
        a->dynamic_active = 0;
        --a->dynamic_command[1];
    }
    if (a->interpolation_left < 1) { a->dynamic_active = 1; return; }
    actor_interpolation_tick(a);
}

// OmUtil::IsExceedDest：核對匯出跳表，任一行進軸到達中心即成立（含相等）。
// 斜向使用OR，不是點積；非法方向回0。
static int actor_passed_cell_center(const Actor *a) {
    int d = a->facing;
    if ((unsigned)d > 7u) return 0;
    int dx = ax64(a) - (a->grid_x * 64 + 32), dy = ay64(a) - (a->grid_y * 64 + 32);
    return (GRID_X[d] && dx * GRID_X[d] >= 0) || (GRID_Y[d] && dy * GRID_Y[d] >= 0);
}

// Poq.exe 0x4945d0：動態32先檢查越界／相鄰通行，再推占格，最後消費插值。
// 保留原版以「格座標、該格世界中心」算側向方向的混合單位，不能改成位置到中心。
static void actor_charge_tick(Actor *a) {
    if (a->dynamic_state != 32) return;
    if (actor_passed_cell_center(a)) {
        int x = ax64(a) / 64, y = ay64(a) / 64;
        int side = dir_of(x, y, x * 64 + 32, y * 64 + 32, 8);
        int next = a->facing + 1 < 8 ? a->facing + 1 : 0;
        int prev = a->facing - 1 < 0 ? 7 : a->facing - 1;
        if (side == next || side == prev) {
            int cx = x, cy = y;
            move_dir(&cx, &cy, a->facing, 1);
            const Cell *c = cell_at(cx, cy);
            // 側向只查地形層；角色占位交後面的actor_place判斷。
            if (!c || !cell_pass(c, a->layer)) {
                actor_dynamic_command(a, 1, 0, 0, 0);
                return;
            }
        }
        if (a->interpolation_left > 0) {
            move_dir(&x, &y, a->facing, 1);
            if (!actor_place(a, x, y, 0)) {
                actor_dynamic_command(a, 1, 0, 0, 0);
                return;
            }
        }
    }
    if (a->interpolation_left < 1) actor_dynamic_command(a, 1, 0, 0, 0);
    else actor_interpolation_tick(a);
}

// Poq.exe 0x4959d0：動態11沿反向byte路徑逐格步進。活動時先更新速度、等待byte先扣早退；
// 新路徑跳過距離／索引重尋；首點被同組占住就送讓路請求並跳過（最多一點）；
// 方向差1格直線可一次占兩格，差4且戰鬥中朝目標可倒退走；占格遇同組移動最多3次rand()%30+33等待，
// 大小>2失敗停移，其他重尋；插值只在direction_clock%move_b==0推進，插值完才標下一格可走。
// shortcut: 原版直線分支失敗後反編譯讓回退格落在「下一點」的全域暫存，這裡改占目前路徑點；
// 回放模式（scene_record_mode==2）不走這裡。
static void actor_walk_step(Actor *a) {
    a->moving = 0;
    if (a->dynamic_active) {
        actor_speed_update(a);
        if (a->walk_wait) { a->walk_wait--; return; }
        if (!a->dynamic_new_path) {
            int dx = abs(actor_init_x(a) - a->dynamic_destination_x), dy = abs(actor_init_y(a) - a->dynamic_destination_y);
            if ((dx > dy ? dx : dy) <= a->dynamic_command[2]) { actor_dynamic_command(a, 1, 0, 0, 0); return; }
            if (a->dynamic_path < 0) {
                actor_dynamic_enter(a);
                if (!actor_dynamic_retry(a)) actor_dynamic_command(a, 1, 0, 0, 0);
                return;
            }
        } else a->dynamic_new_path = 0;
        int count = 0, px, py;
        for (;;) {
            if ((unsigned)a->dynamic_path > 249u) return;
            px = a->path_buf[a->dynamic_path * 2]; py = a->path_buf[a->dynamic_path * 2 + 1];
            count++;
            const Cell *c = cell_at(px, py);
            Actor *o = c && cell_pass(c, a->layer) ? cell_occupant(px, py) : NULL;
            if (!o || (unsigned)(o->object_kind - 30) > 70u || o->grp != a->grp || count > 1) break;
            if (o != a) o->dynamic_aux = a->sh ? a->sh : (int)a->object_id;  // 讓路請求
            a->dynamic_path--;
        }
        int tx = px * 64 + 32, ty = py * 64 + 32;
        int dir = dir_of(ax64(a), ay64(a), tx, ty, 8);
        int diff = abs(dir - a->animation_dir), straight = 0, back = 0;
        if (diff == 1 || diff == 7) straight = 1;
        else if (diff == 4 && has_status(a, 0)) {
            Actor *t = actor_target(a);
            if (t && dist_of(ax64(a), ay64(a), ax64(t), ay64(t)) <= 0x1df) {
                int d = abs(dir - dir_of(ax64(a), ay64(a), ax64(t), ay64(t), 8));
                d = d == 7 ? 1 : d == 6 ? 2 : d == 5 ? 3 : d;
                back = d <= 1;
            }
        }
        int done = 0;
        if (straight && (unsigned)(a->dynamic_path - 1) <= 0xfa) {
            int nx = a->path_buf[(a->dynamic_path - 1) * 2], ny = a->path_buf[(a->dynamic_path - 1) * 2 + 1];
            if (dir_of(tx, ty, nx * 64 + 32, ny * 64 + 32, 8) == a->animation_dir && actor_place(a, nx, ny, 0)) {
                a->dynamic_path--;
                done = 1;
            }
        }
        if (!done) {
            a->reverse_playback = back;
            actor_direction_set(a, back ? (dir + 4) % 8 : dir);
            if (!actor_place(a, px, py, 0)) {
                Actor *o = cell_at(px, py) ? cell_occupant(px, py) : NULL;
                if (o && (unsigned)(o->object_kind - 30) <= 70u && o->grp == a->grp && a->walk_block < 3 &&
                    o->dynamic_state != 1 && !o->walk_wait) {
                    a->walk_block++;
                    a->walk_wait = rand() % 30 + 33;
                    return;
                }
                if (a->body_size > 2) { a->dynamic_path_result = 0; actor_dynamic_command(a, 1, 0, 0, 0); return; }
                actor_dynamic_enter(a);
                if (!actor_dynamic_retry(a)) actor_dynamic_command(a, 1, 0, 0, 0);
                return;
            }
            a->walk_block = 0;
        }
        actor_interpolate(a, -1, -1, -1);
        a->dynamic_path--;
        a->dynamic_active = 0;
    }
    if (a->move_b > 0 && a->direction_clock % a->move_b == 0) {
        if (a->interpolation_left > 0) { actor_interpolation_tick(a); a->moving = 1; return; }
        a->dynamic_active = 1;
    }
}

// Poq.exe 0x497310：客戶端／回放位置更新；不尋路、不在插值結束時退出動態。
// 靜止先讓路，跳躍中點先轉下降；任何動態只要剩餘拍數為正都消費插值。
static int actor_client_motion_tick(Actor *a) {
    if (a->dynamic_state == 1) actor_yield_tick(a);
    else if (a->dynamic_state == 22 && a->interpolation_left == a->jump_half)
        actor_jump_phase(a, 0);
    if (a->interpolation_left > 0) actor_interpolation_tick(a);
    return 1;
}

// Poq.exe 0x496110：動態狀態分派（1讓路、11步進、21/22衝刺跳躍、31擊退、32衝鋒），固定回1。
// 末尾的狀態同步封包0x323單機不送，只更新已回報狀態。
static int actor_dynamic_dispatch(Actor *a) {
    if (a->dynamic_state == 1) actor_yield_tick(a);
    actor_rush_update(a);
    actor_knock_tick(a);
    actor_charge_tick(a);
    if (a->dynamic_state == 11 && a->path_walk) actor_walk_step(a);
    a->dynamic_reported_state = a->dynamic_state;
    return 1;
}

// Poq.exe 0x48c050：活動時動態分派→虛擬播放更新，再處理位置同步與首次鎖存。
// 動態分派原版固定回傳1；其動態11路徑限制由基底分派承接。
static int actor_motion_tick(Actor *a) {
    int enabled = !a->activity_disabled;
    a->animation_sample_pending = 0;
    if (enabled) {
        if (scene_record_mode == 2) actor_client_motion_tick(a);
        else {
            actor_dynamic_dispatch(a);
        }
        // 單次動作稍後才交MethodMain消費；保留播放前的幀及完成值，
        // 延遲只有1拍的首／末幀也必須各消費一次。
        actor_body_select(a);
        const Seq *q = role_seq(a->role, a->body_seq);
        a->sampled_action_serial = a->action_serial;
        a->sampled_frame = q || a->act != ACT_ATTACK ? actor_frame(a, a->body_seq) : 99;
        a->sampled_finished = !q || actor_body_done(a, q);
        a->animation_sample_pending = 1;
        actor_animation_tick(a);
        actor_height_tick(a);
    }
    // 單機使用服務端分支。CClientScreen的dirty與視野廣播只同步客戶端副本，
    // 已列skip；本地共用Actor即時位置，不建立第二份可見物件鏈。
    // 停用也會處理位移；門檻嚴格大於127，更新兩軸快照。
    // DAT_00663804是目前伺服場景，建立／切換才綁定，清場及回放可為0。
    if (unit_scene && scene_record_mode != 2 &&
        (llabs((long long)a->command_world_x - ax64(a)) > 127 ||
         llabs((long long)a->command_world_y - ay64(a)) > 127)) {
        a->command_world_x = ax64(a);
        a->command_world_y = ay64(a);
    }
    // 原版停用也會鎖存首次更新，但回傳進入時的啟用值；不能當成重新啟用。
    if (!a->activity_latched) a->activity_latched = 1;
    return enabled;
}

// 原版受擊命令5傳WhatDir8(角色, 招式來源)，方向從來源指向角色。
static void knock_back(Actor *t, const Actor *a, int cells) {
    actor_knock_command(t, dir_of(ax64(a), ay64(a), ax64(t), ay64(t), 8), cells, 20);
}

// 打中後的反應（Poq.exe 0x48c3a0）：JDR = 攻方 B_C_JDR + 攻方這招的 JDR（技能欄 18），DJHF = 守方 B_C_DJHF（怪物表打击恢复率）+ 守方正在放的招的 DJHF（欄 19）。
// 這招 JDR 是 −1 的不反應。① JDR > 100 時千分之 (200 − DJHF)(JDR − 100)/500×10 擊退 (隨機(JDR)/80 + JDR/100 + 1) 格、硬直 格數×20 + 30 + 擲值/40 拍；
// ② JDR > 0 時千分之 (200 − DJHF)×JDR/200×10 擊退 (隨機(JDR)/80 + JDR/80 + 2) 格；③ 千分之 (200 − DJHF)×1000/(DJHF + 200) 挨打（播挨打動作）
// shortcut: ① 原版還要角色表 +0xa4 < 0x577100 的值（意義沒查），這裡都當成立；主角的 B_C_JDR、B_C_DJHF 原版從模板讀，這裡當 0；
// 擊退已按動態1／11與狀態2／10／101門檻接受；完整命令5可用性仍待共用分派。
static void hit_react(Actor *a, Actor *t, const SkillRow *sk) {
    const SkillRow *ts = t->act == ACT_ATTACK ? (t->casting ? t->cast : t->skill) : NULL;
    int sj = sk ? (int)skill_field(sk, 18, 0) : 0;
    if (sj == -1) return;
    int jdr = a->ex[74] + sj, djhf = t->ex[75] + (ts ? (int)skill_field(ts, 19, 0) : 0);
    if (jdr > 100) {
        int r = rand() % 1000;
        if (r < (int)((200.0f - djhf) * (jdr - 100.0f) / 500 * 10)) {
            int n = (int)((float)(rand() % jdr / 80) + jdr / 100.0f + 1);
            knock_back(t, a, n);
            int st = n * 20 + 30 + r / 40;
            if (st > t->stun) t->stun = st;
        }
    }
    if (jdr > 0 && rand() % 1000 < (int)((200.0f - djhf) * jdr / 200 * 10))
        knock_back(t, a, (int)((float)(rand() % jdr / 80) + jdr / 80.0f + 2));
    if (rand() % 1000 < (int)((200.0f - djhf) * 1000 / (djhf + 200.0f)) &&
        t->dynamic_state == 1 && t->knock != 1 && !has_status(t, 101) &&
        !has_status(t, 2) && !has_status(t, 10)) {
        actor_command(t, 12, 0, 0, 0, 0, 0);
        if (has_status(t, 101)) start_act(t, ACT_HIT, t->def->hit);
    }
}

// 位移由固定拍的動態31推進；第5修正只控制動畫／命令門檻，不自行產生落點。
static int react_tick(Actor *a, const Scene *s) {
    (void)s;
    if (a->stun > 0) a->stun--;
    return a->dynamic_state == 31 || a->knock || a->stun > 0;
}

// 盾牌格擋（Poq.exe 0x48c3a0 的 0x48c4e3 段）：守方有旗標 0x40000（招式觸發 8 設的格擋姿勢）時，
// 要是守方沒在放招（+0x4e7 = 現在的 MethodMain 沒有）、放的招不在技能鏈上、或 rand()%100 ≤ 那招的 p7（SkillUnit +0x28）− 100，
// 就量「從攻擊來源指向守方的方向」（WhatDir8(守方, 來源)）和守方面向（+0xa7）的差（不繞圈的絕對值）：差 ≥ 2 且 ≠ 7 = 攻擊從正面半圈來，
// 擋住：在守方腳下放單位 0x11（擋住的火花），層同守方，這一下不算打中。回 1 = 擋住
// shortcut: 來源位置用攻擊者的格子（原版用招式單位 +0x34，箭、法術飛行時是單位當下的位置）；0x3a7 旗標與層差的前置條件（同層或來源種類 6）沒翻
static int shield_block(Actor *a, Actor *t, Uint32 tick) {
    if (!actor_flags(t, 0x40000)) return 0;
    const MUnit *cur = main_of(t);
    int i = cur && cur->m ? actor_skill_find(t, cur->m->id) : -1;
    if (cur && i >= 0 && rand() % 100 > t->skills[i].p7 - 100) return 0;
    int diff = abs(dir_of(ax64(a), ay64(a), ax64(t), ay64(t), 8) - t->facing);
    if (diff == 7 || diff < 2) return 0;
    MUnit src = {0};
    MUnit *u = unit_new(0x11, ax64(t), ay64(t), &src, tick);
    if (u) u->layer = t->layer;
    return 1;
}

static void strike(Actor *a, Actor *t, const SkillRow *sk, Uint32 tick) {
    (void)tick;
    if (!alive(t)) return;
    if (t->mon && !t->foe) actor_target_set(t, a);  // 被打的怪物會反擊
    actor_enter_fight(t);
    if (shield_block(a, t, tick)) return;
    // 命中（Poq.exe 0x48c600，CCharactor 虛擬函式第 104 格）：千分比 = HR×1000 / (SR + HR)，夾在 100~900；HR 0 當 1
    int hr = a->hr ? a->hr : 1, chance = hr * 1000 / (t->sr + hr);
    if (rand() % 1000 >= (chance < 100 ? 100 : chance > 900 ? 900 : chance)) { if (debug_cells == 2) printf("strike t=%u #%d -> #%d miss chance=%d hr=%d sr=%d\n", tick, (int)(a - actors), (int)(t - actors), chance, a->hr, t->sr); return; }
    // 原版受擊入口：4/7先留血，其餘0/1/2/3/8在反應後留血。
    if (sk && (sk->attack == 4 || sk->attack == 7)) actor_blood(t, 1);
    // 技能參數（EasyRPG 0x10024xxx 一屬性一個處理，0x1039e628 起的函式表）存成這次攻擊的暫時加值，分固定值、百分比兩組：
    // 210~220 偶數 = 眩暈、中毒、冰凍、失明、石化、潰散的機率，222、223 = 吸血、吸精氣，250、251 = AP、AP_MAX，252~261 = MAP1~5、MAP1~5_MAX，
    // 287 = 自損生命（Attack方式 10）；ID > 500 拆成 ID−300、ID−299 兩個。262~269 寫進的變數整個 dll 沒有讀，沒作用
    float add[100] = {0}, pct[100] = {0};
    for (int i = 0; sk && i < sk->mod_n; i++) {
        const SkillMod *m = &sk->mod[i];
        float v = skill_value(m->init, m->incr, sk->level);
        for (int id = m->id > 500 ? m->id - 300 : m->id; id <= (m->id > 500 ? m->id - 299 : m->id); id++)
            if (id >= 200 && id < 300) (m->pct ? pct : add)[id - 200] = v;
    }
    // 物理（0x10033140）：攻擊 = (AP + 加值) × (100 + 百分比) / 100，AP~AP_MAX 隨機；損血 = 攻擊² / (攻擊 + DP/2)
    int ap = (int)((a->ap + add[50]) * (pct[50] + 100) / 100), ap_max = (int)((a->ap_max + add[51]) * (pct[51] + 100) / 100);
    int dmg = ap + (ap_max > ap ? rnd(ap_max - ap + 1) : 0);
    // 致命一击／会心一击（Poq.exe 0x48c3a0 擲完送 0x13b → EasyRPG 0x10033140）：百分之 B_C_ZMYJ 物理損 ×2；否則百分之 B_C_HXYJ 不算對方防禦
    int crit = rand() % 100 < a->ex[72] ? -1 : rand() % 100 < a->ex[73] ? -2 : 0, dp = crit == -2 ? 0 : t->dp;
    int phys = dmg + dp / 2 ? dmg * dmg / (dmg + dp / 2) * (crit == -1 ? 2 : 1) : 0;
    // 法術 MAP1~5 對 MDP1~5（同一函式）：最大值用的百分比跟最小值同一格（原版如此）；MDP > 0 時損 = MAP² / (MAP + MDP)、比例 = MAP / (MAP + MDP)，
    // MDP ≤ 0 時損 = MAP × (MAP − 2·MDP) / (MAP − MDP)、比例 1。比例乘在該屬性觸發狀態的機率上；中毒的毒傷 = MAP2 擲出的值
    int magic = 0, poison = 0;
    float ratio[5] = {0};
    for (int i = 0; i < 5; i++) {
        int lo = (int)((a->ex[43 + 2 * i] + add[52 + 2 * i]) * (pct[52 + 2 * i] + 100) / 100);
        int hi = (int)((a->ex[44 + 2 * i] + add[53 + 2 * i]) * (pct[52 + 2 * i] + 100) / 100), mdp = t->ex[54 + i];
        if (lo < hi) lo += rnd(hi - lo + 1);
        if (!lo) continue;
        if (i == 1) poison = lo;
        if (mdp < 1 ? lo - mdp : lo + mdp) {
            ratio[i] = mdp < 1 ? 1 : (float)lo / (lo + mdp);
            magic += (int)(mdp < 1 ? (float)(lo - 2 * mdp) / (lo - mdp) * lo : ratio[i] * lo);
        }
        if (magic < 0) magic = 0;
    }
    // 減傷：B_JSWL 減物理、B_JSMF 減法術的百分比，最多 75
    int jswl = t->ex[76] > 75 ? 75 : t->ex[76], jsmf = t->ex[77] > 75 ? 75 : t->ex[77];
    int loss = phys - jswl * phys / 100 + magic - jsmf * magic / 100;
    if (loss <= 0) loss = 3;  // 總損血不到 1 才補成 3（原版 -1 < 損 → -3）
    t->hp -= loss;
    if (debug_cells == 2) printf("strike t=%u #%d -> #%d loss=%d phys=%d magic=%d t.hp=%d refl=%d atk=%d\n", tick, (int)(a - actors), (int)(t - actors), loss, phys, magic, t->hp, t->ex[68], sk ? sk->attack : -1);
    hit_wear(a, t, sk);
    // 反彈（對方 B_FJXS% 的損血打回來）、自損（Attack方式 10：HP_MAX × 287 參數%）、吸血（B_XQSM + 222，補到滿為止）、吸精氣（B_XQMF + 223）
    if (t->ex[68] > 0) a->hp -= t->ex[68] * loss / 100;
    if (sk && sk->attack == 10 && add[87] > 0) a->hp -= a->hp_max * (int)add[87] / 100;
    int heal = (a->ex[70] + (int)add[22]) * loss / 100, mana = (a->ex[71] + (int)add[23]) * loss / 100;
    if (heal > 0) a->hp += heal < a->hp_max - a->hp ? heal : a->hp_max - a->hp;
    if (mana > 0) a->mp = a->mp + mana > a->mp_max ? a->mp_max : a->mp + mana;
    // 打中觸發狀態：機率 = (B_xxRATE + 技能加值) × (100 + 技能百分比) / 100 → 成功率 = 機率×100 / (機率 + K) × 該屬性比例；
    // 對方 B_xxIMMU ≥ 100 免疫，時間 = ((機率 + 隨機(機率)) / D + C) × (100 − 免疫) / 100 秒。
    // 眩暈 10（金，K 200、30、3）、中毒 1（木，100、10、12）、冰凍 4（水，100、20、6）、失明 33（火，100、15、8）、石化 2（土，500、6、15）
    static const int ST[5] = {10, 1, 4, 33, 2}, K[5] = {200, 100, 100, 100, 500}, D[5] = {30, 10, 20, 15, 6}, C[5] = {3, 12, 6, 8, 15};
    for (int k = 0; k < 5 && alive(t); k++) {
        int rate = (int)((pct[10 + 2 * k] + 100) * (a->ex[80 + 2 * k] + add[10 + 2 * k]) / 100);
        if (rate <= 0) continue;
        int im = t->ex[92 + k] < 0 ? 0 : t->ex[92 + k], p = rate * 100 / (rate + K[k]);
        if (p <= 0 || im >= 100) continue;
        int time = ((rate + rnd(rate)) / D[k] + C[k]) * (100 - im) / 100;
        if (rnd(100) < (int)(p * ratio[k])) buff_add(t, NULL, ST[k], 1, time, k == 1 ? poison : 0);
    }
    // 潰散 17：成功率直接 = 機率，7 秒
    int ks = (int)((pct[20] + 100) * (a->ex[90] + add[20]) / 100);
    if (ks > 0 && t->ex[97] < 100 && rnd(100) < ks && alive(t)) buff_add(t, NULL, 17, 1, 7, 0);
    if (a->hp <= 0 && alive(a)) actor_kill(a, t);
    if (t->hp <= 0) { actor_kill_mode(t, a, sk ? sk->die : 1); return; }
    hit_react(a, t, sk);
    if (!sk || sk->attack == 0 || sk->attack == 1 || sk->attack == 2 || sk->attack == 3 || sk->attack == 8)
        actor_blood(t, 1);
}

// 敵對：不同隊伍的
static int hostile(const Actor *a, const Actor *e) { return e != a && alive(e) && e->side != a->side; }

// 招式打中的對象（Poq.exe 0x43c4f0 → 0x438a00 依 iTriID）：觸發 0 的 id 30 = 直接打攻擊目標（原版 case 0x1e：目標是人就呼叫中心物件虛擬函式 0x68，不看位置），
// 觸發 15（結算，目標 −1）= 對自己放（原版封包 0x13b 沒有目標）；兩者在這裡。其他 id（0、2、3、5~13、0x14）照 0x438a00 掃格子，在 unit_hits／hit_find（unit.c）
static void method_hits(Actor *a, Uint32 tick) {
    const SkillRow *sk = a->casting ? a->cast : a->skill;
    Actor *t1 = a->casting && a->cast && !skill_needs_foe(a->cast) ? a : a->foe;
    skill_hit(a, t1, sk, tick);
    if (t1 && t1 != a) main_hit(a, t1, tick);
    if (a->casting) a->casting = 2;
}

// 角色出手的觸發中跟出手的人有關的（0x43c4f0，來源是 MethodMain）：0 命中、15 結算、16 的 0x14/0x16 放分身、2 放單位（放出點用出手者動作的熱點）、
// 13 改幀延遲、8 動作旗標、11 離地、5/7 衝過去；其他（音效、震畫面、結束、16 改目標點）照招式單位的做。回傳 0 = 不是這裡的
static int owner_trig(MUnit *u, const Trig *t, Uint32 tick) {
    Actor *a = u->owner;
    if (t->type == 0 && t->id != 30) {  // 0x438a00：掃格子找人打，沿途的 0x80 建築（水缸）也被「使用」（hit_find）
        unit_hits(u, t->id, tick);
        if (a->casting) a->casting = 2;
    }
    else if (t->type == 0 || t->type == 15) {
        method_hits(a, tick);
        if (t->type == 0) { hit_buildings_only = 1; unit_hits(u, t->id, tick); hit_buildings_only = 0; }
    }
    else if (t->type == 16 && (t->id == 0x14 || t->id == 0x16))  // 分身 ID2 個：0x14 殘影（idx 從 2 起）、0x16 環繞（idx 從 0 起）
        for (int k = 0; k < t->id2; k++) phantom_new(u, t->id == 0x14 ? 1 : 3, t->id == 0x14 ? k + 2 : k);
    else if (t->type == 2) {  // 單位自己會打中（有命中觸發或是飛行物）時，左手技能不用在放完時補結算
        int before = unit_n, ex, ey;
        actor_hot(a, &ex, &ey);
        unit_spawn(t->id, t, u, u->x, u->y, ex, ey, tick);
        for (int k = before; a->casting && k < unit_n; k++)
            if (units[k].def && unit_can_hit(units[k].def)) a->casting = 2;
    }
    else if (t->type == 13) a->adj = t->id - 5;
    else if (t->type == 12) { if (t->id) zoom_add(0.24f); else zoom_back(); }  // 鏡頭拉近／放回（招式單位的 12 是透明度）
    else if (t->type == 8) act_flags(a, t);
    else if (t->type == 11) {  // 離地：每拍升降 ID、最高 ID2（0x43c4f0 → 0x4961e0）
        actor_height_set(a, t->id, t->id2);
    }
    else if (t->type == 5 || t->type == 7) {
        actor_rush_command(a, t->type == 5 ? 21 : 22, u->tx / 64, u->ty / 64, u->m->range_max);
        if (t->type == 7) a->z = t->id; // 原版即使命令拒絕，仍寫觸發指定的高度。
    }
    else return 0;
    return 1;
}

// 角色每拍的聲音和腳印（Poq.exe 0x486ed0）：千分之一播平時／戰鬥聲音；走路時每 max((15 − A)×2, 10) 拍腳步聲，
// 腳數 6 用 0x218、軟地（地面種類 3）0x51、其他 0x50；角色大小 1、兩隻腳在軟地上離上個腳印 x 或 y 超過 32 就踩一個，左右腳輪流
// shortcut: 音效原版依位置算音量（0x4804a0），這裡只播主角附近 12 格內的；騎乘時不出聲仍待接入；探索已交固定拍更新。
// Poq.exe 0x575ae0：採樣每個部件當前幀，離地高度以世界X/Y各扣2z承接。
static void actor_illusion_sample(Actor *a) {
    const Role *r = a->role;
    // 原版取首個部件的播放幀索引，各部件越界時回第0幀。
    if (!r || !r->part_count) return;
    int seq = actor_seq(a);
    if (seq < 0 || seq >= r->seq_count) return;
    int frame = actor_frame(a, seq);
    for (int i = 0; i < r->part_count; i++) {
        const Anim *an = r->body[i];
        if (!a->illusion[i] || !an || seq >= an->count || !an->seqs[seq].count) continue;
        const Frame *f = &an->seqs[seq].frames[frame >= 0 && frame < an->seqs[seq].count ? frame : 0];
        illusion_sample(a->illusion[i], ax64(a) - a->z * 2, ay64(a) - a->z * 2, f);
    }
}

static void walk_fx(Actor *a, const Scene *s, Uint32 tick) {
    const RoleDef *d = a->def;
    if (!d || !a->role || !alive(a)) return;
    actor_illusion_sample(a);
    actor_blood(a, 0);
    if (a->direction_clock % 20 == 1) actor_explore(a);
    int near = fabsf(a->x - actors[0].x) < 12 && fabsf(a->y - actors[0].y) < 12;
    int snd = a->fight ? d->snd_fight : d->snd_idle;
    if (snd && snd != 0xffff && rand() % 1000 == 100 && near) audio_sfx(snd);
    if (!a->moving || a->act) return;
    int cx = (int)a->x, cy = (int)a->y;
    const Cell *c = cx >= 0 && cy >= 0 && cx < s->w && cy < s->h ? &s->cells[cy * s->w + cx] : NULL;
    int soft = c && c->kind == 3, sp = a->speed < 0 ? 0 : a->speed > 59 ? 59 : a->speed, step = (15 - SPEED_A[sp]) * 2;
    if (tick % (step < 10 ? 10 : step) == 0 && near) audio_sfx(d->legs == 6 ? 0x218 : soft ? 0x51 : 0x50);
    if (d->size == 1 && d->legs == 2 && soft && (abs(ax64(a) - a->print_x) >= 0x20 || abs(ay64(a) - a->print_y) >= 0x20)) {
        foot_new(ax64(a), ay64(a), DIR_MAP[a->dir] + (a->foot ? 0 : 0x10));
        a->foot = !a->foot;
        a->print_x = ax64(a), a->print_y = ay64(a);
    }
}

// 震畫面（觸發 type 14 ID 4 → 0x44ca40 → 0x461180）：還在震就不重來；剩下拍數每 5 拍換一次上下，幅度 = 畫面寬/640×25×剩下/總共（0x45bxxx）

// 單次動作的進度：攻擊依招式各幀觸發音效與命中；播完回到平常（死亡除外）
// shortcut: type 6（只對物件種類 7）、type 10（招式物件虛擬函式 0x20 = 0x43a760）還沒做
static void action_tick(Actor *a, const Scene *s, Uint32 tick) {
    (void)s;
    if (a->activity_disabled) return;
    actor_animation_select(a, a->act_seq + DIR_MAP[a->dir]);
    actor_body_select(a);
    int seq = actor_seq(a);
    const Seq *q = role_seq(a->role, seq);
    int finished = !q || actor_body_done(a, q);
    int f = q || a->act != ACT_ATTACK ? actor_frame(a, seq) : 99;  // 角色沒有這招的動作（陷阱）：觸發一次全放
    int played = a->animation_sample_pending;
    if (played && a->sampled_action_serial == a->action_serial) {
        f = a->sampled_frame;
        finished = a->sampled_finished;
    }
    a->animation_sample_pending = 0;
    if (a->act == ACT_ATTACK && a->atk) {  // 招式的觸發由角色出手物件（U_MAIN）放
        if (a->last_frame < 0 && !main_of(a)) main_new(a);
        MUnit *u = main_of(a);
        if (u) {
            u->x = ax64(a), u->y = ay64(a), u->frame = u->no_animation ? INT_MAX : f;
            unit_base(u, tick);
        }
    }
    a->last_frame = f;
    // 先讓MethodMain消費末幀，再結束動作；延遲1的末幀也不能漏掉觸發。
    if (!played) actor_animation_tick(a);
    if (a->act == ACT_ATTACK && finished) {  // 動作播完：連鎖就重播（0x43a760），否則結束角色出手
        MUnit *u = main_of(a);
        if (u && unit_bounce(u)) {
            a->act_frame = a->frame_tick = a->act_done = 0;
            a->last_frame = -1;
            return;
        }
        if (u) unit_die(u, tick);
    }
    if (a->act != ACT_DEAD && finished) {
        if (a->act == ACT_ATTACK && a->mon && a->skill) {  // 怪物打完一招記成上一招；完整NPC主AI仍待CNpc移植。
            a->last_skill = a->skill;
            a->skill = NULL;
        }
        if (a->casting) {  // 左手技能放完：招式沒有打中的觸發就在這裡生效，換回右手招式
            if (a->casting == 1 && a->cast) skill_hit(a, skill_needs_foe(a->cast) ? a->foe : a, a->cast, tick);
            a->cast = NULL;
            a->atk = a->atk_rh;
            a->casting = 0;
        }
        act_end(a);
        a->act = ACT_NONE;
        a->act_tick = 0;
    }
}

// 怪物選招（Poq.exe 0x4906c0，每次出手前；技能1~8 依序看招式表的 wManualID、Reserve3）：
// wManualID 0、3 且 Reserve3 = 1（遠程招）：一半機率考慮，RangeMax < 3 時目標在範圍內就用、否則距離（格，|dx|+|dy|）≥ RangeMax 且不是上一招就用；
// wManualID 1、4，或 0、3 而 Reserve3 ≠ 1：Reserve3 = 0 的招數 n 從 1 起累加，已選過時 1/n 機率才再看；
// 再看 Reserve3：0 = 選、2 = 一半機率、3 = 1/3 機率（都要不是上一招）、5 = 血少於 40% 才選、4 和其他 = 不選。其他 wManualID 不選
// 怪物依角色技能鏈的Enable與移動速度限制排除不可用招式；精气仍由技能結算層處理。
static const SkillRow *mon_pick(const Actor *a) {
    const SkillRow *pick = NULL;
    int n = 1;
    for (int i = 0; i < 8; i++) {
        const SkillRow *r = npc_skill(a->mon->skill[i]);
        const Method *m = r ? method_find(skill_attack_id(r)) : NULL;
        if (!m || !actor_row_enabled(a, r) || !actor_can_use_id(a, skill_attack_id(r))) continue;
        if ((m->manual == 0 || m->manual == 3) && m->ai == 1) {
            if (rand() % 2 && alive(a->foe)) {
                int range = actor_row_range(a, r), d = (int)(fabsf(a->x - a->foe->x) + fabsf(a->y - a->foe->y));
                if (range < 3 ? cell_dist(a, a->foe) <= range + 0.5f : d >= range && r != a->last_skill) return r;
            }
            continue;
        }
        if (m->manual != 0 && m->manual != 1 && m->manual != 3 && m->manual != 4) continue;
        if (!m->ai) n++;
        if (pick && rand() % n) continue;
        if (m->ai == 0 || (m->ai == 2 && rand() % 2 && r != a->last_skill) || (m->ai == 3 && rand() % 3 == 1 && r != a->last_skill) ||
            (m->ai == 5 && a->hp * 100 < a->hp_max * 40))
            pick = r;
    }
    return pick;
}

// 隊友與分身的選招（Poq.exe 0x4906c0 本體走的是角色技能鏈，怪物也一樣；規則同 mon_pick）：技能列 + 模板普通攻擊（沒有技能列的節點）。
// shortcut: 原版節點 Enable 由伺服端依精氣更新（0x1004c2a0），這裡用 skill_usable 代替；回傳節點索引，沒有 −1
static int chain_pick(const Actor *a) {
    int pick = -1, n = 1;
    for (int i = 0; i < a->skill_n; i++) {
        const ActorSkill *s = &a->skills[i];
        const SkillRow *r = s->row;
        const Method *m = method_find(s->id);
        if (!m || !(a->def ? actor_skill_enabled(a, s) : s->enabled) || (r && !skill_usable(a, r)) || (a->def && !actor_can_use_id(a, s->id))) continue;
        if ((m->manual == 0 || m->manual == 3) && m->ai == 1) {
            if (rand() % 2 && alive(a->foe)) {
                int range = actor_skill_range(a, s), d = (int)(fabsf(a->x - a->foe->x) + fabsf(a->y - a->foe->y));
                if (range < 3 ? cell_dist(a, a->foe) <= range + 0.5f : d >= range && r != a->last_skill) return i;
            }
            continue;
        }
        if (m->manual != 0 && m->manual != 1 && m->manual != 3 && m->manual != 4) continue;
        if (!m->ai) n++;
        if (pick >= 0 && rand() % n) continue;
        if (m->ai == 0 || (m->ai == 2 && rand() % 2 && r != a->last_skill) || (m->ai == 3 && rand() % 3 == 1 && r != a->last_skill) ||
            (m->ai == 5 && a->hp * 100 < a->hp_max * 40))
            pick = i;
    }
    return pick;
}

static void cast_start(Actor *h, const SkillRow *r);

static void actor_skill_command_tick(Actor *a);

static void gbk_utf8(const char *in, char *out, size_t n);
static void actor_overhead(Actor *a, const char *text, unsigned char duration, unsigned char floating);
// ---- CNpc（Poq.exe 0x490240～0x493320）----
// 建構／解構0x490240、0x4902c0、0x490360、0x490e30及空函數0x490960、0x579dd0以Actor零初始化與生命週期承接（見funcs_skip）。
// 想像中的「主人名單」遍歷用 actor_near_begin／actor_near_next（0x48b9e0／0x48ba50）。

// Poq.exe 0x491c80：技能鏈第一個manual==4的技能（IDTable物件+0x34）；共用遍歷游標由呼叫端索引取代。
LATER static ActorSkill *npc_find_manual4(Actor *a) {
    for (int i = 0; i < a->skill_n; i++) {
        const Method *m = method_find(a->skills[i].id);
        if (m && m->manual == 4) return &a->skills[i];
    }
    return NULL;
}

static int npc_in_fight(const Actor *a) { return has_status(a, 0) != 0; }

// Poq.exe 0x493320：先找索敵圈內的人，沒有就在單機取主角；目標是角色（種類30..100）且組別不同才選為目標並入戰。
// shortcut: DAT_00662c4c（有操控角色）單機恆真。
LATER static void npc_aggro_nearest(Actor *a) {
    Actor *t = actor_near_begin(a, 0, -1);
    if (!t) t = &actors[0];
    if ((unsigned)(t->object_kind - 30) < 71u && t->grp != a->grp) {
        actor_target_set(a, t);
        actor_command(a, 11, 1, 0, 0, 0, 0);
    }
}

// Poq.exe 0x492370：程序0x2a——有目標且能出招時，叫周圍同組以manual4技能的射程為半徑一起打同一目標。
LATER static void npc_group_2a(Actor *a) {
    Actor *t = actor_target(a);
    if (!t || a->def->program != 0x2a || !actor_can_use_id(a, -1)) return;
    int tx = actor_init_x(t), ty = actor_init_y(t);
    ActorSkill *sk = npc_find_manual4(a);
    if (!sk) return;
    actor_command(a, 8, sk->id, tx + 1, ty, 1, 0);
    for (Actor *p = actor_near_begin(a, 1, sk->max); p; p = actor_near_next(a))
        if (actor_can_use_id(p, -1)) {
            actor_target_set(p, t);
            actor_command(p, 11, 1, 0, 0, 0, 0);
        }
}

// Poq.exe 0x4924b0：程序0——有目標時自己先站到目標右邊，同程序0的同組依序站成包圍陣（技能0xa1）。
LATER static void npc_formation(Actor *a) {
    Actor *t = actor_target(a);
    if (!t || a->def->program != 0 || !actor_can_use_id(a, -1)) return;
    int tx = actor_init_x(t), ty = actor_init_y(t);
    actor_command(a, 8, 0xa1, tx + 1, ty, 1, 0);
    static const int OFF[7][2] = {{-1, 0}, {0, -1}, {0, 1}, {1, -1}, {-1, -1}, {-1, 1}, {1, 1}};
    int i = 0;
    for (Actor *p = actor_near_begin(a, 1, -1); p; p = actor_near_next(a)) {
        if (p->def->program != 0 || !actor_can_use_id(p, -1)) continue;
        actor_target_set(p, t);
        actor_command(p, 11, 1, 0, 0, 0, 0);
        actor_command(p, 8, 0xa1, tx + (i < 7 ? OFF[i][0] : 0), ty + (i < 7 ? OFF[i][1] : 0), 1, 0);
        i++;
    }
}

// Poq.exe 0x490c20：自己的求援除數（欄26第4值）<1時，找同組中除數>=1且不在戰鬥中的人；
// 每次1/除數的機率重登記目標並入戰。
// shortcut: 反編譯沒帶出has_status的物件，取被掃到的同伴；0x48eda0以自己為this（命令也是自己）。
LATER static void npc_assist_call(Actor *a) {
    Actor *t = actor_target(a);
    if (!t || a->def->help >= 1 || !actor_can_use_id(a, -1)) return;
    Actor *p = actor_near_begin(a, 1, -1);
    while (p && (p->def->help < 1 || npc_in_fight(p))) p = actor_near_next(a);
    if (p && rand() % p->def->help == 0) {
        actor_target_set(a, t);
        actor_command(a, 11, 1, 0, 0, 0, 0);
    }
}

// Poq.exe 0x4909b0：依程序分派包圍（0）或群體出手（0x2a），最後做求援。
LATER static void npc_alert(Actor *a) {
    if (a->def->program == 0) npc_formation(a);
    else if (a->def->program == 0x2a) npc_group_2a(a);
    npc_assist_call(a);
}

// Poq.exe 0x490d10／0x490e70：NPC隨機台詞。戰鬥用欄40第1、2值（顯示70拍），平時第3、4值（50拍）；
// 頭頂槽0是空的、rand()%255==100才從[起,終)抽一句；0x490e70先要求累積拍%10==1（回傳恆1）。
LATER static void npc_chat(Actor *a) {
    int fight = npc_in_fight(a), lo = a->def->chat[fight ? 0 : 2], hi = a->def->chat[fight ? 1 : 3];
    if (lo == 65535 || hi == 65535 || hi <= lo || a->overhead[0].left) return;
    if (rand() % 0xff != 100) return;
    char g[256], u[256];
    if (res_dlg("Chat.dlg", rand() % (hi - lo) + lo, g, sizeof g)) {
        gbk_utf8(g, u, sizeof u);
        actor_overhead(a, u, fight ? 0x46 : 0x32, 0);
    }
}
LATER static int npc_chat_tick(Actor *a) {
    if (a->direction_clock % 10 == 1) npc_chat(a);
    return 1;
}

// 往foe的反方向逃 step 格的點（GetFleePos）。步長是呼叫點的隱藏參數（機碼核對）：0x490fc0 恐懼逃跑 = 精氣視野修正/3 + 1；
// 0x491af0 退開 = 呼叫端傳的 dist；0x4911b0 射程退一步 = 技能射程 − 距離 − 2
static void npc_flee_pos(const Actor *a, const Actor *foe, int *x, int *y, int step) {
    *x = actor_init_x(a); *y = actor_init_y(a);
    move_dir(x, y, dir_of(actor_init_x(foe), actor_init_y(foe), *x, *y, 8), step);
}

// Poq.exe 0x490fc0：狀態0x11（恐懼）逃跑。無敵人時隨機走開並解除，有敵人時往反方向逃；逃不掉有1/15機率解除。
static int npc_flee(Actor *a) {
    if (!has_status(a, 0x11)) return 0;
    if (a->dynamic_state == 1) {
        Actor *foe = actor_near_begin(a, 0, actor_seek(a) - 2);
        int x, y;
        if (!foe) {
            actor_status_notify(a, 0x11, 0, -1);
            open_dir(&x, &y, actor_init_x(a), actor_init_y(a), a->seek_adjust / 3 + 1);
            actor_command(a, 2, x, y, 2, 1, 0);
        } else {
            npc_flee_pos(a, foe, &x, &y, a->seek_adjust / 3 + 1);
            if (actor_command(a, 1, x, y, 2, 1, 0) && a->def->panic > 0 && rand() % 15 == 0)
                actor_status_notify(a, 0x11, 0, -1);
        }
        if (!actor_dynamic_retry(a)) actor_status_notify(a, 0x11, 0, -1);
    }
    return 1;
}

// Poq.exe 0x491af0：閒置時往空曠處（無敵人，距離dist）或遠離敵人走一步，成功就清目標；
// 無敵人那邊還離戰（命令11關）。
LATER static void npc_move_away(Actor *a, Actor *foe, int dist) {
    if (!actor_command_idle(a) || a->dynamic_state != 1) return;
    int x, y;
    if (!foe) {
        open_dir(&x, &y, actor_init_x(a), actor_init_y(a), dist);
        if (actor_command(a, 1, x, y, 0, 0, 0)) {
            actor_clear_target(a);
            actor_command(a, 11, 0, 0, 0, 0, 0);
        }
    } else {
        npc_flee_pos(a, foe, &x, &y, dist);
        if (actor_command(a, 1, x, y, 0, 0, 0)) actor_clear_target(a);
    }
}

// Poq.exe 0x493080：程序0x28（守衛）。第一次記出生格，之後有目標時近了回出生格附近，遠一點出技能，太遠追上；
// 回到出生格3格內就把家記成-2停用。回傳1表示本拍已處理。
static int npc_guard(Actor *a) {
    if (a->def->program != 0x28 || a->npc_home_x == -2) return 0;
    if (a->npc_home_x == -1 || a->npc_home_y == -1) {
        a->npc_home_x = actor_init_x(a);
        a->npc_home_y = actor_init_y(a);
    }
    Actor *t = actor_target(a);
    if (!t) {
        if (a->direction_clock % 100 == 3) npc_aggro_nearest(a);
        return 0;
    }
    if (!actor_command_idle(a) || a->dynamic_state != 1) return 1;
    if (a->direction_clock % 0x21 == 3) npc_aggro_nearest(a);
    int tx = actor_init_x(t), ty = actor_init_y(t), sx = actor_init_x(a), sy = actor_init_y(a);
    int d = dist_of(sx, sy, tx, ty);
    if (d < 13) {
        if (d < 9) {
            int home = dist_of(sx, sy, a->npc_home_x, a->npc_home_y);
            if (home < 3) { a->npc_home_x = -2; return 1; }
            actor_command(a, 2, a->npc_home_x, a->npc_home_y, home - 8, 1, 0);
        } else {
            ActorSkill *sk;
            if (!actor_can_use_id(a, -1) || a->direction_clock % 0xaa != 3 || !(sk = npc_find_manual4(a))) return 1;
            actor_command(a, 8, sk->id, tx, ty, 1, 0);
        }
    } else if (a->direction_clock % 0x2d == 3) actor_command(a, 1, tx, ty, 9, 1, 0);
    return 1;
}

// Poq.exe 0x490430：NPC選目標。有恐懼／魅惑（狀態0x12）且已有目標就不動；索敵圈內取第一個候選
// （程序第3值==1時優先取有狀態0x79者）；NPC組只有候選在技能射程（至少4格）內才登記目標；
// 沒候選且退戰旗標開時清目標，force或累積拍%5==1才離戰。最後一律入戰（狀態0）。
// shortcut: 場景類型0x12／0x13早退（CScene+0x6c來源是0x123封包）單機沒有；0x79的has_status取候選。
static void npc_pick_target(Actor *a, int force) {
    if (has_status(a, 0x12) && actor_target(a)) return;
    Actor *first = actor_near_begin(a, 0, -1), *pick = first;
    if (first && a->def->prog_extra[1] == 1) {
        for (Actor *p = first; p; p = actor_near_next(a))
            if (has_status(p, 0x79)) { pick = p; break; }
    }
    if (!pick) {
        if (a->battle_return) {
            actor_clear_target(a);
            if ((force || a->direction_clock % 5 == 1) && npc_in_fight(a)) actor_command(a, 11, 0, 0, 0, 0, 0);
        }
        return;
    }
    int near = 1;
    if ((unsigned)(a->grp - 10000u) <= 50000u) {
        int radius = 0x100;
        if (a->selected >= 0 && a->selected < a->skill_n && a->skills[a->selected].max * 0x40 > radius)
            radius = a->skills[a->selected].max * 0x40;
        near = (int)sqrt((double)(ax64(a) - ax64(pick)) * (ax64(a) - ax64(pick)) +
                         (double)(ay64(a) - ay64(pick)) * (ay64(a) - ay64(pick))) < radius;
    }
    if (near) actor_target_set(a, pick);
    if (!npc_in_fight(a)) actor_command(a, 11, 1, 0, 0, 0, 0);
}

// Poq.exe 0x4906c0後的選取結果：沒選取時先選招（怪物才有招式表），回目前選取的技能或NULL。
static ActorSkill *npc_selected(Actor *a) {
    if ((a->selected < 0 || a->selected >= a->skill_n) && a->mon && npc_skill(a->mon->skill[0])) {
        const SkillRow *r = mon_pick(a);
        if (r) { a->skill = r; actor_right_skill(a, skill_attack_id(r)); }
    }
    if ((a->selected < 0 || a->selected >= a->skill_n) && (a->team || a->cloned)) {  // 隊友與分身：同一個選招（0x4906c0），走技能鏈
        int k = chain_pick(a);
        if (k >= 0) actor_skill_select(a, k);
    }
    return a->selected >= 0 && a->selected < a->skill_n ? &a->skills[a->selected] : NULL;
}

// 0x491d00與主AI共用：用技能sk出手（命令8）並記住上一招、清選取。
// 原版manual==0的招式有一半機率不記上一招也不清選取（保留下一拍再出）。
static void npc_cast_command(Actor *a, ActorSkill *sk, int x, int y) {
    actor_command(a, 8, sk->id, x, y, 1, 0);
    const Method *m = method_find(sk->id);
    if (m && m->manual == 0 && rand() % 2) return;
    a->last_skill = sk->row;
    actor_skill_select(a, -1);
}

// 目標超出直線射程時是否要放棄：任一邊在高層（>9）不檢查，否則要直線過得去（0x443790，模式3）。
static int npc_line_blocked(const Actor *a, const Actor *t, int range) {
    return a->layer <= 9 && t->layer <= 9 &&
        !line_clear(actor_init_x(a), actor_init_y(a), actor_init_x(t), actor_init_y(t), range, 3);
}

// Poq.exe 0x491d00：+0x579（status_aux）狀態機。>2 = 追著目標打（每150～249拍續一次）；
// 2 = 找目標、試出手，打不到15～34拍再試；1 = 走回修正目的格（<3格就結束）；<1 = 不處理。回傳1表示本拍已處理。
static int npc_aux_ai(Actor *a) {
    int aux = a->status_aux;
    if (aux < 1) return 0;
    if (aux > 2) {
        if (a->direction_clock % 0x1e == 1) npc_pick_target(a, 0);
        Actor *t = actor_target(a);
        ActorSkill *sk;
        if (!t) { a->status_aux--; goto relook; }
        if (!(sk = npc_selected(a))) { a->status_aux--; actor_clear_target(a); goto relook; }
        int d = dist_of(actor_init_x(a), actor_init_y(a), actor_init_x(t), actor_init_y(t));
        if ((sk->max > 1 || sk->max < d) && npc_line_blocked(a, t, sk->max)) {
            a->status_aux--;
            actor_clear_target(a);
            goto relook;
        }
        a->status_aux = rand() % 100 + 150;
        npc_cast_command(a, sk, actor_init_x(t), actor_init_y(t));
        return 1;
    relook:;
        Actor *p = actor_near_begin(a, 0, actor_seek(a) - 2);
        if (p && (unsigned)(p->object_kind - 30) < 71u && actor_cooling(a)) {
            a->status_aux = 0;
            actor_target_set(a, p);
        }
        if (a->status_aux == 2) a->status_aux = 1;
        return 1;
    }
    if (aux != 2) {
        if (actor_command_idle(a) || (a->dynamic_state == 1 && (unsigned)(a->grp - 10000u) > 50000u)) {
            if (actor_dynamic_retry(a) &&
                dist_of(a->corrected_x, a->corrected_y, actor_init_x(a), actor_init_y(a)) < 3) a->status_aux = 0;
            int none = a->corrected_x == -1 && a->corrected_y == -1;
            if (!none && !(a->corrected_x == actor_init_x(a) && a->corrected_y == actor_init_y(a)))
                actor_command(a, 1, a->corrected_x, a->corrected_y, 0, 1, 0);
        }
        return 1;
    }
    Actor *t = actor_target(a);
    ActorSkill *sk = t ? npc_selected(a) : NULL;
    if (!t || !sk) {
        a->status_aux = 1;
        actor_clear_target(a);
        actor_cancel_command(a);
        return 1;
    }
    int d = dist_of(actor_init_x(a), actor_init_y(a), actor_init_x(t), actor_init_y(t));
    if ((sk->max >= 2 || sk->max < d) && npc_line_blocked(a, t, sk->max)) {
        a->status_aux = rand() % 20 + 15;
        actor_clear_target(a);
        actor_cancel_command(a);
        actor_dynamic_command(a, 1, 0, 0, 0);
        return 1;
    }
    a->status_aux = rand() % 100 + 150;
    npc_cast_command(a, sk, actor_init_x(t), actor_init_y(t));
    return 1;
}

// Poq.exe 0x4911b0：CNpc完整主AI（有技能鏈的戰鬥一拍）。回傳1表示本拍到此為止。
// 取目標→（NPC組）每20拍／（其他組）30或400拍重選→出生格距離與追擊界限→計攻擊間隔→選招→
// 太近退一步、到間隔且目標站著才出手（射程內直線過得去用命令9，否則走近到射程-1）。
// 打物件（目標種類21..29）要招式的 Reserve4（Method +0x36）& 4，否則清目標與命令；
// 連線客戶端（DAT_00942f28==2）分支單機沒有；vtable+0x48(1,1)的清除副作用因此也不做。
static int npc_main_ai(Actor *a) {
    Actor *t = actor_target(a);
    int npc = (unsigned)(a->grp - 10000u) <= 50000u;
    if (npc) {
        if (actor_command_idle(a) && a->dynamic_state == 1 && !main_of(a) && !t && a->direction_clock % 0x14 == 1) {
            npc_pick_target(a, 0);
            t = actor_target(a);
        }
    } else if (!t) {
        actor_cancel_command(a);
        if (a->direction_clock % 0x1e == 1) npc_pick_target(a, 0);
    } else {
        int m = a->direction_clock % 400;
        const NpcSpec *sp = a->spec;
        if (m == 1) npc_pick_target(a, 0);
        else if (m == 200 && sp && sp->rw != 0 &&
                 !(actor_init_x(a) >= sp->rx && actor_init_x(a) < sp->rx + sp->rw &&
                   actor_init_y(a) >= sp->ry && actor_init_y(a) < sp->ry + sp->rh)) {
            actor_command(a, 1, sp->rw / 2 + sp->rx, sp->rh / 2 + sp->ry, 0, 1, 0);
            actor_clear_target(a);
        }
    }
    if (a->skill_n == 0) { npc_move_away(a, t, 8); return 0; }
    a->npc_idle++;
    if (a->dynamic_active && a->grp - 10000u > 50000u) {
        if (!actor_command_idle(a) && a->dynamic_state != 1) {
            if (a->npc_idle % 0x50 == 0) {
                actor_cancel_command(a);
                actor_dynamic_command(a, 1, 0, 0, 0);
            }
        } else a->npc_idle = 0;
    }
    if (!t) return 0;
    int sx = actor_init_x(a), sy = actor_init_y(a), tx = actor_init_x(t), ty = actor_init_y(t);
    int manhattan = abs(sy - ty) + abs(sx - tx), giveup = 0;
    if (a->direction_clock % 300 == 10) {
        int home = !(a->corrected_x == 0 && a->corrected_y == 0) && !(a->corrected_x == -1 && a->corrected_y == -1);
        if (!home) {
            if (actor_seek(a) < manhattan) giveup = 1;
            else if (manhattan > 6) npc_formation(a);
        } else if ((unsigned)(a->grp - 10000u) > 50000u &&
                   actor_chase(a) < abs(sy - a->corrected_y) + abs(sx - a->corrected_x)) {
            a->status_aux = rand() % 100 + 150;
            actor_cancel_command(a);
            actor_clear_target(a);
            actor_dynamic_command(a, 1, 0, 0, 0);
            return 1;
        }
        if (!giveup && a->def->legs == 4) {
            Actor *p = actor_near_begin(a, 0, 2);
            if (p) actor_target_set(a, p);
        }
    } else if (a->direction_clock % 0x1e == 10 && (unsigned)(t->object_kind - 30) < 71u &&
               (t->grp == a->grp || has_status(a, 0x66))) giveup = 1;
    if (!giveup && has_status(a, 0x21) && dist_of(ax64(t), ay64(t), ax64(a), ay64(a)) > 0x60) giveup = 1;
    if (giveup) {
        actor_clear_target(a);
        actor_cancel_command(a);
        return 1;
    }
    // 攻擊間隔：不能出招或招式還在放時，歸零後重抽；能出招且沒有招式在放才遞減。
    if (!actor_can_use_id(a, -1) || main_of(a)) {
        if (a->npc_wait < 0) {
            int delay = a->def->atk_delay;
            if (npc) a->npc_wait = (int)((float)delay * actor_fight_ratio(a));
            else a->npc_wait = delay > 0 ? rand() % delay + delay : 0;
        }
    } else if (a->npc_wait >= 0) a->npc_wait--;
    if (actor_flags(a, 1)) {
        actor_clear_flags(a, 1);
        actor_clear_target(a);
        actor_cancel_command(a);
        return 0;
    }
    if (!actor_command_idle(a) || a->dynamic_state != 1 || !actor_can_use_id(a, -1) || a->skill_n < 1) return 0;
    ActorSkill *sk = a->selected >= 0 && a->selected < a->skill_n ? &a->skills[a->selected] : NULL;
    if (!sk) {
        if (!npc_selected(a)) npc_move_away(a, t, 8);
        return 0;
    }
    const Method *method = method_find(sk->id);
    if (method && (unsigned)(t->object_kind - 0x15) < 9u && !(method->r4 & 4)) {  // 0x4911b0
        actor_clear_target(a);
        actor_cancel_command(a);
        actor_dynamic_command(a, 1, 0, 0, 0);
        return 0;
    }
    int dist = dist_of(sx, sy, tx, ty);
    if (a->direction_clock % 0x3d == 2 && !(method && method->manual == 1)) {
        if (sk->max - dist - 2 > 1) {  // 比射程近太多：退一步（IsMaxRange必然成立）
            int fx, fy;
            npc_flee_pos(a, t, &fx, &fy, sk->max - dist - 2);
            actor_command(a, 1, fx, fy, 0, 0, 0);
        }
    } else if (a->npc_wait < 0 || t->dynamic_state != 1) {
        int range = sk->max;
        if (range < 2 || dist > range || a->layer > 9 || t->layer > 9 || line_clear(sx, sy, tx, ty, range, 3)) {
            actor_command(a, 9, sk->id, t->sh ? t->sh : (int)t->object_id, 0, 1, 0);
            if (method && method->manual == 0 && rand() % 2) return 0;
            a->last_skill = sk->row;
            actor_skill_select(a, -1);
        } else actor_command(a, 1, tx, ty, range - 1, 0, 0);
    }
    return 0;
}

// Poq.exe 0x492730：CNpc閒置AI。每100拍選目標（入戰就結束）；有關聯識別每60拍找人互動；
// 沒主人時依記錄邏輯：0站著（播兩個初始態序列，每5拍抽1次換）、1矩形閒逛、2路徑巡邏、3跟隨、
// 其他偶爾走回修正格；被互動（修正7）時面向對方；有主人（召喚）則每70拍走回主人身邊。
// 跟隨（logic 3）：ONT 類型 15 的鍵 a、s、d、x、y = SInitChaFollow +0x7c／+0x80（模式 0 走／1 跑）／+0x84（距離）／+0x88、+0x8c（連結的 SInit 座標，res.c 解析）；
// 有連結時目標 = 座標相符的 SInit 的角色（0x46a260），沒有連結才找附近同組（actor_near_begin 3,6）。
// shortcut: 模式0與非0原版只看得到命令2的參數，命令1的參數反編譯沒帶出，兩者都用（對象格、距離）；遠距主人消失（0x497d00）改清owner。
static void npc_idle_ai(Actor *a) {
    const NpcSpec *sp = a->spec;
    if (a->direction_clock % 100 == 1) {
        npc_pick_target(a, 0);
        if (npc_in_fight(a)) return;
    }
    if (a->search_link && a->direction_clock % 60 == 1) {
        Actor *p = actor_near_begin(a, 2, 10);
        if (p) actor_command(a, 15, p->sh ? p->sh : (int)p->object_id, 0, 0, 1, 0);
    }
    int cx = actor_init_x(a), cy = actor_init_y(a);
    if (!actor_owner(a) && sp) {
        int logic = sp->logic;
        if (a->modifier[7].value) {
            if (logic || a->def->speed) {
                Actor *p = actor_of(a->modifier[7].value);
                if (!p) actor_modifier_set(a, 7, 0, -1);
                else if (a->dynamic_active) {
                    if (a->dynamic_state == 1) actor_direction_set(a, dir_of(ax64(a), ay64(a), ax64(p), ay64(p), 8));
                    else actor_dynamic_command(a, 1, 0, 0, 0);
                }
            }
        } else if (logic == 0) {
            int idle = actor_command_idle(a);
            if (idle && a->dynamic_state == 1 && a->move_a != 0 &&
                (a->corrected_x != cx || a->corrected_y != cy) && rand() % 100 == 1)
                actor_command(a, 1, a->corrected_x, a->corrected_y, 0, 1, 0);
            if (idle && a->dynamic_state == 1) {
                const Seq *q = role_seq(a->role, a->body_seq);
                int s0 = sp->initial_state[0], s1 = sp->initial_state[1];
                if ((!q || actor_body_done(a, q)) && s0 < 13 && s1 < 13 &&
                    a->def->initial_seq[s0] != 65535 && a->def->initial_seq[s1] != 65535) {
                    int first = (uint16_t)(a->def->initial_seq[s0] + sp->dir);
                    int second = (short)a->def->initial_seq[s1] + sp->dir;
                    if ((a->body_ready ? a->body_seq : actor_seq(a)) == first) {
                        if (rand() % 5 == 1) actor_animation_select(a, second);
                    } else actor_animation_select(a, first);
                }
            }
        } else if (logic != 1 && logic != 2 && logic != 3) {
            if (actor_command_idle(a) && a->dynamic_state == 1 && rand() % 100 == 1 &&
                !(a->corrected_x == -1 && a->corrected_y == -1) && !(a->corrected_x == cx && a->corrected_y == cy))
                actor_command(a, 1, a->corrected_x, a->corrected_y, 0, 1, 0);
        } else if (a->dynamic_state == 1) {
            int rest = a->npc_rest_stop ? 4 : sp->rest;
            if (a->npc_timer > 0) a->npc_timer--;
            else if (rest == 4) a->npc_timer = 10000;
            else {
                if (rest != 0) a->npc_timer = rand() % (rest * 200 + 10);
                if (logic == 1) {
                    int x = cx - 5 + rand() % 10, y = cy - 5 + rand() % 10;
                    x = x <= sp->rx + sp->rw ? (x < sp->rx ? sp->rx : x) : sp->rx + sp->rw;
                    y = y <= sp->ry + sp->rh ? (y < sp->ry ? sp->ry : y) : sp->ry + sp->rh;
                    actor_command(a, 1, x, y, 0, 1, 0);
                } else if (logic == 2) {
                    if (sp->path_n > 0) {
                        int i = a->path_i % sp->path_n;
                        a->path_i = i + 1 >= sp->path_n ? 0 : i + 1;
                        actor_command(a, 1, sp->path[i][0], sp->path[i][1], 0, 1, 0);
                    }
                } else if (!a->follow_id) {
                    if (!sp->link) {
                        Actor *p = actor_near_begin(a, 3, 6);
                        if (p) { a->follow_id = p->sh ? p->sh : (int)p->object_id; a->npc_timer = 100; }
                    } else {  // 0x46a260 找座標相符的 SInit（種類 10~16），再由它找角色；找不到 → 休息值改 4（原地不動）
                        for (int i = 1; i < actor_count; i++) {
                            const NpcSpec *o = actors[i].spec;
                            if (o && o->ont && o->type >= 10 && o->type <= 16 && o->x == sp->link_x && o->y == sp->link_y && actors[i].role) {
                                a->follow_id = actors[i].sh ? actors[i].sh : (int)actors[i].object_id;
                                break;
                            }
                        }
                        if (!a->follow_id) a->npc_rest_stop = 1;
                    }
                } else {
                    Actor *p = actor_of(a->follow_id);
                    if (!p) { if (!sp->link) a->follow_id = 0; else a->npc_rest_stop = 1; }
                    else if (is_foe(a, p)) a->follow_id = 0;
                    else if (sp->fol_dist < dist_of(actor_init_x(p), actor_init_y(p), cx, cy))
                        actor_command(a, sp->fol_mode ? 2 : 1, actor_init_x(p), actor_init_y(p), sp->fol_dist, 1, 0);
                }
            }
        }
    }
    if (actor_owner(a) && !a->foe && a->direction_clock % 70 == 1) {
        if (!actor_owner(a)->role) actor_owner_set(a, NULL);
        else {
            int r = npc_in_fight(a) ? 2 : 4, ox = actor_init_x(actor_owner(a)), oy = actor_init_y(actor_owner(a));
            if (abs(ox - cx) > r || abs(oy - cy) > r) actor_command(a, 1, ox, oy, r, 1, 0);
        }
    }
}

// Poq.exe 0x492ed0：CNpc每拍主分派（動態／播放／高度的0x48c050已在主迴圈前置）。
// 順序：恐懼逃跑 → 命令空且動態可重試 → 重設延遲 → 守衛 → status_aux → 玩家組旗標0x10的跟隨 → 入戰走主AI，否則閒置AI。
// shortcut: 玩家組旗標0x10且1/200、目標超過321單位時原版呼叫招式物件虛擬函式+0x20（來源沒查到），省略。
static int npc_dispatch(Actor *a) {
    if (a->activity_disabled) return 0;
    int before = a->dynamic_state;
    if (npc_flee(a)) return 1;
    if (a->remembered[0] != -1 || !actor_dynamic_retry(a)) return 0;
    if (a->remember_delay >= 1) { a->remember_delay--; return 0; }
    if (grp_npc(a->grp) && before == 11 && a->dynamic_state != 11) npc_pick_target(a, 1);
    if (npc_guard(a) || npc_aux_ai(a)) return 1;
    if (!grp_npc(a->grp) && actor_flags(a, 0x10)) return 1;
    if (npc_in_fight(a)) npc_main_ai(a);
    else npc_idle_ai(a);
    return 1;
}

// 畫面上點到哪隻活著的怪物：0x442b10（點的格子周圍 5×5 格、點在方框裡、離方框中心最近）
// 點選與可見名單共用當幀紙娃娃矩形，包含比例、離地高度、影子及腳圈。
static SDL_Rect viewport_actor_rect(const Actor *a, int sx, int sy, Uint32 tick);
static Uint32 actor_draw_tick;
static int pick_ox, pick_oy;
static const Scene *pick_s;
static void pick_box(const Actor *a, int *l, int *t, int *w, int *h) {
    int shift = a->view_link_type == 2 || a->view_link_type == 3 ? 128 : 0;
    int fx = scene_px(pick_s, (int)(a->x * 64) + shift, (int)(a->y * 64) + shift) - pick_ox;
    int fy = scene_py(pick_s, (int)(a->x * 64) + shift, (int)(a->y * 64) + shift) - pick_oy;
    SDL_Rect r = viewport_actor_rect(a, fx, fy, actor_draw_tick);
    *l = r.x; *t = r.y; *w = r.w; *h = r.h;
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

// 顯示用簡體 → 繁體（使用者 2026-11-20：文字儲存的遊戲提示改繁體；圖片裡的字不動）。所有畫字、量字寬都經過這裡，
// 遊戲邏輯比對用的字串不受影響（只在解碼成要畫的字時轉）。text_trad = 0 關閉
#include "s2t_table.h"
static int text_trad = 1;
static uint32_t s2t_char(uint32_t c) {
    if (!text_trad || c < 0x4E00 || c > 0x9FFF) return c;
    int lo = 0, hi = S2T_N - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (S2T[m][0] == c) return S2T[m][1];
        if (S2T[m][0] < c) lo = m + 1; else hi = m - 1;
    }
    return c;
}
static uint32_t utf8_raw(const char **p) {
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = *s++;
    if (c >= 0xE0 && s[0] && s[1]) { c = (c & 0x0F) << 12 | (s[0] & 0x3F) << 6 | (s[1] & 0x3F); s += 2; }
    else if (c >= 0xC0 && s[0]) { c = (c & 0x1F) << 6 | (s[0] & 0x3F); s += 1; }
    *p = (const char *)s;
    return c;
}
static uint32_t utf8_next(const char **p) { return s2t_char(utf8_raw(p)); }  // 顯示用（轉繁體）；比對／轉碼用 utf8_raw

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

#ifdef STORY
// UTF-8 → GBK（劇情檔的字串是 UTF-8，原版資料表是 GBK）；查表找不到的字變 '?'
static void utf8_gbk(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *q = in; *q && o + 3 < n;) {
        uint32_t c = utf8_raw(&q);
        if (c < 0x80) { out[o++] = (char)c; continue; }
        int k = 0;
        while (k < 126 * 191 && gbk_uni[k] != c) k++;
        if (k == 126 * 191) { out[o++] = '?'; continue; }
        out[o++] = (char)(0x81 + k / 191);
        out[o++] = (char)(0x40 + k % 191);
    }
    out[o] = 0;
}
#endif

// 文字放大倍率（直式手機畫面縮小時，過場對話框用來維持可讀；平常 1）
static float text_zoom = 1;
static int ui_portrait;  // 直式手機：對話框做窄一點，放大到下方時字才夠大（touch_layout 後更新）
static float screen_zoom = 1;  // 邏輯畫面寬 / 實際視窗寬（> 1 = 遊戲被縮小顯示）
// 一行字的寬度（置中用）
static int text_width(const char *t) {
    int w = 0;
    for (const char *q = t; *q;) { uint32_t c = utf8_next(&q); glyph(c); w += (int)(glyph_adv[c <= 0xFFFF ? c : '?'] * text_zoom + 0.5f); }
    return w;
}

// 行距 16：原版 m_pDescribe 框高 84 要放 5 行。從 (x, y) 開始寫，超過 maxw 換行；draw = 0 只量高度。回傳用掉的高度
static int draw_text(int x, int y, const char *t, int maxw, Uint32 rgb, int draw) {
    int cx = 0, cy = 0, step = (int)(16 * text_zoom + 0.5f);
    while (*t) {
        uint32_t c = utf8_next(&t);
        if (c == '\n') { cx = 0; cy += step; continue; }
        SDL_Texture *g = glyph(c);
        if (!g) return 0;
        int adv = (int)(glyph_adv[c] * text_zoom + 0.5f);
        if (cx + adv > maxw) { cx = 0; cy += step; }
        if (draw) {
            SDL_SetTextureColorMod(g, rgb >> 16, rgb >> 8 & 0xFF, rgb & 0xFF);
            SDL_Rect r = {x + cx, y + cy, step, step};
            SDL_RenderCopy(ren, g, NULL, &r);
        }
        cx += adv;
    }
    return cy + step;
}

// 載入畫面小提示（CLoadingDialog 0x502450）：rand()&3 選一組（UI.Dlg 起點／筆數 2500×9、2600×22、2700×13、2800×10），組內再 rand 一筆。
// 文字裡的「&1xxxxxxxxxx」是原版顏色／名稱標記，連同前面的括號拿掉
static void loading_tip(char *out, size_t cap) {
    static const int base[4] = {2500, 2600, 2700, 2800}, cnt[4] = {9, 22, 13, 10};
    int k = rand() & 3;
    char t[512];
    ui_text(base[k] + rand() % cnt[k], t, sizeof t);
    size_t n = 0;
    for (const char *p = t; *p && n + 4 < cap;) {
        if (p[0] == '&' && p[1] == '1' && strspn(p + 2, "0123456789") >= 10) {
            p += 12;
            if (n >= 3 && !memcmp(out + n - 3, "\xef\xbc\x88", 3) && !memcmp(p, "\xef\xbc\x89", 3)) { n -= 3; p += 3; }
            continue;
        }
        out[n++] = *p++;
    }
    out[n] = 0;
}

// Poq.exe 0x48b710：單機模式2；普通文字覆寫槽0，上飄用第一空槽，滿時用第一上飄槽。
// shortcut: 以UTF-8緩衝和現有字型取代200x100的SBMP，超過1023 bytes截斷；需要超長台詞時改動態字串。
static void actor_overhead(Actor *a, const char *text, unsigned char duration, unsigned char floating) {
    if (!text || !*text) return;
    int slot = 0;
    if (floating == 1) {
        int first = -1;
        for (; slot < 8; slot++) {
            if (!a->overhead[slot].left) break;
            if (a->overhead[slot].floating == 1 && first == -1) first = slot;
        }
        if (slot == 8) slot = first;
    }
    // 正常狀態最多只有槽0是普通文字，故滿槽必有上飄槽；損壞資料不越界寫入。
    if (slot < 0) return;
    a->overhead[slot].left = a->overhead[slot].total = duration;
    a->overhead[slot].floating = floating;
    size_t n = strlen(text);
    if (n >= sizeof a->overhead[slot].text) {
        n = sizeof a->overhead[slot].text - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
    }
    memcpy(a->overhead[slot].text, text, n);
    a->overhead[slot].text[n] = 0;
}

// Poq.exe 0x48b6c0：原版lstrlenA以GBK bytes計時；移植後字串是UTF-8，先換算原版長度。
static void actor_say(Actor *a, const char *text) {
    if (!text) return;
    size_t bytes = 0;
    for (const char *p = text; *p; ) bytes += utf8_next(&p) < 128 ? 1 : 2;
    actor_overhead(a, text, bytes > 23 ? 255 : (unsigned char)(bytes * 10 + 16), 0);
}

// Poq.exe 0x4719a0：場景存在才掃編號，種類30..100包含已死角色，第一筆命中即停止。
static void scene_character_say(int id, const char *text) {
    if (!unit_scene) return;
    for (int i=0; i<actor_count; i++) {
        Actor *a=&actors[i];
        if (a->sh == id && (unsigned)(a->object_kind - 30) <= 70u) {
            actor_say(a, text);
            return;
        }
    }
}

// 劇情訊息的場景名與台詞分詞；空白在台詞中以byte14編碼。
static void scene_character_say_message(int id, const char *message) {
    const char *p = message + strcspn(message, " ");
    p += strspn(p, " ");
    char text[256];
    size_t n = strcspn(p, " ");
    if (n >= sizeof text) n = sizeof text - 1;
    memcpy(text, p, n); text[n] = 0;
    for (char *q = text; *q; q++) if (*q == 14) *q = ' ';
    scene_character_say(id, text);
}

// 角色繪圖的文字槽分支：先扣倒數；前／後八畫面淡入淡出，中段上飄再扣一次。
// 不由固定拍更新；畫面未繪到的角色不扣時間。回傳原版0..8透明度供SDL字型承接。
static int actor_overhead_frame(Actor *a, int slot) {
    __typeof__(a->overhead[0]) *t = &a->overhead[slot];
    if (!t->left) { t->total = t->floating = 0; return -1; }
    unsigned left = --t->left;
    if ((int)t->total - 8 < (int)left) return t->total - left;
    if (left < 8) return (int)left;
    if (t->floating == 1) --t->left;
    return 8;
}

static void actor_overhead_draw(Actor *a, int sx, int sy) {
    for (int i = 0; i < 8; i++) {
        int alpha = actor_overhead_frame(a, i);
        if (alpha < 0) continue;
        __typeof__(a->overhead[0]) *t = &a->overhead[i];
        if (t->floating > 1) continue;
        int y = sy - a->head_height + (t->floating ? -80 + t->left : -60);
        // shortcut: 原版SBMP的描邊／調色盤色號未解；沿用白字，待文字渲染基礎補齊後校色。
        // draw_text使用共用glyph，逐字設定alpha並在此還原，不污染介面字型。
        for (const char *p = t->text; *p; ) {
            SDL_Texture *g = glyph(utf8_next(&p));
            if (g) SDL_SetTextureAlphaMod(g, (Uint8)(alpha * 255 / 8));
        }
        draw_text(sx - 100, y, t->text, 200, 0xffffff, 1);
        for (const char *p = t->text; *p; ) {
            SDL_Texture *g = glyph(utf8_next(&p));
            if (g) SDL_SetTextureAlphaMod(g, 255);
        }
    }
}



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
        // 0x400000 沒有 0x8 的按鈕一開始隱藏（換隊友的向前／向后、修理全部、战斗／跑步切換的战斗等）。shortcut: 從各介面的旗標歸納，原版建立控制項那段沒查
        if (c->type == 2 && c->flags & 0x400000 && !(c->flags & 8)) c->flags &= ~9;
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
static void ui_bar_ex(const UiCtrl *c, int ox, int oy, int value, int max, int from_right, int backdrop) {
    const Sprite *s = ui_image(c->img, 0);
    if (!s || max <= 0) return;
    int v = value < 0 ? 0 : value > max ? max : value;
    if (backdrop) {  // 0x5972f0：旗標 0x10 沒設 → 虛擬 +0x20(0) 選第 0 組動作（底圖）畫整張，再 +0x20(1) 選第 1 組動作（填色）。+0x20 = 設動作序號（0x40443f 寫圖組 +0xc、幀歸零），不是暗化
        SDL_Rect all = {ox + c->x, oy + c->y, s->w, s->h};
        SDL_RenderCopy(ren, s->tex, NULL, &all);
        const Anim *an = anim_get((uint32_t)c->img);
        if (an && an->count >= 2 && an->seqs[1].count) { const Sprite *f = ui_image(c->img, an->seqs[0].count); if (f) s = f; }
    }
    SDL_Rect src = {0, 0, s->w, s->h};
    if (c->flags & 0x4000) { src.y = s->h - s->h * v / max; src.h = s->h - src.y; }
    else { src.w = s->w * v / max; if (from_right) src.x = s->w - src.w; }
    SDL_Rect dst = {ox + c->x + src.x, oy + c->y + src.y, src.w, src.h};
    SDL_RenderCopy(ren, s->tex, &src, &dst);
}
static void ui_bar(const UiCtrl *c, int ox, int oy, int value, int max) { ui_bar_ex(c, ox, oy, value, max, 0, 0); }

// 畫整個介面（進度條另外畫）；(ox, oy) = 介面左上角
static void ui_draw(UiDialog *d, int ox, int oy) {
    for (int i = 0; i < d->count; i++) {
        const UiCtrl *c = &d->c[i];
        if (c->type == 6 || c->type == 10 || !c->img || !(c->flags & 9)) continue;  // 種類 10（文字欄）的圖不是底圖（4326 是勾勾）。shortcut: 旗標 0x1、0x8 都沒有當成「一開始隱藏」（升級提示、選取框、成套裝備欄都是），還沒對原版
        draw(ui_image(c->img, 0), ox + c->x, oy + c->y);
    }
}

static int dialog_open;  // 劇情對話開著：原版 0x565330 把主列、隊伍、小地圖都藏起來，關了再顯示

// 對話視窗 JM108（Poq.exe CChooseDialog）：0x5655a0 畫說話者（[0名字]）與台詞、0x5650a0 每幀顯示／隱藏、0x5651f0 關閉（對話結束）、0x565b90 設台詞（story_dialog.text）、
// 0x565c10 設旗標、0x565c50 關閉並讓 HUD 回來、0x565c70 收到說話者與台詞並開窗（story_talk 後 dialog_open = 1）、0x564bf0 取消鈕＝關。
// 高度：底圖 342 固定 600×150，說話者 m_Title (27,7)、
// 台詞 m_Text (28,26) 寬 439 高 72、「Choose：」m_Title2 (27,100)；台詞或選項放不下時往上加高（shortcut: 原版用捲軸，這裡直接長高）
static int choose_height(int text_h, int opt_h) {
    int h = 26 + (text_h > 72 ? text_h : 72) + 8;
    if (opt_h > 0) h += 16 + opt_h + 4;
    return h > 150 ? h : 150;
}

#ifdef STORY
static int opt_y0[STORY_MAX_OPTIONS], opt_y1[STORY_MAX_OPTIONS];
static UiDialog choose_ui;
static int choose_loaded;

// 對話：JM108 擺在畫面底部置中（原版 0x565330 擺位 mode 2），上方空出主列位置（HUD 這時是藏起來的）
// 對話框（手機版自訂外觀，2026-11-20 使用者：原版 JM108 圖框在我們的排版下文字溢出、選項太小點不到）：
// 底部滿寬的深色面板，說話者黃字、台詞自動換行、每個選項一整列大按鈕（高 ≥ 30，整列可點）。直式時整塊由 modal_rect 放大到下方。
// 原版 JM108 的底圖與固定座標沒用（shortcut）；對話內容、選項順序、選了之後的流程都照原版（story_choose）
static SDL_Rect dialog_box(void) {
    const StoryDialog *d = &story_dialog;
    int w = ui_portrait ? 400 : vw - 24, tw = w - 24;
    int th = d->text[0] ? draw_text(0, 0, d->text, tw, 0, 0) : 0, oh = 0;
    for (int i = 0; i < d->option_count; i++) { int h = draw_text(0, 0, d->options[i].text, tw - 24, 0, 0); oh += (h < 22 ? 22 : h) + 12; }
    int h = 12 + 18 + (th ? th + 8 : 0) + oh + 8;
    if (h < 200) h = 200;  // 固定最小高度：台詞長短不同也不會忽大忽小（內容超過才加高）
    if (h > vh - 130) h = vh - 130;
    return (SDL_Rect){(vw - w) / 2, vh - h - 8, w, h};
}
static void dialog_draw(void) {
    if (!dialog_open) return;
    const StoryDialog *d = &story_dialog;
    SDL_Rect r = dialog_box();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 10, 12, 10, 235);
    SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawColor(ren, 150, 130, 80, 255);
    SDL_RenderDrawRect(ren, &r);
    int tw = r.w - 24, y = r.y + 8;
    draw_text(r.x + 12, y, d->speaker, 400, 0xFFFF00, 1);
    y += 20;
    if (d->text[0]) y += draw_text(r.x + 12, y, d->text, tw, 0xBECDCE, 1) + 8;
    for (int i = 0; i < d->option_count; i++) {
        int h = draw_text(0, 0, d->options[i].text, tw - 24, 0, 0);
        if (h < 22) h = 22;
        opt_y0[i] = y; opt_y1[i] = y + h + 10;
        SDL_Rect b = {r.x + 8, y, r.w - 16, h + 10};
        SDL_SetRenderDrawColor(ren, 40, 52, 66, 255);
        SDL_RenderFillRect(ren, &b);
        SDL_SetRenderDrawColor(ren, 110, 150, 190, 255);
        SDL_RenderDrawRect(ren, &b);
        draw_text(r.x + 20, y + 5, "\xe2\x96\xb6", 20, 0x80E0FF, 1);  // ▶
        draw_text(r.x + 40, y + 5, d->options[i].text, tw - 24, 0xE0F4FF, 1);
        y += h + 12;
    }
}

// 對話中點畫面：點到選項就選（0x565c20 選項回應）；沒有選項時點哪裡都關掉（0x564bb0 單機確定鈕直接關閉）
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

// 主畫面介面，位置照原版（Poq.exe 0x5d3xxx 建立介面那段）：
// 底部主列 JM51_状态界面_中间 = 底部置中；隊伍面板 JM51_状态界面_主控 = 右緣、貼在主列上方
static UiDialog bar, party, enemy;

// 視窗：原版 人物属性（JM52）開在畫面中線左邊（x = W/2 − 寬），技能（JM53）、道具（JM54）開在中線右邊（x = W/2）；
// y 都是垂直置中再往上 50（Poq.exe 0x4ff3xx、0x5004xx、0x4feexx）。左右各一個，可以同時開
typedef struct { UiDialog d; int open; } Win;
static Win win_left, win_right;
static SDL_Texture *modal_tex;  // 直式放大顯示的居中視窗（地圖、系統選單、對話框…）畫在這張，不再同時留在遊戲畫面上（2026-11-21：以前上面一份、下面放大一份）
static int modal_on, modal_cleared;
static int modal_rect(SDL_Rect *r);
static void modal_begin(void);
static void modal_end(void);
static SDL_Texture *win_tex;  // 直式觸控：側邊視窗畫在這張（800x600 同座標），再放大貼到遊戲畫面下方（touch_ui.c）
static int win_off;       // 1 = 視窗現在畫在 win_tex，不在遊戲畫面上（遊戲畫面上的點擊不看視窗）

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

// 加屬性點（EasyRPG.dll 0x1003bb10，文字指令 c_extrause 0x10013e40 也走這）：還有剩餘點數才加，第 i 項（力、體、敏、悟、魅、智）+n、剩餘點數 −n（不夠補 0）、重算數值
static void hero_attr_points(int i, int n) {
    if (hero_extra_exp <= 0 || i < 0 || i > 5) return;
    hero_attr[i] += n;
    hero_extra_exp -= n;
    if (hero_extra_exp < 0) hero_extra_exp = 0;
    hero_derive(&actors[0]);
}

// 加點按鈕 m_pOK[i]（Poq.exe 0x553b00）：送 0x200 訊息讓第 i 項屬性 +1（hero_attr_points），再照目前模式重整（點數用完就回顯示模式）。
// shortcut: 原版按住會越加越快（間隔 400 → 50 毫秒），這裡一下一點
static void role_add_point(int i) {
    if (hero_extra_exp <= 0 || i < 0 || i > 5) return;
    hero_attr_points(i, 1);
    role_set_mode(role_mode);
}

static void win_toggle(Win *w, const char *section) {
    if (w->open && !strcmp(w->d.name, section)) { w->open = 0; return; }
    w->open = ui_load(section, &w->d) > 0;
    if (w->open && !strcmp(section, JM52)) role_set_mode(1);  // 開人物屬性是顯示模式（Poq.exe 0x554080）
}

// 原版各視窗的擺位函式（FUN_00583790 型 6 置中後；左邊的還有 0x552ab0、0x557880、0x557e20、0x56b540、0x5422e0（打造），
// 右邊的還有 0x55e880，地圖 0x53cee0 置中不貼邊）：左邊 x = W/2 − 寬（人物屬性 0x554150、裝備 0x556200、交易 0x55a5e0），
// 右邊 x = W/2（技能 0x560380、道具 0x54e340）；y = H/2 − 高/2 − 50
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
    SkillRow *r = &hero_skill[k];
    actor_skill_update(&actors[0], k + 1, skill_attack_id(r),
        (int)skill_field(r, 12, skill_value(r->range_min, r->range_min_inc, r->level)),
        skill_range_max(r), 0, 0);
    actor_skill_enable(&actors[0], k + 1, r->level > 0, 0);
    hero_extra_skill--;
}

static void skills_load(void) {
    if (hero_skill_n < 0) hero_skill_n = skill_rows(hero_prof, hero_skill, 20);
}

// 主列的右手、左手技能（原版 SkillData 的 CurRSkillID、CurLSkillID）：hero_skill 的第幾個，-1 = 空。
// 開局哪手拿哪招：原版技能物件的 UI_Cur 位元（1 右手、2 左手）在加技能時（Poq 0x485f80 的 hand 低兩位，封包 0x524 由 EasyRPG.dll 0x10041130 送出）
// 決定。story/skill/主角/*技能/ 裡只有各職業第一招 UI_Cur = 3（兩手），其餘 0；所以開局兩手都是那一招，不是猜的。
static int cur_skill[2];
static void hero_default_hands(void) {
    static const char *const UI_CUR[5] = {"\xd3\xce\xcf\xc0_\xd5\xb6\xbc\xac\xc5\xfb\xbe\xa3", "\xc1\xa6\xca\xbf_\xbb\xa2\xd7\xa6\xc0\xc7\xd1\xc0", "\xb4\xcc\xbf\xcd_\xc1\xf7\xd0\xc7\xbc\xfd\xd3\xb0", "\xca\xf5\xca\xbf_\xc5\xad\xbd\xa3\xbf\xaa\xc9\xbd", "\xce\xd7\xca\xa6_\xba\xe1\xc9\xa8\xc7\xa7\xbe\xfc"};  // GBK：游侠_斩棘披荆、力士_虎爪狼牙、刺客_流星箭影、术士_怒剑开山、巫师_横扫千军
    skills_load();
    cur_skill[0] = cur_skill[1] = -1;
    for (int i = 0; i < hero_skill_n && hero_prof >= 1 && hero_prof <= 5; i++)
        if (!strcmp(hero_skill[i].key, UI_CUR[hero_prof - 1])) cur_skill[0] = cur_skill[1] = i;
}

// 原版鍵盤分派的五組索引：Alt單獨按不換組；Ctrl+Alt優先於Shift。
static int skill_key_index(SDL_Keycode key, SDL_Keymod mod) {
    if (key < SDLK_1 || key > SDLK_9) return -1;
    int base = (mod & KMOD_CTRL) && (mod & KMOD_ALT) ? 36 :
        (mod & KMOD_SHIFT) && (mod & KMOD_ALT) ? 27 :
        mod & KMOD_SHIFT ? 18 : mod & KMOD_CTRL ? 9 : 0;
    return base + (int)(key - SDLK_1);
}

static void hero_selected_skill_apply(Actor *h) {
    // shortcut: Actor.atk兼作正在播放的招式；完整角色施法入口接入時拆出播放快照。
    // 原版只改SkillUnit選取，當前MethodMain繼續原招；單機等動作結束再綁下一招。
    if (h->act || h->casting) { h->selection_pending = 1; return; }
    h->selection_pending = 0;
    h->skill = NULL;
    h->atk = NULL;
    if (h->selected < 0 || h->selected >= h->skill_n) return;
    int id = h->skills[h->selected].id;
    // 原模板項沒有EasyRPG技能列；按招式ID接結算資料。
    for (int i = 0; i < hero_skill_n; i++)
        if (skill_attack_id(&hero_skill[i]) == id) { h->skill = &hero_skill[i]; break; }
    h->atk = method_find(id);
}

// 點敵人用右手技能的招式（技能表 Attack ID）。shortcut: 手機點一下對應哪一手是自訂的
static void hero_skill_apply(Actor *h) {
    skills_load();
    hero_hand_update();
    int k = cur_skill[0];
    actor_skills_rebuild(h);
    // shortcut: 玩家技能列用index+1作外部識別，保留0給原模板；完整技能物件接入後換回句柄。
    for (int i = 0; i < hero_skill_n; i++) actor_skill_append(h, &hero_skill[i], i + 1);
    actor_right_skill(h, k >= 0 && k < hero_skill_n ? skill_attack_id(&hero_skill[k]) : -1);
    int left = cur_skill[1];
    actor_left_skill(h, left >= 0 && left < hero_skill_n ? skill_attack_id(&hero_skill[left]) : -1);
    hero_selected_skill_apply(h);
}

// 放左手技能：技能等級 > 0、精气夠、武器合、沒有「攻击不能」的狀態，招式開始才扣精气。
// 要對著敵人的技能要有目標；不在作用範圍最大內就先走過去（pursue），到了再放
static void cast_start(Actor *h, const SkillRow *r) {
    const Method *m = method_find(skill_attack_id(r));
    if (h->act || h->casting || !m || !actor_row_enabled(h, r) || !actor_can_use_id(h, skill_attack_id(r)) || !skill_usable(h, r) || status_noattack(h)) return;
    h->mp -= skill_mpu(r);
    if (h == &actors[0]) arrow_spend(h, r);
    h->cast = r;
    h->casting = 1;
    h->atk_rh = h->atk;
    h->atk = m;
    start_act(h, ACT_ATTACK, m->seq);
}

// 共用現有技能結算與動畫，明確傳遞世界座標及對象至MethodMain。
// shortcut: 沒有EasyRPG技能列的模板招式仍可播放與觸發，沒有額外技能數值結算。
static int actor_cast_try(Actor *a, int id, int x, int y, Actor *target) {
    int i = actor_skill_find(a, id);
    if (i < 0) return 0;
    ActorSkill *s = &a->skills[i];
    const Method *m = method_find(s->id);
    if (!m || !s->enabled || (a->def->speed && a->move_a <= 0 && s->max >= 3)) return 0;
    // 原版先選明確傳入的角色目標；MethodMain建立失敗也保留此副作用。
    // 地點施法稍後找占格供同隊技能檢查，不在這裡改角色目標。
    if (target && !is_trap(target) && (unsigned)(target->object_kind - 30) < 71u)
        actor_target_set(a, target);
    if (!target) target = cell_actor0(x / 64, y / 64);
    if (m->manual == 2) {
        if (!target || a->grp != target->grp) return 0;
    }
    if (unit_n == MAX_UNIT) return 0;
    const SkillRow *r = s->row;
    if (!r && a == &actors[0])
        for (int k = 0; k < hero_skill_n; k++)
            if (skill_attack_id(&hero_skill[k]) == s->id) { r = &hero_skill[k]; break; }
    if (r && !a->mon && !skill_usable(a, r)) return 0;
    MUnit *old = main_of(a);
    const Method *right = a->casting ? a->atk_rh : a->atk;
    if (old) unit_die(old, SDL_GetTicks());
    else act_end(a);
    if (r && !a->mon) a->mp -= skill_mpu(r);
    if (r && a == &actors[0]) arrow_spend(a, r);
    a->act = ACT_NONE; a->atk = m;
    a->cast = r; a->casting = 1;
    start_act(a, ACT_ATTACK, m->seq);
    a->atk_rh = right;
    a->aim_valid = 1; a->aim_x = x; a->aim_y = y;
    a->aim_actor = target; a->aim_range = actor_skill_range(a, s);
    main_new(a);
    return 1;
}

// Poq.exe 0x48ffe0：非強制冷卻直接返回；成功建立招式後才送入戰命令。
// 單機模式2省略同步封包；原版NPC敵對入戰通知為空函數，沒有額外副作用。
static int actor_cast_at(Actor *a, int id, int x, int y, Actor *target, int forced) {
    if (!forced && actor_cooling(a)) return 0;
    int result = actor_cast_try(a, id, x, y, target);
    if (result && !has_status(a, 0)) actor_command(a, 11, 1, 0, 0, 0, 0);
    if (a->remembered[0] == 8 || a->remembered[0] == 9) {
        Actor *t = a->remembered[1] == id ? NULL : actor_target(a);
        if (a->remembered[1] == id || !t || is_trap(t) ||
            (unsigned)(t->object_kind - 30) >= 71u || !is_foe(a, t))
            actor_forget_command(a);
    }
    return result;
}

// ---- CTrapBase 陷阱物件（Poq.exe 0x49b8d0～0x49c710，種類 24）----
// 0x49c410 設狀態：1 = 觸發動畫（發現、播音效 0x3c），2 = 冷卻 500 拍，0 = 回待命。序列跟著狀態（actor_requested_seq）。
// shortcut: 音效 0x3c 原版有位置（0x4804a0 帶陷阱座標），audio_sfx 沒有位置音量，全音量播。
static void trap_set_state(Actor *t, int state) {
    t->trap_state = state;
    if (state == 1) { t->trap_revealed = 1; audio_sfx(0x3c); }
    else if (state == 2) t->trap_timer = 500;
}

// 0x49c580 放招：連鎖範圍 > 1 時往 8 方向挑最遠的格子（0x443230），格子中心當目標點；成功放出（0x43acb0）才進狀態 1、計數 100。
// shortcut: 原版範圍來自陷阱模型 +0x3e，這裡用技能範圍。
static void trap_fire(Actor *t) {
    t->trap_fired = 1;
    const SkillRow *r = npc_skill(t->mon->skill[0]);
    if (!r) return;
    int x = (int)t->x, y = (int)t->y, range = skill_range_max(r);
    if (range > 1) open_dir(&x, &y, x, y, range);
    if (x == -1) return;
    t->skill = r;
    if (actor_cast_at(t, skill_attack_id(r), x * 64 + 32, y * 64 + 32, NULL, 1)) {
        trap_set_state(t, 1);
        t->trap_timer = 100;
    }
}

// 0x49bef0 每拍（單機 = 伺服端）。狀態 0：每 30 拍看 128 單位內有沒有別組的人（組別 = 主人的組，0x49be00），
// 有就每拍看 30 單位內是不是有別組的人、有就放招。狀態 1：動畫播完 → 冷卻。狀態 2：500 拍後次數 −1，用完移除（0x49c710），否則回待命。
// shortcut: 格子上有建築（+0xe）時原版依建築種類可能不查 30 單位就放招，建築還不是物件、一律照有人才放；
// 滑鼠指著暫停動畫手機沒有指著，不做；次數（模型 +0x32）沒找到來源，一律 1 次。
static void trap_tick(Actor *t) {
    if (t->trap_state == 0) {
        if (t->trap_fired) return;
        int group = trap_owner_group(t);
        if (t->trap_timer++ % 30 == 0) {
            t->trap_trig = 0;
            near_begin(ax64(t), ay64(t), 0x80);
            for (Actor *e = near_next(); e; e = near_next())
                if (e->grp != group) { t->trap_trig = 1; break; }
        }
        if (!t->trap_trig) return;
        near_begin(ax64(t), ay64(t), 0x1e);
        for (Actor *e = near_next(); e; e = near_next())
            if (e->grp != group && e != t) { trap_fire(t); return; }
    } else if (t->trap_state == 1) {
        if (t->body_seq == 1 && t->act_done) trap_set_state(t, 2);
    } else if (--t->trap_timer == 0) {
        if (--t->trap_uses < 1) { t->role = NULL; t->object_dead = 1; return; }  // 0x49c710 移除
        t->trap_state = 0;
        t->trap_fired = t->trap_trig = 0;
    }
}

// Poq.exe 0x4861e0：第一個正ID且查得到SkillUnit的候選即停止，施法失敗也不試後面的。
static void actor_candidate_cast(Actor *a) {
    for (int i = 0; i < 4; i++) {
        int id = a->candidate[i];
        if (id <= 0 || actor_skill_find(a, id) < 0) continue;
        Actor *t = actor_target(a);
        actor_cast_at(a, id, ax64(t ? t : a), ay64(t ? t : a), t, 1);
        return;
    }
}

// 舊AI的入戰呼叫轉入原版狀態0通知；重複入戰只延長現有退戰計時。
static void actor_enter_fight(Actor *a) {
    a->fight = FIGHT_TICKS;
    actor_status_notify(a, 0, 1, -1);
}

// Poq.exe 0x485b80：玩家組且模板思想1，沿鏈找manual3技能；目的地含六圈找空格。
// 原版怪現象：整數距離>2即回成功，即使技能可用性失敗或施法入口拒絕也不再走路。
static int actor_move_cast(Actor *a, unsigned short cx, unsigned short cy) {
    if ((unsigned)(a->grp - 10000u) <= 50000u || a->def->move_cast != 1) return 0;
    for (int i = 0; i < a->skill_n; i++) {
        ActorSkill *s = &a->skills[i];
        if (a->def->speed && a->speed <= 0 && s->max >= 3) continue;
        const Method *m = s->enabled ? method_find(s->id) : NULL;
        if (!m || m->manual != 3) continue;
        int x = cx, y = cy;
        if (!vacant_cell(&x, &y, a->layer, 6)) continue;
        int dx = (int)a->x - x, dy = (int)a->y - y;
        if ((int)sqrt((double)dx * dx + (double)dy * dy) <= 2) continue;
        int slot = actor_skill_find(a, s->id);
        if (actor_can_use_skill(a, slot >= 0 ? &a->skills[slot] : NULL)) {
            a->moving = a->has_target = 0;
            actor_cast_at(a, s->id, x * 64 + 32, y * 64 + 32, NULL, 1);
        }
        return 1;
    }
    return 0;
}

// Poq.exe 0x48d2f0：四欄逐一保存；不清記住命令、不啟動技能。
static void actor_queue_command(Actor *a, int command, int p0, int p1, int p2) {
    a->command[0] = command; a->command[1] = p0;
    a->command[2] = p1; a->command[3] = p2;
}

static int actor_command_equal(const Actor *a, int command, int p0, int p1, int p2) {
    return a->command[0] == command && a->command[1] == p0 &&
        a->command[2] == p1 && a->command[3] == p2;
}

// Poq.exe 0x48a700：單機同時承接原版場景服務端；記住與目標副作用先於一般可用性。
// 陷阱沿用Actor承接種類24，不經角色選目標的敵對通知。
static int actor_command(Actor *a, int command, int p0, int p1, int p2, int cancel, int remember) {
    if (has_status(a, 2) || has_status(a, 10)) return 0;
    int i = actor_skill_find(a, -1);
    const ActorSkill *skill = i >= 0 ? &a->skills[i] : NULL;
    MUnit *u = main_of(a);
    switch (command) {
    case 1: case 2:
        if (remember) {
            a->remembered[0] = command; a->remembered[1] = p0;
            a->remembered[2] = p1; a->remembered[3] = p2;
        }
        break;
    case 4:
        if (p1 & 1) actor_right_skill(a, p0);
        if (p1 & 2) actor_left_skill(a, p0);
        break;
    case 8: case 9:
        if (remember) {
            Actor *target = command == 9 ? actor_of(p1) : cell_occupant(p1, p2);
            if (target) {
                if (command == 8 || (!is_trap(target) && (unsigned)(target->object_kind - 30) < 71u))
                    actor_target_set(a, target);
                else if (target->object_kind == 24 || is_trap(target)) a->foe = target;
            }
            a->remembered[0] = command; a->remembered[1] = p0;
            a->remembered[2] = p1; a->remembered[3] = p2;
        }
        break;
    case 11:
        if (actor_can_use_skill(a, skill)) {
            if (p0 && remember) a->battle_return = 0;
            actor_status_notify(a, 0, p0, -1);
        }
        break;
    case 13:
        if (a->dynamic_state == 1 && (actor_can_move(a) || (u && u->m && u->m->interruptible))) {
            actor_direction_set(a, p0);
            return 1;
        }
        break;
    case 14:
        if (u && u->m && u->m->interruptible) u->no_animation = 1;
        break;
    }
    u = main_of(a);
    if ((actor_flags(a, 0x10) || actor_flags(a, 0x40000)) && u && u->m && u->m->interruptible)
        u->no_animation = 1;
    if (!actor_can_move(a) && !actor_can_use_skill(a, skill) && !u) return 0;
    if ((a->dynamic_state != 1 && a->dynamic_state != 11) || has_status(a, 101) || a->knock == 1) return 0;
    if (cancel && command != 3) actor_cancel_command(a);
    switch (command) {
    case 1: case 2:
        if (actor_can_move(a) && (!has_status(a, 0) || !actor_move_cast(a, (unsigned short)p0, (unsigned short)p1))) {
            if (command == 2) actor_set_flags(a, 0x10000, 1);
            actor_dynamic_command(a, 11, p0, p1, p2);
            return 1;
        }
        break;
    case 3:
        actor_set_flags(a, 0x10000, p0);
        actor_animation_select(a, -1);
        return 1;
    case 5: case 6: case 7: case 10:
        actor_dynamic_command(a, command == 5 ? 31 : command == 6 ? 32 : command == 7 ? 22 : 21, p0, p1, p2);
        return 1;
    case 8: case 9:
        if (!actor_can_use_skill(a, skill)) return 0;
        if (actor_command_equal(a, command, p0, p1, p2)) return 0;
        actor_queue_command(a, command, p0, p1, p2);
        break;
    case 12:
        if (a->dynamic_state == 1) {
            actor_status_notify(a, 101, 1, -1);
            actor_animation_select(a, -1);
            return 1;
        }
        break;
    case 15: case 16: case 17: case 18: case 19:
        if (remember) actor_forget_command(a);
        if (actor_command_equal(a, command, p0, p1, p2)) return 0;
        if (command == 15 && !has_status(a, 103)) {
            Actor *target = character_of(p0);
            if (target && target->modifier[7].time < 101 &&
                (target->modifier[7].value != a->sh || target->modifier[7].time != 100)) {
                target->modifier[7].value = a->sh; target->modifier[7].time = 100;
                if (!a->sh) target->modifiers_dirty = 1;
            }
        }
        actor_queue_command(a, command, p0, p1, p2);
        break;
    case 102:
        if (actor_can_move(a)) {
            actor_dynamic_command(a, 11, a->grid_x + p0, a->grid_y + p1, p2);
            return 1;
        }
    }
    return 1;
}

// OmUtil::IsRange：最大方框含邊界，最小方框內部排除；不是歐氏距離。
// 已核對OmUtil匯出本體；min==max時仍可命中外框。
static int actor_skill_in_range(int dx, int dy, int max, int min) {
    dx = abs(dx); dy = abs(dy);
    return dx <= max && dy <= max && (dx >= min || dy >= min);
}

// 互動命令15..19的收尾與接近（0x489730 的 0x48a2f9／0x48a352）。
static void actor_command_finish(Actor *a) { actor_dynamic_command(a, 1, 0, 0, 0); actor_cancel_command(a); }
static void actor_interact_approach(Actor *a, int x, int y) {
    if (a->dynamic_state == 11 && (int)actor_flags(a, 0x10000) == a->dynamic_flag_snapshot &&
        a->dynamic_command[0] == x && a->dynamic_command[1] == y) return;
    if (!actor_dynamic_retry(a)) { actor_command_finish(a); return; }
    actor_command(a, 1, x, y, 0, 0, 0);
}

// 原版封包0x75（說話）／0x68（撿道具）的單機接收端。
static Actor *talk_from, *talk_to;
// 走近自動說話（輔助，預設關，2026-11-21 使用者反映離很遠就突然開口；--autotalk／?autotalk=1 開）。shortcut: 原版沒有「主角走近 NPC 自動開口」（只有 s_pctstart 觸發區、
// 命令 15 點 NPC、NPC 頭頂隨機台詞 npc_chat），手機很難點準 NPC 才加。主角站著時 3 格內、走動時 2 格內觸發；同一個 NPC 說過後要離開 6 格才再觸發。
static int show_pos, show_perf, no_interp;
static int auto_talk = 0;
static int auto_stick_lock;  // 自動說話觸發後忽略搖桿直到放開
#define AUTO_TALK_LEAVE 6
static int auto_talk_step(int dist, int still, unsigned char *done) {
    if (dist > AUTO_TALK_LEAVE) *done = 0;
    if (*done || dist > (still ? 3 : 2)) return 0;
    return *done = 1;
}
static void ground_pick(int g);
static int ground_find(int id) {
    for (int i = 0; i < ground_n; i++) if (ground[i].id == id) return i;
    return -1;
}

// Poq.exe 0x489730 命令15..19：15對話、16進建築、17撿地上道具、18使用物件（0x81／0x8c）、19開箱（0x85）。
// 都是先走到範圍內再送封包；單機把封包換成直接動作（說話請求交給主迴圈）。
// shortcut: 物件編號 = Scene.objects 索引（原版是帶世代的句柄）；原版呼叫的 0x405597（距離類判斷，推 0x60 = 96 單位內）、
// 0x4017f3／0x406181（控制權／句柄有效）來源在映像外，分別當距離判斷與恆真；建築名稱載入（封包0x142，CTriBld）與
// 開箱／使用物件（封包0x8e）尚無接收系統，只做到定位、記 interaction 與收尾。
// 點門（命令 0x10）到位後要送的出口名（封包 0x142 的字串 = 建築名稱第 2 個分詞），由主迴圈換場景；door_cool = DAT_00663ee8（送出後 0x60 拍內不重送）
#define BLD_TARGET 0x20000  // 命令 9 的目標欄 >= 這個值 = 建築索引 + BLD_TARGET（原版是句柄）
static char door_go[128];
static int door_go_due, door_cool;
static void chest_open_object(int idx);  // 開箱（0x85 建築，封包 0x8e → 文字指令 open）
static void door_gather_say(void);  // 訊息 0x508（伺服端 0x10069af0 收到 "null" 時說；隊友沒到齊）
// 0x443600：(x, y) 周圍 3×3 格裡有沒有建築 idx 的 0x88 格
static int door_near(int x, int y, int idx) {
    for (int j = y - 1; j <= y + 1; j++)
        for (int i = x - 1; i <= x + 1; i++) {
            const Cell *cl = cell_at(i, j);
            if (cl && cl->building == idx && (cl->flags & 0x88) == 0x88) return 1;
        }
    return 0;
}
static void actor_interact_tick(Actor *a) {
    int c = a->command[0], sx = ax64(a) >> 6, sy = ay64(a) >> 6;
    const Scene *sc = unit_scene;
    if (c == 15) {
        Actor *t = character_of(a->command[1]);
        if (!t || is_foe(t, a)) { actor_command_finish(a); return; }
        int tx = ax64(t) >> 6, ty = ay64(t) >> 6;
        if (abs(sx - tx) > 3 || abs(sy - ty) > 3 || !line_clear(sx, sy, tx, ty, 3, 3)) { actor_interact_approach(a, tx, ty); return; }
        Actor *from = a, *to = t;
        if (a->grp > 60000) { a->search_link = 0; from = t; to = a; }  // 組60001以上：說話者與對象互換
        else if (has_status(t, 0x67)) return;  // 目標無对话（0x489e74：ecx = 目標）：等狀態解除，命令保留
        actor_dynamic_command(a, 1, 0, 0, 0);
        // shortcut: 原版模板+0x118==1（單方向模型）不轉向，這裡都轉
        actor_direction_set(t, dir_of(ax64(a), ay64(a), ax64(t), ay64(t), 8));
        actor_body_apply(t, -1);
        actor_dynamic_command(a, 1, 0, 0, 0);
        actor_direction_set(a, dir_of(ax64(t), ay64(t), ax64(a), ay64(a), 8));
        actor_body_apply(a, -1);
        talk_from = from; talk_to = to;
        actor_command_finish(a);
    } else if (c == 16) {
        // 命令 0x10（0x489730 case 0x10）：門點 (x, y)、建築。0x443600 = 3×3 格裡有這棟建築的 0x88 格；隊友任一人在門旁且全隊離主角 ≤ 0x180（0x406181＋0x4017f3）也算到。
        // 到了且是 CTriBld：隊伍沒到齊（0x4017f3 假）說訊息 0x508；冷卻中（DAT_00663ee8）不重送；其餘送封包 0x142 = 名稱第 2 個分詞（go <出口> 的出口名）。沒到就走去門點。
        int tx = a->command[1], ty = a->command[2], idx = (unsigned short)a->command[3];
        int near = door_near(sx, sy, idx), any = near, gathered = 1;
        for (int i = 0; i < actor_count; i++) {
            Actor *m = &actors[i];
            if (m == a || !m->team || !alive(m)) continue;
            if (door_near(ax64(m) >> 6, ay64(m) >> 6, idx)) any = 1;
            int dx = ax64(m) - ax64(a), dy = ay64(m) - ay64(a);
            if (dx * dx + dy * dy > 0x180 * 0x180) gathered = 0;
        }
        const Object *o = sc && idx < sc->object_count ? &sc->objects[idx] : NULL;
        if (!(near || (any && gathered)) || !o || !o->m || !o->tri) { actor_interact_approach(a, tx, ty); return; }
        if (!gathered) door_gather_say();
        else if (door_cool <= 0 && o->name[0]) {
            const char *t = o->name + strspn(o->name, " ");
            t += strcspn(t, " ");
            t += strspn(t, " ");
            snprintf(door_go, sizeof door_go, "%.*s", (int)strcspn(t, " "), t);
            door_go_due = 1; door_cool = 0x60;
            if (debug_cells) printf("door go %s (hero %d,%d)\n", door_go, sx, sy);
        }
        actor_command_finish(a);
    } else if (c == 17) {
        int g = ground_find(a->command[1]);
        if (g < 0) { actor_command_finish(a); return; }
        int tx = (int)ground[g].x, ty = (int)ground[g].y;
        if (abs(sx - tx) > 2 || abs(sy - ty) > 2) { actor_interact_approach(a, tx, ty); return; }
        actor_dynamic_command(a, 1, 0, 0, 0);
        actor_cancel_command(a);
        ground_pick(g);
        actor_command_finish(a);
    } else {
        // 0x484e80（CTriBld 使用，模型 128 且狀態 0／2 → 封包 0x8e）= 命令 18 到位後的收尾；封包 0x8e 的接收端單機沒有（見上 shortcut）
        int idx = a->command[1];
        if (!sc || idx < 0 || idx >= sc->object_count || !sc->objects[idx].m) { actor_command_finish(a); return; }
        const Object *o = &sc->objects[idx];
        int tx = o->x, ty = o->y;
        // 命令 0x12（使用物件，0x81／0x8c）：隊員離建築 < 0x60（歐氏距離，0x466bd0）→ 封包 0x8e；命令 0x13（開箱，0x85）：4 格內（IsMaxRange）→ 開
        int dx = ax64(a) - (tx * 64 + 32), dy = ay64(a) - (ty * 64 + 32);
        int ok = c == 18 ? dx * dx + dy * dy < 0x60 * 0x60 : abs(sx - tx) <= 4 && abs(sy - ty) <= 4;
        if (ok) { chest_open_object(idx); actor_command_finish(a); } else actor_interact_approach(a, tx, ty);
    }
}

// Poq.exe 0x489730 技能／命令tick：記住命令重送、命令8/9施法追近、15..19互動（actor_interact_tick）。
// 8/9追近的GetFleePos隱藏步長與NPC入戰全域選人同步的shortcut見內文。
static void actor_skill_command_tick(Actor *a) {
    if (a->remembered[0] != -1 && memcmp(a->remembered, a->command, sizeof a->command)) {
        int c = a->remembered[0];
        if (c == 1 || c == 2) {
            if (!actor_dynamic_retry(a) ||
                (actor_init_x(a) == a->remembered[1] && actor_init_y(a) == a->remembered[2]))
                actor_forget_command(a);
            else if (a->dynamic_state == 1)
                actor_command(a, c, a->remembered[1], a->remembered[2], a->remembered[3], 1, 0);
        } else if (c == 8 || c == 9) {
            if (actor_flags(a, 1)) actor_forget_command(a);
            else if (actor_can_use_id(a, -1))
                actor_command(a, c, a->remembered[1], a->remembered[2], a->remembered[3], 1, 0);
        } else actor_forget_command(a);
    }
    if (actor_command_idle(a) || !a->dynamic_active) return;
    int c = a->command[0];
    if (c >= 15 && c <= 19) { actor_interact_tick(a); return; }
    if (c != 8 && c != 9) return;
    int id = a->command[1] == -1 ? a->right_id : a->command[1];
    if (c == 8) {
        actor_dynamic_command(a, 1, 0, 0, 0);
        if (!actor_can_use_id(a, id)) {
            actor_set_flags(a, 1, 1);
            actor_cancel_command(a);
        } else if (actor_cast_at(a, id, a->command[2] * 64 + 32, a->command[3] * 64 + 32, NULL, 0))
            actor_cancel_command(a);
        return;
    }
    int slot = actor_skill_find(a, id);
    if (slot < 0 || !actor_can_use_skill(a, &a->skills[slot])) return;
    if (a->command[2] >= BLD_TARGET) {  // 目標是建築（種類 10～20）：座標 = 擺放格；已不是 0／2 狀態（打過了）就放棄。被打中的是 hit_find（0x439fb0）呼叫建築使用
        const Scene *sc = unit_scene;
        int bi = a->command[2] - BLD_TARGET;
        const Object *o = sc && bi < sc->object_count ? &sc->objects[bi] : NULL;
        if (!o || !o->m || !building_available(o)) {
            actor_dynamic_command(a, 1, 0, 0, 0);
            actor_clear_target(a);
            actor_cancel_command(a);
            if (a->remembered[0] == 9) actor_forget_command(a);
            return;
        }
        ActorSkill *skill = &a->skills[slot];
        int range = actor_skill_range(a, skill);
        if (range >= 4) --range;
        int x = o->x, y = o->y, sx = ax64(a) >> 6, sy = ay64(a) >> 6;
        if (debug_cells && a == &actors[0]) printf("hit bld %d: hero %d,%d bld %d,%d range %d min %d id %d inrange=%d\n", bi, sx, sy, x, y, range, skill->min, id, actor_skill_in_range(sx - x, sy - y, range, skill->min));
        if (actor_skill_in_range(sx - x, sy - y, range, skill->min)) {
            actor_dynamic_command(a, 1, 0, 0, 0);
            int casted = actor_cast_at(a, id, x * 64 + 32, y * 64 + 32, NULL, 0);
            if (debug_cells && a == &actors[0]) printf("cast at bld -> %d\n", casted);
            if (casted) actor_cancel_command(a);
            return;
        }
        if (a->dynamic_state == 11 && a->dynamic_command[0] == x && a->dynamic_command[1] == y) return;
        if (!actor_dynamic_retry(a)) { actor_clear_target(a); actor_cancel_command(a); return; }
        actor_command(a, 1, x, y, 0, 0, 0);  // 建築擺放格本身不能走，走到旁邊（range 0 = 走到格子為止，同 actor_interact_approach）
        return;
    }
    Actor *t = actor_of(a->command[2]);
    if (!t) {
        actor_dynamic_command(a, 1, 0, 0, 0);
        actor_cancel_command(a);
        actor_clear_target(a);
        if (a->remembered[0] == 8 || a->remembered[0] == 9) actor_forget_command(a);
        return;
    }
    int character = !is_trap(t) && (unsigned)(t->object_kind - 30) < 71u;
    if (character && (has_status(t, 102) || actor_flags(t, 0x4000))) {
        actor_dynamic_command(a, 1, 0, 0, 0);
        actor_set_flags(a, 1, 1);
        actor_cancel_command(a);
        return;
    }
    if ((unsigned)(t->object_kind - 10) < 11u ? t->object_dead :
        (t->object_kind == 24 || is_trap(t)) && t->object_dead) {
        actor_dynamic_command(a, 1, 0, 0, 0);
        actor_clear_target(a);
        actor_cancel_command(a);
        return;
    }
    ActorSkill *skill = &a->skills[slot];
    int range = actor_skill_range(a, skill);
    if (range >= 4) --range;
    if (character) range += t->body_size / 2;
    int x = ax64(t) >> 6, y = ay64(t) >> 6;
    int sx = ax64(a) >> 6, sy = ay64(a) >> 6;
    if (!has_status(a, 0)) actor_command(a, 11, 1, 0, 0, 0, 0);
    // 單機省略NPC入戰的全域選人／介面同步，狀態與技能仍由共用接收端執行。
    if (actor_skill_in_range(sx - x, sy - y, range, skill->min)) {
        actor_dynamic_command(a, 1, 0, 0, 0);
        if (actor_cast_at(a, id, ax64(t), ay64(t), t, 0)) actor_cancel_command(a);
        return;
    }
    if (a->dynamic_state == 11 && a->dynamic_command[0] == x && a->dynamic_command[1] == y) return;
    if (!actor_dynamic_retry(a)) { actor_clear_target(a); actor_cancel_command(a); return; }  // 原版（非回放）一併清目標句柄+0xa3
    if (abs(sx - x) < range && abs(sy - y) < range) {
        // GetFleePos隱藏步長由Poq呼叫點核對，為SkillUnit.min+1。
        int fx = sx, fy = sy;
        move_dir(&fx, &fy, dir_of(x, y, sx, sy, 8), skill->min + 1);
        actor_command(a, 1, fx, fy, 0, 0, 0);
    } else {
        if (range > 1 && !line_clear(sx, sy, x, y, range, 3)) range = 2;
        actor_command(a, 1, x, y, range, 0, 0);
    }
}

static void hero_cast_point(Actor *h, int x, int y) {
    int k = cur_skill[1];
    if (k < 0 || k >= hero_skill_n || h->act || h->casting) return;
    const SkillRow *r = &hero_skill[k];
    int manual = skill_manual(r);
    if (manual == 2 || manual == 3) {
        // 命令8的參數是格座標；實際施法入口再取格子中心。
        actor_command(h, 8, skill_attack_id(r), x / 64, y / 64, 1, 1);
        return;
    }
    if (!skill_needs_foe(r)) { cast_start(h, r); return; }
    if (!alive(h->foe) || !skill_usable(h, r)) return;
    actor_command(h, 9, skill_attack_id(r), h->foe->sh ? h->foe->sh : (int)h->foe->object_id, 0, 1, 1);
}

static void hero_cast(Actor *h) { hero_cast_point(h, ax64(h), ay64(h)); }

// 玩家明確要打 t（點擊、攻擊鍵）：命令 9、右手技能（-1），記住；目標選取本身不會出手
// 主角沒有任何命令的一拍：原版沒有自動出手（0x48f560 只選目標＋入戰姿勢命令 11），攻擊只來自玩家的命令 9（點擊／攻擊鍵），由記住的命令續打
static void hero_idle_tick(Actor *h, const Scene *s, Uint32 tick) {
    if (h->foe && !alive(h->foe)) actor_clear_target(h);
    actor_tick(h, s, 0, 0, tick);
}
static void hero_attack(Actor *h, Actor *t) { actor_command(h, 9, -1, t->sh ? t->sh : (int)t->object_id, 0, 1, 1); }

// 快捷技能視窗（Poq.exe 0x504d90 開 JM514 = 右手、0x504b40 開 JM515 = 左手；0x564360 填格子）：
// 依序放進左右鍵位等於這隻手或 2（兩手都行）、等級 > 0 的技能，第 n 個放 m_pItem[n]。
// shortcut: 原版還要 SpeType == 0（存檔欄位，來源沒找到，先當都是 0）；視窗位置原版 0x583790 還沒翻，先貼在該手的按鈕上方
#define JM514 "JM514_\xbf\xec\xbd\xdd\xbc\xbc\xc4\xdc\xbd\xe7\xc3\xe6"
#define JM515 "JM515_\xbf\xec\xbd\xdd\xbc\xbc\xc4\xdc\xbd\xe7\xc3\xe6"
static UiDialog pick;
static int pick_hand = -1, pick_skill[24], pick_n;

// 原版 CShortcutSkillDialog 0x5640a0：把 24 個 m_pItem 控制項（0x6a5 起）取好，這裡 ui_load 一次載入；0x564310 關閉＝ pick_hand = -1
static void pick_open(int hand) {
    if (pick_hand == hand) { pick_hand = -1; return; }
    skills_load();
    pick_hand = ui_load(hand ? JM515 : JM514, &pick) > 0 ? hand : -1;
    pick_n = 0;
    for (int k = 0; k < hero_skill_n && pick_n < 24; k++)
        if ((hero_skill[k].hand == hand || hero_skill[k].hand == 2) && hero_skill[k].level > 0) pick_skill[pick_n++] = k;
}

// 原版 0x564250：左手（+0x2e4 == 0）貼在左手鍵位置的視窗左邊界，右手對齊右邊界，視窗底貼在主列上緣
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
// shortcut: 不能用（精气不夠、武器不合）時畫暗一點是自訂的，原版怎麼表示還沒查
static void skill_icon(const UiCtrl *c, int ox, int oy, int k) {
    const Sprite *bg = ui_image(c->img, 0), *ic = k >= 0 && k < hero_skill_n ? ui_image(hero_skill[k].icon, 0) : NULL;
    if (!bg || !ic) return;
    int dim = !skill_usable(&actors[0], &hero_skill[k]);  // actors[0] = 主角
    if (dim) SDL_SetTextureColorMod(ic->tex, 100, 100, 100);
    draw(ic, ox + c->x + (bg->w - ic->w) / 2, oy + c->y + (bg->h - ic->h) / 2);
    if (dim) SDL_SetTextureColorMod(ic->tex, 255, 255, 255);
}

// ---- 道具：背包、穿脫、外觀、掉寶、撿取、使用、修理、鑑定、耐久 ----
static RoleDef hero_def;  // 主角的角色表列，外觀加上身上裝備（道具表該職業的裝備串）
static int item_sel = -1;  // 背包選中的格子（UI_Pos）
static char toast[128];    // 系統訊息（原版 0x1004e560 秀在畫面上）
static Uint32 toast_until;
// shortcut: 原版訊息的字串表（0x10060280 的編號）還沒對，先用自訂的中文
// ---- 任務日誌（自訂，手機版；原版 JM61 日誌界面沒做）----
// 劇情 journal（0x1004dcc0）以前只 printf，玩家從來看不到任務提示（新手進遊戲不知道要做什麼的根因，2026-11-20）。
// 存起來（顏色標記 [0名] 拿掉），有新的就自動開日誌視窗；J 鍵、點畫面上方提示字再開；點任何地方關。shortcut: 只顯示最新幾則，沒有捲動
#define JOURNAL_MAX 60
static char journal_txt[JOURNAL_MAX][768];
static int journal_n, journal_open, journal_pop;
static void journal_add(const char *t) {
    char *o = journal_txt[journal_n % JOURNAL_MAX];
    size_t k = 0;
    for (const char *p = t; *p && k < sizeof journal_txt[0] - 1; p++) {
        if (*p == '[' && p[1] >= '0' && p[1] <= '9') { p++; continue; }  // [0xxx] → xxx
        if (*p == ']') continue;
        o[k++] = *p;
    }
    o[k] = 0;
    journal_n++;
    journal_pop = 1;
}
static void journal_draw(int vw_, int vh_) {
    if (!journal_open) return;
    text_zoom = screen_zoom > 2 ? 2 : screen_zoom < 1 ? 1 : screen_zoom;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 8, 8, 8, 225);
    SDL_Rect r = {20, 20, vw_ - 40, vh_ - 110};  // 下方留給操作列
    SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawColor(ren, 190, 160, 90, 255);
    SDL_RenderDrawRect(ren, &r);
    int y = r.y + 8, w = r.w - 16;
    draw_text(r.x + 8, y, "\xe4\xbb\xbb\xe5\x8b\x99\xe6\x97\xa5\xe8\xaa\x8c", w, 0xE8D890, 1);  // 任務日誌
    y += 26 * (int)(text_zoom + 0.5f) / 1;
    int shown = 0;
    for (int i = journal_n - 1; i >= 0 && i >= journal_n - JOURNAL_MAX; i--) {  // 最新在上
        const char *t = journal_txt[i % JOURNAL_MAX];
        int h = draw_text(0, 0, t, w, 0, 0);
        if (y + h > r.y + r.h - 24 && shown) break;
        draw_text(r.x + 8, y, t, w, shown ? 0xA8A898 : 0xFFFFFF, 1);
        y += h + 10; shown++;
    }
    if (!journal_n) draw_text(r.x + 8, y, "\xef\xbc\x88\xe9\x82\x84\xe6\xb2\x92\xe6\x9c\x89\xe6\x97\xa5\xe8\xaa\x8c\xef\xbc\x89", w, 0xA8A898, 1);  // （還沒有日誌）
    draw_text(r.x + 8, r.y + r.h - 20, "\xe9\xbb\x9e\xe4\xbb\xbb\xe4\xbd\x95\xe5\x9c\xb0\xe6\x96\xb9\xe9\x97\x9c\xe9\x96\x89\xef\xbc\x88J \xe9\x8d\xb5\xe6\x88\x96\xe9\xbb\x9e\xe4\xb8\x8a\xe6\x96\xb9\xe6\x8f\x90\xe7\xa4\xba\xe5\xad\x97\xe5\x86\x8d\xe9\x96\x8b\xef\xbc\x89", w, 0x909080, 1);  // 點任何地方關閉（J 鍵或點上方提示字再開）
    text_zoom = 1;
}
// 提示訊息顯示時間依長度加長（長的任務提示、如「一塊巨石阻住去路…」2.5 秒讀不完）：每個字多 90 毫秒，最多 9 秒
static void hud_say(const char *t) {
    snprintf(toast, sizeof toast, "%s", t);
    int chars = 0;
    for (const char *p = t; *p; p++) chars += (*p & 0xC0) != 0x80;
    int ms = 2500 + chars * 90;
    toast_until = SDL_GetTicks() + (ms > 9000 ? 9000 : ms);
}
#include "caption.c"
#include "mapwin.c"
#include "party.c"
#include "input.c"
static void character_death_message(void) {
    ui_text(1500, toast, sizeof toast);
    toast_until = SDL_GetTicks() + 1000;
}
static void trap_detected_message(void) {
    // CGameApp訊息1501，類型16，1000ms；使用原版字串表。
    ui_text(1501, toast, sizeof toast);
    toast_until = SDL_GetTicks() + 1000;
}
// 原版系統訊息：EasyRPG_C.dlg 第 id 筆（GBK 格式字串）照參數排好再秀
static void hud_msg(int id, ...) {
    char f[256], g[256];
    if (!res_dlg("EasyRPG_C.dlg", id, f, sizeof f)) return;
    va_list ap;
    va_start(ap, id);
    vsnprintf(g, sizeof g, f, ap);
    va_end(ap);
    gbk_utf8(g, toast, sizeof toast);
    toast_until = SDL_GetTicks() + 2500;
}

// 學技能（Poq 封包 0x231 → 0x1006a120；文字指令 skl_learn 0x1001d980 同式，AddSkill 0x10037d70、建技能物件 0x100529a0）。
// 已學過 → 1325；技能不在 SKL_ToLearn（「索引名 分類 索引名 分類…」，分類 = S_Class）→ 1322；同分類已學 4 個 → 1324；成功 → 1321「你学会了技能」。
// shortcut: SKL_ToLearn 原版由伺服器的老師封包寫入（0x1004f0xx），單機劇情沒人教，所以只有 --learn（網址 learn=）塞得進去；
//   技能表沒有 S_Class 欄（在技能物件上），已學的技能分類記在學的當下（hero_skill_class，起始 −1）；學到的等級用 1（表上初始等級空白 = −1，引擎 0 = 沒學）
static char hero_to_learn[512];
static int hero_skill_class[20] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static int hero_learn_skill(const char *key) {
    skills_load();
    int k = -1;
    for (int i = 0; i < hero_skill_n; i++)
        if (!strcmp(hero_skill[i].key, key)) k = i;
    if (k >= 0 && hero_skill[k].level > 0) { hud_msg(1325); return 0; }
    const char *p = strstr(hero_to_learn, key);
    if (!p || k < 0) { hud_msg(1322); return 0; }
    p += strlen(key);
    int cls = atoi(p), n = 0;
    for (int i = 0; i < hero_skill_n; i++) n += hero_skill[i].level > 0 && hero_skill_class[i] == cls;
    if (n > 3) { hud_msg(1324); return 0; }
    SkillRow *r = &hero_skill[k];
    r->level = 1;
    hero_skill_class[k] = cls;
    actor_skill_update(&actors[0], k + 1, skill_attack_id(r), (int)skill_field(r, 12, skill_value(r->range_min, r->range_min_inc, r->level)),
        skill_range_max(r), 0, 0);
    actor_skill_enable(&actors[0], k + 1, 1, 0);
    char g[128] = "";
    res_dlg("name_C.dlg", r->name_id, g, sizeof g);
    hud_msg(1321, g);
    return 1;
}

// 忘記技能（文字指令 skl_forget 0x1001d840；放棄 abandon 0x10011410 → 核心 0x10053ef0：技能物件從技能清單刪掉、送客戶端 DEL_S）：技能物件刪掉，訊息 1323；不是自己的技能 1267。單機沒有技能物件代號，用技能表索引
static int hero_forget_skill(int k) {
    if (k < 0 || k >= hero_skill_n || hero_skill[k].level <= 0) { hud_msg(1267, ""); return 0; }
    char g[128] = "";
    res_dlg("name_C.dlg", hero_skill[k].name_id, g, sizeof g);
    hero_skill[k].level = 0;
    hero_skill_class[k] = -1;
    actor_skill_enable(&actors[0], k + 1, 0, 0);
    hud_msg(1323, g);
    return 1;
}

// SetSkill（0x10054cb0 → AddSkill 0x10037d70）：直接給技能並設等級；同名技能已有 → 加不進去（回 0，原版回 NULL）；技能表沒有 → 0（「创建物体失败」）
// shortcut: 原版沒有單機呼叫者（劇情 0 處），所以沒有入口；--setskill 之類的除錯入口也沒做，只有 skilltest 呼叫
static int hero_set_skill(const char *key, int level) {
    skills_load();
    int k = -1;
    for (int i = 0; i < hero_skill_n; i++)
        if (!strcmp(hero_skill[i].key, key)) k = i;
    if (k < 0 || hero_skill[k].level > 0) return 0;
    SkillRow *r = &hero_skill[k];
    r->level = level;
    actor_skill_update(&actors[0], k + 1, skill_attack_id(r), (int)skill_field(r, 12, skill_value(r->range_min, r->range_min_inc, r->level)),
        skill_range_max(r), 0, 0);
    actor_skill_enable(&actors[0], k + 1, 1, 0);
    return 1;
}

static int bag_free(void) {
    for (int i = 0; i < 24; i++)
        if (!inv[i].name[0]) return i;
    return -1;
}

// 負重：身上道具重量合計，上限 BD_MAX = Str × 1.5 + 30。超重效果（速度減半、改走路動畫）見 burden_threshold（2026-11-20 查到原版 0x488540／0x48c050）
static int hero_burden(void) {
    int w = 0;
    for (int i = 0; i < INV_N; i++) w += inv[i].name[0] ? inv[i].weight : 0;
    return w;
}
static int hero_burden_max(const Actor *h) { return (int)((hero_attr[STR] + h->ex[27]) * 1.5f + 30); }
static char hero_name[64];
// 訊息裡的 %s 是主角名（UTF-8，不能走 hud_msg 的 GBK 轉碼）：先轉格式再填名字
static void hud_msg_hero(int id) {
    char f[256], u[512];
    if (!res_dlg("EasyRPG_C.dlg", id, f, sizeof f)) return;
    gbk_utf8(f, u, sizeof u);
    snprintf(toast, sizeof toast, u, hero_name);
    toast_until = SDL_GetTicks() + 2500;
}
// 東西搬進身上（UI_Pos 從 200~299 倉庫／地上 → <200，0x1004cb00）：加進去前不超重、加完超重 → 訊息 3026「%s的東西太多走不動了」（一次，之後 BD > BD_MAX 不再提醒）。
// BD 加減本身 = hero_burden 即時加總，不必存。超重的速度懲罰見 burden_threshold（speed_threshold ≥ 101 時速度減半）
static void burden_warn(const Item *it) {
    int bd = hero_burden(), mx = hero_burden_max(&actors[0]);
    if (bd <= mx && bd + it->weight > mx) hud_msg_hero(3026);
}

// 外觀：角色表的款式蓋上身上每件裝備的「該職業裝備串」（道具表 33~37 欄），重新載入主角的部件
static void hero_look(void) {
    const RoleDef *d = roledef_find(hero_pic);
    if (!d) d = roledef_by_graphic(HERO_GRAPHIC);
    if (!d) return;
    hero_def = *d;
    if (hero_look_str[0]) look_override(&hero_def, hero_look_str);  // 範本的外觀字串（動作方式＋款式）
    for (int s = INV_BAG; s < INV_N; s++)
        if (inv[s].name[0] && inv[s].equip[hero_prof - 1][0])
            styles_equip(hero_def.styles, sizeof hero_def.styles, d->graphic, hero_def.pli, inv[s].equip[hero_prof - 1]);
}
// 裝備變動後重找箭袋（0x10055b50）：左手 UI_Pos 33 的類型 13／14 = HasArrow，有的話打開所有 W_Type 1／3 的弓技能（Enable = 1）。
// 沒有箭時原版不關（關是箭射完那一刻 0x1003f370，arrow_spend）
static void arrow_refresh(Actor *h) {
    if (!inv[33].name[0] || (inv[33].type != 13 && inv[33].type != 14)) return;
    for (int i = 0; i < h->skill_n; i++)
        if (h->skills[i].row && (h->skills[i].row->wtype == 1 || h->skills[i].row->wtype == 3)) h->skills[i].enabled = 1;
}
// 換武器後右手技能不合手持類：改選第一個能用的右手技能（拿弓 → 射箭技能；換回刀 → 刀技能）。
// shortcut: 原版換武器時 Poq 怎麼重選右手技能還沒找到（EasyRPG 只更新 Hand），先用「第一個右手鍵位、等級 > 0、武器合的」
static void hero_right_fit(Actor *h) {
    int k = cur_skill[0];
    if (k >= 0 && k < hero_skill_n && skill_usable(h, &hero_skill[k]) && hero_skill[k].hand != 1) return;
    for (int i = 0; i < hero_skill_n; i++)
        if (hero_skill[i].hand != 1 && hero_skill[i].wtype >= 0 && skill_usable(h, &hero_skill[i]) && !skill_manual(&hero_skill[i])) {
            cur_skill[0] = i;
            actor_right_skill(h, skill_attack_id(&hero_skill[i]));
            hero_selected_skill_apply(h);
            return;
        }
}
static void hero_refresh(Actor *h) {
    arrow_refresh(h);
    hero_hand_update();
    if (hero_skill_n > 0) hero_right_fit(h);
    hero_derive(h);
    if (!h->role) return;  // 還沒進場景（開場）
    hero_look();
    uint32_t g[MAX_ROLE_PARTS * 2];
    res_fetch_now(g, role_anim_ids(hero_def.graphic, hero_def.pli, hero_def.styles, g, MAX_ROLE_PARTS * 2));
    actor_look_set(h, &hero_def);
}

// 能不能穿（EasyRPG.dll 0x1004c0e0，回傳值反編譯漏掉，由機碼核對）：回「差哪幾項」的位元，0 = 可以穿（伺服端設進道具 Enable，客戶端依此畫紅字）：
// 1 等級 < LevelReq、2 Str < StrReq、4 Con < ConReq、8 Dex < DexReq、0x10 道具 HP == 0（壞了）、0x20 沒鑑定。數值是人物目前的 Level／Str／Con／Dex 屬性
static int item_equip_flags(const Actor *h, const Item *it) {
    return (h->level < it->lv_req) | (hero_attr[STR] + h->ex[27] < it->str_req) << 1 | (hero_attr[CON] + h->ex[28] < it->con_req) << 2 |
           (hero_attr[DEX] + h->ex[29] < it->dex_req) << 3 | (it->hp == 0) << 4 | !it->identify << 5;
}
// 客戶端穿裝前的檢查（Poq.exe 0x50bb30）：類型 7~24、27、33、34（武器、盾）要在職業可用表（0x63b490，每職業 6 格）裡；
// 其他類型不限。職業序號 1~5 以外原版會讀到別的表（隊友用），這裡不限
static int prof_can_use(int prof, int type) {
    static const int W[5][6] = {{8, 10, 22, 11, 13, 27}, {18, 19, 20, 21, 23, 24}, {7, 9, 11, 12, 13, 14}, {15, 17, 33, 34, 0, 0}, {15, 16, 33, 34, 0, 0}};
    if (!((type >= 7 && type <= 24) || type == 27 || type == 33 || type == 34) || prof < 1 || prof > 5) return 1;
    for (int i = 0; i < 6; i++) if (W[prof - 1][i] == type) return 1;
    return 0;
}
static int item_can_equip(const Actor *h, const Item *it) { return it->part > 0 && !item_equip_flags(h, it) && prof_can_use(hero_prof, it->type); }

// 部位 → 裝備欄（Poq.exe 0x508c60 的 EquipPos 轉換，EquipPos = 道具 Part）：1~7 → 30~36、8 項鍊 → 38、9 → 34、10 雙手 → 33（左手）、11 腰帶 → 39；
// 再依 0x50c390 的放法調整（equip_route）。類型要在 7~34
static int equip_slot(const Item *it) {
    static const int M[12] = {-1, 30, 31, 32, 33, 34, 35, 36, 38, 34, 33, 39};
    return it->type >= 7 && it->type <= 34 && it->part >= 1 && it->part <= 11 ? M[it->part] : -1;
}
// 放法調整（Poq.exe 0x50c390 的 0x50d0xx 一段）：目標是手（32、33）且已有東西、新道具類型 7~24 或 27：
// 弓 11 放在箭 13 上、箭 13 放在弓 11 上、弩 12 與矢 14 同理、類型 <11 或 22 放在盾 27 上、盾放在類型 7~10 或 22 上、
// 類型 21／23／24 放在 21／23／24 上 → 改放另一隻手；目標是戒指（36、37）且已有、類型 30、另一枚戒指欄是空的 → 放另一欄。其他照目標欄放（互換）。
// 原版這段要一個旗標參數 param_7 非 0 才做，呼叫點沒有解出，這裡一律做
static int equip_route(int s, const Item *it) {
    int t = it->type;
    if ((s == 32 || s == 33) && inv[s].name[0] && ((t >= 7 && t <= 24) || t == 27)) {
        int e = inv[s].type, sw = 0;
        if (t == 11) sw = e == 13;
        else if (t == 13) sw = e == 11;
        else if (t == 12) sw = e == 14;
        else if (t == 14) sw = e == 12;
        else if ((t < 11 || t == 22) && e == 27) sw = 1;
        else if (t == 27) sw = (e >= 7 && e <= 10) || e == 22;
        else if ((t == 21 || t == 23 || t == 24) && (e == 21 || e == 23 || e == 24)) sw = 1;
        if (sw) return s == 32 ? 33 : 32;
    }
    if ((s == 36 || s == 37) && t == 30 && inv[s].name[0] && !inv[s == 36 ? 37 : 36].name[0]) return s == 36 ? 37 : 36;
    return s;
}

// 成套武装（EasyRPG.dll 0x1001eaa0 把各件裝備移進一件「秦殇/道具/成套武装」放 UI_Pos 40，裡面的道具保留原本的 UI_Pos）。
// 原版對照（2026-11-23）：組套（0x1001eaa0 多參數版）只把各件 FUN_1004df20 掛進容器、各件 UI_Pos 不動，沒有走 0x1004cb00，所以不重算數值——
// 套裝沒有額外加成，各件的特性在穿上時早已複製進人物（0x1002fe20）；拆套（兩參數版）= 刪掉容器，0x1003e920 把內容物掛回持有者、UI_Pos 仍在 30~39。
// 所以「成員照單件算」就是原版。引擎不搬進容器，inv[40] 只記是哪幾格（set_mask）。
// 沒移植：容器本身的模板屬性（秦殇/道具/成套武装 不在 story 抽出檔內）、客戶端對容器成員數的鏡像（Poq 0x50c390 的 MemberCount／HangData）；
// 成員欄一有變動就拆掉套裝是簡化（原版容器仍包著，成員被搬走時只改該件 UI_Pos）。套裝本身不能放進背包，點它 = 拆開
static void set_asunder(void) {  // Poq.exe 0x5554c0 「weaponset #主角 #套裝」→ EasyRPG.dll 0x1001eaa0 的兩參數版：把套裝拆回各件
    if (!inv[40].name[0]) { hud_msg(1207); return; }
    memset(&inv[40], 0, sizeof inv[40]);
    hud_msg(1208, "\xb7\xfc\xcb\xd5");  // 扶苏（GBK）
}
static void set_combine(void) {  // Poq.exe 0x555380：件數 > 1 才送；0x1001eaa0 的多參數版
    int n = 0, first = -1, mask = 0;
    for (int s = INV_BAG; s < 40; s++)
        if (inv[s].name[0]) { n++; mask |= 1 << (s - INV_BAG); if (first < 0) first = s; }
    if (n < 2) { hud_msg(1205); return; }
    if (inv[40].name[0]) set_asunder();  // 原版只有一件，新的蓋過去（拆開舊的）
    Item *c = &inv[40];
    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "\xb3\xc9\xcc\xd7\xce\xe4\xd7\xb0");  // 成套武装（GBK，只是佔位）
    snprintf(c->alias, sizeof c->alias, "chengtaowuzhuang");
    c->icon = inv[first].icon;  // shortcut: 原版圖示是模板自己的，沒查到，借第一件的
    c->pos = 40;
    c->set_mask = mask;
    hud_msg(1210, "\xb7\xfc\xcb\xd5");
}

// 五行換算（0x10058900）：產品的五行 fe 決定它的五行特性（屬性 43~52 兩個一組、54~58）用哪一組。回 −1 = 這條特性不要（沒有五行 fe < 1）
static int fe_conv(int fe, int id) {
    static const int A[6] = {0, 43, 45, 47, 49, 51}, B[6] = {0, 55, 58, 57, 54, 56};
    if (fe < 1) return (id >= 43 && id <= 52) || (id >= 54 && id <= 58) ? -1 : id;
    if (id >= 43 && id <= 52) return fe <= 5 ? A[fe] + (id % 2 == 0) : id;
    if (id >= 54 && id <= 58) return fe <= 5 ? B[fe] : id;
    return id;
}

// 五行相生（裝備搭檔，0x10055d50）：腰帶 39—盔甲 31、項鍊 38—頭盔 30、右戒 36—右手 32、左戒 37—左手 33 兩件都穿著，
// 飾品（前者）的五行 f 經相生表 {0,3,4,2,5,1}[f] 等於另一件的五行（0x100581b0）→ 搭檔的「相生特性」（種類 2）生效（0x1006e340），
// 拿掉其中一件 → 搭檔的相生特性失效（0x1006f920）。原版只開關搭檔那件（剛穿上的那件本身不動），所以先穿的那件才拿到加成；
// 照原版保留：道具自己的開關旗標不會因為離開裝備欄而還原
static void fe_toggle(int s, int equip) {
    static const int P[4][2] = {{39, 31}, {38, 30}, {36, 32}, {37, 33}}, MAP[6] = {0, 3, 4, 2, 5, 1};
    if (s < 30 || s > 39 || !inv[s].name[0]) return;
    for (int k = 0; k < 4; k++) {
        int idx = P[k][0] == s ? 0 : P[k][1] == s ? 1 : -1;
        if (idx < 0) continue;
        int o = P[k][1 - idx], acc = P[k][0], arm = P[k][1];
        if (!inv[o].name[0] || inv[acc].fe < 0 || inv[acc].fe > 5 || MAP[inv[acc].fe] != inv[arm].fe) return;
        for (int i = 0; i < inv[o].fx_n; i++)
            if (inv[o].fx[i].kind == 2) inv[o].fx[i].on = equip;
        return;
    }
}
static void inv_swap(int a, int b) {
    if ((a >= INV_BAG && a < 40) || (b >= INV_BAG && b < 40)) memset(&inv[40], 0, sizeof inv[40]);
    fe_toggle(a, 0); fe_toggle(b, 0);  // 先把要離開裝備欄的對搭檔關掉
    Item t = inv[a];
    inv[a] = inv[b];
    inv[b] = t;
    inv[a].pos = a;
    inv[b].pos = b;
    fe_toggle(a, 1); fe_toggle(b, 1);
}

// 脫下 UI_Pos s 的裝備放進背包空格；背包滿了不脫
static int item_unequip(Actor *h, int s) {
    int b = bag_free();
    if (!inv[s].name[0]) return 0;
    if (b < 0) { hud_say("背包已滿"); return 0; }
    inv_swap(s, b);
    hero_refresh(h);
    return 1;
}

// 穿上背包 p 的道具，原來那格的換回背包。檢查與放法照原版客戶端（item_can_equip／equip_slot／equip_route）：
// 原版沒有「雙手武器要空出左手」「左手有東西右手不能是雙手」的規則——雙手武器（Part 10）本來就放左手欄 33，與盾、箭互換
static void item_equip_slot(Actor *h, int p, int want) {
    Item *it = &inv[p];
    int s = equip_slot(it);
    if (s < 0) return;
    if (want >= 0 && want != s) {  // 拖放到指定格：只有該部位本來能放的格（戒指兩格、手兩格都行）
        if (!((it->part == 7 && (want == 36 || want == 37)) || ((s == 32 || s == 33) && (want == 32 || want == 33)))) { hud_say("這件裝備不能放在這一格"); return; }
        s = want;
    }
    int fl = item_equip_flags(h, it);
    if (it->part <= 0 || fl || !prof_can_use(hero_prof, it->type)) {
        hud_say(fl & 0x20 ? "需要先鑑定" : fl & 0x10 ? "已經損壞，需要修理" : fl ? "條件不足，不能裝備" : "這個職業不能用");
        return;
    }
    s = equip_route(s, it);
    inv_swap(p, s);
    item_sel = -1;
    hero_refresh(h);
}
static void item_equip(Actor *h, int p) { item_equip_slot(h, p, -1); }

// 施放技能後箭消耗（EasyRPG.dll 0x1003f370，精氣由呼叫處先扣）：技能 Attack == 1（射箭）時，左手的箭堆（HasArrow = UI_Pos 33 的類型 13／14）
// 每發 −1；用光就刪箭堆、訊息 1289「箭射完了」，並關掉所有 W_Type 1／3 的弓技能（Enable = 0）。原版另送箭數給客戶端，單機不需要
static void arrow_spend(Actor *h, const SkillRow *r) {
    Item *ar = &inv[33];
    if (!r || r->attack != 1 || !ar->name[0] || (ar->type != 13 && ar->type != 14)) return;
    if (--ar->arrows > 0) return;
    memset(ar, 0, sizeof *ar);
    hud_msg_hero(1289);
    for (int i = 0; i < h->skill_n; i++) {
        const SkillRow *w = h->skills[i].row;
        if (w && (w->wtype == 1 || w->wtype == 3)) h->skills[i].enabled = 0;
    }
    hero_refresh(h);
}

// 吃（0x10016be0）：要已鑑定；把特性套到身上後道具消失。食物 21 = 生命、292 = 生命%、22 = 精氣、293 = 精氣%；
// 書 27~33 = 永久加力量、体质、敏捷、（30 不用）悟性、智慧、魅力
// shortcut: can_eat 在原版是物件模板設的，沒找到設的地方，先當食物（51）和書（49）能吃
// 文字指令 use（0x1001e660）同一套：不看 can_eat，通用 vtable 第 11 格回 0 = 用掉，訊息 1200（把%s用掉了）；
// 差別只在 use 多了「use X to Y」給別人（單機只有主角，沒做）和類型 49 書印說明（送客戶端的文字，沒做）
// 第 11 格：story_use（只有《孙膑兵法》覆寫，設旗標 sunbinbingfa，走查時發現以前沒呼叫）
static int item_eat(Actor *h, int p, int use) {
    Item *it = &inv[p];
    if (!use && it->type != 51 && it->type != 49) return 0;
    if (!it->identify) { hud_msg(1293); return 1; }
    if (h->hp <= 0) return 1;  // Dying：原版直接 return
#ifdef STORY
    if (it->sh && story_use(it->sh) < 0) return 1;  // 原版 0x1006a58b：物件第 11 格先跑（《孙膑兵法》設旗標），< 0 不用
#endif
    for (int i = 0; i < it->fx_n; i++) {
        const ItemFx *f = &it->fx[i];
        int v = (int)f->v;
        if (f->id == 21) h->hp += v;
        else if (f->id == 292) h->hp += h->hp_max * v / 100;
        else if (f->id == 22) h->mp += v;
        else if (f->id == 293) h->mp += h->mp_max * v / 100;
        else if (f->id >= 27 && f->id <= 33) {
            static const int A[7] = {STR, CON, DEX, -1, SAVVY, WISDOM, CHARM};
            if (A[f->id - 27] >= 0) hero_attr[A[f->id - 27]] += v;
        }
    }
    if (h->hp > h->hp_max) h->hp = h->hp_max;
    if (h->mp > h->mp_max) h->mp = h->mp_max;
    if (use) { char g[64]; snprintf(g, sizeof g, "%s", inv[p].name); hud_msg(1200, g); }
    inv[p].name[0] = 0;
    item_sel = -1;
    hero_derive(h);
    return 1;
}

static int hero_skill_level(const char *name) {
    for (int i = 0; i < hero_skill_n; i++)
        if (strstr(hero_skill[i].key, name)) return hero_skill[i].level;
    return 0;
}

// 界面技能（G_IsUISkill）的施展者：主角有就是主角，否則隊伍裡學了這個技能的隊友（原版隊友學到 G_IsUISkill 技能時 0x1004d240 登記 UIS_P、
// 0x5fa／0x606 把「誰有什麼界面技能、幾級」送給客戶端，客戶端挑施展者）。回傳施展者，*lv = 技能等級（0 = 沒人會）。
// shortcut: 多人都會時取主角、其次依隊伍順序第一個；客戶端挑選規則（MakeData 的順序、0x543810）沒完全對照
static Actor *ui_skill_actor(const char *gbk, const char *utf8, int *lv) {
    *lv = hero_skill_level(gbk);
    if (*lv > 0) return &actors[0];
#ifdef STORY
    for (int i = 1; i < actor_count; i++) {
        Actor *a = &actors[i];
        if (!a->team || !alive(a) || !a->sh) continue;
        for (int c = story_next(a->sh, 0); c; c = story_next(a->sh, c))
            if (story_kind(c) == 5 && !strcmp(story_name(c), utf8) && story_get_int(c, "Level") > 0) { *lv = story_get_int(c, "Level"); return a; }
    }
#else
    (void)utf8;
#endif
    return &actors[0];
}

// 修理（0x10054090）：自己的修缮之术，道具等級 ≤ 技能等級 × 3；價錢 = 原價 × 損耗 × 4 / 耐久上限（找 NPC 修 ×2）。
// 箭 13、14，29、30 和 7~34 以外的不能修。shortcut: NPC 修理要商店介面，還沒做
static void item_repair(int p) {
    Item *it = &inv[p];
    int lv;
    ui_skill_actor("\xd0\xde\xc9\xc6\xd6\xae\xca\xf5", "修缮之术", &lv);  // 修缮之术（GBK）；隊友學了也算
    if (it->type < 7 || it->type > 34 || it->type == 13 || it->type == 14 || it->type == 29 || it->type == 30) { hud_say("這個不能修理"); return; }
    if (it->hp_max <= 0 || it->hp >= it->hp_max) { hud_say("不需要修理"); return; }
    if (it->level > lv * 3) { hud_say(lv ? "修缮之术等級不夠" : "需要修缮之术"); return; }
    int price = it->value_un * (it->hp_max - it->hp) * 4 / it->hp_max;
    if (hero_money < price) { hud_msg(3020); return; }
    hero_money -= price;
    it->hp = it->hp_max;
    it->hit = 0;
    hud_say("修理完成");
}

// 鑑定（0x1004be60）：自己的慧眼识金，道具等級 ≤ 技能等級 × 3，花 (等級 + 4) × 10 精氣。shortcut: NPC 鑑定（花原價的錢）要商店介面，還沒做
static void item_identify(Actor *h, int p) {
    Item *it = &inv[p];
    int lv, mp = (it->level + 4) * 10;
    Actor *who = ui_skill_actor("\xbb\xdb\xd1\xdb\xca\xb6\xbd\xf0", "慧眼识金", &lv);  // 慧眼识金（GBK）；施展者的精氣
    (void)h;
    if (it->identify) { hud_say("不需要鑑定"); return; }
    if (it->level > lv * 3) { hud_say(lv ? "慧眼识金等級不夠" : "需要慧眼识金"); return; }
    if (who->mp < mp) { hud_say("精氣不足"); return; }
    who->mp -= mp;
    it->identify = 1;
    hud_say("鑑定完成");
}

// 整理：道具往前排，空格移到後面
static void bag_neaten(void) {
    int k = 0;
    for (int i = 0; i < 24; i++)
        if (inv[i].name[0]) { if (i != k) inv_swap(i, k); k++; }
    item_sel = -1;
}

// 耐久（0x1003c110 → 0x10056730）：每 40 次 HitPoint 扣 1 點耐久，剩 HP_MAX/8 + 1 時提醒，0 就脫下來
static void item_wear(Actor *h, int s) {
    Item *it = &inv[s];
    if (!it->name[0] || it->hp_max <= 0 || ++it->hit < 40) return;
    it->hit = 0;
    if (--it->hp == it->hp_max / 8 + 1) hud_say("裝備快要損壞了");
    if (it->hp <= 0) { it->hp = 0; hud_say("裝備損壞了"); item_unequip(h, s); }
}
// 物理攻擊（Attack方式 1、10）打中後：攻擊方隨機一件武器（類型 7~24，箭除外）磨損；
// 防守方千分比 200/500/700/800/900/1000 → 盔甲 25、26（33、34 同類）、27、28、31、32 那件磨損
static void hit_wear(Actor *a, Actor *t, const SkillRow *sk) {
    if (sk && sk->attack != 1 && sk->attack != 10) return;
    if (a == &actors[0]) {
        int w[2], n = 0;
        for (int s = 32; s <= 33; s++)
            if (inv[s].name[0] && inv[s].type >= 7 && inv[s].type <= 24 && inv[s].type != 13 && inv[s].type != 14) w[n++] = s;
        if (n) item_wear(a, w[rnd(n)]);
    }
    if (t == &actors[0]) {
        static const int C[6] = {200, 500, 700, 800, 900, 1000}, T[6] = {25, 26, 27, 28, 31, 32};
        int r = rnd(1000), k = 0;
        while (r >= C[k]) k++;
        for (int s = INV_BAG; s < INV_N; s++) {
            int ty = inv[s].type == 33 ? 25 : inv[s].type == 34 ? 26 : inv[s].type;
            if (inv[s].name[0] && ty == T[k]) { item_wear(t, s); break; }
        }
    }
}

// ---- 地上的道具（宣告在 unit.c 之前，near.c 要看格子上有沒有道具）----

static void ground_drop(const Item *it, float x, float y) {
    if (ground_n == MAX_GROUND || !it->name[0]) return;
    ground[ground_n].it = *it;
    ground[ground_n].it.pos = -1;
    if (debug_cells) printf("ground drop %s at %.1f,%.1f\n", it->name, x, y);
    int cx = (int)x, cy = (int)y, sx = cx, sy = cy;
    if (!free_cell(&cx, &cy, 10)) return;  // CPropObj 放進場景（0x49a350）：周圍 10 圈第一個空格，沒有就不放
    ground[ground_n].x = cx + 0.5f;
    ground[ground_n].id = ground_next_id++;
    ground[ground_n].y = cy + 0.5f;
    // 拋出動畫（0x49a350）：從原格中央 30 拍直線到落點格中央（每拍 1/30）
    float tx = cx * 64 + 32, ty = cy * 64 + 32;
    ground[ground_n].dx = sx * 64 + 32, ground[ground_n].dy = sy * 64 + 32;
    ground[ground_n].t = ground[ground_n].z = 0;
    float dist = sqrtf((tx - ground[ground_n].dx) * (tx - ground[ground_n].dx) + (ty - ground[ground_n].dy) * (ty - ground[ground_n].dy));
    ground[ground_n].vx = (int)dist < 1 ? 0 : (tx - ground[ground_n].dx) / PROP_TICKS;
    ground[ground_n].vy = (int)dist < 1 ? 0 : (ty - ground[ground_n].dy) / PROP_TICKS;
    ground_n++;
}

// 每拍（CPropObj 0x49a550）：前 PROP_TICKS-1 拍沿直線飛，高度 +PROP_STEP 到 PROP_SPLIT 拍、之後落回 0；最後一拍高度歸零。
// 怪現象（照原版）：只飛 29 步，停在落點前 1/30；PROP_SPLIT（DAT_00663f24）沒有任何寫入處 = 0，所以高度一直是 0。
// shortcut: 起飛／落地音（物件模型 +0x32／+0x2e）、地面動畫幀、地上光照／陰影還沒做，等道具模型表
// 地上道具模型（ObjMdl/PropMdl.txt：名称、地面图片ID、图片ID、落地声音、下落声音；CPropObj +0x85 的模型，0x49a070 設）：
// 依道具的地面圖 G_PicID 找列；0x49a550 在第 1 拍播 +0x32（下落声音）、第 29 拍（落地）播 +0x2e（落地声音），空欄 = 0xffff 不播
static int prop_sound(int ground_id, int landing) {
    static int loaded, n, tab[256][3];
    if (!loaded) {
        loaded = 1;
        size_t len;
        uint8_t *d = res_file("ObjMdl", "PropMdl.txt", &len);
        if (d) {
            if (len && d[0] == 0x80) len = res_unxor(d, len);
            char *save = NULL;
            strtok_r((char *)d, "\r\n", &save);  // 表頭
            for (char *l; n < 256 && (l = strtok_r(NULL, "\r\n", &save));) {
                char *f[6] = {0};
                int k = 0;
                for (char *p = l; k < 6; k++) { f[k] = p; p = strchr(p, '\t'); if (!p) { k++; break; } *p++ = 0; }
                if (k < 5) continue;
                tab[n][0] = atoi(f[1]);
                tab[n][1] = f[3][0] ? atoi(f[3]) : 0xffff;
                tab[n][2] = f[4][0] ? atoi(f[4]) : 0xffff;
                n++;
            }
            free(d);
        }
    }
    for (int i = 0; i < n; i++)
        if (tab[i][0] == ground_id) return tab[i][landing ? 1 : 2];
    return 0xffff;
}
static void ground_tick(void) {
    for (int i = 0; i < ground_n; i++) {
        if (++ground[i].t >= PROP_TICKS) continue;
        int snd = ground[i].t == PROP_TICKS - 1 ? prop_sound(ground[i].it.ground, 1) : ground[i].t == 1 ? prop_sound(ground[i].it.ground, 0) : 0xffff;
        if (snd != 0xffff) audio_sfx(snd);
        if (ground[i].t < PROP_SPLIT) ground[i].z += PROP_STEP;
        else if (ground[i].z > 0) ground[i].z -= PROP_STEP;
        ground[i].dx += ground[i].vx, ground[i].dy += ground[i].vy;
        if (ground[i].t == PROP_TICKS - 1) { ground[i].z = 0; ground[i].t = PROP_TICKS; }
    }
}

// 怪物掉寶（0x1006fa50）：掉 最小～最大 件，每件擲千分比依序對五組（類型、機率×10）累加，中的那組產生一件，
// 隨機等級用 SLevel_i 第「怪物等級 / 5 + 1」欄（最多 9）。原版出生時就產生、倒下時丟出來；這裡倒下時才產生，機率一樣
static void mon_loot(const Actor *m) {
    const MonDef *d = m->mon;
    int n = d->drop_max > d->drop_min ? rnd((int)(d->drop_max - d->drop_min) + 1) + (int)d->drop_min : (int)d->drop_min;
    int col = m->level / 5 + 1 > 9 ? 9 : m->level / 5 + 1;
    for (int k = 0; k < n; k++) {
        int r = rnd(1000);
        float sum = 0;
        for (int j = 0; j < 5; j++) {
            if (d->drop[j][1] <= 0) continue;
            sum += d->drop[j][1] * 10;
            if (r >= sum) continue;
            Item it;
            if (!item_new(&it, (int)d->drop[j][0], -1, col, NULL)) break;
            ground_drop(&it, m->x, m->y);
            printf("掉落 類型 %d 等級 %d\n", it.type, it.level);
            break;
        }
    }
}

// 撿起來（同時是 CPropObj 移除 0x49a880：格子道具記號隨 ground[] 一起消失）：錢直接加到金錢（金钱 = 類型 60，數量在 Amount），其他放進背包空格
#ifdef STORY
static void story_begin(void);
static void story_end(void);
#endif
static void ground_pick(int g) {
    Item *it = &ground[g].it;
#ifdef STORY
    int sh = it->sh;  // 劇情道具（箱子掉的）：撿起後要補物件第 15 格
#endif
    if (it->type == 60) hero_money += it->amount;
    else {
        int b = bag_free();
        if (b < 0) { hud_msg_hero(1118); return; }  // 1118 身上的東西太多（0x1001f4e0 傳回 −1）；1117 can_get = 0 的搬不動，單機地上道具都能撿
        burden_warn(it);
        inv[b] = *it;
        inv[b].pos = b;
        char nm[96], got[128]; gbk_utf8(it->name, nm, sizeof nm); snprintf(got, sizeof got, "撿起 %s", nm); hud_say(got);  // shortcut: 提示文字是自訂（原版撿起沒有找到訊息）
    }
    ground[g] = ground[--ground_n];
#ifdef STORY
    if (sh) { story_begin(); story_item_moved(sh, story_player(), sroom); story_end(); }  // 房間 → 主角：物件第 15 格
#endif
}

// 按鈕名稱 → 開的視窗（主列和人物屬性視窗裡的 m_pRole／m_pSkill／m_pProp 同名同作用）
static void sys_toggle(void);
static void ui_text(int id, char *out, size_t cap);
static void ui_command(const char *name) {
    if (!strcmp(name, "m_pRole")) win_toggle(&win_left, JM52);
    else if (!strcmp(name, "m_pSkill")) { skill_mode = 1; win_toggle(&win_right, JM53); }
    else if (!strcmp(name, "m_pProp")) win_toggle(&win_right, JM54);
    else if (!strncmp(name, "m_pPage[", 8)) skill_page = atoi(name + 8) - 1;  // 技能頁籤 0x560550：m_pPage[1]~[5] = 第 0~4 頁（[6] 藏著）
    else if (!strncmp(name, "m_pSkill[", 9)) pick_open(name[9] == '1');  // 主列 m_pSkill[0] 右手、[1] 左手
    else if (!strcmp(name, "m_pEquip")) win_toggle(&win_left, JM55);
    else if (!strcmp(name, "m_pMap")) map_show();  // 主列的地圖鈕（JM51 中間 m_pMap）開 JM60
    else if (!strcmp(name, "m_pSystem")) sys_toggle();  // 道具視窗的装备：原版開在中線左邊（Poq.exe 0x54ad87）
}

// 畫面上點到哪件地上的道具（圖示置中在格子中央）
static int ground_at(int px, int py, int ox, int oy, const Scene *s) {
    for (int i = ground_n - 1; i >= 0; i--) {
        int fx = scene_px(s, (int)(ground[i].x * 64), (int)(ground[i].y * 64)) - ox, fy = scene_py(s, (int)(ground[i].x * 64), (int)(ground[i].y * 64)) - oy;
        if (abs(px - fx) <= 24 && abs(py - fy) <= 20) return i;
    }
    return -1;
}

// 道具視窗（JM54）的格子和按鈕。點道具 = 選取，再點一次 = 使用／裝備；選了以後點空格 = 移過去
// shortcut: 原版用滑鼠拖放、右鍵使用，手機改成點選；打造（m_pMake）、镶嵌（m_pEnchase）、查看（m_LookOver）另外的系統，還沒做
#ifdef STORY
static int shop_bag_act(int p);
static int store_bag_act(int p);
static int make_self;  // 打造介面是從背包（JM54 的 m_pMake／m_pEnchase，Poq.exe 0x54af70／0x54b620）開的 = 用自己的技能，不是商人
static void make_open(int mode);
#endif
// 合併箭堆（文字指令 c_arrow 0x100133c0，客戶端把一堆拖到另一堆時送）：dst 有數量上限 500（.data 0x1039f554），超過的留在 src。
// 條件照原版：dst 類型 13 或 src 類型 14（原版 `Type(a) == 13 || Type(b) == 14`，不要求兩邊同類）；不符合不動。回 1 = 有合併
#define ARROW_MAX 500
static int arrow_merge(int dst, int src) {
    if (dst == src || !inv[dst].name[0] || !inv[src].name[0] || !(inv[dst].type == 13 || inv[src].type == 14)) return 0;
    int n = inv[dst].arrows + inv[src].arrows, rest = n > ARROW_MAX ? n - ARROW_MAX : 0;
    inv[dst].arrows = n - rest;
    if (rest) inv[src].arrows = rest;
    else inv[src].name[0] = 0;
    return 1;
}
static void bag_use(int p) {
    if (inv[p].part > 0) item_equip(&actors[0], p);
    else if (!item_eat(&actors[0], p, 0) && !(inv[p].fx_n && item_eat(&actors[0], p, 1))) hud_say("不能使用");  // shortcut: 原版 use 連沒特性的道具也會用掉，這裡只讓有特性的用
}
static void bag_button(const char *name) {
    int p = !strncmp(name, "m_pItem[", 8) ? atoi(name + 8) : !strncmp(name, "m_pShortcutItem[", 16) ? 24 + atoi(name + 16) : -1;
    int sel = item_sel >= 0 && inv[item_sel].name[0] ? item_sel : -1;
    if (p >= 0) {
#ifdef STORY
        if (p == sel && (shop_bag_act(p) || store_bag_act(p))) return;
#endif
        if (p == sel) bag_use(p);
        else if (inv[p].name[0]) { if (sel >= 0) arrow_merge(p, sel); item_sel = p; }  // 選中箭堆再點另一堆 = 合併（c_arrow）
        else if (sel >= 0) { inv_swap(sel, p); item_sel = p; }
    } else if (!strcmp(name, "m_pNeaten")) bag_neaten();
#ifdef STORY
    else if (!strcmp(name, "m_pMake") || !strcmp(name, "m_pEnchase")) { make_self = 1; make_open(name[3] == 'M' ? -1 : 1); }
#endif
    else if (sel < 0) { if (!strcmp(name, "m_pEquip")) ui_command(name); }
    else if (!strcmp(name, "m_pUse")) bag_use(sel);
    else if (!strcmp(name, "m_pRepair")) item_repair(sel);
    else if (!strcmp(name, "m_pIdentify")) item_identify(&actors[0], sel);
    else if (!strcmp(name, "m_pReject")) {  // 丢弃：放到主角腳下
        ground_drop(&inv[sel], actors[0].x, actors[0].y);
        inv[sel].name[0] = 0;
        item_sel = -1;
    } else ui_command(name);
}

#ifdef STORY
// ---- 買賣（原版交易介面 JM63，Poq.exe CTradeDialog；買賣規則在 EasyRPG.dll）----
// 劇情選項叫 0x10055990(主角, 商人) → 左邊開 JM63、右邊開道具 JM54；商人的貨 = 劇情物件身上的道具（add_item、drop_items）。
// 頁籤（0x368；點頁籤 0x558850、0x5588a0、0x5588f0、0x558940）：m_pWeapons 3、m_pShield 2、m_pLeechdom 1、m_pOther 0，開的時候是 3（0x55aed0）。
// 每頁列商人身上該類的前 20 件（EasyRPG 0x1003aef0）：類型 0~6、49、50、52 → 0；51 → 1；25~34 → 2；7~24 → 3
#define JM56 "JM56_\xb4\xf2\xd4\xec\xbd\xe7\xc3\xe6"  // JM56_打造界面
#define JM63 "JM63_\xbd\xbb\xd2\xd7\xbd\xe7\xc3\xe6"  // JM63_交易界面
static int shop_npc, shop_tab = 3, shop_sel = -1, shop_slot[20], shop_n;
// 游標模式（原版 m_pBuy 0x558ac0 → 7、m_pSell 0x558b20 → 8，修理、鑑定同樣是游標）：在這個模式下點道具 = 做這件事
enum { SHOP_NONE, SHOP_BUY = 7, SHOP_SELL, SHOP_REPAIR, SHOP_IDENTIFY };
static int shop_mode;

static int shop_kind(int t) {
    return (t >= 0 && t <= 6) || t == 49 || t == 50 || t == 52 ? 0 : t == 51 ? 1 : t >= 25 && t <= 34 ? 2 : t >= 7 && t <= 24 ? 3 : -1;
}

static void shop_list(void) {
    shop_n = 0;
    for (int h = story_next(shop_npc, 0); h && shop_n < 20; h = story_next(shop_npc, h)) {
        const Item *it = story_item(h);
        if (it && shop_kind(it->type) == shop_tab) shop_slot[shop_n++] = h;
    }
    if (shop_sel >= shop_n) shop_sel = -1;
}

static void ui_show(UiDialog *d, const char *name, int on) {
    UiCtrl *c = ui_find(d, name);
    if (c) c->flags = on ? c->flags | 9 : c->flags & ~9;
}

// 買賣價（EasyRPG 0x10029400）：沒鑑定的用 ValueUn。c = 主角魅力、m = 隊伍平均魅力（整數除法；主角加上 Friend > 1 的隊友 = 隊員，2026-11-21 補上隊友）。
// 賣 = 價 × (c+20) × (m+20) / (c+50) / (m+50)；買 = 價 × (c+50) × (m+50) / (c+20) / (m+20)，沒鑑定的買價再 ×10；上限 99999999
static int trade_price(const Item *it, int buy) {
    int hc = hero_attr[CHARM] + actors[0].ex[33], sum = hc, cnt = 1;
    for (int i = 1; i < actor_count; i++)
        if (actors[i].role && actors[i].team && alive(&actors[i])) { sum += actors[i].team_attr[CHARM] + actors[i].ex[33]; cnt++; }
    float c = (float)hc, m = (float)(sum / cnt), v = (float)(it->identify ? it->value : it->value_un);
    int p = buy ? (int)((c + 50) * v * (m + 50) / (c + 20) / (m + 20)) : (int)((c + 20) * v * (m + 20) / (c + 50) / (m + 50));
    if (buy && !it->identify) p *= 10;
    return p < 0 || p > 99999999 ? 99999999 : p;
}
// 找 NPC 修理（0x100587c0 第 2 種）= ValueUn × 損耗 × 2 / 耐久上限；鑑定（0x100586e0 第 2 種）= ValueUn
static int npc_repair_price(const Item *it) { return it->hp_max > 0 ? it->value_un * (it->hp_max - it->hp) * 2 / it->hp_max : 0; }

static void shop_open(int npc) {
    shop_npc = npc;
    shop_tab = 3;
    shop_sel = item_sel = -1;
    shop_mode = SHOP_NONE;
    win_left.open = ui_load(JM63, &win_left.d) > 0;
    if (!(win_right.open && !strcmp(win_right.d.name, JM54))) win_toggle(&win_right, JM54);
    // 商人的 UIS_Repair、UIS_Identify、UIS_Produce 決定修理、鑑定、打造／镶嵌能不能用（0x10055990 → 0x55adb0）。
    // shortcut: 寄存（m_pKeep、m_pPop、m_pCheck）是另一個介面 JM73，先藏起來；按鈕的確切顯示規則（0x558990）沒照翻
    int produce = story_get_int(npc, "UIS_Produce") > 0;
    ui_show(&win_left.d, "m_pRepair", story_get_int(npc, "UIS_Repair") > 0);
    ui_show(&win_left.d, "m_pIdentify", !produce && story_get_int(npc, "UIS_Identify") > 0);
    ui_show(&win_left.d, "m_pMake", produce);
    ui_show(&win_left.d, "m_pEnchase", produce);
    ui_show(&win_left.d, "m_pKeep", 0);
    ui_show(&win_left.d, "m_pPop", 0);
    ui_show(&win_left.d, "m_pCheck", 0);
    shop_list();
}

// 交易視窗開著世界暫停（CTradeDialog 顯示掛鉤 0x55a680 → 0x532570(1) 暫停計數 +1；連線版 0x4f6960 == 2 不暫停）。
// shortcut: 原版是計數式，這裡每拍看交易視窗開不開（開著就停），關窗自動恢復
static int trade_paused(void) { return win_left.open && !strcmp(win_left.d.name, JM63); }

static void shop_close(void) {
    shop_npc = 0;
    shop_mode = SHOP_NONE;
    if (win_left.open && (!strcmp(win_left.d.name, JM63) || !strcmp(win_left.d.name, JM56))) win_left.open = 0;
}

// 買（EasyRPG buy 0x10012ef0；介面判斷 0x55b980）；錢付給商人，道具放進背包空格
static void shop_buy(int i) {
    if (i < 0 || i >= shop_n) return;
    Item *it = story_item(shop_slot[i]);
    int p = trade_price(it, 1), b = bag_free();
    if (hero_money < p) {  // 介面先擋（CTradeDialog 買 0x55b980：價格 > Money 不送封包、秀 UI_C 1508）；0x10012ef0 伺服器端的 1185 不會到
        char m[128];
        ui_text(1508, m, sizeof m);
        hud_say(m);
        return;
    }
    if (b < 0) { hud_say("背包已滿"); return; }
    hero_money -= p;
    story_set_int(shop_npc, "Money", story_get_int(shop_npc, "Money") + p);
    inv[b] = *it;
    inv[b].pos = b;
    inv[b].sh = shop_slot[i];
    free(it);
    story_hold(shop_slot[i]);
    shop_sel = -1;
    shop_list();
}

// 賣（EasyRPG sell 0x1001cfa0）：價格加到主角的錢，道具給商人（之後可以照買價買回來）
static void shop_sell(int p) {
    Item *it = malloc(sizeof *it);
    if (!it) return;
    hero_money += trade_price(&inv[p], 0);
    *it = inv[p];
    it->pos = -1;
    if (it->sh) { story_move(it->sh, shop_npc); story_set_item(it->sh, it); }
    else it->sh = story_put(shop_npc, it);
    inv[p].name[0] = 0;
    item_sel = -1;
    shop_list();
}

static void shop_repair(int p) {
    Item *it = &inv[p];
    int price = npc_repair_price(it);
    if (it->type == 13 || it->type == 14 || it->hp_max <= 0 || it->hp >= it->hp_max) { hud_say("不需要修理"); return; }
    if (hero_money < price) { hud_msg(3020); return; }
    hero_money -= price;
    it->hp = it->hp_max;
    it->hit = 0;
}

static void shop_identify(int p) {
    Item *it = &inv[p];
    if (it->identify) { hud_say("不需要鑑定"); return; }
    if (hero_money < it->value_un) { hud_msg(3018); return; }
    hero_money -= it->value_un;
    it->identify = 1;
}

// 在游標模式下對背包的道具做事；回 1 = 做了
static int make_on(void);
static int make_bag_act(int p);
static void ui_text(int id, char *out, size_t cap);
static void item_icon(UiDialog *d, const char *key, int wx, int wy, const Item *it);
static void item_icon_fit(UiDialog *d, const char *key, int wx, int wy, const Item *it);
static int shop_bag_act(int p) {
    if ((shop_npc || make_self) && p >= 0 && p < INV_BAG && inv[p].name[0] && make_on()) return make_bag_act(p);
    if (!shop_npc || p < 0 || p >= INV_BAG || !inv[p].name[0]) return 0;
    if (shop_mode == SHOP_SELL) shop_sell(p);
    else if (shop_mode == SHOP_REPAIR) shop_repair(p);
    else if (shop_mode == SHOP_IDENTIFY) shop_identify(p);
    else return 0;
    return 1;
}

// JM63 的按鈕。shortcut: 原版游標模式點一下就做；手機改成點第一下選取、看價格，再點一下（或按該模式的按鈕）才做
static void shop_button(const char *name) {
    static const struct { const char *n; int tab; } tabs[] = {{"m_pWeapons", 3}, {"m_pShield", 2}, {"m_pLeechdom", 1}, {"m_pOther", 0}};
    for (int i = 0; i < 4; i++)
        if (!strcmp(name, tabs[i].n)) { shop_tab = tabs[i].tab; shop_sel = -1; shop_list(); return; }
    int sel = item_sel >= 0 && item_sel < INV_BAG && inv[item_sel].name[0] ? item_sel : -1;
    if (!strncmp(name, "m_pItem[", 8)) {
        int i = atoi(name + 8);
        if (i < shop_n && (i == shop_sel || shop_mode == SHOP_BUY)) shop_buy(i);
        else shop_sel = i < shop_n ? i : -1;
    } else if (!strcmp(name, "m_pBuy")) {
        if (shop_sel >= 0) shop_buy(shop_sel);
        else { shop_mode = SHOP_BUY; hud_say("買：點商品"); }
    } else if (!strcmp(name, "m_pSell") || !strcmp(name, "m_pRepair") || !strcmp(name, "m_pIdentify")) {
        shop_mode = name[3] == 'S' ? SHOP_SELL : name[3] == 'R' ? SHOP_REPAIR : SHOP_IDENTIFY;
        shop_sel = -1;
        if (sel >= 0) shop_bag_act(sel);
        else hud_say(shop_mode == SHOP_SELL ? "賣：點背包的道具" : shop_mode == SHOP_REPAIR ? "修理：點背包的道具" : "鑑定：點背包的道具");
    } else if (!strcmp(name, "m_pMake") || !strcmp(name, "m_pEnchase")) { make_self = 0; make_open(name[3] == 'M' ? -1 : 1); }
    else if (!strcmp(name, "m_pCClose")) shop_close();
}

// ---- 打造、镶嵌（原版 JM56_打造界面，Poq.exe CMakePropDialog 0x540230；規則在 EasyRPG.dll）----
// 交易介面的 m_pMake（0x558f40，模式 −1 = 打造）、m_pEnchase（0x559390，模式 1 = 镶嵌）打開，取代左邊的交易介面，關掉回交易。
// 打造：配方 ObjMdl/MakeMdl.txt；按鈕 m_pMakeType1~8 選類別、m_pTypeList 選配方；材料格 m_pMaterial[15] 是 5 欄 × 3 列，
// 第 c 欄 = 道具類型 c+1（木材、矿石、兽骨、兽皮、兽筋，m_pMaterialType 的圖），每欄要配方的件數。打造（0x205 → produce 0x10051530）：
// 找 NPC 打造 = 等級是商人的 UIS_Produce、錢 = 材料 ValueUn 總和（不夠 3025）；產品 = item_new(類型, 等級)；
// 主材（第一件）的類型 = 配方主材就用它的五行；材料的特性合併（同屬性相加，金色特性只取主材類型的材料）後加到產品上，
// 等級需求取最大、價格／重量／力敏體需求相加，單機價格再 ×1.2（0x1023991c）；雜項（類型 ≥ 1000）只產生、不合併。
// 镶嵌（addgem 0x10011a20 → 0x10037690）：m_pProduct 放武器或防具（類型 7~34，13、14 箭矢除外，1215），寶石（類型 0，否則 1298）
// 放上去；鑲滿 SlotMax 顆（1299）。寶石的特性依道具種類過濾（0x10026fc0：武器去掉防禦類、防具去掉攻擊類）後加上，價格 + 寶石價 ×1.5。
// m_pBackout 拆除寶石（removegem 0x10053a40）：寶石毀掉，特性拿掉，價格扣回寶石價 ×1.5（不低於 0）。
// 自己用技能打造（背包 m_pMake，扣精氣 (等級+10)×5）、五行屬性換算 0x10058900 已做。shortcut: 打造中的進度動畫 m_pProductFinish 還沒做；
// 類別按鈕對配方類別是照按鈕名稱對的（刀剑 0、斧锤 4、棍杖 2、抢矛 3、弓箭 1、盾甲 5、配饰 6、其它 7）
static const int MAKE_CAT[8] = {0, 4, 2, 3, 1, 5, 6, 7};
static int make_mode, make_cat, make_rec = -1, make_mat[15], make_item = -1, make_gem = -1;

static int make_on(void) { return win_left.open && !strcmp(win_left.d.name, JM56); }

static int make_rows(int *rows) {
    const Recipe *r;
    int n = recipes_load(&r), k = 0;
    for (int i = 0; i < n && k < 16; i++)
        if (r[i].cat == make_cat) rows[k++] = i;
    return k;
}
static const Recipe *make_recipe(void) {
    const Recipe *r;
    return make_rec >= 0 && make_rec < recipes_load(&r) ? &r[make_rec] : NULL;
}

static void make_open(int mode) {
    make_mode = mode;
    make_rec = make_item = make_gem = -1;
    make_cat = 0;
    for (int i = 0; i < 15; i++) make_mat[i] = -1;
    if (!(win_left.open = ui_load(JM56, &win_left.d) > 0)) return;
    ui_show(&win_left.d, "m_pMakerInfo", 0);  // shortcut: 原版在這裡寫打造者的說明（和配方列表重疊），還沒查它何時顯示
    // 兩種模式各自顯示的元件（0x543120）
    static const char *mk[] = {"m_pMakeType", "m_pMakeBK", "m_pMake", "m_pClear", "m_pTypeList", "m_pMaterial[", "m_pMaterialType"};
    static const char *gm[] = {"m_pProductBK", "m_pProductBox", "m_pProduct", "m_pPropName", "m_pGem[", "m_pBackout"};
    for (int i = 0; i < win_left.d.count; i++) {
        UiCtrl *c = &win_left.d.c[i];
        for (size_t j = 0; j < sizeof mk / sizeof *mk; j++)
            if (!strncmp(c->name, mk[j], strlen(mk[j]))) c->flags = mode < 0 ? c->flags | 9 : c->flags & ~9;
        for (size_t j = 0; j < sizeof gm / sizeof *gm; j++)
            if (!strncmp(c->name, gm[j], strlen(gm[j]))) c->flags = mode > 0 ? c->flags | 9 : c->flags & ~9;
    }
}

static int make_used(int p) {
    for (int i = 0; i < 15; i++)
        if (make_mat[i] == p) return 1;
    return 0;
}

// 合併一件材料的特性（0x10051530；特性清單合併 0x10032740：同屬性、同種類、同百分比的相加）
static void fx_merge(ItemFx *list, int *n, const ItemFx *f) {
    for (int i = 0; i < *n; i++)
        if (list[i].id == f->id && list[i].kind == f->kind && list[i].pct == f->pct) { list[i].v += f->v; return; }
    if (*n < 32) list[(*n)++] = *f;
}

// 自己用技能打造（0x54af70 開窗、模式 0）：要有「打造之术」，等級 = 技能等級，扣精氣 (等級 + 10) × 5（0x1004b8b0 模式 0；不夠 3024），不花錢
static void make_produce(void) {
    const Recipe *r = make_recipe();
    if (!r || !recipe_ready(r, make_mat)) return;  // 材料還沒放齊（原版打造鈕是灰的）
    int self_lv = 0;
    Actor *maker = &actors[0];
    if (make_self) maker = ui_skill_actor("\xb4\xf2\xd4\xec\xd6\xae\xca\xf5", "打造之术", &self_lv);  // 打造之术（GBK）；隊友學了也算
    if (make_self && self_lv < 1) return;  // 0x541c50：沒有技能、也沒商人等級，不送封包
    int cost = 0, first = -1;
    for (int c = 0; c < 5; c++)
        for (int k = 0; k < r->need[c]; k++) {
            int p = make_mat[k * 5 + c];
            if (p < 0 || !inv[p].name[0]) return;
            cost += inv[p].value_un;
        }
    if (r->main >= 1 && r->main <= 5) first = make_mat[r->main - 1];
    int mp_cost = (self_lv + 10) * 5;
    if (make_self ? maker->mp < mp_cost : hero_money < cost) { hud_msg(make_self ? 3024 : 3025); return; }
    Item out;
    int level = make_self ? self_lv : story_get_int(shop_npc, "UIS_Produce");
    if (!item_new(&out, r->type, level > 0 ? level : 1, -1, NULL)) { hud_msg(3014, r->type); return; }
    if (make_self) maker->mp -= mp_cost; else hero_money -= cost;
    if (out.type >= 7 && out.type <= 34) {
        out.fe = first >= 0 && inv[first].type == r->main ? inv[first].fe : -1;  // 原版 FE 一律設（沒有主材 = −1）
        for (int i = 0; i < out.fx_n;) {  // 產品自己的五行特性依 FE 換算，沒有五行就拿掉
            int id = out.fx[i].id, c = id >= 41 && id <= 58 ? fe_conv(out.fe, id) : id;
            if (c < 0) memmove(&out.fx[i], &out.fx[i + 1], (--out.fx_n - i) * sizeof out.fx[0]);
            else out.fx[i++].id = c;
        }
        ItemFx add[32];
        int n = 0;
        for (int i = 0; i < 15; i++) {
            const Item *m = make_mat[i] >= 0 ? &inv[make_mat[i]] : NULL;
            if (!m) continue;
            for (int j = 0; j < m->fx_n; j++)
                if (m->fx[j].kind != 3 || m->type == r->main) {
                    ItemFx f = m->fx[j];
                    int c = f.id >= 41 && f.id <= 58 ? fe_conv(out.fe, f.id) : f.id;
                    if (c >= 0) f.id = c;  // 材料的特性：換不出來就保留原樣（0x10051530 第二輪）
                    fx_merge(add, &n, &f);
                }
            if (m->lv_req > out.lv_req) out.lv_req = m->lv_req;
            out.value += m->value > 0 ? m->value : 0;
            out.weight += m->weight > 0 ? m->weight : 0;
            out.str_req += m->str_req > 0 ? m->str_req : 0;
            out.con_req += m->con_req > 0 ? m->con_req : 0;
            out.dex_req += m->dex_req > 0 ? m->dex_req : 0;
        }
        for (int i = 0; i < n && out.fx_n < 32; i++) out.fx[out.fx_n++] = add[i];
        out.value = (int)(out.value * 1.2f);
    }
    for (int i = 0; i < 15; i++)
        if (make_mat[i] >= 0) { inv[make_mat[i]].name[0] = 0; make_mat[i] = -1; }
    item_sel = -1;
    int b = bag_free();
    if (b < 0) ground_drop(&out, actors[0].x, actors[0].y);
    else { inv[b] = out; inv[b].pos = b; }
    hud_msg(1214, out.name);
}

static void gem_add(int g) {
    Item *w = &inv[make_item];
    int max = w->slot_max < 3 ? w->slot_max : 3;
    if (w->gem_n >= max) { hud_msg(1299); return; }
    const Item *gem = &inv[g];
    int weapon = w->type >= 7 && w->type <= 24;
    for (int i = 0; i < gem->fx_n && w->fx_n < 32; i++) {
        int id = gem->fx[i].id;
        int def = (id >= 53 && id <= 58) || (id >= 62 && id <= 64) || id == 66 || (id >= 75 && id <= 77) || (id >= 92 && id <= 98);
        int atk = (id >= 41 && id <= 52) || id == 65 || (id >= 67 && id <= 74) || (id >= 80 && id <= 91);
        if (weapon ? def : atk) continue;
        w->fx[w->fx_n] = gem->fx[i];
        w->fx[w->fx_n++].gem = w->gem_n + 1;
        gem_fx(w, &gem->fx[i], 1);
    }
    w->gem_icon[w->gem_n] = gem->icon;
    w->gem_value[w->gem_n++] = gem->value;
    w->value += (int)(gem->value * 1.5f);
    inv[g].name[0] = 0;
    item_sel = -1;
}

static void gem_remove(int k) {
    Item *w = &inv[make_item];
    if (k < 0 || k >= w->gem_n) return;
    int n = 0, v = (int)(w->gem_value[k] * 1.5f);
    for (int i = 0; i < w->fx_n; i++) {
        if (w->fx[i].gem == k + 1) { gem_fx(w, &w->fx[i], -1); continue; }
        w->fx[n] = w->fx[i];
        if (w->fx[n].gem > k + 1) w->fx[n].gem--;
        n++;
    }
    w->fx_n = n;
    w->value -= v < w->value ? v : w->value;
    for (int i = k; i + 1 < w->gem_n; i++) { w->gem_icon[i] = w->gem_icon[i + 1]; w->gem_value[i] = w->gem_value[i + 1]; }
    w->gem_n--;
    make_gem = -1;
}

// 打造介面開著時，在背包點兩下道具：打造模式 = 放進它那一欄的材料格；镶嵌模式 = 放上武器、或鑲上寶石
static int make_bag_act(int p) {
    int t = inv[p].type;
    if (make_mode < 0) {
        const Recipe *r = make_recipe();
        if (!r || t < 1 || t > 5 || make_used(p)) return 1;
        for (int k = 0; k < r->need[t - 1]; k++)
            if (make_mat[k * 5 + t - 1] < 0) { make_mat[k * 5 + t - 1] = p; return 1; }
        return 1;
    }
    if (t == 0 && make_item >= 0) gem_add(p);
    else if (t >= 7 && t <= 34 && t != 13 && t != 14) { make_item = p; make_gem = -1; }
    else if (make_item < 0) hud_msg(1215, inv[p].name, t);
    else hud_msg(1298, inv[p].name, t);
    return 1;
}

static void make_button(const char *name) {
    if (!strncmp(name, "m_pMakeType", 11)) { make_cat = MAKE_CAT[(atoi(name + 11) + 7) % 8]; make_rec = -1; }
    else if (!strncmp(name, "m_pMaterial[", 12)) make_mat[atoi(name + 12)] = -1;
    else if (!strcmp(name, "m_pClear")) { for (int i = 0; i < 15; i++) make_mat[i] = -1; }
    else if (!strcmp(name, "m_pMake")) make_produce();
    else if (!strcmp(name, "m_pProduct")) make_item = make_gem = -1;
    else if (!strncmp(name, "m_pGem[", 7)) make_gem = atoi(name + 7);
    else if (!strcmp(name, "m_pBackout")) {
        if (make_item >= 0) gem_remove(make_gem >= 0 ? make_gem : inv[make_item].gem_n - 1);
    } else if (!strcmp(name, "m_pCClose")) { if (shop_npc) shop_open(shop_npc); else win_left.open = 0; make_self = 0; }
}

// 配方列表一行 16 點
static int make_list_tap(int x, int y, int wx, int wy) {
    UiCtrl *c = ui_find(&win_left.d, "m_pTypeList");
    if (!c || make_mode >= 0 || x < wx + c->x || x >= wx + c->x + c->w || y < wy + c->y || y >= wy + c->y + c->h) return 0;
    int rows[16], n = make_rows(rows), i = (y - wy - c->y - 2) / 16;
    if (i >= 0 && i < n && rows[i] != make_rec) { make_rec = rows[i]; for (int k = 0; k < 15; k++) make_mat[k] = -1; }
    return 1;
}

static void make_draw(UiDialog *d, int wx, int wy) {
    char key[24], t[256];
    UiCtrl *c;
    if (make_mode < 0) {
        const Recipe *r;
        recipes_load(&r);
        int rows[16], n = make_rows(rows);
        if ((c = ui_find(d, "m_pTypeList")))
            for (int i = 0; i < n && 2 + i * 16 + 16 <= c->h; i++) {
                ui_text(r[rows[i]].text, t, sizeof t);
                draw_text(wx + c->x + 4, wy + c->y + 2 + i * 16, t, c->w - 8, rows[i] == make_rec ? 0xFFE080 : 0xBECDCE, 1);
            }
        const Recipe *rec = make_recipe();
        for (int i = 0; i < 15; i++) {
            snprintf(key, sizeof key, "m_pMaterial[%d]", i);
            if ((c = ui_find(d, key)) && !(rec && i / 5 < rec->need[i % 5])) {
                const Sprite *s = ui_image(c->img, 0);  // 這格不用放：蓋上暗色
                if (s) { SDL_SetRenderDrawColor(ren, 0, 0, 0, 160); SDL_RenderFillRect(ren, &(SDL_Rect){wx + c->x, wy + c->y, s->w, s->h}); }
            }
            if (make_mat[i] >= 0) item_icon(d, key, wx, wy, &inv[make_mat[i]]);
        }
        return;
    }
    if (make_item < 0 || !inv[make_item].name[0]) { make_item = -1; return; }
    const Item *w = &inv[make_item];
    item_icon(d, "m_pProduct", wx, wy, w);
    for (int i = 0; i < w->gem_n; i++) {
        snprintf(key, sizeof key, "m_pGem[%d]", i);
        item_icon(d, key, wx, wy, &(Item){.name = "g", .icon = w->gem_icon[i], .identify = 1});
        snprintf(key, sizeof key, "m_pGemBox[%d]", i);
        if (i == make_gem && (c = ui_find(d, key))) draw(ui_image(c->img, 0), wx + c->x, wy + c->y);
    }
    if ((c = ui_find(d, "m_pPropName"))) {
        gbk_utf8(w->name, t, sizeof t);
        draw_text(wx + c->x, wy + c->y, t, c->w, 0xBECDCE, 1);
    }
}

// ---- 寄存（原版 JM73_寄存界面）：商人選項叫 0x1004ec80 → 0x61b 訊息打開，左邊寄存、右邊道具。40 格
// 店家的台詞說「小店还可以为你将寄存物品运送到全国各地」，所以全國共用一個寄存箱。
// 點寄存的道具 = 選取，再點 = 拿回背包；背包選了道具點寄存空格 = 放進那格；背包的道具點兩下 = 放進第一個空格。
// shortcut: 原版寄存的道具放在哪（EasyRPG 那邊的物件、UI_Pos）還沒查，存檔時要一起存
#define JM73 "JM73_\xbc\xc4\xb4\xe6\xbd\xe7\xc3\xe6"  // JM73_寄存界面
#define STORE_N 40
static Item store[STORE_N];
static int store_sel = -1;

static int store_on(void) { return win_left.open && !strcmp(win_left.d.name, JM73); }

static void store_open(void) {
    store_sel = item_sel = -1;
    win_left.open = ui_load(JM73, &win_left.d) > 0;
    if (!(win_right.open && !strcmp(win_right.d.name, JM54))) win_toggle(&win_right, JM54);
}

static int store_bag_act(int p) {
    if (!store_on() || p < 0 || p >= INV_BAG || !inv[p].name[0]) return 0;
    for (int i = 0; i < STORE_N; i++)
        if (!store[i].name[0]) { store[i] = inv[p]; store[i].pos = -1; inv[p].name[0] = 0; item_sel = -1; return 1; }
    hud_say("寄存箱滿了");
    return 1;
}

static void store_button(const char *name) {
    if (!strcmp(name, "m_pCClose")) { win_left.open = 0; return; }
    if (strncmp(name, "m_pItem[", 8)) return;
    int i = atoi(name + 8), b;
    if (i < 0 || i >= STORE_N) return;
    if (i == store_sel && store[i].name[0]) {
        if ((b = bag_free()) < 0) { hud_say("背包已滿"); return; }
        burden_warn(&store[i]);
        inv[b] = store[i];
        inv[b].pos = b;
        store[i].name[0] = 0;
        store_sel = -1;
    } else if (store[i].name[0]) store_sel = i;
    else if (item_sel >= 0 && item_sel < INV_BAG && inv[item_sel].name[0]) {
        store[i] = inv[item_sel];
        store[i].pos = -1;
        inv[item_sel].name[0] = 0;
        item_sel = -1;
    }
}

static void store_draw(UiDialog *d, int wx, int wy) {
    char key[24];
    UiCtrl *c;
    for (int i = 0; i < STORE_N; i++) {
        snprintf(key, sizeof key, "m_pItem[%d]", i);
        item_icon(d, key, wx, wy, &store[i]);
    }
    snprintf(key, sizeof key, "m_pHItem[%d]", store_sel);
    if (store_sel >= 0 && (c = ui_find(d, key))) draw(ui_image(c->img, 0), wx + c->x, wy + c->y);
}

// 劇情的道具物件 = 引擎的 Item（item_new）；原版 item_new 也把物件的別名設成道具表的別名
static void *story_item_new(int h, int type, int level, int col, const char *alias) {
    Item *it = malloc(sizeof *it);
    if (it && !item_new(it, type, level, col, alias)) { free(it); it = NULL; }
    if (it) { it->sh = h; story_set_alias(h, it->alias); }
    return it;
}

// 劇情給經驗（0x1003d270：主角和隊友各加）、系統訊息（0x1004dd00 → 介面訊息 0x5ea）
static int story_exp_need(int lv) { return lv < exp_levels ? exp_table[lv] : 0x7fffffff; }  // 原版 DAT_104784e8[lv]
static void story_exp(int h, int exp) { if (h == story_player()) hero_gain_exp(&actors[0], exp); }
static const char *story_text(int id) {
    static char out[256];
    char g[256] = "";
    out[0] = 0;
    if (res_dlg("EasyRPG_C.dlg", id, g, sizeof g)) gbk_utf8(g, out, sizeof out);
    return out;
}
static void story_log(int h, int kind, const char *text) { (void)kind; if (h == story_player()) hud_say(text); }

// 劇情和引擎共用主角的錢、身上的東西。叫劇情前：錢給它，inv 的每件道具放回主角的物件身上（find_item 找得到、
// move_object 拿得走）；叫完：拿回錢，劇情新放到主角身上的道具（add_item、give_object、move_object）收進 inv：
// 有 UI_Pos 且那格空著就放那格（開場的扶苏武器 32、布衣 31），否則背包空格，滿了掉在腳下；被劇情拿走的從 inv 移除。
// 劇情叫出買賣介面就打開
static void story_msgs(void);
static char rec_pending[64];  // 劇情事件要播的錄影別名（訊息 0x52e）
static void story_begin(void) {
    int pl = story_player();
    story_set_int(pl, "Money", hero_money);
    story_set_int(pl, "Bank", hero_bank);
    story_unhold(pl);
    for (int p = 0; p < INV_N; p++) {
        if (!inv[p].name[0]) continue;
        if (!inv[p].sh) { inv[p].sh = story_put(pl, NULL); story_set_alias(inv[p].sh, inv[p].alias); story_hold(inv[p].sh); }
        story_move(inv[p].sh, pl);
        story_set_int(inv[p].sh, "UI_Pos", p);
    }
}
static void story_end(void) {
    int pl = story_player(), equip = 0;
    hero_money = story_get_int(pl, "Money");
    hero_bank = story_get_int(pl, "Bank");
    for (int p = 0; p < INV_N; p++) {
        if (!inv[p].name[0] || !inv[p].sh || story_parent(inv[p].sh) == pl) continue;
        Item *it = malloc(sizeof *it);  // 給了別人：道具跟著物件走
        if (it) { *it = inv[p]; it->pos = -1; story_set_item(inv[p].sh, it); }
        inv[p].name[0] = 0;
        equip |= p >= INV_BAG;
    }
    for (int h = story_next(pl, 0); h; h = story_next(pl, h)) {
        Item *it = story_item(h);
        if (!it) continue;
        int p = story_get_int(h, "UI_Pos");
        if (p <= 0 || p >= INV_N || inv[p].name[0]) p = bag_free();
        it->sh = h;
        if (p < 0) { ground_drop(it, actors[0].x, actors[0].y); story_take(h); }
        else { inv[p] = *it; inv[p].pos = p; equip |= p >= INV_BAG; story_hold(h); }
        free(it);
    }
    if (equip) hero_refresh(&actors[0]);
    if (story_dialog.shop) { shop_open(story_dialog.shop); story_dialog.shop = 0; }
    if (story_dialog.store) { store_open(); story_dialog.store = 0; }
    story_msgs();
}

// ---- 劇情世界（原版 EasyRPG 的房間、人物）和畫面的連接 ----
// 原版 EasyRPG 用訊息通知畫面（Poq.exe 0x47d4e0、0x5174d0 分派）。這裡先排隊，劇情呼叫完（story_end）再處理，
// 免得在走訪角色時改動角色表
typedef struct { int id, n; intptr_t v[8]; char s[256]; } StoryMsg;
static StoryMsg smsg[256];
static int smsg_n, actors_dirty;

static void story_msg(int id, const intptr_t *v, int n, const char *s) {
    if (smsg_n == 256) { fprintf(stderr, "劇情訊息太多，丟掉 0x%x\n", id); return; }
    StoryMsg *m = &smsg[smsg_n++];
    m->id = id;
    m->n = n;
    memcpy(m->v, v, sizeof m->v);
    snprintf(m->s, sizeof m->s, "%s", s);
}

// 0x100730c0：依怪物表設人物數值（單機公式見 mon_stat）。數值放在人物物件上（劇情會讀），引擎的角色也照同一列算
static int story_person(int h, const char *name, int level, int tier) {
    if (!mondefs_load()) return 0;
    if (level < 0) {
        level = tier * 5 - rand() % 5;
        if (level > 100) level = 99;
    } else if (tier < 0 && (tier = level / 5 + 1) > 9) tier = 9;
    if (level > 99) level = 99;
    char gname[64];
    utf8_gbk(name, gname, sizeof gname);
    const MonDef *m = mondef_pick(gname);
    if (!m) { fprintf(stderr, "Person attributes not found <%s>!\n", name); return 0; }
    char g[128], u[128] = "";
    if (m->name_id >= 0 && res_dlg("name_C.dlg", m->name_id, g, sizeof g)) gbk_utf8(g, u, sizeof u);
    story_set_name(h, u);
    gbk_utf8(m->alias, u, sizeof u);
    story_set_alias(h, u);
    story_set_int(h, "Level", level);
    story_set_int(h, "Exp", mon_stat(m->exp, level, 0));
    story_set_int(h, "HP", mon_stat(m->hp, level, 1));
    story_set_int(h, "HP_MAX", mon_stat(m->hp, level, 1));
    story_set_int(h, "AP", mon_stat(m->ap, level, 2));
    story_set_int(h, "AP_MAX", mon_stat(m->ap_max, level, 2));
    story_set_int(h, "FE", m->melem ? m->melem : -1);
    if (m->melem >= 1 && m->melem <= 5) {
        char k[16];
        snprintf(k, sizeof k, "MAP%d", m->melem);
        story_set_int(h, k, mon_stat(m->map, level, 2));
        snprintf(k, sizeof k, "MAP%d_MAX", m->melem);
        story_set_int(h, k, mon_stat(m->map_max, level, 2));
    }
    story_set_int(h, "DP", mon_stat(m->dp, level, 3));
    for (int k = 0; k < 5; k++) {
        char key[8];
        snprintf(key, sizeof key, "MDP%d", k + 1);
        story_set_int(h, key, mon_stat(m->mdp[k], level, 3));
    }
    story_set_int(h, "SR", mon_stat(m->hit, level, 3));
    story_set_int(h, "HR", mon_stat(m->dodge, level, 3));
    if (m->nroles) story_set_int(h, "G_PicID", m->roles[rand() % m->nroles]);
    story_set_int(h, "B_C_MOVE_SPEED", m->mspeed);
    story_set_int(h, "B_C_FIGHT_SPEED", m->mfight);
    story_set_str(h, "Index", name);
    story_set_str(h, "MonRow", m->name);  // 自己加的：挑到哪一列（引擎的角色依它算數值）
    story_set_int(h, "B_C_VRANGE", m->vrange);
    story_set_int(h, "B_C_DJHF", m->djhf);
    story_set_int(h, "B_JSWL", m->jswl);
    story_set_int(h, "B_JSMF", m->jsmf);
    static const char *const IMMU[7] = {"B_XYIMMU", "B_ZDIMMU", "B_BDIMMU", "B_SMIMMU", "B_SHIMMU", "B_KSIMMU", "B_QTIMMU"};
    for (int k = 0; k < 7; k++) story_set_int(h, IMMU[k], m->immu[k]);
    story_set_int(h, "istrap", m->istrap);
    story_set_int(h, "HP_SPEED", m->regen * mon_stat(m->hp, level, 1) / 10);
    story_set_int(h, "Tier", tier);  // 自己加的：掉寶的等級欄（原版出生時就掉寶，這裡倒下才算）
    return 1;
}

// 依索引名找技能表的列（NPC 技能表加五個職業；找不到 NULL）
static const SkillRow *skill_row_any(const char *key) {
    static SkillRow rows[512];
    static int n = -1;
    if (n < 0) {
        n = skill_rows_file("skill_npc.txt", rows, 256);
        for (int p = 1; p <= 5; p++) n += skill_rows(p, rows + n, 512 - n);
    }
    char g[96];
    utf8_gbk(key, g, sizeof g);  // 劇情檔的字串是 UTF-8、技能表是 GBK：兩種寫法都認
    for (int i = 0; i < n; i++)
        if (!strcmp(rows[i].key, key) || !strcmp(rows[i].key, g)) return &rows[i];
    return NULL;
}

// 0x100757a0：依技能表設技能物件（陷阱 NPC 的技能用）。shortcut: 技能的效果（0x10037490）由引擎的技能表直接算，這裡只放屬性
static int story_skill(int h, const char *key, int level) {
    const SkillRow *r = skill_row_any(key);
    if (r) {
        char g[128], u[128];
        if (r->name_id >= 0 && res_dlg("name_C.dlg", r->name_id, g, sizeof g)) { gbk_utf8(g, u, sizeof u); story_set_name(h, u); }
        story_set_alias(h, key);
        story_set_int(h, "Level_Req", r->req);
        story_set_int(h, "Level_Add", r->req_add);
        story_set_int(h, "Level_Max", r->max_level);
        story_set_int(h, "W_Type", r->wtype);
        story_set_int(h, "AttackID", r->attack);
        story_set_int(h, "G_PicID2", r->icon);
        if (r->status > 0) story_set_int(h, "Status", r->status);
        if (r->estatus > 0) story_set_int(h, "EStatus", r->estatus);
        story_set_int(h, "Level", level < 0 && r->level >= 0 ? r->level : level);
        return 1;
    }
    fprintf(stderr, "Skill attributes not found <%s>!\n", key);
    return 0;
}

// ---- 隊友角色（單機劇情的 Friend 2 隊員：方仲、赵茜…）----
// 隊員在劇情裡是人物物件（team_add 0x10037850，G_AIGroup 跟著主角），引擎這邊給他一個 Actor：數值走 0x1002d8d0（team_derive）、
// 技能 = 他身上的技能物件（Level > 0）、AI = CNpc 的 npc_dispatch（跟隨邏輯 3，戰鬥走 npc_main_ai）、受傷死亡走 actor_kill（story_killed 刪人物 =
// 0x1004f3a0 非玩家的 DelObject）、經驗等級走 kill_exp → story_gain_exp（0x10026d30）後 team_sync。
// shortcut: 沒有裝備模型（外觀用模板圖、攻擊力用等級估）；技能挑選見 npc_selected；出生在主角旁邊空格（原版隊員在入口點／主角身邊的規則沒查）。
#define TEAM_MAX 8
static SkillRow team_row[TEAM_MAX][MAX_ACTOR_SKILLS];
static NpcSpec team_spec[TEAM_MAX];
static const char *const TEAM_ATTR[6] = {"Str", "Con", "Dex", "Savvy", "Charm", "Wisdom"};

// 技能物件 → 引擎技能列（等級 > 0 的，同一個索引名只放一次）
static void team_skills(Actor *a) {
    SkillRow *rows = team_row[a->team - 1];
    int n = 0;
    actor_skills_rebuild(a);
    for (int h = story_next(a->sh, 0); h && n < MAX_ACTOR_SKILLS; h = story_next(a->sh, h)) {
        const SkillRow *r = story_kind(h) == 5 ? skill_row_any(story_alias(h)) : NULL;
        if (!r || story_get_int(h, "Level") < 1) continue;
        rows[n] = *r;
        rows[n].level = story_get_int(h, "Level");
        actor_skill_append(a, &rows[n], n + 1);
        n++;
    }
    actor_right_skill(a, -1);
    actor_left_skill(a, -1);
}

// 劇情人物的現值 → 角色（升級、入隊、讀檔後）：等級、屬性、技能列、數值重算；血氣不補（讀檔時由 HP／MP 屬性放回）
static Item team_eq[TEAM_MAX][INV_N];
// 隊友的裝備：劇情人物身上 UI_Pos 30~40 的道具物件（原版隊友模板 add_item 給的武器、護甲，例如刺客赵茜 = 赵茜武器1／2、皮甲），
// 數值照主角同一套（item_mods）算；沒有任何裝備就維持「2×等級 + 8」的估計（person_derive 的 !equip 分支）
static void team_equip_load(Actor *a) {
    Item *eq = team_eq[a->team - 1];
    memset(eq, 0, sizeof team_eq[0]);
    int n = 0;
    for (int h = story_next(a->sh, 0); h; h = story_next(a->sh, h)) {
        const Item *it = story_item(h);
        int p = story_get_int(h, "UI_Pos");
        if (it && p >= INV_BAG && p < INV_N && !eq[p].name[0]) { eq[p] = *it; eq[p].pos = p; n++; }
    }
    a->eq = n ? eq : NULL;
}
static void team_sync(Actor *a) {
    team_equip_load(a);
    a->level = story_get_int(a->sh, "Level");
    for (int k = 0; k < 6; k++) a->team_attr[k] = story_get_int(a->sh, TEAM_ATTR[k]);
    a->team_prof = story_get_int(a->sh, "IsActorNPC");
    team_skills(a);
    team_derive(a);
    actor_effects_recompute(a);
}

static Actor *team_make(const Scene *s, int h) {
    int slot = 0;
    for (; slot < TEAM_MAX; slot++) {  // 空槽 = 沒有活著的隊友角色用它
        int used = 0;
        for (int i = 1; i < actor_count; i++) used |= actors[i].team == slot + 1 && actors[i].role != NULL;
        if (!used) break;
    }
    const RoleDef *d = roledef_find(story_get_int(h, "G_PicID"));
    if (slot == TEAM_MAX || actor_count >= MAX_ACTORS || !d || !role_drawable(d)) return NULL;
    Actor *hero = &actors[0];
    int cx = (int)hero->x, cy = (int)hero->y;
    if (!free_cell(&cx, &cy, 6)) return NULL;
    Actor *a = &actors[actor_count++];
    NpcSpec *sp = &team_spec[slot];
    // 邏輯 3 = 跟隨（npc_idle_ai）：離主角超過 rh 格就走過去（命令 2 = 跟著走）
    *sp = (NpcSpec){.type = 15, .logic = 3, .tmpl = d->id, .x = cx, .y = cy, .fol_mode = 1, .fol_dist = 3, .dir = 0};
    memset(a, 0, sizeof *a);
    npc_behavior_reset(a);
    a->x = cx + 0.5f;
    a->y = cy + 0.5f;
    a->spec = sp;
    a->object_kind = 31;
    a->team = slot + 1;
    a->sh = h;
    actor_setup(a, d);
    actor_group(a, story_get_int(h, "G_AIGroup"));
    a->side = !scene_client_group(&scene_client, a->grp);
    if (!actor_bind_scene(a, s) || !actor_place(a, cx, cy, 1)) { a->role = NULL; a->team = 0; return NULL; }
    team_sync(a);
    int hp = story_get_int(h, "HP"), mp = story_get_int(h, "MP");
    a->hp = hp > 0 && hp <= a->hp_max ? hp : a->hp_max;  // 剛入隊（HP 屬性還是模板值）補滿
    a->mp = mp > 0 && mp <= a->mp_max ? mp : a->mp_max;
    a->follow_id = hero->object_id;
    return a;
}

// 房間裡的人物 → 角色：還沒有角色的加上（原版用 G_PosX = −100 表示「放在入口 G_PosY」），不在房間裡的角色拿掉（倒下的播完）
static void actors_sync(const Scene *s) {
    actors_dirty = 0;
    if (!mondefs_load()) return;
    for (int i = 1; i < actor_count; i++) {
        Actor *a = &actors[i];
        if (a->sh && a->role && a->act != ACT_DEAD && story_parent(a->sh) != sroom) {
            actor_unoccupy(a); a->role = NULL;
        }
    }
    int pl = story_player(), grp = story_get_int(pl, "G_AIGroup");
    actor_group(&actors[0], grp);
    for (int m = story_team_next(pl, 0); m; m = story_team_next(pl, m)) {  // 隊友：Friend 2、還沒有角色的補上（ONT 擺的 NPC 已有自己的角色）
        Actor *t = actor_of(m);
        if (!t && story_get_int(m, "Friend") >= 2 && story_parent(m) == sroom && team_make(s, m)) printf("隊友 %s 入隊\n", story_name(m));
    }
    for (int h = story_next(sroom, 0); h && actor_count < MAX_ACTORS; h = story_next(sroom, h)) {
        if (h == pl || story_kind(h) != 1 || actor_of(h)) continue;
        const MonDef *m = mondef_find(story_get_str(h, "MonRow"));
        const RoleDef *d = roledef_find(story_get_int(h, "G_PicID"));
        if (!m || !d || !role_drawable(d)) continue;
        if (story_get_int(h, "G_PosX") != -100) continue;  // 劇情 NPC（ONT 擺的）由 spawn_npcs 放
        // 劇情建角色（0x44fd20 種類 0、G_PosX −100）：同名入口點隨機挑一個（0x4cc9c0，第 n 個以 1/n 換掉），面向入口方向
        // 入口格交角色場景綁定／占位，依原版邊圈順序找空格。
        int n = 0, pick = -1, area = story_get_int(h, "G_PosY");
        for (int i = 0; i < s->entry_count; i++)
            if (s->entries[i].id == area && rand() % ++n == 0) pick = i;
        if (pick < 0) continue;
        const Entry *en = &s->entries[pick];
        int x = en->x, y = en->y;
        Actor *a = mon_make(m, d, x, y, story_get_int(h, "Level"));
        a->sh = h;
        actor_person_init(a, h, story_get_int);
        for (int k = 0; k < 8; k++)
            if (DIR_MAP[k] == en->dir) a->dir = k;
        actor_group(a, story_get_int(h, "G_AIGroup"));
        a->side = !scene_client_group(&scene_client, a->grp);  // side 是我們自己的「主角這邊」，敵對另看 grp
    }
}

// 怪物倒下：原版 0x100574e0 最後呼叫第 10 格再刪掉（Boss 的第 10 格開劇情）
static void camera_look_at(int h);
static void story_killed(int h) {
    story_begin();
    story_die(h);
    story_end();
}

// 處理排隊的訊息（編號照原版；Poq.exe 0x47d4e0 等的處理還沒全部翻，沒做的印出來）
static void story_msgs(void) {
    for (int i = 0; i < smsg_n; i++) {
        StoryMsg *m = &smsg[i];
        Actor *a = actor_of((int)m->v[0]);
        switch (m->id) {
        case 0x51d:  // 物件換位置：進出目前房間就重新對角色
            if (m->v[1] == sroom || (a && a != &actors[0])) actors_dirty = 1;
            break;
        case 0x53e:
            // 單機共用物件，台詞直接交客戶端顯示，不往返字串同步封包。
            scene_character_say_message((int)m->v[0], m->s);
            break;
        case 0x535: if (a) a->ex[20] = (int)m->v[1]; break;                 // Invincible
        case 0x52a: if (a) actor_weapon_type_set(a, (int)m->v[1]); break;
        case 0x51c:  // G_AIGroup
            a = character_of((int)m->v[0]);
            if (a) actor_group(a, (int)m->v[1]);
            if (a && a != &actors[0]) a->side = m->v[1] != story_get_int(story_player(), "G_AIGroup");
            break;
        case 0x574e0: if (a && alive(a)) actor_kill(a, NULL); break;        // 劇情讓人物死掉
        case 0x42aa0: if (a && a != &actors[0]) a->hp = story_get_int(a->sh, "HP"); break;
        case 0x52e: snprintf(rec_pending, sizeof rec_pending, "%s", story_plot.alias); break;  // 播 Rec/User/<別名>.rec（主迴圈 rec_begin）；播完 rec_end 跑第 18 格
        case 0x47870:
            if (a && m->s[0] == '@') actor_attribute_changed(a, m->s + 1, story_get_int(a->sh, m->s + 1));
            break;
        case 0x51b:
            a = character_of((int)m->v[0]);
            // 原版僅第三參數>0時寫第二參數；0及負數只改狀態103。
            if (a) actor_status_notify(a, 103, m->v[2] != 0, -1);  // 0x51b：第三參數 0 解除「无对话」，其餘設上
            if (a && m->v[2] > 0) actor_search_link_set(a, (int)m->v[1]);
            break;
        case 0x533:  // 畫面效果（Poq.exe 0x533）：v[1] = 1 → v[2] 1 黑幕淡出停 0x50 拍再淡入、2 淡出後黑著；v[1] = 2 → 鏡頭平滑移到物件 v[2]（0x44e380，不是震動）
            if (m->v[1] == 1 && m->v[2] == 1) screen_effect(2, 0x50);
            else if (m->v[1] == 1 && m->v[2] == 2) screen_effect(2, 0);
            else if (m->v[1] == 2) camera_look_at((int)m->v[2]);
            break;
        case 0x531: if (a) a->arrow_kind = (int)m->v[1]; break;  // 箭種 +0x52d（EasyRPG 0x100433e0 送 FE）
        case 0x51e:  // 隊友離隊（RemoveMember 0x10053b40）：不再跟著主角，站著
            if (a && a->team) { ((NpcSpec *)a->spec)->logic = 0; a->team = 0; a->follow_id = 0; a->skill_n = 0; }
            break;
        case 0x49b70: actors_dirty = 1; break;  // 隊伍面板加一格（面板每幀照隊伍表畫）；隊友補角色
        case 0x53a: chapter_message((int)m->v[1], (int)m->v[2]); break;     // 章節（0x4fca00）
        case 0x532:  // 人物位置（Poq 0x47e6xx 0x532；EasyRPG 0x1004e200 的 a1 = 格 x、a2 = 格 y、a3 = 面向、a4 ≠ 0）：清移動目標／命令，0x496e30 放到格中央，0x4963e0 設面向。
            // a4 == 0 的分支是鏡頭（虛擬 +0x54(1, …)），劇情資料全都 a4 = 1，沒做
            if (!a && (int)m->v[0] == story_player()) a = &actors[0];  // 主角的角色沒有劇情代號，用玩家代號認
            if (a && m->v[4]) {
                a->foe = NULL; a->want = NULL; a->moving = 0;
                if (actor_restore_position(a, (int)m->v[1] + 0.5f, (int)m->v[2] + 0.5f)) {
                    a->pvx = a->x; a->pvy = a->y;
                    actor_direction_set(a, (int)m->v[3] & 7);
                }
            }
            break;
        case 0x52c: case 0x519: break;  // 出口開關（走出口時再查）、轉向
        default: fprintf(stderr, "劇情訊息 0x%x（%s）還沒做\n", m->id, m->s); break;
        }
    }
    smsg_n = 0;
}

// ---- 開箱（EasyRPG.dll 文字指令 open 0x10019c70、openall 0x10019d70，規則 0x1004e680）----
// 房間裡的寶箱是 BNT 建築（模型種類 0x85），名稱 = 劇情物件的別名；箱裡的道具是劇情物件的子物件。
static void chest_say(int h, const char *key) {  // 訊息 kind 0xf（0x1004dd00）：非空才顯示
    const char *t = story_get_str(h, key);
    if (t && *t) hud_say(t);
}
// 0x1004e680 開箱規則：isbuilding < 1 不能開（訊息 0x49b）；2／6 已開（openedinfo）；有 key 要身上有那件（deletekey ≠ 0 用掉），
// 沒有就說 lockinfo；可以開 → isbuilding 1 → 2、5 → 6，箱裡的道具（G_AIGroup 設成箱子代號）掉到地上，說 openinfo。回 1 = 開了
// 虛擬格 0x44 = 第 17 格（story_can_open）；shortcut: 身上找鑰匙只比別名（原版 0x10059fb0 還會進背包容器找）
static int chest_open_rule(int h, float cx, float cy) {
    int isb = story_get_int(h, "isbuilding");
    if (isb < 1) { hud_say(story_text(0x49b)); return 0; }
    if (isb == 2 || isb == 6) { chest_say(h, "openedinfo"); return 0; }
    const char *key = story_get_str(h, "key");
    if (key && *key) {
        int pl = story_player(), kh = 0;
        for (int k = story_next(pl, 0); k && !kh; k = story_next(pl, k))
            if (!strcmp(story_alias(k), key)) kh = k;
        if (!kh) { chest_say(h, "lockinfo"); return 0; }
        if (story_get_int(h, "deletekey")) story_delete(kh);
    }
    if (!story_can_open(h)) return 0;  // 虛擬格 0x44（第 17 格）：覆寫的箱子、門在這裡做事，回 0 不開
    if (debug_cells) { int n = 0, k2 = 0; for (int c = story_next(h, 0); c; c = story_next(h, c)) { n++; k2 += story_kind(c) == 2; } printf("chest children=%d items=%d\n", n, k2); }
    story_set_int(h, "isbuilding", isb == 1 ? 2 : isb == 5 ? 6 : isb);
    for (int it = story_next(h, 0); it;) {
        int next = story_next(h, it);
        if (story_kind(it) == 2) {
            if (!story_get_int(it, "G_AIGroup")) story_set_int(it, "G_AIGroup", -1);
            story_set_int(it, "G_AIGroup", h);
            Item *item = story_item(it);
            if (item) { item->sh = it; ground_drop(item, cx, cy); story_take(it); story_item_moved(it, story_parent(h), h); }  // 箱子 → 房間：物件第 15 格（0x10059020）
        }
        it = next;
    }
    chest_say(h, "openinfo");
    return 1;
}
// 0x10019c70 open：找不到物件訊息 0x49a；Level 1～9（要開鎖技能，Attack 5／6 單機劇情沒用到）只說 lockinfo，其他走開箱規則
static void chest_open(int h, float cx, float cy) {
    if (!h) { hud_say(story_text(0x49a)); return; }
    int lv = story_get_int(h, "Level");
    if (lv >= 1 && lv <= 9) { chest_say(h, "lockinfo"); return; }
    chest_open_rule(h, cx, cy);
}
// 房間裡同名的劇情物件（ITEM：水缸A、树木A…，add_npc 數量 = BNT 同名建築數）依序對應同名的建築。
// shortcut: 原版 0x44fd20 種類 4「依名字隨機挑一個可打壞的建築」綁定，這裡照 BNT 順序綁（結果看不出來）。
static int object_story(const Scene *sc, int idx) {
    const Object *o = &sc->objects[idx];
    int n = 0;
    for (int i = 0; i < idx; i++) if (!strcmp(sc->objects[i].name, o->name)) n++;
    for (int h = story_next(sroom, 0); h; h = story_next(sroom, h))
        if (!strcmp(story_alias(h), o->name) && n-- == 0) return h;
    return 0;
}
// 0x10041d10 → 封包 0x523：isbuilding 是 1、5 = 還沒開（狀態 0，可操作）；其他 = 已開（客戶端 0x47d4e0 case 0x523 → 狀態 5 播互動動畫，播完回 1）。
// animate = 0：進場景時同步已開的，直接 1（不重播動畫）。所有有劇情物件的可破壞／可使用建築（0x80～0x100 非門）都會同步，不只箱子。
static void chest_state_sync(Scene *scene, int animate) {
    for (int i = 0; scene && i < scene->object_count; i++) {
        Object *o = &scene->objects[i];
        if (!o->m || !o->tri || o->m->type == 0x96 || !o->name[0]) continue;
        int h = object_story(scene, i);
        if (debug_cells) printf("sync bld %d %s story=%d isb=%d state=%d anim=%d sroom=%d\n", i, o->name, h, h ? story_get_int(h, "isbuilding") : -9, o->state, animate, sroom);
        if (!h) continue;
        int isb = story_get_int(h, "isbuilding");
        if (isb == 1 || isb == 5) { if (!building_available(o)) scene_building_state(scene, i, 0); }
        else if (animate && building_available(o)) scene_building_state(scene, i, 5);
        else if (o->state != 1 && o->state != 5) scene_building_state(scene, i, 1);
    }
}
static void chest_open_object(int idx) {
    Scene *scene = (Scene *)unit_scene;  // shortcut: 場景是 main 的區域變數，只能走 unit_scene（const），改狀態時去掉 const
    if (!scene || idx < 0 || idx >= scene->object_count) return;
    const Object *o = &scene->objects[idx];
    story_begin();
    if (debug_cells) printf("use bld %d %s story=%d isbuilding=%d\n", idx, o->name, object_story(scene, idx), object_story(scene, idx) ? story_get_int(object_story(scene, idx), "isbuilding") : -9);
    chest_open(o->name[0] ? object_story(scene, idx) : 0, o->x + 0.5f, o->y + 0.5f);
    story_end();
    chest_state_sync(scene, 1);
}
// 0x10019d70 openall：房間裡別名相符、isbuilding 是 1 或 5 的箱子全部開（逐個執行 open #代號）
static void chest_open_all(const char *alias) {
    story_begin();
    for (int h = story_next(sroom, 0); h; h = story_next(sroom, h)) {
        int isb = story_get_int(h, "isbuilding");
        if (strcmp(story_alias(h), alias) || (isb != 1 && isb != 5)) continue;
        float x = actors[0].x, y = actors[0].y;
        for (int i = 0; unit_scene && i < unit_scene->object_count; i++)
            if (unit_scene && !strcmp(unit_scene->objects[i].name, alias)) { x = unit_scene->objects[i].x + 0.5f; y = unit_scene->objects[i].y + 0.5f; break; }
        chest_open(h, x, y);
    }
    story_end();
    chest_state_sync((Scene *)unit_scene, 1);
}
static void door_gather_say(void) { hud_say(story_text(0x508)); }

// 開鎖（0x10034010）：技能等級 < 目標 Level → 5005；isbuilding > 0（1 = 鎖著：key 清空、訊息 5007）→ 走開箱規則 0x1004e680；其他 5006
static int lockpick_open(const SkillRow *r, int h, float cx, float cy) {
#ifdef STORY
    if (r->level < story_get_int(h, "Level")) { hud_msg(5005); return 0; }
    int isb = story_get_int(h, "isbuilding");
    if (isb > 0) {
        if (isb == 1) { story_set_str(h, "key", ""); hud_msg(5007); }
        story_begin();
        chest_open_rule(h, cx, cy);
        story_end();
        chest_state_sync((Scene *)unit_scene, 1);
        return 1;
    }
    hud_msg(5006, story_alias(h));
#else
    (void)r; (void)h; (void)cx; (void)cy;
#endif
    return 0;
}
static void skill_open_lock(Actor *a, const SkillRow *r) {
#ifdef STORY
    const Scene *sc = unit_scene;
    float ax = a->aim_valid ? a->aim_x / 64.0f : a->x, ay = a->aim_valid ? a->aim_y / 64.0f : a->y;
    int best = -1;
    float bd = 4;
    for (int i = 0; sc && i < sc->object_count; i++) {
        const Object *o = &sc->objects[i];
        float d = (o->x + 0.5f - ax) * (o->x + 0.5f - ax) + (o->y + 0.5f - ay) * (o->y + 0.5f - ay);
        if (o->name[0] && d < bd && object_story(sc, i) && story_get_int(object_story(sc, i), "isbuilding") > 0) { best = i; bd = d; }
    }
    if (best < 0) { hud_msg(5006, ""); return; }
    lockpick_open(r, object_story(sc, best), sc->objects[best].x + 0.5f, sc->objects[best].y + 0.5f);
#else
    (void)a; (void)r;
#endif
}
#else
static void door_gather_say(void) {}
static void chest_open_object(int idx) { (void)idx; }
static void chest_state_sync(Scene *scene, int animate) { (void)scene; (void)animate; }
static void skill_open_lock(Actor *a, const SkillRow *r) { (void)a; (void)r; hud_msg(5006, ""); }
#endif


// ---- 存檔（原版 JM50_内部主界面 的保存、读取 → JM62_存取盘界面，Poq.exe CSystemDialog 0x5354a0、CLoadDialog 0x56c400；開存／讀視窗 0x570900、0x5728c0，畫格子 0x56ce20）----
// 一頁 6 格、最多 16 頁；第 0 頁前兩格是自动存盘（穿梭大場景時存）、快速存盘（F9），UI_C.dlg 2130、2131。
// 每格秀場景縮圖、場景名、時間。存檔格式是自己的（原版是整個 EasyRPG 世界的序列化）：主角、背包、寄存、劇情物件
// shortcut: 每格的隊伍頭像（ResID1~5）還沒做，只有空框；原版 m_pDelete（刪除）是隱藏的，沒做
#define JM50 "JM50_\xc4\xda\xb2\xbf\xd6\xf7\xbd\xe7\xc3\xe6"                      // JM50_内部主界面
#define JM62 "JM62_\xb4\xe6\xc8\xa1\xc5\xcc\xbd\xe7\xc3\xe6"                      // JM62_存取盘界面
#define JM67 "JM67_\xb9\xab\xd3\xc3\xb6\xd4\xbb\xb0\xbf\xf2"                      // JM67_公用对话框
#define JM102 "JM102_\xcd\xcb\xb3\xf6\xd3\xce\xcf\xb7\xbd\xe7\xc3\xe6"            // JM102_退出游戏界面
#define SAVE_SLOTS 96
#define THUMB_W 128
#define THUMB_H 96
#define SAVE_VER 2  // 2：劇情物件多了種類、隊伍、出口
typedef struct { char magic[4]; int ver; long long time; char alias[64], name[64]; int level; uint32_t thumb[THUMB_W * THUMB_H]; } SaveHead;
typedef struct {
    float x, y;
    int dir, level, exp, hp, mp, attr[6], extra_exp, extra_skill, money, prof, cur_skill[2], skill_lv[20];
} SaveHero;
static UiDialog sys_d, sav_d, ask_d, quit_d;
static UiDialog diff_d;  // JM21 難度選擇（CDiffcultyDialog 0x566ec0；0x570820 新遊戲時開窗、0x566f80 選定存難度並關窗）
static int diff_open;
static void new_show(void);
#ifdef STORY
static void hero_from_story(int full);
#endif
static int sys_open, sav_mode, sav_page, sav_sel = -1, ask_open, quit_open;  // sav_mode：1 保存、2 读取
static SaveHead *sav_head[SAVE_SLOTS];   // 開視窗時讀的各格檔頭，NULL = 空格
static SDL_Texture *sav_tex[6];          // 這一頁的縮圖
static uint32_t snap[THUMB_W * THUMB_H]; // 開系統選單前的畫面（原版也是先截圖，0x535ec0）
static int snap_due, save_due = -1, load_due = -1, quit_due, dead_open, load_arg = -1;
static char save_alias[64], save_name[64];  // 目前場景的別名、房間名（劇情版 UTF-8、舊版 GBK）

#ifdef __EMSCRIPTEN__
// 網頁版存在瀏覽器的 IndexedDB：開場掛上、讀進來；每次存完寫回去
EM_ASYNC_JS(void, save_mount, (), {
    try { FS.mkdir('/save'); } catch (e) {}
    FS.mount(IDBFS, {}, '/save');
    await new Promise(r => FS.syncfs(true, r));
});
EM_JS(void, save_flush, (), { FS.syncfs(false, e => { if (e) console.log('存檔寫入失敗', e); }); });
EM_JS(void, page_reload, (), { location.reload(); });
#define SAVE_DIR "/save"
#else
#include <sys/stat.h>
static void save_mount(void) { mkdir("save", 0755); }
static void save_flush(void) {}
#define SAVE_DIR "save"
#endif

static void save_path(int slot, char *out, size_t cap) { snprintf(out, cap, SAVE_DIR "/qin%02d.sav", slot); }

// 畫面縮成縮圖（0x449bf0：把場景畫成 w×h 小圖；開系統選單、自動存檔、快速存檔前在畫介面之前截）
static void snap_take(SDL_Texture *canvas) {
    uint32_t *px = malloc((size_t)vw * vh * 4);
    if (!px) return;
    SDL_SetRenderTarget(ren, canvas);
    if (!SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGBA32, px, vw * 4))
        for (int y = 0; y < THUMB_H; y++)
            for (int x = 0; x < THUMB_W; x++) snap[y * THUMB_W + x] = px[(y * vh / THUMB_H) * vw + x * vw / THUMB_W];
    free(px);
}

// 0x44c830：存小地圖探索過的格子。只存地圖寬 > 32 的場景；位元表長 (寬×高)/8 + 1，y 外 x 內，格子探索值（+0x13）≠ 0 設位元。
// 存檔尾巴：場景別名（64）、寬、高、位元表長、位元表。讀回後進場景時套用（explore_restore）：位元 = 1 的格子至少是 1（看過、現在看不到）
static unsigned char *explore_saved;
static int explore_saved_w, explore_saved_h;
static char explore_saved_alias[64];
static unsigned char *explore_pack(const Scene *s, size_t *n) {
    if (!s || !s->cells || s->w <= 32) return NULL;
    *n = (size_t)s->w * s->h / 8 + 1;
    unsigned char *b = calloc(*n, 1);
    if (!b) return NULL;
    for (int y = 0, i = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++, i++) b[i >> 3] |= (s->cells[y * s->w + x].view != 0) << (i & 7);
    return b;
}
static void explore_restore(Scene *s, const char *alias) {
    if (explore_saved && s->cells && s->w == explore_saved_w && s->h == explore_saved_h && !strcmp(alias, explore_saved_alias))
        for (int i = 0; i < s->w * s->h; i++)
            if ((explore_saved[i >> 3] >> (i & 7) & 1) && !s->cells[i].view) s->cells[i].view = 1;
    free(explore_saved);
    explore_saved = NULL;
}
static int save_write(int slot) {
    char p[64];
    save_path(slot, p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return 0;
    static SaveHead h;
    memset(&h, 0, sizeof h);
    memcpy(h.magic, "QSAV", 4);
    h.ver = SAVE_VER;
    h.time = (long long)time(NULL);
    snprintf(h.alias, sizeof h.alias, "%s", save_alias);
    snprintf(h.name, sizeof h.name, "%s", save_name);
    const Actor *a = &actors[0];
    h.level = a->level;
    memcpy(h.thumb, snap, sizeof snap);
    SaveHero s = {a->x, a->y, a->dir, a->level, a->exp, a->hp, a->mp, {0}, hero_extra_exp, hero_extra_skill, hero_money, hero_prof,
                  {cur_skill[0], cur_skill[1]}, {0}};
    memcpy(s.attr, hero_attr, sizeof s.attr);
    for (int i = 0; i < hero_skill_n && i < 20; i++) s.skill_lv[i] = hero_skill[i].level;
    fwrite(&h, sizeof h, 1, f);
    fwrite(&s, sizeof s, 1, f);
    fwrite(inv, sizeof inv, 1, f);
#ifdef STORY
    fwrite(store, sizeof store, 1, f);
    story_set_int(story_player(), "Bank", hero_bank);  // 錢庄存款跟著劇情物件存
    story_save(f, sizeof(Item));
#endif
    {   // 探索尾巴（0x44c830）：沒有（寬 ≤ 32）寫長度 0
        size_t n = 0;
        unsigned char *b = explore_pack(unit_scene, &n);
        char al[64] = "";
        snprintf(al, sizeof al, "%s", save_alias);
        int dim[2] = {unit_scene ? unit_scene->w : 0, unit_scene ? unit_scene->h : 0};
        fwrite(al, sizeof al, 1, f); fwrite(dim, sizeof dim, 1, f); fwrite(&n, sizeof n, 1, f);
        if (b) fwrite(b, 1, n, f);
        free(b);
    }
    {   // 地上道具尾巴：目前場景名、各場景暫存、目前地上道具
        fwrite("GRND", 4, 1, f); fwrite(ground_alias, sizeof ground_alias, 1, f); fwrite(gstore, sizeof gstore, 1, f);
        fwrite(&ground_n, sizeof ground_n, 1, f); fwrite(ground, sizeof ground[0], ground_n, f);
    }
    {   // 怪物尾巴：有劇情物件的非隊友角色（隊友靠劇情物件的 HP／MP 與隊伍表存）
        MonSave m[256]; int n = 0;
        for (int i = 1; i < actor_count && n < 256; i++) {
            const Actor *c = &actors[i];
            if (c->sh && c->role && !c->team && c->act != ACT_DEAD) m[n++] = (MonSave){c->sh, c->hp, c->mp, c->dir, c->x, c->y};
        }
        fwrite("MONS", 4, 1, f); fwrite(&n, sizeof n, 1, f); fwrite(m, sizeof m[0], n, f);
    }
    int ok = !ferror(f);
    if (fclose(f) || !ok) { remove(p); return 0; }
    save_flush();
    return 1;
}

// 讀檔：先把主角、背包、劇情換掉，場景由呼叫的人進（要房間表）。回傳 0 = 檔案不對、什麼都沒動
static int save_read(int slot, SaveHead *h) {
    char p[64];
    save_path(slot, p, sizeof p);
    FILE *f = fopen(p, "rb");
    SaveHero s;
    static Item tmp[INV_N];
    if (!f) return 0;
    if (fread(h, sizeof *h, 1, f) != 1 || memcmp(h->magic, "QSAV", 4) || h->ver != SAVE_VER || fread(&s, sizeof s, 1, f) != 1 ||
        fread(tmp, sizeof tmp, 1, f) != 1) { fclose(f); return 0; }
#ifdef STORY
    static Item st[STORE_N];
    if (fread(st, sizeof st, 1, f) != 1 || !story_load(f, sizeof(Item))) { fclose(f); return 0; }  // shortcut: 劇情讀到一半壞掉時世界已清空，沒有退回
    memcpy(store, st, sizeof store);
    hero_bank = story_get_int(story_player(), "Bank");
    shop_close();
    dialog_open = 0;
#endif
    {   // 探索尾巴；舊存檔沒有就算了
        char al[64]; int dim[2]; size_t n;
        free(explore_saved); explore_saved = NULL;
        if (fread(al, sizeof al, 1, f) == 1 && fread(dim, sizeof dim, 1, f) == 1 && fread(&n, sizeof n, 1, f) == 1 && n < (1u << 24) && n) {
            explore_saved = malloc(n);
            if (explore_saved && fread(explore_saved, 1, n, f) == n) {
                explore_saved_w = dim[0]; explore_saved_h = dim[1];
                snprintf(explore_saved_alias, sizeof explore_saved_alias, "%s", al);
            } else { free(explore_saved); explore_saved = NULL; }
        }
    }
    ground_pending = 0;
    {   // 地上道具尾巴；舊存檔沒有就算了
        char mg[4]; int n;
        if (fread(mg, 4, 1, f) == 1 && !memcmp(mg, "GRND", 4) && fread(pending_alias, sizeof pending_alias, 1, f) == 1 &&
            fread(&pending_gstore, sizeof pending_gstore, 1, f) == 1 && fread(&n, sizeof n, 1, f) == 1 && n >= 0 && n <= MAX_GROUND &&
            fread(pending_ground, sizeof pending_ground[0], n, f) == (size_t)n) { pending_n = n; ground_pending = 1; }
    }
    pending_mons_n = 0;
    {   // 怪物尾巴；舊存檔沒有就算了
        char mg[4]; int n;
        if (fread(mg, 4, 1, f) == 1 && !memcmp(mg, "MONS", 4) && fread(&n, sizeof n, 1, f) == 1 && n >= 0 && n <= 256 &&
            fread(pending_mons, sizeof pending_mons[0], n, f) == (size_t)n) pending_mons_n = n;
    }
    fclose(f);
    memcpy(inv, tmp, sizeof inv);
    memcpy(hero_attr, s.attr, sizeof hero_attr);
    hero_extra_exp = s.extra_exp;
    hero_extra_skill = s.extra_skill;
    hero_money = s.money;
    hero_prof = s.prof;
#ifdef STORY
    hero_from_story(0);  // 存檔裡的主角物件就是創角選的範本
    {   // 創角時把名字寫進主角物件；舊存檔還是範本的職業名（游侠…），沿用目前的名字
        const char *nm = story_name(story_player());
        if (*nm && !strstr("\xe6\xb8\xb8\xe4\xbe\xa0 \xe5\x8a\x9b\xe5\xa3\xab \xe5\x88\xba\xe5\xae\xa2 \xe6\x9c\xaf\xe5\xa3\xab \xe5\xb7\xab\xe5\xb8\x88", nm)) snprintf(hero_name, sizeof hero_name, "%s", nm);
    }
#endif
    hero_skill_n = -1;
    skills_load();
    for (int i = 0; i < hero_skill_n && i < 20; i++) hero_skill[i].level = s.skill_lv[i];
    cur_skill[0] = s.cur_skill[0];
    cur_skill[1] = s.cur_skill[1];
    hero_look();
    win_left.open = win_right.open = 0;
    pick_hand = item_sel = -1;
    Actor *a = &actors[0];
    a->x = s.x; a->y = s.y; a->dir = s.dir; a->level = s.level; a->exp = s.exp; a->hp = s.hp; a->mp = s.mp;  // 位置等進場景後再放回
    return 1;
}

static void sav_refresh(void) {
    for (int i = 0; i < 6; i++) {
        if (sav_tex[i]) SDL_DestroyTexture(sav_tex[i]);
        sav_tex[i] = NULL;
        const SaveHead *h = sav_head[sav_page * 6 + i];
        if (!h) continue;
        sav_tex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, THUMB_W, THUMB_H);
        SDL_UpdateTexture(sav_tex[i], NULL, h->thumb, THUMB_W * 4);
    }
}

// 頁數（0x56cce0）：保存時多留一頁空的、读取時只到最後一個用過的，最多 16 頁
static int sav_pages(void) {
    int last = -1;
    for (int i = 0; i < SAVE_SLOTS; i++)
        if (sav_head[i]) last = i;
    int n = sav_mode == 1 ? (last + 7) / 6 : last / 6 + 1;
    return n < 1 ? 1 : n > 16 ? 16 : n;
}

static void sav_open(int mode) {
    if (!ui_load(JM62, &sav_d)) return;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        free(sav_head[i]);
        sav_head[i] = NULL;
        char p[64];
        save_path(i, p, sizeof p);
        FILE *f = fopen(p, "rb");
        SaveHead *h = malloc(sizeof *h);
        if (f && h && fread(h, sizeof *h, 1, f) == 1 && !memcmp(h->magic, "QSAV", 4) && h->ver == SAVE_VER) { sav_head[i] = h; h = NULL; }
        free(h);
        if (f) fclose(f);
    }
    sav_mode = mode;
    sav_page = 0;
    sav_sel = -1;
    sav_refresh();
}

static void sav_close(void) { sav_mode = 0; ask_open = 0; }

// 確定：保存 → 有檔就先問要不要覆蓋（UI_C 1535）；读取 → 空格不動
static void sav_ok(int confirmed) {
    if (sav_sel < 0) return;
    if (sav_mode == 2) {
        if (!sav_head[sav_sel]) return;
        load_due = sav_sel;
        sav_close();
        sys_open = 0;
        return;
    }
    if (sav_head[sav_sel] && !confirmed) { ask_open = 1; ui_load(JM67, &ask_d); return; }
    save_due = sav_sel;  // 縮圖用開選單前截的那張
    sav_close();
    sys_open = 0;
}

static void ui_say(int id) { char t[256]; ui_text(id, t, sizeof t); hud_say(t); }

// ---- 遊戲設定 JM31（Poq.exe CGameOptionDialog：開窗 0x571790、滑桿改值 0x5474a0、初始 0x547500、關閉 0x547460）、遊戲說明 JM36（CHelpDialog 開窗 0x571860）----
// 值依控制項名：勾選（透明、血腥、自动存盘、自动暂停 1~4）、滑桿（音效、音乐 0~100、速度 0~5，種類 8：軌道 = 第 10 欄、滑塊 = 第 13 欄）、亮度（進度條 0~100）、光影前後
// shortcut: 預設值與勾選圖的幀（0 關、1 開）沒對原版；速度、亮度、光影、解析度、透明、血腥、自动暂停只存值還沒有作用；滑桿只能點、不能拖；
// 說明視窗只有分頁鈕，內文（建立後讀的檔）沒在遊戲資料裡找到
enum { O_TRANS, O_GORE, O_AUTOSAVE, O_PAUSE1, O_PAUSE2, O_PAUSE3, O_PAUSE4, O_SOUND, O_MUSIC, O_SPEED, O_GAMMA, O_LIGHT, O_RES, O_N };
static const char *const opt_name[O_N] = {"m_pTransparent", "m_pGore", "m_pAutoSave", "m_pAutoPause1", "m_pAutoPause2", "m_pAutoPause3",
                                          "m_pAutoPause4", "m_pSound", "m_pMusic", "m_pSpeed", "m_pGamma", "m_pLight", "m_pResolution"};
static const int opt_max[O_N] = {1, 1, 1, 1, 1, 1, 1, 100, 100, 5, 100, 1, 2};  // 光影 2 種（0x5470f0 取 &1）、解析度 3 種（0x5471b0 取 %3）
static const int opt_def[O_N] = {1, 1, 1, 0, 0, 0, 0, 100, 15, 3, 50, 0, 0};
static int opt_v[O_N] = {1, 1, 1, 0, 0, 0, 0, 100, 15, 3, 50, 0, 0}, opt_bak[O_N];
static UiDialog opt_d, help_d;
static int opt_open, help_open, help_tab;

static void opt_show(void) {  // 0x571790
    if (!ui_load("JM31_\xd3\xce\xcf\xb7\xc9\xe8\xd6\xc3\xbd\xe7\xc3\xe6", &opt_d)) return;
    memcpy(opt_bak, opt_v, sizeof opt_v);
    opt_open = 1;
}
static int help_top;  // 內文捲到第幾行
static void help_content_draw(const UiDialog *d, int ox, int oy);
static int help_content_tap(const UiDialog *d, int ox, int oy, int x, int y);
static void help_show(void) {  // 0x571860
    if (!ui_load("JM36_\xd3\xce\xcf\xb7\xb0\xef\xd6\xfa\xbd\xe7\xc3\xe6", &help_d)) return;
    help_tab = 0;
    help_top = 0;
    help_open = 1;
}
static void opt_apply(void) {  // 0x5474a0：音效／音乐滑桿改值就套到音量
    audio_volume(opt_v[O_MUSIC], opt_v[O_SOUND]);
}
static void opt_ui_pos(const UiDialog *d, int *x, int *y) { *x = (vw - d->w) / 2; *y = (vh - d->h) / 2; }
static int opt_ctrl(const char *name) {
    for (int i = 0; i < O_N; i++)
        if (!strcmp(name, opt_name[i])) return i;
    return -1;
}

static int opt_tap(int x, int y) {
    int ox, oy;
    if (help_open) {
        opt_ui_pos(&help_d, &ox, &oy);
        const UiCtrl *c = ui_button_at(&help_d, ox, oy, x, y);
        static const char *const tab[5] = {"m_pGeneral", "m_pControl", "m_pProp", "m_pFight", "m_pNetwork"};
        for (int i = 0; c && i < 5; i++)
            if (!strcmp(c->name, tab[i])) help_tab = i, help_top = 0;
        if (!c) help_content_tap(&help_d, ox, oy, x, y);
        if (c && !strcmp(c->name, "m_Cancel")) help_open = 0;
        return 1;
    }
    if (!opt_open) return 0;
    opt_ui_pos(&opt_d, &ox, &oy);
    const UiCtrl *c = ui_button_at(&opt_d, ox, oy, x, y);
    if (c) {
        int k = opt_ctrl(c->name);
        if (k >= 0 && k < O_SOUND) opt_v[k] ^= 1;
        else if (!strcmp(c->name, "m_pLight") || !strcmp(c->name, "m_pNextLight")) opt_v[O_LIGHT] = (opt_v[O_LIGHT] + 1) & 1;  // 0x5470f0
        else if (!strcmp(c->name, "m_pPrevLight")) opt_v[O_LIGHT] = opt_v[O_LIGHT] == 0 ? 1 : opt_v[O_LIGHT] - 1;  // 0x547080
        else if (!strcmp(c->name, "m_pNextResolution")) opt_v[O_RES] = (opt_v[O_RES] + 1) % 3;  // 0x5471b0
        else if (!strcmp(c->name, "m_pPrevResolution")) opt_v[O_RES] = opt_v[O_RES] == 0 ? 2 : opt_v[O_RES] - 1;  // 0x547160
        else if (!strcmp(c->name, "m_pDefaults")) { memcpy(opt_v, opt_def, sizeof opt_v); opt_apply(); }
        else if (!strcmp(c->name, "m_pOK")) opt_open = 0;
        else if (!strcmp(c->name, "m_pCancel")) { memcpy(opt_v, opt_bak, sizeof opt_v); opt_apply(); opt_open = 0; }
        return 1;
    }
    for (int k = O_SOUND; k <= O_GAMMA; k++) {  // 滑桿／亮度條：點的位置換算成值
        const UiCtrl *s = ui_find(&opt_d, opt_name[k]);
        const Sprite *t = s ? ui_image(s->type == 8 ? s->sb[0] : s->img, 0) : NULL;
        if (t && x >= ox + s->x && x < ox + s->x + t->w && y >= oy + s->y - 8 && y < oy + s->y + t->h + 8) {
            opt_v[k] = (x - ox - s->x) * opt_max[k] / (t->w > 1 ? t->w - 1 : 1);
            opt_apply();
            return 1;
        }
    }
    return 1;
}

static void opt_draw(void) {
    int ox, oy;
    UiDialog *d = help_open ? &help_d : opt_open ? &opt_d : NULL;
    if (!d) return;
    opt_ui_pos(d, &ox, &oy);
    UiCtrl *bg = ui_find(d, "m_pBackGround");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);  // 底圖旗標是 0，ui_draw 不畫
    ui_draw(d, ox, oy);
    if (help_open) {
        static const char *const tab[5] = {"m_pGeneral", "m_pControl", "m_pProp", "m_pFight", "m_pNetwork"};
        UiCtrl *t = ui_find(d, tab[help_tab]);
        if (t) draw(ui_image(t->img, 1), ox + t->x, oy + t->y);  // 選到的分頁換第 1 幀（猜的）
        help_content_draw(d, ox, oy);
        return;
    }
    for (int k = 0; k < O_N; k++) {
        UiCtrl *c = ui_find(d, opt_name[k]);
        if (!c) continue;
        if (k < O_SOUND) draw(ui_image(c->img, opt_v[k]), ox + c->x, oy + c->y);
        else if (k == O_LIGHT) {  // 0x546fd0：光影鈕的字 = UI_C 2206 + 目前值（静态光影／动态光影）
            char t[64];
            ui_text(2206 + opt_v[k], t, sizeof t);
            draw_text(ox + c->x + (c->w - text_width(t)) / 2, oy + c->y + 1, t, c->w, 0xBECDCE, 1);
        } else if (c->type == 8) {
            const Sprite *tr = ui_image(c->sb[0], 0), *th = ui_image(c->sb[3], 0);
            if (!tr) continue;
            draw(tr, ox + c->x, oy + c->y);
            if (th) draw(th, ox + c->x + (tr->w - th->w) * opt_v[k] / opt_max[k], oy + c->y + (tr->h - th->h) / 2);
        } else if (c->type == 6) ui_bar(c, ox, oy, opt_v[k], opt_max[k]);
    }
}

// ---- 創建新主角 JM09（Poq.exe CCreateNewCharDialog：OnInit 0x573250、選職業頁 0x572a60／0x572aa0、上一個／下一個外貌 0x572140／0x572200、換外貌刷新 0x573530）----
// 5 個職業（m_pRoleFlag1~5 = 職業序號 1~5），每職業有幾種外貌（0x648024：3、3、3、4、3），預覽圖 = 0x63b6d8 表第 4 + 職業×4 + 外貌格；
// 介紹文字 info9_C.dlg 第 3000 + 職業×4 + 外貌 筆（第 0 欄人名、第 1 欄介紹）
// 選定的外貌 = 秦殇/主角 範本（EasyRPG.dll 0x10050630 依 G_PicID2 = 4×(職業序號+1)+外貌 取「秦殇/主角/<職業><人名>」）：
// 角色表列、外觀字串、起始屬性與道具（紙娃娃部件、起始裝備）都跟著範本，名字用輸入框的（new_done → hero_from_story）
// shortcut: 沒有標題畫面所以「取消」當確定；選到的職業鈕（顯示用）換第 1 幀是猜的
static const int new_var_n[5] = {3, 3, 3, 4, 3};
static const int new_pic[24] = {5270, 8200, 8459, 0, 5454, 8198, 8461, 0, 5455, 8196, 8463, 0, 8465, 8455, 5456, 8469, 8144, 8467, 8457, 0, 5450, 0, 0, 0};
static UiDialog new_d;
#include "money.c"

// ---- 開鎖（Attack方式 5，0x10034900 對道具目標 → 0x10034010）、偷竊（Attack方式 6，0x10034900 對人物目標）----
// 原版技能脈絡全域：DAT_104784d0 施放者、d4 目標、d8 技能物件。資料裡沒有技能用 5／6（coverage 0 次），規則照原版，單機劇情與非劇情模式的新技能可用。
// 訊息（EasyRPG_C.dlg）：5005 等級不夠、5006 不是寶箱、5007 鎖拆了、5008 偷失敗、5009 什麼也沒偷到、5010 偷到了 %s(%s)、5129 偷竊技能太低、5130 玩家身上的東西不能偷
// 參數 101（0x10025020 → DAT_10478a2c）開鎖旗標只在開完後歸零；參數 103（0x10025050 → DAT_10478d28）= 偷竊成功率（百分比，與 rand()%100 比）。
// shortcut: 參數 ID 101／103 是用函式表 0x1039e8f4 的位置推出來的（entry 位址差 / 4），沒有資料可驗證。
// 開鎖目標 = 施放點（aim）一格內最近、isbuilding > 0 的劇情物件（原版目標是點選的物件，引擎技能沒有「對建築」的點選）
static int steal_money_get(Actor *t) {
#ifdef STORY
    if (t->sh) return story_get_int(t->sh, "Money");
#endif
    (void)t;
    return 0;
}
// 偷竊（0x10034900 人物分支）
static void skill_steal(Actor *a, Actor *t, const SkillRow *r) {
    if (!t || t == a || !alive(t)) return;
    if (r->level < t->level / 9) { hud_msg(5129); return; }          // 0x1409
    int roll = rnd(100);
    if (skill_param(r, 103) < roll) { hud_msg(5008); return; }       // 0x1390 偷失敗
    if (t->owner || t->side == a->side) { hud_msg(5130); return; }   // 0x140a：有主人的（召喚獸、隊友、自己這邊的）偷不得
    Actor *to = a->owner ? a->owner : a;                              // 東西給施放者的主人（FUN_1004af10）
    if (!(roll & 1)) {
        int take = steal_money_get(t) / 3;
        if (take < 1) { hud_msg(5009); return; }
#ifdef STORY
        story_set_int(t->sh, "Money", story_get_int(t->sh, "Money") - take);
#endif
        if (to == &actors[0]) money_add(take);
        char nm[64] = "", amt[16];
        res_dlg("EasyRPG_C.dlg", 5028, nm, sizeof nm);                // 金钱
        snprintf(amt, sizeof amt, "%d", take);
        hud_msg(5010, nm, amt);
        return;
    }
#ifdef STORY
    if (t->sh && to == &actors[0]) {
        int n = 0, pick = 0, ch = 0;
        for (int c = story_next(t->sh, 0); c; c = story_next(t->sh, c)) n++;
        pick = rnd(n);
        for (int c = story_next(t->sh, 0), i = 0; c; c = story_next(t->sh, c), i++)
            if (i == pick) ch = c;
        int ty = ch ? story_get_int(ch, "Type") : 0, pos = ch ? story_get_int(ch, "UI_Pos") : 0;
        if (ch && ty != 0x31 && ty != 0x32 && !(pos >= 0x1e && pos <= 0x28)) {  // 0x31／0x32 與穿在身上的（UI_Pos 30~40）偷不得
            const char *an = story_alias(ch);
            story_begin();
            story_move(ch, story_player());
            story_end();
            hud_msg(5010, an, an);
            return;
        }
    }
#endif
    hud_msg(5009);
}
static int new_open, new_prof, new_var;
static char hero_name[64] = "\xe6\x89\xb6\xe8\x8b\x8f";  // 扶苏（UTF-8）；創角 m_pRoleName 輸入框可改
static char new_name[64];  // 創角輸入框的內容（UTF-8）；換外貌時重設成該外貌的人名
static int name_len(const char *t) {  // 輸入框上限 20 個位元組（原版 ini 第 13 欄 20，GBK：中文 2、英數 1）；這裡中文算 2
    int n = 0;
    for (const char *q = t; *q;) { uint32_t c = utf8_next(&q); n += c >= 0x2E80 ? 2 : 1; }
    return n;
}
static void new_name_add(const char *t) {  // SDL_TEXTINPUT
    size_t k = strlen(new_name);
    if (name_len(new_name) + name_len(t) <= 20 && k + strlen(t) < sizeof new_name) memcpy(new_name + k, t, strlen(t) + 1);
}
static void new_name_back(void) {
    size_t k = strlen(new_name);
    while (k && ((unsigned char)new_name[k - 1] & 0xC0) == 0x80) k--;
    if (k) new_name[k - 1] = 0;
}
static void new_name_default(void);

static void new_pick(int prof) { new_prof = prof; new_var = 0; new_name_default(); }  // 0x572a60／0x572aa0 → 0x573530：外貌回第 0 個
static void new_step(int d) {  // 0x572140 上一個（0 → 最後）、0x572200 下一個（取餘數）
    new_var = (new_var + d + new_var_n[new_prof]) % new_var_n[new_prof];
    new_name_default();
}
static void new_name_default(void) {  // 0x573530 換外貌：名字欄換成該外貌的人名
    char g[64];
    new_name[0] = 0;
    if (res_dlg_nth("info9_C.dlg", 3000 + new_prof * 4 + new_var, 0, g, sizeof g)) gbk_utf8(g, new_name, sizeof new_name);
}
static void new_show(void) {  // 0x573250
    if (!ui_load("JM09_\xb4\xb4\xbd\xa8\xd0\xc2\xd6\xf7\xbd\xc7\xbd\xe7\xc3\xe6", &new_d)) { diff_open = 1; return; }
    new_open = 1;
    new_prof = hero_prof - 1;
    new_var = 0;
    new_name_default();
    SDL_StartTextInput();
}
#ifdef STORY
// 創角範本：[職業序號 − 1][外貌] = G_PicID2 4×(職業序號)+外貌 對到的劇情人物（story/npc/主角/，G_PicID2：游俠 4~6、力士 8~10、刺客 12~14、術士 16~19、巫師 20~22）
static const char *const NEW_TMPL[5][4] = {
    {"游侠扶苏", "游侠韩信", "游侠李由", NULL}, {"力士孔刚", "力士冒顿", "力士黥布", NULL}, {"刺客赵茜", "刺客荆无忌", "刺客田玥琳", NULL},
    {"术士张良", "术士昆蒲", "术士方仲", "术士廖异"}, {"巫师茵珠", "巫师瑶彤", "巫师淳于秋", NULL}};
// 範本人物 → 引擎的主角：職業（IsActorNPC）、六項屬性、性別年齡、外觀；full = 新遊戲（讀檔時屬性、錢、名字由存檔給）
static void hero_from_story(int full) {
    int pl = story_player();
    char k[16];
    hero_pic = story_get_int(pl, "G_PicID");
    int prof = story_get_int(pl, "IsActorNPC");
    snprintf(k, sizeof k, "G_EquipStr%d", prof);
    const char *eq = story_get_str(pl, k);
    snprintf(hero_look_str, sizeof hero_look_str, "%s", eq ? eq : "");
    hero_gender = story_get_int(pl, "Gender");
    hero_age = story_get_int(pl, "Age");
    if (!full) return;
    hero_prof = prof >= 1 && prof <= 5 ? prof : 1;
    static const char *const A[6] = {"Str", "Con", "Dex", "Savvy", "Charm", "Wisdom"};
    for (int i = 0; i < 6; i++) hero_attr[i] = story_get_int(pl, A[i]);
    hero_money = story_get_int(pl, "Money");
    story_set_name(pl, hero_name);
}
#endif
static void new_done(void) {
    hero_prof = new_prof + 1;
    if (new_name[0]) snprintf(hero_name, sizeof hero_name, "%s", new_name);  // 輸入框空白就保留舊名
#ifdef STORY
    char path[96];
    snprintf(path, sizeof path, "\xe7\xa7\xa6\xe6\xae\x87/\xe4\xb8\xbb\xe8\xa7\x92/%s", NEW_TMPL[new_prof][new_var]);  // 秦殇/主角/
    memset(inv, 0, sizeof inv);  // 預設主角的道具（story_end 已同步進來）隨舊主角物件刪掉；不清的話新範本的武器、布衣因欄位被舊的佔著而掉進背包，變成兩份
    story_player_choose(path);
    hero_from_story(1);
    hero_skill_n = -1;  // 技能表依新職業重讀
    hero_default_hands();
    story_begin(); story_start(sroom); story_end();  // 開場房間的進房間檢查（赵村：亡命天涯）
#endif
    SDL_StopTextInput();
    new_open = 0;
    diff_open = 1;
}
static int new_tap(int x, int y) {
    if (!new_open) return 0;
    int ox = (vw - new_d.w) / 2, oy = (vh - new_d.h) / 2;
    for (int i = 0; i < 5; i++) {  // 職業名稱 m_pRoleName1~5 在原版 DialogInfo 是種類 2 的按鈕（無圖，框 108×26，旗標 0x3804019），和圖示一樣選職業
        char k[24];
        snprintf(k, sizeof k, "m_pRoleName%d", i + 1);
        const UiCtrl *n = ui_find(&new_d, k);
        if (n && n->flags & 9 && x >= ox + n->x && x < ox + n->x + n->w && y >= oy + n->y && y < oy + n->y + n->h) { new_pick(i); return 1; }
    }
    const UiCtrl *c = ui_button_at(&new_d, ox, oy, x, y);
    if (!c) return 1;
    if (!strncmp(c->name, "m_pRoleFlag", 11)) {
        int k = c->name[strlen(c->name) - 1] - '1';
        if (k >= 0 && k < 5) new_pick(k);
    } else if (!strcmp(c->name, "m_pPrevRole")) new_step(-1);
    else if (!strcmp(c->name, "m_pNextRole")) new_step(1);
    else if (!strcmp(c->name, "m_pNewGame") || !strcmp(c->name, "m_pCancel")) new_done();
    return 1;
}
static void new_draw(void) {
    if (!new_open) return;
    int ox = (vw - new_d.w) / 2, oy = (vh - new_d.h) / 2;
    UiCtrl *bg = ui_find(&new_d, "m_pBackGround");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);
    ui_draw(&new_d, ox, oy);
    char k[24], t[96], g[1024], u[2048];
    for (int i = 0; i < 5; i++) {
        snprintf(k, sizeof k, "m_pRoleFlag%d", i + 1);
        UiCtrl *c = ui_find(&new_d, k);
        if (c) draw(ui_image(c->img, i == new_prof), ox + c->x, oy + c->y);
        snprintf(k, sizeof k, "m_pRoleName%d", i + 1);
        c = ui_find(&new_d, k);
        if (c) { ui_text(2132 + i, t, sizeof t); draw_text(ox + c->x + 4, oy + c->y + 5, t, c->w, i == new_prof ? 0xFFFF00 : 0xBECDCE, 1); }
    }
    UiCtrl *c = ui_find(&new_d, "m_pRoleName");  // 名字輸入框（種類 3，上限 20 位元組）
    if (c) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
        SDL_Rect box = {ox + c->x - 2, oy + c->y - 2, c->w + 4, c->h + 6};
        SDL_RenderFillRect(ren, &box);
        draw_text(ox + c->x, oy + c->y, new_name, c->w, 0xFFFFFF, 1);
        if (SDL_GetTicks() / 500 % 2) draw_text(ox + c->x + text_width(new_name), oy + c->y, "|", 8, 0xFFFFFF, 1);
    }
    c = ui_find(&new_d, "m_pRoleTitle");
    if (c) { ui_text(2204, t, sizeof t); draw_text(ox + c->x, oy + c->y, t, c->w, 0xBECDCE, 1); }
    c = ui_find(&new_d, "m_pPortrait");
    int pic = new_pic[new_prof * 4 + new_var];
    if (c) draw(ui_image(pic ? pic : c->img, 0), ox + c->x, oy + c->y);
    c = ui_find(&new_d, "m_pRoleInfo");
    if (c && res_dlg_nth("info9_C.dlg", 3000 + new_prof * 4 + new_var, 1, g, sizeof g)) {
        gbk_utf8(g, u, sizeof u);
        char o[2048];
        size_t n = 0;
        for (const char *p = u; *p && n + 1 < sizeof o; p++) {  // 拿掉顏色標記 [1…] 和圖示 &10000008046
            if (*p == '[' && p[1] >= '0' && p[1] <= '9') { p++; continue; }
            if (*p == ']') continue;
            if (*p == '&' && p[1] >= '0' && p[1] <= '9') { p++; while (*p >= '0' && *p <= '9') p++; p--; continue; }
            o[n++] = *p;
        }
        o[n] = 0;
        SDL_Rect clip = {ox + c->x, oy + c->y, c->w, c->h};
        SDL_RenderSetClipRect(ren, &clip);
        draw_text(ox + c->x, oy + c->y, o, c->w, 0xBECDCE, 1);  // shortcut: 介紹比框長時沒有捲動
        SDL_RenderSetClipRect(ren, NULL);
    }
}

static void sys_button(const char *name) {
    if (!strcmp(name, "m_pSave")) {
        if (actors[0].fight > 0) { ui_say(1507); return; }  // 战斗时不能够保存进度
#ifdef STORY
        if (win_left.open && !strcmp(win_left.d.name, JM56)) { ui_say(1533); return; }  // 镶嵌打造时不能存盘
#endif
        sav_open(1);
    } else if (!strcmp(name, "m_pLoad")) sav_open(2);
    else if (!strcmp(name, "m_pResume") || !strcmp(name, "m_pClose")) sys_open = 0;
    else if (!strcmp(name, "m_pQuit")) quit_open = ui_load(JM102, &quit_d) > 0;
    else if (!strcmp(name, "m_pSetting")) opt_show();
    else if (!strcmp(name, "m_pHelp")) help_show();
}

static void sys_toggle(void) {
    if (dead_open) return;
    if (caption_esc()) return;
    if (map_open) { map_close(); return; }  // 0x53cc10：Esc 關地圖
    if (sys_open) { sys_open = opt_open = help_open = 0; sav_close(); quit_open = 0; return; }
    if (!ui_load(JM50, &sys_d)) return;
    UiCtrl *q = ui_find(&sys_d, "m_pQuit");
    if (q) q->flags |= 9;  // 單機版顯示「退出」（保存并退出 是網路版的）
    sys_open = 1;
    snap_due = 1;
}

// ---- 死亡界面 JM513（Poq.exe 0x4f5240 開窗）：背景 334、m_pGameOver「游戏结束」、m_pQuit 退出（ID 2）、單機的 m_pLoad 读取（ID 201）；
// m_pRestart 重新加入（ID 1）是連線版重生用，單機隱藏。读取 = 開讀檔視窗（sav_open(2)），退出 = 離開遊戲（quit_due）。
// shortcut: 原版死亡到開窗之間的等待與淡出沒查，用 180 拍；掉經驗與扣錢是連線版（0x1004f3a0 NetType > 1）不做。
#define JM513 "JM513_\xcb\xc0\xcd\xf6\xbd\xe7\xc3\xe6"  // JM513_死亡界面
static UiDialog dead_d;
static void dead_show(void) {
    if (!ui_load(JM513, &dead_d)) { quit_due = 1; return; }
    UiCtrl *l = ui_find(&dead_d, "m_pLoad");
    if (l) l->flags |= 9;
    dead_open = 1;
    sys_open = 0;
}
static void hero_dies(void) {
    for (int i = 0; i < INV_N; i++)
        if (inv[i].name[0] && inv[i].type == 61) {
            ground_drop(&inv[i], actors[0].x, actors[0].y);
#ifdef STORY
            if (inv[i].sh) story_take(inv[i].sh);
#endif
            memset(&inv[i], 0, sizeof inv[i]);
            inv[i].pos = -1;
        }
}
static int dead_tap(int x, int y) {
    const UiCtrl *c = ui_button_at(&dead_d, (vw - dead_d.w) / 2, (vh - dead_d.h) / 2, x, y);
    if (!c) return 1;
    if (!strcmp(c->name, "m_pLoad")) sav_open(2);
    else if (!strcmp(c->name, "m_pQuit")) quit_due = 1;
    return 1;
}

// 公用對話框（JM67 只有底圖）：文字 + 是／否。shortcut: 按鈕借 JM102 的圖，位置是自訂的
static void ask_pos(int *x, int *y) { *x = (vw - ask_d.w) / 2; *y = (vh - ask_d.h) / 2; }
static int ask_button(int x, int y) {  // 1 是、2 否、0 沒點到
    int ox, oy;
    ask_pos(&ox, &oy);
    for (int b = 0; b < 2; b++) {
        const Sprite *s = ui_image(b ? 1522 : 1526, 0);
        int bx = ox + (b ? ask_d.w * 3 / 4 : ask_d.w / 4) - (s ? s->w / 2 : 0), by = oy + ask_d.h - 44;
        if (s && x >= bx && x < bx + s->w && y >= by && y < by + s->h) return b + 1;
    }
    return 0;
}

// 系統選單、存取盘、確認框是蓋在畫面上的（點哪裡都吃掉）
static int sys_tap(int x, int y) {
    if (caption_tap(x, y)) return 1;
    if (map_tap(x, y)) return 1;
    if (new_tap(x, y)) return 1;
    if (diff_open) {  // 新遊戲開場先選難度：m_pEasy 410 = 1、m_pNormal 411 = 2、m_pHard 412 = 3（shortcut: 按鈕代碼對應值沒在 Poq 查到，照 0x1039b000 的 1 容易、2 普通、3 困難）
        const Sprite *bg = ui_image(196, 0);
        const UiCtrl *c = ui_button_at(&diff_d, (vw - (bg ? bg->w : 0)) / 2, (vh - (bg ? bg->h : 0)) / 2, x, y);
        if (c && !strcmp(c->name, "m_pEasy")) difficulty = 1;
        else if (c && !strcmp(c->name, "m_pNormal")) difficulty = 2;
        else if (c && !strcmp(c->name, "m_pHard")) difficulty = 3;
        if (c) diff_open = 0;
        return 1;
    }
    if (opt_tap(x, y)) return 1;
    if (money_tap(x, y)) return 1;
    if (quit_open) {
        const UiCtrl *c = ui_button_at(&quit_d, (vw - quit_d.w) / 2, (vh - quit_d.h) / 2, x, y);
        if (c && !strcmp(c->name, "m_Yes")) quit_due = 1;
        if (c) quit_open = 0;
        return 1;
    }
    if (ask_open) {
        int b = ask_button(x, y);
        if (b == 1 && ask_open == 2) {  // 刪除（UI_C 1534 存盘将被删除，确定吗？）
            char p[64];
            save_path(sav_sel, p, sizeof p);
            remove(p);
            save_flush();
            free(sav_head[sav_sel]);
            sav_head[sav_sel] = NULL;
            sav_refresh();
        } else if (b == 1) sav_ok(1);
        if (b) ask_open = 0;
        return 1;
    }
    if (sav_mode) {
        int ox = (vw - sav_d.w) / 2, oy = (vh - sav_d.h) / 2;
        const UiCtrl *c = ui_button_at(&sav_d, ox, oy, x, y);
        if (!c) return 1;
        if (!strncmp(c->name, "m_pBox", 6)) {
            int k = sav_page * 6 + atoi(c->name + 6) - 1;
            if (sav_mode == 1 && k < 2) return 1;  // shortcut: 自动、快速存盘的格子不能手動存，原版是否如此沒查
            sav_sel = k;
        } else if (!strcmp(c->name, "m_Ok")) sav_ok(0);
        else if (!strcmp(c->name, "m_Cancel")) sav_close();
        else if (!strcmp(c->name, "m_pDelete") && sav_sel >= 0 && sav_head[sav_sel]) { ask_open = 2; ui_load(JM67, &ask_d); }
        else if (!strcmp(c->name, "m_pUp") || !strcmp(c->name, "m_pDown")) {
            int n = sav_pages(), p = sav_page + (c->name[3] == 'D' ? 1 : -1);
            if (p >= 0 && p < n) { sav_page = p; sav_refresh(); }
        }
        return 1;
    }
    if (dead_open) return dead_tap(x, y);
    if (sys_open) {
        const UiCtrl *c = ui_button_at(&sys_d, (vw - sys_d.w) / 2, (vh - sys_d.h) / 2, x, y);
        if (c) sys_button(c->name);
        return 1;
    }
    return 0;
}

static void sys_draw(void) {
    if (dead_open) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_Rect all = {0, 0, vw, vh};
        SDL_RenderFillRect(ren, &all);
        ui_draw(&dead_d, (vw - dead_d.w) / 2, (vh - dead_d.h) / 2);
    }
    if (sys_open && !sav_mode) ui_draw(&sys_d, (vw - sys_d.w) / 2, (vh - sys_d.h) / 2);
    if (sav_mode) {
        int ox = (vw - sav_d.w) / 2, oy = (vh - sav_d.h) / 2;
        UiCtrl *title = ui_find(&sav_d, "m_pTitle");
        if (title) title->flags &= ~9;
        UiCtrl *bg = ui_find(&sav_d, "m_pBackGround");
        if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);  // 底圖旗標是 0，ui_draw 不畫
        ui_draw(&sav_d, ox, oy);
        if (title) { draw(ui_image(title->img, sav_mode == 2), ox + title->x, oy + title->y); title->flags |= 9; }  // 標題：第 0 幀保存、第 1 幀读取（0x56cb90）
        for (int i = 0; i < 6; i++) {
            int k = sav_page * 6 + i;
            char key[24], t[96];
            snprintf(key, sizeof key, "m_pScene%d", i + 1);
            UiCtrl *sc = ui_find(&sav_d, key);
            const Sprite *fr = sc ? ui_image(sc->img, 0) : NULL;
            if (sc && fr && sav_tex[i]) {  // m_pScene 的圖是空格的「秦殇」，有存檔就換成縮圖
                SDL_Rect r = {ox + sc->x, oy + sc->y, fr->w, fr->h};
                SDL_RenderCopy(ren, sav_tex[i], NULL, &r);
            }
            snprintf(key, sizeof key, "m_pBox%d", i + 1);
            UiCtrl *bx = ui_find(&sav_d, key);
            const Sprite *bs = bx ? ui_image(bx->img, 2) : NULL;
            if (bs && k == sav_sel) {  // 選中：按下的那幀。shortcut: 那幀跟平常幾乎一樣，另外描一圈金框
                draw(bs, ox + bx->x, oy + bx->y);
                SDL_Rect r = {ox + bx->x, oy + bx->y, bs->w, bs->h};
                SDL_SetRenderDrawColor(ren, 0xE0, 0xC0, 0x60, 255);
                SDL_RenderDrawRect(ren, &r);
            }
            snprintf(key, sizeof key, "m_pSceneName%d", i + 1);
            UiCtrl *nm = ui_find(&sav_d, key);
            const SaveHead *h = sav_head[k];
            t[0] = 0;
            if (k < 2) ui_text(2130 + k, t, sizeof t);
            if (h) {
                char u[96];
#ifdef STORY
                snprintf(u, sizeof u, "%s", h->name[0] ? h->name : h->alias);  // 劇情版的房間名（story_name）是 UTF-8，不能再當 GBK 轉（以前存檔格子顯示「璧垫潙」= 赵村）
#else
                gbk_utf8(h->name[0] ? h->name : h->alias, u, sizeof u);
#endif
                snprintf(t + strlen(t), sizeof t - strlen(t), "%s%s", t[0] ? " " : "", u);
            }
            if (nm && t[0]) draw_text(ox + nm->x, oy + nm->y + 5, t, nm->w, 0xBECDCE, 1);
            snprintf(key, sizeof key, "m_pTime%d", i + 1);
            UiCtrl *tm = ui_find(&sav_d, key);
            if (tm && h) {
                time_t tt = (time_t)h->time;
                struct tm *lt = localtime(&tt);
                strftime(t, sizeof t, "%Y-%m-%d %H:%M", lt);
                draw_text(ox + tm->x, oy + tm->y + 5, t, tm->w, 0xBECDCE, 1);
            }
        }
        UiCtrl *pg = ui_find(&sav_d, "m_pPageNum");
        if (pg) {
            char t[32];
            snprintf(t, sizeof t, "%d / %d", sav_page + 1, sav_pages());
            draw_text(ox + pg->x + (pg->w - text_width(t)) / 2, oy + pg->y + 5, t, pg->w, 0xBECDCE, 1);
        }
    }
    if (ask_open) {
        int ox, oy;
        ask_pos(&ox, &oy);
        ui_draw(&ask_d, ox, oy);
        char t[128];
        ui_text(ask_open == 2 ? 1534 : 1535, t, sizeof t);  // 存盘将被删除／覆盖，确定吗？
        draw_text(ox + (ask_d.w - text_width(t)) / 2, oy + 36, t, ask_d.w, 0xBECDCE, 1);
        for (int b = 0; b < 2; b++) {
            const Sprite *s = ui_image(b ? 1522 : 1526, 0);
            if (s) draw(s, ox + (b ? ask_d.w * 3 / 4 : ask_d.w / 4) - s->w / 2, oy + ask_d.h - 44);
        }
    }
    if (quit_open) ui_draw(&quit_d, (vw - quit_d.w) / 2, (vh - quit_d.h) / 2);
    money_draw();
    opt_draw();
    new_draw();
    map_draw();
    caption_draw();
    if (diff_open) {
        if (!diff_d.count) ui_load("JM21_\xc4\xd1\xb6\xc8\xd1\xa1\xd4\xf1\xbd\xe7\xc3\xe6", &diff_d);
        const Sprite *bg = ui_image(196, 0);
        ui_draw(&diff_d, (vw - (bg ? bg->w : 0)) / 2, (vh - (bg ? bg->h : 0)) / 2);
    }
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

// 說明視窗內文（Poq.exe 0x5743d0，原名「~CHelpDialog」其實是換頁）：第 tab 頁 = UI_C.dlg 從 0x63a6c0 表的起點取筆數筆（2500×9 一般、2600×22 控制、2700×13 道具、2800×10 戰鬥、2900×3 網路），
// 每筆後面加換行，整串交給 m_pContent（0x294）。「&1xxxxxxxxxx」是圖示標記（拿掉）。shortcut: 捲軸位置同 m_pInfo 的推測，只有箭頭能點
static void help_text(int tab, char *out, size_t cap) {
    static const int tbl[5][2] = {{2500, 9}, {2600, 22}, {2700, 13}, {2800, 10}, {2900, 3}};
    size_t n = 0;
    out[0] = 0;
    for (int i = 0; tab >= 0 && tab < 5 && i < tbl[tab][1]; i++) {
        char t[1024];
        ui_text(tbl[tab][0] + i, t, sizeof t);
        for (const char *p = t; *p && n + 2 < cap; p++) {
            if (*p == '&' && p[1] == '1' && strspn(p + 2, "0123456789") >= 10) { p += 11; continue; }
            out[n++] = *p;
        }
        if (n + 2 < cap) out[n++] = '\n';
    }
    out[n] = 0;
}
static void help_content_draw(const UiDialog *d, int ox, int oy) {
    const UiCtrl *c = ui_find((UiDialog *)d, "m_pContent");
    if (!c) return;
    char t[8192];
    help_text(help_tab, t, sizeof t);
    Scroll sc;
    int bar = text_scroll(c, ox, oy, &sc), tw = c->w - 4 - (bar ? sc.track.w : 0);
    int rows = (draw_text(0, 0, t, tw, 0, 0) - 1) / 16, fit = (c->h - 4) / 16;
    if (help_top > rows - fit) help_top = rows - fit;
    if (help_top < 0) help_top = 0;
    SDL_Rect clip = {ox + c->x, oy + c->y, c->w, c->h};
    SDL_RenderSetClipRect(ren, &clip);
    draw_text(clip.x + 2, clip.y + 2 - help_top * 16, t, tw, 0xBECDCE, 1);
    SDL_RenderSetClipRect(ren, NULL);
    if (bar && rows > fit) {
        SDL_RenderSetClipRect(ren, &clip);  // 軌道圖比框高，剪在框內
        draw(ui_image(c->sb[0], 0), sc.track.x, sc.track.y);
        draw(ui_image(c->sb[1], 0), sc.up.x, sc.up.y);
        draw(ui_image(c->sb[2], 0), sc.down.x, sc.down.y);
        SDL_RenderSetClipRect(ren, NULL);
    }
}
static int help_content_tap(const UiDialog *d, int ox, int oy, int x, int y) {
    const UiCtrl *c = ui_find((UiDialog *)d, "m_pContent");
    Scroll sc;
    if (!c || !text_scroll(c, ox, oy, &sc)) return 0;
    if (in_rect(&sc.up, x, y)) help_top--;
    else if (in_rect(&sc.down, x, y)) help_top++;
    else return 0;
    return 1;
}

// 側邊視窗吃掉的點擊回 1（win_off 時視窗畫在別處，由 touch_ui.c 把放大區的座標換算回來再呼叫）
static int win_tap(int x, int y) {
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
        if (!strcmp(w->d.name, JM54)) {  // 道具視窗的錢數鈕 m_pMoney → 丟錢視窗（Poq.exe 0x54b990）
            const UiCtrl *mc = ui_find(&w->d, "m_pMoney");
            if (mc && x >= ox + mc->x && x < ox + mc->x + mc->w && y >= oy + mc->y && y < oy + mc->y + mc->h) { money_open(0); return 1; }
        }
#ifdef STORY
        if (c && !strcmp(w->d.name, JM63)) { shop_button(c->name); return 1; }
        if (!strcmp(w->d.name, JM73)) { if (c) store_button(c->name); return 1; }
        if (!strcmp(w->d.name, JM56)) { if (c) make_button(c->name); else make_list_tap(x, y, ox, oy); return 1; }
#endif
        if (c && !strcmp(c->name, "m_pCClose")) w->open = 0;
        else if (c && role && !strncmp(c->name, "m_pOK[", 6)) role_add_point(atoi(c->name + 6));
        else if (c && !strcmp(w->d.name, JM53) && !strncmp(c->name, "m_pAdd[", 7)) skill_add(atoi(c->name + 7));
        else if (c && role && !strcmp(c->name, "m_pUpgrade")) {  // 升级（Poq.exe 0x5538a0）：切到加點模式，右邊開技能
            role_set_mode(0);
            skill_mode = 0;
            if (!win_right.open || strcmp(win_right.d.name, JM53)) win_toggle(&win_right, JM53);
        } else if (c && !strcmp(w->d.name, JM54)) bag_button(c->name);
        else if (c && !strcmp(w->d.name, JM55) && !strcmp(c->name, "m_pCombo")) set_combine();
        else if (c && !strcmp(w->d.name, JM55) && (!strcmp(c->name, "m_Asunder") || !strcmp(c->name, "m_pItem[10]"))) set_asunder();
        else if (c && !strcmp(w->d.name, JM55) && !strncmp(c->name, "m_pItem[", 8)) item_unequip(&actors[0], INV_BAG + atoi(c->name + 8));
        else if (c) ui_command(c->name);
        return 1;
    }
    return 0;
}

// ---- 道具拖放：從道具視窗的格子（或裝備視窗的裝備欄）按下，拖到別處放開。滑鼠與手機手指共用（手機放開前仍是放大鏡預覽）。
// 拖到裝備欄（或裝備視窗任何地方）= 穿上；拖到別的背包格 = 搬移／交換（箭堆合併）；裝備欄拖到背包格 = 脫下放到那格。
// 沒拖動（< 12 點）就當一般點擊（選取、再點使用）。原版規則沒找到：Poq.exe 拖放事件的細節（哪些格互換、放到地上）沒查，照部位表做
static int drag_src = -1, drag_sx, drag_sy, drag_x, drag_y;
static int drag_cell_at(int x, int y) {
    Win *wr = &win_right, *wl = &win_left;
    if (wr->open && !strcmp(wr->d.name, JM54)) {
        int ox = win_x(wr), oy = win_y(wr);
        for (int i = 0; i < INV_BAG; i++) {
            char key[24];
            snprintf(key, sizeof key, i < 24 ? "m_pItem[%d]" : "m_pShortcutItem[%d]", i < 24 ? i : i - 24);
            const UiCtrl *c = ui_find(&wr->d, key);
            if (!c) continue;
            const Sprite *sp = ui_image(c->img, 0);
            int w = sp ? sp->w : 24, h = sp ? sp->h : 24;
            if (x >= ox + c->x && x < ox + c->x + w && y >= oy + c->y && y < oy + c->y + h) return i;
        }
    }
    if (wl->open && !strcmp(wl->d.name, JM55)) {
        const UiCtrl *c = ui_button_at(&wl->d, win_x(wl), win_y(wl), x, y);
        if (c && !strncmp(c->name, "m_pItem[", 8) && atoi(c->name + 8) < 10) return INV_BAG + atoi(c->name + 8);
    }
    return -1;
}
static int drag_in_equip_win(int x, int y) {
    Win *wl = &win_left;
    return wl->open && !strcmp(wl->d.name, JM55) && x >= win_x(wl) && x < win_x(wl) + wl->d.w && y >= win_y(wl) && y < win_y(wl) + wl->d.h;
}
static int drag_press(int x, int y) {
#ifdef STORY
    if (trade_paused() || make_on() || store_on()) { drag_src = -1; return 0; }  // 買賣、製作、倉庫中點一下就是操作，不拖
#endif
    int c = drag_cell_at(x, y);
    drag_src = c >= 0 && inv[c].name[0] ? c : -1;
    drag_sx = drag_x = x; drag_sy = drag_y = y;
    return drag_src >= 0;
}
static int drag_moved(void) { return drag_src >= 0 && abs(drag_x - drag_sx) + abs(drag_y - drag_sy) >= 12; }
// 放開：有拖動 → 做搬移回 1；沒拖動回 0（呼叫端當一般點擊）
static int drag_release(int x, int y) {
    int src = drag_src;
    drag_src = -1;
    if (src < 0 || abs(x - drag_sx) + abs(y - drag_sy) < 12) return 0;
    Actor *h = &actors[0];
    int dst = drag_cell_at(x, y);
    if (src < INV_BAG) {
        if (dst >= INV_BAG) item_equip_slot(h, src, dst);
        else if (dst >= 0 && dst != src) { if (!arrow_merge(dst, src)) inv_swap(src, dst); item_sel = dst; }
        else if (dst < 0 && drag_in_equip_win(x, y)) item_equip(h, src);
    } else if (dst >= 0 && dst < INV_BAG) {
        if (!inv[dst].name[0]) { inv_swap(src, dst); hero_refresh(h); }
        else item_equip_slot(h, dst, src);  // 放到有東西的格子：背包那件換上來（要穿得上、部位對）
    }
    return 1;
}
static void drag_draw(void) {
    if (!drag_moved()) return;
    const Sprite *sp = inv[drag_src].name[0] && inv[drag_src].icon ? ui_image(inv[drag_src].icon, 0) : NULL;
    if (!sp) return;
    Uint8 a = draw_alpha;
    draw_alpha = 200;
    draw(sp, drag_x - sp->w / 2, drag_y - sp->h / 2);
    draw_alpha = a;
}

// 介面吃掉的點擊回 1（不再拿去走路、打怪）。shortcut: 主列只有按鈕吃點擊，主列底圖空白處還是會走路
static int hud_tap(int x, int y) {
    if (sys_tap(x, y)) return 1;
    if (party_tap(x, y)) return 1;
    if (pick_hand >= 0) {  // 快捷技能視窗開著（0x563ea0 → 0x507180：選到的技能設成該手的當前技能並關窗）：點格子換技能；點哪裡都收起來（原版 0x564140 逐格 PtInRect，沒點到格子就交給基底 0x521020 收起來）
        int px, py;
        pick_pos(&px, &py);
        const UiCtrl *c = ui_button_at(&pick, px, py, x, y);
        int i = c && !strncmp(c->name, "m_pItem[", 8) ? atoi(c->name + 8) : -1;
        if (i >= 0 && i < pick_n) {
            cur_skill[pick_hand] = pick_skill[i];  // 0x4599e0 右手（+0x4ff）／0x459a90 左手（+0x503），包成指令 4 給角色
            hero_skill_apply(&actors[0]);
        }
        pick_hand = -1;
        return 1;
    }
    if (!win_off && win_tap(x, y)) return 1;
    const UiCtrl *c = bar.count ? ui_button_at(&bar, (vw - bar.w) / 2, vh - bar.h, x, y) : NULL;
    if (!c) return 0;
    ui_command(c->name);
    return 1;
}

// ---- 觸控預覽用：不動作的命中測試（touch_ui.c 的放大鏡）----
// 回 0 = 不是介面（遊戲場景；維持立即點擊）；1 = 是介面、放開會點到某個控制項（*r = 它的框，介面座標）；2 = 是介面但那裡沒有控制項（放開 = 取消）。
// win = 1：點在視窗（放大區換算回來的座標），只看側邊視窗。判斷順序同 hud_tap／sys_tap。
// shortcut: 地圖、創角、選項／說明（有滑桿）、字幕、對話不套預覽（回 0，照舊立即點）；詢問框、交易製作清單的框是點附近 24 點的方塊
static SDL_Rect ctrl_rect(const UiCtrl *c, int ox, int oy) {
    const Sprite *s = ui_image(c->img, 0);
    return (SDL_Rect){ox + c->x, oy + c->y, s ? s->w : 24, s ? s->h : 24};
}
static int ctrl_target(const UiDialog *d, int ox, int oy, int x, int y, SDL_Rect *r) {
    const UiCtrl *c = ui_button_at(d, ox, oy, x, y);
    if (!c) return 2;
    *r = ctrl_rect(c, ox, oy);
    return 1;
}
static int win_target(int x, int y, SDL_Rect *r) {
    Win *ws[2] = {&win_left, &win_right};
    for (int i = 0; i < 2; i++) {
        Win *w = ws[i];
        if (!w->open) continue;
        int ox = win_x(w), oy = win_y(w);
        if (x < ox || x >= ox + w->d.w || y < oy || y >= oy + w->d.h) continue;
        UiCtrl *info = ui_find(&w->d, "m_pInfo");
        Scroll sc;
        if (info && info->flags & 9 && text_scroll(info, ox, oy, &sc)) {
            if (in_rect(&sc.up, x, y)) { *r = sc.up; return 1; }
            if (in_rect(&sc.down, x, y)) { *r = sc.down; return 1; }
        }
        int t = ctrl_target(&w->d, ox, oy, x, y, r);
#ifdef STORY
        if (t == 2 && !strcmp(w->d.name, JM56)) { *r = (SDL_Rect){x - 12, y - 12, 24, 24}; return 1; }  // 打造清單點列
#endif
        return t;
    }
    return 0;
}
static int hud_target(int x, int y, int win, SDL_Rect *r) {
    if (win) return win_target(x, y, r);
#ifdef STORY
    if (dialog_open) return 0;
#endif
    if (map_open) {  // 地圖：點到控制項就是控制項（可吸附）；點在地圖框裡沒控制項 = 回 0，照一般點擊（選地點／移鏡頭）
        int ox = (vw - map_d.w) / 2, oy = (vh - map_d.h) / 2 - 0x32;
        const UiCtrl *c = ui_button_at(&map_d, ox, oy, x, y);
        if (c) { *r = ctrl_rect(c, ox, oy); return 1; }
        return 0;
    }
    if (new_open || opt_open || help_open || cr.open || ch.open) return 0;
    if (mny.open) return 2;
    if (diff_open) {
        const Sprite *bg = ui_image(196, 0);
        return ctrl_target(&diff_d, (vw - (bg ? bg->w : 0)) / 2, (vh - (bg ? bg->h : 0)) / 2, x, y, r);
    }
    if (quit_open) return ctrl_target(&quit_d, (vw - quit_d.w) / 2, (vh - quit_d.h) / 2, x, y, r);
    if (ask_open) {
        if (!ask_button(x, y)) return 2;
        *r = (SDL_Rect){x - 20, y - 14, 40, 28};
        return 1;
    }
    if (sav_mode) return ctrl_target(&sav_d, (vw - sav_d.w) / 2, (vh - sav_d.h) / 2, x, y, r);
    if (dead_open) return ctrl_target(&dead_d, (vw - dead_d.w) / 2, (vh - dead_d.h) / 2, x, y, r);
    if (sys_open) return ctrl_target(&sys_d, (vw - sys_d.w) / 2, (vh - sys_d.h) / 2, x, y, r);
    {
        int px, py;
        party_pos(&px, &py);
        Actor *m[PARTY_SLOTS];
        int n = party_list(m);
        for (int i = 0; i < n; i++) {
            char key[32];
            snprintf(key, sizeof key, "m_pRoleBox[%d]", i);
            const UiCtrl *c = ui_find(&party, key);
            const Sprite *s = c ? ui_image(c->img, 0) : NULL;
            if (s && x >= px + c->x && x < px + c->x + s->w && y >= py + c->y && y < py + c->y + s->h) { *r = ctrl_rect(c, px, py); return 1; }
        }
    }
    if (pick_hand >= 0) {
        int px, py;
        pick_pos(&px, &py);
        const UiCtrl *c = ui_button_at(&pick, px, py, x, y);
        if (!c || strncmp(c->name, "m_pItem[", 8)) return 2;
        *r = ctrl_rect(c, px, py);
        return 1;
    }
    if (!win_off) {
        int t = win_target(x, y, r);
        if (t) return t;
    }
    return bar.count && ui_button_at(&bar, (vw - bar.w) / 2, vh - bar.h, x, y) ? ctrl_target(&bar, (vw - bar.w) / 2, vh - bar.h, x, y, r) : 0;
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
    // 聲望 = 劇情主角物件的 Fame 屬性（劇情用 set_attr 改，殺人 -1 等）；五行攻擊 MAP1~5 = ex[43+2i]～ex[44+2i]、五行防禦 MDP1~5 = ex[54+i]（同 strike）
    int fame = 0;
#ifdef STORY
    fame = story_get_int(story_player(), "Fame");
#endif
    int v[25][2] = {{h->exp}, {next}, {fame}, {h->hp, h->hp_max}, {h->mp, h->mp_max}, {hero_attr[STR]}, {hero_attr[CON]}, {hero_attr[DEX]},
                    {hero_attr[SAVVY]}, {hero_attr[CHARM]}, {hero_attr[WISDOM]}, {h->hr}, {h->sr}, {h->ap, h->ap_max},
                    {h->ex[43], h->ex[44]}, {h->ex[45], h->ex[46]}, {h->ex[47], h->ex[48]}, {h->ex[49], h->ex[50]}, {h->ex[51], h->ex[52]}, {h->dp},
                    {h->ex[54]}, {h->ex[55]}, {h->ex[56]}, {h->ex[57]}, {h->ex[58]}};
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

// 道具圖示置中畫在格子上；壞掉的（耐久 0）畫成紅的、沒鑑定的畫暗一點。shortcut: 原版怎麼標示這兩種還沒查
static void item_icon(UiDialog *d, const char *key, int wx, int wy, const Item *it) {
    UiCtrl *c = ui_find(d, key);
    const Sprite *bg = c ? ui_image(c->img, 0) : NULL, *s = c && it->name[0] && it->icon ? ui_image(it->icon, 0) : NULL;
    if (!s) return;
    int broken = it->hp == 0 && it->hp_max > 0;
    if (broken || !it->identify) SDL_SetTextureColorMod(s->tex, 255, broken ? 90 : 160, broken ? 90 : 160);
    draw(s, wx + c->x + ((bg ? bg->w : s->w) - s->w) / 2, wy + c->y + ((bg ? bg->h : s->h) - s->h) / 2);
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
}

// 裝備欄：圖示放大填滿欄位（等比），黑底用加亮混合去掉（圖沒有透明通道）。shortcut: 原版裝備欄怎麼畫（0x556000）沒細查，樣子照使用者記憶
static void item_icon_fit(UiDialog *d, const char *key, int wx, int wy, const Item *it) {
    UiCtrl *c = ui_find(d, key);
    const Sprite *bg = c ? ui_image(c->img, 0) : NULL, *s = c && it->name[0] && it->icon ? ui_image(it->icon, 0) : NULL;
    if (!s || !bg || s->w < 1 || s->h < 1) return;
    int bw = bg->w - 6, bh = bg->h - 6;
    float k = (float)bw / s->w < (float)bh / s->h ? (float)bw / s->w : (float)bh / s->h;
    SDL_Rect r = {wx + c->x + (bg->w - (int)(s->w * k)) / 2, wy + c->y + (bg->h - (int)(s->h * k)) / 2, (int)(s->w * k), (int)(s->h * k)};
    int broken = it->hp == 0 && it->hp_max > 0;
    if (broken || !it->identify) SDL_SetTextureColorMod(s->tex, 255, broken ? 90 : 160, broken ? 90 : 160);
    SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_ADD);
    SDL_RenderCopy(ren, s->tex, NULL, &r);
    SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
}

// 道具說明：名稱、等級、攻防、耐久、需求、重量、價格、特性。
// shortcut: 原版的說明文字（0x100279b0，619 行、UI_C.dlg 的格式字串）還沒翻，特性名稱先用屬性表的中文
static void item_describe(const Item *it, char *out, size_t cap) {
    char nm[96];
    gbk_utf8(it->name, nm, sizeof nm);
    size_t k = snprintf(out, cap, "%s\n等級 %d\n", nm, it->level);
    float ap = 0, ap_max = 0;
    for (int i = 0; i < it->fx_n; i++) {
        if (it->fx[i].id == 41 && !it->fx[i].pct) ap += it->fx[i].v;
        if (it->fx[i].id == 42 && !it->fx[i].pct) ap_max += it->fx[i].v;
    }
    if (ap_max > 0) k += snprintf(out + k, cap - k, "攻擊 %d-%d\n", (int)ap, (int)ap_max);
    if (it->hp_max > 0 && k < cap) k += snprintf(out + k, cap - k, "耐久 %d/%d\n", it->hp, it->hp_max);
    static const struct { int id; const char *n; } L[] = {
        {53, "防禦"}, {21, "生命"}, {22, "精氣"}, {292, "生命%"}, {293, "精氣%"}, {23, "生命上限"}, {24, "精氣上限"}, {27, "力量"}, {28, "體質"},
        {29, "敏捷"}, {31, "悟性"}, {32, "智慧"}, {33, "魅力"}, {43, "金攻"}, {44, "金攻上限"}, {45, "木攻"}, {46, "木攻上限"}, {47, "水攻"},
        {48, "水攻上限"}, {49, "火攻"}, {50, "火攻上限"}, {51, "土攻"}, {52, "土攻上限"}, {54, "金防"}, {55, "木防"}, {56, "水防"}, {57, "火防"},
        {58, "土防"}, {60, "閃避"}, {61, "命中"}, {62, "生命回復"}, {63, "精氣回復"}, {64, "移動速度"}, {65, "攻擊速度"}};
    for (int i = 0; i < it->fx_n && k < cap; i++) {
        const ItemFx *f = &it->fx[i];
        if (f->id == 41 || f->id == 42 || (f->id == 65 && !f->v) || (f->kind == 3 && !it->identify)) continue;
        const char *n = NULL;
        for (size_t j = 0; j < sizeof L / sizeof *L; j++)
            if (L[j].id == f->id) n = L[j].n;
        char lab[24];
        if (!n) snprintf(lab, sizeof lab, "特性%d", f->id), n = lab;
        k += snprintf(out + k, cap - k, "%s %+d%s%s\n", n, (int)f->v, f->pct > 0 ? "%" : "", f->on ? "" : "（相生）");
    }
    if (it->amount > 0 && k < cap) k += snprintf(out + k, cap - k, "數量 %d\n", it->amount);
    if (!it->identify && k < cap) k += snprintf(out + k, cap - k, "未鑑定\n");
    if ((it->lv_req > 1 || it->str_req > 0 || it->dex_req > 0 || it->con_req > 0) && k < cap)
        k += snprintf(out + k, cap - k, "需要 等級%d 力%d 敏%d 體%d\n", it->lv_req, it->str_req, it->dex_req, it->con_req);
    if (k < cap) snprintf(out + k, cap - k, "重量 %d  價格 %d", it->weight, it->value);
}

// 道具視窗：背包、快捷欄的圖示、選取框（m_pHItem，4412）、負重，選中的道具說明畫在視窗左邊
// 道具視窗上方的黑框（底圖 781 在 x105~178、y26~103，左右各一顆 m_pPrevRole／m_pNextRole 換人箭頭）：畫主角穿著裝備的紙娃娃，穿脫即時變。
// 原版規則沒找到：黑框的繪製在資料驅動的介面函式（Poq.exe 0x54ed10／0x54f190 一帶，名稱全被轉成 CString 查表），沒找到畫法；
// shortcut: 站姿面向畫面前方、循環站立序列，位置靠框底中央，沒有隊友切換（m_pPrevRole／m_pNextRole 本來就隱藏）
static void doll_draw(int wx, int wy) {
    Actor *h = &actors[0];
    if (!h->role) return;
    int seq = actor_animation_target(h, DIR_MAP[4]);
    const Seq *q = role_seq(h->role, seq);
    if (!q || !q->count) return;
    SDL_Rect box = {wx + 105, wy + 26, 73, 77};
    SDL_RenderSetClipRect(ren, &box);
    role_hide = 0;
    role_palette_set(h->role, NULL, 10);
    role_draw(h->role, seq, (int)(SDL_GetTicks() / 150), box.x + box.w / 2, box.y + box.h - 6);
    SDL_RenderSetClipRect(ren, NULL);
}
static void bag_draw(UiDialog *d, int wx, int wy) {
    char key[24], t[1024];
    doll_draw(wx, wy);
    for (int i = 0; i < INV_BAG; i++) {
        snprintf(key, sizeof key, i < 24 ? "m_pItem[%d]" : "m_pShortcutItem[%d]", i < 24 ? i : i - 24);
        item_icon(d, key, wx, wy, &inv[i]);
    }
    if (item_sel >= 0 && inv[item_sel].name[0]) {
        snprintf(key, sizeof key, item_sel < 24 ? "m_pHItem[%d]" : "m_pHShortcutItem[%d]", item_sel < 24 ? item_sel : item_sel - 24);
        UiCtrl *h = ui_find(d, key);
        if (h) draw(ui_image(h->img, 0), wx + h->x, wy + h->y);
        item_describe(&inv[item_sel], t, sizeof t);
#ifdef STORY
        if (shop_npc) goto burden;  // 買賣中說明畫在交易介面的 m_pItemInfo
#endif
        int bw = 180, bh = draw_text(0, 0, t, bw - 12, 0, 0) + 12, bx = wx - bw - 4 < 0 ? 0 : wx - bw - 4, by = wy + 20;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_Rect r = {bx, by, bw, bh};
        SDL_RenderFillRect(ren, &r);
        draw_text(bx + 6, by + 6, t, bw - 12, 0xBECDCE, 1);
    }
#ifdef STORY
burden:;
#endif
    UiCtrl *b = ui_find(d, "m_pBurden");
    if (b) {
        snprintf(t, sizeof t, "%d/%d", hero_burden(), hero_burden_max(&actors[0]));
        draw_text(wx + b->x + (b->w - text_width(t)) / 2, wy + b->y, t, b->w + 40, 0xBECDCE, 1);
    }
}

#ifdef STORY
// 交易介面：商品圖示、選取框（m_pHItem，4533）、目前頁籤畫成按下（第 2 幀）；m_pItemIcon、m_pItemInfo 畫選中的道具和價格：
// 商品 = 買價（UI_C 2128「价格: %d」）；賣、修理、鑑定模式下選中的背包道具 = 賣價（2199）或修理、鑑定的錢
// shortcut: m_pItemInfo 的捲軸沒做，超過框高的字截掉
static void shop_draw(UiDialog *d, int wx, int wy) {
    static const char *tabs[4] = {"m_pOther", "m_pLeechdom", "m_pShield", "m_pWeapons"};
    char key[24], t[1100], l[64];
    UiCtrl *c = ui_find(d, tabs[shop_tab]);
    if (c) draw(ui_image(c->img, 2), wx + c->x, wy + c->y);
    for (int i = 0; i < shop_n; i++) {
        snprintf(key, sizeof key, "m_pItem[%d]", i);
        item_icon(d, key, wx, wy, story_item(shop_slot[i]));
    }
    const Item *it = NULL;
    int price = 0;
    if (shop_sel >= 0) {
        snprintf(key, sizeof key, "m_pHItem[%d]", shop_sel);
        if ((c = ui_find(d, key))) draw(ui_image(c->img, 0), wx + c->x, wy + c->y);
        it = story_item(shop_slot[shop_sel]);
        price = trade_price(it, 1);
        ui_text(2128, l, sizeof l);
    } else if (shop_mode >= SHOP_SELL && item_sel >= 0 && item_sel < INV_BAG && inv[item_sel].name[0]) {
        it = &inv[item_sel];
        price = shop_mode == SHOP_SELL ? trade_price(it, 0) : shop_mode == SHOP_REPAIR ? npc_repair_price(it) : it->value_un;
        ui_text(2199, l, sizeof l);
    }
    if (!it) return;
    if ((c = ui_find(d, "m_pItemIcon"))) {
        const Sprite *bg = ui_image(c->img, 0), *s = it->icon ? ui_image(it->icon, 0) : NULL;
        if (s) draw(s, wx + c->x + ((bg ? bg->w : s->w) - s->w) / 2, wy + c->y + ((bg ? bg->h : s->h) - s->h) / 2);
    }
    if (!(c = ui_find(d, "m_pItemInfo"))) return;
    item_describe(it, t, sizeof t);
    size_t k = strlen(t);
    char pl[96];
    snprintf(pl, sizeof pl, l, price);
    snprintf(t + k, sizeof t - k, "\n%s", pl);
    SDL_Rect clip = {wx + c->x, wy + c->y, c->w, c->h};
    SDL_RenderSetClipRect(ren, &clip);
    draw_text(wx + c->x + 2, wy + c->y + 2, t, c->w - 4, 0xBECDCE, 1);
    SDL_RenderSetClipRect(ren, NULL);
}
#endif

// 人物屬性視窗的 m_pDescribe（Poq.exe 0x554600）：名字、职业、性别、年龄、等级
static void hero_describe(const Actor *h, char *out, size_t cap) {
    char l[4][64], prof[64], gender[16];
    for (int i = 0; i < 4; i++) ui_text(2101 + i, l[i], sizeof l[i]);
    ui_text(2131 + hero_prof, prof, sizeof prof);
    ui_text(2197 + hero_gender, gender, sizeof gender);
    // shortcut: 名字原版來自開新遊戲畫面的輸入（0x94c358），那個畫面還沒做，先用劇情主角的名字
    char nm[96];
    snprintf(nm, sizeof nm, "%s", hero_name);
    snprintf(out, cap, "%s\n%s%s\n%s%s\n%s%d\n%s%d\n", nm, l[0], prof, l[1], gender, l[2], hero_age, l[3], h->level);
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

// 敵人狀態框顯示誰（0x459ce0；0x455d60 每拍拿游標下的物件呼叫它，0x459fa0 是先依編號找角色）：
// 指著角色（種類 0x1e～0x64：怪物、NPC、自己都算）就開框、填名字（+0x589；舊索引曾與生命百分比+0x507反寫）和血條，沒指著角色就關框。
// 照原版：血條只在對象沒有狀態 102 時「打開」（0x583e40 傳 0 = 不變），這裡也沒有地方關它，所以有狀態 102 一樣看得到血。
// shortcut: 隊伍多選名單與指著非角色物件的標示，等對應介面接入；角色腳圈已接繪製。
// 手機沒有指著，先用攻擊目標；死掉的不顯示（原版只看種類）

// 血條數值：原版 0x459ce0 每拍把角色的生命百分比（+0x507，由 0x48fae0 同步）交給進度條（0x4fa4c0），不是即時 HP；
// 所以受擊那一拍的血條下一拍才降，而且只有整數百分比。
static int enemy_bar_pct(const Actor *t) { return t->hp_percent; }

static Actor *status_target(Actor *h, Actor *hover, int mouse) {
    Actor *t = mouse ? hover : actor_target(h);
    return t && t->act != ACT_DEAD ? t : NULL;
}

// 將現有EasyRPG／怪物文字來源交給角色名稱更新入口，再由HUD消費顯示副本。
// shortcut: SInit顯示名稱來源仍以現有EasyRPG／怪物文字橋接；完整SInit載入時改讀原始名稱。
static void actor_title(Actor *t, char *u, size_t n) {
    char base[256];
    if (t->mon) gbk_utf8(t->mon->title, base, sizeof base);
    else snprintf(base, sizeof base, "%s", t == &actors[0] ? "扶苏" : "");
    const char *s = base;
#ifdef STORY
    if (!t->mon && t->sh && *story_name(t->sh)) s = story_name(t->sh);
#endif
    actor_display_name_set(t, s, actor_name_number(t), 1);
    snprintf(u, n, "%s", actor_name(t));
}

static void hud_draw(Actor *hero) {
    static int tried;
    if (!tried) {
        tried = 1;
        // 原版 CGameDialog 0x537bb0 建立各 HUD 視窗、0x5389e0 依畫面大小重新擺位：JM51 中間 = 底部置中、
        // JM515 敵人狀態 = 上方置中距頂 0x14、JM65 消息 = 貼在主列上方、JM60 地圖 = 置中再往上 0x32、JM108 選擇 = 底部置中。
        // 單機沒有 JM106／JM101（交談輸入輸出）、JM75（網路狀態）。shortcut: JM65／JM108 的位置在各自視窗做時再對
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xd0\xbc\xe4", &bar);      // JM51_状态界面_中间（GBK）
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xf7\xbf\xd8", &party);    // JM51_状态界面_主控
        ui_load("JM515_\xb5\xd0\xc8\xcb\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6", &enemy);    // JM515_敌人状态界面
    }
    if (dialog_open) return;  // 0x565330：對話開著 HUD 全藏
    // 敵人狀態：上方置中、距頂 20（原版位置）
    Actor *t = party_shown();  // 點了隊伍面板的成員（0x551480）
    if (!t) t = status_target(hero, hover_actor, hover_mouse);
    if (enemy.count && t) {
        int ex = (vw - enemy.w) / 2, ey = 20;
        UiCtrl *nm = ui_find(&enemy, "m_pName"), *bl = ui_find(&enemy, "m_pBlood");
        if (nm) {
            char u[96];
            actor_title(t, u, sizeof u);
            draw_text(ex + nm->x + (nm->w - text_width(u)) / 2, ey + nm->y + 3, u, nm->w, 0xBECDCE, 1);
        }
        if (bl) ui_bar_ex(bl, ex, ey, enemy_bar_pct(t), 100, 0, !(bl->flags & 0x10));
    }
    if (!bar.count) return;
    int ox = (vw - bar.w) / 2, oy = vh - bar.h;
    ui_draw(&bar, ox, oy);
    UiCtrl *red = ui_find(&bar, "m_pRed"), *blue = ui_find(&bar, "m_pBlue");
    // 左管（紅）貼右、右管（藍）貼左：兩條都由外側往中間空掉
    if (red) ui_bar_ex(red, ox, oy, hero->hp, hero->hp_max, 1, 0);
    if (blue) ui_bar_ex(blue, ox, oy, hero->mp, hero->mp_max, 0, 0);
    for (int i = 0; i < 2; i++) {
        char key[16];
        snprintf(key, sizeof key, "m_pSkill[%d]", i);
        const UiCtrl *c = ui_find(&bar, key);
        if (c) skill_icon(c, ox, oy, cur_skill[i]);
    }
    if (party.count) {
        int px, py;
        party_pos(&px, &py);
        party_draw(&party, px, py);
    }
    if (win_off && win_tex && (win_left.open || win_right.open)) {  // 直式觸控：視窗畫在另一張圖，之後放大貼到遊戲畫面下方
        SDL_SetRenderTarget(ren, win_tex);
        SDL_SetRenderDrawColor(ren, 16, 16, 22, 255);  // 不透明底：放大時不用處理半透明邊緣
        SDL_RenderClear(ren);
    }
    if (win_left.open) {
        int wx = win_x(&win_left), wy = win_y(&win_left);
        ui_draw(&win_left.d, wx, wy);
#ifdef STORY
        if (!strcmp(win_left.d.name, JM63)) shop_draw(&win_left.d, wx, wy);
        if (!strcmp(win_left.d.name, JM56)) make_draw(&win_left.d, wx, wy);
        if (!strcmp(win_left.d.name, JM73)) store_draw(&win_left.d, wx, wy);
#endif
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
        // 裝備：圖示畫在 m_pItem[UI_Pos − 30]（Poq.exe 0x556000 CEquipDialog::OnInit：11 欄、載入人物 0x5567f0）
        for (int i = INV_BAG; !strcmp(win_left.d.name, JM55) && i < INV_N; i++) {
            char slot[16];
            snprintf(slot, sizeof slot, "m_pItem[%d]", i - INV_BAG);
            item_icon_fit(&win_left.d, slot, wx, wy, &inv[i]);
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
        if (!strcmp(win_right.d.name, JM54)) bag_draw(&win_right.d, wx, wy);
        UiCtrl *money = ui_find(&win_right.d, "m_pMoney");  // 道具：原版把錢數交給 m_pMoney 顯示（Poq.exe 0x54db42），「金钱：%d」是提示文字
        if (money) {
            char t[16];
            snprintf(t, sizeof t, "%d", hero_money);
            draw_text(wx + money->x + (money->w - text_width(t)) / 2, wy + money->y + (money->h - 16) / 2, t, money->w, 0xBECDCE, 1);
        }
    }
    drag_draw();
    if (win_off && win_tex) SDL_SetRenderTarget(ren, fade_canvas);
    if (SDL_GetTicks() < toast_until) {
        int tw = text_width(toast);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
        SDL_Rect r = {(vw - tw) / 2 - 8, 56, tw + 16, 22};
        SDL_RenderFillRect(ren, &r);
        draw_text((vw - tw) / 2, 59, toast, tw + 1, 0xFFE080, 1);
    }
    modal_begin();
    sys_draw();
    modal_end();
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

static void air_draw_view(void);
#include "viewport.c"

static void camera_to_cell(int x, int y) { viewport_center_at(&viewport, unit_scene, x, y); }  // 0x461530

// 劇情鏡頭（Poq.exe 0x44e380；story 0x533 v[1] = 2）：角色類物件（種類 0x1e~0x64）平滑置中（0x461680 平滑模式）
static void camera_look_at(int h) {
    Actor *a = actor_of(h);
    if (a && a->object_kind >= 30 && a->object_kind <= 100) viewport_center_actor(&viewport, unit_scene, a, 1);
}

// 屍體物件換階段（CDeadObj 0x4986c0／CPartDeadObj 0x4990e0／0x498c00；dead_phase 0..2 = 原版 +0xa3 的 1..3）。
// 進階段1（原版2）：高度 0、計數歸零、序列 +8（屍體序列，模板沒有該序列就停在末幀）；
// 進階段2（原版3）：一般屍體原版畫進地面層（0x498240，這裡仍是 Actor）；部件屍體只設 +0x73 = 3 並凍結全部精靈（0x49e9e0），
// 這裡階段1之後已停在末幀，不用另外凍。階段0的格子屍體標記 0x498ba0 引擎沒有（FUNCS skip）。
static void dead_phase_enter(Actor *a, int phase) {
    a->dead_phase = phase;
    a->part_wait = 0;
    if (phase != 1) return;
    a->z = 0;
    if (a->role && role_seq(a->role, a->act_seq + 8 + DIR_MAP[a->dir])) {
        a->act_seq += 8;
        a->act_tick = a->act_frame = a->frame_tick = a->act_done = 0;
        a->last_frame = -1;
        a->body_ready = 0;
    }
}

// 屍體物件每拍（CDeadObj 0x498a30；CPartDeadObj 0x499780 播放 = actor_animation_tick／血跡用時間取幀；CFlyPartDeadObj 0x4997f0 先飛：階段0每4拍（全域拍 %4 == 1）、計數 > -20 時沿 facing 反方向退 fly_speed/64 格）。
// 高度每拍 -4；階段0計數 < 1 且播完 → 階段1（計數先歸零再 -1，與原版相同）；
// 階段1計數 > 200 且序列播完 → 階段2；階段2在畫面內計數歸零，畫面外 > 0x50 拍移除。回傳 0 = 移除。
// 設計數 0x498b80 = part_wait 起始值（0x4988f0 播放 = actor_animation_tick）。
// shortcut: 階段2不畫進地面層、也沒有格子屍體編號（0x498930 寫格子），Actor 一直畫到移除。
static int dead_obj_tick(Actor *a) {
    if (a->z > 0) a->z -= 4;
    if (a->dead_phase == 0) {
        if (a->part_kind == 2 && (actor_logic_tick & 3) == 1 && a->part_wait > -20) {
            a->x -= GRID_X[a->facing & 7] * a->fly_speed / 64.0f;
            a->y -= GRID_Y[a->facing & 7] * a->fly_speed / 64.0f;
        }
        if (a->part_wait < 1 && a->act_done) dead_phase_enter(a, 1);
        --a->part_wait;
    } else if (a->dead_phase == 1) {
        if (a->part_wait++ > 200 && a->act_done) { dead_phase_enter(a, 2); return 1; }
    } else {
        SDL_Point p = viewport_world_screen(&viewport, ax64(a), ay64(a));
        SDL_Rect rect = viewport_actor_rect(a, p.x, p.y, 0), screen = {0, 0, viewport.w, viewport.h};
        if (SDL_HasIntersection(&rect, &screen)) { a->part_wait = 0; return 1; }
        return a->part_wait++ <= 0x50;
    }
    return 1;
}

// Poq.exe 0x4a1b40：由最舊往最新、按原索引餘數採樣，最後不足間隔的項不畫。
static void illusion_draw(const Illusion *v) {
    int blend = v->start;
    for (int i = v->count - 1; i >= v->interval; i--) {
        if (i % v->interval) continue;
        const IllusionUnit *u = &v->unit[i];
        SDL_Point p = viewport_world_screen(&viewport, u->x, u->y);
        int b = blend < 0 ? 0 : blend > 16 ? 16 : blend;
        Uint8 saved = draw_alpha;
        // shortcut: 原版0x280色表混色以SDL alpha承接；格子光照／16位色表接入時升級。
        draw_alpha = (Uint8)(b * 255 / 16);
        draw(image(u->frame.ref), p.x - u->frame.dx, p.y - u->frame.dy);
        draw_alpha = saved;
        blend += v->delta;
    }
}

// Poq.exe 0x575bd0：按部件順序畫殘影，放在腳下效果之後、角色本體之前。
static void actor_illusion_draw(const Actor *a) {
    const Role *r = a->role;
    if (!r || !r->part_count) return;
    int seq = actor_seq(a);
    for (int i = 0; i < r->part_count; i++)
        if (a->illusion[i] && r->body[i] && seq >= 0 && seq < r->body[i]->count)
            illusion_draw(a->illusion[i]);
}
#include "air.c"
static void air_draw_view(void) { air_draw(&viewport); bird_draw(&viewport); }

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
    id_add(&anims, 0x100001);
    for (int i = 0; i < s->object_count; i++)
        for (int p = 0; s->objects[i].m && p < s->objects[i].m->part_count; p++)
            if (s->objects[i].m->parts[p].anim) id_add(&anims, s->objects[i].m->parts[p].gid);
    for (int i = 0; i < s->npc_count; i++) {
        const RoleDef *d = roledef_find(s->npcs[i].tmpl);
        if (d && d->beast && d->id < 1500) id_add(&anims, d->anim);
    }
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
    const Anim *bird_anim = anim_get(0x100001);
    for (int seq = 0; bird_anim && seq < bird_anim->count; seq++) add_seq_frames(&frames, bird_anim, seq);
    for (int k = 0; k < frames.n; k++) {
        if (n % 4096 == 0) v = realloc(v, (n + 4096) * sizeof *v);
        v[n++] = (Near){0, frames.v[k]};
    }
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
    for (int i = 0; i < s->npc_count; i++) {
        const NpcSpec *sp = &s->npcs[i];
        const RoleDef *d = roledef_find(sp->tmpl);
        if (!d || !d->beast || d->id >= 1500) continue;
        frames.n = 0;
        const Anim *a = anim_get(d->anim);
        for (int seq = 0; a && seq < a->count; seq++) add_seq_frames(&frames, a, seq);
        for (int k = 0; k < frames.n; k++) {
            if (n % 4096 == 0) v = realloc(v, (n + 4096) * sizeof *v);
            v[n++] = (Near){dist_to(hero, sp->x, sp->y), frames.v[k]};
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

// 開局裝備：劇情（扶苏）item_new(8, 1, −1, "qingtongwandao1") 放 UI_Pos 32 右手、item_new(26, 1, −1, "buyi1") 放 31 盔甲
#ifndef STORY
static void hero_equip(void) {
    if (item_new(&inv[32], 8, 1, -1, "qingtongwandao1")) inv[32].pos = 32;
    if (item_new(&inv[31], 26, 1, -1, "buyi1")) inv[31].pos = 31;
    hero_look();
}
#endif

// 載入場景（別名 = 場景檔名），主角站到入口 entry_id；失敗時保留原場景
static int enter_scene(Scene *scene, Actor *hero, const char *alias, int entry_id) {
    char file[96];
    snprintf(file, sizeof file, "%.80s.Scn", alias);
    Scene next;
    if (!scene_load(&next, file)) { fprintf(stderr, "載入場景失敗：%s\n", file); return 0; }
    scene_ready_set(0);
    npc_behavior_reset(hero);  // 0x4934b0（CProtagonist虛擬重設）= 寫未讀欄位後走CNpc重設0x492e70
    if (scene->cells) scene_free(scene);
    *scene = next;
    unit_scene = scene;
    hero->occupancy_removed = 1;
    hero->grid_valid = 0;
    scene_client.scene = scene;
    scene_client.controlled = (int)(hero - actors);
#ifdef STORY
    shop_close();
    sroom = story_room(alias);
    if (story_parent(story_player()) != sroom) story_place(sroom);  // 新遊戲、讀檔、測試直接指定場景
    story_begin();
    story_refresh(sroom);  // 重放怪物、商店進貨
    story_end();
    {   // 天氣機率表（EasyRPG 0x10042420 送 0x516）：房間的 G_WEA_*
        static const char *const K[9] = {"G_WEA_BigRain", "G_WEA_SmallRain", "G_WEA_RedRain", "G_WEA_BigSnow", "G_WEA_SmallSnow",
                                         "G_WEA_Hail", "G_WEA_RedSky", "G_WEA_PinkSky", "G_WEA_WhiteSky"};
        int pct[9];
        for (int i = 0; i < 9; i++) pct[i] = (signed char)story_get_int(sroom, K[i]);
        weather_table(pct);
        light_table((signed char)story_get_int(sroom, "G_Day"), (signed char)story_get_int(sroom, "G_Night"),
                    (signed char)story_get_int(sroom, "G_Type"), wx.idx);  // 0x516 的 Night/Day/Type → 0x44e5d0
    }
#else
    light_table(15, 0, 10, 0);  // 房間預設（EasyRPG 0x1005d390）
#endif
    weather_resume();
    light_clear();
    light_buildings(scene);
    // 快取：下載好的資料與貼圖留著（回到剛離開的場景不重新下載、重新解碼），超過上限才整批清
    if (res_cache_clear() || tex_bytes > TEX_CAP || tile_tag_n >= 250) cache_clear(); else role_surface_free();
    // 外觀快取跟著場景換：先留主角的，再重建 NPC
    hero_look();  // 範本外觀＋身上裝備（第一次進場景時還沒有）
    const RoleDef *hd = hero_def.id ? &hero_def : roledef_by_graphic(HERO_GRAPHIC);
    roles_clear();
    preload_roles(scene, hd);
    if (hd) { actor_setup(hero, hd); if (hd == &hero_def) actor_look_set(hero, &hero_def); }
#ifdef STORY
    // 模板會直接覆寫組別；第一拍索敵前便恢復劇情玩家組，不等資源載完的actors_sync。
    actor_group(hero, story_get_int(story_player(), "G_AIGroup"));
#endif
    spawn_npcs(scene);
    {  // 地上的道具留在丟下它的場景（房間的物件）：離開時存起來、回來放回去。原版世界存檔含房間物件（0x1000a8b0），存檔尾巴也存這些
        int old = -1, nw = -1, freeslot = -1;
        for (int i = 0; i < 8; i++) {
            if (!gstore[i].alias[0]) { if (freeslot < 0) freeslot = i; continue; }
            if (!strcmp(gstore[i].alias, ground_alias)) old = i;
            if (!strcmp(gstore[i].alias, alias)) nw = i;
        }
        if (ground_alias[0] && strcmp(ground_alias, alias)) {
            if (old < 0) old = freeslot >= 0 ? freeslot : 0;  // shortcut: 超過 8 個場景就蓋掉第一格
            snprintf(gstore[old].alias, sizeof gstore[old].alias, "%s", ground_alias);
            gstore[old].n = ground_n;
            memcpy(gstore[old].g, ground, sizeof(ground[0]) * ground_n);
        }
        if (strcmp(ground_alias, alias)) {
            ground_n = 0;
            if (nw >= 0 && gstore[nw].n > 0) { ground_n = gstore[nw].n; memcpy(ground, gstore[nw].g, sizeof(ground[0]) * ground_n); gstore[nw].n = 0; }
            snprintf(ground_alias, sizeof ground_alias, "%s", alias);
        }
    }
    walk_cx = -1;
    unit_n = 0;
    const Entry *en = scene_entry(scene, entry_id);
    if (en) {
        hero->grid_valid = 0;
        hero->x = en->x + 0.5f;
        hero->y = en->y + 0.5f;
        for (int o = 0; o < 8; o++)
            if (DIR_MAP[o] == en->dir) hero->dir = o;  // shortcut: 假設入口方向跟動畫方向同編號，還沒跟原版對照
    }
    hero->object_dead = 0;
    if (!actor_bind_scene(hero, scene) || !actor_place(hero, (int)hero->x, (int)hero->y, 1)) return 0;
    hero->moving = 0;
    hero->act_tick = 0;
    viewport_enter(&viewport, scene, hero);
    prefetch_scene(scene, hero);
    air_enter(scene);
    sur_enter(scene);
    scene_ready_set(1);  // 0x46ec80（淡出 + 重設輸入 0x4590f0）
    party_protect = 0;
    input_sel_add(hero);
    input_player_add(hero);  // 0x465cf0
    chest_state_sync(scene, 0);
    for (int i = 0; i < actor_count; i++) if (actors[i].role) actor_explore(&actors[i]);
    return 1;
}

// 原版加密設定檔裡 key=value 的數值（key 是 GBK）；找不到回 def
static int ini_int(const char *text, const char *key, int def) {
    size_t n = strlen(key);
    for (const char *p = text; p && *p; p = strchr(p, '\n'), p = p ? p + 1 : NULL)
        if (!strncmp(p, key, n)) return atoi(p + n);
    return def;
}

// Data/music.txt：場景別名 → 背景音樂檔名（第 2 欄）；回傳第 3 欄（战斗音乐名）是否非空（只決定播完要不要隔幾拍，原版沒有戰鬥曲）。
// 原版 0x579df0 逐列比對別名，找不到用第 1 列（第 2 欄 normal1）
static int scene_music(const char *alias, char *out, size_t n) {
    snprintf(out, n, "normal1");
    size_t size;
    uint8_t *d = res_file("Data", "music.txt", &size);
    if (!d) return 1;
    res_unxor(d, size);
    int gap = 1;
    for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char *tab = strchr(line, '\t');
        if (!tab || (size_t)(tab - line) != strlen(alias) || strncasecmp(line, alias, tab - line)) continue;
        char *end = strchr(tab + 1, '\t');
        gap = end && end[1] && end[1] != '\t';
        if (end) *end = 0;
        if (tab[1]) snprintf(out, n, "%s", tab + 1);
        break;
    }
    free(d);
    return gap;
}
// 進場景：照原版把場景種類（G_Type）與音樂交給 CMusic 狀態機
static void scene_music_enter(const char *alias) {
    char m[64];
    int gap = scene_music(alias, m, sizeof m);
    audio_scene_music(lt.type, m, gap);
}

// ---- 滑鼠（CUserInput，Poq.exe 0x454790～0x457360）----
// 游標種類（0x455d60 每拍算、0x457360 設，全域 0x663f54）= 游標動畫 0x100011 的第幾個序列（它正好 8 個）：
// 0 平常、1 指著地上道具（0x8c）、2 對話、3 箱子（0x85）、4／7 物件（0x81、0x17）、5 不能走、6 攻擊
// shortcut: 種類對到序列是用序列數推的，畫游標的那段（介面伺服器）沒查到
enum { CUR_NORMAL, CUR_ITEM, CUR_TALK, CUR_CHEST, CUR_OBJ, CUR_BLOCK, CUR_ATTACK, CUR_USE };
enum { CMD_NONE, CMD_MOVE, CMD_ATTACK, CMD_TALK, CMD_PICK };

// 游標下的角色（0x454a90 → 0x442b10，all = 1、不分邊），(cx, cy) 回游標那格
static Actor *input_char(int zx, int zy, int ox, int oy, const Scene *s, int *cx, int *cy) {
    pick_ox = ox, pick_oy = oy, pick_s = s;
    SDL_Point grid = viewport_screen_grid(&viewport, zx, zy);
    *cx = grid.x; *cy = grid.y;
    return pick_at(zx, zy, *cx, *cy, 1, -1, pick_box);
}

// 0x4848c0：原版一圈螺旋順序；格旗標必須同時有0x88，排除門與非0/2狀態。
static Object *input_building(const Scene *s, int x, int y) {
    Spiral it; spiral_init(&it, x, y, 1);
    while (spiral_next(&it)) {
        if (it.x < 0 || it.y < 0 || it.x >= s->w || it.y >= s->h || !s->cells) continue;
        const Cell *c = &s->cells[it.y * s->w + it.x];
        if ((c->flags & 0x88) != 0x88 || c->building < 0 || c->building >= s->object_count) continue;
        Object *o = &s->objects[c->building];
        if (o->m && o->m->type >= 0x80 && o->m->type <= 0x100 && o->m->type != 0x96 && building_available(o)) return o;
    }
    return NULL;
}

// 原版旗標 +0x341 第 n 個 = 身上有狀態 n；狀態 0 战斗状态 = 我們的 fight 計時
static int in_fight(const Actor *h) { return h->fight > 0; }

// 游標那格的建築（0x454a90 的 +0x40）：格子沒資料時 0x4848c0 螺旋找；否則格子探索值 >= 2、旗標 0x88、有建築索引；
// 可破壞種類（0x439b20：0x80～0x100 且非門 0x96）狀態不是 0／2（已用過／已破壞）就清掉，門不受狀態限制。
static const char *openall_alias;
static Object *hover_building(int cx, int cy) {
    if (!unit_scene) return NULL;
    const Cell *bc = cell_at(cx, cy);
    if (!bc) return input_building(unit_scene, cx, cy);
    if (bc->view < 2 || (bc->flags & 0x88) != 0x88 || bc->building < 0 || bc->building >= unit_scene->object_count) return NULL;
    Object *o = (Object *)&unit_scene->objects[bc->building];
    if (!o->m || (o->m->type >= 0x80 && o->m->type <= 0x100 && o->m->type != 0x96 && !building_available(o))) return NULL;
    return o;
}
// 門（0x4831c0 → 0x482db0）：模型 0x96、名稱第一個空格分詞是 "go"（0x5ed4ec）。這種門 active = 0（不靠走進去感應），只能點門
static int door_is_go(const Object *o) {
    if (!o->m || o->m->type != 0x96 || !o->name[0]) return 0;
    const char *t = o->name + strspn(o->name, " ");
    return strcspn(t, " ") == 2 && !strncmp(t, "go", 2);
}

// 0x455d60 算游標那段：c = 游標下的角色、item = 指著的地上道具、(cx, cy) = 游標那格。
// 角色是自己（原版看自己這邊的名單）就往下看道具和格子；沒指著角色、格子主角那層不能過（0x435c00）或有角色站 → 5
// shortcut: 原版只在格子 +0x13 ≠ 0 時才看能不能走（+0x13 意義沒查），這裡都看；主角的層當 1（一般地面）；
// 箱子、可破壞、門還不是物件，沒有 3、4、7；拖拉選人框時是 0；游標 1 時原版秀道具名字（訊息 0x143／0x145）沒做；游標在介面上時原版是 0
static int input_cursor(const Actor *h, const Actor *c, int item, int cx, int cy) {
    if (c && c != h) {
        if (in_fight(h) || is_foe(c, h)) return CUR_ATTACK;
        return has_status(c, 103) ? CUR_NORMAL : CUR_TALK;  // 103 无对话（目標的）
    }
    if (item >= 0) return CUR_ITEM;
    Object *b = hover_building(cx, cy);
    if (b) {
        if (b->m->type == 0x8c || door_is_go(b)) return CUR_ITEM;  // 門：0x455d60 → 0x457360(1)（+0xa0 = 0、+0xa8 ≠ 0）
        if (b->m->type == 0x80) return CUR_ATTACK;
        if (b->m->type == 0x81) return b->state == 2 ? CUR_OBJ : CUR_USE;
        if (b->m->type == 0x85) return CUR_CHEST;
    }
    const Cell *cl = cell_at(cx, cy);
    if (!c && cl && (!cell_pass(cl, 1) || cell_actor0(cx, cy))) return CUR_BLOCK;
    return CUR_NORMAL;
}

// XMOUSE 游標數量（+0x34，0x4a0aa0 設；旗標 +0x30 的 0x4a0ac0 沒有用途，見下）：原版介面「拿著道具」時設；這裡沒有游標拿道具（點選再點），沒人寫。
// shortcut: 旗標 0x100／0x140（換色表）、第二精靈（+0x28）沒有來源，不畫；原版畫幀還加 −幀偏移（+0x12／+0x14），引擎幀 dx／dy 當中心偏移用，這裡改成只置中
static int xmouse_count;

// 0x4a05e0：游標畫在序列最大幀寬高（0x49d860／0x49d8f0）置中的位置，數量 > 1 在游標位置寫數字（原版 SetView 游標大小的矩形）
static void xmouse_draw(const Anim *cur, int k, int x, int y, Uint32 tick) {
    if (!cur || k >= cur->count || !cur->seqs[k].count) return;
    const Seq *q = &cur->seqs[k];
    int mw = 0, mh = 0;
    for (int i = 0; i < q->count; i++) {
        const Sprite *s = image(q->frames[i].ref);
        if (!s) continue;
        if (s->w > mw) mw = s->w;
        if (s->h > mh) mh = s->h;
    }
    const Frame *f = &q->frames[frame_at(q, tick)];
    const Sprite *s = image(f->ref);
    if (s) draw(s, x - mw / 2 + f->ex, y - mh / 2 + f->ey);
    if (xmouse_count > 1) {
        char n[16];
        snprintf(n, sizeof n, "%d", xmouse_count);
        draw_text(x, y, n, 64, 0xffffff, 1);
    }
}

// 左鍵放開（0x454f00）要給主角的指令（原版包成 0xf023 訊息送給主角和選到的隊友）。
// 溃散（17）、盅惑人心（14）不收指令；战斗状态點誰都打（9），平常點敵人打、點其他人對話（0xf）；再來是撿道具（0x12），其他 = 走過去（1）
// Ctrl 對地點出手（8）／角色（9）、物件（0x80 打、0x81／0x8c 用、0x85 開、0x96 門 0x10）見 click_building；shortcut: Ctrl 的判斷原版是 vtable+0x60 的按鍵查詢，這裡用 SDL Ctrl 狀態
static int click_cmd(const Actor *h, const Actor *c, int item) {
    if (has_status(h, 17) || has_status(h, 14)) return CMD_NONE;
    if (c && c != h) return in_fight(h) || is_foe(c, h) ? CMD_ATTACK : CMD_TALK;
    return item >= 0 ? CMD_PICK : CMD_MOVE;
}
// 選人名單裡的隊友收左鍵命令（0x454f00 對名單每個人下命令）：移動走到格子、攻擊以點到的敵人為目標。主角照原樣在下面處理
static void input_team_cmd(int cmd, Actor *m, int cx, int cy) {
    for (int i = 0; i < sel_n; i++) {
        Actor *a = sel_list[i];
        if (!a || !a->team || !alive(a) || has_status(a, 0x11) || has_status(a, 0xe)) continue;
        if (cmd == CMD_MOVE) { actor_command(a, 1, cx, cy, 0, 1, 1); actor_clear_target(a); }
        else if (cmd == CMD_ATTACK && m) { actor_target_set(a, m); actor_command(a, 11, 1, 0, 0, 0, 0); }  // 入戰（同 npc_pick_target）
    }
}

// 左鍵放開、游標下有建築時（0x454f00 LAB_004551b0）：0x8c／0x81 → 命令 0x12（使用物件）；0x80 → 命令 9（打，目標是建築）；
// 0x85 → 命令 0x13（開箱）；0x96 且是 "go" 門 → 命令 0x10（門點 x, y、建築）；0x96 其他名稱 → 走過去；
// 其他種類（包含一般建築）原版什麼都不做（回傳 1 = 吃掉這次點擊）。回傳 0 = 照一般走路。
// 門點（0x482dd0）：模型門偏移不是 0 才用「擺放格 − 錨點 + 門偏移」，偏移為零就維持點的那格（出口區 go east 等就是這樣）。
static int click_building(Actor *hero, Object *b, int cx, int cy) {
    int idx = (int)(b - unit_scene->objects), type = b->m->type;
    int ok = 1;
    if (debug_cells) printf("click bld %d %s type=%d state=%d at cell %d,%d hero %d,%d\n", idx, b->name, type, b->state, cx, cy, (int)hero->x, (int)hero->y);
    if (type == 0x8c || type == 0x81) ok = actor_command(hero, 0x12, idx, 0, 0, 1, 1);
    else if (type == 0x80) { if (hero->team || hero == &actors[0]) ok = actor_command(hero, 9, -1, BLD_TARGET + idx, 0, 1, 1); }
    else if (type == 0x85) ok = actor_command(hero, 19, idx, 0, 0, 1, 1);
    else if (type == 0x96) {
        if (!b->name[0]) return 1;
        if (!door_is_go(b)) return 0;
        int dx = cx, dy = cy;
        if (b->m->door_x || b->m->door_y) dx = b->door_x, dy = b->door_y;
        ok = actor_command(hero, 0x10, dx, dy, idx, 1, 1);
    }
    (void)ok;
    hero->foe = NULL; hero->want = NULL; walk_cx = -1;
    return 1;
}

// 遊戲速度鍵（0x455d60，連線客戶端不改）：數字鍵盤 + 或 = 加快（值 −3，最小 16）、數字鍵盤 − 或 - 減慢（值 +3，最大 31），
// 再換回速度 5 − (值 − 16) / 3 設定（0x448a70）。怪現象（照原版）：開局 21 按一次加快 → 18 → 速度 5 → 16（最快）
static void game_speed_key(int faster) {
    int v = game_speed_ms;
    v = faster ? (v < 17 ? 16 : v - 3) : (v < 31 ? v + 3 : 31);
    game_speed_set(5 - (v - 16) / 3);
}

#ifdef STORY
// Poq.exe 0x484a80（CTriBld 每拍）的感應：倒數 < 1 才掃占地格，格子上有 AI 組 10000..60000 的角色（0x4c78b0 取格上單位，+0x339）
// 就執行建築名稱的文字指令（0x482db0 取名稱；BNT 觸發區出現 "s_pctstart <劇情事件路徑>"（開始劇情事件）與 "openall <別名>"（0x10019d70 開同名箱子；
// 資料 90 次，之前漏掉），c_aigroup <原組> <新組>（0x10013270，xianbei 約 5 處）把房間裡原組的人物改成新組；其他名稱（iron_ore、mucai 兩個字串在 Poq.exe／EasyRPG.dll 位元組搜尋 0 次，不是指令，原版沒有作用；snow1 是連線版房間的入口）），倒數設 1000；
// 沒人踩到倒數設 1（隔拍再掃）。動畫部分（狀態 5 播完回 1）在 res.c scene_buildings_tick。
// 原版名稱不是 s_pctstart 時送命令 0x13d 給那個角色（接收端是連線版的指令字串，BNT 觸發區沒有這種名稱，不做）。
static void trigger_sense_tick(Scene *s) {
    for (int i = 0; i < s->trigger_count; i++) {
        Trigger *t = &s->triggers[i];
        int is_pct = !strncmp(t->event, "s_pctstart ", 11), is_oall = !strncmp(t->event, "openall ", 8), is_cai = !strncmp(t->event, "c_aigroup ", 10);  // 文字指令 s_pctstart 0x1001c210、openall 0x10019d70、c_aigroup 0x10013270
        if ((!is_pct && !is_oall && !is_cai) || !t->m || t->m->type != 0x96) continue;  // 0x4847a0：只有 0x96 且名稱不是 go 才 active = 1
        if (t->cooldown > 0) { t->cooldown--; continue; }
        int hit = 0;
        const Model *m = t->m;  // 掃模型占地矩形（沒有模型 = 擺放格）
        int x0 = m ? t->x - m->anchor_x + m->grid_x : t->x, y0 = m ? t->y - m->anchor_y + m->grid_y : t->y;
        for (int y = y0; y < y0 + (m ? m->grid_h : 1) && !hit; y++)
            for (int x = x0; x < x0 + (m ? m->grid_w : 1); x++) {
                if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
                const Actor *a = s->cells[y * s->w + x].actor;
                if (a && a->grp >= 10000 && a->grp <= 60000 && trigger_hit(t, x, y)) { hit = 1; break; }
            }
        if (!hit) { t->cooldown = 1; continue; }
        char path[128];
        t->cooldown = 1000;
        if (is_cai) { int from = -1, to = 0; if (sscanf(t->event + 10, "%d %d", &from, &to) == 2) { story_begin(); story_c_aigroup(from, to); story_end(); } continue; }
        if (is_oall) { gbk_utf8(t->event + 8, path, sizeof path); chest_open_all(path); continue; }  // 觸發區 "openall <別名>"（兵馬俑坑 shhl_neicheng 約 90 處）：同別名的箱子全開
        gbk_utf8(t->event + 11, path, sizeof path);
        story_begin();
        story_pct_start(path);
        story_end();
    }
}
#endif

// 第一次需要時才讀房間表（EasyRPG.dll），回傳目前場景的房間索引
static int load_world(World *world, int *tried, const char *alias) {
    *tried = 1;
    js_status(-1);
    if (!world_load(world)) { fprintf(stderr, "讀不到 EasyRPG.dll 的房間資料，不能切換場景\n"); return -1; }
    return world_find(world, alias);
}

// 大地圖（Poq.exe 0x44ce00：場景寬 > 0x3f，沒有場景也算）換到大地圖才自動存檔（Config 自动存盘、不是回放、不是連線）。
// 原版還要「不在劇情播放模式」：過場播放中 scene_record_mode = 2（rec_begin；0x472e60 讀的 Rec 模式），不自動存檔
static int scene_big(const Scene *s) { return !s || s->w > 0x3f; }
// shortcut: 隊伍在戰鬥中（0x458620）不自動存檔——原版呼叫處沒找到，先用在這裡避免存到打到一半的狀態
static int autosave_due(int was_big, const Scene *now) { return was_big && scene_big(now) && scene_record_mode != 2 && !input_party_fighting(); }

#ifndef STORY
static int trade_paused(void) { return 0; }  // 沒有劇情就沒有交易視窗
#endif

// ---- 過場播放器（劇情事件 CERPCT：start_plot 0x1004f1a0 → 訊息 0x52e → 客戶端回放 Rec/User/<別名>.rec；Poq.exe CScnClientRecord）----
// .rec = 0x80 位元組檔頭（"soloRecord 1.0"）＋ 封包串，每包 u32 長度＋內容。'@' 控制包（0x4725c0）、'#' 伺服器同步包（0x470300），
// '@' 0 = 時間戳：封包依戳在第幾拍播（0x472f50：戳 == 目前拍才處理，之後的等到那拍）。
// 這裡的做法：開始時把真角色全部凍結、不畫（不是原版那樣載入另一份場景拷貝），錄影裡的角色另外建（Actor.rec），播完刪掉、真角色放回。
// shortcut: 錄影的場景名跟目前房間不同就不播（原版會載另一份場景）；'#' 6／8／9（地上道具、飛行單位、招式）、'@' 5／8／10／12（旗標、
// 霧、光影）沒做；'@' 4 的語音、'@' 11 的音效已做（audio_voice）；對話框 = 0x4722f0／0x5963e0 的頭頂文字框（rec_dialog_*）；劇情角色在錄影裡是新建的拷貝（外觀字串照錄影），不是房間裡那個 NPC。
typedef struct {
    uint8_t *d; size_t n, p;
    unsigned tick;
    int id[96], idx[96], cnt;          // 錄影物件編號 → actors 索引
    RoleDef look[96];                  // 各錄影角色目前的外觀（#3 子碼 0x01 在這份上改款式）
    char strq[8][256]; int strn;       // '#' 10 字串佇列
    int follow;                        // 鏡頭跟隨的錄影物件編號（0 = 不跟）
    int saved_count, hero_foe;
    int hold[MAX_ACTORS];              // 被凍結的真角色（佔格已放掉）
    int hold_n;
    int skip;
    // 對話框（CScnClientRecord +0x1fd4 文字、+0x1fd8 說話者、+0x1fec 剩餘拍、+0x1ff0 有語音）
    int dlg_id, dlg_voice, dlg_played, dlg_t, dlg_n6, dlg_hold;  // dlg_t < 0 = 沒顯示；dlg_hold = 這句已經讓時間軸等了幾拍
    char dlg_text[512];
    int sv_wx, sv_wleft, wx_set;       // #1 子類 1 的天氣（vfunc+8 = 0x4697b0 換天氣）蓋掉前的天氣，rec_end 還原
    int gid[32], gid_n;                // #6 放的地上道具編號（ground[].id），rec_end 移除
    int sv_day, sv_night, sv_type, light_set, ctl64;  // #1 子類 1 改過的亮度參數（rec_end 還原）；@8 存的 CScnClient +0x64
    char room[64]; int foreign;        // 錄影開始時的房間；foreign = 錄影目前在別的場景（#13 子類 2 換場景）而沒載成功，那些場景的建築／物件這裡沒有
    char cur[64], back[64];            // cur = 錄影目前載著的場景；back = 開始時的房間（換過場景才有，rec_end 載回去）
    float bx, by; int bdir;            // 開始時主角的位置，載回去時放回
    int dlg_sx, dlg_sy;                // 說話者上一次畫的螢幕位置（actor_draw 記下）
} Rec;
static Rec rec;

static Actor *rec_actor(unsigned id) {
    for (int i = 0; i < rec.cnt; i++) if (rec.id[i] == (int)id) return &actors[rec.idx[i]];
    return NULL;
}
static unsigned rec_u16(const uint8_t *b) { return b[0] | b[1] << 8; }
static unsigned rec_u32(const uint8_t *b) { return b[0] | b[1] << 8 | b[2] << 16 | (unsigned)b[3] << 24; }

// 外觀字串 "動作方式*部件_款式_動作方式*…"（'#' 1 的字串佇列第 3 欄）蓋在角色表列上
static void rec_look(RoleDef *look, const char *str) {
    if (!*str || !strcasecmp(str, "NULL")) return;
    look->pli = atoi(str);
    const char *e = strchr(str, '*');
    if (e) styles_equip(look->styles, sizeof look->styles, look->graphic, look->pli, e + 1);
}

// '#' 1（0x470300 case 1 子類 0）：依圖形 ID（角色表）在 (x, y) 建角色；"player" 用目前主角的外觀
static void rec_add(const uint8_t *b, const Scene *s) {
    char tok[3][256] = {{0}};
    if (rec.strn) sscanf(rec.strq[0], "%255s %255s %255s", tok[0], tok[1], tok[2]);
    rec.strn = 0;
    unsigned id = rec_u16(b + 2), pic = rec_u16(b + 5), grp = rec_u16(b + 13);
    int cx = rec_u16(b + 7) >> 6, cy = rec_u16(b + 9) >> 6;
    int is_player = !strcmp(tok[0], "player");
    const RoleDef *d = is_player ? (hero_look(), &hero_def) : roledef_find((int)pic);
    if (!d || !role_drawable(d) || actor_count >= MAX_ACTORS || rec.cnt >= 96) return;
    RoleDef look = *d;
    if (!is_player) rec_look(&look, tok[2]);
    Actor *a = &actors[actor_count];
    memset(a, 0, sizeof *a);
    npc_behavior_reset(a);
    a->x = cx + 0.5f; a->y = cy + 0.5f;
    a->object_kind = 40;
    a->rec = 1;
    actor_setup(a, d);
    if (!actor_bind_scene(a, s) || !actor_place(a, cx, cy, 1)) { a->role = NULL; a->rec = 0; return; }
    if (memcmp(&look, d, sizeof look)) actor_look_set(a, &look);
    a->grp = grp == 3 || is_player ? actors[0].grp : 20000 + (int)grp;  // 錄影的組別 3 = 主角那組
    a->side = a->grp != actors[0].grp;
    rec.look[rec.cnt] = look;
    rec.id[rec.cnt] = (int)id; rec.idx[rec.cnt++] = actor_count++;
}

static void rec_remove(Actor *a) {
    actor_unoccupy(a);
    a->role = NULL; a->rec = 0;
}

// '#' 3：角色屬性小包（0x470300 case 3）。子碼 0x01 換外觀（0x48d810，字串佇列第 0 條 "部件_款式*…"）、0x02 名字（字串佇列第 0 條）、
// 0x33 移除、0x34 動態狀態、0x35 層號（+0x66）、0x36 名字編號（0x48e820）、0x65 組別、0x66 動畫序列、0x67 生命百分比（0x48e8f0）；其他子碼（資料裡有 0x79）原版沒處理
static void rec_attr(const uint8_t *b, Actor *a) {
    switch (b[4]) {
    case 0x01:
        if (rec.strn) {
            for (int i = 0; i < rec.cnt; i++)
                if (&actors[rec.idx[i]] == a) { styles_equip(rec.look[i].styles, sizeof rec.look[i].styles, rec.look[i].graphic, rec.look[i].pli, rec.strq[0]); actor_look_set(a, &rec.look[i]); }
            rec.strn = 0;
        }
        break;
    case 0x02:
        if (rec.strn) {
            char u[256]; gbk_utf8(rec.strq[0], u, sizeof u);
            actor_display_name_set(a, u, actor_name_number(a), 1);
            rec.strn = 0;
        }
        break;
    case 0x34: actor_dynamic_command(a, b[5], 0, 0, 0); break;                // 0x496550
    case 0x35: a->layer = b[5]; break;                                         // CCharactor +0x66
    case 0x36: { char base[256]; snprintf(base, sizeof base, "%s", actor_name(a)); char *p = strrchr(base, '('); if (p) *p = 0; actor_display_name_set(a, base, b[5], 1); break; }  // 0x48e820
    case 0x66: {  // 0x48c190
        int id = (int)rec_u16(b + 5);
        // 錄影角色放過招（#9）後動作狀態不會自己結束（錄影模式不跑動作計時），一直播招式動畫：下一個明確指定的動畫序列（走路、站立）要把它結束，
        // 不然主角打完敵人離開時一邊走一邊轉圈（2026-11-21 使用者回報；位置 t=1008 起有走、body_seq 卻一直是招式序列）
        if (a->act && id / 8 != a->act_seq / 8) { MUnit *u = main_of(a); if (u) unit_die(u, SDL_GetTicks()); act_end(a); a->act = ACT_NONE; a->casting = 0; a->body_ready = 0; }
        actor_animation_select(a, id);
        break;
    }
    case 0x65: actor_group(a, 20000 + (int)rec_u16(b + 5)); break;             // 0x48e6e0
    case 0x67: actor_health_percent(a, (int)rec_u16(b + 5)); break;            // 0x48e8f0
    case 0x33: actor_remove(a, b[5]); break;                                   // 0x486910
    default: break;
    }
}

// 一包 '#'：0x470300 的分派
// '#' 9：錄影角色 a 對目標點放招式 id（0x43acb0 建 MethodMain）。錄影角色的技能列是模板招式，沒有這招就補一筆（沒有技能數值結算，同 actor_cast_try 的模板招式）
static int rec_cast(Actor *a, int id, int x, int y) {
    const Method *m = method_find(id);
    if (!m || !a->def) return 0;
    if (actor_skill_find(a, id) < 0) actor_skill_add(a, (ActorSkill){.id = id, .min = m->range_min, .max = m->range_max, .enabled = 1, .p8 = 1, .p9 = 1}, 0);
    return actor_cast_try(a, id, x, y, NULL);
}
// 錄影換到別的場景（例如 pct_wmtya 在片頭動畫場景 ptdhcj 演）：第一次換記下主角的位置與原房間，rec_end 載回。回 0 = 載不進去，維持原場景
static int rec_scene_swap(Scene *s, Actor *hero, const char *alias) {
    if (!rec.back[0]) { snprintf(rec.back, sizeof rec.back, "%s", rec.room); rec.bx = hero->x; rec.by = hero->y; rec.bdir = hero->dir; }
    if (!enter_scene(s, hero, alias, -1)) return 0;
    snprintf(rec.cur, sizeof rec.cur, "%s", alias);
    return 1;
}
static void rec_net(const uint8_t *b, size_t n, Scene *s) {
    if (n < 3) return;   // '#' 13 子類 2 只有 3 byte
    Actor *a = n >= 4 ? rec_actor(rec_u16(b + 2)) : NULL;
    switch (b[1]) {
    case 1:
        if (n >= 5 && b[4] == 0) rec_add(b, s);
        else if (n >= 13 && b[4] == 1 && !rec.foreign) {   // 子類 1（0x470300 case 1）：進場景的亮度參數 0x44e5d0(類型 u16 b7、夜 b9、日 b10、b11)；原版載另一份場景，這裡在原房間上暫改、rec_end 還原
            if (!rec.light_set) { rec.sv_day = lt.day; rec.sv_night = lt.night; rec.sv_type = lt.type; rec.light_set = 1; }
            light_table(b[10], b[9], (int)rec_u16(b + 7), wx.idx);
            if (n >= 15 && !rec.wx_set) { rec.sv_wx = wx.idx; rec.sv_wleft = wx.left; rec.wx_set = 1; }
            if (n >= 15) weather_set(b[12], (int)rec_u16(b + 13));  // 0x470300 case 1：(**(CGodClient+8))(b12, u16(b13)) = 0x4697b0 換天氣（天氣編號、持續拍數；資料 b12 都是 0 晴、拍數 239~1370）
        }
        break;
    case 2: if (a) rec_remove(a); break;
    case 3: if (a && n >= 6) rec_attr(b, a); break;
    case 4:   // 0x496260：位置修正（離太遠 > 127 像素才改）、占格換到 (b8, b10)、0x4967d0 插值走過去（回放模式 2 靠逐拍插值推進）
        if (a && n >= 13) {
            // 0x496260：+0xaf（每拍步長 A）= 封包第 12 byte（伺服端當時的速度，插值用它、不是角色自己的速度表），離封包位置超過 127 像素才直接改世界位置
            // （歐氏距離，之前拿 command_world 比而且從來沒移動角色，落後的人永遠追不回來）。shortcut: 層號重算（0x4c7d60）用 actor_place 的
            int px = rec_u16(b + 4), py = rec_u16(b + 6);
            a->move_a = a->move_remaining = b[12];
            if (dist_of(ax64(a), ay64(a), px, py) > 127) { a->x = px / 64.0f; a->y = py / 64.0f; }
            actor_place(a, (int)rec_u16(b + 8), (int)rec_u16(b + 10), 1);
            actor_interpolate(a, -1, -1, -1);
        }
        break;
    case 5: if (a && n >= 5) actor_status_notify(a, b[4] & 0x7f, b[4] >> 7, -1); break;  // 0x48d2c0
    case 10:
        if (rec.strn < 8) { snprintf(rec.strq[rec.strn++], sizeof rec.strq[0], "%s", (const char *)b + 2); }
        break;
    case 7:   // 0x470300 case 7：建築狀態。物件編號 = 低 14 位 − 2001 = BNT 建築順序（0x4a2600 配號：2000 起、先加 1 再用；0x44f4c0 逐筆建築 CTriBld／NormalBuilding 各配一個）；
              // 0x484ee0 只對 CTriBld（類型 0x80～0x100，物件種類 12）起作用，其他種類原版只寫除錯訊息。
              // 原版規則沒找到：資料裡有些場景（hd_mishi 第 1 次進入）偏移是 2000，同場景第 2 次是 2001，配號前還有什麼東西先配了編號沒查到；這裡一律照 2001
        if (!rec.foreign && n >= 5) {
            int i = (int)(rec_u16(b + 2) & 0x3fff) - 2001;
            if (i >= 0 && i < s->object_count && s->objects[i].tri) scene_building_state(s, i, b[4]);
        }
        break;
    case 13:   // 命令字串；子類 2 = 換場景（場景名 = 前一條 #10 字串）：不是開始時的房間就暫停處理場景建築（這裡不會換場景）
        if (n >= 3 && b[2] == 2 && rec.strn) {  // 載入場景：原版載另一份場景拷貝（舊物件全清）。這裡換了場景就真的載入；同一場景再載 = 清掉錄影建的角色
            char sc[64]; snprintf(sc, sizeof sc, "%.63s", rec.strq[rec.strn - 1]); char *sp = strchr(sc, ' '); if (sp) *sp = 0;
            for (int i = 0; i < rec.cnt; i++) { Actor *r = &actors[rec.idx[i]]; if (r->rec) rec_remove(r); }
            rec.cnt = 0;
            if (strcasecmp(sc, rec.cur)) rec.foreign = !rec_scene_swap(s, &actors[0], sc);
        }
        rec.strn = 0;
        break;
    case 6:   // 地上道具（0x470300 case 6 → AddProp 0x450cc0）：PicID b4 ≥ 2000（PropMdl 地面圖）、b8 = 0 才有掉落動畫（0x49a7e0）、格子 x = u16(b9)、y = u16(b11)、擁有者 u16(b13)。
        // 資料 24 筆中 20 筆只有 12 byte（舊版格式）：原版照新格式讀 x、y 會讀到封包外的 0xcdcd（格子在地圖外），AddObj 失敗什麼都不放，這裡也不放。PicID < 2000（陷阱等）沒有 15 byte 的資料，不做
        if (n >= 15 && rec_u16(b + 4) >= 2000 && !rec.foreign && rec.gid_n < 32) {
            Item it = {0};
            it.pos = -1;
            snprintf(it.name, sizeof it.name, "item");
            it.ground = (int)rec_u16(b + 4);
            int g0 = ground_n;
            ground_drop(&it, rec_u16(b + 9) + 0.5f, rec_u16(b + 11) + 0.5f);
            if (ground_n > g0) {
                if (b[8]) { ground[g0].t = PROP_TICKS; ground[g0].z = 0; ground[g0].dx = (int)ground[g0].x * 64 + 32; ground[g0].dy = (int)ground[g0].y * 64 + 32; }
                rec.gid[rec.gid_n++] = ground[g0].id;
            }
        }
        break;
    case 8:   // 飛行效果單位（0x450700(單位編號 b2, x b4, y b6, 0, 0)）：沒有放的人（來源 0），單位在 (x, y) 像素
        if (n >= 8) { MUnit src0; memset(&src0, 0, sizeof src0); unit_new((int)rec_u16(b + 2), (int)rec_u16(b + 4), (int)rec_u16(b + 6), &src0, SDL_GetTicks()); }
        break;
    case 9:   // 招式單位（0x43acb0(角色 b2, 招式 b4, 目標點 (s16 b6, s16 b8))）：角色出手動畫 MethodMain + 招式的觸發
        if (a && n >= 10) rec_cast(a, (int)rec_u16(b + 4), (short)rec_u16(b + 6), (short)rec_u16(b + 8));
        break;
    default: break;
    }
}

// 一包 '@'：0x4725c0 的分派
// 0x4c8630 SetFog：全場景探索值 0 → 0xc9（沒探索過）、1 → 1、2 → 0（全亮）。Rec '@' 10 與除錯指令 SetFog 共用
static void scene_fog_set(Scene *sc, int m) {
    for (int i = 0; sc && i < sc->w * sc->h; i++)
        if (m >= 0 && m <= 2) sc->cells[i].view = m == 0 ? 0xc9 : m == 1 ? 1 : 0;
}
// 除錯指令（Poq.exe 0x455d60 讀 DAT_00662924，DAT_0094c1b8 == 1 的測試主控台）：SetFog <n>（0x4c8630）；GetSpeed → "#ObjSpeed"（0x4774d0 回報主角的速度欄位）。
// 單機入口：--cmd "SetFog 0"（可重複，網頁用網址參數 cmd= 要改 web/shell.html，主樹的 web/ 不動）
static char dbg_cmds[16][64];
#ifdef STORY
static char flag_watch[8][48];
static int flag_watch_n, flag_watch_val[8];
static void flag_watch_tick(unsigned tick) {
    if (!flag_watch_n || tick % 30) return;
    story_begin();
    for (int i = 0; i < flag_watch_n; i++) {
        int v = (int)ENGINE(0x1005df70, story_player(), (intptr_t)flag_watch[i]);
        if (v != flag_watch_val[i]) { printf("flag %s = %d\n", flag_watch[i], v); flag_watch_val[i] = v; }
    }
    story_end();
}
#endif
static int dbg_cmd_n;
static char kill_pending[48];
static void debug_command(const char *cmd) {
    char w[32] = "";
    int n = 0;
    sscanf(cmd, "%31s %d", w, &n);
    if (!strcasecmp(w, "SetFog")) scene_fog_set((Scene *)unit_scene, n);
#ifdef STORY
    else if (!strcasecmp(w, "SetFlag")) {  // 走查用：--cmd "SetFlag 旗標名 值" 設主角的劇情旗標（新增＋設值，等於劇情的 NewFlag／SetFlag）
        char name[48]; int v = 0;
        if (sscanf(cmd, "%*s %47s %d", name, &v) == 2) { story_begin(); ENGINE(0x1005e850, story_player(), (intptr_t)name, -1); ENGINE(0x1005f270, story_player(), (intptr_t)name, v); story_end(); printf("SetFlag %s = %d\n", name, v); }
    }
    else if (!strcasecmp(w, "KillAlias")) sscanf(cmd, "%*s %47s", kill_pending);  // 走查用：--cmd "KillAlias 別名"：等場景裡有劇情別名相符的怪物／NPC 就打倒（走原版倒下流程：第 10、12 格、清掉時的第 2 格）
    else if (!strcasecmp(w, "StartPlot")) {  // 走查用：--cmd "StartPlot 劇情事件路徑"（路徑不可有空白）= 感應區的 s_pctstart
        char path[96];
        if (sscanf(cmd, "%*s %95s", path) == 1) { story_begin(); story_pct_start(path); story_end(); printf("StartPlot %s\n", path); }
    }
    else if (!strcasecmp(w, "WatchFlag")) {  // 走查用：--cmd "WatchFlag 旗標名" 之後每 30 拍看一次，值變了就印（沒有這個旗標 = -1）
        char name[48];
        if (sscanf(cmd, "%*s %47s", name) == 1 && flag_watch_n < 8) { snprintf(flag_watch[flag_watch_n], sizeof flag_watch[0], "%s", name); flag_watch_val[flag_watch_n++] = -2; }
    }
#endif
    else if (!strcasecmp(w, "GetSpeed")) {
        const Actor *h = &actors[0];
        snprintf(toast, sizeof toast, " m_iB_C_MOVE_SPEED = %d\n m_iSpeedCent = %d\n m_nNormalSpeed = %d\n m_nFightSpeed = %d\n",
                 actor_move_bonus(h), status_speed(h), h->speed, h->fight_speed);
        toast_until = SDL_GetTicks() + 4000;
        printf("%s", toast);
    }
}
static void debug_commands_apply(void) {
    for (int i = 0; i < dbg_cmd_n; i++) debug_command(dbg_cmds[i]);
    dbg_cmd_n = 0;
#ifdef STORY
    static int kill_tick;
    if (kill_pending[0] && ++kill_tick % 30 == 0) {
        int n = 0;
        for (int i = 1; i < actor_count; i++)
            if (actors[i].sh && !actors[i].team && actors[i].hp > 0 && !strcmp(story_alias(actors[i].sh), kill_pending)) { actors[i].hp = 0; actor_kill(&actors[i], NULL); n++; }
        static int grace;
        if (n) { printf("KillAlias %s x%d\n", kill_pending, n); grace = 80; }  // 同名的怪物是陸續產生的（過場中先出一隻）：打倒後再多等 80 次檢查（2400 拍）才收工
        else if (grace && !--grace) kill_pending[0] = 0;
    }
#endif
}
static void rec_ctl(const uint8_t *b, size_t n, Scene *sc) {
    switch (b[1]) {
    case 1:   // 鏡頭定位（螢幕多出 640×480 的部分對半）
        if (n >= 10) viewport_set(&viewport, unit_scene, (int)rec_u32(b + 2) - (viewport.w - 640) / 2, (int)rec_u32(b + 6) - (viewport.h - 480) / 2);
        break;
    case 2: if (n >= 6) { uint32_t v = rec_u32(b + 2); float f; memcpy(&f, &v, 4); zoom_set(f); } break;  // 0x4493f0
    case 4:   // 0x4725c0 case 4：說話者 +2、PTalk_C.dlg 鍵 +6（也是語音檔號）；對話框文字 = 字串裡 \r\n → \n
        if (n >= 10) {
            char g[1024], u[2048];
            audio_voice((int)rec_u32(b + 6));  // 沒有檔就停掉上一句
            if (res_dlg("PTalk_C.dlg", (int)rec_u32(b + 6), g, sizeof g)) {
                gbk_utf8(g, u, sizeof u);
                size_t k = 0;
                for (const char *q = u; *q && k + 1 < sizeof rec.dlg_text; q++) {  // 拿掉顏色標記 [0 … ]，\r\n → \n
                    if (*q == '[' && q[1] >= '0' && q[1] <= '9') { q++; continue; }
                    if (*q == ']' || *q == '\r') continue;
                    rec.dlg_text[k++] = *q;
                }
                rec.dlg_text[k] = 0;
                rec.dlg_id = (int)rec_u32(b + 2);
                rec.dlg_voice = 1; rec.dlg_played = 0; rec.dlg_n6 = 0; rec.dlg_t = 1 << 30;  // 先當有語音：等語音狀態出來再決定（網頁版要下載）
            }
        }
        break;
    case 6:   // 0x4725c0 case 6：沒有語音才設顯示拍數 n×3，至少 0x28
        if (n >= 6) { rec.dlg_n6 = (int)rec_u32(b + 2) * 3; if (rec.dlg_n6 < 0x28) rec.dlg_n6 = 0x28; if (!rec.dlg_voice) rec.dlg_t = rec.dlg_n6; }
        break;
    case 7: if (n >= 6) rec.follow = (int)rec_u32(b + 2); break;                 // 0x473660 鏡頭跟隨
    case 8: if (n >= 10) { if (rec_u32(b + 2)) screen_effect((int)rec_u32(b + 2), 0); rec.ctl64 = (int)rec_u32(b + 6); } break;  // 0x44ca40(模式, 0)：1／2／3 淡入淡出、4 震；+0x64 只有 '#' 11／12 讀（資料沒有）
    case 10: if (n >= 6) scene_fog_set(sc, (int)rec_u32(b + 2)); break;  // 0x4c8630
    case 12: if (n >= 6) clock_hour_set((int)rec_u32(b + 2)); break;             // 0x469630 + 0x4695f0
    case 11: if (n >= 6) audio_sfx((int)rec_u32(b + 2)); break;                  // 0x4804a0 播音效（錄影的環境音、開門聲）
    case 9: if (n >= 14) viewport_pan(&viewport, (int)rec_u32(b + 2) - (viewport.w - 640) / 2, (int)rec_u32(b + 6) - (viewport.h - 480) / 2, (int)rec_u32(b + 10)); break;  // 0x461150
    default: break;
    }
}

static int rec_scene_moved;  // rec_end 載了別的場景，主迴圈要更新別名、音樂
// 結束 Rec（0x472380 模式 2 還原場景／介面）：錄影角色刪掉、真角色放回、鏡頭回主角，再跑劇情事件的第 18 格
static void rec_end(Scene *s, Actor *hero) {
    printf("過場結束（第 %u 拍）\n", rec.tick);
    char back[64], cur[64];  // 錄影載過別的場景：back = 開始時的房間（沒換過 = 空）、cur = 現在載著的場景
    snprintf(back, sizeof back, "%s", rec.back);
    snprintf(cur, sizeof cur, "%s", rec.cur);
    float bx = rec.bx, by = rec.by;
    int bdir = rec.bdir;
    for (int i = 0; i < rec.cnt; i++) { Actor *a = &actors[rec.idx[i]]; if (a->rec) rec_remove(a); }
    while (actor_count > rec.saved_count && !actors[actor_count - 1].role && !actors[actor_count - 1].mon && actors[actor_count - 1].sh == 0) actor_count--;
    if (!back[0])
        for (int i = 0; i < rec.hold_n; i++) { Actor *a = &actors[rec.hold[i]]; if (a->role) actor_place(a, (int)a->x, (int)a->y, 1); }
    rec_on = 0;
    scene_record_mode = 0;
    if (rec.light_set && !back[0]) light_table(rec.sv_day, rec.sv_night, rec.sv_type, wx.idx);  // 換過場景的由 enter_scene 重設
    if (rec.wx_set && !back[0]) weather_set(rec.sv_wx, rec.sv_wleft > 0 ? rec.sv_wleft : 1000);
    for (int i = 0; i < rec.gid_n; i++)
        for (int g = 0; g < ground_n; g++) if (ground[g].id == rec.gid[i]) { ground[g] = ground[--ground_n]; break; }
    free(rec.d); memset(&rec, 0, sizeof rec); rec.dlg_t = -1; audio_voice(-1);
    zoom_back();
#ifdef STORY
    story_begin();
    story_plot_end();   // 0x1004f2a0：第 18 格（獎勵、日誌）
    story_end();
    {   // 第 18 格可能把主角搬進別的房間（例：pct_kgszhu 把主角放進衙府），畫面跟著載主角所在房間的場景；回到開始的房間就站回原位置
        const char *to = story_room_alias(story_parent(story_player()));
        if (!to || !*to) to = cur;
        // 錄影在別的場景（片頭動畫場景 ptdhcj）演、第 18 格沒把主角搬走：真主角要回到開始的房間（原版錄影是場景拷貝，真主角一直留在原房間）。
        // 以前 enter_scene(錄影場景) 的 story_place 把主角永久搬進去，過場結束就留在 ptdhcj 的 (125,117)，不是起點 (29,107)（使用者 2026-11-20 回報）
        if (back[0] && strcasecmp(to, back) && !strcasecmp(to, cur)) { story_place(story_room(back)); to = back; }
        if (back[0] && !strcasecmp(to, back)) { hero->x = bx; hero->y = by; hero->dir = bdir; }
        if (strcasecmp(to, cur)) {
            if (!enter_scene(s, hero, to, !strcasecmp(to, back) ? -1 : 0)) fprintf(stderr, "過場結束載不進 %s\n", to);
            else rec_scene_moved = 1;
        } else if (back[0]) actor_place(hero, (int)hero->x, (int)hero->y, 1);
    }
#else
    if (back[0] && !enter_scene(s, hero, back, -1)) fprintf(stderr, "過場結束載不回 %s\n", back);
#endif
    viewport.pan = 0;  // 過場的鏡頭平移（case 9）或搬位前的舊目標若留著，會與跟隨各拉一邊，兩拍一輪閃爍（桓府）
    viewport_follow_set(&viewport, s, hero, 1);
}

// 0x472e70：有語音 = 語音播完就收（Mfx_LWave_IsPlaying）；沒有語音 = 倒數 n×3 拍；到 0 就停語音、關對話框
// shortcut: 網頁版語音檔下載完才知道有沒有；下載後沒檔就改用 @6 的拍數（原版一開始就知道）
static void rec_dialog_tick(void) {
    if (rec.dlg_t < 0) return;
    if (rec.dlg_voice) {
        int v = audio_voice_state();
        if (v == 2) rec.dlg_played = 1;
        if (v) return;
        if (rec.dlg_played) rec.dlg_t = 0;          // 播完
        else { rec.dlg_voice = 0; rec.dlg_t = rec.dlg_n6 ? rec.dlg_n6 : 1 << 30; }  // 沒語音：等 @6
    }
    if (rec.dlg_t > 0 && rec.dlg_t < 1 << 30) --rec.dlg_t;
    if (!rec.dlg_t) { audio_voice(-1); rec.dlg_t = -1; rec.dlg_id = 0; }
}

// 0x4722f0 + 0x538f50 + 0x5963e0：文字框置中在說話者頭頂（x = 角色 x，下緣 = 角色 y − 頭高 − 10），超出畫面就推回來（0x5962e0 後段的 OffsetRect 夾邊）
// 原版畫法 0x5950a0（~CToolTip 的 thunk 其實是 Draw）：框貼圖組 +0x260 = DAT_00662860 全程式沒寫入（恆 0）→ 沒有邊框；底色 DAT_0064b2c4／0x64b324 = 0x0421／0x0861 = RGB(8,8,8)；字色 DAT_0064b3b4／0x64b49c = 0x6737／0xce77 = RGB(205,205,189)。
// shortcut: 底色用 UI 管理物件 +0x2d8 判斷不透明（填滿）或半透明（圖案 0x93dcc8 等級 4），沒查到預設值；先用 alpha 200。原版 Rec 對話沒有說話者名與頭像（case 4 只存文字）
static void rec_dialog_draw(int vw, int vh) {
    if (!rec_on || rec.dlg_t < 0 || !rec.dlg_text[0]) return;
    // 直式手機：遊戲畫面被縮小顯示，文字照比例放大才看得清
    text_zoom = screen_zoom > 2 ? 2 : screen_zoom < 1 ? 1 : screen_zoom;
    int maxw = vw - 16, w = 0;
    for (const char *l = rec.dlg_text; *l;) {   // 最寬的一行
        const char *e = strchr(l, '\n'); char ln[512]; size_t k = e ? (size_t)(e - l) : strlen(l);
        snprintf(ln, sizeof ln, "%.*s", (int)k, l); int lw = text_width(ln); if (lw > w) w = lw;
        l += k + (e ? 1 : 0);
    }
    if (w > maxw) w = maxw;
    int h = draw_text(0, 0, rec.dlg_text, w, 0, 0), bw = w + 8, bh = h + 4;
    int x = rec.dlg_sx - bw / 2, y = rec.dlg_sy - bh;
    if (x + bw > vw) x = vw - bw;
    if (x < 0) x = 0;
    if (y + bh > vh) y = vh - bh;
    if (y < 0) y = 0;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 8, 8, 8, 200);
    SDL_Rect r = {x, y, bw, bh};
    SDL_RenderFillRect(ren, &r);
    draw_text(x + 4, y + 2, rec.dlg_text, w, 0xcdcdbd, 1);
    text_zoom = 1;
}
static void rec_dialog_note(const Actor *a, int sx, int sy) {
    if (rec_on && rec.dlg_t >= 0 && rec.dlg_id && a == rec_actor((unsigned)rec.dlg_id)) { rec.dlg_sx = sx; rec.dlg_sy = sy; }
}

// 每拍（0x472f50）：處理戳 == 目前拍的封包
static void rec_tick(Scene *s, Actor *hero) {
    if (!rec_on) return;
    // 一句對白（含語音）還在播／還沒下載好：錄影時間軸等它播完才往下（錄影的時間戳是錄影端的拍數，下一句常常比上一句語音還早 20~60 拍，
    // 不等的話上一句語音被切掉、字也只出現一兩個字就換句；2026-11-21 用 pct_cyzqian 的語音長度對時間戳量出來）。
    // 等太久（語音裝置沒在跑）最多等 n6×2 + 120 拍就放行，避免卡死
    int hold = rec.dlg_t >= 0 && rec.dlg_hold < (rec.dlg_n6 ? rec.dlg_n6 * 2 + 120 : 1500);
    if (hold) rec.dlg_hold++;
    while (!hold && !rec.skip && rec.p + 4 <= rec.n) {
        size_t len = rec_u32(rec.d + rec.p), q = rec.p + 4;
        if (len < 2 || q + len > rec.n) { rec.p = rec.n; break; }
        const uint8_t *b = rec.d + q;
        if (b[0] == '@' && b[1] == 0 && len >= 6 && rec_u32(b + 2) > rec.tick) break;  // 還沒到
        rec.p = q + len;
        if (b[0] == '#') rec_net(b, len, s);
        else if (b[0] == '@') rec_ctl(b, len, s);
    }
    rec_dialog_tick();
    if (rec.dlg_t < 0) rec.dlg_hold = 0;
    if (rec.follow) { Actor *f = rec_actor((unsigned)rec.follow); if (f) viewport_center_actor(&viewport, s, f, 1); }
    if (!hold) rec.tick++;
    if (rec.skip || (!hold && rec.p + 4 > rec.n)) rec_end(s, hero);
}

// 開始播放（訊息 0x52e）：找不到檔、場景不對就不播，直接收尾
static int rec_begin(const char *name, Scene *s, Actor *hero, const char *room) {
    char file[96];
    size_t n = 0;
    snprintf(file, sizeof file, "%s.rec", name);
    uint8_t *d = res_file("Rec/User", file, &n);
    if (!d) d = res_file("Rec", file, &n);
    if (!d || n < 0x84 || memcmp(d, "soloRecord 1.0", 14)) { fprintf(stderr, "過場 %s：找不到錄影檔\n", name); free(d); return 0; }
    memset(&rec, 0, sizeof rec);
    rec.d = d; rec.n = n; rec.p = 0x80; rec.dlg_t = -1; snprintf(rec.room, sizeof rec.room, "%s", room); snprintf(rec.cur, sizeof rec.cur, "%s", room);
    // 場景名：第一個 '#' 13 子類 2 之前的最後一條 '#' 10 字串
    for (size_t p = 0x80, last = 0; p + 6 <= n;) {
        size_t len = rec_u32(d + p);
        if (len < 2 || p + 4 + len > n) break;
        const uint8_t *b = d + p + 4;
        if (b[0] == '#' && b[1] == 10) last = p + 6;
        if (b[0] == '#' && b[1] == 13 && b[2] == 2 && last) {
            char sc[64]; snprintf(sc, sizeof sc, "%.63s", (const char *)d + last);
            char *sp = strchr(sc, ' '); if (sp) *sp = 0;
            if (strcasecmp(sc, room) && !rec_scene_swap(s, hero, sc)) { fprintf(stderr, "過場 %s 的場景是 %s，載不進去（目前是 %s），不播\n", name, sc, room); free(d); memset(&rec, 0, sizeof rec); return 0; }
            break;
        }
        p += 4 + len;
    }
    rec.saved_count = actor_count;
    for (int i = 0; i < actor_count; i++)
        if (actors[i].role) { actor_unoccupy(&actors[i]); rec.hold[rec.hold_n++] = i; }
    hero->foe = NULL; hero->want = NULL; hero->moving = 0;
    viewport_follow_set(&viewport, s, hero, 0);
    rec_on = 1;
    scene_record_mode = 2;  // 0x472e60：Rec 模式 2 = 回放
    printf("過場 %s 開始\n", name);
    return 1;
}


#include "touch_ui.c"

// 視窗（網頁 = 瀏覽器可視區）實際大小，排版用
#ifdef __EMSCRIPTEN__
EM_JS(int, js_inner_w, (), { return window.qinW || window.innerWidth; });  // qinW／qinH：shell.html 扣掉瀏海／橫條的安全區
EM_JS(int, js_inner_h, (), { return window.qinH || window.innerHeight; });
#endif
static void screen_size(SDL_Window *win, int *w, int *h) {
#ifdef __EMSCRIPTEN__
    (void)win;
    *w = js_inner_w(); *h = js_inner_h();
#else
    SDL_GetWindowSize(win, w, h);
#endif
}

static const char *bounce_alias;
static int at_mon;
// 直式手機：視窗放大用（touch_layout 的 ws）。側邊視窗（背包、裝備）原本就放大；系統選單、存讀檔、設定、說明、詢問框、地圖、難度這些整個
// 居中小視窗也一樣放大，不然縮在上方 290 點高的遊戲畫面裡字很小、點不準，點歪還會變成點地面走路（使用者 2026-11-20 回報）
static void modal_begin(void) {
    if (!(tch.L.ui_dst.w > 0 && modal_tex && modal_rect(&(SDL_Rect){0}))) return;
    SDL_SetRenderTarget(ren, modal_tex);
    if (!modal_cleared) { SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE); SDL_SetRenderDrawColor(ren, 0, 0, 0, 0); SDL_RenderClear(ren); modal_cleared = 1; }
    modal_on = 2;  // 2 = 目前正畫在 modal_tex（modal_end 要切回去）；最後貼圖用 modal_cleared
}
static void modal_end(void) {
    if (modal_on == 2) { SDL_SetRenderTarget(ren, fade_canvas); modal_on = 1; }
}
static int modal_rect(SDL_Rect *r) {
    int ox, oy, w = 0, h = 0;
#ifdef STORY
    if (dialog_open) { SDL_Rect b = dialog_box(); *r = b; return 1; }
#endif
    if (sav_mode) { w = sav_d.w; h = sav_d.h; ox = (vw - w) / 2; oy = (vh - h) / 2; }
    else if (opt_open || help_open) { UiDialog *d = help_open ? &help_d : &opt_d; w = d->w; h = d->h; opt_ui_pos(d, &ox, &oy); }
    else if (sys_open) { w = sys_d.w; h = sys_d.h; ox = (vw - w) / 2; oy = (vh - h) / 2; }
    else if (ask_open) { w = ask_d.w; h = ask_d.h; ask_pos(&ox, &oy); }
    else if (quit_open) { w = quit_d.w; h = quit_d.h; ox = (vw - w) / 2; oy = (vh - h) / 2; }
    else if (map_open) { w = map_d.w; h = map_d.h; ox = (vw - w) / 2; oy = (vh - h) / 2 - 0x32; }
    else if (diff_open) { const Sprite *bg = ui_image(196, 0); w = bg ? bg->w : 0; h = bg ? bg->h : 0; ox = (vw - w) / 2; oy = (vh - h) / 2; }  // 選難度（新遊戲第一個畫面）：小，直式要放大才點得到
    else return 0;
    if (w <= 0 || h <= 0) return 0;
    {  // 視窗實際畫出來的範圍可能比 UiDialog 的 w／h 大（地圖視窗右邊的按鈕欄在 w 之外）：放大區要包含全部，不然右邊被切掉
        const UiDialog *d = map_open ? &map_d : sys_open ? &sys_d : (opt_open || help_open) ? (help_open ? &help_d : &opt_d) : NULL;
        for (int i = 0; d && i < d->count; i++) {
            const UiCtrl *c = &d->c[i];
            const Sprite *sp = c->img ? ui_image(c->img, 0) : NULL;
            if (sp && c->x + sp->w > w && c->x >= 0 && c->x < w + 200) w = c->x + sp->w;
            if (sp && c->y + sp->h > h && c->y >= 0 && c->y < h + 200) h = c->y + sp->h;
        }
    }
    *r = (SDL_Rect){ox, oy, w, h};
    return 1;
}
int main(int argc, char **argv) {
    const char *game = "game", *scn = "xianyang", *shot = NULL, *open_win = NULL;
    int press_x = -1, press_y = -1;
    int pos = 0, warp = -1, at_x = -1, at_y = -1, level = 1, gives = 0, money = -1;
    const char *give[8], *learn[8];
    int give_eq[8], drop_t = -1, drop_lv = 1;
    int learns = 0, win_w = 0, win_h = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pause-traps")) trap_auto_pause = 1;
        if (!strcmp(argv[i], "--pause-deaths")) death_auto_pause = 1;
        else if (!strcmp(argv[i], "--openall") && i + 1 < argc) openall_alias = argv[++i];  // 文字指令 openall（0x10019d70）
        if (!strcmp(argv[i], "--simple-deaths")) death_keep_mode = 0;
        else if (!strcmp(argv[i], "--debug-bounce") && i + 1 < argc) bounce_alias = argv[++i];  // 測試用：200 拍後進這個場景、420 拍後回來，印耗時
        else if (!strcmp(argv[i], "--autotalk")) auto_talk = 1;
        else if (!strcmp(argv[i], "--debug-cells")) debug_cells = 1;
        else if (!strcmp(argv[i], "--debug-trace")) debug_cells = 2, audio_trace = 1;  // 疊圖並每 2 拍印主角座標與鏡頭
        else if (!strcmp(argv[i], "--640")) { vw = 640; vh = 480; }
        else if (!strcmp(argv[i], "--touch")) tch.forced = 1;  // 測試用：永遠顯示觸控介面
        else if (!strcmp(argv[i], "--win") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &win_w, &win_h);  // 測試用：視窗大小，配 --touch --shot 看橫直式版面
        else if (!strcmp(argv[i], "--open") && i + 1 < argc) open_win = argv[++i];  // 測試用：開側邊視窗 bag／skill／role／equip／both，配 --touch --shot 看畫面
        else if (!strcmp(argv[i], "--press") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &press_x, &press_y);  // 測試用：假裝有一根手指按在螢幕座標 X,Y 的介面上（看放大鏡與亮框）
        else if (!strcmp(argv[i], "--movie") && i + 1 < argc) movie_play(argv[++i]);  // 測試用：只播影片（End 等）
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
#ifdef STORY
        else if (!strcmp(argv[i], "--rec") && i + 1 < argc) snprintf(rec_pending, sizeof rec_pending, "%s", argv[++i]);  // 測試用：進場景就播這個過場
#endif
        else if (!strcmp(argv[i], "--warp") && i + 1 < argc) warp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &at_x, &at_y);
        else if (!strcmp(argv[i], "--load") && i + 1 < argc) load_arg = atoi(argv[++i]);  // 測試用：開場後讀這一格存檔
        else if (!strcmp(argv[i], "--no-interp")) no_interp = 1;  // 測試用：關閉畫面補間（比較順暢度）
        else if (!strcmp(argv[i], "--perf")) show_perf = 1;  // 測試用：畫面左上顯示 FPS、最長一幀毫秒、還在下載的圖片數（網頁 ?perf=1）
        else if (!strcmp(argv[i], "--show-pos")) show_pos = 1;  // 測試用：畫面左上顯示主角格座標（網頁 ?pos=1），請使用者回報位置用
        else if (!strcmp(argv[i], "--at-mon")) at_mon = 1;  // 測試用：開場後把主角放到離最近怪物兩格的地方（怪物出生點是隨機的）
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--money") && i + 1 < argc) money = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--weather") && i + 1 < argc) dbg_clk.weather = atoi(argv[++i]);  // 0~10 天氣、11~16 色調（0x4697b0 的 idx）
        else if (!strcmp(argv[i], "--cmd") && i + 1 < argc) { if (dbg_cmd_n < 16) snprintf(dbg_cmds[dbg_cmd_n++], sizeof dbg_cmds[0], "%s", argv[i + 1]); i++; }  // 除錯指令 SetFog／GetSpeed
        else if (!strcmp(argv[i], "--hour") && i + 1 < argc) dbg_clk.hour = atoi(argv[++i]);        // 0x44e880
        else if (!strcmp(argv[i], "--clock") && i + 1 < argc)                                         // 0x44e890：年,月,日,時,拍
            dbg_clk.use_clock = sscanf(argv[++i], "%d,%d,%d,%d,%d", &dbg_clk.clock[0], &dbg_clk.clock[1], &dbg_clk.clock[2], &dbg_clk.clock[3], &dbg_clk.clock[4]) == 5;
        else if (!strcmp(argv[i], "--learn") && i + 1 < argc && learns < 8) learn[learns++] = argv[++i];  // 測試用：模擬文字指令 skl_learn「索引名,分類」
        else if (!strcmp(argv[i], "--item") && i + 1 < argc && gives < 8) give[gives++] = argv[++i];
        else if (!strcmp(argv[i], "--drop") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &drop_t, &drop_lv);  // 測試用：開場在主角右前方 2 格放一件地上的道具（類型,等級），試撿取
        else if (!strcmp(argv[i], "--prof") && i + 1 < argc) hero_prof = atoi(argv[++i]) < 1 || atoi(argv[i]) > 5 ? 1 : atoi(argv[i]);
        else if (pos++ == 0) game = argv[i];
        else scn = argv[i];
    }
    res_init(game);
    save_mount();
#ifdef STORY
    story_register_all();
    if (pos < 2 && story_start_room()) { scn = story_start_room(); new_show(); }  // 沒指定場景 = 新遊戲，從原版的開場房間開始，先創角（JM09）再選難度
#endif

    if (pos < 2 && !shot) { movie_play("logo1"); if (intro_wanted()) movie_play("Start"); }  // 0x449f50 狀態 8~0xb
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
#ifdef STORY
    story_item_make = story_item_new;
    story_on_exp = story_exp;
    story_on_log = story_log;
    story_on_journal = journal_add;
    story_on_person = story_person;
    story_on_skill = story_skill;
    story_on_msg = story_msg;
    story_on_text = story_text;
    story_exp_for = story_exp_need;
    story_end();  // 開場裝備照劇情的主角（扶苏武器、布衣）
    hero_look();
#else
    hero_equip();
#endif
    for (int i = 0; i < gives; i++) {  // 測試用：--item 類型,等級 放一件到背包
        int t = 0, lv = 1, eq = 0, b = bag_free();
        sscanf(give[i], "%d,%d,%d", &t, &lv, &eq);
        if (b >= 0 && item_new(&inv[b], t, lv, -1, NULL)) { inv[b].pos = b; give_eq[i] = eq ? b : -1; }  // 第三欄 1 = 穿上（弓／箭袋測試用，開場時主角已建好才呼叫 item_equip）
        else give_eq[i] = -1;
    }
    if (money >= 0) hero_money = money;  // 測試用 --money
    Scene scene = {0};
    Actor *hero = &actors[0];
    hero->dir = 4;
    protagonist_new(hero);
    if (!enter_scene(&scene, hero, alias, -1)) return 1;
    if (!hero->role) { fprintf(stderr, "載入主角失敗\n"); return 1; }
    hero_stats(hero, level < 1 ? 1 : level > 99 ? 99 : level);
    for (int i = 0; i < gives; i++) if (give_eq[i] >= 0) item_equip(hero, give_eq[i]);
    hero_extra_exp = 10 * (hero->level - 1), hero_extra_skill = hero->level - 1;  // 測試用 --level：當成從 1 級升上來，點數照原版給
    for (int i = 0; i < learns; i++) {  // 測試用 --learn 索引名,分類：先放進 SKL_ToLearn 再學（skl_learn）
        char key[64] = "";
        int cls = 0;
        sscanf(learn[i], "%63[^,],%d", key, &cls);
        snprintf(hero_to_learn + strlen(hero_to_learn), sizeof hero_to_learn - strlen(hero_to_learn), "%s %d ", key, cls);
        if (hero_learn_skill(key)) hero_skill_apply(hero);
    }
    int room = -1;
    int monsters_due = 1;      // 進場景後等圖片都下載好，再讀房間表放怪物（網頁版開場不用等 4MB 的 DLL）
    if (at_x >= 0) actor_restore_position(hero, at_x + 0.5f, at_y + 0.5f);
    if (drop_t >= 0) { Item di; if (item_new(&di, drop_t, drop_lv, -1, NULL)) ground_drop(&di, hero->x + 2, hero->y); }
    if (debug_cells) for (int i = 0; i < scene.trigger_count; i++) { char ev[256]; gbk_utf8(scene.triggers[i].event, ev, sizeof ev); printf("trigger %d %s at %d,%d\n", i, ev, scene.triggers[i].x, scene.triggers[i].y); }  // 事件名是 GBK，轉 UTF-8 才看得懂（走查要看 s_pctstart 的劇情事件路徑）
    if (warp >= 0 && warp < scene.trigger_count) {
        int wt_x = 0, wt_y = 0;
        {   // 站到觸發區的感應格裡（可走、離擺放格最近的那格）；找不到就站擺放格
            const Trigger *wt = &scene.triggers[warp];
            wt_x = wt->x; wt_y = wt->y;
            int bx = wt->x, by = wt->y, best = 1 << 30;
            for (int y = wt->y - 20; y <= wt->y + 20; y++)
                for (int x = wt->x - 20; x <= wt->x + 20; x++) {
                    int d = (x - wt->x) * (x - wt->x) + (y - wt->y) * (y - wt->y);
                    if (d < best && scene_walkable(&scene, x, y) && trigger_hit(wt, x, y)) { best = d; bx = x; by = y; }
                }
            actor_restore_position(hero, bx + 0.5f, by + 0.5f);
        }
        for (int i = 0; i < scene.object_count; i++) {  // 站定後等於點了那個出口：下命令 0x10（門點 = 站的那格），到位就送 go
            const Object *wo = &scene.objects[i];
            if (wo->tri && wo->x == wt_x && wo->y == wt_y && wo->model == scene.triggers[warp].model && door_is_go(wo)) {
                actor_command(hero, 0x10, (int)hero->x, (int)hero->y, i, 1, 1);
                break;
            }
        }
        printf("站到觸發區 %d：%s\n", warp, scene.triggers[warp].event);
    }

    if (at_x >= 0 || warp >= 0) {
        viewport.pan = 0;
        viewport_home(&viewport, &scene, hero, 0);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");  // 觸控只送手指事件，多點才分得清（遊戲畫面上的手指再改寫成滑鼠事件）
    SDL_Window *win = SDL_CreateWindow("秦殤", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w > 0 ? win_w : vw, win_h > 0 ? win_h : vh,
#ifdef __EMSCRIPTEN__
                                       SDL_WINDOW_RESIZABLE);  // 網頁不開高像素密度：直式畫布 800x1731 的 3 倍會超過手機 GPU 上限，退成軟體繪製而卡住
#else
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
#endif
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);  // 沒有 GPU 驅動時改用軟體繪製
    if (!win || !ren) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    // 邏輯尺寸 = 遊戲畫面加上留空（touch_layout，每幀依視窗長寬比更新）；遊戲內容仍是固定 vw x vh，貼在 tch.L.game
    SDL_RenderSetLogicalSize(ren, vw, vh);
#ifndef __EMSCRIPTEN__
    tch.kbm = !tch.forced;  // 電腦版預設是鍵盤滑鼠，第一個手指事件才顯示觸控介面；網頁版預設顯示，第一次按鍵／滑鼠才收起
#endif
    // 先以 1:1 畫在畫布上再整張放大；直接放大每塊磁磚會在接縫透出細線
    SDL_Texture *canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);
    fade_canvas = canvas;
    win_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);
    modal_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);
    if (modal_tex) SDL_SetTextureBlendMode(modal_tex, SDL_BLENDMODE_BLEND);
    if (open_win) {  // 測試用 --open：開著側邊視窗再截圖
        if (!strcmp(open_win, "bag")) { win_toggle(&win_right, JM54); }
        else if (!strcmp(open_win, "skill")) { skill_mode = 1; win_toggle(&win_right, JM53); }
        else if (!strcmp(open_win, "role")) win_toggle(&win_left, JM52);
        else if (!strcmp(open_win, "equip")) win_toggle(&win_left, JM55);
        else if (!strcmp(open_win, "both")) { win_toggle(&win_left, JM55); win_toggle(&win_right, JM54); }
    }
    SDL_Texture *zoom_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);  // 縮放時的暫存
    // 音量與開關照原版 ini/Config.ini（[音频] 音效、音乐、音效音量、音乐音量）
    size_t cfg_size;
    uint8_t *cfg = res_file("ini", "Config.ini", &cfg_size);
    if (cfg) res_unxor(cfg, cfg_size);
    const char *cfg_text = cfg ? (const char *)cfg : "";
    audio_init(ini_int(cfg_text, "\xd2\xf4\xc0\xd6=", 1) ? ini_int(cfg_text, "\xd2\xf4\xc0\xd6\xd2\xf4\xc1\xbf=", 15) : 0,   // 音乐、音乐音量
               ini_int(cfg_text, "\xd2\xf4\xd0\xa7=", 1) ? ini_int(cfg_text, "\xd2\xf4\xd0\xa7\xd2\xf4\xc1\xbf=", 100) : 0);  // 音效、音效音量
    free(cfg);
    scene_music_enter(alias);
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);

    Uint64 fps_t = SDL_GetPerformanceCounter();
    Uint32 logic_ms = SDL_GetTicks(), tick = 0;
    SDL_Point vp_prev = {0, 0};  // 畫面補間：鏡頭上一拍的位置
    int frames = 0, running = 1, pointer_down = 0, click_down = 0, px = 0, py = 0, ox = 0, oy = 0, mouse_seen = 0;
    SDL_ShowCursor(SDL_DISABLE);  // 0x433dc0：關掉系統游標，畫原版游標動畫 0x100011

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { touch_release_all(); pointer_down = click_down = 0; }  // 失焦收不到放開事件：全部放開，免得按鍵卡住
            if (rec_on && (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_FINGERDOWN)) { rec.skip = 1; continue; }  // 按任一鍵／點一下跳過
            if (rec_on && (e.type == SDL_KEYUP || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION || e.type == SDL_TEXTINPUT)) continue;
            // 觸控：手指事件依用途分流（搖桿、按鈕、遊戲畫面、雙指縮放）；遊戲畫面上的單指改寫成滑鼠事件走原本的點擊流程
            if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) {
                tch.last_touch = SDL_GetTicks();
                tch.kbm = 0;
                Sint64 fid = (Sint64)e.tfinger.fingerId;
                int fx = (int)(e.tfinger.x * tch.L.sw), fy = (int)(e.tfinger.y * tch.L.sh), btn = 0, kind;
                float dz = 0;
                if (e.type == SDL_FINGERDOWN) kind = touch_down(fid, fx, fy, &btn);
                else if (e.type == SDL_FINGERMOTION) kind = touch_move(fid, fx, fy, &dz);
                else kind = touch_up(fid, &btn);
                if (kind == TK_PINCH) {
                    if (e.type == SDL_FINGERDOWN) pointer_down = click_down = 0;  // 第二指落下：取消第一指的按住走路
                    if (dz != 0) zoom_add(dz);
                    continue;
                }
                if (kind == TK_BTN) {
                    if (e.type == SDL_FINGERDOWN) touch_button(hero, btn);
                    continue;
                }
                if (kind == TK_GAME && e.type == SDL_FINGERDOWN) {  // 按在遊戲介面（視窗、選單、HUD 按鈕）上：不立刻點，改成預覽放大鏡，放開才點
                    SDL_Rect hr;
                    int snx = fx - tch.L.game.x, sny = fy - tch.L.game.y;
                    if (hud_target(snx, sny, 0, &hr) || touch_snap(&snx, &sny, 0, 0)) { touch_set_kind(fid, TK_UI); kind = TK_UI; }  // 按在控制項上，或附近有控制項（吸附）= 介面點擊
                }
                if (kind == TK_UI || kind == TK_WIN) {
                    int dux, duy, dwin;
                    if (touch_ui_pt(&tch.L, fx, fy, &dux, &duy, &dwin)) {  // 道具拖放：按在道具格上拖到裝備欄／別格；沒拖動走原來的預覽點擊
                        if (e.type == SDL_FINGERDOWN) drag_press(dux, duy);
                        else if (e.type == SDL_FINGERMOTION) { drag_x = dux; drag_y = duy; }
                        else if (drag_release(dux, duy)) continue;
                    }
                    if (e.type == SDL_FINGERUP) touch_ui_release(fx, fy);  // 放開在控制項上才點；滑出介面外 = 取消
                    continue;
                }
                if (kind != TK_GAME) continue;
                int gx = fx - tch.L.game.x, gy = fy - tch.L.game.y;
                if (e.type == SDL_FINGERDOWN && tch.aim >= 0 && touch_active()) {  // 瞄準模式：這一點就是施放位置
                    tch.aim = -1;
                    touch_kill(fid);
                    touch_aim_cast(hero, gx, gy, ox, oy, &scene);
                    continue;
                }
                SDL_Event me;
                memset(&me, 0, sizeof me);
                if (e.type == SDL_FINGERMOTION) {
                    me.type = SDL_MOUSEMOTION; me.motion.which = SDL_TOUCH_MOUSEID; me.motion.x = gx; me.motion.y = gy;
                } else {
                    me.type = e.type == SDL_FINGERDOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                    me.button.which = SDL_TOUCH_MOUSEID; me.button.button = SDL_BUTTON_LEFT;
                    me.button.state = e.type == SDL_FINGERDOWN ? SDL_PRESSED : SDL_RELEASED; me.button.clicks = 1;
                    me.button.x = gx; me.button.y = gy;
                }
                e = me;
            } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL) {
                if (SDL_GetTicks() - tch.last_touch < 700 && tch.last_touch) continue;  // 瀏覽器在觸控後補送的滑鼠事件，丟掉
                if (e.type != SDL_MOUSEWHEEL) {  // 真的滑鼠：座標扣掉遊戲畫面的位置（螢幕邏輯座標 → 遊戲座標）
                    if (e.type == SDL_MOUSEMOTION) { e.motion.x -= tch.L.game.x; e.motion.y -= tch.L.game.y; }
                    else { e.button.x -= tch.L.game.x; e.button.y -= tch.L.game.y; }
                }
                if (e.type != SDL_MOUSEMOTION || e.motion.which != SDL_TOUCH_MOUSEID) tch.kbm = 1;
            } else if (e.type == SDL_KEYDOWN) tch.kbm = 1;
            if (e.type == SDL_MOUSEMOTION && drag_src >= 0) { drag_x = e.motion.x; drag_y = e.motion.y; }
            if (e.type == SDL_QUIT) running = 0;
            else if (journal_open && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_KEYDOWN)) { journal_open = 0; pointer_down = 0; continue; }  // 日誌開著：點任何地方或按鍵關閉
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_j && !e.key.repeat && !new_open && !sys_open && !mny.open && !diff_open && !dialog_open) journal_open = 1;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && e.button.y < 36 && e.button.x > vw / 4 && e.button.x < vw * 3 / 4 &&
                     !new_open && !sys_open && !mny.open && !diff_open && !dialog_open && !rec_on && journal_n) { journal_open = 1; continue; }  // 點畫面上方提示字開日誌（手機沒有 J 鍵）
            else if (mny.open && (e.type == SDL_TEXTINPUT || e.type == SDL_KEYDOWN)) {  // 丟錢／銀行輸入框：數字、退格、Enter 確定、Esc 取消
                if (e.type == SDL_TEXTINPUT) money_text(e.text.text);
                else if (e.key.keysym.sym == SDLK_BACKSPACE) money_back();
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) money_ok(mny.kind == 1);
                else if (e.key.keysym.sym == SDLK_ESCAPE) money_close();
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_b && !new_open && !sys_open && !mny.open) money_open(1);  // B：銀行視窗（自訂入口）
            else if (new_open && (e.type == SDL_TEXTINPUT || e.type == SDL_KEYDOWN)) {  // 創角名字輸入框：打字、退格；其他鍵不傳給遊戲
                if (e.type == SDL_TEXTINPUT) new_name_add(e.text.text);
                else if (e.key.keysym.sym == SDLK_BACKSPACE) new_name_back();
            }
            // shortcut: 暫停介面未完成，Esc暫代繼續；JM31接入後使用原版暫停按鈕。
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE && scene_paused) { do scene_pause_count(0); while (scene_paused); }  // 計數全部放掉
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) sys_toggle();  // 原版 Esc 開系統選單
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F9) {  // F9 快速存盘（UI_C 2620）
                if (hero->fight > 0) ui_say(1507);
                else { snap_due = 1; save_due = 1; }
            }
            // 縮放：原版 PageUp／PageDown、滑鼠滾輪。shortcut: 原版鍵盤是每拍查按鍵狀態，這裡用按鍵事件（含長按重複）
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_h)
                viewport_home(&viewport, &scene, hero, 1);
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_PAGEUP) zoom_add(0.04f);
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_PAGEDOWN) zoom_add(-0.04f);
            else if (e.type == SDL_MOUSEWHEEL) zoom_add(e.wheel.y < 1 ? -0.04f : 0.04f);
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f)
                SDL_SetWindowFullscreen(win, SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            // 滑鼠與觸控（SDL 預設會把觸控也送成滑鼠事件）：按住就一直往指的地方走
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && drag_src >= 0) {  // 道具拖放放開；沒拖動 = 補上延後的點擊
                if (!drag_release(e.button.x, e.button.y)) win_tap(e.button.x, e.button.y);
                continue;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                px = e.button.x;
                py = e.button.y;
                if (!win_off && !dialog_open && !sys_open && !diff_open && drag_press(px, py)) continue;  // 按在道具格上：點擊延後到放開（拖動 = 搬移）
#ifdef STORY
                if (dialog_open) { story_begin(); dialog_tap(py); story_end(); continue; }  // 對話中不走路
#endif
                if (hud_tap(px, py)) continue;
                int zx = px, zy = py;
                zoom_point(&zx, &zy);  // 介面不縮放，場景裡的點要換回縮放前
                // 手機沒有右鍵：點主角自己 = 右鍵放左手技能（左手技能多是對自己的增益）
                if (abs(zx - vw / 2) <= 36 && zy - (vh / 2 - 30) >= -70 && zy - (vh / 2 - 30) <= 40) { hero_cast(hero); continue; }
                // 原版按下只記位置（0x454eb0），放開才下指令（0x454f00）。shortcut: 按住空地會一直往那邊走（手機要用；原版按住拖拉是選人框）
                int cx, cy;
                click_down = 1;
                if (mouse_seen) input_box_down(px, py);  // 0x454eb0
                pointer_down = !input_char(zx, zy, ox, oy, &scene, &cx, &cy) && ground_at(zx, zy, ox, oy, &scene) < 0;
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && click_down) {
                click_down = pointer_down = 0;
                if (input_box_up()) continue;  // 0x454f00：框選，不下指令
                if (SDL_GetModState() & KMOD_ALT) continue;  // 0x454f00：Alt 按著不下指令
                int zx = e.button.x, zy = e.button.y, cx, cy;
                zoom_point(&zx, &zy);
                Actor *m = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
                int g = ground_at(zx, zy, ox, oy, &scene);
                if (debug_cells) printf("mouse up %d,%d cell %d,%d actor=%p ground=%d sel_n=%d\n", zx, zy, cx, cy, (void *)m, g, sel_n);
                // 0x454f00 Ctrl 分支：Ctrl 按著 = 對點的地方出手（命令 8，技能 -1 = 右手技能）；點到角色就對他出手（命令 9）
                if ((SDL_GetModState() & KMOD_CTRL) && click_cmd(hero, m, g) != CMD_NONE && (input_sel_has(hero) || sel_n == 0)) {
                    if (m && m != hero) actor_command(hero, 9, -1, m->sh ? m->sh : (int)m->object_id, 0, 1, 1);
                    else actor_command(hero, 8, -1, cx, cy, 1, 1);
                    hero->foe = NULL; hero->want = NULL; walk_cx = -1;
                    continue;
                }
                // 0x454f00 LAB_004551b0：沒點到角色、道具時看游標下的建築
                if (!m && g < 0 && click_cmd(hero, m, g) != CMD_NONE && (input_sel_has(hero) || sel_n == 0)) {
                    Object *b = hover_building(cx, cy);
                    if (debug_cells) printf("click cell %d,%d hover=%s\n", cx, cy, b ? b->name : "(none)");
                    if (b && click_building(hero, b, cx, cy)) continue;
                }
                int cmd = click_cmd(hero, m, g);
                input_team_cmd(cmd, m, cx, cy);
                if (!input_sel_has(hero) && sel_n > 0) continue;  // 主角不在選人名單（只點了隊友頭像）：只有名單裡的人動
                switch (cmd) {
                case CMD_ATTACK: hero_attack(hero, m); hero->want = NULL; walk_cx = -1; break;
                case CMD_PICK: actor_command(hero, 0x11, ground[g].id, 0, 0, 1, 1); hero->foe = NULL; hero->want = NULL; walk_cx = -1; break;
                case CMD_MOVE:
                    if (!actor_command(hero, 1, cx, cy, 0, 1, 1)) break;
                    mark_new(cx, cy); hero->foe = NULL; hero->want = NULL;
                    walk_cx = hero->dynamic_state == 11 ? hero->dynamic_destination_x : -1;
                    walk_cy = hero->dynamic_destination_y;
                    break;
                case CMD_TALK:  // 原版指令0xf：走近到3格內再說話，由actor_interact_tick接手
                    actor_command(hero, 0xf, m->sh ? m->sh : (int)m->object_id, 0, 0, 1, 1);
                    hero->foe = NULL; hero->want = NULL; walk_cx = -1;
                    break;
                }
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && !sys_open &&
#ifdef STORY
                     !dialog_open &&
#endif
                     skill_key_index(e.key.keysym.sym, (SDL_Keymod)e.key.keysym.mod) >= 0) {
                actor_skill_select_index(hero, skill_key_index(e.key.keysym.sym, (SDL_Keymod)e.key.keysym.mod));
                // 選取改變後重綁實際攻擊資料，不重建技能鏈或覆寫左右手ID。
                hero_selected_skill_apply(hero);
            }
            // 遊戲速度：數字鍵盤 +／= 加快、數字鍵盤 −／- 減慢（0x455d60）
            else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_KP_PLUS || e.key.keysym.sym == SDLK_EQUALS)) game_speed_key(1);
            else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_KP_MINUS || e.key.keysym.sym == SDLK_MINUS)) game_speed_key(0);
            // R：自己這邊全部切換跑步旗標 0x10000（0x459170 命令 3）。shortcut: 原版的切換按鈕位置沒找到，先用 R 鍵
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r && !e.key.repeat) input_side_command(run_on = !run_on);
            // 空白鍵／手把 A：打最近的怪物
            else if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) ||
                     (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A)) {
                touch_attack_nearest(hero);
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {  // 右鍵：對點到的怪物放左手技能
                int zx = e.button.x, zy = e.button.y, cx, cy;
                zoom_point(&zx, &zy);
                Actor *m = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
                SDL_Point wp = viewport_screen_world(&viewport, zx, zy);  // 0x4a11f0
                input_cast(wp.x, wp.y, m);  // 0x457be0
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) pointer_down = 0;
            else if (e.type == SDL_MOUSEMOTION) {
                px = e.motion.x; py = e.motion.y; mouse_seen = e.motion.which != SDL_TOUCH_MOUSEID;
                if (click_down && input_box_move(px, py)) pointer_down = 0;  // 拖出框：不再按住走路
            }
            else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
        }
        if (pad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
            running = 0;
        if (quit_due) {  // shortcut: 還沒有標題畫面，退出 = 網頁重新整理（新遊戲）、電腦版關掉
#ifdef __EMSCRIPTEN__
            page_reload();
#endif
            running = 0;
        }
        if (load_arg >= 0 && tick > 60) { load_due = load_arg; load_arg = -1; }
        if (load_due >= 0) {
            static SaveHead h;
            if (save_read(load_due, &h)) {
                float x = hero->x, y = hero->y;
                int dir = hero->dir, lv = hero->level, exp = hero->exp, hp = hero->hp, mp = hero->mp;
                js_status(-1);
                if (enter_scene(&scene, hero, h.alias, -1)) { snprintf(alias, sizeof alias, "%s", h.alias); explore_restore(&scene, alias); }
                ground_n = 0;  // 讀檔：地上的道具以存檔為準，不留讀檔前的
                if (ground_pending) { memcpy(gstore, pending_gstore, sizeof gstore); memcpy(ground, pending_ground, sizeof pending_ground[0] * pending_n); ground_n = pending_n; snprintf(ground_alias, sizeof ground_alias, "%s", pending_alias); ground_pending = 0; }
                actor_restore_position(hero, x, y); hero->dir = dir;
                viewport_home(&viewport, &scene, hero, 0);  // 讀檔後鏡頭回到主角（同場景讀檔時鏡頭原本留在舊位置，主角在畫面角落）
                hero->foe = NULL; hero->want = NULL; hero->cast = NULL; hero->casting = 0; hero->act = ACT_NONE; hero->object_dead = 0; dead_open = 0;
                hero->buff_n = hero->fight = hero->dead_ticks = hero->knock = hero->stun = hero->z = 0;
                hero_stats(hero, lv);
                hero->exp = exp; hero->hp = hp; hero->mp = mp;
                hero_skill_apply(hero);
                room = world_tried ? world_find(&world, alias) : -1;
                monsters_due = 1;
                pointer_down = 0;
                walk_cx = -1;
                scene_music_enter(alias);
                hud_say("\xe6\xb8\xb8\xe6\x88\x8f\xe8\xbf\x9b\xe5\xba\xa6\xe8\xaf\xbb\xe5\x8f\x96\xe6\x88\x90\xe5\x8a\x9f");  // 游戏进度读取成功（EasyRPG_C 7054，拿掉顏色標記）
            } else hud_say("\xe8\xaf\xbb\xe5\x8f\x96\xe5\xa4\xb1\xe8\xb4\xa5");  // shortcut: 自訂「读取失败」
            load_due = -1;
        }

        if (journal_pop && !rec_on && !dialog_open && !journal_open && !(dead_open || sys_open || map_open || new_open || ask_open || quit_open || sav_mode || diff_open || trade_paused())) {
            journal_open = 1; journal_pop = 0; pointer_down = 0;  // 新日誌：等過場／對話結束才自動打開
        }
        if (hero->selection_pending && !hero->act && !hero->casting) hero_selected_skill_apply(hero);
        Uint64 now = SDL_GetPerformanceCounter();
        {  // 觸控介面：對話、系統選單、創角、地圖、死亡、詢問框、離開確認、交易製作視窗開著就整組收起來（背包、裝備、技能這種側邊視窗開著保留，多一顆 ✕ 關閉）；視窗長寬比（轉向、改視窗大小）或側邊視窗變了就重排版
            int blocked = journal_open || dead_open || sys_open || map_open || new_open || pick_hand >= 0 || ask_open || quit_open || sav_mode || diff_open || trade_paused();
#ifdef STORY
            blocked |= dialog_open || make_on() || store_on();
#endif
            int side = (win_left.open || win_right.open) && !blocked;
            touch_win_track();
            int ww, wh;
            screen_size(win, &ww, &wh);
            screen_zoom = tch.L.sw > 0 && ww > 0 ? (float)tch.L.sw / ww : 1;
            SDL_Rect ws = {0};
            if (modal_rect(&ws)) { SDL_Rect c = {0, 0, vw, vh}; SDL_IntersectRect(&ws, &c, &ws); }
            else if (win_left.open || win_right.open) {  // 開著的側邊視窗（遊戲座標）的聯集
                for (int i = 0; i < 2; i++) {
                    Win *w = i ? &win_right : &win_left;
                    if (!w->open) continue;
                    SDL_Rect r = {win_x(w), win_y(w), w->d.w, w->d.h}, c = {0, 0, vw, vh};
                    SDL_IntersectRect(&r, &c, &r);
                    if (ws.w) SDL_UnionRect(&ws, &r, &ws); else ws = r;
                }
            }
            int touching = tch.forced || !tch.kbm || SDL_GetNumTouchDevices() > 0;  // 有觸控裝置就算（第一次點的就是放大的視窗時，kbm 還沒清掉會漏掉）  // 電腦用滑鼠時視窗不搬位置
            static int last_w, last_h, last_side, last_touching;
            static SDL_Rect last_ws;
            if (blocked != tch.blocked || side != last_side) { tch.blocked = blocked; touch_release_all(); }
            if (tch.L.sw == 0 || ww != last_w || wh != last_h || side != last_side || touching != last_touching || memcmp(&ws, &last_ws, sizeof ws)) {
                last_w = ww; last_h = wh; last_side = side; last_touching = touching; last_ws = ws;
                int old_sw = tch.L.sw, old_sh = tch.L.sh;
                touch_layout(&tch.L, ww, wh, vw, vh, tch.size, tch.swap, ws.w && touching ? &ws : NULL);
                ui_portrait = tch.L.portrait;
                if (old_sw != tch.L.sw || old_sh != tch.L.sh) {
                    touch_release_all();
                    SDL_RenderSetLogicalSize(ren, tch.L.sw, tch.L.sh);
#ifdef __EMSCRIPTEN__
                    SDL_SetWindowSize(win, tch.L.sw, tch.L.sh);  // 網頁：畫布緩衝的長寬比跟著瀏覽器可視區，CSS 再塞滿
#endif
                }
            }
            win_off = tch.L.ui_dst.w > 0;
            if (press_x >= 0 && !tch.f[0].used) tch.f[0] = (TFinger){.id = 99, .used = 1, .kind = touch_in(&tch.L.ui_dst, press_x, press_y) ? TK_WIN : TK_UI, .x = press_x, .y = press_y};
            tch.L.b[TB_CLOSE].r = side ? tch.L.b[TB_CLOSE].r : 0;  // 只有側邊視窗開著（沒被別的介面擋住）才有 ✕
        }

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
            int zx = px, zy = py;
            zoom_point(&zx, &zy);
            mx = zx - vw / 2;
            my = zy - (vh / 2 - 40);
            if (mx * mx + my * my < 64) mx = my = 0;
        }
        {  // 觸控搖桿：優先於按住走路（兩個手指可以同時，一個推搖桿、一個點畫面）
            float jx, jy;
            if (touch_joy(&jx, &jy)) { mx = jx; my = jy; }
            touch_frame(hero);
        }
        // 走近自動說話觸發時搖桿還按著：不鎖的話下一拍的移動會取消「走過去說話」命令，主角直接走開、對話永遠不開（2026-11-20 瀏覽器實測抓到）。
        // 鎖住到搖桿放開為止（auto_stick_lock 在自動說話那段設）
        if (auto_stick_lock) { if (mx * mx + my * my <= 0.01f) auto_stick_lock = 0; else mx = my = 0; }

        // ---- 固定節拍：跟原版一樣每 16ms 一拍，畫面多快都不影響遊戲速度 ----
        Uint32 now_ms = SDL_GetTicks();
        if (now_ms - logic_ms > 250) logic_ms = now_ms - TICK_MS;  // 卡頓或切到背景後不要一次補太多拍
        while (now_ms - logic_ms >= TICK_MS) {
            logic_ms += TICK_MS;
            for (int i = 0; i < actor_count; i++) { actors[i].pvx = actors[i].x; actors[i].pvy = actors[i].y; }  // 畫面補間：記下這一拍之前的位置
            vp_prev = viewport.pos;
            if (scene_paused || trade_paused()) continue;
            // 過場等圖片下載好才往下播（網頁圖片是邊畫邊下載；手機網路慢時過場在黑畫面上播完，玩家什麼都沒看到，2026-11-20 使用者回報）。
            // 最多等 30 秒，之後照播（有張圖一直下載失敗也不會卡死）。原版啟動時資源都在本機，沒有這問題
            static Uint32 rec_wait_from;
            if (rec_on && res_loading() > 0) {
                Uint32 nowm = SDL_GetTicks();
                if (!rec_wait_from) rec_wait_from = nowm;
                if (nowm - rec_wait_from < 30000) continue;
            } else rec_wait_from = 0;
            ++tick;
            actor_logic_tick = tick;
#ifdef STORY
            // 劇情事件的過場（新遊戲要等難度選完才開演；NPC 對話還開著也要等關掉：選項「好的，前面帶路」那一點會被當成跳過，
            // 過場一瞬間就收掉，桓峰／第二章開場實測）
            if (rec_pending[0] && !diff_open && !dialog_open) {  // 沒檔、場景載不進去就直接收尾
                if (!rec_begin(rec_pending, &scene, hero, story_room_alias(sroom))) { story_begin(); story_plot_end(); story_end(); }
                rec_pending[0] = 0;
            }
#endif
            rec_tick(&scene, hero);
#ifdef STORY
            if (rec_scene_moved && !rec_on) {  // 過場結束時畫面換了場景（rec_end）：別名、音樂跟著
                rec_scene_moved = 0;
                snprintf(alias, sizeof alias, "%s", story_room_alias(sroom));
                scene_music_enter(alias);
                monsters_due = 1;
            }
#endif
            if (tick * TICK_MS / 1000 != (tick - 1) * TICK_MS / 1000 && !rec_on) for (int i = 0; i < actor_count; i++)
                    if (!i || actors[i].mon || actors[i].team) {
                        actor_second(&actors[i]);
#ifdef STORY
                        if (actors[i].team && actors[i].sh) { story_set_int(actors[i].sh, "HP", actors[i].hp); story_set_int(actors[i].sh, "MP", actors[i].mp); }  // 存檔、劇情讀得到
#endif
                    }
            viewport_tick(&viewport, &scene, hero);
            caption_tick();
            audio_music_tick(0);  // CMusic 每拍（0x44e750 → 0x57a010）
            if (caption_music) {  // 片尾字幕放 credit，關窗回場景音樂（0x57a910／0x57ab50）
                if (caption_music == 1) audio_music_special("credit"); else audio_music_pop();
                caption_music = 0;
            }
            ground_tick();
            for (int i = 0; i < actor_count; i++) {
                if (rec_on && !actors[i].rec) continue;  // 過場：真角色凍結
                if (actors[i].role) actor_motion_tick(&actors[i]);
                actor_health_sync(&actors[i]);
                if (actors[i].role) actor_maintenance_tick(&actors[i]);
                if (actors[i].role && actors[i].act != ACT_DEAD) actor_skill_command_tick(&actors[i]);
                if (actors[i].fight > 0 && !actors[i].foe && !grp_npc(actors[i].grp) && !--actors[i].fight)
                    actor_status_notify(&actors[i], 0, 0, -1);
            }
#ifdef STORY
            if (talk_to) {  // 原版封包0x75的接收端：開對話。shortcut: 只有ONT擺的（有事件ID）的NPC能說，且只認主角當說話者
                Actor *n = talk_to, *from = talk_from;
                talk_to = talk_from = NULL;
                const char *event = actor_init_name(n);
                if (!n->mon && event && *event && from == scene_client_actor(&scene_client)) {
                    int npc = story_find(story_room(alias), event);
                    if (debug_cells == 2) printf("talk %s npc=%d\n", event, npc);
                    if (npc) { story_begin(); story_talk(npc); story_end(); dialog_open = 1; }
                    else printf("劇情裡找不到 NPC %s\n", event);
                }
            }
#endif
#ifdef STORY
            if (auto_talk && !rec_on && !dialog_open && !(dead_open || sys_open || map_open || new_open || pick_hand >= 0 || ask_open || quit_open || sav_mode || diff_open || trade_paused()) &&
                hero->act == 0 && !has_status(hero, 17) && !has_status(hero, 14)) {
                int hsx = ax64(hero) >> 6, hsy = ay64(hero) >> 6;
                for (int i = 1; i < actor_count; i++) {
                    Actor *n = &actors[i];
                    const char *ev = actor_init_name(n);
                    if (n->mon || !n->role || n->act == ACT_DEAD || !ev || !*ev || is_foe(n, hero)) continue;
                    int nx = ax64(n) >> 6, ny = ay64(n) >> 6, d = abs(hsx - nx) > abs(hsy - ny) ? abs(hsx - nx) : abs(hsy - ny);
                    if (!auto_talk_step(d, !hero->moving && mx * mx + my * my <= 0.01f, &n->auto_talked)) continue;
                    if (!line_clear(hsx, hsy, nx, ny, 3, 3) || !story_find(story_room(alias), ev)) { n->auto_talked = 0; continue; }
                    if (debug_cells == 2) printf("autotalk %s\n", ev);
                    if (hero->moving && !has_status(n, 0x67)) {  // 走動中觸發：命令 15 在移動中不會被消費（瀏覽器實測 autotalk 有印、對話沒開），這裡照 actor_interact_tick 的收尾直接開口
                        actor_direction_set(n, dir_of(ax64(hero), ay64(hero), ax64(n), ay64(n), 8)); actor_body_apply(n, -1);
                        actor_direction_set(hero, dir_of(ax64(n), ay64(n), ax64(hero), ay64(hero), 8)); actor_body_apply(hero, -1);
                        talk_from = hero; talk_to = n; hero->moving = 0;
                    } else
                    actor_command(hero, 0xf, n->sh ? n->sh : (int)n->object_id, 0, 0, 1, 1);
                    hero->foe = NULL; hero->want = NULL; walk_cx = -1;
                    auto_stick_lock = 1;
                    break;
                }
            }
#endif
            int held = !rec_on && hero->act != ACT_DEAD && react_tick(hero, &scene);
            if (rec_on) { }
            else if (hero->act == ACT_DEAD) {  // 死亡：播完倒下動作停一下（shortcut: 180 拍是自訂，原版開 JM513 的時機沒查到），開死亡界面等讀取或退出
                if (++hero->dead_ticks > 180 && !dead_open) dead_show();
                action_tick(hero, &scene, tick);
            } else if (hero->act) action_tick(hero, &scene, tick);
            else if (held) hero->moving = 0;
            else if (mx * mx + my * my > 0.01f) {
                actor_cancel_command(hero); actor_forget_command(hero);
                hero->foe = NULL; hero->want = NULL; walk_cx = -1;
                actor_tick(hero, &scene, mx, my, tick);
            }  // 自己走 = 放棄目標與記住的技能命令。
            else if (!actor_command_idle(hero)) { }  // 逐格路徑由actor_motion_tick推進
            else if (walk_cx >= 0 && !hero->foe) {
                if (hero->dynamic_state != 11) walk_cx = -1;
            }
            else hero_idle_tick(hero, &scene, tick);
            if (debug_cells == 2 && tick % 2 == 0)
                printf("trace t=%u hero=%.3f,%.3f cam=%d,%d moving=%d dir=%d act=%d frame=%d done=%d hp=%d/%d mp=%d lv=%d exp=%d arrows=%d money=%d foe=%d ground=%d hand=%d rs=%d ls=%d inv32=%d/%d rsk=%d/%d/%d use=%d\n", tick, hero->x, hero->y, viewport.pos.x, viewport.pos.y, hero->moving, hero->dir, hero->act, hero->act_frame, hero->act_done, hero->hp, hero->hp_max, hero->mp, hero->level, hero->exp, inv[33].name[0] ? inv[33].arrows : -1, hero_money, hero->foe ? hero->foe->hp : -1, ground_n, hero_hand, cur_skill[0], cur_skill[1], inv[32].name[0] ? inv[32].type : -1, inv[33].name[0] ? inv[33].type : -1, hero_skill_n > 0 ? hero_skill[0].wtype : -9, hero_skill_n > 0 ? hero_skill[0].level : -9, hero_skill_n > 0 ? skill_attack_id(&hero_skill[0]) : -9, hero_skill_n > 0 ? skill_usable(hero, &hero_skill[0]) : -9);
            if (debug_cells == 2 && tick % 120 == 0)  // 除錯：列出可對話的 NPC（事件ID、格子、畫面座標），自動操作腳本用來點它們
                for (int i = 1; i < actor_count; i++) {
                    const char *ev = actor_init_name(&actors[i]);
                    if (actors[i].mon || !ev || !*ev) continue;
                    SDL_Point sp = viewport_world_screen(&viewport, ax64(&actors[i]), ay64(&actors[i]));
                    printf("npc %s cell=%d,%d screen=%d,%d role=%d disp=%d flags=%x dead=%d vis=%d\n", ev, (int)actors[i].x, (int)actors[i].y, sp.x, sp.y, actors[i].role != NULL, actor_display(&actors[i]), actors[i].flags, actors[i].object_dead, actor_draw_visible(&actors[i]));
                }
            if (at_mon && tick > 60) {
                int best = -1;
                for (int i = 1; i < actor_count; i++)
                    if (actors[i].mon && actors[i].role && actors[i].mon->istrap != 1 && (best < 0 || cell_dist(hero, &actors[i]) < cell_dist(hero, &actors[best]))) best = i;
                if (best >= 0) {
                    actor_restore_position(hero, actors[best].x - 2.0f, actors[best].y);
                    viewport_home(&viewport, &scene, hero, 0);
                    printf("at-mon: 怪物 %d 在 %.1f,%.1f，主角站到 %.1f,%.1f\n", best, actors[best].x, actors[best].y, hero->x, hero->y);
                    if (debug_cells == 2) for (int i = 0; i < hero_skill_n; i++) printf("hskill %d key=%s wtype=%d hand=%d attack=%d lv=%d max=%d req=%d usable=%d\n", i, hero_skill[i].key, hero_skill[i].wtype, hero_skill[i].hand, skill_attack_id(&hero_skill[i]), hero_skill[i].level, hero_skill[i].max_level, hero_skill[i].req, skill_usable(hero, &hero_skill[i]));
                    at_mon = 0;
                }
            }
            if (debug_cells == 2 && tick % 60 == 0)  // 除錯：列出怪物（格子、血量、距離主角）
                for (int i = 1; i < actor_count; i++)
                    if (actors[i].mon && actors[i].role) printf("mon %d cell=%.1f,%.1f hp=%d/%d dead=%d dist=%.1f act=%d\n", i, actors[i].x, actors[i].y, actors[i].hp, actors[i].hp_max, actors[i].object_dead, cell_dist(hero, &actors[i]), actors[i].act);
            if (debug_cells == 2 && tick % 120 == 0)  // 除錯：地上的道具（畫面座標，自動操作腳本點它們）
                for (int i = 0; i < ground_n; i++) {
                    SDL_Point gp = viewport_world_screen(&viewport, (int)(ground[i].x * 64), (int)(ground[i].y * 64));
                    printf("ground %d %s cell=%.1f,%.1f screen=%d,%d\n", i, ground[i].it.name, (double)ground[i].x, (double)ground[i].y, gp.x, gp.y);
                }
            if (debug_cells == 2 && tick % 120 == 0)  // 除錯：列出可互動建築（名稱、種類、狀態、格子、畫面座標），自動操作腳本點它們
                for (int i = -1; i < scene.object_count; i++) {
                    if (i < 0) { SDL_Point hp = viewport_world_screen(&viewport, ax64(hero), ay64(hero)); printf("bld hero screen=%d,%d vw=%d vh=%d zoom=%.3f\n", hp.x, hp.y, vw, vh, (double)zoom); continue; }
                    const Object *o = &scene.objects[i];
                    if (!o->tri || !o->m) continue;
                    if (!strncmp(o->name, "s_pct", 5)) continue;
                    SDL_Point sp = viewport_world_screen(&viewport, o->x * 64 + 32, o->y * 64 + 32);
                    if (sp.x < -200 || sp.y < -200 || sp.x > vw + 200 || sp.y > vh + 200) continue;
                    { SDL_Point dp = viewport_world_screen(&viewport, o->door_x * 64 + 32, o->door_y * 64 + 32);
                    printf("bld %d %s type=%d state=%d walk=%d cell=%d,%d screen=%d,%d door=%d,%d doorscreen=%d,%d\n", i, o->name, o->m->type, o->state, scene_walkable(&scene, o->x, o->y), o->x, o->y, sp.x, sp.y, o->door_x, o->door_y, dp.x, dp.y); }
                    if (o->m->type >= 0x80 && o->m->type <= 0x100) { int n88 = 0, tot = 0, fx = -1, fy = -1; for (int cy2 = 0; cy2 < scene.h; cy2++) for (int cx2 = 0; cx2 < scene.w; cx2++) { const Cell *c2 = &scene.cells[cy2 * scene.w + cx2]; if (c2->building == i) { tot++; if ((c2->flags & 0x88) == 0x88) { if (!n88++) fx = cx2, fy = cy2; } } } printf("bldcells %d %s cells=%d flag88=%d first88=%d,%d\n", i, o->name, tot, n88, fx, fy); }
                }
            for (int i = 1; i < actor_count; i++) {
                Actor *a = &actors[i];
                if (rec_on) continue;  // 過場：錄影角色只照封包動（actor_motion_tick），真角色凍結
                if (!a->mon) { if (!react_tick(a, &scene)) npc_dispatch(a); }
                else if (!a->role) continue;
                else if (a->act == ACT_DEAD) { action_tick(a, &scene, tick); if (!dead_obj_tick(a)) a->role = NULL; }
                else if (react_tick(a, &scene) && !a->act) a->moving = 0;
                else if (a->act) action_tick(a, &scene, tick);
                else if (a->object_kind == 24) trap_tick(a);
                else npc_dispatch(a);
            }
            // 小動物（BeastieMdl，原版 CSurBeastie：每拍 0x49b160、逃跑 0x49b6e0、移動 0x49b2d0）。不是 CCharactor：不受自動鎖定、主角打不到。
            //  ①「對人反應 = 1」且不在逃（狀態 ≠ 2）：每 80 拍找 0x1c0 像素（7 格）內 NPC 組（10000~60000）的角色 → 逃（高速、往反方向）。
            //  ② 逃到目的地（0x49b2d0）：0x200 像素（8 格）內還有任何角色（不分組，含主角）→ 再逃一次；沒有 → 回到慢走。
            //  ③ 死亡：圖形序列數 > 0x18（有死亡序列：小鸡、1001 有，公鸡、母鸡 24 個沒有 = 不會死）、每 30 拍的餘數 ≠ 0（原版 +0x77 計數，這裡用全域拍數近似）、
            //     NPC 組角色靠到 0x1e（30 像素）內且該角色不是站著（動態狀態 ≠ 1）→ 死（狀態 3，播死亡聲音，500 拍後消失）。玩家組（主角、隊友、村民組 2）只有 ② 會讓牠逃，踩不死牠。
            // 逃跑目的地照 0x49b6e0（見下）；shortcut: 高速用旗標 0x10000（速度 ×3/2）代替 BeastieMdl 高速；死亡不播專用聲音
            for (int i = 1; i < actor_count; i++) {
                Actor *a = &actors[i];
                if (!a->role || !a->def || !a->def->beast || a->act == ACT_DEAD || rec_on) continue;
                Actor *npc_near = NULL, *any_near = NULL;
                long npc_d2 = 0, any_d2 = 0;
                for (int j = 1; j < actor_count; j++) {
                    Actor *t = &actors[j];
                    if (t == a || !alive(t) || (t->def && t->def->beast)) continue;
                    long dx = ax64(a) - ax64(t), dy = ay64(a) - ay64(t), d2 = dx * dx + dy * dy;
                    if (beast_trampled(a, t, tick)) { actor_kill(a, NULL); npc_near = NULL; any_near = NULL; break; }
                    if (grp_npc(t->grp) && (!npc_near || d2 < npc_d2)) { npc_near = t; npc_d2 = d2; }
                    if (!any_near || d2 < any_d2) { any_near = t; any_d2 = d2; }
                }
                if (a->act == ACT_DEAD || a->def->reaction != 1) continue;
                int fleeing = actor_flags(a, 0x10000);
                Actor *from = NULL;
                if (!fleeing && tick % 80 == 1 && npc_near && npc_d2 <= 448L * 448) from = npc_near;
                else if (fleeing && a->dynamic_state == 1) {
                    if (any_near && any_d2 <= 512L * 512) from = any_near;
                    else actor_set_flags(a, 0x10000, 0);  // 逃完了，回到慢走
                }
                if (!from) continue;
                // 0x49b6e0：logic 1 往威脅的反方向（WhatDir8 → GetFleePos，隱藏步長 5，air.c 由呼叫點機碼確認）跑 5 格；其他 logic 跑到離威脅最遠的路點，沒路線就保持原目標
                const NpcSpec *sp = a->spec;
                int tx = (int)a->x, ty = (int)a->y;
                if (sp && sp->logic != 1) {
                    int best = -1, far = 0;
                    for (int k = 0; k < sp->path_n; k++) {
                        int d = abs((int)from->y - sp->path[k][1]) + abs((int)from->x - sp->path[k][0]);
                        if (d > far) { far = d; best = k; }
                    }
                    actor_set_flags(a, 0x10000, 1);
                    if (sp->path_n) tx = sp->path[best < 0 ? 0 : best][0], ty = sp->path[best < 0 ? 0 : best][1];
                    else continue;
                } else {
                    move_dir(&tx, &ty, dir_of((int)from->x, (int)from->y, tx, ty, 8), 5);
                    actor_set_flags(a, 0x10000, 1);
                }
                if (tx < 0) tx = 0;
                if (ty < 0) ty = 0;
                if (tx >= scene.w) tx = scene.w - 1;
                if (ty >= scene.h) ty = scene.h - 1;
                actor_command(a, 1, tx, ty, 0, 1, 0);
            }
            for (int i = 0; i < actor_count; i++) {
                actor_near_reactions(&actors[i]);
                walk_fx(&actors[i], &scene, tick);
            }
            unit_scene = &scene;
            input_tick();
#ifdef STORY
            if (openall_alias) { chest_open_all(openall_alias); openall_alias = NULL; }
#endif
            units_tick(tick);
            scene_buildings_tick(&scene);
            light_tick(&scene);
            scene_explore_decay(&scene);  // 0x46f100 最後一步：格子探索衰減（0x4c7790）
            clock_tick();
            debug_clock_apply();
            debug_commands_apply();
#ifdef STORY
            flag_watch_tick(tick);
#endif
            weather_tick();
            ambient_tick(&scene);
#ifdef STORY
            if (!rec_on) trigger_sense_tick(&scene);
#endif
            air_tick(scene_client_actor(&scene_client));
            sur_tick();
            bird_tick(&scene, &viewport);
            zoom_tick();
            // 出口（原版：點門命令 0x10 走到門點 → 封包 0x142 "go <出口>"；走進感應區不會觸發 go，見 trigger_sense_tick）
            const char *go_name = door_go_due && !rec_on ? door_go : NULL;
            door_go_due = 0;
            if (door_cool > 0) door_cool--;
#ifdef STORY
            if (go_name) {
                char path[128];
                int entry = 0, to = 0;
                if (!story_exit(sroom, go_name, path, sizeof path, &entry)) continue;
                story_begin();
                story_random_exit(sroom, go_name, path, sizeof path, &entry);
                to = story_room_by_path(path);
                int ok = to && story_enter(to);
                story_end();
                js_status(-1);
                int was_big = scene_big(&scene);
                if (ok && enter_scene(&scene, hero, story_room_alias(to), entry)) {
                    monsters_due = 1;
                    snprintf(alias, sizeof alias, "%s", story_room_alias(to));
                    scene_music_enter(alias);
                    printf("進入 %s\n", alias);
                    snap_due = 1;  // 穿梭場景時自动存盘（UI_C 2503）：大場景換大場景才存（0x44ce00）
                    if (opt_v[O_AUTOSAVE] && autosave_due(was_big, &scene)) save_due = 0;
                }
            }
#else
            if (go_name) {
                if (!world_tried) room = load_world(&world, &world_tried, alias);
                if (room < 0) continue;
                const Room *r = &world.rooms[room];
                for (int i = 0; i < r->exit_count; i++) {
                    const Exit *x = &world.exits[r->first_exit + i];
                    if (strcmp(x->dir, go_name) || x->to < 0) continue;
                    js_status(-1);
                    int was_big = scene_big(&scene);
                    if (enter_scene(&scene, hero, world.rooms[x->to].alias, x->entry)) {
                        room = x->to;
                        monsters_due = 1;
                        snprintf(alias, sizeof alias, "%s", world.rooms[room].alias);
                        scene_music_enter(world.rooms[room].alias);
                        printf("進入 %s\n", world.rooms[room].alias);
                            snap_due = 1;  // 穿梭場景時自动存盘（UI_C 2503）：大場景換大場景才存（0x44ce00）
                        if (opt_v[O_AUTOSAVE] && autosave_due(was_big, &scene)) save_due = 0;
                    }
                    break;
                }
            }
#endif
        }

        if (bounce_alias && tick > 200) {  // --debug-bounce：進 bounce_alias 再回原場景，量換場景（載入／回到快取過的場景）的時間
            static int stage; static Uint32 t0; static char home[64];
            if ((stage == 0 || stage == 4) && tick > (stage ? 420u : 200u) && !res_loading() && !monsters_due) {  // 等資源都齊了才離開（否則量到的是沒載完的）
                if (!stage) snprintf(home, sizeof home, "%s", alias);
                t0 = SDL_GetTicks(); js_status(-1);
                if (enter_scene(&scene, hero, stage ? home : bounce_alias, -1)) { snprintf(alias, sizeof alias, "%s", stage ? home : bounce_alias); monsters_due = 1; }
                printf("bounce: 進 %s enter_scene %u ms，下載 %u 項 %zu KB\n", alias, SDL_GetTicks() - t0, res_got_n, res_got_bytes >> 10);
                stage++;
            } else if ((stage == 1 || stage == 5) && !res_loading() && !monsters_due) {
                printf("bounce: %s 資源齊了 %u ms（含 enter_scene），累計下載 %u 項 %zu KB\n", alias, SDL_GetTicks() - t0, res_got_n, res_got_bytes >> 10);
                stage++;
            } else if (stage == 2 || stage == 3 || stage == 6) stage++;
        }
#ifdef STORY
        if (actors_dirty && !monsters_due) actors_sync(&scene);
        if (monsters_due && !res_loading()) {
            monsters_due = 0;
#else
        if (monsters_due && !res_loading() && (world_tried || world_ready())) {
            monsters_due = 0;
            if (!world_tried) room = load_world(&world, &world_tried, alias);
#endif
            static int fight_loaded;
            if (!fight_loaded) {  // 戰鬥資料也等開場畫面好了才讀（網頁版不拖慢開場）
                fight_loaded = 1;
                hero_skill_apply(hero);
                size_t n;
                uint8_t *d = res_file("Data", "experience.txt", &n);
                if (d) {
                    res_unxor(d, n);
                    for (char *l = strtok((char *)d, "\r\n"); l && exp_levels < 100; l = strtok(NULL, "\r\n")) exp_table[exp_levels++] = atoi(l);
#ifdef STORY
                    story_exp_max = exp_levels;
#endif
                    free(d);
                }
            }
#ifdef STORY
            actors_sync(&scene);
            for (int k = 0; k < pending_mons_n; k++)  // 讀檔：怪物回到存檔時的位置與血量
                for (int i = 1; i < actor_count; i++) {
                    Actor *c = &actors[i];
                    if (c->sh != pending_mons[k].sh || !c->role || c->team) continue;
                    if (actor_restore_position(c, pending_mons[k].x, pending_mons[k].y)) { c->dir = pending_mons[k].dir; }
                    c->hp = pending_mons[k].hp; c->mp = pending_mons[k].mp;
                    if (debug_cells) printf("讀檔還原怪物 sh=%d hp=%d at %.1f,%.1f\n", c->sh, c->hp, c->x, c->y);
                }
            pending_mons_n = 0;
#else
            if (mondefs_load()) spawn_monsters(&scene, &world, room);
#endif
        }

        if (viewport.w != vw || viewport.h != vh) viewport_resize(&viewport, &scene, vw, vh);
        ox = viewport.pos.x;
        oy = viewport.pos.y;

        // ---- 畫面 ----
        SDL_SetRenderTarget(ren, canvas);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        {
            int hx = px, hy = py, cx, cy; zoom_point(&hx, &hy);
            Actor *c = mouse_seen ? input_char(hx, hy, ox, oy, &scene, &cx, &cy) : NULL;
            actor_hover_set(c, mouse_seen);
            viewport_building_hover(&viewport, &scene, hx, hy, mouse_seen && !c && !win_left.open && !win_right.open, tick);
        }
        {   // 畫面補間：邏輯固定 62.5 拍／秒，手機畫面 48~60 FPS 時每個畫面吃到 1 或 2 拍（有時 0 拍），走路看起來一頓一頓；
            // 畫的時候把角色與鏡頭位置放在「上一拍 → 這一拍」之間（依這個畫面離拍界線多遠），畫完還原（邏輯、點擊判斷用真實位置）。
            // 瞬移（超過 2 格）不補。延遲最多 1 拍（16ms）
            float al = no_interp ? 1.0f : (float)(now_ms - logic_ms) / TICK_MS;
            if (al < 0) al = 0;
            if (al > 1) al = 1;
            static float sx_[MAX_ACTORS], sy_[MAX_ACTORS];
            for (int i = 0; i < actor_count; i++) {
                Actor *a = &actors[i];
                sx_[i] = a->x; sy_[i] = a->y;
                if (fabsf(a->x - a->pvx) < 2 && fabsf(a->y - a->pvy) < 2) { a->x = a->pvx + (a->x - a->pvx) * al; a->y = a->pvy + (a->y - a->pvy) * al; }
            }
            SDL_Point vp_cur = viewport.pos;
            if (abs(vp_cur.x - vp_prev.x) < 200 && abs(vp_cur.y - vp_prev.y) < 200) {
                viewport.pos.x = vp_prev.x + (int)((vp_cur.x - vp_prev.x) * al);
                viewport.pos.y = vp_prev.y + (int)((vp_cur.y - vp_prev.y) * al);
            }
            viewport_draw(&viewport, &scene, tick);
            viewport.pos = vp_cur;
            for (int i = 0; i < actor_count; i++) { actors[i].x = sx_[i]; actors[i].y = sy_[i]; }
        }
        rec_dialog_draw(vw, vh);
        journal_draw(vw, vh);
        if (show_perf) {
            static Uint32 last, worst, frames, shown_fps, shown_worst;
            Uint32 nowp = SDL_GetTicks();
            if (last) { Uint32 dt = nowp - last; if (dt > worst) worst = dt; }
            last = nowp; frames++;
            static Uint32 t0; if (!t0) t0 = nowp;
            if (nowp - t0 >= 1000) { shown_fps = frames * 1000 / (nowp - t0); shown_worst = worst; frames = worst = 0; t0 = nowp; }
            static int lpx, lpy; static double sum_d, sum_d2; static int n_d, shown_j;
            int ddx = viewport.pos.x - lpx, ddy = viewport.pos.y - lpy; lpx = viewport.pos.x; lpy = viewport.pos.y;
            double dd = sqrt((double)(ddx * ddx + ddy * ddy));
            if (dd > 0.5 && dd < 60) { sum_d += dd; sum_d2 += dd * dd; n_d++; }
            if (nowp - t0 < 20 && n_d > 5) { double m = sum_d / n_d, var = sum_d2 / n_d - m * m; shown_j = (int)(sqrt(var > 0 ? var : 0) * 100 / (m > 0 ? m : 1)); sum_d = sum_d2 = 0; n_d = 0; }
            char pf[96]; static int miss_total; miss_total += perf_missing; perf_missing = 0;
            snprintf(pf, sizeof pf, "FPS %u 最慢%ums 待載圖%d 抖%d%% 缺%d", shown_fps, shown_worst, res_loading(), shown_j, miss_total);
            draw_text(6, 24, pf, 300, 0x80FF80, 1);
        }
        if (show_pos && !rec_on) { char pt[64]; snprintf(pt, sizeof pt, "%d,%d", (int)hero->x, (int)hero->y); draw_text(6, 6, pt, 200, 0xFFFF00, 1); }
        input_box_draw();  // 0x46e8b0
        fade_draw(vw, vh);  // 0x44bf00：場景畫完再淡入淡出（介面不受影響）
        if (snap_due) { snap_take(canvas); snap_due = 0; }
        if (save_due >= 0) {
            snprintf(save_alias, sizeof save_alias, "%s", alias);
#ifdef STORY
            snprintf(save_name, sizeof save_name, "%s", story_name(sroom));
#else
            snprintf(save_name, sizeof save_name, "%s", room >= 0 ? world.rooms[room].name : "");
#endif
            int ok = save_write(save_due);
            if (save_due) hud_say(ok ? "\xe6\xb8\xb8\xe6\x88\x8f\xe8\xbf\x9b\xe5\xba\xa6\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98" : "\xe4\xbf\x9d\xe5\xad\x98\xe5\xa4\xb1\xe8\xb4\xa5");  // shortcut: 原版存好的提示沒找到，自訂「游戏进度已保存」「保存失败」
            save_due = -1;
        }
        if (zoom != 0) {  // 0x449f50：場景畫完，把中間那塊放大到整個畫面；介面在這之後畫，不放大
            SDL_Rect in = { (int)(vw * zoom), (int)(vh * zoom), vw - 2 * (int)(vw * zoom), vh - 2 * (int)(vh * zoom) };
            SDL_SetRenderTarget(ren, zoom_tex);
            SDL_RenderCopy(ren, canvas, &in, NULL);
            SDL_SetRenderTarget(ren, canvas);
            SDL_RenderCopy(ren, zoom_tex, NULL, NULL);
        }
        // shortcut: 標籤阻擋先用左右視窗開關，等介面系統接回原版兩個全域阻擋狀態。
        viewport_ground_labels(&viewport, &scene, !!(SDL_GetModState() & KMOD_ALT),
                               !win_left.open && !win_right.open);
        viewport_ground_labels_draw();
        hud_draw(hero);
#ifdef STORY
        modal_begin();
        dialog_draw();
        modal_end();
#endif
        if (mouse_seen) {  // 觸控沒有游標
            const Anim *cur = anim_get(0x100011);
            int zx = px, zy = py, cx, cy;
            zoom_point(&zx, &zy);
            Actor *c = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
            int k = input_cursor(hero, c, ground_at(zx, zy, ox, oy, &scene), cx, cy);
            xmouse_draw(cur, k, px, py, tick);
        }
        {  // 載入中顯示小提示（0x502450：進入載入時挑一次），畫在底部置中
            static char tip[512];
            static int tipping;
            if (res_loading() > 0) {
                if (!tipping) loading_tip(tip, sizeof tip);
                tipping = 1;
                int tw = text_width(tip), th = draw_text(0, 0, tip, vw - 40, 0, 0);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
                SDL_Rect bg = {(vw - (tw < vw - 40 ? tw : vw - 40)) / 2 - 6, vh - th - 14, (tw < vw - 40 ? tw : vw - 40) + 12, th + 8};
                SDL_RenderFillRect(ren, &bg);
                draw_text(bg.x + 6, bg.y + 4, tip, vw - 40, 0xFFFFFF, 1);
            } else tipping = 0;
        }
        {  // 亮度（0x547020 → 0x5322c0：(滑桿值 + 0x32) / 100 倍，預設 50 = 1.0）。shortcut: 原版是顯示卡的伽瑪表，這裡在整個畫面疊黑（< 1）或加亮（> 1）近似
            int g = opt_v[O_GAMMA];
            if (g != 50) {
                SDL_SetRenderTarget(ren, canvas);
                SDL_SetRenderDrawBlendMode(ren, g < 50 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_ADD);
                int a = g < 50 ? (50 - g) * 255 / 100 : (g - 50) * 128 / 100;
                SDL_SetRenderDrawColor(ren, g < 50 ? 0 : a, g < 50 ? 0 : a, g < 50 ? 0 : a, g < 50 ? a : 255);
                SDL_RenderFillRect(ren, NULL);
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
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, canvas, NULL, &tch.L.game);
        if (win_off && win_tex && tch.L.ui_dst.w > 0 && (win_left.open || win_right.open) && !modal_rect(&(SDL_Rect){0})) SDL_RenderCopy(ren, win_tex, &tch.L.ui_src, &tch.L.ui_dst);  // 直式：放大的側邊視窗
        else if (tch.L.ui_dst.w > 0) SDL_RenderCopy(ren, modal_cleared ? modal_tex : canvas, &tch.L.ui_src, &tch.L.ui_dst);  // 直式：系統選單、存讀檔等居中小視窗（畫在遊戲畫面上）也放大，字大、點得準
        touch_draw();
        touch_preview_draw();
        modal_cleared = 0; modal_on = 0;
        if (shot && tch.forced && tick > 3) {  // --touch --shot：存整個螢幕（含留空和按鈕）
            int sw_, sh_;
            SDL_GetRendererOutputSize(ren, &sw_, &sh_);
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, sw_, sh_, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
            SDL_SaveBMP(surf, shot);
            SDL_FreeSurface(surf);
        }
        SDL_RenderPresent(ren);
        res_pump();
        audio_update(res_loading() == 0);
        int loading = res_loading();  // 只在數字變了才更新網頁文字
        if (loading != status_shown) js_status(loading);
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

    viewport_grid_clear();
    free(view_buildings.items); free(view_dynamic.items);
    view_buildings = (ViewList){0}; view_dynamic = (ViewList){0};
    cache_clear();
    audio_quit();
    world_free(&world);
    SDL_DestroyTexture(canvas);
    roles_clear();
    bird_clear();
    free(air); air = NULL; air_n = 0;
    free(sur); sur = NULL; sur_n = 0;
    scene_free(&scene);
    SDL_Quit();
    return 0;
}
