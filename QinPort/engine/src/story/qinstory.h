// 劇情檔（tools/storytool.py 從原版 EasyRPG.dll 反編譯產生）共用的標頭：補上 Ghidra 的型別和巨集，宣告引擎函式。
// 原版是 32 位元；物件在這裡用小整數代號當指標用，所以反編譯裡的 (int)this 不會截斷出錯。
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef intptr_t undefined4;  // Ghidra 的 4 bytes 不分整數和指標，用 intptr_t 才裝得下 64 位元指標
typedef uint8_t undefined1, undefined, byte;
typedef uint16_t undefined2, ushort;
typedef uint32_t undefined3, uint;
typedef int64_t undefined8, longlong;
typedef uint64_t ulonglong;
typedef unsigned long ulong;
#define __thiscall
#define __fastcall
#define __cdecl
#define CONCAT31(a, b) ((intptr_t)(((uint32_t)(a) << 8) | (uint8_t)(b)))
// Ghidra 猜不出來源的暫存器值：原版這裡其實沒用到
static intptr_t unaff_SI, unaff_BP, unaff_DI, in_ECX, unaff_retaddr;
extern void *ExceptionList;
#define __security_check_cookie(...) 0
#define ER_GetStartType() ENGINE(0x10000000)  // shortcut: 開場種類還沒翻，記成未翻的呼叫

// ---- 引擎函式：一律經 engine_call(原版位址, 參數[8])，由 story_rt.c 依位址分派；還沒翻的會記錄下來
// （WebAssembly 不容許參數個數或型別不符的呼叫，所以不直接宣告成 C 函式）
intptr_t engine_call(uint32_t addr, intptr_t *a);
#define ENGINE(addr, ...) engine_call(addr, (intptr_t[8]){__VA_ARGS__})
#define say(...) ENGINE(0x100542e0, ##__VA_ARGS__)
#define clear_options(...) ENGINE(0x1005eed0, ##__VA_ARGS__)
#define option(...) ENGINE(0x1005f780, ##__VA_ARGS__)
#define player(...) ENGINE(0x100558b0, ##__VA_ARGS__)
#define get_flag(...) ENGINE(0x1005df70, ##__VA_ARGS__)
#define set_flag(...) ENGINE(0x1005f270, ##__VA_ARGS__)
#define new_flag(...) ENGINE(0x1005e850, ##__VA_ARGS__)
#define find_flag(...) ENGINE(0x1005da60, ##__VA_ARGS__)
#define find_item(...) ENGINE(0x10054ed0, ##__VA_ARGS__)
#define add_money(...) ENGINE(0x1003d3e0, ##__VA_ARGS__)
#define get_amount(...) ENGINE(0x1004b0d0, ##__VA_ARGS__)
#define quest_log(...) ENGINE(0x1004dd00, ##__VA_ARGS__)
#define get_int(...) ENGINE(0x100226e0, ##__VA_ARGS__)
#define set_int(...) ENGINE(0x10023c90, ##__VA_ARGS__)
#define new_int(...) ENGINE(0x100231d0, ##__VA_ARGS__)
#define set_path(...) ENGINE(0x10023c60, ##__VA_ARGS__)
#define set_name(...) ENGINE(0x10023cf0, ##__VA_ARGS__)
#define set_alias(...) ENGINE(0x10023c40, ##__VA_ARGS__)
#define set_string(...) ENGINE(0x10023d20, ##__VA_ARGS__)
#define room_init(...) ENGINE(0x1005d390, ##__VA_ARGS__)
#define npc_init(...) ENGINE(0x1005cd60, ##__VA_ARGS__)
#define add_exit(...) ENGINE(0x1005d8b0, ##__VA_ARGS__)
#define add_npc(...) ENGINE(0x1005f320, ##__VA_ARGS__)
#define add_item(...) ENGINE(0x1005f300, ##__VA_ARGS__)
#define rand_n(...) ENGINE(0x10052a00, ##__VA_ARGS__)
#define journal(...) ENGINE(0x1004dcc0, ##__VA_ARGS__)
#define start_plot(...) ENGINE(0x1004f1a0, ##__VA_ARGS__)
#define find_room(...) ENGINE(0x1003fe30, ##__VA_ARGS__)
#define find_alias(...) ENGINE(0x1003fd90, ##__VA_ARGS__)
#define set_int_alias(...) ENGINE(0x1003cbf0, ##__VA_ARGS__)
#define place_npc(...) ENGINE(0x10054960, ##__VA_ARGS__)
#define give_object(...) ENGINE(0x10054880, ##__VA_ARGS__)
#define spawn(...) ENGINE(0x10052550, ##__VA_ARGS__)
#define set_spawn_count(...) ENGINE(0x1004c5a0, ##__VA_ARGS__)
#define set_spawn_max(...) ENGINE(0x1004c550, ##__VA_ARGS__)
#define current_room(...) ENGINE(0x100558c0, ##__VA_ARGS__)
#define drop_items(...) ENGINE(0x100521f0, ##__VA_ARGS__)
#define set_attr(...) ENGINE(0x1003c6a0, ##__VA_ARGS__)
#define find_in_bag(...) ENGINE(0x1003fab0, ##__VA_ARGS__)
#define random_exits(...) ENGINE(0x10052a20, ##__VA_ARGS__)
#define item_init(...) ENGINE(0x1005cab0, ##__VA_ARGS__)
#define skill_init(...) ENGINE(0x1005d4a0, ##__VA_ARGS__)
#define chest_init(...) ENGINE(0x100f94c0, ##__VA_ARGS__)
#define random_chest_init(...) ENGINE(0x100f95b0, ##__VA_ARGS__)
#define plot_init(...) ENGINE(0x1005cd40, ##__VA_ARGS__)

// ---- 物件登記：每個劇情檔一個 story_reg_N，登記路徑、種類（0 房間 1 NPC 2 道具 5 技能 6 劇情事件）和自己的方法
typedef struct { int slot; intptr_t (*fn)(intptr_t *a); } StorySlot;  // a[0] = this
void story_fn_add(uint32_t addr, intptr_t (*fn)(intptr_t *a));  // 共用的劇情函式（_shared.c），依原位址登記
intptr_t story_call_addr(uint32_t addr, intptr_t *a);            // 依原位址呼叫共用的劇情函式
void story_register(const char *path, const char *alias, int kind, int start, const char *cls, const StorySlot *slots, int n);
void story_register_all(void);
