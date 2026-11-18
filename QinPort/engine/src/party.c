// 隊伍面板 JM51_状态界面_主控（Poq.exe CMainControlDialog 0x539dd0）：5 個角色格，每格 m_pRole（頭像鈕）、m_pRoleBox（框）、m_pRoleHP／m_pRoleMP（直條）、
// m_pUpgrade（有剩餘點數才顯示）、m_pRoleStatus（狀態圖示兩個）。由 main.c 在 hud_draw 之前 #include。
// 0x53a670：隊伍名單裡有的角色才顯示那格；0x53a7f0：全部藏起來重來；0x53b040：血條範圍 0..100、值 = 百分比（這裡直接用 hp／hp_max 比例）；
// 0x53b370：升級提示 = 有點數才顯示；0x53b0b0：狀態圖示 — 狀態依表 0x63ea40 分 8 組（組號 = 圖示幀），每個隊員記最多 8 個組號（加狀態放進去、
// 解除拿掉第一個相同的），由大到小排序，只顯示最前面兩個（m_pRoleStatus[0][0]／[0][1] 的圖幀 = 組號）。
// 0x551480／0x551560／0x551590：點／指著成員頭像 → 敵人狀態框（0x459ce0）顯示那個人，放開（離開）就關。
// shortcut: 隊伍名單原版是玩家的角色清單（全域 0x940184 鏈），這裡改讀劇情的隊伍表（主角帶頭，最多 5 個）；
//   頭像（m_pRole 的圖）原版依角色圖號，這裡沒有隊友頭像資源對應，只畫鈕底；手機沒有「指著」，改成點頭像框顯示 3 秒；點頭像不切換操控（單機只控主角，0x11 選隊友）。
#define PARTY_SLOTS 5
static Actor *party_show;       // 點到的成員（敵人狀態框顯示他）
static Uint32 party_show_until;

// 狀態 → 圖示組（表 0x63ea40 每列的第一格起、以 −1 結尾；列號 = 組）。沒有的狀態回 −1
static int party_group(int status) {
    static const int rows[8][10] = {{4}, {17}, {10}, {2}, {33}, {3, 61, 70}, {11, 14, 23, 46, 47, 48, 62, 63, 64, 65}, {1}};
    for (int g = 0; g < 8; g++)
        for (int k = 0; k < 10 && rows[g][k]; k++)
            if (rows[g][k] == status) return g;
    return -1;
}

// 0x53b0b0 的結果：把目前所有狀態的組號由大到小排，取前兩個（沒有就 −1）。回傳有幾個組
static int party_icons(const Actor *a, int out[2]) {
    int g[8], n = 0;
    for (int b = 0; b < a->buff_n && n < 8; b++) {  // 原版每人最多記 8 個
        int k = party_group(a->buff[b].status);
        if (k >= 0) g[n++] = k;
    }
    for (int i = 0; i < n; i++)  // 由大到小
        for (int j = i + 1; j < n; j++)
            if (g[j] > g[i]) { int t = g[i]; g[i] = g[j]; g[j] = t; }
    out[0] = n > 0 ? g[0] : -1;
    out[1] = n > 1 ? g[1] : -1;
    return n;
}

// 0x53a670：隊伍名單（主角第一）。隊員來自劇情的隊伍表（AddMember 0x10037850：帶頭者 = 主角；story_rt.c team_add 同時送訊息 0x49b70 讓面板加一格）
static int party_list(Actor **out) {
    int n = 0;
    out[n++] = &actors[0];
#ifdef STORY
    for (int m = story_team_next(story_player(), 0); m && n < PARTY_SLOTS; m = story_team_next(story_player(), m)) {
        Actor *a = character_of(m);
        if (a && a != &actors[0] && alive(a)) out[n++] = a;
    }
#endif
    return n;
}

static void party_pos(int *px, int *py) { *px = vw - party.w; *py = vh - bar.h - party.h; }  // 右緣、貼在主列上方

static Actor *party_shown(void) {  // 敵人狀態框要顯示的隊員（沒有 = NULL）
    if (party_show && SDL_GetTicks() < party_show_until && alive(party_show)) return party_show;
    party_show = NULL;
    return NULL;
}

static void party_draw(UiDialog *d, int px, int py) {
    Actor *m[PARTY_SLOTS];
    int n = party_list(m);
    UiCtrl *bg = ui_find(d, "m_pBackGround");
    if (bg) draw(ui_image(bg->img, 0), px + bg->x, py + bg->y);
    for (int i = 0; i < n; i++) {
        char key[32];
        const char *kinds[] = {"m_pRole[%d]", "m_pRoleBox[%d]"};
        for (int k = 0; k < 2; k++) {
            snprintf(key, sizeof key, kinds[k], i);
            const UiCtrl *c = ui_find(d, key);
            if (c && c->img) draw(ui_image(c->img, 0), px + c->x, py + c->y);
        }
        snprintf(key, sizeof key, "m_pRoleHP[%d]", i);
        UiCtrl *hp = ui_find(d, key);
        snprintf(key, sizeof key, "m_pRoleMP[%d]", i);
        UiCtrl *mp = ui_find(d, key);
        if (hp) ui_bar(hp, px, py, m[i]->hp, m[i]->hp_max);
        if (mp) ui_bar(mp, px, py, m[i]->mp, m[i]->mp_max);
        if (i == 0 && (hero_extra_exp > 0 || hero_extra_skill > 0)) {  // 0x53b370：有點數才顯示
            snprintf(key, sizeof key, "m_pUpgrade[%d]", i);
            const UiCtrl *c = ui_find(d, key);
            if (c && c->img) draw(ui_image(c->img, 0), px + c->x, py + c->y);
        }
        int ic[2];
        party_icons(m[i], ic);
        // ini 裡狀態圖示的名字五格都叫 m_pRoleStatus[0][x]：前五筆（圖 8719）是第一個圖示、後五筆（圖 8713）是第二個，依序位置 = 格 i
        int seen[2] = {0, 0};
        for (int j = 0; j < d->count; j++) {
            const UiCtrl *c = &d->c[j];
            if (strncmp(c->name, "m_pRoleStatus", 13)) continue;
            int which = c->img == 8713;
            if (seen[which]++ == i && ic[which] >= 0) draw(ui_image(c->img, ic[which]), px + c->x, py + c->y);
        }
    }
}

// 點頭像（0x551480／0x551560）：回 1 = 吃掉
static int party_tap(int x, int y) {
    int px, py;
    party_pos(&px, &py);
    Actor *m[PARTY_SLOTS];
    int n = party_list(m);
    for (int i = 0; i < n; i++) {
        char key[32];
        snprintf(key, sizeof key, "m_pRoleBox[%d]", i);
        const UiCtrl *c = ui_find(&party, key);
        const Sprite *s = c ? ui_image(c->img, 0) : NULL;
        if (s && x >= px + c->x && x < px + c->x + s->w && y >= py + c->y && y < py + c->y + s->h) {
            party_show = m[i];
            party_show_until = SDL_GetTicks() + 3000;
            return 1;
        }
    }
    return 0;
}
