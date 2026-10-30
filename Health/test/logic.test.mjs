import assert from 'node:assert/strict';
import { test } from 'node:test';
import { addDays, DEFAULT_GOAL, dietStatus, goalFor, summarizeDay, weekStart, workoutStatus } from '../js/logic.js';
import { normalizeEvent, registry } from '../js/registry.js';

const meal = (nutrients = {}) => ({ type: 'meal', schemaVersion: 1, data: { nutrients } });
const workout = { type: 'workout', schemaVersion: 1, data: { activity: '攀岩', durationSec: 3600 } };
const twoMeals = summarizeDay([meal(), meal()]);

test('加總餐點與快速紀錄，略過未知類型', () => {
  const day = summarizeDay([
    meal({ kcal: 600, protein_g: 30 }),
    { type: 'intake', schemaVersion: 1, data: { nutrients: { water_ml: 250, caffeine_mg: 100 } } },
    { type: 'bloodPressure', schemaVersion: 1, data: { systolic: 120 } },
  ]);
  assert.deepEqual(day.nutrients, { kcal: 600, protein_g: 30, water_ml: 250, caffeine_mg: 100 });
});

test('飲食判定：達到最少餐數才算達成，今天未完成不算失敗', () => {
  const one = summarizeDay([meal()]);
  const goal = DEFAULT_GOAL;
  assert.equal(dietStatus(twoMeals, '2026-10-20', '2026-10-30', goal, '2026-10-01'), 'done');
  assert.equal(dietStatus(one, '2026-10-30', '2026-10-30', goal, '2026-10-01'), 'nodata');
  assert.equal(dietStatus(one, '2026-10-20', '2026-10-30', goal, '2026-10-01'), 'missed');
  assert.equal(dietStatus(one, '2026-09-01', '2026-10-30', goal, '2026-10-01'), 'nodata');
  assert.equal(dietStatus(one, '2026-10-20', '2026-10-30', { ...goal, minMeals: 1 }, '2026-10-01'), 'done');
});

test('運動判定：休息日不適用，排定日未做才算未達成', () => {
  const none = summarizeDay([]);
  const goal = DEFAULT_GOAL;
  assert.equal(workoutStatus(none, '2026-10-20', '2026-10-30', goal, '2026-10-01'), 'na'); // 週二
  assert.equal(workoutStatus(none, '2026-10-19', '2026-10-30', goal, '2026-10-01'), 'missed'); // 週一
  assert.equal(workoutStatus(summarizeDay([workout]), '2026-10-20', '2026-10-30', goal, '2026-10-01'), 'done');
});

test('修改目標不影響生效日之前', () => {
  const goals = [{ effectiveFrom: '2026-11-01', targets: { water_ml: 3000 } }];
  assert.equal(goalFor(goals, '2026-10-31').targets.water_ml, 2500);
  assert.equal(goalFor(goals, '2026-11-02').targets.water_ml, 3000);
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
