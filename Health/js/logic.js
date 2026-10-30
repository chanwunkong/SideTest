// 純計算：日期、目標、每日標準、每日彙總與達成判定。不碰畫面和資料庫
import { registry } from './registry.js';

// 資料沒填齊時才用的通用值，畫面上要標明不是依個人計算
export const GENERIC_TARGETS = { water_ml: 2500, protein_g: 120, carbs_g: 250, fat_g: 60, kcal: 2000, caffeine_mg: 400, creatine_g: 5 };

export const DEFAULT_GOAL = {
  effectiveFrom: '0000-01-01',
  body: { targetWeightKg: null, targetBodyFat: null, targetDate: null },
  targetOverrides: {},
  // 預留：優先序為單日 > 星期 > 訓練日／休息日 > 全部
  dayOverrides: { dayType: {}, weekday: {}, date: {} },
  workoutPlan: {},
  sleep: { hours: 7, bedtime: '23:00' },
};

const ACTIVITY_FACTOR = { training: 1.6, rest: 1.3 };
const KCAL_PER_KG = 7700;
const MAX_DEFICIT = 0.2;
const MAX_SURPLUS = 0.1;

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

const daysBetween = (from, to) => Math.round((parseDateKey(to) - parseDateKey(from)) / 86400000);

export function goalFor(goals, date) {
  const active = goals
    .filter((g) => g.effectiveFrom <= date)
    .sort((a, b) => b.effectiveFrom.localeCompare(a.effectiveFrom))[0];
  return { ...DEFAULT_GOAL, ...active };
}

export const planFor = (goal, date) => goal.workoutPlan[parseDateKey(date).getDay()] ?? null;

function baseMetabolism(weight, bodyFat, profile, date) {
  if (bodyFat) return 370 + 21.6 * weight * (1 - bodyFat / 100); // Katch-McArdle
  const { heightCm, birthYear, sex } = profile;
  if (!heightCm || !birthYear || !sex) return null;
  const age = parseDateKey(date).getFullYear() - birthYear;
  return 10 * weight + 6.25 * heightCm - 5 * age + (sex === 'male' ? 5 : -161); // Mifflin-St Jeor
}

export function targetWeight(goal, weight, bodyFat) {
  const { targetWeightKg, targetBodyFat } = goal.body;
  if (targetWeightKg) return targetWeightKg;
  if (targetBodyFat && bodyFat) return (weight * (1 - bodyFat / 100)) / (1 - targetBodyFat / 100);
  return null;
}

// 回傳當天每日標準。body 為當天以前最新的體重與體脂
export function dailyTargets(goal, profile, body, date) {
  const dayType = planFor(goal, date) ? 'training' : 'rest';
  const weight = body?.weightKg;
  const bmr = weight ? baseMetabolism(weight, body.bodyFat, profile, date) : null;
  const result = { dayType, personalized: !!bmr, capped: false, targets: { ...GENERIC_TARGETS } };

  if (bmr) {
    const tdee = bmr * ACTIVITY_FACTOR[dayType];
    const goalWeight = targetWeight(goal, weight, body.bodyFat);
    const days = goal.body.targetDate ? daysBetween(date, goal.body.targetDate) : 0;
    let delta = goalWeight && days > 0 ? ((goalWeight - weight) * KCAL_PER_KG) / days : 0;
    const limited = Math.min(tdee * MAX_SURPLUS, Math.max(-tdee * MAX_DEFICIT, delta));
    result.capped = limited !== delta;
    delta = limited;
    const kcal = Math.round((tdee + delta) / 10) * 10;
    const protein = Math.round(weight * (delta < 0 ? 2 : 1.8));
    const fat = Math.round(weight * 0.8);
    Object.assign(result.targets, {
      kcal, protein_g: protein, fat_g: fat,
      carbs_g: Math.max(0, Math.round((kcal - protein * 4 - fat * 9) / 4)),
      water_ml: Math.round((weight * 35) / 50) * 50,
    });
  }

  const { dayType: byType = {}, weekday = {}, date: byDate = {} } = goal.dayOverrides;
  Object.assign(result.targets, goal.targetOverrides, byType[dayType], weekday[parseDateKey(date).getDay()], byDate[date]);
  return result;
}

export function summarizeDay(events) {
  const day = { nutrients: {}, meals: [], intakes: [], workouts: [], body: [], sleep: [] };
  for (const event of events) {
    const def = registry[event.type];
    if (def && event.schemaVersion <= def.version) def.summarize(day, event.data, event);
  }
  return day;
}
