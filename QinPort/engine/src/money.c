// 金錢：銀行（EasyRPG.dll 0x1003ac40）、丟錢（0x1003b640、0x10054830）與兩個輸入視窗。由 main.c 在 new_open 前 #include。
// 原版：Poq 背包 JM54 的 m_pMoney 按下 → 0x54b990 開 JM109_扔钱界面（數字輸入框＋確定／取消）→ 確定 0x5465d0 送訊息 0x1ff {金額, 人物} →
// EasyRPG 0x1003b640。銀行只有 GM 文字指令 `bank <0 取 1 存> <金額>`（0x100127c0，權限檢查 0x100566b0）與訊息 0xbf，Poq 沒有銀行視窗。
// shortcut: 銀行視窗是這裡自訂的最小介面（JM109 的底圖與輸入框＋存入／取出／取消三鈕，按 B 鍵開），原版沒有；規則照 0x1003ac40。


// 0x1003d3e0：加減 Money；結果 < 0 或 > 499999999 就歸零（單機玩家看得到的訊息 "The amount of the money you have is too much, we changed to zero."）
static void money_add(int d) {
    long long m = (long long)hero_money + d;
    if (m < 0 || m > 499999999) {
        hero_money = 0;
        snprintf(toast, sizeof toast, "The amount of the money you have is too much, we changed to zero.");
        toast_until = SDL_GetTicks() + 2500;
    } else hero_money = (int)m;
}

// EasyRPG 訊息裡的 [C你]、[A%d] 是「[色碼 文字]」，秀字時把外框拿掉
static void money_tags(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (r[0] == '[' && r[1] >= 'A' && r[1] <= 'Z') { r++; continue; }
        if (*r == ']') continue;
        *w++ = *r;
    }
    *w = 0;
}
static void money_say(const char *a, const char *b) {
    char t[512];
    snprintf(t, sizeof t, "%s%s%s", a, b[0] ? "  " : "", b);
    money_tags(t);
    snprintf(toast, sizeof toast, "%s", t);
    toast_until = SDL_GetTicks() + 3500;
}
static void money_msg_fmt(char *out, size_t cap, int id, int a, int b) {  // EasyRPG_C.dlg 第 id 筆（GBK 格式字串）
    char f[256], g[256];
    out[0] = 0;
    if (!res_dlg("EasyRPG_C.dlg", id, f, sizeof f)) return;
    snprintf(g, sizeof g, f, a, b);
    gbk_utf8(g, out, cap);
}

// 0x1003ac40 銀行：mode 0 取款、非 0 存款。金額 < 1 → 3101；存款錢不夠 3097、取款庫存不夠 3098；成功 3099（存入）／3100（取出）；之後都再秀 3102「身上 錢庄」。
// 回傳 1 = 成功（測試用）
static int bank_op(int mode, int amount) {
    char m1[256] = "", m2[256] = "";
    int ok = 0;
    if (amount < 1) { money_msg_fmt(m1, sizeof m1, 3101, 0, 0); money_say(m1, ""); return 0; }
    if (mode == 0) {
        if (amount <= hero_bank) { money_add(amount); hero_bank -= amount; money_msg_fmt(m1, sizeof m1, 3100, amount, 0); ok = 1; }
        else money_msg_fmt(m1, sizeof m1, 3098, amount, 0);
    } else {
        if (hero_money >= amount) { money_add(-amount); hero_bank += amount; money_msg_fmt(m1, sizeof m1, 3099, amount, 0); ok = 1; }
        else money_msg_fmt(m1, sizeof m1, 3097, amount, 0);
    }
    money_msg_fmt(m2, sizeof m2, 3102, hero_money, hero_bank);  // 第 1 個 %d = 身上（Money）、第 2 個 = 錢庄（Bank）
    money_say(m1, m2);
    return ok;
}

// 0x10054830：地上金錢的圖（G_PicID）依金額：< 300 → 0x7e9、< 1000 → 0x7ea、否則 0x7eb
static int money_pic(int amount) { return amount < 300 ? 0x7e9 : (amount > 999) + 0x7ea; }

// 0x1003b640 丟錢：Money 不夠 → 訊息 3029 並改丟全部；> 0 才丟：Money 減掉，在腳下放一個「秦殇/金钱」（Amount、G_PicID）
// 回傳實際丟出的金額
static int money_drop(int amount) {
    if (hero_money < amount) { hud_msg(3029); amount = hero_money; }
    if (amount <= 0) return 0;
    Item it;
    if (!item_new(&it, 60, 1, -1, NULL)) return 0;  // 金錢 = 道具類型 60，ground_pick 撿到時把 amount 加進 Money
    money_add(-amount);
    it.amount = amount;
    it.ground = money_pic(amount);
    ground_drop(&it, actors[0].x, actors[0].y);
    return amount;
}

// ---- 數字輸入視窗（JM109）：kind 0 = 丟錢（Poq 0x54b990 開、0x5465d0 確定）、1 = 存入、2 = 取出（自訂）----
static struct { int open, kind; char text[12]; UiDialog d; } mny;

#define JM109 "JM109_\xc8\xd3\xc7\xae\xbd\xe7\xc3\xe6"  // JM109_扔钱界面
static void money_open(int kind) {
    if (mny.open || ui_load(JM109, &mny.d) <= 0) return;
    mny.open = 1;
    mny.kind = kind;
    mny.text[0] = 0;
    SDL_StartTextInput();
}
static void money_close(void) { mny.open = 0; SDL_StopTextInput(); }
static void money_text(const char *t) {  // SDL_TEXTINPUT：只收數字，最多 9 位（上限 499999999 以內）
    for (; *t; t++) {
        size_t k = strlen(mny.text);
        if (*t >= '0' && *t <= '9' && k < 9) { mny.text[k] = *t; mny.text[k + 1] = 0; }
    }
}
static void money_back(void) { size_t k = strlen(mny.text); if (k) mny.text[k - 1] = 0; }
static void money_ok(int bank_mode) {  // 確定（0x5465d0）：丟錢 / 銀行（存 = 1、取 = 0）
    int v = atoi(mny.text);
    if (mny.kind == 0) money_drop(v);
    else bank_op(bank_mode, v);
    if (mny.kind == 0 || v >= 1) money_close();
}
// 銀行三鈕的位置（在 JM109 底圖下緣一排）
static SDL_Rect money_btn(int i) {
    int ox = (vw - mny.d.w) / 2, oy = (vh - mny.d.h) / 2, w = mny.d.w / 3 - 14;
    return (SDL_Rect){ox + 10 + i * (w + 10), oy + mny.d.h - 40, w, 28};
}
static int money_tap(int x, int y) {
    if (!mny.open) return 0;
    int ox = (vw - mny.d.w) / 2, oy = (vh - mny.d.h) / 2;
    UiCtrl *f = ui_find(&mny.d, "m_pMoney");
    if (f && x >= ox + f->x && x < ox + f->x + f->w && y >= oy + f->y - 4 && y < oy + f->y + f->h + 4) { SDL_StartTextInput(); return 1; }  // 點輸入框：手機叫出鍵盤
    if (mny.kind == 0) {
        const UiCtrl *c = ui_button_at(&mny.d, ox, oy, x, y);
        if (c && !strcmp(c->name, "m_pOK")) money_ok(0);
        else if (c && !strcmp(c->name, "m_pCancel")) money_close();
    } else {
        for (int i = 0; i < 3; i++) {
            SDL_Rect r = money_btn(i);
            if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) { if (i == 2) money_close(); else money_ok(i == 0); }
        }
    }
    return 1;
}
static void money_draw(void) {
    if (!mny.open) return;
    int ox = (vw - mny.d.w) / 2, oy = (vh - mny.d.h) / 2;
    UiCtrl *bg = ui_find(&mny.d, "m_pBackGround");
    if (bg) draw(ui_image(bg->img, 0), ox + bg->x, oy + bg->y);
    if (mny.kind == 0) ui_draw(&mny.d, ox, oy);
    UiCtrl *f = ui_find(&mny.d, "m_pMoney");
    if (f) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
        SDL_Rect box = {ox + f->x - 2, oy + f->y - 2, f->w + 4, f->h + 6};
        SDL_RenderFillRect(ren, &box);
        draw_text(ox + f->x, oy + f->y, mny.text, f->w, 0xFFFFFF, 1);
        if (SDL_GetTicks() / 500 % 2) draw_text(ox + f->x + text_width(mny.text), oy + f->y, "|", 8, 0xFFFFFF, 1);
    }
    if (mny.kind) {
        char t[256], u[256];
        money_msg_fmt(t, sizeof t, 3102, hero_money, hero_bank);
        snprintf(u, sizeof u, "%s", t);
        money_tags(u);
        draw_text(ox + 10, oy + 14, u, mny.d.w - 20, 0xBECDCE, 1);
        static const char *const L[3] = {"\xe5\xad\x98\xe5\x85\xa5", "\xe5\x8f\x96\xe5\x87\xba", "\xe5\x8f\x96\xe6\xb6\x88"};  // 存入、取出、取消
        for (int i = 0; i < 3; i++) {
            SDL_Rect r = money_btn(i);
            SDL_SetRenderDrawColor(ren, 40, 50, 60, 220);
            SDL_RenderFillRect(ren, &r);
            SDL_SetRenderDrawColor(ren, 190, 205, 206, 255);
            SDL_RenderDrawRect(ren, &r);
            draw_text(r.x + (r.w - text_width(L[i])) / 2, r.y + 6, L[i], r.w, 0xBECDCE, 1);
        }
    }
}
