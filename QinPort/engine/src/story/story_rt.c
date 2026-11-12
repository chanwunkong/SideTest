// 劇情執行環境：照原版 EasyRPG.dll 的物件模型，給 tools/storytool.py 產生的劇情檔呼叫。
// 物件用小整數代號（1 起）當 this；劇情裡的 (int)this 轉型因此不會出錯。
// 引擎函式依原版位址分派（engine_call）；還沒翻的第一次被呼叫時印出位址，之後照原版補。
#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- 類別登記（每個劇情檔一個）----
typedef struct { const char *path, *cls; int kind, n; const StorySlot *slots; } Class;
static Class *classes;
static int class_count, class_cap;

void story_register(const char *path, int kind, const char *cls, const StorySlot *slots, int n) {
    if (class_count == class_cap) classes = realloc(classes, (class_cap = class_cap ? class_cap * 2 : 4096) * sizeof *classes);
    classes[class_count++] = (Class){path, cls, kind, n, slots};
}

static const Class *class_find(const char *path) {
    for (int i = 0; i < class_count; i++)
        if (!strcmp(classes[i].path, path)) return &classes[i];
    return NULL;
}

// ---- 物件：具名整數屬性（CERObject::SetInt/NewInt）、旗標（CERPerson::NewFlag，最多 256 個）、裝的東西 ----
typedef struct { char key[32]; intptr_t val; } KV;
typedef struct {
    const Class *cls;
    char path[128], name[64], alias[64];
    KV *ints, *flags;
    int nints, nflags;
    int parent;  // 在誰身上或哪個房間
} Obj;
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

// 產生一個物件：照原版先呼叫第 1 格（設定路徑、名稱、別名、屬性）
int story_new(const char *path, int parent) {
    if (obj_count >= obj_cap) objs = realloc(objs, (obj_cap = obj_cap ? obj_cap * 2 : 1024) * sizeof *objs);
    int h = obj_count++;
    Obj *o = &objs[h];
    memset(o, 0, sizeof *o);
    o->cls = class_find(path);
    o->parent = parent;
    snprintf(o->path, sizeof o->path, "%s", path);
    if (!o->cls) fprintf(stderr, "劇情：找不到物件 %s\n", path);
    slot_call(h, 1, 0);
    return h;
}

int story_player(void) {
    if (!the_player) the_player = story_new("秦殇/主角", 0);  // shortcut: 主角的類別還沒查，先用空物件
    return the_player;
}

const char *story_name(int h) { return O(h) ? O(h)->name : ""; }

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
    case 0x10054ed0:                                                         // find_item(物件, 別名)：身上有這個別名的東西
        for (int i = 1; i < obj_count; i++)
            if (objs[i].parent == a[0] && !strcmp(objs[i].alias, S(a[1]))) return i;
        return 0;
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
    case 0x1004dcc0: printf("【日誌】%s\n", S(a[2])); return 0;                 // journal
    }
    static uint32_t seen[4096];
    static int nseen;
    for (int i = 0; i < nseen; i++)
        if (seen[i] == addr) return 0;
    if (nseen < 4096) seen[nseen++] = addr;
    fprintf(stderr, "劇情：還沒翻的原版函式 0x%08x\n", addr);
    return 0;
}

void *ExceptionList;
