// 分析只做計算，不判斷意義。項目、數值、算法、範圍和顯示都由使用者選
import { addDays } from './logic.js';

export const CALCS = ['max', 'avg', 'sum', 'latest', 'count', 'delta', 'drop'];
export const VIEWS = ['number', 'table', 'line', 'bar', 'tags'];
export const RANGES = { '7d': 7, '4w': 28, '12w': 84, all: null };
const BUILT_IN = ['minutes', 'sets', 'reps'];

function withParents(exercises, id) {
  const ids = [];
  const seen = new Set();
  for (let current = exercises.find((e) => e.id === id); current && !seen.has(current.id); current = exercises.find((e) => e.id === current.parentId)) {
    seen.add(current.id);
    ids.push(current.id);
  }
  return ids;
}

const exerciseTags = (exercises, ids) => ids.flatMap((id) => exercises.find((e) => e.id === id)?.tags ?? []);

// 把紀錄攤平成一列一列：整次訓練一列，每筆步驟紀錄也各一列
export function collectRows(events, exercises, trainings) {
  return [...events].sort((a, b) => a.occurredAt - b.occurredAt).flatMap((event) => {
    const { data, date, occurredAt } = event;
    const routineTags = trainings.find((t) => t.id === data.trainingId)?.tags ?? [];
    const moves = data.moves ?? [];
    const moveIds = [...new Set([data.exerciseId, ...moves.map((m) => m.exerciseId)].filter(Boolean).flatMap((id) => withParents(exercises, id)))];
    const reps = moves.reduce((total, move, i) => total + (move.metrics?.reps ?? 0) * (data.sets ?? 0) + (data.extra?.[i] ?? 0), 0);
    const base = {
      date, occurredAt, eventId: event.id, activity: data.activity, trainingId: data.trainingId ?? null, exerciseIds: moveIds,
      tags: [...new Set([...routineTags, ...exerciseTags(exercises, moveIds)])],
      values: { minutes: (data.durationSec ?? 0) / 60, sets: data.sets ?? 0, reps },
    };
    const logRows = (data.logs ?? []).map((log) => {
      const ids = log.exerciseId ? withParents(exercises, log.exerciseId) : [];
      return { ...base, activity: log.label, exerciseIds: ids, tags: [...new Set([...routineTags, ...exerciseTags(exercises, ids)])], values: log.values ?? {}, isLog: true };
    });
    return [base, ...logRows];
  });
}

export function metricNames(rows) {
  const custom = new Set(rows.filter((r) => r.isLog).flatMap((r) => Object.keys(r.values)));
  return [...BUILT_IN, ...custom];
}

export function filterRows(rows, source, range, today) {
  const days = RANGES[range];
  const from = days === null || days === undefined ? '' : addDays(today, 1 - days);
  return rows.filter((row) => {
    if (row.date < from) return false;
    if (source.kind === 'exercise') return row.exerciseIds.includes(source.id);
    if (source.kind === 'routine') return row.trainingId === source.id;
    if (source.kind === 'tag') return row.tags.includes(source.id);
    return true;
  });
}

const withMetric = (rows, metric) => rows.filter((row) => typeof row.values[metric] === 'number' && (row.isLog || row.values[metric] > 0));

function dayValue(values, calc) {
  if (calc === 'max') return Math.max(...values);
  if (calc === 'sum') return values.reduce((a, b) => a + b, 0);
  if (calc === 'avg') return values.reduce((a, b) => a + b, 0) / values.length;
  if (calc === 'count') return values.length;
  if (calc === 'drop') return values[0] ? ((values[0] - values.at(-1)) / values[0]) * 100 : 0;
  return values.at(-1);
}

// 每天一個值，供圖表和表格使用
export function dailySeries(rows, metric, calc) {
  const byDate = new Map();
  for (const row of withMetric(rows, metric)) {
    if (!byDate.has(row.date)) byDate.set(row.date, []);
    byDate.get(row.date).push(row.values[metric]);
  }
  const perDay = calc === 'delta' ? 'latest' : calc;
  return [...byDate.entries()].map(([date, values]) => ({ date, value: dayValue(values, perDay) }));
}

export function headline(rows, metric, calc) {
  const values = withMetric(rows, metric).map((row) => row.values[metric]);
  if (!values.length) return null;
  const series = dailySeries(rows, metric, calc);
  if (calc === 'delta') return series.length < 2 ? null : series.at(-1).value - series.at(-2).value;
  if (calc === 'drop') return series.at(-1).value;
  return dayValue(values, calc);
}

export function byTag(rows, metric) {
  const totals = new Map();
  for (const row of withMetric(rows, metric)) {
    for (const tag of row.tags) totals.set(tag, (totals.get(tag) ?? 0) + row.values[metric]);
  }
  return [...totals.entries()].map(([tag, value]) => ({ tag, value })).sort((a, b) => b.value - a.value);
}

// 第一次使用時依現有紀錄列出卡片，之後都由使用者調整
export function defaultCards(rows) {
  const counts = new Map();
  for (const row of rows) for (const id of row.exerciseIds) counts.set(id, (counts.get(id) ?? 0) + 1);
  const top = [...counts.entries()].sort((a, b) => b[1] - a[1]).slice(0, 2).map(([id]) => id);
  const cardFor = (id) => {
    const scoped = rows.filter((row) => row.exerciseIds.includes(id));
    const metric = metricNames(scoped).find((name) => withMetric(scoped, name).length && !BUILT_IN.includes(name)) ?? 'reps';
    return { id: `default-${id}`, source: { kind: 'exercise', id }, metric, calc: 'max', range: '12w', view: 'line' };
  };
  return [{ id: 'default-time', source: { kind: 'all' }, metric: 'minutes', calc: 'sum', range: '4w', view: 'bar' }, ...top.map(cardFor)];
}

// 週期的每一週：預定和實際並排，差多少由使用者判斷
export function cycleWeeks(cycle, rows) {
  const weeks = [];
  let start = cycle.startDate;
  for (const phase of cycle.phases) {
    for (let week = 1; week <= phase.weeks; week += 1) {
      const end = addDays(start, 6);
      const inWeek = rows.filter((row) => row.date >= start && row.date <= end);
      const metrics = [...new Set(['minutes', ...phase.planned.map((p) => p.metric)])];
      weeks.push({
        index: weeks.length + 1, phase: phase.name, week, start, end,
        cells: metrics.map((metric) => ({
          metric,
          planned: phase.planned.find((p) => p.metric === metric)?.value ?? null,
          actual: withMetric(inWeek, metric).reduce((total, row) => total + row.values[metric], 0),
        })),
      });
      start = addDays(start, 7);
    }
  }
  return weeks;
}

export function currentWeek(cycle, today) {
  const total = cycle.phases.reduce((sum, phase) => sum + phase.weeks, 0);
  const days = Math.floor((new Date(`${today}T12:00:00`) - new Date(`${cycle.startDate}T12:00:00`)) / 86400000);
  if (days < 0 || days >= total * 7) return null;
  const index = Math.floor(days / 7);
  let passed = 0;
  for (const phase of cycle.phases) {
    if (index < passed + phase.weeks) return { index: index + 1, total, phase: phase.name };
    passed += phase.weeks;
  }
  return null;
}
