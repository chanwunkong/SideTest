// 天氣（原版 Poq.exe：CGodServer 0x44e5d0～0x44eaa0 擲天氣、CGodClient 0x4697b0/0x468c10 換天氣與每拍、
// 粒子 CRAIN 0x444c60～0x445810、CSnow 0x445a10～0x446160、CGale 0x43f650～0x43fe50）。
// main.c 直接 #include（要用 ren、vw、vh、units）。粒子都在畫面座標。
// 天氣編號：0 晴、1 大雨、2 小雨、3 紅雨、4 大雪、5 小雪、6 冰雹、7 紅天、8 粉天、9 白天（6~9 原版沒有畫面效果）、10 疾風
// shortcut: 光影（亮度 +0x134/+0x138、日夜表、下雨前變暗、閃電變亮、紅雨的紅色調 0x4a5880）還沒做，粒子顏色都用最亮（16）；
// 閃電只用白色半透明蓋一層；依場景種類（G_Type）的環境音（0x468c10 後半）還沒做

#define W_MAX 512                          // 粒子上限（原版 256 × 200%）
static const Uint8 W_TONE[17] = {60, 63, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 155, 160, 160};  // 0x5ecf54 = 0x5ecc00
static const int W_SIZE[8][2] = {{3, 3}, {3, 2}, {2, 3}, {2, 2}, {3, 1}, {2, 1}, {1, 2}, {1, 1}};  // 粒子種類 → 寬高（8 = 不變）

typedef struct { int life, kind, x, y, sx, sy, w, h, dx, dy, sway; } Particle;  // SParticle：+4 壽命、+8 種類、+0xc 位置、+0x14 來源矩形、+0x28 速度

static struct {
    int cum[10], total;                    // CGodServer +0x24 累加機率（[0] = 晴）、+0x20 總和
    int srv_idx, srv_cd;                   // 伺服器端目前天氣、倒數
    int idx, left;                         // 客戶端 +4 天氣、+0xc 剩幾拍
    int phase, vol, flash, thunder, big;   // +8 雨的階段、雨聲衰減（百分之一分貝，0x627d40）、+0x1c4 閃電、+0x1c0 打雷、+0x158
    Particle p[W_MAX];
    int n;                                 // 粒子數（+4）
    int rain_live, rain_amt, rain_grow, rain_red, splash;  // CRAIN +0x28 活著幾個、+0x30 雨量、+0x2c 漸大、+0x38 紅雨、0x65ea84
    int snow_on, snow_big, wind_mode, wind;               // CSnow +0x24 下的片數、+0x18 大雪、+0x1c 風向、+0x20 風
    SDL_Texture *tex[3];                   // 雨：3 條漸層；雪、疾風：tex[0] 3×3
    int tex_kind;                          // 貼圖畫的是哪種（1 雨、2 紅雨、4 雪、10 疾風）
    int cam_x, cam_y;                      // 上次畫的鏡頭位置（濺水換算世界座標用）
} wx;

#ifdef STORY
// 天氣機率表（0x516 → 0x44e5d0）：大雨、小雨、紅雨、大雪、小雪、冰雹、紅天、粉天、白天各幾 %，不到 100 的是晴；馬上重擲
static void weather_table(const int pct[9]) {
    int sum = 0;
    for (int i = 0; i < 9; i++) wx.cum[i + 1] = sum += pct[i];
    wx.total = sum;
    wx.cum[0] = 0;
    if (sum < 0x65) {
        wx.cum[0] = 100 - sum, wx.total = 100;
        for (int i = 1; i < 10; i++) wx.cum[i] += wx.cum[0];
    }
    wx.srv_cd = 0;
}
#endif

static int weather_count(void) { return (vw == 640 ? 100 : vw == 800 ? 0x9c : 0x100) * 200 / 100; }  // 0x444bc0(200)

// 粒子貼圖（雨 0x445810、雪 0x446160、疾風 0x43fe50）。b = 亮度 0~16
static void weather_tex(int kind, int b) {
    for (int k = 0; k < 3; k++) if (wx.tex[k]) SDL_DestroyTexture(wx.tex[k]), wx.tex[k] = NULL;
    wx.tex_kind = kind;
    Uint32 px[30];
    if (kind <= 3) {  // 雨：寬 2 高 10，每條往下每列加 4k
        for (int k = 0; k < 3; k++) {
            int red = kind == 3, r = red ? 255 : W_TONE[b], g = W_TONE[b], bl = red ? W_TONE[b] : W_TONE[b] - 3 * b + 0x2d;
            for (int y = 0; y < 10; y++) {
                for (int x = 0; x < 2; x++) px[y * 2 + x] = 0xff000000u | (Uint32)(r & 255) << 16 | (Uint32)(g & 255) << 8 | (Uint32)(bl & 255);
                if (!red) r += 4 * k;
                g += 4 * k, bl += 4 * k;
            }
            wx.tex[k] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 2, 10);
            if (wx.tex[k]) SDL_UpdateTexture(wx.tex[k], NULL, px, 2 * 4);
        }
        return;
    }
    for (int y = 0; y < 3; y++) {
        int r = W_TONE[b] / 2 + 0x1e + rand() % 0x32, g = W_TONE[b] / 2 + 0x1e + rand() % 0x32, bl = rand() % 0x32;  // 疾風：每列一色（沙黃）
        for (int x = 0; x < 3; x++) {
            if (kind != 10) r = rand() % 0x32 + 0x96, g = rand() % 0x32 + 0x96, bl = rand() % 0x32 + 0x96;  // 雪：每點隨機灰
            px[y * 3 + x] = 0xff000000u | (Uint32)r << 16 | (Uint32)g << 8 | (Uint32)bl;
        }
    }
    if (kind != 10) px[4] = 0xffffffffu;  // 雪花中心白
    wx.tex[0] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 3, 3);
    if (wx.tex[0]) SDL_UpdateTexture(wx.tex[0], NULL, px, 3 * 4);
}

// ---- 雨（CRAIN）----
static void splash_new(int sx, int sy, int red);

static void rain_drop(void) {  // 0x444e50
    if (wx.rain_live >= wx.n) return;
    int i = 0;
    while (i < wx.n && wx.p[i].life > 0) i++;
    if (i == wx.n) return;
    Particle *d = &wx.p[i];
    int top = vh - 0x96;
    d->life = rand() % 0x14 + 2;
    d->x = rand() % vw;
    d->y = rand() % top - rand() % 0x96;
    int sp = wx.rain_amt < 0x14 ? rand() % 7 + 3 : rand() % 10 - rand() % 5;
    if (sp < 0) sp = 0;
    d->kind = rand() % 3;
    d->sx = (unsigned)(sp - 3) < 2 ? rand() % 2 : 1;
    d->w = 2 - d->sx;
    d->sy = sp ? rand() % (sp + 1) : 0;
    d->h = 10 - sp;
    d->dx = rand() % 2 + rand() % 3 - 1;
    d->dy = 10 + 4 + (rand() % 6 - sp);
    wx.rain_live++;
    if (2000 / wx.rain_live < wx.splash++) {  // 地上濺水（0x445180）
        wx.splash = 0;
        splash_new(rand() % vw, rand() % vh, wx.rain_red);
    }
}

static void rain_tick(void) {  // 0x445600
    for (int i = 0; i < wx.n; i++) {
        Particle *d = &wx.p[i];
        if (d->life <= 0) continue;
        d->x += d->dx, d->y += d->dy;
        if (--d->life < 1) wx.rain_live--;
    }
    if (wx.rain_amt < 2) {
        if (wx.rain_live < 0) wx.rain_amt = -1;
        return;
    }
    for (int k = rand() % wx.rain_amt; k > 0; k--) rain_drop();
    if (!wx.rain_grow) { if (rand() % 10 == 3) wx.rain_amt--; }
    else if (wx.rain_amt < 0x1f && rand() % 0x14 == 3) wx.rain_amt++;
}

static void rain_reset(void) {  // 0x4457a0(1)：全部收掉、雨量 −1（停）
    for (int i = 0; i < W_MAX; i++) wx.p[i].life = -1;
    wx.rain_live = wx.splash = 0;
    wx.rain_amt = -1;
}

// 雨聲：衰減 vol（百分之一分貝）換成音量百分比
static int rain_pct(void) { return (int)(100 * powf(10, -wx.vol / 2000.0f)); }

// ---- 雪（CSnow）----
static void flake_tick(Particle *f) {  // 0x445e70。shortcut: 反編譯看不出移動完有沒有 return，這裡當成有（活著就移動，死了才重生）
    if (f->life > 0) {
        if (f->sway == 0) {
            f->sway = rand() % 0xf + 5;
            f->dx = -f->dx;
            f->y += f->dy;
            f->life--;
        }
        f->x += f->dx;
        f->sway--;
        if (wx.snow_big) f->x += wx.wind;
        f->y += f->dy;
        f->life--;
        return;
    }
    f->kind = wx.snow_big ? rand() % 9 : rand() % 3 + 5;
    f->sx = f->sy = 0;
    if (f->kind < 8) f->w = W_SIZE[f->kind][0], f->h = W_SIZE[f->kind][1];
    f->life = rand() % 500 + 500;
    f->dx = rand() % 4;
    f->sway = rand() % 0xf + 5;
    if (rand() % 2 == 1) f->sway = -f->sway;
    f->dy = rand() % (9 - f->kind) + 1;
    f->x = rand() % vw;
    f->y = rand() % vh;
}

static void snow_tick(int left) {  // 0x445d80
    if (rand() % 200 == 1) wx.wind_mode = rand() % 3;
    if (rand() % 10 == 1) {
        if (wx.wind_mode == 0 && wx.wind > -8) wx.wind--;
        else if (wx.wind_mode == 1 && wx.wind) wx.wind += wx.wind > 0 ? -1 : 1;
        else if (wx.wind_mode == 2 && wx.wind < 8) wx.wind++;
    }
    if (left < wx.n && wx.snow_on > 10) wx.snow_on--;
    else if (wx.snow_on < wx.n) wx.snow_on++;
    for (int i = 0; i < wx.snow_on; i++) flake_tick(&wx.p[i]);
}

// ---- 疾風（CGale）----
static void gale_tick(void) {  // 0x43fa20
    for (int i = 0; i < wx.n; i++) {
        Particle *g = &wx.p[i];
        if (g->life > 0) {
            g->x += g->dx, g->y += g->dy, g->life--;
            continue;
        }
        g->kind = rand() % 9;
        g->sx = g->sy = 0;
        if (g->kind < 8) g->w = W_SIZE[g->kind][0], g->h = W_SIZE[g->kind][1];
        g->life = rand() % 500 + 500;
        g->dx = -2 - rand() % (9 - g->kind);
        g->dy = -1 - rand() % (9 - g->kind) / 2;
        g->x = rand() % vw;
        g->y = rand() % vh;
    }
}

// ---- 客戶端：換天氣（0x4697b0）----
static void weather_set(int idx, int dur) {
    int old = wx.idx;
    if (old > 0 && old < 4) audio_loop(0, 0);   // 停雨聲（0x480740(0)）
    else if (old == 10) audio_loop(1, 0);        // 停風聲
    wx.idx = idx;
    wx.left = dur;
    if (old == idx) return;
    wx.n = weather_count();
    memset(wx.p, 0, sizeof wx.p);
    if (idx >= 1 && idx <= 3) {
        wx.phase = 0;
        wx.flash = wx.thunder = 0;
        rain_reset();
        wx.big = idx != 2;
        weather_tex(idx == 3 ? 3 : 1, 16);
    }
    else if (idx == 4 || idx == 5) {
        wx.snow_big = idx != 5;
        wx.snow_on = 0;
        wx.wind_mode = 1, wx.wind = 0;
        weather_tex(4, 16);
    }
    else if (idx == 10) {
        audio_loop(1, 100);  // 風聲（0x480350(1, 1)）
        weather_tex(10, 16);
    }
}

// 每拍：伺服器端倒數到了擲表（0x44e8f0），客戶端倒數到了回晴（0x44e8d0），再推目前天氣（0x468c10）
static void weather_tick(void) {
    if (--wx.srv_cd < 1 && wx.total > 0) {
        int r = rand() % wx.total, i = 0;
        while (i < 10 && wx.cum[i] < r) i++;
        if (i < 10) {
            int dur = rand() % 6000 + 5000;
            wx.srv_cd = dur;
            if (i != wx.srv_idx) wx.srv_idx = i, weather_set(i, dur);
        }
    }
    if (wx.left-- < 1) weather_set(0, 1000);
    switch (wx.idx) {
    case 1: case 2: case 3:
        if (wx.phase == 0) wx.phase = 1;  // shortcut: 原版等畫面暗到亮度 < 7（每 30 拍暗一級）才開始
        else if (wx.phase == 1) {
            if (rand() % 400 == 3 || !wx.big) {  // 小雨馬上下，大雨、紅雨隨機等一下
                wx.rain_red = wx.idx == 3, wx.rain_grow = 1, wx.rain_amt = 2;  // 0x4453f0
                wx.phase = 2;
                wx.vol = 0x14;
                audio_loop(0, rain_pct());
            }
        }
        else if (wx.phase == 2) {
            if (wx.left % 0x14 == 1 && wx.vol >= 140) wx.vol -= 140, audio_loop(0, rain_pct());
            if (wx.left < 0x191) wx.phase = 3, wx.rain_grow = 0;  // 0x445470：剩 400 拍開始收
        }
        else if (wx.left % 0x14 == 1 && wx.vol < 1400) wx.vol += 140, audio_loop(0, rain_pct());
        if (wx.idx != 3 && (wx.phase == 1 ? rand() % 100 == 2 : wx.phase == 2 && rand() % 300 == 2) && wx.flash < 1) {  // 閃電 0x469700
            wx.flash = rand() % 4 + 3;
            if (rand() % 3 == 1) wx.thunder = rand() % 0x28 + 0x1e;
        }
        if (wx.flash > 0) wx.flash--;
        if (wx.thunder > 0 && --wx.thunder == 0) audio_sfx(6);
        if (wx.phase) rain_tick();
        break;
    case 4: case 5: snow_tick(wx.left); break;
    case 10: gale_tick(); break;
    }
}

// 畫（雨 0x445700、雪 0x4460b0、疾風 0x43fda0）：雪、疾風整個在畫面外的重生
static void weather_draw(int ox, int oy) {
    wx.cam_x = ox, wx.cam_y = oy;
    int rain = wx.idx >= 1 && wx.idx <= 3, n = wx.idx == 4 || wx.idx == 5 ? wx.snow_on : wx.idx == 10 ? wx.n : rain ? wx.n : 0;
    for (int i = 0; i < n; i++) {
        Particle *p = &wx.p[i];
        SDL_Texture *t = wx.tex[rain ? p->kind : 0];
        if (rain && p->life <= 0) continue;
        if (p->x >= vw || p->y >= vh || p->x + p->w <= 0 || p->y + p->h <= 0) {
            if (!rain) p->life = 0;
            continue;
        }
        if (!t || p->w <= 0 || p->h <= 0) continue;
        SDL_Rect src = {p->sx, p->sy, p->w, p->h}, dst = {p->x, p->y, p->w, p->h};
        SDL_RenderCopy(ren, t, &src, &dst);
    }
    if (rain && wx.flash > 0) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_RenderFillRect(ren, NULL);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
}

// 地上濺水（0x445180）：畫面上隨機一點換成世界座標，紅雨用 0x100010 隨機序列 0~2，否則 0x100023
// shortcut: 原版物件類別 0x49a910 沒查，這裡當播一次就消失的地面動畫
static void splash_new(int sx, int sy, int red) {
    if (unit_n == MAX_UNIT || !unit_scene) return;
    float a = (float)(sx + wx.cam_x) / (TILE_W / 2) - (unit_scene->h - 1), b = (float)(sy + wx.cam_y) / (TILE_H / 2);
    MUnit *u = &units[unit_n++];
    *u = (MUnit){.cls = U_SPLASH, .x = (int)((a + b) / 2 * 64), .y = (int)((b - a) / 2 * 64)};
    spr_load(&u->spr, red ? 0x100010 : 0x100023);
    spr_set(&u->spr, red ? rand() % 3 : 0, 0);
}
