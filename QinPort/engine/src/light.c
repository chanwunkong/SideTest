// 光影（原版 Poq.exe）。main.c 直接 #include（要用 ren、vw、vh、actors）。
// - 亮度：CGodClient +0x134（0x469650 設定）、半小時一格的亮度表 +0x74（0x4682e0，房間的 G_Day／G_Night）、
//   時鐘 0x662884（0x44e750，每 1200 拍半小時，0~6 點、14~20 點快 10 倍）、整點換亮度（0x4695f0）
// - 光源：CLightMng（0x441e60～0x442460）的 SLightUnit，跟著角色（0x486480）、建築（0x481d60 後半）、招式單位 400（0x43b8c0）
// - 光照圖：SLightMap（0x4a57c0）每格 4 像素，先填亮度（0x4a63e0），光源的橢圓光圈（0x4a5660 → 0x4b9db0）取大的蓋上（0x4a3c90）
// - 色表：0x4a5880 依色調做 19 張 RGB565 對照表，0x4a5010 把畫面每個像素換成光照圖那格的表。
//   這裡改成每格一個 RGB 倍率，用 SDL 乘法混色蓋在畫面上（> 1 的部分再用 dst × (1 + src) 加一次）
// 值：0~15 = 暗到原色、16 = 藍色加亮（閃電）、17 = 全部 ×1.5、18 = 藍 ÷0.4（17、18 單機版沒用到）

#define LT_CELL 4
#define LT_MAX 64
static const float LT_A[16] = {0.45f, 0.5f, 0.54f, 0.6f, 0.66f, 0.71f, 0.76f, 0.8f, 0.84f, 0.88f, 0.91f, 0.94f, 0.96f, 0.98f, 0.99f, 1.0f};  // 0x5f0cb0…
static const float LT_B[16] = {0.7f, 0.72f, 0.74f, 0.76f, 0.78f, 0.8f, 0.82f, 0.84f, 0.86f, 0.88f, 0.9f, 0.92f, 0.94f, 0.96f, 0.98f, 1.0f};    // 0x5f0d10…

static struct {
    int day, night, type;      // CGodServer +0x14 G_Day、+0x10 G_Night、+0x1c G_Type
    int table[48];             // +0x74：每半小時的亮度
    int bright, target;        // +0x134 目前亮度、+0x138 要回到的亮度
    int mode;                  // 色調（0x4a5880 的參數；1 = 不重做）
    float f[19][3];            // 目前色調下每個值的 RGB 倍率
    Uint8 *map;                // 光照圖
    int mw, mh;
    SDL_Texture *tex[2];       // 乘法、加亮
} lt = {.day = 15, .mode = -1};
static struct { int year, month, day, hour, tick, half; } clk = { .hour = 23 };  // 開局 23 點（0x44e6e0）。0x662884：年、月、日、時、拍數、半小時（0/1）
static int light_lock;        // 小雨時亮度不變（CRAIN +0x18，原版 +0x158 = 天氣不是小雨）

typedef struct {
    const Actor *a;            // 跟著誰（NULL = 建築，位置不動）
    const RoleDef *def;        // 認人用（角色格子被重用時當成原本的人不在了）
    int x, y;                  // 世界像素（腳底）
    int vis;                   // +0x34。shortcut: 原版 = 1 時地圖格 +0x13 ≥ 2 就不畫（沒查那欄），這裡都畫
    int r, l_main, l_alt, cur; // 光圈半徑（像素）、+0x38／+0x3c 兩種亮度、+0x40 目前用哪個
    int n, dying;              // +0x44 計數、+0x48 正在收
} Light;
static Light lights[LT_MAX];
static int light_n;

static void weather_retone(int b);  // weather.c：粒子跟著亮度換色（CParticleMng 虛擬函式 +8）

// 色調（0x4a5880）：0 一般（夜裡偏藍）、2 紅、3 綠、4 藍、5 黃、6 變亮、7 淡紅
static void light_mode(int m) {
    if (m == lt.mode || m == 1) return;
    lt.mode = m;
    for (int k = 0; k < 16; k++) {
        float a = LT_A[k], b = LT_B[k], *f = lt.f[k];
        switch (m) {
        case 2: f[0] = 1, f[1] = a, f[2] = a; break;
        case 3: f[0] = a, f[1] = 1, f[2] = a; break;
        case 4: f[0] = a, f[1] = a, f[2] = 1; break;
        case 5: f[0] = 1, f[1] = 1, f[2] = a; break;
        case 6: f[0] = f[1] = f[2] = 1 / a; break;
        case 7: f[0] = 1, f[1] = b, f[2] = b; break;
        default: f[0] = a, f[1] = a, f[2] = b;
        }
    }
    lt.f[16][0] = lt.f[16][1] = 1, lt.f[16][2] = 1.4f;
    lt.f[17][0] = lt.f[17][1] = lt.f[17][2] = 1.5f;
    lt.f[18][0] = lt.f[18][1] = 0.4f, lt.f[18][2] = 2.5f;
}

// 設亮度（0x469650）
static void light_set(int b) {
    if (lt.bright == b || light_lock) return;
    lt.bright = b;
    weather_retone(b);
}

// 整點換亮度（0x4695f0）：下雨中只會變暗（< G_Day / 2 才套）
static void light_hour(int weather) {
    lt.target = lt.table[clk.half + clk.hour * 2];
    if (!weather || lt.target < lt.day / 2) light_set(lt.target);
}

// 房間的亮度（0x516 → 0x44e5d0 → 0x4682e0）：0~6 點 14 格由夜到日、7~12 點白天、13~19 點 14 格由日到夜、20~23 點夜晚
static void light_table(int day, int night, int type, int weather) {
    lt.day = day, lt.night = night, lt.type = type;
    for (int i = 0; i < 14; i++) {
        int up = night + (day - night) * i / 14, down = day - (day - night) * i / 14;
        lt.table[i] = up < day ? up : day;
        lt.table[26 + i] = down > night ? down : night;
    }
    for (int i = 0; i < 12; i++) lt.table[14 + i] = day;
    for (int i = 0; i < 8; i++) lt.table[40 + i] = night;
    light_hour(weather);
}

// 時鐘（0x44e750、0x46baf0）：主迴圈每拍一次；室內（G_Type 12）不走。
// shortcut: 未建立 Rec 回放狀態，啟用回放時補上狀態2的時計阻擋。
// shortcut: 半週期／半小時的日期介面通知未接，日期介面系統移植時補上；天光由 weather_tick 更新。
static void clock_tick(void) {
    if (lt.type == 12) return;
    clk.tick += (clk.hour >= 0 && clk.hour <= 6) || (clk.hour >= 14 && clk.hour <= 20) ? 1200 / 10 : 1;
    if (clk.tick < 1200) return;
    clk.tick = 0;
    if (++clk.half < 2) return;
    clk.half = 0;
    if (++clk.hour < 24) return;
    clk.hour = 0;
    if (++clk.day < 32) return;
    clk.day = 1;
    if (++clk.month < 12) return;  // 原版 > 0xb 即跨年：月份只有1～11
    clk.month = 1, clk.year++;
}

// ---- 光源 ----
static Light *light_of(const Actor *a) {  // 0x442130
    for (int i = 0; i < light_n; i++) if (lights[i].a == a) return &lights[i];
    return NULL;
}
// 加光源（0x442000）：亮度 = 顏色最亮的那個分量 / 17；alt = −1 跟主色一樣
static void light_add(const Actor *a, int x, int y, int vis, int r, Uint32 color, int alt) {
    if (light_n == LT_MAX) return;
    Uint32 c = alt < 0 ? color : (Uint32)alt;
    int m1 = (int)((color >> 16 & 255) > (color >> 8 & 255) ? color >> 16 & 255 : color >> 8 & 255), m2 = (int)((c >> 16 & 255) > (c >> 8 & 255) ? c >> 16 & 255 : c >> 8 & 255);
    if ((int)(color & 255) > m1) m1 = color & 255;
    if ((int)(c & 255) > m2) m2 = c & 255;
    lights[light_n++] = (Light){a, a ? a->def : NULL, x, y, vis, r, m1 / 17, m2 / 17, m1 / 17, 0, 0};
}
// 建築的光源（0x481d60）：1 小灯、2 小白灯、3 大灯、4 大白灯；位置是光源格的中心
static void light_buildings(const Scene *s) {
    static const int R[5] = {0, 0x140, 0x140, 800, 800};
    static const Uint32 ALT[5] = {0, 0xdcdcdc, 0xffffff, 0xe6e6e6, 0xffffff};
    for (int i = 0; i < s->object_count; i++) {
        const Model *m = s->objects[i].m;
        if (!m || !m->light) continue;
        int cx = s->objects[i].x + m->lx, cy = s->objects[i].y + m->ly;
        light_add(NULL, world_x(s, cx, cy) + TILE_W / 2, world_y(s, cx, cy) + TILE_H / 2, 0, R[m->light], 0xffffff, (int)ALT[m->light]);
    }
}
static void light_clear(void) { light_n = 0; }

static int actor_wx(const Actor *a, const Scene *s) { return scene_px(s, (int)(a->x * 64), (int)(a->y * 64)); }
static int actor_wy(const Actor *a, const Scene *s) { return scene_py(s, (int)(a->x * 64), (int)(a->y * 64)); }

// 每拍（0x442180）：角色的光跟著走，人死了或不在了就每拍縮 10 像素，縮到 60 以下拿掉；建築的每 6 拍換一次亮度（閃）
// 角色的光（0x486480，原版角色初始化時加）：有光影的角色半徑 = 角色大小 × 240 + 400，光影闪烁的另一色 0xe1e1e1
static void light_tick(const Scene *s) {
    for (int i = 0; i < actor_count; i++) {
        const Actor *a = &actors[i];
        if (a->role && a->def && a->def->light && a->act != ACT_DEAD && !light_of(a))
            light_add(a, actor_wx(a, s), actor_wy(a, s), 1, a->def->size * 0xf0 + 400, 0xffffff, a->def->flicker ? 0xe1e1e1 : 0xffffff);
    }
    for (int i = 0; i < light_n; i++) {
        Light *L = &lights[i];
        if (L->dying) {
            if (L->r - L->n > 0x3c) L->n += 10;
            else { lights[i--] = lights[--light_n]; continue; }
        } else if (!L->a) {
            if (++L->n % 6 == 1) L->cur = L->cur == L->l_main ? L->l_alt : L->l_main;
        } else if (!L->a->role || L->a->def != L->def) L->dying = 1, L->n = 0;
        else {
            L->x = actor_wx(L->a, s), L->y = actor_wy(L->a, s);
            if (L->a->act == ACT_DEAD) L->dying = 1, L->n = 0;
        }
    }
}

// 光圈（0x4b9db0）蓋到光照圖（0x4a3c90）：寬 n 格、高 n/2 格的橢圓，中間一半是 lv，外圈線性降到 0；取大的
static void light_stamp(int r, int lv, int sx, int sy) {
    int n = r / LT_CELL, half = n / 2, x0 = sx / LT_CELL - n / 2, y0 = sy / LT_CELL - half / 2;
    for (int row = 0; row < half; row++) {
        int y = y0 + row, dy = row - half / 2;
        if (y < 0 || y >= lt.mh) continue;
        for (int col = 0; col < n; col++) {
            int x = x0 + col, dx = col - half;
            if (x < 0 || x >= lt.mw) continue;
            double d = sqrt((double)(dx * dx + dy * dy * 4));
            int v;
            if (half < (int)d) v = 0;
            else if (half * 0.5f < (float)d) {
                int t = (int)((float)lv * ((float)half - (float)d) / ((float)half - half * 0.5f)) & 255;
                v = t < 0x65 ? t : 0;
                if (v > lv) v = lv;
            } else v = lv;
            if (v && lt.map[y * lt.mw + x] < v) lt.map[y * lt.mw + x] = (Uint8)v;
        }
    }
}

// 畫（0x442460 → 0x4a5010）：ox, oy = 鏡頭左上的世界像素
static void light_draw(int ox, int oy) {
    if (lt.mode < 0) light_mode(0);
    int mw = (vw + LT_CELL - 1) / LT_CELL, mh = (vh + LT_CELL - 1) / LT_CELL;
    if (mw != lt.mw || mh != lt.mh || !lt.map) {
        free(lt.map);
        lt.map = malloc((size_t)mw * mh);
        lt.mw = mw, lt.mh = mh;
        for (int k = 0; k < 2; k++) {
            if (lt.tex[k]) SDL_DestroyTexture(lt.tex[k]);
            lt.tex[k] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mw, mh);
            if (lt.tex[k]) SDL_SetTextureScaleMode(lt.tex[k], SDL_ScaleModeNearest);
        }
        if (lt.tex[0]) SDL_SetTextureBlendMode(lt.tex[0], SDL_BLENDMODE_MOD);
        if (lt.tex[1] && SDL_SetTextureBlendMode(lt.tex[1], SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                                                                   SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD)) < 0) {
            SDL_DestroyTexture(lt.tex[1]);
            lt.tex[1] = NULL;  // 不支援自訂混色：> 1 的加亮就不畫
        }
    }
    if (!lt.map || !lt.tex[0]) return;
    int b = lt.bright < 0 ? 0 : lt.bright > 18 ? 18 : lt.bright;
    memset(lt.map, b, (size_t)lt.mw * lt.mh);
    for (int i = 0; i < light_n; i++) {
        const Light *L = &lights[i];
        if (L->dying) light_stamp(L->r - L->n, L->l_main, L->x - ox, L->y - oy);
        else light_stamp(L->r, L->cur, L->x - ox, L->y - oy);
    }
    int dark = 0, bright = 0;
    for (int k = 0; k < 19; k++) {
        if (!memchr(lt.map, k, (size_t)lt.mw * lt.mh)) continue;
        for (int c = 0; c < 3; c++) dark |= lt.f[k][c] < 1, bright |= lt.f[k][c] > 1;
    }
    SDL_Rect dst = {0, 0, lt.mw * LT_CELL, lt.mh * LT_CELL};
    for (int pass = 0; pass < 2; pass++) {
        if (!(pass ? bright : dark) || !lt.tex[pass]) continue;
        Uint32 *px;
        int pitch;
        if (SDL_LockTexture(lt.tex[pass], NULL, (void **)&px, &pitch) < 0) continue;
        for (int y = 0; y < lt.mh; y++)
            for (int x = 0; x < lt.mw; x++) {
                const float *f = lt.f[lt.map[y * lt.mw + x]];
                Uint32 c = 0xff000000u;
                for (int k = 0; k < 3; k++) {
                    float v = pass ? f[k] - 1 : f[k];
                    c |= (Uint32)(v <= 0 ? 0 : v >= 1 ? 255 : v * 255 + 0.5f) << (16 - 8 * k);
                }
                px[y * (pitch / 4) + x] = c;
            }
        SDL_UnlockTexture(lt.tex[pass]);
        SDL_RenderCopy(ren, lt.tex[pass], NULL, &dst);
    }
}
