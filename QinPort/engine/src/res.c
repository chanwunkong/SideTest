// 讀取秦殤原版資源：Lib 封包、xbm 圖片、動畫、場景(.Scn/.MAP/.BNT/.MDL)、主角(PLI)。
// 格式說明見 tools/*.py。
#include "res.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

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
uint32_t *xbm_decode(const uint8_t *x, uint32_t size, int *w, int *h) {
    if (!x || size < 0x40 || memcmp(x, "xbm", 3)) return NULL;
    *w = u32(x + 0x14);
    *h = u32(x + 0x18);
    if (*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096 || 0x40 + (uint32_t)*h * 4 > size) return NULL;
    uint32_t *px = calloc((size_t)*w * *h, 4);
    for (int y = 0; y < *h; y++) {
        uint32_t p = 0x40 + u32(x + 0x40 + y * 4);
        int col = 0, opaque = 0;
        while (col < *w && p + 2 <= size) {
            int n = u16(x + p) / 2;
            p += 2;
            if (opaque) {
                for (int i = 0; i < n && col + i < *w && p + i * 2 + 2 <= size; i++) {
                    uint16_t c = u16(x + p + i * 2);
                    uint8_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
                    px[y * *w + col + i] = 0xFF000000u | b << 16 | g << 8 | r;  // RGBA32 位元組順序
                }
                p += n * 2;
            }
            col += n;
            opaque = !opaque;
        }
    }
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

void role_free(Role *r) {
    for (int s = 0; s < r->seq_count; s++) free(r->order[s]);
    free(r->order);
    free(r->frames);
}

// ---- 世界：掃描 EasyRPG.dll 的房間初始化程式（見 tools/roomtool.py）----
// 每個房間依序呼叫固定的設定函式，參數是 push 進去的立即值（字串位址或數字）
enum { CALL_ROOM = 0x1005d390, CALL_PATH = 0x10023c60, CALL_NAME = 0x10023cf0, CALL_ALIAS = 0x10023c40, CALL_EXIT = 0x1005d8b0 };

typedef struct { uint32_t va, raw, size; } Section;

static const char *pe_str(const uint8_t *d, size_t n, const Section *sec, int nsec, uint32_t va) {
    for (int i = 0; i < nsec; i++)
        if (va >= sec[i].va && va < sec[i].va + sec[i].size) {
            size_t o = sec[i].raw + (va - sec[i].va);
            return o < n && memchr(d + o, 0, n - o) ? (const char *)d + o : NULL;
        }
    return NULL;
}

int world_load(World *w) {
    memset(w, 0, sizeof *w);
    size_t n;
    uint8_t *d = read_all(game_dir, "EasyRPG.dll", &n);
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
    int npush = 0, cur = -1, cap_r = 0, cap_e = 0;
    for (size_t i = text.raw; i + 5 < text.raw + text.size && i + 5 < n;) {
        uint8_t op = d[i];
        if (op == 0x68) { if (npush < 8) push[npush++] = u32(d + i + 1); else { memmove(push, push + 1, 7 * 4); push[7] = u32(d + i + 1); } i += 5; continue; }
        if (op == 0x6a) { if (npush < 8) push[npush++] = d[i + 1]; else { memmove(push, push + 1, 7 * 4); push[7] = d[i + 1]; } i += 2; continue; }
        if (op == 0xe8) {
            uint32_t dst = text.va + (uint32_t)(i - text.raw) + 5 + u32(d + i + 1);
            if (dst == CALL_ROOM) {
                if (w->room_count == cap_r) w->rooms = realloc(w->rooms, (cap_r = cap_r ? cap_r * 2 : 512) * sizeof(Room));
                cur = w->room_count++;
                memset(&w->rooms[cur], 0, sizeof(Room));
                w->rooms[cur].first_exit = w->exit_count;
            } else if (cur >= 0 && npush) {
                Room *r = &w->rooms[cur];
                const char *str = pe_str(d, n, sec, nsec, push[npush - 1]);
                if (dst == CALL_PATH && str) snprintf(r->path, sizeof r->path, "%s", str);
                else if (dst == CALL_NAME && str) snprintf(r->name, sizeof r->name, "%s", str);
                else if (dst == CALL_ALIAS && str) snprintf(r->alias, sizeof r->alias, "%s", str);
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
    memset(w, 0, sizeof *w);
}

// ---- 角色表 ObjMdl/RoleMdl.txt ----
// 欄位：0 名稱、1 ID、2 圖形ID、3 屬性、5 動作方式、7 平時休息、8 平時走路、20 平時速度（多值欄位用 * 分隔，取第一個）
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
        char *col[24] = {0};
        int c = 0;
        for (char *p = line; c < 24; c++) {
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
