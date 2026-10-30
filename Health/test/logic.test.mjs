import assert from 'node:assert/strict';
import { test } from 'node:test';
import { cardDayState, cardHit, DEFAULT_CARDS, metricsOf, monthDates, weekDates } from '../js/cards.js';
import { addDays, dailyTargets, DEFAULT_GOAL, goalFor, summarizeDay, weekStart } from '../js/logic.js';
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
  assert.equal(cardDayState(sleepCard, '2026-10-20', empty, today, start, DEFAULT_GOAL), 'missed');
  assert.equal(cardDayState(sleepCard, '2026-09-20', empty, today, start, DEFAULT_GOAL), 'off');
  assert.equal(cardDayState(sleepCard, '2026-10-31', empty, today, start, DEFAULT_GOAL), 'off');
  assert.equal(cardDayState(activityCard, '2026-10-19', empty, today, start, DEFAULT_GOAL), 'off');
  const plan = { ...DEFAULT_GOAL, workoutPlan: { 1: { activity: '攀岩', minutes: 60 } } };
  assert.equal(cardDayState(activityCard, '2026-10-19', empty, today, start, plan), 'missed'); // 週一
  assert.equal(cardDayState(activityCard, '2026-10-20', empty, today, start, plan), 'off'); // 週二
  assert.equal(cardDayState(activityCard, '2026-10-20', summarizeDay([workout]), today, start, plan), 'done');
  const weekly = { ...bodyCard, days: [6] };
  assert.equal(cardDayState(weekly, '2026-10-20', empty, today, start, DEFAULT_GOAL), 'off');
  assert.equal(cardDayState(weekly, '2026-10-24', empty, today, start, DEFAULT_GOAL), 'missed'); // 週六
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

const profile = { heightCm: 175, birthYear: 1996, sex: 'male' };
const plan = { 1: { activity: '攀岩', minutes: 60 } };

test('每日標準：缺資料用通用值並標明', () => {
  const result = dailyTargets(DEFAULT_GOAL, {}, null, '2026-10-19');
  assert.equal(result.personalized, false);
  assert.equal(result.targets.kcal, 2000);
});

test('每日標準：訓練日高於休息日，依體重計算', () => {
  const goal = { ...DEFAULT_GOAL, workoutPlan: plan };
  const training = dailyTargets(goal, profile, { weightKg: 70 }, '2026-10-19');
  const rest = dailyTargets(goal, profile, { weightKg: 70 }, '2026-10-20');
  assert.equal(training.dayType, 'training');
  assert.equal(rest.dayType, 'rest');
  assert.ok(training.targets.kcal > rest.targets.kcal);
  assert.equal(training.targets.water_ml, 2450);
  assert.equal(training.targets.protein_g, 126);
});

test('每日標準：減重有上限，過於激進會標示', () => {
  const goal = { ...DEFAULT_GOAL, body: { targetWeightKg: 60, targetBodyFat: null, targetDate: '2026-11-01' } };
  const plain = dailyTargets(DEFAULT_GOAL, profile, { weightKg: 70 }, '2026-10-20').targets.kcal;
  const result = dailyTargets(goal, profile, { weightKg: 70 }, '2026-10-20');
  assert.equal(result.capped, true);
  assert.ok(Math.abs(result.targets.kcal - plain * 0.8) <= 10);
});

test('每日標準：覆蓋優先序為單日 > 星期 > 日類型 > 全部', () => {
  const goal = {
    ...DEFAULT_GOAL,
    targetOverrides: { kcal: 1000 },
    dayOverrides: { dayType: { rest: { kcal: 1100 } }, weekday: { 2: { kcal: 1200 } }, date: { '2026-10-20': { kcal: 1300 } } },
  };
  assert.equal(dailyTargets(goal, profile, null, '2026-10-20').targets.kcal, 1300); // 週二
  assert.equal(dailyTargets(goal, profile, null, '2026-10-27').targets.kcal, 1200); // 週二
  assert.equal(dailyTargets(goal, profile, null, '2026-10-21').targets.kcal, 1100); // 週三
  assert.equal(dailyTargets({ ...goal, dayOverrides: {} }, profile, null, '2026-10-21').targets.kcal, 1000);
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
