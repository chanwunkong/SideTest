"""可行性驗證 1：原版每個 PE 檔的匯入表，統計要替換的 Windows 系統呼叫有多少。用法：python imports.py [遊戲資料夾]"""
import sys, os, collections, pefile
G = sys.argv[1] if len(sys.argv) > 1 else '../game'
SYS = {'kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','ole32.dll','oleaut32.dll','winmm.dll','ws2_32.dll','wininet.dll','version.dll','imm32.dll',
       'ddraw.dll','dinput.dll','dinput8.dll','d3d8.dll','d3d9.dll','shell32.dll','netapi32.dll','gdiplus.dll','msvcp140.dll','vcruntime140.dll','comctl32.dll'}
total = collections.defaultdict(set)
for f in ['Poq.exe','EasyRPG.dll','UIWndEx.dll','Gfx3D.dll','Mfx.dll','OmUtil.dll','mfc140.dll','BNNet.dll','BugTrap.dll','OpenAL32.dll','libvlc.dll']:
    p = os.path.join(G, f)
    if not os.path.exists(p): print(f, '不存在'); continue
    pe = pefile.PE(p, fast_load=True); pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    print('==', f, 'ImageBase', hex(pe.OPTIONAL_HEADER.ImageBase), 'size', hex(pe.OPTIONAL_HEADER.SizeOfImage), 'machine', hex(pe.FILE_HEADER.Machine))
    for e in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
        d = e.dll.decode().lower(); names = [i.name.decode() if i.name else 'ord%d' % i.ordinal for i in e.imports]
        mark = 'SYS' if d in SYS else 'GAME'
        print('  %-18s %4d %s' % (d, len(names), mark))
        if d in SYS: total[d].update(names)
print('\n== 需要替換的系統 DLL 匯入（去重，所有檔合計）')
n = 0
for d, s in sorted(total.items()): print('  %-18s %4d' % (d, len(s))); n += len(s)
print('合計', n)
