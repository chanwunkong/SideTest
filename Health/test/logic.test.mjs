import assert from 'node:assert/strict';
import { test } from 'node:test';
import { cardDayState, cardHit, DEFAULT_CARDS, metricsOf, monthDates, weekDates } from '../js/cards.js';
import { addDays, DEFAULT_GOAL, goalFor, nutritionTargetsFor, sevenDayAverage, summarizeDay, weekStart } from '../js/logic.js';
import { moveTotals, scheduleActiveOn, scheduleRemaining, targetSetsOn } from '../js/training.js';
import { MEAL_FORMAT, parseMeal } from '../js/mealImport.js';
import { normalizeEvent, registry } from '../js/registry.js';

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
  const weekly = { ...bodyCard, days: [6] };
  assert.equal(cardDayState(weekly, '2026-10-20', empty, today, start), 'off');
  assert.equal(cardDayState(weekly, '2026-10-24', empty, today, start), 'missed'); // 週六
});

test('多項卡片保留使用者選的順序', () => {
  assert.deepEqual(metricsOf(dietCard).map((m) => m.label), ['蛋白質', '水分', '碳水']);
});

test('週與月的日期範圍', () => {
  assert.deepEqual(weekDates('2026-10-30').slice(0, 2), ['2026-10-26', '2026-10-27']);
  assert.equal(weekDates('2026-10-30').at(-1), '2026-11-01');
  const month = monthDates('2026-10');
  assert.equal(month.filter((d) => d === null).length, 3); // 10/1 是週四
  assert.equal(month.at(-1), '2026-10-31');
});

test('飲食目標只有使用者填的項目，單日可覆蓋', () => {
  const goal = { ...DEFAULT_GOAL, nutritionTargets: { protein_g: 140 }, dayOverrides: { weekday: { 2: { protein_g: 100 } }, date: { '2026-10-20': { protein_g: 90 } } } };
  assert.deepEqual(nutritionTargetsFor(DEFAULT_GOAL, '2026-10-21'), {});
  assert.equal(nutritionTargetsFor(goal, '2026-10-21').protein_g, 140);
  assert.equal(nutritionTargetsFor(goal, '2026-10-27').protein_g, 100); // 週二
  assert.equal(nutritionTargetsFor(goal, '2026-10-20').protein_g, 90);
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
  assert.throws(() => parseMeal('今天吃得不錯'), /找不到資料/);
  assert.throws(() => parseMeal('{"a":1}'), /不是餐點資料/);
  assert.throws(() => parseMeal(`{"format":"${MEAL_FORMAT}","error":"not_food"}`), /不是食物/);
  assert.throws(() => parseMeal(body.replace('750', '"很多"')), /數值有誤/);
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
