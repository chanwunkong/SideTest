import assert from 'node:assert/strict';
import { test } from 'node:test';
import { cardDayState, cardHit, DEFAULT_CARDS, metricsOf, monthDates, periodDates, weekDates } from '../js/cards.js';
import { addDays, allTargetsMet, DEFAULT_GOAL, formatTarget, goalFor, nutritionTargetsFor, sevenDayAverage, summarizeDay, targetMet, weekStart } from '../../core/logic.js';
import { descendantIds, exercisePath, exerciseTotal, exerciseTree, moveTotals, personalBests, resolveExercise, scheduleActiveOn, scheduleRemaining, targetSetsOn, weeklyStats } from '../../core/training.js';
import { MEAL_FORMAT, parseMeal } from '../js/mealImport.js';
import { byTag, collectRows, currentWeek, cycleWeeks, dailySeries, defaultCards, filterRows, headline, metricNames } from '../../core/analysis.js';
import { advanceBy, flattenFlow, flowDuration, flowToRecord, lastLogValue, normalizeFlow } from '../../core/flow.js';
import { normalizeEvent, registry } from '../../core/registry.js';

const meal = (nutrients = {}) => ({ type: 'meal', schemaVersion: 1, data: { nutrients } });
const workout = { type: 'workout', schemaVersion: 1, data: { activity: '攀岩', durationSec: 3600 } };

test('加總餐點與快速紀錄，略過未知類型', () => {
  const day = summarizeDay([
    meal({ kcal: 600, protein_g: 30 }),
    { type: 'intake', schemaVersion: 1, data: { nutrients: { water_ml: 250, caffeine_mg: 100 } } },
    { type: 'bloodPressure', schemaVersion: 1, data: { systolic: 120 } },
  ]);
  assert.deepEqual(day.nutrients, { kcal: 600, protein_g: 30, water_ml: 250, caffeine_mg: 100 });
});

const [bodyCard, dietCard, sleepCard, activityCard] = DEFAULT_CARDS;
const sleepDay = (h) => summarizeDay([{ type: 'sleep', schemaVersion: 1, data: { durationSec: h * 3600 } }]);
const empty = summarizeDay([]);

test('卡片預設有紀錄就算做到，條件由使用者設定', () => {
  assert.equal(cardHit(sleepCard, sleepDay(5)), true);
  assert.equal(cardHit(sleepCard, empty), false);
  const strict = { ...sleepCard, rule: { op: 'gte', value: 7 } };
  assert.equal(cardHit(strict, sleepDay(5)), false);
  assert.equal(cardHit(strict, sleepDay(7.5)), true);
  const limit = { ...dietCard, metrics: ['caffeine_mg'], rule: { op: 'lte', value: 200 } };
  assert.equal(cardHit(limit, summarizeDay([meal({ caffeine_mg: 100 })])), true);
  assert.equal(cardHit(limit, summarizeDay([meal({ caffeine_mg: 300 })])), false);
});

test('卡片每日狀態：沒排定和開始前都不算沒做到', () => {
  const today = '2026-10-30';
  const start = '2026-10-01';
  assert.equal(cardDayState(sleepCard, '2026-10-20', empty, today, start), 'missed');
  assert.equal(cardDayState(sleepCard, '2026-09-20', empty, today, start), 'off');
  assert.equal(cardDayState(sleepCard, '2026-10-31', empty, today, start), 'off');
  assert.equal(cardDayState(activityCard, '2026-10-19', empty, today, start), 'off');
  assert.equal(cardDayState(activityCard, '2026-10-19', empty, today, start, { scheduled: true }), 'missed');
  assert.equal(cardDayState(activityCard, '2026-10-20', summarizeDay([workout]), today, start), 'done');
  assert.equal(cardDayState(bodyCard, '2026-10-20', empty, today, start), 'off'); // 身體組成預設不固定
  const setDays = { ...bodyCard, frequency: { type: 'weekdays' }, days: [6] };
  assert.equal(cardDayState(setDays, '2026-10-20', empty, today, start), 'off');
  assert.equal(cardDayState(setDays, '2026-10-24', empty, today, start), 'missed'); // 週六
  const legacy = { ...bodyCard, days: [6] };
  assert.equal(cardDayState(legacy, '2026-10-24', empty, today, start), 'missed');
});

test('每週幾次：只在今天提示，達到次數就不提示', () => {
  const today = '2026-10-30';
  const weekly = { ...bodyCard, frequency: { type: 'weekly', count: 2 } };
  assert.equal(cardDayState(weekly, today, empty, today, '2026-10-01', { periodHits: 1 }), 'missed');
  assert.equal(cardDayState(weekly, today, empty, today, '2026-10-01', { periodHits: 2 }), 'off');
  assert.equal(cardDayState(weekly, '2026-10-29', empty, today, '2026-10-01', { periodHits: 0 }), 'off');
  assert.deepEqual(periodDates(weekly, today).at(0), '2026-10-26');
  assert.deepEqual(periodDates(weekly, today).at(-1), today);
});

test('只量體脂也算有紀錄，且各項目分開讀取', () => {
  const day = summarizeDay([
    { type: 'body', schemaVersion: 1, data: { weightKg: 70, bodyFat: 18 } },
    { type: 'body', schemaVersion: 1, data: { weightKg: 69.5, bodyFat: null } },
  ]);
  const [weight, fat] = metricsOf(bodyCard);
  assert.equal(weight.read(day), 69.5);
  assert.equal(fat.read(day), 18);
  assert.equal(registry.body.validate({ bodyFat: 18 }), true);
  assert.equal(registry.body.validate({}), false);
});

test('多項卡片保留使用者選的順序', () => {
  assert.deepEqual(metricsOf(dietCard).map((m) => m.label), ['熱量', '蛋白質', '水分']);
});

test('週與月的日期範圍', () => {
  assert.deepEqual(weekDates('2026-10-30').slice(0, 2), ['2026-10-26', '2026-10-27']);
  assert.equal(weekDates('2026-10-30').at(-1), '2026-11-01');
  const month = monthDates('2026-10');
  assert.equal(month.filter((d) => d === null).length, 3); // 10/1 是週四
  assert.equal(month.at(-1), '2026-10-31');
});

test('飲食目標只有使用者填的項目，單日可覆蓋', () => {
  const goal = { ...DEFAULT_GOAL, nutritionTargets: { protein_g: { min: 140, max: null } }, dayOverrides: { weekday: { 2: { protein_g: 100 } }, date: { '2026-10-20': { protein_g: 90 } } } };
  assert.deepEqual(nutritionTargetsFor(DEFAULT_GOAL, '2026-10-21'), {});
  assert.equal(nutritionTargetsFor(goal, '2026-10-21').protein_g.min, 140);
  assert.equal(nutritionTargetsFor(goal, '2026-10-27').protein_g, 100); // 週二
  assert.equal(nutritionTargetsFor(goal, '2026-10-20').protein_g, 90);
});

test('目標支援範圍、至少、至多，舊資料的單一數字視為至少', () => {
  const range = { min: 2300, max: 2700 };
  assert.equal(targetMet(range, 2500), true);
  assert.equal(targetMet(range, 2800), false);
  assert.equal(targetMet({ min: 140, max: null }, 100), false);
  assert.equal(targetMet({ min: null, max: 400 }, undefined), true);
  assert.equal(targetMet(140, 150), true);
  assert.equal(formatTarget(range), '2300–2700');
  assert.equal(formatTarget({ min: 140, max: null }), '≥140');
  assert.equal(formatTarget({ min: null, max: 400 }), '≤400');
  assert.equal(formatTarget(undefined), null);
  assert.equal(allTargetsMet({}, {}), false);
  assert.equal(allTargetsMet({ kcal: range, caffeine_mg: { max: 400 } }, { kcal: 2500 }), true);
});

test('飲食卡可要求全部目標達成才亮，沒設定目標時有紀錄就亮', () => {
  const strict = { ...dietCard, requireTargets: true };
  const day = summarizeDay([meal({ kcal: 2000 })]);
  const today = '2026-10-30';
  const context = { nutritionTargets: { kcal: { min: 2300, max: 2700 } } };
  assert.equal(cardDayState(strict, today, day, today, today, context), 'missed');
  assert.equal(cardDayState(dietCard, today, day, today, today, context), 'done');
  assert.equal(cardDayState(strict, today, day, today, today, { nutritionTargets: {} }), 'done');
  assert.equal(cardDayState(strict, today, summarizeDay([meal({ kcal: 2500 })]), today, today, context), 'done');
});

test('7 天平均只算有紀錄的日子', () => {
  const byDate = { '2026-10-30': [meal({ protein_g: 100 })], '2026-10-28': [meal({ protein_g: 50 })], '2026-10-01': [meal({ protein_g: 999 })] };
  const { average, dayCount } = sevenDayAverage((d) => byDate[d] ?? [], '2026-10-30');
  assert.equal(dayCount, 2);
  assert.equal(average.protein_g, 75);
  assert.deepEqual(sevenDayAverage(() => [], '2026-10-30'), { average: {}, dayCount: 0 });
});

const schedule = { startDate: '2026-10-30', days: 3, weekdays: [], targetSets: null };

test('排程：持續天數與星期', () => {
  assert.equal(scheduleActiveOn(schedule, '2026-10-29'), false);
  assert.equal(scheduleActiveOn(schedule, '2026-11-01'), true);
  assert.equal(scheduleActiveOn(schedule, '2026-11-02'), false);
  assert.equal(scheduleActiveOn({ ...schedule, days: null }, '2027-05-01'), true);
  assert.equal(scheduleActiveOn({ ...schedule, days: null, weekdays: [1] }, '2026-11-03'), false); // 週二
  assert.equal(scheduleRemaining(schedule, '2026-10-31'), 2);
  assert.equal(scheduleRemaining(schedule, '2026-11-09'), 0);
});

test('目標套數未達成不算做到，沒設定就是有記錄算做到', () => {
  const today = '2026-10-30';
  const logged = summarizeDay([{ type: 'workout', schemaVersion: 1, data: { trainingId: 'c', sets: 4 } }]);
  assert.equal(cardDayState(activityCard, today, logged, today, today, { scheduled: true }), 'done');
  assert.equal(cardDayState(activityCard, today, logged, today, today, { scheduled: true, targetSets: 5 }), 'missed');
  assert.equal(targetSetsOn([{ ...schedule, targetSets: 5 }], today), 5);
});

const sampleFlow = normalizeFlow([
  { type: 'rest', label: '準備', duration: 10 },
  { type: 'loop', iterations: 2, children: [
    { type: 'timer', label: '懸掛', duration: 7, exerciseId: 'hang' },
    { type: 'loop', iterations: 2, children: [{ type: 'reps', label: '引體', count: 5, exerciseId: 'pull' }] },
    { type: 'rest', label: '休息', duration: 3, skipOnLast: true },
  ] },
]);

test('巢狀流程展開、循環位置、最後一次略過', () => {
  const steps = flattenFlow(sampleFlow);
  assert.equal(steps.length, 8);
  assert.equal(flowDuration(sampleFlow), 27);
  assert.deepEqual(steps[3].loopState, [{ current: 1, total: 2 }, { current: 2, total: 2 }]);
  assert.equal(steps[1].color, 'orange');
  assert.equal(steps[0].rest, true);
});

test('訓練紀錄沿用同一套格式，休息不算動作，並保留每步紀錄', () => {
  const logs = [{ label: '懸掛', values: { weightKg: 5 }, failure: true }];
  const record = flowToRecord({ id: 't', name: '懸吊' }, flattenFlow(sampleFlow), logs, 120.4, true);
  assert.equal(record.durationSec, 120);
  assert.equal(record.logs[0].failure, true);
  assert.deepEqual(record.moves.map((m) => [m.exerciseId, m.metrics]), [['hang', { holdSec: 14 }], ['pull', { reps: 20 }]]);
  assert.equal(exerciseTotal([record], [{ id: 'pull', parentId: null, name: '引體' }], 'pull'), 20);
});

test('切回畫面時依經過時間前進，遇到次數步驟就停', () => {
  const queue = [{ duration: 10 }, { duration: 5 }, { count: 5 }, { duration: 3 }];
  assert.deepEqual(advanceBy(queue, 0, 10, 4), { index: 0, remaining: 6, passed: [], finished: false });
  assert.deepEqual(advanceBy(queue, 0, 10, 12), { index: 1, remaining: 3, passed: [0], finished: false });
  assert.deepEqual(advanceBy(queue, 0, 10, 100), { index: 2, remaining: null, passed: [0, 1], finished: false });
  assert.deepEqual(advanceBy(queue, 3, 3, 9), { index: 4, remaining: 0, passed: [3], finished: true });
});

test('舊的記錄代碼轉成名稱，自訂名稱保留', () => {
  const [step] = normalizeFlow([{ type: 'timer', label: '懸掛', duration: 7, metrics: ['weightKg', { name: 'RPE' }] }]);
  assert.deepEqual(step.metrics, [{ name: '重量' }, { name: 'RPE' }]);
});

test('次數步驟的預估時間算入總時間', () => {
  assert.equal(flowDuration(normalizeFlow([{ type: 'reps', label: '引體', count: 5, estimate: 30 }, { type: 'timer', duration: 10 }])), 40);
});

test('上次的紀錄值會帶入', () => {
  const events = [
    { occurredAt: 1, data: { logs: [{ label: '懸掛', values: { weightKg: 5 } }] } },
    { occurredAt: 2, data: { logs: [{ label: '懸掛', values: { weightKg: 7 } }, { label: '別的', values: { weightKg: 1 } }] } },
  ];
  assert.equal(lastLogValue(events, '懸掛', 'weightKg'), 7);
  assert.equal(lastLogValue(events, '懸掛', 'reps'), null);
});

test('週統計與個人最佳', () => {
  const events = [
    { date: '2026-10-30', data: { sets: 4, moves: [{ exerciseId: 'a', name: '深蹲', metrics: { reps: 10, weightKg: 60 } }] } },
    { date: '2026-10-29', data: { sets: 6, moves: [{ exerciseId: 'a', name: '深蹲', metrics: { reps: 10 } }], extra: { 0: 3 } } },
    { date: '2026-10-20', data: { durationSec: 1800 } },
  ];
  const weeks = weeklyStats(events, '2026-10-30');
  assert.equal(weeks.length, 4);
  assert.deepEqual(weeks.at(-1), { start: '2026-10-26', days: 2, reps: 103, minutes: 0 });
  assert.equal(weeks.at(-2).minutes, 30);
  assert.deepEqual(personalBests(events).map((b) => [b.reps, b.weightKg]), [[63, 60]]);
});

const exercises = [
  { id: 'squat', name: '深蹲', parentId: null },
  { id: 'barbell', name: '槓鈴深蹲', parentId: 'squat' },
  { id: 'full', name: '全蹲', parentId: 'barbell' },
  { id: 'pull', name: 'Pull-up', parentId: null },
];

test('動作庫：上下層、路徑與名稱比對', () => {
  assert.deepEqual(exerciseTree(exercises).map(({ exercise, depth }) => [exercise.id, depth]), [['pull', 0], ['squat', 0], ['barbell', 1], ['full', 2]]);
  assert.deepEqual(descendantIds(exercises, 'squat'), ['barbell', 'full']);
  assert.equal(exercisePath(exercises, 'full'), '深蹲 › 槓鈴深蹲 › 全蹲');
  assert.equal(resolveExercise(exercises, '深蹲 › 槓鈴深蹲 › 全蹲').id, 'full');
  assert.equal(resolveExercise(exercises, 'pull-UP').id, 'pull');
  assert.equal(resolveExercise(exercises, '硬舉'), null);
  assert.equal(exercisePath([{ id: 'a', name: 'A', parentId: 'b' }, { id: 'b', name: 'B', parentId: 'a' }], 'a'), 'B › A');
});

test('累計次數可以合計到上層', () => {
  const records = [
    { sets: 3, moves: [{ exerciseId: 'full', metrics: { reps: 10 } }, { exerciseId: 'pull', metrics: { reps: 5 } }], extra: { 0: 2 } },
    { sets: 2, moves: [{ exerciseId: 'squat', metrics: { reps: 15 } }] },
  ];
  assert.equal(exerciseTotal(records, exercises, 'full'), 32);
  assert.equal(exerciseTotal(records, exercises, 'squat'), 62);
  assert.equal(exerciseTotal(records, exercises, 'pull'), 15);
});

test('總量用當天保存的動作內容，含另外多做的次數', () => {
  const moves = [{ name: '引體向上', metrics: { reps: 5 } }, { name: '懸吊', metrics: { holdSec: 10 } }];
  assert.deepEqual(moveTotals({ moves, sets: 6, extra: { 0: 3 } }), [{ name: '引體向上', total: 33 }]);
});

test('Gemini 回覆：容忍前後文字，拒絕錯誤內容', () => {
  const body = JSON.stringify({ format: MEAL_FORMAT, items: [{ name: '便當', portion: '1 份' }], confidence: 'medium',
    nutrients: { water_ml: 100, protein_g: 38, carbs_g: 90, fat_g: 25, kcal: 750, caffeine_mg: 0 } });
  const meal = parseMeal(['好的：', '```json', body, '```', '以上'].join('\n'));
  assert.equal(meal.nutrients.kcal, 750);
  assert.equal(meal.items[0].name, '便當');
  assert.throws(() => parseMeal('今天吃得不錯'), /no_data/);
  assert.throws(() => parseMeal('{"a":1}'), /not_meal/);
  assert.throws(() => parseMeal(`{"format":"${MEAL_FORMAT}","error":"not_food"}`), /not_food/);
  assert.throws(() => parseMeal(body.replace('750', '"很多"')), /bad_value/);
});

test('修改目標不影響生效日之前', () => {
  const goals = [{ effectiveFrom: '2026-11-01', sleep: { hours: 8, bedtime: '22:30' } }];
  assert.equal(goalFor(goals, '2026-10-31').sleep.hours, 7);
  assert.equal(goalFor(goals, '2026-11-02').sleep.hours, 8);
});

test('舊版事件讀取時升級，未知類型原樣保留', () => {
  registry.demo = { version: 2, upgrade: { 1: (d) => ({ ml: d.l * 1000 }) } };
  assert.deepEqual(normalizeEvent({ type: 'demo', schemaVersion: 1, data: { l: 2 } }).data, { ml: 2000 });
  delete registry.demo;
  const unknown = { type: 'future', schemaVersion: 9, data: { a: 1 } };
  assert.equal(normalizeEvent(unknown), unknown);
});

test('日期計算跨月、以週一為一週開始', () => {
  assert.equal(addDays('2026-10-31', 1), '2026-11-01');
  assert.equal(weekStart('2026-11-01'), '2026-10-26');
});

test('分析：攤平紀錄，依動作上層和分類篩選，算法只做計算', () => {
  const exercises = [{ id: 'squat', name: '深蹲', parentId: null, tags: ['下肢'] }, { id: 'bar', name: '槓鈴深蹲', parentId: 'squat', tags: ['肌力'] }];
  const event = (date, weights) => ({
    id: date, date, occurredAt: Date.parse(date),
    data: { activity: '腿', durationSec: 600, sets: 2, moves: [{ exerciseId: 'bar', name: '槓鈴深蹲', metrics: { reps: 5 } }], logs: weights.map((v) => ({ label: '槓鈴深蹲', exerciseId: 'bar', values: { 速度: v } })) },
  });
  const rows = collectRows([event('2026-10-20', [1, 0.8]), event('2026-10-27', [1.2, 0.9])], exercises, []);
  const today = '2026-10-30';
  const squat = filterRows(rows, { kind: 'exercise', id: 'squat' }, 'all', today);
  assert.equal(squat.length, 6);
  assert.ok(metricNames(rows).includes('速度'));
  assert.equal(headline(squat, '速度', 'max'), 1.2);
  assert.equal(Math.round(headline(squat, '速度', 'drop')), 25);
  assert.equal(Math.round(headline(squat, '速度', 'delta') * 10) / 10, 0.1);
  assert.deepEqual(dailySeries(squat, '速度', 'max').map((p) => p.value), [1, 1.2]);
  assert.equal(filterRows(rows, { kind: 'tag', id: '下肢' }, '7d', today).length, 3);
  assert.equal(headline(filterRows(rows, { kind: 'all' }, 'all', today), '速度', 'max'), 1.2);
  assert.equal(headline(filterRows(rows, { kind: 'all' }, 'all', today), 'minutes', 'sum'), 20);
  assert.deepEqual(byTag(filterRows(rows, { kind: 'all' }, 'all', today), 'minutes'), [{ tag: '肌力', value: 20 }, { tag: '下肢', value: 20 }]);
  assert.equal(headline([], '速度', 'max'), null);
  assert.equal(defaultCards(rows)[1].metric, '速度');
});

test('週期：每週預定和實際並排，只做加總', () => {
  const cycle = { startDate: '2026-11-02', phases: [{ name: '基礎', weeks: 2, planned: [{ metric: 'minutes', value: 100 }] }, { name: '減量', weeks: 1, planned: [] }] };
  const rows = [{ date: '2026-11-03', values: { minutes: 40 } }, { date: '2026-11-05', values: { minutes: 30 } }, { date: '2026-11-10', values: { minutes: 20 } }];
  const weeks = cycleWeeks(cycle, rows);
  assert.equal(weeks.length, 3);
  assert.deepEqual(weeks[0].cells, [{ metric: 'minutes', planned: 100, actual: 70 }]);
  assert.equal(weeks[1].cells[0].actual, 20);
  assert.equal(weeks[2].phase, '減量');
  assert.equal(weeks[2].cells[0].planned, null);
  assert.deepEqual(currentWeek(cycle, '2026-11-17'), { index: 3, total: 3, phase: '減量' });
  assert.equal(currentWeek(cycle, '2026-11-01'), null);
  assert.equal(currentWeek(cycle, '2026-11-23'), null);
});
