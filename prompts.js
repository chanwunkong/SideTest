/* prompts.js - 語塊協議 Prompt 總部 */

const CHUNK_PROMPTS = {
    // 1. 核心規則：定義語塊拆分原則與色彩協議
    SYSTEM_RULES: `
        CRITICAL RULE: NEVER output a full sentence as one chunk. 
        A "chunk" must be a meaningful phrase of 1-3 words.
        Example of CORRECT chunking: 
        - "Right now," (adv)
        - "I am focused" (verb)
        - "on Japanese" (noun)
        - "and Korean." (other)

        Coloring Protocol: noun(blue), verb(red), adj(green), adv(orange), other(yellow).
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
    getTest3Prompt(lang, stage, scene, lastMsg) {
        return `
            ${this.SYSTEM_RULES}
            Target Language: ${lang}. Stage: ${stage}. Scene: ${scene}.
            Last Context: "${lastMsg}"

            TASK: Generate 3 response options for the user. 
            Each option MUST be an array of MULTIPLE small phrasal chunks.

            JSON structure:
            {
                "heavy": [{"t": "Phrase 1", "p": "noun/verb/adj/adv/other"}, {"t": "Phrase 2", "p": "..."}],
                "light": [...],
                "distractor": [...]
            }
        `;
    }
};