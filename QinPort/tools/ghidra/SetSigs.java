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
        // random_exits(房間, 方向, 房間路徑)：兩個字串，Ghidra 猜成 byte（0x10132725 起）
        {"10052a20", "void FUN_10052a20(void *param_1, char *param_2, char *param_3)"},
        // RandSetPersonExBox(房間, 名稱, 數量, 入口, 等級, 階, G_AIGroup)：7 個（Ghidra 用 in_stack 讀第 7 個）
        {"100528d0", "int * FUN_100528d0(int *param_1, char *param_2, int param_3, int param_4, int param_5, int param_6, int param_7)"},
        // "@poqdata %d %d"(人物, a, b)：呼叫端 push 3 個
        {"10056640", "void FUN_10056640(int param_1, int param_2, int param_3)"},
        // 廣播(字串, 誰)：push 2 個
        {"10055670", "void FUN_10055670(char *param_1, int param_2)"},
        // GetAttrAtRoom(房間路徑, 別名, 屬性名)：push 3 個（Ghidra 把第 3 個當成常數 "szName"）
        {"1004a940", "int FUN_1004a940(char *param_1, char *param_2, char *param_3)"},
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
