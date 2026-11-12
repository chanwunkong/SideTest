"""從原版劇情引擎 EasyRPG.dll 抽出「房間」(場景)定義：路徑、名稱、別名、出口、NPC。

每個房間是一段初始化程式，依序呼叫固定的設定函式，參數用 push 立即值傳入：
  0x1005d390 房間基底初始化（每個房間的開頭）
  0x10023c60 路徑    0x10023cf0 名稱    0x10023c40 別名（= 場景檔 / 觸發區事件用的代號）
  0x1005d8b0 出口(方向, 目的房間路徑, 入口編號)   0x1005f320 NPC(路徑, 數量)
只取規則與數值，不複製程式碼。輸出 JSON。
用法：python3 roomtool.py <EasyRPG.dll> <輸出.json>
"""
import json, struct, sys

CALLS = {0x1005d390: 'room', 0x10023c60: 'path', 0x10023cf0: 'name', 0x10023c40: 'alias',
         0x1005d8b0: 'exit', 0x1005f320: 'npc'}


def main(dll, out):
    d = open(dll, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec, opt = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for k in range(nsec):
        s = pe + 24 + opt + k * 40
        vs, va, rs, ro = struct.unpack_from('<IIII', d, s + 8)
        secs.append((d[s:s + 8].rstrip(b'\0'), base + va, ro, rs))

    def cstr(v):
        for _, vv, ro, rs in secs:
            if vv <= v < vv + rs:
                o = ro + v - vv
                return d[o:d.index(b'\0', o)].decode('gbk', 'replace')
        return None

    _, tva, tro, trs = next(s for s in secs if s[0] == b'.text')
    rooms, cur, pushes = [], None, []
    i = tro
    while i < tro + trs - 5:
        op = d[i]
        if op == 0x68:  # push imm32
            pushes.append(struct.unpack_from('<I', d, i + 1)[0]); i += 5; continue
        if op == 0x6a:  # push imm8
            pushes.append(d[i + 1]); i += 2; continue
        if op == 0xe8:
            dst = tva + (i - tro) + 5 + struct.unpack_from('<i', d, i + 1)[0]
            kind = CALLS.get(dst)
            if kind == 'room':
                cur = {'exits': [], 'npcs': []}
                rooms.append(cur)
            elif kind and cur is not None and pushes:
                if kind in ('path', 'name', 'alias'):
                    cur[kind] = cstr(pushes[-1])
                elif kind == 'exit' and len(pushes) >= 3:  # 參數由右到左 push：入口編號、目的、方向
                    cur['exits'].append({'dir': cstr(pushes[-1]), 'to': cstr(pushes[-2]), 'entry': pushes[-3]})
                elif kind == 'npc' and len(pushes) >= 2:
                    cur['npcs'].append({'path': cstr(pushes[-1]), 'count': pushes[-2]})
            pushes = []
            i += 5
            continue
        if op in (0xc3, 0xc2):  # ret：房間函式結束
            cur, pushes = None, []
        i += 1
    rooms = [r for r in rooms if r.get('path')]
    json.dump(rooms, open(out, 'w'), ensure_ascii=False, indent=1)
    print(f'{len(rooms)} 個房間，{sum(len(r["exits"]) for r in rooms)} 個出口，{sum(len(r["npcs"]) for r in rooms)} 個 NPC → {out}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
