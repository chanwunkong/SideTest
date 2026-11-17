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

// 換場景時清空：磁磚 key 只有序號，不同場景的磁磚封包會撞號
static void cache_clear(void) {
    SDL_DestroyTexture(role_surface);
    role_surface = NULL;
    role_surface_w = role_surface_h = 0;
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
typedef struct { const SkillRow *skill; int level, status, elapsed, time, side, patch; } Buff;  // side 1 = 別人放在身上的（套「是否对敌」的參數）；
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
    int dynamic_command[3], dynamic_flag_snapshot;
    int dynamic_enter_tick, interpolation_left, dynamic_reported_state;
    float interpolation_x, interpolation_y, interpolation_dx, interpolation_dy;
    int jump_half, dust_x, dust_y;
    int dynamic_cache_x, dynamic_cache_y, dynamic_counter;
    int dynamic_destination_x, dynamic_destination_y;
    int dynamic_pending, dynamic_path, dynamic_aux, dynamic_path_result;
    const Scene *bound_scene;
    int grid_x, grid_y, grid_valid, occupancy_removed, object_dead, object_kind;
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
    int behavior_aux, behavior_unused, debug_rectangle;
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
    int body_seq, body_start, body_end, body_ready, animation_stepped;
    int act_frame, frame_tick, act_done;  // 單次動作播到第幾幀、這幀停了幾拍、播完了沒
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
    int copied, copy[12];
    int knock, stun;               // 擊退中、硬直剩幾拍（+0x42f、+0x433：不能動、不能出手）
    int print_x, print_y, foot;    // 上一個腳印的位置（+0x3c9）、這次踩哪隻腳（+0x3c5）
    int ai_tick, flee;             // 怪物 AI 計數（+0xbb，起始隨機 0~99）、正在拉開距離（走向 tx, ty）          // 陷阱：放的人的 AP、AP_MAX、MAP1~5、MAP1~5_MAX（0x10034cb0 照抄）
    int cool, fight, dead_ticks;   // 攻擊冷卻（拍）、戰鬥姿態剩幾拍、死了幾拍
    int sh;                        // 劇情物件（原版 EasyRPG 的人物；0 = 沒有）
    int remembered[4], remember_delay; // 原版記住命令與取消後延遲
    int command[4];                // 待執行命令，第一格 -1 表示沒有（原版 +0x5b6）
    char init_name[64], display_name[256]; // SInit事件ID為原始bytes；顯示名稱為UTF-8
    int display_number;           // 顯示名稱附加編號；不屬於SInit事件ID
    ActorSkill skills[MAX_ACTOR_SKILLS];
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
// 完整每拍入口的活動門檻、光環等分支仍待移植，不標完整入口位址。
static void actor_modifiers_tick(Actor *a) {
    if (a->direction_clock % 10) return;
    actor_status_countdown(a);
    for (int i = 0; i < 20; i++) {
        if (a->modifier[i].time < 0) continue;
        a->modifier[i].time -= 10;
        if (a->modifier[i].time < 0) {
            a->modifier[i].value = 0;
            a->modifier[i].time = -1;
            // 到期分支只有1/5/6具有副作用；不重新擲第3修正的潰散。
            if (i == 1 || i == 5 || i == 6) actor_modifier_changed(a, i, 0);
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

static Uint32 seq_ticks(const Seq *q) {
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
    if (a->def && a->def->anim) return 0;  // shortcut: 小動物專用三序列的狀態切換待還原。
    // 靜態SInit的初始態是明確選定的目標序列，不能每拍用休息序列覆蓋。
    if (!a->act && a->spec && !a->spec->logic && a->dynamic_state == 1)
        return a->animation_target;
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

// 三個附加精靈各自一份播放器；替換原先逐個Buff重畫且跟全域拍數循環的簡化版。
// shortcut: 頭上／身上位置沿用動畫內建偏移，模型高度欄位接入後核對額外錨點偏移。
static void status_fx_draw(const Actor *a, int sx, int sy, Uint32 tick, int foot) {
    (void)tick;
    for (int k = 0; k < 3; k++) {
        if ((k == 2) != foot || !a->effect[k]) continue;
        const Frame *f = spr_frame(a->effect[k]);
        if (f) draw(image(f->ref), sx - f->dx, sy - f->dy);
    }
}

// CSprGroup矩形基礎：與繪製共用同一序列／幀；沒載到的圖不加入聯集。
static void frame_rect_union(SDL_Rect *rect, const Anim *a, int seq, int frame, int x, int y) {
    if (!a || seq < 0 || seq >= a->count || frame < 0 || !a->seqs[seq].count) return;
    const Frame *f = &a->seqs[seq].frames[frame % a->seqs[seq].count];
    const Sprite *im = image(f->ref);
    if (!im || im->w <= 0 || im->h <= 0) return;
    SDL_Rect r = {x - f->dx, y - f->dy, im->w, im->h};
    if (rect->w) SDL_UnionRect(rect, &r, rect); else *rect = r;
}

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
static void role_draw(const Role *r, int seq, int frame, int sx, int sy) {
    if (!r || seq < 0 || seq >= r->seq_count || !r->frames[seq]) return;
    frame %= r->frames[seq];
    const uint8_t *order = r->order[seq] + frame * r->part_count;
    for (int i = 0; i < r->part_count; i++)
        if (order[i] < r->part_count) draw_frame(r->body[order[i]], seq, frame, sx, sy, 0);
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
            draw_frame(a->role->shadow[i], seq, frame, sx - src.x, sy - src.y, 1);
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

static void actor_draw(const Actor *a, int sx, int sy, Uint32 tick) {
    const Role *r = a->role;
    if (!r) return;
    int seq = actor_seq(a);
    if (seq < 0 || seq >= r->seq_count || !r->frames[seq]) return;
    int frame = actor_frame(a, seq) % r->frames[seq];
    if (a->scale) actor_scaled_draw(a, seq, frame, sx, sy, 1);
    else for (int i = 0; i < r->part_count; i++) draw_frame(r->shadow[i], seq, frame, sx, sy, 1);
    if (a->flags & 0x100) {
        uint32_t id = actor_relation_anim(a);
        const Anim *ring = id ? anim_get(id) : NULL;
        if (ring && ring->count) draw_frame(ring, 0, frame_at(&ring->seqs[0], tick), sx, sy, 0);
    }
    if (a->act != ACT_DEAD) status_fx_draw(a, sx, sy, tick, 1);
    actor_illusion_draw(a);
    if (a->scale) actor_scaled_draw(a, seq, frame, sx, sy - a->z, 0);
    else role_draw(r, seq, frame, sx, sy - a->z);  // 影子留在地上
    if (a->act != ACT_DEAD) status_fx_draw(a, sx, sy - a->z, tick, 0);
}

// 面向畫面方向 (sdx, sdy)
static void face(Actor *a, float sdx, float sdy) {
    int oct = (int)lroundf(atan2f(sdx, -sdy) / (float)(M_PI / 4));
    a->dir = (oct + 8) % 8;
    a->facing = DIR_MAP[a->dir];
}

// 對話開始：主角和 NPC 停下來面對面（0x44caf0）。
// shortcut: 原版是 EasyRPG 經回呼表叫的（哪個劇情指令還沒查），這裡先在點 NPC 說話時做；只有一個方向的模型（+0x118 = 1）原版不轉，沒分
static void talk_face(Actor *h, Actor *n) {
    float dx = n->x - h->x, dy = n->y - h->y;
    h->moving = n->moving = 0;
    face(h, dx - dy, (dx + dy) / 2);
    face(n, dy - dx, -(dx + dy) / 2);
}

// 往畫面方向 (sdx, sdy) 走 px 個畫面像素。畫面上各方向等速（格子寬 64 高 32，若照格子等速，上下會只有左右一半快）
static void actor_step(Actor *h, const Scene *s, float sdx, float sdy, float px) {
    if (h->dynamic_state == 21 || h->dynamic_state == 22 || h->dynamic_state == 31) return;
    float len = sqrtf(sdx * sdx + sdy * sdy);
    if (len < 0.01f) return;
    sdx /= len;
    sdy /= len;
    // 畫面位移 → 格子位移：畫面 x = (gx - gy) * 32，畫面 y = (gx + gy) * 16
    float gx = (sdx / 64 + sdy / 32) * px, gy = (-sdx / 64 + sdy / 32) * px;
    float previous_x = h->x, previous_y = h->y;
    // 能走就走；撞牆時試著只沿一個軸滑動
    if (scene_walkable(s, (int)(h->x + gx), (int)(h->y + gy))) { h->x += gx; h->y += gy; }
    else if (scene_walkable(s, (int)(h->x + gx), (int)h->y)) h->x += gx;
    else if (scene_walkable(s, (int)h->x, (int)(h->y + gy))) h->y += gy;
    if (h->grid_valid && ((int)h->x != h->grid_x || (int)h->y != h->grid_y) && !actor_place(h, (int)h->x, (int)h->y, 0)) {
        h->x = previous_x; h->y = previous_y;
    }
    face(h, sdx, sdy);
    h->moving = 1;
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

// 動態物件移速套用：基準比值只用A/B，不把C的小數移動量算進動畫速率。
// shortcut: 狀態切換沿用現有dynamic_state／moving；完整狀態轉接接入時補虛擬退出入口。
// 每個Actor保存調整量，避免修改共用Role資源而讓同模型的其他角色一起加速。
static void actor_move_speed(Actor *a, int speed) {
    if (speed < 1 && a->dynamic_state == 11) {
        // 零速也走角色退出分支，必須在改寫舊狀態前收尾移動殘影。
        actor_trail_exit(a);
        a->dynamic_state = 1;
        a->moving = 0;
        a->print_x = a->print_y = -100;
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
    if (a->dynamic_state == 21 || a->dynamic_state == 22 || a->dynamic_state == 31) return;
    a->moving = 0;
    // shortcut: 現有移動迴圈承接平常／移動狀態，跳躍等完整動態狀態仍待移植。
    int state = mx * mx + my * my > 0.01f ? 11 : 1;
    int changed = state != a->dynamic_state;
    if (state != a->dynamic_state) {
        actor_trail_exit(a);
        a->dynamic_state = state;
        actor_trail_enter(a);
    }
    actor_speed_update(a);
    if (mx * mx + my * my > 0.01f && actor_can_move(a)) {
        float px = tick % a->move_b ? 0 : a->move_a + a->move_c / 10.0f;
        actor_step(a, s, mx, my, px);
    }
    if (!(a->flags & 0x400)) a->animation_dir = DIR_MAP[a->dir];
    if (changed || state == 11 || !a->spec || a->spec->logic)
        actor_animation_select(a, -1);
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
// shortcut: 目前用劇情 handle 對 actors；完整 CScene 物件索引接入時改用原版編號。
static inline Actor *actor_of(int h) {
    for (int i = 0; h && i < actor_count; i++)
        if (actors[i].sh == h && actors[i].role) return &actors[i];
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

// Poq.exe 0x489380：只揭開操控角色同組；上層擴大半徑，迴圈右／下界不含端點。
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
// 劇情handle為正整數；無劇情物件使用高半區，跨場景不重用，避免關聯命中另一個角色。
static uint32_t next_actor_object_id = 0x80000000u;

// Poq.exe 0x4862e0：套用角色模板；狀態103先啟用，再寫模型種類、組別、光源、占位與探索。
static void actor_setup(Actor *a, const RoleDef *d) {
    // 新角色的候選初值屬於建構；重套模板不可先清掉既有候選。
    if (!a->def) {
        if (!a->object_id) a->object_id = next_actor_object_id++;
        for (int i = 0; i < 4; i++) a->candidate[i] = -1;
        a->remembered[0] = -1;
        a->dynamic_state = 1;
        a->layer = 1;
        if (!a->object_kind) a->object_kind = 31;
        for (int i = 0; i < 20; i++) a->modifier[i].time = -1;
        a->hp_percent = 100;
        a->effect_speed = 100;
        a->display_number = -1;
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
    actor_properties_update(a);
}

static void spawn_npcs(const Scene *s) {
    actor_count = 1;  // actors[0] 是主角
    for (int i = 0; i < s->npc_count && actor_count < MAX_ACTORS; i++) {
        const NpcSpec *n = &s->npcs[i];
        const RoleDef *d = roledef_find(n->tmpl);
        if (!d || (d->beast && d->air) || !atoi(d->graphic)) continue;  // shortcut: 圖形 ID 是名字的（怪物）用另一套格式，之後做戰鬥時處理
        Actor *a = &actors[actor_count++];
        memset(a, 0, sizeof *a);
        actor_behavior_reset(a);
        a->x = n->x + 0.5f;
        a->y = n->y + 0.5f;
        a->spec = n;
        a->object_kind = 40;
        actor_setup(a, d);
        if (!actor_bind_scene(a, s) || !actor_place(a, (int)a->x, (int)a->y, 1)) a->role = NULL;
#ifdef STORY
        const char *event = actor_init_name(a);
        a->sh = event && *event ? story_find(sroom, event) : 0;
        if (a->sh) actor_person_init(a, a->sh, story_get_int);
#endif
    }
}

// 房間的怪物群（EasyRPG.dll 0x10052550）：每群挑一個「前场景名称 = 區域」的入口點，整群出現在附近
#define MON_WANDER 4  // shortcut: 怪物閒逛半徑（格）是自訂的，原版 AI 還沒拆
static NpcSpec mon_specs[MAX_ACTORS];

static void mon_derive(Actor *a);
static int skill_attack_id(const SkillRow *r);

// 怪物的技能1（Data/skill_npc.txt，0x100529a0 → 0x10059b20 依索引名找）：技能等級用表上的初始技能等级，沒填 0
static const SkillRow *npc_skill(const char *key) {
    static SkillRow rows[512];
    static int n = -1;
    if (n < 0) n = skill_rows_file("skill_npc.txt", rows, 512);
    for (int i = 0; i < n; i++)
        if (!strcmp(rows[i].key, key)) return &rows[i];
    return NULL;
}

// 放一隻怪物在 (x, y)，閒逛範圍以 (cx, cy) 為中心
static Actor *mon_make(const MonDef *m, const RoleDef *d, int x, int y, int cx, int cy, int lv) {
    Actor *a = &actors[actor_count];
    NpcSpec *sp = &mon_specs[actor_count++];
    *sp = (NpcSpec){.type = 13, .logic = 1, .tmpl = d->id, .x = x, .y = y, .rx = cx - MON_WANDER, .ry = cy - MON_WANDER,
                    .rw = MON_WANDER * 2 + 1, .rh = MON_WANDER * 2 + 1};
    memset(a, 0, sizeof *a);
    actor_behavior_reset(a);
    a->x = x + 0.5f;
    a->y = y + 0.5f;
    a->spec = sp;
    a->mon = m;
    a->object_kind = m->istrap ? 24 : 40;
    // shortcut: 陷阱沿用mon_make建立，預設未被發現；完整CTrapBase建立入口接入時對照初始旗標。
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
    if (unit_scene && !m->istrap && (!actor_bind_scene(a, unit_scene) ||
        !actor_place(a, (int)a->x, (int)a->y, 1))) a->role = NULL;
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
            if (!d || (!atoi(d->graphic) && !d->anim)) continue;  // shortcut: 圖形 ID 是名字的（蝙蝠、骷髏、蜘蛛）用另一套格式，還沒做
            int x = en->x, y = en->y;
            int lv = mg->level, tier = mg->tier;
            if (lv < 0) lv = tier * 5 - rand() % 5;
            Actor *a = mon_make(m, d, x, y, en->x, en->y, lv);
            printf("怪物 %s Lv%d HP%d 在 (%d,%d)\n", m->name, a->level, a->hp, x, y);
        }
    }
}

// ---- 戰鬥 ----
#define FIGHT_TICKS 300  // shortcut: 打完多久收起戰鬥姿態是自訂的（約 5 秒）
#define CORPSE_TICKS 600 // shortcut: 屍體留多久是自訂的（約 10 秒）
static int exp_table[100], exp_levels;  // Data/experience.txt：第 n 列 = 升到 n+1 級要的經驗

static int alive(const Actor *a) { return a && a->role && !a->object_dead && a->act != ACT_DEAD; }

static float cell_dist(const Actor *a, const Actor *b) {
    return fmaxf(fabsf(a->x - b->x), fabsf(a->y - b->y));
}

// 攻速（Poq.exe 0x48d330，出手時）：縮短 = 總拍數 − trunc(float(總拍數)×攻速倍率)；每幀延遲調整 = −縮短 / 幀數。
// 有餘數時：縮短 > 0 調整再 −1、要補 幀數 − 餘數 拍，否則補 −餘數 拍；調整 < −1 時補的減半。補的（> 0 才算）加到出手間隔
// shortcut: 原版 +0x345 有值時調整再 +2，那個欄位的意思沒查
static void attack_speed(Actor *a) {
    const Seq *q = role_seq(a->role, actor_seq(a));
    if (!q || !q->count) return;
    int total = (int)seq_ticks(q), cut = total - (int)((float)total * actor_fight_ratio(a)), rem = cut % q->count;
    a->adj = -(cut / q->count);
    if (rem) {
        if (cut < 1) a->extra = -rem;
        else {
            a->adj--;
            a->extra = q->count - rem;
        }
        if (a->adj < -1) a->extra /= 2;
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
static const int hero_gender = 0, hero_age = 29;  // 同一模板的 Gender、Age
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
            if ((m->enemy > 0) != h->buff[b].side || m->pct != pct) continue;
            int *f = stat_ptr(h, m->id);
            float v = skill_value(m->init, m->incr, h->buff[b].level);
            if (f) *f += (int)(pct ? *f * v / 100 : v);
        }
}

// 身上裝備的特性（穿上時 0x1002fe20 複製到人身上，重算 0x100300f0 只算生效的）：固定值、百分比分兩輪，跟技能參數同一套屬性
static void item_mods(Actor *h, int pct) {
    for (int s = INV_BAG; s < INV_N; s++)
        for (int i = 0; inv[s].name[0] && i < inv[s].fx_n; i++) {
            const ItemFx *f = &inv[s].fx[i];
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
static void hero_derive(Actor *h) {
    const int K = 150, lv = h->level;
    h->hp_max = 120 + 28 * lv;
    h->mp_max = 140 + 4 * lv;
    h->ap = h->ap_max = h->dp = h->hr = h->sr = h->hp_speed = h->mp_speed = 0;
    memset(h->ex, 0, sizeof h->ex);
    item_mods(h, 0);
    actor_mods(h, 0);
    // 裝備、技能加的力量 27、体质 28、敏捷 29、智慧 32 也算進倍率和回復
    const int str = hero_attr[STR] + h->ex[27], con = hero_attr[CON] + h->ex[28], dex = hero_attr[DEX] + h->ex[29], wis = hero_attr[WISDOM] + h->ex[32];
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
    item_mods(h, 1);
    actor_mods(h, 1);
    status_patch(h);
    actor_properties_update(h);
    if (h->hp > h->hp_max) h->hp = h->hp_max;
    if (h->mp > h->mp_max) h->mp = h->mp_max;
}

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
static void actor_derive(Actor *a) { if (a->mon) mon_derive(a); else hero_derive(a); }

static void hero_stats(Actor *h, int level) {
    h->level = level;
    hero_derive(h);
    h->hp = h->hp_max;
    h->mp = h->mp_max;
}

static void actor_kill(Actor *t, Actor *by);
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

// 主角拿的武器類（Hand，0x1003b760）：右手道具類型 8（刀）、左手空 → 0
// shortcut: 只有開局的青銅彎刀，換武器、左手道具還沒做，先固定 0
static const int hero_hand = 0;

// 技能能不能用（0x1004c2a0）：等級 > 0、精气夠、武器合 W_Type（-1 不限；相同可以；0 也接受 Hand < 0、1 接受 2、4 接受 5）
static int skill_usable(const Actor *h, const SkillRow *r) {
    int w = r->wtype;
    if (r->level < 1 || h->mp < skill_mpu(r)) return 0;
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
static int buff_add(Actor *a, const SkillRow *r, int status, int side, int time, int patch) {
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
    actor_status_notify(a, status, 1, -1);
    printf("狀態 %d 生效 %d 秒（%s）\n", status, time, side ? "對方" : "自己");
    return 1;
}

// 修正入口的副作用；狀態17／100是原版byte狀態，不能用帶時間的技能Buff取代。
static void actor_modifier_changed(Actor *a, int index, int value) {
    if (index == 1) actor_speed_update(a);
    else if (index == 6) {
        if (value) actor_illusion_begin(a);
        else { actor_illusion_end(a); a->modifiers_dirty = 1; }
    } else if (index == 5) {
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
static void actor_health_sync(Actor *a) {
    if (!a->def || a->hp_max <= 0) return;
    actor_parameters(a, (int)((int64_t)a->hp * 100 / a->hp_max), a->speed_threshold,
        a->behavior[0], a->behavior[1], a->behavior[3], a->behavior[2]);
}

static void strike(Actor *a, Actor *t, const SkillRow *sk, Uint32 tick);

// 立即生效的參數（0x10035fd0，屬性分類 1）：21 HP、22 MP 加值（百分比 = 目前值的 %），292、293 = HP_MAX、MP_MAX 的 % 加到 HP、MP
static void skill_now(Actor *x, const SkillMod *m, int level) {
    float v = skill_value(m->init, m->incr, level);
    int *f = m->id == 21 || m->id == 292 ? &x->hp : m->id == 22 || m->id == 293 ? &x->mp : NULL;
    if (!f) return;
    int max = f == &x->hp ? x->hp_max : x->mp_max;
    *f += (int)(m->id > 100 ? v * max / 100 : m->pct ? v * *f / 100 : v);
    *f = *f < 0 ? 0 : *f > max ? max : *f;
}

// 原版 0x10052a00：n > 1 時 rand() % n，否則 0
static int rnd(int n) { return n > 1 ? rand() % n : 0; }

// 召喚（Attack方式 3，0x100355d0）：參數 273 選召喚兽（1 狼、2 老虎、3 熊、4 鸟、5 白狼、6 木偶、7 狗、8 虎蛟、9 狍鸮、10 朱厌；
// 原版表上第 11 個善翼超出檢查範圍，不會召出來），278 = 最多幾隻；選狼時 rand()%100 是 3 的倍數就換白狼。等級同召喚者，跟召喚者同一邊
// 數量：同種的（選狼時狼和白狼一起算）已有 278 隻就不召；召前 6 種時先移掉別種的召喚兽（狼、白狼互不移）。
// shortcut: 放在召喚者旁邊（原版放在攻擊位置）；原版數的是整隊的召喚兽（istrap 20），這裡只數這個召喚者的
static void summon(Actor *a, const SkillRow *r) {
    static const char *N[10] = {"\xd5\xd9\xbb\xbd\xca\xde\xc0\xc7", "\xd5\xd9\xbb\xbd\xca\xde\xc0\xcf\xbb\xa2", "\xd5\xd9\xbb\xbd\xca\xde\xd0\xdc", "\xd5\xd9\xbb\xbd\xca\xde\xc4\xf1", "\xd5\xd9\xbb\xbd\xca\xde\xb0\xd7\xc0\xc7", "\xd5\xd9\xbb\xbd\xca\xde\xc4\xbe\xc5\xbc", "\xd5\xd9\xbb\xbd\xca\xde\xb9\xb7", "\xd5\xd9\xbb\xbd\xca\xde\xbb\xa2\xf2\xd4", "\xd5\xd9\xbb\xbd\xca\xde\xe1\xf3\xfb\x5e", "\xd5\xd9\xbb\xbd\xca\xde\xd6\xec\xd1\xe1"};
    int k = -1, max = 0;
    for (int i = 0; i < r->mod_n; i++) {
        int v = (int)skill_value(r->mod[i].init, r->mod[i].incr, r->level);
        if (r->mod[i].id == 273) k = v - 1;
        else if (r->mod[i].id == 278) max = v;
    }
    if (k < 0 || k > 9 || max > 9) return;
    int n = 0;
    for (int i = 0; i < actor_count; i++) {
        const Actor *e = &actors[i];
        if (e->owner == a && alive(e)) n += !strcmp(e->mon->name, N[k]) || (k == 0 && !strcmp(e->mon->name, N[4]));
    }
    if (n >= max) return;
    for (int i = 0; k < 6 && i < actor_count; i++) {
        Actor *e = &actors[i];
        if (e->owner == a && alive(e) && strcmp(e->mon->name, N[k]) && !(k == 0 && !strcmp(e->mon->name, N[4]))) {
            actor_unoccupy(e); e->role = NULL;
        }
    }
    if (k == 0 && rnd(100) % 3 == 0) k = 4;
    const MonDef *m = mondef_pick(N[k]);
    const RoleDef *d = m ? roledef_find(m->role) : NULL;
    if (!d || actor_count >= MAX_ACTORS) return;
    Actor *s = mon_make(m, d, (int)a->x + rnd(3) - 1, (int)a->y + rnd(3) - 1, (int)a->x, (int)a->y, a->level);
    actor_owner_set(s, a);
    s->side = a->side;
    actor_group(s, a->grp);
    s->foe = a->foe;
    printf("召喚 %s\n", m->name);
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

// 在 a 旁邊放一隻怪物表上叫 name 的，加入 a 的隊伍
// shortcut: 原版放在攻擊位置（G_PosX/Y）、等級用表上的（FUN_10059870 沒設），這裡放在旁邊、等級跟放的人一樣
static Actor *make_beside(Actor *a, const char *name) {
    const MonDef *m = mondef_find(name);
    const RoleDef *d = m ? roledef_find(m->role) : NULL;
    if (!d || actor_count >= MAX_ACTORS) return NULL;
    Actor *s = mon_make(m, d, (int)a->x + rnd(3) - 1, (int)a->y + rnd(3) - 1, (int)a->x, (int)a->y, a->level);
    join(s, a);
    s->exp = 0;
    printf("放出 %s\n", m->name);
    return s;
}

// 馴服（Attack方式 2，0x10035370）：只對 istrap 10 的動物，隊上 istrap 10 的已有參數 271 隻就不行；成功就加入隊伍
// shortcut: 原版還看 Friend ≥ 4（敵對的）、失敗會出提示字（5000、0x138a、0x1400），這裡只看是不是別隊的、不出字
static void tame(Actor *a, Actor *t, const SkillRow *r) {
    if (!t || !alive(t) || t->side == a->side || !t->mon || t->mon->istrap != 10) return;
    int n = 0;
    for (int i = 0; i < actor_count; i++) n += alive(&actors[i]) && actors[i].side == a->side && actors[i].mon && actors[i].mon->istrap == 10;
    if (n >= skill_param(r, 271)) return;
    join(t, a);
    printf("馴服 %s\n", t->mon->name);
}

// 陷阱（Attack方式 8，0x10034cb0）：參數 286 選 1 猎杀陷阱、2 引蜂术、3 捆仙之索、4 陷地毒签、5 无情箭阱；自己的陷阱超過 7 個不放。
// 攻擊力、法術攻擊照抄放的人，不給經驗
static int trap_cell(int *x, int *y, int n);  // near.c
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
    s->trap_group = a->grp;
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

// 分身（Attack方式 11，0x10034140）：已有分身就不放；拿弓（Hand 2、5）放克隆刺客弓，否則克隆刺客刀
// shortcut: 原版把放的人的裝備（G_EquipStr3 → 分身的 G_EquipStr1）給分身，換裝還沒做
static void clone(Actor *a) {
    for (int i = 0; i < actor_count; i++)
        if (alive(&actors[i]) && actors[i].owner == a && !strncmp(actors[i].mon->name, "\xbf\xcb\xc2\xa1", 4)) return;
    make_beside(a, hero_hand == 2 || hero_hand == 5 ? "\xbf\xcb\xc2\xa1\xb4\xcc\xbf\xcd\xb9\xad" : "\xbf\xcb\xc2\xa1\xb4\xcc\xbf\xcd\xb5\xb6");
}

// 技能打中（0x1003f590）：我方状态、敌方状态都沒填就不加；我方状态已在自己身上、敌方状态已在對方身上也不加。
// 否則自己加我方状态、對方加敌方状态（0x1002fe20 → 0x10037f40），時間 = 作用時間。
// 再算攻擊（0x10025b70）：立即生效的參數照「是否对敌」給對方或自己；Attack方式 = 0 不造成傷害
// Attack方式：1、4、10 物理，2 馴服，3 召喚，8 陷阱，9 除障，11 分身；5 開鎖、6 偷竊、12 清狀態、13 界面技能沒有技能用到，沒做
// shortcut: 沒有敌方状态但有對敵參數的（點石成俑）也當成狀態加在對方身上
static void skill_hit(Actor *a, Actor *t, const SkillRow *r, Uint32 tick) {
    if (r && (r->status > 0 || r->estatus > 0) && !(r->status > 0 && has_status(a, r->status)) && !(t && r->estatus > 0 && has_status(t, r->estatus))) {
        if (r->status > 0) buff_add(a, r, r->status, 0, skill_time(r), 0);
        int enemy_mod = 0;
        for (int i = 0; i < r->mod_n; i++) enemy_mod |= r->mod[i].enemy > 0 && r->mod[i].id >= 20 && r->mod[i].id < 100;
        if (t && (t != a || skill_manual(r) == 2) && alive(t) && (r->estatus > 0 || enemy_mod)) buff_add(t, r, r->estatus, 1, skill_time(r), 0);
    }
    for (int i = 0; r && i < r->mod_n; i++) {
        const SkillMod *m = &r->mod[i];
        if (m->id == 21 || m->id == 22 || m->id == 292 || m->id == 293) skill_now(m->enemy > 0 && t ? t : a, m, r->level);
    }
    int k = r ? r->attack : 1;
    if (k == 2) tame(a, t, r);
    else if (k == 3) summon(a, r);
    else if (k == 8) trap(a, r);
    else if (k == 9) disarm(a, t, r);
    else if (k == 11) clone(a);
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

// 倒下（0x100574e0）：播死亡動作，被主角打死的怪物給經驗，夠了就升級
static void actor_kill(Actor *t, Actor *by) {
    t->hp = 0;
    actor_destroy(t);
    start_act(t, ACT_DEAD, t->def->death);
    if (t->def->snd_death) audio_sfx(t->def->snd_death);
    if (t->mon) mon_loot(t);
    if (by && !by->mon && t->mon) hero_gain_exp(by, t->exp);
    printf("%s 倒下\n", t->mon ? "怪物" : "主角");
#ifdef STORY
    if (t->sh) story_killed(t->sh);  // 原版 0x100574e0 最後刪掉人物；Boss 的第 10 格會開劇情
#endif
}

// ---- 招式單位（觸發 type 2 → MethodUnit_Mdl.ini）：原版的特效與飛行物件，見 unit.c ----
static int shake_left, shake_total;
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
static struct { Item it; float x, y; } ground[MAX_GROUND];
static int ground_n, ground_want = -1;  // ground_want：主角正走過去撿的
static int walk_cx = -1, walk_cy;       // 點地面要走去的格子（-1 = 沒有）
#include "light.c"
#define scene (*unit_scene)
#include "unit.c"
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
// 完整活動／動態更新門檻仍待接齊，不標完整角色更新入口位址。
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
    if (gate < 1 || !a->move_remaining) return 1;
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

// 動態進入的21/22分支；完整狀態主分派仍待補，不標主入口完成。
static void actor_rush_enter(Actor *a) {
    a->dynamic_enter_tick = (int)actor_logic_tick;
    actor_speed_update(a); // 非靜止的虛擬+0x40，角色有效速度。
    if (a->move_a < 2 || !actor_rush_destination(a)) {
        a->dynamic_path_result = 0;
        return;
    }
    actor_set_flags(a, 0x200, 1);
    if (a->dynamic_state == 22) actor_jump_phase(a, 1);
}

// 命令7/10與動態狀態設定的衝刺／跳躍分支，供MethodMain觸發使用。
// shortcut: 共用命令分派的技能可用性尚未齊備；僅接已存在MethodMain的內部觸發，完整分派接齊後合併。
static int actor_rush_command(Actor *a, int state, int x, int y, int distance) {
    if (has_status(a, 2) || has_status(a, 10) || has_status(a, 101) || a->knock == 1 ||
        (a->dynamic_state != 1 && a->dynamic_state != 11)) return 0;
    int flag = actor_flags(a, 0x10000);
    actor_trail_exit(a);
    a->dynamic_path_result = 1;
    a->dynamic_state = state;
    a->dynamic_command[0] = x; a->dynamic_command[1] = y; a->dynamic_command[2] = distance;
    actor_trail_enter(a);
    actor_rush_enter(a);
    if (!a->dynamic_path_result) {
        actor_trail_exit(a);
        a->dynamic_state = 1;
        a->moving = 0;
        a->print_x = a->print_y = -100;
        actor_animation_select(a, -1);
    }
    a->dynamic_flag_snapshot = flag;
    return 1;
}

// Poq.exe 0x494a20：最後一拍只插值；下一拍才退出。平常命令(1,1,0,0)強制離開21/22。
static void actor_rush_tick(Actor *a) {
    if (a->interpolation_left > 0) { actor_interpolation_tick(a); return; }
    int flag = actor_flags(a, 0x10000);
    if (a->dynamic_state != 1 || a->dynamic_command[0] != 1 || a->dynamic_command[1] ||
        a->dynamic_command[2] || a->dynamic_flag_snapshot != flag) {
        int old = a->dynamic_state;
        actor_trail_exit(a);
        a->print_x = a->print_y = -100;
        a->dynamic_state = 1;
        a->moving = 0;
        if (old != 21 && old != 22) actor_animation_select(a, -1);
    }
    a->dynamic_flag_snapshot = flag;
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

// 共用byte通知及套表，供修正17／100與技能Buff使用。
// 已接本類別的隱身、反擊開關、蠱惑與特殊共用倒數；
// 主分派的周遭命令與外部通知仍todo，不標完整主入口位址。
static void actor_dynamic_rest(Actor *a);
static void actor_candidate_cast(Actor *a);
static void actor_status_notify(Actor *a, int status, int on, int skill) {
    if ((unsigned)status >= 128u || a->state[status] == on) return;
    unsigned char previous = a->state[status];
    a->state[status] = (unsigned char)on;
    switch (status) {
    case 0:
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

// 特殊狀態共用倒數與狀態12偵測；介面自動暫停通知仍待接，故不標完整入口位址。
static void actor_status_countdown(Actor *a) {
    --a->status_countdown;
    int status = a->state[105] ? 105 : a->state[12] ? 12 :
                 a->state[111] ? 111 : a->state[115] ? 115 :
                 a->state[114] ? 114 : -1;
    if (status == 12) {
        if (!unit_scene || !unit_scene->cells) return; // 尚未登記場景時沒有可查詢物件。
        Actor *t = near_trap((int)a->x, (int)a->y, 5, a->grp);
        if (t) {
            // todo: 介面自動暫停選項啟用時，先送訊息1501/16/1000並暫停場景。
            // 尚未接CGameApp介面選項與暫停通知，不能自行以玩家組或side猜測條件。
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
    const Seq *old = role_seq(a->role, a->shown_seq);
    const Seq *next = role_seq(a->role, target);
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
    next = role_seq(a->role, a->shown_seq);
    // 原版只保存幀索引；換方向重新選序列後的幀內拍數為零。
    a->act_frame = next && frame < next->count ? frame : 0;
}

// Poq.exe 0x487f30：空中不推本體，但仍清挨打、播放三個附加精靈。
static void actor_animation_tick(Actor *a) {
    if (a->head_height_pending) actor_head_height(a);
    a->animation_stepped = 1;
    actor_direction_tick(a);
    actor_body_select(a);
    const Seq *q = role_seq(a->role, a->body_seq);
    if (!(a->flags & 0x200) && q && q->count) {
        if (a->knock == 1) {
            for (int i = 0; i < 3; i++) actor_body_step(a, q, 1);
        } else {
            // shortcut: 死亡仍由Actor屍體計時承接，保留停末幀；原版死亡物件移交接入時移除此橋接。
            actor_body_step(a, q, a->act == ACT_DEAD);
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

// 角色更新中已核對的維護分支：冷卻先扣，空中非靜止且未旋轉時整段略過。
// shortcut: 動態基底／動畫仍由固定拍既有入口推進；完整角色更新接齊後合併活動門檻與光環。
static void actor_maintenance_tick(Actor *a) {
    actor_cooldown_tick(a);
    if (!(a->flags & 0x400) && (a->flags & 0x200) && a->dynamic_state != 1) return;
    actor_modifiers_tick(a);
    actor_scale_tick(a);
    if (a->modifiers_dirty) actor_effects_recompute(a);
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

// 動態狀態設定的(1,0,0,0)分支，供換裝使用。
// shortcut: 其他狀態命令仍由既有移動／招式入口承接，完整動態分派接齊後合併。
static void actor_dynamic_rest(Actor *a) {
    int flag = actor_flags(a, 0x10000);
    if (a->dynamic_state != 1 || a->dynamic_command[0] || a->dynamic_command[1] ||
        a->dynamic_command[2] || a->dynamic_flag_snapshot != flag) {
        int old = a->dynamic_state;
        actor_trail_exit(a);
        a->print_x = a->print_y = -100;
        // 原版靜止命令不強制打斷21/22；仍會先執行退出的降落與清旗標。
        if (old != 21 && old != 22) {
            a->dynamic_state = 1;
            a->moving = 0;
            actor_animation_select(a, -1);
        }
    }
    a->dynamic_flag_snapshot = flag;
}

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
    a->status_aux = a->behavior_aux = a->cool = a->behavior_unused = a->debug_rectangle = 0;
    a->corrected_x = a->corrected_y = 0;
    actor_status_notify(a, 0, 0, -1);
    actor_light_init(a);
    actor_dynamic_rest(a);
    actor_height_set(a, -20, 0);
    actor_dynamic_reset(a);
    // 單機令牌配對沿用Actor身分；此處不複製伺服端名單節點。
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
static void actor_appearance_init(Actor *a, const char *override) {
    const RoleDef *d = a->def;
    const Role *r = NULL;
    if (override && *override && strcasecmp(override, "NULL")) {
        RoleDef look = *d;
        // 原版星號分詞略過空token：首值是動作方式（PLI），餘值重新以*串接。
        const char *p = override + strspn(override, "*");
        size_t n = strcspn(p, "*");
        look.pli = atoi(p);
        look.styles[0] = 0;
        p += n;
        size_t used = 0;
        while (*(p += strspn(p, "*"))) {
            n = strcspn(p, "*");
            // shortcut: 沿用RoleDef的96-byte款式上限，遇到更長外觀資料時改動態字串。
            if (used + n + (used != 0) >= sizeof look.styles) { r = NULL; break; }
            if (used) look.styles[used++] = '*';
            memcpy(look.styles + used, p, n); used += n;
            look.styles[used] = 0;
            p += n;
        }
        if (!*p) r = role_variant_for(&look);
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
// 入戰透過actor_enter_fight轉入狀態byte0；完整命令11的選招門檻仍待主命令分派。
static void actor_target_set(Actor *a, Actor *t) {
    if (!t) { actor_clear_target(a); return; }
    a->foe = t;
    if (!grp_npc(t->grp)) return;
    foe_grp(a, t);
    if (grp_npc(a->grp)) return;
    int dx = (int)a->x - (int)t->x, dy = (int)a->y - (int)t->y;
    if ((int)sqrt((double)dx * dx + (double)dy * dy) <= actor_seek(t) && !t->fight) {
        // 原版先轉入戰鬥，再互設目標；順序可避免重入時再次觸發候選招式。
        actor_enter_fight(t);
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

// 動態進入的31分支：命令方向背離攻方，面向反向，位移沿命令方向。
// shortcut: 命令5的共同技能可用性仍待角色主分派；目前戰鬥受擊直接接此分支。
static int actor_knock_command(Actor *a, int direction, int cells, int speed) {
    if (has_status(a, 2) || has_status(a, 10) || has_status(a, 101) || a->knock == 1 ||
        (a->dynamic_state != 1 && a->dynamic_state != 11)) return 0;
    int flag = actor_flags(a, 0x10000);
    if (a->dynamic_state != 31 || a->dynamic_command[0] != direction ||
        a->dynamic_command[1] != cells || a->dynamic_command[2] != speed ||
        a->dynamic_flag_snapshot != flag) {
        actor_trail_exit(a);
        a->dynamic_path_result = 1;
        a->dynamic_state = 31;
        a->dynamic_command[0] = direction;
        a->dynamic_command[1] = cells;
        a->dynamic_command[2] = speed;
        actor_trail_enter(a);
        a->dynamic_enter_tick = (int)actor_logic_tick;
        actor_speed_update(a);
        actor_direction_set(a, direction >= 0 && direction < 8 ? (direction + 4) % 8 : direction);
        if (speed) actor_move_speed(a, speed);
        a->reverse_playback = 1;
        actor_animation_select(a, -1);
    }
    a->dynamic_flag_snapshot = flag;
    return 1;
}

static void actor_knock_finish(Actor *a) {
    actor_dynamic_rest(a);
    if (a->dynamic_counter) {
        a->dynamic_counter = 0;
        // shortcut: 原版在這裡送回動態11的快取目的地；完整尋路未齊，
        // 先用既有目的地行走消費端，完成動態11後改交共用設定入口。
        a->tx = a->dynamic_destination_x + 0.5f;
        a->ty = a->dynamic_destination_y + 0.5f;
        a->has_target = 1;
        if (a == actors) { walk_cx = a->dynamic_destination_x; walk_cy = a->dynamic_destination_y; }
    }
}

// 動態31逐格更新；原版先移占格再插值，插值結束的下一拍才標記可走下一格。
// shortcut: 返回動態11沿actor_knock_finish的既有行走橋接，完整尋路接齊後移除。
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

// 角色固定拍先跑動態分派，再由動態基底推高度與共用時鐘。
// shortcut: 此處只整合已完成的21/22/31；11/32、活動byte與動畫虛擬呼叫
// 尚未齊備，完整角色更新接齊後合併，不把主入口標成完成。
static void actor_motion_tick(Actor *a) {
    actor_rush_update(a);
    actor_knock_tick(a);
    actor_height_tick(a);
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
        // shortcut: 命令12的共同可用性／技能選取門檻尚未完整接入，
        // 完成角色命令主分派時由同一接收端處理；此處保留狀態與招式中止業務。
        actor_status_notify(t, 101, 1, -1);
        actor_animation_select(t, -1);
        if (has_status(t, 101)) start_act(t, ACT_HIT, t->def->hit);
    }
}

// 位移由固定拍的動態31推進；第5修正只控制動畫／命令門檻，不自行產生落點。
static int react_tick(Actor *a, const Scene *s) {
    (void)s;
    if (a->stun > 0) a->stun--;
    return a->dynamic_state == 31 || a->knock || a->stun > 0;
}

static void strike(Actor *a, Actor *t, const SkillRow *sk, Uint32 tick) {
    (void)tick;
    if (!alive(t)) return;
    if (t->mon && !t->foe) actor_target_set(t, a);  // 被打的怪物會反擊
    actor_enter_fight(t);
    // 命中（Poq.exe 0x48c600，CCharactor 虛擬函式第 104 格）：千分比 = HR×1000 / (SR + HR)，夾在 100~900；HR 0 當 1
    // shortcut: 之前還有一段盾牌格擋（0x48c4e3，看防守方 +0x4e7 的裝備和攻擊方向），沒有盾的主角用不到，還沒做
    int hr = a->hr ? a->hr : 1, chance = hr * 1000 / (t->sr + hr);
    if (rand() % 1000 >= (chance < 100 ? 100 : chance > 900 ? 900 : chance)) return;
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
    if (t->hp <= 0) { actor_kill(t, a); return; }
    hit_react(a, t, sk);
    if (!sk || sk->attack == 0 || sk->attack == 1 || sk->attack == 2 || sk->attack == 3 || sk->attack == 8)
        actor_blood(t, 1);
}

// 敵對：不同隊伍的
static int hostile(const Actor *a, const Actor *e) { return e != a && alive(e) && e->side != a->side; }

// 招式打中的對象（Poq.exe 0x43c4f0 → 0x438a00 依 iTriID，0x439fb0 找範圍內的、模式 0 只打第一個）：
// 6~10 = 招式物件位置（角色出手時 = 出手的人，0x43af50）周圍 64~320（1~5 格）內全部；12 = 目標周圍 0xc0（3 格）內全部（中心 0x4395f0 = 目標，沒有就目標點）；
// 5、11、13 = 目標點附近第一個；30 = 直接打目標；0、2、3、0x14 = 前方第一個。單一的這裡都打攻擊目標。
// type 16：0x16 = 向四周放 ID2 個單位（疾風掃落葉 8 個），0x14 = 向前放 ID2 個（穿心一擊）
// shortcut: 16 的單位打到誰用技能作用範圍最大估：四周 = 範圍內全部，向前 = 朝目標方向寬 1 格的直線上
static void method_hits(Actor *a, const Trig *t, Uint32 tick) {
    const SkillRow *sk = a->casting ? a->cast : a->skill;
    Actor *foe = a->foe;
    float cx = a->x, cy = a->y, r = -1, ux = 0, uy = 0;
    if (t->type == 0 && t->id >= 6 && t->id <= 10) r = t->id - 5;
    else if (t->type == 0 && t->id == 12) {
        if (!alive(foe)) return;
        cx = foe->x;
        cy = foe->y;
        r = 3;
    }
    else if (t->type == 16) {
        r = sk ? skill_range_max(sk) : 1;
        if (t->id == 0x14) {
            if (!alive(foe)) return;
            float len = cell_dist(a, foe);
            ux = (foe->x - a->x) / (len > 0 ? len : 1);
            uy = (foe->y - a->y) / (len > 0 ? len : 1);
        }
    }
    if (r < 0) {
        Actor *t1 = a->casting && !skill_needs_foe(a->cast) ? a : foe;
        skill_hit(a, t1, sk, tick);
        if (t1 && t1 != a) main_hit(a, t1, tick);
    } else {
        if (sk && sk->status > 0) skill_hit(a, NULL, sk, tick);  // 自己的狀態只加一次
        for (int i = 0; i < actor_count; i++) {
            Actor *e = &actors[i];
            float dx = e->x - cx, dy = e->y - cy, along = dx * ux + dy * uy;
            if (!hostile(a, e)) continue;
            if (ux || uy ? along < 0 || along > r + 0.5f || fabsf(dx * uy - dy * ux) > 1 : sqrtf(dx * dx + dy * dy) > r + 0.5f) continue;
            skill_hit(a, e, sk, tick);
            main_hit(a, e, tick);
        }
    }
    if (a->casting) a->casting = 2;
}

// 角色出手的觸發中跟出手的人有關的（0x43c4f0，來源是 MethodMain）：0 命中、15 結算、16 的 0x14/0x16 放分身、2 放單位（放出點用出手者動作的熱點）、
// 13 改幀延遲、8 動作旗標、11 離地、5/7 衝過去；其他（音效、震畫面、結束、16 改目標點）照招式單位的做。回傳 0 = 不是這裡的
static int owner_trig(MUnit *u, const Trig *t, Uint32 tick) {
    Actor *a = u->owner;
    if (t->type == 0 || t->type == 15) method_hits(a, t, tick);
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
    actor_animation_select(a, a->act_seq + DIR_MAP[a->dir]);
    actor_body_select(a);
    int seq = actor_seq(a);
    const Seq *q = role_seq(a->role, seq);
    int finished = !q || actor_body_done(a, q);
    int f = q || a->act != ACT_ATTACK ? actor_frame(a, seq) : 99;  // 角色沒有這招的動作（陷阱）：觸發一次全放
    if (a->act == ACT_ATTACK && a->atk) {  // 招式的觸發由角色出手物件（U_MAIN）放
        if (a->last_frame < 0 && !main_of(a)) main_new(a);
        MUnit *u = main_of(a);
        if (u) {
            u->x = ax64(a), u->y = ay64(a), u->frame = f;
            unit_base(u, tick);
        }
    }
    a->last_frame = f;
    // 先讓MethodMain消費末幀，再結束動作；延遲1的末幀也不能漏掉觸發。
    actor_animation_tick(a);
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
        // 攻擊間隔（Poq.exe 0x48d330）：NPC組用float攻速倍率，其餘組為間隔+隨機(間隔)。
        // shortcut: 仍於動作結束才裝冷卻，完整MethodMain狀態接入後改於綁定時裝入；NPC種類60的免間隔分支待物件種類接入。
        if (a->act == ACT_ATTACK && a->mon && a->skill) {  // 怪物打完一招：記成上一招，下次重選（0x4911b0）
            a->last_skill = a->skill;
            a->skill = NULL;
        }
        if (a->act == ACT_ATTACK) {
            a->cool = (grp_npc(a->grp) ? (int)((float)a->def->atk_delay * actor_fight_ratio(a)) : a->def->atk_delay + rnd(a->def->atk_delay)) + (a->extra > 0 ? a->extra : 0);
        }
        if (a->casting) {  // 左手技能放完：招式沒有打中的觸發就在這裡生效，換回右手招式
            if (a->casting == 1) skill_hit(a, skill_needs_foe(a->cast) ? a->foe : a, a->cast, tick);
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

static void cast_start(Actor *h, const SkillRow *r);

// 追打目標：在招式距離內就面向它出手，否則往它走。回傳 0 = 沒有目標
static int pursue(Actor *a, const Scene *s, Uint32 tick) {
    Actor *t = actor_target(a);
    if (!t) return 0;
    if (!alive(t)) { actor_clear_target(a); return 0; }
    actor_enter_fight(a);
    if (main_of(a)) return 1;
    float gx = t->x - a->x, gy = t->y - a->y;
    float sdx = (gx - gy) * 32, sdy = (gx + gy) * 16;  // 格子方向 → 畫面方向
    // 技能的作用範圍最大（RangeMax，原版送給 Poq.exe 的 0x524 訊息）決定走多近；怪物用招式的攻擊距離
    // 作用範圍最小（RangeMin）原版送進 Poq.exe 的 SkillUnit（+0x1c）後沒有讀，太近也照打
    if (a->mon && !a->skill && npc_skill(a->mon->skill[0])) {
        if (!(a->skill = mon_pick(a))) {  // 這次沒選到招：站著，下一拍再選
            a->moving = 0;
            return 1;
        }
        a->atk = method_find(skill_attack_id(a->skill));
    }
    const SkillRow *use = a->want ? a->want : a->skill;
    int range = use ? actor_row_range(a, use) : a->atk && a->atk->range_max > 0 ? a->atk->range_max : 1;
    // 拉開距離（0x4911b0）：每 61 拍一次，這招 Reserve3 ≠ 1、目標在 範圍−2 的方框內、n = 範圍 − 距離 − 2 ≥ 2 時，
    // 往遠離目標的 8 方向走 n 格（OmUtil GetFleePos = WhatDir8 + MoveDirForSpeed）
    if (a->mon && a->skill && ++a->ai_tick % 61 == 2 && a->atk && a->atk->ai != 1) {
        int dx = (int)a->x - (int)t->x, dy = (int)a->y - (int)t->y, n = range - (int)sqrtf((float)(dx * dx + dy * dy)) - 2;
        if (abs(dx) <= range - 2 && abs(dy) <= range - 2 && n >= 2 && (dx || dy)) {
            int oct = (int)lroundf(atan2f((float)dy, (float)dx) / (float)(M_PI / 4));
            static const int OX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, OY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            a->tx = (int)a->x + OX[(oct + 8) % 8] * n + 0.5f;
            a->ty = (int)a->y + OY[(oct + 8) % 8] * n + 0.5f;
            a->flee = 60;  // shortcut: 原版走到為止（動作 1），這裡最多走 60 拍，免得卡牆一直走
        }
    }
    if (a->flee > 0) {
        float fx = a->tx - a->x, fy = a->ty - a->y;
        if (--a->flee && fx * fx + fy * fy > 0.04f) {
            actor_tick(a, s, (fx - fy) * 32, (fx + fy) * 16, tick);
            return 1;
        }
        a->flee = 0;
    }
    if (cell_dist(a, t) <= range + 0.5f) {
        face(a, sdx, sdy);
        a->moving = 0;
        // 主角技能要夠精气才能用（0x1004c2a0），出手時扣（Poq.exe 0x48ffe0 送 0x13a → EasyRPG 0x1003f370）
        if (a->want) {  // 走到範圍內了：放等著的左手技能
            const SkillRow *w = a->want;
            a->want = NULL;
            cast_start(a, w);
        } else if (!actor_cooling(a) && !status_noattack(a) && (!a->skill || a->mon || skill_usable(a, a->skill))) {
            if (a->skill && !a->mon) a->mp -= skill_mpu(a->skill);  // shortcut: 怪物沒有精气，技能不扣也不檢查
            start_act(a, ACT_ATTACK, a->atk ? a->atk->seq : a->fidle);
        }
        return 1;
    }
    actor_tick(a, s, sdx, sdy, tick);
    return 1;
}

// 怪物：主角進入索敌范围就追上去打，離出生點超過追击范围就放棄回去閒逛
static void monster_think(Actor *a, Actor *hero, const Scene *s, Uint32 tick) {
    (void)hero;
    Actor *owner = actor_owner(a);
    if (owner && alive(owner)) {  // 主人那邊的：閒逛、追击范围都以主人為中心
        mon_specs[a - actors].rx = (int)owner->x - MON_WANDER;
        mon_specs[a - actors].ry = (int)owner->y - MON_WANDER;
    }
    if (a->foe && (!alive(a->foe) || fabsf(a->x - a->spec->rx - MON_WANDER) + fabsf(a->y - a->spec->ry - MON_WANDER) > actor_chase(a)))
        a->foe = NULL;
    // 暫時不可選取的目標只讓getter回空，不清掉其識別。
    Actor *target = actor_target(a);
    if (target && actor_flags(target, 0x4000)) actor_clear_target(a);
    // shortcut: 陷阱的觸發系統尚未移植，仍以技能範圍作明確半徑；完成陷阱系統時移除。
    const SkillRow *ts = a->mon && a->mon->istrap == 1 ? npc_skill(a->mon->skill[0]) : NULL;
    int radius = !actor_seek(a) && ts ? skill_range_max(ts) : -1;
    Actor *near = actor_target(a) ? NULL : actor_near_begin(a, 0, radius);
    if (near) actor_target_set(a, near);
    if (!pursue(a, s, tick)) npc_think(a, s, tick);
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

#ifdef STORY
// UTF-8 → GBK（劇情檔的字串是 UTF-8，原版資料表是 GBK）；查表找不到的字變 '?'
static void utf8_gbk(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *q = in; *q && o + 3 < n;) {
        uint32_t c = utf8_next(&q);
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
// shortcut: 開局兩手都是斩棘披荆（第 0 個，左右鍵位 2）是猜的，原版開新遊戲怎麼設還沒找到
static int cur_skill[2];

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
    int k = cur_skill[0];
    actor_skills_rebuild(h);
    // shortcut: 玩家技能列用index+1作外部識別，保留0給原模板；完整技能物件接入後換回句柄。
    for (int i = 0; i < hero_skill_n; i++) actor_skill_append(h, &hero_skill[i], i + 1);
    actor_right_skill(h, k >= 0 && k < hero_skill_n ? skill_attack_id(&hero_skill[k]) : -1);
    int left = cur_skill[1];
    actor_left_skill(h, left >= 0 && left < hero_skill_n ? skill_attack_id(&hero_skill[left]) : -1);
    hero_selected_skill_apply(h);
}

// 放左手技能（原版右鍵 → Poq.exe 0x48ffe0 手 = 1）：技能等級 > 0、精气夠、武器合、沒有「攻击不能」的狀態，招式開始才扣精气。
// 要對著敵人的技能要有目標；不在作用範圍最大內就先走過去（pursue），到了再放
static void cast_start(Actor *h, const SkillRow *r) {
    const Method *m = method_find(skill_attack_id(r));
    if (h->act || h->casting || !m || !actor_row_enabled(h, r) || !actor_can_use_id(h, skill_attack_id(r)) || !skill_usable(h, r) || status_noattack(h)) return;
    h->mp -= skill_mpu(r);
    h->cast = r;
    h->casting = 1;
    h->atk_rh = h->atk;
    h->atk = m;
    start_act(h, ACT_ATTACK, m->seq);
}

// 共用現有技能結算與動畫，明確傳遞世界座標及對象至MethodMain。
// shortcut: 原版施法入口的同步及完整戰鬥狀態副作用仍待主命令接齊。
// shortcut: 沒有EasyRPG技能列的模板招式仍可播放與觸發，沒有額外技能數值結算。
static int actor_cast_try(Actor *a, int id, int x, int y, Actor *target) {
    int i = actor_skill_find(a, id);
    if (i < 0) return 0;
    ActorSkill *s = &a->skills[i];
    const Method *m = method_find(s->id);
    if (!m || !s->enabled || (a->def->speed && a->speed <= 0 && s->max >= 3)) return 0;
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
    a->act = ACT_NONE; a->atk = m;
    a->cast = r; a->casting = 1;
    start_act(a, ACT_ATTACK, m->seq);
    a->atk_rh = right;
    a->aim_valid = 1; a->aim_x = x; a->aim_y = y;
    a->aim_actor = target; a->aim_range = actor_skill_range(a, s);
    if (target) actor_target_set(a, target);
    main_new(a);
    return 1;
}

// 施法入口的記住命令收尾：失敗也執行；不同技能且仍有敵對角色目標時保留。
// shortcut: 單機模式2；完整服務端入口接入時補非模式2的無條件清除分支。
static int actor_cast_at(Actor *a, int id, int x, int y, Actor *target) {
    int result = actor_cast_try(a, id, x, y, target);
    if ((a->remembered[0] == 8 || a->remembered[0] == 9) &&
        (a->remembered[1] == id || !a->foe || !is_foe(a, a->foe)))
        actor_forget_command(a);
    return result;
}

// Poq.exe 0x4861e0：第一個正ID且查得到SkillUnit的候選即停止，施法失敗也不試後面的。
static void actor_candidate_cast(Actor *a) {
    for (int i = 0; i < 4; i++) {
        int id = a->candidate[i];
        if (id <= 0 || actor_skill_find(a, id) < 0) continue;
        Actor *t = actor_target(a);
        actor_cast_at(a, id, ax64(t ? t : a), ay64(t ? t : a), t);
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
            actor_cast_at(a, s->id, x * 64 + 32, y * 64 + 32, NULL);
        }
        return 1;
    }
    return 0;
}

static void hero_cast_point(Actor *h, int x, int y, Actor *target) {
    int k = cur_skill[1];
    if (k < 0 || k >= hero_skill_n || h->act || h->casting) return;
    const SkillRow *r = &hero_skill[k];
    int manual = skill_manual(r);
    if (manual == 2 || manual == 3) {
        // shortcut: 技能命令8的世界座標由既有右鍵入口橋接；完整分派接入後共用命令入口。
        h->remembered[0] = 8; h->remembered[1] = skill_attack_id(r);
        h->remembered[2] = x; h->remembered[3] = y;
        if (actor_can_use_id(h, skill_attack_id(r)) && skill_usable(h, r))
            actor_cast_at(h, skill_attack_id(r), x, y, target);
        return;
    }
    if (!skill_needs_foe(r)) { cast_start(h, r); return; }
    if (!alive(h->foe) || !skill_usable(h, r)) return;
    if (cell_dist(h, h->foe) <= actor_row_range(h, r) + 0.5f) cast_start(h, r);
    else h->want = r;
}

static void hero_cast(Actor *h) { hero_cast_point(h, ax64(h), ay64(h), h); }

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
static void hud_say(const char *t) { snprintf(toast, sizeof toast, "%s", t); toast_until = SDL_GetTicks() + 2500; }
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

static int bag_free(void) {
    for (int i = 0; i < 24; i++)
        if (!inv[i].name[0]) return i;
    return -1;
}

// 負重：身上道具重量合計，上限 BD_MAX = Str × 1.5 + 30。shortcut: 超重的效果還沒查
static int hero_burden(void) {
    int w = 0;
    for (int i = 0; i < INV_N; i++) w += inv[i].name[0] ? inv[i].weight : 0;
    return w;
}
static int hero_burden_max(const Actor *h) { return (int)((hero_attr[STR] + h->ex[27]) * 1.5f + 30); }

// 外觀：角色表的款式蓋上身上每件裝備的「該職業裝備串」（道具表 33~37 欄），重新載入主角的部件
static void hero_look(void) {
    const RoleDef *d = roledef_by_graphic(HERO_GRAPHIC);
    if (!d) return;
    hero_def = *d;
    for (int s = INV_BAG; s < INV_N; s++)
        if (inv[s].name[0] && inv[s].equip[hero_prof - 1][0])
            styles_equip(hero_def.styles, sizeof hero_def.styles, d->graphic, d->pli, inv[s].equip[hero_prof - 1]);
}
static void hero_refresh(Actor *h) {
    hero_derive(h);
    if (!h->role) return;  // 還沒進場景（開場）
    hero_look();
    uint32_t g[MAX_ROLE_PARTS * 2];
    res_fetch_now(g, role_anim_ids(hero_def.graphic, hero_def.pli, hero_def.styles, g, MAX_ROLE_PARTS * 2));
    actor_look_set(h, &hero_def);
}

// 能不能穿（0x1004c0e0）：等級、力量、體質、敏捷夠，耐久不是 0，已鑑定
static int item_can_equip(const Actor *h, const Item *it) {
    return it->part > 0 && h->level >= it->lv_req && hero_attr[STR] + h->ex[27] >= it->str_req && hero_attr[CON] + h->ex[28] >= it->con_req &&
           hero_attr[DEX] + h->ex[29] >= it->dex_req && it->hp != 0 && it->identify;
}

// 部位 → 裝備欄：1 頭盔、2 盔甲、3 單手（箭 13、14 放左手）、4 盾 = 左手、5 護腕、6 靴子、7 戒指（右手空著放右手）、8 項鍊、10 雙手 = 右手、11 腰帶
static int equip_slot(const Item *it) {
    switch (it->part) {
    case 1: return 30;
    case 2: return 31;
    case 3: return it->type == 13 || it->type == 14 ? 33 : 32;
    case 4: return 33;
    case 5: return 34;
    case 6: return 35;
    case 7: return inv[36].name[0] && !inv[37].name[0] ? 37 : 36;
    case 8: return 38;
    case 10: return 32;
    case 11: return 39;
    }
    return -1;
}

static void inv_swap(int a, int b) {
    Item t = inv[a];
    inv[a] = inv[b];
    inv[b] = t;
    inv[a].pos = a;
    inv[b].pos = b;
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

// 穿上背包 p 的道具，原來那格的換回背包。雙手武器要空出左手（箭除外），左手放東西時右手不能是雙手武器
// shortcut: 原版 Poq.exe 拖放裝備的檢查（哪些部位能互放、弓配箭）還沒拆，照部位表推的
static void item_equip(Actor *h, int p) {
    Item *it = &inv[p];
    int s = equip_slot(it);
    if (s < 0) return;
    if (!item_can_equip(h, it)) { hud_say(!it->identify ? "需要先鑑定" : it->hp == 0 ? "已經損壞，需要修理" : "條件不足，不能裝備"); return; }
    if (s == 32 && it->part == 10 && inv[33].name[0] && inv[33].type != 13 && inv[33].type != 14 && !item_unequip(h, 33)) return;
    if (s == 33 && inv[32].name[0] && inv[32].part == 10 && !(it->type == 13 || it->type == 14) && !item_unequip(h, 32)) return;
    inv_swap(p, s);
    item_sel = -1;
    hero_refresh(h);
}

// 吃（0x10016be0）：要已鑑定；把特性套到身上後道具消失。食物 21 = 生命、292 = 生命%、22 = 精氣、293 = 精氣%；
// 書 27~33 = 永久加力量、体质、敏捷、（30 不用）悟性、智慧、魅力
// shortcut: can_eat 在原版是物件模板設的，沒找到設的地方，先當食物（51）和書（49）能吃
static int item_eat(Actor *h, int p) {
    Item *it = &inv[p];
    if (it->type != 51 && it->type != 49) return 0;
    if (!it->identify) { hud_say("需要先鑑定"); return 1; }
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

// 修理（0x10054090）：自己的修缮之术，道具等級 ≤ 技能等級 × 3；價錢 = 原價 × 損耗 × 4 / 耐久上限（找 NPC 修 ×2）。
// 箭 13、14，29、30 和 7~34 以外的不能修。shortcut: NPC 修理要商店介面，還沒做
static void item_repair(int p) {
    Item *it = &inv[p];
    int lv = hero_skill_level("\xd0\xde\xc9\xc6\xd6\xae\xca\xf5");  // 修缮之术（GBK）
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
    int lv = hero_skill_level("\xbb\xdb\xd1\xdb\xca\xb6\xbd\xf0"), mp = (it->level + 4) * 10;  // 慧眼识金（GBK）
    if (it->identify) { hud_say("不需要鑑定"); return; }
    if (it->level > lv * 3) { hud_say(lv ? "慧眼识金等級不夠" : "需要慧眼识金"); return; }
    if (h->mp < mp) { hud_say("精氣不足"); return; }
    h->mp -= mp;
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
    int cx = (int)x, cy = (int)y;
    if (!free_cell(&cx, &cy, 10)) return;  // CPropObj 放進場景（0x49a350）：周圍 10 圈第一個空格，沒有就不放
    ground[ground_n].x = cx + 0.5f;
    ground[ground_n++].y = cy + 0.5f;
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

// 撿起來：錢直接加到金錢（金钱 = 類型 60，數量在 Amount），其他放進背包空格
static void ground_pick(int g) {
    Item *it = &ground[g].it;
    if (it->type == 60) hero_money += it->amount;
    else {
        int b = bag_free();
        if (b < 0) { hud_say("背包已滿"); return; }
        inv[b] = *it;
        inv[b].pos = b;
    }
    ground[g] = ground[--ground_n];
}

// 按鈕名稱 → 開的視窗（主列和人物屬性視窗裡的 m_pRole／m_pSkill／m_pProp 同名同作用）
static void sys_toggle(void);
static void ui_text(int id, char *out, size_t cap);
static void ui_command(const char *name) {
    if (!strcmp(name, "m_pRole")) win_toggle(&win_left, JM52);
    else if (!strcmp(name, "m_pSkill")) { skill_mode = 1; win_toggle(&win_right, JM53); }
    else if (!strcmp(name, "m_pProp")) win_toggle(&win_right, JM54);
    else if (!strncmp(name, "m_pPage[", 8)) skill_page = atoi(name + 8) - 1;  // 技能頁籤：m_pPage[1]~[5] = 第 0~4 頁（[6] 藏著）
    else if (!strncmp(name, "m_pSkill[", 9)) pick_open(name[9] == '1');  // 主列 m_pSkill[0] 右手、[1] 左手
    else if (!strcmp(name, "m_pEquip")) win_toggle(&win_left, JM55);
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

// 走過去撿：到了（一格內）就撿起來
static void ground_walk(Actor *h, const Scene *s, Uint32 tick) {
    float gx = ground[ground_want].x - h->x, gy = ground[ground_want].y - h->y;
    if (gx * gx + gy * gy < 1.0f) {
        h->moving = 0;
        ground_pick(ground_want);
        ground_want = -1;
        return;
    }
    actor_tick(h, s, (gx - gy) * 32, (gx + gy) * 16, tick);
}

// 道具視窗（JM54）的格子和按鈕。點道具 = 選取，再點一次 = 使用／裝備；選了以後點空格 = 移過去
// shortcut: 原版用滑鼠拖放、右鍵使用，手機改成點選；打造（m_pMake）、镶嵌（m_pEnchase）、查看（m_LookOver）另外的系統，還沒做
#ifdef STORY
static int shop_bag_act(int p);
static int store_bag_act(int p);
#endif
static void bag_use(int p) {
    if (inv[p].part > 0) item_equip(&actors[0], p);
    else if (!item_eat(&actors[0], p)) hud_say("不能使用");
}
static void bag_button(const char *name) {
    int p = !strncmp(name, "m_pItem[", 8) ? atoi(name + 8) : !strncmp(name, "m_pShortcutItem[", 16) ? 24 + atoi(name + 16) : -1;
    int sel = item_sel >= 0 && inv[item_sel].name[0] ? item_sel : -1;
    if (p >= 0) {
#ifdef STORY
        if (p == sel && (shop_bag_act(p) || store_bag_act(p))) return;
#endif
        if (p == sel) bag_use(p);
        else if (inv[p].name[0]) item_sel = p;
        else if (sel >= 0) { inv_swap(sel, p); item_sel = p; }
    } else if (!strcmp(name, "m_pNeaten")) bag_neaten();
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
// 頁籤（0x368）：m_pWeapons 3、m_pShield 2、m_pLeechdom 1、m_pOther 0，開的時候是 3（0x55aed0）。
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

// 買賣價（EasyRPG 0x10029400）：沒鑑定的用 ValueUn。c = 主角魅力、m = 隊伍平均魅力（只算 Friend > 1 的隊友，現在只有主角）。
// 賣 = 價 × (c+20) × (m+20) / (c+50) / (m+50)；買 = 價 × (c+50) × (m+50) / (c+20) / (m+20)，沒鑑定的買價再 ×10；上限 99999999
static int trade_price(const Item *it, int buy) {
    float c = (float)(hero_attr[CHARM] + actors[0].ex[33]), m = c, v = (float)(it->identify ? it->value : it->value_un);
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

static void shop_close(void) {
    shop_npc = 0;
    shop_mode = SHOP_NONE;
    if (win_left.open && (!strcmp(win_left.d.name, JM63) || !strcmp(win_left.d.name, JM56))) win_left.open = 0;
}

// 買（EasyRPG buy 0x10012ef0）：錢不夠秀 1185；錢付給商人，道具放進背包空格
static void shop_buy(int i) {
    if (i < 0 || i >= shop_n) return;
    Item *it = story_item(shop_slot[i]);
    int p = trade_price(it, 1), b = bag_free();
    if (hero_money < p) { hud_msg(1185, p, hero_money); return; }
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
static void make_open(int mode);
static void ui_text(int id, char *out, size_t cap);
static void item_icon(UiDialog *d, const char *key, int wx, int wy, const Item *it);
static int shop_bag_act(int p) {
    if (shop_npc && p >= 0 && p < INV_BAG && inv[p].name[0] && make_on()) return make_bag_act(p);
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
    } else if (!strcmp(name, "m_pMake") || !strcmp(name, "m_pEnchase")) make_open(name[3] == 'M' ? -1 : 1);
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
// shortcut: 自己用技能打造（扣精氣 (等級+10)×5）、五行屬性換算（0x10058900）、打造中的進度動畫 m_pProductFinish 還沒做；
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

// 合併一件材料的特性（0x10051530：同屬性、同種類、同百分比的相加）
static void fx_merge(ItemFx *list, int *n, const ItemFx *f) {
    for (int i = 0; i < *n; i++)
        if (list[i].id == f->id && list[i].kind == f->kind && list[i].pct == f->pct) { list[i].v += f->v; return; }
    if (*n < 32) list[(*n)++] = *f;
}

static void make_produce(void) {
    const Recipe *r = make_recipe();
    if (!r) return;
    int cost = 0, first = -1;
    for (int c = 0; c < 5; c++)
        for (int k = 0; k < r->need[c]; k++) {
            int p = make_mat[k * 5 + c];
            if (p < 0 || !inv[p].name[0]) return;  // 材料還沒放齊（原版打造鈕是灰的）
            cost += inv[p].value_un;
        }
    if (r->main >= 1 && r->main <= 5) first = make_mat[r->main - 1];
    if (hero_money < cost) { hud_msg(3025); return; }
    Item out;
    int level = story_get_int(shop_npc, "UIS_Produce");
    if (!item_new(&out, r->type, level > 0 ? level : 1, -1, NULL)) { hud_msg(3014, r->type); return; }
    hero_money -= cost;
    if (out.type >= 7 && out.type <= 34) {
        if (first >= 0 && inv[first].type == r->main) out.fe = inv[first].fe;
        ItemFx add[32];
        int n = 0;
        for (int i = 0; i < 15; i++) {
            const Item *m = make_mat[i] >= 0 ? &inv[make_mat[i]] : NULL;
            if (!m) continue;
            for (int j = 0; j < m->fx_n; j++)
                if (m->fx[j].kind != 3 || m->type == r->main) fx_merge(add, &n, &m->fx[j]);
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
        if (w->fx[i].gem == k + 1) continue;
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
    } else if (!strcmp(name, "m_pCClose")) shop_open(shop_npc);
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
static void story_exp(int h, int exp) { if (h == story_player()) hero_gain_exp(&actors[0], exp); }
static void story_log(int h, int kind, const char *text) { (void)kind; if (h == story_player()) hud_say(text); }

// 劇情和引擎共用主角的錢、身上的東西。叫劇情前：錢給它，inv 的每件道具放回主角的物件身上（find_item 找得到、
// move_object 拿得走）；叫完：拿回錢，劇情新放到主角身上的道具（add_item、give_object、move_object）收進 inv：
// 有 UI_Pos 且那格空著就放那格（開場的扶苏武器 32、布衣 31），否則背包空格，滿了掉在腳下；被劇情拿走的從 inv 移除。
// 劇情叫出買賣介面就打開
static void story_msgs(void);
static void story_begin(void) {
    int pl = story_player();
    story_set_int(pl, "Money", hero_money);
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

// 0x100757a0：依技能表設技能物件（陷阱 NPC 的技能用）。shortcut: 技能的效果（0x10037490）由引擎的技能表直接算，這裡只放屬性
static int story_skill(int h, const char *key, int level) {
    static SkillRow rows[512];
    static int n = -1;
    if (n < 0) {
        n = skill_rows_file("skill_npc.txt", rows, 256);
        for (int p = 1; p <= 5; p++) n += skill_rows(p, rows + n, 512 - n);
    }
    for (int i = 0; i < n; i++) {
        const SkillRow *r = &rows[i];
        if (strcmp(r->key, key)) continue;
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
    for (int h = story_next(sroom, 0); h && actor_count < MAX_ACTORS; h = story_next(sroom, h)) {
        if (h == pl || story_kind(h) != 1 || actor_of(h)) continue;
        const MonDef *m = mondef_find(story_get_str(h, "MonRow"));
        const RoleDef *d = roledef_find(story_get_int(h, "G_PicID"));
        if (!m || !d || (!atoi(d->graphic) && !d->anim)) continue;  // shortcut: 圖形 ID 是名字的怪物（蝙蝠、骷髏、蜘蛛）還沒做
        if (story_get_int(h, "G_PosX") != -100) continue;  // 劇情 NPC（ONT 擺的）由 spawn_npcs 放
        // 劇情建角色（0x44fd20 種類 0、G_PosX −100）：同名入口點隨機挑一個（0x4cc9c0，第 n 個以 1/n 換掉），面向入口方向
        // 入口格交角色場景綁定／占位，依原版邊圈順序找空格。
        int n = 0, pick = -1, area = story_get_int(h, "G_PosY");
        for (int i = 0; i < s->entry_count; i++)
            if (s->entries[i].id == area && rand() % ++n == 0) pick = i;
        if (pick < 0) continue;
        const Entry *en = &s->entries[pick];
        int x = en->x, y = en->y;
        Actor *a = mon_make(m, d, x, y, en->x, en->y, story_get_int(h, "Level"));
        a->sh = h;
        actor_person_init(a, h, story_get_int);
        for (int k = 0; k < 8; k++)
            if (DIR_MAP[k] == en->dir) a->dir = k;
        actor_group(a, story_get_int(h, "G_AIGroup"));
        a->side = !scene_client_group(&scene_client, a->grp);  // side 是我們自己的「主角這邊」，敵對另看 grp
    }
}

// 怪物倒下：原版 0x100574e0 最後呼叫第 10 格再刪掉（Boss 的第 10 格開劇情）
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
        case 0x535: if (a) a->ex[20] = (int)m->v[1]; break;                 // Invincible
        case 0x52a: if (a) actor_weapon_type_set(a, (int)m->v[1]); break;
        case 0x51c:  // G_AIGroup
            a = character_of((int)m->v[0]);
            if (a) actor_group(a, (int)m->v[1]);
            if (a && a != &actors[0]) a->side = m->v[1] != story_get_int(story_player(), "G_AIGroup");
            break;
        case 0x574e0: if (a && alive(a)) actor_kill(a, NULL); break;        // 劇情讓人物死掉
        case 0x42aa0: if (a && a != &actors[0]) a->hp = story_get_int(a->sh, "HP"); break;
        case 0x52e: story_plot_end(); break;  // shortcut: 過場播放器還沒做（清單 D），先直接結束（第 18 格）
        case 0x47870:
            if (a && m->s[0] == '@') actor_attribute_changed(a, m->s + 1, story_get_int(a->sh, m->s + 1));
            break;
        case 0x51b:
            a = character_of((int)m->v[0]);
            // 原版僅第三參數>0時寫第二參數；0及負數只改狀態103。
            // shortcut: 狀態103的設定／解除仍待完整角色狀態入口，
            // 接入後補齊本分支；目前只接原版獨立的關聯識別setter。
            if (a && m->v[2] > 0) actor_search_link_set(a, (int)m->v[1]);
            break;
        case 0x52c: case 0x519: break;  // 出口開關（走出口時再查）、轉向
        default: fprintf(stderr, "劇情訊息 0x%x（%s）還沒做\n", m->id, m->s); break;
        }
    }
    smsg_n = 0;
}
#endif

// ---- 存檔（原版 JM50_内部主界面 的保存、读取 → JM62_存取盘界面，Poq.exe CSystemDialog 0x5354a0、CLoadDialog 0x56c400）----
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
static int sys_open, sav_mode, sav_page, sav_sel = -1, ask_open, quit_open;  // sav_mode：1 保存、2 读取
static SaveHead *sav_head[SAVE_SLOTS];   // 開視窗時讀的各格檔頭，NULL = 空格
static SDL_Texture *sav_tex[6];          // 這一頁的縮圖
static uint32_t snap[THUMB_W * THUMB_H]; // 開系統選單前的畫面（原版也是先截圖，0x535ec0）
static int snap_due, save_due = -1, load_due = -1, quit_due;
static char save_alias[64], save_name[64];  // 目前場景的別名、房間名（GBK）

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

// 畫面縮成縮圖（開系統選單、自動存檔、快速存檔前在畫介面之前截）
static void snap_take(SDL_Texture *canvas) {
    uint32_t *px = malloc((size_t)vw * vh * 4);
    if (!px) return;
    SDL_SetRenderTarget(ren, canvas);
    if (!SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGBA32, px, vw * 4))
        for (int y = 0; y < THUMB_H; y++)
            for (int x = 0; x < THUMB_W; x++) snap[y * THUMB_W + x] = px[(y * vh / THUMB_H) * vw + x * vw / THUMB_W];
    free(px);
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
    story_save(f, sizeof(Item));
#endif
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
    shop_close();
    dialog_open = 0;
#endif
    fclose(f);
    memcpy(inv, tmp, sizeof inv);
    memcpy(hero_attr, s.attr, sizeof hero_attr);
    hero_extra_exp = s.extra_exp;
    hero_extra_skill = s.extra_skill;
    hero_money = s.money;
    hero_prof = s.prof;
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
    // shortcut: 设置、帮助（JM31、JM36）還沒做
}

static void sys_toggle(void) {
    if (sys_open) { sys_open = 0; sav_close(); quit_open = 0; return; }
    if (!ui_load(JM50, &sys_d)) return;
    UiCtrl *q = ui_find(&sys_d, "m_pQuit");
    if (q) q->flags |= 9;  // 單機版顯示「退出」（保存并退出 是網路版的）
    sys_open = 1;
    snap_due = 1;
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
    if (sys_open) {
        const UiCtrl *c = ui_button_at(&sys_d, (vw - sys_d.w) / 2, (vh - sys_d.h) / 2, x, y);
        if (c) sys_button(c->name);
        return 1;
    }
    return 0;
}

static void sys_draw(void) {
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
                gbk_utf8(h->name[0] ? h->name : h->alias, u, sizeof u);
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
    if (sys_tap(x, y)) return 1;
    if (pick_hand >= 0) {  // 快捷技能視窗開著：點格子換技能；點哪裡都收起來
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
        else if (c && !strcmp(w->d.name, JM55) && !strncmp(c->name, "m_pItem[", 8)) item_unequip(&actors[0], INV_BAG + atoi(c->name + 8));
        else if (c) ui_command(c->name);
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
static void bag_draw(UiDialog *d, int wx, int wy) {
    char key[24], t[1024];
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

// 敵人狀態框顯示誰（0x459ce0；0x455d60 每拍拿游標下的物件呼叫它，0x459fa0 是先依編號找角色）：
// 指著角色（種類 0x1e～0x64：怪物、NPC、自己都算）就開框、填名字（+0x589；舊索引曾與生命百分比+0x507反寫）和血條，沒指著角色就關框。
// 照原版：血條只在對象沒有狀態 102 時「打開」（0x583e40 傳 0 = 不變），這裡也沒有地方關它，所以有狀態 102 一樣看得到血。
// shortcut: 隊伍多選名單與指著非角色物件的標示，等對應介面接入；角色腳圈已接繪製。
// 手機沒有指著，先用攻擊目標；死掉的不顯示（原版只看種類）

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
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xd0\xbc\xe4", &bar);      // JM51_状态界面_中间（GBK）
        ui_load("JM51_\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6_\xd6\xf7\xbf\xd8", &party);    // JM51_状态界面_主控
        ui_load("JM515_\xb5\xd0\xc8\xcb\xd7\xb4\xcc\xac\xbd\xe7\xc3\xe6", &enemy);    // JM515_敌人状态界面
    }
    // 敵人狀態：上方置中、距頂 20（原版位置）
    Actor *t = status_target(hero, hover_actor, hover_mouse);
    if (enemy.count && t) {
        int ex = (vw - enemy.w) / 2, ey = 20;
        UiCtrl *nm = ui_find(&enemy, "m_pName"), *bl = ui_find(&enemy, "m_pBlood");
        if (nm) {
            char u[96];
            actor_title(t, u, sizeof u);
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
        // 裝備：圖示畫在 m_pItem[UI_Pos − 30]
        for (int i = INV_BAG; !strcmp(win_left.d.name, JM55) && i < INV_N; i++) {
            char slot[16];
            snprintf(slot, sizeof slot, "m_pItem[%d]", i - INV_BAG);
            item_icon(&win_left.d, slot, wx, wy, &inv[i]);
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
    if (SDL_GetTicks() < toast_until) {
        int tw = text_width(toast);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
        SDL_Rect r = {(vw - tw) / 2 - 8, 56, tw + 16, 22};
        SDL_RenderFillRect(ren, &r);
        draw_text((vw - tw) / 2, 59, toast, tw + 1, 0xFFE080, 1);
    }
    sys_draw();
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
        if (d && d->beast && d->air && d->id < 1500) id_add(&anims, d->anim);
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
        if (!d || !d->beast || !d->air || d->id >= 1500) continue;
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
    scene_client.ready = 0;
    actor_behavior_reset(hero);
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
    cache_clear();
    res_cache_clear();
    // 外觀快取跟著場景換：先留主角的，再重建 NPC
    const RoleDef *hd = hero_def.id ? &hero_def : roledef_by_graphic(HERO_GRAPHIC);
    roles_clear();
    preload_roles(scene, hd);
    if (hd) actor_setup(hero, hd);
#ifdef STORY
    // 模板會直接覆寫組別；第一拍索敵前便恢復劇情玩家組，不等資源載完的actors_sync。
    actor_group(hero, story_get_int(story_player(), "G_AIGroup"));
#endif
    spawn_npcs(scene);
    ground_n = 0;  // 地上的道具不跟著換場景
    ground_want = walk_cx = -1;
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
    scene_client.ready = 1;
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

// 0x455d60 算游標那段：c = 游標下的角色、item = 指著的地上道具、(cx, cy) = 游標那格。
// 角色是自己（原版看自己這邊的名單）就往下看道具和格子；沒指著角色、格子主角那層不能過（0x435c00）或有角色站 → 5
// shortcut: 原版只在格子 +0x13 ≠ 0 時才看能不能走（+0x13 意義沒查），這裡都看；主角的層當 1（一般地面）；
// 箱子、可破壞、門還不是物件，沒有 3、4、7；拖拉選人框時是 0；游標 1 時原版秀道具名字（訊息 0x143／0x145）沒做；游標在介面上時原版是 0
static int input_cursor(const Actor *h, const Actor *c, int item, int cx, int cy) {
    if (c && c != h) {
        if (in_fight(h) || is_foe(c, h)) return CUR_ATTACK;
        return has_status(h, 103) ? CUR_NORMAL : CUR_TALK;  // 103 无对话
    }
    if (item >= 0) return CUR_ITEM;
    Object *b = NULL;
    const Cell *bc = cell_at(cx, cy);
    if (!bc && unit_scene) b = input_building(unit_scene, cx, cy);
    else if (bc && bc->view >= 2 && (bc->flags & 0x88) == 0x88 && bc->building >= 0 &&
             bc->building < unit_scene->object_count) {
        Object *o = &unit_scene->objects[bc->building];
        if (o->m && (!o->tri || building_available(o))) b = o;
    }
    if (b) {
        if (b->m->type == 0x8c) return CUR_ITEM;
        if (b->m->type == 0x80) return CUR_ATTACK;
        if (b->m->type == 0x81) return b->state == 2 ? CUR_OBJ : CUR_USE;
        if (b->m->type == 0x85) return CUR_CHEST;
    }
    const Cell *cl = cell_at(cx, cy);
    if (!c && cl && (!cell_pass(cl, 1) || cell_actor0(cx, cy))) return CUR_BLOCK;
    return CUR_NORMAL;
}

// 左鍵放開（0x454f00）要給主角的指令（原版包成 0xf023 訊息送給主角和選到的隊友）。
// 溃散（17）、盅惑人心（14）不收指令；战斗状态點誰都打（9），平常點敵人打、點其他人對話（0xf）；再來是撿道具（0x12），其他 = 走過去（1）
// shortcut: 拖拉選人框、Ctrl 對地點出手（8）、物件（0x80 打、0x85 開、0x96 門 0x10、0x17 → 0x11）還沒做
static int click_cmd(const Actor *h, const Actor *c, int item) {
    if (has_status(h, 17) || has_status(h, 14)) return CMD_NONE;
    if (c && c != h) return in_fight(h) || is_foe(c, h) ? CMD_ATTACK : CMD_TALK;
    return item >= 0 ? CMD_PICK : CMD_MOVE;
}

// 遊戲速度鍵（0x455d60，連線客戶端不改）：數字鍵盤 + 或 = 加快（值 −3，最小 16）、數字鍵盤 − 或 - 減慢（值 +3，最大 31），
// 再換回速度 5 − (值 − 16) / 3 設定（0x448a70）。怪現象（照原版）：開局 21 按一次加快 → 18 → 速度 5 → 16（最快）
static void game_speed_key(int faster) {
    int v = game_speed_ms;
    v = faster ? (v < 17 ? 16 : v - 3) : (v < 31 ? v + 3 : 31);
    game_speed_set(5 - (v - 16) / 3);
}

// 點地面後走過去（指令 1）：直線走到格子中央。shortcut: 原版指令 1 的走法（尋路）在 CCharactor，還沒做
static void point_walk(Actor *h, const Scene *s, Uint32 tick) {
    float gx = walk_cx + 0.5f - h->x, gy = walk_cy + 0.5f - h->y;
    if (gx * gx + gy * gy < 0.25f) { h->moving = 0; walk_cx = -1; return; }
    actor_tick(h, s, (gx - gy) * 32, (gx + gy) * 16, tick);
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
    int pos = 0, warp = -1, at_x = -1, at_y = -1, level = 1, gives = 0, money = -1;
    const char *give[8];
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--640")) { vw = 640; vh = 480; }
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--warp") && i + 1 < argc) warp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &at_x, &at_y);
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--money") && i + 1 < argc) money = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--item") && i + 1 < argc && gives < 8) give[gives++] = argv[++i];
        else if (!strcmp(argv[i], "--prof") && i + 1 < argc) hero_prof = atoi(argv[++i]) < 1 || atoi(argv[i]) > 5 ? 1 : atoi(argv[i]);
        else if (pos++ == 0) game = argv[i];
        else scn = argv[i];
    }
    res_init(game);
    save_mount();
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
#ifdef STORY
    story_item_make = story_item_new;
    story_on_exp = story_exp;
    story_on_log = story_log;
    story_on_person = story_person;
    story_on_skill = story_skill;
    story_on_msg = story_msg;
    story_end();  // 開場裝備照劇情的主角（扶苏武器、布衣）
    hero_look();
#else
    hero_equip();
#endif
    for (int i = 0; i < gives; i++) {  // 測試用：--item 類型,等級 放一件到背包
        int t = 0, lv = 1, b = bag_free();
        sscanf(give[i], "%d,%d", &t, &lv);
        if (b >= 0 && item_new(&inv[b], t, lv, -1, NULL)) inv[b].pos = b;
    }
    if (money >= 0) hero_money = money;  // 測試用 --money
    Scene scene = {0};
    Actor *hero = &actors[0];
    hero->dir = 4;
    if (!enter_scene(&scene, hero, alias, -1)) return 1;
    if (!hero->role) { fprintf(stderr, "載入主角失敗\n"); return 1; }
    hero_stats(hero, level < 1 ? 1 : level > 99 ? 99 : level);
    hero_extra_exp = 10 * (hero->level - 1), hero_extra_skill = hero->level - 1;  // 測試用 --level：當成從 1 級升上來，點數照原版給
    int room = -1, armed = 0;  // armed：離開觸發區後才會再觸發，避免一進場就被送回去
    int monsters_due = 1;      // 進場景後等圖片都下載好，再讀房間表放怪物（網頁版開場不用等 4MB 的 DLL）
    if (at_x >= 0) actor_restore_position(hero, at_x + 0.5f, at_y + 0.5f);
    if (warp >= 0 && warp < scene.trigger_count) {
        actor_restore_position(hero, scene.triggers[warp].x + 0.5f, scene.triggers[warp].y + 0.5f);
        armed = 1;
        printf("站到觸發區 %d：%s\n", warp, scene.triggers[warp].event);
    }

    if (at_x >= 0 || warp >= 0) {
        viewport_home(&viewport, &scene, hero, 0);
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
    SDL_Texture *zoom_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, vw, vh);  // 縮放時的暫存
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
    int frames = 0, running = 1, pointer_down = 0, click_down = 0, px = 0, py = 0, ox = 0, oy = 0, mouse_seen = 0;
    SDL_ShowCursor(SDL_DISABLE);  // 0x433dc0：關掉系統游標，畫原版游標動畫 0x100011

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
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
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                px = e.button.x;
                py = e.button.y;
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
                pointer_down = !input_char(zx, zy, ox, oy, &scene, &cx, &cy) && ground_at(zx, zy, ox, oy, &scene) < 0;
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && click_down) {
                click_down = pointer_down = 0;
                if (SDL_GetModState() & KMOD_ALT) continue;  // 0x454f00：Alt 按著不下指令
                int zx = e.button.x, zy = e.button.y, cx, cy;
                zoom_point(&zx, &zy);
                Actor *m = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
                int g = ground_at(zx, zy, ox, oy, &scene);
                switch (click_cmd(hero, m, g)) {
                case CMD_ATTACK: actor_target_set(hero, m); hero->want = NULL; ground_want = walk_cx = -1; break;
                case CMD_PICK: ground_want = g; hero->foe = NULL; hero->want = NULL; walk_cx = -1; break;
                case CMD_MOVE:
                    // shortcut: 目前輸入橋接記住命令1；完整主分派接入後移到共用入口。
                    hero->remembered[0] = 1;
                    hero->remembered[1] = cx; hero->remembered[2] = cy; hero->remembered[3] = 0;
                    mark_new(cx, cy); hero->foe = NULL; hero->want = NULL; ground_want = -1;
                    walk_cx = hero->fight && actor_move_cast(hero, (unsigned short)cx, (unsigned short)cy) ? -1 : cx;
                    walk_cy = cy;
                    break;
                case CMD_TALK: {  // shortcut: 原版指令 0xf 是走過去再說；這裡馬上說，而且只有 ONT 擺的（有事件 ID）的 NPC 能說
                    actor_forget_command(hero);
#ifdef STORY
                    const char *event = actor_init_name(m);
                    if (!m->mon && event && *event) {
                        int npc = story_find(story_room(alias), event);
                        if (npc) { Actor *controlled = scene_client_actor(&scene_client); if (controlled) talk_face(controlled, m); story_begin(); story_talk(npc); story_end(); dialog_open = 1; }
                        else printf("劇情裡找不到 NPC %s\n", event);
                    }
#endif
                    break;
                }
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
            // 空白鍵／手把 A：打最近的怪物
            else if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) ||
                     (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A)) {
                float best = hero->def ? actor_seek(hero) : 10;
                for (int i = 1; i < actor_count; i++)
                    if (hostile(hero, &actors[i]) && cell_dist(hero, &actors[i]) <= best) { best = cell_dist(hero, &actors[i]); actor_target_set(hero, &actors[i]); }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {  // 右鍵：對點到的怪物放左手技能
                int zx = e.button.x, zy = e.button.y, cx, cy;
                zoom_point(&zx, &zy);
                Actor *m = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
                if (m) actor_target_set(hero, m);
                // shortcut: 點選地點取格子中心；像素級地點施法待世界座標逆投影接入。
                hero_cast_point(hero, cx * 64 + 32, cy * 64 + 32, m);
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) pointer_down = 0;
            else if (e.type == SDL_MOUSEMOTION) { px = e.motion.x; py = e.motion.y; mouse_seen = e.motion.which != SDL_TOUCH_MOUSEID; }
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
        if (load_due >= 0) {
            static SaveHead h;
            if (save_read(load_due, &h)) {
                float x = hero->x, y = hero->y;
                int dir = hero->dir, lv = hero->level, exp = hero->exp, hp = hero->hp, mp = hero->mp;
                js_status(-1);
                if (enter_scene(&scene, hero, h.alias, -1)) snprintf(alias, sizeof alias, "%s", h.alias);
                actor_restore_position(hero, x, y); hero->dir = dir;
                hero->foe = NULL; hero->want = NULL; hero->cast = NULL; hero->casting = 0; hero->act = ACT_NONE;
                hero->buff_n = hero->fight = hero->dead_ticks = hero->knock = hero->stun = hero->z = 0;
                hero_stats(hero, lv);
                hero->exp = exp; hero->hp = hp; hero->mp = mp;
                hero_skill_apply(hero);
                room = world_tried ? world_find(&world, alias) : -1;
                monsters_due = 1;
                armed = 0;
                pointer_down = 0;
                walk_cx = -1;
                scene_music(alias, music, sizeof music);
                audio_music(music);
                hud_say("\xe6\xb8\xb8\xe6\x88\x8f\xe8\xbf\x9b\xe5\xba\xa6\xe8\xaf\xbb\xe5\x8f\x96\xe6\x88\x90\xe5\x8a\x9f");  // 游戏进度读取成功（EasyRPG_C 7054，拿掉顏色標記）
            } else hud_say("\xe8\xaf\xbb\xe5\x8f\x96\xe5\xa4\xb1\xe8\xb4\xa5");  // shortcut: 自訂「读取失败」
            load_due = -1;
        }

        if (hero->selection_pending && !hero->act && !hero->casting) hero_selected_skill_apply(hero);
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
            int zx = px, zy = py;
            zoom_point(&zx, &zy);
            mx = zx - vw / 2;
            my = zy - (vh / 2 - 40);
            if (mx * mx + my * my < 64) mx = my = 0;
        }

        // ---- 固定節拍：跟原版一樣每 16ms 一拍，畫面多快都不影響遊戲速度 ----
        Uint32 now_ms = SDL_GetTicks();
        if (now_ms - logic_ms > 250) logic_ms = now_ms - TICK_MS;  // 卡頓或切到背景後不要一次補太多拍
        while (now_ms - logic_ms >= TICK_MS) {
            logic_ms += TICK_MS;
            ++tick;
            actor_logic_tick = tick;
            if (tick * TICK_MS / 1000 != (tick - 1) * TICK_MS / 1000) for (int i = 0; i < actor_count; i++)
                    if (!i || actors[i].mon) actor_second(&actors[i]);
            viewport_tick(&viewport, &scene, hero);
            for (int i = 0; i < actor_count; i++) {
                if (actors[i].role) actor_motion_tick(&actors[i]);
                actor_health_sync(&actors[i]);
                actors[i].animation_stepped = 0;
                if (actors[i].role) actor_maintenance_tick(&actors[i]);
                if (actors[i].fight > 0 && !actors[i].foe && !--actors[i].fight)
                    actor_status_notify(&actors[i], 0, 0, -1);
            }
            int held = hero->act != ACT_DEAD && react_tick(hero, &scene);
            if (hero->act == ACT_DEAD) {  // shortcut: 原版死亡後的處理（回城、掉經驗）還沒做，先 3 秒後原地復活
                if (++hero->dead_ticks > 180) { hero->act = ACT_NONE; hero->object_dead = 0; hero->dead_ticks = 0; hero->hp = hero->hp_max; hero->mp = hero->mp_max; actor_place(hero, (int)hero->x, (int)hero->y, 1); }
                action_tick(hero, &scene, tick);
            } else if (hero->act) action_tick(hero, &scene, tick);
            else if (held) hero->moving = 0;
            else if (mx * mx + my * my > 0.01f) { hero->foe = NULL; hero->want = NULL; ground_want = walk_cx = -1; actor_tick(hero, &scene, mx, my, tick); }  // 自己走 = 放棄目標
            else if (ground_want >= 0 && !hero->foe) ground_walk(hero, &scene, tick);
            else if (walk_cx >= 0 && !hero->foe) point_walk(hero, &scene, tick);
            else if (!pursue(hero, &scene, tick)) actor_tick(hero, &scene, 0, 0, tick);
            for (int i = 1; i < actor_count; i++) {
                Actor *a = &actors[i];
                if (!a->mon) { if (!react_tick(a, &scene)) npc_think(a, &scene, tick); }
                else if (!a->role) continue;
                else if (a->act == ACT_DEAD) { action_tick(a, &scene, tick); if (++a->dead_ticks > CORPSE_TICKS) a->role = NULL; }
                else if (react_tick(a, &scene) && !a->act) a->moving = 0;
                else if (a->act) action_tick(a, &scene, tick);
                else monster_think(a, hero, &scene, tick);
            }
            for (int i = 0; i < actor_count; i++) {
                if (actors[i].role && !actors[i].animation_stepped) actor_animation_tick(&actors[i]);
                actor_near_reactions(&actors[i]);
                walk_fx(&actors[i], &scene, tick);
            }
            unit_scene = &scene;
            units_tick(tick);
            scene_buildings_tick(&scene);
            light_tick(&scene);
            clock_tick();
            weather_tick();
            air_tick(scene_client_actor(&scene_client));
            bird_tick(&scene, &viewport);
            zoom_tick();
            // 踩到觸發區：事件 "go <出口>" 對應目前房間的出口，切到目的房間的入口
            const Trigger *t = trigger_at(&scene, hero);
            if (!t) armed = 1;
#ifdef STORY
            // 原版 go（EasyRPG 0x10017e30）：出口要開著；隨機地下城出口（0x10052af0）；舊房間第 13 格、新房間第 14 格同意才進
            else if (armed && !strncmp(t->event, "go ", 3)) {
                char path[128];
                int entry = 0, to = 0;
                armed = 0;
                if (!story_exit(sroom, t->event + 3, path, sizeof path, &entry)) continue;
                story_begin();
                story_random_exit(sroom, t->event + 3, path, sizeof path, &entry);
                to = story_room_by_path(path);
                int ok = to && story_enter(to);
                story_end();
                js_status(-1);
                if (ok && enter_scene(&scene, hero, story_room_alias(to), entry)) {
                    monsters_due = 1;
                    snprintf(alias, sizeof alias, "%s", story_room_alias(to));
                    scene_music(alias, music, sizeof music);
                    audio_music(music);
                    printf("進入 %s\n", alias);
                    snap_due = 1;  // 穿梭場景時自动存盘（UI_C 2503）。shortcut: 原版只在「大場景」之間存，這裡每個出口都存
                    save_due = 0;
                }
            }
#else
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
                        snap_due = 1;  // 穿梭場景時自动存盘（UI_C 2503）。shortcut: 原版只在「大場景」之間存，這裡每個出口都存
                        save_due = 0;
                    }
                    armed = 0;
                    break;
                }
            }
#endif
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
                    free(d);
                }
            }
#ifdef STORY
            actors_sync(&scene);
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
        viewport_draw(&viewport, &scene, tick);
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
        dialog_draw();
#endif
        if (mouse_seen) {  // 觸控沒有游標
            const Anim *cur = anim_get(0x100011);
            int zx = px, zy = py, cx, cy;
            zoom_point(&zx, &zy);
            Actor *c = input_char(zx, zy, ox, oy, &scene, &cx, &cy);
            int k = input_cursor(hero, c, ground_at(zx, zy, ox, oy, &scene), cx, cy);
            if (cur && k < cur->count) draw_frame(cur, k, frame_at(&cur->seqs[k], tick), px, py, 0);
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
    scene_free(&scene);
    SDL_Quit();
    return 0;
}
