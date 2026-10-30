// 資料類型清單：新增資料只要在這裡加一項，不必改資料庫結構
export const NUTRIENTS = [
  { key: 'kcal', label: '熱量', unit: 'kcal' },
  { key: 'carbs_g', label: '碳水', unit: 'g' },
  { key: 'fat_g', label: '脂肪', unit: 'g' },
  { key: 'protein_g', label: '蛋白質', unit: 'g' },
  { key: 'water_ml', label: '水分', unit: 'ml' },
  { key: 'caffeine_mg', label: '咖啡因', unit: 'mg', isLimit: true },
  { key: 'creatine_g', label: '肌酸', unit: 'g' },
];

export const MEAL_SLOTS = [
  { key: 'breakfast', label: '早餐', short: '早', required: true },
  { key: 'lunch', label: '午餐', short: '午', required: true },
  { key: 'dinner', label: '晚餐', short: '晚', required: true },
  { key: 'snack', label: '點心', short: '點', required: false },
];

export const ACTIVITIES = ['攀岩', '指力', '重訓', '跑步', '單車', '游泳', '健走', '其他'];
export const INTENSITIES = [
  { key: 'easy', label: '輕鬆' },
  { key: 'moderate', label: '中等' },
  { key: 'hard', label: '高強度' },
];

function addNutrients(day, nutrients = {}) {
  for (const [key, value] of Object.entries(nutrients)) {
    if (typeof value === 'number' && Number.isFinite(value)) day.nutrients[key] = (day.nutrients[key] || 0) + value;
  }
}

export const registry = {
  meal: {
    version: 1,
    validate: (data) => MEAL_SLOTS.some((s) => s.key === data.mealSlot),
    upgrade: {},
    summarize(day, data, event) {
      day.meals.push(event);
      if (!data.skipped) addNutrients(day, data.nutrients);
    },
  },
  intake: {
    version: 1,
    validate: (data) => !!data.nutrients,
    upgrade: {},
    summarize(day, data, event) {
      day.intakes.push(event);
      addNutrients(day, data.nutrients);
    },
  },
  workout: {
    version: 1,
    validate: (data) => !!data.activity && data.durationSec > 0,
    upgrade: {},
    summarize(day, data, event) {
      day.workouts.push(event);
    },
  },
};

// 讀取時把舊版事件升級；不認識或比程式新的事件原樣保留
export function normalizeEvent(event) {
  const def = registry[event.type];
  if (!def || event.schemaVersion >= def.version) return event;
  let { data, schemaVersion: version } = event;
  while (version < def.version) data = def.upgrade[version++](data);
  return { ...event, data, schemaVersion: version };
}
