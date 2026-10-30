import { analyzeMeal, getApiKey, setApiKey } from './ai.js';
import { compressImage } from './image.js';
import { addDays, dateKey, dietStatus, goalFor, parseDateKey, summarizeDay, weekStart, workoutStatus } from './logic.js';
import { ACTIVITIES, INTENSITIES, NUTRIENTS } from './registry.js';
import { healthStore as store } from './store.js';

const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const toastEl = document.getElementById('toast');
const photoInput = document.getElementById('photo-input');

const state = {
  tab: 'home',
  dietDate: dateKey(),
  month: dateKey().slice(0, 7),
  draft: null,
};

const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六'];
const STATUS_TEXT = { done: '已達成', missed: '未達成', na: '休息日', nodata: '尚無資料' };

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const round = (value) => Math.round((value + Number.EPSILON) * 10) / 10;
const PRIMARY = NUTRIENTS.filter((n) => n.primary);
const SECONDARY = NUTRIENTS.filter((n) => !n.primary);
const intensityLabel = (key) => INTENSITIES.find((i) => i.key === key)?.label ?? '';

function dayData(date) {
  const summary = summarizeDay(store.eventsOn(date));
  const today = dateKey();
  const start = store.trackingStart();
  const goal = goalFor(store.goals, date);
  return {
    summary,
    goal,
    diet: dietStatus(summary, date, today, goal, start),
    workout: workoutStatus(summary, date, today, goal, start),
  };
}

function formatDate(key) {
  const date = parseDateKey(key);
  return `${date.getMonth() + 1}月${date.getDate()}日 週${WEEKDAYS[date.getDay()]}`;
}

function mealText(meal) {
  const items = (meal.data.items || []).map((i) => i.name).join('、');
  return items || meal.data.note || '尚未估算';
}

function workoutText(event) {
  const minutes = Math.round(event.data.durationSec / 60);
  return `${minutes} 分鐘${event.data.intensity ? `・${intensityLabel(event.data.intensity)}` : ''}`;
}

// ---- 畫面 ----

function render() {
  document.querySelectorAll('.tabs button').forEach((b) => b.classList.toggle('active', b.dataset.tab === state.tab));
  view.innerHTML = { home: renderHome, diet: renderDiet, workout: renderWorkout, calendar: renderCalendar }[state.tab]();
  hydrateImages(view);
}

function hydrateImages(root) {
  root.querySelectorAll('img[data-attachment]').forEach(async (img) => {
    const attachment = await store.getAttachment(img.dataset.attachment);
    if (attachment) img.src = attachment.dataUrl;
  });
}

function renderBars(list, summary, goal) {
  return list.map(({ key, label, unit, isLimit }) => {
    const value = summary.nutrients[key] || 0;
    const target = goal.targets[key] || 0;
    const percent = target ? Math.min(100, (value / target) * 100) : 0;
    const over = isLimit && value > target;
    return `<div class="bar">
      <div class="bar-label"><span>${label}</span><span class="muted">${round(value)} / ${target} ${unit}${isLimit ? ' 上限' : ''}</span></div>
      <div class="bar-track"><div class="bar-fill${over ? ' over' : ''}" style="width:${percent}%"></div></div>
    </div>`;
  }).join('');
}

function renderHome() {
  const today = dateKey();
  const { summary, goal } = dayData(today);
  const { nutrients } = summary;
  const minutes = summary.workouts.reduce((sum, w) => sum + w.data.durationSec / 60, 0);

  return `
    <header class="topbar">
      <div><h1>今日</h1><p class="muted">${formatDate(today)}</p></div>
      <button class="icon-btn" data-action="settings" aria-label="設定">⚙</button>
    </header>
    <section class="quick">
      <button class="btn full" data-action="photo">📷 拍照記餐</button>
      <button class="btn wide-water" data-action="quick" data-kind="water">喝水 +${goal.quick.water_ml} ml</button>
    </section>
    <section class="card">${renderBars(PRIMARY, summary, goal)}</section>
    <section class="minor">
      <button data-action="quick" data-kind="coffee">咖啡因 ${round(nutrients.caffeine_mg || 0)} mg ＋</button>
      <button data-action="quick" data-kind="creatine">肌酸 ${round(nutrients.creatine_g || 0)} g ＋</button>
      <button data-action="workout-new">運動 ${Math.round(minutes)} 分 ＋</button>
    </section>`;
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
    const { diet, workout } = dayData(date);
    cells.push(`<button class="day${date === today ? ' today' : ''}" data-action="day" data-date="${date}" aria-label="${date} 飲食${STATUS_TEXT[diet]}、運動${STATUS_TEXT[workout]}">
      ${d}<div class="dots"><i class="dot diet ${diet}"></i><i class="dot move ${workout}"></i></div>
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
        <span><i class="dot missed"></i>未達成</span>
      </div>
    </section>`;
}

function mealEntry(meal) {
  const kcal = meal.data.nutrients?.kcal;
  const image = meal.attachmentIds[0]
    ? `<img data-attachment="${esc(meal.attachmentIds[0])}" alt="餐點照片">`
    : '<div class="thumb"></div>';
  return `<button class="entry" data-action="meal-open" data-id="${esc(meal.id)}">
    ${image}
    <span class="entry-body">
      <strong>${esc(mealText(meal))}</strong>
      <span class="muted">${kcal ? `${Math.round(kcal)} kcal・蛋白質 ${round(meal.data.nutrients.protein_g || 0)} g` : '點一下修改'}</span>
    </span>
  </button>`;
}

function renderDiet() {
  const date = state.dietDate;
  const { summary, goal } = dayData(date);

  return `
    <header class="topbar">
      <button class="icon-btn" data-action="diet-date" data-delta="-1" aria-label="前一天">‹</button>
      <div style="text-align:center"><h1>飲食</h1><p class="muted">${formatDate(date)}</p></div>
      <button class="icon-btn" data-action="diet-date" data-delta="1" aria-label="後一天"${date >= dateKey() ? ' disabled' : ''}>›</button>
    </header>
    <section class="grid-2 meal-actions">
      <button class="btn primary" data-action="photo">📷 拍照</button>
      <button class="btn" data-action="meal-manual">手動輸入</button>
    </section>
    ${summary.meals.length ? `<section class="card">${summary.meals.map(mealEntry).join('')}</section>` : ''}
    <section class="card">${renderBars(PRIMARY, summary, goal)}</section>
    <details class="card">
      <summary>熱量、咖啡因、肌酸</summary>
      ${renderBars(SECONDARY, summary, goal)}
    </details>`;
}

function renderWorkout() {
  const today = dateKey();
  const start = weekStart(today);
  const dates = Array.from({ length: 7 }, (_, i) => addDays(start, i));
  const week = dates.flatMap((d) => summarizeDay(store.eventsOn(d)).workouts);
  const goal = goalFor(store.goals, today);
  const minutes = week.reduce((sum, w) => sum + w.data.durationSec / 60, 0);
  const activeDays = new Set(week.map((w) => w.date)).size;
  const byActivity = {};
  week.forEach((w) => { byActivity[w.data.activity] = (byActivity[w.data.activity] || 0) + w.data.durationSec / 60; });
  const recent = store.liveEvents()
    .filter((e) => e.type === 'workout')
    .sort((a, b) => b.occurredAt - a.occurredAt)
    .slice(0, 20);

  return `
    <header class="topbar"><h1>運動</h1></header>
    <button class="btn primary wide" style="margin-bottom:12px" data-action="workout-new">記錄運動</button>
    <section class="card">
      <h2>本週</h2>
      <div class="row"><span>訓練天數</span><strong>${activeDays} / ${goal.workoutDays.length} 天</strong></div>
      <div class="row"><span>總時間</span><strong>${Math.round(minutes)} 分鐘</strong></div>
      ${Object.entries(byActivity).map(([name, value]) => `<div class="row"><span class="muted">${esc(name)}</span><span class="muted">${Math.round(value)} 分鐘</span></div>`).join('')}
    </section>
    <section class="card">
      <h2>最近紀錄</h2>
      ${recent.map((w) => `<button class="entry" data-action="workout-open" data-id="${esc(w.id)}">
        <span class="entry-body"><strong>${esc(w.data.activity)}</strong><span class="muted">${formatDate(w.date)}・${workoutText(w)}</span></span>
      </button>`).join('') || '<p class="muted">還沒有運動紀錄</p>'}
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

function renderMealSheet() {
  const { draft } = state;
  const fields = NUTRIENTS.map(({ key, label, unit }) => `<label class="field"><span>${label}（${unit}）</span>
    <input type="number" inputmode="decimal" min="0" step="any" name="${key}" value="${draft.nutrients[key] ?? ''}"></label>`).join('');
  const items = draft.items.map((i) => `${esc(i.name)}（${esc(i.portion)}）`).join('、');
  const confidence = { low: '低', medium: '中', high: '高' }[draft.confidence];

  openSheet(`<form class="stack" data-form="meal">
    <div class="row"><h2 style="margin:0">${draft.id ? '修改餐點' : '確認餐點'}</h2>
      <button type="button" class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button></div>
    ${draft.photo ? `<img class="preview" src="${draft.photo}" alt="餐點照片">` : draft.attachmentId ? `<img class="preview" data-attachment="${esc(draft.attachmentId)}" alt="餐點照片">` : ''}
    <p class="muted" id="ai-status">${draft.analyzing ? '正在估算…' : draft.error ? esc(draft.error) : items ? `${items}${confidence ? `｜可信度：${confidence}` : ''}` : ''}</p>
    <label class="field"><span>補充說明</span><input name="note" value="${esc(draft.note)}" placeholder="例如：飯只吃一半"></label>
    ${draft.photo || draft.attachmentId ? `<button type="button" class="btn wide" data-action="reanalyze"${draft.analyzing ? ' disabled' : ''}>重新估算</button>` : ''}
    <div class="grid-2">${fields}</div>
    <button class="btn primary wide">儲存</button>
    ${draft.id ? '<button type="button" class="btn danger wide" data-action="meal-delete">刪除</button>' : ''}
  </form>`);
}

function readDraftForm() {
  const form = sheet.querySelector('form[data-form="meal"]');
  if (!form || !state.draft) return;
  const data = new FormData(form);
  state.draft.note = data.get('note');
  for (const { key } of NUTRIENTS) {
    const value = data.get(key);
    state.draft.nutrients[key] = value === '' ? undefined : Number(value);
  }
}

async function runAnalysis() {
  const { draft } = state;
  readDraftForm();
  draft.analyzing = true;
  draft.error = '';
  renderMealSheet();
  try {
    const dataUrl = draft.photo ?? (await store.getAttachment(draft.attachmentId))?.dataUrl;
    const result = await analyzeMeal(dataUrl, draft.note);
    if (state.draft !== draft) return;
    if (!result.is_food) {
      draft.error = '照片裡看不到食物，請手動輸入';
    } else {
      draft.items = result.items;
      draft.confidence = result.confidence;
      draft.model = result.model;
      draft.nutrients = { ...result.nutrients, creatine_g: draft.nutrients.creatine_g };
    }
  } catch (error) {
    draft.error = `無法估算：${error.message}`;
  }
  if (state.draft !== draft) return;
  draft.analyzing = false;
  renderMealSheet();
}

function saveMeal() {
  const { draft } = state;
  readDraftForm();
  const nutrients = Object.fromEntries(Object.entries(draft.nutrients).filter(([, value]) => value !== undefined));
  const data = { nutrients, items: draft.items, note: draft.note };
  const raw = draft.model ? { model: draft.model, confidence: draft.confidence } : draft.raw;
  if (draft.id) {
    store.updateEvent(draft.id, data, { raw });
  } else {
    const attachmentIds = draft.photo ? [store.saveAttachment(draft.photo, 'image/jpeg')] : [];
    store.addEvent('meal', data, { date: draft.date, source: draft.model ? 'photo_ai' : 'manual', raw, attachmentIds });
  }
  closeSheet();
  toast('已記錄這一餐');
}

function newDraft(date, photo = null) {
  return { id: null, date, photo, attachmentId: null, nutrients: {}, items: [], note: '', confidence: null, model: null, raw: null, analyzing: false, error: '' };
}

function openMeal(id) {
  const event = store.events.get(id);
  state.draft = {
    ...newDraft(event.date),
    id,
    attachmentId: event.attachmentIds[0] ?? null,
    nutrients: { ...event.data.nutrients },
    items: event.data.items ?? [],
    note: event.data.note ?? '',
    confidence: event.raw?.confidence ?? null,
    raw: event.raw,
  };
  renderMealSheet();
}

async function handlePhoto(file) {
  const date = state.tab === 'diet' ? state.dietDate : dateKey();
  state.draft = newDraft(date);
  state.draft.analyzing = true;
  renderMealSheet();
  try {
    state.draft.photo = await compressImage(file);
  } catch {
    closeSheet();
    toast('照片無法讀取，請換一張');
    return;
  }
  if (getApiKey()) {
    runAnalysis();
  } else {
    state.draft.analyzing = false;
    state.draft.error = '尚未設定 API 金鑰，可以先手動輸入，照片會保留';
    renderMealSheet();
  }
}

function openWorkout(id = null, date = dateKey()) {
  const event = id ? store.events.get(id) : null;
  const activity = event?.data.activity ?? ACTIVITIES[0];
  const intensity = event?.data.intensity ?? 'moderate';
  state.draft = { id, date: event?.date ?? date, activity, intensity };
  const minutes = event ? Math.round(event.data.durationSec / 60) : 60;
  openSheet(`<form class="stack" data-form="workout">
    <div class="row"><h2 style="margin:0">${id ? '修改運動' : '記錄運動'}</h2>
      <button type="button" class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button></div>
    <div class="chips" data-chip-group="activity">${ACTIVITIES.map((a) => `<button type="button" class="chip${a === activity ? ' active' : ''}" data-action="chip" data-value="${esc(a)}">${esc(a)}</button>`).join('')}</div>
    <label class="field"><span>時間（分鐘）</span><input type="number" inputmode="numeric" min="1" name="minutes" value="${minutes}" required></label>
    <div class="chips" data-chip-group="intensity">${INTENSITIES.map((i) => `<button type="button" class="chip${i.key === intensity ? ' active' : ''}" data-action="chip" data-value="${i.key}">${i.label}</button>`).join('')}</div>
    <label class="field"><span>日期</span><input type="date" name="date" max="${dateKey()}" value="${state.draft.date}" required></label>
    <label class="field"><span>備註</span><input name="note" value="${esc(event?.data.note)}"></label>
    <button class="btn primary wide">儲存</button>
    ${id ? '<button type="button" class="btn danger wide" data-action="workout-delete">刪除</button>' : ''}
  </form>`);
}

function saveWorkout(form) {
  const { draft } = state;
  const values = new FormData(form);
  const data = { activity: draft.activity, intensity: draft.intensity, durationSec: Number(values.get('minutes')) * 60, note: values.get('note') };
  const date = values.get('date');
  if (draft.id) store.updateEvent(draft.id, data, { date, occurredAt: new Date(`${date}T12:00:00`).getTime() });
  else store.addEvent('workout', data, { date });
  closeSheet();
  toast('已記錄運動');
}

function openDay(date) {
  const { summary } = dayData(date);
  const entries = [
    ...summary.meals.map(mealEntry),
    ...summary.workouts.map((w) => `<button class="entry" data-action="workout-open" data-id="${esc(w.id)}"><span class="entry-body"><strong>${esc(w.data.activity)}</strong><span class="muted">${workoutText(w)}</span></span></button>`),
  ].join('');
  const nutrition = NUTRIENTS.filter(({ key }) => summary.nutrients[key]).map(({ key, label, unit }) => `${label} ${round(summary.nutrients[key])} ${unit}`).join('・');
  openSheet(`<div class="stack">
    <div class="row"><h2 style="margin:0">${formatDate(date)}</h2>
      <button class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button></div>
    ${entries || '<p class="muted">這天沒有紀錄</p>'}
    ${nutrition ? `<p class="muted">${esc(nutrition)}</p>` : ''}
    ${date <= dateKey() ? `<button class="btn wide" data-action="diet-goto" data-date="${date}">補記飲食</button>
    <button class="btn wide" data-action="workout-new" data-date="${date}">補記運動</button>` : ''}
  </div>`);
}

function openSettings() {
  const goal = goalFor(store.goals, dateKey());
  const targets = NUTRIENTS.map(({ key, label, unit, isLimit }) => `<label class="field"><span>${label}${isLimit ? '上限' : ''}（${unit}）</span>
    <input type="number" inputmode="decimal" min="0" step="any" name="${key}" value="${goal.targets[key]}"></label>`).join('');
  const days = [1, 2, 3, 4, 5, 6, 0].map((d) => `<button type="button" class="chip${goal.workoutDays.includes(d) ? ' active' : ''}" data-action="chip-toggle" data-value="${d}">${WEEKDAYS[d]}</button>`).join('');
  const account = store.user
    ? `<p class="muted">已登入：${esc(store.user.email)}</p><button type="button" class="btn wide" data-action="sign-out">登出</button>`
    : `<p class="muted">未登入，資料只存在這台裝置。登入後會自動上傳並跨裝置同步。</p>
       <button type="button" class="btn wide" data-action="sign-in"${store.canSignIn ? '' : ' disabled'}>使用 Google 登入</button>`;

  openSheet(`<form class="stack" data-form="settings">
    <div class="row"><h2 style="margin:0">設定</h2>
      <button type="button" class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button></div>
    <section class="card stack"><h2>帳號</h2>${account}</section>
    <section class="card stack"><h2>每日目標（從今天起生效）</h2><div class="grid-2">${targets}</div>
      <h2>每週訓練日</h2><div class="chips" data-chip-group="days">${days}</div></section>
    <section class="card stack"><h2>飲食達成</h2>
      <label class="field"><span>每天至少記幾餐</span><input type="number" inputmode="numeric" min="1" name="minMeals" value="${goal.minMeals}"></label></section>
    <section class="card stack"><h2>快速按鈕份量</h2><div class="grid-2">
      <label class="field"><span>喝水（ml）</span><input type="number" inputmode="numeric" min="0" name="quick_water_ml" value="${goal.quick.water_ml}"></label>
      <label class="field"><span>咖啡因（mg）</span><input type="number" inputmode="numeric" min="0" name="quick_caffeine_mg" value="${goal.quick.caffeine_mg}"></label>
      <label class="field"><span>肌酸（g）</span><input type="number" inputmode="decimal" min="0" step="any" name="quick_creatine_g" value="${goal.quick.creatine_g}"></label>
    </div></section>
    <section class="card stack"><h2>照片估算</h2>
      <label class="field"><span>Anthropic API 金鑰（只存在這支手機）</span>
      <input type="password" name="apiKey" autocomplete="off" value="${esc(getApiKey())}" placeholder="sk-ant-..."></label></section>
    <button class="btn primary wide">儲存設定</button>
    <button type="button" class="btn wide" data-action="export">匯出全部資料（JSON）</button>
  </form>`);
}

function saveSettings(form) {
  const values = new FormData(form);
  const number = (name) => Number(values.get(name)) || 0;
  const workoutDays = [...form.querySelectorAll('[data-chip-group="days"] .chip.active')].map((c) => Number(c.dataset.value));
  store.saveGoal({
    targets: Object.fromEntries(NUTRIENTS.map(({ key }) => [key, number(key)])),
    workoutDays,
    minMeals: number('minMeals') || 1,
    quick: { water_ml: number('quick_water_ml'), caffeine_mg: number('quick_caffeine_mg'), creatine_g: number('quick_creatine_g') },
  });
  setApiKey(values.get('apiKey').trim());
  closeSheet();
  toast('設定已儲存');
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

function logQuick(kind) {
  const { quick } = goalFor(store.goals, dateKey());
  const presets = {
    water: { label: `喝水 ${quick.water_ml} ml`, nutrients: { water_ml: quick.water_ml } },
    coffee: { label: `咖啡 ${quick.caffeine_mg} mg`, nutrients: { caffeine_mg: quick.caffeine_mg } },
    creatine: { label: `肌酸 ${quick.creatine_g} g`, nutrients: { creatine_g: quick.creatine_g } },
  };
  const event = store.addEvent('intake', presets[kind]);
  toast(`已記錄 ${presets[kind].label}`, () => store.deleteEvent(event.id));
}

const actions = {
  tab: ({ tab }) => { state.tab = tab; render(); scrollTo(0, 0); },
  settings: openSettings,
  'sheet-close': closeSheet,
  quick: ({ kind }) => logQuick(kind),
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
  photo: () => photoInput.click(),
  'meal-manual': () => { state.draft = newDraft(state.dietDate); renderMealSheet(); },
  'meal-open': ({ id }) => openMeal(id),
  'meal-delete': () => {
    const { id } = state.draft;
    store.deleteEvent(id);
    closeSheet();
    toast('已刪除', () => store.restoreEvent(id));
  },
  reanalyze: runAnalysis,
  'workout-new': ({ date }) => openWorkout(null, date),
  'workout-open': ({ id }) => openWorkout(id),
  'workout-delete': () => {
    const { id } = state.draft;
    store.deleteEvent(id);
    closeSheet();
    toast('已刪除', () => store.restoreEvent(id));
  },
  chip: ({ value }, target) => {
    const group = target.closest('[data-chip-group]');
    group.querySelectorAll('.chip').forEach((c) => c.classList.toggle('active', c === target));
    state.draft[group.dataset.chipGroup] = value;
  },
  'chip-toggle': (_, target) => target.classList.toggle('active'),
  'sign-in': () => store.signIn().then(closeSheet).catch((error) => toast(`登入失敗：${error.message}`)),
  'sign-out': () => store.signOut().then(closeSheet),
  export: () => exportData().catch((error) => toast(`匯出失敗：${error.message}`)),
};

document.addEventListener('click', (event) => {
  const target = event.target.closest('[data-action]');
  if (target) actions[target.dataset.action](target.dataset, target);
});

sheet.addEventListener('submit', (event) => {
  event.preventDefault();
  const kind = event.target.dataset.form;
  if (kind === 'meal') saveMeal();
  if (kind === 'workout') saveWorkout(event.target);
  if (kind === 'settings') saveSettings(event.target);
});

// close 事件是非同步的，此時可能已經開了下一張表單
sheet.addEventListener('close', () => { if (!sheet.open) state.draft = null; });

photoInput.addEventListener('change', () => {
  const [file] = photoInput.files;
  photoInput.value = '';
  if (file) handlePhoto(file);
});

store.addEventListener('change', render);
store.addEventListener('error', (event) => toast(`資料儲存失敗：${event.detail.message}`));

// 回到 App 時同步其他裝置的新資料
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && !sheet.open) store.reload();
});

render();
store.init().catch((error) => toast(`啟動失敗：${error.message}`));

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
