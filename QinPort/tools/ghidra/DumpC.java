// Ghidra 無頭模式腳本：把整個程式每個函式反編譯成 C，寫到 <輸出資料夾>/<程式名>.c
// 用法：analyzeHeadless <專案資料夾> <專案名> -import <檔案> -postScript DumpC.java <輸出資料夾>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class DumpC extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0] + "/" + currentProgram.getName() + ".c";
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        int n = 0, fail = 0;
        try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(out), "UTF-8"))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                w.println("// ==== " + f.getEntryPoint() + " " + f.getName());
                if (r != null && r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
                else { w.println("// 反編譯失敗"); fail++; }
                n++;
            }
        }
        println("函式 " + n + "，失敗 " + fail + " → " + out);
    }
}
