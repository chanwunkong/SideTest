import re,os,subprocess,sys
# 掃第 10～17 格：反組譯（r2）的 eax 常數 vs 反編譯的 return 常數，找「Ghidra 把回傳值丟掉」。cd qinport && python3 -I tools/retscan.py（輸出 = 可疑的格；已知誤判：0x55555556 = 除以 3）SL={10,12,13,14,15,16,17}
fs=[]
for dp,_,files in os.walk('story'):
  for f in files:
    p=os.path.join(dp,f)
    t=open(p,encoding='utf-8',errors='ignore').read()
    parts=re.split(r'(?m)^// ---- 第 ',t)[1:]
    for pt in parts:
      m=re.match(r'(\d+) 格（原 (\w+)）\n',pt)
      if not m or int(m.group(1)) not in SL: continue
      body=pt[m.end():].split('\nstatic intptr_t')[0]
      fs.append((int(m.group(1)),m.group(2),p,body))
print(len(fs),file=sys.stderr)
cmds=[]
for s,a,p,b in fs: cmds.append(f'?e === {a}'); cmds.append(f's 0x{a}; af; pdf')
open('/tmp/r2cmds','w').write('\n'.join(cmds))
out=subprocess.run(['r2','-q','-e','scr.color=0','-i','/tmp/r2cmds','game/EasyRPG.dll'],capture_output=True,text=True).stdout
chunks=re.split(r'=== (\w+)\n',out)
dis={chunks[i]:chunks[i+1] for i in range(1,len(chunks)-1,2)}
bad=0
for s,a,p,b in fs:
  d=dis.get(a,'')
  sig=b.split('{')[0]
  isvoid=sig.strip().startswith('void')
  consts=set(int(x,16) if x.startswith('0x') else int(x) for x in re.findall(r'mov eax, (0x[0-9a-f]+|\d+)\b',d))
  xor0=bool(re.search(r'xor eax, eax',d))
  rets=set(int(x) for x in re.findall(r'return (-?\d+);',b))
  other_ret=bool(re.search(r'return [^;0-9\-][^;]*;',b))
  dc=set(consts)|({0} if xor0 else set())
  if isvoid or (dc-rets and not other_ret and any(c!=0 for c in dc-rets)) :
    bad+=1; print(s,a,p.replace('story/',''),'void' if isvoid else '', 'dis',sorted(dc),'C',sorted(rets), 'cfn' if other_ret else '')
print(bad,file=sys.stderr)
