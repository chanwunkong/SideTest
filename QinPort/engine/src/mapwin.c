// 地圖視窗 JM60（Poq.exe CMapDialog）。由 main.c 在 hud_say 之後 #include。
// 原版：主列 m_pMap 開窗；擺位 0x53cee0 = 畫面置中再往上 0x32；OnInit 0x53cb30 把 38 個控制項（全國地圖、場景地圖、36 個地點）的剪裁框都設成地圖框 DAT_0063f454 = (29,22)–(541,278)，
// 再顯示／隱藏幾個子控制項；Esc（0x1b）關窗（0x53cc10 → 0x53c960），其他鍵交給視窗基底；m_pCClose 也關。
// m_pAll（全国地图）切換全國地圖／場景地圖：全國地圖畫底圖 1489 與 36 個地點按鈕 m_pSite[i]（提示 = ToolTip_C 第 594 + i 筆，點了名字顯示在 m_pSiteName）；
// 場景地圖 = 512×256 的小地圖（原版生成函數：每格畫成 8×4 的菱形，沒探索的格子不畫；格子太大就縮小並置中），上面疊人物（m_pCharacter）與地點標記（m_pLocus）。
// 探索程度來自格子的 view（actor_explore 0x4608xx 提升）。
// shortcut: 小地圖用單色格（可走暗綠、不可走灰）取代原版的地磚／建築縮圖（原版小地圖生成函數與建築縮圖函數都還沒翻，所以不標位址）；
//   地點按鈕全部顯示（原版 MapData 由伺服器給哪些去過）；自定義標記（m_pLable／m_pShowLable／m_pHideLable／m_pDrag）、目的地（m_pDestiation）、
// 點場景地圖 = 鏡頭移到那一格（原版 0x53cc60 的點圖定位分支；它的另一支是伺服器 MapData 的標記提示，不做）。
//   隊友位置（伺服器 MapData 的 PosX／PosY）、地點標記的滑鼠提示（伺服器 MapData 的 ShowLable／MapIndex）沒做；
//   原版重畫用髒矩形、探索標記格（0x200×0x100 的 MapData 格）和門檻分段（沒有呼叫者）都沒有對應，見 funcs_skip.txt。
#define JM60 "JM60_\xb5\xd8\xcd\xbc\xbd\xe7\xc3\xe6"  // JM60_地图界面（GBK）
#define MAP_W 512
#define MAP_H 256
static UiDialog map_d;
static int map_nx, map_ny;  // 全國地圖在地圖框裡的偏移（0 或負）
static int map_open, map_all, map_chars = 1, map_marks = 1, map_site = -1, map_age;
static SDL_Texture *map_tex;
static uint32_t map_px[MAP_W * MAP_H];

static void map_close(void) { map_open = 0; }  // 0x53c960

static void map_show(void) {  // 開窗 + OnInit 0x53cb30
    if (map_open) { map_close(); return; }
    if (!ui_load(JM60, &map_d)) return;
    UiCtrl *dst = ui_find(&map_d, "m_pDestiation");
    if (dst) dst->flags &= ~9;  // 目的地要伺服器的路徑指令，單機隱藏
    map_open = 1;
    map_all = 0;
    map_site = -1;
    map_nx = map_ny = 0;
    map_age = 0;
}

static void map_fill(int cx, int cy, int w, int h, uint32_t c) {
    for (int y = cy; y < cy + h; y++)
        for (int x = cx; x < cx + w; x++)
            if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H) map_px[y * MAP_W + x] = c;
}

// 場景格 → 小地圖像素（格中心）；回傳縮放（1 = 8×4 菱形）
static float map_cell_px(const Scene *s, float x, float y, int *px, int *py) {
    int n = s->w + s->h;
    float sc = n * 4 > MAP_W ? (float)MAP_W / (n * 4) : 1.0f;
    float left = (MAP_W - n * 4 * sc) / 2, top = (MAP_H - n * 2 * sc) / 2;
    *px = (int)(left + (x - y + s->h) * 4 * sc);
    *py = (int)(top + (x + y + 1) * 2 * sc);
    return sc;
}

// 小地圖像素 → 場景格（map_cell_px 的反算）；落在場景外回 0
static int map_pixel_cell(const Scene *s, int mx, int my, int *cx, int *cy) {
    int n = s->w + s->h;
    float sc = n * 4 > MAP_W ? (float)MAP_W / (n * 4) : 1.0f;
    float left = (MAP_W - n * 4 * sc) / 2, top = (MAP_H - n * 2 * sc) / 2;
    float u = (mx - left) / (4 * sc) - s->h, v = (my - top) / (2 * sc) - 1;  // u = x − y、v = x + y
    *cx = (int)floorf((u + v) / 2);
    *cy = (int)floorf((v - u) / 2);
    return *cx >= 0 && *cy >= 0 && *cx < s->w && *cy < s->h;
}
static void camera_to_cell(int x, int y);  // viewport.c 之後（鏡頭置中到格）
static void map_base_update(const Scene *s);  // viewport.c 之後（0x46f180 地磚／建築縮圖底圖）
static SDL_Texture *map_base;

static void map_build(void) {  // 小地圖疊圖：標記、人物（地磚與建築在 map_base，0x46f180）
    memset(map_px, 0, sizeof map_px);
    const Scene *s = unit_scene;
    if (!s || !s->cells) return;
    if (map_marks)
        for (int i = 0; i < s->entry_count; i++) {
            int px, py;
            map_cell_px(s, s->entries[i].x, s->entries[i].y, &px, &py);
            map_fill(px - 2, py - 2, 5, 5, 0xFF30D8F0);
        }
    if (map_chars)
        for (int i = actor_count - 1; i >= 0; i--) {
            const Actor *a = &actors[i];
            if (!alive(a) || a->bound_scene != s) continue;
            const Cell *c = &s->cells[(int)a->y * s->w + (int)a->x];
            if (i != 0 && !c->view) continue;
            int px, py;
            map_cell_px(s, a->x, a->y, &px, &py);
            map_fill(px - 1, py - 1, 3, 3, i == 0 ? 0xFFFFFFFF : hostile(&actors[0], a) ? 0xFF3030E0 : 0xFFE0C040);
        }
}

static void map_draw(void) {
    if (!map_open) return;
    int ox = (vw - map_d.w) / 2, oy = (vh - map_d.h) / 2 - 0x32;  // 0x53cee0
    for (int i = 0; i < map_d.count; i++) {
        const UiCtrl *c = &map_d.c[i];
        if (c->type == 6 || c->type == 10 || !c->img || !(c->flags & 9)) continue;
        if (!strncmp(c->name, "m_pSite[", 8) || !strcmp(c->name, "m_pNationalMap") || !strcmp(c->name, "m_pSceneMap")) continue;
        draw(ui_image(c->img, 0), ox + c->x, oy + c->y);
    }
    if (map_all) {
        const UiCtrl *nat = ui_find(&map_d, "m_pNationalMap");
        SDL_Rect clip = {ox + 29, oy + 22, MAP_W, MAP_H};  // 全國地圖比地圖框大，控制項都剪在框內（0x53cb30）
        SDL_RenderSetClipRect(ren, &clip);
        if (nat) draw(ui_image(nat->img, 0), clip.x + map_nx, clip.y + map_ny);
        for (int i = 0; i < map_d.count; i++) {
            const UiCtrl *c = &map_d.c[i];
            if (!strncmp(c->name, "m_pSite[", 8) && c->img) draw(ui_image(c->img, 0), clip.x + map_nx + c->x, clip.y + map_ny + c->y);  // 地點座標是全國地圖圖上的
        }
        SDL_RenderSetClipRect(ren, NULL);
        const UiCtrl *nm = ui_find(&map_d, "m_pSiteName");
        char g[128], u[256];
        if (nm && map_site >= 0 && res_dlg("ToolTip_C.dlg", 594 + map_site, g, sizeof g)) {
            gbk_utf8(g, u, sizeof u);
            draw_text(ox + nm->x, oy + nm->y, u, nm->w, 0xFFFF00, 1);
        }
    } else {
        if (!map_tex) map_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MAP_W, MAP_H);
        if (!map_age--) { map_age = 30; map_base_update(unit_scene); map_build(); SDL_UpdateTexture(map_tex, NULL, map_px, MAP_W * 4); SDL_SetTextureBlendMode(map_tex, SDL_BLENDMODE_BLEND); }
        SDL_Rect r = {ox + 29, oy + 22, MAP_W, MAP_H};
        if (map_base) SDL_RenderCopy(ren, map_base, NULL, &r);
        SDL_RenderCopy(ren, map_tex, NULL, &r);
    }
}

static int map_tap(int x, int y) {
    if (!map_open) return 0;
    int ox = (vw - map_d.w) / 2, oy = (vh - map_d.h) / 2 - 0x32;
    const UiCtrl *c = ui_button_at(&map_d, ox, oy, x, y);
    if (!c && !map_all && unit_scene && x >= ox + 29 && x < ox + 29 + MAP_W && y >= oy + 22 && y < oy + 22 + MAP_H) {  // 場景地圖：點一下把鏡頭移到那格
        int cx, cy;
        if (map_pixel_cell(unit_scene, x - ox - 29, y - oy - 22, &cx, &cy)) camera_to_cell(cx, cy);
        return 1;
    }
    if (!c) {  // 全國地圖：點到地點 = 選它（名字顯示）；點別處 = 把那一點移到地圖框中央（shortcut: 原版用 m_pDrag 拖動，這裡改點一下置中）
        if (!map_all || x < ox + 29 || x >= ox + 29 + MAP_W || y < oy + 22 || y >= oy + 22 + MAP_H) return 1;
        int hit = 0;
        for (int i = 0; i < map_d.count; i++) {
            const UiCtrl *s = &map_d.c[i];
            const Sprite *sp = !strncmp(s->name, "m_pSite[", 8) ? ui_image(s->img, 0) : NULL;
            int sx = ox + 29 + map_nx + s->x, sy = oy + 22 + map_ny + s->y;
            if (sp && x >= sx && x < sx + sp->w && y >= sy && y < sy + sp->h) map_site = atoi(s->name + 8), hit = 1;
        }
        const UiCtrl *nat = ui_find(&map_d, "m_pNationalMap");
        const Sprite *im = nat ? ui_image(nat->img, 0) : NULL;
        if (!hit && im) {
            map_nx += MAP_W / 2 - (x - ox - 29);
            map_ny += MAP_H / 2 - (y - oy - 22);
            if (map_nx > 0 || im->w <= MAP_W) map_nx = 0; else if (map_nx < MAP_W - im->w) map_nx = MAP_W - im->w;
            if (map_ny > 0 || im->h <= MAP_H) map_ny = 0; else if (map_ny < MAP_H - im->h) map_ny = MAP_H - im->h;
        }
        return 1;
    }
    if (!strcmp(c->name, "m_pCClose")) map_close();
    else if (!strcmp(c->name, "m_pAll")) map_all = !map_all, map_age = 0;
    else if (!strcmp(c->name, "m_pCharacter")) map_chars = !map_chars, map_age = 0;
    else if (!strcmp(c->name, "m_pLocus")) map_marks = !map_marks, map_age = 0;
    return 1;
}
