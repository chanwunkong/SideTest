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
        "Stage 1": "Survival: 1-3 simple words only. No grammar, just key nouns/verbs. (e.g., 'Water, please', 'I go')",
        "Stage 2": "Basic: 3-5 words. Simple S-V-O patterns. Use common adjectives. (e.g., 'I like big dogs')",
        "Stage 3": "Breakthrough (A1): 5-8 words. Basic functional phrases, present tense only, simple connectors like 'and'.",
        "Stage 4": "Waystage (A2): 8-12 words. Past/future simple tense, basic frequency adverbs (always, often).",
        "Stage 5": "Threshold (B1): 12-18 words. Relative clauses (who/which), present perfect, and basic modal verbs.",
        "Stage 6": "Vantage (B2): 18-25 words. Complex sentences, passive voice, and abstract vocabulary.",
        "Stage 7": "Proficiency (C1): Unlimited length. Idiomatic expressions, professional jargon, and nuanced tone.",
        "Stage 8": "Mastery (C2): Native-level complexity. Full range of stylistic and implicit meanings."
    },

    // 3. AI 回應協議：加入 scene 與 stageRule 控制
    getAiResponsePrompt(userText, lang, stage, scene) {
        const stageRule = this.STAGES[stage] || "";
        return `
        ${this.SYSTEM_RULES}
        Context/Scenario: ${scene}
        User: "${userText}"
        Task: Respond naturally in ${lang} at ${stage} level.
        STRICT DIFFICULTY RULE: ${stageRule}
        Output JSON: {"s": [{"t": "...", "p": "..."}]}
    `;
    },

    // 4. 發牌員協議：確保三路選項嚴格遵守字數限制
    getTest3Prompt(lang, stage, scene, lastMsg) {
        const stageRule = this.STAGES[stage] || "";
        return `
        ${this.SYSTEM_RULES}
        Scenario: ${scene}. AI: "${lastMsg}"
        Language: ${lang}
        Task: Generate 3 DISTINCT paths (heavy/light/distractor) for User.
        
        CRITICAL DIFFICULTY - ${stage}:
        Rule: ${stageRule}
        Rule: Each path is ONE coherent sentence strictly following the word count limits of ${stage}.
        
        POS Rule: Only use "other" for particles/conjunctions. 
        Output: {
            "heavy": [{"t": "...", "p": "..."}],
            "light": [{"t": "...", "p": "..."}],
            "distractor": [{"t": "...", "p": "..."}]
        }
    `;
    }
};