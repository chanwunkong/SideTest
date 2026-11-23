// 任務日誌視窗 JM61（Poq.exe CLogDialog：開窗 0x4fe510、初始化 0x5679d0、章節日誌 0x567cf0、任務清單 0x567e60）。由 main.c 在 help_content_tap 之後 #include。
// 原版：主列 m_pLog（JM51 中間 (669,73)）或 L 鍵開；置中再往上 0x32（同地圖）。兩頁：m_pLog 頁 = 章節日誌（m_pLogList，標題「第N章」= UI_C 0x859 + 章，
// m_pPrevChapter／m_pNextChapter 換章）；m_pMission 頁 = 接受任務 m_pUnaccomplishList（UI_C 2209）＋完成任務 m_pAccomplishedList（UI_C 2208）。資料 journal.c。
// shortcut: 清單用上下箭頭捲（原版是清單控制項的捲軸／滑鼠滾輪）；章節按鈕只在日誌頁；m_pChapter（8806）的用途沒查，照 ini 畫在頁籤中間。
#define JM61 "JM61_\xc8\xd5\xd6\xbe\xbd\xe7\xc3\xe6"  // JM61_日志界面（GBK）
static int jn_page, jn_view = 1, jn_top[3];  // 頁（0 日誌、1 任務）、看的章、三個清單捲到第幾行

static void journal_show(void) {
    if (journal_open) { journal_open = 0; return; }
    if (!ui_load(JM61, &jn_d)) return;
    jn_pop = 0;
    jn_view = jn_chap;
    memset(jn_top, 0, sizeof jn_top);
    journal_open = 1;
}

static void jn_pos(int *ox, int *oy) { *ox = (vw - jn_d.w) / 2; *oy = (vh - jn_d.h) / 2 - 0x32; }
static int jn_is_chapter_ctrl(const UiCtrl *c) { return !strcmp(c->name, "m_pPrevChapter") || !strcmp(c->name, "m_pNextChapter"); }

static const UiCtrl *jn_button(int x, int y) {
    int ox, oy;
    jn_pos(&ox, &oy);
    const UiCtrl *c = ui_button_at(&jn_d, ox, oy, x, y);
    return c && jn_page && jn_is_chapter_ctrl(c) ? NULL : c;
}

// 目前頁的清單：名稱、索引、文字
static int jn_lists(const char *names[2], int idx[2], char *txt[2], char *buf0, char *buf1, size_t cap) {
    if (!jn_page) {
        names[0] = "m_pLogList"; idx[0] = 0; txt[0] = jn_log[jn_view];
        return 1;
    }
    jn_list(0, buf0, cap); jn_list(1, buf1, cap);
    names[0] = "m_pUnaccomplishList"; idx[0] = 1; txt[0] = buf0;
    names[1] = "m_pAccomplishedList"; idx[1] = 2; txt[1] = buf1;
    return 2;
}

static void journal_draw(void) {
    if (!journal_open) return;
    int ox, oy;
    jn_pos(&ox, &oy);
    static char b0[JN_LOGSZ], b1[JN_LOGSZ];
    const UiCtrl *bg = ui_find(&jn_d, "m_pBackGround"), *bk = ui_find(&jn_d, jn_page ? "m_pMissionBK" : "m_pLogBK");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);
    if (bk) draw(ui_image(bk->img, 0), ox + bk->x, oy + bk->y);
    char t[96];
    if (!jn_page) {
        const UiCtrl *ti = ui_find(&jn_d, "m_LogTitle");
        ui_text(2137 + jn_view, t, sizeof t);
        if (ti) draw_text(ox + ti->x, oy + ti->y, t, ti->w, 0x45B1B1, 1);
    } else {
        const UiCtrl *a = ui_find(&jn_d, "m_pAccomplishTitle"), *u = ui_find(&jn_d, "m_pUnaccomplishTitle");
        ui_text(2208, t, sizeof t);
        if (a) draw_text(ox + a->x, oy + a->y, t, a->w, 0x45B1B1, 1);
        ui_text(2209, t, sizeof t);
        if (u) draw_text(ox + u->x, oy + u->y, t, u->w, 0x45B1B1, 1);
    }
    const char *nm[2]; int ix[2]; char *tx[2];
    int n = jn_lists(nm, ix, tx, b0, b1, sizeof b0);
    for (int k = 0; k < n; k++) {
        const UiCtrl *c = ui_find(&jn_d, nm[k]);
        if (!c) continue;
        Scroll sc;
        int bar = text_scroll(c, ox, oy, &sc), w = c->w - 4 - (bar ? sc.track.w : 0);
        int rows = (draw_text(0, 0, tx[k], w, 0, 0) - 1) / 16, fit = (c->h - 4) / 16;
        if (jn_top[ix[k]] > rows - fit) jn_top[ix[k]] = rows - fit;
        if (jn_top[ix[k]] < 0) jn_top[ix[k]] = 0;
        SDL_Rect clip = {ox + c->x, oy + c->y, c->w, c->h};
        SDL_RenderSetClipRect(ren, &clip);
        if (tx[k][0]) draw_text(clip.x + 2, clip.y + 2 - jn_top[ix[k]] * 16, tx[k], w, 0xBECDCE, 1);
        if (bar && rows > fit) {
            draw(ui_image(c->sb[0], 0), sc.track.x, sc.track.y);
            draw(ui_image(c->sb[1], 0), sc.up.x, sc.up.y);
            draw(ui_image(c->sb[2], 0), sc.down.x, sc.down.y);
        }
        SDL_RenderSetClipRect(ren, NULL);
    }
    for (int i = 0; i < jn_d.count; i++) {  // 按鈕與箭頭圖
        const UiCtrl *c = &jn_d.c[i];
        if ((c->type != 2 && c->type != 9) || !c->img || !(c->flags & 9) || (c->type == 9 && !strcmp(c->name, "m_pBackGround"))) continue;
        if (c->type == 9 && (!strncmp(c->name, "m_pLogBK", 8) || !strncmp(c->name, "m_pMissionBK", 12))) continue;
        if (jn_page && (jn_is_chapter_ctrl(c) || !strcmp(c->name, "m_pNextAni") || !strcmp(c->name, "m_pPrevAni"))) continue;
        draw(ui_image(c->img, 0), ox + c->x, oy + c->y);
    }
}

// 觸控放大鏡用：點在清單捲動箭頭上回 1（*r = 箭頭框）
static int journal_scroll_rect(int x, int y, SDL_Rect *r) {
    int ox, oy;
    jn_pos(&ox, &oy);
    static char b0[JN_LOGSZ], b1[JN_LOGSZ];
    const char *nm[2]; int ix[2]; char *tx[2];
    int n = jn_lists(nm, ix, tx, b0, b1, sizeof b0);
    for (int k = 0; k < n; k++) {
        const UiCtrl *c = ui_find(&jn_d, nm[k]);
        Scroll sc;
        if (!c || !text_scroll(c, ox, oy, &sc)) continue;
        if (in_rect(&sc.up, x, y)) { *r = sc.up; return -(ix[k] + 1); }
        if (in_rect(&sc.down, x, y)) { *r = sc.down; return ix[k] + 1; }
    }
    return 0;
}

static int journal_tap(int x, int y) {
    if (!journal_open) return 0;
    SDL_Rect r;
    int s = journal_scroll_rect(x, y, &r);
    if (s) { jn_top[abs(s) - 1] += s < 0 ? -3 : 3; return 1; }  // 範圍在畫的時候夾住
    const UiCtrl *c = jn_button(x, y);
    if (!c) return 1;
    if (!strcmp(c->name, "m_pCClose")) journal_open = 0;
    else if (!strcmp(c->name, "m_pLog")) jn_page = 0;
    else if (!strcmp(c->name, "m_pMission")) jn_page = 1;
    else if (!strcmp(c->name, "m_pPrevChapter")) { if (jn_view > 1) jn_view--; jn_top[0] = 0; }
    else if (!strcmp(c->name, "m_pNextChapter")) { if (jn_view < jn_chap) jn_view++; jn_top[0] = 0; }
    return 1;
}

#ifdef STORY
// 劇情 journal：記進資料，並照原版 0x1004dd00 存成玩家屬性 Log_<n> = "種類 編號 文字"（讀檔時重放；Chapter 與每則的章 LogChap_<n> 是引擎自己加的欄位）
static void journal_persist(int id, int flag, const char *text) {
    long before = jn_sig();
    jn_story(id, flag, text);
    int pl = story_player();
    if (!pl || jn_sig() == before) return;
    int n = story_get_int(pl, "LogCount");
    char k[24], v[JN_TXT + 24];
    snprintf(k, sizeof k, "Log_%d", n);
    snprintf(v, sizeof v, "%d %d %s", id < 1 ? 0 : flag == 1 ? 2 : 1, id < 1 ? -1 : id, text);
    story_set_str(pl, k, v);
    snprintf(k, sizeof k, "LogChap_%d", n);
    story_set_int(pl, k, jn_chap);
    story_set_int(pl, "LogCount", n + 1);
}
static void journal_replay(void) {  // 讀檔後從玩家屬性重建
    jn_reset();
    int pl = story_player();
    for (int i = 0, n = story_get_int(pl, "LogCount"); i < n; i++) {
        char k[24];
        snprintf(k, sizeof k, "Log_%d", i);
        const char *v = story_get_str(pl, k);
        int kind, id, off = 0;
        if (!v || sscanf(v, "%d %d %n", &kind, &id, &off) < 2) continue;
        snprintf(k, sizeof k, "LogChap_%d", i);
        jn_apply(kind, id, story_get_int(pl, k), v + off);
    }
    int c = story_get_int(pl, "Chapter");
    if (c >= 1 && c <= JN_CHAP) jn_chap = c;
    jn_pop = 0;
}
#endif
