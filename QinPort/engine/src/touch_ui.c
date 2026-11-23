// 手機觸控介面（橫式＋直式）。由 main.c 在 main() 之前引入。原版沒有觸控，整份是自訂設計（使用者 2026-11-19 定案），沒有原版位址。
// 畫面分三層：虛擬 4:3 遊戲畫面（vw×vh，不拉伸）、螢幕（邏輯尺寸 sw×sh，依視窗長寬比排）、虛擬按鈕疊在螢幕上。
//   橫式：遊戲置中、左右留空放控制（長寬比接近 4:3 時沒有留空，按鈕半透明疊在遊戲上）。
//   直式：遊戲貼上緣滿版寬，下方是控制區（搖桿在左、按鈕在右）。
// 輸入全走 SDL 手指事件（關掉觸控轉滑鼠，才能多點同時）：手指按下時決定用途（按鈕、搖桿、遊戲點擊），之後一直算那個用途。
// shortcut: 沒有 Playwright／實機，觸控事件鏈（手指事件 → 改寫成滑鼠事件）只靠編譯與單元測試驗證；版面用 --touch --win WxH --shot 看圖。

enum { TB_ATK, TB_SK0, TB_SK1, TB_SK2, TB_SK3, TB_RUN, TB_SYS, TB_BAG, TB_MAP, TB_HAND, TB_EQP, TB_ROLE, TB_SKL, TB_CLOSE, TB_N };
// TK_UI = 手指按在遊戲畫面裡的介面上（預覽放大鏡，放開才點）；TK_WIN = 手指按在放大顯示的側邊視窗區（直式）
enum { TK_NONE, TK_JOY, TK_BTN, TK_GAME, TK_PINCH, TK_UI, TK_WIN };
typedef struct { int x, y, r; } TCircle;
typedef struct {
    int sw, sh, portrait, u;  // 螢幕邏輯尺寸、是否直式、基本單位（按鈕大小）
    SDL_Rect game, joy;       // 遊戲畫面在螢幕上的位置、搖桿可以按下的區域
    TCircle jc;               // 搖桿圈：位置、大小與右邊攻擊鍵左右鏡像（固定位置）
    SDL_Rect ui_src, ui_dst;  // 直式開側邊視窗：視窗在遊戲座標的範圍 → 放大後在螢幕上的範圍（ui_dst.w = 0 = 沒有放大）
    float ui_s;               // 放大倍數
    TCircle b[TB_N];
} TLayout;

// 版面：ww×wh = 視窗（或網頁）實際大小，gw×gh = 遊戲畫面，size 0／1／2 = 小／中／大，swap = 左右手對調（搖桿換右、按鈕換左）。
// ws = 開著的側邊視窗在遊戲座標的範圍（NULL = 沒開）：橫式只多一顆「✕ 關閉」；直式視窗放大到螢幕寬、放在遊戲畫面下方，控制鍵縮到最下面兩排。
// 螢幕邏輯尺寸 = 遊戲畫面加上空出來的部分，邏輯 1 點 ≈ 實際 1 點（SDL 再整個縮放到視窗）
static void touch_layout(TLayout *L, int ww, int wh, int gw, int gh, int size, int swap, const SDL_Rect *ws) {
    if (ww < 1) ww = 1;
    if (wh < 1) wh = 1;
    memset(L, 0, sizeof *L);
    L->portrait = ww * 5 < wh * 4;  // 比 4:5 窄才算直式
    if (L->portrait) {
        L->sw = gw;
        L->sh = (int)((long)gw * wh / ww);
        if (L->sh < gh) L->sh = gh;
        L->game = (SDL_Rect){0, 0, gw, gh};
    } else {
        L->sh = gh;
        L->sw = (int)((long)gh * ww / wh);
        if (L->sw < gw) L->sw = gw;
        L->game = (SDL_Rect){(L->sw - gw) / 2, 0, gw, gh};
    }
    int sw = L->sw, sh = L->sh, u = 56 * (size == 0 ? 80 : size == 2 ? 125 : 100) / 100;
    if (L->portrait) {
        u = u * 135 / 100;  // 直式整個 800 點寬縮到手機寬，按鈕要放大才按得到
        int ch = sh - gh;
        if (u * 79 > ch * 10) u = ch * 10 / 79;  // 控制區太矮：縮小到放得下
    }
    L->u = u;
    TCircle *b = L->b;
    int ax = sw - u * 23 / 10, ay = sh - u * 23 / 10;
    b[TB_ATK] = (TCircle){ax, ay, u * 125 / 100};
    L->jc = (TCircle){sw - ax, ay, u * 125 / 100};  // 搖桿 = 攻擊鍵的鏡像
    for (int i = 0; i < 4; i++) {  // 技能鍵在攻擊鍵左上的弧上，從正左到正上
        double a = 3.14159265 + i * (3.14159265 / 2) / 3;
        b[TB_SK0 + i] = (TCircle){ax + (int)(u * 3.2 * cos(a)), ay + (int)(u * 3.2 * sin(a)), u * 8 / 10};
    }
    if (L->portrait) {  // 控制區一排由右到左：選單、背包、跑步、換手、大小
        int ry = ay - u * 47 / 10;  // 靠近拇指（技能弧上方）；控制區太矮時貼著控制區上緣
        if (ry < gh + u * 9 / 10) ry = gh + u * 9 / 10;
        // 固定的一排（由右到左）：選、包、裝、屬、技、圖、跑、換；位置不隨視窗開關改變（使用者要求固定）
        static const int ord[8] = {TB_SYS, TB_BAG, TB_EQP, TB_ROLE, TB_SKL, TB_MAP, TB_RUN, TB_HAND};
        int br = u * 5 / 10, step = (sw - 2 * br - 2 - u * 8 / 10 + br) / 7;  // 8 顆平均排滿寬度（最大尺寸也放得下）
        if (step > u * 11 / 10) step = u * 11 / 10;
        for (int i = 0; i < 8; i++) b[ord[i]] = (TCircle){sw - u * 8 / 10 - i * step, ry, br};
        int jy = ry + br + u / 4;  // 搖桿區在那一排按鈕下面（以前蓋到那排，被按鈕切成只剩左邊窄窄一條，往左右推不對稱）
        jy = L->jc.y - L->jc.r * 2;
        L->joy = (SDL_Rect){0, jy, L->jc.x + L->jc.r * 2, sh - jy};
    } else {
        b[TB_RUN] = (TCircle){sw - u * 95 / 100, ay - u * 37 / 10, u * 65 / 100};
        static const int ord[7] = {TB_SYS, TB_BAG, TB_EQP, TB_ROLE, TB_SKL, TB_MAP, TB_HAND};
        for (int i = 0; i < 7; i++) b[ord[i]] = (TCircle){sw - u * (8 + 12 * i) / 10, u * 8 / 10, u * 5 / 10};
        int m = L->game.x, zw = m > sw * 3 / 10 ? m : sw * 3 / 10;
        L->joy = (SDL_Rect){0, sh * 3 / 10, zw, sh - sh * 3 / 10};
    }
    if (ws && ws->w > 0 && ws->h > 0) {
        if (!L->portrait) b[TB_CLOSE] = (TCircle){u * 11 / 10, u * 11 / 10, u * 9 / 10};
        else {  // 直式：下面兩排放控制（上排 ✕ 與技能、下排攻擊／跑／選單），上方整塊給視窗
            int ch = sh - gh;
            u = L->u = u * 85 / 100;  // 視窗開著：控制鍵縮小一點，多留空間給放大的視窗
            if (u * 82 > ch * 6) u = L->u = ch * 6 / 82;  // 控制區要留出至少 40% 給視窗
            ax = sw - u * 17 / 10; ay = sh - u * 19 / 10;
            b[TB_ATK] = (TCircle){ax, ay, u * 125 / 100};
            L->jc = (TCircle){sw - ax, ay, u * 125 / 100};
            for (int i = 0; i < 4; i++) b[TB_SK0 + i] = (TCircle){sw - u * 12 / 10 - i * u * 19 / 10, sh - u * 47 / 10, u * 8 / 10};
            {  // 固定那一排八顆照樣顯示（放在技能列上方，與視窗關閉時順序相同）
                static const int ord[8] = {TB_SYS, TB_BAG, TB_EQP, TB_ROLE, TB_SKL, TB_MAP, TB_RUN, TB_HAND};
                int br = u * 5 / 10, step = (sw - 2 * br - 2 - u * 8 / 10 + br) / 7;
                if (step > u * 11 / 10) step = u * 11 / 10;
                for (int i = 0; i < 8; i++) b[ord[i]] = (TCircle){sw - u * 8 / 10 - i * step, sh - u * 69 / 10, br};
            }
            b[TB_CLOSE] = (TCircle){u * 14 / 10, sh - u * 47 / 10, u};
            L->joy = (SDL_Rect){0, sh - u * 35 / 10, sw - u * 64 / 10, u * 35 / 10};
            // 放大的視窗蓋在整個遊戲畫面上（原版視窗就是畫在主畫面上，以前縮在控制區上面只剩一小塊）：從螢幕上緣到控制鍵上方都可以用
            int zt = sh - u * 76 / 10, avail = zt;
            float sx = (float)sw / ws->w, sy = (float)avail / ws->h, s = sx < sy ? sx : sy;
            if (s > 3) s = 3;
            L->ui_src = *ws;
            L->ui_s = s;
            L->ui_dst = (SDL_Rect){(int)((sw - ws->w * s) / 2), (int)((avail - ws->h * s) / 2), (int)(ws->w * s), (int)(ws->h * s)};
        }
    }
    for (int i = 0; i < TB_N; i++)  // 搖桿區不能蓋到按鈕（左邊界取離搖桿區最近的按鈕左緣）
        if (b[i].r && b[i].y + b[i].r > L->joy.y && b[i].x - b[i].r < L->joy.x + L->joy.w) L->joy.w = b[i].x - b[i].r > 0 ? b[i].x - b[i].r : 0;
    if (swap) {
        for (int i = 0; i < TB_N; i++) b[i].x = sw - b[i].x;
        L->jc.x = sw - L->jc.x;
        L->joy.x = sw - L->joy.w;
    }
}

// 按到哪個按鈕（圓形，半徑放寬 15% 好按）；沒有 = -1，重疊取離圓心最近的
static int touch_hit(const TLayout *L, int x, int y) {
    int best = -1;
    long bd = 0;
    for (int i = 0; i < TB_N; i++) {
        if (!L->b[i].r) continue;
        long dx = x - L->b[i].x, dy = y - L->b[i].y, r = L->b[i].r * 115 / 100, d = dx * dx + dy * dy;
        if (d <= r * r && (best < 0 || d < bd)) { best = i; bd = d; }
    }
    return best;
}
static int touch_in(const SDL_Rect *r, int x, int y) { return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h; }

// 螢幕座標 → 介面座標（800x600 遊戲座標）。*win = 1：點在放大的側邊視窗區（座標已換算回視窗原尺寸）；0：在遊戲畫面上。都不在回 0
static int touch_ui_pt(const TLayout *L, int sx, int sy, int *ux, int *uy, int *win) {
    if (L->ui_dst.w > 0 && touch_in(&L->ui_dst, sx, sy)) {
        *ux = L->ui_src.x + (int)((sx - L->ui_dst.x) / L->ui_s);
        *uy = L->ui_src.y + (int)((sy - L->ui_dst.y) / L->ui_s);
        *win = 1;
        return 1;
    }
    *win = 0;
    if (!touch_in(&L->game, sx, sy)) return 0;
    *ux = sx - L->game.x;
    *uy = sy - L->game.y;
    return 1;
}
// 介面座標的矩形 → 螢幕座標
static SDL_Rect touch_ui_rect(const TLayout *L, int win, SDL_Rect r) {
    if (!win || L->ui_dst.w <= 0) { r.x += L->game.x; r.y += L->game.y; return r; }
    return (SDL_Rect){L->ui_dst.x + (int)((r.x - L->ui_src.x) * L->ui_s), L->ui_dst.y + (int)((r.y - L->ui_src.y) * L->ui_s), (int)(r.w * L->ui_s + 0.5f), (int)(r.h * L->ui_s + 0.5f)};
}
// 放大鏡（約 2.5 倍）的位置：手指上方；太靠上緣放不下就翻到手指下方；左右夾在螢幕內
#define LOUPE_ZOOM 2.5f
static SDL_Rect touch_loupe_rect(const TLayout *L, int fx, int fy) {
    int sz = L->u * 34 / 10, gap = L->u * 13 / 10;
    SDL_Rect r = {fx - sz / 2, fy - gap - sz, sz, sz};
    if (r.y < 0) r.y = fy + gap;
    if (r.x < 0) r.x = 0;
    if (r.x + sz > L->sw) r.x = L->sw - sz;
    if (r.y + sz > L->sh) r.y = L->sh - sz;
    return r;
}

// 搖桿向量 → 移動方向（畫面方向，長度 100；主迴圈的 mx、my 只看方向）。死區內回 0
static int touch_joy_vec(int dx, int dy, int dead, float *mx, float *my) {
    double d = sqrt((double)dx * dx + (double)dy * dy);
    if (d < dead || d <= 0) { *mx = *my = 0; return 0; }
    *mx = (float)(dx * 100 / d);
    *my = (float)(dy * 100 / d);
    return 1;
}
// 搖桿原點跟著手指：手指拉超過最大半徑時，原點被拉過去（手指永遠在圈內）
static void touch_joy_follow(int *ox, int *oy, int x, int y, int maxr) {
    double dx = x - *ox, dy = y - *oy, d = sqrt(dx * dx + dy * dy);
    if (d <= maxr || d <= 0) return;
    *ox = x - (int)(dx * maxr / d);
    *oy = y - (int)(dy * maxr / d);
}

// 搖桿半徑：1.4 個單位；搖桿區太窄放不下就縮到區寬的一半（左右對稱、圈不被切掉）
static int touch_joy_r(const TLayout *L) { return L->jc.r > 8 ? L->jc.r : 8; }
#define TF_MAX 6
typedef struct { Sint64 id; int used, kind, btn, x, y; } TFinger;
typedef struct {
    TLayout L;
    int size, swap, forced, kbm, blocked;  // forced = --touch（測試用，永遠顯示）；kbm = 偵測到鍵盤或滑鼠；blocked = 對話或選單開著
    Uint32 last_touch;
    TFinger f[TF_MAX];
    int held[TB_N];
    int joy_on, joy_ox, joy_oy, joy_x, joy_y;
    int pinch_d;
    int aim;  // 瞄準中的技能鍵（TB_SK0..3），-1 = 沒有
    SDL_Rect flash;  // 確認點擊的那個控制項（螢幕座標）閃一下
    Uint32 flash_until;
} Touch;
static Touch tch = {.size = 1, .aim = -1};

static int touch_active(void) { return (tch.forced || !tch.kbm) && !tch.blocked; }
static TFinger *touch_find(Sint64 id) {
    for (int i = 0; i < TF_MAX; i++) if (tch.f[i].used && tch.f[i].id == id) return &tch.f[i];
    return NULL;
}
// 按住狀態一律由還在按的手指推出來（不用加減計數：重複落下、漏掉放開都不會卡住）
static void touch_sync(void) {
    memset(tch.held, 0, sizeof tch.held);
    for (int i = 0; i < TF_MAX; i++) if (tch.f[i].used && tch.f[i].kind == TK_BTN) tch.held[tch.f[i].btn]++;
}
// 全部放開（選單打開、換版面時）
static void touch_release_all(void) {
    memset(tch.f, 0, sizeof tch.f);
    memset(tch.held, 0, sizeof tch.held);
    tch.joy_on = 0;
    tch.aim = -1;
}
static void touch_pinch_dist(int *d) {
    const TFinger *a = NULL, *b = NULL;
    for (int i = 0; i < TF_MAX; i++)
        if (tch.f[i].used && tch.f[i].kind == TK_PINCH) { if (!a) a = &tch.f[i]; else b = &tch.f[i]; }
    *d = a && b ? (int)hypot(a->x - b->x, a->y - b->y) : -1;
}

// 手指按下（螢幕邏輯座標）：決定用途。回 TK_BTN 時 *btn = 按鈕；TK_PINCH = 第二根手指落在遊戲畫面上，雙指縮放開始（第一根改為縮放用，呼叫端要取消按住走路）
static int touch_down(Sint64 id, int x, int y, int *btn) {
    TFinger *f = touch_find(id);
    if (f) { if (f->kind == TK_JOY) tch.joy_on = 0; f->used = 0; }  // 同一指重複落下（漏了放開）：先當放開
    f = NULL;
    for (int i = 0; i < TF_MAX && !f; i++) if (!tch.f[i].used) f = &tch.f[i];
    if (!f) return TK_NONE;
    *f = (TFinger){.id = id, .used = 1, .x = x, .y = y};
    int b = touch_active() ? touch_hit(&tch.L, x, y) : -1;
    if (b >= 0) { f->kind = TK_BTN; f->btn = b; touch_sync(); *btn = b; return TK_BTN; }
    if (touch_active() && !tch.joy_on && touch_in(&tch.L.joy, x, y)) {
        f->kind = TK_JOY;
        // 固定位置的搖桿：原點 = 搖桿圈中心（與攻擊鍵鏡像），手指在區內哪裡按都從圈心算，左右推的距離一樣
        int ox = tch.L.jc.x, oy = tch.L.jc.y;
        tch.joy_on = 1; tch.joy_ox = ox; tch.joy_oy = oy; tch.joy_x = x; tch.joy_y = y;
        return TK_JOY;
    }
    if (tch.L.ui_dst.w > 0 && touch_in(&tch.L.ui_dst, x, y)) { f->kind = TK_WIN; return TK_WIN; }  // 直式放大的側邊視窗（不套雙指縮放）
    if (touch_in(&tch.L.game, x, y)) {
        for (int i = 0; i < TF_MAX; i++)
            if (&tch.f[i] != f && tch.f[i].used && tch.f[i].kind == TK_GAME) {
                tch.f[i].kind = f->kind = TK_PINCH;
                touch_pinch_dist(&tch.pinch_d);
                return TK_PINCH;
            }
        f->kind = TK_GAME;
        return TK_GAME;
    }
    f->used = 0;
    return TK_NONE;
}
// 手指移動：回用途；TK_PINCH 時 *zoom 是這次要加的縮放量（張開 = 拉近）
static int touch_move(Sint64 id, int x, int y, float *zoom) {
    TFinger *f = touch_find(id);
    *zoom = 0;
    if (!f) return TK_NONE;
    f->x = x; f->y = y;
    if (f->kind == TK_JOY) {
        tch.joy_x = x; tch.joy_y = y;
        touch_joy_follow(&tch.joy_ox, &tch.joy_oy, x, y, touch_joy_r(&tch.L));
    } else if (f->kind == TK_PINCH) {
        int d;
        touch_pinch_dist(&d);
        if (d >= 0 && tch.pinch_d >= 0) *zoom = (d - tch.pinch_d) * 0.0015f;
        tch.pinch_d = d;
    }
    return f->kind;
}
static int touch_up(Sint64 id, int *btn) {
    TFinger *f = touch_find(id);
    if (!f) return TK_NONE;
    int k = f->kind;
    *btn = f->btn;
    if (k == TK_JOY) tch.joy_on = 0;
    f->used = 0;
    touch_sync();
    return k;
}
static void touch_kill(Sint64 id) { TFinger *f = touch_find(id); if (f) f->kind = TK_NONE; touch_sync(); }
static void touch_set_kind(Sint64 id, int k) { TFinger *f = touch_find(id); if (f) f->kind = k; }
// 搖桿目前的移動方向（沒按或在死區回 0）
static int touch_joy(float *mx, float *my) {
    *mx = *my = 0;
    if (!tch.joy_on || !touch_active()) return 0;
    return touch_joy_vec(tch.joy_x - tch.joy_ox, tch.joy_y - tch.joy_oy, tch.L.u * 25 / 100, mx, my);
}

// 第 n 個（0~3）技能鍵對應的技能：快捷技能視窗左手那一份（左右鍵位 1 或 2、等級 > 0）的前 4 個
// shortcut: 只有前 4 個，沒有翻頁；要更多技能請用主列的快捷視窗換左手技能
static int touch_skill_idx(int n) {
    int c = 0;
    for (int k = 0; k < hero_skill_n && k < 20; k++)
        if ((hero_skill[k].hand == 1 || hero_skill[k].hand == 2) && hero_skill[k].level > 0 && c++ == n) return k;
    return -1;
}
// 要不要先瞄準：要對著目標或地點放的技能（要敵人、同隊、地點、陷阱）。對自己的增益直接放。
// shortcut: 判斷用 skill_needs_foe／manual 近似（原版技能表 +0x36 旗標沒有欄位）
static int touch_skill_aims(const SkillRow *r) {
    int m = skill_manual(r);
    return skill_needs_foe(r) || m == 2 || m == 3 || m == 5;
}

// 攻擊鍵／空白鍵：選最近的敵人當目標（右手技能由追打放；拿弓 = 射箭）
static Object *hover_building(int cx, int cy);
static int click_building(Actor *h, Object *b, int cx, int cy);
static int touch_attack_building_done;  // 這次按住已經對建築出過手（一次按住只出一次，不然每拍重送命令）
static void touch_attack_nearest(Actor *h) {
    float best = h->def ? actor_seek(h) : 10;
    Actor *found = NULL;
    for (int i = 1; i < actor_count; i++)
        if (hostile(h, &actors[i]) && cell_dist(h, &actors[i]) <= best) { best = cell_dist(h, &actors[i]); found = &actors[i]; }
    if (found) hero_attack(h, found);
    if (found || touch_attack_building_done || !unit_scene) return;
    // 沒有敵人：攻擊鍵 = 原版「點游標下的東西」。最近的可互動建築（樹、礦、水缸、箱子、門）8 格內，照原版點建築的做法（click_building）
    // shortcut: 手機沒有滑鼠游標，攻擊鍵代替點擊；原版是點到哪個建築就對哪個
    int hx = (int)h->x, hy = (int)h->y; Object *pick = NULL; int pcx = 0, pcy = 0, pd = 1 << 30;
    for (int i = 0; i < unit_scene->object_count; i++) {
        Object *o = (Object *)&unit_scene->objects[i];
        int ty = o->m ? o->m->type : 0;
        if (!(ty == 0x8c || ty == 0x81 || ty == 0x80 || ty == 0x85 || door_is_go(o))) continue;  // 只挑點了會有動作的（click_building 對一般建築什麼都不做卻回 1，會擋住後面的撿東西／說話）
        // 建築占的是一塊格子（錨點格不一定是它的格子，門尤其如此）：掃占地格，取離主角最近、游標會選到它的那格
        int x0 = o->x - o->m->anchor_x + o->m->grid_x, y0 = o->y - o->m->anchor_y + o->m->grid_y, w = o->m->grid_w > 0 ? o->m->grid_w : 1, h2 = o->m->grid_h > 0 ? o->m->grid_h : 1;
        for (int y = y0; y < y0 + h2; y++)
            for (int x = x0; x < x0 + w; x++) {
                int dx = x - hx, dy = y - hy, d = dx * dx + dy * dy;
                if (d > 25 || hover_building(x, y) != o) continue;
                if (door_is_go(o)) d /= 4;  // 門優先（不然站在樹旁永遠進不了屋）；門 5 格內、其他東西只認 2 格內（不要被遠處的罐子拉過去）
                else if (d > 4) continue;
                if (d >= pd) continue;
                pick = o; pcx = x; pcy = y; pd = d;
            }
    }
    if (pick && click_building(h, pick, pcx, pcy)) { touch_attack_building_done = 1; return; }
    // 附近沒有建築可用：撿 3 格內最近的地上道具（原版命令 0x11 = 點地上道具）。挖礦後道具掉在身邊，常被右側隊伍欄／技能鍵蓋住點不到
    int gb = -1; float gd = 9;
    for (int i = 0; i < ground_n; i++) { float dx = ground[i].x - h->x, dy = ground[i].y - h->y, d = dx * dx + dy * dy; if (d <= gd) { gd = d; gb = i; } }
    if (gb >= 0) { actor_command(h, 0x11, ground[gb].id, 0, 0, 1, 1); h->foe = NULL; h->want = NULL; touch_attack_building_done = 1; return; }
    // 最後：3 格內最近的人 = 點人說話（原版命令 0xf）
    Actor *np = NULL; float nd = 9;
    for (int i = 1; i < actor_count; i++) {
        Actor *n = &actors[i];
        if (n->mon || !n->role || n->act == ACT_DEAD || n->team || is_foe(n, h) || has_status(n, 103)) continue;
        float dx = n->x - h->x, dy = n->y - h->y, d = dx * dx + dy * dy;
        if (d <= nd) { nd = d; np = n; }
    }
    if (np) { actor_command(h, 0xf, np->sh ? np->sh : (int)np->object_id, 0, 0, 1, 1); h->foe = NULL; h->want = NULL; touch_attack_building_done = 1; }
}

// 瞄準模式下點了畫面 (gx, gy)（遊戲畫面座標）：放左手技能。點到怪物就對牠；點空地找 3 格內最近的敵人，都沒有就對該點
// shortcut: 空地「找附近的敵人」是為了範圍魔法在手機上好點，原版是滑鼠點哪放哪
static void touch_aim_cast(Actor *h, int gx, int gy, int ox, int oy, Scene *sc) {
    int zx = gx, zy = gy, cx, cy;
    zoom_point(&zx, &zy);
    Actor *m = input_char(zx, zy, ox, oy, sc, &cx, &cy);
    SDL_Point wp = viewport_screen_world(&viewport, zx, zy);
    if (!m) {
        double best = 3.0 * 64;
        for (int i = 1; i < actor_count; i++) {
            double d = hypot(ax64(&actors[i]) - wp.x, ay64(&actors[i]) - wp.y);
            if (hostile(h, &actors[i]) && d <= best) { best = d; m = &actors[i]; }
        }
    }
    if (!input_cast(wp.x, wp.y, m)) {  // 選人名單空時（還沒進場景）直接給主角
        if (m) actor_target_set(h, m);
        hero_cast_point(h, wp.x, wp.y);
    }
}

static int run_on;  // 跑步旗標（R 鍵與跑步鍵共用）
static Win *win_last;  // 最後開的側邊視窗（✕ 關閉先關它）
static void touch_win_track(void) {
    static int pl, pr;
    if (win_left.open && !pl) win_last = &win_left;
    if (win_right.open && !pr) win_last = &win_right;
    pl = win_left.open; pr = win_right.open;
}

// 按鈕按下（只有一次，不含按住的攻擊鍵）
static void touch_button(Actor *h, int b) {
    if (b >= TB_SK0 && b <= TB_SK3) {
        int k = touch_skill_idx(b - TB_SK0);
        if (tch.aim == b) { tch.aim = -1; return; }  // 再按一次 = 取消瞄準
        tch.aim = -1;
        if (k < 0 || h->act == ACT_DEAD) return;
        if (cur_skill[1] != k) { cur_skill[1] = k; hero_skill_apply(h); }
        const SkillRow *r = &hero_skill[k];
        int m = skill_manual(r);
        // 已經有目標、單體技能：直接對目標放，不用再點
        if (touch_skill_aims(r) && !(skill_needs_foe(r) && alive(h->foe) && m != 2 && m != 3 && m != 5)) tch.aim = b;
        else hero_cast(h);
    } else if (b == TB_RUN) input_side_command(run_on = !run_on);
    else if (b == TB_SYS) {
        if (scene_paused) { do scene_pause_count(0); while (scene_paused); }  // 同 Esc
        else sys_toggle();
    } else if (b == TB_CLOSE) {  // 關掉最上層（最後開的）側邊視窗
        Win *w = win_last && win_last->open ? win_last : win_left.open ? &win_left : &win_right;
        w->open = 0;
    } else if (b == TB_BAG) win_toggle(&win_right, JM54);
    else if (b == TB_MAP) map_show();
    else if (b == TB_EQP) win_toggle(&win_left, JM55);
    else if (b == TB_ROLE) win_toggle(&win_left, JM52);
    else if (b == TB_SKL) { skill_mode = 1; win_toggle(&win_right, JM53); }
    else if (b == TB_HAND) { tch.swap ^= 1; tch.L.sw = 0; }                 // sw = 0 讓主迴圈重排版
}
// 每幀：按住攻擊鍵就一直打最近的敵人（目標死了或沒有才重選，免得每幀換目標）
static void touch_frame(Actor *h) {
    touch_sync();
    if (!touch_active()) return;
    if (!tch.held[TB_ATK]) touch_attack_building_done = 0;
    if (tch.held[TB_ATK] && h->act != ACT_DEAD && !alive(h->foe)) touch_attack_nearest(h);
}

// ---- 繪圖（螢幕座標，在遊戲畫面貼好之後畫）----
static void touch_ring(int cx, int cy, int ro, int ri, Uint8 r, Uint8 g, Uint8 bl, Uint8 a) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, r, g, bl, a);
    for (int y = -ro; y <= ro; y++) {
        int xo = (int)sqrt((double)ro * ro - (double)y * y);
        int xi = ri > 0 && abs(y) < ri ? (int)sqrt((double)ri * ri - (double)y * y) : -1;
        if (xi < 0) SDL_RenderDrawLine(ren, cx - xo, cy + y, cx + xo, cy + y);
        else {
            SDL_RenderDrawLine(ren, cx - xo, cy + y, cx - xi, cy + y);
            SDL_RenderDrawLine(ren, cx + xi, cy + y, cx + xo, cy + y);
        }
    }
}
static void touch_label(int cx, int cy, const char *t) { draw_text(cx - text_width(t) / 2, cy - 8, t, 200, 0xFFFFFF, 1); }
static void touch_draw(void) {
    if (!touch_active()) return;
    const TLayout *L = &tch.L;
    int u = L->u;
    if (tch.joy_on) {
        touch_ring(tch.joy_ox, tch.joy_oy, touch_joy_r(L), touch_joy_r(L) * 13 / 14, 255, 255, 255, 110);
        touch_ring(tch.joy_ox, tch.joy_oy, touch_joy_r(L), 0, 0, 0, 0, 50);
        int dx = tch.joy_x - tch.joy_ox, dy = tch.joy_y - tch.joy_oy;
        touch_ring(tch.joy_ox + dx, tch.joy_oy + dy, u * 6 / 10, 0, 255, 255, 255, 120);
    } else {  // 沒按時畫一圈淡淡的提示，告訴玩家搖桿在這一帶
        int cx = L->jc.x, cy = L->jc.y;  // 提示圈 = 攻擊鍵的鏡像（一樣大、一樣位置）
        touch_ring(cx, cy, touch_joy_r(L), touch_joy_r(L) * 13 / 14, 255, 255, 255, 50);
    }
    static const char *label[TB_N] = {"\xe6\x94\xbb", "1", "2", "3", "4", "\xe8\xb7\x91", "\xe9\x81\xb8", "\xe5\x8c\x85", "\xe5\x9c\x96", "\xe6\x8f\x9b", "\xe8\xa3\x9d", "\xe5\xb1\xac", "\xe6\x8a\x80"};  // 攻 1 2 3 4 跑 選 包 圖 換 裝 屬 技
    for (int i = 0; i < TB_N; i++) {
        const TCircle *c = &L->b[i];
        if (!c->r) continue;
        if (i == TB_CLOSE) {  // 醒目的紅色 ✕ 關閉
            touch_ring(c->x, c->y, c->r, 0, 200, 40, 40, tch.held[i] > 0 ? 230 : 170);
            touch_ring(c->x, c->y, c->r, c->r - 3, 255, 255, 255, 220);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            int h = c->r * 4 / 10, oy = -c->r / 5;
            for (int k = -h; k <= h; k++) {  // 用小方塊拼出 ✕（斜線在某些繪圖後端會不見）
                SDL_Rect a = {c->x + k - 2, c->y + oy + k - 2, 5, 5}, b2 = {c->x + k - 2, c->y + oy - k - 2, 5, 5};
                SDL_RenderFillRect(ren, &a);
                SDL_RenderFillRect(ren, &b2);
            }
            draw_text(c->x - text_width("\xe5\x85\xb3\xe9\x97\xad") / 2, c->y + c->r / 2 - 4, "\xe5\x85\xb3\xe9\x97\xad", 64, 0xFFFFFF, 1);  // 关闭（簡體：遊戲字型只有 GBK 字）
            continue;
        }
        int on = tch.held[i] > 0 || tch.aim == i, k = i >= TB_SK0 && i <= TB_SK3 ? touch_skill_idx(i - TB_SK0) : -1;
        int dim = i >= TB_SK0 && i <= TB_SK3 && (k < 0 || !skill_usable(&actors[0], &hero_skill[k]));
        touch_ring(c->x, c->y, c->r, 0, on ? 255 : 0, on ? 255 : 0, on ? 255 : 0, on ? 120 : 90);
        touch_ring(c->x, c->y, c->r, c->r - 3, tch.aim == i ? 255 : 255, tch.aim == i ? 220 : 255, tch.aim == i ? 80 : 255, dim ? 80 : 170);
        const Sprite *ic = k >= 0 ? ui_image(hero_skill[k].icon, 0) : NULL;
        if (ic && ic->tex) {
            SDL_Rect d = {c->x - ic->w / 2, c->y - ic->h / 2, ic->w, ic->h};
            SDL_SetTextureColorMod(ic->tex, dim ? 100 : 255, dim ? 100 : 255, dim ? 100 : 255);
            SDL_RenderCopy(ren, ic->tex, NULL, &d);
            SDL_SetTextureColorMod(ic->tex, 255, 255, 255);
            draw_text(c->x - c->r * 7 / 10, c->y - c->r * 8 / 10, label[i], 20, 0xFFFF80, 1);  // 左上角小號碼
        } else touch_label(c->x, c->y, i == TB_RUN && run_on ? "\xe8\xb5\xb0" : label[i]);  // 跑步開著時顯示「走」＝再按變回走路
    }
    if (tch.aim >= 0) touch_label(L->game.x + L->game.w / 2, 24, "\xe9\xbb\x9e\xe4\xb8\x80\xe4\xb8\x8b\xe7\x9e\x84\xe6\xba\x96");  // 點一下瞄準
}

// ---- 預覽放大鏡：手指按在介面上，在手指上方放大顯示手指周圍並框出放開會點到的控制項；放開才點 ----
static const TFinger *touch_ui_finger(void) {
    for (int i = 0; i < TF_MAX; i++) if (tch.f[i].used && (tch.f[i].kind == TK_UI || tch.f[i].kind == TK_WIN)) return &tch.f[i];
    return NULL;
}
// 放開時：手指所在位置（螢幕座標）若會點到某個控制項 → 閃一下、震動、真的點下去；滑出介面或沒有控制項 = 取消。回 1 = 有點
static int modal_rect(SDL_Rect *r);
// 點偏了也吸附到附近的控制項：手指點在視窗內、沒點到控制項時，在半徑約 0.45 個單位（螢幕座標）內找最近的控制項，把點移過去
// （原版視窗在手機寬度下控制項只有約 14 點寬，手指點不準；不放大視窗、視窗不會跑）
static int touch_snap(int *ux, int *uy, int win, int side) {
    SDL_Rect r;
    if (hud_target(*ux, *uy, side, &r) == 1) return 1;
    float sc = win && tch.L.ui_s > 0 ? tch.L.ui_s : 1.0f;
    int maxr = (int)(tch.L.u * 45 / 100 / sc);
    for (int rad = 4; rad <= maxr; rad += 4)
        for (int k = 0; k < 16; k++) {
            double a = k * 3.14159265358979 / 8;
            int px = *ux + (int)lround(rad * cos(a)), py = *uy + (int)lround(rad * sin(a));
            if (hud_target(px, py, side, &r) == 1) { *ux = px; *uy = py; return 1; }
        }
    return 0;
}
static int touch_ui_release(int sx, int sy) {
    int ux, uy, win;
    SDL_Rect r, mr;
    if (!touch_ui_pt(&tch.L, sx, sy, &ux, &uy, &win)) return 0;
#ifdef STORY
    if (win && dialog_open) { story_begin(); dialog_tap(uy); story_end(); return 1; }  // 放大的對話框：整列選項可點
#endif
    int side = win && !modal_rect(&mr);  // 放大的是居中小視窗（系統選單、存讀檔…）時，點擊走一般介面（hud_tap），不是側邊視窗（win_tap）
    int tg = touch_snap(&ux, &uy, win, side) ? 1 : hud_target(ux, uy, side, &r);
    if (tg == 1) hud_target(ux, uy, side, &r);  // 吸附後的點的控制項框（亮框用）
    if (tg == 0 && !side && modal_rect(&mr)) { js_vibrate(15); hud_tap(ux, uy); return 1; }  // 地圖、選項／說明、創角不套預覽（hud_target 回 0）：放大顯示時放開就當一般點擊（2026-11-21 前直式點不到）
    if (tg != 1) return 0;
    tch.flash = touch_ui_rect(&tch.L, win, r);
    tch.flash_until = SDL_GetTicks() + 160;
    js_vibrate(15);
    if (side) win_tap(ux, uy); else hud_tap(ux, uy);
    return 1;
}
static void touch_box(SDL_Rect r, int th, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, R, G, B, A);
    for (int i = 0; i < th; i++) { SDL_Rect q = {r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i}; SDL_RenderDrawRect(ren, &q); }
}
static void touch_preview_draw(void) {
    const TLayout *L = &tch.L;
    Uint32 now = SDL_GetTicks();
    if (now < tch.flash_until && tch.flash.w > 0) {  // 確認：該控制項短暫反白
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 170);
        SDL_RenderFillRect(ren, &tch.flash);
    }
    const TFinger *f = touch_ui_finger();
    if (!f) return;
    int ux, uy, win, hit = 0;
    SDL_Rect hr = {0}, sr = {0};
    SDL_Rect mr_;
    if (touch_ui_pt(L, f->x, f->y, &ux, &uy, &win) && touch_snap(&ux, &uy, win, win && !modal_rect(&mr_)) && hud_target(ux, uy, win && !modal_rect(&mr_), &hr) == 1) { hit = 1; sr = touch_ui_rect(L, win, hr); }
    if (hit) touch_box(sr, 3, 255, 230, 60, 255);  // 亮框
    SDL_Rect lp = touch_loupe_rect(L, f->x, f->y);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &lp);
    SDL_RenderSetClipRect(ren, &lp);
    float cx = lp.x + lp.w / 2.0f, cy = lp.y + lp.h / 2.0f, m = LOUPE_ZOOM;
#define LP(R) ((SDL_Rect){(int)(cx + ((R).x - f->x) * m), (int)(cy + ((R).y - f->y) * m), (int)((R).w * m + 0.5f), (int)((R).h * m + 0.5f)})
    SDL_Rect g = LP(L->game);
    SDL_RenderCopy(ren, fade_canvas, NULL, &g);
    if (L->ui_dst.w > 0 && (modal_cleared || win_tex)) {  // 放大區的來源：居中小視窗畫在 modal_tex（不在 canvas 上，放大鏡以前只抄 canvas 所以一片黑），側邊視窗畫在 win_tex
        SDL_Rect w = LP(L->ui_dst);
        SDL_RenderCopy(ren, modal_cleared ? modal_tex : win_tex, &L->ui_src, &w);
    }
    if (hit) { SDL_Rect h = LP(sr); touch_box(h, 3, 255, 230, 60, 255); }
#undef LP
    SDL_RenderSetClipRect(ren, NULL);
    touch_box(lp, 3, 255, 255, 255, 255);
    SDL_SetRenderDrawColor(ren, 255, 60, 60, 255);  // 手指位置（放大鏡中心）
    SDL_RenderDrawLine(ren, (int)cx - 8, (int)cy, (int)cx + 8, (int)cy);
    SDL_RenderDrawLine(ren, (int)cx, (int)cy - 8, (int)cx, (int)cy + 8);
}
