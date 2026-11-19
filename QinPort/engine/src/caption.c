// 章節字幕（CChapterDialog JM69）與片尾字幕（CCreditsDialog JM33）。由 main.c 在 sys_tap／sys_draw 前 #include。
// 兩個都是蓋滿畫面的視窗：一張底圖加一塊會往上捲的文字，文字只畫在矩形框內。

// ---- 章節字幕（Poq.exe 0x4ffe40 開窗、0x566920 載一頁、0x566740 每拍捲動、0x566630 重畫、0x5664f0 結束）----
// 頁 p（2~11 = 第 1~10 章回顧、12 起 = 結局章 10~28）→ 章節 = p − 2（表 0x645168）；章節 < 10 一頁放好幾段（行數表 0x6450c8），
// ≥ 10 一頁一段。文字 id 表 0x6450f0（UI_C.dlg；同一張表也是配音檔 sound\C%.3d.wav 的編號）。
// 結局章分 7 組（0x645270 各組頁數 3、3、2、3、3、2、3）；組 g 的起頭頁 = 12 + 前面各組頁數和（0x4fca00 的迴圈）。
// 捲動：文字從框底升起（每拍 1 像素到閘門 +0x28c），之後每 +0x2b0 拍把閘門上移一行（16 像素，再用 16 拍升上去）；
// 文字剩不到 6 行高就換頁（章節 ≥ 10 且還有下一頁）或結束（章節 < 10 關窗；≥ 10 最後一頁 → 模式 12 = 片尾字幕）。
// 開始 0x566550 把 HUD 藏起來（這裡整個畫面蓋黑底）；結束 0x5664f0 停配音、HUD 回來；OK 鈕 0x566590：章節 ≥ 10 = 下一頁（最後一頁就結束），< 10 = 關窗。
// 配音 sound\C%.3d.wav（0x566920 每頁開始播，檔號 = 文字 id 表 ch_text；Mfx_LWave_IsPlaying 配音沒播完不換頁）：audio_voice(1000 + 檔號)。
//   每行間隔 +0x2b0 = (1000 / 每拍毫秒) × 配音秒數 / 行數（+0x2b8 = 文字高 / 行高）；沒有配音（或還沒載好）= 0 → 200 拍。網頁版配音要先下載，
//   shortcut: 下載中（audio_voice_state == 1）捲動暫停，載好才依長度算間隔（原版 Mfx_LWave_Play 同步播）；
//   字色 (192,200,184) 是 16 位元色 0xC657 轉的，原版的陰影／描邊旗標（DrawText 旗標 7）沒做；
//   遊戲模式旗標（原版開窗時寫 7、4、結束寫 12）反編譯裡沒有地方讀它，沒做。
#define CH_TOP 361   // 0x953a50~5c = 0x645568：框 (44, 361) – (597, 441)，兩種章節同一個框
#define CH_LEFT 44
#define CH_RIGHT 597
#define CH_BOTTOM 441
#define CH_LINE 16
static const int ch_lines[10] = {4, 3, 2, 2, 2, 3, 3, 4, 2, 3};
static const int ch_text[29] = {11, 15, 18, 20, 22, 24, 27, 30, 34, 36, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61};
static const int ch_group_pages[8] = {0, 3, 3, 2, 3, 3, 2, 3};  // 0x645270（第 0 筆 61 是配音表最後一筆，組從 1 起）
static struct {
    int open, page, chap, first, group, y, gate, h, step, tick, vwait;  // vwait：配音還沒載好，等載好再算 step  // 原版 +0x2a4、+0x29c、+0x2a0、+0x2a8、+0x288、+0x28c、+0x294、+0x2b0、DAT_00645164
    UiDialog d;
    char text[4096];
} ch;
static void credits_start(void);

static void ch_page(int page) {  // 0x566920
    ch.page = page;
    ch.chap = page < 2 ? 0 : page - 2;
    if (ch.chap > 28) ch.chap = 28;
    if (ch.first == -1) ch.first = ch.chap;
    ch.tick = 1;
    ch.y = CH_BOTTOM;
    ch.gate = ch.y - CH_LINE;
    ch.text[0] = 0;
    size_t n = 0;
    for (int i = 0; i < (ch.chap < 10 ? ch_lines[ch.chap] : 1); i++) {
        char t[1024];
        ui_text(ch_text[ch.chap] + i, t, sizeof t);
        n += (size_t)snprintf(ch.text + n, sizeof ch.text - n, "%s\n", t);
    }
    ch.h = draw_text(0, 0, ch.text, CH_RIGHT - CH_LEFT, 0, 0) - CH_LINE;
    ch.step = 200;  // 沒有配音：+0x2b0 = 0 → 200
    audio_voice(1000 + ch_text[ch.chap]);  // 0x57a6b0 播配音（沒有檔 = 停掉上一個）
    ch.vwait = 1;
}

static void ch_open(int page, int group) {  // 0x4ffe40（頁）、0x4fca00（負的章節 = 組）
    if (!ui_load("JM69_\xd5\xc2\xbd\xda\xbd\xe7\xc3\xe6", &ch.d)) return;
    ch.open = 1;
    ch.first = -1;
    ch.group = group;
    ch_page(page);
}

// 組 g（1~7）的起頭頁
static int ch_group_page(int g) {
    int p = 12;
    for (int k = 1; k < g && k < 8; k++) p += ch_group_pages[k];
    return p;
}

static void ch_close(void) {  // 0x5664f0：停配音
    audio_voice(-1);
    ch.open = 0;
}

static void ch_next(void) {  // 0x566590：結局章下一頁；最後一頁 = 結束 → 片尾字幕（模式 12）
    if (ch.chap < ch.first - 1 + ch_group_pages[ch.group > 0 && ch.group < 8 ? ch.group : 0]) ch_page(ch.page + 1);
    else { ch_close(); credits_start(); }
}

// 配音長度 → 每行間隔（0x566920 尾）：(1000 / 每拍毫秒) × 秒數 / 行數；行數 = 文字高 / 行高
static int ch_voice_step(float secs, int h) {
    int lines = h / CH_LINE, step = lines > 0 ? (1000 / game_speed_ms) * (int)secs / lines : 0;
    return step > 0 ? step : 200;
}
static void ch_tick(void) {  // 0x566740
    if (!ch.open) return;
    if (ch.vwait) {
        int st = audio_voice_state();
        if (st == 1) return;  // 下載中
        if (st == 2) ch.step = ch_voice_step(audio_voice_seconds(), ch.h);
        ch.vwait = 0;
    }
    if (ch.gate < ch.y) { ch.y--; ch.tick++; return; }
    if (ch.h - 5 * CH_LINE + ch.y < CH_TOP) {  // 文字剩不到 6 行：配音還在播就等（Mfx_LWave_IsPlaying）
        if (audio_voice_state() == 2) return;
        if (ch.chap < 10) ch_close();
        else ch_next();
    } else {
        if (ch.tick % ch.step == 0) ch.gate -= CH_LINE;
        ch.tick++;
    }
}

static void ch_draw(void) {  // 0x566630 + 底圖
    if (!ch.open) return;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, NULL);
    int ox = (vw - ch.d.w) / 2, oy = (vh - ch.d.h) / 2;
    UiCtrl *bg = ui_find(&ch.d, "m_pBackGround"), *title = ui_find(&ch.d, "m_pChapter");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);
    if (title) draw(ui_image(title->img, ch.chap), ox + title->x, oy + title->y);  // 0x5933c0：標題圖的第 章節 幀
    SDL_Rect clip = {ox + CH_LEFT, oy + CH_TOP, CH_RIGHT - CH_LEFT, CH_BOTTOM - CH_TOP};
    SDL_RenderSetClipRect(ren, &clip);
    draw_text(ox + CH_LEFT, oy + ch.y, ch.text, CH_RIGHT - CH_LEFT, 0xC0C8B8, 1);
    SDL_RenderSetClipRect(ren, NULL);
    UiCtrl *ok = ui_find(&ch.d, "m_pOK");
    if (ok) draw(ui_image(ok->img, 0), ox + ok->x, oy + ok->y);
}

static int ch_tap(int x, int y) {
    if (!ch.open) return 0;
    const UiCtrl *c = ui_button_at(&ch.d, (vw - ch.d.w) / 2, (vh - ch.d.h) / 2, x, y);
    if (c && !strcmp(c->name, "m_pOK")) { if (ch.chap >= 10) ch_next(); else ch_close(); }
    return 1;
}

// 劇情訊息 0x53a（Poq.exe 0x47e2xx → 0x4fca00）：v[1] = 章（高 16 位）、v[2] = 節；節 = 0 沒事。
// 章 ≥ 0：系統訊息 UI_C 0x5ef + 章（第二章。…最终章）；章 < 0：開結局章字幕第 −章 組；−100 = 設模式 2（沒讀者，不做）
static void chapter_message(int chap, int section) {
    if (!section || chap == -100) return;
    if (chap < 0) {
        if (-chap < 8) ch_open(ch_group_page(-chap), -chap);
        return;
    }
    char t[128];
    ui_text(0x5ef + chap, t, sizeof t);
    if (t[0]) hud_say(t);
}

// ---- 片尾字幕（Poq.exe CCreditsDialog：0x568310 開始、0x568620 重畫、0x568720 每拍、0x5685e0／0x5685f0 設速度、0x5682c0 結束）----
// 文字 = info9_C.dlg 第 0x81c（2076）筆的第 2 個有字的欄（「制作人员名单」…），Tab 換成 4 個空格；框 (515,64)–(719,538)（0x646030）；
// 從框底升起，每 +0x298 拍上移 1 像素（開始 3，按住 1 = 快轉）；文字整個捲出框頂就關窗。音樂 credit.wav（0x57a910(0)），關窗回場景音樂（0x57ab50）。
// shortcut: 原版放開才回 3，這裡每拍看滑鼠左鍵（手機的觸控也是滑鼠）；Esc 也能關（原版 m_Exit 是隱藏的）；陰影／描邊旗標 0x13 沒做
#define CR_LEFT 515
#define CR_TOP 64
#define CR_RIGHT 719
#define CR_BOTTOM 538
static struct { int open, y, h, tick, speed; UiDialog d; char text[2048]; } cr;
static int caption_music;  // 1 = 播 credit，2 = 回場景音樂（主迴圈處理）

static void credits_start(void) {  // 0x568310
    char g[2048], u[4096];
    if (!ui_load("JM33_\xd6\xc6\xd7\xf7\xc8\xba\xbd\xe7\xc3\xe6", &cr.d)) return;
    if (!res_dlg_nth("info9_C.dlg", 0x81c, 1, g, sizeof g)) return;
    gbk_utf8(g, u, sizeof u);
    size_t k = 0;
    for (const char *p = u; *p && k + 5 < sizeof cr.text; p++) {  // [0…] 顏色標記拿掉（同 ui_text）
        if (*p == '[' && p[1] >= '0' && p[1] <= '9') { p++; continue; }
        if (*p == ']') continue;
        if (*p == '\t') { memcpy(cr.text + k, "    ", 4); k += 4; continue; }
        if (*p != '\r') cr.text[k++] = *p;
    }
    cr.text[k++] = '\n';
    cr.text[k] = 0;
    cr.h = draw_text(0, 0, cr.text, CR_RIGHT - CR_LEFT, 0, 0);
    cr.open = 1;
    cr.y = CR_BOTTOM;
    cr.tick = 0;
    cr.speed = 3;
    caption_music = 1;
}

static void credits_close(void) {  // 0x5682c0
    cr.open = 0;
    caption_music = 2;
}

static void credits_tick(void) {  // 0x568720
    if (!cr.open) return;
    cr.speed = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK ? 1 : 3;  // 0x5685e0／0x5685f0
    if (cr.h + cr.y < CR_TOP) { credits_close(); return; }
    if (cr.tick++ % cr.speed == 0) cr.y--;
}

static void credits_draw(void) {  // 0x568620
    if (!cr.open) return;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, NULL);
    int ox = (vw - cr.d.w) / 2, oy = (vh - cr.d.h) / 2;
    UiCtrl *bg = ui_find(&cr.d, "m_pBackGround");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);
    SDL_Rect clip = {ox + CR_LEFT, oy + CR_TOP, CR_RIGHT - CR_LEFT, CR_BOTTOM - CR_TOP};
    SDL_RenderSetClipRect(ren, &clip);
    draw_text(ox + CR_LEFT, oy + cr.y, cr.text, CR_RIGHT - CR_LEFT, 0xC8C8B8, 1);
    SDL_RenderSetClipRect(ren, NULL);
}

static int caption_tap(int x, int y) {
    if (cr.open) return 1;
    return ch_tap(x, y);
}
static int caption_esc(void) {  // Esc：關掉字幕（回 1 = 有字幕開著）
    if (cr.open) { credits_close(); return 1; }
    if (ch.open) { ch_close(); return 1; }
    return 0;
}
static void caption_tick(void) { ch_tick(); credits_tick(); }
static void caption_draw(void) { ch_draw(); credits_draw(); }
