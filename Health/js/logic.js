// 純計算：日期、目標生效、每日彙總與達成判定。不碰畫面和資料庫
import { registry } from './registry.js';

export const DEFAULT_GOAL = {
  effectiveFrom: '0000-01-01',
  targets: { kcal: 2000, carbs_g: 250, fat_g: 60, protein_g: 120, water_ml: 2500, caffeine_mg: 400, creatine_g: 5 },
  workoutDays: [1, 3, 5],
  minMeals: 2,
  quick: { water_ml: 250, caffeine_mg: 100, creatine_g: 5 },
};

const pad = (n) => String(n).padStart(2, '0');

export function dateKey(date = new Date()) {
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
}

export function parseDateKey(key) {
  const [y, m, d] = key.split('-').map(Number);
  return new Date(y, m - 1, d);
}

export function addDays(key, delta) {
  const date = parseDateKey(key);
  date.setDate(date.getDate() + delta);
  return dateKey(date);
}

export function weekStart(key) {
  const day = parseDateKey(key).getDay();
  return addDays(key, -((day + 6) % 7));
}

export function goalFor(goals, date) {
  const active = goals
    .filter((g) => g.effectiveFrom <= date)
    .sort((a, b) => b.effectiveFrom.localeCompare(a.effectiveFrom))[0];
  return { ...DEFAULT_GOAL, ...active };
}

export function summarizeDay(events) {
  const day = { nutrients: {}, meals: [], intakes: [], workouts: [] };
  for (const event of events) {
    const def = registry[event.type];
    if (def && event.schemaVersion <= def.version) def.summarize(day, event.data, event);
  }
  return day;
}

// 狀態：done 達成、missed 未達成、na 不適用、nodata 尚無資料（含今天還沒結束）
export function dietStatus(day, date, today, goal, trackingStart) {
  if (day.meals.length >= goal.minMeals) return 'done';
  if (date >= today || !trackingStart || date < trackingStart) return 'nodata';
  return 'missed';
}

export function workoutStatus(day, date, today, goal, trackingStart) {
  if (day.workouts.length) return 'done';
  if (!goal.workoutDays.includes(parseDateKey(date).getDay())) return 'na';
  if (date >= today || !trackingStart || date < trackingStart) return 'nodata';
  return 'missed';
}
