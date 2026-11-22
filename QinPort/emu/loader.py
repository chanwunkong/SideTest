"""可行性驗證 2：用 Unicorn（x86 模擬）載入原版 Poq.exe 與它的 DLL，系統呼叫換成主機端實作，看能跑到哪裡。
用法：python loader.py [遊戲資料夾] [最多印幾筆呼叫]
真實載入的模組：Poq.exe、OmUtil、mfc140、EasyRPG、Gfx3D、Mfx、UIWndEx；其餘（BNNet、BugTrap、OpenAL、libvlc、系統 DLL）全部由主機端提供。"""
import sys, os, struct, collections, time
import pefile
from winapi_sig import SIG, msvcp_sig
from unicorn import *
from unicorn.x86_const import *

USE_UCRT = os.environ.get('HOST_CRT') is None   # 預設載入真正的 ucrtbase；HOST_CRT=1 改用主機端 CRT 實作
G = sys.argv[1] if len(sys.argv) > 1 else '../game'
MAXLOG = int(sys.argv[2]) if len(sys.argv) > 2 else 400
REAL = ['ucrtbase.dll', 'vcruntime140.dll', 'msvcp140.dll', 'omutil.dll', 'mfc140.dll', 'easyrpg.dll', 'gfx3d.dll', 'mfx.dll', 'uiwndex.dll']   # 載入順序
BASES = {'poq.exe': 0x400000, 'easyrpg.dll': 0x10000000, 'mfx.dll': 0x30000000, 'omutil.dll': 0x40000000,
         'mfc140.dll': 0x50000000, 'ucrtbase.dll': 0x72000000, 'vcruntime140.dll': 0x70000000, 'msvcp140.dll': 0x71000000, 'gfx3d.dll': 0x60000000, 'uiwndex.dll': 0x68000000}
STUB_BASE, STUB_SIZE = 0x7F000000, 0x200000
HEAP_BASE, HEAP_SIZE = 0x20000000, 0x10000000
STACK_TOP, STACK_SIZE = 0x0B000000, 0x100000
TEB, PEB = 0x7FFDF000, 0x7FFDE000

uc = Uc(UC_ARCH_X86, UC_MODE_32)
mods = {}            # 小寫檔名 → {base, pe, exports{name/ord: va}}
stubs = {}           # va → (dll, name)
stub_va = {}         # (dll, name) → va
stub_next = [STUB_BASE]
calls = collections.Counter()
unknown = []

def rd(a, n): return bytes(uc.mem_read(a, n))
def u32(a): return struct.unpack('<I', rd(a, 4))[0]
def w32(a, v): uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
def cstr(a, mx=260):
    b = b''
    while len(b) < mx:
        c = rd(a + len(b), 1)
        if c == b'\0': break
        b += c
    return b.decode('latin1')

def _mk_iterm(check):
    b = bytearray.fromhex('56578B74240C8B7C241039FE730F8B0685C07406FFD0')
    b += bytes.fromhex('85C07506') if check else bytes.fromhex('90909090')
    b += bytes.fromhex('83C604EBEB31C05F5EC3')
    # 位移：jae(12)→done(31)；jz(18)→next(26)；jnz(24)→done_ret(33)；jmp(29)→loop(10)
    b[13] = 17; b[19] = 6; b[30] = 0xEB
    if check: b[25] = 7   # 非檢查版此處是 nop，不可改成 0x07（= pop es）
    return bytes(b)
RAW = {'EnterCriticalSection': b'\xc2\x04\x00', 'LeaveCriticalSection': b'\xc2\x04\x00', 'InitializeCriticalSection': b'\xc2\x04\x00', 'DeleteCriticalSection': b'\xc2\x04\x00',
       'TryEnterCriticalSection': b'\xb8\x01\x00\x00\x00\xc2\x04\x00', 'InitializeCriticalSectionAndSpinCount': b'\xb8\x01\x00\x00\x00\xc2\x08\x00',
       'InitializeCriticalSectionEx': b'\xb8\x01\x00\x00\x00\xc2\x0c\x00'}   # 單執行緒：臨界區是空操作，用機器碼而非 Python 回呼（省下大量時間）
CODE_AT = {}
def code_stub(name):
    if name in RAW:
        if name not in CODE_AT:
            va = STUB_BASE + 0x140000 + 0x20 * len(CODE_AT); CODE_AT[name] = va; uc.mem_write(va, RAW[name])
        return CODE_AT[name]
    if name in ('_initterm', '_initterm_e'):
        if name not in CODE_AT:
            va = STUB_BASE + 0x100000 + 0x40 * len(CODE_AT); CODE_AT[name] = va
            uc.mem_write(va, _mk_iterm(name == '_initterm_e'))
        return CODE_AT[name]
READY = [False]
def stub_for(dll, name):
    cs = code_stub(name)
    if cs: return cs
    k = (dll, name)
    if k not in stub_va:
        va = stub_next[0]; stub_next[0] += 16
        stub_va[k] = va; stubs[va] = k
        try: uc.mem_write(va, b'\xc3')
        except Exception: pass
        if READY[0]: finalize_one(va, k)
    return stub_va[k]

REDIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'redist')
def map_pe(fname):
    path = os.path.join(G, fname)
    if fname.lower() in ('vcruntime140.dll', 'msvcp140.dll', 'ucrtbase.dll'): path = os.path.join(REDIST, fname.lower())
    pe = pefile.PE(path)
    base = BASES[fname.lower()]
    if pe.OPTIONAL_HEADER.ImageBase != base:
        pe.relocate_image(base)
    img = pe.get_memory_mapped_image()
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xfff) & ~0xfff
    uc.mem_map(base, size, UC_PROT_ALL)
    uc.mem_write(base, img)
    exports = {}
    ed = pe.OPTIONAL_HEADER.DATA_DIRECTORY[0]
    if ed.VirtualAddress:   # 直接讀 EAT／名稱表（pefile 的 symbols 會漏掉大型匯出表）
        e = struct.unpack('<IIHHIIIIIII', img[ed.VirtualAddress:ed.VirtualAddress + 40])
        obase, nfunc, nnames, aof, aon, aoo = e[5], e[6], e[7], e[8], e[9], e[10]
        eat = struct.unpack('<%dI' % nfunc, img[aof:aof + 4 * nfunc])
        for i, rva in enumerate(eat):
            if rva and not (ed.VirtualAddress <= rva < ed.VirtualAddress + ed.Size): exports['#%d' % (obase + i)] = base + rva
        names = struct.unpack('<%dI' % nnames, img[aon:aon + 4 * nnames]); ords = struct.unpack('<%dH' % nnames, img[aoo:aoo + 2 * nnames])
        for nr, o in zip(names, ords):
            rva = eat[o]
            if rva and not (ed.VirtualAddress <= rva < ed.VirtualAddress + ed.Size):
                nm = img[nr:img.index(b'\0', nr)].decode(); exports[nm] = base + rva
    mods[fname.lower()] = dict(base=base, pe=pe, exports=exports)
    return mods[fname.lower()]

def resolve_imports(m):
    pe = m['pe']
    pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    for e in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
        d = e.dll.decode().lower()
        d2 = d
        if d.startswith('api-ms-win-crt') and USE_UCRT and 'ucrtbase.dll' in mods: d2 = 'ucrtbase.dll'
        elif d.startswith(('api-ms-win-core', 'ext-ms-win')): d2 = 'kernel32.dll'
        for i in e.imports:
            name = i.name.decode() if i.name else '#%d' % i.ordinal
            if d2 in mods and name in mods[d2]['exports']:
                va = mods[d2]['exports'][name]
            else:
                va = stub_for(d2 if d2 == 'kernel32.dll' else d, name)
            w32(i.address, va)

# ---- 載入 ----
uc.mem_map(STUB_BASE, STUB_SIZE, UC_PROT_ALL)
uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE + 0x1000, UC_PROT_ALL)
uc.mem_map(PEB, 0x2000, UC_PROT_ALL)
for f in ['Poq.exe'] + [r for r in REAL]:
    map_pe(next((x for x in os.listdir(G) if x.lower() == f.lower()), f))
for m in mods.values():
    resolve_imports(m); m['pe'].parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_TLS']])
print('載入模組：', {k: hex(v['base']) for k, v in mods.items()})
print('需要主機提供的匯入（去重）：', len(stub_va))
by_dll = collections.Counter(d for d, n in stub_va)
for d, n in by_dll.most_common(): print('   %-34s %d' % (d, n))

# stub 機器碼：ret N 由 handler 決定，先全部寫 'ret'，handler 會改寫 esp
for va in stubs: uc.mem_write(va, b'\xc3')

# ---- 執行環境：TEB／PEB、堆積、主機端 API ----
uc.mem_write(TEB, b'\0' * 0x1000)
w32(TEB + 0x00, 0xffffffff); w32(TEB + 0x04, STACK_TOP); w32(TEB + 0x08, STACK_TOP - STACK_SIZE)
w32(TEB + 0x18, TEB); w32(TEB + 0x20, 1000); w32(TEB + 0x24, 1001); w32(TEB + 0x30, PEB)
w32(PEB + 0x08, mods['poq.exe']['base'])
# 分段：32 位元平坦模式需要一份 GDT（SS／DS／ES／CS 都是 4GB 平坦、32 位元；FS 的基底指向 TEB）
GDT_ADDR = 0x7FF00000
uc.mem_map(GDT_ADDR, 0x1000, UC_PROT_ALL)
def gdt_entry(base, limit, access, flags):
    return struct.pack('<HHBBBB', limit & 0xffff, base & 0xffff, (base >> 16) & 0xff, access, ((limit >> 16) & 0xf) | (flags << 4), (base >> 24) & 0xff)
gdt = b'\0' * 8 + gdt_entry(0, 0xfffff, 0x9a, 0xc) + gdt_entry(0, 0xfffff, 0x92, 0xc) + gdt_entry(TEB, 0xfff, 0x92, 0x4)
uc.mem_write(GDT_ADDR, gdt)
uc.reg_write(UC_X86_REG_GDTR, (0, GDT_ADDR, len(gdt) - 1, 0))
uc.reg_write(UC_X86_REG_CS, 0x08); uc.reg_write(UC_X86_REG_SS, 0x10); uc.reg_write(UC_X86_REG_DS, 0x10)
uc.reg_write(UC_X86_REG_ES, 0x10); uc.reg_write(UC_X86_REG_GS, 0x10); uc.reg_write(UC_X86_REG_FS, 0x18)

heap_ptr = [HEAP_BASE + 0x1000]
hsize = {}            # 區塊位址 → 配置大小（對齊後）
hfree = collections.defaultdict(list)   # 大小 → 可重用區塊
def alloc(n, zero=True):
    n = max((n + 15) & ~15, 16)
    if hfree[n]:
        p = hfree[n].pop()
        if zero: uc.mem_write(p, b'\0' * n)
        return p
    p = heap_ptr[0]; heap_ptr[0] += n; hsize[p] = n
    if heap_ptr[0] > HEAP_BASE + HEAP_SIZE: raise MemoryError('heap')
    return p
def hfree_(p):
    if p in hsize: hfree[hsize[p]].append(p)
def hrealloc(p, n, zero=False):
    if not p: return alloc(n)
    old = hsize.get(p, 0)
    if old >= n: return p
    q = alloc(n, False); uc.mem_write(q, rd(p, old)); hfree_(p); return q


# 靜態 TLS：fs:[0x2c] → 指標陣列，每個有 TLS 目錄的模組一格
tls_arr = alloc(64 * 4); w32(TEB + 0x2c, tls_arr)
for _i, _m in enumerate([m for m in mods.values() if hasattr(m['pe'], 'DIRECTORY_ENTRY_TLS')]):
    _t = _m['pe'].DIRECTORY_ENTRY_TLS.struct
    _a, _b = _t.StartAddressOfRawData, _t.EndAddressOfRawData
    _p = alloc(_b - _a + _t.SizeOfZeroFill + 16)
    uc.mem_write(_p, rd(_a, _b - _a)); w32(tls_arr + 4 * _i, _p); w32(_t.AddressOfIndex, _i)
    print('TLS', hex(_m['base']), 'index', _i, 'size', _b - _a + _t.SizeOfZeroFill)

API = {}   # (dll, name) → (nargs, cdecl, fn)
def api(dll, name, nargs, cdecl=False):
    def d(fn):
        API[(dll.lower(), name)] = (nargs, cdecl, fn); return fn
    return d

seen_unknown = set()
last_error = [0]
tls_slots = {}
trace = []

CRTISH = ('api-ms-win-crt', 'vcruntime140', 'openal32', 'libvlc', 'ucrtbase')
def _default(*a): return 0
def resolve(k):
    """(nargs, cdecl, fn, known)；None 表示簽章不明，不可安全回傳。"""
    h = API.get(k) or API.get(('crt', k[1]))
    if h: return h + (True,)
    if k in SIG: return (SIG[k], False, _default, True)
    if k[0] == 'msvcp140.dll':
        n, c = msvcp_sig(k[1]); return (n, c, _default, True)
    if k[0].startswith(CRTISH): return (0, True, _default, True)
    return None

import types
TRAMP = STUB_BASE + 0x1F0100
pending = []      # 主機端 generator 堆疊：(gen, 原 stub 位址, 回到 TRAMP 時的 esp, cdecl 引數個數)
skip = [0]
def drive(gen, stub_addr, send=None):
    """推進主機端 generator：它 yield ('call', addr, args[, cdecl]) 就在 guest 執行該函式，回到 TRAMP 時再用回傳值繼續。"""
    try:
        req = gen.send(send)
    except StopIteration as e:
        uc.reg_write(UC_X86_REG_EAX, (e.value or 0) & 0xffffffff)
        if stub_addr is not None: skip[0] = stub_addr; uc.reg_write(UC_X86_REG_EIP, stub_addr)
        return
    _, addr, args = req[:3]; cd = len(args) if len(req) > 3 and req[3] else 0
    sp = uc.reg_read(UC_X86_REG_ESP)
    for a in reversed(args): sp -= 4; w32(sp, a & 0xffffffff)
    sp -= 4; w32(sp, TRAMP)
    pending.append((gen, stub_addr, sp + 4 + (0 if cd else 4 * len(args)), cd))   # 第三項 = 回到 TRAMP 時的 esp（cdecl 的引數由 on_tramp 清掉）
    uc.reg_write(UC_X86_REG_ESP, sp); uc.reg_write(UC_X86_REG_EIP, addr)
def on_tramp(uc_, addr, size, ud):
    if addr != TRAMP: return
    esp = uc.reg_read(UC_X86_REG_ESP)
    while pending and pending[-1][2] != esp and pending[-1][2] < esp: pending.pop()   # 被 unwind 掉的框架（catch 後不會回來）
    gen, st, _e, cd = pending.pop()
    if cd: uc.reg_write(UC_X86_REG_ESP, esp + 4 * cd)
    drive(gen, st, uc.reg_read(UC_X86_REG_EAX))
uc.hook_add(UC_HOOK_CODE, on_tramp, begin=TRAMP, end=TRAMP)

def on_stub(uc_, addr, size, ud):
    if skip[0] == addr: skip[0] = 0; return
    k = stubs.get(addr)
    if not k: return
    dll, name = k
    calls[k] += 1
    TICK[0] += 1
    if TICK[0] % 300000 == 0: print('   [狀態] 主機呼叫 %d 次, 檔案事件 %d, 階段 %s, 畫格 %d, %.0fs, 最常呼叫 %s' % (TICK[0], FCOUNT[0], PHASE[0], len(FRAMES), time.time() - T0, [(n, c) for (d_, n), c in calls.most_common(4)]), flush=True)
    esp = uc.reg_read(UC_X86_REG_ESP)
    ret = u32(esp)
    h = resolve(k)
    if not h:
        if k not in seen_unknown:
            seen_unknown.add(k); unknown.append((dll, name, ret)); print('!! 未實作且簽章不明，停止', dll, name, 'from', hex(ret))
        uc.emu_stop(); return
    nargs, cdecl, fn, known = h
    args = [u32(esp + 4 + 4 * i) for i in range(nargs)]
    if fn is _default and k not in seen_default:
        seen_default.add(k); defaults.append(k)
    if len(trace) < MAXLOG: trace.append((dll, name, [hex(a) for a in args]))
    r = fn(*args)
    if isinstance(r, types.GeneratorType): drive(r, addr); return
    uc.reg_write(UC_X86_REG_EAX, (r or 0) & 0xffffffff)

seen_default = set(); defaults = []; TICK = [0]; T0 = time.time()
uc.hook_add(UC_HOOK_CODE, on_stub, begin=STUB_BASE, end=STUB_BASE + 0xFFFFF)

# 除錯：保留最近 24 條指令，出錯時反組譯列出
import collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
_cs = Cs(CS_ARCH_X86, CS_MODE_32)
ring = collections.deque(maxlen=400)
def _ring(uc_, addr, size, ud):
    ring.append((addr, size))
if os.environ.get('RING'): uc.hook_add(UC_HOOK_CODE, _ring)   # 逐指令記錄很慢，預設關閉
def _it(uc_, addr, size, ud):
    esp = uc.reg_read(UC_X86_REG_ESP)
    print('   [initterm@%x] esp=%x ret=%x a=%x b=%x' % (addr, esp, u32(esp), u32(esp+4), u32(esp+8)))

def dump_ring():
    for a, sz in ring:
        try:
            ins = next(_cs.disasm(bytes(uc.mem_read(a, sz)), a)); print('     %08x  %s %s' % (a, ins.mnemonic, ins.op_str))
        except Exception: print('     %08x  ??' % a)


def crt(name, nargs, cdecl=True):
    def d(fn):
        API[('crt', name)] = (nargs, cdecl, fn); return fn
    return d

# ---- kernel32 ----
K = 'kernel32.dll'
@api(K, 'GetLastError', 0)
def _(): return last_error[0]
@api(K, 'SetLastError', 1)
def _(e): last_error[0] = e
@api(K, 'GetCurrentProcessId', 0)
def _(): return 1000
@api(K, 'GetCurrentThreadId', 0)
def _(): return 1001
@api(K, 'GetCurrentProcess', 0)
def _(): return 0xffffffff
@api(K, 'GetCurrentThread', 0)
def _(): return 0xfffffffe
def wstr(a, mx=260):
    b = b''
    while len(b) < mx * 2:
        c = rd(a + len(b), 2)
        if c == b'\0\0': break
        b += c
    return b.decode('utf-16-le', 'replace')
sysmods = {}   # 假模組：控制代碼 → dll 名稱
def modname(nm):
    nm = nm.lower().replace('/', '\\').split('\\')[-1]
    if nm.startswith(('api-ms-win-core', 'ext-ms-win', 'api-ms-win-eventing', 'api-ms-win-security')): return 'kernel32.dll'
    if not nm.endswith(('.dll', '.exe')): nm += '.dll'
    return nm
def modhandle(nm, load=True):
    nm = modname(nm)
    if nm in mods: return mods[nm]['base']
    for h, n in sysmods.items():
        if n == nm: return h
    if not load and nm not in KNOWN_SYS: return 0
    h = 0x7e000000 + 0x10000 * (len(sysmods) + 1); sysmods[h] = nm; return h
KNOWN_SYS = {'kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'ole32.dll', 'oleaut32.dll', 'ntdll.dll', 'kernelbase.dll', 'shell32.dll', 'imm32.dll', 'winmm.dll', 'comctl32.dll', 'uxtheme.dll', 'shlwapi.dll', 'version.dll', 'ws2_32.dll', 'ucrtbase.dll', 'vcruntime140.dll', 'msvcp140.dll', 'd3d9.dll', 'ddraw.dll', 'wininet.dll', 'gdiplus.dll', 'user32', 'kernel32'}
@api(K, 'GetModuleHandleW', 1)
def _(n): return mods['poq.exe']['base'] if n == 0 else modhandle(wstr(n), False)
@api(K, 'GetModuleHandleA', 1)
def _(n): return mods['poq.exe']['base'] if n == 0 else modhandle(cstr(n), False)
@api(K, 'GetModuleHandleExW', 3)
def _(flags, n, out):
    h = modhandle(wstr(n), False) if not flags & 4 else mods['poq.exe']['base']
    if out: w32(out, h)
    return 1 if h else 0
@api(K, 'GetProcAddress', 2)
def _(h, n):
    name = cstr(n) if n > 0xffff else '#%d' % n
    for m in mods.values():
        if m['base'] == h: return m['exports'].get(name, 0)
    dll = sysmods.get(h)
    if dll is None: return 0
    k = (dll, name)
    if resolve(k) is None and k not in stub_va: print('   [GetProcAddress 無此函式，回傳 NULL]', dll, name); return 0
    return stub_for(dll, name)
@api(K, 'LoadLibraryA', 1)
def _(n): return modhandle(cstr(n))
@api(K, 'LoadLibraryW', 1)
def _(n): return modhandle(wstr(n))
@api(K, 'LoadLibraryExA', 3)
def _(n, f, fl): return modhandle(cstr(n))
@api(K, 'LoadLibraryExW', 3)
def _(n, f, fl): return modhandle(wstr(n))
@api(K, 'FreeLibrary', 1)
def _(h): return 1
@api(K, 'InitializeSListHead', 1)
def _(p): w32(p, 0); w32(p + 4, 0)
@api(K, 'IsProcessorFeaturePresent', 1)
def _(f): return 1 if f in (10,) else 0
@api(K, 'IsDebuggerPresent', 0)
def _(): return 0
@api(K, 'SetUnhandledExceptionFilter', 1)
def _(f): return 0
@api(K, 'UnhandledExceptionFilter', 1)
def _(f): return 0
@api(K, 'TerminateProcess', 2)
def _(h, c): print('TerminateProcess', c); uc.emu_stop(); return 0
@api(K, 'GetSystemTimeAsFileTime', 1)
def _(p):
    t = int((time.time() + 11644473600) * 10000000); w32(p, t & 0xffffffff); w32(p + 4, t >> 32)
@api(K, 'QueryPerformanceCounter', 1)
def _(p):
    t = int(time.perf_counter() * 1e6); w32(p, t & 0xffffffff); w32(p + 4, t >> 32); return 1
@api(K, 'QueryPerformanceFrequency', 1)
def _(p): w32(p, 1000000); w32(p + 4, 0); return 1
@api(K, 'GetTickCount', 0)
def _(): return int(time.time() * 1000) & 0xffffffff
@api(K, 'Sleep', 1)
def _(ms): pass
@api(K, 'GetProcessHeap', 0)
def _(): return 0x1000
@api(K, 'HeapCreate', 3)
def _(a, b, c): return 0x1000
@api(K, 'SetErrorMode', 1)
def _(m): return 0
@api(K, 'LocalAlloc', 2)
def _(f, n): return alloc(n)
@api(K, 'LocalFree', 1)
def _(p): hfree_(p); return 0
@api(K, 'GlobalAlloc', 2)
def _(f, n): return alloc(n)
@api(K, 'GlobalFree', 1)
def _(p): hfree_(p); return 0
@api(K, 'GlobalLock', 1)
def _(p): return p
@api(K, 'GlobalUnlock', 1)
def _(p): return 1
@api(K, 'HeapAlloc', 3)
def _(h, flags, n): return alloc(n)
@api(K, 'HeapFree', 3)
def _(h, f, p): hfree_(p); return 1
@api(K, 'HeapReAlloc', 4)
def _(h, f, p, n): return hrealloc(p, n)
@api(K, 'HeapSize', 3)
def _(h, f, p): return hsize.get(p, 0)
@api(K, 'HeapDestroy', 1)
def _(h): return 1
@api(K, 'HeapValidate', 3)
def _(h, f, p): return 1
@api(K, 'HeapQueryInformation', 5)
def _(*a): return 0
@api(K, 'HeapCompact', 2)
def _(h, f): return 0x1000
@api(K, 'GetStartupInfoW', 1)
def _(p): uc.mem_write(p, b'\0' * 68); w32(p, 68)
@api(K, 'GetStartupInfoA', 1)
def _(p): uc.mem_write(p, b'\0' * 68); w32(p, 68)
@api(K, 'GetCommandLineA', 0)
def _():
    p = alloc(64); uc.mem_write(p, b'Poq.exe\0'); return p
@api(K, 'GetCommandLineW', 0)
def _():
    p = alloc(64); uc.mem_write(p, 'Poq.exe\0'.encode('utf-16-le')); return p
@api(K, 'GetEnvironmentStringsW', 0)
def _():
    p = alloc(16); uc.mem_write(p, b'\0' * 16); return p
@api(K, 'FreeEnvironmentStringsW', 1)
def _(p): return 1
@api(K, 'TlsAlloc', 0)
def _():
    i = len(tls_slots); tls_slots[i] = 0; return i
@api(K, 'TlsGetValue', 1)
def _(i): return tls_slots.get(i, 0)
@api(K, 'TlsSetValue', 2)
def _(i, v): tls_slots[i] = v; return 1
@api(K, 'TlsFree', 1)
def _(i): return 1
@api(K, 'FlsAlloc', 1)
def _(cb):
    i = 100 + len(tls_slots); tls_slots[i] = 0; return i
@api(K, 'FlsGetValue', 1)
def _(i): return tls_slots.get(i, 0)
@api(K, 'FlsSetValue', 2)
def _(i, v): tls_slots[i] = v; return 1
@api(K, 'FlsFree', 1)
def _(i): return 1
@api(K, 'InitializeCriticalSection', 1)
def _(p): pass
@api(K, 'InitializeCriticalSectionAndSpinCount', 2)
def _(p, c): return 1
@api(K, 'InitializeCriticalSectionEx', 3)
def _(p, c, f): return 1
@api(K, 'EnterCriticalSection', 1)
def _(p): pass
@api(K, 'LeaveCriticalSection', 1)
def _(p): pass
@api(K, 'DeleteCriticalSection', 1)
def _(p): pass
@api(K, 'DecodePointer', 1)
def _(p): return p
@api(K, 'EncodePointer', 1)
def _(p): return p
@api(K, 'GetVersionExA', 1)
def _(p):
    uc.mem_write(p + 4, struct.pack('<IIII', 6, 1, 7601, 2)); return 1
@api(K, 'GetVersion', 0)
def _(): return 0x1db10106
@api(K, 'GetSystemInfo', 1)
def _(p): uc.mem_write(p, b'\0' * 36); w32(p + 4, 4096); w32(p + 20, 4)
@api(K, 'GlobalMemoryStatus', 1)
def _(p): uc.mem_write(p, struct.pack('<IIIIIIII', 32, 50, 0x40000000, 0x20000000, 0x80000000, 0x70000000, 0x80000000, 0x70000000))
@api(K, 'MultiByteToWideChar', 6)
def _(cp, fl, s, n, d, dn):
    b = rd(s, n) if n != 0xffffffff else (cstr(s, 4096) + '\0').encode('latin1')
    t = b.decode('latin1').encode('utf-16-le')
    if dn: uc.mem_write(d, t[:dn * 2])
    return len(t) // 2
@api(K, 'WideCharToMultiByte', 8)
def _(cp, fl, s, n, d, dn, dc, pu):
    if n == 0xffffffff:
        b = b''
        while rd(s + len(b), 2) != b'\0\0': b += rd(s + len(b), 2)
        raw = b + b'\0\0'
    else: raw = rd(s, n * 2)
    t = raw.decode('utf-16-le', 'replace').encode('latin1', 'replace')
    if dn: uc.mem_write(d, t[:dn])
    return len(t)
@api(K, 'lstrlenA', 1)
def _(p): return len(cstr(p, 1 << 16))
@api(K, 'GetACP', 0)
def _(): return 950
@api(K, 'GetOEMCP', 0)
def _(): return 950
@api(K, 'GetCPInfo', 2)
def _(cp, p): uc.mem_write(p, b'\0' * 20); w32(p, 2); return 1
@api(K, 'IsDBCSLeadByte', 1)
def _(c): return 1 if 0x81 <= (c & 0xff) <= 0xfe else 0
@api(K, 'GetStdHandle', 1)
def _(i): return 0x100 + (i & 3)
@api(K, 'GetFileType', 1)
def _(h): return 2
@api(K, 'GetStringTypeW', 5)
def _(a, b, c, d, e): return 1
@api(K, 'LCMapStringW', 6)
def _(a, b, c, d, e, f): return 0
@api(K, 'GetLocaleInfoW', 4)
def _(a, b, c, d): return 0

@api(K, 'OutputDebugStringA', 1)
def _(p): print('[dbg]', cstr(p))
@api(K, 'OutputDebugStringW', 1)
def _(p): pass
@api(K, 'CloseHandle', 1)
def _(h): return 1
@api(K, 'GetModuleFileNameA', 3)
def _(h, p, n):
    s = b'Z:\\game\\Poq.exe\0'; uc.mem_write(p, s); return len(s) - 1
@api(K, 'GetModuleFileNameW', 3)
def _(h, p, n):
    s = 'Z:\\game\\Poq.exe\0'.encode('utf-16-le'); uc.mem_write(p, s); return len(s) // 2 - 1


# ---- 控制代碼、同步物件 ----
handles = {}          # 控制代碼 → ('event'|'file'|..., 物件)
_hn = [0x200]
def newh(kind, obj=None):
    _hn[0] += 4; handles[_hn[0]] = (kind, obj); return _hn[0]
for _n in ('CreateEventA', 'CreateEventW'): API[(K, _n)] = (4, False, lambda a, b, c, d: newh('event'))
API[(K, 'CreateMutexA')] = (3, False, lambda a, b, c: newh('mutex'))
API[(K, 'CreateSemaphoreA')] = (4, False, lambda a, b, c, d: newh('sem'))
API[(K, 'CreateThread')] = (6, False, lambda a, b, fn, arg, fl, tid: (print('   [CreateThread]', hex(fn), '(不執行)'), newh('thread'))[1])
for _n, _k in (('SetEvent', 1), ('ResetEvent', 1), ('ReleaseMutex', 1), ('ResumeThread', 1)): API[(K, _n)] = (1, False, lambda h: 1)
API[(K, 'ReleaseSemaphore')] = (3, False, lambda h, n, p: 1)
API[(K, 'WaitForSingleObject')] = (2, False, lambda h, ms: 0)
API[(K, 'WaitForSingleObjectEx')] = (3, False, lambda h, ms, a: 0)
API[(K, 'WaitForMultipleObjects')] = (4, False, lambda n, p, a, ms: 0)
API[(K, 'InterlockedIncrement')] = (1, False, lambda p: (w32(p, u32(p) + 1), u32(p))[1])
API[(K, 'InterlockedDecrement')] = (1, False, lambda p: (w32(p, u32(p) - 1), u32(p))[1])
API[(K, 'InterlockedExchange')] = (2, False, lambda p, v: (u32(p), w32(p, v))[0])
API[(K, 'DisableThreadLibraryCalls')] = (1, False, lambda h: 1)
API[(K, 'GetThreadLocale')] = (0, False, lambda: 0x404)
API[(K, 'GetUserDefaultLCID')] = (0, False, lambda: 0x404)
API[(K, 'GetSystemDefaultLangID')] = (0, False, lambda: 0x404)
API[(K, 'GetUserDefaultUILanguage')] = (0, False, lambda: 0x404)
API[(K, 'GetSystemDefaultUILanguage')] = (0, False, lambda: 0x404)
API[(K, 'GlobalReAlloc')] = (3, False, lambda p, n, f: hrealloc(p, n))
API[(K, 'LocalReAlloc')] = (3, False, lambda p, n, f: hrealloc(p, n))
API[(K, 'LocalLock')] = (1, False, lambda p: p)
API[(K, 'LocalUnlock')] = (1, False, lambda p: 1)
API[(K, 'GlobalHandle')] = (1, False, lambda p: p)
API[(K, 'GlobalSize')] = (1, False, lambda p: hsize.get(p, 0))
API[(K, 'GlobalFlags')] = (1, False, lambda p: 0)
API[(K, 'MulDiv')] = (3, False, lambda a, b, c: (int(a if a < 1 << 31 else a - (1 << 32)) * (b if b < 1 << 31 else b - (1 << 32)) // (c if c < 1 << 31 else c - (1 << 32)) if c else 0xffffffff) & 0xffffffff)
API[(K, 'lstrcpyA')] = (2, False, lambda d, sr: (uc.mem_write(d, rd(sr, len(cstr(sr, 1 << 16)) + 1)), d)[1])
API[(K, 'lstrcatA')] = (2, False, lambda d, sr: (uc.mem_write(d + len(cstr(d, 1 << 16)), rd(sr, len(cstr(sr, 1 << 16)) + 1)), d)[1])
API[(K, 'lstrcpynA')] = (3, False, lambda d, sr, n: (uc.mem_write(d, cstr(sr, max(n - 1, 0)).encode('latin1') + b'\0') if n else 0, d)[1])
API[(K, 'lstrcmpA')] = (2, False, lambda a, b: (cstr(a, 1 << 16) > cstr(b, 1 << 16)) - (cstr(a, 1 << 16) < cstr(b, 1 << 16)) & 0xffffffff)
API[(K, 'lstrcmpiA')] = (2, False, lambda a, b: ((cstr(a, 1 << 16).lower() > cstr(b, 1 << 16).lower()) - (cstr(a, 1 << 16).lower() < cstr(b, 1 << 16).lower())) & 0xffffffff)
API[(K, 'GetLocalTime')] = (1, False, lambda p: uc.mem_write(p, struct.pack('<8H', 2026, 11, 0, 22, 12, 0, 0, 0)))
API[(K, 'GetSystemTime')] = API[(K, 'GetLocalTime')]
API[(K, 'GetEnvironmentVariableA')] = (3, False, lambda n, b, sz: 0)
API[(K, 'GetEnvironmentVariableW')] = (3, False, lambda n, b, sz: 0)
API[(K, 'IsBadWritePtr')] = (2, False, lambda p, n: 0)
API[(K, 'VirtualProtect')] = (4, False, lambda a, n, p, o: (w32(o, 4) if o else 0, 1)[1])
API[(K, 'GetDriveTypeA')] = (1, False, lambda p: 3)
API[(K, 'GetWindowsDirectoryA')] = (2, False, lambda p, n: (uc.mem_write(p, b'C:\\WINDOWS\0'), 10)[1])
API[(K, 'GetTempPathA')] = (2, False, lambda n, p: (uc.mem_write(p, b'C:\\TEMP\\\0'), 8)[1])
API[(K, 'GetSystemDirectoryW')] = (2, False, lambda p, n: (uc.mem_write(p, 'C:\\WINDOWS\\system32\0'.encode('utf-16-le')), 19)[1])
API[(K, 'GetCurrentDirectoryA')] = (2, False, lambda n, p: (uc.mem_write(p, b'Z:\\game\0'), 7)[1])
API[(K, 'SetCurrentDirectoryA')] = (1, False, lambda p: 1)
API[(K, 'GetFullPathNameA')] = (4, False, lambda n, sz, b, pf: (uc.mem_write(b, cstr(n).encode('latin1') + b'\0'), len(cstr(n)))[1])

# ---- 檔案：對應到遊戲資料夾，並記錄存取模式 ----
FILELOG = []      # (序號, 階段, 動作, 檔名, offset, 長度)
PHASE = ['startup']
def hostpath(p):
    p = p.replace('\\', '/')
    if len(p) > 1 and p[1] == ':': p = p[2:]
    p = p.lstrip('/')
    for pre in ('game/', 'Game/'):
        if p.startswith(pre): p = p[len(pre):]
    base = os.path.abspath(G)
    q = os.path.join(base, p)
    d, f = os.path.split(q)
    try:   # 大小寫不分
        if not os.path.exists(q) and os.path.isdir(d):
            for x in os.listdir(d):
                if x.lower() == f.lower(): return os.path.join(d, x)
    except Exception: pass
    return q
FSTAT = collections.OrderedDict()   # (階段, 檔名) → dict(reads, bytes, seeks, opens, minoff, maxoff, first, samples)
FCOUNT = [0]
def flog(act, name, off=0, n=0):
    FCOUNT[0] += 1
    if len(FILELOG) < 3000: FILELOG.append((FCOUNT[0], PHASE[0], act, name, off, n))
    d = FSTAT.setdefault((PHASE[0], name), dict(reads=0, bytes=0, seeks=0, opens=0, lo=1 << 62, hi=0, first=FCOUNT[0], samples=[]))
    if act in ('read', 'fread', '_read'): d['reads'] += 1; d['bytes'] += n; d['lo'] = min(d['lo'], off); d['hi'] = max(d['hi'], off + n)
    elif 'seek' in act: d['seeks'] += 1
    elif act.startswith(('open', 'fopen', 'create')): d['opens'] += 1
    if len(d['samples']) < 24 and act != 'close': d['samples'].append((act, off, n))
@api(K, 'CreateFileA', 7)
def _(name, acc, share, sec, disp, flags, tmpl):
    nm = cstr(name); hp = hostpath(nm)
    if acc & 0x40000000 or disp in (1, 2, 4):   # 寫入／建立：不真的寫到遊戲資料夾，給記憶體檔
        import io; f = io.BytesIO(open(hp, 'rb').read() if os.path.exists(hp) and disp != 2 else b'')
        flog('create-w', nm); return newh('file', [f, nm, True])
    if not os.path.isfile(hp):
        last_error[0] = 2; flog('open-miss', nm); return 0xffffffff
    flog('open', nm); return newh('file', [open(hp, 'rb'), nm, False])
@api(K, 'ReadFile', 5)
def _(h, buf, n, pr, ov):
    o = handles.get(h)
    if not o or o[0] != 'file': return 0
    f, nm, _w = o[1]; off = f.tell(); d = f.read(n)
    flog('read', nm, off, len(d))
    if d: uc.mem_write(buf, d)
    if pr: w32(pr, len(d))
    return 1
@api(K, 'WriteFile', 5)
def _(h, buf, n, pw, ov):
    o = handles.get(h)
    if o and o[0] == 'file':
        f, nm, _w = o[1]; flog('write', nm, f.tell(), n); f.write(rd(buf, n))
    if pw: w32(pw, n)
    return 1
@api(K, 'SetFilePointer', 4)
def _(h, lo, phi, meth):
    o = handles.get(h)
    if not o or o[0] != 'file': return 0xffffffff
    f, nm, _w = o[1]; lo = lo - (1 << 32) if lo >= 1 << 31 else lo
    if phi: hi = u32(phi); lo = (hi << 32) | (lo & 0xffffffff)
    f.seek(lo, meth); flog('seek', nm, f.tell(), 0); return f.tell() & 0xffffffff
@api(K, 'GetFileSize', 2)
def _(h, phi):
    o = handles.get(h)
    if not o or o[0] != 'file': return 0xffffffff
    f = o[1][0]; p = f.tell(); f.seek(0, 2); n = f.tell(); f.seek(p)
    if phi: w32(phi, n >> 32)
    return n & 0xffffffff
@api(K, 'GetFileSizeEx', 2)
def _(h, p):
    n = API[(K, 'GetFileSize')][2](h, 0); w32(p, n); w32(p + 4, 0); return 1
@api(K, 'CloseHandle', 1)
def _(h):
    o = handles.pop(h, None)
    if o and o[0] == 'file': flog('close', o[1][1])
    return 1
@api(K, 'GetFileAttributesA', 1)
def _(p):
    hp = hostpath(cstr(p)); flog('attr', cstr(p))
    return (0x10 if os.path.isdir(hp) else 0x80) if os.path.exists(hp) else 0xffffffff
@api(K, 'GetFileAttributesExA', 3)
def _(p, lvl, out):
    hp = hostpath(cstr(p))
    if not os.path.exists(hp): return 0
    uc.mem_write(out, b'\0' * 36); w32(out, 0x10 if os.path.isdir(hp) else 0x80); sz = os.path.getsize(hp); w32(out + 28, sz); return 1
@api(K, 'GetModuleFileNameA', 3)
def _(h, p, n):
    s = b'Z:\\game\\Poq.exe\0'; uc.mem_write(p, s); return len(s) - 1
@api(K, 'FindFirstFileA', 2)
def _(pat, out):
    import glob; hp = hostpath(cstr(pat)); flog('find', cstr(pat)); l = sorted(glob.glob(hp))
    if not l: last_error[0] = 2; return 0xffffffff
    h = newh('find', [l, 0, out]); _findnext(h, out); return h
def _fill(out, path):
    uc.mem_write(out, b'\0' * 320); w32(out, 0x10 if os.path.isdir(path) else 0x80); w32(out + 32, os.path.getsize(path) if os.path.isfile(path) else 0)
    uc.mem_write(out + 44, os.path.basename(path).encode('latin1', 'replace')[:259] + b'\0')
API[(K, 'FindNextFileA')] = (2, False, lambda h, out: _findnext(h, out))
def _findnext(h, out):
    o = handles.get(h)
    if not o: return 0
    l, i, _ = o[1]
    if i >= len(l): last_error[0] = 18; return 0
    _fill(out, l[i]); o[1][1] = i + 1; return 1

# ---- shlwapi ----
SH = 'shlwapi.dll'
def _pfind(p):
    t = cstr(p, 1 << 16); i = max(t.rfind('\\'), t.rfind('/')) + 1; return t, i
@api(SH, 'PathFindExtensionA', 1)
def _(p):
    t, i = _pfind(p); j = t.rfind('.')
    return p + (j if j >= i else len(t))
@api(SH, 'PathFindFileNameA', 1)
def _(p): return p + _pfind(p)[1]
@api(SH, 'PathRemoveExtensionA', 1)
def _(p):
    t, i = _pfind(p); j = t.rfind('.')
    if j >= i: uc.mem_write(p + j, b'\0')
@api(SH, 'PathIsUNCA', 1)
def _(p): return 1 if cstr(p).startswith('\\\\') else 0
@api(SH, 'PathStripToRootA', 1)
def _(p):
    t = cstr(p); uc.mem_write(p + min(3, len(t)), b'\0'); return 1
@api(SH, 'PathRemoveFileSpecW', 1)
def _(p):
    t = wstr(p); i = max(t.rfind('\\'), t.rfind('/'))
    if i < 0: return 0
    uc.mem_write(p + 2 * i, b'\0\0'); return 1

# ---- UCRT ----
@crt('_set_app_type', 1)
def _(t): pass
@crt('_set_fmode', 1)
def _(m): return 0
@crt('__p__commode', 0)
def _(): return alloc(4)
@crt('__p___argc', 0)
def _():
    p = alloc(4); w32(p, 1); return p
@crt('__p___argv', 0)
def _():
    a = alloc(16); s = alloc(16); uc.mem_write(s, b'Poq.exe\0'); w32(a, s); w32(a + 4, 0)
    p = alloc(4); w32(p, a); return p
@crt('_configure_narrow_argv', 1)
def _(m): return 0
@crt('_initialize_narrow_environment', 0)
def _(): return 0
@crt('_get_initial_narrow_environment', 0)
def _():
    p = alloc(8); return p
@crt('_crt_atexit', 1)
def _(f): return 0
@crt('_initialize_onexit_table', 1)
def _(t): w32(t, 0); w32(t + 4, 0); w32(t + 8, 0); return 0
@crt('_register_onexit_function', 2)
def _(t, f): return 0
@crt('_execute_onexit_table', 1)
def _(t): return 0
@crt('_seh_filter_exe', 2)
def _(a, b): return 0
@crt('_set_new_mode', 1)
def _(m): return 0
@crt('_controlfp_s', 3)
def _(a, b, c): return 0
@crt('_configthreadlocale', 1)
def _(a): return 0
@crt('__setusermatherr', 1)
def _(f): pass
@crt('_cexit', 0)
def _(): pass
@crt('exit', 1)
def _(c): print('exit', c); uc.emu_stop()
@crt('_exit', 1)
def _(c): print('_exit', c); uc.emu_stop()
@crt('_invalid_parameter_noinfo_noreturn', 0)
def _(): print('invalid_parameter'); uc.emu_stop()
@crt('abort', 0)
def _(): print('abort'); uc.emu_stop()
@crt('malloc', 1)
def _(n): return alloc(n)
@crt('calloc', 2)
def _(a, b): return alloc(a * b)
@crt('free', 1)
def _(p): pass
@crt('realloc', 2)
def _(p, n):
    q = alloc(n)
    return q
@crt('_callnewh', 1)
def _(n): return 0
@crt('memcpy', 3)
def _(d, s, n): uc.mem_write(d, rd(s, n)); return d
@crt('memmove', 3)
def _(d, s, n): uc.mem_write(d, rd(s, n)); return d
@crt('memset', 3)
def _(d, c, n): uc.mem_write(d, bytes([c & 0xff]) * n); return d
@crt('strlen', 1)
def _(p): return len(cstr(p, 1 << 20))
@crt('??2@YAPAXI@Z', 1)
def _(n): return alloc(n)
@crt('??3@YAXPAX@Z', 1)
def _(p): pass
@crt('__std_type_info_destroy_list', 1)
def _(a): pass
@crt('_except_handler4_common', 6)
def _(*a): return 1
@crt('__CxxFrameHandler3', 4)
def _(*a): return 1

V = 'vcruntime140.dll'
@api(V, '__vcrt_InitializeCriticalSectionEx', 3)
def _(p, c, f): return 1
@crt('memchr', 3)
def _(p, c, n):
    i = rd(p, n).find(bytes([c & 0xff])); return p + i if i >= 0 else 0
@crt('memcmp', 3)
def _(a, b, n):
    x, y = rd(a, n), rd(b, n); return (x > y) - (x < y)

# ================= UCRT：字串／轉換／printf／stdio／時間 =================
import math, io, re as _re
def wput(p, t): uc.mem_write(p, t.encode('utf-16-le') + b'\0\0')
def sput(p, t): uc.mem_write(p, t if isinstance(t, bytes) else t.encode('latin1', 'replace')); 
def S(p, mx=1 << 20): return cstr(p, mx)
def sgn(v): return v - (1 << 32) if v >= 1 << 31 else v
@crt('strcpy_s', 3)
def _(d, n, sr): t = rd(sr, len(S(sr)) + 1); uc.mem_write(d, t) if len(t) <= n else 0; return 0
@crt('strncpy_s', 4)
def _(d, n, sr, c):
    t = S(sr).encode('latin1'); t = t[:n - 1] if c == 0xffffffff else t[:c]; uc.mem_write(d, t[:max(n - 1, 0)] + b'\0'); return 0
@crt('strcat_s', 3)
def _(d, n, sr): uc.mem_write(d + len(S(d)), rd(sr, len(S(sr)) + 1)); return 0
@crt('strncat', 3)
def _(d, sr, n): uc.mem_write(d + len(S(d)), S(sr).encode('latin1')[:n] + b'\0'); return d
@crt('strncmp', 3)
def _(a, b, n): x, y = rd(a, 0) + S(a).encode('latin1')[:n], S(b).encode('latin1')[:n]; return (x > y) - (x < y) & 0xffffffff
@crt('strncpy', 3)
def _(d, sr, n): t = S(sr).encode('latin1')[:n]; uc.mem_write(d, t + b'\0' * (n - len(t))); return d
@crt('strnlen', 2)
def _(p, n): return min(len(S(p, n)), n)
@crt('_stricmp', 2)
def _(a, b): x, y = S(a).lower(), S(b).lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_strnicmp', 3)
def _(a, b, n): x, y = S(a)[:n].lower(), S(b)[:n].lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_strlwr', 1)
def _(p): sput(p, S(p).lower().encode('latin1') + b'\0'); return p
@crt('_strupr', 1)
def _(p): sput(p, S(p).upper().encode('latin1') + b'\0'); return p
@crt('_strdup', 1)
def _(p): n = len(S(p)) + 1; q = alloc(n); uc.mem_write(q, rd(p, n)); return q
@crt('tolower', 1)
def _(c): return c + 32 if 65 <= c <= 90 else c
@crt('toupper', 1)
def _(c): return c - 32 if 97 <= c <= 122 else c
@crt('isupper', 1)
def _(c): return 1 if 65 <= c <= 90 else 0
@crt('iswspace', 1)
def _(c): return 1 if c in (9, 10, 11, 12, 13, 32, 0x3000) else 0
@crt('wcslen', 1)
def _(p): return len(wstr(p, 1 << 20))
@crt('wcsnlen', 2)
def _(p, n): return min(len(wstr(p, n)), n)
@crt('wcscmp', 2)
def _(a, b): x, y = wstr(a, 1 << 20), wstr(b, 1 << 20); return (x > y) - (x < y) & 0xffffffff
@crt('wcscoll', 2)
def _(a, b): x, y = wstr(a, 1 << 20), wstr(b, 1 << 20); return (x > y) - (x < y) & 0xffffffff
@crt('_wcsicmp', 2)
def _(a, b): x, y = wstr(a, 1 << 20).lower(), wstr(b, 1 << 20).lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_wcsicoll', 2)
def _(a, b): x, y = wstr(a, 1 << 20).lower(), wstr(b, 1 << 20).lower(); return (x > y) - (x < y) & 0xffffffff
@crt('wcscpy_s', 3)
def _(d, n, sr): wput(d, wstr(sr, 1 << 20)); return 0
@crt('wcsncpy_s', 4)
def _(d, n, sr, c): t = wstr(sr, 1 << 20); t = t[:n - 1] if c == 0xffffffff else t[:c]; wput(d, t[:max(n - 1, 0)]); return 0
@crt('wcscat_s', 3)
def _(d, n, sr): wput(d + 2 * len(wstr(d, 1 << 20)), wstr(sr, 1 << 20)); return 0
@crt('_wcslwr_s', 2)
def _(p, n): wput(p, wstr(p, 1 << 20).lower()); return 0
@crt('_wcsupr_s', 2)
def _(p, n): wput(p, wstr(p, 1 << 20).upper()); return 0
@crt('_wcsrev', 1)
def _(p): wput(p, wstr(p, 1 << 20)[::-1]); return p
@crt('wmemcpy_s', 4)
def _(d, n, sr, c): uc.mem_write(d, rd(sr, 2 * c)); return 0
@crt('wcsspn', 2)
def _(a, b):
    x, y = wstr(a, 1 << 20), wstr(b, 1 << 20); i = 0
    while i < len(x) and x[i] in y: i += 1
    return i
@crt('wcscspn', 2)
def _(a, b):
    x, y = wstr(a, 1 << 20), wstr(b, 1 << 20); i = 0
    while i < len(x) and x[i] not in y: i += 1
    return i
@crt('wcspbrk', 2)
def _(a, b):
    x, y = wstr(a, 1 << 20), wstr(b, 1 << 20)
    for i, c in enumerate(x):
        if c in y: return a + 2 * i
    return 0
@crt('wcstombs_s', 5)
def _(pret, d, n, sr, mx):
    t = wstr(sr, 1 << 20).encode('cp950', 'replace')
    if d: uc.mem_write(d, t[:max(n - 1, 0)] + b'\0')
    if pret: w32(pret, len(t) + 1)
    return 0
# 轉換
def _num(txt, base=10):
    m = _re.match(r'\s*([+-]?)(0[xX])?([0-9a-zA-Z]*)', txt)
    if not m: return 0, 0
    sgnc, pre, digs = m.groups()
    if base == 0: base = 16 if pre else (8 if digs.startswith('0') and len(digs) > 1 else 10)
    if base == 16 and False: pass
    v = 0; n = 0
    for ch in digs:
        d = int(ch, 36)
        if d >= base: break
        v = v * base + d; n += 1
    used = m.start(3) + n
    if base == 16 and pre and n == 0: used = m.start(2) + 1
    return (-v if sgnc == '-' else v), used
def _strto(p, endp, base, bits):
    t = S(p); v, used = _num(t, base)
    if endp: w32(endp, p + (used if used else 0))
    return v & ((1 << bits) - 1)
@crt('strtol', 3)
def _(p, e, b): return _strto(p, e, b, 32)
@crt('strtoul', 3)
def _(p, e, b): return _strto(p, e, b, 32)
@crt('atoi', 1)
def _(p): return _strto(p, 0, 10, 32)
@crt('atol', 1)
def _(p): return _strto(p, 0, 10, 32)
@crt('_wtoi', 1)
def _(p): v, _u = _num(wstr(p)); return v & 0xffffffff
def _ret64(v): uc.reg_write(UC_X86_REG_EDX, (v >> 32) & 0xffffffff); return v & 0xffffffff
@crt('strtoll', 3)
def _(p, e, b): t = S(p); v, used = _num(t, b); (w32(e, p + used) if e else 0); return _ret64(v & ((1 << 64) - 1))
@crt('strtoull', 3)
def _(p, e, b): t = S(p); v, used = _num(t, b); (w32(e, p + used) if e else 0); return _ret64(v & ((1 << 64) - 1))
def _setd(v):   # 回傳 double：x87 st0 + xmm0
    try:
        uc.reg_write(UC_X86_REG_XMM0, int.from_bytes(struct.pack('<d', v) + b'\0' * 8, 'little'))
    except Exception: pass
    _setst0(v)
def _setst0(v):
    # 推入一個值到 x87 堆疊：TOP=7、ST0=v
    try:
        uc.reg_write(UC_X86_REG_FPSW, 0x3800); uc.reg_write(UC_X86_REG_FPTAG, 0x3fff)
        import numpy  # noqa
    except Exception: pass
    m, e = math.frexp(v) if v else (0, 0)
    if v == 0: ent = (0, 0)
    else:
        mant = int(abs(m) * (1 << 64)); ent = (mant, (e - 1 + 16383) | (0x8000 if v < 0 else 0))
    try: uc.reg_write(UC_X86_REG_ST0, ent)
    except Exception as ex: print('   [st0 寫入失敗]', ex)
@crt('atof', 1)
def _(p):
    try: v = float(_re.match(r'\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?', S(p)).group(0))
    except Exception: v = 0.0
    _setd(v)
@crt('strtod', 2)
def _(p, e):
    t = S(p); m = _re.match(r'\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?', t)
    v = float(m.group(0)) if m else 0.0
    if e: w32(e, p + (m.end() if m else 0))
    _setd(v)
def _itoa(v, base, signed=True):
    if signed: v = sgn(v)
    if v == 0: return '0'
    neg = v < 0; v = abs(v); o = ''
    while v: o = '0123456789abcdefghijklmnopqrstuvwxyz'[v % base] + o; v //= base
    return ('-' if neg else '') + o
@crt('_itoa', 3)
def _(v, p, b): sput(p, _itoa(v, b).encode() + b'\0'); return p
@crt('_itoa_s', 4)
def _(v, p, n, b): sput(p, _itoa(v, b).encode() + b'\0'); return 0
@crt('_ltoa_s', 4)
def _(v, p, n, b): sput(p, _itoa(v, b).encode() + b'\0'); return 0
@crt('_ultoa_s', 4)
def _(v, p, n, b): sput(p, _itoa(v, b, False).encode() + b'\0'); return 0
@crt('_itow', 3)
def _(v, p, b): wput(p, _itoa(v, b)); return p
@crt('abs', 1)
def _(v): return abs(sgn(v)) & 0xffffffff
@crt('labs', 1)
def _(v): return abs(sgn(v)) & 0xffffffff
@crt('rand', 0)
def _():
    import random; return random.randrange(32768)
@crt('srand', 1)
def _(s): pass
@crt('getenv', 1)
def _(p): return 0
@crt('qsort', 4)
def _(base, n, sz, cmp): print('   [qsort 未實作（需呼叫 guest 比較函式）]', hex(base), n, sz); return 0
# 時間
@crt('clock', 0)
def _(): return int(time.perf_counter() * 1000) & 0xffffffff
@crt('_time64', 1)
def _(p):
    t = int(time.time())
    if p: w32(p, t & 0xffffffff); w32(p + 4, t >> 32)
    return _ret64(t)
def _tm(p, t):
    g = time.localtime(t); uc.mem_write(p, struct.pack('<9i', g.tm_sec, g.tm_min, g.tm_hour, g.tm_mday, g.tm_mon - 1, g.tm_year - 1900, (g.tm_wday + 1) % 7, g.tm_yday - 1, g.tm_isdst))
_tmbuf = [0]
@crt('_localtime64', 1)
def _(pt):
    if not _tmbuf[0]: _tmbuf[0] = alloc(64)
    _tm(_tmbuf[0], u32(pt)); return _tmbuf[0]
@crt('_localtime64_s', 2)
def _(ptm, pt): _tm(ptm, u32(pt)); return 0
@crt('_mktime64', 1)
def _(ptm):
    f = struct.unpack('<9i', rd(ptm, 36)); t = int(time.mktime((f[5] + 1900, f[4] + 1, f[3], f[2], f[1], f[0], 0, 0, -1))); return _ret64(t)
# 數學（x87 回傳）
for _n, _f in (('sin', math.sin), ('cos', math.cos), ('sqrt', math.sqrt), ('exp', math.exp), ('floor', math.floor), ('ceil', math.ceil), ('fabs', abs)):
    def _mk(f):
        def fn(lo, hi):
            v = struct.unpack('<d', struct.pack('<II', lo, hi))[0]
            try: _setd(float(f(v)))
            except Exception: _setd(float('nan'))
        return fn
    API[('crt', _n)] = (2, True, _mk(_f))
@crt('atan2', 4)
def _(a, b, c, d): _setd(math.atan2(struct.unpack('<d', struct.pack('<II', a, b))[0], struct.unpack('<d', struct.pack('<II', c, d))[0]))
for _n, _f in (('_libm_sse2_sin_precise', math.sin), ('_libm_sse2_cos_precise', math.cos), ('_libm_sse2_sqrt_precise', math.sqrt), ('_libm_sse2_atan_precise', math.atan), ('_libm_sse2_acos_precise', math.acos), ('_libm_sse2_asin_precise', math.asin)):
    def _mk2(f):
        def fn():
            v = struct.unpack('<d', uc.reg_read(UC_X86_REG_XMM0).to_bytes(16, 'little')[:8])[0]
            try: r = f(v)
            except Exception: r = float('nan')
            uc.reg_write(UC_X86_REG_XMM0, int.from_bytes(struct.pack('<d', r) + b'\0' * 8, 'little'))
        return fn
    API[('crt', _n)] = (0, True, _mk2(_f))
# 多位元組（Big5 雙位元組）
def _lead(c): return 0x81 <= c <= 0xfe
@crt('_ismbblead', 1)
def _(c): return 1 if _lead(c & 0xff) else 0
@crt('_ismbcalnum', 1)
def _(c): return 1 if chr(c & 0xff).isalnum() and c < 128 else 0
@crt('_ismbcalpha', 1)
def _(c): return 1 if chr(c & 0xff).isalpha() and c < 128 else 0
@crt('_ismbcdigit', 1)
def _(c): return 1 if 48 <= c <= 57 else 0
@crt('_ismbcprint', 1)
def _(c): return 1 if 32 <= c < 127 or c > 255 else 0
@crt('_ismbcspace', 1)
def _(c): return 1 if c in (9, 10, 11, 12, 13, 32) else 0
@crt('_mbctolower', 1)
def _(c): return c + 32 if 65 <= c <= 90 else c
@crt('_mbctoupper', 1)
def _(c): return c - 32 if 97 <= c <= 122 else c
def _mbs(p): return rd(p, len(S(p)))
@crt('_mbsinc', 1)
def _(p): return p + (2 if _lead(rd(p, 1)[0]) and rd(p + 1, 1)[0] else 1)
@crt('_mbsdec', 2)
def _(a, p): return p - 1 if p > a else 0
@crt('_mbscmp', 2)
def _(a, b): x, y = _mbs(a), _mbs(b); return (x > y) - (x < y) & 0xffffffff
@crt('_mbscoll', 2)
def _(a, b): x, y = _mbs(a), _mbs(b); return (x > y) - (x < y) & 0xffffffff
@crt('_mbsicmp', 2)
def _(a, b): x, y = _mbs(a).lower(), _mbs(b).lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_mbsicoll', 2)
def _(a, b): x, y = _mbs(a).lower(), _mbs(b).lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_mbsnbcmp', 3)
def _(a, b, n): x, y = _mbs(a)[:n], _mbs(b)[:n]; return (x > y) - (x < y) & 0xffffffff
@crt('_mbsnbicmp', 3)
def _(a, b, n): x, y = _mbs(a)[:n].lower(), _mbs(b)[:n].lower(); return (x > y) - (x < y) & 0xffffffff
@crt('_mbsnbcpy_s', 4)
def _(d, n, sr, c): t = _mbs(sr)[:c]; uc.mem_write(d, t[:max(n - 1, 0)] + b'\0'); return 0
@crt('_mbschr', 2)
def _(p, c):
    i = _mbs(p).find(bytes([c & 0xff])); return p + i if i >= 0 else 0
@crt('_mbsrchr', 2)
def _(p, c):
    i = _mbs(p).rfind(bytes([c & 0xff])); return p + i if i >= 0 else 0
@crt('_mbsstr', 2)
def _(a, b):
    i = _mbs(a).find(_mbs(b)); return a + i if i >= 0 else 0
@crt('_mbsspn', 2)
def _(a, b):
    x, y = _mbs(a), _mbs(b); i = 0
    while i < len(x) and x[i] in y: i += 1
    return i
@crt('_mbscspn', 2)
def _(a, b):
    x, y = _mbs(a), _mbs(b); i = 0
    while i < len(x) and x[i] not in y: i += 1
    return i
@crt('_mbspbrk', 2)
def _(a, b):
    x, y = _mbs(a), _mbs(b)
    for i, c in enumerate(x):
        if c in y: return a + i
    return 0
@crt('_mbsrev', 1)
def _(p): uc.mem_write(p, _mbs(p)[::-1]); return p
@crt('_mbslwr_s', 2)
def _(p, n): uc.mem_write(p, _mbs(p).lower()); return 0
@crt('_mbsupr_s', 2)
def _(p, n): uc.mem_write(p, _mbs(p).upper()); return 0
@crt('_setmbcp', 1)
def _(c): return 0
@crt('localeconv', 0)
def _():
    p = alloc(64); d = alloc(8); uc.mem_write(d, b'.\0'); e = alloc(8)
    for i in range(0, 24, 4): w32(p + i, e)
    w32(p, d); return p
@crt('_errno', 0)
def _():
    global _errp
    if not _errp: _errp = alloc(4)
    return _errp
_errp = 0
@crt('__doserrno', 0)
def _(): return _crt('_errno')
def _crt(n): return API[('crt', n)][2]()

# ---- printf ----
class VA:
    def __init__(s, p): s.p = p
    def u(s): v = u32(s.p); s.p += 4; return v
    def q(s): lo = u32(s.p); hi = u32(s.p + 4); s.p += 8; return lo | (hi << 32)
    def d(s): v = struct.unpack('<d', rd(s.p, 8))[0]; s.p += 8; return v
def cfmt(f, va, wide=False):
    out = []; i = 0; n = len(f)
    while i < n:
        c = f[i]
        if c != '%': out.append(c); i += 1; continue
        m = _re.compile(r'%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|I64|I32|I|L|w|z|j|t)?([diouxXcCsSeEfgGpn%])').match(f, i)
        if not m: out.append(c); i += 1; continue
        fl, wd, pr, ln, cv = m.groups(); i = m.end()
        if wd == '*': wd = sgn(va.u())
        if pr == '*': pr = sgn(va.u())
        wd = int(wd) if wd not in (None, '') else 0; pr = int(pr) if pr not in (None, '') else None
        if cv == '%': out.append('%'); continue
        if cv in 'di':
            v = va.q() if ln in ('ll', 'I64') else va.u()
            v = v - (1 << 64) if (ln in ('ll', 'I64') and v >= 1 << 63) else (sgn(v) if ln not in ('ll', 'I64') else v)
            if ln == 'h': v = ((v & 0xffff) ^ 0x8000) - 0x8000
            t = str(abs(v)); 
            if pr is not None: t = t.zfill(pr)
            sg = '-' if v < 0 else ('+' if '+' in fl else (' ' if ' ' in fl else '')); t = sg + t
        elif cv in 'ouxX':
            v = va.q() if ln in ('ll', 'I64') else va.u()
            if ln == 'h': v &= 0xffff
            t = {'o': '%o', 'u': '%d', 'x': '%x', 'X': '%X'}[cv] % v
            if pr is not None: t = t.zfill(pr)
            if '#' in fl and v and cv in 'xX': t = '0' + cv + t
        elif cv in 'cC':
            v = va.u(); t = chr(v & 0xffff) if (wide or cv == 'C' or ln in ('l', 'w')) else bytes([v & 0xff]).decode('cp950', 'replace') if v & 0xff else ''
        elif cv in 'sS':
            p = va.u(); wcs = (cv == 's' and (wide or ln in ('l', 'w'))) or (cv == 'S' and not wide and ln not in ('l', 'w') and False) or (cv == 'S' and (not wide))
            if cv == 'S' and wide: wcs = False
            if cv == 's' and not wide and ln not in ('l', 'w'): wcs = False
            if p == 0: t = '(null)'
            elif wcs: t = wstr(p, pr if pr is not None else 1 << 16)
            else: t = S(p, pr if pr is not None else 1 << 16).encode('latin1').decode('cp950', 'replace')
            if pr is not None: t = t[:pr]
        elif cv in 'eEfgG':
            v = va.d(); t = ('%' + ('.%d' % pr if pr is not None else '') + cv) % v
            if '+' in fl and v >= 0: t = '+' + t
        elif cv == 'p': t = '%08X' % va.u()
        else: t = ''
        if len(t) < wd: t = t.ljust(wd) if '-' in fl else (t.rjust(wd, '0') if '0' in fl and cv not in 'sScC' else t.rjust(wd))
        out.append(t)
    return ''.join(out)
def _emit(buf, cnt, t, wide, limit=None):
    """寫入緩衝區（含結尾 0），回傳字元數。"""
    n = len(t)
    if buf:
        cap = cnt if limit is None else limit
        tt = t[:max(cap - 1, 0)] if cap != 0xffffffff else t
        if wide: wput(buf, tt)
        else: uc.mem_write(buf, tt.encode('cp950', 'replace') + b'\0')
    return n
def _fmtarg(p, wide): return wstr(p, 1 << 16) if wide else S(p, 1 << 16).encode('latin1').decode('cp950', 'replace')
for _nm, _wide in (('', False), ('w', True)):
    def _m(wide):
        API[('crt', '__stdio_common_v%ssprintf' % ('w' if wide else ''))] = (7, True, lambda lo, hi, buf, cnt, fmt, loc, va: _emit(buf, cnt, cfmt(_fmtarg(fmt, wide), VA(va), wide), wide, 0xffffffff))
        API[('crt', '__stdio_common_v%ssprintf_s' % ('w' if wide else ''))] = (7, True, lambda lo, hi, buf, cnt, fmt, loc, va: _emit(buf, cnt, cfmt(_fmtarg(fmt, wide), VA(va), wide), wide))
        API[('crt', '__stdio_common_v%ssprintf_p' % ('w' if wide else ''))] = API[('crt', '__stdio_common_v%ssprintf_s' % ('w' if wide else ''))]
        API[('crt', '__stdio_common_v%ssnprintf_s' % ('w' if wide else ''))] = (8, True, lambda lo, hi, buf, cnt, mx, fmt, loc, va: _emit(buf, cnt, cfmt(_fmtarg(fmt, wide), VA(va), wide), wide))
        for suf in ('', '_s', '_p'):
            API[('crt', '__stdio_common_v%sfprintf%s' % ('w' if wide else '', suf))] = (6, True, lambda lo, hi, fp, fmt, loc, va: _fwrite_text(fp, cfmt(_fmtarg(fmt, wide), VA(va), wide)))
    _m(_wide)
API[('crt', '__stdio_common_vswprintf')] = (7, True, lambda lo, hi, buf, cnt, fmt, loc, va: _emit(buf, cnt, cfmt(wstr(fmt, 1 << 16), VA(va), True), True, 0xffffffff))
API[('crt', '__stdio_common_vsnwprintf_s')] = API[('crt', '__stdio_common_vsnprintf_s')]
@crt('__stdio_common_vsscanf', 7)
def _(*a): print('   [sscanf 未實作]', S(a[4])); return 0
@crt('__stdio_common_vsscanf', 7)
def _(*a): print('   [sscanf 未實作]', S(a[4])[:40]); return 0
@crt('__stdio_common_vfscanf', 6)
def _(*a): print('   [fscanf 未實作]'); return 0

# ---- stdio（FILE*／低階 fd）：全部記錄到 FILELOG ----
files = {}        # FILE* → [obj, 名稱, eof, err, 可寫]
fds = {}          # fd → FILE*
def _fopen(path, mode):
    hp = hostpath(path); w = any(c in mode for c in 'wa+')
    if w:
        f = io.BytesIO(open(hp, 'rb').read() if os.path.exists(hp) and 'w' not in mode else b''); flog('fopen-w', path)
    else:
        if not os.path.isfile(hp): flog('fopen-miss', path); return 0
        f = open(hp, 'rb'); flog('fopen', path)
    p = alloc(32); files[p] = [f, path, False, False, w]; return p
@crt('fopen', 2)
def _(p, m): return _fopen(S(p), S(m))
@crt('fclose', 1)
def _(fp):
    o = files.pop(fp, None)
    if o: flog('fclose', o[1])
    return 0
@crt('fread', 4)
def _(buf, sz, n, fp):
    o = files.get(fp)
    if not o or not sz: return 0
    off = o[0].tell(); d = o[0].read(sz * n); flog('fread', o[1], off, len(d))
    if d: uc.mem_write(buf, d)
    if len(d) < sz * n: o[2] = True
    return len(d) // sz
@crt('fwrite', 4)
def _(buf, sz, n, fp):
    o = files.get(fp)
    if o and o[4]: o[0].write(rd(buf, sz * n)); flog('fwrite', o[1], 0, sz * n)
    return n
def _fwrite_text(fp, t):
    o = files.get(fp)
    if o and o[4]: o[0].write(t.encode('cp950', 'replace'))
    elif fp in (STDIO[1], STDIO[2]): print('   [stdout]', t.rstrip())
    return len(t)
@crt('fseek', 3)
def _(fp, off, whence):
    o = files.get(fp)
    if not o: return 0xffffffff
    o[0].seek(sgn(off), whence); o[2] = False; flog('fseek', o[1], o[0].tell(), 0); return 0
@crt('_fseeki64', 4)
def _(fp, lo, hi, whence):
    o = files.get(fp)
    if not o: return 0xffffffff
    v = lo | (hi << 32); v = v - (1 << 64) if v >= 1 << 63 else v
    o[0].seek(v, whence); o[2] = False; flog('fseek', o[1], o[0].tell(), 0); return 0
@crt('ftell', 1)
def _(fp): o = files.get(fp); return o[0].tell() if o else 0xffffffff
@crt('rewind', 1)
def _(fp): o = files.get(fp); o and o[0].seek(0)
@crt('fgetpos', 2)
def _(fp, pos): o = files.get(fp); w32(pos, o[0].tell()); w32(pos + 4, 0); return 0
@crt('fsetpos', 2)
def _(fp, pos): o = files.get(fp); o[0].seek(u32(pos)); return 0
@crt('feof', 1)
def _(fp): o = files.get(fp); return 1 if o and o[2] else 0
@crt('ferror', 1)
def _(fp): return 0
@crt('clearerr_s', 1)
def _(fp): return 0
@crt('fflush', 1)
def _(fp): return 0
@crt('setvbuf', 4)
def _(fp, b, m, n): return 0
@crt('fgetc', 1)
def _(fp):
    o = files.get(fp)
    if not o: return 0xffffffff
    d = o[0].read(1)
    if not d: o[2] = True; return 0xffffffff
    return d[0]
@crt('ungetc', 2)
def _(c, fp): o = files.get(fp); o[0].seek(-1, 1); o[2] = False; return c
@crt('fputc', 2)
def _(c, fp): o = files.get(fp); o and o[4] and o[0].write(bytes([c & 0xff])); return c
@crt('fputs', 2)
def _(p, fp): return _fwrite_text(fp, S(p)) and 0
@crt('fgets', 3)
def _(buf, n, fp):
    o = files.get(fp)
    if not o: return 0
    line = o[0].readline(max(n - 1, 0))
    if not line: o[2] = True; return 0
    uc.mem_write(buf, line + b'\0'); return buf
@crt('_fileno', 1)
def _(fp):
    for fd, f in fds.items():
        if f == fp: return fd
    fd = 3 + len(fds); fds[fd] = fp; return fd
@crt('_fdopen', 2)
def _(fd, m): return fds.get(fd, 0)
@crt('_get_osfhandle', 1)
def _(fd): return 0xffffffff
@crt('_get_stream_buffer_pointers', 4)
def _(*a): return 0
@crt('_filelength', 1)
def _(fd):
    o = files.get(fds.get(fd)); 
    if not o: return 0xffffffff
    p = o[0].tell(); o[0].seek(0, 2); n = o[0].tell(); o[0].seek(p); return n
@crt('_sopen_dispatch', 6)
def _(path, oflag, sh, perm, pfh, bsecure):
    p = S(path); mode = 'wb' if oflag & 0x301 else 'rb'
    fp = _fopen(p, mode)
    if not fp: w32(_crt('_errno'), 2); return 2
    fd = 3 + len(fds); fds[fd] = fp; w32(pfh, fd); return 0
@crt('_open', 3)
def _(path, oflag, perm):
    fp = _fopen(S(path), 'wb' if oflag & 0x301 else 'rb')
    if not fp: return 0xffffffff
    fd = 3 + len(fds); fds[fd] = fp; return fd
@crt('_close', 1)
def _(fd):
    fp = fds.pop(fd, None)
    if fp: API[('crt', 'fclose')][2](fp)
    return 0
@crt('_read', 3)
def _(fd, buf, n):
    o = files.get(fds.get(fd))
    if not o: return 0xffffffff
    off = o[0].tell(); d = o[0].read(n); flog('_read', o[1], off, len(d)); (uc.mem_write(buf, d) if d else 0); return len(d)
@crt('_lseek', 3)
def _(fd, off, wh):
    o = files.get(fds.get(fd))
    if not o: return 0xffffffff
    o[0].seek(sgn(off), wh); flog('_lseek', o[1], o[0].tell(), 0); return o[0].tell()
STDIO = [0, 0, 0]
@crt('__acrt_iob_func', 1)
def _(i):
    if not STDIO[i]: STDIO[i] = alloc(32)
    return STDIO[i]
@crt('_access', 2)
def _(p, m): hp = hostpath(S(p)); flog('access', S(p)); return 0 if os.path.exists(hp) else 0xffffffff
@crt('_stat64i32', 2)
def _(p, st):
    hp = hostpath(S(p)); flog('stat', S(p))
    if not os.path.exists(hp): return 0xffffffff
    uc.mem_write(st, b'\0' * 40); w32(st + 6 if False else st + 6, 0)
    w32(st + 4, 0x8000 if os.path.isfile(hp) else 0x4000); w32(st + 0x14, os.path.getsize(hp) if os.path.isfile(hp) else 0); return 0
@crt('_fullpath', 3)
def _(buf, p, n):
    if not buf: buf = alloc(260)
    t = S(p); uc.mem_write(buf, (t if ':' in t else 'Z:\\game\\' + t).encode('latin1') + b'\0'); return buf
@crt('remove', 1)
def _(p): flog('remove', S(p)); return 0
@crt('rename', 2)
def _(a, b): return 0
@crt('_splitpath_s', 10)
def _(path, dr, drn, dir_, dirn, fn, fnn, ex, exn):
    t = S(path).replace('/', '\\'); d = ''; 
    if len(t) > 1 and t[1] == ':': d, t = t[:2], t[2:]
    i = t.rfind('\\'); di, rest = (t[:i + 1], t[i + 1:]) if i >= 0 else ('', t); j = rest.rfind('.'); f, e = (rest[:j], rest[j:]) if j >= 0 else (rest, '')
    for p_, v in ((dr, d), (dir_, di), (fn, f), (ex, e)):
        if p_: uc.mem_write(p_, v.encode('latin1') + b'\0')
    return 0
@crt('_makepath_s', 6)
def _(buf, n, dr, dir_, fn, ex):
    t = ''.join(S(x) if x else '' for x in (dr, dir_, fn)); e = S(ex) if ex else ''
    uc.mem_write(buf, (t + (('.' if not e.startswith('.') and e else '') + e)).encode('latin1') + b'\0'); return 0
@crt('_lock_file', 1)
def _(fp): pass
@crt('_unlock_file', 1)
def _(fp): pass

# ---- 堆積（可 realloc/_msize）----
blocks = {}
@crt('malloc', 1)
def _(n): p = alloc(max(n, 1)); blocks[p] = n; return p
@crt('calloc', 2)
def _(a, b): p = alloc(max(a * b, 1)); blocks[p] = a * b; return p
@crt('_recalloc', 3)
def _(p, a, b): q = alloc(max(a * b, 1)); blocks[q] = a * b; (uc.mem_write(q, rd(p, min(blocks.get(p, 0), a * b))) if p and blocks.get(p, 0) else 0); return q
@crt('realloc', 2)
def _(p, n):
    q = alloc(max(n, 1)); old = blocks.get(p, 0); blocks[q] = n
    if p and old: uc.mem_write(q, rd(p, min(old, n)))
    return q
@crt('_expand', 2)
def _(p, n): return 0
@crt('_msize', 1)
def _(p): return blocks.get(p, 0x100)
@crt('terminate', 0)
def _(): print('terminate()'); uc.emu_stop()
@crt('_beginthreadex', 6)
def _(sec, stk, fn, arg, fl, ptid): print('   [_beginthreadex]', hex(fn), '(不執行)'); return newh('thread')
@crt('_beginthread', 3)
def _(fn, stk, arg): print('   [_beginthread]', hex(fn), '(不執行)'); return newh('thread')
@crt('_get_narrow_winmain_command_line', 0)
def _():
    p = alloc(16); uc.mem_write(p, b'\0'); return p
@crt('_register_thread_local_exe_atexit_callback', 1)
def _(f): pass
@crt('_crt_at_quick_exit', 1)
def _(f): return 0
@crt('_seh_filter_dll', 2)
def _(a, b): return 0
@crt('_invalid_parameter_noinfo', 0)
def _(): print('_invalid_parameter_noinfo'); 

def _find(p, c, last=False, wide=False):
    step = 2 if wide else 1; r = 0; q = p
    while True:
        v = int.from_bytes(rd(q, step), 'little')
        if v == c: r = q
        if v == 0: break
        q += step
        if r and not last: break
    return q if c == 0 else r
@crt('strchr', 2)
def _(p, c): return _find(p, c & 0xff)
@crt('strrchr', 2)
def _(p, c): return _find(p, c & 0xff, True)
@crt('wcschr', 2)
def _(p, c): return _find(p, c & 0xffff, False, True)
@crt('wcsrchr', 2)
def _(p, c): return _find(p, c & 0xffff, True, True)
@crt('strstr', 2)
def _(a, b):
    i = cstr(a, 1 << 20).encode('latin1').find(cstr(b, 1 << 20).encode('latin1')); return a + i if i >= 0 else 0
@crt('_purecall', 0)
def _(): print('_purecall'); uc.emu_stop()
@crt('_CxxThrowException', 2)
def _(a, b): print('_CxxThrowException obj', hex(a), 'type', hex(b), 'ret', hex(u32(uc.reg_read(UC_X86_REG_ESP)))); uc.emu_stop()
@crt('__std_terminate', 0)
def _(): print('__std_terminate'); uc.emu_stop()


# ================= 視窗管理員（user32 子集）=================
U = 'user32.dll'
wins = {}                    # hwnd → dict
classes = {}                 # 小寫類別名 → dict
atoms = {}                   # atom → 類別名
hooks = {}                   # 掛鉤類型 → [fn]
msgq = []                    # (hwnd, msg, wp, lp)
_hw = [0x10020]
props = {}
focus = [0]; active = [0]; capture = [0]
idle = [0]
WLOG = []                    # 視窗／訊息事件記錄
def newhwnd(): _hw[0] += 0x10; return _hw[0]
DESKTOP = 0x10010
DEFPROC = stub_for(U, 'DefWindowProcA')
def rect_w(p, l, t, r, b): uc.mem_write(p, struct.pack('<4i', l, t, r, b))
def sget(p):   # ANSI 字串或 atom
    return ('#%d' % p) if p < 0x10000 else S(p)
# ---- 預設視窗程序（主機）----
def defproc(h, m, w, l):
    if m == 0x81: return 1                       # NCCREATE
    if m == 0x1: return 0
    if m == 0x14: return 1                       # ERASEBKGND
    if m == 0x10: return 0                       # CLOSE -> 之後由 DestroyWindow 處理
    if m == 0x24: return 0
    if m == 0x83: return 0
    if m == 0x86: return 1
    if m == 0x7f: return 0
    return 0
def sysclass(nm):
    classes[nm.lower()] = dict(name=nm, proc=DEFPROC, style=0, extra=0, wextra=0, inst=0)
for _c in ('button', 'edit', 'static', 'combobox', 'listbox', 'scrollbar', 'sysheader32', 'systreeview32', 'syslistview32', 'tooltips_class32', 'msctls_statusbar32', 'toolbarwindow32', 'msctls_updown32', 'msctls_progress32', 'msctls_trackbar32', 'sysanimate32', 'systabcontrol32', '#32768', '#32770', 'comboboxex32', 'reBarwindow32', 'richedit', 'ComboLBox', 'tooltips_class'): sysclass(_c)
def _regclass(p, ex):
    o = 4 if ex else 0
    f = struct.unpack('<10I', rd(p + o, 40)); nm = S(f[9]) if f[9] >= 0x10000 else '#%d' % f[9]
    classes[nm.lower()] = dict(name=nm, style=f[0], proc=f[1], extra=f[2], wextra=f[3], inst=f[4], icon=f[5], cursor=f[6], bg=f[7])
    a = 0xC000 + len(atoms); atoms[a] = nm.lower(); WLOG.append(('RegisterClass', nm)); return a
API[(U, 'RegisterClassA')] = (1, False, lambda p: _regclass(p, False))
API[(U, 'RegisterClassExA')] = (1, False, lambda p: _regclass(p, True))
@api(U, 'UnregisterClassA', 2)
def _(n, i): classes.pop(sget(n).lower(), None); return 1
def _getclass(inst, n, out, ex):
    c = classes.get(sget(n).lower())
    if not c: last_error[0] = 1411; return 0
    o = 4 if ex else 0
    if ex: w32(out, 48)
    uc.mem_write(out + o, struct.pack('<10I', c['style'], c['proc'], c['extra'], c['wextra'], c['inst'], c.get('icon', 0), c.get('cursor', 0), c.get('bg', 0), 0, 0))
    if ex: w32(out + 44, 0)
    return 1
API[(U, 'GetClassInfoA')] = (3, False, lambda h, n, o: _getclass(h, n, o, False))
API[(U, 'GetClassInfoExA')] = (3, False, lambda h, n, o: _getclass(h, n, o, True))
@api(U, 'GetClassNameA', 3)
def _(h, p, n):
    w = wins.get(h); nm = (w['cls'] if w else '').encode('latin1')[:max(n - 1, 0)]; uc.mem_write(p, nm + b'\0'); return len(nm)
@api(U, 'DefWindowProcA', 4)
def _(h, m, w, l):
    if m == 0x10: return DestroyWindow(h)
    return defproc(h, m, w, l)
def sendmsg(h, m, w, l):
    wd = wins.get(h)
    if not wd: return 0
    p = wd['proc']
    if p in stubs: return defproc(h, m, w, l) if m != 0x10 else DestroyWindow(h)
    WLOG.append(('send', hex(h), hex(m)))
    r = yield ('call', p, (h, m, w, l))
    return r
def _sendgen(h, m, w, l):
    r = yield from sendmsg(h, m, w, l); return r
API[(U, 'SendMessageA')] = (4, False, lambda h, m, w, l: _sendgen(h, m, w, l))
API[(U, 'SendNotifyMessageA')] = (4, False, lambda h, m, w, l: _sendgen(h, m, w, l))
@api(U, 'CallWindowProcA', 5)
def _(prev, h, m, w, l):
    if prev in stubs: return defproc(h, m, w, l)
    r = yield ('call', prev, (h, m, w, l)); return r
@api(U, 'SendDlgItemMessageA', 5)
def _(h, i, m, w, l): return 0
@api(U, 'PostMessageA', 4)
def _(h, m, w, l): msgq.append((h, m, w, l)); return 1
@api(U, 'PostThreadMessageA', 4)
def _(t, m, w, l): msgq.append((0, m, w, l)); return 1
@api(U, 'PostQuitMessage', 1)
def _(c): msgq.append((0, 0x12, c, 0))
def _putmsg(p, m): uc.mem_write(p, struct.pack('<7I', m[0], m[1], m[2], m[3], int(time.time() * 1000) & 0xffffffff, 0, 0))
@api(U, 'GetMessageA', 4)
def _(p, h, a, b):
    PHASE[0] = 'running' if PHASE[0] != 'running' else PHASE[0]
    if not msgq:
        idle[0] += 1
        if idle[0] > 50: print('   [GetMessage：佇列空，閒置 50 次，結束模擬]'); uc.emu_stop(); return 0
        msgq.append((0, 0x113 if False else 0, 0, 0))
    m = msgq.pop(0); _putmsg(p, m); return 0 if m[1] == 0x12 else 1
@api(U, 'PeekMessageA', 5)
def _(p, h, a, b, rm):
    if not msgq: return 0
    m = msgq[0]
    if rm & 1: msgq.pop(0)
    _putmsg(p, m); return 1
@api(U, 'DispatchMessageA', 1)
def _(p):
    h, m, w, l = struct.unpack('<4I', rd(p, 16))
    if not h: return 0
    r = yield from sendmsg(h, m, w, l); return r
API[(U, 'TranslateMessage')] = (1, False, lambda p: 0)
API[(U, 'TranslateAcceleratorA')] = (3, False, lambda h, a, m: 0)
API[(U, 'IsDialogMessageA')] = (2, False, lambda h, m: 0)
API[(U, 'WaitMessage')] = (0, False, lambda: 1)
API[(U, 'MsgWaitForMultipleObjectsEx')] = (5, False, lambda a, b, c, d, e: 0)
API[(U, 'InSendMessage')] = (0, False, lambda: 0)
API[(U, 'GetMessagePos')] = (0, False, lambda: 0)
API[(U, 'GetMessageTime')] = (0, False, lambda: int(time.time() * 1000) & 0xffffffff)
@api(U, 'SetWindowsHookExA', 4)
def _(t, fn, inst, tid): hooks.setdefault(t, []).append(fn); WLOG.append(('hook', t, hex(fn))); return newh('hook', (t, fn))
@api(U, 'UnhookWindowsHookEx', 1)
def _(h):
    o = handles.pop(h, None)
    if o and o[1][1] in hooks.get(o[1][0], []): hooks[o[1][0]].remove(o[1][1])
    return 1
API[(U, 'CallNextHookEx')] = (4, False, lambda h, c, w, l: 0)
# ---- 建立／銷毀 ----
def CreateWindowEx(ex, cls, name, style, x, y, w, h, parent, menu, inst, param):
    if PHASE[0] == 'startup': PHASE[0] = 'window'
    cn = sget(cls); c = classes.get(cn.lower())
    if not c and cls < 0x10000 and atoms.get(cls): c = classes.get(atoms[cls])
    if not c: print('   [CreateWindowEx：找不到類別]', cn); last_error[0] = 1407; return 0
    nm = S(name).encode('latin1').decode('cp950', 'replace') if name else ''
    hw = newhwnd(); wins[hw] = dict(cls=c['name'], proc=c['proc'], style=style, ex=ex, parent=parent, rect=[x if x < 1 << 31 else 0, y if y < 1 << 31 else 0, w if w < 1 << 31 else 640, h if h < 1 << 31 else 480], text=nm, vis=False, id=menu, user=0, inst=inst, extra=bytearray(max(c['wextra'], 8)))
    WLOG.append(('CreateWindowEx', cn, nm, hex(hw), (x, y, w, h), hex(style)))
    print('   [CreateWindowEx] class=%s title=%r hwnd=%#x rect=%s style=%#x' % (cn, nm, hw, (sgn(x), sgn(y), sgn(w), sgn(h)), style))
    cs = alloc(48); uc.mem_write(cs, struct.pack('<12I', param, inst, menu, parent, h, w, y, x, style, name, cls, ex))
    for fn in list(hooks.get(5, [])):
        cb = alloc(8); w32(cb, cs); w32(cb + 4, 0)
        r = yield ('call', fn, (3, hw, cb))
        if r: wins.pop(hw, None); return 0
    r = yield from sendmsg(hw, 0x81, 0, cs)
    if not r: wins.pop(hw, None); return 0
    yield from sendmsg(hw, 0x83, 0, 0)
    r = yield from sendmsg(hw, 0x1, 0, cs)
    if r == 0xffffffff: wins.pop(hw, None); return 0
    if style & 0x10000000: wins[hw]['vis'] = True
    return hw
API[(U, 'CreateWindowExA')] = (12, False, lambda *a: CreateWindowEx(*a))
def DestroyWindow(h):
    if h not in wins: return 0
    yield from sendmsg(h, 0x2, 0, 0)
    yield from sendmsg(h, 0x82, 0, 0)
    wins.pop(h, None); return 1
API[(U, 'DestroyWindow')] = (1, False, lambda h: DestroyWindow(h))
@api(U, 'ShowWindow', 2)
def _(h, c):
    w = wins.get(h)
    if not w: return 0
    old = w['vis']; w['vis'] = c != 0; WLOG.append(('ShowWindow', hex(h), c))
    yield from sendmsg(h, 0x18, 1 if c else 0, 0); return 1 if old else 0
@api(U, 'UpdateWindow', 1)
def _(h): return 1
@api(U, 'IsWindow', 1)
def _(h): return 1 if h in wins or h == DESKTOP else 0
@api(U, 'IsWindowVisible', 1)
def _(h): return 1 if wins.get(h, {}).get('vis') else 0
@api(U, 'IsWindowEnabled', 1)
def _(h): return 1
@api(U, 'IsIconic', 1)
def _(h): return 0
@api(U, 'IsZoomed', 1)
def _(h): return 0
@api(U, 'IsChild', 2)
def _(p, c): return 1 if wins.get(c, {}).get('parent') == p else 0
@api(U, 'GetParent', 1)
def _(h): return wins.get(h, {}).get('parent', 0)
@api(U, 'GetWindow', 2)
def _(h, c): return 0
@api(U, 'GetTopWindow', 1)
def _(h): return 0
@api(U, 'GetDesktopWindow', 0)
def _(): return DESKTOP
@api(U, 'GetActiveWindow', 0)
def _(): return active[0]
@api(U, 'SetActiveWindow', 1)
def _(h): o = active[0]; active[0] = h; return o
@api(U, 'GetForegroundWindow', 0)
def _(): return active[0]
@api(U, 'SetForegroundWindow', 1)
def _(h): active[0] = h; return 1
@api(U, 'GetFocus', 0)
def _(): return focus[0]
@api(U, 'SetFocus', 1)
def _(h): o = focus[0]; focus[0] = h; return o
@api(U, 'GetCapture', 0)
def _(): return capture[0]
@api(U, 'SetCapture', 1)
def _(h): o = capture[0]; capture[0] = h; return o
@api(U, 'ReleaseCapture', 0)
def _(): capture[0] = 0; return 1
@api(U, 'EnableWindow', 2)
def _(h, e): return 0
@api(U, 'GetLastActivePopup', 1)
def _(h): return h
@api(U, 'GetWindowThreadProcessId', 2)
def _(h, p):
    if p: w32(p, 1000)
    return 1001
@api(U, 'GetWindowLongA', 2)
def _(h, i):
    w = wins.get(h)
    if not w: return 0
    i = sgn(i)
    if i >= 0: return int.from_bytes(w['extra'][i:i + 4], 'little') if i + 4 <= len(w['extra']) else 0
    return {-4: w['proc'], -16: w['style'], -20: w['ex'], -21: w['user'], -12: w['id'], -6: w['inst'], -8: w['parent']}.get(i, 0)
@api(U, 'SetWindowLongA', 3)
def _(h, i, v):
    w = wins.get(h)
    if not w: return 0
    i = sgn(i)
    if i >= 0:
        if i + 4 > len(w['extra']): w['extra'] += bytearray(i + 4 - len(w['extra']))
        o = int.from_bytes(w['extra'][i:i + 4], 'little'); w['extra'][i:i + 4] = struct.pack('<I', v); return o
    key = {-4: 'proc', -16: 'style', -20: 'ex', -21: 'user', -12: 'id', -6: 'inst', -8: 'parent'}.get(i)
    if not key: return 0
    o = w[key]; w[key] = v; return o
@api(U, 'GetClassLongA', 2)
def _(h, i): return 0
@api(U, 'SetClassLongA', 3)
def _(h, i, v): return 0
@api(U, 'GetPropA', 2)
def _(h, k): return props.get((h, sget(k)), 0)
@api(U, 'SetPropA', 3)
def _(h, k, v): props[(h, sget(k))] = v; return 1
@api(U, 'RemovePropA', 2)
def _(h, k): return props.pop((h, sget(k)), 0)
@api(U, 'GetWindowRect', 2)
def _(h, p):
    w = wins.get(h)
    if not w: rect_w(p, 0, 0, 1920, 1080); return 1 if h == DESKTOP else 0
    x, y, ww, hh = [sgn(v) for v in w['rect']]; rect_w(p, x, y, x + ww, y + hh); return 1
@api(U, 'GetClientRect', 2)
def _(h, p):
    w = wins.get(h)
    if not w: rect_w(p, 0, 0, 1920, 1080); return 1 if h == DESKTOP else 0
    rect_w(p, 0, 0, sgn(w['rect'][2]), sgn(w['rect'][3])); return 1
@api(U, 'SetWindowPos', 7)
def _(h, after, x, y, cx, cy, fl):
    w = wins.get(h)
    if w:
        if not fl & 2: w['rect'][0], w['rect'][1] = x, y
        if not fl & 1: w['rect'][2], w['rect'][3] = cx, cy
        if fl & 0x40: w['vis'] = True
        if fl & 0x80: w['vis'] = False
    return 1
@api(U, 'MoveWindow', 6)
def _(h, x, y, cx, cy, rp):
    w = wins.get(h)
    if w: w['rect'] = [x, y, cx, cy]
    return 1
API[(U, 'AdjustWindowRect')] = (3, False, lambda p, st, m: 1)
API[(U, 'AdjustWindowRectEx')] = (4, False, lambda p, st, m, ex: 1)
@api(U, 'ClientToScreen', 2)
def _(h, p): return 1
@api(U, 'ScreenToClient', 2)
def _(h, p): return 1
@api(U, 'MapWindowPoints', 4)
def _(a, b, p, n): return 0
@api(U, 'GetWindowTextA', 3)
def _(h, p, n):
    t = wins.get(h, {}).get('text', '').encode('cp950', 'replace')[:max(n - 1, 0)]; uc.mem_write(p, t + b'\0'); return len(t)
@api(U, 'SetWindowTextA', 2)
def _(h, p):
    if h in wins: wins[h]['text'] = S(p); print('   [SetWindowText]', hex(h), S(p))
    return 1
@api(U, 'GetWindowTextLengthA', 1)
def _(h): return len(wins.get(h, {}).get('text', ''))
@api(U, 'GetDlgCtrlID', 1)
def _(h): return wins.get(h, {}).get('id', 0)
@api(U, 'GetDlgItem', 2)
def _(h, i): return 0
@api(U, 'EnumChildWindows', 3)
def _(h, fn, lp): return 1
@api(U, 'FindWindowA', 2)
def _(c, t): return 0
@api(U, 'WindowFromPoint', 2)
def _(x, y): return 0
@api(U, 'GetCursorPos', 1)
def _(p): w32(p, 100); w32(p + 4, 100); return 1
@api(U, 'SetCursorPos', 2)
def _(x, y): return 1
@api(U, 'ShowCursor', 1)
def _(b): return 1 if b else 0
@api(U, 'SetCursor', 1)
def _(c): return 0
@api(U, 'ClipCursor', 1)
def _(p): return 1
@api(U, 'GetAsyncKeyState', 1)
def _(k): return 0
@api(U, 'GetKeyState', 1)
def _(k): return 0
@api(U, 'GetKeyboardState', 1)
def _(p): uc.mem_write(p, b'\0' * 256); return 1
@api(U, 'GetKeyboardLayout', 1)
def _(t): return 0x04040404
@api(U, 'MapVirtualKeyA', 2)
def _(c, t): return c
@api(U, 'SetTimer', 4)
def _(h, i, ms, fn): return i or 1
@api(U, 'KillTimer', 2)
def _(h, i): return 1
@api(U, 'InvalidateRect', 3)
def _(h, r, e): return 1
@api(U, 'ValidateRect', 2)
def _(h, r): return 1
@api(U, 'GetUpdateRect', 3)
def _(h, r, e): return 0
@api(U, 'RedrawWindow', 4)
def _(h, r, g, f): return 1
@api(U, 'GetDC', 1)
def _(h): return newh('dc', None)
@api(U, 'GetWindowDC', 1)
def _(h): return newh('dc', None)
@api(U, 'GetDCEx', 3)
def _(h, r, f): return newh('dc', None)
@api(U, 'ReleaseDC', 2)
def _(h, dc): return 1
@api(U, 'BeginPaint', 2)
def _(h, ps): uc.mem_write(ps, b'\0' * 64); w32(ps, newh('dc', None)); return w32 and u32(ps)
@api(U, 'EndPaint', 2)
def _(h, ps): return 1
SYSM = {0: 1920, 1: 1080, 2: 17, 3: 17, 4: 19, 5: 1, 6: 1, 7: 3, 8: 3, 9: 17, 10: 17, 11: 32, 12: 32, 13: 4, 14: 4, 15: 20, 16: 1920, 17: 1040, 18: 0, 19: 0, 20: 17, 21: 17, 22: 0, 23: 0, 28: 160, 29: 24, 30: 18, 31: 18, 32: 4, 33: 4, 34: 112, 35: 27, 36: 4, 37: 4, 38: 20, 39: 14, 40: 4, 41: 1, 43: 3, 44: 13, 45: 3, 46: 3, 47: 0, 48: 18, 49: 18, 0x44: 4, 0x45: 4, 78: 1920, 79: 1080, 80: 1}
@api(U, 'GetSystemMetrics', 1)
def _(i): return SYSM.get(i, 0)
@api(U, 'SystemParametersInfoA', 4)
def _(a, b, c, d): return 1
@api(U, 'GetSysColor', 1)
def _(i): return 0xc0c0c0 if i != 2 else 0x800000
@api(U, 'GetSysColorBrush', 1)
def _(i): return 0x900 + i
for _n, _a in (('LoadCursorA', 2), ('LoadCursorW', 2), ('LoadIconA', 2), ('LoadIconW', 2), ('LoadBitmapA', 2), ('LoadBitmapW', 2), ('LoadImageA', 6), ('LoadImageW', 6), ('LoadMenuA', 2), ('LoadMenuW', 2), ('LoadAcceleratorsA', 2), ('LoadAcceleratorsW', 2), ('LoadCursorFromFileA', 1), ('CopyIcon', 1), ('CopyImage', 5), ('CreateMenu', 0), ('CreatePopupMenu', 0), ('CreateAcceleratorTableA', 2)):
    API[(U, _n)] = (_a, False, lambda *a: newh('gdiobj', None))
API[(U, 'RegisterWindowMessageA')] = (1, False, lambda p: 0xC100 + (hash(S(p)) & 0xff))
API[(U, 'RegisterClipboardFormatA')] = (1, False, lambda p: 0xC200 + (hash(S(p)) & 0xff))
@api(U, 'MessageBoxA', 4)
def _(h, t, c, f): print('   [MessageBox] %s | %s (flags %#x)' % (S(c) if c else '', S(t) if t else '', f)); return 1
@api(U, 'wsprintfA', 0, True)
def _(): 
    esp = uc.reg_read(UC_X86_REG_ESP); buf = u32(esp + 4); fmt = S(u32(esp + 8)); t = cfmt(fmt, VA(esp + 12)); uc.mem_write(buf, t.encode('cp950', 'replace') + b'\0'); return len(t)
@api(U, 'wvsprintfA', 3)
def _(buf, fmt, va): t = cfmt(S(fmt), VA(va)); uc.mem_write(buf, t.encode('cp950', 'replace') + b'\0'); return len(t)
@api(U, 'CharNextA', 1)
def _(p): return p + (2 if _lead(rd(p, 1)[0]) else 1) if rd(p, 1)[0] else p
@api(U, 'CharUpperA', 1)
def _(p): return p
@api(U, 'IsCharLowerA', 1)
def _(c): return 1 if 97 <= c <= 122 else 0
@api(U, 'SetRect', 5)
def _(p, l, t, r, b): uc.mem_write(p, struct.pack('<4I', l, t, r, b)); return 1
@api(U, 'SetRectEmpty', 1)
def _(p): uc.mem_write(p, b'\0' * 16); return 1
@api(U, 'CopyRect', 2)
def _(d, s): uc.mem_write(d, rd(s, 16)); return 1
@api(U, 'IsRectEmpty', 1)
def _(p): l, t, r, b = struct.unpack('<4i', rd(p, 16)); return 1 if r <= l or b <= t else 0
@api(U, 'EqualRect', 2)
def _(a, b): return 1 if rd(a, 16) == rd(b, 16) else 0
@api(U, 'PtInRect', 3)
def _(r, x, y): l, t, rr, b = struct.unpack('<4i', rd(r, 16)); return 1 if l <= sgn(x) < rr and t <= sgn(y) < b else 0
@api(U, 'OffsetRect', 3)
def _(p, dx, dy): l, t, r, b = struct.unpack('<4i', rd(p, 16)); rect_w(p, l + sgn(dx), t + sgn(dy), r + sgn(dx), b + sgn(dy)); return 1
@api(U, 'InflateRect', 3)
def _(p, dx, dy): l, t, r, b = struct.unpack('<4i', rd(p, 16)); rect_w(p, l - sgn(dx), t - sgn(dy), r + sgn(dx), b + sgn(dy)); return 1
@api(U, 'IntersectRect', 3)
def _(d, a, b):
    x = struct.unpack('<4i', rd(a, 16)); y = struct.unpack('<4i', rd(b, 16)); l, t, r, bt = max(x[0], y[0]), max(x[1], y[1]), min(x[2], y[2]), min(x[3], y[3])
    if r <= l or bt <= t: rect_w(d, 0, 0, 0, 0); return 0
    rect_w(d, l, t, r, bt); return 1
@api(U, 'UnionRect', 3)
def _(d, a, b):
    x = struct.unpack('<4i', rd(a, 16)); y = struct.unpack('<4i', rd(b, 16)); rect_w(d, min(x[0], y[0]), min(x[1], y[1]), max(x[2], y[2]), max(x[3], y[3])); return 1
@api(U, 'GetMonitorInfoA', 2)
def _(m, p): w32(p + 4, 0); w32(p + 8, 0); w32(p + 12, 1920); w32(p + 16, 1080); w32(p + 20, 0); w32(p + 24, 0); w32(p + 28, 1920); w32(p + 32, 1040); w32(p + 36, 1); return 1
@api(U, 'MonitorFromWindow', 2)
def _(h, f): return 0x70001
@api(U, 'MonitorFromPoint', 3)
def _(x, y, f): return 0x70001
@api(U, 'EnumDisplayMonitors', 4)
def _(a, b, c, d): return 1
@api(U, 'GetWindowPlacement', 2)
def _(h, p): uc.mem_write(p, struct.pack('<10I', 44, 0, 1, 0, 0, 0, 0, 0, 640, 480)); return 1
@api(U, 'SetLayeredWindowAttributes', 4)
def _(h, c, a, f): return 1
@api(U, 'TrackMouseEvent', 1)
def _(p): return 1
@api(U, 'GetMenu', 1)
def _(h): return 0
@api(U, 'GetSystemMenu', 2)
def _(h, r): return 0
@api(U, 'SetMenu', 2)
def _(h, m): return 1
@api(U, 'GetMenuItemCount', 1)
def _(m): return 0
@api(U, 'DrawMenuBar', 1)
def _(h): return 1
@api(U, 'GetDoubleClickTime', 0)
def _(): return 500
@api(U, 'GetDialogBaseUnits', 0)
def _(): return 0x00130008
@api(U, 'NotifyWinEvent', 4)
def _(a, b, c, d): return 0
@api(U, 'GetIconInfo', 2)
def _(i, p): uc.mem_write(p, b'\0' * 20); return 1
# 剪貼簿：全部「沒有」
for _n in ('OpenClipboard', 'IsClipboardFormatAvailable', 'GetClipboardData', 'EmptyClipboard', 'CloseClipboard', 'CountClipboardFormats', 'SetClipboardData'): pass

# ================= GDI（記帳用，不真的畫）=================
Gd = 'gdi32.dll'
for _n, _a in (('CreateCompatibleDC', 1), ('CreateDCA', 4), ('CreateCompatibleBitmap', 3), ('CreateBitmap', 5), ('CreateDIBitmap', 6), ('CreateSolidBrush', 1), ('CreatePen', 3), ('CreateRectRgn', 4), ('CreateFontA', 14), ('CreateFontIndirectA', 1), ('CreatePalette', 1), ('CreatePatternBrush', 1), ('CreateRectRgnIndirect', 1), ('CreateEllipticRgn', 4), ('CreateRoundRectRgn', 6), ('CreatePolygonRgn', 3), ('ExtCreatePen', 5), ('CreateHatchBrush', 2), ('CreateDIBPatternBrushPt', 2)):
    API[(Gd, _n)] = (_a, False, lambda *a: newh('gdiobj', None))
@api(Gd, 'CreateDIBSection', 6)
def _(dc, bmi, usage, ppbits, sec, off):
    w, h, bpp = struct.unpack('<iiH', rd(bmi + 4, 10))[0], struct.unpack('<i', rd(bmi + 8, 4))[0], struct.unpack('<H', rd(bmi + 14, 2))[0]
    n = abs(w) * abs(h) * max(bpp // 8, 1) + 64; p = alloc(n)
    if ppbits: w32(ppbits, p)
    return newh('dib', (w, h, bpp, p))
@api(Gd, 'GetStockObject', 1)
def _(i): return 0x800 + i
@api(Gd, 'GetDeviceCaps', 2)
def _(dc, i): return {8: 1920, 10: 1080, 12: 32, 14: 1, 88: 96, 90: 96, 24: 0xffffffff, 38: 0x7e99, 104: 0, 108: 0, 2: 1, 4: 508, 6: 285}.get(i, 0)
@api(Gd, 'SelectObject', 2)
def _(dc, o): return 0x1
@api(Gd, 'DeleteObject', 1)
def _(o): handles.pop(o, None); return 1
@api(Gd, 'DeleteDC', 1)
def _(o): handles.pop(o, None); return 1
@api(Gd, 'GetObjectA', 3)
def _(o, n, p):
    ob = handles.get(o)
    if ob and ob[0] == 'dib' and p:
        w, h, bpp, bits = ob[1]; uc.mem_write(p, struct.pack('<5I', 0, abs(w), abs(h), abs(w) * max(bpp // 8, 1), 1 | (bpp << 16))[:min(n, 20)]); return min(n, 24)
    return 0
@api(Gd, 'SaveDC', 1)
def _(dc): return 1
@api(Gd, 'GetTextMetricsA', 2)
def _(dc, p): uc.mem_write(p, struct.pack('<5i', 16, 13, 3, 0, 0) + b'\0' * 40); w32(p + 20, 8); w32(p + 24, 16); return 1
@api(Gd, 'GetTextExtentPoint32A', 4)
def _(dc, s, n, p): w32(p, 8 * n); w32(p + 4, 16); return 1
@api(Gd, 'GetTextExtentPointA', 4)
def _(dc, s, n, p): w32(p, 8 * n); w32(p + 4, 16); return 1
@api(Gd, 'GetTextExtentPoint32W', 4)
def _(dc, s, n, p): w32(p, 8 * n); w32(p + 4, 16); return 1
@api(Gd, 'GetClipBox', 2)
def _(dc, p): rect_w(p, 0, 0, 1920, 1080); return 2
@api(Gd, 'GetMapMode', 1)
def _(dc): return 1
@api(Gd, 'SetMapMode', 2)
def _(dc, m): return 1
@api(Gd, 'GetBkMode', 1)
def _(dc): return 2
@api(Gd, 'SetBkMode', 2)
def _(dc, m): return 2
@api(Gd, 'GetTextColor', 1)
def _(dc): return 0
@api(Gd, 'SetTextColor', 2)
def _(dc, c): return 0
@api(Gd, 'GetBkColor', 1)
def _(dc): return 0xffffff
@api(Gd, 'SetBkColor', 2)
def _(dc, c): return 0xffffff
@api(Gd, 'GetROP2', 1)
def _(dc): return 13
@api(Gd, 'GetViewportOrgEx', 2)
def _(dc, p): (w32(p, 0), w32(p + 4, 0)) if p else 0; return 1
@api(Gd, 'GetWindowOrgEx', 2)
def _(dc, p): (w32(p, 0), w32(p + 4, 0)) if p else 0; return 1
@api(Gd, 'GetViewportExtEx', 2)
def _(dc, p): (w32(p, 1), w32(p + 4, 1)) if p else 0; return 1
@api(Gd, 'GetWindowExtEx', 2)
def _(dc, p): (w32(p, 1), w32(p + 4, 1)) if p else 0; return 1
@api(Gd, 'GetCurrentPositionEx', 2)
def _(dc, p): (w32(p, 0), w32(p + 4, 0)) if p else 0; return 1
@api(Gd, 'GetNearestColor', 2)
def _(dc, c): return c
@api(Gd, 'GetObjectType', 1)
def _(o): return 1
@api(Gd, 'GetCurrentObject', 2)
def _(dc, t): return 0x800
@api(Gd, 'GetPixel', 3)
def _(dc, x, y): return 0
@api(Gd, 'BitBlt', 9)
def _(*a): return 1
@api(Gd, 'StretchBlt', 11)
def _(*a): return 1
@api(Gd, 'PatBlt', 6)
def _(*a): return 1
@api(Gd, 'TextOutA', 5)
def _(*a): return 1
@api(Gd, 'ExtTextOutA', 8)
def _(*a): return 1
@api(Gd, 'IntersectClipRect', 5)
def _(*a): return 2
@api(Gd, 'SelectClipRgn', 2)
def _(*a): return 2
@api(Gd, 'GdiFlush', 0)
def _(): return 1
@api(Gd, 'RestoreDC', 2)
def _(*a): return 1
@api(Gd, 'DPtoLP', 3)
def _(*a): return 1
@api(Gd, 'LPtoDP', 3)
def _(*a): return 1
@api(Gd, 'GetTextCharsetInfo', 3)
def _(dc, p, f): return 0x88
@api(Gd, 'GetTextFaceA', 3)
def _(dc, n, p):
    if p and n: uc.mem_write(p, b'MingLiU\0')
    return 8
API[(U, 'FillRect')] = (3, False, lambda dc, r, b: 1)
API[(U, 'DrawTextA')] = (5, False, lambda dc, t, n, r, f: 16)

# ================= advapi32／ini／資源 =================
Ad = 'advapi32.dll'
for _n in ('RegOpenKeyExA', 'RegOpenKeyExW', 'RegQueryValueExA', 'RegQueryValueA', 'RegEnumKeyA', 'RegEnumKeyExA', 'RegEnumValueA', 'RegDeleteKeyA', 'RegDeleteValueA'):
    API[(Ad, _n)] = (SIG[(Ad, _n)], False, lambda *a: 2)
API[(Ad, 'RegCloseKey')] = (1, False, lambda h: 0)
API[(Ad, 'RegCreateKeyExA')] = (9, False, lambda *a: 5)    # 拒絕存取
API[(Ad, 'RegSetValueExA')] = (6, False, lambda *a: 0)
API[(Ad, 'RegSetValueA')] = (5, False, lambda *a: 0)
def _ini(fname):
    hp = hostpath(fname); flog('ini', fname)
    d = {}; sec = None
    if os.path.isfile(hp):
        for ln in open(hp, 'rb').read().decode('cp950', 'replace').splitlines():
            ln = ln.strip()
            if ln.startswith('[') and ']' in ln: sec = ln[1:ln.index(']')].lower(); d.setdefault(sec, {})
            elif '=' in ln and sec is not None and not ln.startswith(';'): k, v = ln.split('=', 1); d[sec][k.strip().lower()] = v.strip()
    return d
@api(K, 'GetPrivateProfileStringA', 6)
def _(app, key, dflt, buf, n, fn):
    d = _ini(S(fn)); v = None
    if app and key: v = d.get(S(app).lower(), {}).get(S(key).lower())
    if v is None: v = S(dflt) if dflt else ''
    t = v.encode('cp950', 'replace')[:max(n - 1, 0)]; uc.mem_write(buf, t + b'\0'); return len(t)
@api(K, 'GetPrivateProfileIntA', 4)
def _(app, key, dflt, fn):
    v = _ini(S(fn)).get(S(app).lower(), {}).get(S(key).lower())
    try: return int(v) & 0xffffffff
    except Exception: return dflt
@api(K, 'WritePrivateProfileStringA', 4)
def _(*a): return 1
@api(K, 'GetProfileIntA', 3)
def _(a, b, d): return d
# 資源（PE .rsrc）
def _restree(m, tp, nm):
    pe = m['pe']
    if not hasattr(pe, 'DIRECTORY_ENTRY_RESOURCE'):
        pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
    if not hasattr(pe, 'DIRECTORY_ENTRY_RESOURCE'): return None
    def match(e, key):
        if isinstance(key, int): return e.id == key
        return e.name is not None and str(e.name).lower() == key.lower()
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if not match(t, tp): continue
        for n in t.directory.entries:
            if not match(n, nm): continue
            return n.directory.entries[0].data.struct
    return None
def _resid(p, w=False):
    if p < 0x10000: return p
    t = wstr(p) if w else S(p)
    return int(t[1:]) if t.startswith('#') else t
def _modof(h):
    for m in mods.values():
        if m['base'] == h: return m
    return mods['poq.exe'] if h == 0 else None
resh = {}
def _findres(h, nm, tp, w):
    m = _modof(h); 
    if not m: return 0
    d = _restree(m, _resid(tp, w), _resid(nm, w))
    if not d: last_error[0] = 1814; return 0
    hh = newh('res', (m['base'] + d.OffsetToData, d.Size)); return hh
API[(K, 'FindResourceA')] = (3, False, lambda h, n, t: _findres(h, n, t, False))
API[(K, 'FindResourceW')] = (3, False, lambda h, n, t: _findres(h, n, t, True))
API[(K, 'FindResourceExW')] = (4, False, lambda h, t, n, l: _findres(h, n, t, True))
API[(K, 'LoadResource')] = (2, False, lambda h, r: r)
API[(K, 'LockResource')] = (1, False, lambda r: handles[r][1][0] if r in handles else 0)
API[(K, 'SizeofResource')] = (2, False, lambda h, r: handles[r][1][1] if r in handles else 0)
API[(K, 'CreateActCtxW')] = (1, False, lambda p: newh('actctx'))
API[(K, 'ActivateActCtx')] = (2, False, lambda h, p: (w32(p, 1) if p else 0, 1)[1])
API[(K, 'DeactivateActCtx')] = (2, False, lambda f, c: 1)
API[(K, 'ReleaseActCtx')] = (1, False, lambda h: 0)
API[(K, 'InitializeConditionVariable')] = (1, False, lambda p: w32(p, 0))
API[(K, 'SleepConditionVariableCS')] = (3, False, lambda cv, cs, ms: 1)
API[(K, 'WakeAllConditionVariable')] = (1, False, lambda p: 0)
API[(K, 'WakeConditionVariable')] = (1, False, lambda p: 0)
API[(K, 'FormatMessageA')] = (7, False, lambda *a: 0)
API[(K, 'FindClose')] = (1, False, lambda h: 1)
API[(K, 'OutputDebugStringA')] = (1, False, lambda p: print('   [dbg]', S(p).strip()))
API[(K, 'QueryActCtxW')] = (7, False, lambda fl, h, sub, cls, buf, cb, pw: (uc.mem_write(buf, b'\0' * min(cb, 8)) if buf else 0, w32(pw, 8) if pw else 0, 1)[2])
API[(K, 'GetComputerNameA')] = (2, False, lambda p, n: (uc.mem_write(p, b'PC\0'), w32(n, 2), 1)[2])
API[(K, 'GetTickCount')] = (0, False, lambda: int(time.time() * 1000) & 0xffffffff)
API[('winmm.dll', 'timeGetTime')] = (0, False, lambda: int(time.time() * 1000) & 0xffffffff)
API[('winmm.dll', 'timeBeginPeriod')] = (1, False, lambda a: 0)
API[('winmm.dll', 'timeGetDevCaps')] = (2, False, lambda p, n: (w32(p, 1), w32(p + 4, 1000000), 0)[2])
API[('ole32.dll', 'CoCreateInstance')] = (5, False, lambda *a: 0x80040154)
API[('ole32.dll', 'CoGetClassObject')] = (5, False, lambda *a: 0x80040154)
API[('ole32.dll', 'CoTaskMemAlloc')] = (1, False, lambda n: alloc(n))
API[('ole32.dll', 'CoTaskMemFree')] = (1, False, lambda p: 0)
API[('oleaut32.dll', 'SysAllocString')] = (1, False, lambda p: _bstr(wstr(p, 1 << 16) if p else ''))
API[('oleaut32.dll', 'SysFreeString')] = (1, False, lambda p: 0)
def _bstr(t):
    b = t.encode('utf-16-le'); p = alloc(len(b) + 8); w32(p, len(b)); uc.mem_write(p + 4, b + b'\0\0'); return p + 4


# ================= x86 結構化例外（SEH）分派：RaiseException／RtlUnwind／RtlCaptureContext =================
CTX_OFF = dict(edi=0x9c, esi=0xa0, ebx=0xa4, edx=0xa8, ecx=0xac, eax=0xb0, ebp=0xb4, eip=0xb8, eflags=0xc0, esp=0xc4)
def capture_ctx(p, eip, esp):
    uc.mem_write(p, b'\0' * 0x2cc); w32(p, 0x1007f)
    for nm, reg in (('edi', UC_X86_REG_EDI), ('esi', UC_X86_REG_ESI), ('ebx', UC_X86_REG_EBX), ('edx', UC_X86_REG_EDX), ('ecx', UC_X86_REG_ECX), ('eax', UC_X86_REG_EAX), ('ebp', UC_X86_REG_EBP)):
        w32(p + CTX_OFF[nm], uc.reg_read(reg))
    w32(p + 0xb8, eip); w32(p + 0xc4, esp); w32(p + 0xbc, 0x08); w32(p + 0xc8, 0x10); w32(p + 0x90, 0x18); w32(p + 0xc0, 0x246)
EXC_LOG = []
def raise_exception(code, flags, n, args_p, ret, esp_after):
    rec = alloc(80); ctx = alloc(0x2cc + 16); dc = alloc(32)
    uc.mem_write(rec, b'\0' * 80); w32(rec, code); w32(rec + 4, flags); w32(rec + 12, ret); w32(rec + 16, n)
    if n and args_p: uc.mem_write(rec + 20, rd(args_p, 4 * min(n, 15)))
    capture_ctx(ctx, ret, esp_after)
    EXC_LOG.append((hex(code), hex(ret)))
    print('   [RaiseException %#x from %#x params=%s]' % (code, ret, [hex(u32(rec + 20 + 4 * i)) for i in range(min(n, 4))]))
    if len(EXC_LOG) < 3:
        bp = uc.reg_read(UC_X86_REG_EBP); tr = []
        for _ in range(14):
            try: tr.append(hex(u32(bp + 4))); bp = u32(bp)
            except Exception: break
        print('      ebp 鏈回溯', tr, 'fs:0 =', hex(u32(TEB)))
    frame = u32(TEB)
    while frame != 0xffffffff and frame:
        handler = u32(frame + 4)
        r = yield ('call', handler, (rec, frame, ctx, dc), True)
        if len(EXC_LOG) < 6: print('      handler', hex(handler), 'frame', hex(frame), '→', r)
        if r == 0:   # ContinueExecution：用 ctx 內容恢復
            for nm, reg in (('edi', UC_X86_REG_EDI), ('esi', UC_X86_REG_ESI), ('ebx', UC_X86_REG_EBX), ('ebp', UC_X86_REG_EBP)): uc.reg_write(reg, u32(ctx + CTX_OFF[nm]))
            return 0
        frame = u32(frame)      # ContinueSearch
    print('   [未處理的例外 %#x，結束模擬]' % code); uc.emu_stop(); return 0
@api(K, 'RaiseException', 4)
def _(c, f, n, a):
    esp = uc.reg_read(UC_X86_REG_ESP); ret = u32(esp)
    r = yield from raise_exception(c, f, n, a, ret, esp + 4 + 16)
    return r
@api(K, 'RtlUnwind', 4)
def _(target, ip, rec, rv):
    esp = uc.reg_read(UC_X86_REG_ESP)
    ctx = alloc(0x2cc + 16); dc = alloc(32); capture_ctx(ctx, u32(esp), esp + 4 + 16)
    if not rec: rec = alloc(80); uc.mem_write(rec, b'\0' * 80); w32(rec + 4, 2 if target else 6)
    else: w32(rec + 4, u32(rec + 4) | (2 if target else 6))
    w32(ctx + 0xb0, rv)
    frame = u32(TEB)
    while frame != 0xffffffff and frame and frame != target:
        h = u32(frame + 4)
        w32(TEB, u32(frame))        # 先把這個框架從鏈上拿掉再呼叫其處理常式
        yield ('call', h, (rec, frame, ctx, dc), True)
        frame = u32(TEB)
    return 0
@api(K, 'RtlCaptureContext', 1)
def _(p):
    esp = uc.reg_read(UC_X86_REG_ESP); capture_ctx(p, u32(esp), esp + 8)


# ================= 第三方 DLL 假實作（libvlc／OpenAL：給非空控制代碼，其餘回傳 0）=================
for _n in ('libvlc_new', 'libvlc_media_player_new', 'libvlc_media_new_path', 'libvlc_media_new_location', 'libvlc_media_new_as_node'):
    API[('libvlc.dll', _n)] = (0, True, lambda: newh('vlc'))
API[('libvlc.dll', 'libvlc_media_player_will_play')] = (0, True, lambda: 0)
for _n in ('alcOpenDevice', 'alcCreateContext', 'alcGetCurrentContext', 'alcGetContextsDevice'):
    API[('openal32.dll', _n)] = (0, True, lambda: newh('al'))


# ================= COM 主機物件（DirectDraw 7 / Direct3D 9）=================
com_vt = {}       # 介面名 → vtable 位址
com_obj = {}      # 物件位址 → dict(iface, ref, ...)
COMLOG = collections.Counter()
GUIDS = {0x15e65ec0: 'IDirectDraw7', 0x6c14db80: 'IDirectDraw', 0xb3a6f3e0: 'IDirectDraw2', 0x9c59509a: 'IDirectDraw4', 0x06675a80: 'IDirectDrawSurface7', 0x1c7ce4d1: 'IDirectDrawSurface7'}
def guid_name(p):
    if not p: return None
    return GUIDS.get(u32(p))
def com_define(iface, methods):
    """methods：依 vtable 順序的 [(名稱, 引數個數(不含 this), 實作 或 None)]；預設實作回傳 0（S_OK）。"""
    vt = alloc(4 * len(methods)); com_vt[iface] = vt
    for i, (m, n, fn) in enumerate(methods):
        k = ('com:' + iface, m)
        def mk(fn, m):
            def h(this, *a):
                COMLOG[(iface, m)] += 1
                o = com_obj.get(this)
                if fn is None: return 0
                return fn(o, this, *a)
            return h
        API[k] = (n + 1, False, mk(fn, m))
        w32(vt + 4 * i, stub_for(*k))
    return vt
def com_new(iface, **st):
    o = alloc(16); w32(o, com_vt[iface]); com_obj[o] = dict(iface=iface, ref=1, addr=o, **st); return o
def _qi(o, this, riid, ppv):
    nm = guid_name(riid)
    if nm and nm in com_vt:
        n = com_new(nm, **{k: v for k, v in o.items() if k not in ('iface', 'ref', 'addr')}); w32(ppv, n); return 0
    print('   [COM QueryInterface 不認識的 IID]', o['iface'], rd(riid, 16).hex() if riid else None); w32(ppv, 0); return 0x80004002
def _addref(o, this): o['ref'] += 1; return o['ref']
def _release(o, this): o['ref'] -= 1; return max(o['ref'], 0)
def _std(extra): return [('QueryInterface', 2, _qi), ('AddRef', 0, _addref), ('Release', 0, _release)] + extra
D = lambda name, n, fn=None: (name, n, fn)
# --- DirectDraw 7 ---
def _dd_createsurface(o, this, desc, ppsurf, outer):
    sz = rd(desc, 124); fl = struct.unpack('<I', sz[4:8])[0]; h, w = struct.unpack('<II', sz[8:16]); caps = struct.unpack('<I', sz[104:108])[0]
    bpp = struct.unpack('<I', sz[84:88])[0] if fl & 0x1000 else 32
    if caps & 0x200: w, h = 800, 600       # PRIMARYSURFACE：用視窗大小
    st = dict(w=w, h=h, bpp=bpp or 32, caps=caps, mem=0, pitch=0)
    if w and h: st['pitch'] = (w * ((bpp or 32) // 8) + 3) & ~3; st['mem'] = alloc(st['pitch'] * h + 64)
    s_ = com_new('IDirectDrawSurface7', **st); w32(ppsurf, s_)
    WLOG.append(('DDraw CreateSurface', w, h, bpp, hex(caps), hex(fl))); return 0
com_define('IDirectDraw7', _std([D('Compact', 0), D('CreateClipper', 3, lambda o, t, f, pp, ou: (w32(pp, com_new('IDirectDrawClipper')), 0)[1]), D('CreatePalette', 4, lambda o, t, f, e, pp, ou: (w32(pp, com_new('IDirectDrawPalette')), 0)[1]),
    D('CreateSurface', 3, _dd_createsurface), D('DuplicateSurface', 2), D('EnumDisplayModes', 4), D('EnumSurfaces', 4), D('FlipToGDISurface', 0), D('GetCaps', 2),
    D('GetDisplayMode', 1, lambda o, t, p: (uc.mem_write(p, struct.pack('<IIII', 124, 0x1006 if False else 0x6, 600, 800) + struct.pack('<I', 3200) + b'\0' * 100), 0)[1]),
    D('GetFourCCCodes', 2), D('GetGDISurface', 1), D('GetMonitorFrequency', 1, lambda o, t, p: (w32(p, 60), 0)[1]), D('GetScanLine', 1), D('GetVerticalBlankStatus', 1, lambda o, t, p: (w32(p, 1), 0)[1]),
    D('Initialize', 1), D('RestoreDisplayMode', 0), D('SetCooperativeLevel', 2, lambda o, t, h, fl: (WLOG.append(('DDraw SetCooperativeLevel', hex(h), hex(fl))), 0)[1]),
    D('SetDisplayMode', 5, lambda o, t, w, h, b, r, f: (WLOG.append(('DDraw SetDisplayMode', w, h, b, r)), 0)[1]), D('WaitForVerticalBlank', 2),
    D('GetAvailableVidMem', 3, lambda o, t, caps, tot, free: ((w32(tot, 256 << 20) if tot else 0), (w32(free, 200 << 20) if free else 0), 0)[2]),
    D('GetSurfaceFromDC', 2), D('RestoreAllSurfaces', 0), D('TestCooperativeLevel', 0), D('GetDeviceIdentifier', 2), D('StartModeTest', 3), D('EvaluateMode', 2)]))
for _alias in ('IDirectDraw', 'IDirectDraw2', 'IDirectDraw4'): com_vt[_alias] = com_vt['IDirectDraw7']
def _lock(o, this, rect, desc, fl, ev):
    uc.mem_write(desc + 8, struct.pack('<II', o['h'], o['w'])); w32(desc + 16, o['pitch']); w32(desc + 36, o['mem']); w32(desc + 4, u32(desc + 4) | 0x1 | 0x2 | 0x4 | 0x8 | 0x1000); return 0
com_define('IDirectDrawSurface7', _std([D('AddAttachedSurface', 1), D('AddOverlayDirtyRect', 1), D('Blt', 5), D('BltBatch', 3), D('BltFast', 5), D('DeleteAttachedSurface', 2), D('EnumAttachedSurfaces', 2), D('EnumOverlayZOrders', 3),
    D('Flip', 2, lambda o, t, a, b: (frame_mark('dd-flip'), 0)[1]), D('GetAttachedSurface', 2, lambda o, t, caps, pp: (w32(pp, com_new('IDirectDrawSurface7', **{k: v for k, v in o.items() if k not in ('iface', 'ref', 'addr')})), 0)[1]),
    D('GetBltStatus', 1), D('GetCaps', 1, lambda o, t, p: (w32(p, o['caps']), 0)[1]), D('GetClipper', 1), D('GetColorKey', 2), D('GetDC', 1, lambda o, t, p: (w32(p, newh('dc', None)), 0)[1]), D('GetFlipStatus', 1), D('GetOverlayPosition', 2),
    D('GetPalette', 1), D('GetPixelFormat', 1, lambda o, t, p: (uc.mem_write(p, struct.pack('<8I', 32, 0x40, 0, o['bpp'], 0xf800 if o['bpp'] == 16 else 0xff0000, 0x7e0 if o['bpp'] == 16 else 0xff00, 0x1f if o['bpp'] == 16 else 0xff, 0)), 0)[1]),
    D('GetSurfaceDesc', 1, lambda o, t, p: (uc.mem_write(p, struct.pack('<IIII', 124, 0x7, o['h'], o['w']) + struct.pack('<I', o['pitch'])), 0)[1]), D('Initialize', 2), D('IsLost', 0), D('Lock', 4, _lock), D('ReleaseDC', 1),
    D('Restore', 0), D('SetClipper', 1), D('SetColorKey', 2), D('SetOverlayPosition', 2), D('SetPalette', 1), D('Unlock', 1), D('UpdateOverlay', 5), D('UpdateOverlayDisplay', 1), D('UpdateOverlayZOrder', 2),
    D('GetDDInterface', 1, lambda o, t, p: (w32(p, com_new('IDirectDraw7')), 0)[1]), D('PageLock', 1), D('PageUnlock', 1), D('SetSurfaceDesc', 2), D('SetPrivateData', 4), D('GetPrivateData', 3), D('FreePrivateData', 1),
    D('GetUniquenessValue', 1), D('ChangeUniquenessValue', 0), D('SetPriority', 1), D('GetPriority', 1), D('SetLOD', 1), D('GetLOD', 1)]))
com_define('IDirectDrawClipper', _std([D('GetClipList', 3), D('GetHWnd', 1), D('Initialize', 2), D('IsClipListChanged', 1), D('SetClipList', 2), D('SetHWnd', 2)]))
com_define('IDirectDrawPalette', _std([D('GetCaps', 1), D('GetEntries', 4), D('Initialize', 3), D('SetEntries', 4)]))
@api('ddraw.dll', 'DirectDrawCreate', 3)
def _(guid, pp, outer): WLOG.append(('DirectDrawCreate',)); w32(pp, com_new('IDirectDraw7')); return 0
API[('ddraw.dll', 'DirectDrawCreateEx')] = (4, False, lambda g, pp, iid, outer: (w32(pp, com_new('IDirectDraw7')), 0)[1])


# ---- 寬字元／Ex 檔案 API（真 ucrtbase 的 fopen/_wfopen 走這些）----
def _wpath(p): return wstr(p, 1024)
@api(K, 'AreFileApisANSI', 0)
def _(): return 1
@api(K, 'CreateFileW', 7)
def _(name, acc, share, sec, disp, flags, tmpl):
    nm = _wpath(name); hp = hostpath(nm)
    if acc & 0x40000000 or disp in (1, 2, 4):
        f = io.BytesIO(open(hp, 'rb').read() if os.path.exists(hp) and disp != 2 else b''); flog('create-w', nm); return newh('file', [f, nm, True])
    if not os.path.isfile(hp): last_error[0] = 2; flog('open-miss', nm); return 0xffffffff
    flog('open', nm); return newh('file', [open(hp, 'rb'), nm, False])
@api(K, 'SetFilePointerEx', 5)
def _(h, lo, hi, pnew, meth):
    o = handles.get(h)
    if not o or o[0] != 'file': return 0
    f, nm, _w = o[1]; v = lo | (hi << 32); v = v - (1 << 64) if v >= 1 << 63 else v
    f.seek(v, meth); flog('seek', nm, f.tell(), 0)
    if pnew: w32(pnew, f.tell() & 0xffffffff); w32(pnew + 4, f.tell() >> 32)
    return 1
@api(K, 'GetFileInformationByHandle', 2)
def _(h, p):
    o = handles.get(h)
    if not o: return 0
    n = API[(K, 'GetFileSize')][2](h, 0); uc.mem_write(p, b'\0' * 52); w32(p, 0x80); w32(p + 32, n); w32(p + 40, 1); return 1
@api(K, 'GetFileType', 1)
def _(h): return 1 if handles.get(h, ('',))[0] == 'file' else 2
@api(K, 'GetFileAttributesExW', 3)
def _(p, lvl, out):
    hp = hostpath(_wpath(p)); flog('attr', _wpath(p))
    if not os.path.exists(hp): last_error[0] = 2; return 0
    uc.mem_write(out, b'\0' * 36); w32(out, 0x10 if os.path.isdir(hp) else 0x80); w32(out + 28, os.path.getsize(hp) if os.path.isfile(hp) else 0); return 1
@api(K, 'GetFullPathNameW', 4)
def _(n, sz, b, pf):
    t = _wpath(n); t = t if ':' in t else 'Z:\\game\\' + t; wput(b, t); return len(t)
@api(K, 'GetCurrentDirectoryW', 2)
def _(n, p): wput(p, 'Z:\\game'); return 7
@api(K, 'SetCurrentDirectoryW', 1)
def _(p): return 1
@api(K, 'GetTempPathW', 2)
def _(n, p): wput(p, 'C:\\TEMP\\'); return 8
@api(K, 'FindFirstFileExW', 6)
def _(pat, lvl, out, op, sp, fl):
    import glob; hp = hostpath(_wpath(pat)); flog('find', _wpath(pat)); l = sorted(glob.glob(hp))
    if not l: last_error[0] = 2; return 0xffffffff
    h = newh('find', [l, 0, out]); _findnextw(h, out); return h
def _findnextw(h, out):
    o = handles.get(h)
    if not o: return 0
    l, i, _ = o[1]
    if i >= len(l): last_error[0] = 18; return 0
    p = l[i]; uc.mem_write(out, b'\0' * 592); w32(out, 0x10 if os.path.isdir(p) else 0x80); w32(out + 32, os.path.getsize(p) if os.path.isfile(p) else 0)
    uc.mem_write(out + 44, os.path.basename(p).encode('utf-16-le')[:518] + b'\0\0'); o[1][1] = i + 1; return 1
API[(K, 'FindNextFileW')] = (2, False, lambda h, out: _findnextw(h, out))
for _n, _a in (('DeleteFileW', 1), ('CreateDirectoryW', 2), ('RemoveDirectoryW', 1), ('SetFileAttributesW', 2), ('MoveFileExW', 3), ('GetDiskFreeSpaceW', 5), ('GetDriveTypeW', 1), ('LockFileEx', 6), ('UnlockFileEx', 5), ('SetEndOfFile', 1), ('SetFileTime', 4), ('FlushFileBuffers', 1)):
    API[(K, _n)] = (_a, False, lambda *a: 1)
for _n, _a in (('GetConsoleCP', 0), ('GetConsoleMode', 2), ('SetConsoleMode', 2), ('SetConsoleCtrlHandler', 2), ('GetNumberOfConsoleInputEvents', 2), ('PeekConsoleInputA', 4), ('ReadConsoleInputA', 4), ('ReadConsoleInputW', 4), ('ReadConsoleW', 5), ('WriteConsoleW', 5), ('PeekNamedPipe', 6), ('CreatePipe', 4), ('Beep', 2), ('SetStdHandle', 2), ('GetLogicalDrives', 0), ('SetEnvironmentVariableA', 2), ('SetEnvironmentVariableW', 2), ('SetLocalTime', 1), ('GetExitCodeProcess', 2), ('TzSpecificLocalTimeToSystemTime', 3), ('IsValidCodePage', 1), ('IsValidLocale', 2), ('GetUserDefaultLCID', 0), ('GetDateFormatW', 6), ('GetTimeFormatW', 6), ('EnumSystemLocalesW', 2), ('CompareStringW', 6), ('CompareStringEx', 9), ('GetLocaleInfoEx', 4), ('LCMapStringEx', 9), ('LCMapStringW', 6), ('GetStringTypeW', 5), ('GetTimeZoneInformation', 1), ('VirtualAlloc', 4), ('VirtualQuery', 3), ('InterlockedFlushSList', 1), ('InterlockedPushEntrySList', 2), ('QueryDepthSList', 1), ('FindActCtxSectionStringW', 5), ('GetSystemTimePreciseAsFileTime', 1), ('ExitProcess', 1), ('ExitThread', 1), ('FreeLibraryAndExitThread', 2), ('CreateProcessA', 10), ('CreateProcessW', 10), ('FindFirstFileExA', 6), ('GetModuleHandleExA', 3)):
    if (K, _n) not in API: SIG[(K, _n)] = _a
@api(K, 'VirtualAlloc', 4)
def _(a, n, t, p): return a or alloc(n)
@api(K, 'GetSystemTimePreciseAsFileTime', 1)
def _(p): return API[(K, 'GetSystemTimeAsFileTime')][2](p)
@api(K, 'GetStringTypeW', 4)
def _(t, s, n, out):
    n = n if n != 0xffffffff else len(wstr(s, 1 << 16)) + 1
    for i in range(n):
        c = chr(u32(s + 2 * i) & 0xffff) if True else ''
        v = (1 if c.isupper() else 0) | (2 if c.islower() else 0) | (4 if c.isdigit() else 0) | (8 if c.isspace() else 0) | (0x10 if c in '.,;:!?' else 0) | (0x80 if c.isalpha() else 0)
        uc.mem_write(out + 2 * i, struct.pack('<H', v))
    return 1
@api(K, 'LCMapStringW', 6)
def _(loc, fl, s, n, d, dn):
    t = wstr(s, n if n != 0xffffffff else 1 << 16); t = t.upper() if fl & 0x200 else t.lower() if fl & 0x100 else t
    if dn: uc.mem_write(d, t.encode('utf-16-le')[:dn * 2])
    return len(t)
@api(K, 'LCMapStringEx', 9)
def _(nm, fl, s, n, d, dn, a, b, c): return API[(K, 'LCMapStringW')][2](0, fl, s, n, d, dn)
@api(K, 'CompareStringW', 6)
def _(loc, fl, a, na, b, nb):
    x, y = wstr(a, na if na != 0xffffffff else 1 << 16), wstr(b, nb if nb != 0xffffffff else 1 << 16)
    if fl & 1: x, y = x.lower(), y.lower()
    return 2 + ((x > y) - (x < y))
@api(K, 'CompareStringEx', 9)
def _(nm, fl, a, na, b, nb, x, y, z): return API[(K, 'CompareStringW')][2](0, fl, a, na, b, nb)
@api(K, 'GetLocaleInfoW', 4)
def _(loc, ty, buf, n):
    t = {0x1004: '950', 0x1: '886', 0x1001: 'Chinese', 0x1002: 'zh', 0x2: 'Taiwan'}.get(ty & 0xffff, '0') + '\0'
    if ty & 0x20000000: w32(buf, int(t[:-1]) if t[:-1].isdigit() else 0); return 2
    if n: wput(buf, t[:-1][:n - 1])
    return len(t)
@api(K, 'GetLocaleInfoEx', 4)
def _(nm, ty, buf, n): return API[(K, 'GetLocaleInfoW')][2](0x404, ty, buf, n)
@api(K, 'EnumSystemLocalesW', 2)
def _(fn, fl): return 1
@api(K, 'GetTimeZoneInformation', 1)
def _(p): uc.mem_write(p, b'\0' * 172); return 0
@api(K, 'InterlockedFlushSList', 1)
def _(h): p = u32(h); w32(h, 0); return p
@api(K, 'InterlockedPushEntrySList', 2)
def _(h, e): old = u32(h); w32(e, old); w32(h, e); return old
@api(K, 'ExitProcess', 1)
def _(c): print('ExitProcess', c); uc.emu_stop()
@api(K, 'GetModuleHandleExA', 3)
def _(fl, n, out): return 0
@api(K, 'IsValidCodePage', 1)
def _(c): return 1
@api(K, 'IsValidLocale', 2)
def _(a, b): return 1
@api(K, 'GetConsoleMode', 2)
def _(h, p): return 0


# ================= Direct3D 9 + D3DX9 假實作（只記帳，不繪圖）=================
def _res_methods():
    return [D('GetDevice', 1, lambda o, t, p: (w32(p, o.get('dev', 0)), 0)[1]), D('SetPrivateData', 4), D('GetPrivateData', 3), D('FreePrivateData', 1), D('SetPriority', 1), D('GetPriority', 0), D('PreLoad', 0), D('GetType', 0)]
def _bpp(fmt): return {21: 4, 22: 4, 23: 2, 25: 2, 26: 2, 28: 1, 50: 1, 35: 4, 31: 4}.get(fmt, 4)
def _lockrect(o, this, *a):
    lv, pl = (a[0], a[1]) if len(a) == 4 else (0, a[0])
    if not o.get('mem'): o['pitch'] = max(o['w'] * _bpp(o['fmt']), 4); o['mem'] = alloc(o['pitch'] * max(o['h'], 1) + 64)
    w32(pl, o['pitch']); w32(pl + 4, o['mem']); return 0
def _surf_of(o, tex_dev=None): return com_new('IDirect3DSurface9', w=o['w'], h=o['h'], fmt=o['fmt'], dev=o.get('dev', 0), mem=0, pitch=0)
com_define('IDirect3DTexture9', _std(_res_methods() + [D('SetLOD', 1), D('GetLOD', 0), D('GetLevelCount', 0, lambda o, t: o.get('levels', 1) or 1), D('SetAutoGenFilterType', 1), D('GetAutoGenFilterType', 0), D('GenerateMipSubLevels', 0),
    D('GetLevelDesc', 2, lambda o, t, lv, p: (uc.mem_write(p, struct.pack('<7I', o['fmt'], 3, 0, 0, 0, 0, 0) + struct.pack('<II', o['w'], o['h'])), 0)[1]),
    D('GetSurfaceLevel', 2, lambda o, t, lv, pp: (w32(pp, _surf_of(o)), 0)[1]), D('LockRect', 4, _lockrect), D('UnlockRect', 1), D('AddDirtyRect', 1)]))
com_define('IDirect3DSurface9', _std(_res_methods() + [D('GetContainer', 2), D('GetDesc', 1, lambda o, t, p: (uc.mem_write(p, struct.pack('<7I', o['fmt'], 1, 0, 0, 0, 0, 0) + struct.pack('<II', o['w'], o['h'])), 0)[1]),
    D('LockRect', 3, _lockrect), D('UnlockRect', 0), D('GetDC', 1, lambda o, t, p: (w32(p, newh('dc', None)), 0)[1]), D('ReleaseDC', 1)]))
def _buf_lock(o, this, off, size, ppdata, fl):
    if not o.get('mem'): o['mem'] = alloc(o['len'] + 64)
    w32(ppdata, o['mem'] + off); return 0
for _b in ('IDirect3DVertexBuffer9', 'IDirect3DIndexBuffer9'):
    com_define(_b, _std(_res_methods() + [D('Lock', 4, _buf_lock), D('Unlock', 0), D('GetDesc', 1)]))
com_define('IDirect3DGeneric9', _std([D('m%d' % i, 0) for i in range(30)]))
_dev_log = collections.Counter()
def _mkres(iface, **kw): return lambda o, t, *a: _mkres_(iface, o, a, kw)
def _mkres_(iface, o, a, kw): raise NotImplementedError
def _tex(o, t, w, h, lv, us, fmt, pool, pp, sh):
    WLOG.append(('D3D CreateTexture', w, h, lv, fmt)); w32(pp, com_new('IDirect3DTexture9', w=w, h=h, fmt=fmt, levels=lv, dev=t, mem=0, pitch=0)); return 0
def _vb(o, t, ln, us, fvf, pool, pp, sh): w32(pp, com_new('IDirect3DVertexBuffer9', len=ln, dev=t, mem=0)); return 0
def _ib(o, t, ln, us, fmt, pool, pp, sh): w32(pp, com_new('IDirect3DIndexBuffer9', len=ln, dev=t, mem=0)); return 0
def _surfmk(o, t, w, h, fmt, *rest): pp = rest[-2]; w32(pp, com_new('IDirect3DSurface9', w=w, h=h, fmt=fmt, dev=t, mem=0, pitch=0)); return 0
def _gen(n): return lambda o, t, *a: (w32(a[n], com_new('IDirect3DGeneric9')), 0)[1]
def _bb(o, t, sc, idx, ty, pp): w32(pp, com_new('IDirect3DSurface9', w=o['w'], h=o['h'], fmt=22, dev=t, mem=0, pitch=0)); return 0
def _present(o, t, *a): frame_mark('d3d-present'); o['frames'] = o.get('frames', 0) + 1; WLOG.append(('D3D Present', o['frames'])) if o['frames'] <= 3 else 0; return 0
_Z = lambda name, n: D(name, n)
dev_methods = [D('TestCooperativeLevel', 0), D('GetAvailableTextureMem', 0, lambda o, t: 512 << 20), _Z('EvictManagedResources', 0), D('GetDirect3D', 1), D('GetDeviceCaps', 1, lambda o, t, p: _fillcaps(p)),
    D('GetDisplayMode', 2, lambda o, t, sc, p: (uc.mem_write(p, struct.pack('<4I', 1920, 1080, 60, 22)), 0)[1]), D('GetCreationParameters', 1), _Z('SetCursorProperties', 3), _Z('SetCursorPosition', 3), _Z('ShowCursor', 1),
    D('CreateAdditionalSwapChain', 2, _gen(1)), D('GetSwapChain', 2, _gen(1)), D('GetNumberOfSwapChains', 0, lambda o, t: 1), _Z('Reset', 1), D('Present', 4, _present), D('GetBackBuffer', 4, _bb), _Z('GetRasterStatus', 2),
    _Z('SetDialogBoxMode', 1), _Z('SetGammaRamp', 3), _Z('GetGammaRamp', 2), D('CreateTexture', 8, _tex), D('CreateVolumeTexture', 9, _gen(7)), D('CreateCubeTexture', 7, _gen(5)), D('CreateVertexBuffer', 6, _vb), D('CreateIndexBuffer', 6, _ib),
    D('CreateRenderTarget', 8, _surfmk), D('CreateDepthStencilSurface', 8, _surfmk), _Z('UpdateSurface', 4), _Z('UpdateTexture', 2), _Z('GetRenderTargetData', 2), _Z('GetFrontBufferData', 2), _Z('StretchRect', 5), _Z('ColorFill', 3),
    D('CreateOffscreenPlainSurface', 6, _surfmk), _Z('SetRenderTarget', 2), D('GetRenderTarget', 2, _gen(1)), _Z('SetDepthStencilSurface', 1), D('GetDepthStencilSurface', 1, _gen(0)), _Z('BeginScene', 0), _Z('EndScene', 0), _Z('Clear', 6),
    _Z('SetTransform', 2), _Z('GetTransform', 2), _Z('MultiplyTransform', 2), _Z('SetViewport', 1), _Z('GetViewport', 1), _Z('SetMaterial', 1), _Z('GetMaterial', 1), _Z('SetLight', 2), _Z('GetLight', 2), _Z('LightEnable', 2), _Z('GetLightEnable', 2),
    _Z('SetClipPlane', 2), _Z('GetClipPlane', 2), _Z('SetRenderState', 2), _Z('GetRenderState', 2), D('CreateStateBlock', 2, _gen(1)), _Z('BeginStateBlock', 0), D('EndStateBlock', 1, _gen(0)), _Z('SetClipStatus', 1), _Z('GetClipStatus', 1),
    D('GetTexture', 2, lambda o, t, st, pp: (w32(pp, 0), 0)[1]), _Z('SetTexture', 2), _Z('GetTextureStageState', 3), _Z('SetTextureStageState', 3), _Z('GetSamplerState', 3), _Z('SetSamplerState', 3), _Z('ValidateDevice', 1),
    _Z('SetPaletteEntries', 2), _Z('GetPaletteEntries', 2), _Z('SetCurrentTexturePalette', 1), _Z('GetCurrentTexturePalette', 1), _Z('SetScissorRect', 1), _Z('GetScissorRect', 1), _Z('SetSoftwareVertexProcessing', 1), _Z('GetSoftwareVertexProcessing', 0),
    _Z('SetNPatchMode', 1), _Z('GetNPatchMode', 0), _Z('DrawPrimitive', 3), _Z('DrawIndexedPrimitive', 6), _Z('DrawPrimitiveUP', 4), _Z('DrawIndexedPrimitiveUP', 8), _Z('ProcessVertices', 6), D('CreateVertexDeclaration', 2, _gen(1)),
    _Z('SetVertexDeclaration', 1), D('GetVertexDeclaration', 1, _gen(0)), _Z('SetFVF', 1), _Z('GetFVF', 1), D('CreateVertexShader', 2, _gen(1)), _Z('SetVertexShader', 1), D('GetVertexShader', 1, _gen(0)),
    _Z('SetVertexShaderConstantF', 3), _Z('GetVertexShaderConstantF', 3), _Z('SetVertexShaderConstantI', 3), _Z('GetVertexShaderConstantI', 3), _Z('SetVertexShaderConstantB', 3), _Z('GetVertexShaderConstantB', 3),
    _Z('SetStreamSource', 4), D('GetStreamSource', 4), _Z('SetStreamSourceFreq', 2), _Z('GetStreamSourceFreq', 2), _Z('SetIndices', 1), D('GetIndices', 1, _gen(0)), D('CreatePixelShader', 2, _gen(1)), _Z('SetPixelShader', 1),
    D('GetPixelShader', 1, _gen(0)), _Z('SetPixelShaderConstantF', 3), _Z('GetPixelShaderConstantF', 3), _Z('SetPixelShaderConstantI', 3), _Z('GetPixelShaderConstantI', 3), _Z('SetPixelShaderConstantB', 3), _Z('GetPixelShaderConstantB', 3),
    _Z('DrawRectPatch', 3), _Z('DrawTriPatch', 3), _Z('DeletePatch', 1), D('CreateQuery', 2, _gen(1))]
assert len(dev_methods) == 116, len(dev_methods)
def _fillcaps(p):
    b = bytearray(b'\xff' * 304); struct.pack_into('<II', b, 0, 1, 0)
    for off, v in ((88, 4096), (92, 4096), (96, 2048), (100, 8192), (104, 4096), (108, 16)): struct.pack_into('<I', b, off, v)
    for off in (112, 116, 120, 124, 128): struct.pack_into('<f', b, off, 1e10)
    for off in (132, 136, 140, 144, 148, 152, 156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204, 208): struct.pack_into('<I', b, off, 8)
    uc.mem_write(p, bytes(b)); return 0
com_define('IDirect3DDevice9', _std(dev_methods[:]))
def _cd(o, t, adapter, dtype, hwnd, beh, pp, ppdev):
    w, h = struct.unpack('<II', rd(pp, 8)); WLOG.append(('D3D CreateDevice', hex(hwnd), w, h, hex(beh), 'windowed=%d' % u32(pp + 32))); w32(ppdev, com_new('IDirect3DDevice9', w=w or 800, h=h or 600, d3d=t)); return 0
com_define('IDirect3D9', _std([D('RegisterSoftwareDevice', 1), D('GetAdapterCount', 0, lambda o, t: 1), D('GetAdapterIdentifier', 3, lambda o, t, a, f, p: (uc.mem_write(p, b'Emu\0' + b'\0' * 1200), 0)[1]), D('GetAdapterModeCount', 2, lambda o, t, a, f: 1),
    D('EnumAdapterModes', 4, lambda o, t, a, f, i, p: (uc.mem_write(p, struct.pack('<4I', 1920, 1080, 60, 22)), 0)[1]), D('GetAdapterDisplayMode', 2, lambda o, t, a, p: (uc.mem_write(p, struct.pack('<4I', 1920, 1080, 60, 22)), 0)[1]),
    D('CheckDeviceType', 5), D('CheckDeviceFormat', 6), D('CheckDeviceMultiSampleType', 6), D('CheckDepthStencilMatch', 5), D('CheckDeviceFormatConversion', 4), D('GetDeviceCaps', 3, lambda o, t, a, ty, p: _fillcaps(p)),
    D('GetAdapterMonitor', 1, lambda o, t, a: 0x70001), D('CreateDevice', 6, _cd)]))
@api('d3d9.dll', 'Direct3DCreate9', 1)
def _(ver): WLOG.append(('Direct3DCreate9', ver)); return com_new('IDirect3D9')
# ---- D3DX9（浮點引數是堆疊上的 IEEE single）----
DX = 'd3dx9_43.dll'
def _f(v): return struct.unpack('<f', struct.pack('<I', v))[0]
def _rm(p): return list(struct.unpack('<16f', rd(p, 64)))
def _wm(p, m): uc.mem_write(p, struct.pack('<16f', *m)); return p
def _rv(p, n=3): return list(struct.unpack('<%df' % n, rd(p, 4 * n)))
def _wv(p, v): uc.mem_write(p, struct.pack('<%df' % len(v), *v)); return p
def _mmul(a, b): return [sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4)) for r in range(4) for c in range(4)]
def _inv(m):
    n = 4; a = [m[i * 4:(i + 1) * 4] + [1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(a[r][c]))
        if abs(a[p][c]) < 1e-12: return None
        a[c], a[p] = a[p], a[c]; pv = a[c][c]; a[c] = [x / pv for x in a[c]]
        for r in range(n):
            if r != c: f = a[r][c]; a[r] = [x - f * y for x, y in zip(a[r], a[c])]
    return [a[i][4 + j] for i in range(4) for j in range(4)]
def _norm(v):
    l = math.sqrt(sum(x * x for x in v)) or 1.0; return [x / l for x in v]
def _cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def _dot(a, b): return sum(x * y for x, y in zip(a, b))
@api(DX, 'D3DXMatrixMultiply', 3)
def _(o, a, b): return _wm(o, _mmul(_rm(a), _rm(b)))
@api(DX, 'D3DXMatrixTranspose', 2)
def _(o, a): m = _rm(a); return _wm(o, [m[c * 4 + r] for r in range(4) for c in range(4)])
@api(DX, 'D3DXMatrixInverse', 3)
def _(o, pdet, a):
    r = _inv(_rm(a))
    if r is None: return 0
    if pdet: w32(pdet, struct.unpack('<I', struct.pack('<f', 1.0))[0])
    return _wm(o, r)
@api(DX, 'D3DXMatrixTranslation', 4)
def _(o, x, y, z): return _wm(o, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, _f(x), _f(y), _f(z), 1])
@api(DX, 'D3DXMatrixLookAtLH', 4)
def _(o, eye, at, up):
    e, a, u = _rv(eye), _rv(at), _rv(up); z = _norm([a[i] - e[i] for i in range(3)]); x = _norm(_cross(u, z)); y = _cross(z, x)
    return _wm(o, [x[0], y[0], z[0], 0, x[1], y[1], z[1], 0, x[2], y[2], z[2], 0, -_dot(x, e), -_dot(y, e), -_dot(z, e), 1])
@api(DX, 'D3DXMatrixPerspectiveFovLH', 5)
def _(o, fovy, asp, zn, zf):
    fovy, asp, zn, zf = _f(fovy), _f(asp), _f(zn), _f(zf); ys = 1 / math.tan(fovy / 2); xs = ys / asp
    return _wm(o, [xs, 0, 0, 0, 0, ys, 0, 0, 0, 0, zf / (zf - zn), 1, 0, 0, -zn * zf / (zf - zn), 0])
@api(DX, 'D3DXMatrixRotationQuaternion', 2)
def _(o, q):
    x, y, z, w = _rv(q, 4)
    return _wm(o, [1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0, 2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0, 2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0, 0, 0, 0, 1])
@api(DX, 'D3DXQuaternionMultiply', 3)
def _(o, a, b):
    x1, y1, z1, w1 = _rv(a, 4); x2, y2, z2, w2 = _rv(b, 4)
    return _wv(o, [w2 * x1 + x2 * w1 + y2 * z1 - z2 * y1, w2 * y1 + y2 * w1 + z2 * x1 - x2 * z1, w2 * z1 + z2 * w1 + x2 * y1 - y2 * x1, w2 * w1 - x2 * x1 - y2 * y1 - z2 * z1])
@api(DX, 'D3DXQuaternionRotationAxis', 3)
def _(o, ax, ang):
    v = _norm(_rv(ax)); h = _f(ang) / 2; s = math.sin(h); return _wv(o, [v[0] * s, v[1] * s, v[2] * s, math.cos(h)])
@api(DX, 'D3DXVec3Normalize', 2)
def _(o, v): return _wv(o, _norm(_rv(v)))
@api(DX, 'D3DXAssembleShaderFromFileA', 6)
def _(f, d, i, fl, ps, pe): flog('d3dx-shader', S(f)); return 0x80004005
@api(DX, 'D3DXCreateTextureFromFileExA', 14)
def _(dev, f, w, h, mip, us, fmt, pool, flt, mflt, ck, info, pal, pp):
    nm = S(f); hp = hostpath(nm); flog('d3dx-texture', nm, 0, os.path.getsize(hp) if os.path.isfile(hp) else 0)
    if not os.path.isfile(hp): return 0x8876086c
    w = w if w not in (0, 0xffffffff, 0xfffffffe) else 256; h = h if h not in (0, 0xffffffff, 0xfffffffe) else 256
    w32(pp, com_new('IDirect3DTexture9', w=w, h=h, fmt=21, levels=1, dev=dev, mem=0, pitch=0)); return 0

# ================= DirectInput8（鍵盤／滑鼠假裝置）=================
com_define('IDirectInputDevice8', _std([D('GetCapabilities', 1), D('EnumObjects', 3), D('GetProperty', 2), D('SetProperty', 2), D('Acquire', 0), D('Unacquire', 0), D('GetDeviceState', 2, lambda o, t, n, p: (uc.mem_write(p, b'\0' * n), 0)[1]),
    D('GetDeviceData', 4, lambda o, t, sz, buf, pn, fl: (w32(pn, 0), 0)[1]), D('SetDataFormat', 1), D('SetEventNotification', 1), D('SetCooperativeLevel', 2), D('GetObjectInfo', 3), D('GetDeviceInfo', 1), D('RunControlPanel', 2),
    D('Initialize', 3), D('CreateEffect', 4), D('EnumEffects', 3), D('GetEffectInfo', 2), D('GetForceFeedbackState', 1), D('SendForceFeedbackCommand', 1), D('EnumCreatedEffectObjects', 3), D('Escape', 1), D('Poll', 0),
    D('SendDeviceData', 4), D('EnumEffectsInFile', 4), D('WriteEffectToFile', 4), D('BuildActionMap', 3), D('SetActionMap', 3), D('GetImageInfo', 1)]))
com_define('IDirectInput8', _std([D('CreateDevice', 3, lambda o, t, g, pp, ou: (w32(pp, com_new('IDirectInputDevice8')), 0)[1]), D('EnumDevices', 4), D('GetDeviceStatus', 1), D('RunControlPanel', 2), D('Initialize', 2), D('FindDevice', 3), D('EnumDevicesBySemantics', 5), D('ConfigureDevices', 4)]))
@api('dinput8.dll', 'DirectInput8Create', 5)
def _(inst, ver, iid, pp, ou): WLOG.append(('DirectInput8Create',)); w32(pp, com_new('IDirectInput8')); return 0


# BNNet（線上模組，簽章不明，先當 0 引數回 0；實際被呼叫時要回頭校正）
for _n in ('#1', '#2', '#6', '#7', '#9', '#10', '#11', '#12', '#15', '#18', '#19', '#23', '?PlayerLogout@@YAHPADK@Z'): SIG[('bnnet.dll', _n)] = 0
API[(K, 'TryEnterCriticalSection')] = (1, False, lambda p: 1)
API[(K, 'GetNativeSystemInfo')] = (1, False, lambda p: API[(K, 'GetSystemInfo')][2](p))
API[(K, 'SwitchToThread')] = (0, False, lambda: 0)

# ================= DirectSound 8（假裝置，緩衝區有記憶體）=================
def _dsb_create(o, t, desc, pp, ou):
    sz, fl, nbytes = struct.unpack('<III', rd(desc, 12)); WLOG.append(('DSound CreateSoundBuffer', hex(fl), nbytes))
    w32(pp, com_new('IDirectSoundBuffer8', len=nbytes, mem=alloc(nbytes + 64) if nbytes else 0)); return 0
def _dsb_lock(o, t, off, n, p1, l1, p2, l2, fl):
    w32(p1, o['mem'] + off); w32(l1, n)
    if p2: w32(p2, 0)
    if l2: w32(l2, 0)
    return 0
com_define('IDirectSound8', _std([D('CreateSoundBuffer', 3, _dsb_create), D('GetCaps', 1), D('DuplicateSoundBuffer', 2, lambda o, t, b, pp: (w32(pp, com_new('IDirectSoundBuffer8', len=0, mem=0)), 0)[1]), D('SetCooperativeLevel', 2),
    D('Compact', 0), D('GetSpeakerConfig', 1), D('SetSpeakerConfig', 1), D('Initialize', 1), D('VerifyCertification', 1)]))
com_define('IDirectSoundBuffer8', _std([D('GetCaps', 1), D('GetCurrentPosition', 2), D('GetFormat', 4), D('GetVolume', 1), D('GetPan', 1), D('GetFrequency', 1), D('GetStatus', 1, lambda o, t, p: (w32(p, 0), 0)[1]), D('Initialize', 2),
    D('Lock', 8, _dsb_lock), D('Play', 3), D('SetCurrentPosition', 1), D('SetFormat', 1), D('SetVolume', 1), D('SetPan', 1), D('SetFrequency', 1), D('Stop', 0), D('Unlock', 4), D('Restore', 0), D('SetFX', 3), D('AcquireResources', 4), D('GetObjectInPath', 4)]))
@api('dsound.dll', '#11', 3)
def _(g, pp, ou): WLOG.append(('DirectSoundCreate8',)); w32(pp, com_new('IDirectSound8')); return 0

# ================= 畫格標記與指令計數（COUNT=1 才開，會變慢）=================
INS = [0]; FRAMES = []; LOOPLOG = []
_blk = {}
def _blockhook(uc_, addr, size, ud):
        n = _blk.get(addr)
        if n is None:
            n = _blk[addr] = sum(1 for _ in _cs.disasm(bytes(uc.mem_read(addr, size)), addr))
        INS[0] += n
if os.environ.get('COUNT') == '1': uc.hook_add(UC_HOOK_BLOCK, _blockhook)
def frame_mark(kind):
    if PHASE[0] != 'loop' and os.environ.get('COUNT') == 'lazy':   # 進主迴圈才開始計指令（啟動期太長，全程計數會慢一倍）
        uc.hook_add(UC_HOOK_BLOCK, _blockhook); uc.ctl_flush_tb()
    if PHASE[0] != 'loop': PHASE[0] = 'loop'; print('   [進入遊戲主迴圈：第一個畫格標記 %s，累計指令 %d]' % (kind, INS[0]))
    FRAMES.append((time.time(), INS[0], kind))
_oldflog = flog
def flog(act, name, off=0, n=0):
    _oldflog(act, name, off, n)
    if PHASE[0] == 'loop' and len(LOOPLOG) < 40000: LOOPLOG.append((len(FRAMES), act, name, off, n))

# ================= 輸入注入／訊息迴圈驅動 =================
# INJECT="tick:key:13,tick:click:400:300,..."；tick = 畫格數（尚無畫格標記時用 PeekMessage 空轉次數）
keys_down = set(); mouse = [400, 300, 0]    # x, y, 按鍵遮罩
PEEKS = [0]; INJ = []; INJ_DONE = [0]
for _t in filter(None, os.environ.get('INJECT', '').split(',')):
    p_ = _t.split(':'); INJ.append((int(p_[0]), p_[1], [int(x) for x in p_[2:]]))
VK2DIK = {13: 0x1c, 27: 1, 32: 0x39, 37: 0xcb, 38: 0xc8, 39: 0xcd, 40: 0xd0, 65: 0x1e, 90: 0x2c, 88: 0x2d}
def main_hwnd(): return next(iter(wins), 0)
def inject_tick():
    tick = len(FRAMES) if FRAMES else PEEKS[0]
    while INJ_DONE[0] < len(INJ) and INJ[INJ_DONE[0]][0] <= tick:
        _, kind, a = INJ[INJ_DONE[0]]; INJ_DONE[0] += 1; h = main_hwnd(); print('   [注入 %s %s 於 tick %d]' % (kind, a, tick))
        if kind == 'key':
            keys_down.add(a[0]); msgq.append((h, 0x100, a[0], 1)); msgq.append((h, 0x102, a[0], 1)); inj_later.append((tick + 2, h, 0x101, a[0], 0xc0000001, a[0]))
        elif kind == 'click':
            mouse[0], mouse[1], mouse[2] = a[0], a[1], 1; lp = (a[1] << 16) | a[0]
            msgq.append((h, 0x200, 0, lp)); msgq.append((h, 0x201, 1, lp)); inj_later.append((tick + 2, h, 0x202, 0, lp, -1))
inj_later = []
def inject_later():
    tick = len(FRAMES) if FRAMES else PEEKS[0]
    for it in list(inj_later):
        if it[0] <= tick:
            inj_later.remove(it); _, h, m, w, l, k = it; msgq.append((h, m, w, l))
            if k >= 0: keys_down.discard(k)
            else: mouse[2] = 0
@api(U, 'PeekMessageA', 5)
def _(p, h, a, b, rm):
    PEEKS[0] += 1
    if PEEKS[0] % 2000 == 0 and PEEKS[0] > 0 and not FRAMES: pass
    inject_tick(); inject_later()
    if not msgq:
        # 閒置：偶爾送 WM_PAINT 給主視窗，讓 MFC 的 OnIdle／遊戲自己的 idle 迴圈照常前進
        return 0
    m = msgq[0]
    if rm & 1: msgq.pop(0)
    _putmsg(p, m); return 1
@api(U, 'GetCursorPos', 1)
def _(p): w32(p, mouse[0]); w32(p + 4, mouse[1]); return 1
@api(U, 'GetAsyncKeyState', 1)
def _(k): return 0x8000 if (k in keys_down or (k == 1 and mouse[2])) else 0
@api(U, 'GetKeyState', 1)
def _(k): return 0x8000 if (k in keys_down or (k == 1 and mouse[2])) else 0
def _di_state(o, t, n, p):
    kind = o.get('kind')
    if kind == 'kbd':
        b = bytearray(n)
        for vk in keys_down:
            d = VK2DIK.get(vk)
            if d is not None and d < n: b[d & 0x7f if d < 0x80 else d] = 0x80
        uc.mem_write(p, bytes(b))
    elif kind == 'mouse':
        uc.mem_write(p, struct.pack('<iii', 0, 0, 0) + bytes([0x80 if mouse[2] else 0, 0, 0, 0]) + b'\0' * max(n - 16, 0))
    else: uc.mem_write(p, b'\0' * n)
    return 0
def _di_create(o, t, g, pp, ou):
    k = {0x6f1d2b61: 'kbd', 0x6f1d2b60: 'mouse'}.get(u32(g)); WLOG.append(('DInput CreateDevice', k or hex(u32(g)))); w32(pp, com_new('IDirectInputDevice8', kind=k)); return 0
com_define('IDirectInputDevice8', _std([D('GetCapabilities', 1), D('EnumObjects', 3), D('GetProperty', 2), D('SetProperty', 2), D('Acquire', 0), D('Unacquire', 0), D('GetDeviceState', 2, _di_state),
    D('GetDeviceData', 4, lambda o, t, sz, buf, pn, fl: (w32(pn, 0), 0)[1]), D('SetDataFormat', 1), D('SetEventNotification', 1), D('SetCooperativeLevel', 2), D('GetObjectInfo', 3), D('GetDeviceInfo', 1), D('RunControlPanel', 2),
    D('Initialize', 3), D('CreateEffect', 4), D('EnumEffects', 3), D('GetEffectInfo', 2), D('GetForceFeedbackState', 1), D('SendForceFeedbackCommand', 1), D('EnumCreatedEffectObjects', 3), D('Escape', 1), D('Poll', 0),
    D('SendDeviceData', 4), D('EnumEffectsInFile', 4), D('WriteEffectToFile', 4), D('BuildActionMap', 3), D('SetActionMap', 3), D('GetImageInfo', 1)]))
com_define('IDirectInput8', _std([D('CreateDevice', 3, _di_create), D('EnumDevices', 4), D('GetDeviceStatus', 1), D('RunControlPanel', 2), D('Initialize', 2), D('FindDevice', 3), D('EnumDevicesBySemantics', 5), D('ConfigureDevices', 4)]))
def finalize_one(va, k):
    h = resolve(k)
    if h and not h[1] and h[0]: uc.mem_write(va, b'\xc2' + struct.pack('<H', 4 * h[0]))
    else: uc.mem_write(va, b'\xc3')
for _va, _k in list(stubs.items()): finalize_one(_va, _k)
READY[0] = True

def call(addr, args, cdecl=False):
    esp = uc.reg_read(UC_X86_REG_ESP)
    sp = esp
    for a in reversed(args):
        sp -= 4; w32(sp, a)
    sp -= 4; w32(sp, SENT)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.emu_start(addr, SENT, timeout=int(float(os.environ.get('MAXSEC', '0')) * 1000000), count=int(os.environ.get('MAXINS', '60000000')))
    r = uc.reg_read(UC_X86_REG_EAX)
    uc.reg_write(UC_X86_REG_ESP, esp)
    return r

def on_unmapped(uc_, access, address, size, value, ud):
    print('   [未映射存取] type', access, 'addr', hex(address), 'size', size, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)))
    return False
uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, on_unmapped)
SENT = STUB_BASE + 0x1F0000
uc.reg_write(UC_X86_REG_ESP, STACK_TOP - 0x100)

# ---- 執行：DllMain → Poq 進入點 ----
order = ['ucrtbase.dll', 'vcruntime140.dll', 'msvcp140.dll', 'omutil.dll', 'mfc140.dll', 'easyrpg.dll', 'gfx3d.dll', 'mfx.dll', 'uiwndex.dll']
import time as _t
t0 = _t.time()
for d in order:
    m = mods[d]; ep = m['pe'].OPTIONAL_HEADER.AddressOfEntryPoint
    if not ep: continue
    print('DllMain', d, hex(m['base'] + ep)); sys.stdout.flush()
    try:
        r = call(m['base'] + ep, [m['base'], 1, 0])
        print('   → eax', r)
    except UcError as e:
        dump_ring(); print('   !! UcError', e, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)), 'esp', hex(uc.reg_read(UC_X86_REG_ESP)), 'esi', hex(uc.reg_read(UC_X86_REG_ESI)), 'edi', hex(uc.reg_read(UC_X86_REG_EDI))); break
if True:
    # 檔頭進入點 0x9c9310 在 .bind（加密外殼）；改從真正的 CRT 啟動 (__scrt_common_main_seh 的跳板) 開始
    ep = 0x5a7dc7
    print('Poq 進入點', hex(ep)); sys.stdout.flush()
    try:
        r = call(ep, []); print('Poq call 返回 eax', hex(r), 'eip', hex(uc.reg_read(UC_X86_REG_EIP))); [print('    ', hex(a), ' '.join(str(x) for x in next(_cs.disasm(bytes(uc.mem_read(a, sz)), a), None) and [next(_cs.disasm(bytes(uc.mem_read(a, sz)), a)).mnemonic, next(_cs.disasm(bytes(uc.mem_read(a, sz)), a)).op_str])) for a, sz in list(ring)[-16:]]
    except UcError as e:
        dump_ring(); print('!! UcError', e, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)), {r: hex(uc.reg_read(g)) for r, g in (('eax', UC_X86_REG_EAX), ('ecx', UC_X86_REG_ECX), ('edx', UC_X86_REG_EDX), ('ebx', UC_X86_REG_EBX), ('esi', UC_X86_REG_ESI), ('edi', UC_X86_REG_EDI), ('ebp', UC_X86_REG_EBP), ('esp', UC_X86_REG_ESP))}); _e = uc.reg_read(UC_X86_REG_EIP); print([f'{i.address:x} {i.mnemonic} {i.op_str}' for i in list(_cs.disasm(bytes(uc.mem_read(_e - 24, 48)), _e - 24))]); _bp = uc.reg_read(UC_X86_REG_EBP); print('ebp chain', [hex(u32(_bp + 4 + 0)) ] if True else 0)
print('耗時 %.1f 秒；呼叫過 %d 種主機函式；未實作：%s' % (_t.time() - t0, len(calls), unknown[:3]))
print('只用「預設回傳 0」的函式（沒有真實作）：', [(d, n, calls[(d, n)]) for d, n in defaults if (d, n) in calls][:80])
for t in trace[:MAXLOG]: print('  ', t)

# ---- 結尾摘要：視窗事件、檔案存取 ----
print('\n== 視窗事件（前 40）==')
for w in WLOG[:40]: print('  ', w)
print('\n== 檔案存取（共 %d 筆事件；依「階段/檔案」統計）==' % FCOUNT[0])
for (ph, nm), d in FSTAT.items():
    print('  [%s] %-44s opens=%d reads=%d bytes=%d seeks=%d range=[%d,%d) 第一次事件#%d' % (ph, nm, d['opens'], d['reads'], d['bytes'], d['seeks'], d['lo'] if d['reads'] else 0, d['hi'], d['first']))
for (ph, nm), d in FSTAT.items():
    if d['reads'] > 200: print('  樣本', nm, d['samples'][:24])

print('\n== 畫格 ==  標記次數', len(FRAMES), '累計指令', INS[0])
if len(FRAMES) > 1:
    d = [FRAMES[i + 1][1] - FRAMES[i][1] for i in range(len(FRAMES) - 1)]; tt = [FRAMES[i + 1][0] - FRAMES[i][0] for i in range(len(FRAMES) - 1)]
    print('每畫格指令數：最小 %d 平均 %d 最大 %d；每畫格牆鐘 %.3fs' % (min(d), sum(d) // len(d), max(d), sum(tt) / len(tt)))
print('主迴圈期間檔案事件（前 60）：')
for r in LOOPLOG[:60]: print('  畫格%d %s %s off=%d n=%d' % r)
