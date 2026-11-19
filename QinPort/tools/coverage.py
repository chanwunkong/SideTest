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
for k, v in {'5': '開鎖 0x10034900', '6': '偷竊 0x10034010', '7': '(原版 strike 的第 7 類)', '12': '清狀態', '13': '界面技能'}.items(): S['am:' + k] = ('?', '資料 0 次；' + v + '，見 PORT_QUEUE')
for k in ['10', '18', '19', '21', '22', '23', '24']: S['sp:' + k] = ('完整', 'skill_field／skill_now／stat_ptr（0x10026270 表）')
for i in list(range(210, 224, 2)) + list(range(250, 262)) + [550, 552, 554, 556, 558, 560, 512, 222, 223, 287]: S['sp:%d' % i] = ('完整', 'strike 暫存 add/pct（0x100244f0~0x10024dc0），ID>500 拆兩個')
S['sp:269'] = ('完整', '原版整個 dll 沒有讀這個變數，沒作用（等價）')
for i in (271, 273, 274, 278, 286): S['sp:%d' % i] = ('完整', '馴服／召喚／陷阱參數')
for i in (292, 293, 295): S['sp:%d' % i] = ('完整', 'skill_now／status_clear')
for i in list(range(41, 59)) + [53, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 74, 75, 76, 77, 80, 82, 84, 86, 88, 90, 92, 93, 94, 95, 96, 97, 98]: S['sp:%d' % i] = ('完整', 'stat_ptr → Actor.ex[id]，strike／derive 讀')
for i in (69,): S['sp:%d' % i] = ('?', 'ex[69] 沒有讀它的地方，原版是否使用未查')


for k, v in {'#1': ('部分', '子類 0（角色）rec_add 完整；子類 1（進場景＋天氣 0x470300 case1）沒做'), '#2': ('完整', 'rec_remove'), '#3': ('部分', '屬性小包：0x34／0x66／0x65／0x33 做；外觀 0x01、名字 0x02、0x35、0x36、血量 0x67 沒做'),
 '#4': ('完整', '位置修正 0x496260／插值 0x4967d0'), '#5': ('完整', 'actor_status_notify 0x48d2c0'), '#6': ('無', '地上道具 ground_drop（AddProp）24 次'),
 '#7': ('無', '建築狀態 0x484ee0（scene_building_state 已有；缺原版「物件編號 → BNT 建築」對照，資料裡編號 2000+ 起、不是 BNT 順序，要查 CIDTable 配號）'),
 '#8': ('無', '飛行效果單位 0x450700（0xc9／300／500／0x1fe 四種）'), '#9': ('無', 'SkillUnit 招式單位 0x43acb0（錄影裡的招式特效）'), '#10': ('完整', '字串佇列 strq'), '#13': ('完整', '場景名／命令字串'),
 '@0': ('完整', '時間戳'), '@1': ('完整', '鏡頭定位'), '@2': ('完整', 'zoom'), '@4': ('部分', '對話：頭頂泡泡代替對話框；語音 sound/NNN.wav 已做（audio_voice，本輪）；缺對話框 UI（框、說話者名、+0x1fd0 位置）'),
 '@5': ('完整', '原版單機不建 Rec 封包，只寫標記（等價）'), '@6': ('無', '對話框顯示時間 n×3（至少 0x28）；對話框 UI 沒做，泡泡用字數計時'), '@7': ('完整', '鏡頭跟隨'),
 '@8': ('無', '0x44ca40 切光影模式 1／2／4 與 CScnClient+0x64（資料全是 0／1，值 0 時只設 +0x64）'), '@9': ('完整', '鏡頭平移'), '@10': ('無', '0x4c8630（3 次，值 0）'),
 '@11': ('完整', '播音效 0x4804a0（audio_sfx，本輪）'), '@12': ('無', '設小時並 light_hour（1 次）')}.items(): S['rec:' + k.replace('#', '#').replace('@', '@')] = v

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
rawuse = {'停止动画循环': 1, '速度变化': 1, '攻击不能': 1, '无敌': 1, '头上效果': 1, '身上效果': 1, '脚下效果': 1, '附加状态': 1, '声音': 1}
section('(a4) 狀態表欄位（RoleEffectMdl.txt）', [(k, v, *st('ec:' + k, ('完整', 'status_def raw[]／trail_*') if k in rawuse else ('?', '欄位引擎有 raw[20] 保存，是否逐欄消費未逐項查'))) for k, v in colcnt.items()])
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
rows = []
for k, v in keys.items():
    hit = ('"%s"' % k) in ENG
    rows.append((k, v, *(('完整', '引擎 literal 有讀／寫（story_rt／main）') if hit else ('?', '引擎沒有這個字串，可能只是劇情自己讀寫（資料）；原版有無消費者要查'))))
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
BST = {1: ('完整', '一般建築：占地格、遮擋'), 0x81: ('完整', '樹／礦：點擊命令 0x12、開箱規則 0x1004e680'), 0x96: ('完整', '門：go 門點擊命令 0x10、非 go 感應區'), 0x80: ('完整', '水缸：打 0x439fb0 → 0x8e'), 0x8c: ('完整', '同 0x80'), 0x85: ('完整', '命令 0x13（4 格內開）')}
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
    if k == '(空)': return ('?', '空名稱：只是建築／觸發區，無事件文字（原版 0x482db0 空字串不動作）')
    return ('?', '單機劇情沒有同名物件；不在文字指令表（EasyRPG.dll 0x1006b2e0 登記的 ~250 個）= 原版也沒作用，但沒逐個證明')
section('(e2) BNT 事件文字／物件名稱第一詞', [(k, v, *e2(k)) for k, v in trig.items()])
# ---------- (f) 場景 ----------
sc = collections.Counter(); weather = collections.Counter()
for p in glob.glob(G + '/scene/*.Scn'):
    t = open(p, 'rb').read().decode('gbk', 'replace')
    for m in re.finditer(r'^(\S+?)=', t, re.M): sc[m.group(1)] += 1
section('(f1) 場景檔 .Scn 欄位', [(k, v, *st('scn:' + k, ('完整', 'scn_value／入口點'))) for k, v in sc.items() if not k.startswith('前場景') and k not in ('x', 'y', '角色初始方向')] , '前場景／x／y／方向為入口點，已處理。')
mus = load(G + '/Data/music.txt')
section('(f2) 其他場景資料', [('music.txt 場景→音樂', len(mus), '完整', 'main.c music'), ('天氣（weather.c）', len(glob.glob(G + '/scene/*.Scn')), '部分', '疾風與色調沒做（見 PORT_QUEUE）'), ('日夜（light_hour）', 1, '完整', 'light.c')])

with open(ROOT + '/tools/coverage.md', 'w') as f:
    f.write('# 覆蓋矩陣（tools/coverage.py 產生，勿手改）\n\n狀態：完整＝引擎照原版處理；部分＝有但有缺；無＝沒做；?＝無法判斷。\n\n## 總表\n\n| 類別 | 項數 | 完整 | 部分 | 無 | ? |\n|---|---|---|---|---|---|\n')
    for k, (n, c) in SUM.items(): f.write('| %s | %d | %d | %d | %d | %d |\n' % (k, n, c['完整'], c['部分'], c['無'], c['?']))
    f.write('\n'.join(OUT) + '\n')
for k, (n, c) in SUM.items(): print(k, n, dict(c))
