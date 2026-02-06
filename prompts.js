/* prompts.js - 語塊協議 Prompt 總部 */

const CHUNK_PROMPTS = {
    // 1. 核心規則：定義語塊拆分原則與色彩協議
    SYSTEM_RULES: `
        ROLE: You are an active conversation partner. 
        CRITICAL: DO NOT repeat or echo the user's words. RESPOND to them naturally.
        CHUNK RULE: 1-5 words per chunk. Break every response into meaningful phrases.
        Colors: noun(blue), verb(red), adj(green), adv(orange), other(yellow).
    `,

    // 2. 階段定義：對應 Stage 1-8 難度
    STAGES: {
        "Stage 1": "Survival level. Use ONLY Core 40 words. Extremely short chunks (1-2 words).",
        "Stage 2": "Social level. Basic phrases with Adjectives.",
        "Stage 3": "CEFR A1. Basic functional chunks.",
        "Stage 4": "CEFR A2. Chunks with frequency adverbs.",
        "Stage 5": "CEFR B1. Complex verb chunks.",
        "Stage 6": "CEFR B2. Sophisticated language.",
        "Stage 7": "CEFR C1. Idiomatic professional chunks.",
        "Stage 8": "CEFR C2. Master level nuance."
    },

    // 3. Test 2 專用：生成排序練習
    getTest2Prompt(lang, stage) {
        return `
            ${this.SYSTEM_RULES}
            Task: Create a natural sentence in ${lang} for a ${stage} learner.
            Constraint: ${this.STAGES[stage]}
            Example Chunking: ["The big apple", "is sitting", "on the table"]
            Output JSON ONLY:
            {
                "sentence": "Full string",
                "chunks": [
                    {"text": "meaningful phrase", "type": "noun/verb/adj/adv/other", "analysis": "繁中解釋"}
                ]
            }
        `;
    },

    // 4. Test 3 專用：生成對話流發牌選項
    // 用於生成 AI 的回覆
    getAiResponsePrompt(userText) {
        return `
            ${this.SYSTEM_RULES}
            User said: "${userText}"
            Task: Respond to the user naturally as a partner. 
            Output JSON: {"s": [{"t": "Your response", "p": "pos"}]}
        `;
    },

    // 用於生成使用者的發牌選項
    getTest3Prompt(lang, stage, scene, lastMsg) {
        return `
            ${this.SYSTEM_RULES}
            Scenario: ${scene}.
            AI's Last Words: "${lastMsg}"
            
            TASK: Generate 3 DISTINCT paths for the USER to respond.
            Each path must be ONE COMPLETE SENTENCE broken into CHUNKS (1-5 words each).

            - "heavy": Advanced path. Pushes the dialogue forward.
            - "light": Standard path. Natural and polite.
            - "distractor": Repair path. Slightly off-topic or awkward.

            JSON ONLY:
            {
                "heavy": [{"t": "...", "p": "pos"}, {"t": "...", "p": "pos"}],
                "light": [...],
                "distractor": [...]
            }
        `;
    }
};