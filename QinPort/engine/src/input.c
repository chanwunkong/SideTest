// CUserInput 選人名單（Poq.exe 0x457a30～0x459170）。由 main.c 在 party.c 之後引入。
// 原版名單是 60 格靜態鏈（物件編號），旗標 0x100 記在角色上；這裡直接存 Actor 指標、旗標放 Actor.sel。
// 隊友有 Actor（team_make）：點頭像（party_tap）加進名單；名單裡的人收左鍵移動／攻擊（input_team_cmd）與右鍵出手（input_cast）。
#define SEL_MAX 60
static Actor *sel_list[SEL_MAX];
static int sel_n, sel_changed;  // sel_changed = +0x3f0 名單變了
static Actor *sel_target;       // +4 目前點選的目標物件（原版放開時呼叫虛擬格 0x20 取消高亮）

static int sel_find(const Actor *a) {  // 名單裡的位置，沒有 = -1（原版掃 60 格的有效旗標）
    for (int i = 0; i < sel_n; i++) if (sel_list[i] == a) return i;
    return -1;
}
// 0x458400：某人在不在選到的名單
static int input_sel_has(const Actor *a) { return sel_find(a) >= 0; }
// 0x457a30：加人（已在名單回 0）；角色設旗標 0x100、名單變了
static int input_sel_add(Actor *a) {
    if (sel_find(a) >= 0) return 0;
    if (a && sel_n < SEL_MAX) { a->sel = 1; sel_list[sel_n++] = a; }
    sel_changed = 1;
    return 1;
}
// 0x458fc0：移除（不在名單回 0）；拿掉旗標
static int input_sel_remove(Actor *a) {
    int i = sel_find(a);
    if (i < 0) return 0;
    if (a) a->sel = 0;
    memmove(&sel_list[i], &sel_list[i + 1], (size_t)(sel_n - i - 1) * sizeof *sel_list);
    sel_n--;
    sel_changed = 1;
    return 1;
}
// 0x458f30：清空名單，再放掉目標
static void input_target_clear(void) { sel_target = NULL; }  // 0x459050
static void input_sel_clear(void) {
    while (sel_n > 0) input_sel_remove(sel_list[0]);
    input_target_clear();
}
// 0x458550：角色在不在自己這邊的名單（+0x1c 的 side 名單 = 玩家這一方、非敵對方）
static int input_side_has(const Actor *a) { return a && alive(a) && a->side == actors[0].side; }
// 0x4585c0：角色在不在本機玩家的隊伍（全域 0x940184 名單：主角與他的隊員）
static int input_party_has(const Actor *a) {
    Actor *m[PARTY_SLOTS];
    for (int i = 0, n = party_list(m); i < n; i++) if (m[i] == a) return 1;
    return 0;
}
// 0x458700：本機隊伍有沒有人以 a 為目標
static int input_party_targets(const Actor *a) {
    Actor *m[PARTY_SLOTS];
    for (int i = 0, n = party_list(m); i < n; i++)
        if (alive(m[i]) && (unsigned)(m[i]->object_kind - 30) < 71u && m[i]->foe == a) return 1;
    return 0;
}
// 0x458620：本機隊伍有沒有人在戰鬥狀態且有敵對目標（或附近 0x48b9e0 找得到敵人）
static int input_party_fighting(void) {
    Actor *m[PARTY_SLOTS];
    for (int i = 0, n = party_list(m); i < n; i++) {
        Actor *p = m[i];
        if (!alive(p) || (unsigned)(p->object_kind - 30) >= 71u || !has_status(p, 0)) continue;  // 狀態 0 = 战斗
        if (p->foe && (unsigned)(p->foe->object_kind - 30) < 71u && is_foe(p, p->foe)) return 1;
        if (actor_near_begin(p, 0, -1)) return 1;
    }
    return 0;
}
// 0x4590f0：重設：清選人和目標，名單變更旗標歸零（0x45a030 重建自己這邊名單、0x45a100 同步選人名單：
//   自己這邊名單在這裡是 side 即時比對，不必存；選人名單只留還在自己這邊的）
static void input_reset(void) {
    input_sel_clear();
    sel_changed = 0;
}
// 0x459170：給自己這邊每個人下命令 3（原版送封包 0xf023；單機直接下）
static int input_side_command(int p0) {
    if (!scene_client.ready) return 0;
    for (int i = 0; i < actor_count; i++)
        if (input_side_has(&actors[i]) && (unsigned)(actors[i].object_kind - 30) < 71u) actor_command(&actors[i], 3, p0, 0, 0, 1, 0);
    return 1;
}

// 0x467620：進場保護倒數（+0x4c）；> 0 時每個隊員有狀態 102（不能被當目標），歸零就拿掉
static int party_protect;
static void input_protect_set(int n) {
    if (party_protect == n) return;
    party_protect = n;
    Actor *m[PARTY_SLOTS];
    for (int i = 0, k = party_list(m); i < k; i++) actor_status_notify(m[i], 102, n > 0, -1);
}
// 0x465cf0：加進本機玩家的隊伍名單；名單原本是空的就開 250 拍進場保護；保護中的新人也套狀態 102
// shortcut: 名單用 party_list（主角＋劇情隊員）即時算，不另存；所以「原本是空的」改看保護倒數是否為 0 且是主角。
static void input_player_add(Actor *a) {
    if (!a) return;
    if (a == &actors[0] && party_protect == 0) { party_protect = 0; input_protect_set(250); }
    else if (party_protect > 0) actor_status_notify(a, 102, 1, -1);
}
// 每個邏輯拍：進場保護倒數（shortcut: 原版倒數在客戶端更新裡，位址沒找；先在主迴圈每拍減 1）
static void input_tick(void) {
    if (party_protect > 0 && --party_protect == 0) { party_protect = 1; input_protect_set(0); }
}

// 0x457be0：右鍵出手。場景就緒且名單有人才做；名單每個本機隊員（0x4585c0）、沒有狀態 0x11／0xe 的人放自己的左手技能（+0x503）。
// 目標（點到的角色）有的話放在 foe；沒有就用 (wx, wy) 這個點。
// shortcut: 技能表 +0x36 旗標（0 地點命令 8／1 敵人／2、4 物件／8 隊友／0x10 自己／0x20 NPC 組／0x40 沒目標也能放）引擎沒有這欄，
//   由 hero_cast_point 用 skill_manual／skill_needs_foe 近似；隊友沒有左手技能欄，改放 npc_selected 挑的對敵技能（有目標用命令 9、沒有用命令 8 打地點）。
static void team_cast_point(Actor *a, int wx, int wy, Actor *target) {
    ActorSkill *sk = npc_selected(a);
    if (!sk) return;
    if (target) { actor_target_set(a, target); actor_command(a, 9, sk->id, target->sh ? target->sh : (int)target->object_id, 0, 1, 0); }
    else actor_command(a, 8, sk->id, wx / 64, wy / 64, 1, 0);
    actor_skill_select(a, -1);
}
static int input_cast(int wx, int wy, Actor *target) {
    if (!scene_client.ready || sel_n < 1) return 0;
    if (target) actor_target_set(&actors[0], target);
    for (int i = 0; i < sel_n; i++) {
        Actor *a = sel_list[i];
        if (!a || !input_party_has(a) || has_status(a, 0x11) || has_status(a, 0xe)) continue;
        if (a == &actors[0]) hero_cast_point(a, wx, wy);
        else if (a->team) team_cast_point(a, wx, wy, target);
    }
    return 1;
}
static int input_sel_count(void) { return sel_n; }


// 0x46ec80：場景就緒狀態轉換（沒變就不做）。離場（0）：關敵人狀態框（0x459ce0）、解除暫停（0x46c260(0)）、清框選與目標、淡出歸零（0x44ca40(0,0)）；
// 進場（1）：換場景交叉淡出（0x44ca40(3,0)）、重設輸入（0x4426b0）。
// shortcut: 離場時大場景（寬 > 32）原版呼叫 0x4f9fb0（存探索，位址沒細看）；探索存檔在 save_write；原版進場後 10 拍恢復游標（0x46e8b0）沒做，
//   游標每幀由主迴圈決定
static void input_box_clear(void);
static void scene_ready_set(int on) {
    if (scene_client.ready == on) return;
    scene_client.ready = on;
    if (!on) {
        party_show = NULL;
        scene_pause(0);
        input_box_clear();
        input_target_clear();
        fade_start(0);
    } else {
        fade_start(3);
        input_reset();
    }
}
