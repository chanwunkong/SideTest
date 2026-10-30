// 純計算：日期、目標、每日標準、每日彙總與達成判定。不碰畫面和資料庫
import { registry } from './registry.js';

export const DEFAULT_GOAL = {
  effectiveFrom: '0000-01-01',
  body: { targetWeightKg: null, targetBodyFat: null, targetDate: null },
  nutritionTargets: {},
  // 預留：優先序為單日 > 星期 > 全部，日後支援訓練日與休息日不同目標
  dayOverrides: { weekday: {}, date: {} },
  sleep: { hours: 7, bedtime: '23:00' },
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

export function targetWeight(goal, weight, bodyFat) {
  const { targetWeightKg, targetBodyFat } = goal.body;
  if (targetWeightKg) return targetWeightKg;
  if (targetBodyFat && bodyFat) return (weight * (1 - bodyFat / 100)) / (1 - targetBodyFat / 100);
  return null;
}

// 使用者設定的飲食目標，沒填的項目就沒有目標
export function nutritionTargetsFor(goal, date) {
  const { weekday = {}, date: byDate = {} } = goal.dayOverrides;
  return { ...goal.nutritionTargets, ...weekday[parseDateKey(date).getDay()], ...byDate[date] };
}

// 最近 7 天（含今天）有紀錄的日子平均，避免沒記錄的日子把平均拉低
export function sevenDayAverage(eventsOn, today) {
  const days = Array.from({ length: 7 }, (_, i) => summarizeDay(eventsOn(addDays(today, -i))))
    .filter((day) => day.meals.length + day.intakes.length > 0);
  const totals = {};
  for (const day of days) {
    for (const [key, value] of Object.entries(day.nutrients)) totals[key] = (totals[key] || 0) + value;
  }
  const average = Object.fromEntries(Object.entries(totals).map(([key, value]) => [key, value / days.length]));
  return { average, dayCount: days.length };
}

export function summarizeDay(events) {
  const day = { nutrients: {}, meals: [], intakes: [], workouts: [], body: [], sleep: [] };
  for (const event of events) {
    const def = registry[event.type];
    if (def && event.schemaVersion <= def.version) def.summarize(day, event.data, event);
  }
  return day;
}
