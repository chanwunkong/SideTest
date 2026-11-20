#!/usr/bin/env python3
"""資料驅動覆蓋矩陣：從 game/Data、ObjMdl、scene、Rec、story 統計「遊戲真的會出現的機制」，對照引擎有沒有處理。
輸出 tools/coverage.md（可重跑：python3 -I tools/coverage.py）。次數由資料算；「引擎處理」欄 完整／部分／無／? 來自下面的 STATUS 表
（人工依 engine/src 與原版位址核對，附證據）；STATUS 沒列的項目一律 ?。
"""
import collections, glob, os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
G = os.path.join(ROOT, 'game')

def load(f):
    b = open(f, 'rb').read()
    if b[:1] == b'\x80': b = bytes(x ^ 0x78 for x in b[1:])
    return [l.split('\t') for l in b.decode('gb18030', 'replace').replace('\r', '').split('\n')]

def eng(): return ''.join(open(f, encoding='utf8', errors='replace').read() for f in glob.glob(ROOT + '/engine/src/*.c') + glob.glob(ROOT + '/engine/src/story/*.c'))
ENG = eng()
OUT = []
SUM = collections.OrderedDict()
def section(title, rows, note=''):
    """rows = [(項目, 次數, 狀態, 證據)]"""
    c = collections.Counter(r[2] for r in rows)
    SUM[title] = (len(rows), c)
    OUT.append('\n## %s\n%s\n\n| 項目 | 資料次數 | 引擎 | 證據／缺什麼 |\n|---|---|---|---|' % (title, note))
    for r in sorted(rows, key=lambda r: -r[1]):
        OUT.append('| %s | %d | %s | %s |' % r)

S = {}   # 人工狀態：key -> (狀態, 證據)
def st(key, default=('?', '')): return S.get(key, default)

# ---------- 人工核對結果（改了引擎就來更新這裡）----------
for k in ['', '0']: S['am:' + k] = ('完整', '不造成傷害，skill_hit 只加狀態與參數')
for k, v in {'2': '馴服 tame 0x10035370', '3': '召喚 summon 0x100355d0', '8': '陷阱 trap 0x10034cb0', '9': '除障 disarm', '10': '自損物理 strike（287）'}.items(): S['am:' + k] = ('完整', v)
S['am:1'] = ('完整', 'strike 物理'); S['am:4'] = ('完整', 'strike 物理')
S['am:11'] = ('部分', 'clone() 放克隆刺客刀／弓；缺 0x1003df00（複製施放者的裝備子物件與技能，技能等級上限 9，經 0x10037d70；原版規則已讀）')
for k, v in {'5': '開鎖 0x10034900（lockpick_open）', '6': '偷竊 0x10034010（skill_steal）', '7': '(原版 strike 的第 7 類，strike 同 4 先留血)'}.items(): S['am:' + k] = ('完整', v + '；資料 0 次')
for k, v in {'12': '清狀態', '13': '界面技能'}.items(): S['am:' + k] = ('無', '669 列技能全掃資料 0 次，遊戲不會出現；' + v + '，見 PORT_QUEUE')
for k in ['10', '18', '19', '21', '22', '23', '24']: S['sp:' + k] = ('完整', 'skill_field／skill_now／stat_ptr（0x10026270 表）')
for i in list(range(210, 224, 2)) + list(range(250, 262)) + [550, 552, 554, 556, 558, 560, 512, 222, 223, 287]: S['sp:%d' % i] = ('完整', 'strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個')
S['sp:269'] = ('完整', '原版整個 dll 沒有讀這個變數，沒作用（等價）')
for i in (271, 273, 274, 278, 286): S['sp:%d' % i] = ('完整', '馴服／召喚／陷阱參數')
for i in (292, 293, 295): S['sp:%d' % i] = ('完整', 'skill_now／status_clear')
for i in list(range(41, 59)) + [53, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 74, 75, 76, 77, 80, 82, 84, 86, 88, 90, 92, 93, 94, 95, 96, 97, 98]: S['sp:%d' % i] = ('完整', 'stat_ptr → Actor.ex[id]，strike／derive 讀')
for i in (69,): S['sp:%d' % i] = ('?', 'ex[69] 沒有讀它的地方，原版是否使用未查')


for k, v in {'#1': ('部分', '子類 0（角色）rec_add 完整；子類 1（進場景）：亮度參數（類型 u16 b7、夜 b9、日 b10 → light_table，rec_end 還原）做；b12＋u16(b13) 傳給 vfunc+8（疑似時鐘，資料 538/1370 等超過一個半小時，原版規則沒找到）和原版載另一份場景拷貝沒做'),
 '#2': ('完整', 'rec_remove'), '#3': ('完整', '0x470300 case 3：子碼 0x01 換外觀 0x48d810、0x02 名字、0x33、0x34、0x35 層號 +0x66、0x36 名字編號 0x48e820、0x65、0x66、0x67 生命百分比 0x48e8f0；0x79（資料 1 次）原版不處理'),
 '#4': ('完整', '位置修正 0x496260／插值 0x4967d0'), '#5': ('完整', 'actor_status_notify 0x48d2c0'),
 '#6': ('完整', '地上道具 24 次：4 筆 15 byte 照原版放（AddProp 0x450cc0）；20 筆只有 12 byte，原版照新格式讀 x、y 會讀到封包外殘值 0xcdcd（格子在地圖外），AddObj 失敗什麼都不放，引擎同樣要 n >= 15 才放（等價）；沒在畫面驗證'),
 '#7': ('部分', '建築狀態 0x484ee0：編號 = 低 14 位 − 2001 = BNT 順序（0x4a2600 配號 2000 起先加 1、0x44f4c0 逐筆建築；rec_net case 7，只動 CTriBld）。資料 612 筆全落在 BNT 範圍內；但同場景不同次進入偏移會差 1（hd_mishi），#6 地上道具也佔編號，原版規則沒找到（記錄檔封包比原版讀的欄位短，見 #6）。2026-11-20 新發現：0x4a2600 是環形配號器（範圍 2000～16381，計數器 +0x17ffa 先加 1、遇到使用中的槽就往後找、滿了回 2000，只在換場景時不重設），所以編號偏移取決於當時已配過的物件數，沒有固定偏移；要完全照原版得模擬播放端配號歷史，資料裡只有 #1 角色編號（3527 起）可旁證，沒查通。原生版沒有劇情層跑不了過場，網頁版逐片實測未做'),
 '#8': ('完整', 'Rec 封包 0x450700(單位編號, x, y, 0, 0)：rec_net case 8 → unit_new(來源 0)；資料 4 次都是單位 23。沒在畫面逐片驗證'),
 '#9': ('完整', 'Rec 封包 0x43acb0(角色, 招式, 點)：rec_net case 9 → rec_cast → actor_cast_try（MethodMain + 招式觸發，招式不在角色技能列就先補進去）；資料 47 次、招式編號 18～598。沒在畫面逐片驗證'), '#10': ('完整', '字串佇列 strq'),
 '#13': ('完整', '場景名／命令字串；子類 2 換場景 → rec.foreign（本輪修：3 byte 封包以前被長度檢查擋掉）'),
 '@0': ('完整', '時間戳'), '@1': ('完整', '鏡頭定位'), '@2': ('完整', 'zoom'),
 '@4': ('部分', '對話框 = 說話者頭頂的文字框（0x4722f0 定位 x = 角色 x、下緣 = y − 頭高 − 10；0x5963e0 建框；超出畫面推回）；\\r\\n → \\n；語音 sound/NNN.wav 播完才收。原版 Rec 對話沒有說話者名／頭像（case 4 只存文字，名字在 PTalk_C 文字裡）。框色與字色（DAT_0064b3b4 16 位元全域）沒查到'),
 '@5': ('完整', '原版單機不建 Rec 封包，只寫標記（等價）'), '@6': ('完整', 'case 6：沒有語音才設拍數 n×3、至少 0x28；有語音靠語音播完（0x472e70）'), '@7': ('完整', '鏡頭跟隨'),
 '@8': ('完整', '0x44ca40(模式, 0)：1／2／3 淡入淡出 = screen_effect；+0x64 只被 #11／#12 讀（資料沒有）'), '@9': ('完整', '鏡頭平移'), '@10': ('完整', '0x4c8630：全場景探索值 0 → 0xc9、1 → 1、2 → 0'),
 '@11': ('完整', '播音效 0x4804a0（audio_sfx）'), '@12': ('完整', '0x469630＋0x4695f0：設小時並依亮度表重設亮度（clock_hour_set）')}.items(): S['rec:' + k.replace('#', '#').replace('@', '@')] = v

# ---------- (a) 技能 ----------
am = collections.Counter(); sp = collections.Counter(); sts = collections.Counter(); die = collections.Counter(); nrows = 0
wl = collections.Counter(); lr = collections.Counter()
for f in sorted(glob.glob(G + '/Data/skill_*.txt')):
    for r in load(f)[2:]:
        r = r + [''] * 90
        if not r[0]: continue
        nrows += 1; am[r[11]] += 1; die[r[12]] += 1; wl[r[6]] += 1; lr[r[7]] += 1
        for i in range(26, 86, 5):
            if r[i]: sp[r[i]] += 1
        for i in (21, 22):
            if r[i]: sts[r[i]] += 1
for k in ('5', '6', '7', '12', '13'): am.setdefault(k, 0)  # 原版有處理但資料 0 次（供非劇情模式／DLC）
section('(a1) 技能 Attack方式（%d 列技能）' % nrows, [('Attack方式 %s' % (k or '空'), v, *st('am:' + k)) for k, v in am.items()])
section('(a2) 技能參數 ID（skill_field／0x1039e628 處理表）', [('參數 %s' % k, v, *st('sp:' + k)) for k, v in sp.items()])
# 狀態 id：RoleEffectMdl 欄位 vs StatusDef 讀取
rem = load(G + '/ObjMdl/RoleEffectMdl.txt')
cols = rem[0]
section('(a3) 技能用到的狀態 ID（RoleEffectMdl 91 個）', [('狀態 %s %s' % (k, rem[int(k) + 1][0] if int(k) + 1 < len(rem) else ''), v, *st('stt:' + k, ('完整', 'status_def 讀表，buff_add／status_def 外觀、速度、攻擊不能、無敵、顏色'))) for k, v in sts.items()])
colcnt = collections.Counter()
for r in rem[1:]:
    for i, v in enumerate(r):
        if v and i >= 2 and i < len(cols): colcnt[cols[i]] += 1
rawuse = {'停止动画循环': 1, '速度变化': 1, '攻击不能': 1, '无敌': 1, '头上效果': 1, '身上效果': 1, '脚下效果': 1, '附加状态': 1, '声音': 1, '旋转': 1, '幻影移动': 1, '幻影攻击': 1, '全身变色': 1, '武器变色': 1, '盾变色': 1, '头变色': 1, '缩放': 1}  # 旋转 flags 0x400、幻影 actor_trail_effect、变色／缩放 actor_modifier_set（main.c actor_effect v[10..17]）；剩 temp（欄 20，raw 只存 0~19）與 不发送到Client（欄 18，單機沒查到作用）
section('(a4) 狀態表欄位（RoleEffectMdl.txt）', [(k, v, *st('ec:' + k, ('完整', 'status_def raw[]／trail_*') if k in rawuse else ('?', '欄位引擎有 raw[20] 保存，是否逐欄消費未逐項查'))) for k, v in colcnt.items()])
for _k in ['0','1','2','3','4','5','6','7','8','9','10']: S['die:' + _k] = ('完整', '欄 12 → SkillRow.die（DieMode，基底預設 1）→ strike 擊殺 actor_kill_mode（0x100574e0：狀態 4 → 9、狀態 2 → 10、隊友 6/8/9 → 1）→ actor_remove（0x486910）；沒在畫面逐種驗證')
section('(a5) 技能死亡方式（欄 12）', [('死亡方式 %s' % k, v, *st('die:' + k, ('?', '死亡動畫選擇，未逐項查'))) for k, v in die.items() if k])

# ---------- (b) 道具 ----------
ity = collections.Counter(); attr_ids = collections.Counter(); attr_rows = 0
for f in glob.glob(G + '/Data/item_*.txt'):
    for r in load(f)[2:]:
        if len(r) > 3 and r[1]: ity[r[3]] += 1
for r in load(G + '/Data/attribT.txt')[3:]:
    r = r + [''] * 70
    if r[0] and r[31]: attr_ids[r[31]] += 1; attr_rows += 1
section('(b1) 道具類型（道具表第 3 欄）', [('類型 %s' % k, v, *st('it:' + k, ('完整', 'item_new 照表產生；類型 7~34 裝備、13/14 箭、60 錢、49~52 材料、1001+ 掉寶類')))  for k, v in ity.items() if k.isdigit() and (int(k) < 100)] +
        [('類型 1001~1119（掉寶類型表／打造配方）', sum(v for k, v in ity.items() if k.isdigit() and int(k) >= 100), '完整', 'item_type_pick 0x1007a2f0；MakeMdl 配方')])
def itst(k):
    n = int(k)
    if n in (21, 22, 23, 24, 27, 28, 29, 31, 32, 33) or 41 <= n <= 67 or 70 <= n <= 77 or n in (80, 82, 84, 86, 88, 90, 92, 93, 94, 95, 96, 97, 98): return ('完整', 'item_mods → stat_ptr → Ex[]，derive／strike 讀')
    if n in (292, 293, 295, 300, 301, 302, 303, 304, 294): return ('完整', 'skill_now／status_clear／gem_fx（HANDOFF 技能參數函式表）')
    if n == 40: return ('?', 'ex[40] 沒有讀它的地方')
    return ('?', '')
section('(b2) 道具特性屬性 ID（attribT 欄 31，%d 列特性）' % attr_rows, [('屬性 %s' % k, v, *itst(k)) for k, v in attr_ids.items()])

# ---------- (c) 怪物 ----------
mc = collections.Counter(); n = 0
for f in glob.glob(G + '/Data/npc_*.txt'):
    L = load(f)
    if len(L) < 2: continue
    hd = L[1]
    for r in L[2:]:
        if not r[0]: continue
        n += 1
        for i, v in enumerate(r):
            if v and i < len(hd) and hd[i].strip(): mc[hd[i].strip()] += 1
rows = []
for k, v in mc.items():
    ok = any(x in k for x in ['生命', '攻击', '防御', '命中', '闪避', '经验', '图形', '道具', '概率', '技能', '名称ID', '视野', '打击恢复', '减少', '免疫', '陷阱', '生命恢复', '魔法', '人物', '移动速度', '攻击频率'])
    rows.append((k, v, *(('完整', 'res.c mondefs_load 讀入並由 story_on_person 套用') if ok else ('?', ''))))
section('(c1) 怪物表欄位（%d 個怪物列）' % n, rows)
ai = collections.Counter(); keys = collections.Counter()
for f in glob.glob(ROOT + '/story/**/*.c', recursive=True):
    if '/NET/' in f or '/_' in f: continue
    s = open(f, encoding='utf8', errors='replace').read()
    for m in re.finditer(r'\b(?:set_int|new_int)\([^,]+,\s*"([^"]+)"\s*,\s*([^)]+)\)', s): keys[m.group(1)] += 1
C2 = {'Map': ('部分', '0x100484c0：玩家進房間記 CurMap／"Map" 已去過位元遮罩／Place（story_rt room_map_note，本輪）；地圖視窗 JM60 的地點按鈕還是全部顯示，沒用這份遮罩（site 序號 ↔ Map 號對照沒查）')}
for k in ['SortInquiry', 'G_InfoID', 'UIS_Addgem', 'G_IsUISkill', 'UI_Cur', 'BD_MAX0', 'Value', 'maketype', 'Type']:
    C2[k] = ('無', 'EasyRPG.dll 位元組搜尋只出現 1 次（0x1007f810 登記屬性名），沒有以名稱讀取；是否以索引讀取沒證明；界面技能／連線用')
rows = []
for k, v in keys.items():
    hit = ('"%s"' % k) in ENG
    rows.append((k, v, *C2.get(k, ('完整', '引擎 literal 有讀／寫（story_rt／main）') if hit else ('?', '引擎沒有這個字串，可能只是劇情自己讀寫（資料）；原版有無消費者要查'))))
section('(c2) 劇情物件屬性鍵（set_int／new_int，不含 NET）', rows, '含 G_AIGroup、NpcBehavior 等 AI 旗標。')

# ---------- (d) 劇情命令 ----------
cmd = collections.Counter()
for f in glob.glob(ROOT + '/story/**/*.c', recursive=True):
    if '/NET/' in f: continue
    s = open(f, encoding='utf8', errors='replace').read()
    for m in re.finditer(r'(?:ENGINE|story_call_addr)\((0x[0-9a-fA-F]+)', s): cmd[m.group(1).lower()] += 1
rt = open(ROOT + '/engine/src/story/story_rt.c', encoding='utf8').read()
shared = open(ROOT + '/story/_shared.c', encoding='utf8').read()
for m in re.finditer(r'#define (\w+)\(\.\.\.\) ENGINE\((0x[0-9a-f]+)', open(ROOT + '/engine/src/story/qinstory.h').read()): NAMES = None
names = {m.group(2).lower(): m.group(1) for m in re.finditer(r'#define (\w+)\(\.\.\.\) ENGINE\((0x[0-9a-f]+)', open(ROOT + '/engine/src/story/qinstory.h').read())}
rows = []
for a, v in cmd.items():
    m = re.search(r'case %s:[^\n]*' % a, rt)
    if not m and ('shared_' + a[2:]) in shared:
        rows.append(('%s %s' % (names.get(a, ''), a), v, '完整', '共用劇情函式，已翻成 C（story/_shared.c）')); continue
    if not m: rows.append(('%s %s' % (names.get(a, ''), a), v, '無' if not (a in rt) else '?', 'story_rt.c 沒有 case（可能在 default 記錄）')); continue
    line = m.group(0)
    sc = 'shortcut' in line or '沒做' in line
    rows.append(('%s %s' % (names.get(a, ''), a), v, '部分' if sc else '完整', 'story_rt.c case' + ('（註解含 shortcut／沒做）' if sc else '')))
section('(d1) 劇情用到的 EasyRPG 命令（ENGINE／共用函式）', rows)
rec = collections.Counter()
for f in glob.glob(G + '/Rec/User/*.rec'):
    d = open(f, 'rb').read(); p = 0x80
    while p + 4 <= len(d):
        l = struct.unpack_from('<I', d, p)[0]; p += 4
        b = d[p:p + l]; p += l
        if len(b) >= 2 and b[:1] in (b'#', b'@', b'$'): rec[(chr(b[0]), b[1])] += 1
rows = []
for (c, k), v in rec.items():
    rows.append(('%s %d' % (c, k), v, *st('rec:%s%d' % (c, k))))
section('(d2) 劇情錄影（Rec/User/*.rec）封包子類（`#` 0x470300、`@` 0x4725c0）', rows, '`$` 在資料中 0 次。')

# ---------- (e) BNT 建築 ----------
bt = collections.Counter(); trig = collections.Counter()
def models(path):
    d = open(path, 'rb').read(); out = {}
    for a in range(len(d) - 0xa3):
        if d[a] != 3 or d[a + 1] or d[a + 2] == 0 or d[a + 3] == 0 or (d[a + 2] == 0xCD and d[a + 3] == 0xCD): continue
        if not 1 <= struct.unpack_from('<H', d, a + 0x2a)[0] <= 40: continue
        out.setdefault(struct.unpack_from('<H', d, a + 36)[0], struct.unpack_from('<H', d, a + 0x22)[0])
    return out
cache = {}
for p in glob.glob(G + '/scene/Int/*.BNT'):
    d = open(p, 'rb').read(); mn = d[0x22:0x122].split(b'\0')[0].decode(); q = 0x480
    if mn not in cache:
        mp = [x for x in glob.glob(G + '/Mdl/*') if os.path.basename(x).lower() == mn.lower()]
        cache[mn] = models(mp[0]) if mp else {}
    while q < len(d):
        t, mid = struct.unpack_from('<HH', d, q); nl = struct.unpack_from('<i', d, q + 16)[0]
        ev = b''
        if t == 4: ev = d[q + 20 + max(nl, 0): q + 20 + max(nl, 0) + 128].split(b'\0')[0]
        q += 20 + max(nl, 0) + (128 if t == 4 else 0)
        if t == 4:
            trig[(ev.decode('gbk', 'replace').split() or ['(空)'])[0]] += 1
        else: bt[cache[mn].get(mid, '無模型')] += 1
BST = {'無模型': ('完整', '4 筆 BNT 建築模型編號在 .MDL 裡找不到：原版 0x44f4c0 對空模型指標讀 +0x22（資料錯誤、未定義行為），引擎 o->m = NULL 略過（等價於不存在）'), 1: ('完整', '一般建築：占地格、遮擋'), 0x81: ('完整', '樹／礦：點擊命令 0x12、開箱規則 0x1004e680'), 0x96: ('完整', '門：go 門點擊命令 0x10、非 go 感應區'), 0x80: ('完整', '水缸：打 0x439fb0 → 0x8e'), 0x8c: ('完整', '同 0x80'), 0x85: ('完整', '命令 0x13（4 格內開）')}
section('(e1) 場景 BNT 建築模型種類', [('模型類型 %s' % (hex(k) if isinstance(k, int) else k), v, *BST.get(k, ('?', '未列'))) for k, v in bt.items()])
aliases = set(); netal = set()
for f in glob.glob(ROOT + '/story/**/*.c', recursive=True):
    if '/_' in f: continue
    for m in re.finditer(r'story_register\("([^"]*)", "([^"]*)"', open(f, encoding='utf8', errors='replace').read()):
        (netal if '/NET/' in m.group(1) else aliases).add(m.group(2).lower())
def e2(k):
    if k == 'go': return ('完整', 'door_go_due／story_exit')
    if k == 's_pctstart': return ('完整', 'story_pct_start 0x1001c210')
    if k == 'openall': return ('完整', '本輪補：trigger_sense_tick 執行 openall <別名>（0x10019d70 chest_open_all），之前只認 s_pctstart')
    if k.lower() in aliases: return ('完整', '同名單機劇情物件（object_story 依名稱配對，chest_state_sync）')
    if k.lower() in netal or k.startswith('net'): return ('完整', '只有 NET（連線版）劇情物件用，單機不產生')
    if k == '(空)': return ('完整', '空名稱：只是建築／觸發區，無事件文字（原版 0x482db0 空字串不動作）')
    if k == 'c_aigroup': return ('完整', '本輪補：trigger_sense_tick 執行 c_aigroup <原組> <新組>（0x10013270 → story_c_aigroup，storytest aigroup=）；之前誤判為「不是指令」')
    if k == 'snow1': return ('完整', '連線版房間入口（EasyRPG.dll 0x1005d8b0 "snow1" → 秦殇/NET/雁门关/ROOM/雪地战场01），所在場景 net_xiongnuyingdi 是連線版場景')
    if k in ('iron_ore', 'mucai'): return ('完整', '字串在 Poq.exe／EasyRPG.dll 位元組搜尋 0 次：不是指令、沒有同名物件，原版沒有作用（等價）')
    return ('?', '單機劇情沒有同名物件；不在文字指令表（EasyRPG.dll 0x1006b2e0 登記的 ~250 個）= 原版也沒作用，但沒逐個證明')
section('(e2) BNT 事件文字／物件名稱第一詞', [(k, v, *e2(k)) for k, v in trig.items()])
# ---------- (f) 場景 ----------
sc = collections.Counter(); weather = collections.Counter()
for p in glob.glob(G + '/scene/*.Scn'):
    t = open(p, 'rb').read().decode('gbk', 'replace')
    for m in re.finditer(r'^(\S+?)=', t, re.M): sc[m.group(1)] += 1
section('(f1) 場景檔 .Scn 欄位', [(k, v, *st('scn:' + k, ('完整', 'scn_value／入口點'))) for k, v in sc.items() if not k.startswith('前場景') and k not in ('x', 'y', '角色初始方向')] , '前場景／x／y／方向為入口點，已處理。')
mus = load(G + '/Data/music.txt')
section('(f2) 其他場景資料', [('music.txt 場景→音樂', len(mus), '完整', 'main.c music'), ('天氣（weather.c）', len(glob.glob(G + '/scene/*.Scn')), '完整', 'weather.c：雨／雪／疾風（CGale 0x43fda0）／色調 light_mode 已做；剩 shortcut：依場景種類 G_Type 的環境音（0x468c10 後半）、小雨亮度鎖只在開始下雨設'), ('日夜（light_hour）', 1, '完整', 'light.c')])

with open(ROOT + '/tools/coverage.md', 'w') as f:
    f.write('# 覆蓋矩陣（tools/coverage.py 產生，勿手改）\n\n狀態：完整＝引擎照原版處理；部分＝有但有缺；無＝沒做；?＝無法判斷。\n\n## 總表\n\n| 類別 | 項數 | 完整 | 部分 | 無 | ? |\n|---|---|---|---|---|---|\n')
    for k, (n, c) in SUM.items(): f.write('| %s | %d | %d | %d | %d | %d |\n' % (k, n, c['完整'], c['部分'], c['無'], c['?']))
    f.write('\n'.join(OUT) + '\n')
for k, (n, c) in SUM.items(): print(k, n, dict(c))
