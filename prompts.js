/* prompts.js - 語塊協議 Prompt 總部 */

const CHUNK_PROMPTS = {
    // 1. 核心規則：定義顏色與語塊拆分原則
    SYSTEM_RULES: `
        CRITICAL RULE: Break the sentence into 3-5 MEANINGFUL CHUNKS (phrases), NOT single words.
        Coloring Protocol: noun(blue), verb(red), adj(green), adv(orange), other(yellow).
    `,

    // 2. 階段定義：對應 Stage 1-8 協議
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

    // 3. Test 2 專用：生成排序題目的指令
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

    // 4. Test 3 專用：生成對話選項的指令
    getTest3Prompt(lang, stage, scene, lastMsg) {
        return `
        Role: Linguistic Dealer. Target: ${lang}, Stage: ${stage}.
        Scenario: ${scene}.
        Last Context: "${lastMsg}"

        Generate 3 DIFFERENT response paths for the user.
        EACH path MUST be a SINGLE sentence broken into phrasal chunks.
        
        Colors: noun(blue), verb(red), adj(green), adv(orange), other(yellow).

        Output JSON:
        {
            "heavy": [{"t": "Complex phrase", "p": "pos"}],
            "light": [{"t": "Simple phrase", "p": "pos"}],
            "distractor": [{"t": "Off-topic phrase", "p": "pos"}]
        }
    `;
    }
};