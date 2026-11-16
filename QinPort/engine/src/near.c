// 場景格子查詢（Poq.exe 0x442660～0x444740）：從一格往外螺旋走、找格子上的角色／道具／陷阱、沿直線走到擋住為止。
// unit.c 直接 #include 這個檔（要用 Actor、scene、move_dir、ground…）。
// 原版格子 21 bytes：+4 能走、+6 層（Cell.layer）、+0xc 站在上面的角色、+0xe 建築、+0x10 道具或陷阱、+0x13 旗標。
// 我們沒有每格的物件編號，改成掃 actors／ground 找位置在那格的。
// shortcut: 原版一格只記一個角色（後進來的蓋掉）；這裡同一格有幾個都算，依 actors 的順序

#define LATER __attribute__((unused))  // 呼叫者還沒移植

typedef struct { int x, y, dir, len, step, max; } Spiral;

static void spiral_init(Spiral *s, int x, int y, int n) { *s = (Spiral){x, y, 0, 1, 0, n * 2 + 1}; }  // 0x442730：n 圈

// 下一格（0x442a80）：第一次是中心，之後 y−1、x−1、y+1、x+1 輪流走，邊長 1,1,2,2,3,3…；邊長到 max 的那邊走完回 0
static int spiral_next(Spiral *s) {
    switch (s->dir) {
    case 0: s->dir = 1; return 1;
    case 1: s->y--; break;
    case 2: s->x--; break;
    case 3: s->y++; break;
    case 4: s->x++; break;
    }
    if (++s->step < s->len) return 1;
    if (s->max <= s->len) return 0;
    if (++s->dir >= 5) s->dir = 1;
    if (s->dir == 1 || s->dir == 3) s->len++;
    s->step = 0;
    return 1;
}

static const Cell *cell_at(int x, int y) {  // 0x4c7xxx 取格子：出界 = NULL
    return x < 0 || y < 0 || x >= scene.w || y >= scene.h ? NULL : &scene.cells[y * scene.w + x];
}

// 格子能不能讓某一層的東西過（0x435c00）：層 1 = 一般地面，只看能不能走；11 過 12 不過 14；13 過 12、14；
// 15 只能在 12、14、16；其他層要格子的層一樣。除了 15 和「一樣」，層 16 的格子都不過
static int cell_pass(const Cell *c, int layer) {
    int k = c->layer;
    if (layer != 1) {
        if (layer == 11) {
            if (k == 12) return 1;
            if (k == 14) return 0;
        } else if (layer == 13) {
            if (k == 12 || k == 14) return 1;
        } else if (layer == 15) return k == 12 || k == 14 || k == 16;
        else return k == layer;
        if (k == 16) return 0;
    }
    return c->walk != 0;
}

static int is_trap(const Actor *a) { return a->mon && a->mon->istrap == 1; }  // 陷阱在原版是格子 +0x10 的物件（種類 0x18），不是角色

// 格子上的第 i 個起的角色（原版 +0xc → 0x4c78b0，物件種類 0x1e～0x64）
static Actor *cell_actor(int x, int y, int *i) {
    for (; *i < actor_count; (*i)++) {
        Actor *t = &actors[*i];
        if (alive(t) && !is_trap(t) && (int)t->x == x && (int)t->y == y) return t;
    }
    return NULL;
}
static Actor *cell_actor0(int x, int y) { int i = 0; return cell_actor(x, y, &i); }

// Poq.exe 0x4c74b0：含中心及第n圈，依上、下、左、右邊掃描。
// 邊界先夾進地圖；只排除角色占位，道具及建築不額外排除。
static int vacant_cell(int *x, int *y, int layer, int n) {
    if (!scene.cells || scene.w < 1 || scene.h < 1) return 0;
    int cx = *x, cy = *y;
    for (int r = 0; ; r++) {
        int l = cx - r, t = cy - r, right = cx + r, b = cy + r;
        l = l < 0 ? 0 : l >= scene.w ? scene.w - 1 : l;
        right = right < 0 ? 0 : right >= scene.w ? scene.w - 1 : right;
        t = t < 0 ? 0 : t >= scene.h ? scene.h - 1 : t;
        b = b < 0 ? 0 : b >= scene.h ? scene.h - 1 : b;
        for (int edge = 0; edge < 4; edge++) {
            int from = edge < 2 ? l : t, to = edge < 2 ? right : b;
            for (int k = from; k <= to; k++) {
                int px = edge < 2 ? k : edge == 2 ? l : right;
                int py = edge < 2 ? (edge == 0 ? t : b) : k;
                if (cell_pass(cell_at(px, py), layer) && !cell_actor0(px, py)) {
                    *x = px; *y = py;
                    return 1;
                }
            }
        }
        if (r >= n) return 0;
    }
}

// 格子上的道具或陷阱（原版 +0x10）：地上的道具、陷阱
static int cell_prop(int x, int y) {
    for (int g = 0; g < ground_n; g++)
        if ((int)ground[g].x == x && (int)ground[g].y == y) return 1;
    for (int i = 0; i < actor_count; i++)
        if (alive(&actors[i]) && is_trap(&actors[i]) && (int)actors[i].x == x && (int)actors[i].y == y) return 1;
    return 0;
}

// 格子 +0xe 的建築索引沿用場景占地表，取代原本一律回無建築的簡化版。
static int cell_bld(int x, int y) {
    const Cell *c = cell_at(x, y);
    return c && (c->flags & 128) && c->building >= 0 && c->building < scene.object_count ? c->building : -1;
}

// 物件種類 0x80～0x100 且不是 0x96（0x439b20：可被打壞的物件）
LATER static int obj_breakable(int kind) { return (unsigned)(kind - 0x80) < 0x81 && kind != 0x96; }

// ---- AI 組關係表（CScnClient +0x7df0 串列：每個 NPC 組一個 CPlayerGroupAI，+0x14 組號、+0x1c 敵人組名單最多 200 個）----
// shortcut: 原版串列不限組數，這裡最多 64 組；原版跟著 CScnClient，這裡整個遊戲共用一張（換場景沒清）
#define GRP_MAX 64
static struct { int id, n, foe[200]; } grps[GRP_MAX];
static int grp_n;

// 找組（0x452460）
static int grp_find(int id) {
    for (int i = 0; i < grp_n; i++)
        if (grps[i].id == id) return i;
    return -1;
}

// 組 g 把組 e 當敵人（0x450bc0：沒有這組就建一個；0x44f460：已在名單裡不再加，滿 200 個不加）
static void grp_enemy_add(int g, int e) {
    int i = grp_find(g);
    if (i < 0) {
        if (grp_n == GRP_MAX) return;
        i = grp_n++;
        grps[i].id = g, grps[i].n = 0;
    }
    for (int k = 0; k < grps[i].n; k++)
        if (grps[i].foe[k] == e) return;
    if (grps[i].n < 200) grps[i].foe[grps[i].n++] = e;
}

// 組 g 的敵人名單裡有沒有 e（0x453210）
static int grp_is_enemy(int g, int e) {
    int i = grp_find(g);
    for (int k = 0; i >= 0 && k < grps[i].n; k++)
        if (grps[i].foe[k] == e) return 1;
    return 0;
}

static int grp_npc(int g) { return (unsigned)(g - 10000) < 50001; }  // NPC 組 10000～60000

// 敵對（0x443560）：不同組，任一邊是玩家組（0～99）就是；否則查 NPC 組那邊的敵人名單。
// 怪現象（照原版）：a 是 NPC 組就只查 a 那組的名單，所以 B 記了 A 為敵人時 is_foe(B, A) 成立、is_foe(A, B) 不一定
static int is_foe(const Actor *a, const Actor *b) {
    if (!a || !b || a->grp == b->grp) return 0;
    if ((unsigned)a->grp < 100 || (unsigned)b->grp < 100) return 1;
    if (grp_npc(a->grp)) return grp_is_enemy(a->grp, b->grp);
    return grp_npc(b->grp) && grp_is_enemy(b->grp, a->grp);
}

// self 把 t 設成目標時（CCharactor 設目標裡的一段）：t 是 NPC 組、self 不是玩家組也不是 9000～9999 組，t 的組就記 self 的組為敵人
// shortcut: 原版還排除 self 有狀態 0x66 或旗標 0x4000 的，沒做
static void foe_grp(const Actor *self, const Actor *t) {
    if (t && grp_npc(t->grp) && self->grp > 99 && (unsigned)(self->grp - 9000) > 999) grp_enemy_add(t->grp, self->grp);
}

// ---- 從一點往外找角色（全域狀態 DAT_0065ea50～0x65ea78）----
static struct { Spiral s; int l, t, size, i; } nr;

// 0x442970：從 (px, py)（1/64 格）找 r 以內：走 r/64 圈（至少 1），角色位置要在方框 (px−r, py−r) 起、邊長 3r 之內。
// 怪現象（照原版）：方框邊長是 3r 不是 2r，右下兩邊多出 r
static void near_begin(int px, int py, int r) {
    int n = r / 64;
    nr.l = px - r, nr.t = py - r, nr.size = r * 3;
    spiral_init(&nr.s, px / 64, py / 64, n < 1 ? 1 : n);
    nr.i = 0;
}

// 下一個（0x443a00）。shortcut: GRECT::IsContain 的邊界（含不含右下邊）沒對 OmUtil，這裡左上含、右下不含
static Actor *near_next(void) {
    for (;;) {
        if (!nr.i && !spiral_next(&nr.s)) return NULL;
        Actor *t;
        while ((t = cell_actor(nr.s.x, nr.s.y, &nr.i))) {
            nr.i++;
            int x = ax64(t), y = ay64(t);
            if (x >= nr.l && y >= nr.t && x < nr.l + nr.size && y < nr.t + nr.size) return t;
        }
        nr.i = 0;
    }
}

static Actor *near_first(int px, int py, int r) { near_begin(px, py, r); return near_next(); }  // 0x442810

// 角色遍歷共用模式；與底層near游標一樣，巢狀開始會覆蓋前次遍歷。
static int actor_near_mode;

// shortcut: 模板顯示方式尚未載入，暫僅用第10修正排除50；模板入口補齊時接RoleMdl欄位。
// shortcut: Actor名單皆為角色，以存活角色承接原版種類30..100檢查；物件種類接入時改精確篩選。
// shortcut: 戰鬥狀態0沿用fight，初始化記錄座標以目前整數格承接；完整狀態／SInit接入後替換。
// 此處尚非完整角色遍歷入口，不以原版位址標記完成。
static Actor *actor_near_next(const Actor *a) {
    for (Actor *t; (t = near_next());) {
        if (actor_flags(t, 0x20010) || t->display_override == 50) continue;
        if (actor_near_mode == 0 && !has_status(a, 102) && !actor_flags(a, 0x4000)) {
            if (is_foe(a, t)) return t;
            Actor *foe = t->foe;
            if (t != a && t->grp == a->grp && t->fight && alive(foe) && foe->grp != a->grp) {
                int d = dist_of((int)a->x, (int)a->y, (int)foe->x, (int)foe->y);
                int td = dist_of((int)a->x, (int)a->y, (int)t->x, (int)t->y);
                if (d < actor_seek(a) && (t->def->program == 40 || td < 1)) return foe;
            }
        } else if (actor_near_mode == 1) {
            if (t != a && t->grp == a->grp) return t;
        } else if (actor_near_mode == 2) {
            // shortcut: 物件識別沿用劇情handle；非劇情物件ID建立後改用共用CObj識別。
            if (t != a && a->search_link == t->sh) return t;
        } else if (actor_near_mode == 3) {
            if (t != a && grp_npc(t->grp)) return t;
        }
    }
    return NULL;
}

// Poq.exe 0x48b9e0：只有預設半徑才依高層加8；失敗不重設底層游標，仍會更新模式。
static Actor *actor_near_begin(const Actor *a, int mode, int radius) {
    actor_near_mode = mode;
    if (radius == -1) radius = actor_seek(a);
    if (radius < 1) return NULL;
    near_begin(ax64(a), ay64(a), radius * 64);
    return actor_near_next(a);
}

// 0x443cd0：r 以內第一個不是 side 這邊的
LATER static Actor *near_other(int px, int py, int r, int side) {
    for (Actor *t = near_first(px, py, r); t; t = near_next())
        if (t->side != side) return t;
    return NULL;
}

// 點到的角色（0x442b10）：點的格子 (cx, cy) 周圍 5×5 格（從 +2,+2 掃到 −2,−2），點 (px, py) 落在角色畫面方框裡的，
// 取離「角色位置的畫面 x、方框垂直中心」最近的（一樣近取先掃到的）；all = 0 時不要身上有 0x80 的。side ≥ 0 時只看那一邊的（我們的呼叫者要的）。
// box(a, &l, &t, &w, &h) = 角色的畫面方框（原版物件 +0x3c）。
// shortcut: 原版還跳過格子 +0x13 < 2 的（意義沒查）；最後一個參數的隊伍判斷（已選的是 NPC 組、新的是玩家組時直接選新的）沒做
static Actor *pick_at(int px, int py, int cx, int cy, int all, int side, void (*box)(const Actor *, int *, int *, int *, int *)) {
    Actor *best = NULL;
    int best_d = 10000;
    for (int y = cy + 2; y >= cy - 2; y--)
        for (int x = cx + 2; x >= cx - 2; x--) {
            if (!cell_at(x, y)) continue;
            int i = 0;
            for (Actor *t; (t = cell_actor(x, y, &i)); i++) {
                if (side >= 0 && t->side != side) continue;
                int l, tp, w, h;
                box(t, &l, &tp, &w, &h);
                if (px < l || py < tp || px >= l + w || py >= tp + h) continue;
                int dx = l + w / 2 - px,  /* 我們的方框以腳底為中心，l + w/2 = 位置的畫面 x */ dy = tp + h / 2 - py, d = (int)sqrt((double)(dx * dx + dy * dy));
                if (d < best_d && (all || !(t->flags & 0x80))) best = t, best_d = d;
            }
        }
    return best;
}

// ---- 沿直線、八方向 ----
// 格子讓不讓過（0x442eb0／0x443790 的 mode）：0 = 這層能過且沒有角色；1 = 這層能過；3 = 層 2～9 的要這層能過；4 = 層 3～9 的要這層能過
static int line_ok(const Cell *c, int mode, int layer, int x, int y) {
    switch (mode) {
    case 0: return cell_pass(c, layer) && !cell_actor0(x, y);
    case 1: return cell_pass(c, layer);
    case 3: return (unsigned)(c->layer - 2) <= 7 ? cell_pass(c, layer) : 1;
    case 4: return (unsigned)(c->layer - 3) <= 6 ? cell_pass(c, layer) : 1;
    }
    return 0;
}

// 0x442eb0：從格子 f 往 t 一格一格（每步 (t−f)/距離，小數取整）走，最多 steps 步，擋住就停；
// 回最後一個「這層能過且沒有角色」的格子（都沒有就是 f）。stop = 1 時走到距離那麼多步、而且已經離開 f 就停
LATER static void line_end(int *ox, int *oy, int fx, int fy, int tx, int ty, int steps, int stop, int mode, int layer) {
    int d = (int)sqrt((double)((tx - fx) * (tx - fx) + (ty - fy) * (ty - fy)));
    int rx = fx, ry = fy;
    float x = (float)fx, y = (float)fy;
    for (int i = 1; d > 0 && i <= steps; i++) {
        x += (float)(tx - fx) / (float)d;
        y = (float)(ty - fy) / (float)d + y;
        int cx = (int)x, cy = (int)y;
        const Cell *c = cell_at(cx, cy);
        if (!c || !line_ok(c, mode, layer, cx, cy)) break;
        if (cell_pass(c, layer) && !cell_actor0(cx, cy)) rx = cx, ry = cy;
        if (stop && d <= i && (rx != fx || ry != fy)) break;
    }
    *ox = rx, *oy = ry;
}

// 0x443790：f 到 t 的直線（最多 steps 步）都過得去回 1；同一格回 1；出界、擋住、步數用完還沒到回 0。
// mode 0 要格子能走且沒有角色（不看層），其他同 line_ok 但 cell_pass 換成「能走」
LATER static int line_clear(int fx, int fy, int tx, int ty, int steps, int mode) {
    int d = (int)sqrt((double)((tx - fx) * (tx - fx) + (ty - fy) * (ty - fy)));
    if (!d) return 1;
    float x = (float)fx, y = (float)fy;
    for (int i = 1; i <= steps; i++) {
        x += (float)(tx - fx) / (float)d;
        y = (float)(ty - fy) / (float)d + y;
        int cx = (int)x, cy = (int)y;
        const Cell *c = cell_at(cx, cy);
        if (!c) return 0;
        int ok = mode == 0 ? c->walk && !cell_actor0(cx, cy)
               : mode == 1 ? c->walk != 0
               : mode == 3 ? (unsigned)(c->layer - 2) > 7 || c->walk
               : mode == 4 ? (unsigned)(c->layer - 3) > 6 || c->walk : 0;
        if (!ok) return 0;
        if (d <= i) return 1;
    }
    return 0;
}

// 0x443230：從格子 f 往 8 個方向各試 1～max−1 格，出界或「層 2～9 而且不能走」就停；
// 取能走最遠的方向（一樣遠時第 k 個以 1/(k+1) 機率換掉），回那方向擋住前一格。最遠的連 1 格都走不了（< 2）回 (−1, −1)
LATER static void open_dir(int *ox, int *oy, int fx, int fy, int max) {
    int run[8], px[8], py[8];
    for (int d = 0; d < 8; d++) {
        int k = 1;
        px[d] = fx, py[d] = fy;
        for (; k < max; k++) {
            int x = fx, y = fy;
            move_dir(&x, &y, d, k);
            const Cell *c = cell_at(x, y);
            if (!c || ((unsigned)(c->layer - 2) < 8 && !c->walk)) {
                if (k > 1) px[d] = fx, py[d] = fy, move_dir(&px[d], &py[d], d, k - 1);
                break;
            }
            px[d] = x, py[d] = y;
        }
        run[d] = k;
    }
    int b = 0;
    for (int d = 1; d < 8; d++)
        if (run[b] < run[d]) b = d;
        else if (run[d] == run[b] && rand() % (d + 1) == 0) b = d;
    if (run[b] < 2) *ox = *oy = -1;
    else *ox = px[b], *oy = py[b];
}

// 0x443c30：每次換下一個方向（全域 DAT_00625b28 0～7 輪流），從 (x, y) 往那方向移 dist
static int spread_dir;
LATER static void spread_pt(int *x, int *y, int dist) {
    if (++spread_dir > 7) spread_dir = 0;
    move_dir(x, y, spread_dir, dist);
}

// ---- 螺旋找格子 ----
// 0x443600：(x, y) 的 3×3 格裡有沒有 bld 這棟建築（格子旗標 +0x12 要有 0x88）
LATER static int bld_near(int x, int y, int bld) {
    for (int yy = y + 1; yy >= y - 1; yy--)
        for (int xx = x + 1; xx >= x - 1; xx--)
            if (cell_at(xx, yy) && cell_bld(xx, yy) == bld && bld >= 0) return 1;
    return 0;
}

// 0x443e80：(x, y) 周圍 n 圈（至少 1，不含中心）找建築種類 0xc、+0x5c = 0x96 的。回建築編號，沒有 −1
LATER static int near_bld96(int x, int y, int n) {
    Spiral s;
    spiral_init(&s, x, y, n > 0 ? n : 1);
    spiral_next(&s);
    while (spiral_next(&s)) {
        int b = cell_bld(s.x, s.y);
        if (cell_at(s.x, s.y) && b >= 0) return b;  // shortcut: 建築的種類、+0x5c 要等建築物件做了才看得到
    }
    return -1;
}

// 0x444130：(x, y) 周圍 n 圈（含中心）找第一個「不是 side 這邊放的、還有效（+0x9f）」的陷阱。
// 陷阱的隊伍 = 放的人目前的隊伍（0x49be00，放的人不在了就用放的時候記的）
// shortcut: 陷阱「還有效」（+0x9f）沒有對應的欄位，活著就當有效
LATER static Actor *near_trap(int x, int y, int n, int side) {
    Spiral s;
    spiral_init(&s, x, y, n > 0 ? n : 1);
    while (spiral_next(&s)) {
        if (!cell_at(s.x, s.y)) continue;
        for (int i = 0; i < actor_count; i++) {
            Actor *t = &actors[i];
            if (!alive(t) || !is_trap(t) || (int)t->x != s.x || (int)t->y != s.y) continue;
            int tside = t->owner && alive(t->owner) ? t->owner->side : t->side;
            if (tside != side) return t;
        }
    }
    return NULL;
}

// 0x444430：(x, y) 周圍 n 圈（至少 1，含中心）第一個能走、沒有道具也沒有角色的格子（放掉落的道具 0x49a350 用，n = 10）。
// 怪現象（照原版）：中心那格要沒有角色，往外的格子只看能走和沒有道具
static int free_cell(int *x, int *y, int n) {
    Spiral s;
    spiral_init(&s, *x, *y, n > 0 ? n : 1);
    spiral_next(&s);
    const Cell *c = cell_at(s.x, s.y);
    if (c && c->walk && !cell_prop(s.x, s.y) && !cell_actor0(s.x, s.y)) return 1;
    while (spiral_next(&s)) {
        c = cell_at(s.x, s.y);
        if (c && c->walk && !cell_prop(s.x, s.y)) { *x = s.x, *y = s.y; return 1; }
    }
    return 0;
}

// 0x444740：(x, y) 周圍 n 圈（含中心）第一個沒有道具、而且「不是層 2～9 或能走」的格子（放陷阱 0x49be40 用，n = 10）
static int trap_cell(int *x, int *y, int n) {
    Spiral s;
    spiral_init(&s, *x, *y, n > 0 ? n : 1);
    while (spiral_next(&s)) {
        const Cell *c = cell_at(s.x, s.y);
        if (c && ((unsigned)(c->layer - 2) > 7 || c->walk) && !cell_prop(s.x, s.y)) { *x = s.x, *y = s.y; return 1; }
    }
    return 0;
}
