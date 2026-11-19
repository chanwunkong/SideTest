// 不用畫面測劇情：storytest <物件路徑> [選項序號...]，例如 storytest 秦殇/咸阳/NPC/绣女 0 1
// 參數 give=物件路徑 先放到主角身上，flag=旗標=值 先設主角的旗標（放在選項前面）；最後印主角身上的東西
// 主線走查：storytest walk=-（腳本從標準輸入，用 tools/walk.sh 餵；指令見 walk_test）；make walktest 跑回歸
#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qinstory.h"

static void show(void) {
    printf("【%s】%s\n", story_dialog.speaker, story_dialog.text);
    for (int i = 0; i < story_dialog.option_count; i++) printf("  %d. %s（%s）\n", i, story_dialog.options[i].text, story_dialog.options[i].key);
}

static int skill_stub(int h, const char *key, int level) { story_set_alias(h, key); story_set_int(h, "Level_Max", 10); story_set_int(h, "Level_Req", 1); story_set_int(h, "Level", level); return 1; }
static int exp_for(int lv) { return lv * lv * 10; }  // 測試用的經驗表（升到 lv+1 要 lv²×10）

// storytest team=<隊友路徑> [exp=主角經驗]：隊友入隊（0x10037850 → 自動加點 0x100295b0），印等級、經驗、點數、屬性和技能等級
static int team_test(const char *path, int exp) {
    int pl = story_player();
    story_exp_for = exp_for, story_exp_max = 40, story_on_skill = skill_stub;
    story_set_int(pl, "Exp", exp);
    int m = story_new(path, 0);
    printf("入隊前 Level %d Exp %d Extra_Exp %d Wisdom %d Wisdom0 %d pic2 %d Savvy %d 主角 Savvy %d\n", story_get_int(m, "Level"), story_get_int(m, "Exp"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Wisdom"), story_get_int(m, "Wisdom0"), story_get_int(m, "G_PicID2"), story_get_int(m, "Savvy"), story_get_int(pl, "Savvy"));
    engine_call(0x10037850, (intptr_t[8]){pl, m});
    printf("隊友 %s Level %d Exp %d NextExp %d Extra_Exp %d Extra_Skill %d\n", story_name(m), story_get_int(m, "Level"), story_get_int(m, "Exp"),
           story_get_int(m, "NextExp"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Extra_Skill"));
    printf("屬性 Str %d Con %d Dex %d Savvy %d Charm %d Wisdom %d\n", story_get_int(m, "Str"), story_get_int(m, "Con"), story_get_int(m, "Dex"),
           story_get_int(m, "Savvy"), story_get_int(m, "Charm"), story_get_int(m, "Wisdom"));
    for (int h = story_next(m, 0); h; h = story_next(m, h)) printf(" %s=%d", story_alias(h), story_get_int(h, "Level"));
    printf("\n");
    // 檢查：術士方仲（G_PicID2 18，表：Wisdom 基礎 35、每級 3 點）優先加 Wisdom；技能點花掉一部分（剩下 < 升的級數）
    int lv = story_get_int(m, "Level"), wis = 35 + (int)(3.0f * (lv - 1)), skills = 0, learned = 0;
    for (int h = story_next(m, 0); h; h = story_next(m, h)) if (story_get_int(h, "Level") > 0) skills += story_get_int(h, "Level"), learned++;
    if (story_get_int(m, "Wisdom") != wis || story_get_int(m, "Extra_Skill") >= lv || !learned) { printf("FAIL team_autoadd wis %d/%d skills %d\n", story_get_int(m, "Wisdom"), wis, skills); return 1; }
    story_gain_exp(m, 1000);
    if (story_get_int(m, "Level") <= lv || story_get_int(m, "Extra_Skill") <= 0) { printf("FAIL story_gain_exp\n"); return 1; }
    printf("team ok\n");  // 再加經驗連升級：每級 Extra_Exp +10、Extra_Skill +1
    printf("加 1000 經驗後 Level %d Extra_Exp %d Extra_Skill %d\n", story_get_int(m, "Level"), story_get_int(m, "Extra_Exp"), story_get_int(m, "Extra_Skill"));
    return 0;
}

// storytest hero=<主角範本路徑>：創角選的範本（story_player_choose）→ 職業、角色表列、外觀字串、屬性、起始道具
static int hero_test(const char *path) {
    story_register_all();
    story_player_choose(path);
    int pl = story_player(), prof = story_get_int(pl, "IsActorNPC"), items = 0;
    char k[16];
    snprintf(k, sizeof k, "G_EquipStr%d", prof);
    const char *eq = story_get_str(pl, k);
    for (int h = story_next(pl, 0); h; h = story_next(pl, h)) items++;
    printf("%s 職業 %d G_PicID %d G_PicID2 %d 外觀 %s Str %d Charm %d 道具 %d\n", story_path(pl), prof, story_get_int(pl, "G_PicID"), story_get_int(pl, "G_PicID2"), eq, story_get_int(pl, "Str"), story_get_int(pl, "Charm"), items);
    if (strcmp(story_path(pl), path) || prof < 1 || prof > 5 || !eq || !*eq || !items || story_get_int(pl, "G_PicID2") / 4 != prof) return 1;
    printf("hero ok\n");
    return 0;
}

// storytest room=<房間別名>：第一次進房間（story_new 後立刻第 6 格，原版 newobject）箱子子類（礦、樹、水缸）就有隨機掉落
static int room_test(const char *alias) {
    story_register_all();
    story_player();
    int r = story_room(alias), chests = 0, filled = 0;
    for (int h = story_next(r, 0); h; h = story_next(r, h)) {
        if (!story_get_int(h, "isbuilding")) continue;
        chests++;
        if (story_next(h, 0)) filled++;
    }
    printf("%s 箱子 %d 有掉落 %d\n", alias, chests, filled);
    if (!chests || !filled) return 1;
    printf("room ok\n");
    return 0;
}

// storytest aigroup=<人物路徑>：c_aigroup（0x10013270）把主角房間裡 G_AIGroup 102 的人物改成 0，存 G_AIGroup_Save；別組不動；新組 < 100 清 Invincible
static int aigroup_test(const char *path) {
    story_register_all();
    int room = story_room("zhaocun1");
    story_place(room);
    int a = story_new(path, room), b = story_new(path, room);
    story_set_int(a, "G_AIGroup", 102); story_set_int(a, "Invincible", 1); story_set_int(b, "G_AIGroup", 103);
    story_c_aigroup(102, 0);
    printf("a: 組 %d 存 %d 無敵 %d；b: 組 %d\n", story_get_int(a, "G_AIGroup"), story_get_int(a, "G_AIGroup_Save"), story_get_int(a, "Invincible"), story_get_int(b, "G_AIGroup"));
    if (story_get_int(a, "G_AIGroup") != 0 || story_get_int(a, "G_AIGroup_Save") != 102 || story_get_int(a, "Invincible") || story_get_int(b, "G_AIGroup") != 103) { puts("aigroup FAILED"); return 1; }
    puts("aigroup ok");
    return 0;
}


// ---- storytest walk=<腳本檔>：主線走查（無畫面）。一個行程內連續推進，旗標、道具、日誌都保留。
// 指令：start | go <出口> | enter <房間別名> | talk <別名> [選項序號或鍵…] | plot <路徑> | flag 名=值 | expect 名=值 | has <道具名> | say <文字>
// 對話、旗標變化、日誌、章節、劇情事件（自動播完＝呼叫第 18 格）都印出來；expect／has 不符印 FAIL 並讓結束碼為 1
static int walk_fail;
static void walk_log(int h, int kind, const char *t) { printf("  [日誌 kind=%d] %.100s\n", kind, t); (void)h; }
static void walk_msg(int id, const intptr_t *v, int n, const char *s) {
    if (id == 0x53a) printf("  [章節 章=%d 節=%d]\n", (int)v[1], (int)v[2]);
    else if (id == 0x52e) printf("  [過場 %s]\n", s);
    else if (id == 0x5dd) return;
    (void)n;
}
static const char *walk_text(int id) { (void)id; return "(系統訊息)"; }
static int walk_pl_flags[512]; static char walk_pl_keys[512][32]; static int walk_pl_n;
static void walk_flag_diff(void) {  // 印主角旗標與上次的差異
    int pl = story_player(), v, i;
    const char *k;
    for (i = 0; (k = story_flag_at(pl, i, &v)); i++) {
        int j;
        for (j = 0; j < walk_pl_n; j++) if (!strcmp(walk_pl_keys[j], k)) break;
        if (j == walk_pl_n && j < 512) { snprintf(walk_pl_keys[j], 32, "%s", k); walk_pl_n++; walk_pl_flags[j] = 0x7fffffff; }
        if (walk_pl_flags[j] != v) { printf("  旗標 %s: %s -> %d\n", k, walk_pl_flags[j] == 0x7fffffff ? "(新)" : (snprintf(walk_pl_keys[511], 32, "%d", walk_pl_flags[j]), walk_pl_keys[511]), v); walk_pl_flags[j] = v; }
    }
}
static int walk_flag(const char *name) { int pl = story_player(), v, i; const char *k; for (i = 0; (k = story_flag_at(pl, i, &v)); i++) if (!strcmp(k, name)) return v; return -9999; }
static void walk_plot_run(void) {
    while (story_plot.h) { printf("  [劇情事件 %s 播完 → 第 18 格]\n", story_plot.alias); story_plot_end(); }
}
static void walk_inv(void) { int pl = story_player(); printf("  主角身上：");
    for (int h = story_next(pl, 0); h; h = story_next(pl, h)) printf(" %s", story_name(h)); printf("\n"); }
static int walk_here(void) { return story_parent(story_player()); }
static int walk_enter(int to, const char *how) {
    int ok = to && story_enter(to);
    printf("  %s → %s %s\n", how, to ? story_room_alias(to) : "(無此房間)", ok ? "進入" : "【進不去】");
    if (!ok) walk_fail = 1;
    walk_plot_run();
    return ok;
}
static void walk_show(void) {
    printf("  【%s】%.300s\n", story_dialog.speaker, story_dialog.text);
    for (int i = 0; i < story_dialog.option_count; i++) printf("    %d. %.60s（%s）\n", i, story_dialog.options[i].text, story_dialog.options[i].key);
}

// ---- walk 的 explore：自動把能談的 NPC 全問一遍、能走的出口全走一遍，重複到沒有新進展為止（單機劇情「可達」檢查）
// 假設：主角有 2000 文錢；BNT 感應區的劇情事件（trig 房間 路徑，由 tools/webtrig.mjs 掃出）進房間就觸發；
// 不模擬戰鬥、撿地上道具、點建築（砍樹、開箱）——要這些才推進的環節會顯示成「卡住」，要另外用瀏覽器驗證
static char ex_trig_room[3000][40], ex_trig_path[3000][100]; static int ex_ntrig;
static char ex_seen[40000][24]; static int ex_nseen;  // 對話節點簽名（NPC 路徑雜湊＋文字＋選項）
static int ex_novel;
static int ex_room_state[65536];  // 0 沒去過 1 去過 2 進不去
static int ex_known[4096], ex_nknown;
static void *ex_item_make(int h, int type, int level, int col, const char *alias) { (void)type; (void)level; (void)col; story_set_alias(h, alias ? alias : ""); return calloc(1, 256); }
static unsigned ex_hash(const char *s, unsigned h) { while (*s) h = h * 131 + (unsigned char)*s++; return h; }
static int ex_new_sig(unsigned h) { char b[24]; snprintf(b, sizeof b, "%u", h); for (int i = 0; i < ex_nseen; i++) if (!strcmp(ex_seen[i], b)) return 0; if (ex_nseen < 40000) snprintf(ex_seen[ex_nseen++], 24, "%s", b); return 1; }
static void ex_note_room(int r) { for (int i = 0; i < ex_nknown; i++) if (ex_known[i] == r) return; if (ex_nknown < 4096) ex_known[ex_nknown++] = r; }
static char ex_ctx[160];
static void ex_flag_diff(void) {  // 旗標變化：只印新（名稱，值）組合，附是誰推動的
    int pl = story_player(), v, i;
    const char *k;
    for (i = 0; (k = story_flag_at(pl, i, &v)); i++) {
        int j;
        for (j = 0; j < walk_pl_n; j++) if (!strcmp(walk_pl_keys[j], k)) break;
        if (j == walk_pl_n && j < 500) { snprintf(walk_pl_keys[j], 32, "%s", k); walk_pl_n++; walk_pl_flags[j] = 0x7fffffff; }
        if (walk_pl_flags[j] != v) { printf("    旗標 %s %d -> %d  ← %s\n", k, walk_pl_flags[j] == 0x7fffffff ? -1 : walk_pl_flags[j], v, ex_ctx); walk_pl_flags[j] = v; ex_novel++; }
    }
}
// 依「開著的出口」找路：從主角現在的房間廣度優先走到 r，每一步都走原版的進出房間檢查（第 13／14 格）
static int ex_enter(int r) {
    int pl = story_player(), old = story_parent(pl);
    if (old == r) return 1;
    static int prev[65536], seen[65536], q[65536]; int qh = 0, qt = 0;
    memset(seen, 0, sizeof seen);
    q[qt++] = old; seen[old] = 1; prev[old] = 0;
    while (qh < qt && !seen[r]) {
        int cur = q[qh++];
        for (int i = 0; ; i++) {
            const char *dir, *path; int entry, on;
            if (!story_exit_nth(cur, i, &dir, &path, &entry, &on)) break;
            if (!on) continue;
            int to = story_room_by_path(path);
            if (to && to < 65536 && !seen[to]) { seen[to] = 1; prev[to] = cur; q[qt++] = to; }
        }
    }
    if (!seen[r]) return 0;
    int hops[64], nh = 0;
    for (int c = r; c != old && nh < 64; c = prev[c]) hops[nh++] = c;
    for (int i = nh - 1; i >= 0; i--) {
        if (!story_enter(hops[i])) { printf("  （路上 %s → %s 被擋）\n", story_room_alias(story_parent(pl)), story_room_alias(hops[i])); return 0; }
        walk_plot_run();
    }
    return 1;
}
static void ex_fire_plots(int room) {
    const char *al = story_room_alias(room);
    for (int i = 0; i < ex_ntrig; i++) if (!strcmp(ex_trig_room[i], al)) {
        snprintf(ex_ctx, sizeof ex_ctx, "觸發區 %s → %s", al, ex_trig_path[i]);
        story_pct_start(ex_trig_path[i]);
        if (story_plot.h) { printf("    過場 %s（%s）\n", story_plot.alias, ex_trig_path[i]); ex_novel++; }
        walk_plot_run();
        ex_flag_diff();
    }
}
// 問一個 NPC：廣度優先走遍所有選項
static void ex_npc(int npc, const char *roomal) {
    char (*q)[10][16] = malloc(60 * sizeof *q); int qn[60], nq = 0, qi = 0;
    memset(qn, 0, sizeof qn); nq = 1; qn[0] = 0;
    int pl = story_player(), room0 = story_parent(pl);
    while (qi < nq && qi < 60) {
        int d = qn[qi]; char keys[10][16]; memcpy(keys, q[qi], sizeof keys); qi++;
        story_talk(npc);
        int ok = 1;
        for (int i = 0; i < d && ok; i++) {
            int k = -1;
            for (int j = 0; j < story_dialog.option_count; j++) if (!strcmp(story_dialog.options[j].key, keys[i])) k = j;
            if (k < 0) { ok = 0; break; }
            { char pp[120] = ""; for (int t = 0; t <= i; t++) { strncat(pp, t ? ">" : "", 100); strncat(pp, keys[t], 100); } snprintf(ex_ctx, sizeof ex_ctx, "%s %s [%s]", roomal, story_alias(npc), pp); }
            story_choose(k);
            walk_plot_run();
            ex_flag_diff();
            if (story_parent(pl) != room0) { ex_note_room(story_parent(pl)); ok = 0; }
        }
        if (!ok) continue;
        unsigned h = ex_hash(story_path(npc), 7);
        h = ex_hash(story_dialog.text, h);
        for (int j = 0; j < story_dialog.option_count; j++) h = ex_hash(story_dialog.options[j].key, ex_hash(story_dialog.options[j].text, h));
        if (!ex_new_sig(h)) continue;
        ex_novel++;
        printf("    對話 %s/%s 深度%d：%.40s｜選項 %d\n", roomal, story_alias(npc), d, story_dialog.text[0] ? story_dialog.text : "(無台詞)", story_dialog.option_count);
        if (d < 10) for (int j = 0; j < story_dialog.option_count && nq < 60; j++) { memcpy(q[nq], keys, sizeof keys); snprintf(q[nq][d], 16, "%s", story_dialog.options[j].key); qn[nq++] = d + 1; }
    }
    free(q);
}
static void ex_room(int room) {
    int pl = story_player();
    const char *al = story_room_alias(room);
    if (!ex_enter(room)) { if (ex_room_state[room] != 2) { printf("  進不去 %s\n", al); ex_room_state[room] = 2; } return; }
    if (ex_room_state[room] != 1) { printf("  進入 %s（%s）\n", al, story_name(room)); ex_room_state[room] = 1; ex_novel++; walk_plot_run(); ex_fire_plots(room); }
    if (story_parent(pl) != room) { ex_note_room(story_parent(pl)); return; }
    int skip[16], ns = 0;
    for (int m = story_team_next(pl, 0); m && ns < 16; m = story_team_next(pl, m)) skip[ns++] = m;
    int list[400], n = 0;
    for (int h = story_next(room, 0); h && n < 400; h = story_next(room, h)) {
        if (story_kind(h) != 1 || h == pl) continue;
        int sk = 0; (void)ns; (void)skip;  /* 隊友也要問（孔刚是主線的嚮導） */
        if (!sk) list[n++] = h;
    }
    for (int i = 0; i < n; i++) { ex_npc(list[i], al); if (story_parent(pl) != room) { ex_note_room(story_parent(pl)); return; } }
    for (int i = 0; ; i++) {
        const char *dir, *path; int entry, on;
        if (!story_exit_nth(room, i, &dir, &path, &entry, &on)) break;
        int to = story_room_by_path(path);
        if (to) ex_note_room(to);
    }
}
static void explore_run(int maxr) {
    story_item_make = ex_item_make;
    engine_call(0x1003d3e0, (intptr_t[8]){story_player(), 2000});
    int r0 = story_parent(story_player());
    ex_note_room(r0);
    for (int round = 1; round <= maxr; round++) {
        ex_novel = 0;
        printf("== 第 %d 輪（已知房間 %d）\n", round, ex_nknown);
        for (int i = 0; i < ex_nknown; i++) ex_room(ex_known[i]);
        if (!ex_novel) { printf("== 沒有新進展，停止\n"); break; }
    }
    // 第二階段：打倒有死亡處理的人物（第 2、10、12 格被覆寫）、撿起有撿起處理的道具（第 15 格），然後再探一遍
    for (int phase = 0; phase < 3; phase++) {
        int did = 0;
        for (int i = 0; i < ex_nknown; i++) {
            int room = ex_known[i];
            if (!ex_enter(room)) continue;
            int pl = story_player();
            for (int h = story_next(room, 0), nx; h; h = nx) {
                nx = story_next(room, h);
                if (story_kind(h) == 1 && h != pl && (story_has_slot(h, 10) || story_has_slot(h, 2) || story_has_slot(h, 12))) {
                    snprintf(ex_ctx, sizeof ex_ctx, "打倒 %s/%s", story_room_alias(room), story_alias(h));
                    if (story_die(h)) { printf("  打倒 %s/%s\n", story_room_alias(room), story_alias(h)); did++; walk_plot_run(); ex_flag_diff(); }
                    if (story_parent(pl) != room) break;
                }
            }
            for (int h = story_next(room, 0), nx; h; h = nx) {
                nx = story_next(room, h);
                for (int c = story_next(h, 0), cn; c && story_kind(h) != 1; c = cn) {
                    cn = story_next(h, c);
                    if (story_has_slot(c, 15)) {
                        snprintf(ex_ctx, sizeof ex_ctx, "撿 %s/%s", story_room_alias(room), story_path(c));
                        if (story_move_full(pl, c)) { printf("  撿起 %s/%s\n", story_room_alias(room), story_path(c)); did++; walk_plot_run(); ex_flag_diff(); }
                    }
                }
                if (story_has_slot(h, 15) && story_kind(h) == 2) {
                    snprintf(ex_ctx, sizeof ex_ctx, "撿 %s/%s", story_room_alias(room), story_path(h));
                    if (story_move_full(pl, h)) { printf("  撿起 %s/%s\n", story_room_alias(room), story_path(h)); did++; walk_plot_run(); ex_flag_diff(); }
                }
            }
        }
        printf("== 第二階段 %d：%d 件\n", phase + 1, did);
        for (int round = 1; round <= maxr; round++) {
            ex_novel = 0;
            for (int i = 0; i < ex_nknown; i++) ex_room(ex_known[i]);
            if (!ex_novel) break;
        }
        if (!did) break;
    }
    int blocked = 0, visited = 0;
    for (int i = 0; i < ex_nknown; i++) { if (ex_room_state[ex_known[i]] == 2) {
            blocked++; printf("卡住的房間 %s ←", story_room_alias(ex_known[i]));
            for (int j = 0; j < ex_nknown; j++) for (int e = 0; ; e++) {  // 誰有出口通到這裡、開不開
                const char *dir, *path; int entry, on;
                if (!story_exit_nth(ex_known[j], e, &dir, &path, &entry, &on)) break;
                if (story_room_by_path(path) == ex_known[i]) printf(" %s.%s%s", story_room_alias(ex_known[j]), dir, on ? "" : "(關)");
            }
            printf("\n");
        } else if (ex_room_state[ex_known[i]] == 1) visited++; }
    printf("去過 %d 間、進不去 %d 間、沒走到 %d 間\n", visited, blocked, ex_nknown - visited - blocked);
}

static int walk_test(const char *file) {
    story_register_all();
    story_on_log = walk_log; story_on_msg = walk_msg; story_on_text = walk_text; story_on_skill = skill_stub;
    story_exp_for = exp_for; story_exp_max = 40;
    int pl = story_player();
    FILE *f = !strcmp(file, "-") ? stdin : fopen(file, "r");  // walk=- 讀標準輸入（node 版 wasm 看不到主機檔案）
    char line[8192];
    if (!f) { printf("開不了 %s\n", file); return 1; }
    walk_flag_diff();
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        for (char *r = line, *w = line;; r++) {  // node 的標準輸入會弄壞部分位元組：腳本裡的非 ASCII 寫成 %XX（tools/walk.sh 轉）
            if (*r == '%' && r[1] && r[2]) { char h[3] = {r[1], r[2], 0}; *w++ = (char)strtol(h, NULL, 16); r += 2; } else if (!(*w++ = *r)) break;
        }
        if (!line[0] || line[0] == '#') { if (line[0] == '#') printf("%s\n", line); continue; }
        printf("> %s\n", line);
        char *tok[24]; int nt = 0;
        for (char *p = strtok(line, " \t"); p && nt < 24; p = strtok(NULL, " \t")) tok[nt++] = p;
        const char *c = tok[0];
        if (!strcmp(c, "start")) { int r = story_room(story_start_room()); story_start(r); printf("  開場房間 %s\n", story_room_alias(r)); walk_plot_run(); }
        else if (!strcmp(c, "enter") && nt > 1) walk_enter(story_room(tok[1]), "enter");
        else if (!strcmp(c, "go") && nt > 1) {
            char path[128]; int entry = 0, room = walk_here();
            if (!story_exit(room, tok[1], path, sizeof path, &entry)) { printf("  出口 %s 沒有或沒開（房間 %s）【出不去】\n", tok[1], story_room_alias(room)); walk_fail = 1; continue; }
            story_random_exit(room, tok[1], path, sizeof path, &entry);
            walk_enter(story_room_by_path(path), "go");
        } else if (!strcmp(c, "canopen") && nt > 1) {  // 開箱規則的「能不能開」（箱子／門第 17 格）：物件放在目前房間，印回傳值
            int h = story_new(tok[1], walk_here()), r = story_can_open(h); printf("  story_can_open = %d\n", r); if (!r) walk_fail = 1; walk_plot_run();
        } else if (!strcmp(c, "roomhas") && nt > 2) {  // 房間（別名）裡有沒有路徑含某字串的物件
            int r = story_room(tok[1]), n = 0; for (int h = story_next(r, 0); h; h = story_next(r, h)) if (strstr(story_path(h), tok[2])) n++;
            printf(n ? "  ok 房間 %s 有 %s ×%d\n" : "  FAIL 房間 %s 沒有 %s\n", tok[1], tok[2], n); if (!n) { walk_fail = 1; for (int h = story_next(r, 0); h; h = story_next(r, h)) printf("    房間裡有 %s\n", story_path(h)); }
        } else if (!strcmp(c, "money") && nt > 1) { engine_call(0x1003d3e0, (intptr_t[8]){pl, atoi(tok[1])});
        } else if (!strcmp(c, "kill") && nt > 1) {  // 打倒房間裡別名是 tok[1] 的所有人物（引擎 story_killed 的對應：DelObject 問第 12、10 格，再第 2 格）
            int room = walk_here(), n = 0;
            for (int h = story_next(room, 0), nx; h; h = nx) { nx = story_next(room, h); if (story_kind(h) == 1 && !strcmp(story_alias(h), tok[1])) { n += story_die(h); walk_plot_run(); } }
            printf(n ? "  kill %s ×%d\n" : "  FAIL 房間 %s 沒有可打倒的 %s\n", n ? tok[1] : story_room_alias(room), n ? n : 0); if (!n) { walk_fail = 1; }
        } else if (!strcmp(c, "use") && nt > 1) {  // 使用道具（引擎 item_eat 先問物件第 11 格）：道具放到主角身上再用
            int h = story_new(tok[1], pl); printf("  story_use = %d\n", story_use(h)); walk_plot_run();
        } else if (!strcmp(c, "pickup") && nt > 1) {  // 撿起劇情道具（引擎 ground_pick 的對應）：物件放到主角身上，補第 15 格
            int h = story_new(tok[1], pl); story_item_moved(h, pl, 0); walk_plot_run();
        } else if (!strcmp(c, "trig") && nt > 2) { if (ex_ntrig < 3000) { snprintf(ex_trig_room[ex_ntrig], 40, "%s", tok[1]); snprintf(ex_trig_path[ex_ntrig++], 100, "%s", tok[2]); }
        } else if (!strcmp(c, "explore")) { explore_run(nt > 1 ? atoi(tok[1]) : 12);
        } else if (!strcmp(c, "plot") && nt > 1) { story_pct_start(tok[1]); if (!story_plot.h) printf("  劇情事件沒有開始（第 17 格不同意）\n"); walk_plot_run(); }
        else if (!strcmp(c, "talk") && nt > 1) {
            int room = walk_here(), npc = story_find(room, tok[1]);
            if (!npc) { printf("  房間 %s 裡找不到 NPC %s 【NPC 不在】\n", story_room_alias(room), tok[1]); walk_fail = 1; continue; }
            story_talk(npc);
            walk_show();
            for (int i = 2; i < nt; i++) {
                int k = -1;
                if (!strcmp(tok[i], "auto")) {  // 一直選最後一個選項，直到沒有選項（線性對話）
                    for (int n = 0; n < 60 && story_dialog.option_count; n++) { int last = story_dialog.option_count - 1; printf("  > 選 %s\n", story_dialog.options[last].text); story_choose(last); walk_show(); walk_plot_run(); }
                    break;
                }
                if (tok[i][0] >= '0' && tok[i][0] <= '9') k = atoi(tok[i]);
                else for (int j = 0; j < story_dialog.option_count; j++) if (!strcmp(story_dialog.options[j].key, tok[i])) k = j;
                if (k < 0 || k >= story_dialog.option_count) { printf("  > 選項 %s 不存在【選項沒有】\n", tok[i]); walk_fail = 1; break; }
                printf("  > 選 %s\n", story_dialog.options[k].text);
                story_choose(k);
                walk_show();
                walk_plot_run();
            }
        } else if (!strcmp(c, "flag") && nt > 1) { char *v = strchr(tok[1], '='); if (v) { *v++ = 0; engine_call(0x1005e850, (intptr_t[8]){pl, (intptr_t)tok[1], atoi(v)}); engine_call(0x1005f270, (intptr_t[8]){pl, (intptr_t)tok[1], atoi(v)}); } }
        else if (!strcmp(c, "expect") && nt > 1) { char *v = strchr(tok[1], '='); if (v) { *v++ = 0; int g = walk_flag(tok[1]); if (g != atoi(v)) { printf("  FAIL 旗標 %s = %d（要 %s）\n", tok[1], g, v); walk_fail = 1; } else printf("  ok 旗標 %s = %s\n", tok[1], v); } }
        else if (!strcmp(c, "has") && nt > 1) { int found = 0; for (int h = story_next(pl, 0); h; h = story_next(pl, h)) if (!strcmp(story_name(h), tok[1]) || strstr(story_path(h), tok[1])) found++; /* 道具名字由引擎的道具表設，無畫面時只有路徑 */ printf(found ? "  ok 有 %s ×%d\n" : "  FAIL 沒有 %s\n", tok[1], found); if (!found) walk_fail = 1; }
        else { printf("  不認得的指令\n"); walk_fail = 1; }
        walk_flag_diff();
        { static int last; int r = walk_here(); if (r != last) { printf("  （現在在 %s）\n", story_room_alias(r)); last = r; } }
    }
    walk_inv();
    printf(walk_fail ? "walk FAILED\n" : "walk ok\n");
    return walk_fail;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    if (!strncmp(argv[1], "walk=", 5)) return walk_test(argv[1] + 5);
    if (!strncmp(argv[1], "aigroup=", 8)) return aigroup_test(argv[1] + 8);
    if (!strncmp(argv[1], "room=", 5)) return room_test(argv[1] + 5);
    if (!strncmp(argv[1], "hero=", 5)) return hero_test(argv[1] + 5);
    if (!strncmp(argv[1], "team=", 5)) { story_register_all(); return team_test(argv[1] + 5, argc > 2 ? atoi(argv[2]) : 3000); }
    story_register_all();
    int pl = story_player();
    int a = 2;
    for (; a < argc && strchr(argv[a], '='); a++) {
        char *eq = strchr(argv[a], '=');
        *eq = 0;
        if (!strcmp(argv[a], "give")) story_new(eq + 1, pl);
        char *v = strchr(eq + 1, '=');
        if (!strcmp(argv[a], "flag") && v) { *v = 0; engine_call(0x1005e850, (intptr_t[8]){pl, (intptr_t)(eq + 1), atoi(v + 1)}); }
    }
    int npc = story_new(argv[1], 0);
    story_talk(npc);
    show();
    for (int i = a; i < argc; i++) {
        printf("> 選 %s\n", argv[i]);
        story_choose(atoi(argv[i]));
        show();
    }
    printf("主角身上：");
    for (int h = story_next(pl, 0); h; h = story_next(pl, h)) printf(" %s", story_name(h));
    printf("\n");
    return 0;
}
