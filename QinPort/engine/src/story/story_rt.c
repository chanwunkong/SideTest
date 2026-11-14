// 劇情執行環境：照原版 EasyRPG.dll 的物件模型，給 tools/storytool.py 產生的劇情檔呼叫。
// 物件用小整數代號（1 起）當 this；劇情裡的 (int)this 轉型因此不會出錯。
// 引擎函式依原版位址分派（engine_call）；還沒翻的第一次被呼叫時印出位址，之後照原版補。
#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
typedef struct {
    const Class *cls;
    char path[128], name[64], alias[64];
    KV *ints, *flags, *strs;  // strs 的 val = strdup 的字串
    int nints, nflags, nstrs;
    int parent;  // 在誰身上或哪個房間
    void *item;  // 道具物件在引擎這邊的 Item（story_item_make 產生）
    int held;    // 道具在引擎的 inv 裡（item 交給引擎了）
} Obj;
static int refreshing, spawn_max = -1, spawn_count = -1;  // 原版 0x10478d3c、0x1039f548、0x1039f544
static Obj *objs;  // objs[0] 不用：代號 0 = 沒有
static int obj_count = 1, obj_cap;
static int the_player;

static Obj *O(intptr_t h) { return h > 0 && h < obj_count ? &objs[h] : NULL; }

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

static intptr_t slot_call(intptr_t h, int slot, intptr_t arg) {
    Obj *o = O(h);
    if (!o || !o->cls) return 0;
    for (int i = 0; i < o->cls->n; i++)
        if (o->cls->slots[i].slot == slot) return o->cls->slots[i].fn((intptr_t[8]){h, arg});
    return 0;
}

static int obj_add(int parent) {
    if (obj_count >= obj_cap) objs = realloc(objs, (obj_cap = obj_cap ? obj_cap * 2 : 1024) * sizeof *objs);
    int h = obj_count++;
    memset(&objs[h], 0, sizeof objs[h]);
    objs[h].parent = parent;
    return h;
}

// 產生一個物件：照原版先呼叫第 1 格（設定路徑、名稱、別名、屬性）
int story_new(const char *path, int parent) {
    int h = obj_add(parent);
    Obj *o = &objs[h];
    o->cls = class_find(path);
    snprintf(o->path, sizeof o->path, "%s", path);
    if (!o->cls) fprintf(stderr, "劇情：找不到物件 %s\n", path);
    slot_call(h, 1, 0);
    return h;
}

int story_player(void) {
    if (!the_player) the_player = story_new("秦殇/主角/游侠扶苏", 0);  // 劇情模式的主角（遊俠扶蘇）
    return the_player;
}

const char *story_name(int h) { return O(h) ? O(h)->name : ""; }

void (*story_on_exp)(int h, int exp);
void (*story_on_log)(int h, int kind, const char *text);
void *(*story_item_make)(int h, int type, int level, int col, const char *alias);

int story_next(int owner, int after) {
    for (int i = after + 1; i < obj_count; i++)
        if (objs[i].parent == owner) return i;
    return 0;
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
void story_set_int(int h, const char *key, int v) { if (O(h)) kv_add(&O(h)->ints, &O(h)->nints, key)->val = v; }

// 進房間（依別名，= 場景檔名，例如 xy_hs2）：每個房間只產生一次（原版房間一直存在，NPC 和旗標跟著留著）；
// 第 1 格會用 add_npc 放進 NPC。找不到回 0
int story_room(const char *alias) {
    for (int i = 1; i < obj_count; i++)
        if (objs[i].cls && objs[i].cls->kind == 0 && !strcmp(objs[i].cls->alias, alias)) return i;
    for (int i = 0; i < class_count; i++)
        if (classes[i].kind == 0 && !strcmp(classes[i].alias, alias)) return story_new(classes[i].path, 0);
    return 0;
}

// 房間刷新（原版 0x10052e40 每隔一段時間輪流對沒有玩家的房間呼叫第 6 格）：房間裡的物件有第 6 格的就呼叫（商店進貨等）。
// shortcut: 原版的週期沒算，改成每次進房間刷新一次（離開期間＝沒有玩家）
void story_refresh(int room) {
    for (int h = story_next(room, 0); h; h = story_next(room, h)) {
        const Class *c = objs[h].cls;
        for (int i = 0; c && i < c->n; i++)
            if (c->slots[i].slot == 6) {
                refreshing = 1;
                c->slots[i].fn((intptr_t[8]){h});
                refreshing = 0;
                break;
            }
    }
}

// 新遊戲的開場房間別名（登記時第 4 個參數 = 1 的房間）
const char *story_start_room(void) {
    for (int i = 0; i < class_count; i++)
        if (classes[i].kind == 0 && classes[i].start) return classes[i].alias;
    return NULL;
}

// 房間裡別名是 alias 的物件（場景 .ONT 的「事件ID」= 劇情 NPC 的別名）
int story_find(int room, const char *alias) {
    for (int i = 1; i < obj_count; i++)
        if (objs[i].parent == room && !strcmp(objs[i].alias, alias)) return i;
    return 0;
}

// ---- 對話：原版用 0x5dd 訊息把說話者、台詞、選項送給畫面 ----
StoryDialog story_dialog;

static void dialog_line(const char *s) {
    size_t n = strlen(story_dialog.text);
    snprintf(story_dialog.text + n, sizeof story_dialog.text - n, "%s%s", n ? "\n" : "", s);
}

// 跟 NPC 說話（原版 0x10058d30）：先送名字，再呼叫第 17 格（參數 1）
void story_talk(int npc) {
    memset(&story_dialog, 0, sizeof story_dialog);
    story_dialog.npc = npc;
    snprintf(story_dialog.speaker, sizeof story_dialog.speaker, "%s", story_name(npc));
    slot_call(npc, 17, 1);
}

// 選第 i 個回答：清掉畫面上的台詞和選項，呼叫選項登記的處理
void story_choose(int i) {
    if (i < 0 || i >= story_dialog.option_count) return;
    StoryOption op = story_dialog.options[i];
    story_dialog.text[0] = 0;
    story_dialog.option_count = 0;
    if (op.fn) op.fn((intptr_t[8]){story_dialog.npc});
    else if (fn_find(op.addr)) story_call_addr(op.addr, (intptr_t[8]){story_dialog.npc});
    else fprintf(stderr, "劇情：選項 %s 指到原版基底 0x%08x，還沒翻\n", op.key, op.addr);
}

// ---- 引擎函式（依原版位址）----
static const char *S(intptr_t p) { return p ? (const char *)p : ""; }

// say 的格式字串最多帶 7 個參數；%s 以外都當整數
static void say_fmt(intptr_t *a) {
    char out[1024];
    snprintf(out, sizeof out, S(a[0]), a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    dialog_line(out);
}

intptr_t engine_call(uint32_t addr, intptr_t *a) {
    static int trace = -1;
    if (trace < 0) trace = getenv("STORY_TRACE") != NULL;
    if (trace) fprintf(stderr, "[0x%08x %ld %ld %ld]\n", addr, (long)a[0], (long)a[1], (long)a[2]);
    Obj *o = O(a[0]);
    KV *e;
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
    case 0x10023c60: if (o) snprintf(o->path, sizeof o->path, "%s", S(a[1])); return 0;  // set_path
    case 0x10023cf0: if (o) snprintf(o->name, sizeof o->name, "%s", S(a[1])); return 0;  // set_name
    case 0x10023c40: if (o) snprintf(o->alias, sizeof o->alias, "%s", S(a[1])); return 0;  // set_alias
    case 0x1005d390: case 0x1005cd60: case 0x1005cab0: case 0x1005d4a0: case 0x100f94c0: case 0x100f95b0: case 0x1005cd40:
        return 0;  // 各種基底初始化。shortcut: 基底的預設屬性還沒照原版設
    // find_item(物件, 別名)：身上有這個別名的東西（原版再找隊友身上）；find_in_bag 只找身上，"#N" = 物件編號
    case 0x1003fab0:
        if (S(a[1])[0] == '#') return O(atoi(S(a[1]) + 1)) ? atoi(S(a[1]) + 1) : 0;
        // fallthrough
    case 0x10054ed0:
        for (int i = 1; i < obj_count; i++)
            if (objs[i].parent == a[0] && !strcmp(objs[i].alias, S(a[1]))) return i;
        return 0;
    case 0x1005f320: case 0x1005f300:                                        // add_npc、add_item(自己, 路徑, 數量)
    case 0x10054880:                                                         // give_object(物件, 路徑, 數量)
        for (int i = 0; i < (a[2] > 0 ? a[2] : 1); i++) story_new(S(a[1]), (int)a[0]);
        return 0;
    case 0x1003d3e0:                                                         // add_money：原版超過 5 億或負數歸零
        if (o) {
            e = kv_add(&o->ints, &o->nints, "Money");
            e->val += a[1];
            if (e->val < 0 || e->val > 499999999) e->val = 0;
        }
        return 0;
    case 0x1003d270: if (story_on_exp) story_on_exp((int)a[0], (int)a[1]); return 0;  // add_exp(主角, 值)：shortcut: 隊友（Friend 2）還沒有
    case 0x1004dd00:                                                         // quest_log(誰, 種類, 顏色, 文字)
        if (story_on_log) story_on_log((int)a[0], (int)a[1], S(a[3]));
        else printf("【訊息 %d】%s\n", (int)a[1], S(a[3]));
        return 0;
    case 0x10023410: case 0x10023d20:                                        // new_str、set_str（原版 set 不存在時報錯，這裡直接建）
        if (o) { e = kv_add(&o->strs, &o->nstrs, S(a[1])); free((void *)e->val); e->val = (intptr_t)strdup(a[2] ? S(a[2]) : ""); }
        return 0;
    case 0x1005f3c0: return 0;  // 設 NPC 升級參數 Upgrade_<鍵>。shortcut: 劇情 NPC 還沒有自己的數值（戰鬥用怪物表）
    case 0x1004dcc0: printf("【日誌】%s\n", S(a[2])); return 0;                 // journal
    case 0x10070360:                                                         // item_new(道具, 類型, 等級, SLevel 欄, 別名)
        if (o && story_item_make) o->item = story_item_make(a[0], a[1], a[2], a[3], S(a[4]));
        return 0;
    case 0x100521f0: {                                                       // drop_items(誰, 類型, 件數, PosY, 等級, 欄)：newitemex 放到身上
        int n = (int)a[2];
        // 進貨時有上限（0x10478d3c）：身上總件數到 spawn_max 就不放。原版另外數名字叫 item_<類型> 的物件（0x1004acb0）跟
        // spawn_count 比，但產生的道具名字是道具表的別名，數到的都是 0，只有 spawn_count = 0 時才擋
        if (refreshing) {
            if (spawn_count < 0 && spawn_max < 0) return 0;
            int all = 0;
            for (int h = story_next(a[0], 0); h; h = story_next(a[0], h)) all++;
            if (spawn_count == 0 || (spawn_max > 0 && all >= spawn_max)) return 0;
        }
        for (int i = 0; i < n; i++) {
            int h = obj_add(a[0]);
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
    case 0x1004b0d0: return o && kv_find(o->ints, o->nints, "Amount") ? story_get_int(a[0], "Amount") : story_get_int(a[0], "Money");  // get_amount
    case 0x10052a00: return a[0] > 1 ? rand() % a[0] : 0;                   // rand_n
    case 0x1004ec80: story_dialog.store = 1; return 0;                       // 打開寄存介面(主角)
    case 0x10055990: story_dialog.shop = (int)a[1]; return 0;                 // 打開買賣介面(主角, 商人)
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
    fwrite(&n, 4, 1, f);
    for (int i = 0; i < n; i++) {
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
        int hdr[3] = {o->parent, o->held, o->item != NULL};
        fwrite(hdr, 4, 3, f);
        put_kv(f, o->ints, o->nints, 0);
        put_kv(f, o->flags, o->nflags, 0);
        put_kv(f, o->strs, o->nstrs, 1);
        if (o->item) fwrite(o->item, item_size, 1, f);
    }
}

static void objs_free(void) {
    for (int h = 1; h < obj_count; h++) {
        for (int i = 0; i < objs[h].nstrs; i++) free((void *)objs[h].strs[i].val);
        free(objs[h].ints); free(objs[h].flags); free(objs[h].strs); free(objs[h].item);
    }
    obj_count = 1;
}

// 讀回 story_save 寫的；壞檔回 0（物件已清掉，呼叫的人要當新遊戲處理）
int story_load(FILE *f, size_t item_size) {
    int n = 0, pl = 0;
    if (fread(&n, 4, 1, f) != 1 || fread(&pl, 4, 1, f) != 1 || n < 1) return 0;
    objs_free();
    memset(&story_dialog, 0, sizeof story_dialog);
    for (int h = 1; h < n; h++) {
        obj_add(0);
        Obj *o = &objs[h];
        char cls[128];
        int hdr[3];
        if (!get_str(f, cls, sizeof cls) || !get_str(f, o->path, sizeof o->path) || !get_str(f, o->name, sizeof o->name) ||
            !get_str(f, o->alias, sizeof o->alias) || fread(hdr, 4, 3, f) != 3 || !get_kv(f, &o->ints, &o->nints, 0) ||
            !get_kv(f, &o->flags, &o->nflags, 0) || !get_kv(f, &o->strs, &o->nstrs, 1)) { objs_free(); return 0; }
        o->cls = cls[0] ? class_find(cls) : NULL;
        o->parent = hdr[0];
        o->held = hdr[1];
        if (hdr[2] && (!(o->item = malloc(item_size)) || fread(o->item, item_size, 1, f) != 1)) { objs_free(); return 0; }
    }
    the_player = pl;
    return 1;
}
