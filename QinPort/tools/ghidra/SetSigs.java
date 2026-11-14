// 修正 Ghidra 猜錯參數個數的引擎函式（反編譯前跑）：照呼叫端 push 的個數設定簽名
// 用法：analyzeHeadless /tmp/ghproj qin -process EasyRPG.dll -noanalysis -preScript SetSigs.java -postScript DumpC.java <輸出資料夾>
import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class SetSigs extends GhidraScript {
    // 位址 → C 簽名
    static final String[][] SIGS = {
        // drop_items(誰, 類型, 件數, PosY, 等級, SLevel 欄)：呼叫端都 push 6 個（例如 0x101db5d7 起）
        {"100521f0", "void FUN_100521f0(int *param_1, int param_2, int param_3, int param_4, int param_5, int param_6)"},
    };

    @Override
    public void run() throws Exception {
        if (!currentProgram.getName().equals("EasyRPG.dll")) return;
        for (String[] s : SIGS) {
            Function f = getFunctionAt(toAddr(Long.parseLong(s[0], 16)));
            FunctionDefinitionDataType sig = CParserUtils.parseSignature((ghidra.app.services.DataTypeManagerService) null, currentProgram, s[1]);
            sig.setCallingConvention("__cdecl");
            new ApplyFunctionSignatureCmd(f.getEntryPoint(), sig, SourceType.USER_DEFINED).applyTo(currentProgram, monitor);
            println("簽名 " + s[0] + " → " + f.getSignature());
        }
    }
}
