"""可行性驗證 2：用 Unicorn（x86 模擬）載入原版 Poq.exe 與它的 DLL，系統呼叫換成主機端實作，看能跑到哪裡。
用法：python loader.py [遊戲資料夾] [最多印幾筆呼叫]
真實載入的模組：Poq.exe、OmUtil、mfc140、EasyRPG、Gfx3D、Mfx、UIWndEx；其餘（BNNet、BugTrap、OpenAL、libvlc、系統 DLL）全部由主機端提供。"""
import sys, os, struct, collections, time
import pefile
from unicorn import *
from unicorn.x86_const import *

G = sys.argv[1] if len(sys.argv) > 1 else '../game'
MAXLOG = int(sys.argv[2]) if len(sys.argv) > 2 else 400
REAL = ['omutil.dll', 'mfc140.dll', 'easyrpg.dll', 'gfx3d.dll', 'mfx.dll', 'uiwndex.dll']   # 載入順序
BASES = {'poq.exe': 0x400000, 'easyrpg.dll': 0x10000000, 'mfx.dll': 0x30000000, 'omutil.dll': 0x40000000,
         'mfc140.dll': 0x50000000, 'gfx3d.dll': 0x60000000, 'uiwndex.dll': 0x68000000}
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
    b[13] = 17; b[19] = 6; b[25] = 7; b[30] = 0xEB
    return bytes(b)
CODE_AT = {}
def code_stub(name):
    if name in ('_initterm', '_initterm_e'):
        if name not in CODE_AT:
            va = STUB_BASE + 0x100000 + 0x40 * len(CODE_AT); CODE_AT[name] = va
            uc.mem_write(va, _mk_iterm(name == '_initterm_e'))
        return CODE_AT[name]
def stub_for(dll, name):
    cs = code_stub(name)
    if cs: return cs
    k = (dll, name)
    if k not in stub_va:
        va = stub_next[0]; stub_next[0] += 16
        stub_va[k] = va; stubs[va] = k
        try: uc.mem_write(va, b'\xc3')
        except Exception: pass
    return stub_va[k]

def map_pe(fname):
    path = os.path.join(G, fname)
    pe = pefile.PE(path)
    base = BASES[fname.lower()]
    if pe.OPTIONAL_HEADER.ImageBase != base:
        pe.relocate_image(base)
    img = pe.get_memory_mapped_image()
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xfff) & ~0xfff
    uc.mem_map(base, size, UC_PROT_ALL)
    uc.mem_write(base, img)
    exports = {}
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            va = base + s.address
            if s.name: exports[s.name.decode()] = va
            exports['#%d' % s.ordinal] = va
    mods[fname.lower()] = dict(base=base, pe=pe, exports=exports)
    return mods[fname.lower()]

def resolve_imports(m):
    pe = m['pe']
    pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    for e in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
        d = e.dll.decode().lower()
        for i in e.imports:
            name = i.name.decode() if i.name else '#%d' % i.ordinal
            if d in mods and name in mods[d]['exports']:
                va = mods[d]['exports'][name]
            else:
                va = stub_for(d, name)
            w32(i.address, va)

# ---- 載入 ----
uc.mem_map(STUB_BASE, STUB_SIZE, UC_PROT_ALL)
uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE + 0x1000, UC_PROT_ALL)
uc.mem_map(PEB, 0x2000, UC_PROT_ALL)
for f in ['Poq.exe'] + [r for r in REAL]:
    map_pe(next(x for x in os.listdir(G) if x.lower() == f.lower()))
for m in mods.values(): resolve_imports(m)
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
def alloc(n, zero=True):
    n = (n + 15) & ~15
    p = heap_ptr[0]; heap_ptr[0] += n
    if heap_ptr[0] > HEAP_BASE + HEAP_SIZE: raise MemoryError('heap')
    return p

API = {}   # (dll, name) → (nargs, cdecl, fn)
def api(dll, name, nargs, cdecl=False):
    def d(fn):
        API[(dll.lower(), name)] = (nargs, cdecl, fn); return fn
    return d

last_error = [0]
tls_slots = {}
trace = []

def on_stub(uc_, addr, size, ud):
    k = stubs.get(addr)
    if not k: return
    dll, name = k
    calls[k] += 1
    h = API.get(k)
    esp = uc.reg_read(UC_X86_REG_ESP)
    ret = u32(esp)
    if not h:
        # api-ms-win-crt-* 與 ucrtbase 同名函式共用處理
        h = API.get(('crt', name))
    if not h:
        unknown.append((dll, name, ret)); print('!! 未實作', dll, name, 'from', hex(ret)); uc.emu_stop(); return
    nargs, cdecl, fn = h
    args = [u32(esp + 4 + 4 * i) for i in range(nargs)]
    if len(trace) < MAXLOG: trace.append((dll, name, [hex(a) for a in args]))
    r = fn(*args)
    uc.reg_write(UC_X86_REG_EAX, (r or 0) & 0xffffffff)   # 返回由 stub 位址上的 ret／ret N 機器碼完成（finalize_stubs 依參數個數寫入）

uc.hook_add(UC_HOOK_CODE, on_stub, begin=STUB_BASE, end=STUB_BASE + STUB_SIZE)

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
@api(K, 'GetModuleHandleW', 1)
def _(n): return mods['poq.exe']['base'] if n == 0 else 0
@api(K, 'GetModuleHandleA', 1)
def _(n):
    if n == 0: return mods['poq.exe']['base']
    nm = cstr(n).lower()
    if not nm.endswith('.dll') and not nm.endswith('.exe'): nm += '.dll'
    return mods[nm]['base'] if nm in mods else 0
@api(K, 'GetModuleHandleExW', 3)
def _(flags, n, out): return 0
@api(K, 'GetProcAddress', 2)
def _(h, n):
    for m in mods.values():
        if m['base'] == h:
            name = cstr(n) if n > 0xffff else '#%d' % n
            return m['exports'].get(name, 0)
    name = cstr(n) if n > 0xffff else '#%d' % n
    return stub_for('dyn', name)
@api(K, 'LoadLibraryA', 1)
def _(n):
    nm = cstr(n).lower()
    return mods[nm]['base'] if nm in mods else 0x7e000000
@api(K, 'LoadLibraryW', 1)
def _(n): return 0x7e000000
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
@api(K, 'HeapAlloc', 3)
def _(h, flags, n): return alloc(n)
@api(K, 'HeapFree', 3)
def _(h, f, p): return 1
@api(K, 'HeapReAlloc', 4)
def _(h, f, p, n):
    q = alloc(n)
    if p: uc.mem_write(q, rd(p, min(n, 64)) if False else b'')
    return q
@api(K, 'HeapSize', 3)
def _(h, f, p): return 0x1000
@api(K, 'HeapDestroy', 1)
def _(h): return 1
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
@api(K, 'RaiseException', 4)
def _(c, f, n, a): print('RaiseException', hex(c)); uc.emu_stop()
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

def finalize_stubs():
    for va, k in stubs.items():
        h = API.get(k) or API.get(('crt', k[1]))
        if h and not h[1] and h[0]: uc.mem_write(va, b'\xc2' + struct.pack('<H', 4 * h[0]))
        else: uc.mem_write(va, b'\xc3')
finalize_stubs()

def call(addr, args, cdecl=False):
    esp = uc.reg_read(UC_X86_REG_ESP)
    sp = esp
    for a in reversed(args):
        sp -= 4; w32(sp, a)
    sp -= 4; w32(sp, SENT)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.emu_start(addr, SENT, count=200000000)
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
order = ['omutil.dll', 'mfc140.dll', 'easyrpg.dll', 'gfx3d.dll', 'mfx.dll', 'uiwndex.dll']
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
        print('   !! UcError', e, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)), 'esp', hex(uc.reg_read(UC_X86_REG_ESP)), 'esi', hex(uc.reg_read(UC_X86_REG_ESI)), 'edi', hex(uc.reg_read(UC_X86_REG_EDI))); break
    if unknown: break
if not unknown:
    ep = mods['poq.exe']['base'] + mods['poq.exe']['pe'].OPTIONAL_HEADER.AddressOfEntryPoint
    print('Poq 進入點', hex(ep)); sys.stdout.flush()
    try:
        call(ep, [])
    except UcError as e:
        print('!! UcError', e, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)))
print('耗時 %.1f 秒；呼叫過 %d 種主機函式；未實作：%s' % (_t.time() - t0, len(calls), unknown[:3]))
for t in trace[:MAXLOG]: print('  ', t)
