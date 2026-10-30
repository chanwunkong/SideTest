// 首頁卡片：資料和顯示方式分開。新增卡片種類只要在 CARD_TYPES 加一項
import { addDays, parseDateKey } from './logic.js';

const nutrient = (key, label, unit) => ({ key, label, unit, read: (day) => day.nutrients[key] ?? null });
const bodyMetric = (key, label, unit) => ({ key, label, unit, read: (day) => day.body.at(-1)?.data[key] ?? null });
const sum = (list, read) => (list.length ? list.reduce((total, item) => total + read(item), 0) : null);

export const CARD_TYPES = {
  body: {
    label: '身體組成', color: 'var(--accent)', eventTypes: ['body'],
    icon: '<path d="M12 7a3 3 0 1 0 0-6 3 3 0 0 0 0 6zM5 22l2-9h10l2 9"/>',
    metrics: [bodyMetric('weightKg', '體重', 'kg'), bodyMetric('bodyFat', '體脂', '%'), bodyMetric('muscleKg', '肌肉', 'kg')],
    hasRecord: (day) => day.body.length > 0,
  },
  diet: {
    label: '飲食', color: 'var(--diet)', eventTypes: ['meal', 'intake'],
    icon: '<path d="M7 2v9a3 3 0 0 0 3 3v8M4 2v6M10 2v6M17 2c-2 2-3 5-3 9h3v11"/>',
    metrics: [
      nutrient('protein_g', '蛋白質', 'g'), nutrient('water_ml', '水分', 'ml'), nutrient('carbs_g', '碳水', 'g'),
      nutrient('fat_g', '脂肪', 'g'), nutrient('kcal', '熱量', 'kcal'), nutrient('caffeine_mg', '咖啡因', 'mg'), nutrient('creatine_g', '肌酸', 'g'),
    ],
    hasRecord: (day) => day.meals.length + day.intakes.length > 0,
  },
  sleep: {
    label: '睡眠', color: 'var(--rest)', eventTypes: ['sleep'],
    icon: '<path d="M20 15A8 8 0 1 1 9 4a7 7 0 0 0 11 11z"/>',
    metrics: [{ key: 'hours', label: '睡眠', unit: '小時', read: (day) => sum(day.sleep, (s) => s.data.durationSec / 3600) }],
    hasRecord: (day) => day.sleep.length > 0,
  },
  activity: {
    label: '活動', color: 'var(--move)', eventTypes: ['workout'],
    icon: '<path d="M3 12h4l3-8 4 16 3-8h4"/>',
    metrics: [
      { key: 'sets', label: '套數', unit: '套', read: (day) => sum(day.workouts.filter((w) => w.data.sets), (w) => w.data.sets) },
      { key: 'minutes', label: '運動', unit: '分', read: (day) => sum(day.workouts.filter((w) => w.data.durationSec), (w) => w.data.durationSec / 60) },
    ],
    hasRecord: (day) => day.workouts.length > 0,
  },
};

export const DEFAULT_CARDS = [
  { id: 'body', type: 'body', metrics: ['weightKg', 'bodyFat', 'muscleKg'] },
  { id: 'diet', type: 'diet', metrics: ['protein_g', 'water_ml', 'carbs_g'] },
  { id: 'sleep', type: 'sleep', metrics: ['hours'] },
  { id: 'activity', type: 'activity', metrics: ['sets', 'minutes'] },
];

export const metricsOf = (card) => card.metrics.map((key) => CARD_TYPES[card.type].metrics.find((m) => m.key === key)).filter(Boolean);

// 預設有紀錄就算做到；使用者設定條件後，用第一個項目判斷
export function cardHit(card, day) {
  const type = CARD_TYPES[card.type];
  if (!type.hasRecord(day)) return false;
  if (!card.rule) return true;
  const value = metricsOf(card)[0]?.read(day);
  if (value === null || value === undefined) return false;
  return card.rule.op === 'lte' ? value <= card.rule.value : value >= card.rule.value;
}

function isRequired(card, date, context) {
  if (card.days?.length) return card.days.includes(parseDateKey(date).getDay());
  if (card.type === 'activity') return context.scheduled;
  return true;
}

// 狀態只有三種：done 做到、missed 該做沒做到、off 不需要做或還沒到
// context 是當天排程：scheduled 有沒有排、targetSets 目標套數
export function cardDayState(card, date, day, today, start, context = {}) {
  const sets = day.workouts.reduce((total, w) => total + (w.data.sets ?? 0), 0);
  const belowTarget = card.type === 'activity' && context.targetSets && sets < context.targetSets;
  if (cardHit(card, day) && !belowTarget) return 'done';
  if (date > today || date < start || !isRequired(card, date, context)) return 'off';
  return 'missed';
}

export function weekDates(today) {
  const offset = (parseDateKey(today).getDay() + 6) % 7;
  return Array.from({ length: 7 }, (_, i) => addDays(today, i - offset));
}

// 回傳當月日期，前面用 null 補齊到週一開始
export function monthDates(month) {
  const first = `${month}-01`;
  const [y, m] = month.split('-').map(Number);
  const lead = (parseDateKey(first).getDay() + 6) % 7;
  const count = new Date(y, m, 0).getDate();
  return [...Array(lead).fill(null), ...Array.from({ length: count }, (_, i) => addDays(first, i))];
}
