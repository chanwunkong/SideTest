"""產生 engine/src/gbk.h：GBK 雙位元組（首 0x81–0xFE、尾 0x40–0xFE）→ Unicode 對照表，給原版字型（依 Unicode 排）用。
用法：python3 tools/gbktable.py > engine/src/gbk.h
"""
vals = []
for lead in range(0x81, 0xFF):
    for trail in range(0x40, 0xFF):
        try:
            c = bytes((lead, trail)).decode('gbk')
            vals.append(ord(c) if len(c) == 1 and ord(c) <= 0xFFFF else 0)
        except UnicodeDecodeError:
            vals.append(0)
print('// 由 tools/gbktable.py 產生：gbk_uni[(首 - 0x81) * 191 + (尾 - 0x40)] = Unicode（0 = 沒有）')
print('#pragma once\n#include <stdint.h>\nstatic const uint16_t gbk_uni[126 * 191] = {')
for i in range(0, len(vals), 16):
    print('    ' + ','.join(str(v) for v in vals[i:i + 16]) + ',')
print('};')
