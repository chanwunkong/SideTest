import {
  addDays, dailyTargets, dateKey, dietStatus, goalFor, parseDateKey, planFor,
  sleepStatus, summarizeDay, targetWeight, weekStart, workoutStatus,
} from './logic.js';
import { GEMINI_PROMPT, parseMeal } from './mealImport.js';
import { ACTIVITIES, INTENSITIES, NUTRIENTS } from './registry.js';
import { healthStore as store } from './store.js';

const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const toastEl = document.getElementById('toast');

const state = {
  tab: 'home',
  dietDate: dateKey(),
  month: dateKey().slice(0, 7),
  draft: null,
};

const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六'];
const WEEK_ORDER = [1, 2, 3, 4, 5, 6, 0];
const STATUS_TEXT = { done: '已達成', missed: '未達成', na: '不適用', nodata: '尚無資料' };
const PRIMARY = NUTRIENTS.filter((n) => n.primary);
const SECONDARY = NUTRIENTS.filter((n) => !n.primary);

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const round = (value) => Math.round((value + Number.EPSILON) * 10) / 10;
const intensityLabel = (key) => INTENSITIES.find((i) => i.key === key)?.label ?? '';
const closeButton = '<button type="button" class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button>';
const hours = (seconds) => round(seconds / 3600);

const sheetHeader = (title) => `<div class="row"><h2 style="margin:0">${title}</h2>${closeButton}</div>`;

function latestBody(date) {
  const records = store.liveEvents().filter((e) => e.type === 'body' && e.date <= date).sort((a, b) => b.occurredAt - a.occurredAt);
  if (!records.length) return null;
  return { weightKg: records[0].data.weightKg, bodyFat: records.find((r) => r.data.bodyFat)?.data.bodyFat ?? null, muscleKg: records[0].data.muscleKg ?? null, date: records[0].date };
}

function dayData(date) {
  const summary = summarizeDay(store.eventsOn(date));
  const today = dateKey();
  const start = store.trackingStart();
  const goal = goalFor(store.goals, date);
  return {
    summary,
    goal,
    plan: planFor(goal, date),
    diet: dietStatus(summary, date, today, goal, start),
    workout: workoutStatus(summary, date, today, goal, start),
    sleep: sleepStatus(summary, date, today, goal, start),
  };
}

const targetsOn = (goal, date) => dailyTargets(goal, store.profile, latestBody(date), date);

function formatDate(key) {
  const date = parseDateKey(key);
  return `${date.getMonth() + 1}月${date.getDate()}日 週${WEEKDAYS[date.getDay()]}`;
}

function mealText(meal) {
  const items = (meal.data.items || []).map((i) => i.name).join('、');
  return items || meal.data.note || '未命名餐點';
}

function workoutText(event) {
  const minutes = Math.round(event.data.durationSec / 60);
  return `${minutes} 分鐘${event.data.intensity ? `・${intensityLabel(event.data.intensity)}` : ''}`;
}

const planText = (plan) => (plan ? `${esc(plan.activity)} ${plan.minutes} 分鐘` : '休息');

// ---- 畫面 ----

function render() {
  const screens = { home: renderHome, diet: renderDiet, workout: renderWorkout, rest: renderRest, body: renderBody, calendar: renderCalendar };
  document.querySelectorAll('.tabs button').forEach((b) => b.classList.toggle('active', b.dataset.tab === state.tab));
  view.innerHTML = screens[state.tab]();
  hydrateImages(view);
}

function hydrateImages(root) {
  root.querySelectorAll('img[data-attachment]').forEach(async (img) => {
    const attachment = await store.getAttachment(img.dataset.attachment);
    if (attachment) img.src = attachment.dataUrl;
  });
}

function renderBars(list, summary, targets) {
  return list.map(({ key, label, unit, isLimit }) => {
    const value = summary.nutrients[key] || 0;
    const target = targets[key] || 0;
    const percent = target ? Math.min(100, (value / target) * 100) : 0;
    const over = isLimit && value > target;
    return `<div class="bar">
      <div class="bar-label"><span>${label}</span><span class="muted">${round(value)} / ${target} ${unit}${isLimit ? ' 上限' : ''}</span></div>
      <div class="bar-track"><div class="bar-fill${over ? ' over' : ''}" style="width:${percent}%"></div></div>
    </div>`;
  }).join('');
}

function focusCard({ label, kind, tab, main, sub = '', actions = '', complete = false }) {
  return `<section class="focus${complete ? ' complete' : ''}">
    <button class="focus-link" data-action="tab" data-tab="${tab}">
      <span class="focus-label ${kind}">${label}</span>
      <div class="focus-main">${main}</div>
      ${sub ? `<div class="muted">${sub}</div>` : ''}
    </button>
    ${actions ? `<div class="focus-actions">${actions}</div>` : ''}
  </section>`;
}

function renderHome() {
  const today = dateKey();
  const { summary, goal, plan, workout } = dayData(today);
  const { targets } = targetsOn(goal, today);
  const { nutrients } = summary;
  const sleepSeconds = summary.sleep.reduce((sum, s) => sum + s.data.durationSec, 0);
  const minutes = summary.workouts.reduce((sum, w) => sum + w.data.durationSec / 60, 0);
  const body = summary.body.at(-1);

  const workoutMain = workout === 'done' ? `已完成 ${Math.round(minutes)} 分鐘` : plan ? `今天排定：${planText(plan)}` : '今天休息';

  return `
    <header class="topbar">
      <div><h1>今日</h1><p class="muted">${formatDate(today)}</p></div>
      <div class="chips">
        <button class="icon-btn" data-action="tab" data-tab="calendar" aria-label="月曆">▦</button>
        <button class="icon-btn" data-action="settings" aria-label="設定">⚙</button>
      </div>
    </header>
    ${focusCard({
      label: '飲食', kind: 'diet', tab: 'diet',
      main: `蛋白質 ${round(nutrients.protein_g || 0)} / ${targets.protein_g} g`,
      sub: `水分 ${round(nutrients.water_ml || 0)} / ${targets.water_ml} ml・${summary.meals.length} 餐`,
      actions: `<button class="btn primary" data-action="meal-paste">記一餐</button>
        <button class="btn" data-action="quick">喝水 +${goal.quick.water_ml} ml</button>`,
    })}
    ${focusCard({
      label: '運動', kind: 'move', tab: 'workout', main: workoutMain, complete: workout === 'done' || !plan,
      actions: plan ? '<button class="btn" data-action="workout-new">完成訓練</button>' : '',
    })}
    ${focusCard({
      label: '休息', kind: 'rest', tab: 'rest',
      main: sleepSeconds ? `昨晚睡 ${hours(sleepSeconds)} 小時（目標 ${goal.sleep.hours}）` : '昨晚睡眠尚未記錄',
      sub: `今晚建議 ${esc(goal.sleep.bedtime)} 前上床`,
      actions: sleepSeconds ? '' : '<button class="btn" data-action="sleep-new">記錄睡眠</button>',
    })}
    ${focusCard({
      label: '身體', kind: 'body', tab: 'body', complete: !!body,
      main: body ? `今天 ${body.data.weightKg} kg` : '今天還沒量體重',
      actions: body ? '' : '<button class="btn" data-action="body-new">記錄</button>',
    })}`;
}

function renderCalendar() {
  const today = dateKey();
  const [year, month] = state.month.split('-').map(Number);
  const first = `${state.month}-01`;
  const lead = (parseDateKey(first).getDay() + 6) % 7;
  const count = new Date(year, month, 0).getDate();
  const cells = [];
  for (let i = 0; i < lead; i++) cells.push('<div></div>');
  for (let d = 1; d <= count; d++) {
    const date = addDays(first, d - 1);
    const { diet, workout, sleep } = dayData(date);
    cells.push(`<button class="day${date === today ? ' today' : ''}" data-action="day" data-date="${date}" aria-label="${date} 飲食${STATUS_TEXT[diet]}、運動${STATUS_TEXT[workout]}、休息${STATUS_TEXT[sleep]}">
      ${d}<div class="dots"><i class="dot diet ${diet}"></i><i class="dot move ${workout}"></i><i class="dot rest ${sleep}"></i></div>
    </button>`);
  }
  const dows = ['一', '二', '三', '四', '五', '六', '日'].map((d) => `<div class="dow">${d}</div>`).join('');
  return `
    <header class="topbar">
      <button class="icon-btn" data-action="month" data-delta="-1" aria-label="上個月">‹</button>
      <h1>${year} 年 ${month} 月</h1>
      <button class="icon-btn" data-action="month" data-delta="1" aria-label="下個月">›</button>
    </header>
    <section class="card">
      <div class="calendar">${dows}${cells.join('')}</div>
      <div class="legend">
        <span><i class="dot diet done"></i>飲食</span>
        <span><i class="dot move done"></i>運動</span>
        <span><i class="dot rest done"></i>休息</span>
        <span><i class="dot missed"></i>未達成</span>
      </div>
    </section>`;
}

function mealEntry(meal) {
  const { kcal, protein_g: protein } = meal.data.nutrients ?? {};
  const image = meal.attachmentIds[0] ? `<img data-attachment="${esc(meal.attachmentIds[0])}" alt="餐點照片">` : '';
  return `<button class="entry" data-action="meal-open" data-id="${esc(meal.id)}">
    ${image}
    <span class="entry-body">
      <strong>${esc(mealText(meal))}</strong>
      <span class="muted">${kcal ? `${Math.round(kcal)} kcal・` : ''}蛋白質 ${round(protein || 0)} g</span>
    </span>
  </button>`;
}

function renderDiet() {
  const date = state.dietDate;
  const { summary, goal } = dayData(date);
  const { targets, personalized } = targetsOn(goal, date);

  return `
    <header class="topbar">
      <button class="icon-btn" data-action="diet-date" data-delta="-1" aria-label="前一天">‹</button>
      <div style="text-align:center"><h1>飲食</h1><p class="muted">${formatDate(date)}</p></div>
      <button class="icon-btn" data-action="diet-date" data-delta="1" aria-label="後一天"${date >= dateKey() ? ' disabled' : ''}>›</button>
    </header>
    <section class="grid-2 meal-actions">
      <button class="btn primary" data-action="meal-paste">貼上 Gemini 結果</button>
      <button class="btn" data-action="meal-manual">手動輸入</button>
    </section>
    ${summary.meals.length ? `<section class="card">${summary.meals.map(mealEntry).join('')}</section>` : ''}
    <section class="card">${renderBars(PRIMARY, summary, targets)}
      ${personalized ? '' : '<p class="muted" style="margin-top:12px">目前是通用標準。到身體頁填資料後，會改成依你計算。</p>'}
    </section>
    <details class="card">
      <summary>熱量、咖啡因、肌酸</summary>
      ${renderBars(SECONDARY, summary, targets)}
    </details>
    <button class="textlink" data-action="prompt-show">查看 Gemini 提示詞</button>`;
}

function renderWorkout() {
  const today = dateKey();
  const start = weekStart(today);
  const dates = Array.from({ length: 7 }, (_, i) => addDays(start, i));
  const week = dates.flatMap((d) => summarizeDay(store.eventsOn(d)).workouts);
  const goal = goalFor(store.goals, today);
  const plan = planFor(goal, today);
  const planned = Object.keys(goal.workoutPlan).length;
  const minutes = week.reduce((sum, w) => sum + w.data.durationSec / 60, 0);
  const activeDays = new Set(week.map((w) => w.date)).size;
  const recent = store.liveEvents()
    .filter((e) => e.type === 'workout')
    .sort((a, b) => b.occurredAt - a.occurredAt)
    .slice(0, 10);

  return `
    <header class="topbar"><h1>運動</h1></header>
    <section class="focus">
      <div class="focus-main">今天：${planText(plan)}</div>
      <div class="focus-actions"><button class="btn primary" data-action="workout-new">記錄運動</button></div>
    </section>
    <section class="card">
      <h2>本週</h2>
      <div class="stat-row"><span>訓練天數</span><strong>${activeDays}${planned ? ` / ${planned}` : ''} 天</strong></div>
      <div class="stat-row"><span>總時間</span><strong>${Math.round(minutes)} 分鐘</strong></div>
    </section>
    <section class="card">
      <h2>週課表</h2>
      ${WEEK_ORDER.map((d) => `<button class="plan-row" data-action="plan-edit" data-day="${d}"><span>週${WEEKDAYS[d]}</span><span class="muted">${planText(goal.workoutPlan[d])}</span></button>`).join('')}
    </section>
    <section class="card">
      <h2>最近紀錄</h2>
      ${recent.map((w) => `<button class="entry" data-action="workout-open" data-id="${esc(w.id)}">
        <span class="entry-body"><strong>${esc(w.data.activity)}</strong><span class="muted">${formatDate(w.date)}・${workoutText(w)}</span></span>
      </button>`).join('') || '<p class="muted">還沒有運動紀錄</p>'}
    </section>`;
}

function renderRest() {
  const today = dateKey();
  const goal = goalFor(store.goals, today);
  const days = Array.from({ length: 7 }, (_, i) => addDays(today, -i));
  const rows = days.flatMap((d) => summarizeDay(store.eventsOn(d)).sleep);
  const average = rows.length ? rows.reduce((sum, s) => sum + s.data.durationSec, 0) / rows.length : 0;

  return `
    <header class="topbar"><h1>休息</h1></header>
    <section class="focus">
      <div class="focus-main">今晚建議 ${esc(goal.sleep.bedtime)} 前上床</div>
      <div class="muted">睡眠目標 ${goal.sleep.hours} 小時</div>
      <div class="focus-actions">
        <button class="btn primary" data-action="sleep-new">記錄睡眠</button>
        <button class="btn" data-action="sleep-goal">調整目標</button>
      </div>
    </section>
    <section class="card">
      <h2>最近 7 天</h2>
      ${rows.length ? `<div class="stat-row"><span>平均</span><strong>${hours(average)} 小時</strong></div>` : ''}
      ${rows.map((s) => `<button class="entry" data-action="sleep-open" data-id="${esc(s.id)}">
        <span class="entry-body"><strong>${hours(s.data.durationSec)} 小時</strong><span class="muted">${formatDate(s.date)} 起床</span></span>
      </button>`).join('') || '<p class="muted">還沒有睡眠紀錄</p>'}
    </section>`;
}

function renderBody() {
  const today = dateKey();
  const goal = goalFor(store.goals, today);
  const body = latestBody(today);
  const { profile } = store;
  const height = profile.heightCm / 100;
  const bmi = body && height ? round(body.weightKg / (height * height)) : null;
  const goalWeight = body ? targetWeight(goal, body.weightKg, body.bodyFat) : goal.body.targetWeightKg;
  const result = targetsOn(goal, today);
  const records = store.liveEvents().filter((e) => e.type === 'body').sort((a, b) => b.occurredAt - a.occurredAt).slice(0, 7);
  const hasGoal = goal.body.targetDate && (goal.body.targetWeightKg || goal.body.targetBodyFat);

  return `
    <header class="topbar"><h1>身體</h1></header>
    <section class="focus">
      <div class="focus-main">${body ? `${body.weightKg} kg${body.bodyFat ? `・體脂 ${body.bodyFat}%` : ''}` : '還沒有身體數據'}</div>
      ${bmi ? `<div class="muted">BMI ${bmi}・${formatDate(body.date)}</div>` : ''}
      <div class="focus-actions">
        <button class="btn primary" data-action="body-new">記錄數據</button>
        <button class="btn" data-action="profile-edit">基本資料</button>
      </div>
    </section>
    <section class="card">
      <div class="row"><h2 style="margin:0">目標</h2><button class="btn" data-action="goal-edit">${hasGoal ? '修改' : '設定'}</button></div>
      ${hasGoal ? `<p style="margin-top:8px">${goalWeight ? `${round(goalWeight)} kg` : `體脂 ${goal.body.targetBodyFat}%`}・${formatDate(goal.body.targetDate)} 前</p>` : '<p class="muted" style="margin-top:8px">還沒有設定目標</p>'}
      ${result.capped ? '<p class="muted" style="margin-top:8px">這個時程太趕，每日標準已限制在安全範圍內，實際可能需要更長時間。</p>' : ''}
    </section>
    <section class="card">
      <div class="row"><h2 style="margin:0">今日標準</h2><button class="btn" data-action="targets-edit">手動調整</button></div>
      <p class="muted" style="margin-top:8px">${result.personalized ? `依你的數據估算・${result.dayType === 'training' ? '訓練日' : '休息日'}` : '通用標準：填體重，以及體脂或基本資料後會改成個人估算'}</p>
      ${NUTRIENTS.map(({ key, label, unit }) => `<div class="stat-row"><span>${label}</span><span>${result.targets[key]} ${unit}</span></div>`).join('')}
    </section>
    <section class="card">
      <h2>最近紀錄</h2>
      ${records.map((r) => `<button class="entry" data-action="body-open" data-id="${esc(r.id)}">
        <span class="entry-body"><strong>${r.data.weightKg} kg${r.data.bodyFat ? `・體脂 ${r.data.bodyFat}%` : ''}</strong><span class="muted">${formatDate(r.date)}</span></span>
      </button>`).join('') || '<p class="muted">還沒有紀錄</p>'}
    </section>`;
}

// ---- 底部表單 ----

function openSheet(html) {
  sheet.innerHTML = html;
  hydrateImages(sheet);
  if (!sheet.open) sheet.showModal();
}

function closeSheet() {
  sheet.close();
  state.draft = null;
}

const numberField = (name, label, value, attrs = '') => `<label class="field"><span>${label}</span>
  <input ${attrs} type="number" inputmode="decimal" min="0" step="any" name="${name}" value="${value ?? ''}"></label>`;
const dateField = (value) => `<label class="field"><span>日期</span><input type="date" name="date" max="${dateKey()}" value="${value}" required></label>`;
const deleteButton = '<button type="button" class="btn danger wide" data-action="event-delete">刪除</button>';
const optionalNumber = (value) => (value === '' || value === null ? null : Number(value));

function openPrompt() {
  openSheet(`<div class="stack">
    ${sheetHeader('Gemini 提示詞')}
    <p class="muted">建議存成 Gemini 的 Gem，之後只要拍照；不能用 Gem 就每次先貼這段。</p>
    <button class="btn primary wide" data-action="prompt-copy">複製提示詞</button>
    <label class="field"><span>提示詞內容</span><textarea readonly rows="10">${esc(GEMINI_PROMPT)}</textarea></label>
  </div>`);
}

function openPaste() {
  openSheet(`<form class="stack" data-form="paste">
    ${sheetHeader('貼上 Gemini 結果')}
    <button type="button" class="btn primary wide" data-action="paste-clipboard">從剪貼簿貼上</button>
    <label class="field"><span>或貼在這裡</span><textarea name="text"></textarea></label>
    <p class="muted" id="paste-error"></p>
    <button class="btn wide">確認</button>
    <button type="button" class="textlink" data-action="prompt-show">查看 Gemini 提示詞</button>
  </form>`);
}

function importMeal(text) {
  try {
    const meal = parseMeal(text);
    state.draft = { ...newMealDraft(), ...meal, source: 'gemini_paste' };
    renderMealSheet();
  } catch (error) {
    sheet.querySelector('#paste-error').textContent = error.message;
  }
}

function newMealDraft() {
  return { id: null, date: state.tab === 'diet' ? state.dietDate : dateKey(), nutrients: {}, items: [], note: '', confidence: null, source: 'manual' };
}

function renderMealSheet() {
  const { draft } = state;
  const fields = NUTRIENTS.map(({ key, label, unit }) => numberField(key, `${label}（${unit}）`, draft.nutrients[key])).join('');
  const items = draft.items.map((i) => `${esc(i.name)}（${esc(i.portion)}）`).join('、');
  const confidence = { low: '低', medium: '中', high: '高' }[draft.confidence];

  openSheet(`<form class="stack" data-form="meal">
    ${sheetHeader(draft.id ? '修改餐點' : '確認餐點')}
    ${items ? `<p class="muted">${items}${confidence ? `｜可信度：${confidence}` : ''}</p>` : ''}
    <label class="field"><span>名稱或備註</span><input name="note" value="${esc(draft.note)}"></label>
    <div class="grid-2">${fields}</div>
    <button class="btn primary wide">儲存</button>
    ${draft.id ? deleteButton : ''}
  </form>`);
}

function saveMeal(form) {
  const { draft } = state;
  const values = new FormData(form);
  const nutrients = {};
  for (const { key } of NUTRIENTS) {
    const value = optionalNumber(values.get(key));
    if (value !== null) nutrients[key] = value;
  }
  const data = { nutrients, items: draft.items, note: values.get('note') };
  const raw = draft.confidence ? { confidence: draft.confidence } : null;
  if (draft.id) store.updateEvent(draft.id, data);
  else store.addEvent('meal', data, { date: draft.date, source: draft.source, raw });
  closeSheet();
  toast('已記錄這一餐');
}

function openMeal(id) {
  const event = store.events.get(id);
  state.draft = {
    ...newMealDraft(), id, date: event.date,
    nutrients: { ...event.data.nutrients }, items: event.data.items ?? [], note: event.data.note ?? '',
    confidence: event.raw?.confidence ?? null,
  };
  renderMealSheet();
}

const chipGroup = (name, options, active) => `<div class="chips" data-chip-group="${name}">${options.map((o) => `<button type="button" class="chip${o.value === active ? ' active' : ''}" data-action="chip" data-value="${esc(o.value)}">${esc(o.label)}</button>`).join('')}</div>`;
const activityOptions = ACTIVITIES.map((a) => ({ value: a, label: a }));

function openWorkout(id = null, date = dateKey()) {
  const event = id ? store.events.get(id) : null;
  const plan = planFor(goalFor(store.goals, date), date);
  const activity = event?.data.activity ?? plan?.activity ?? ACTIVITIES[0];
  const intensity = event?.data.intensity ?? 'moderate';
  state.draft = { id, date: event?.date ?? date, activity, intensity };
  const minutes = event ? Math.round(event.data.durationSec / 60) : plan?.minutes ?? 60;
  openSheet(`<form class="stack" data-form="workout">
    ${sheetHeader(id ? '修改運動' : '記錄運動')}
    ${chipGroup('activity', activityOptions, activity)}
    ${numberField('minutes', '時間（分鐘）', minutes, 'min="1" required')}
    ${chipGroup('intensity', INTENSITIES.map((i) => ({ value: i.key, label: i.label })), intensity)}
    ${dateField(state.draft.date)}
    <label class="field"><span>備註</span><input name="note" value="${esc(event?.data.note)}"></label>
    <button class="btn primary wide">儲存</button>
    ${id ? deleteButton : ''}
  </form>`);
}

function saveWorkout(form) {
  const { draft } = state;
  const values = new FormData(form);
  const date = values.get('date');
  const data = { activity: draft.activity, intensity: draft.intensity, durationSec: Number(values.get('minutes')) * 60, note: values.get('note') };
  if (draft.id) store.updateEvent(draft.id, data, { date, occurredAt: new Date(`${date}T12:00:00`).getTime() });
  else store.addEvent('workout', data, { date });
  closeSheet();
  toast('已記錄運動');
}

function openPlan(day) {
  const goal = goalFor(store.goals, dateKey());
  const plan = goal.workoutPlan[day];
  const activity = plan?.activity ?? '';
  state.draft = { day, activity };
  openSheet(`<form class="stack" data-form="plan">
    ${sheetHeader(`週${WEEKDAYS[day]}的訓練`)}
    ${chipGroup('activity', [{ value: '', label: '休息' }, ...activityOptions], activity)}
    ${numberField('minutes', '時間（分鐘）', plan?.minutes ?? 60)}
    <button class="btn primary wide">儲存</button>
  </form>`);
}

function savePlan(form) {
  const { day, activity } = state.draft;
  const workoutPlan = { ...goalFor(store.goals, dateKey()).workoutPlan };
  if (activity) workoutPlan[day] = { activity, minutes: Number(new FormData(form).get('minutes')) || 60 };
  else delete workoutPlan[day];
  store.saveGoal({ workoutPlan });
  closeSheet();
  toast('週課表已更新');
}

function openSleep(id = null) {
  const event = id ? store.events.get(id) : null;
  state.draft = { id };
  openSheet(`<form class="stack" data-form="sleep">
    ${sheetHeader('記錄睡眠')}
    ${numberField('hours', '睡了幾小時', event ? hours(event.data.durationSec) : '', 'min="0.5" required')}
    ${dateField(event?.date ?? dateKey())}
    <button class="btn primary wide">儲存</button>
    ${id ? deleteButton : ''}
  </form>`);
}

function saveSleep(form) {
  const { draft } = state;
  const values = new FormData(form);
  const date = values.get('date');
  const data = { durationSec: Math.round(Number(values.get('hours')) * 3600) };
  if (draft.id) store.updateEvent(draft.id, data, { date, occurredAt: new Date(`${date}T12:00:00`).getTime() });
  else store.addEvent('sleep', data, { date });
  closeSheet();
  toast('已記錄睡眠');
}

function openSleepGoal() {
  const { sleep } = goalFor(store.goals, dateKey());
  openSheet(`<form class="stack" data-form="sleep-goal">
    ${sheetHeader('睡眠目標')}
    ${numberField('hours', '每晚目標（小時）', sleep.hours, 'min="1" required')}
    <label class="field"><span>建議上床時間</span><input type="time" name="bedtime" value="${esc(sleep.bedtime)}" required></label>
    <button class="btn primary wide">儲存</button>
  </form>`);
}

function openBody(id = null) {
  const event = id ? store.events.get(id) : null;
  const last = latestBody(dateKey());
  state.draft = { id };
  openSheet(`<form class="stack" data-form="body">
    ${sheetHeader('記錄身體數據')}
    ${numberField('weightKg', '體重（kg）', event?.data.weightKg ?? last?.weightKg, 'required')}
    <div class="grid-2">
      ${numberField('bodyFat', '體脂（%）', event?.data.bodyFat)}
      ${numberField('muscleKg', '肌肉量（kg）', event?.data.muscleKg)}
    </div>
    ${dateField(event?.date ?? dateKey())}
    <button class="btn primary wide">儲存</button>
    ${id ? deleteButton : ''}
  </form>`);
}

function saveBody(form) {
  const { draft } = state;
  const values = new FormData(form);
  const date = values.get('date');
  const data = { weightKg: Number(values.get('weightKg')), bodyFat: optionalNumber(values.get('bodyFat')), muscleKg: optionalNumber(values.get('muscleKg')) };
  if (draft.id) store.updateEvent(draft.id, data, { date, occurredAt: new Date(`${date}T12:00:00`).getTime() });
  else store.addEvent('body', data, { date });
  closeSheet();
  toast('已記錄身體數據');
}

function openProfile() {
  const { profile } = store;
  const sex = profile.sex ?? '';
  state.draft = { sex };
  openSheet(`<form class="stack" data-form="profile">
    ${sheetHeader('基本資料')}
    ${numberField('heightCm', '身高（cm）', profile.heightCm)}
    ${numberField('birthYear', '出生年（西元）', profile.birthYear)}
    ${chipGroup('sex', [{ value: 'male', label: '男' }, { value: 'female', label: '女' }], sex)}
    <p class="muted">只用來估算基礎代謝。有體脂紀錄時改用體脂計算。</p>
    <button class="btn primary wide">儲存</button>
  </form>`);
}

function openGoal() {
  const { body } = goalFor(store.goals, dateKey());
  openSheet(`<form class="stack" data-form="goal">
    ${sheetHeader('目標')}
    <p class="muted">目標體重和目標體脂填一個就好，兩個都填時以體重為準。</p>
    <div class="grid-2">
      ${numberField('targetWeightKg', '目標體重（kg）', body.targetWeightKg)}
      ${numberField('targetBodyFat', '目標體脂（%）', body.targetBodyFat)}
    </div>
    <label class="field"><span>目標日期</span><input type="date" name="targetDate" min="${addDays(dateKey(), 1)}" value="${esc(body.targetDate)}"></label>
    <button class="btn primary wide">儲存</button>
  </form>`);
}

function openTargets() {
  const goal = goalFor(store.goals, dateKey());
  const computed = dailyTargets({ ...goal, targetOverrides: {} }, store.profile, latestBody(dateKey()), dateKey()).targets;
  openSheet(`<form class="stack" data-form="targets">
    ${sheetHeader('手動調整標準')}
    <p class="muted">留空就使用自動估算；填了就固定用你的數字。</p>
    <div class="grid-2">${NUTRIENTS.map(({ key, label, unit }) => numberField(key, `${label}（${unit}）`, goal.targetOverrides[key], `placeholder="${computed[key]}"`)).join('')}</div>
    <button class="btn primary wide">儲存</button>
  </form>`);
}

function openDay(date) {
  const { summary } = dayData(date);
  const entries = [
    ...summary.meals.map(mealEntry),
    ...summary.workouts.map((w) => `<button class="entry" data-action="workout-open" data-id="${esc(w.id)}"><span class="entry-body"><strong>${esc(w.data.activity)}</strong><span class="muted">${workoutText(w)}</span></span></button>`),
    ...summary.sleep.map((s) => `<button class="entry" data-action="sleep-open" data-id="${esc(s.id)}"><span class="entry-body"><strong>睡眠 ${hours(s.data.durationSec)} 小時</strong></span></button>`),
  ].join('');
  const nutrition = PRIMARY.filter(({ key }) => summary.nutrients[key]).map(({ key, label, unit }) => `${label} ${round(summary.nutrients[key])} ${unit}`).join('・');
  openSheet(`<div class="stack">
    ${sheetHeader(formatDate(date))}
    ${entries || '<p class="muted">這天沒有紀錄</p>'}
    ${nutrition ? `<p class="muted">${esc(nutrition)}</p>` : ''}
    ${date <= dateKey() ? `<button class="btn wide" data-action="diet-goto" data-date="${date}">補記飲食</button>
    <button class="btn wide" data-action="workout-new" data-date="${date}">補記運動</button>` : ''}
  </div>`);
}

function openSettings() {
  const goal = goalFor(store.goals, dateKey());
  const account = store.user
    ? `<p class="muted">已登入：${esc(store.user.email)}</p><button type="button" class="btn wide" data-action="sign-out">登出</button>`
    : `<p class="muted">未登入，資料只存在這台裝置。登入後會自動上傳並跨裝置同步。</p>
       <button type="button" class="btn wide" data-action="sign-in"${store.canSignIn ? '' : ' disabled'}>使用 Google 登入</button>`;

  openSheet(`<form class="stack" data-form="settings">
    ${sheetHeader('設定')}
    <section class="card stack"><h2>帳號</h2>${account}</section>
    <section class="card stack"><h2>飲食</h2>
      ${numberField('minMeals', '每天至少記幾餐算達成', goal.minMeals, 'min="1"')}
      ${numberField('quick_water_ml', '喝水一次（ml）', goal.quick.water_ml)}
    </section>
    <button class="btn primary wide">儲存設定</button>
    <button type="button" class="btn wide" data-action="export">匯出全部資料（JSON）</button>
  </form>`);
}

function saveGoalForm(form, kind) {
  const values = new FormData(form);
  const goal = goalFor(store.goals, dateKey());
  if (kind === 'goal') {
    store.saveGoal({ body: {
      targetWeightKg: optionalNumber(values.get('targetWeightKg')),
      targetBodyFat: optionalNumber(values.get('targetBodyFat')),
      targetDate: values.get('targetDate') || null,
    } });
  } else if (kind === 'targets') {
    const targetOverrides = {};
    for (const { key } of NUTRIENTS) {
      const value = optionalNumber(values.get(key));
      if (value !== null) targetOverrides[key] = value;
    }
    store.saveGoal({ targetOverrides });
  } else if (kind === 'sleep-goal') {
    store.saveGoal({ sleep: { hours: Number(values.get('hours')), bedtime: values.get('bedtime') } });
  } else {
    store.saveGoal({
      minMeals: Number(values.get('minMeals')) || 1,
      quick: { ...goal.quick, water_ml: Number(values.get('quick_water_ml')) || goal.quick.water_ml },
    });
  }
  closeSheet();
  toast('已儲存');
}

function saveProfile(form) {
  const values = new FormData(form);
  store.saveProfile({ heightCm: optionalNumber(values.get('heightCm')), birthYear: optionalNumber(values.get('birthYear')), sex: state.draft.sex || null });
  closeSheet();
  toast('已儲存');
}

async function exportData() {
  const data = await store.exportAll();
  const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = `health-${dateKey()}.json`;
  link.click();
  URL.revokeObjectURL(url);
}

// ---- 互動 ----

let toastTimer;
function toast(message, undo = null) {
  toastEl.hidden = false;
  toastEl.textContent = message;
  if (undo) {
    const button = document.createElement('button');
    button.textContent = '復原';
    button.onclick = () => { undo(); toastEl.hidden = true; };
    toastEl.append(button);
  }
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toastEl.hidden = true; }, 4000);
}

function logWater() {
  const { quick } = goalFor(store.goals, dateKey());
  const event = store.addEvent('intake', { label: '喝水', nutrients: { water_ml: quick.water_ml } });
  toast(`已記錄喝水 ${quick.water_ml} ml`, () => store.deleteEvent(event.id));
}

const actions = {
  tab: ({ tab }) => { state.tab = tab; render(); scrollTo(0, 0); },
  settings: openSettings,
  'sheet-close': closeSheet,
  quick: logWater,
  month: ({ delta }) => {
    const date = parseDateKey(`${state.month}-01`);
    date.setMonth(date.getMonth() + Number(delta));
    state.month = dateKey(date).slice(0, 7);
    render();
  },
  day: ({ date }) => openDay(date),
  'diet-date': ({ delta }) => {
    const next = addDays(state.dietDate, Number(delta));
    if (next <= dateKey()) { state.dietDate = next; render(); }
  },
  'diet-goto': ({ date }) => { closeSheet(); state.tab = 'diet'; state.dietDate = date; render(); scrollTo(0, 0); },
  'meal-paste': openPaste,
  'meal-manual': () => { state.draft = newMealDraft(); renderMealSheet(); },
  'meal-open': ({ id }) => openMeal(id),
  'prompt-show': openPrompt,
  'prompt-copy': () => navigator.clipboard.writeText(GEMINI_PROMPT).then(() => toast('已複製提示詞')).catch(() => toast('無法複製，請長按文字手動複製')),
  'paste-clipboard': () => navigator.clipboard.readText().then(importMeal).catch(() => {
    sheet.querySelector('#paste-error').textContent = '無法讀取剪貼簿，請直接貼到下方';
  }),
  'event-delete': () => {
    const { id } = state.draft;
    store.deleteEvent(id);
    closeSheet();
    toast('已刪除', () => store.restoreEvent(id));
  },
  'workout-new': ({ date }) => openWorkout(null, date),
  'workout-open': ({ id }) => openWorkout(id),
  'plan-edit': ({ day }) => openPlan(Number(day)),
  'sleep-new': () => openSleep(),
  'sleep-open': ({ id }) => openSleep(id),
  'sleep-goal': openSleepGoal,
  'body-new': () => openBody(),
  'body-open': ({ id }) => openBody(id),
  'profile-edit': openProfile,
  'goal-edit': openGoal,
  'targets-edit': openTargets,
  chip: ({ value }, target) => {
    const group = target.closest('[data-chip-group]');
    group.querySelectorAll('.chip').forEach((c) => c.classList.toggle('active', c === target));
    state.draft[group.dataset.chipGroup] = value;
  },
  'sign-in': () => store.signIn().then(closeSheet).catch((error) => toast(`登入失敗：${error.message}`)),
  'sign-out': () => store.signOut().then(closeSheet),
  export: () => exportData().catch((error) => toast(`匯出失敗：${error.message}`)),
};

document.addEventListener('click', (event) => {
  const target = event.target.closest('[data-action]');
  if (target) actions[target.dataset.action](target.dataset, target);
});

const formHandlers = {
  paste: (form) => importMeal(new FormData(form).get('text')),
  meal: saveMeal,
  workout: saveWorkout,
  plan: savePlan,
  sleep: saveSleep,
  body: saveBody,
  profile: saveProfile,
  goal: (form) => saveGoalForm(form, 'goal'),
  targets: (form) => saveGoalForm(form, 'targets'),
  'sleep-goal': (form) => saveGoalForm(form, 'sleep-goal'),
  settings: (form) => saveGoalForm(form, 'settings'),
};

sheet.addEventListener('submit', (event) => {
  event.preventDefault();
  formHandlers[event.target.dataset.form](event.target);
});

// close 事件是非同步的，此時可能已經開了下一張表單
sheet.addEventListener('close', () => { if (!sheet.open) state.draft = null; });

store.addEventListener('change', render);
store.addEventListener('error', (event) => toast(`資料儲存失敗：${event.detail.message}`));

// 回到 App 時同步其他裝置的新資料
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && !sheet.open) store.reload();
});

render();
store.init().catch((error) => toast(`啟動失敗：${error.message}`));

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
