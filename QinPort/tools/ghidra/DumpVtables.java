// 列出每個類別虛擬函式表（RTTI 的 vftable 符號）的各格函式：<輸出資料夾>/<程式名>.vtables.txt
// 每行：類別 格號 函式位址
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import java.io.*;

public class DumpVtables extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0] + "/" + currentProgram.getName() + ".vtables.txt";
        Memory mem = currentProgram.getMemory();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        int n = 0;
        try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(out), "UTF-8"))) {
            for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
                if (!s.getName().equals("vftable")) continue;
                String cls = s.getParentNamespace().getName(true);
                Address a = s.getAddress();
                for (int i = 0; i < 400; i++) {
                    Address p = a.add(i * 4);
                    if (i > 0 && currentProgram.getSymbolTable().getPrimarySymbol(p) != null) break;  // 下一個符號 = 表結束
                    long v = mem.getInt(p) & 0xffffffffL;
                    Function f = getFunctionAt(sp.getAddress(v));
                    if (f == null) break;
                    w.println(cls + "\t" + i + "\t" + Long.toHexString(v));
                }
                n++;
            }
        }
        println("vftable " + n + " → " + out);
    }
}
