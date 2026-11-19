import sys,re
# usage: callers.py addr...  -> which functions (in EasyRPG.dll.c / Poq.exe.c) mention FUN_addr
src={}
for b in ('EasyRPG.dll','Poq.exe'):
    src[b]=open(f'decomp/{b}.c',errors='replace').read()
for a in sys.argv[1:]:
    for b,t in src.items():
        cur=None; out=set()
        for l in t.split('\n'):
            m=re.match(r'^// ==== (\w+) ',l)
            if m: cur=m.group(1)
            elif f'FUN_{a}' in l and cur: out.add(cur)
        out.discard(a)
        print(a,b,sorted(out))
