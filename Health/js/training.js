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
