// 劇情執行環境：照原版 EasyRPG.dll 的物件模型，給 tools/storytool.py 產生的劇情檔呼叫。
// 物件用小整數代號（1 起）當 this；劇情裡的 (int)this 轉型因此不會出錯。
// 引擎函式依原版位址分派（engine_call）；還沒翻的第一次被呼叫時印出位址，之後照原版補。
// 原版 EasyRPG 用訊息（0x100646b0、0x10060cd0）通知畫面（Poq.exe）；這裡經 story_on_msg 交給引擎，編號照原版。
#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---- 類別登記（每個劇情檔一個）----
typedef struct { const char *path, *alias, *cls; int kind, start, n; const StorySlot *slots; } Class;
static Class *classes;
static int class_count, class_cap;

void story_register(const char *path, const char *alias, int kind, int start, const char *cls, const StorySlot *slots, int n) {
    if (class_count == class_cap) classes = realloc(classes, (class_cap = class_cap ? class_cap * 2 : 4096) * sizeof *classes);
    classes[class_count++] = (Class){path, alias, cls, kind, start, n, slots};
}

static const Class *class_find(const char *path) {
    for (int i = 0; i < class_count; i++)
        if (!strcmp(classes[i].path, path)) return &classes[i];
    return NULL;
}

// ---- 共用的劇情函式（依原位址）----
typedef struct { uint32_t addr; intptr_t (*fn)(intptr_t *a); } Fn;
static Fn *fns;
static int fn_count, fn_cap;

void story_fn_add(uint32_t addr, intptr_t (*fn)(intptr_t *a)) {
    if (fn_count == fn_cap) fns = realloc(fns, (fn_cap = fn_cap ? fn_cap * 2 : 1024) * sizeof *fns);
    fns[fn_count++] = (Fn){addr, fn};
}

static Fn *fn_find(uint32_t addr) {
    for (int i = 0; i < fn_count; i++)
        if (fns[i].addr == addr) return &fns[i];
    return NULL;
}

intptr_t story_call_addr(uint32_t addr, intptr_t *a) {
    Fn *f = fn_find(addr);
    if (f) return f->fn(a);
    fprintf(stderr, "劇情：沒有共用函式 0x%08x\n", addr);
    return 0;
}

// ---- 物件：具名整數屬性（CERObject::SetInt/NewInt）、旗標（CERPerson::NewFlag，最多 256 個）、裝的東西 ----
typedef struct { char key[32]; intptr_t val; } KV;
// 房間的出口（CERRoom::Exit 0x1005d8b0，最多 64 個）：方向、目的房間路徑、入口編號、開關（+0xc，預設 1）
typedef struct { char dir[32], path[128]; int entry, on; } ExitRec;
typedef struct {
    const Class *cls;
    char path[128], name[64], alias[64];
    KV *ints, *flags, *strs;  // strs 的 val = strdup 的字串
    int nints, nflags, nstrs;
    int parent;  // 在誰身上或哪個房間（-1 = 不在世界裡）
    int kind;    // 0 房間 1 人物 2 道具 5 技能 6 劇情事件（原版類別 CERRoom、CERPerson…）；-1 已刪除
    int leader;  // 人物：所屬隊伍的帶頭者（原版隊伍表 0x1005e0b0 的 +4）；0 = 自己帶隊或沒有隊伍
    ExitRec *exits;
    int nexits;
    void *item;  // 道具物件在引擎這邊的 Item（story_item_make 產生）
    int held;    // 道具在引擎的 inv 裡（item 交給引擎了）
} Obj;
static int refreshing, spawn_max = -1, spawn_count = -1, spawn_level = -1;  // 原版 0x10478d3c、0x1039f548、0x1039f544、0x1039f54c
static int checks = 1;                     // 移動時呼叫房間的出入檢查（原版 0x1039f550；go 已經查過就關掉）
static int quiet;                          // 產生物件中（原版 0x10478d40）：不送訊息給畫面
static Obj *objs;  // objs[0] 不用：代號 0 = 沒有
static int obj_count = 1, obj_cap;
static int the_player;

static Obj *O(intptr_t h) { return h > 0 && h < obj_count && objs[h].kind >= 0 ? &objs[h] : NULL; }

static KV *kv_find(KV *v, int n, const char *k) {
    for (int i = 0; i < n; i++)
        if (!strcmp(v[i].key, k)) return &v[i];
    return NULL;
}

static KV *kv_add(KV **v, int *n, const char *k) {
    KV *e = kv_find(*v, *n, k);
    if (e) return e;
    *v = realloc(*v, (*n + 1) * sizeof **v);
    e = &(*v)[(*n)++];
    snprintf(e->key, sizeof e->key, "%s", k);
    e->val = 0;
    return e;
}

static const char *S(intptr_t p) { return p ? (const char *)p : ""; }

// 原版虛擬函式表各格的預設（CERObject 0x10023520 等）：10 死亡／刪除前 1、11 0、12 1、13 能不能離開 1、14 能不能進來 1、
// 15 移動後、16 進房間後 1；17 人物說話 0，道具、劇情事件、建築 1
// 寶箱基底（原版 0x100f94c0：item_init 再設路徑、名稱、別名與開箱欄位）。isbuilding 1 = 還沒開、可開（開箱規則 0x1004e680）；
// 水缸、礦石、樹木（ITEM/赵村/…）都從它繼承，所以它們和寶箱一樣用「開」處理。
static void chest_base(intptr_t h, Obj *o, const char *path, const char *name, const char *alias) {
    story_set_int(h, "isbuilding", 1);
    if (o) { snprintf(o->path, sizeof o->path, "%s", path); snprintf(o->name, sizeof o->name, "%s", name); snprintf(o->alias, sizeof o->alias, "%s", alias); }
    story_set_str((int)h, "long", "宝箱的基类，一般不直接使用。可以被打开。");
    story_set_str((int)h, "key", "");
    story_set_int(h, "deletekey", 0);
    story_set_str((int)h, "lockinfo", "箱子紧锁，无法打开。");
    story_set_str((int)h, "openinfo", "");
    story_set_int(h, "contain", -1);
    story_set_int(h, "Level", 0);
}

static intptr_t slot_default(const Obj *o, int slot) {
    if (slot == 11) return 0;
    if (slot == 17) return o->kind != 1;
    return slot >= 10 && slot <= 17;
}

static intptr_t slot_calla(intptr_t h, int slot, intptr_t *args) {
    Obj *o = O(h);
    if (!o) return 0;
    args[0] = h;
    for (int i = 0; o->cls && i < o->cls->n; i++)
        if (o->cls->slots[i].slot == slot) return o->cls->slots[i].fn(args);
    return slot_default(o, slot);
}
static intptr_t slot_call(intptr_t h, int slot, intptr_t arg) { return slot_calla(h, slot, (intptr_t[8]){h, arg}); }

static int obj_add(int parent) {
    if (obj_count >= obj_cap) objs = realloc(objs, (obj_cap = obj_cap ? obj_cap * 2 : 1024) * sizeof *objs);
    int h = obj_count++;
    memset(&objs[h], 0, sizeof objs[h]);
    objs[h].parent = parent;
    objs[h].kind = 2;
    return h;
}

// 產生一個物件：照原版先呼叫第 1 格（設定路徑、名稱、別名、屬性）
int story_new(const char *path, int parent) {
    int h = obj_add(parent);
    Obj *o = &objs[h];
    o->cls = class_find(path);
    o->kind = o->cls ? o->cls->kind : 2;
    snprintf(o->path, sizeof o->path, "%s", path);
    if (!o->cls) fprintf(stderr, "劇情：找不到物件 %s\n", path);
    quiet++;
    slot_call(h, 1, 0);
    quiet--;
    return h;
}

static void obj_delete(int h);
int story_player(void) {
    if (!the_player) {
        the_player = story_new("秦殇/主角/游侠扶苏", 0);  // 沒創角就直接開始（測試、--prof）：遊俠扶蘇
        objs[the_player].kind = 1;
    }
    return the_player;
}
// 創角（JM09）選定的主角範本：EasyRPG.dll 0x10050630 依 G_PicID2 取「秦殇/主角/<職業人名>」產生玩家人物（含起始道具、屬性、外觀字串）。
// 新遊戲還沒進任何房間時呼叫，換掉預設的主角
void story_player_choose(const char *path) {
    if (the_player) obj_delete(the_player);
    the_player = story_new(path, 0);
    objs[the_player].kind = 1;
}

const char *story_name(int h) { return O(h) ? O(h)->name : ""; }
const char *story_alias(int h) { return O(h) ? O(h)->alias : ""; }
const char *story_path(int h) { return O(h) ? O(h)->path : ""; }
int story_kind(int h) { return O(h) ? O(h)->kind : -1; }

void (*story_on_exp)(int h, int exp);
void (*story_on_log)(int h, int kind, const char *text);
void *(*story_item_make)(int h, int type, int level, int col, const char *alias);
int (*story_on_person)(int h, const char *name, int level, int tier);
int (*story_on_skill)(int h, const char *key, int level);
void (*story_on_msg)(int id, const intptr_t *v, int n, const char *s);
const char *(*story_on_text)(int id);  // 系統訊息表 EasyRPG_C.dlg（原版 0x10060280）第 id 筆，沒有回 ""

static void sysmsg(int h, int kind, int id) {  // 0x1004dd00(誰, 種類, −1, 0x10060280(id))
    const char *t = story_on_text ? story_on_text(id) : "";
    if (story_on_log) story_on_log(h, kind, t);
}

static void msg(int id, const char *s, int n, ...) {
    if (quiet || !story_on_msg) return;
    intptr_t v[8] = {0};
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n && i < 8; i++) v[i] = va_arg(ap, intptr_t);
    va_end(ap);
    story_on_msg(id, v, n, s ? s : "");
}

int story_next(int owner, int after) {
    for (int i = after + 1; i < obj_count; i++)
        if (objs[i].parent == owner && objs[i].kind >= 0) return i;
    return 0;
}
static int child_count(int owner) {  // 原版 +0xc（0x10021cd0）
    int n = 0;
    for (int h = story_next(owner, 0); h; h = story_next(owner, h)) n++;
    return n;
}
void *story_item(int h) { return O(h) ? O(h)->item : NULL; }
void story_take(int h) { if (O(h)) { O(h)->parent = -1; O(h)->item = NULL; O(h)->held = 0; } }
void story_hold(int h) { if (O(h)) { O(h)->item = NULL; O(h)->held = 1; } }
void story_set_item(int h, void *item) { if (O(h)) { O(h)->item = item; O(h)->held = 0; } }
int story_parent(int h) { return O(h) ? O(h)->parent : -1; }
void story_move(int h, int parent) { if (O(h)) O(h)->parent = parent; }
void story_unhold(int owner) {
    for (int i = 1; i < obj_count; i++)
        if (objs[i].parent == owner && objs[i].held) objs[i].parent = -1;
}
void story_set_alias(int h, const char *s) { if (O(h)) snprintf(O(h)->alias, sizeof O(h)->alias, "%s", s); }
int story_put(int owner, void *item) {
    int h = obj_add(owner);
    objs[h].item = item;
    return h;
}
int story_get_int(int h, const char *key) {
    KV *e = O(h) ? kv_find(O(h)->ints, O(h)->nints, key) : NULL;
    return e ? (int)e->val : 0;
}
int story_has_int(int h, const char *key) { return O(h) && kv_find(O(h)->ints, O(h)->nints, key); }
void story_set_int(int h, const char *key, int v) { if (O(h)) kv_add(&O(h)->ints, &O(h)->nints, key)->val = v; }
const char *story_get_str(int h, const char *key) {  // 原版 0x10022ac0：沒有回 ""
    KV *e = O(h) ? kv_find(O(h)->strs, O(h)->nstrs, key) : NULL;
    return e ? S(e->val) : "";
}
void story_set_str(int h, const char *key, const char *s) {
    if (!O(h)) return;
    KV *e = kv_add(&O(h)->strs, &O(h)->nstrs, key);
    free((void *)e->val);
    e->val = (intptr_t)strdup(s ? s : "");
}
void story_set_name(int h, const char *s) { if (O(h)) snprintf(O(h)->name, sizeof O(h)->name, "%s", s); }

// ---- 世界：房間、出口 ----
// 依路徑找已經產生的物件（原版 0x1005b0d0）
static int find_path(const char *path) {
    if (!*path) return 0;
    for (int i = 1; i < obj_count; i++)
        if (objs[i].kind >= 0 && !strcmp(objs[i].path, path)) return i;
    return 0;
}
// 房間要用時才產生（原版 0x1004db90：找不到就依路徑建，G_Created = 0）
static int load_room(const char *path) {
    int h = find_path(path);
    if (h || !*path || !class_find(path)) return h;
    h = story_new(path, 0);
    story_set_int(h, "G_Created", 0);
    story_set_int(h, "__fresh", 1);  // 剛產生（第 1 格放好了），第一次進去不用刷新
    return h;
}
// 先找、找不到再產生（劇情 API 指定房間路徑的寫法：0x1005b0d0 → 0x1004db90）
static int room_at(const char *path) { int h = find_path(path); return h ? h : load_room(path); }

// 進房間（依別名，= 場景檔名，例如 xy_hs2）：每個房間只產生一次。找不到回 0
int story_room(const char *alias) {
    for (int i = 1; i < obj_count; i++)
        if (objs[i].kind == 0 && objs[i].cls && !strcmp(objs[i].cls->alias, alias)) return i;
    for (int i = 0; i < class_count; i++)
        if (classes[i].kind == 0 && !strcmp(classes[i].alias, alias)) return load_room(classes[i].path);
    return 0;
}
int story_room_by_path(const char *path) { return load_room(path); }
const char *story_room_alias(int room) { return O(room) && O(room)->cls ? O(room)->cls->alias : ""; }

// 主角所在的房間（原版 0x100558c0 = 目前處理中的玩家的位置）
static int w_current_room(void) { return O(the_player) ? O(the_player)->parent : 0; }

static ExitRec *exit_find(int room, const char *dir, int exact) {  // 原版 0x1005d9d0：先完全相符，再開頭相符
    Obj *o = O(room);
    if (!o) return NULL;
    for (int i = 0; i < o->nexits; i++)
        if (!strcmp(o->exits[i].dir, dir)) return &o->exits[i];
    if (!exact)
        for (int i = 0; i < o->nexits; i++)
            if (!strncmp(o->exits[i].dir, dir, strlen(o->exits[i].dir)) && strstr(o->exits[i].dir, dir) == o->exits[i].dir) return &o->exits[i];
    return NULL;
}
static void exit_set(int room, const char *dir, const char *path, int entry) {  // 0x1005d8b0：同方向就改
    Obj *o = O(room);
    if (!o) return;
    if (o->nexits >= 64) fprintf(stderr, "CERRoom::Exit() 警告 : 出口的数量超过最大值<%d:64>!\n", o->nexits);
    ExitRec *x = exit_find(room, dir, 1);
    if (!x) {
        o->exits = realloc(o->exits, (o->nexits + 1) * sizeof *o->exits);
        x = &o->exits[o->nexits++];
        memset(x, 0, sizeof *x);
        snprintf(x->dir, sizeof x->dir, "%s", dir);
        x->on = 1;
    }
    snprintf(x->path, sizeof x->path, "%s", path);
    x->entry = entry;
}
// 出口開關（0x1003ed10、0x1003ee60）：送 0x52c（"房間別名 方向" + 開關）給畫面
static int exit_enable(int room, const char *dir, int on) {
    ExitRec *x = exit_find(room, dir, 0);
    if (!x) return 0;
    x->on = on < 2 ? on : 1;
    char s[200];
    snprintf(s, sizeof s, "%s %s", story_alias(room), x->dir);
    msg(0x52c, s, 1, x->on);
    return 1;
}
int story_exit(int room, const char *dir, char *path, int cap, int *entry) {  // 引擎走出口用：找到且開著回 1
    ExitRec *x = exit_find(room, dir, 0);
    if (!x || !x->on) return 0;
    snprintf(path, cap, "%s", x->path);
    *entry = x->entry;
    return 1;
}
int story_exit_on(int room, const char *dir) { ExitRec *x = exit_find(room, dir, 0); return x ? x->on : -1; }

// ---- 依別名找物件 ----
static int alias_match(const char *a, const char *pre) { return strstr(a, pre) == a; }  // 原版 0x1000df90 是「不相符」
// 容器裡別名完全相符、再開頭相符的（原版 0x1003fab0 的找法；"#N" = 物件編號）
static int find_in(int owner, const char *alias) {
    if (alias[0] == '#') return O(atoi(alias + 1)) ? atoi(alias + 1) : 0;
    for (int h = story_next(owner, 0); h; h = story_next(owner, h))
        if (!strcmp(objs[h].alias, alias)) return h;
    return 0;
}
// 0x1003fd90：mode 1 先看目前房間自己、再找房間裡的；mode 0 找全部（先完全相符，再開頭相符）
static int w_find_alias(const char *alias, int mode) {
    if (alias[0] == '#') return O(atoi(alias + 1)) ? atoi(alias + 1) : 0;
    if (mode == 1) {
        int room = w_current_room();
        if (O(room) && !strcmp(objs[room].alias, alias)) return room;
        for (int h = story_next(room, 0); h; h = story_next(room, h))
            if (!strcmp(objs[h].alias, alias)) return h;
        for (int h = story_next(room, 0); h; h = story_next(room, h))
            if (alias_match(objs[h].alias, alias)) return h;
        return 0;
    }
    for (int i = 1; i < obj_count; i++)
        if (objs[i].kind >= 0 && !strcmp(objs[i].alias, alias)) return i;
    for (int i = 1; i < obj_count; i++)
        if (objs[i].kind >= 0 && alias_match(objs[i].alias, alias)) return i;
    return 0;
}

// ---- 隊伍（原版每個人物有一張隊伍表 0x1005e0b0；帶頭者的表放隊員，隊員的表 +4 指向帶頭者）----
static int leader_of(int h) { return O(h) && O(objs[h].leader) ? objs[h].leader : h; }  // 0x10054f90
static int team_next(int leader, int after) {  // 帶頭者的隊員依序（0x100406a0、0x1004e4c0）
    for (int i = after + 1; i < obj_count; i++)
        if (objs[i].kind >= 0 && objs[i].leader == leader && i != leader) return i;
    return 0;
}
static int team_size(int h) { int n = 0; for (int m = team_next(h, 0); m; m = team_next(h, m)) n++; return n; }
int story_team_next(int leader, int after) { return team_next(leader, after); }

// ---- 屬性改變時通知畫面 ----
// 原版 0x10047870 依屬性名稱送不同的更新訊息；這裡統一送 "@屬性" 給引擎（0x47870），引擎再從物件讀
static void attr_changed(int h, const char *key) {
    char s[64];
    snprintf(s, sizeof s, "@%s", key);
    msg(0x47870, s, 1, h);
}

// 原版 0x1003c6a0 SetAttr：B_ 開頭的加上去（B_C_ 另外通知）、HP/MP 改了送血量、Invincible 送 0x535、
// G_NPCActive 送 0x51b、Money 走 add_money；其餘照加。最後通知 "@屬性"
static void w_add_money(int h, int v);
static void w_set_attr(int h, const char *key, int v) {
    Obj *o = O(h);
    if (!o) return;
    KV *e;
    if (key[0] == 'B' && key[1] == '_') {
        e = kv_add(&o->ints, &o->nints, key);
        e->val += v;
        if (key[2] == 'C' && key[3] == '_') attr_changed(h, key);
        return;
    }
    if (!strcmp(key, "Money")) w_add_money(h, v);
    else {
        e = kv_add(&o->ints, &o->nints, key);
        if (!strcmp(key, "HP") || !strcmp(key, "MP")) {  // 0x10026c80：夾在 0 ~ <屬性>_MAX；已經滿了不再加
            char mk[40];
            snprintf(mk, sizeof mk, "%s_MAX", key);
            int mx = story_get_int(h, mk);
            if (mx == e->val && v >= 0) return;
            e->val = mx < e->val + v ? mx : e->val + v < 0 ? 0 : e->val + v;
        } else e->val += v;
        if (!strcmp(key, "Invincible")) msg(0x535, NULL, 2, h, (intptr_t)e->val);
        else if (!strcmp(key, "G_NPCActive")) msg(0x51b, NULL, 3, h, the_player, (intptr_t)e->val);
        else if (!strcmp(key, "HP") || !strcmp(key, "MP") || !strcmp(key, "BD") || !strcmp(key, "BD_MAX")) msg(0x42aa0, NULL, 1, h);
    }
    attr_changed(h, key);
}

static void w_add_money(int h, int v) {  // 0x1003d3e0：超過 5 億或負數歸零
    Obj *o = O(h);
    if (!o) return;
    KV *e = kv_add(&o->ints, &o->nints, "Money");
    e->val += v;
    if (e->val < 0 || e->val > 499999999) e->val = 0;
}

static void set_aigroup(int h, int g, int notify) {  // 0x1005a3b0：連同隊員
    story_set_int(h, "G_AIGroup", g);
    if (notify) msg(0x51c, NULL, 2, h, g);
    for (int m = team_next(h, 0); m; m = team_next(h, m)) set_aigroup(m, g, 1);
}

// ---- 移動、刪除 ----
// 原版 0x10059020 move(目的, 物件)：查出入（舊位置第 13 格、目的第 14 格），搬過去後物件第 15 格（目的, 舊位置）。
// 目的 = 0 時拿出世界。回傳 > 0 = 搬了
static int obj_move(int dest, int h) {
    Obj *o = O(h);
    if (!o) return 0;
    int old = o->parent;
    int user = h == the_player;
    if (checks && O(old) && dest && (user || (O(dest) && objs[dest].kind == 1 && objs[old].kind == 1)))
        if (!slot_calla(old, 13, (intptr_t[8]){0, dest, h})) return 0;
    if (!dest) { o->parent = -1; msg(0x51d, NULL, 1, h); return 1; }
    if (checks && O(old) && (user || (objs[dest].kind == 1 && objs[old].kind == 1))) {
        if (!slot_calla(dest, 14, (intptr_t[8]){0, h, old})) return 0;
    }
    o->parent = dest;
    slot_calla(h, 15, (intptr_t[8]){0, dest, old});
    msg(0x51d, NULL, 2, h, dest);  // 物件換位置：引擎看是不是進出目前房間
    return 1;
}

// 0x1004df20：搬到 dest（同一個地方就不動）
static int put_in(int h, int dest) {
    if (dest && O(h) && objs[h].parent == dest) return 0;
    return obj_move(dest, h);
}

static void team_leave(int h);
// 0x1003e920 DelObject：第 12、10 格都同意才刪；身上的東西放到它所在的地方，人物先離開隊伍
static void obj_delete(int h) {
    Obj *o = O(h);
    if (!o) return;
    if (!slot_call(h, 12, 0) || !slot_call(h, 10, 0)) return;
    int where = o->parent;
    for (int c = story_next(h, 0); c; c = story_next(h, c)) put_in(c, where > 0 ? where : 0);
    if (o->kind == 1) {
        team_leave(h);
        for (int m; (m = team_next(h, 0));) team_leave(m);
    }
    put_in(h, 0);
    o->kind = -1;
    o->parent = -1;
}
void story_delete(int h) { obj_delete(h); }

// ---- 人物（怪物）----
static int person_new(const char *name, int level, int tier) {  // 0x10059870 newpersonex
    int h = obj_add(0);
    objs[h].kind = 1;
    objs[h].parent = -1;
    if (!story_on_person || !story_on_person(h, name, level, tier)) {
        fprintf(stderr, "newpersonex() 错误 (0) : 没有找到物体类型<%s>!\n", name);
        objs[h].kind = -1;
        return 0;
    }
    return h;
}

// 房間裡 Index 是 name（等級 = level，−1 不管）的人物數（0x1004acb0）
static int count_index(int room, const char *name, int level) {
    int n = 0;
    for (int h = story_next(room, 0); h; h = story_next(room, h))
        if (objs[h].kind == 1 && !strcmp(story_get_str(h, "Index"), name) && (level < 0 || story_get_int(h, "Level") == level)) n++;
    return n;
}
static int players_in(int room) { return O(the_player) && objs[the_player].parent == room; }  // 0x1004b740

// 0x10052550 RandSetPersonEx(房間, 怪物表名稱, 數量, 入口編號, 等級, 階, G_AIGroup)
static int w_spawn(int room, const char *name, int n, int start, int level, int tier, int ai) {
    if (child_count(room) >= 0x200) return 0;
    if (room) {
        int k = players_in(room);
        k = k < 1 ? 1 : k > 16 ? 16 : k;
        n = (k + 3) * n / 4;
    }
    if (refreshing) {
        if (spawn_count < 0 && spawn_max < 0) { fprintf(stderr, "RandSetPersonEx() 错误 (0) : 需要调用InitSetMax()函数来设置最大值!\n"); return 0; }
        int c = count_index(room, name, spawn_level);
        if (!(c < spawn_count || spawn_count < 0)) return 0;
        if (spawn_max > 0 && child_count(room) >= spawn_max) return 0;
        n = (n - c) / 3;
        if (!n) n = 1;
    }
    int first = person_new(name, level, tier);
    int is_room = O(room) && objs[room].kind == 0;
    if (first) {
        story_set_int(first, "G_AIGroup", ai);
        if (is_room) { story_set_int(first, "G_PosX", -100); story_set_int(first, "G_PosY", start); }
        story_set_int(first, "StartPoint", start);
        put_in(first, room);
    }
    for (int i = 1; first && i < n; i++) {  // 其餘照第一隻複製（同一列數值、同等級；原版第 9 格 copy），掉寶各自算
        int h = obj_add(-1);
        Obj *d = &objs[h], *s = &objs[first];
        d->kind = 1;
        memcpy(d->path, s->path, sizeof d->path);
        memcpy(d->name, s->name, sizeof d->name);
        memcpy(d->alias, s->alias, sizeof d->alias);
        for (int k = 0; k < s->nints; k++) kv_add(&d->ints, &d->nints, s->ints[k].key)->val = s->ints[k].val;
        for (int k = 0; k < s->nstrs; k++) story_set_str(h, s->strs[k].key, S(s->strs[k].val));
        story_set_int(h, "G_AIGroup", ai);
        if (is_room) { story_set_int(h, "G_PosX", -100); story_set_int(h, "G_PosY", start); }
        story_set_int(h, "StartPoint", start);
        put_in(h, room);
    }
    return first;
}

// 0x100528d0 RandSetPersonExBox(房間, 名稱, 數量, …, G_AIGroup)：剛好 n 隻，各自依表產生
static int spawn_exact(int room, const char *name, int n, int level, int tier, int ai) {
    if (refreshing) {
        if (spawn_count < 0 && spawn_max < 0) { fprintf(stderr, "RandSetPersonExBox() 错误 (0) : 需要调用InitSetMax()函数来设置最大值!\n"); return 0; }
        int c = count_index(room, name, -1);
        if (!((c < spawn_count || spawn_count < 0) && (spawn_max < 1 || child_count(room) < spawn_max))) return 0;
    }
    int h = 0;
    for (int i = 0; i < n; i++)
        if ((h = person_new(name, level, tier))) { story_set_int(h, "G_AIGroup", ai); put_in(h, room); }
    return h;
}

// 隊友依職業技能樹學技能（0x100265d0，原版由升級和入隊自動加點函數呼叫；這裡只在入隊時呼叫，隊友升級和自動加點還沒做）：Friend >= 2、IsActorNPC 1~5（游侠、力士、刺客、术士、巫师）；
// 技能目錄 秦殇/主角/<職業>技能 下還沒學的，先建暫時物件看 Level_Req，高於等級就丟掉，否則放進隊友身上。
// shortcut: 只做單機目錄（原版連線版改用 秦殇/NET/…）；AddSkill 的技能效果和 G_IsUISkill == 4 的介面登記沒做，隊友用引擎的技能表出招
static void team_learn(int h, int level) {
    static const char *const CLS[5] = {"游侠", "力士", "刺客", "术士", "巫师"};
    int actor = story_get_int(h, "IsActorNPC") - 1;
    if (!O(h) || actor < 0 || actor > 4 || story_get_int(h, "Friend") < 2) return;
    char pre[64];
    snprintf(pre, sizeof pre, "秦殇/主角/%s技能/", CLS[actor]);
    for (int i = 0; i < class_count; i++) {
        if (classes[i].kind != 5 || strncmp(classes[i].path, pre, strlen(pre))) continue;
        int have = 0;
        for (int c = story_next(h, 0); c && !have; c = story_next(h, c)) have = !strcmp(objs[c].path, classes[i].path);
        if (have) continue;
        int sk = story_new(classes[i].path, -1);
        if (!O(sk)) continue;
        if (story_get_int(sk, "Level_Req") > level) { objs[sk].kind = -1; continue; }  // 原版刪暫時物件
        put_in(sk, h);
    }
}

#include "team_tables.h"
int (*story_exp_for)(int level);  // 升到 level+1 要的累計經驗（引擎的 Data/experience.txt，原版 DAT_104784e8[level]）；沒設 = 不升級
int story_exp_max;                // 最高等級（原版 DAT_1039b5e8）

// 隊友（非玩家人物）加經驗（0x10026d30）：Exp、CurExp 加上去，夠了連升幾級，每級 Extra_Exp +10、Extra_Skill +1；
// 升級後學新技能（0x100265d0 = team_learn）、訊息 0x44e。shortcut: 升級後的 Upgrade_ 屬性重算（HP_MAX／MP_MAX 隨等級）沒做：單機劇情物件上沒有 Upgrade_* 欄位，
// 隊友的 HP_MAX 等等有隊友角色（引擎）時再算
void story_gain_exp(int h, int exp) {
    if (!O(h)) return;
    story_set_int(h, "Exp", story_get_int(h, "Exp") + exp);
    story_set_int(h, "CurExp", story_get_int(h, "CurExp") + exp);
    int lv = story_get_int(h, "Level"), nl = lv, e = story_get_int(h, "Exp");
    if (story_exp_for && story_exp_for(lv) <= e) {
        do nl++; while (nl < story_exp_max && story_exp_for(nl) <= e);
        if (nl > story_exp_max) nl = story_exp_max;
        story_set_int(h, "Level", nl);
    }
    if (story_exp_for) story_set_int(h, "NextExp", story_exp_for(nl) - e);
    int gain = nl > lv ? nl - lv : 0;
    if (gain < 1) return;
    story_set_int(h, "Extra_Exp", (story_get_int(h, "Extra_Exp") < 0 ? 0 : story_get_int(h, "Extra_Exp")) + gain * 10);
    story_set_int(h, "Extra_Skill", (story_get_int(h, "Extra_Skill") < 0 ? 0 : story_get_int(h, "Extra_Skill")) + gain);
    team_learn(h, nl);
    if (story_on_text && story_on_log) {
        char s[256];
        snprintf(s, sizeof s, story_on_text(0x44e), story_name(h));
        story_on_log(h, 14, s);
    }
}

// 0x10029550：升級後算下一級經驗（NextExp = 經驗表[等級] − Exp）、學技能（0x100265d0）
// shortcut: 之後原版做 Upgrade_ 屬性重算，同 story_gain_exp 沒做
static void team_level_set(int h, int level) {
    if (story_exp_for) story_set_int(h, "NextExp", story_exp_for(level) - story_get_int(h, "Exp"));
    team_learn(h, level);
}

// 隊友入隊自動加點（0x100295b0，AddMember 0x10037850 在 Friend 2、IsActorNPC 1~5、還沒有旗標 jiaruduiwu 時呼叫）：
// 經驗補到帶頭者的 Exp × (隊友悟性+250)/(帶頭者悟性+250) × 90/100；再依職業（G_PicID2）表把 Extra_Exp 照屬性優先順序加進六項屬性
// （每項要 (int)(每級點數 × (等級−1)) 點，不夠就停），Extra_Skill 依 8 個技能的等級公式 ((等級−1−req)/step + 1) 加技能等級。
// 照原版保留：點數用完（> 0 才寫回）時 Extra_Exp、Extra_Skill 屬性不更新（留舊值）
// shortcut: 只做單機表（連線版 DAT_10478d30 == 1 用 秦殇/NET/ 路徑）；屬性同時寫 *0 基礎值和現值（原版只寫 *0，再整個重算）；
// 技能升級後原版套技能參數，隊友沒有引擎角色，不做
static void team_autoadd(int lead, int m) {
    if (!O(lead) || !O(m)) return;
    int pic2 = story_get_int(m, "G_PicID2");
    const TeamClass *tc = NULL;
    for (size_t i = 0; i < sizeof team_class / sizeof *team_class; i++) if (team_class[i].pic2 == pic2) tc = &team_class[i];
    float ratio = (float)(story_get_int(m, "Savvy") + 250) / (float)(story_get_int(lead, "Savvy") + 250);
    unsigned target = (unsigned)((float)(unsigned)story_get_int(lead, "Exp") * ratio * 90.0f / 100.0f);
    unsigned have = (unsigned)story_get_int(m, "Exp");
    if (!(have < target || lead == m)) return;
    story_gain_exp(m, (int)(target - have));
    team_level_set(m, story_get_int(m, "Level"));
    if (!tc) return;  // 表上沒有這個職業（原版會寫空名稱的屬性）
    int lv = story_get_int(m, "Level"), ex = story_get_int(m, "Extra_Exp");
    for (int k = 0; k < 6; k++) {
        int need = (int)(tc->incr[k] * (float)(lv - 1));
        if (ex < need) break;
        ex -= need;
        char cur[16];
        snprintf(cur, sizeof cur, "%.*s", (int)strlen(tc->attr[k]) - 1, tc->attr[k]);  // Str0 → Str
        story_set_int(m, tc->attr[k], (int)tc->base[k] + need);
        story_set_int(m, cur, (int)tc->base[k] + need);
    }
    if (ex > 0) story_set_int(m, "Extra_Exp", ex);
    int pts = story_get_int(m, "Extra_Skill");
    for (int k = 0; k < 8; k++) {
        if (pts < 1) return;
        int sk = 0;
        for (int c = story_next(m, 0); c && !sk; c = story_next(m, c)) if (!strcmp(story_alias(c), tc->skill[k])) sk = c;
        int want = (lv - 1 - tc->req[k]) / tc->step[k] + 1, cur = sk ? story_get_int(sk, "Level") : 0;
        if (sk && want > 0 && cur < want) {
            int add = want - cur, room = story_get_int(sk, "Level_Max") - cur;
            if (room < add) add = room;
            if (add > pts) add = pts;
            pts -= add;
            story_set_int(sk, "Level", cur + add);
        }
    }
    if (pts > 0) {
        story_set_int(m, "Extra_Skill", pts);
        if (ex < 1) story_set_int(m, "Extra_Exp", 0);  // 原版刪掉這個屬性
    }
}

// 0x10037850 AddMember(帶頭者, 隊員)
static void team_add(int lead, int m) {
    if (!O(lead) || objs[lead].kind != 1) { fprintf(stderr, "AddMember: 不是人物\n"); return; }
    if (!O(m) || objs[m].kind != 1) return;
    objs[m].leader = lead;
    story_set_int(m, "G_AIGroup_Save", story_get_int(m, "G_AIGroup"));
    team_learn(m, story_get_int(m, "Level"));
    set_aigroup(m, story_get_int(lead, "G_AIGroup"), 0);
    int friend_ = story_get_int(m, "Friend"), actor = story_get_int(m, "IsActorNPC");
    if (friend_ == 2 && actor >= 1 && actor <= 5) {  // 主角級的隊友：外觀字串照職業
        static const char *const EQ[5] = {
            "001*CA_009*HD_009*LA_009*LE_009*LF_009*RA_009*RE_009*RF_009*RH_009_001*SD_000*TK_009",
            "001*CA_001*HD_001*LA_001*LE_001*LF_001*LH_001*RA_001*RE_001*RF_001*RH_001_001*TK_001",
            "001*CA_001*HD_001*LA_001*LE_001*LF_001*LH_001*RA_001*RE_001*RF_001*RH_001_001*TK_001",
            "001*CA_001*HD_001*LA_000*LE_000*LF_001*LH_001*RA_000*RE_000*RF_001*RH_004_001*TK_001",
            "001*CA_001*HD_005*LA_001*LE_000*LF_001*LH_004_001*RA_001*RE_000*RF_001*RH_001*TK_001"};
        char key[32];
        snprintf(key, sizeof key, "G_EquipStr%d", actor);
        (void)EQ;  // shortcut: 原版這裡用 (&local_420)[IsActorNPC] 取字串，索引偏了一格（local_420 = Friend），照抄會拿到 EQ[actor-1]
        story_set_str(m, key, EQ[actor - 1]);
    }
    KV *jr;
    if (lead == the_player) {
        msg(0x49b70, NULL, 2, lead, m);  // 原版 0x10049b70、0x100443d0：隊伍面板加一格
        if (friend_ == 2 && actor >= 1 && actor <= 5 && (!(jr = kv_find(O(m)->flags, O(m)->nflags, "jiaruduiwu")) || jr->val < 1)) team_autoadd(lead, m);  // 0x100295b0；有旗標 = 已入過隊
        if (friend_ >= 2) {
            char s[256];
            snprintf(s, sizeof s, "%s加入了队伍。", story_name(m));  // shortcut: 原版字串 easyrpg.dlg 0x40c，還沒讀那個檔
            if (story_on_log) story_on_log(lead, 3, s);
        }
    }
}
// 0x10053b40 RemoveMember(隊員)：回復原本的 G_AIGroup，Friend 1 改 −1
static void team_leave(int h) {
    if (!O(h) || objs[h].kind != 1) return;
    int lead = objs[h].leader;
    if (!O(lead)) { objs[h].leader = 0; return; }
    if (lead == the_player) msg(0x51e, NULL, 1, h);
    set_aigroup(h, story_get_int(h, "G_AIGroup_Save"), 0);
    if (story_get_int(h, "Friend") == 1) story_set_int(h, "Friend", -1);
    msg(0x51c, NULL, 2, h, story_get_int(h, "G_AIGroup"));
    objs[h].leader = 0;
}

// ---- 劇情事件（CERPCT）----
StoryPlot story_plot;
// 0x1004f1a0 開始劇情事件：產生物件，第 17 格檢查不過就刪掉；過了送 0x52e（播 Rec/User/<別名>.rec），播完由引擎呼叫 story_plot_end
static void w_start_plot(const char *path) {
    if (!class_find(path)) { fprintf(stderr, "劇情事件找不到 <%s>（easyrpg.dlg 0xbcd）\n", path); return; }
    int h = story_new(path, -1);
    if (objs[h].kind != 6) { fprintf(stderr, "不是劇情事件 <%s>（0xbce）\n", path); return; }
    if (!slot_call(h, 17, 0)) { objs[h].kind = -1; return; }
    if (story_plot.h) { fprintf(stderr, "劇情事件 %s 還沒結束又開始 %s\n", story_path(story_plot.h), path); }
    story_plot.h = h;
    snprintf(story_plot.alias, sizeof story_plot.alias, "%s", objs[h].alias);
    msg(0x52e, objs[h].alias, 0);
}
void story_pct_start(const char *path) { w_start_plot(path); }
// 0x1004f2a0：第 18 格（給獎勵、寫日誌）後刪掉
void story_plot_end(void) {
    int h = story_plot.h;
    story_plot.h = 0;
    if (!O(h)) return;
    slot_call(h, 18, 0);
    objs[h].kind = -1;
}

// ---- 房間刷新 ----
// 原版 0x10052e40 每隔一段時間輪流對沒有玩家的房間呼叫第 6 格（重放怪物、商店進貨）。
// shortcut: 原版的週期沒算，改成每次進房間刷新一次（離開期間＝沒有玩家）
void story_refresh(int room) {
    if (!O(room)) return;
    if (story_get_int(room, "__fresh")) { story_set_int(room, "__fresh", 0); return; }
    refreshing = 1;
    slot_call(room, 6, 0);
    for (int h = story_next(room, 0); h; h = story_next(room, h)) slot_call(h, 6, 0);
    refreshing = 0;
}

// 新遊戲的開場房間別名（登記時第 4 個參數 = 1 的房間）
const char *story_start_room(void) {
    for (int i = 0; i < class_count; i++)
        if (classes[i].kind == 0 && classes[i].start) return classes[i].alias;
    return NULL;
}

// 房間裡別名是 alias 的物件（場景 .ONT 的「事件ID」= 劇情 NPC 的別名）
int story_find(int room, const char *alias) { return find_in(room, alias); }

// 主角進房間（原版 go 0x10017e30 → 0x1004f9a0 → 0x1003d470）：舊房間第 13 格、新房間第 14 格都同意才進，
// 進了以後隊員跟著，新房間第 16 格。回傳 1 = 進去了
int story_enter(int room) {
    int pl = story_player(), old = objs[pl].parent;
    if (!O(room)) return 0;
    if (O(old) && old != room) {
        if (!slot_calla(old, 13, (intptr_t[8]){0, room, pl})) return 0;
        if (!slot_calla(room, 14, (intptr_t[8]){0, pl, old})) return 0;
    }
    checks = 0;
    story_set_int(pl, "G_PosX", -1);
    story_set_int(pl, "G_PosY", -1);
    objs[pl].parent = room;
    for (int m = team_next(pl, 0); m; m = team_next(pl, m)) objs[m].parent = room;
    story_set_int(pl, "FightStatus", 0);
    checks = 1;
    slot_call(room, 16, pl);
    return 1;
}
// 主角放到房間但不查（新遊戲、讀檔）
void story_place(int room) {
    int pl = story_player();
    objs[pl].parent = room;
    for (int m = team_next(pl, 0); m; m = team_next(pl, m)) objs[m].parent = room;
}

// 新遊戲開局：主角放進開場房間後跑進房間檢查（第 14 格）。赵村的第 14 格在旗標 kaishiyouxi 還沒有時開「亡命天涯」（日誌「重返咸阳」、血量扣 2/3），
// 單機沒有別的房間可「走進來」，不跑的話新遊戲永遠沒有這段開場、也沒有任務。shortcut: 原版新遊戲怎麼進開場房間（0x1003d470 的 old = 空，不查第 13／14 格）沒查到，這裡補跑
void story_start(int room) {
    story_place(room);
    if (O(room)) slot_calla(room, 14, (intptr_t[8]){0, story_player(), 0});
}

// 隨機出口（0x10052af0）：房間的 "Exit_<方向>_<n>" 字串有 1/(n+1) 機會不用，否則挑一個產生地下城房間，
// 它的 "out" 出口回到原本的出口。有換時 path 改成新房間、回 1
int story_random_exit(int room, const char *dir, char *path, int cap, int *entry) {
    Obj *o = O(room);
    if (!o) return 0;
    char pre[64];
    snprintf(pre, sizeof pre, "Exit_%s_", dir);
    int cand[64], n = 0;
    for (int i = 0; i < o->nstrs && n < 64; i++)
        if (alias_match(o->strs[i].key, pre)) cand[n++] = i;
    if (!n) return 0;
    int k = rand() % (n + 1);
    if (k == n) return 0;
    const char *target = S(o->strs[cand[k]].val);
    int old = find_path(target);
    if (old) obj_delete(old);
    int nr = class_find(target) ? story_new(target, 0) : 0;
    ExitRec *x = exit_find(room, dir, 1);
    if (!nr || !x) return 0;
    exit_set(nr, "out", x->path, x->entry);
    snprintf(path, cap, "%s", target);
    *entry = 1;  // 地下城從入口 1 進
    return 1;
}

// ---- 對話：原版用 0x5dd 訊息把說話者、台詞、選項送給畫面 ----
StoryDialog story_dialog;

static void dialog_line(const char *s) {
    size_t n = strlen(story_dialog.text);
    snprintf(story_dialog.text + n, sizeof story_dialog.text - n, "%s%s", n ? "\n" : "", s);
}

// 跟 NPC 說話（原版 0x10058d30）：不是人物的改成 look；人物先轉向（0x519），送名字，再呼叫第 17 格（參數 1），
// 回 1 時排列問答（0x100569f0），G_NPCActive 是 1~9 的減一
void story_talk(int npc) {
    memset(&story_dialog, 0, sizeof story_dialog);
    story_dialog.npc = npc;
    if (story_kind(npc) == 1) msg(0x519, NULL, 2, npc, the_player);
    snprintf(story_dialog.speaker, sizeof story_dialog.speaker, "%s", story_name(npc));
    if (!slot_call(npc, 17, 1)) return;
    int act = story_get_int(npc, "G_NPCActive");
    if (act != 10 && act > 0) w_set_attr(npc, "G_NPCActive", -1);  // SetAttr 是加上去：少一
}

// 選第 i 個回答：清掉畫面上的台詞和選項，呼叫選項登記的處理
void story_choose(int i) {
    if (i < 0 || i >= story_dialog.option_count) return;
    StoryOption op = story_dialog.options[i];
    story_dialog.text[0] = 0;
    story_dialog.option_count = 0;
    if (op.fn) op.fn((intptr_t[8]){story_dialog.npc, the_player});
    else if (fn_find(op.addr)) story_call_addr(op.addr, (intptr_t[8]){story_dialog.npc, the_player});
    else fprintf(stderr, "劇情：選項 %s 指到原版基底 0x%08x，還沒翻\n", op.key, op.addr);
}

// 物件死亡／被打倒（引擎呼叫）：第 10 格（例如 Boss 死了開劇情），回 1 才刪
int story_die(int h) {
    if (!O(h)) return 0;
    if (!slot_call(h, 10, 0)) return 0;
    obj_delete(h);
    return 1;
}

// say 的格式字串最多帶 7 個參數；%s 以外都當整數
static void say_fmt(intptr_t *a) {
    char out[1024];
    snprintf(out, sizeof out, S(a[0]), a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    dialog_line(out);
}

// 房間裡別名開頭相符的物件都做 fn（0x1003cbf0、0x1003c550、0x1003cd10 的走訪）；all = 0 只做第一個
static int room_arg(const char *path) { return path && *path ? room_at(path) : w_current_room(); }

// 從 owner（和它的隊員）身上刪掉 n 個別名是 alias、UI_Pos < 200 的（0x1003e160）
static void remove_items(int owner, const char *alias, int n) {
    int done = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int who = pass ? team_next(owner, 0) : owner; who; who = pass ? team_next(owner, who) : 0) {
            for (int h = story_next(who, 0); h; h = story_next(who, h))
                if (!strcmp(objs[h].alias, alias) && (unsigned)story_get_int(h, "UI_Pos") < 200) {
                    obj_delete(h);
                    if (++done >= n) return;
                }
            if (!pass) break;
        }
}

// 身上別名開頭相符、UI_Pos < 200 的件數（0x1004abd0）；連隊員（0x1004ad90）
static int count_alias(int owner, const char *alias) {
    int n = 0;
    for (int h = story_next(owner, 0); h; h = story_next(owner, h))
        if (alias_match(objs[h].alias, alias) && story_get_int(h, "UI_Pos") < 200) n++;
    return n;
}

intptr_t engine_call(uint32_t addr, intptr_t *a) {
    static int trace = -1;
    if (trace < 0) trace = getenv("STORY_TRACE") != NULL;
    if (trace) fprintf(stderr, "[0x%08x %ld %ld %ld]\n", addr, (long)a[0], (long)a[1], (long)a[2]);
    Obj *o = O(a[0]);
    KV *e;
    int h, n;
    switch (addr) {
    case 0x100542e0: say_fmt(a); return 0;                                    // say
    case 0x1005eed0: return 0;  // clear_options：原版在 say 之後呼叫，清的是畫面上一輪的選項；這裡選擇時就清了
    case 0x1005f780:                                                         // option(自己, 鍵, 處理, 0, 0, 0, 文字)
        if (story_dialog.option_count < STORY_MAX_OPTIONS) {
            StoryOption *op = &story_dialog.options[story_dialog.option_count++];
            snprintf(op->key, sizeof op->key, "%s", S(a[1]));
            snprintf(op->text, sizeof op->text, "%s", S(a[6]));
            // 處理：劇情檔自己的方法傳的是包裝函式指標，原版基底的格子傳的是 0x10xxxxxx 位址
            if ((uintptr_t)a[2] >= 0x10000000u && (uintptr_t)a[2] < 0x11000000u) op->addr = (uint32_t)a[2];
            else op->fn = (intptr_t (*)(intptr_t *))a[2];
        }
        return 0;
    case 0x100558b0: return story_player();                                  // player
    case 0x10000000: return 1;  // ER_GetStartType：ER_Init 第 2 個參數。shortcut: 單機值當 1（劇情只比 2、3 = 連線版）
    case 0x1005df70: return o && (e = kv_find(o->flags, o->nflags, S(a[1]))) ? e->val : -1;  // get_flag
    case 0x1005da60: return o && kv_find(o->flags, o->nflags, S(a[1])) ? 1 : 0;              // find_flag
    case 0x1005f270:                                                         // set_flag：原版旗標不存在時報錯
        if (o && (e = kv_find(o->flags, o->nflags, S(a[1])))) e->val = a[2];
        else fprintf(stderr, "劇情：SetFlag 旗標不存在 <%s>\n", S(a[1]));
        return 0;
    case 0x1005e850:                                                         // new_flag：值 < 0 只建立不設值
        if (!o) return 0;
        e = kv_add(&o->flags, &o->nflags, S(a[1]));
        if (a[2] >= 0) e->val = a[2];
        return 0;
    case 0x100226e0: return o && (e = kv_find(o->ints, o->nints, S(a[1]))) ? e->val : 0;     // get_int
    case 0x10023c90: case 0x100231d0:                                        // set_int、new_int
        if (o) kv_add(&o->ints, &o->nints, S(a[1]))->val = a[2];
        return 0;
    case 0x10021c60:                                                         // add_int：回傳新值（原版屬性不存在時報錯）
        if (!o) return 0;
        e = kv_add(&o->ints, &o->nints, S(a[1]));
        return e->val += a[2];
    case 0x10023c60: if (o) snprintf(o->path, sizeof o->path, "%s", S(a[1])); return 0;  // set_path
    case 0x10023cf0: if (o) snprintf(o->name, sizeof o->name, "%s", S(a[1])); return 0;  // set_name
    case 0x10023c40: if (o) snprintf(o->alias, sizeof o->alias, "%s", S(a[1])); return 0;  // set_alias
    case 0x100226d0: return o ? (intptr_t)o->path : (intptr_t)"";             // 路徑
    case 0x10022490: return o ? (intptr_t)o->alias : (intptr_t)"";            // 別名
    case 0x10022980: case 0x1003f2c0: return o ? (o->parent > 0 ? o->parent : 0) : 0;  // 所在的地方（+4）
    case 0x10021cd0: return child_count((int)a[0]);                          // 身上幾個東西（+0xc）
    case 0x10022ac0: return (intptr_t)story_get_str((int)a[0], S(a[1]));      // 字串屬性，沒有回 ""
    case 0x100222f0: return o && kv_find(o->strs, o->nstrs, S(a[1])) ? 1 : 0;  // 有沒有這個字串屬性
    case 0x1003f300: return !strcmp(S(a[0]), S(a[1]));                       // 字串相等
    case 0x1003c510: strcat((char *)a[0], S(a[1])); return 0;               // strcat
    case 0x1020f731: return (intptr_t)malloc((size_t)a[0]);
    case 0x1020f761: free((void *)a[0]); return 0;
    case 0x1003aea0:                                                         // 打亂整數陣列（每次抽一個移到最前）
        for (int k = 0; k < a[1]; k++) {
            int *v = (int *)a[0], j = a[1] < 2 ? 0 : rand() % a[1], t = v[j];
            for (; j > 0; j--) v[j] = v[j - 1];
            v[0] = t;
        }
        return 0;
    case 0x1005e490: if (o) story_set_int(a[0], "__4b4", (int)a[1]); return 0;  // 人物 +0x4b4（shortcut: 用途還沒查）
    case 0x1005d790: return 0;                                               // 房間的解構（基底第 2 格），不用做
    case 0x1005d390:                                                         // room_init：房間的預設屬性（白天亮度 15、夜晚 0…）
        if (o) {
            static const char *const K[] = {"Level", "G_Type", "G_Day", "G_Night", "G_WEA_BigRain", "G_WEA_SmallRain", "G_WEA_RedRain",
                                            "G_WEA_BigSnow", "G_WEA_SmallSnow", "G_WEA_Hail", "G_WEA_RedSky", "G_WEA_PinkSky",
                                            "G_WEA_WhiteSky", "G_WEA_Gale", "G_Ufo", "G_Created", "RandomExitCount"};
            static const int V[] = {1, 10, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
            for (int k = 0; k < (int)(sizeof V / sizeof *V); k++) story_set_int(a[0], K[k], V[k]);
        }
        return 0;
    case 0x1005cd60: case 0x1005cab0: case 0x1005d4a0: case 0x1005cd40:
        return 0;  // 其他基底初始化。shortcut: 基底的預設屬性還沒照原版設
    case 0x100f94c0: chest_base(a[0], o, "秦殇/宝箱", "宝箱", "boxes"); return 0;  // 寶箱基底（原版 0x100f94c0；isbuilding 1 = 可開）
    case 0x100f95b0:                                                         // 随机物品宝箱（先跑基底，再改名）
        chest_base(a[0], o, "秦殇/随机物品宝箱", "随机物品宝箱", "suijiwupinbaoxiang");
        story_set_str((int)a[0], "long", "");
        return 0;
    // find_item(物件, 別名)：身上有這個別名的東西（原版再找隊友身上）；find_in_bag 只找身上，"#N" = 物件編號
    case 0x1003fab0: return find_in((int)a[0], S(a[1]));
    case 0x10054ed0:
        if ((h = find_in((int)a[0], S(a[1])))) return h;
        for (int m = team_next((int)a[0], 0); m; m = team_next((int)a[0], m))
            if ((h = find_in(m, S(a[1])))) return h;
        return 0;
    case 0x1005f320: case 0x1005f300:                                        // add_npc、add_item(自己, 路徑, 數量)
    case 0x10054880: case 0x1005f2e0:                                        // give_object(物件, 路徑, 數量)
        h = 0;
        for (int i = 0; i < (a[2] > 0 ? a[2] : 1); i++) h = story_new(S(a[1]), (int)a[0]);
        return h;
    case 0x1003d3e0: w_add_money((int)a[0], (int)a[1]); return 0;              // add_money
    case 0x1003d270: {                                                       // add_exp(人物, 值)：帶頭者的隊伍裡 Friend 2 的隊友各加同樣的經驗，再加本人
        int who = (int)a[0], lead = leader_of(who);
        for (int m = story_team_next(lead, 0); m; m = story_team_next(lead, m))
            if (m != who && story_get_int(m, "Friend") == 2) story_gain_exp(m, (int)a[1]);
        if (story_on_exp) story_on_exp(who, (int)a[1]);
        return 0;
    }
    case 0x1004dd00:                                                         // quest_log(誰, 種類, 顏色, 文字)
        if (story_on_log) story_on_log((int)a[0], (int)a[1], S(a[3]));
        else printf("【訊息 %d】%s\n", (int)a[1], S(a[3]));
        return 0;
    case 0x10023410: case 0x10023d20:                                        // new_str、set_str（原版 set 不存在時報錯，這裡直接建）
        story_set_str((int)a[0], S(a[1]), a[2] ? S(a[2]) : "");
        return 0;
    case 0x1005f3c0:                                                         // 升級參數 Upgrade_<+偏移>
        if (o) { char k[32]; snprintf(k, sizeof k, "Upgrade_%d", (int)a[1]); story_set_int(a[0], k, (int)a[2]); }  // 浮點傳進來已轉成整數
        return 0;  // shortcut: 原版依偏移存進數值表（0x20 = HP_MAX、0x24 = MP_MAX），只有主角用；主角數值引擎自己算
    case 0x1004dcc0: printf("【日誌】%s\n", S(a[2])); return 0;                 // journal
    case 0x10070360:                                                         // item_new(道具, 類型, 等級, SLevel 欄, 別名)
        if (o && story_item_make) o->item = story_item_make(a[0], a[1], a[2], a[3], S(a[4]));
        return 0;
    case 0x100521f0: {                                                       // drop_items(誰, 類型, 件數, PosY, 等級, 欄)：newitemex 放到身上
        n = (int)a[2];
        // 進貨時有上限（0x10478d3c）：身上總件數到 spawn_max 就不放。原版另外數名字叫 item_<類型> 的物件（0x1004acb0）跟
        // spawn_count 比，但產生的道具名字是道具表的別名，數到的都是 0，只有 spawn_count = 0 時才擋
        if (refreshing) {
            if (spawn_count < 0 && spawn_max < 0) return 0;
            if (spawn_count == 0 || (spawn_max > 0 && child_count((int)a[0]) >= spawn_max)) return 0;
        }
        for (int i = 0; i < n; i++) {
            h = obj_add(a[0]);
            if (!(objs[h].item = story_item_make ? story_item_make(h, a[1], a[4], a[5], NULL) : NULL)) story_take(h);
        }
        return 0;
    }
    // move_object(給誰, 物件, UI_Pos)：交出、拿到道具。shortcut: 原版的交易中（Trading）、類型 61 的特例還沒做
    case 0x1004bcf0:
        if (!O(a[1]) || O(a[1])->parent == a[0]) return 0;
        O(a[1])->parent = (int)a[0];
        kv_add(&O(a[1])->ints, &O(a[1])->nints, "UI_Pos")->val = a[2];
        return 1;
    case 0x1003da30:                                                         // 清掉身上的東西（商人進貨前）
        for (int i = 1; i < obj_count; i++)
            if (objs[i].parent == a[0] && objs[i].item) { free(objs[i].item); story_take(i); }
        return 0;
    // 進貨上限（0x1004c550 總件數、0x1004c5a0 同類型件數）；第 2、3 個參數 ≠ 0 時依隊伍人數 k 放大成 (k+3)/4 倍，一個人 = 原值
    case 0x1004c550: spawn_max = (int)a[0]; return 0;
    case 0x1004c5a0: spawn_count = (int)a[0]; return 0;
    case 0x1004c4f0:                                                         // InitSetMax(同類上限, 房間)：等級不限
        spawn_count = (int)a[0];
        spawn_level = -1;
        return 0;
    case 0x1004b0d0: return o && kv_find(o->ints, o->nints, "Amount") ? story_get_int(a[0], "Amount") : story_get_int(a[0], "Money");  // get_amount
    case 0x10052a00: return a[0] > 1 ? rand() % a[0] : 0;                   // rand_n
    case 0x1004ec80: story_dialog.store = 1; return 0;                       // 打開寄存介面(主角)
    case 0x10055990: story_dialog.shop = (int)a[1]; return 0;                 // 打開買賣介面(主角, 商人)

    // ---- 房間、出口 ----
    case 0x1005d8b0: exit_set((int)a[0], S(a[1]), S(a[2]), (int)a[3]); return 0;          // add_exit(房間, 方向, 路徑, 入口)
    case 0x1003d200:                                                         // ChangeExitInRoom(房間路徑, 方向, 路徑, 入口)
        if (!(h = room_at(S(a[0])))) { fprintf(stderr, "ChangeExitInRoom() 错误 (0) : 没有找到该场景索引<%s>!\n", S(a[0])); return 0; }
        exit_set(h, S(a[1]), S(a[2]), (int)a[3]);
        return 0;
    case 0x1003ee60: return (h = room_at(S(a[0]))) ? exit_enable(h, S(a[1]), (int)a[2]) : 0;  // EnableExitInRoom(房間路徑, 方向, 開關)
    case 0x1003ed10: return exit_enable((int)a[0], S(a[1]), (int)a[2]);       // 同上，給房間物件
    case 0x10052a20: {                                                       // random_exits(房間, 方向, 房間路徑)：Exit_<方向>_<n>
        if (!o || o->kind != 0) { fprintf(stderr, "RandomExit() : Error(0) - It is not a valid room object(%s).\n", o ? o->name : ""); return 0; }
        char k[128];
        snprintf(k, sizeof k, "Exit_%s_%d", S(a[1]), story_get_int(a[0], "RandomExitCount"));
        story_set_str((int)a[0], k, S(a[2]));
        e = kv_add(&o->ints, &o->nints, "RandomExitCount");
        e->val += 1;
        return 0;
    }
    case 0x100558c0: return w_current_room();                                  // current_room
    case 0x1003fe30: return find_path(S(a[0]));                              // find_room(路徑)：只找已經產生的
    case 0x1004db90: return load_room(S(a[1]));                              // load_room(誰, 路徑)
    case 0x1003fd90: return w_find_alias(S(a[0]), (int)a[1]);                  // w_find_alias(別名, 1 = 目前房間)
    case 0x1003ff70:                                                         // 依路徑找玩家（0x104797c0 玩家表）
        return O(the_player) && !strcmp(objs[the_player].path, S(a[0])) ? the_player : 0;
    case 0x10054960:                                                         // place_npc(房間路徑, 物件路徑, 數量)
        if (!(h = room_at(S(a[0])))) { fprintf(stderr, "SetObjectInRoom() 错误 (0) : 没有找到该场景索引<%s>!\n", S(a[0])); return 0; }
        for (int i = 0; i < (a[2] > 0 ? a[2] : 1); i++) story_new(S(a[1]), h);
        return 0;
    case 0x100549d0:                                                         // SetObjectToNPC(別名, 房間路徑, 物件路徑, 數量)
        if (!(h = room_at(S(a[1])))) { fprintf(stderr, "SetObjectToNPC() 错误 (0) : 没有找到该场景索引<%s>!\n", S(a[1])); return 0; }
        if ((h = find_in(h, S(a[0])))) for (int i = 0; i < (a[3] > 0 ? a[3] : 1); i++) story_new(S(a[2]), h);
        return 0;
    case 0x1003e5d0:                                                         // DelObjectFromRoom(房間路徑, 別名, 數量)：連身上的東西一起刪
        if (!(h = room_at(S(a[0])))) { fprintf(stderr, "DelObjectFromRoom() 错误 (0) : 没有找到该场景索引<%s>!\n", S(a[0])); return 0; }
        for (int i = 0; i < a[2]; i++) {
            int x = find_in(h, S(a[1]));
            if (!x) break;
            for (int c; (c = story_next(x, 0));) { obj_delete(c); if (O(c) && objs[c].parent == x) break; }
            obj_delete(x);
        }
        return 0;
    case 0x1004e0d0: {                                                       // MoveObjectBetweenRoom(房間路徑, 別名, 目的路徑, 入口)
        if (!(h = room_at(S(a[0])))) { fprintf(stderr, "MoveObjectBetweenRoom() 错误 (0) : 没有找到场景索引<%s>!\n", S(a[0])); return 0; }
        int dest = room_at(S(a[2])), x;
        while ((x = find_in(h, S(a[1])))) {
            story_set_int(x, "G_PosX", -100);
            story_set_int(x, "G_PosY", (int)a[3]);
            if (!dest || !put_in(x, dest)) break;
        }
        return 0;
    }
    case 0x1004e1b0:                                                         // (目的路徑, 物件, 入口)：搬到那個房間的入口
        story_set_int(a[1], "G_PosX", -100);
        story_set_int(a[1], "G_PosY", (int)a[2]);
        if ((h = room_at(S(a[0])))) {
            if (a[1] == the_player) story_enter(h);
            else put_in((int)a[1], h);
        }
        return 0;
    case 0x1004b6c0:                                                         // GetObjectCountInRoom(房間路徑, 物件路徑)
        if (!(h = room_at(S(a[0])))) return -1;
        // fallthrough
    case 0x1004aac0:                                                         // 容器裡路徑相符、UI_Pos < 200 的件數
        if (addr == 0x1004aac0) h = (int)a[0];
        n = 0;
        for (int x = story_next(h, 0); x; x = story_next(h, x))
            if (!strcmp(objs[x].path, S(a[1])) && story_get_int(x, "UI_Pos") < 200) n++;
        return n;
    case 0x1004abd0: return count_alias((int)a[0], S(a[1]));                 // 身上別名相符的件數
    case 0x1004ad90:                                                         // 連隊員
        n = count_alias((int)a[0], S(a[1]));
        for (int m = team_next((int)a[0], 0); m; m = team_next((int)a[0], m)) n += count_alias(m, S(a[1]));
        return n;
    case 0x1003e160: remove_items((int)a[0], S(a[1]), (int)a[2]); return 0;   // 從身上刪掉 n 件
    case 0x1003e920: obj_delete((int)a[0]); return 0;                         // DelObject
    case 0x1004a940:                                                         // GetAttrAtRoom(房間路徑, 別名, 屬性)
        if (!(h = room_at(S(a[0])))) return -1;
        return (h = find_in(h, S(a[1]))) ? story_get_int(h, S(a[2])) : -1;
    case 0x1003c9f0:                                                         // ChangeAttrAtRoom(房間路徑, 別名, 屬性, 值)
        if ((h = room_at(S(a[0]))) && (h = find_in(h, S(a[1])))) w_set_attr(h, S(a[2]), (int)a[3]);
        return 0;
    case 0x1003ce20:                                                         // ChangeAttrStrAtRoom(房間路徑, 別名, 屬性, 字串)
        if ((h = room_at(S(a[0]))) && (h = find_in(h, S(a[1])))) { story_set_str(h, S(a[2]), S(a[3])); attr_changed(h, S(a[2])); }
        return 0;
    case 0x1003cd10:                                                         // ChangeAttrPtr(別名, 屬性, 值, 房間)
    case 0x1003cbf0:                                                         // set_int_alias(別名, 屬性, 值, 房間路徑)
        h = addr == 0x1003cd10 ? (int)a[3] : room_arg(S(a[3]));
        for (int x = story_next(h, 0); x; x = story_next(h, x))
            if (alias_match(objs[x].alias, S(a[0]))) w_set_attr(x, S(a[1]), (int)a[2]);
        return 0;
    case 0x1003c550:                                                         // 改 G_AIGroup(別名, 組, 房間路徑, 全部)
        h = room_arg(S(a[2]));
        for (int x = story_next(h, 0); x; x = story_next(h, x))
            if (alias_match(objs[x].alias, S(a[0]))) {
                story_set_int(x, "G_AIGroup_Save", story_get_int(x, "G_AIGroup"));
                story_set_int(x, "G_AIGroup", (int)a[1]);
                msg(0x51c, NULL, 2, x, a[1]);
                if (!a[3]) break;
            }
        return 0;

    // ---- 屬性 ----
    case 0x1003c6a0: w_set_attr((int)a[0], S(a[1]), (int)a[2]); return 0;      // set_attr（加上去）
    case 0x1003ca70:                                                         // 六屬性改了要重算（0x1003b390、0x1002d8d0）
        w_set_attr((int)a[0], S(a[1]), (int)a[2]);
        msg(0x2d8d0, NULL, 1, a[0]);
        return 0;
    case 0x1003d380:                                                         // 加 HP；到 0 以下就死（0x100574e0）
        if (story_get_int(a[0], "HP") + a[1] < 1) {
            story_set_int(a[0], "HP", story_get_int(a[0], "HP") + (int)a[1]);
            msg(0x574e0, NULL, 1, a[0]);
            return 0;
        }
        w_set_attr((int)a[0], "HP", (int)a[1]);
        return 0;
    case 0x1004e640:                                                         // 設 G_NPCActive 並通知
        story_set_int(a[0], "G_NPCActive", (int)a[1]);
        msg(0x51b, NULL, 3, a[0], the_player, a[1]);
        return 0;
    case 0x10054810: msg(0x522, NULL, 3, a[0], (0x42 << 16) | (a[1] == 1), -1); return 0;  // 狀態 0x42 開關（0x10041930）
    case 0x1004d180: {                                                       // UIS_P 第 n 個旗標（每 7 個一個字元）
        char s[1024] = {0};
        snprintf(s, sizeof s, "%s", story_get_str(a[0], "UIS_P"));
        int i = (int)a[1] / 7;
        if (i < 1023) s[i] |= (char)(1 << ((i * 7 - a[1] + 6) & 31));
        story_set_str((int)a[0], "UIS_P", s);
        msg(0x48e20, NULL, 1, a[0]);
        return 0;
    }

    // ---- 人物、怪物 ----
    case 0x10052550: return w_spawn((int)a[0], S(a[1]), (int)a[2], (int)a[3], (int)a[4], (int)a[5], (int)a[6]);  // spawn
    case 0x100528d0: return spawn_exact((int)a[0], S(a[1]), (int)a[2], (int)a[4], (int)a[5], (int)a[6]);
    case 0x100730c0: return story_on_person ? story_on_person((int)a[0], S(a[1]), (int)a[2], (int)a[3]) : 0;  // 依怪物表設數值
    case 0x100757a0: return story_on_skill ? story_on_skill((int)a[0], S(a[1]), (int)a[2]) : 0;              // 依技能表設數值
    case 0x1005f390: {                                                       // SetSkill(人物, 技能路徑, 等級)：學會技能
        if (!o || o->kind != 1) return 0;
        h = story_new(S(a[1]), (int)a[0]);
        story_set_int(h, "Level", (int)a[2]);
        return h;
    }

    // ---- 隊伍 ----
    case 0x10037850: team_add((int)a[0], (int)a[1]); return 0;               // AddMember(帶頭者, 隊員)
    case 0x10053b40: team_leave((int)a[0]); return 0;                         // RemoveMember
    case 0x10053e10:                                                         // 路徑相符的隊員都離隊
        for (int m = team_next((int)a[0], 0); m; m = team_next((int)a[0], m))
            if (!strcmp(objs[m].path, S(a[1]))) team_leave(m);
        return 0;
    case 0x10054f90: return leader_of((int)a[0]);                            // 帶頭者
    case 0x1004b020:                                                         // 隊伍人數（Friend = 種類；−1 全部）
        if (!o || o->kind != 1) return 0;
        if (a[1] == -1) return team_size((int)a[0]);
        n = 0;
        for (int m = team_next((int)a[0], 0); m; m = team_next((int)a[0], m)) n += story_get_int(m, "Friend") == a[1];
        return n;
    case 0x1004c6d0:                                                         // b 在 a 的隊伍裡（a = 0：b 有隊伍）
        if (!O(a[1]) || objs[a[1]].kind != 1) return 0;
        if (!a[0]) return O(objs[a[1]].leader) ? 1 : 0;
        if (a[0] == a[1]) return 1;
        return O(a[1]) && objs[a[1]].leader == a[0];
    // 隊員依序：迭代器 *a[1] = 下一個（0 = 沒了），跟原版串列節點一樣，劇情先看迭代器是不是 0 再叫下一個
    case 0x100406a0:
        h = team_next((int)a[0], 0);
        *(int *)a[1] = h ? team_next((int)a[0], h) : 0;
        return h;
    case 0x1004e4c0:
        h = *(int *)a[1];
        *(int *)a[1] = h ? team_next((int)a[0], h) : 0;
        return h;
    case 0x1003fc80:                                                         // 主角隊伍裡別名開頭相符的（"#N" 也可以）
        if (S(a[0])[0] == '#') return O(atoi(S(a[0]) + 1)) && objs[atoi(S(a[0]) + 1)].kind == 1 ? atoi(S(a[0]) + 1) : 0;
        for (int m = team_next(the_player, 0); m; m = team_next(the_player, m))
            if (alias_match(objs[m].alias, S(a[0]))) return m;
        return 0;

    // ---- 劇情事件、畫面 ----
    case 0x1004f1a0: w_start_plot(S(a[0])); return 0;                          // start_plot
    case 0x10058d30: story_talk((int)a[0]); return 0;                         // 跟 NPC 說話
    case 0x1004e200: msg(0x532, NULL, 5, a[0], a[1], a[2], a[3], a[4]); return 0;  // 人物位置／鏡頭（Poq 0x532）
    case 0x10054630: msg(0x533, NULL, 3, a[0], a[1], a[2]); return 0;         // 畫面效果（Poq 0x533：1 = 淡入淡出、2 = 震動）
    case 0x1003d0b0:                                                         // 章節(主角, 章, 節) → 0x53a、@poqdata
        if (a[0] != the_player) return 0;
        story_set_int(a[0], "Chapter", (int)a[1]);
        msg(0x53a, NULL, 3, a[0], a[1], a[2]);
        return 0;
    case 0x10056640: msg(0x56640, NULL, 3, a[0], a[1], a[2]); return 0;       // "@poqdata a b"（只有主角）
    case 0x10055670: msg(0x3f8, S(a[0]), 1, a[1]); return 0;                 // 廣播（easyrpg.dlg 0x3f8）
    case 0x10055ae0: msg(0x607, NULL, 2, a[0], a[1]); return 0;
    case 0x1003a880: msg(0x629, NULL, 2, a[0], a[1]); return 1;
    case 0x1003daf0:                                                         // 清掉 Chosen（自己和 Friend > 1 的隊員）並送 0x60b
        if (story_get_int(a[0], "Chosen") > 0) story_set_int(a[0], "Chosen", 0);
        for (int m = team_next((int)a[0], 0); m; m = team_next((int)a[0], m))
            if (story_get_int(m, "Friend") > 1) story_set_int(m, "Chosen", 0);
        msg(0x60b, NULL, 3, story_get_int(a[0], "Running") < 0 ? 0 : story_get_int(a[0], "Running"), 1, a[0]);
        return 0;
    case 0x10048790: {                                                       // 地圖開了一塊（MapUpdate 加上 n，送 0x605）
        char s[1024], w[32];
        snprintf(s, sizeof s, "%s", story_get_str(a[0], "MapUpdate"));
        for (char *p = s; sscanf(p, "%31s", w) == 1; p = strstr(p, w) + strlen(w))
            if (atoi(w) == a[1]) return 0;
        size_t L = strlen(s);
        snprintf(s + L, sizeof s - L, " %d", (int)a[1]);
        story_set_str((int)a[0], "MapUpdate", s);
        msg(0x605, NULL, 3, a[0], a[1], a[2]);
        return 0;
    }
    case 0x10040760:                                                         // 清掉日誌（Log_*、LogCount），送 0x61d
        if (!o) return 0;
        for (int i = 0; i < o->nstrs; i++)
            if (alias_match(o->strs[i].key, "Log_")) o->strs[i].key[0] = 0;
        for (int i = 0; i < o->nints; i++)
            if (!strcmp(o->ints[i].key, "LogCount")) o->ints[i].key[0] = 0;
        msg(0x61d, NULL, 1, a[0]);
        return 0;
    case 0x1004ea10:                                                         // 建築狀態（寶箱、礦）：isbuilding 2 → 1、6 → 5，或打開（0x1004e680）
        msg(0x4ea10, S(a[1]), 3, a[0], a[2], a[3]);
        return 0;

    // ---- 基底的第 1 格（衍生的寶箱、礦石類別先呼叫）----
    case 0x100fa310: chest_base(a[0], o, "秦殇/NET/宝箱", "宝箱", "boxes"); return 0;  // 秦殇/NET/宝箱（同一份基底屬性）
    case 0x100f9630: case 0x100f95f0: case 0x100fa200: {                     // 随机矿石、随机木材树、NET 随机物品宝箱
        static const char *const T[3][3] = {{"秦殇/随机矿石", "矿石", "suijikuangshi"}, {"秦殇/随机木材树", "树", "random_tree"},
                                            {"秦殇/NET/随机物品宝箱", "箱子", "netsuijiwupinbaoxiang"}};
        int k = addr == 0x100f9630 ? 0 : addr == 0x100f95f0 ? 1 : 2;
        chest_base(a[0], o, T[k][0], T[k][1], T[k][2]);
        if (o) { snprintf(o->path, sizeof o->path, "%s", T[k][0]); snprintf(o->name, sizeof o->name, "%s", T[k][1]); snprintf(o->alias, sizeof o->alias, "%s", T[k][2]); }
        story_set_str((int)a[0], "long", "");
        return 0;
    }
    case 0x100f9570:                                                         // 秦殇/金钱：類型 60 的道具
        if (o) snprintf(o->path, sizeof o->path, "秦殇/金钱");
        if (o && story_item_make) o->item = story_item_make(a[0], 0x3c, 1, -1, "");
        story_set_int(a[0], "Amount", 0);
        return 0;

    // ---- 連線版（幫派）：Job 只由幫派指令寫（0x1004xxxx 任命、升降），單機沒有幫派，Job 恆為 0 ----
    case 0x10039b60:                                                         // 撤銷幫派：Job < 9 → 「你不是掌门人不能撤销门派」
        sysmsg((int)a[0], 0x10, 0xbf4);
        return 0;                                                            // shortcut: Job ≥ 9 之後（BAPA 比對、解散幫派檔）單機到不了
    case 0x10038f90: {                                                       // 天王令牌升幫派（玩家, 模式, 值）：Job ≥ 5、錢 ≥ 40 萬、身上有令牌
        int p = (int)a[0];
        if (story_get_int(p, "Job") < 5) { sysmsg(p, 0x10, 0xbf7); return 0; }
        if (story_get_int(p, "Money") < 400000) { sysmsg(p, 0x10, 0xc13); return 0; }
        int tok = find_in(p, "tianwanglingpai");
        if (!tok) { sysmsg(p, 0x10, 0xc14); return 0; }
        // shortcut: 模式 1 要讀幫派檔（BAPA 的 .bp，0x1000f810／0x1000fde0 改人數上限），單機沒有幫派檔：回「没有找到门派信息」(0xbf5) 不扣錢
        if (a[1] == 1) { sysmsg(p, 0x10, 0xbf5); return 0; }
        w_add_money(p, -400000);
        story_delete(tok);                                                   // 令牌用掉（0x1004df20 移出身上、0x1003e920 刪）
        return 0;
    }
    case 0x1004d500: return 0;  // 令牌（Type 0x3d）換手的訊息：shortcut: 只有連線版的天王令牌用到
    }
    Fn *f = fn_find(addr);  // 劇情呼叫別的類別的方法（例如衍生類別先跑基底的第 1 格）
    if (f) return f->fn(a);
    static uint32_t seen[4096];
    static int nseen;
    for (int i = 0; i < nseen; i++)
        if (seen[i] == addr) return 0;
    if (nseen < 4096) seen[nseen++] = addr;
    fprintf(stderr, "劇情：還沒翻的原版函式 0x%08x\n", addr);
    return 0;
}

void *ExceptionList;

// ---- 存檔：所有物件照順序寫出（代號不變，引擎 inv 的 sh 照樣對得上）；道具是 item_size 大小的整塊（Item 沒有指標）----
static void put_str(FILE *f, const char *s) { int n = (int)strlen(s); fwrite(&n, 4, 1, f); fwrite(s, 1, n, f); }
static int get_str(FILE *f, char *out, int cap) {
    int n = 0;
    if (fread(&n, 4, 1, f) != 1 || n < 0 || n >= cap || fread(out, 1, n, f) != (size_t)n) return 0;
    out[n] = 0;
    return 1;
}
static void put_kv(FILE *f, const KV *v, int n, int str) {
    int m = 0;
    for (int i = 0; i < n; i++) m += v[i].key[0] != 0;
    fwrite(&m, 4, 1, f);
    for (int i = 0; i < n; i++) {
        if (!v[i].key[0]) continue;  // 刪掉的屬性
        put_str(f, v[i].key);
        if (str) put_str(f, S(v[i].val));
        else { int x = (int)v[i].val; fwrite(&x, 4, 1, f); }
    }
}
static int get_kv(FILE *f, KV **v, int *n, int str) {
    int m = 0;
    if (fread(&m, 4, 1, f) != 1 || m < 0) return 0;
    *v = calloc(m ? m : 1, sizeof **v);
    *n = m;
    for (int i = 0; i < m; i++) {
        char s[4096];
        if (!get_str(f, (*v)[i].key, sizeof (*v)[i].key)) return 0;
        if (str) { if (!get_str(f, s, sizeof s)) return 0; (*v)[i].val = (intptr_t)strdup(s); }
        else { int x; if (fread(&x, 4, 1, f) != 1) return 0; (*v)[i].val = x; }
    }
    return 1;
}

void story_save(FILE *f, size_t item_size) {
    fwrite(&obj_count, 4, 1, f);
    fwrite(&the_player, 4, 1, f);
    for (int h = 1; h < obj_count; h++) {
        Obj *o = &objs[h];
        put_str(f, o->cls ? o->cls->path : "");
        put_str(f, o->path); put_str(f, o->name); put_str(f, o->alias);
        int hdr[6] = {o->parent, o->held, o->item != NULL, o->kind, o->leader, o->nexits};
        fwrite(hdr, 4, 6, f);
        for (int i = 0; i < o->nexits; i++) {
            put_str(f, o->exits[i].dir); put_str(f, o->exits[i].path);
            fwrite(&o->exits[i].entry, 4, 1, f); fwrite(&o->exits[i].on, 4, 1, f);
        }
        put_kv(f, o->ints, o->nints, 0);
        put_kv(f, o->flags, o->nflags, 0);
        put_kv(f, o->strs, o->nstrs, 1);
        if (o->item) fwrite(o->item, item_size, 1, f);
    }
}

static void objs_free(void) {
    for (int h = 1; h < obj_count; h++) {
        for (int i = 0; i < objs[h].nstrs; i++) free((void *)objs[h].strs[i].val);
        free(objs[h].ints); free(objs[h].flags); free(objs[h].strs); free(objs[h].item); free(objs[h].exits);
    }
    obj_count = 1;
}

// 讀回 story_save 寫的；壞檔回 0（物件已清掉，呼叫的人要當新遊戲處理）
int story_load(FILE *f, size_t item_size) {
    int n = 0, pl = 0;
    if (fread(&n, 4, 1, f) != 1 || fread(&pl, 4, 1, f) != 1 || n < 1) return 0;
    objs_free();
    memset(&story_dialog, 0, sizeof story_dialog);
    memset(&story_plot, 0, sizeof story_plot);
    for (int h = 1; h < n; h++) {
        obj_add(0);
        Obj *o = &objs[h];
        char cls[128];
        int hdr[6];
        if (!get_str(f, cls, sizeof cls) || !get_str(f, o->path, sizeof o->path) || !get_str(f, o->name, sizeof o->name) ||
            !get_str(f, o->alias, sizeof o->alias) || fread(hdr, 4, 6, f) != 6 || hdr[5] < 0 || hdr[5] > 64) { objs_free(); return 0; }
        o->exits = calloc(hdr[5] ? hdr[5] : 1, sizeof *o->exits);
        o->nexits = hdr[5];
        for (int i = 0; i < hdr[5]; i++)
            if (!get_str(f, o->exits[i].dir, sizeof o->exits[i].dir) || !get_str(f, o->exits[i].path, sizeof o->exits[i].path) ||
                fread(&o->exits[i].entry, 4, 1, f) != 1 || fread(&o->exits[i].on, 4, 1, f) != 1) { objs_free(); return 0; }
        if (!get_kv(f, &o->ints, &o->nints, 0) || !get_kv(f, &o->flags, &o->nflags, 0) || !get_kv(f, &o->strs, &o->nstrs, 1)) { objs_free(); return 0; }
        o->cls = cls[0] ? class_find(cls) : NULL;
        o->parent = hdr[0];
        o->held = hdr[1];
        o->kind = hdr[3];
        o->leader = hdr[4];
        if (hdr[2] && (!(o->item = malloc(item_size)) || fread(o->item, item_size, 1, f) != 1)) { objs_free(); return 0; }
    }
    the_player = pl;
    return 1;
}
