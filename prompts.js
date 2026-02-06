const CHUNK_PROMPTS = {
    // 1. 核心規則：定義唯一的數據格式協議
    SYSTEM_RULES: `
        ROLE: Natural partner. NO repetition.
        RULE: 1-5 words per chunk. NO repetition.
        FORMAT: JSON ONLY. Use "t" for text, "p" for pos.
        POS: noun, verb, adj, adv, other.
    `,

    // 2. 階段定義 (供發牌員參考)
    STAGES: {
        "Stage 1": "Core survival, short chunks.",
        "Stage 2": "Basic phrases.",
        "Stage 3": "A1 tasks.",
        "Stage 4": "A2 base.",
        "Stage 5": "B1 threshold.",
        "Stage 6": "B2 vantage.",
        "Stage 7": "C1 proficiency.",
        "Stage 8": "C2 mastery."
    },

    // 3. AI 回應協議：確保產出單一連續的回應流
    getAiResponsePrompt(userText, lang, stage) {
        return `
            ${this.SYSTEM_RULES}
            User: "${userText}"
            Task: Respond naturally in ${lang} at ${stage} level.
            Output JSON: {"s": [{"t": "...", "p": "..."}]}
        `;
    },

    // 4. 發牌員協議：確保三路選項皆為獨立的連貫長句
    getTest3Prompt(lang, stage, scene, lastMsg) {
        return `
            ${this.SYSTEM_RULES}
            Scenario: ${scene}. AI: "${lastMsg}"
            Task: Generate 3 DISTINCT paths (heavy/light/distractor) for User.
            Rule: Each path is ONE long, coherent sentence broken into chunks.
            
            POS Rule: Only use "other" for particles/conjunctions. 
            Output: {
                "heavy": [{"t": "...", "p": "..."}],
                "light": [{"t": "...", "p": "..."}],
                "distractor": [{"t": "...", "p": "..."}]
            }
        `;
    }
};