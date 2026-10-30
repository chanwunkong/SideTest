// 資料類型清單：新增資料只要在這裡加一項，不必改資料庫結構
export const NUTRIENTS = [
  { key: 'kcal', label: '熱量', unit: 'kcal', primary: true },
  { key: 'water_ml', label: '水分', unit: 'ml', primary: true },
  { key: 'protein_g', label: '蛋白質', unit: 'g', primary: true },
  { key: 'carbs_g', label: '碳水', unit: 'g', primary: true },
  { key: 'fat_g', label: '脂肪', unit: 'g', primary: true },
  { key: 'caffeine_mg', label: '咖啡因', unit: 'mg' },
  { key: 'creatine_g', label: '肌酸', unit: 'g' },
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
    validate: (data) => !!data.nutrients,
    upgrade: {},
    summarize(day, data, event) {
      day.meals.push(event);
      addNutrients(day, data.nutrients);
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
  body: {
    version: 1,
    validate: (data) => data.weightKg > 0 || data.bodyFat > 0 || data.muscleKg > 0,
    upgrade: {},
    summarize(day, data, event) {
      day.body.push(event);
    },
  },
  sleep: {
    version: 1,
    validate: (data) => data.durationSec > 0,
    upgrade: {},
    summarize(day, data, event) {
      day.sleep.push(event);
    },
  },
  workout: {
    version: 1,
    validate: (data) => !!data.trainingId || (!!data.activity && data.durationSec > 0),
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
