// 運動紀錄的定義與計算：模式和指標都是清單，新增時不影響舊資料
import { addDays, parseDateKey } from './logic.js';

export const MODES = ['check', 'sets'];

// 指標全部選填；reps 預設顯示，其餘放在更多指標
export const MOVE_METRICS = [
  { key: 'reps', type: 'number' },
  { key: 'holdSec', type: 'number' },
  { key: 'weightKg', type: 'number' },
  { key: 'distanceM', type: 'number' },
  { key: 'gripType', type: 'text' },
  { key: 'edgeMm', type: 'number' },
];

export function scheduleActiveOn(schedule, date) {
  if (date < schedule.startDate) return false;
  if (schedule.days && date > addDays(schedule.startDate, schedule.days - 1)) return false;
  return !schedule.weekdays?.length || schedule.weekdays.includes(parseDateKey(date).getDay());
}

export function schedulesOn(schedules, date) {
  return schedules.filter((s) => scheduleActiveOn(s, date));
}

export function targetSetsOn(schedules, date) {
  const targets = schedulesOn(schedules, date).map((s) => s.targetSets).filter(Boolean);
  return targets.length ? Math.max(...targets) : null;
}

export function scheduleRemaining(schedule, today) {
  if (!schedule.days) return null;
  const last = addDays(schedule.startDate, schedule.days - 1);
  if (today > last) return 0;
  return Math.round((parseDateKey(last) - parseDateKey(today < schedule.startDate ? schedule.startDate : today)) / 86400000) + 1;
}

// 紀錄保存當天的動作內容，算總量不受之後修改訓練影響
export function moveTotals(record) {
  const { moves = [], sets = 0, extra = {} } = record;
  return moves
    .map((move, i) => ({ name: move.name, total: (move.metrics?.reps ?? 0) * sets + (extra[i] ?? 0) }))
    .filter((move) => move.total > 0);
}

// ---- 動作庫：使用者自己的動作，可以有上下層 ----

const childrenOf = (list, parentId) => list
  .filter((e) => (e.parentId ?? null) === parentId)
  .sort((a, b) => a.name.localeCompare(b.name));

export function exerciseTree(list, parentId = null, depth = 0) {
  return childrenOf(list, parentId).flatMap((exercise) => [{ exercise, depth }, ...exerciseTree(list, exercise.id, depth + 1)]);
}

export function descendantIds(list, id) {
  return childrenOf(list, id).flatMap((child) => [child.id, ...descendantIds(list, child.id)]);
}

export function exercisePath(list, id, nameOf = (e) => e.name) {
  const names = [];
  const seen = new Set();
  for (let current = list.find((e) => e.id === id); current && !seen.has(current.id); current = list.find((e) => e.id === current.parentId)) {
    seen.add(current.id);
    names.unshift(nameOf(current));
  }
  return names.join(' › ');
}

// 輸入可能是完整路徑或單一名稱，大小寫不影響比對
export function resolveExercise(list, text, nameOf = (e) => e.name) {
  const wanted = text.trim().toLowerCase();
  return list.find((e) => exercisePath(list, e.id, nameOf).toLowerCase() === wanted)
    ?? list.find((e) => nameOf(e).toLowerCase() === wanted || e.name.toLowerCase() === wanted)
    ?? null;
}

// 累計次數可以合計到上層：深蹲包含底下的槓鈴全蹲
export function exerciseTotal(records, list, id) {
  const ids = new Set([id, ...descendantIds(list, id)]);
  return records.reduce((total, { sets = 0, moves = [], extra = {} }) => total + moves.reduce((sum, move, i) => (
    ids.has(move.exerciseId) ? sum + (move.metrics?.reps ?? 0) * sets + (extra[i] ?? 0) : sum
  ), 0), 0);
}

// ---- 第一批分析：只用已記錄的資料計算 ----

const repsOf = ({ moves = [], sets = 0, extra = {} }) => moves.reduce((total, move, i) => total + (move.metrics?.reps ?? 0) * sets + (extra[i] ?? 0), 0);

// 最近 4 週，舊到新
export function weeklyStats(events, today) {
  const monday = addDays(today, -((parseDateKey(today).getDay() + 6) % 7));
  return [21, 14, 7, 0].map((back) => {
    const start = addDays(monday, -back);
    const end = addDays(start, 6);
    const records = events.filter((e) => e.date >= start && e.date <= end);
    return {
      start,
      days: new Set(records.map((e) => e.date)).size,
      reps: records.reduce((total, e) => total + repsOf(e.data), 0),
      minutes: Math.round(records.reduce((total, e) => total + (e.data.durationSec ?? 0), 0) / 60),
    };
  });
}

// 每個動作單次最多次數和最大負重
export function personalBests(events) {
  const bests = new Map();
  for (const { data } of events) {
    (data.moves ?? []).forEach((move, i) => {
      const key = move.exerciseId ?? move.name;
      const best = bests.get(key) ?? { key, exerciseId: move.exerciseId, name: move.name, reps: 0, weightKg: 0 };
      const reps = (move.metrics?.reps ?? 0) * (data.sets ?? 0) + (data.extra?.[i] ?? 0);
      bests.set(key, { ...best, reps: Math.max(best.reps, reps), weightKg: Math.max(best.weightKg, move.metrics?.weightKg ?? 0) });
    });
  }
  return [...bests.values()].filter((b) => b.reps || b.weightKg).sort((a, b) => b.reps - a.reps);
}
