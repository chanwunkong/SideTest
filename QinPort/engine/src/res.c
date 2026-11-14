// 讀取秦殤原版資源：Lib 封包、xbm 圖片、動畫、場景(.Scn/.MAP/.BNT/.MDL)、主角(PLI)。
// 格式說明見 tools/*.py。
#include "res.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <unistd.h>

// 網頁版：檔案都跟伺服器要（tools/webserve.py）。ASYNCIFY 讓這些 await 在 C 裡看起來是同步的
EM_ASYNC_JS(uint8_t *, js_get, (const char *path, int *size), {
    const r = await fetch(UTF8ToString(path));
    if (!r.ok) { HEAP32[size >> 2] = 0; return 0; }
    const b = new Uint8Array(await r.arrayBuffer());
    const p = _malloc(b.length + 1);
    HEAPU8.set(b, p);
    HEAPU8[p + b.length] = 0;
    HEAP32[size >> 2] = b.length;
    return p;
});

EM_ASYNC_JS(uint8_t *, js_batch, (const char *json, int *size), {
    const r = await fetch('batch', { method: 'POST', body: UTF8ToString(json) });
    if (!r.ok) { HEAP32[size >> 2] = 0; return 0; }
    const b = new Uint8Array(await r.arrayBuffer());
    const p = _malloc(b.length + 1);
    HEAPU8.set(b, p);
    HEAP32[size >> 2] = b.length;
    return p;
});
#endif

static uint16_t u16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

// 遊戲檔名大小寫不一致（例如 bdj_cave1.MAP 實際是 BDJ_CAVE1.MAP）；Linux 上要逐一比對
static FILE *open_ci(const char *dir, const char *name) {
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *e;
    FILE *f = NULL;
    while ((e = readdir(d)))
        if (!strcasecmp(e->d_name, name)) {
            char path[1024];
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            f = fopen(path, "rb");
            break;
        }
    closedir(d);
    if (!f) fprintf(stderr, "找不到 %s/%s\n", dir, name);
    return f;
}

#ifdef __EMSCRIPTEN__
// 網頁版的小檔案快取（PLI、場景檔…）：同一個檔案不重複下載；1MB 以上的不留
#define FILE_CACHE 512
static struct { char key[200]; uint8_t *data; size_t size; } file_cache[FILE_CACHE];
static int file_cache_n;

static int file_cached(const char *key) {
    for (int i = 0; i < file_cache_n; i++)
        if (!strcasecmp(file_cache[i].key, key)) return i;
    return -1;
}

static void file_cache_put(const char *key, const uint8_t *data, size_t size) {
    if (size > (1u << 20) || file_cached(key) >= 0) return;
    int i = file_cache_n < FILE_CACHE ? file_cache_n++ : FILE_CACHE - 1;  // 滿了就蓋最後一格
    free(file_cache[i].data);
    snprintf(file_cache[i].key, sizeof file_cache[i].key, "%s", key);
    file_cache[i].data = malloc(size + 1);
    memcpy(file_cache[i].data, data, size);
    file_cache[i].data[size] = 0;
    file_cache[i].size = size;
}
#endif

static uint8_t *read_all(const char *dir, const char *name, size_t *size) {
#ifdef __EMSCRIPTEN__
    char url[1400];
    snprintf(url, sizeof url, "%s/%s", dir, name);
    int c = file_cached(url);
    if (c >= 0) {
        uint8_t *d = malloc(file_cache[c].size + 1);
        memcpy(d, file_cache[c].data, file_cache[c].size + 1);
        *size = file_cache[c].size;
        return d;
    }
    int n = 0;
    uint8_t *d = js_get(url, &n);
    *size = n;
    if (!d) fprintf(stderr, "找不到 %s\n", url);
    else file_cache_put(url, d, n);
    return d;
#else
    FILE *f = open_ci(dir, name);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(*size + 1);
    if (fread(data, 1, *size, f) != *size) { free(data); data = NULL; }
    else data[*size] = 0;
    fclose(f);
    return data;
#endif
}

#ifdef __EMSCRIPTEN__
// 伺服器上一段檔案內容；回傳 malloc 的資料
typedef struct { const char *path; uint32_t off, len; int32_t entry; } Span;  // entry >= 0：Lib 的第幾項（伺服器查索引）；-2：整個檔案

static uint8_t **fetch_spans(const Span *sp, int n, uint32_t *lens) {
    uint8_t **out = calloc(n ? n : 1, sizeof *out);
    for (int start = 0; start < n; start += 2000) {  // 一批最多 2000 段
        int end = start + 2000 < n ? start + 2000 : n;
        size_t cap = 64, len = 1;
        for (int i = start; i < end; i++) cap += strlen(sp[i].path) + 32;
        char *json = malloc(cap);
        json[0] = '[';
        for (int i = start; i < end; i++)
            len += sp[i].entry >= 0 ? snprintf(json + len, cap - len, "%s[\"%s\",-1,%d]", i > start ? "," : "", sp[i].path, sp[i].entry)
                 : sp[i].entry == -2 ? snprintf(json + len, cap - len, "%s[\"%s\",0,-1]", i > start ? "," : "", sp[i].path)
                                    : snprintf(json + len, cap - len, "%s[\"%s\",%u,%u]", i > start ? "," : "", sp[i].path, sp[i].off, sp[i].len);
        snprintf(json + len, cap - len, "]");
        int size = 0;
        uint8_t *d = js_batch(json, &size);
        free(json);
        for (int i = start, p = 0; d && i < end && p + 4 <= size; i++) {
            uint32_t l = d[p] | d[p + 1] << 8 | d[p + 2] << 16 | (uint32_t)d[p + 3] << 24;
            p += 4;
            if (l && p + (int)l <= size) { out[i] = malloc(l); memcpy(out[i], d + p, l); }
            lens[i] = l;
            p += l;
        }
        free(d);
    }
    return out;
}
#endif

// 網頁版每個項目的狀態
enum { ST_NONE, ST_QUEUED, ST_URGENT, ST_SENT, ST_DONE, ST_FAIL };  // URGENT = 已在優先佇列，不重複排
int res_waiting;  // 最近一次非阻塞讀取是因為還在下載而回 NULL
#ifdef __EMSCRIPTEN__
static int pending;  // 排隊中加上下載中的項目數
#endif

#ifdef __EMSCRIPTEN__
// 背景批次下載：開始後立刻返回，之後每幀用 js_batch_take 收結果（不在 JS 回呼裡呼叫 C，ASYNCIFY 比較安全）
EM_JS(void, js_batch_start, (int id, const char *json), {
    Module.qinBatch = Module.qinBatch || {};
    Module.qinBatch[id] = null;
    fetch('batch', { method: 'POST', body: UTF8ToString(json) })
        .then(r => r.ok ? r.arrayBuffer() : new ArrayBuffer(0))
        .then(b => { Module.qinBatch[id] = new Uint8Array(b); })
        .catch(() => { Module.qinBatch[id] = new Uint8Array(0); });
});

EM_JS(uint8_t *, js_batch_take, (int id, int *size), {
    const b = Module.qinBatch && Module.qinBatch[id];
    if (!b) return 0;
    delete Module.qinBatch[id];
    const p = _malloc(b.length + 1);
    HEAPU8.set(b, p);
    HEAP32[size >> 2] = b.length;
    return p;
});

static const char *lib_rel(const Lib *lib) { return strchr(lib->path, '/') ? strchr(lib->path, '/') + 1 : lib->path; }  // 去掉開頭的 game/

static void lib_store(Lib *lib, uint32_t i, uint8_t *data, uint32_t len) {
    if (lib->state[i] == ST_QUEUED || lib->state[i] == ST_URGENT || lib->state[i] == ST_SENT) pending--;
    free(lib->cache[i]);
    lib->cache[i] = data;
    lib->cache_len[i] = len;
    lib->state[i] = data ? ST_DONE : ST_FAIL;
}
#endif

int lib_open(Lib *lib, const char *dir, const char *name) {
    memset(lib, 0, sizeof *lib);
#ifdef __EMSCRIPTEN__
    snprintf(lib->path, sizeof lib->path, "%s/%s", dir, name);
    Span sp = {lib_rel(lib), 0, 0x100, -1};  // 只抓檔頭；索引表很大（最大 1.4MB），由伺服器查
    uint32_t len = 0;
    uint8_t **head = fetch_spans(&sp, 1, &len);
    if (!head[0] || len < 0x100 || memcmp(head[0], "Lib 1.01", 8)) { free(head[0]); free(head); fprintf(stderr, "找不到 %s\n", lib->path); return 0; }
    lib->count = u32(head[0] + 16);
    free(head[0]);
    free(head);
    lib->cache = calloc(lib->count + 1, sizeof *lib->cache);
    lib->cache_len = calloc(lib->count + 1, sizeof *lib->cache_len);
    lib->state = calloc(lib->count + 1, 1);
    return 1;
#else
    uint8_t head[0x100];
    if (!(lib->f = open_ci(dir, name))) return 0;
    if (fread(head, 1, sizeof head, lib->f) != sizeof head || memcmp(head, "Lib 1.01", 8)) { lib_close(lib); return 0; }
    lib->count = u32(head + 16);
    lib->index = malloc((size_t)lib->count * 8 + 1);
    if (fread(lib->index, 8, lib->count, lib->f) != lib->count) { lib_close(lib); return 0; }
    return 1;
#endif
}

static uint32_t entry_len(const Lib *lib, uint32_t i) { return u32((const uint8_t *)&lib->index[i * 2 + 1]); }

uint8_t *lib_read(Lib *lib, uint32_t i, uint32_t *size) {
#ifdef __EMSCRIPTEN__
    if (!lib->state || i >= lib->count) return NULL;
    if (lib->state[i] != ST_DONE) lib_fetch_now(lib, &i, 1);  // 一定要等的資料：沒有就馬上抓
    if (lib->state[i] != ST_DONE) return NULL;
    uint8_t *copy = malloc(lib->cache_len[i]);
    memcpy(copy, lib->cache[i], lib->cache_len[i]);
    if (size) *size = lib->cache_len[i];
    return copy;
#else
    if (!lib->f || i >= lib->count) return NULL;
    uint32_t off = u32((uint8_t *)&lib->index[i * 2]), len = entry_len(lib, i);
    if (!len) return NULL;
    uint8_t *data = malloc(len);
    fseek(lib->f, off, SEEK_SET);
    if (fread(data, 1, len, lib->f) != len) { free(data); return NULL; }
    if (size) *size = len;
    return data;
#endif
}

static void lib_cache_clear(Lib *lib) {
    for (uint32_t i = 0; lib->cache && i < lib->count; i++) { free(lib->cache[i]); lib->cache[i] = NULL; lib->state[i] = ST_NONE; }
#ifdef __EMSCRIPTEN__
    pending = 0;  // 只在換場景時整批清（res_cache_clear），所以直接歸零
#endif
}

// 阻塞：一次批次抓齊（給動畫定義這類一定要先有的小資料）
void lib_fetch_now(Lib *lib, const uint32_t *idx, int n) {
#ifdef __EMSCRIPTEN__
    if (!lib->state) return;
    Span *sp = malloc((n ? n : 1) * sizeof *sp);
    uint32_t *which = malloc((n ? n : 1) * sizeof *which), *lens = calloc(n ? n : 1, sizeof *lens);
    int m = 0;
    for (int k = 0; k < n; k++) {
        uint32_t i = idx[k];
        if (i >= lib->count || lib->state[i] == ST_DONE || lib->state[i] == ST_FAIL) continue;
        if (lib->state[i] == 0xFF) continue;  // 同一批重複
        lib->state[i] = 0xFF;
        sp[m] = (Span){lib_rel(lib), 0, 0, (int32_t)i};
        which[m++] = i;
    }
    uint8_t **got = fetch_spans(sp, m, lens);
    for (int k = 0; k < m; k++) lib_store(lib, which[k], got[k], lens[k]);
    free(got);
    free(sp);
    free(which);
    free(lens);
#else
    (void)lib; (void)idx; (void)n;
#endif
}

// ---- 網頁版背景下載佇列：urgent = 畫面正要用的，background = 進場景時預排的 ----
#ifdef __EMSCRIPTEN__
typedef struct { Lib *lib; uint32_t i; } Want;
typedef struct { Want *v; int head, n, cap; } Queue;
typedef struct { int id, gen, n; Want *items; } Flight;
static Queue urgent, background;
static Flight flights[1];  // 一次只送一批：兩批同時下載會互搶頻寬，畫面附近的圖反而變慢
static int next_flight = 1, generation = 1;

static void queue_push(Queue *q, Lib *lib, uint32_t i) {
    if (q->head == q->n) q->head = q->n = 0;
    if (q->n == q->cap) q->v = realloc(q->v, (q->cap = q->cap ? q->cap * 2 : 1024) * sizeof *q->v);
    q->v[q->n++] = (Want){lib, i};
}
#endif

void lib_want(Lib *lib, const uint32_t *idx, int n) {
#ifdef __EMSCRIPTEN__
    for (int k = 0; lib->state && k < n; k++) {
        uint32_t i = idx[k];
        if (i < lib->count && lib->state[i] == ST_NONE) {
            lib->state[i] = ST_QUEUED;
            pending++;
            queue_push(&background, lib, i);
        }
    }
#else
    (void)lib; (void)idx; (void)n;
#endif
}

uint8_t *lib_try(Lib *lib, uint32_t i, uint32_t *size) {
#ifdef __EMSCRIPTEN__
    if (!lib->state || i >= lib->count) return NULL;
    uint8_t st = lib->state[i];
    if (st == ST_DONE) return lib_read(lib, i, size);
    if (st == ST_FAIL) return NULL;
    if (st == ST_NONE || st == ST_QUEUED) {  // 排到最前面（背景佇列裡那份送出時會被略過）
        if (st == ST_NONE) pending++;
        lib->state[i] = ST_URGENT;
        queue_push(&urgent, lib, i);
    }
    res_waiting = 1;
    return NULL;
#else
    return lib_read(lib, i, size);
#endif
}

void res_pump(void) {
#ifdef __EMSCRIPTEN__
    for (int f = 0; f < (int)(sizeof flights / sizeof flights[0]); f++) {
        Flight *fl = &flights[f];
        if (fl->id) {  // 收結果
            int size = 0;
            uint8_t *d = js_batch_take(fl->id, &size);
            if (!d) continue;
            for (int k = 0, p = 0; k < fl->n; k++) {
                Want *w = &fl->items[k];
                uint32_t l = 0;
                if (p + 4 <= size) { l = d[p] | d[p + 1] << 8 | d[p + 2] << 16 | (uint32_t)d[p + 3] << 24; p += 4; }
                if (fl->gen != generation) { p += l; continue; }  // 已經換場景，丟掉（Lib 可能已經關掉）
                if (!size) { w->lib->state[w->i] = ST_NONE; pending--; continue; }  // 網路失敗，畫的時候會重排
                uint8_t *data = NULL;
                if (l && p + (int)l <= size) { data = malloc(l); memcpy(data, d + p, l); }
                lib_store(w->lib, w->i, data, l);
                if (l) w->lib->avg = w->lib->avg ? (w->lib->avg * 7 + l) / 8 : l;  // 平均項目大小（下一批分量用）
                p += l;
            }
            free(d);
            free(fl->items);
            memset(fl, 0, sizeof *fl);
        }
        if (fl->id) continue;  // 這批還在路上
        // 送新的一批：先 urgent 再 background；每批約 400KB（依各封包的平均項目大小估），
        // 小批一點畫面附近的圖才能很快插隊回來
        Want *items = malloc(400 * sizeof *items);
        int n = 0;
        uint32_t bytes = 0;
        Queue *qs[2] = {&urgent, &background};
        for (int q = 0; q < 2 && n < 400 && bytes < 400000; q++)
            while (qs[q]->head < qs[q]->n && n < 400 && bytes < 400000) {
                Want w = qs[q]->v[qs[q]->head++];
                if (w.lib->state[w.i] != ST_QUEUED && w.lib->state[w.i] != ST_URGENT) continue;
                w.lib->state[w.i] = ST_SENT;
                items[n++] = w;
                bytes += w.lib->avg ? w.lib->avg : 10000;
            }
        if (!n) { free(items); continue; }
        size_t cap = 64 + (size_t)n * 300, len = 1;
        char *json = malloc(cap);
        json[0] = '[';
        for (int k = 0; k < n; k++)
            len += snprintf(json + len, cap - len, "%s[\"%s\",-1,%u]", k ? "," : "", lib_rel(items[k].lib), items[k].i);
        snprintf(json + len, cap - len, "]");
        *fl = (Flight){next_flight++, generation, n, items};
        js_batch_start(fl->id, json);
        free(json);
    }
#endif
}

int res_loading(void) {
#ifdef __EMSCRIPTEN__
    return pending;
#else
    return 0;
#endif
}

void lib_close(Lib *lib) {
    lib_cache_clear(lib);
    free(lib->cache);
    free(lib->cache_len);
    free(lib->state);
    if (lib->f) fclose(lib->f);
    free(lib->index);
    memset(lib, 0, sizeof *lib);
}

// ---- 全域資源編號 ----
static const struct { const char *name; uint32_t base; } LIBS[] = {
    {"ResUI.Lib", 0}, {"Res00.Lib", 0x100000}, {"Res01.Lib", 0x200000}, {"Res02.Lib", 0x300000},
    {"Res03.Lib", 0x400000}, {"Res04.Lib", 0x500000}, {"Res05.Lib", 0x600000}, {"Res06.Lib", 0x700000},
    {"Res07.Lib", 0x800000}, {"Res08.Lib", 0x900000}, {"ResUI2.Lib", 0xA00000}, {"ResUI5.Lib", 0xA80000},
    {"ResUI3.Lib", 0xB00000}, {"ResUI4.Lib", 0xC00000},
};
#define LIB_N (int)(sizeof LIBS / sizeof LIBS[0])
static Lib libs[LIB_N];
static int lib_tried[LIB_N];
static char game_dir[1024];

void res_init(const char *game) { snprintf(game_dir, sizeof game_dir, "%s", game); }
const char *res_game(void) { return game_dir; }

FILE *res_fopen(const char *sub, const char *name) {
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/%s", game_dir, sub);
    return open_ci(dir, name);
}

uint8_t *res_file(const char *sub, const char *name, size_t *size) {
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/%s", game_dir, sub);
    return read_all(dir, name, size);
}

uint8_t *res_span(const char *name, uint32_t off, uint32_t len) {
#ifdef __EMSCRIPTEN__
    Span sp = {name, off, len, -1};
    uint32_t got = 0;
    uint8_t **d = fetch_spans(&sp, 1, &got), *out = d[0];
    free(d);
    if (out && got != len) { free(out); out = NULL; }
    return out;
#else
    FILE *f = open_ci(game_dir, name);
    uint8_t *out = f ? malloc(len) : NULL;
    if (out && (fseek(f, off, SEEK_SET) || fread(out, 1, len, f) != len)) { free(out); out = NULL; }
    if (f) fclose(f);
    return out;
#endif
}

void res_preload(const char *sub, const char *const *names, int n) {
#ifdef __EMSCRIPTEN__
    Span *sp = calloc(n ? n : 1, sizeof *sp);
    char (*rel)[200] = calloc(n ? n : 1, sizeof *rel);
    uint32_t *lens = calloc(n ? n : 1, sizeof *lens);
    int m = 0;
    for (int i = 0; i < n; i++) {
        char key[200];
        snprintf(key, sizeof key, "%s/%s/%s", game_dir, sub, names[i]);
        if (file_cached(key) >= 0) continue;
        snprintf(rel[m], sizeof rel[m], "%s/%s", sub, names[i]);
        sp[m] = (Span){rel[m], 0, 0, -2};
        m++;
    }
    uint8_t **got = fetch_spans(sp, m, lens);
    for (int i = 0; i < m; i++) {
        if (got[i]) {
            char key[220];
            snprintf(key, sizeof key, "%s/%s", game_dir, rel[i]);
            file_cache_put(key, got[i], lens[i]);
        }
        free(got[i]);
    }
    free(got);
    free(sp);
    free(rel);
    free(lens);
#else
    (void)sub; (void)names; (void)n;
#endif
}

size_t res_unxor(uint8_t *d, size_t n) {
    if (!n || d[0] != 0x80) return n;
    for (size_t i = 1; i < n; i++) d[i - 1] = d[i] ^ 0x78;
    d[n - 1] = 0;
    return n - 1;
}

static Lib *lib_for(uint32_t gid, uint32_t *index) {
    int k = -1;
    gid &= 0xFFFFFF;
    for (int i = 0; i < LIB_N; i++)
        if (gid >= LIBS[i].base) k = i;
    if (k < 0) return NULL;
    if (!lib_tried[k]) {
        char dir[1100];
        snprintf(dir, sizeof dir, "%s/Res", game_dir);
        lib_open(&libs[k], dir, LIBS[k].name);
        lib_tried[k] = 1;
    }
    *index = gid - LIBS[k].base;
    return &libs[k];
}

uint8_t *res_read(uint32_t gid, uint32_t *size) {
    uint32_t i;
    Lib *lib = lib_for(gid, &i);
    return lib ? lib_read(lib, i, size) : NULL;
}

// 依封包分組後呼叫 fn（全域編號 → 各封包序號）
static void by_lib(const uint32_t *gids, int n, void (*fn)(Lib *, const uint32_t *, int)) {
    uint32_t *idx = malloc((n ? n : 1) * sizeof *idx);
    for (int k = 0; k < LIB_N; k++) {
        int m = 0;
        for (int j = 0; j < n; j++) {
            uint32_t i;
            Lib *lib = (gids[j] & 0xFFFFFF) >= LIBS[k].base && (k + 1 == LIB_N || (gids[j] & 0xFFFFFF) < LIBS[k + 1].base) ? lib_for(gids[j], &i) : NULL;
            if (lib) idx[m++] = i;
        }
        if (m) fn(&libs[k], idx, m);
    }
    free(idx);
}

void res_fetch_now(const uint32_t *gids, int n) { by_lib(gids, n, lib_fetch_now); }
void res_want(const uint32_t *gids, int n) { by_lib(gids, n, lib_want); }

void res_cache_clear(void) {
    for (int k = 0; k < LIB_N; k++) lib_cache_clear(&libs[k]);
#ifdef __EMSCRIPTEN__
    generation++;  // 還在路上的結果作廢
    urgent.head = urgent.n = background.head = background.n = 0;
#endif
}

// xbm：0x14 寬、0x18 高；0x40 起每列 u32 位移(相對 0x40)；每列交替 u16(透明 bytes / 不透明 bytes + RGB565)
// 0x24 = 1 的是半透明圖（特效、部分建築）：0x40 起先是 w×h bytes 的 alpha（長度在 0x28），列位移表接在後面
uint32_t *xbm_decode(const uint8_t *x, uint32_t size, int *w, int *h) {
    if (!x || size < 0x40 || memcmp(x, "xbm", 3)) return NULL;
    *w = u32(x + 0x14);
    *h = u32(x + 0x18);
    if (*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096) return NULL;
    uint32_t base = 0x40;
    const uint8_t *alpha = NULL;
    uint8_t *unpacked = NULL;
    uint32_t flag = u32(x + 0x24), alen = u32(x + 0x28);
    if (flag == 1 && alen >= (uint32_t)*w * *h && alen < size) {
        alpha = x + 0x40;
        base += alen;
    } else if (flag == 0x01000001 && 0x40 + alen < size && alen >= (uint32_t)*h * 4) {
        // 0x27 = 1：alpha 也是逐列壓縮（列位移表 + 交替 u16 跳過 / 有值的 bytes 數 + 值），跳過的是 0
        const uint8_t *a = x + 0x40;
        alpha = unpacked = calloc((size_t)*w * *h, 1);
        for (int y = 0; y < *h; y++) {
            uint32_t p = u32(a + y * 4);
            for (int col = 0, on = 0; col < *w && p + 2 <= alen; on ^= 1) {
                int n = u16(a + p);
                p += 2;
                for (int i = 0; on && i < n && col + i < *w && p + i < alen; i++) unpacked[y * *w + col + i] = a[p + i];
                if (on) p += n;
                col += n;
            }
        }
        base += alen;
    }
    if (base + (uint32_t)*h * 4 > size) { free(unpacked); return NULL; }
    uint32_t *px = calloc((size_t)*w * *h, 4);
    for (int y = 0; y < *h; y++) {
        uint32_t p = base + u32(x + base + y * 4);
        int col = 0, opaque = 0;
        while (col < *w && p + 2 <= size) {
            int n = u16(x + p) / 2;
            p += 2;
            if (opaque) {
                for (int i = 0; i < n && col + i < *w && p + i * 2 + 2 <= size; i++) {
                    uint16_t c = u16(x + p + i * 2);
                    uint8_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
                    uint32_t a = alpha ? alpha[y * *w + col + i] : 0xFF;
                    px[y * *w + col + i] = a << 24 | b << 16 | g << 8 | r;  // RGBA32 位元組順序
                }
                p += n * 2;
            }
            col += n;
            opaque = !opaque;
        }
    }
    free(unpacked);
    return px;
}

uint32_t *res_image(uint32_t ref, int *w, int *h) {
    uint32_t size, i;
    Lib *lib = lib_for(ref, &i);
    uint8_t *d = lib ? lib_try(lib, i, &size) : NULL;
    uint32_t *px = NULL;
    if (d && size >= 0x18 && !memcmp(d, "xbmgroup", 8)) {
        uint32_t n = u32(d + 0x10), f = (ref >> 24) ? (ref >> 24) - 1 : 0;
        if (f < n && 0x18 + (f + 1) * 4 <= size) {
            uint32_t off = u32(d + 0x18 + f * 4);
            if (off < size) px = xbm_decode(d + off, size - off, w, h);
        }
    } else if (d) {
        px = xbm_decode(d, size, w, h);
    }
    free(d);
    return px;
}

// ---- 動畫：u32 序列數；每序列 (幀數, ?, ?) + 每幀 7 個 u32 (?, 停留拍數, ref, ?, ?, dx, dy) ----
// 每個動畫各自配置：回傳的指標要一直有效（陣列 realloc 會讓舊指標失效）
static Anim **anims;
static int anim_count, anim_cap;

const Anim *anim_get(uint32_t gid) {
    if (!gid) return NULL;  // 找不到定義（例如某些部件沒有影子）
    for (int i = 0; i < anim_count; i++)
        if (anims[i]->gid == gid) return anims[i]->count ? anims[i] : NULL;
    if (anim_count == anim_cap) anims = realloc(anims, (anim_cap = anim_cap ? anim_cap * 2 : 64) * sizeof(Anim *));
    Anim *a = calloc(1, sizeof(Anim));
    anims[anim_count++] = a;
    a->gid = gid;
    uint32_t size;
    uint8_t *d = res_read(gid, &size);
    if (!d || size < 4) { free(d); return NULL; }
    uint32_t n = u32(d), p = 4;
    if (n > 4096) { free(d); return NULL; }
    a->seqs = calloc(n, sizeof(Seq));
    a->count = n;
    for (uint32_t s = 0; s < n; s++) {
        if (p + 12 > size) { a->count = s; break; }
        uint32_t f = u32(d + p);
        p += 12;
        if (f > 256 || p + f * 28 > size) { a->count = s; break; }
        a->seqs[s].count = f;
        a->seqs[s].frames = malloc(f * sizeof(Frame) + 1);
        for (uint32_t j = 0; j < f; j++, p += 28)
            a->seqs[s].frames[j] = (Frame){u32(d + p + 8), (int)u32(d + p + 4), (int32_t)u32(d + p + 20), (int32_t)u32(d + p + 24)};
    }
    free(d);
    return a->count ? a : NULL;
}

// ---- 場景 ----
// .Scn 是 GBK 文字：第 1 行 Map名称=、第 2 行 IntBld名称=，之後是入口點 x=、y=
static void scn_value(const char *text, int line, char *out, size_t n) {
    const char *p = text;
    while (line-- > 0 && (p = strchr(p, '\n'))) p++;
    out[0] = 0;
    if (!p || !(p = strchr(p, '='))) return;
    size_t i = 0;
    for (p++; *p && *p != '\r' && *p != '\n' && i + 1 < n; p++) out[i++] = *p;
    out[i] = 0;
}

static void parse_models(Scene *s, const uint8_t *d, size_t n) {
    static const uint8_t part_tag[] = {0xB2, 0xBF, 0xBC, 0xFE, 0};  // GBK「部件\0」
    int cap = 0;
    // 模型開頭：03 00 + 名稱(非 00/CD)，名稱後第 32 byte 是部件數 1..40
    for (size_t a = 0; a + 2 + 40 < n; a++) {
        if (d[a] != 3 || d[a + 1] || d[a + 2] == 0 || d[a + 2] == 0xCD || d[a + 3] == 0 || d[a + 3] == 0xCD) continue;
        uint16_t count = u16(d + a + 34);
        if (count < 1 || count > 40) continue;
        if (s->model_count == cap) s->models = realloc(s->models, (cap = cap ? cap * 2 : 256) * sizeof(Model));
        Model *m = &s->models[s->model_count++];
        memset(m, 0, sizeof *m);
        m->id = u16(d + a + 36);
        m->start = a + 2;
    }
    // 部件："部件\0" + 3 bytes + 動畫旗標 u8 + 圖號 u16 + 封包 u16（合起來就是全域編號）+ dx, dy
    for (int i = 0; i < s->model_count; i++) {
        Model *m = &s->models[i];
        size_t end = i + 1 < s->model_count ? s->models[i + 1].start : n;
        for (size_t p = m->start; p + 21 <= end && m->part_count < MAX_PARTS; p++) {
            if (memcmp(d + p, part_tag, 5)) continue;
            m->parts[m->part_count++] = (Part){d[p + 8], u32(d + p + 9), (int32_t)u32(d + p + 13), (int32_t)u32(d + p + 17)};
        }
    }
}

// .ONT（GBK 文字）：每個物件以 iType= 開頭，後面是 key=value 行
static void parse_ont(Scene *s, char *text) {
    int cap = 0;
    NpcSpec *n = NULL;
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        int iv = atoi(v);
        if (!strcmp(k, "iType")) {
            if (iv < 12 || iv > 15) { n = NULL; continue; }
            if (s->npc_count == cap) s->npcs = realloc(s->npcs, (cap = cap ? cap * 2 : 32) * sizeof(NpcSpec));
            n = &s->npcs[s->npc_count++];
            memset(n, 0, sizeof *n);
            n->type = iv;
        } else if (!n) {
            continue;
        } else if (!strcmp(k, "\xc4\xa3\xb0\xe5" "ID")) n->tmpl = iv;                  // 模板ID
        else if (!strcmp(k, "\xb5\xd8\xcd\xbc\xd7\xf8\xb1\xea" "X")) n->x = iv;      // 地图坐标X
        else if (!strcmp(k, "\xb5\xd8\xcd\xbc\xd7\xf8\xb1\xea" "Y")) n->y = iv;      // 地图坐标Y
        else if (!strcmp(k, "\xb7\xbd\xcf\xf2")) n->dir = iv;                           // 方向
        else if (!strcmp(k, "\xca\xc2\xbc\xfe" "ID")) snprintf(n->event, sizeof n->event, "%s", v);  // 事件ID
        else if (!strcmp(k, "x")) n->rx = iv;
        else if (!strcmp(k, "y")) n->ry = iv;
        else if (!strcmp(k, "cx")) n->rw = iv;
        else if (!strcmp(k, "cy")) n->rh = iv;
        else if (!strcmp(k, "PathMapX") && n->path_n < MAX_PATH) n->path[n->path_n][0] = iv;
        else if (!strcmp(k, "PathMapY") && n->path_n < MAX_PATH) n->path[n->path_n++][1] = iv;
    }
}

static int cmp_obj(const void *a, const void *b) {
    const Object *x = a, *y = b;
    int d = (x->x + x->y) - (y->x + y->y);
    return d ? d : x->x - y->x;
}

int scene_load(Scene *s, const char *scn_name) {
    memset(s, 0, sizeof *s);
    char dir[1100], name[256], map_name[128], bnt_name[128], ont_name[128], v[32];
    size_t size;
    snprintf(dir, sizeof dir, "%s/scene", game_dir);
    char *text = (char *)read_all(dir, scn_name, &size);
    if (!text) return 0;
    scn_value(text, 0, map_name, sizeof map_name);
    scn_value(text, 1, bnt_name, sizeof bnt_name);
    scn_value(text, 2, ont_name, sizeof ont_name);
    // 第 4 行「入口点数」，之後每個入口 4 行：前场景名称(入口編號)、x、y、角色初始方向
    scn_value(text, 3, v, sizeof v);
    int n = atoi(v);
    s->entries = calloc(n > 0 ? n : 1, sizeof(Entry));
    for (int i = 0; i < n; i++) {
        int f[4];
        for (int k = 0; k < 4; k++) { scn_value(text, 4 + i * 4 + k, v, sizeof v); f[k] = atoi(v); }
        s->entries[s->entry_count++] = (Entry){f[0], f[1], f[2], f[3]};
    }
    if (s->entry_count) { s->start_x = s->entries[0].x; s->start_y = s->entries[0].y; }
    free(text);

    snprintf(dir, sizeof dir, "%s/scene/Map", game_dir);
    uint8_t *map = read_all(dir, map_name, &size);
    if (!map || size < 0x200 || memcmp(map, "SoloMap", 7)) { free(map); return 0; }
    s->w = u16(map + 0x22);
    s->h = u16(map + 0x24);
    size_t grid = size - (size_t)s->w * s->h * 10;
    s->cells = malloc((size_t)s->w * s->h * sizeof(Cell));
    for (int i = 0; i < s->w * s->h; i++) {
        const uint8_t *c = map + grid + i * 10;
        s->cells[i] = (Cell){u16(c), u16(c + 2), c[4]};
    }
    snprintf(name, sizeof name, "%s", (const char *)map + 0x90);
    free(map);
    snprintf(dir, sizeof dir, "%s/Res", game_dir);
    if (!lib_open(&s->tiles, dir, name)) return 0;

    snprintf(dir, sizeof dir, "%s/scene/Int", game_dir);
    uint8_t *bnt = read_all(dir, bnt_name, &size);
    if (bnt && size >= 0x480) {
        snprintf(name, sizeof name, "%s", (const char *)bnt + 0x22);
        for (size_t p = 0x480; p + 20 <= size;) {
            uint16_t kind = u16(bnt + p);
            // 第 16 byte 起是名稱長度（-1 = 沒有），名稱接在 20 bytes 之後
            int32_t name_len = (int32_t)u32(bnt + p + 16);
            size_t extra = name_len > 0 ? (size_t)name_len : 0;
            if (kind == 4) {  // 觸發區：名稱後面 128 bytes 是事件文字
                s->triggers = realloc(s->triggers, (s->trigger_count + 1) * sizeof(Trigger));
                Trigger *t = &s->triggers[s->trigger_count++];
                t->x = u32(bnt + p + 4);
                t->y = u32(bnt + p + 8);
                size_t at = p + 20 + extra;
                snprintf(t->event, sizeof t->event, "%.*s", at + 128 <= size ? 128 : 0, (const char *)bnt + at);
            }
            if (kind == 3) {
                s->objects = realloc(s->objects, (s->object_count + 1) * sizeof(Object));
                s->objects[s->object_count++] = (Object){u16(bnt + p + 2), u32(bnt + p + 4), u32(bnt + p + 8), NULL};
            }
            p += 20 + extra + (kind == 4 ? 128 : 0);
        }
        qsort(s->objects, s->object_count, sizeof(Object), cmp_obj);
        snprintf(dir, sizeof dir, "%s/Mdl", game_dir);
        uint8_t *mdl = read_all(dir, name, &size);
        if (mdl) parse_models(s, mdl, size);
        free(mdl);
        for (int i = 0; i < s->object_count; i++)
            for (int j = 0; j < s->model_count; j++)
                if (s->models[j].id == s->objects[i].model) { s->objects[i].m = &s->models[j]; break; }
    }
    free(bnt);
    snprintf(dir, sizeof dir, "%s/scene/Int", game_dir);
    char *ont = (char *)read_all(dir, ont_name, &size);
    if (ont) parse_ont(s, ont);
    free(ont);
    return 1;
}

const Entry *scene_entry(const Scene *s, int id) {
    for (int i = 0; i < s->entry_count; i++)
        if (s->entries[i].id == id) return &s->entries[i];
    return s->entry_count ? &s->entries[0] : NULL;
}

int scene_walkable(const Scene *s, int x, int y) {
    return x >= 0 && y >= 0 && x < s->w && y < s->h && s->cells[y * s->w + x].walk == 1;
}

void scene_free(Scene *s) {
    free(s->cells);
    free(s->entries);
    free(s->triggers);
    free(s->npcs);
    free(s->objects);
    free(s->models);
    lib_close(&s->tiles);
}

// ---- 主角紙娃娃 ----
// PLI/0000_001.PLI：#define SPR_ROLE_<角色>_<部件>[S]_<款式>_<PLI檔號>\t<編號>
static uint32_t find_define(const char *defs, const char *key) {
    const char *p = defs;
    size_t n = strlen(key);
    while ((p = strstr(p, key))) {
        if (p[n] == '\t') return strtoul(p + n + 1, NULL, 10);
        p += n;
    }
    return 0;
}

// 算出一個角色所有部件（含影子）的動畫編號，不載入；回傳個數
int role_anim_ids(const char *graphic, int pli_no, const char *styles, uint32_t *out, int max) {
    char dir[1100], name[64], key[96];
    size_t size, dsize;
    snprintf(dir, sizeof dir, "%s/PLI", game_dir);
    char *defs = (char *)read_all(dir, "0000_001.PLI", &dsize);
    snprintf(name, sizeof name, "%.8s_%03d.PLI", graphic, pli_no);
    uint8_t *p = read_all(dir, name, &size);
    int n = 0;
    if (defs && p && size >= 0x28 && !memcmp(p, "PLI", 3)) {
        const char *st = styles;
        for (int i = 0; i < p[0x22] && i < MAX_ROLE_PARTS && n + 2 <= max && 0x32u + i * 10 <= size; i++) {
            const char *part = (const char *)p + 0x28 + i * 10;
            char style[8];
            snprintf(style, sizeof style, "%.4s", part + 5);
            if (st && *st) {
                snprintf(style, sizeof style, "%.3s", st);
                st = strchr(st, '*');
                st = st ? st + 1 : "";
            }
            if (!strcmp(style, "000")) continue;
            snprintf(key, sizeof key, "SPR_ROLE_%s_%.4s_%s_%03d", graphic, part, style, pli_no);
            out[n++] = find_define(defs, key);
            snprintf(key, sizeof key, "SPR_ROLE_%s_%.4sS_%s_%03d", graphic, part, style, pli_no);
            out[n++] = find_define(defs, key);
        }
    }
    free(defs);
    free(p);
    return n;
}

int role_load(Role *r, const char *graphic, int pli_no, const char *styles) {
    memset(r, 0, sizeof *r);
    char dir[1100], name[64], key[96];
    size_t size, dsize;
    snprintf(dir, sizeof dir, "%s/PLI", game_dir);
    char *defs = (char *)read_all(dir, "0000_001.PLI", &dsize);
    snprintf(name, sizeof name, "%.8s_%03d.PLI", graphic, pli_no);
    uint8_t *p = read_all(dir, name, &size);
    if (!defs || !p || size < 0xb8 || memcmp(p, "PLI", 3)) { free(defs); free(p); return 0; }
    // 0x22 部件數；0x28 起每筆 (部件名[5], 款式[5])；角色表的「屬性」依序覆蓋款式
    r->part_count = p[0x22] < MAX_ROLE_PARTS ? p[0x22] : MAX_ROLE_PARTS;
    uint32_t want[MAX_ROLE_PARTS * 2];
    int nwant = 0;
    for (int pass = 0; pass < 2; pass++) {  // 第一輪只收集編號並一次預抓，第二輪才解析
    const char *st = styles;
    for (int i = 0; i < r->part_count; i++) {
        const char *part = (const char *)p + 0x28 + i * 10;
        char style[8];
        snprintf(style, sizeof style, "%.4s", part + 5);
        if (st && *st) {
            snprintf(style, sizeof style, "%.3s", st);
            st = strchr(st, '*');
            st = st ? st + 1 : "";
        }
        if (!strcmp(style, "000")) continue;  // 沒有這個部件（例如沒拿盾）
        snprintf(key, sizeof key, "SPR_ROLE_%s_%.4s_%s_%03d", graphic, part, style, pli_no);
        uint32_t body = find_define(defs, key);
        snprintf(key, sizeof key, "SPR_ROLE_%s_%.4sS_%s_%03d", graphic, part, style, pli_no);
        uint32_t shadow = find_define(defs, key);
        if (pass == 0) { want[nwant++] = body; want[nwant++] = shadow; continue; }
        r->body[i] = anim_get(body);
        r->shadow[i] = anim_get(shadow);
    }
    if (pass == 0) res_fetch_now(want, nwant);
    }
    // 部件清單後隔 0x20：u16 序列數；每序列 u16 幀數 + 每幀 n 個部件索引（疊圖順序）
    int n = p[0x22];
    size_t t = 0x28 + (size_t)n * 10 + 0x20, q = t + 2;
    if (q > size) { free(defs); free(p); return 0; }
    r->seq_count = u16(p + t);
    r->order = calloc(r->seq_count, sizeof *r->order);
    r->frames = calloc(r->seq_count, sizeof *r->frames);
    for (int s = 0; s < r->seq_count && q + 2 <= size; s++) {
        int f = u16(p + q);
        if (q + 2 + (size_t)f * n > size) break;
        r->frames[s] = f;
        r->order[s] = malloc((size_t)f * n + 1);
        memcpy(r->order[s], p + q + 2, (size_t)f * n);
        q += 2 + (size_t)f * n;
    }
    free(defs);
    free(p);
    return 1;
}

int role_load_anim(Role *r, uint32_t gid) {
    memset(r, 0, sizeof *r);
    const Anim *a = anim_get(gid);
    if (!a || !a->count) return 0;
    r->part_count = 1;
    r->body[0] = a;
    r->seq_count = a->count;
    r->order = calloc(a->count, sizeof *r->order);
    r->frames = calloc(a->count, sizeof *r->frames);
    for (int s = 0; s < a->count; s++) {
        r->frames[s] = a->seqs[s].count;
        r->order[s] = calloc((size_t)a->seqs[s].count + 1, 1);
    }
    return 1;
}

void role_free(Role *r) {
    for (int s = 0; s < r->seq_count; s++) free(r->order[s]);
    free(r->order);
    free(r->frames);
}

// ---- 世界：掃描 EasyRPG.dll 的房間初始化程式（見 tools/roomtool.py）----
// 每個房間依序呼叫固定的設定函式，參數是 push 進去的立即值（字串位址或數字）
enum { CALL_ROOM = 0x1005d390, CALL_PATH = 0x10023c60, CALL_NAME = 0x10023cf0, CALL_ALIAS = 0x10023c40, CALL_EXIT = 0x1005d8b0, CALL_MONSTER = 0x10052550 };

typedef struct { uint32_t va, raw, size; } Section;

static const char *pe_str(const uint8_t *d, size_t n, const Section *sec, int nsec, uint32_t va) {
    for (int i = 0; i < nsec; i++)
        if (va >= sec[i].va && va < sec[i].va + sec[i].size) {
            size_t o = sec[i].raw + (va - sec[i].va);
            return o < n && memchr(d + o, 0, n - o) ? (const char *)d + o : NULL;
        }
    return NULL;
}

#ifdef __EMSCRIPTEN__
// 網頁版：房間表 4MB，在背景下載到記憶體檔案系統，不卡住遊戲（同音樂的做法）
EM_JS(void, js_fetch_world, (void), {
    fetch('game/EasyRPG.dll').then(r => r.ok ? r.arrayBuffer() : null).then(b => {
        if (!b) return;
        try { FS.mkdir('/pre'); } catch (e) {}
        FS.writeFile('/pre/EasyRPG.dll.part', new Uint8Array(b));
        FS.rename('/pre/EasyRPG.dll.part', '/pre/EasyRPG.dll');
    });
});
#endif

int world_ready(void) {
#ifdef __EMSCRIPTEN__
    static int started;
    if (!started) { started = 1; js_fetch_world(); }
    return access("/pre/EasyRPG.dll", F_OK) == 0;
#else
    return 1;
#endif
}

int world_load(World *w) {
    memset(w, 0, sizeof *w);
    size_t n;
#ifdef __EMSCRIPTEN__
    uint8_t *d = NULL;
    FILE *f = world_ready() ? fopen("/pre/EasyRPG.dll", "rb") : NULL;
    if (f) {  // 背景下載好的檔案在記憶體檔案系統；還沒好就照舊跟伺服器要
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        d = malloc(n);
        if (fread(d, 1, n, f) != n) { free(d); d = NULL; }
        fclose(f);
    }
    if (!d) d = read_all(game_dir, "EasyRPG.dll", &n);
#else
    uint8_t *d = read_all(game_dir, "EasyRPG.dll", &n);
#endif
    if (!d || n < 0x400) { free(d); return 0; }
    uint32_t pe = u32(d + 0x3c), base = u32(d + pe + 52);
    int nsec = u16(d + pe + 6), opt = u16(d + pe + 20);
    Section sec[32], text = {0};
    if (nsec > 32) nsec = 32;
    for (int i = 0; i < nsec; i++) {
        const uint8_t *h = d + pe + 24 + opt + i * 40;
        sec[i] = (Section){base + u32(h + 12), u32(h + 20), u32(h + 16)};
        if (!memcmp(h, ".text", 5)) text = sec[i];
    }
    // 出口的目的地先存路徑字串，全部房間讀完再換成房間索引
    const char **to_path = NULL;
    uint32_t push[8];
    int npush = 0, cur = -1, cap_r = 0, cap_e = 0, cap_g = 0;
    for (size_t i = text.raw; i + 5 < text.raw + text.size && i + 5 < n;) {
        uint8_t op = d[i];
        if (op == 0x68) { if (npush < 8) push[npush++] = u32(d + i + 1); else { memmove(push, push + 1, 7 * 4); push[7] = u32(d + i + 1); } i += 5; continue; }
        if (op == 0x6a) { uint32_t v = (uint32_t)(int8_t)d[i + 1];  // push imm8 會帶號擴展：0xff = -1
            if (npush < 8) push[npush++] = v; else { memmove(push, push + 1, 7 * 4); push[7] = v; } i += 2; continue; }
        if (op == 0xe8) {
            uint32_t dst = text.va + (uint32_t)(i - text.raw) + 5 + u32(d + i + 1);
            if (dst == CALL_ROOM) {
                if (w->room_count == cap_r) w->rooms = realloc(w->rooms, (cap_r = cap_r ? cap_r * 2 : 512) * sizeof(Room));
                cur = w->room_count++;
                memset(&w->rooms[cur], 0, sizeof(Room));
                w->rooms[cur].first_exit = w->exit_count;
                w->rooms[cur].first_group = w->group_count;
            } else if (cur >= 0 && npush) {
                Room *r = &w->rooms[cur];
                const char *str = pe_str(d, n, sec, nsec, push[npush - 1]);
                if (dst == CALL_PATH && str) snprintf(r->path, sizeof r->path, "%s", str);
                else if (dst == CALL_NAME && str) snprintf(r->name, sizeof r->name, "%s", str);
                else if (dst == CALL_ALIAS && str) snprintf(r->alias, sizeof r->alias, "%s", str);
                else if (dst == CALL_MONSTER && npush >= 6 && str) {  // 由右到左：名稱、數量、區域、等級、級數、AI 群組
                    if (w->group_count == cap_g) w->groups = realloc(w->groups, (cap_g = cap_g ? cap_g * 2 : 1024) * sizeof(MonGroup));
                    MonGroup *g = &w->groups[w->group_count++];
                    snprintf(g->name, sizeof g->name, "%s", str);
                    g->count = (int32_t)push[npush - 2];
                    g->area = (int32_t)push[npush - 3];
                    g->level = (int32_t)push[npush - 4];
                    g->tier = (int32_t)push[npush - 5];
                    g->ai = (int32_t)push[npush - 6];
                    r->group_count++;
                }
                else if (dst == CALL_EXIT && npush >= 3 && str) {  // 參數由右到左：入口編號、目的路徑、方向
                    const char *to = pe_str(d, n, sec, nsec, push[npush - 2]);
                    if (to) {
                        if (w->exit_count == cap_e) {
                            cap_e = cap_e ? cap_e * 2 : 1024;
                            w->exits = realloc(w->exits, cap_e * sizeof(Exit));
                            to_path = realloc(to_path, cap_e * sizeof(char *));
                        }
                        Exit *e = &w->exits[w->exit_count];
                        snprintf(e->dir, sizeof e->dir, "%s", str);
                        e->entry = push[npush - 3];
                        e->to = -1;
                        to_path[w->exit_count++] = to;
                        r->exit_count++;
                    }
                }
            }
            npush = 0;
            i += 5;
            continue;
        }
        if (op == 0xc3 || op == 0xc2) { cur = -1; npush = 0; }
        i++;
    }
    for (int i = 0; i < w->exit_count; i++)
        for (int j = 0; j < w->room_count; j++)
            if (!strcmp(w->rooms[j].path, to_path[i])) { w->exits[i].to = j; break; }
    free(to_path);
    free(d);
    return w->room_count > 0;
}

int world_find(const World *w, const char *alias) {
    for (int i = 0; i < w->room_count; i++)
        if (w->rooms[i].alias[0] && !strcasecmp(w->rooms[i].alias, alias)) return i;
    return -1;
}

void world_free(World *w) {
    free(w->rooms);
    free(w->exits);
    free(w->groups);
    memset(w, 0, sizeof *w);
}

// ---- 角色表 ObjMdl/RoleMdl.txt ----
// 欄位：0 名稱、1 ID、2 圖形ID、3 屬性、5 動作方式、7 平時休息、8 平時走路、9 戰鬥休息、10 戰鬥走路、11 挨打、14 死亡、20 平時速度、
// 21 戰鬥速度、24 索敌范围、25 追击范围、31 死亡聲音、34 挨打聲音、39 攻击延时（多值欄位用 * 分隔，取第一個）
static RoleDef *roledefs;
static int roledef_count;

int roledefs_load(void) {
    char dir[1100];
    size_t n;
    snprintf(dir, sizeof dir, "%s/ObjMdl", game_dir);
    uint8_t *d = read_all(dir, "RoleMdl.txt", &n);
    if (!d || n < 2) { free(d); return 0; }
    n = res_unxor(d, n);
    int cap = 0, line_no = 0;
    for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n"), line_no++) {
        if (!line_no) continue;  // 標題列
        char *col[42] = {0};
        int c = 0;
        for (char *p = line; c < 42; c++) {
            col[c] = p;
            char *t = strchr(p, '\t');
            if (!t) { c++; break; }
            *t = 0;
            p = t + 1;
        }
        if (c < 21 || !col[1][0]) continue;
        if (roledef_count == cap) roledefs = realloc(roledefs, (cap = cap ? cap * 2 : 256) * sizeof(RoleDef));
        RoleDef *r = &roledefs[roledef_count++];
        r->id = atoi(col[1]);
        snprintf(r->graphic, sizeof r->graphic, "%s", col[2]);
        snprintf(r->styles, sizeof r->styles, "%s", col[3]);
        r->pli = atoi(col[5]);
        r->idle = atoi(col[7]);
        r->walk = atoi(col[8]);
        r->speed = atoi(col[20]);
#define COL(i) (c > (i) ? atoi(col[i]) : 0)
        r->fight_idle = COL(9); r->fight_walk = COL(10); r->hit = COL(11); r->death = COL(14); r->fight_speed = COL(21);
        r->seek = COL(24); r->chase = COL(25); r->snd_death = COL(31); r->snd_hit = COL(34); r->atk_delay = COL(39);
#undef COL
    }
    free(d);
    // 小動物表 BeastieMdl.txt（Poq.exe 跟 RoleMdl 一起讀，0x44b2xx）：0 名稱、1 ID、2 图素ID、5 低速、6 高速、12 索敌范围；陷阱的角色在這裡
    d = read_all(dir, "BeastieMdl.txt", &n);
    if (d && n > 1) {
        n = res_unxor(d, n);
        line_no = 0;
        for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n"), line_no++) {
            char *col[18] = {0};
            int c = 0;
            for (char *p = line; c < 18; c++) {
                col[c] = p;
                char *t = strchr(p, '\t');
                if (!t) { c++; break; }
                *t = 0;
                p = t + 1;
            }
            if (!line_no || c < 7 || atoi(col[1]) <= 0 || roledef_find(atoi(col[1]))) continue;
            if (roledef_count == cap) roledefs = realloc(roledefs, (cap = cap ? cap * 2 : 256) * sizeof(RoleDef));
            RoleDef *r = &roledefs[roledef_count++];
            memset(r, 0, sizeof *r);
            r->id = atoi(col[1]);
            r->anim = (uint32_t)strtoul(col[2], NULL, 10);
            r->speed = atoi(col[5]);
            r->fight_speed = atoi(col[6]);
            r->seek = c > 12 ? atoi(col[12]) : 0;
        }
    }
    free(d);
    return roledef_count;
}

const RoleDef *roledef_find(int id) {
    for (int i = 0; i < roledef_count; i++)
        if (roledefs[i].id == id) return &roledefs[i];
    return NULL;
}

const RoleDef *roledef_by_graphic(const char *graphic) {
    for (int i = 0; i < roledef_count; i++)
        if (!strcmp(roledefs[i].graphic, graphic)) return &roledefs[i];
    return NULL;
}

// ---- 怪物表 Data/npc_*.txt ----
// 欄位：0 人物类型、5/6 生命、7/8 攻击力、9/10 攻击力大、16/17 防御、28/29 命中、30/31 闪避、32/33 经验、34 图形1
static MonDef *mondefs;
static int mondef_count;
static const char *const MON_FILES[] = {"npc_bxw.txt", "npc_dwys.txt", "npc_gn.txt", "npc_jr.txt", "npc_qj.txt", "npc_smll.txt",
                                        "npc_szlk.txt", "npc_xj.txt", "npc_xn.txt", "npc_yj.txt", "npc_zhs.txt"};

int mondefs_load(void) {
    if (mondef_count) return mondef_count;
    int nf = (int)(sizeof MON_FILES / sizeof *MON_FILES), cap = 0;
    res_preload("Data", MON_FILES, nf);
    for (int f = 0; f < nf; f++) {
        size_t n;
        uint8_t *d = res_file("Data", MON_FILES[f], &n);
        if (!d || n < 2) { free(d); continue; }
        res_unxor(d, n);
        int line_no = 0, skill_col = -1;
        for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n"), line_no++) {
            if (line_no < 1) continue;  // 分類列
            char *col[96] = {0};
            int c = 0;
            for (char *p = line; c < 96; c++) {
                col[c] = p;
                char *t = strchr(p, '\t');
                if (!t) { c++; break; }
                *t = 0;
                p = t + 1;
            }
            if (line_no == 1) {  // 欄名列：找「技能1」在第幾欄（各檔不同）
                for (int k = 0; k < c; k++)
                    if (!strcmp(col[k], "\xbc\xbc\xc4\xdc" "1")) skill_col = k;
                continue;
            }
            if (c < 35 || !col[0][0] || !atoi(col[34])) continue;
            if (mondef_count == cap) mondefs = realloc(mondefs, (cap = cap ? cap * 2 : 512) * sizeof(MonDef));
            MonDef *m = &mondefs[mondef_count++];
            snprintf(m->name, sizeof m->name, "%s", col[0]);
            snprintf(m->title, sizeof m->title, "%s", col[1]);
            m->role = atoi(col[34]);
            for (int k = 0; k < 8; k++) snprintf(m->skill[k], sizeof m->skill[k], "%s", skill_col >= 0 && skill_col + k < c ? col[skill_col + k] : "");
#define STAT(i) ((Stat){(float)atof(col[i]), (float)atof(col[(i) + 1])})
            m->hp = STAT(5); m->ap = STAT(7); m->ap_max = STAT(9); m->dp = STAT(16);
            m->hit = STAT(28); m->dodge = STAT(30); m->exp = STAT(32);
            // 魔法攻击属性 1~5 → 魔法攻击（第 12、13 欄）、魔法攻击大（14、15）放到 MAP<属性>、MAP<属性>_MAX（EasyRPG 0x1006xxxx 讀表）
            m->mspeed = (int)atof(col[3]);
            m->mfight = (int)atof(col[4]);
            m->melem = atoi(col[11]);
            m->map = STAT(12); m->map_max = STAT(14);
            for (int k = 0; k < 5; k++) m->mdp[k] = STAT(18 + 2 * k);
            // 技能1 後面第 10 欄起：打击恢复率、减少物理／魔法伤害、眩晕~溃散、其他免疫、是否陷阱、生命恢复率（空白或負的當 0）
#define COL(k) (skill_col >= 0 && skill_col + (k) < c && atoi(col[skill_col + (k)]) > 0 ? atoi(col[skill_col + (k)]) : 0)
            m->djhf = COL(10); m->jswl = COL(11); m->jsmf = COL(12);
            for (int k = 0; k < 7; k++) m->immu[k] = COL(13 + k);
            m->istrap = COL(20); m->regen = COL(21);
            // 第 39、40 欄最小／最大道具随机数，41 起五組（类型、概率）；空白 = −1（原版空格子都當 −1）
#define F(i) ((i) < c && *col[i] ? (float)atof(col[i]) : -1.f)
            m->drop_min = F(39); m->drop_max = F(40);
            for (int k = 0; k < 5; k++) { m->drop[k][0] = F(41 + 2 * k); m->drop[k][1] = F(42 + 2 * k); }
#undef F
#undef COL
#undef STAT
        }
        free(d);
    }
    return mondef_count;
}

const MonDef *mondef_find(const char *name) {
    for (int i = 0; i < mondef_count; i++)
        if (!strcmp(mondefs[i].name, name)) return &mondefs[i];
    return NULL;
}

const MonDef *mondef_pick(const char *prefix) {
    size_t len = strlen(prefix);
    int n = 0;
    for (int i = 0; i < mondef_count; i++) n += !strncmp(mondefs[i].name, prefix, len);
    if (!n) return NULL;
    int k = rand() % n;
    for (int i = 0; i < mondef_count; i++)
        if (!strncmp(mondefs[i].name, prefix, len) && !k--) return &mondefs[i];
    return NULL;
}

int recipes_load(const Recipe **out) {
    static Recipe r[64];
    static int n = -1;
    if (n < 0) {
        n = 0;
        size_t len;
        uint8_t *d = res_file("ObjMdl", "MakeMdl.txt", &len);
        if (d) {
            res_unxor(d, len);
            for (char *line = strtok((char *)d, "\r\n"); line && n < 64; line = strtok(NULL, "\r\n")) {
                char *col[11] = {0}, *p = line;
                for (int c = 0; c < 11 && p; c++) { col[c] = p; p = strchr(p, '\t'); if (p) *p++ = 0; }
                if (!col[10] || atoi(col[1]) <= 0) continue;  // 欄名列、右邊的說明
                Recipe *e = &r[n++];
                e->id = atoi(col[1]), e->cat = atoi(col[2]), e->type = atoi(col[3]);
                for (int i = 0; i < 5; i++) e->need[i] = atoi(col[4 + i]);
                e->main = atoi(col[9]), e->text = atoi(col[10]);
            }
            free(d);
        }
    }
    *out = r;
    return n;
}

// ---- 角色狀態表 ObjMdl/RoleEffectMdl.txt（Poq.exe 0x48b2f0 依狀態序號查）----
const StatusDef *status_def(int id) {
    static StatusDef defs[128];
    static int loaded;
    if (!loaded) {
        loaded = 1;
        for (int i = 0; i < 128; i++) defs[i] = (StatusDef){-1, 0, 0, 0, 0, 0, 0};
        size_t n;
        uint8_t *d = res_file("ObjMdl", "RoleEffectMdl.txt", &n);
        if (!d) return NULL;
        res_unxor(d, n);
        int line_no = 0;
        for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n"), line_no++) {
            if (!line_no) continue;  // 欄名列
            char *col[24] = {0};
            int c = 0;
            for (char *p = line; c < 24; c++) {
                col[c] = p;
                char *t = strchr(p, '\t');
                if (!t) { c++; break; }
                *t = 0;
                p = t + 1;
            }
            if (c < 20 || !col[1][0] || atoi(col[1]) < 0 || atoi(col[1]) >= 128) continue;
            // 序号、停止动画循环、速度变化（%，空白 = 不變）、攻击不能、无敌、头上／身上／脚下效果、…、声音（第 19 欄）
            defs[atoi(col[1])] = (StatusDef){col[3][0] ? atoi(col[3]) : -1, atoi(col[2]), atoi(col[4]), atoi(col[6]), atoi(col[7]), atoi(col[8]), atoi(col[19])};
        }
        free(d);
    }
    return id >= 0 && id < 128 ? &defs[id] : NULL;
}

// ---- 招式表 ObjMdl/MethodMdl.ini（XOR 加密的 key=value，每招以 wType= 開頭；觸發以 iFlag= 開頭）----
static Method *methods;
static int method_count;

const Method *method_find(int auto_id) {
    if (!method_count) {
        size_t n;
        uint8_t *d = res_file("ObjMdl", "MethodMdl.ini", &n);
        if (!d) return NULL;
        res_unxor(d, n);
        int cap = 0;
        Method *m = NULL;
        Trig *t = NULL;
        for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n")) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char *k = line;
            int v = atoi(eq + 1);
            if (!strcmp(k, "wType")) {
                if (method_count == cap) methods = realloc(methods, (cap = cap ? cap * 2 : 1024) * sizeof(Method));
                m = &methods[method_count++];
                memset(m, 0, sizeof *m);
                t = NULL;
            } else if (!m) continue;
            else if (!strcmp(k, "wAutoID")) m->id = v;
            else if (!strcmp(k, "wSprIndex")) m->seq = v;
            else if (!strcmp(k, "wManualID")) m->manual = v;
            else if (!strcmp(k, "Reserve3")) m->ai = v;
            else if (!strcmp(k, "\xd7\xee\xb4\xf3\xb9\xa5\xbb\xf7\xbe\xe0\xc0\xeb")) m->range_max = v;  // 最大攻击距离
            else if (!strcmp(k, "\xd7\xee\xd0\xa1\xb9\xa5\xbb\xf7\xbe\xe0\xc0\xeb")) m->range_min = v;  // 最小攻击距离
            else if (!strcmp(k, "iFlag")) {
                t = m->trig_count < MAX_TRIG ? &m->trig[m->trig_count++] : NULL;
                if (t) t->flag = v;
            } else if (t && !strcmp(k, "iWhatFrame")) t->frame = v;
            else if (t && !strcmp(k, "iTriType")) t->type = v;
            else if (t && !strcmp(k, "iTriID")) t->id = v;
            else if (t && !strcmp(k, "iTriID2")) t->id2 = v;
            else if (t && !strcmp(k, "iTriPos")) t->pos = v;
        }
        free(d);
    }
    for (int i = 0; i < method_count; i++)
        if (methods[i].id == auto_id) return &methods[i];
    return NULL;
}

// 招式單位表：同樣的 key=value 格式，每個單位以 wType= 開頭，觸發以 iFlag= 開頭
static Unit *units;
static int unit_count;

const Unit *unit_find(int auto_id) {
    if (!unit_count) {
        size_t n;
        uint8_t *d = res_file("ObjMdl", "MethodUnit_Mdl.ini", &n);
        if (!d) return NULL;
        res_unxor(d, n);
        int cap = 0;
        Unit *u = NULL;
        Trig *t = NULL;
        for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n")) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char *k = line;
            int v = atoi(eq + 1);
            if (!strcmp(k, "wType")) {
                if (unit_count == cap) units = realloc(units, (cap = cap ? cap * 2 : 512) * sizeof *units);
                u = &units[unit_count++];
                memset(u, 0, sizeof *u);
                u->id = -1;
                t = NULL;
            } else if (!u) continue;
            else if (!strcmp(k, "wAutoID")) u->id = v;
            else if (!strcmp(k, "wManualID")) u->manual = v;
            else if (!strcmp(k, "dData0")) u->anim = (uint32_t)v;
            else if (!strcmp(k, "dData2")) u->kind = v;
            else if (!strcmp(k, "bData3")) u->speed = v;
            else if (!strcmp(k, "iFlag")) {
                t = u->trig_count < MAX_TRIG ? &u->trig[u->trig_count++] : NULL;
                if (t) t->flag = v;
            } else if (t && !strcmp(k, "iWhatFrame")) t->frame = v;
            else if (t && !strcmp(k, "iTriType")) t->type = v;
            else if (t && !strcmp(k, "iTriID")) t->id = v;
            else if (t && !strcmp(k, "iTriID2")) t->id2 = v;
            else if (t && !strcmp(k, "iTriPos")) t->pos = v;
        }
        free(d);
    }
    for (int i = 0; i < unit_count; i++)
        if (units[i].id == auto_id) return &units[i];
    return NULL;
}

uint32_t unit_anim(int auto_id) {
    const Unit *u = unit_find(auto_id);
    return u ? u->anim : 0;
}

// ---- 技能表 Data/skill_npc.txt、skill_1..5.txt：0 索引名、8 Attack ID ----
typedef struct { char name[40]; int attack; } SkillDef;
static SkillDef *skills;
static int skill_count;

int skill_attack(const char *index_name) {
    if (!skill_count) {
        static const char *const files[] = {"skill_npc.txt", "skill_1.txt", "skill_2.txt", "skill_3.txt", "skill_4.txt", "skill_5.txt"};
        int cap = 0;
        res_preload("Data", files, 6);
        for (int f = 0; f < 6; f++) {
            size_t n;
            uint8_t *d = res_file("Data", files[f], &n);
            if (!d) continue;
            res_unxor(d, n);
            for (char *line = strtok((char *)d, "\r\n"); line; line = strtok(NULL, "\r\n")) {
                char *col[9] = {0};
                int c = 0;
                for (char *p = line; c < 9; c++) {
                    col[c] = p;
                    char *t = strchr(p, '\t');
                    if (!t) { c++; break; }
                    *t = 0;
                    p = t + 1;
                }
                if (c < 9 || !col[0][0] || !atoi(col[8])) continue;
                if (skill_count == cap) skills = realloc(skills, (cap = cap ? cap * 2 : 512) * sizeof(SkillDef));
                snprintf(skills[skill_count].name, sizeof skills[0].name, "%s", col[0]);
                skills[skill_count++].attack = atoi(col[8]);
            }
            free(d);
        }
    }
    for (int i = 0; i < skill_count; i++)
        if (!strcmp(skills[i].name, index_name)) return skills[i].attack;
    return -1;
}

int skill_rows(int prof, SkillRow *out, int max) {
    char name[32];
    snprintf(name, sizeof name, "skill_%d.txt", prof);
    return skill_rows_file(name, out, max);
}

int skill_rows_file(const char *name, SkillRow *out, int max) {
    size_t n;
    uint8_t *d = res_file("Data", name, &n);
    if (!d) return 0;
    res_unxor(d, n);
    int k = 0, line_no = 0;
    for (char *line = strtok((char *)d, "\r\n"); line && k < max; line = strtok(NULL, "\r\n"), line_no++) {
        if (line_no < 2 || !strncmp(line, "\xcd\xf8\xc2\xe7", 4)) continue;  // 分類列、欄名列；「网络」（GBK）
        char *col[86] = {0};
        int c = 0;
        for (char *p = line; c < 86; c++) {
            col[c] = p;
            char *t = strchr(p, '\t');
            if (!t) { c++; break; }
            *t = 0;
            p = t + 1;
        }
        if (c < 26 || !col[0][0]) continue;
        for (int i = c; i < 86; i++) col[i] = "";
        out[k] = (SkillRow){"", atoi(col[2]), atoi(col[3]), atoi(col[4]), atoi(col[5]), col[6][0] ? atoi(col[6]) : -1, atoi(col[7]), atoi(col[10]), col[11][0] ? atoi(col[11]) : 1, atoi(col[25]),
                            atoi(col[21]) > 0 ? atoi(col[21]) : -1, atoi(col[22]) > 0 ? atoi(col[22]) : -1, col[19][0] ? atoi(col[19]) : -1, 0,
                            atof(col[15]) > 0 ? atof(col[15]) : 0, atof(col[16]) > 0 ? atof(col[16]) : 0, col[23][0] ? atof(col[23]) : -1, atof(col[24]) > 0 ? atof(col[24]) : 0, atof(col[20]) > 0 ? atof(col[20]) : 0, atof(col[17]), atof(col[18]), {{0}}};
        // 武器限制（第 6 欄 W_Type，空白 -1）、Attack方式（11，空白 = 原版基底預設 1 物理，技能基底 0x1005d4a0 的預設）；精气消耗（第 15、16 欄 → MPU）、作用時間（23、24 → Time）、我方／敌方状态（21、22）照 0x100757a0：負的或空白當 0，狀態 < 1 不設
        // 參數組：屬性ID 空白或 < 0 跳過；初始值、增量空白當 -1，是否对敌、是否百分比空白或 < 0 當 0
        for (int g = 26; g < 86; g += 5) {
            if (!col[g][0] || atoi(col[g]) < 0) continue;
            SkillMod *m = &out[k].mod[out[k].mod_n++];
            m->id = atoi(col[g]);
            m->init = col[g + 1][0] ? atof(col[g + 1]) : -1;
            m->incr = col[g + 2][0] ? atof(col[g + 2]) : -1;
            m->enemy = atoi(col[g + 3]) > 0 ? atoi(col[g + 3]) : 0;
            m->pct = atoi(col[g + 4]) > 0 ? atoi(col[g + 4]) : 0;
        }
        snprintf(out[k++].key, sizeof out[0].key, "%s", col[0]);
    }
    free(d);
    return k;
}

// ---- 道具（EasyRPG.dll 0x10070360）----
// 原版 Excel 表（0x10078170 等）：每列切成字串，空格子是同一個空字串（&DAT_104ba410），數字讀成 −1
typedef struct { int rows, cols; char **cell; uint8_t *text; } Tab;

static int tab_load(Tab *t, const char *name, int skip) {
    size_t n;
    uint8_t *d = res_file("Data", name, &n);
    memset(t, 0, sizeof *t);
    if (!d) return 0;
    n = res_unxor(d, n);
    d[n] = 0;
    t->text = d;
    int cap = 0, line_no = 0;
    for (char *line = (char *)d, *next; line && *line; line = next, line_no++) {
        next = strpbrk(line, "\r\n");
        if (next) { *next++ = 0; while (*next == '\r' || *next == '\n') next++; }
        if (line_no < skip) continue;
        int c = 1;
        for (char *p = line; (p = strchr(p, '\t')); p++) c++;
        if (c > t->cols) t->cols = c;
        if (t->rows == cap) t->cell = realloc(t->cell, (cap = cap ? cap * 2 : 64) * 96 * sizeof(char *));
        char **row = &t->cell[t->rows++ * 96];
        for (int k = 0; k < 96; k++) row[k] = "";
        for (int k = 0; k < 96 && line; k++) {
            row[k] = line;
            line = strchr(line, '\t');
            if (line) *line++ = 0;
        }
    }
    return 1;
}
static const char *tab_s(const Tab *t, int r, int c) { return r >= 0 && r < t->rows && c >= 0 && c < 96 ? t->cell[r * 96 + c] : ""; }
static int tab_i(const Tab *t, int r, int c) { const char *v = tab_s(t, r, c); return *v ? atoi(v) : -1; }
static float tab_f(const Tab *t, int r, int c) { const char *v = tab_s(t, r, c); return *v ? (float)atof(v) : -1.f; }
static int rnd(int n) { return n < 2 ? 0 : rand() % n; }  // 0x10052a00

// 道具表的類型（.data 0x103a0ae0，41 個）；表格內 0 级别、1 名称、2 别名、3 道具类型、4 文字介绍ID、5 基准价格、6 重量、7 速度、8 唯一、
// 9~12 等级/力量/敏捷/体质限制、13/14 攻击力小大、15/16 防御力小大、17/18 耐久度小大、19/20 最小/最大特性数、21~30 固定抽取特性与概率、
// 31 道具栏图片ID、32 地面图片ID、33~37 各職業裝備串、38 名称ID、39 五行属性、40/41 五行攻击、42/43 五行防御、44 相生特性、
// 45 镶嵌宝石数量、46/47 五行出現機率（第一次中 = 連相生特性一起）
static const int ITEM_TYPES[41] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52, 60, 61};
static Tab item_tab[41], attr_tab, slevel_i, slevel_t;
static int items_loaded;

int item_tables_load(void) {
    if (items_loaded) return 1;
    char names[44][24];
    const char *list[44];
    for (int i = 0; i < 41; i++) snprintf(names[i], sizeof names[i], "item_%d.txt", ITEM_TYPES[i]);
    snprintf(names[41], 24, "attribT.txt");
    snprintf(names[42], 24, "SLevel_i.txt");
    snprintf(names[43], 24, "SLevel_t.txt");
    for (int i = 0; i < 44; i++) list[i] = names[i];
    res_preload("Data", list, 44);
    for (int i = 0; i < 41; i++) tab_load(&item_tab[i], names[i], 2);  // 分類列、欄名列
    tab_load(&attr_tab, names[41], 1);
    tab_load(&slevel_i, names[42], 0);  // 第 0 列是「ratio% 1 2 …」，第 1~9 列 = 等級
    tab_load(&slevel_t, names[43], 0);
    return items_loaded = 1;
}

// 依機率表（SLevel 的某一欄，第 1~9 列是百分比，×10 成千分之）抽等級（0x1006e3f0、0x10070360 開頭）；都沒中回 none
static int slevel_pick(const Tab *t, int col, int none) {
    float sum = 0;
    int r = rnd(1000);
    for (int lv = 1; lv < 10; lv++) {
        sum += tab_f(t, lv, col) * 10;
        if (r < sum) return lv;
    }
    return none;
}

// 掉寶類型的權重（.data 0x103a0d58，依道具表類型）
static const int TYPE_WEIGHT[62] = {3, 1, 2, 0, 0, 0, 0, 4, 5, 4, 5, 5, 3, 8, 4, 5, 7, 7, 3, 3, 3, 4, 3, 4, 4, 9, 12, 5, 5, 4, 6,
                                    5, 5, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0};
static int pick_weighted(const int *list, int n) {  // 0x1007a930
    int sum = 0;
    for (int i = 0; i < n; i++) sum += TYPE_WEIGHT[list[i]];
    if (!sum) return list[rnd(n)];
    int r = rnd(sum);
    for (int i = 0; i < n; i++) {
        if (r < TYPE_WEIGHT[list[i]]) return list[i];
        r -= TYPE_WEIGHT[list[i]];
    }
    return -1;
}

int item_type_pick(int type) {
    static const int any[46] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52, 60};
    static const int g100[7] = {0, 1, 2, 3, 4, 5, 6}, g101[28] = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                                                  21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34};
    static const int g103[4] = {3, 4, 5, 6}, g201[4] = {7, 8, 9, 10}, g202[4] = {11, 12, 13, 14}, g203[3] = {15, 16, 17},
                     g204[3] = {18, 19, 20}, g205[4] = {21, 22, 23, 24}, g206[5] = {25, 26, 27, 33, 34}, g207[5] = {28, 29, 30, 31, 32};
    if (type >= 0 && type < 100) return type;
    switch (type) {
    case 100: return pick_weighted(g100, 7);
    case 101: return pick_weighted(g101, 28);
    case 103: return pick_weighted(g103, 4);
    case 201: return pick_weighted(g201, 4);
    case 202: return pick_weighted(g202, 4);
    case 203: return pick_weighted(g203, 3);
    case 204: return pick_weighted(g204, 3);
    case 205: return pick_weighted(g205, 4);
    case 206: return pick_weighted(g206, 5);
    case 207: return pick_weighted(g207, 5);
    case 999: return pick_weighted(any, 46);
    }
    if (type >= 1001 && type <= 1009) return 3;  // 獸骨、皮、筋、肉
    if (type >= 1010 && type <= 1018) return 4;
    if (type >= 1019 && type <= 1027) return 5;
    if (type >= 1028 && type <= 1036) return 6;
    if (type >= 1101 && type <= 1119) return 52;
    return -1;
}

static void fx_add(Item *it, int id, int pct, int on, int kind, float v) {
    if (it->fx_n < 32) it->fx[it->fx_n++] = (ItemFx){id, pct, on, kind, v, 0};
}

// 一個特性名（attribT 第 0 欄）加到道具上（0x1006e4c0；相生特性 0x1006e880 同式但效果先不生效、種類 2）。
// T 開頭 = 單一屬性，U 開頭 = 組合（子属性1~5 各用 SLevel_t 再抽一次等級）。lv = 這次的等級，col = SLevel 欄
static void attr_apply(Item *it, const char *name, int lv, int col, int kind, int *req, int *price) {
    int r = -1;
    for (int i = 0; i < attr_tab.rows; i++)
        if (!strcmp(tab_s(&attr_tab, i, 0), name)) { r = i; break; }
    if (r < 0) { fprintf(stderr, "Attributes not found <%s>\n", name); return; }
    int vl = lv < 11 ? lv : 9, L = tab_i(&attr_tab, r, 1);
    if (L <= lv) L = lv;
    if (tab_i(&attr_tab, r, 3) > 0 && L * 4 - 3 > *req) *req = L * 4 - 3;  // 是否影响等级限制
    else L = 1;
    if (tab_i(&attr_tab, r, 51) > 0) *price += tab_i(&attr_tab, r, 51) * L;  // 价格修正率
    if (vl > 10) vl = 9;
    int id = tab_i(&attr_tab, r, 31), pct = tab_i(&attr_tab, r, 32);
    int lo = tab_i(&attr_tab, r, 31 + 2 * vl), hi = tab_i(&attr_tab, r, 32 + 2 * vl), v = rnd(hi - lo + 1) + lo;
    int k = kind;
    if (kind != 2 && tab_i(&attr_tab, r, 65) > 0) { k = 3; it->identify = 0; }  // 是否为金色属性：要鑑定
    if (name[0] == 'T') {
        if (id == 40) it->amount = v;          // 金錢數量
        else if (id == 294) it->arrows = v;    // 箭數
        else if (id >= 0) fx_add(it, id, pct, kind != 2, k, (float)v);
    } else if (name[0] == 'U') {
        int sub = slevel_pick(&slevel_t, col, 10);
        for (int s = 0; s < 5; s++)
            if (*tab_s(&attr_tab, r, 52 + s)) attr_apply(it, tab_s(&attr_tab, r, 52 + s), sub, col, kind, req, price);
    }
}

// 道具部位（0x10070360 的 Part 表，依道具表序號）：1 头盔、2 盔甲、3 單手武器、4 盾、5 护腕、6 靴子、7 戒指、8 项链、10 雙手、11 腰带
static const int ITEM_PART[41] = {-1, -1, -1, -1, -1, -1, -1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 10, 10, 10, 10, 10,
                                  3, 3, 3, 3, 1, 2, 4, 6, 8, 7, 5, 11, 1, 2, -1, -1, -1, -1, -1, -1};

int item_new(Item *it, int type, int level, int col, const char *alias) {
    item_tables_load();
    memset(it, 0, sizeof *it);
    it->pos = -1;
    it->identify = 1;
    if (level < 0) level = slevel_pick(&slevel_i, col, col);  // 隨機等級：SLevel_i 第 col 欄
    else if (col < 0) col = level;
    if (level > 10) level = 9;
    if (col > 10) col = 9;
    int lv_col = col >= 0 ? col : level;  // 抽特性用的 SLevel 欄（原版 local_98c）
    int t = item_type_pick(type), ti = -1;
    for (int i = 0; i < 41; i++)
        if (ITEM_TYPES[i] == t) ti = i;
    if (ti < 0) return 0;
    const Tab *tb = &item_tab[ti];
    int cand[64], n = 0;
    for (int i = 0; alias && *alias && i < tb->rows && n < 64; i++)  // 别名開頭相符（0x100777c0）
        if (*tab_s(tb, i, 2) && strstr(tab_s(tb, i, 2), alias) == tab_s(tb, i, 2)) cand[n++] = i;
    while (!n) {
        if (type >= 1000)
            for (int i = 0; i < tb->rows && n < 64; i++) { if (tab_i(tb, i, 3) == type) cand[n++] = i; }
        else
            for (int L = level; L > 0 && !n; L--)
                for (int i = 0; i < tb->rows && n < 64; i++)
                    if (tab_i(tb, i, 0) == L) cand[n++] = i;
        int keep = 0;  // 唯一（Y）的不隨機掉
        for (int i = 0; i < n; i++)
            if (tab_s(tb, cand[i], 8)[0] != 'Y' && tab_s(tb, cand[i], 8)[0] != 'y') cand[keep++] = cand[i];
        n = keep;
        if (!n) {
            if (level < 2 || type >= 1000) { fprintf(stderr, "Item attributes not found <%d:%d>\n", type, level); return 0; }
            level--;
        }
    }
    int r = cand[rnd(n)];
    it->type = t;
    it->level = tab_i(tb, r, 0);
    char nm[64] = "";
    if (!res_dlg("name_C.dlg", tab_i(tb, r, 38), nm, sizeof nm)) snprintf(nm, sizeof nm, "%s", tab_s(tb, r, 1));
    snprintf(it->name, sizeof it->name, "%s", nm);
    snprintf(it->alias, sizeof it->alias, "%s", tab_s(tb, r, 2));
    int ty = tab_i(tb, r, 3);
    it->type = ty < 0 ? -1 : ty > 1000 ? t : ty == 6 ? 51 : ty;  // 狗肉等（類型 6）當食物 51
    it->info = tab_i(tb, r, 4);
#define NN(c) (tab_i(tb, r, c) > 0 ? tab_i(tb, r, c) : 0)
    it->value = NN(5);
    it->weight = NN(6);
    it->lv_req = NN(9);
    it->str_req = NN(10);
    it->dex_req = NN(11);
    it->con_req = NN(12);
    float lo = tab_f(tb, r, 13), hi = tab_f(tb, r, 14);
    if (lo >= 0 && hi >= 0) {  // 攻擊力：小在下半段、大在上半段
        int span = (int)((hi - lo) * 0.5f + 1);
        fx_add(it, 41, 0, 1, 0, (float)rnd(span) + lo);
        fx_add(it, 42, 0, 1, 0, hi - (float)rnd(span));
    }
    lo = tab_f(tb, r, 15), hi = tab_f(tb, r, 16);
    if (lo >= 0 && hi >= 0) fx_add(it, 53, 0, 1, 0, (float)rnd((int)(hi - lo + 1)) + lo);
    lo = tab_f(tb, r, 17), hi = tab_f(tb, r, 18);
    it->hp = it->hp_max = (int)((float)rnd((int)(hi - lo + 1)) + lo);
    it->icon = tab_i(tb, r, 31) >= 0 ? tab_i(tb, r, 31) : 0xfeb;
    it->ground = tab_i(tb, r, 32) >= 0 ? tab_i(tb, r, 32) : 0x897;
    for (int k = 0; k < 5; k++) snprintf(it->equip[k], sizeof it->equip[k], "%s", tab_s(tb, r, 33 + k));
    it->part = ITEM_PART[ti];
    // 五行：第一次機率中 = 五行 + 相生特性，第二次中 = 只有五行
    int xs = 0, five = rnd(100) < tab_i(tb, r, 46) ? (xs = 1) : rnd(100) < tab_i(tb, r, 47);
    if (five) {
        it->fe = tab_i(tb, r, 39);
        lo = tab_f(tb, r, 40), hi = tab_f(tb, r, 41);
        if (lo >= 0 && hi >= 0 && it->fe >= 1 && it->fe <= 5) {
            int span = (int)((hi - lo) * 0.5f + 1);
            fx_add(it, 41 + 2 * it->fe, 0, 1, 1, (float)rnd(span) + lo);
            fx_add(it, 42 + 2 * it->fe, 0, 1, 1, hi - (float)rnd(span));
        }
        lo = tab_f(tb, r, 42), hi = tab_f(tb, r, 43);
        if (lo >= 0 && hi >= 0 && it->fe >= 1 && it->fe <= 5) fx_add(it, 53 + it->fe, 0, 1, 1, (float)rnd((int)(hi - lo + 1)) + lo);
    }
    // 固定抽取特性：從第 1 種（沒中相生的從第 2 種）起累加機率×100，萬分之一擲一次
    char names[24][40];
    int nn = 0;
    float sum = 0;
    int roll = rnd(10000);
    for (int u = !xs; u < 5; u++) {
        sum += tab_f(tb, r, 22 + 2 * u) * 100;
        if (roll < sum) {
            if (*tab_s(tb, r, 21 + 2 * u)) snprintf(names[nn++], 40, "%s", tab_s(tb, r, 21 + 2 * u));
            break;
        }
    }
    int req = it->lv_req, price = 0;
    for (int i = 0; i < nn; i++) attr_apply(it, names[i], slevel_pick(&slevel_t, lv_col, 10), lv_col, 0, &req, &price);
    // 隨機特性（裝備類）：次數在最小～最大特性数之間，每次三分之一機會依抽取几率挑一個（attribT 該類型欄 > 0、属性等级 ≤ 欄），不重複
    nn = 0;
    if (ti < 35) {
        int cnt = rnd(tab_i(tb, r, 20) - tab_i(tb, r, 19) + 1) + tab_i(tb, r, 19);
        int acol = ti <= 3 ? 57 + ti : ti == 4 ? 61 : ti == 5 ? 62 : ti == 6 ? -1 : ti <= 32 ? ti - 2 : ti == 33 ? 63 : 64;
        int pool[512], w[512], pn = 0, total = 0;
        for (int i = 0; acol >= 0 && i < attr_tab.rows && pn < 512; i++)
            if (tab_i(&attr_tab, i, acol) > 0 && tab_i(&attr_tab, i, 1) <= lv_col) {
                pool[pn] = i;
                total += w[pn++] = tab_i(&attr_tab, i, 2);
            }
        for (int k = 0; k < cnt && nn < 24; k++) {
            if (rnd(3)) continue;
            int x = rnd(total), acc = 0, pick = -1;
            for (int i = 0; i < pn; i++)
                if (x < (acc += w[i])) { pick = pool[i]; break; }
            if (pick < 0) continue;
            int dup = 0;
            for (int i = 0; i < nn; i++) dup |= !strcmp(names[i], tab_s(&attr_tab, pick, 0));
            if (!dup) snprintf(names[nn++], 40, "%s", tab_s(&attr_tab, pick, 0));
        }
    }
    if (nn > 2) it->identify = 0;  // 三個以上的特性要鑑定
    for (int i = 0; i < nn; i++) attr_apply(it, names[i], slevel_pick(&slevel_t, lv_col, 10), lv_col, 0, &req, &price);
    if (xs && *tab_s(tb, r, 44)) attr_apply(it, tab_s(tb, r, 44), lv_col, lv_col, 2, &req, &price);
    it->slot_max = NN(45);
    fx_add(it, 65, 0, 1, 0, (float)(tab_i(tb, r, 7) != -1 ? tab_i(tb, r, 7) : 0));  // 速度 → B_C_FIGHT_SPEED
#undef NN
    if (it->lv_req < req) it->lv_req = req;
    it->value_un = it->value;
    it->value = (price + 100) * it->value / 100;
    return 1;
}

void styles_equip(char *styles, size_t n, const char *graphic, int pli_no, const char *equip) {
    char dir[1100], name[64];
    size_t size;
    snprintf(dir, sizeof dir, "%s/PLI", game_dir);
    snprintf(name, sizeof name, "%.8s_%03d.PLI", graphic, pli_no);
    uint8_t *p = read_all(dir, name, &size);
    if (!p || size < 0x28) { free(p); return; }
    int parts = p[0x22] < MAX_ROLE_PARTS ? p[0x22] : MAX_ROLE_PARTS;
    char st[MAX_ROLE_PARTS][4] = {{0}};
    const char *s = styles;
    for (int i = 0; i < parts; i++) {  // 原本的款式，缺的用 PLI 預設
        snprintf(st[i], 4, "%.3s", s && *s ? s : (const char *)p + 0x28 + i * 10 + 5);
        s = s ? strchr(s, '*') : NULL;
        if (s) s++;
    }
    for (const char *e = equip; e && *e;) {  // "部件_款式[_動作方式]"，多個用 * 分隔
        for (int i = 0; i < parts; i++)
            if (!strncmp(e, (const char *)p + 0x28 + i * 10, 2) && e[2] == '_') snprintf(st[i], 4, "%.3s", e + 3);
        e = strchr(e, '*');
        if (e) e++;
    }
    size_t len = 0;
    for (int i = 0; i < parts && len + 4 < n; i++) len += snprintf(styles + len, n - len, "%s%s", i ? "*" : "", st[i]);
    free(p);
}

// Dlg/*.dlg（Poq.exe CDlgFile::ReadItem 0x4c1b10，格式說明見 tools/dlgtool.py）。最近讀過的一個檔留在記憶體
static uint32_t dlg_u32(const uint8_t *b) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)(b[i] ^ (uint8_t)(0xAD - i)) << (8 * i);
    return v;
}

int res_dlg(const char *name, int id, char *out, size_t cap) {
    static struct { char name[64]; uint8_t *d; size_t n; } cache[4];  // 介面字（UI_C）和名稱（name_C）每畫面都會查，各留一份
    int c = 0;
    while (c < 4 && cache[c].name[0] && strcmp(cache[c].name, name)) c++;
    if (c == 4) { free(cache[3].d); c = 3; cache[3].name[0] = 0; }
    if (!cache[c].name[0]) {
        cache[c].d = res_file("Dlg", name, &cache[c].n);
        snprintf(cache[c].name, sizeof cache[c].name, "%s", name);
    }
    uint8_t *d = cache[c].d;
    size_t n = cache[c].n;
    if (!d || n < 0x5c || !cap) return 0;
    uint32_t table, count;
    memcpy(&table, d + 0x50, 4);
    memcpy(&count, d + 0x58, 4);
    if (id < 0 || (uint32_t)id >= count || table + (uint64_t)id * 8 + 8 > n) return 0;
    int32_t flag;
    uint32_t off, len;
    memcpy(&flag, d + table + id * 8, 4);
    memcpy(&off, d + table + id * 8 + 4, 4);
    if (flag < 0 || !off || (uint64_t)off + 4 > n) return 0;
    memcpy(&len, d + off, 4);
    const uint8_t *b = d + off + 4, *end = b + len;
    if (end > d + n || len < 4) return 0;
    uint32_t fields = dlg_u32(b), p = 4;
    for (uint32_t f = 0; f < fields && b + p + 4 <= end; f++) {
        uint32_t kind = dlg_u32(b + p);
        p += 4;
        if (kind == 0x64 || kind == 0x65) { p += 4; continue; }
        if (kind < 0x66 || kind > 0x69 || b + p + 4 > end) return 0;
        uint32_t m = dlg_u32(b + p);
        p += 4;
        if (b + p + m > end) return 0;
        size_t k = 0;
        for (uint32_t i = 0; i < m && k + 1 < cap; i++) {
            uint8_t c = b[p + i] ^ (uint8_t)(m - i + 0xA9);
            if (!c) break;
            out[k++] = (char)c;
        }
        out[k] = 0;
        p += m;
        if (k) return 1;  // 前面幾個字串欄常是空的
    }
    return 0;
}
