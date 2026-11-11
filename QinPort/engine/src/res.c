// 讀取秦殤原版資源：Lib 封包、xbm 圖片、動畫、場景(.Scn/.MAP/.BNT/.MDL)、主角(PLI)。
// 格式說明見 tools/*.py。
#include "res.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static uint8_t *read_all(const char *dir, const char *name, size_t *size) {
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
}

int lib_open(Lib *lib, const char *dir, const char *name) {
    memset(lib, 0, sizeof *lib);
    uint8_t head[0x100];
    if (!(lib->f = open_ci(dir, name))) return 0;
    if (fread(head, 1, sizeof head, lib->f) != sizeof head || memcmp(head, "Lib 1.01", 8)) { lib_close(lib); return 0; }
    lib->count = u32(head + 16);
    lib->index = malloc((size_t)lib->count * 8 + 1);
    if (fread(lib->index, 8, lib->count, lib->f) != lib->count) { lib_close(lib); return 0; }
    return 1;
}

uint8_t *lib_read(Lib *lib, uint32_t i, uint32_t *size) {
    if (!lib->f || i >= lib->count) return NULL;
    uint32_t off = u32((uint8_t *)&lib->index[i * 2]), len = u32((uint8_t *)&lib->index[i * 2 + 1]);
    if (!len) return NULL;
    uint8_t *data = malloc(len);
    fseek(lib->f, off, SEEK_SET);
    if (fread(data, 1, len, lib->f) != len) { free(data); return NULL; }
    if (size) *size = len;
    return data;
}

void lib_close(Lib *lib) {
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

uint8_t *res_read(uint32_t gid, uint32_t *size) {
    int k = -1;
    for (int i = 0; i < LIB_N; i++)
        if (gid >= LIBS[i].base) k = i;
    if (k < 0) return NULL;
    if (!lib_tried[k]) {
        char dir[1100];
        snprintf(dir, sizeof dir, "%s/Res", game_dir);
        lib_open(&libs[k], dir, LIBS[k].name);
        lib_tried[k] = 1;
    }
    return lib_read(&libs[k], gid - LIBS[k].base, size);
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
    uint32_t size;
    uint8_t *d = res_read(ref & 0xFFFFFF, &size);
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

// ---- 動畫：u32 序列數；每序列 (幀數, ?, ?) + 每幀 7 個 u32 (?, ?, ref, ?, ?, dx, dy) ----
static Anim *anims;
static int anim_count, anim_cap;

const Anim *anim_get(uint32_t gid) {
    if (!gid) return NULL;  // 找不到定義（例如某些部件沒有影子）
    for (int i = 0; i < anim_count; i++)
        if (anims[i].gid == gid) return anims[i].count ? &anims[i] : NULL;
    if (anim_count == anim_cap) anims = realloc(anims, (anim_cap = anim_cap ? anim_cap * 2 : 64) * sizeof(Anim));
    Anim *a = &anims[anim_count++];
    memset(a, 0, sizeof *a);
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
            a->seqs[s].frames[j] = (Frame){u32(d + p + 8), (int32_t)u32(d + p + 20), (int32_t)u32(d + p + 24)};
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

static int cmp_obj(const void *a, const void *b) {
    const Object *x = a, *y = b;
    int d = (x->x + x->y) - (y->x + y->y);
    return d ? d : x->x - y->x;
}

int scene_load(Scene *s, const char *scn_name) {
    memset(s, 0, sizeof *s);
    char dir[1100], name[256], map_name[128], bnt_name[128], v[32];
    size_t size;
    snprintf(dir, sizeof dir, "%s/scene", game_dir);
    char *text = (char *)read_all(dir, scn_name, &size);
    if (!text) return 0;
    scn_value(text, 0, map_name, sizeof map_name);
    scn_value(text, 1, bnt_name, sizeof bnt_name);
    // 第一個入口點：第 5 行起「前场景名称、x、y、方向」
    scn_value(text, 5, v, sizeof v); s->start_x = atoi(v);
    scn_value(text, 6, v, sizeof v); s->start_y = atoi(v);
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
            if (kind == 3) {  // 4 = 觸發區，不畫
                s->objects = realloc(s->objects, (s->object_count + 1) * sizeof(Object));
                s->objects[s->object_count++] = (Object){u16(bnt + p + 2), u32(bnt + p + 4), u32(bnt + p + 8), NULL};
            }
            p += 20 + (kind == 4 ? 128 : 0);
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
    return 1;
}

int scene_walkable(const Scene *s, int x, int y) {
    return x >= 0 && y >= 0 && x < s->w && y < s->h && s->cells[y * s->w + x].walk == 1;
}

void scene_free(Scene *s) {
    free(s->cells);
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

int role_load(Role *r, int role_id, int pli_no) {
    memset(r, 0, sizeof *r);
    char dir[1100], name[64], key[96];
    size_t size, dsize;
    snprintf(dir, sizeof dir, "%s/PLI", game_dir);
    char *defs = (char *)read_all(dir, "0000_001.PLI", &dsize);
    snprintf(name, sizeof name, "%04d_%03d.PLI", role_id, pli_no);
    uint8_t *p = read_all(dir, name, &size);
    if (!defs || !p || size < 0xb8 || memcmp(p, "PLI", 3)) { free(defs); free(p); return 0; }
    // 0x22 部件數；0x28 起每筆 (部件名[5], 款式[5])
    r->part_count = p[0x22] < MAX_ROLE_PARTS ? p[0x22] : MAX_ROLE_PARTS;
    for (int i = 0; i < r->part_count; i++) {
        const char *part = (const char *)p + 0x28 + i * 10, *style = part + 5;
        snprintf(key, sizeof key, "SPR_ROLE_%04d_%.4s_%.4s_%03d", role_id, part, style, pli_no);
        r->body[i] = anim_get(find_define(defs, key));
        snprintf(key, sizeof key, "SPR_ROLE_%04d_%.4sS_%.4s_%03d", role_id, part, style, pli_no);
        r->shadow[i] = anim_get(find_define(defs, key));
    }
    // 0xb6：u16 序列數；每序列 u16 幀數 + 每幀 part_count 個部件索引（疊圖順序）
    int n = p[0x22];
    size_t q = 0xb8;
    r->seq_count = u16(p + 0xb6);
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
