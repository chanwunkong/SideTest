// 招式單位（原版 Poq.exe 0x435600–0x43f300：MethodBase 的觸發機制、CMethodUnit 與子類別）。
// main.c 直接 #include 這個檔（要用 Actor、skill_hit、audio_sfx…）。
// 原版座標每格 64 單位（格子 = 座標 >> 6，格子中心 = 格×64 + 32）；角色的 x, y 是格子座標（浮點），換算 = ×64。
// 方向：dir8／dir16 = 畫面角度從「右」起逆時針每 45°／22.5° 一格（= 動畫方向編號）。

// ---- 動畫播放器（CSprGroup 0x49e720：一個序列一個播放器 0x49cd00/0x49d060）----
typedef struct { const Anim *a; int seq, cur, tick, start, end; } Spr;

static const Frame *spr_frame(const Spr *s) {  // 0x49d6a0
    if (!s->a || s->seq >= s->a->count || !s->a->seqs[s->seq].count) return NULL;
    return &s->a->seqs[s->seq].frames[s->cur % s->a->seqs[s->seq].count];
}
static int spr_delay(const Spr *s) {  // 幀延遲 + 調整（+0x14）+ 全域調整（0x663f2c）；shortcut: 兩個調整都當 0
    const Frame *f = spr_frame(s);
    return f && f->delay > 1 ? f->delay : 1;
}
static int spr_len(const Spr *s) { return s->a && s->seq < s->a->count ? s->a->seqs[s->seq].count : 0; }
// 換序列（0x49e6a0）：從第一幀（last = 1 時從最後一幀）開始，循環整個序列
static void spr_set(Spr *s, int seq, int last) {
    s->seq = seq;
    s->tick = s->start = 0;
    s->end = spr_len(s) > 0 ? spr_len(s) - 1 : 0;
    s->cur = last ? s->end : 0;
}
static void spr_load(Spr *s, uint32_t gid) {  // 0x49e8d0
    s->a = anim_get(gid);
    spr_set(s, 0, 0);
}
static int spr_count(const Spr *s) { return s->a ? s->a->count : 0; }  // 序列數（+0x454）
static void spr_seq(Spr *s, int seq) { if (s->seq != seq) spr_set(s, seq, 0); }
// 前進一拍（0x49e150）：這幀停滿就換下一幀，到 end 回 start
static void spr_next(Spr *s) {
    if (++s->tick < spr_delay(s)) return;
    s->tick = 0;
    s->cur = s->cur == s->end ? s->start : s->cur + 1;
    if (s->cur > spr_len(s) - 1) s->cur = 0;
}
// 前進一拍但停在最後一幀（0x49dfc0）
static void spr_step(Spr *s) {
    if (s->cur >= s->end && s->tick == spr_delay(s) - 1) return;
    if (++s->tick < spr_delay(s)) return;
    s->tick = 0;
    if (s->cur == s->end) return;
    if (++s->cur > spr_len(s) - 1) s->cur = 0;
}
// 播完了（0x49dcd0）：在最後一幀而且停到最後一拍
static int spr_done(const Spr *s) { return s->cur >= s->end && s->tick == spr_delay(s) - 1; }

// ---- OmUtil.dll 的幾何函式 ----
static int dir_of(int fx, int fy, int tx, int ty, int n) {  // WhatDir8/WhatDir16(到, 從)
    float sx = (tx - fx) - (ty - fy), sy = ((tx - fx) + (ty - fy)) / 2.0f;  // 世界 → 畫面
    if (!sx && !sy) return 0;
    return ((int)lroundf(atan2f(-sy, sx) / (float)(2 * M_PI / n)) + n) % n;
}
static const int GRID_X[8] = {1, 0, -1, -1, -1, 0, 1, 1}, GRID_Y[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
static void move_dir(int *x, int *y, int dir, int speed) {  // MoveDirForSpeed：往 dir 的相鄰格方向走 speed（64 = 一格）
    *x += GRID_X[dir & 7] * speed;
    *y += GRID_Y[dir & 7] * speed;
}
static float angle_of(int ax, int ay, int bx, int by) { return atan2f((float)(by - ay), (float)(bx - ax)); }  // GetAngle(a, b)
static void move_angle(int *ox, int *oy, int fx, int fy, float ang, float dist) {  // MoveForAngle(輸出, 從, 角度, 距離)
    *ox = fx + (int)(cosf(ang) * dist);
    *oy = fy + (int)(sinf(ang) * dist);
}
static int dist_of(int ax, int ay, int bx, int by) { return (int)sqrtf((float)(ax - bx) * (ax - bx) + (float)(ay - by) * (ay - by)); }  // 0x43bee0

static int ax64(const Actor *a) { return (int)(a->x * 64); }
static int ay64(const Actor *a) { return (int)(a->y * 64); }
#include "near.c"

// 角色目前這幀的出手點（執行期幀 +0xe、+0x10）
static void actor_hot(const Actor *a, int *ex, int *ey) {
    *ex = *ey = 0;
    if (!a->role || !a->role->part_count) return;
    int seq = actor_seq(a);
    const Anim *an = a->role->body[0];
    if (!an || seq >= an->count || !an->seqs[seq].count) return;
    const Frame *f = &an->seqs[seq].frames[actor_frame(a, seq) % an->seqs[seq].count];
    *ex = f->ex;
    *ey = f->ey;
}

// ---- 招式物件共同部分（MethodBase 0x4381a0）：角色出手（MethodMain）和招式單位都有 ----
enum { U_NORMAL, U_FLY, U_FLYNORMAL, U_ARROW, U_GHOST, U_CYCLE, U_SURFIRE, U_FIREMAN, U_MAIN, U_PHANTOM, U_FOOT, U_SPLASH };  // U_MAIN = 角色出手（MethodMain）、U_PHANTOM = 分身（CPhantasmObj）
typedef struct MUnit {
    int cls, dead;                   // 類別、已結束（+0x58）
    Actor *owner, *target;           // +0x87 放的人、+0x83 目標
    const Method *m;                 // +0x8b 招式
    const SkillRow *sk;              // 結算用的技能（+0x93、+0x97）
    int range;                       // +0x9b 攻擊距離（格）
    int x, y, tx, ty, sx, sy;        // +0x34 位置、+0x73 目標點、+0x7b 一開始的目標點
    int dir, h, persist;             // +0x6b 方向、+0x42f 高度、+0x3e7 打中後還留著
    int ticks, t5, t0;               // +0x423 經過拍數、+0x427 計時觸發進度、+0x42b 幀觸發進度
    int path, path_mode, px, py;     // +0x3d7 沿路命中、+0x3e3 用哪種命中、+0x3db 上次掃到的點
    const Unit *def;                 // +0x460 單位表
    Spr spr;                         // +0x444 動畫
    uint32_t shadow;                 // dData1 影子動畫（0x100000 = 沒有）
    Actor *stick;                    // +0x464 附在誰身上
    int life, life2, mode;           // 各類別的計數（+0x47c、+0x480…）、CGhostMan 繞誰（+0x488）
    float ang;                       // +0x484
    int cx, cy, cells, left;         // 飛行：目前格（+0x127）、經過幾格（+0x129）、剩幾拍（+0x126）
    float fx, fy, vx, vy, kx, ky;    // +0x4a8 浮點位置、+0x4b0 每拍位移、+0x4bc 落地後的減速
    int vh, acc, step, div, landed;  // +0x4c8 升速、+0x4cc、+0x4d0、+0x4d4、+0x4d8
    const Trig *trig;                // +0x3f3 觸發：單位表的；角色出手是招式表的
    int trig_n;
    Actor *hit[50];                  // +0x9f 打中名單（CStaticLinkList<int,50>）
    int hit_n, bounce, frame;        // +0x233 名單數量、+0x434 連鎖剩幾次、角色出手 = 出手者動作目前幀
    int seq, fade, fading, ftick;    // 分身：畫出手者的哪個序列、+0x4ea 淡出剩幾級、+0x4ee 淡出中、換幀計時
    int pq[32][2], pq_n;             // 分身 +0x4f2 出手者走過的位移（CDMList<GPOINT>）
} MUnit;

#define MAX_UNIT 512  // 腳印也算在內
static MUnit units[MAX_UNIT];
static int unit_n;

static void unit_trig(MUnit *u, const Trig *t, Uint32 tick);
static void unit_die(MUnit *u, Uint32 tick);

// 打中觸發（0x438560 → 0x439710）：flag 1 那組裡 iWhatFrame = 事件（1 打中人、2 撞牆）或 −1 的
static void unit_event(MUnit *u, int ev, Uint32 tick) {
    for (int i = 0; i < u->trig_n; i++)
        if (u->trig[i].flag == 1 && (ev < 0 || u->trig[i].frame == ev || u->trig[i].frame == -1)) unit_trig(u, &u->trig[i], tick);
}

// 加進打中名單（0x438560）：放打中觸發；招式 wEffect 1~3 時對方擲抗性 隨機(200) < B_C_DJHF + 正在放的招的 DJHF（0x488630），
// 抗住就到此為止，否則加狀態 110（受傷旋飛：不能動、不能攻擊，不會到期）。原版不檢查重複，名單滿 50 個就加不進去
static void hit_add(MUnit *u, Actor *t, Uint32 tick) {
    unit_event(u, 1, tick);
    int e = u->m ? u->m->effect : 0;
    if (e >= 1 && e <= 3 && alive(t)) {
        const SkillRow *ts = t->act == ACT_ATTACK ? (t->casting ? t->cast : t->skill) : NULL;
        if (rand() % 200 < t->ex[75] + (ts ? (int)skill_field(ts, 19, 0) : 0)) return;
        buff_add(t, NULL, 110, 1, -1, 0);
    }
    if (u->hit_n < 50) u->hit[u->hit_n++] = t;
}

// 放開打中名單（0x439e70）：wEffect 1~3 時移掉名單上每個人的狀態 110，清空
static void hit_release(MUnit *u) {
    int e = u->m ? u->m->effect : 0;
    for (int i = 0; e >= 1 && e <= 3 && i < u->hit_n; i++) buff_drop(u->hit[i], 110);
    u->hit_n = 0;
}

// 名單上已經不在的拿掉（0x4393c0、0x437540 開頭：0x4883b0 找不到物件）
static void hit_prune(MUnit *u) {
    int n = 0;
    for (int i = 0; i < u->hit_n; i++)
        if (alive(u->hit[i])) u->hit[n++] = u->hit[i];
    u->hit_n = n;
}

// ClockEllipsePathMove（OmUtil.dll 0x400084a0）：p 繞 c 轉。離 c 比 r 近就往外、遠就往內走 speed/2，再往順時針方向走 speed
static void ellipse_move(int *px, int *py, int cx, int cy, int r, int speed) {
    int d = dir_of(cx, cy, *px, *py, 8), dist = dist_of(*px, *py, cx, cy);
    if (dist < r) move_dir(px, py, d, speed / 2);
    else if (dist > r) move_dir(px, py, (d - 4) & 7, speed / 2);
    move_dir(px, py, (d - 2) & 7, speed);
}

// 找範圍內的人並打（0x439fb0）：從 pt 的格子螺旋走 (r+48)/64 圈（至少 1，0x442730/0x442a80），格子上的人位置在 pt ± (r+64) 方框內的；
// all = 0 只打第一個。招式 wManualID 1、2 打同隊（加血之類），其他打不同隊。回傳打中幾個
// shortcut: 原版還能打可破壞物件（obj_breakable，+0x5c）、連線版的建築；人是否「接受」這次打擊（虛擬函式 0x68）這裡都當接受
static int hit_find(MUnit *u, int px, int py, int all, int r, Uint32 tick) {
    Actor *o = u->owner;
    if (!o) return 0;
    int n = (r + 48) / 64, box = r + 64, hits = 0;
    Spiral sp;
    spiral_init(&sp, px / 64, py / 64, n < 1 ? 1 : n);
    while (spiral_next(&sp)) {
        int i = 0;
        for (Actor *t; (t = cell_actor(sp.x, sp.y, &i)); i++) {
            int x = ax64(t), y = ay64(t);
            if (x < px - box || y < py - box || x >= px + box || y >= py + box) continue;
            int friendly = u->m && (u->m->manual == 1 || u->m->manual == 2);
            if (friendly ? t->side != o->side : t == o || t->side == o->side) continue;
            skill_hit(o, t, u->sk, tick);
            if (u->cls >= 0) hit_add(u, t, tick);
            hits++;
            if (!all) return hits;
        }
    }
    return hits;
}

// 依 iTriID 打（0x438a00）：中心物件 = 單位自己（招式單位的虛擬函式 0x24 = 0x4357a0）
// 0 前方一格（半徑 0x30）；2 前方，沒打到再看左右前方；3 再多看前方第二排；5 自己的點；6~10 自己周圍 64~320 全部；
// 11 目標（物件或目標點）0x30；12 目標周圍 0xc0 全部；13 目標周圍 0xc0 第一個；0x14 前方一格、兩格、左右前方
static void unit_hits(MUnit *u, int id, Uint32 tick) {
    int hit = 0, d = u->dir, x, y;
    int gx = u->target && alive(u->target) ? ax64(u->target) : u->tx, gy = u->target && alive(u->target) ? ay64(u->target) : u->ty;
#define AT(dd, sp, r, all) (x = u->x, y = u->y, move_dir(&x, &y, (dd), (sp)), hit_find(u, x, y, all, r, tick))
    switch (id) {
    case 0: hit = AT(d, 64, 0x30, 0); break;
    case 2: hit = AT(d, 64, 0x3c, 0) || AT(d + 1, 64, 0x30, 0) || AT(d + 7, 64, 0x30, 0); break;
    case 3: hit = AT(d, 64, 0x30, 0) || AT(d + 1, 64, 0x30, 0) || AT(d + 7, 64, 0x30, 0);
        if (!hit) {
            int bx = u->x, by = u->y;
            move_dir(&bx, &by, d, 64);
            for (int k = 0; k < 3 && !hit; k++) {
                x = bx, y = by;
                move_dir(&x, &y, d + (k == 1 ? 1 : k == 2 ? 7 : 0), 64);
                hit = hit_find(u, x, y, 0, 0x30, tick);
            }
        }
        break;
    case 5: hit = hit_find(u, u->x, u->y, 0, 0, tick); break;
    case 6: case 7: case 8: case 9: case 10: hit = hit_find(u, u->x, u->y, 1, (id - 5) * 64, tick); break;
    case 11: hit = hit_find(u, gx, gy, 0, 0x30, tick); break;
    case 12: hit = hit_find(u, gx, gy, 1, 0xc0, tick); break;
    case 13: hit = hit_find(u, gx, gy, 0, 0xc0, tick); break;
    case 0x14: hit = AT(d, 64, 0x30, 0) || AT(d, 128, 0x30, 0) || AT(d + 1, 64, 0x30, 0) || AT(d + 7, 64, 0x30, 0); break;
    }
#undef AT
    if (hit && !u->persist) unit_die(u, tick);
}

// 沿路命中（0x439b80）：從上次掃到的點往現在的位置每 64 掃一次
static void unit_sweep(MUnit *u, Uint32 tick) {
    int x = u->x, y = u->y;
    if (u->px == -100) {
        u->px = x, u->py = y;
        unit_hits(u, u->path_mode, tick);
    }
    int steps = 0;
    while (!u->dead && dist_of(u->px, u->py, x, y) > 0x3f) {
        move_angle(&u->px, &u->py, u->px, u->py, angle_of(u->px, u->py, x, y), 64);
        u->x = u->px, u->y = u->py;
        unit_hits(u, u->path_mode, tick);
        steps++;
    }
    if (steps) u->x = u->px = x, u->y = u->py = y;
}

// 放出點（0x43bf50，依觸發的 iTriPos）：0 出手點、1 自己、2/4 目標點、3 目標（沒有就目標點）、5/9/10/11 出手點 ±32/64/96/128、
// 6 自己 ±32、7 目標點 ±32、8 目標點 ±64。出手點 = 中心物件位置 + 這幀的 (ex, ey)（畫面 → 世界：x += 2(ey−高度)+ex, y += 2(ey−高度)−ex）
static void unit_from(int *ox, int *oy, int pos, int cx, int cy, int ex, int ey, int h, int tx, int ty, const Actor *target) {
    int hx = cx + (ey - h) * 2 + ex, hy = cy + (ey - h) * 2 - ex;
    switch (pos) {
    case 0: *ox = hx, *oy = hy; break;
    case 1: *ox = cx, *oy = cy; break;
    case 3: if (target && alive(target)) { *ox = ax64(target), *oy = ay64(target); break; }  // fallthrough
    case 2: case 4: *ox = tx, *oy = ty; break;
    case 5: *ox = hx + (rand() & 63) - 32, *oy = hy + (rand() & 63) - 32; break;
    case 6: *ox = cx + (rand() & 63) - 32, *oy = cy + (rand() & 63) - 32; break;
    case 7: *ox = tx + (rand() & 63) - 32, *oy = ty + (rand() & 63) - 32; break;
    case 8: *ox = tx + (rand() & 127) - 64, *oy = ty + (rand() & 127) - 64; break;
    case 9: *ox = hx + (rand() & 127) - 64, *oy = hy + (rand() & 127) - 64; break;
    case 10: *ox = hx + rand() % 0xc0 - 0x60, *oy = hy + rand() % 0xc0 - 0x60; break;
    case 11: *ox = hx + (rand() & 255) - 128, *oy = hy + (rand() & 255) - 128; break;
    default: *ox = cx, *oy = cy;
    }
    if (*ox < 0) *ox = 0;
    if (*oy < 0) *oy = 0;
}

// ---- 產生（0x450700 依單位表分類 → 0x43d150 從來源抄招式資料 → 各類別的初始化 0x30）----
static void unit_init(MUnit *u);
static void unit_timed(MUnit *u, Uint32 tick);

// src = 放出它的東西：角色出手時 (owner, target, m, sk, range, 目標點)，單位放單位時是那個單位
static MUnit *unit_new(int id, int x, int y, const MUnit *src, Uint32 tick) {
    const Unit *d = unit_find(id);
    if (!d || unit_n == MAX_UNIT) return NULL;
    int cls = d->manual == 300 ? U_NORMAL : d->manual == 500 ? U_SURFIRE : d->manual == 510 ? U_FIREMAN
            : d->manual != 201 ? -1 : d->kind == 1 ? U_FLYNORMAL : d->kind == 2 ? U_ARROW : d->kind == 4 || d->kind == 6 ? U_GHOST : d->kind == 5 ? U_CYCLE : -1;
    if (cls < 0) return NULL;
    MUnit *u = &units[unit_n++];
    memset(u, 0, sizeof *u);
    u->cls = cls;
    u->def = d;
    u->trig = d->trig, u->trig_n = d->trig_count;
    u->persist = 1;
    u->mode = d->kind == 6;
    u->shadow = (uint32_t)d->d[1];
    u->x = x & 0xffff, u->y = y & 0xffff;
    // 0x43d150：抄來源的放的人、目標、招式、技能；目標點 = 來源的目標點（放的人不是角色時：來源位置，依招式距離拉長／截短）
    u->owner = src->owner;
    u->target = src->target;
    u->m = src->m;
    u->sk = src->sk;
    u->range = src->range;
    u->dir = src->dir;  // 原版來源是物件種類 1 時才 /2，角色出手和單位都不是
    if (u->owner) u->tx = src->tx, u->ty = src->ty;
    else u->tx = src->x, u->ty = src->y;
    u->sx = u->tx, u->sy = u->ty;
    unit_init(u);
    unit_timed(u, tick);  // 第 0 拍的計時觸發（0x43d150 最後呼叫 0x43a660）
    return u;
}

// 計時觸發（0x43a660）：flag 5 在第 iWhatFrame 拍放一次；flag ≥ 6 每 iWhatFrame 拍放一次
static void unit_timed(MUnit *u, Uint32 tick) {
    int k = 0;
    for (int i = 0; i < u->trig_n && !u->dead; i++) {
        const Trig *t = &u->trig[i];
        if (t->flag != 5) continue;
        if (k++ < u->t5) continue;
        if (u->ticks < t->frame) break;
        unit_trig(u, t, tick);
        u->t5++;
    }
    u->ticks++;
    for (int i = 0; i < u->trig_n && !u->dead; i++)
        if (u->trig[i].flag >= 6 && u->trig[i].frame > 0 && u->ticks % u->trig[i].frame == 0) unit_trig(u, &u->trig[i], tick);
}

// 幀觸發（0x4393c0）：名單上的人 wEffect 2、3 時繞單位轉（半徑 64、速度 8）；沿路命中；flag 0 在動畫播到 iWhatFrame 時放
// （角色出手看出手者的動作幀，0x43af30）
static void unit_frames(MUnit *u, Uint32 tick) {
    hit_prune(u);
    int e = u->m ? u->m->effect : 0;
    for (int i = 0; (e == 2 || e == 3) && i < u->hit_n; i++) {
        Actor *a = u->hit[i];
        int x = ax64(a), y = ay64(a);
        ellipse_move(&x, &y, u->x, u->y, 0x40, 8);
        a->x = x / 64.0f, a->y = y / 64.0f;  // shortcut: 原版也不看地形，直接搬
    }
    if (u->path) unit_sweep(u, tick);
    int cur = u->cls == U_MAIN ? u->frame : u->spr.cur;
    int k = 0;
    for (int i = 0; i < u->trig_n && !u->dead; i++) {
        const Trig *t = &u->trig[i];
        if (t->flag != 0) continue;
        if (k++ < u->t0) continue;
        if (cur < t->frame) break;
        unit_trig(u, t, tick);
        u->t0++;
    }
}

static void unit_base(MUnit *u, Uint32 tick) {  // 0x43d750。shortcut: 透明度漸變（+0x46c → +0x470）沒做
    unit_timed(u, tick);
    if (!u->dead) unit_frames(u, tick);
}

// 連鎖（0x43a760 前半）：放的人還在、招式 wQuality 3、還有次數時不結束，重來一次：名單有人就照原方向，
// 否則從單位的格子螺旋找 攻擊距離×64 內第一個敵人，放的人轉向它、目標點改到它；都沒有就次數歸 0（放的人回平常 0x496550）。回傳 1 = 重來
static int unit_bounce(MUnit *u) {
    Actor *o = u->owner;
    if (!o || !alive(o) || !u->m || u->m->quality != 3 || u->bounce <= 0) return 0;
    if (!u->hit_n) {
        Actor *e = near_first(u->x, u->y, u->range * 64);  // 0x442810/0x443a00 螺旋找、0x443560 敵對
        while (e && !is_foe(o, e)) e = near_next();
        if (!e) { u->bounce = 0; return 0; }
        face(o, (e->x - o->x - (e->y - o->y)) * 32, (e->x - o->x + e->y - o->y) * 16);
        o->foe = e;
        foe_grp(o, e);
        u->tx = ax64(e), u->ty = ay64(e);
        u->dir = dir_of(u->x, u->y, u->tx, u->ty, 8);
    }
    u->bounce--;
    hit_release(u);
    u->ticks = u->t5 = u->t0 = 0;
    return 1;
}

// 結束（0x43a760，虛擬函式 0x20）：不連鎖就放開打中名單、放 flag 3 的觸發
static void unit_die(MUnit *u, Uint32 tick) {
    if (u->dead) return;
    u->dead = 1;
    hit_release(u);
    for (int i = 0; i < u->trig_n; i++)
        if (u->trig[i].flag == 3) unit_trig(u, &u->trig[i], tick);
}

// 放單位（0x43b8c0）：wManualID 2000 = 隨機挑 dData0~4 其中一個；456 = 設定沿路命中（bData0 開關、bData1 命中方式）；
// 招式 wQuality 6、7 時扇形放 Reserve1 − 1 個（6 = 60°、7 = 一圈），目標點離放出點至少 64
// 400 = 放的人身上加光源（0x442000）：半徑 dData0、顏色 bData0~2。
// shortcut: 455（目標身上的效果 0x48ec60）、依武器換箭（+0x52d）沒做
static void unit_spawn(int id, const Trig *t, MUnit *src, int cx, int cy, int ex, int ey, Uint32 tick) {
    const Unit *d = unit_find(id);
    if (!d || d->manual == 1) return;
    if (d->manual == 2000) { unit_spawn(d->d[rand() % 5], t, src, cx, cy, ex, ey, tick); return; }
    if (d->manual == 456) {
        src->path = d->b[0];
        src->path_mode = d->b[1];
        if (src->path) src->px = src->py = -100;
        return;
    }
    if (d->manual == 400) {
        if (src->owner) light_add(src->owner, actor_wx(src->owner, &scene), actor_wy(src->owner, &scene), 1, d->d[0], (Uint32)(d->b[0] | d->b[1] << 8 | d->b[2] << 16), -1);
        return;
    }
    if (t->pos == 3 && !(src->target && alive(src->target))) return;
    int x, y;
    unit_from(&x, &y, t->pos, cx, cy, ex, ey, src->h, src->tx, src->ty, src->target);
    if (d->manual == 201 && src->m && (src->m->quality == 6 || src->m->quality == 7) && src->m->r1 > 1) {
        float span = src->m->quality == 7 ? 6.2831855f : 1.0471975f, ang;
        int n = src->m->r1 - 1, tx = src->tx, ty = src->ty, dist = dist_of(x, y, tx, ty);
        ang = angle_of(x, y, tx, ty) - span * 0.5f;
        if (dist < 64) dist = 64;
        for (int i = 0; i < n; i++, ang += span / n) {
            move_angle(&src->tx, &src->ty, x, y, ang, (float)dist);
            unit_new(id, x, y, src, tick);
        }
        src->tx = tx, src->ty = ty;
        return;
    }
    unit_new(id, x, y, src, tick);
}

// 執行一個觸發（0x43c4f0，招式單位當來源）：0 命中、1 音效、2 放單位、10 結束、14 震畫面、16 改目標點（1 往側邊、0x21 隨機方向、0x22 抖動）
// shortcut: 3/4（地上的痕跡物件 0x498c40）、5/7/8/11/13（對放的人的動作，單位很少用）、12 透明度沒做
static void unit_trig(MUnit *u, const Trig *t, Uint32 tick) {
    if (u->cls == U_MAIN && owner_trig(u, t, tick)) return;
    switch (t->type) {
    case 0: unit_hits(u, t->id, tick); break;
    case 1: audio_sfx(t->id); break;
    case 2: {
        const Frame *f = spr_frame(&u->spr);
        unit_spawn(t->id, t, u, u->x, u->y, f ? f->ex : 0, f ? f->ey : 0, tick);
        break;
    }
    case 10: unit_die(u, tick); break;
    case 14:
        if (t->id == 4 && shake_left < 1 && abs(u->x - ax64(&actors[0])) + abs(u->y - ay64(&actors[0])) < 20 * 64) shake_left = shake_total = t->id2;
        break;
    case 16:
        if (t->id == 1) {
            int dd = u->dir + 2;
            u->tx = u->sx, u->ty = u->sy;
            move_dir(&u->tx, &u->ty, dd < 0 ? dd + 8 : dd > 7 ? dd - 8 : dd, t->id2 * 64);
        } else if (t->id == 0x21) {
            static const float A[16] = {0, 0.3926991f, 0.7853982f, 1.1780972f, 1.5707964f, 1.9634954f, 2.3561945f, 2.7488935f,
                                        3.1415927f, 3.5342917f, 3.9269907f, 4.3196898f, 4.712389f, 5.105088f, 5.497787f, 5.8904862f};
            int r = rand() % (t->id2 > 0 ? t->id2 : 1), m = u->range < 1 ? 1 : u->range;
            move_angle(&u->tx, &u->ty, u->x, u->y, A[rand() & 15] - 1.5707964f, (float)((m <= r ? r : m) * 64));
        } else if (t->id == 0x22 && t->id2 > 0) {
            int w = t->id2 * 64;
            u->tx += rand() % w - w / 2;
            u->ty += rand() % w - w / 2;
        }
        break;
    }
}

// ---- 各類別 ----
// 撞牆（0x435c00 看格子的地形種類與單位的層）。shortcut: 格子的地形種類還沒讀，用能不能走代替；經過 3 格以上撞牆時 1/(格數−1) 機率穿過
static int unit_blocked(MUnit *u) {
    int cx = u->x >> 6, cy = u->y >> 6;
    if (cx == u->cx && cy == u->cy) return 0;
    u->cx = cx, u->cy = cy;
    u->cells++;
    if (cx < 0 || cy < 0 || cx >= scene.w || cy >= scene.h || scene_walkable(&scene, cx, cy)) return 0;
    return u->cells <= 2 || rand() % (u->cells - 1) != 0;
}

// 飛行物初始化（CFlyEffect 0x4368c0）：沿路命中方式 5；高度 bData1、打中後留著 bData2、速度 bData3；
// 拍數 = 距離 ÷ 速度（0 當 2），每拍等速前進，高度依拋物線（升速 = 速度，高度 ≥ 0x33 時 0x46，每拍減 速度/(拍數/2)）；
// 落地後的減速 0.8~0.99 隨機
static void fly_init(MUnit *u) {
    const Unit *d = u->def;
    u->path = 1, u->path_mode = 5, u->px = u->py = -100;
    u->h = d->b[1];
    u->persist = d->b[2];
    int speed = d->b[3];
    spr_load(&u->spr, (uint32_t)d->d[0]);
    if ((u->tx >> 6) == (u->x >> 6) && (u->ty >> 6) == (u->y >> 6))
        move_angle(&u->tx, &u->ty, u->x, u->y, angle_of(u->tx, u->ty, u->x, u->y), 64);
    int dist = dist_of(u->x, u->y, u->tx, u->ty);
    u->dir = dir_of(u->x, u->y, u->tx, u->ty, 16);
    spr_seq(&u->spr, u->dir);
    u->fx = (float)u->x, u->fy = (float)u->y;
    if (speed) {
        int n = dist / speed;
        if (!n) n = 2;
        u->left = n;
        u->vx = (float)(u->tx - u->x) / n;
        u->vy = (float)(u->ty - u->y) / n;
        u->vh = u->h < 0x33 ? speed : 0x46;
        u->acc = 0;
        u->step = u->vh;
        u->div = n / 2 < 1 ? 1 : n / 2;
    }
    u->kx = (float)(rand() % 20) / 100 + 0.8f;
    u->ky = (float)(rand() % 20) / 100 + 0.8f;
    u->cx = u->x >> 6, u->cy = u->y >> 6;
}

static void fly_arc(MUnit *u) {  // 0x436cf0
    u->acc += u->step;
    int k = u->acc / u->div;
    if (k) u->acc %= u->div;
    u->vh -= k;
    u->h += u->vh / 4;
}

static void fly_land(MUnit *u, Uint32 tick) {  // 虛擬函式 0x40
    if (u->cls == U_FLYNORMAL) {  // 0x437b80：落地爆炸（序列 dir+0x20；沒有就直接結束）
        u->landed = 1;
        if (spr_count(&u->spr) < 0x30) unit_die(u, tick);
        else spr_seq(&u->spr, u->dir + 0x20);
        return;
    }
    fly_arc(u);
    if (u->cls != U_ARROW) return;
    // CARROW 0x435720：高度不到 0x28（或 0x3c）時換成下落的序列（+0x10）
    int seq = u->spr.seq, n = spr_count(&u->spr);
    if (u->h < 0x28 ? seq < 0x20 && n > 0x10 : u->h < 0x3c && seq < 0x10 && n > 0x20) spr_seq(&u->spr, seq + 0x10);
}

static void fly_tick(MUnit *u, Uint32 tick) {  // 0x436d60
    spr_next(&u->spr);
    unit_base(u, tick);
    if (u->dead) return;
    if (unit_blocked(u)) {
        unit_event(u, 2, tick);
        unit_die(u, tick);
        return;
    }
    if (u->left < 1) {
        if (u->h < 0xb || u->def->kind == 1) {  // 落到地上：放打中觸發，結束
            unit_event(u, -1, tick);
            unit_die(u, tick);
            return;
        }
        u->vx *= u->kx;
        u->vy *= u->ky;
        fly_land(u, tick);
        if (u->dead) return;
    } else if (u->h > 1) fly_arc(u);
    u->left--;
    if (u->def->b[3] >= 2 || rand() % 5 == 1) {
        u->fx += u->vx;
        u->fy += u->vy;
        u->x = (int)u->fx, u->y = (int)u->fy;
        if (u->def->b[3] < 2) {  // 慢的會亂飄
            u->fx += (float)(rand() % 3 - 1);
            u->fy += (float)(rand() % 3 - 1);
        }
    }
}

static void flynormal_tick(MUnit *u, Uint32 tick) {  // CFlyEffectNormal 0x437540
    if (u->spr.seq / 16 == 0 && spr_done(&u->spr) && spr_count(&u->spr) > 0x1f) spr_seq(&u->spr, u->dir + 0x10);
    if (!u->landed) {
        if (u->m && u->m->effect == 7) {  // 左右擺動前進
            u->life2++;
            int d8 = u->dir / 2, side = (u->life2 / 4) % 2 == 1;
            int dd = side ? (d8 + 1 < 8 ? d8 + 1 : 0) + 1 : (d8 - 1 < 0 ? 7 : d8 - 1) - 1;
            move_dir(&u->x, &u->y, (dd + 8) % 8, 4);
            u->fx = (float)u->x, u->fy = (float)u->y;
        }
        fly_tick(u, tick);
        return;
    }
    hit_prune(u);
    for (int i = 0; u->m && u->m->effect == 1 && i < u->hit_n; i++)  // 落地後往打中的人飄 4
        move_dir(&u->x, &u->y, dir_of(u->x, u->y, ax64(u->hit[i]) + 4, ay64(u->hit[i]) + 4, 8), 4);
    spr_next(&u->spr);
    if (spr_done(&u->spr)) unit_die(u, tick);
}

static void ghost_init(MUnit *u) {  // CGhostMan 0x437d00
    const Unit *d = u->def;
    u->life = d->b[3];
    spr_load(&u->spr, (uint32_t)d->d[0]);
    u->life2 = 2000;
    int cx = u->target && alive(u->target) ? ax64(u->target) : u->tx, cy = u->target && alive(u->target) ? ay64(u->target) : u->ty;
    u->ang = angle_of(cx, cy, u->x, u->y);
    u->range = (abs(u->tx - u->x) + abs(u->ty - u->y)) / 64;
}

// CGhostMan 0x437e30：繞著目標（mode 0，慢慢靠近，距離 < 10 時打中結束）或放的人（mode 1）轉圈
static void ghost_tick(MUnit *u, Uint32 tick) {
    unit_base(u, tick);
    if (u->dead) return;
    if (u->life2 == -1) { unit_die(u, tick); return; }
    if (u->life2-- < 1) return;
    const Actor *c = u->mode ? u->owner : u->target;
    int cx = c && alive(c) ? ax64(c) : u->tx, cy = c && alive(c) ? ay64(c) : u->ty, dist = dist_of(u->x, u->y, cx, cy), adj;
    if (!u->mode) {
        if (dist < 10) { unit_hits(u, 5, tick); unit_die(u, tick); return; }
        adj = -4;
    } else {
        if (!c || !alive(c)) { unit_die(u, tick); return; }
        int r = u->range * 64;
        if (r < dist) u->life = -(rand() & 1);
        else if (dist < r / 2) u->life = rand() % 5 + 5;
        adj = u->life;
    }
    float ang = 12.566371f / (float)(adj + dist) + u->ang;
    u->ang = ang;
    int nx, ny;
    move_angle(&nx, &ny, cx, cy, ang, (float)(adj + dist));
    u->dir = dir_of(u->x, u->y, nx, ny, 16);
    u->x = nx, u->y = ny;
    spr_seq(&u->spr, u->dir);
    spr_next(&u->spr);
}

static void cycle_init(MUnit *u) {  // CFlyCycleObj 0x436250
    const Unit *d = u->def;
    u->persist = d->b[2];
    spr_load(&u->spr, (uint32_t)d->d[0]);
    int dist = dist_of(u->x, u->y, u->tx, u->ty);
    spr_seq(&u->spr, dir_of(u->x, u->y, u->tx, u->ty, 16));
    u->dir = dir_of(u->x, u->y, u->tx, u->ty, 8);
    move_angle(&u->x, &u->y, u->x, u->y, angle_of(u->x, u->y, u->tx, u->ty), 64);
    u->life = dist / 128;
    u->cx = u->x >> 6, u->cy = u->y >> 6;
}

// CFlyCycleObj 0x436390：每播完一輪動畫往目標跳 64 並重新開始觸發，跳完 距離/128 次結束
static void cycle_tick(MUnit *u, Uint32 tick) {
    spr_next(&u->spr);
    unit_base(u, tick);
    if (u->dead) return;
    if (unit_blocked(u)) { unit_event(u, 2, tick); unit_die(u, tick); return; }
    if (!spr_done(&u->spr)) return;
    const Frame *f = spr_frame(&u->spr);
    if (f) u->x += f->ey * 2 + f->ex, u->y += f->ey * 2 - f->ex;
    move_angle(&u->x, &u->y, u->x, u->y, angle_of(u->x, u->y, u->tx, u->ty), 64);
    u->life--;
    int ticks = u->ticks, t5 = u->t5;
    u->t0 = 0;  // 0x438680 重新整理觸發
    if (u->life < 1) unit_die(u, tick);
    u->ticks = ticks, u->t5 = t5;
}

static void fireman_init(MUnit *u) {  // CFireDeadMan 0x435a80：跑 bData1 內隨機拍數，速度 bData0，方向隨機
    const Unit *d = u->def;
    spr_load(&u->spr, (uint32_t)d->d[0]);
    u->life = d->b[0];
    u->left = rand() % (d->b[1] > 0 ? d->b[1] : 1);
    u->dir = rand() & 7;
    spr_seq(&u->spr, u->dir);
}

// CFireDeadMan 0x435ca0：著火亂跑（擋住就換方向），1/5 機率在身邊 ±10 放火（dData2~4 隨機），跑完播倒地（序列 dir+8）
static void fireman_tick(MUnit *u, Uint32 tick) {
    unit_base(u, tick);
    if (u->dead) return;
    const Unit *d = u->def;
    if (u->left == -1) { unit_die(u, tick); return; }
    if (u->left < 1) {
        if (rand() % 15 == 10) unit_new(d->d[2 + rand() % 3], u->x - 20 + rand() % 40, u->y - 20 + rand() % 40, u, tick);
        spr_seq(&u->spr, u->dir + 8);
        spr_step(&u->spr);
        if (spr_done(&u->spr)) u->left = -1;
        return;
    }
    for (int k = 0; k < 8; k++) {
        int x = u->x, y = u->y;
        move_dir(&x, &y, u->dir, 0x20);
        int cx = x >> 6, cy = y >> 6;
        if (cx >= 0 && cy >= 0 && cx < scene.w && cy < scene.h && scene_walkable(&scene, cx, cy)) break;
        u->dir = (u->dir + 1) & 7;
    }
    move_dir(&u->x, &u->y, u->dir, u->life);
    u->left--;
    spr_seq(&u->spr, u->dir);
    spr_next(&u->spr);
    if (rand() % 5 == 1) unit_new(d->d[2 + rand() % 3], u->x - 10 + rand() % 20, u->y - 10 + rand() % 20, u, tick);
}

static void surfire_init(MUnit *u) {  // CSurFireArrow 0x43f020
    const Unit *d = u->def;
    spr_load(&u->spr, (uint32_t)d->d[0]);
    u->life = d->d[1];
    u->life2 = d->b[1];
    if (spr_count(&u->spr) > 7) spr_seq(&u->spr, u->dir);
    // shortcut: 原版初始化還有一段依 bData0 在身邊放 dData2~4 的迴圈，反編譯的次數變數沒初始化，看不出幾個，沒做
}

static void surfire_tick(MUnit *u, Uint32 tick) {  // 0x43f1b0：停 bData1 拍後播結束的序列（+0x10），播完結束
    if (u->life2 != -1) {
        if (u->life2 < 1) {
            if (spr_count(&u->spr) >= 8) { if (u->spr.seq < 0x10) spr_seq(&u->spr, u->spr.seq + 0x10); }
            else if (spr_count(&u->spr) > 1 && u->spr.seq == 0) spr_seq(&u->spr, 1);
            spr_step(&u->spr);
            if (spr_done(&u->spr)) u->life2 = -1;
        } else {
            u->life2--;
            spr_next(&u->spr);
        }
    }
    if (u->life2 == -1) { unit_die(u, tick); return; }
    unit_base(u, tick);
}

// 一般特效（CNormalEffect 0x43d980）：bData4 選序列（0 第一個、1 隨機、2 依方向）；bData1 = 1 時活 bData2×10 拍；
// 附著（0x43da80，bData3）：0 放在原地、1 跟著放的人、其他跟著目標（沒有就找那格上的人）
static void normal_init(MUnit *u, int x, int y) {
    const Unit *d = u->def;
    spr_load(&u->spr, (uint32_t)d->d[0]);
    if (d->b[4] == 1 && spr_count(&u->spr) > 0) spr_seq(&u->spr, rand() % spr_count(&u->spr));
    else if (d->b[4] == 2) spr_seq(&u->spr, u->dir);
    u->life = d->b[1] == 1 ? d->b[2] * 10 : -1;
    if (d->b[3] == 0) { u->x = x, u->y = y; return; }
    if (d->b[3] == 1) u->stick = u->owner;
    else {
        u->stick = u->target;
        if (!u->stick) {
            u->x = x, u->y = y;
            for (int i = 0; i < actor_count; i++)
                if (alive(&actors[i]) && (int)actors[i].x == x >> 6 && (int)actors[i].y == y >> 6) { u->stick = &actors[i]; break; }
        }
    }
    if (u->stick && alive(u->stick)) u->x = ax64(u->stick) + 1, u->y = ay64(u->stick) + 1;
    else u->stick = NULL;
}

static void normal_tick(MUnit *u, Uint32 tick) {  // 0x43dc30
    unit_base(u, tick);
    if (u->dead) return;
    if (u->stick) {
        if (!alive(u->stick)) { unit_die(u, tick); return; }
        u->x = ax64(u->stick) + 1, u->y = ay64(u->stick) + 1;
    }
    int b1 = u->def->b[1];
    if (b1 == 0) { if (spr_done(&u->spr)) { unit_die(u, tick); return; } }
    else if (b1 == 1) { if (u->life-- <= 0) { unit_die(u, tick); return; } }
    else if (!u->stick) { unit_die(u, tick); return; }
    spr_next(&u->spr);
}

static void unit_init(MUnit *u) {  // 虛擬函式 0x30
    switch (u->cls) {
    case U_NORMAL: normal_init(u, u->x, u->y); break;
    case U_FLYNORMAL: case U_ARROW: fly_init(u); break;
    case U_GHOST: ghost_init(u); break;
    case U_CYCLE: cycle_init(u); break;
    case U_FIREMAN: fireman_init(u); break;
    case U_SURFIRE: surfire_init(u); break;
    }
}

static void phantom_tick(MUnit *u, Uint32 tick);
static void foot_tick(MUnit *u, Uint32 tick);
static void units_tick(Uint32 tick) {
    for (int i = 0; i < unit_n; i++) {
        MUnit *u = &units[i];
        if (u->dead) continue;
        switch (u->cls) {
        case U_NORMAL: normal_tick(u, tick); break;
        case U_FLYNORMAL: flynormal_tick(u, tick); break;
        case U_ARROW: fly_tick(u, tick); break;
        case U_GHOST: ghost_tick(u, tick); break;
        case U_CYCLE: cycle_tick(u, tick); break;
        case U_FIREMAN: fireman_tick(u, tick); break;
        case U_SURFIRE: surfire_tick(u, tick); break;
        case U_MAIN: if (!alive(u->owner) || u->owner->act != ACT_ATTACK) unit_die(u, tick); break;  // 動作被打斷；平常由 action_tick 推
        case U_PHANTOM: phantom_tick(u, tick); break;
        case U_FOOT: foot_tick(u, tick); break;
        case U_SPLASH: if (spr_done(&u->spr)) unit_die(u, tick); else spr_step(&u->spr); break;  // 下雨濺水：播一次
        }
    }
    int n = 0;
    for (int i = 0; i < unit_n; i++)
        if (!units[i].dead) units[n++] = units[i];
    unit_n = n;
}

// 畫（0x43d500、飛行物 0x436ca0 畫面 y 減高度；箭 0x4356a0 先畫影子動畫 dData1）
// shortcut: bData0 的混色方式（加亮、半透明…）、光影（0x626398）還沒做，都用一般貼圖；分層與遮擋由 viewport.c 的可見名單處理
static void unit_draw(const MUnit *u, int sx, int sy) {
        int fly = u->cls == U_FLYNORMAL || u->cls == U_ARROW;
        if (fly && u->shadow && u->shadow != 0x100000) {
            const Anim *sa = anim_get(u->shadow);
            if (sa) draw_frame(sa, u->spr.seq < sa->count ? u->spr.seq : 0, 0, sx, sy, 1);
        }
        if (u->spr.a) draw_frame(u->spr.a, u->spr.seq, u->spr.cur, sx, sy - (fly ? u->h : 0), 0);
        // 分身（0x43e740）：光影開著時混色 0x1d、透明度 = 淡出級數；shortcut: 0x1d 當半透明，級數 8 = 一半
        if (u->cls == U_PHANTOM && u->owner) {
            draw_alpha = (Uint8)(u->fade * 16);
            role_draw(u->owner->role, u->seq, u->frame, sx, sy - u->h);
            draw_alpha = 255;
        }
 }

// 單位自己會打中人：飛行物（沿路命中），或觸發裡有命中（type 0）
static int unit_can_hit(const Unit *d) {
    if (d->manual == 201) return 1;
    for (int i = 0; i < d->trig_count; i++)
        if (d->trig[i].type == 0) return 1;
    return 0;
}

// ---- 角色出手（MethodMain）：攻擊動作開始時產生，跟著出手的人，觸發看出手者的動作幀 ----
static MUnit *main_of(const Actor *a) {
    for (int i = 0; i < unit_n; i++)
        if (units[i].cls == U_MAIN && units[i].owner == a && !units[i].dead) return &units[i];
    return NULL;
}

// 目標點（0x43af50）：目標 = 攻擊對象；招式 Reserve2 ≠ 1 時目標點一律拉到攻擊距離，= 1 時超過才截短。連鎖次數 = Reserve1
static void main_new(Actor *a) {
    if (unit_n == MAX_UNIT) return;
    MUnit *u = &units[unit_n++];
    const SkillRow *sk = a->casting ? a->cast : a->skill;
    *u = (MUnit){.cls = U_MAIN, .owner = a, .target = alive(a->foe) ? a->foe : NULL, .m = a->atk, .sk = sk, .x = ax64(a), .y = ay64(a), .persist = 1,
                 .trig = a->atk->trig, .trig_n = a->atk->trig_count, .bounce = a->atk->r1};
    u->range = sk ? actor_row_range(a, sk) : a->atk->range_max;
    if (u->range < 0) u->range = 0;
    if (u->target) u->tx = ax64(u->target), u->ty = ay64(u->target);
    else {
        u->tx = u->x, u->ty = u->y;
        move_dir(&u->tx, &u->ty, DIR_MAP[a->dir & 7], 64);
    }
    u->dir = dir_of(u->x, u->y, u->tx, u->ty, 8);
    int dist = dist_of(u->x, u->y, u->tx, u->ty);
    if (dist > 0 && (u->m->r2 != 1 || u->range * 64 < dist)) {
        float k = (float)(u->range * 64) / dist;
        u->tx = u->x + (int)((u->tx - u->x) * k);
        u->ty = u->y + (int)((u->ty - u->y) * k);
    }
    u->sx = u->tx, u->sy = u->ty;
}

// 角色出手打中人（0x439fb0 → 0x438560）：加進它的打中名單
static void main_hit(Actor *a, Actor *t, Uint32 tick) {
    MUnit *u = main_of(a);
    if (u) hit_add(u, t, tick);
}

// ---- 分身（CPhantasmObj 0x43dd90～0x43ef50）：畫出手者的紙娃娃，半透明 ----
// 產生（0x43e100）：src = 角色出手。模式 1 = 殘影：用出手者目前的動作和幀，idx/2（單數取負）往側邊滑 |idx/2|×16、之後照出手者走過的路延遲跟著；
// 模式 3 = 環繞：動作 0x30+方向（主角 1201 用 0x50），隨機起始幀、換幀快 5 拍，角度 = 方向角 + 90° + idx 的偏移，
// 半徑 8 起每拍 +2 往外，活 攻擊距離×32 拍，沿路用範圍 6 打。模式 4（受傷閃避時 0x48c3a0 放）見 shortcut
static void phantom_new(MUnit *src, int mode, int idx) {
    static const float OFF[8] = {0, 4, 2, 6, 1, 5, 3, 7};  // ×π/4（0x43e100 的 idx 0~7）
    Actor *o = src->owner;
    if (!o || !o->role || unit_n == MAX_UNIT) return;
    MUnit *u = &units[unit_n++];
    *u = (MUnit){.cls = U_PHANTOM, .owner = o, .target = src->target, .m = src->m, .sk = src->sk, .range = src->range,
                 .x = ax64(o), .y = ay64(o), .tx = src->tx, .ty = src->ty, .dir = DIR_MAP[o->dir & 7], .h = (int)o->z,
                 .mode = mode, .life2 = idx, .seq = actor_seq(o), .fade = 8, .persist = 1};
    u->sx = u->x, u->sy = u->y;
    if (mode == 1) {
        u->life2 = idx & 1 ? -(idx / 2) : idx / 2;
        u->life = abs(u->life2 * 64) / 4;
        u->frame = actor_frame(o, u->seq);
    }
    else if (mode == 3) {
        u->seq = (o->def && !strcmp(o->def->graphic, HERO_GRAPHIC) ? 0x50 : 0x30) + u->dir;
        u->life = u->range << 5;
        u->step = 8;
        const Seq *q = role_seq(o->role, u->seq);
        u->frame = q && q->count ? rand() % q->count : 0;
        u->path_mode = 6, u->px = u->py = -100;
        // 方向角（0x442780）：方向 d → (d+1)×45°；原版 MoveForAngle 的 y 跟這裡的 move_angle 相反，所以取負
        u->ang = -((float)M_PI / 4 * ((u->dir + 1) % 8) + (float)M_PI / 2 + (idx < 8 ? OFF[idx] * (float)M_PI / 4 : 0));
    }
}

// 每拍（0x43e830）。結束（虛擬函式 0x20 = 0x43ef50）只是開始淡出：每 3 拍降一級，降完消失
static void phantom_tick(MUnit *u, Uint32 tick) {
    u->ticks++;
    hit_prune(u);
    if (u->fading) {
        if (u->ticks % 3 == 1) u->fade--;
        if (u->fade < 1) { unit_die(u, tick); return; }
    }
    Actor *o = alive(u->owner) ? u->owner : NULL;
    MUnit *mm = o ? main_of(o) : NULL;
    if (!o) { u->fading = 1; return; }
    if (u->mode == 1) {
        int pop = 1;
        u->frame = actor_frame(o, u->seq);
        if (u->life > 0) {
            move_dir(&u->x, &u->y, (u->dir + (u->life2 < 1 ? -2 : 2)) & 7, 1);
            u->life--;
            if (u->life2) pop = u->pq_n / abs(u->life2) > 2;
        }
        int ox = ax64(o), oy = ay64(o);
        if ((ox != u->sx || oy != u->sy) && u->pq_n < 32) {  // shortcut: 原版名單不限長度，這裡最多記 32 步
            u->pq[u->pq_n][0] = ox - u->sx, u->pq[u->pq_n][1] = oy - u->sy;
            u->pq_n++;
            u->sx = ox, u->sy = oy;
        }
        if (!mm) u->fading = 1;
        if (pop && u->pq_n > 0) {
            u->x += u->pq[0][0], u->y += u->pq[0][1];
            memmove(u->pq, u->pq + 1, sizeof u->pq[0] * --u->pq_n);
        }
    }
    else if (u->mode == 3) {
        if (u->life > 0 && --u->life < 1) u->fading = 1;
        // 出手者沒在出手，或還是放出它的那一招才繼續轉。shortcut: 原版比招式物件編號，這裡比招式
        if (mm && mm->m != u->m) { u->fading = 1; return; }
        u->step += 2;
        move_angle(&u->x, &u->y, ax64(o), ay64(o), u->ang, (float)u->step);
        const Seq *q = role_seq(o->role, u->seq);
        if (q && q->count && ++u->ftick >= (q->frames[u->frame % q->count].delay - 5 > 1 ? q->frames[u->frame % q->count].delay - 5 : 1)) {
            u->ftick = 0;
            u->frame = (u->frame + 1) % q->count;
        }
        unit_sweep(u, tick);
    }
}

// ---- 腳印（CFootStep 0x43f300～0x43f5e0，地面物件 CNormalSurobj）：動畫 0x100309，序列 = 方向（另一隻腳 +0x10）----
// 前 300 拍停在第一幀（原版畫進地面層），之後換序列 +8 播一次淡掉，播完消失
static void foot_new(int x, int y, int seq) {
    if (unit_n == MAX_UNIT) return;
    MUnit *u = &units[unit_n++];
    *u = (MUnit){.cls = U_FOOT, .x = x, .y = y, .life = 300};
    spr_load(&u->spr, 0x100309);
    spr_set(&u->spr, seq, 0);
}

// 點地面的痕跡（0x459840）：拿掉上一個，在格子中央放動畫 0x1001bd（地上痕跡物件 0x9b，0x450da0）
// shortcut: 0x9b 的類別沒查，+0x97 = 1 當成播一次就消失（走腳印淡掉那段）
static void mark_new(int cx, int cy) {
    const Anim *a = anim_get(0x1001bd);
    for (int i = 0; i < unit_n; i++)
        if (a && units[i].cls == U_FOOT && units[i].spr.a == a) unit_die(&units[i], 0);
    if (unit_n == MAX_UNIT) return;
    MUnit *u = &units[unit_n++];
    *u = (MUnit){.cls = U_FOOT, .x = cx * 64 + 32, .y = cy * 64 + 32, .mode = 1};
    spr_load(&u->spr, 0x1001bd);
}

static void foot_tick(MUnit *u, Uint32 tick) {  // 0x43f5e0
    if (!u->mode) {
        if (u->ticks++ > u->life) u->mode = 1, spr_set(&u->spr, u->spr.seq + 8, 0);  // 0x43f3e0
        return;
    }
    if (spr_done(&u->spr)) unit_die(u, tick);
    else spr_step(&u->spr);
}
