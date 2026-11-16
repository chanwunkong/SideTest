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
uint8_t *res_span(const char *name, uint32_t off, uint32_t len);  // 遊戲資料夾裡 name 從 off 起 len bytes（網頁版只下載這段）；讀不到回 NULL
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
typedef struct { uint32_t ref; int delay, dx, dy, ex, ey; } Frame;  // delay = 這幀停留幾拍（1 拍 16ms）；ex, ey = 執行期幀 +0xe、+0x10（出手點，Poq.exe 0x43bf50 用）
typedef struct { int count; Frame *frames; } Seq;
typedef struct { uint32_t gid; int count; Seq *seqs; } Anim;

const Anim *anim_get(uint32_t gid);  // 快取，不用釋放；失敗回 NULL

// ---- 場景 ----
#define MAX_PARTS 40
typedef struct { uint8_t anim; uint32_t gid; int dx, dy; } Part;
typedef struct { int id, part_count; size_t start; Part parts[MAX_PARTS]; int light, lx, ly; int bottom, top, special, block_x, block_y;
    int sound;
    int display, type, rect_x, rect_y, rect_w, rect_h;
    int grid_x, grid_y, grid_w, grid_h, anchor_x, anchor_y, door_x, door_y;
    uint16_t *footprint;
} Model;  // light = 光源種類 1~4（BldMdl +0xa9）；lx, ly = 光源格相對擺放格（+0x63 − +0x53、+0x67 − +0x57）
typedef struct { uint16_t ground, over; uint8_t walk, kind, layer, view, flags; int building; uint8_t origin; } Cell;  // kind = 地面種類（記錄第 7 byte；3 = 會留腳印的軟地）；layer = 第 6 byte（原版格子 +6，0x435c00）
typedef struct { int model, x, y; const Model *m; unsigned phase; int highlight, door_x, door_y; char name[128];
    int tri, state, active, cooldown, bottom, top, dead;
    int seq[MAX_PARTS], frame[MAX_PARTS], elapsed[MAX_PARTS];
} Object;  // m 在載入時查好，可能為 NULL

typedef struct { int id, x, y, dir; } Entry;          // .Scn 入口點：從入口 id 進來時站的位置與方向
typedef struct { int x, y; char event[48]; } Trigger;  // BNT 類型 4：踩到就觸發事件（例如 "go east"）

// .ONT 角色擺放。類型 = 平時邏輯：12 站立、13 範圍內閒逛、14 沿路線巡邏、15 跟隨
#define MAX_PATH 32
typedef struct {
    int type, tmpl, x, y, dir;
    int logic, rest; // SInitCha +0x18/+0x1c；平时逻辑类型、行走休息
    int rx, ry, rw, rh;           // 類型 13 的閒逛範圍（格）
    int path_n, path[MAX_PATH][2];  // 類型 14 的巡邏路線（格）
    char event[32];               // 事件 ID（對話、劇情用）
} NpcSpec;

typedef struct {
    int w, h, start_x, start_y;
    int view_x, view_y, view_w, view_h; // MAP 原始世界限制矩形
    Entry *entries;
    int entry_count;
    Trigger *triggers;
    int trigger_count;
    NpcSpec *npcs;
    int npc_count;
    Cell *cells;
    Object *objects;  // BNT 順序，CViewPort 每幀建可見名單
    int object_count;
    Model *models;
    int model_count;
    Lib tiles;
} Scene;

int scene_load(Scene *s, const char *scn_name);
const Entry *scene_entry(const Scene *s, int id);  // 找不到就回第一個入口
void scene_free(Scene *s);
void scene_building_cells(Scene *s, int index, int walk);
void scene_building_state(Scene *s, int index, int state);
void scene_buildings_tick(Scene *s);
int building_part_tick(Object *o, int part, const Anim *a);
int building_available(const Object *o);
int scene_walkable(const Scene *s, int x, int y);

// 格子 → 世界像素（菱形磁磚左上角）
static inline int world_x(const Scene *s, int x, int y) { return (x - y + s->w / 2 + s->h / 2 - 1) * TILE_W / 2; }
static inline int world_y(const Scene *s, int x, int y) { return (x + y + s->w / 2 - s->h / 2) * TILE_H / 2; }

// CRHOMB 世界座標（64 = 一格）投影；保留查表的向零除法。
static inline int scene_px(const Scene *s, int x, int y) {
    return world_x(s, x / 64, y / 64) + (x % 64 - y % 64) / 2 + 32;
}
static inline int scene_py(const Scene *s, int x, int y) {
    return world_y(s, x / 64, y / 64) + (x % 64 + y % 64) / 4;
}

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
int role_load_anim(Role *r, uint32_t gid);  // 一個動畫當成一個部件的角色

// ObjMdl/RoleMdl.txt 角色表（XOR 0x78 加密的 GBK、tab 分隔）
typedef struct {
    int id, pli, idle, walk, speed;  // pli = 動作方式；idle/walk = 動作起點序列；speed = 平時速度
    int fight_idle, fight_walk, hit, death, fight_speed;  // 戰鬥休息/走路、挨打、死亡的動作起點（0 = 沒有）、戰鬥速度
    int seek, chase, atk_delay, snd_death, snd_hit;       // 索敌/追击范围（格）、攻击延时（拍）、死亡/挨打聲音
    int size, legs, snd_idle, snd_fight;                  // 角色大小、脚数、平时/战斗声音（0 = 沒有）
    int light, flicker;                                   // 有无光影、光影闪烁（欄 15、16，RoleMdl +0xb8、+0xbc）
    char graphic[16], styles[96];
    uint16_t skill_ids[100], skill_count; // RoleMdl.rmd：依RoleMdl列序的原始招式表
    int move_cast, candidate[4]; // RoleMdl欄37、38：移動施法思想與入戰候選招式
    int beast, air, display, reaction, snd_run; // BeastieMdl：移動屬性、顯示方式、對人反應
    uint32_t anim;  // BeastieMdl.txt 的小動物、陷阱：只有一個動畫（图素ID），graphic 空白
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
    char alias[32];  // 第 2 欄：別名（劇情找人用）
    char skill[8][32]; // 技能1~8，例如「狼_抓」→ Data/skill_npc.txt；技能1 是普通攻擊
    int role;       // 图形1 = RoleMdl 角色 ID
    int roles[5], nroles;  // 图形1~5（> 0 的），出生時隨機挑一個當 G_PicID
    int name_id, vrange;   // 名称ID（name_C.dlg）、视野范围
    Stat hp, ap, ap_max, dp, hit, dodge, exp;
    int melem;          // 魔法攻击属性 1~5（0 = 沒有）
    Stat map, map_max, mdp[5];
    int mspeed, mfight;  // 移动速度、攻击频率 → B_C_MOVE_SPEED、B_C_FIGHT_SPEED（0x10073e30）
    int djhf, jswl, jsmf, immu[7], istrap, regen;  // 打击恢复率、减少物理／魔法伤害、眩晕~其他免疫、是否陷阱、生命恢复率
    float drop_min, drop_max, drop[5][2];  // 最小／最大道具随机数、随机道具类型 1~5 與概率（0x1006fa50）
} MonDef;
int mondefs_load(void);
// 名稱前綴相符的列隨機挑一列（原版 0x10077670 收集相符列再隨機取）；找不到回 NULL
const MonDef *mondef_pick(const char *prefix);
const MonDef *mondef_find(const char *name);
// ---- 道具（EasyRPG.dll 0x10070360 依道具表 Data/item_<類型>.txt 產生一件）----
// 道具身上的效果（原版 0x10037490 加的一筆，戴上時照抄到人身上 0x1002fe20）：屬性代號、是否百分比、作用中、
// 種類（0 一般、1 五行、2 相生、3 金色）、值
typedef struct { int id, pct, on, kind; float v; int gem; } ItemFx;  // gem = 第幾顆鑲嵌的寶石帶來的（1 起，0 = 道具本身）
typedef struct {
    int type, level, part, value, value_un, weight, lv_req, str_req, dex_req, con_req;
    int hp, hp_max, hit;  // 耐久、耐久上限、磨損計數（HitPoint，滿 40 扣 1 點耐久）
    int icon, ground, info, fe, slot_max, identify, amount, arrows, pos;  // G_PicID2 道具栏图、G_PicID 地面图、G_InfoID、五行、
    char name[32], alias[40], equip[5][96];                              // 鑲嵌數、已鑑定、金錢數、箭數、UI_Pos（−1 = 不在身上）
    int fx_n;
    ItemFx fx[32];
    int sh;  // 劇情那邊對應的道具物件（0 = 還沒有；網頁版劇情用）
    int gem_n, gem_icon[3], gem_value[3];  // 鑲嵌的寶石（0x10037690）：圖示、價值（拆掉時扣回）
} Item;
// 產生一件：type = 道具表類型（或掉寶類型，經 item_type_pick），level < 0 時依 col（SLevel_i 的欄）隨機等級，
// alias 非空時找别名開頭相符的列。失敗回 0
int item_new(Item *it, int type, int level, int col, const char *alias);
int item_type_pick(int type);  // 掉寶類型 → 道具表類型（0x1007a2f0）；不認得回 −1
int item_tables_load(void);
// 打造配方 ObjMdl/MakeMdl.txt：所属类别（打造介面的按鈕 0 刀剑…7 杂项）、物品類型、木材 矿材 骨头 皮毛 筋 各要幾個、
// 主材（1~5 = 道具類型 1~5）、文字ID（UI_C.dlg 的名稱）
typedef struct { int id, cat, type, need[5], main, text; } Recipe;
int recipes_load(const Recipe **out);    // 道具表、attribT、SLevel_i/t 一次讀齊（網頁版一次批次）
// 裝備串（例如 "RH_010_001" 或 "CA_009*TK_009"）覆蓋角色表「屬性」裡對應部件的款式；styles 依 PLI 部件順序、用 * 分隔
void styles_equip(char *styles, size_t n, const char *graphic, int pli_no, const char *equip);

// ---- 招式 ObjMdl/MethodMdl.ini：動作序列、攻擊距離、各幀觸發（type 0 = 命中、1 = 音效、2 = 特效；flag 1 = 打中對方時）----
typedef struct { int flag, frame, type, id, id2, pos; } Trig;  // pos：特效位置，0 = 自己、1 = 對方
#define MAX_TRIG 24
// manual = wManualID、ai = Reserve3（怪物選招用，Poq.exe 0x4906c0）
// effect = wEffect（+0x2c：1 路徑上的人跟著繞、7 亂飄）、quality = wQuality（+0x2e：3 連鎖、6/7 扇形放單位、8 自動找目標）、
// r1 = Reserve1（+0x30：連鎖次數／扇形個數）、r2 = Reserve2（+0x32：1 = 目標點不超過攻擊距離）
typedef struct { int id, manual, ai, seq, range_min, range_max, effect, quality, r1, r2, trig_count, interruptible; Trig trig[MAX_TRIG]; } Method;
const Method *method_find(int auto_id);  // 依 wAutoID 找；第一次呼叫時載入
// 招式單位 ObjMdl/MethodUnit_Mdl.ini：wManualID 201 = 飛行物（Poq.exe 0x450700 依 dData2 分類，2 = CARROW），
// dData0 = Res00 動畫、bData3 = 每拍飛幾像素（0x4368c0）；觸發 flag 1 = 打中時、5 = 放出時
// d = dData0~4、b = bData0~5（各類別意義見 unit.c）
typedef struct { int id, manual, kind, speed, trig_count; uint32_t anim; int d[5], b[6]; Trig trig[MAX_TRIG]; } Unit;
const Unit *unit_find(int auto_id);
uint32_t unit_anim(int auto_id);  // dData0 的 Res00 動畫編號；找不到回 0
// 技能表 Data/skill_*.txt 的「索引名」→ Attack ID（= 招式 wAutoID）；找不到回 -1
int skill_attack(const char *index_name);
// 職業技能表 Data/skill_<職業>.txt 的單機列（「网络」開頭的是連線版，跳過），照表的順序：技能視窗第 p 頁是第 4p~4p+3 列
// 欄位：2 名稱ID（Dlg/name_C.dlg）、3 等级需求、4 等级需求增量、5 技能等級數、7 左右键位、10 图标ID、25 初始技能等級
// 技能參數（26 欄起每 5 欄一組：屬性ID、初始值、增量、是否對敵有效、是否百分比；EasyRPG 0x100757a0）
typedef struct { int id, enemy, pct; float init, incr; } SkillMod;
typedef struct { char key[40]; int name_id, req, req_add, max_level, wtype, hand, icon, attack, level, status, estatus, range, mod_n; float mpu, mpu_inc, time, time_inc, range_inc, range_min, range_min_inc; SkillMod mod[12]; } SkillRow;  // key = 索引名（skill_attack 用）
int skill_rows(int prof, SkillRow *out, int max);
int skill_rows_file(const char *name, SkillRow *out, int max);  // 例如 skill_npc.txt（怪物的技能）
// 角色狀態表 ObjMdl/RoleEffectMdl.txt：速度變化（%，-1 = 不變）、停止動畫、不能攻擊、頭上／身上／腳下效果（圖形 ID）、聲音
typedef struct { int speed, stop, noattack, head, body, foot, sound; } StatusDef;
const StatusDef *status_def(int id);
// 怪物數值（EasyRPG.dll 0x100730c0，單機）：(L/除數 + 1) × ((L − 1) × 參數 + 基準)；表上空格 = −1。
// kind 0 經驗（除數 3）、1 生命（10，乘難度）、2 攻擊 AP/MAP（1000，乘難度）、3 防禦 DP/MDP/命中/閃避（50）
// 難度（0x1039b000）：1 容易 ×2/3、2 普通、3 困難 ×1.5，乘在基準和參數上
extern int difficulty;
static inline int mon_stat(Stat s, int level, int kind) {
    static const float div[4] = {3, 10, 1000, 50};
    float L = (float)level, m = (kind == 1 || kind == 2) ? (difficulty == 1 ? 2.f / 3 : difficulty == 3 ? 1.5f : 1.f) : 1.f;
    return (int)((L / div[kind] + 1) * ((L - 1) * s.per * m + s.base * m));
}
