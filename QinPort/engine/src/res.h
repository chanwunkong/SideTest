#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TILE_W 64
#define TILE_H 32

// ---- 封包：只把索引放記憶體，圖片用到才從檔案讀（R36S 只有 1GB RAM）----
// 網頁版沒有本機檔案：path 是伺服器上的路徑，讀過的項目放在 cache，state 記下載狀態（進新場景時清掉）
typedef struct { FILE *f; char path[256]; uint32_t count, *index; uint8_t **cache; uint32_t *cache_len; uint8_t *state; uint32_t avg; } Lib;  // avg = 網頁版估計的平均項目大小（分批用）  // index = (offset, size) * count

int lib_open(Lib *lib, const char *dir, const char *name);
uint8_t *lib_read(Lib *lib, uint32_t i, uint32_t *size);  // 呼叫者 free；失敗回 NULL。網頁版會等下載
void lib_close(Lib *lib);
// 網頁版的下載方式（原生版都是直接讀硬碟）：
//   lib_try   不等：還沒下載好就排到最前面、設 res_waiting = 1、回 NULL（給畫圖用）
//   lib_want  排進背景佇列（進場景時預排）
//   lib_fetch_now 等到一批全部下載完（給一定要先有的小資料）
extern int res_waiting;
uint8_t *lib_try(Lib *lib, uint32_t i, uint32_t *size);
void lib_want(Lib *lib, const uint32_t *idx, int n);
void lib_fetch_now(Lib *lib, const uint32_t *idx, int n);
void res_want(const uint32_t *gids, int n);       // 全域編號（ref 的高 8 位會忽略）
void res_fetch_now(const uint32_t *gids, int n);
void res_pump(void);                              // 每幀呼叫：收下載結果、送下一批
int res_loading(void);                            // 還在排隊或下載中的項目數
void res_cache_clear(void);                      // 換場景時釋放網頁版快取

// ---- 全域資源編號（Res/Combination.ini）：編號落在哪個封包的區間 ----
void res_init(const char *game);
const char *res_game(void);
// 遊戲資料夾下的一般檔案（檔名不分大小寫）。res_file 整個讀進來（呼叫者 free，結尾多補一個 0）
FILE *res_fopen(const char *sub, const char *name);
uint8_t *res_file(const char *sub, const char *name, size_t *size);
// 一次預載多個小檔案（網頁版一個請求；原生版不需要）
void res_preload(const char *sub, const char *const *names, int n);
// 原版加密文字檔（第一個 byte 0x80，其餘 XOR 0x78）就地解開，回傳文字長度
size_t res_unxor(uint8_t *d, size_t n);
// 原版文字表 Dlg/<name>（例如 UI_C.dlg）第 id 筆的第一個非空字串（GBK，含原版的 [1…] 標記）；沒有回 0
int res_dlg(const char *name, int id, char *out, size_t cap);
uint8_t *res_read(uint32_t gid, uint32_t *size);
// 圖片：xbm 直接解；xbmgroup 用 ref 高 8 位當幀號(從 1 起)。回傳 RGBA32，呼叫者 free。
// 網頁版不等下載：還沒到就回 NULL 並設 res_waiting
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
int role_anim_ids(const char *graphic, int pli_no, const char *styles, uint32_t *out, int max);  // 預抓用

// ObjMdl/RoleMdl.txt 角色表（XOR 0x78 加密的 GBK、tab 分隔）
typedef struct {
    int id, pli, idle, walk, speed;  // pli = 動作方式；idle/walk = 動作起點序列；speed = 平時速度
    int fight_idle, fight_walk, hit, death, fight_speed;  // 戰鬥休息/走路、挨打、死亡的動作起點（0 = 沒有）、戰鬥速度
    int seek, chase, atk_delay, snd_death, snd_hit;       // 索敌/追击范围（格）、攻击延时（拍）、死亡/挨打聲音
    char graphic[16], styles[96];
} RoleDef;
int roledefs_load(void);
const RoleDef *roledef_find(int id);
const RoleDef *roledef_by_graphic(const char *graphic);
void role_free(Role *r);

// ---- 世界（原版劇情引擎 EasyRPG.dll 裡的房間定義）----
// 房間 = 一個場景：別名 = 場景檔名，出口 = (事件方向, 目的房間, 目的入口編號)
typedef struct { char dir[32]; int to, entry; } Exit;
// 怪物群（0x10052550）：name 是怪物表的名稱前綴（例如「动物山魈」，對到 A、B… 各列），在場景「前场景名称 = area」的入口點出現。
// level < 0 → 等級 = tier×5 − 隨機 0..4；tier < 0 → tier = level/5 + 1（EasyRPG.dll 0x100730c0）。ai = G_AIGroup
typedef struct { char name[32]; int count, area, level, tier, ai; } MonGroup;
typedef struct { char path[96], name[48], alias[48]; int first_exit, exit_count, first_group, group_count; } Room;
typedef struct { Room *rooms; int room_count; Exit *exits; int exit_count; MonGroup *groups; int group_count; } World;

int world_load(World *w);
int world_ready(void);  // 網頁版：第一次呼叫開始背景下載，下載好才回 1；原生版永遠 1
int world_find(const World *w, const char *alias);  // 依別名找房間，找不到回 -1
void world_free(World *w);

// ---- 怪物表 Data/npc_*.txt（XOR 加密、tab 分隔，第 2 列是欄名）：數值 = 基準值 + 參數 × 等級 ----
typedef struct { float base, per; } Stat;
typedef struct {
    char name[32];  // 人物类型，例如「动物山魈A」
    char title[32];  // 人物名称（畫面上顯示的，GBK），例如「狼」
    char skill[32]; // 技能1（普通攻擊），例如「狼_抓」→ Data/skill_npc.txt
    int role;       // 图形1 = RoleMdl 角色 ID
    Stat hp, ap, ap_max, dp, hit, dodge, exp;
} MonDef;
int mondefs_load(void);
// 名稱前綴相符的列隨機挑一列（原版 0x10077670 收集相符列再隨機取）；找不到回 NULL
const MonDef *mondef_pick(const char *prefix);
// ---- 道具表 Data/item_<類型>.txt（XOR、tab，第 2 列是欄名）：依「级别」欄找列 ----
// 欄位：0 级别、1 名称、9 等级限制、13/14 攻击力小/大、15/16 防御力小/大、31 道具栏图片ID（原版 G_PicID2，空的用 0xfeb）、33~37 各職業裝備串（遊俠、力士、刺客、術士、巫師）
typedef struct { char name[32]; int ap, ap_max, dp_min, dp_max, icon; char equip[5][96]; } Item;
int item_find(int type, int row, Item *out);  // 找不到回 0
// 裝備串（例如 "RH_010_001" 或 "CA_009*TK_009"）覆蓋角色表「屬性」裡對應部件的款式；styles 依 PLI 部件順序、用 * 分隔
void styles_equip(char *styles, size_t n, const char *graphic, int pli_no, const char *equip);

// ---- 招式 ObjMdl/MethodMdl.ini：動作序列、攻擊距離、各幀觸發（type 0 = 命中、1 = 音效、2 = 特效；flag 1 = 打中對方時）----
typedef struct { int flag, frame, type, id, pos; } Trig;  // pos：特效位置，0 = 自己、1 = 對方
#define MAX_TRIG 24
typedef struct { int id, seq, range_min, range_max, trig_count; Trig trig[MAX_TRIG]; } Method;
const Method *method_find(int auto_id);  // 依 wAutoID 找；第一次呼叫時載入
// 招式單位 ObjMdl/MethodUnit_Mdl.ini（特效）：wAutoID → dData0 的 Res00 動畫編號；找不到回 0
uint32_t unit_anim(int auto_id);
// 技能表 Data/skill_*.txt 的「索引名」→ Attack ID（= 招式 wAutoID）；找不到回 -1
int skill_attack(const char *index_name);
static inline int stat_at(Stat s, int level) { return (int)(s.base + s.per * level); }
