// 任務日誌資料（無 SDL；main.c 與 story/storytest.c 共用）。
// 原版：劇情 journal(id, 旗標, 文字)（EasyRPG.dll 0x1004dcc0）→ 0x10046630 → 介面訊息 0x5ea，Poq.exe 0x51a8e0 依種類記進
// ChapterData／LogData（種類 0）或 MissionData（1 = 接受任務、2 = 完成任務）；視窗 JM61（CLogDialog 0x5679d0／0x567e60）顯示。
// journal：id < 1 → 種類 0、否則旗標 == 1 → 種類 2、其他 1（id = 任務編號）。
#include <stdio.h>
#include <string.h>
#include <time.h>

#define JN_CHAP 12            // 章數上限（UI_C 0x85a 起第一章～最終章 = 11）
#define JN_LOGSZ 8192
#define JN_MAX 160
#define JN_TXT 640
typedef struct { int id, fin; char un[JN_TXT], ac[JN_TXT]; } JnMission;
static char jn_log[JN_CHAP + 1][JN_LOGSZ];  // 每章一串：「日-時:分:」+ 內文 + 兩個換行（0x51a8e0 種類 0）
static JnMission jn_m[JN_MAX];
static int jn_mn, jn_chap = 1, jn_pop;      // jn_chap = 目前章（ChapterData.Chapter，至少 1）；jn_pop = 有新東西（自動開視窗）

static void jn_clean(const char *t, char *o, size_t cap) {  // [0名字] 的顏色標記拿掉
    size_t k = 0;
    for (const char *p = t; *p && k + 1 < cap; p++) {
        if (*p == '[' && p[1] >= '0' && p[1] <= '9') { p++; continue; }
        if (*p == ']') continue;
        o[k++] = *p;
    }
    o[k] = 0;
}

static void jn_reset(void) { memset(jn_log, 0, sizeof jn_log); jn_mn = 0; jn_chap = 1; jn_pop = 0; }

// 種類 0（記事）：同一章已經有一模一樣的文字就不再加（0x51a8e0 的 Find == −1 才加）。
// shortcut: 時間戳記格式 "%d-%H:%M:" 是從反編譯拼字串推的（日-時:分:），格式字串原件沒完整讀到
static void jn_apply(int kind, int id, int chap, const char *text) {
    char t[JN_TXT];
    jn_clean(text, t, sizeof t);
    if (chap < 1 || chap > JN_CHAP) chap = jn_chap;
    if (kind == 0) {
        if (strstr(jn_log[chap], t)) return;
        time_t now = time(NULL);
        char stamp[32] = "";
        struct tm tm;
        if (localtime_r(&now, &tm)) strftime(stamp, sizeof stamp, "%d-%H:%M:", &tm);
        size_t n = strlen(jn_log[chap]);
        if (n + strlen(stamp) + strlen(t) + 3 >= JN_LOGSZ) return;  // 滿了就不記（一章 8KB 夠用）
        snprintf(jn_log[chap] + n, JN_LOGSZ - n, "%s%s\n\n", stamp, t);
        jn_pop = 1;
        return;
    }
    if (kind != 1 && kind != 2) return;
    JnMission *m = NULL;
    for (int i = 0; i < jn_mn; i++) if (jn_m[i].id == id) m = &jn_m[i];
    if (!m) {
        if (jn_mn == JN_MAX) return;
        m = &jn_m[jn_mn++];
        memset(m, 0, sizeof *m);
        m->id = id;
    } else if (kind == 1) return;  // 已經接過
    if (kind == 1) snprintf(m->un, sizeof m->un, "%s", t);
    else { snprintf(m->ac, sizeof m->ac, "%s", t); m->fin = 1; }
    jn_pop = 1;
}

// 劇情叫 journal 的入口
static void jn_story(int id, int flag, const char *text) {
    if (id < 1) jn_apply(0, -1, 0, text);
    else jn_apply(flag == 1 ? 2 : 1, id, 0, text);
}

// 任務清單文字：fin = 0 接受中、1 已完成；每項一行（內文本身會自動換行）
static void jn_list(int fin, char *out, size_t cap) {
    size_t n = 0;
    out[0] = 0;
    for (int i = 0; i < jn_mn; i++) {
        if (jn_m[i].fin != fin) continue;
        const char *s = fin ? jn_m[i].ac : jn_m[i].un;
        if (n + strlen(s) + 3 >= cap) break;
        n += (size_t)snprintf(out + n, cap - n, "%s\n\n", s);
    }
}
static int jn_any(void) {
    if (jn_mn) return 1;
    for (int i = 1; i <= JN_CHAP; i++) if (jn_log[i][0]) return 1;
    return 0;
}
static long jn_sig(void) {  // 有沒有變（存檔去重用）
    long n = jn_mn * 1000L;
    for (int i = 1; i <= JN_CHAP; i++) n += (long)strlen(jn_log[i]);
    for (int i = 0; i < jn_mn; i++) n += jn_m[i].fin * 100L + (long)strlen(jn_m[i].un) + (long)strlen(jn_m[i].ac);
    return n;
}
