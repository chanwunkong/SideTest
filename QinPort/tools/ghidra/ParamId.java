// 匯入前開啟「Decompiler Parameter ID」分析，讓 Ghidra 推出每個函式真正的參數和呼叫方式（含 ecx 傳 this）
import ghidra.app.script.GhidraScript;

public class ParamId extends GhidraScript {
    @Override
    public void run() throws Exception {
        setAnalysisOption(currentProgram, "Decompiler Parameter ID", "true");
    }
}
