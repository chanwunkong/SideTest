#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_W 64
#define TILE_H 32

// ---- 封包：只把索引放記憶體，圖片用到才從檔案讀（R36S 只有 1GB RAM）----
typedef struct { FILE *f; uint32_t count, *index; } Lib;  // index = (offset, size) * count

int lib_open(Lib *lib, const char *dir, const char *name);
uint8_t *lib_read(Lib *lib, uint32_t i, uint32_t *size);  // 呼叫者 free；失敗回 NULL
void lib_close(Lib *lib);

// ---- 全域資源編號（Res/Combination.ini）：編號落在哪個封包的區間 ----
void res_init(const char *game);
const char *res_game(void);
uint8_t *res_read(uint32_t gid, uint32_t *size);
// 圖片：xbm 直接解；xbmgroup 用 ref 高 8 位當幀號(從 1 起)。回傳 RGBA32，呼叫者 free
uint32_t *res_image(uint32_t ref, int *w, int *h);
uint32_t *xbm_decode(const uint8_t *xbm, uint32_t size, int *w, int *h);

// ---- 動畫(Res00)：序列 = 動作*8 + 方向，每幀 = 圖片 ref + 腳底位移 ----
typedef struct { uint32_t ref; int dx, dy; } Frame;
typedef struct { int count; Frame *frames; } Seq;
typedef struct { uint32_t gid; int count; Seq *seqs; } Anim;

const Anim *anim_get(uint32_t gid);  // 快取，不用釋放；失敗回 NULL

// ---- 場景 ----
#define MAX_PARTS 8
typedef struct { uint8_t anim; uint32_t gid; int dx, dy; } Part;
typedef struct { int id, part_count; size_t start; Part parts[MAX_PARTS]; } Model;
typedef struct { uint16_t ground, over; uint8_t walk; } Cell;
typedef struct { int model, x, y; const Model *m; } Object;  // m 在載入時查好，可能為 NULL

typedef struct {
    int w, h, start_x, start_y;
    Cell *cells;
    Object *objects;  // 已依畫面深度排序
    int object_count;
    Model *models;
    int model_count;
    Lib tiles;
} Scene;

int scene_load(Scene *s, const char *scn_name);
void scene_free(Scene *s);
int scene_walkable(const Scene *s, int x, int y);

// 格子 → 世界像素（菱形磁磚左上角）
static inline int world_x(const Scene *s, int x, int y) { return (x - y + s->h - 1) * TILE_W / 2; }
static inline int world_y(const Scene *s, int x, int y) { (void)s; return (x + y) * TILE_H / 2; }

// ---- 主角紙娃娃（PLI）----
#define MAX_ROLE_PARTS 16
typedef struct {
    int part_count, seq_count;
    const Anim *body[MAX_ROLE_PARTS], *shadow[MAX_ROLE_PARTS];
    uint8_t **order;  // order[序列] = 該序列所有幀的疊圖順序，每幀 part_count 個部件索引
    int *frames;      // frames[序列] = 幀數
} Role;

int role_load(Role *r, int role_id, int pli_no);
void role_free(Role *r);
