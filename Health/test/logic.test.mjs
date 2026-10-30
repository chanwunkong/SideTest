import assert from 'node:assert/strict';
import { test } from 'node:test';
import { addDays, DEFAULT_GOAL, defaultSlot, dietStatus, goalFor, summarizeDay, weekStart, workoutStatus } from '../js/logic.js';
import { normalizeEvent, registry } from '../js/registry.js';

const meal = (slot, nutrients = {}, extra = {}) => ({ type: 'meal', schemaVersion: 1, data: { mealSlot: slot, nutrients, ...extra } });
const workout = { type: 'workout', schemaVersion: 1, data: { activity: '攀岩', durationSec: 3600 } };
const threeMeals = summarizeDay([meal('breakfast'), meal('lunch'), meal('dinner', {}, { skipped: true })]);

test('加總餐點與快速紀錄，略過標記沒吃的餐與未知類型', () => {
  const day = summarizeDay([
    meal('lunch', { kcal: 600, protein_g: 30 }),
    meal('dinner', { kcal: 999 }, { skipped: true }),
    { type: 'intake', schemaVersion: 1, data: { nutrients: { water_ml: 250, caffeine_mg: 100 } } },
    { type: 'bloodPressure', schemaVersion: 1, data: { systolic: 120 } },
  ]);
  assert.deepEqual(day.nutrients, { kcal: 600, protein_g: 30, water_ml: 250, caffeine_mg: 100 });
});

test('飲食判定：三餐都處理才達成，今天未完成不算失敗', () => {
  const partial = summarizeDay([meal('breakfast')]);
  assert.equal(dietStatus(threeMeals, '2026-10-20', '2026-10-30', '2026-10-01'), 'done');
  assert.equal(dietStatus(partial, '2026-10-30', '2026-10-30', '2026-10-01'), 'nodata');
  assert.equal(dietStatus(partial, '2026-10-20', '2026-10-30', '2026-10-01'), 'missed');
  assert.equal(dietStatus(partial, '2026-09-01', '2026-10-30', '2026-10-01'), 'nodata');
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
  assert.equal(defaultSlot(new Date(2026, 9, 30, 12, 0)), 'lunch');
});
