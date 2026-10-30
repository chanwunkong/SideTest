// 解析 Gemini 回覆。容忍前後多餘文字與程式碼框，但內容不對就拒絕，不存錯誤資料
export const MEAL_FORMAT = 'health-meal-v1';

export const GEMINI_PROMPT = `你是營養估算助手。我每次會傳一張餐點照片，可能附上補充說明。
請估算照片中全部食物與飲料的總量，只回覆一個 JSON 程式碼區塊，不要有其他文字，格式如下：

{
  "format": "${MEAL_FORMAT}",
  "items": [{ "name": "食物名稱", "portion": "估計份量" }],
  "nutrients": {
    "water_ml": 0,
    "protein_g": 0,
    "carbs_g": 0,
    "fat_g": 0,
    "kcal": 0,
    "caffeine_mg": 0
  },
  "confidence": "low"
}

規則：
- items 用繁體中文。
- nutrients 全部填數字，是整餐加總；water_ml 是飲料與湯的水分。
- 看不見的油脂和醬料依常見做法估計；看不太出來時 confidence 用 low，一般用 medium，很確定才用 high。
- 我的補充說明優先於你從照片看到的判斷。
- 照片不是食物時，只回覆 {"format": "${MEAL_FORMAT}", "error": "not_food"}。`;

const NUTRIENT_KEYS = ['water_ml', 'protein_g', 'carbs_g', 'fat_g', 'kcal', 'caffeine_mg'];

export function parseMeal(text) {
  const start = text.indexOf('{');
  const end = text.lastIndexOf('}');
  if (start < 0 || end < start) throw new Error('找不到資料，請確認複製的是 Gemini 的完整回覆');
  let data;
  try {
    data = JSON.parse(text.slice(start, end + 1));
  } catch {
    throw new Error('格式有誤，請重新複製 Gemini 的回覆');
  }
  if (data.format !== MEAL_FORMAT) throw new Error('這不是餐點資料，請先用 App 提供的提示詞');
  if (data.error) throw new Error('Gemini 判斷照片裡不是食物');
  if (!data.nutrients) throw new Error('回覆缺少營養數值');

  const nutrients = {};
  for (const key of NUTRIENT_KEYS) {
    const value = Number(data.nutrients[key]);
    if (!Number.isFinite(value) || value < 0) throw new Error(`數值有誤：${key}`);
    nutrients[key] = value;
  }
  const items = (Array.isArray(data.items) ? data.items : []).map((i) => ({ name: String(i.name ?? ''), portion: String(i.portion ?? '') }));
  const confidence = ['low', 'medium', 'high'].includes(data.confidence) ? data.confidence : null;
  return { items, nutrients, confidence };
}
