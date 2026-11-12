#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_W 64
#define TILE_H 32

// ---- 封包：只把索引放記憶體，圖片用到才從檔案讀（R36S 只有 1GB RAM）----
// 網頁版沒有本機檔案：path 是伺服器上的路徑，讀過的項目放在 cache（進新場景時清掉）
typedef struct { FILE *f; char path[256]; uint32_t count, *index; uint8_t **cache; uint32_t *cache_len; } Lib;  // index = (offset, size) * count

int lib_open(Lib *lib, const char *dir, const char *name);
uint8_t *lib_read(Lib *lib, uint32_t i, uint32_t *size);  // 呼叫者 free；失敗回 NULL
void lib_close(Lib *lib);
// 預先抓一批項目（網頁版一次批次請求；原生版直接讀硬碟，不用預抓）
void lib_prefetch(Lib *lib, const uint32_t *idx, int n);
void res_prefetch(const uint32_t *gids, int n);  // 全域編號（ref 的高 8 位會忽略）
void res_cache_clear(void);                      // 換場景時釋放網頁版快取

// ---- 全域資源編號（Res/Combination.ini）：編號落在哪個封包的區間 ----
void res_init(const char *game);
const char *res_game(void);
// 遊戲資料夾下的一般檔案（檔名不分大小寫）。res_file 整個讀進來（呼叫者 free，結尾多補一個 0）
FILE *res_fopen(const char *sub, const char *name);
uint8_t *res_file(const char *sub, const char *name, size_t *size);
// 原版加密文字檔（第一個 byte 0x80，其餘 XOR 0x78）就地解開，回傳文字長度
size_t res_unxor(uint8_t *d, size_t n);
uint8_t *res_read(uint32_t gid, uint32_t *size);
// 圖片：xbm 直接解；xbmgroup 用 ref 高 8 位當幀號(從 1 起)。回傳 RGBA32，呼叫者 free
uint32_t *res_image(uint32_t ref, int *w, int *h);
uint32_t *xbm_decode(const uint8_t *xbm, uint32_t size, int *w, int *h);

// ---- 動畫(Res00)：序列 = 動作*8 + 方向，每幀 = 圖片 ref + 停留拍數 + 腳底位移 ----
typedef struct { uint32_t ref; int delay, dx, dy; } Frame;  // delay = 這幀停留幾拍（1 拍 16ms）
typedef struct { int count; Frame *frames; } Seq;
typedef struct { uint32_t gid; int count; Seq *seqs; } Anim;

const Anim *anim_get(uint32_t gid);  // 快取，不用釋放；失敗回 NULL

// ---- 場景 ----
#define MAX_PARTS 8
typedef struct { uint8_t anim; uint32_t gid; int dx, dy; } Part;
typedef struct { int id, part_count; size_t start; Part parts[MAX_PARTS]; } Model;
typedef struct { uint16_t ground, over; uint8_t walk; } Cell;
typedef struct { int model, x, y; const Model *m; } Object;  // m 在載入時查好，可能為 NULL

typedef struct { int id, x, y, dir; } Entry;          // .Scn 入口點：從入口 id 進來時站的位置與方向
typedef struct { int x, y; char event[48]; } Trigger;  // BNT 類型 4：踩到就觸發事件（例如 "go east"）

// .ONT 角色擺放。類型 = 平時邏輯：12 站立、13 範圍內閒逛、14 沿路線巡邏、15 跟隨
#define MAX_PATH 32
typedef struct {
    int type, tmpl, x, y, dir;
    int rx, ry, rw, rh;           // 類型 13 的閒逛範圍（格）
    int path_n, path[MAX_PATH][2];  // 類型 14 的巡邏路線（格）
    char event[32];               // 事件 ID（對話、劇情用）
} NpcSpec;

typedef struct {
    int w, h, start_x, start_y;
    Entry *entries;
    int entry_count;
    Trigger *triggers;
    int trigger_count;
    NpcSpec *npcs;
    int npc_count;
    Cell *cells;
    Object *objects;  // 已依畫面深度排序
    int object_count;
    Model *models;
    int model_count;
    Lib tiles;
} Scene;

int scene_load(Scene *s, const char *scn_name);
const Entry *scene_entry(const Scene *s, int id);  // 找不到就回第一個入口
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

// styles：角色表的「屬性」，例如 "009*009*001*000"，依 PLI 部件順序指定款式（000 = 不畫這個部件）；NULL = 用 PLI 預設
int role_load(Role *r, const char *graphic, int pli_no, const char *styles);

// ObjMdl/RoleMdl.txt 角色表（XOR 0x78 加密的 GBK、tab 分隔）
typedef struct {
    int id, pli, idle, walk, speed;  // pli = 動作方式；idle/walk = 動作起點序列；speed = 平時速度
    char graphic[16], styles[96];
} RoleDef;
int roledefs_load(void);
const RoleDef *roledef_find(int id);
const RoleDef *roledef_by_graphic(const char *graphic);
void role_free(Role *r);

// ---- 世界（原版劇情引擎 EasyRPG.dll 裡的房間定義）----
// 房間 = 一個場景：別名 = 場景檔名，出口 = (事件方向, 目的房間, 目的入口編號)
typedef struct { char dir[32]; int to, entry; } Exit;
typedef struct { char path[96], name[48], alias[48]; int first_exit, exit_count; } Room;
typedef struct { Room *rooms; int room_count; Exit *exits; int exit_count; } World;

int world_load(World *w);
int world_find(const World *w, const char *alias);  // 依別名找房間，找不到回 -1
void world_free(World *w);
