"""從遊戲自帶的 VC_redist.x86.exe（_CommonRedist/vcredist/2019）取出真正的 vcruntime140／msvcp140／ucrtbase／mfc140（14.2x）。
原因：vcruntime140／msvcp140／ucrtbase 不在遊戲資料夾（Windows 上由 vcredist 安裝到系統）。mfc140.dll 用遊戲資料夾自己的（匯出表有 14448 個序號，pefile 預設只解析一部分，loader 改成直接讀 EAT）。
輸出到 redist/（已加入 .gitignore，不進 git）。用法：python fetch_redist.py [遊戲資料夾] [VC_redist.x86.exe 路徑]；需要 macOS 的 bsdtar（libarchive 可讀 cab）。"""
import sys, os, re, struct, subprocess, tempfile, shutil
G = sys.argv[1] if len(sys.argv) > 1 else '../game'
# 預設用遊戲自帶的 2019 版；argv[2] 可改指定別的 VC_redist.x86.exe
src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(G, '_CommonRedist/vcredist/2019/VC_redist.x86.exe')
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'redist'); os.makedirs(out, exist_ok=True)
d = open(src, 'rb').read()
tmp = tempfile.mkdtemp()
def untar(blob, name):
    p = os.path.join(tmp, name); open(p, 'wb').write(blob); o = os.path.join(tmp, name + '.d'); os.makedirs(o)
    subprocess.run(['bsdtar', '-xf', p, '-C', o], check=True); return o
big = max((m.start() for m in re.finditer(b'MSCF\0\0\0\0', d)), key=lambda o: struct.unpack('<I', d[o + 8:o + 12])[0])
top = untar(d[big:big + struct.unpack('<I', d[big + 8:big + 12])[0]], 'top')
for f in sorted(os.listdir(top)):
    b = open(os.path.join(top, f), 'rb').read()
    if b[:4] == b'MSCF':
        o = untar(b, f)
        for x in os.listdir(o):
            n = x[:-4] if x.lower().endswith('_x86') else x      # 新版檔名帶 _x86 後綴
            if n.lower() in ('vcruntime140.dll', 'msvcp140.dll', 'ucrtbase.dll') :
                shutil.copy(os.path.join(o, x), os.path.join(out, n))
shutil.rmtree(tmp); print(sorted(os.listdir(out))[:6], '...', len(os.listdir(out)), '個檔')
