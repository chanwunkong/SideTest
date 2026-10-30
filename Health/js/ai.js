// 用 Claude 從餐點照片估算營養。金鑰只存在這支手機
const MODEL = 'claude-opus-5-5';
const SDK_URL = 'https://esm.sh/@anthropic-ai/sdk@1.3.0';
const KEY_STORAGE = 'health.anthropicKey';

const NUMBER = { type: 'number' };
const MEAL_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['is_food', 'items', 'nutrients', 'confidence'],
  properties: {
    is_food: { type: 'boolean' },
    items: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['name', 'portion'],
        properties: { name: { type: 'string' }, portion: { type: 'string' } },
      },
    },
    nutrients: {
      type: 'object',
      additionalProperties: false,
      required: ['kcal', 'carbs_g', 'fat_g', 'protein_g', 'water_ml', 'caffeine_mg'],
      properties: { kcal: NUMBER, carbs_g: NUMBER, fat_g: NUMBER, protein_g: NUMBER, water_ml: NUMBER, caffeine_mg: NUMBER },
    },
    confidence: { type: 'string', enum: ['low', 'medium', 'high'] },
  },
};

const SYSTEM = `你是營養估算助手。根據照片估算這一餐全部可見食物與飲料的總量。
- items 用繁體中文列出每項食物與估計份量。
- nutrients 是整餐加總：熱量 kcal、碳水 g、脂肪 g、蛋白質 g、飲料與湯的水分 ml、咖啡因 mg。
- 看不見的油脂、醬料依常見做法估計，看不出來時 confidence 用 low。
- 照片裡不是食物時，is_food 設為 false，營養數值全部填 0。
- 使用者的補充說明優先於你從照片看到的判斷。`;

export const getApiKey = () => localStorage.getItem(KEY_STORAGE) || '';
export const setApiKey = (key) => (key ? localStorage.setItem(KEY_STORAGE, key) : localStorage.removeItem(KEY_STORAGE));

export async function analyzeMeal(dataUrl, note = '') {
  const apiKey = getApiKey();
  if (!apiKey) throw new Error('請先到設定填入 Anthropic API 金鑰');
  const { default: Anthropic } = await import(SDK_URL);
  const client = new Anthropic({ apiKey, dangerouslyAllowBrowser: true });
  const response = await client.beta.messages.create(buildRequest(dataUrl, note)).catch((error) => {
    throw new Error(friendlyError(error));
  });
  if (response.stop_reason === 'refusal') throw new Error('這張照片無法分析');
  if (response.stop_reason === 'max_tokens') throw new Error('分析結果過長，請再試一次');
  const text = response.content.find((block) => block.type === 'text')?.text;
  if (!text) throw new Error('沒有收到分析結果');
  return { ...JSON.parse(text), model: response.model };
}

function friendlyError(error) {
  if (error.status === 401) return 'API 金鑰無效，請到設定檢查';
  if (error.status === 429) return '請求太頻繁或額度不足，請稍後再試';
  if (error.status >= 500) return '服務暫時忙碌，請稍後再試';
  return error.status ? `服務回應錯誤（${error.status}）` : '連線失敗，請檢查網路';
}

function buildRequest(dataUrl, note) {
  return {
    model: MODEL,
    max_tokens: 16000,
    betas: ['server-side-fallback-2026-07-01'],
    fallbacks: 'default',
    output_config: { effort: 'medium', format: { type: 'json_schema', schema: MEAL_SCHEMA } },
    system: SYSTEM,
    messages: [{
      role: 'user',
      content: [
        { type: 'image', source: { type: 'base64', media_type: 'image/jpeg', data: dataUrl.split(',')[1] } },
        { type: 'text', text: note ? `補充說明：${note}` : '請估算這一餐。' },
      ],
    }],
  };
}
