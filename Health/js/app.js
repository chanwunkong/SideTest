import { CARD_TYPES, DEFAULT_CARDS, FREQUENCIES, cardDayState, cardHit, frequencyOf, metricsOf, monthDates, periodDates, weekDates } from './cards.js';
import { LANGUAGES, lang, setLang, t } from './i18n.js';
import { addDays, dateKey, goalFor, nutritionTargetsFor, parseDateKey, sevenDayAverage, summarizeDay, targetWeight, weekStart } from './logic.js';
import { GEMINI_PROMPT, parseMeal } from './mealImport.js';
import { ACTIVITIES, INTENSITIES, NUTRIENTS } from './registry.js';
import { healthStore as store } from './store.js';
import { MODES, MOVE_METRICS, moveTotals, scheduleRemaining, schedulesOn, targetSetsOn } from './training.js';

const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const toastEl = document.getElementById('toast');

const state = {
  tab: 'home',
  dietDate: dateKey(),
  range: 'week',
  cardId: null,
  month: dateKey().slice(0, 7),
  recordDate: null,
  draft: null,
};

const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六'];
const WEEK_ORDER = [1, 2, 3, 4, 5, 6, 0];
const PRIMARY = NUTRIENTS.filter((n) => n.primary);
const SECONDARY = NUTRIENTS.filter((n) => !n.primary);

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const round = (value) => Math.round((value + Number.EPSILON) * 10) / 10;
const intensityLabel = (key) => INTENSITIES.find((i) => i.key === key)?.label ?? '';
const closeButton = '<button type="button" class="icon-btn" data-action="sheet-close" aria-label="關閉">✕</button>';
const hours = (seconds) => round(seconds / 3600);

const sheetHeader = (title) => `<div class="row"><h2 style="margin:0">${title}</h2>${closeButton}</div>`;

// 每個身體項目各自找最新一筆，體重和體脂可以不同天量
function latestMetric(key, date) {
  const record = store.liveEvents()
    .filter((e) => e.type === 'body' && e.date <= date && e.data[key] != null)
    .sort((a, b) => b.occurredAt - a.occurredAt)[0];
  return record ? { value: record.data[key], date: record.date } : null;
}

function latestBody(date) {
  const weight = latestMetric('weightKg', date);
  const bodyFat = latestMetric('bodyFat', date);
  if (!weight && !bodyFat) return null;
  return { weightKg: weight?.value ?? null, bodyFat: bodyFat?.value ?? null, date: (weight ?? bodyFat).date };
}

function dayData(date) {
  return { summary: summarizeDay(store.eventsOn(date)), goal: goalFor(store.goals, date) };
}

const activeSchedules = () => store.activeDocs('schedules');
const trainingById = (id) => store.trainings.find((training) => training.id === id);

const cardsList = () => store.profile.cards ?? DEFAULT_CARDS;
const saveCards = (cards) => store.saveProfile({ cards });

function formatDate(key) {
  return parseDateKey(key).toLocaleDateString(lang, { month: 'long', day: 'numeric', weekday: 'short' });
}

function mealText(meal) {
  const items = (meal.data.items || []).map((i) => i.name).join('、');
  return items || meal.data.note || '未命名餐點';
}

function workoutText({ data }) {
  const parts = [];
  if (data.sets) parts.push(t('workout.setsValue', { n: data.sets }));
  if (data.durationSec) parts.push(t('workout.minutes', { n: Math.round(data.durationSec / 60) }));
  if (data.intensity) parts.push(intensityLabel(data.intensity));
  return parts.join('・');
}

// ---- 畫面 ----

function render() {
  const screens = { home: renderHome, diet: renderDiet, workout: renderWorkout, rest: renderRest, body: renderBody };
  document.documentElement.lang = lang;
  document.querySelectorAll('.tabs button').forEach((b) => {
    b.classList.toggle('active', b.dataset.tab === state.tab);
    b.textContent = t(`tab.${b.dataset.tab}`);
  });
  view.innerHTML = screens[state.tab]();
  hydrateImages(view);
}

function hydrateImages(root) {
  root.querySelectorAll('img[data-attachment]').forEach(async (img) => {
    const attachment = await store.getAttachment(img.dataset.attachment);
    if (attachment) img.src = attachment.dataUrl;
  });
}

function cardState(card, date) {
  const today = dateKey();
  const start = store.firstRecordDate(CARD_TYPES[card.type].eventTypes) ?? today;
  const periodHits = date === today ? periodDates(card, today).filter((d) => cardHit(card, summarizeDay(store.eventsOn(d)))).length : 0;
  const context = { scheduled: schedulesOn(activeSchedules(), date).length > 0, targetSets: targetSetsOn(activeSchedules(), date), periodHits };
  return cardDayState(card, date, summarizeDay(store.eventsOn(date)), today, start < today ? start : today, context);
}

// 上方表格：每張卡片一列、每天一欄，週與月用同一種表格
function renderTracker(cards, dates) {
  const today = dateKey();
  const head = dates.map((date) => {
    const day = parseDateKey(date).getDate();
    const label = state.range === 'week' ? day : day === 1 || day % 5 === 0 ? day : '';
    return `<span class="${date === today ? 'now' : ''}">${label}</span>`;
  }).join('');
  const rows = cards.map((card) => {
    const type = CARD_TYPES[card.type];
    const marks = dates.map((date) => `<i class="mark ${cardState(card, date)}${date === today ? ' today' : ''}"></i>`).join('');
    return `<div class="tracker-row" role="img" aria-label="${type.label}" style="--c:${type.color}">
      <svg class="card-icon on" viewBox="0 0 24 24" aria-hidden="true">${type.icon}</svg>${marks}
    </div>`;
  }).join('');
  return `<section class="tracker ${state.range}" style="--days:${dates.length}">
    <div class="tracker-row tracker-head"><span></span>${head}</div>
    ${rows}
  </section>`;
}

function formatValue(value) {
  return value === null || value === undefined ? '—' : round(value).toLocaleString();
}

// 身體數據保留最新一筆並標示幾天前；其他項目顯示今天
function metricValue(metric, summary, card) {
  if (!CARD_TYPES[card.type].keepsLatest) return { value: metric.read(summary), age: '' };
  const latest = latestMetric(metric.key, dateKey());
  if (!latest) return { value: null, age: '' };
  const days = Math.round((parseDateKey(dateKey()) - parseDateKey(latest.date)) / 86400000);
  return { value: latest.value, age: days ? t('card.daysAgo', { n: days }) : '' };
}

function renderCard(card, summary) {
  const type = CARD_TYPES[card.type];
  const metrics = metricsOf(card);
  const doneToday = cardState(card, dateKey()) === 'done';
  const shown = metrics.map((metric) => ({ metric, ...metricValue(metric, summary, card) }));
  const age = (item) => (item.age ? `<em>${item.age}</em>` : '');
  const values = shown.length === 1
    ? `<div class="card-big">${formatValue(shown[0].value)}<small> ${shown[0].metric.unit}</small></div>${age(shown[0])}`
    : `<div class="card-rows">${shown.map((item) => `<div><span>${item.metric.label}${age(item)}</span><strong>${formatValue(item.value)}</strong></div>`).join('')}</div>`;

  return `<button class="health-card" data-action="card-open" data-id="${esc(card.id)}" style="--c:${type.color}" aria-label="${type.label}${doneToday ? '，今天已做到' : ''}">
    <span class="card-head">
      <svg class="card-icon${doneToday ? ' on' : ''}" viewBox="0 0 24 24" aria-hidden="true">${type.icon}</svg>
      <span>${type.label}</span>
    </span>
    ${values}
  </button>`;
}

function renderHome() {
  const today = dateKey();
  const { summary } = dayData(today);
  const dates = state.range === 'week' ? weekDates(today) : monthDates(today.slice(0, 7)).filter(Boolean);
  const cards = cardsList();

  return `
    <header class="topbar">
      <div><h1>${formatDate(today)}</h1></div>
      <div class="switch" role="group" aria-label="日曆範圍">
        <button data-action="range" data-range="week" aria-pressed="${state.range === 'week'}">週</button>
        <button data-action="range" data-range="month" aria-pressed="${state.range === 'month'}">月</button>
      </div>
    </header>
    ${renderTracker(cards, dates)}
    <div class="row overview-head">
      <h2 style="margin:0">生理功能總覽</h2>
      <div class="chips">
        <button class="plain" data-action="cards-edit">編輯</button>
        <button class="plain" data-action="settings" aria-label="設定">⚙</button>
      </div>
    </div>
    <section class="health-cards">${cards.map((card) => renderCard(card, summary)).join('')}</section>`;
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

function nutrientRows(list, summary, average, targets) {
  return list.map(({ key, unit }) => `<div class="nutrient-row">
    <span>${t(`nutrient.${key}`)}</span>
    <strong>${formatValue(summary.nutrients[key] ?? null)}</strong>
    <span>${formatValue(average[key] ?? null)}</span>
    <span>${formatValue(targets[key] ?? null)}</span>
    <small>${unit}</small>
  </div>`).join('');
}

function renderDiet() {
  const date = state.dietDate;
  const { summary, goal } = dayData(date);
  const targets = nutritionTargetsFor(goal, date);
  const { average, dayCount } = sevenDayAverage((d) => store.eventsOn(d), date);

  return `
    <header class="topbar">
      <button class="icon-btn" data-action="diet-date" data-delta="-1" aria-label="‹">‹</button>
      <div style="text-align:center"><h1>${t('diet.title')}</h1><p class="muted">${formatDate(date)}</p></div>
      <button class="icon-btn" data-action="diet-date" data-delta="1" aria-label="›"${date >= dateKey() ? ' disabled' : ''}>›</button>
    </header>
    <section class="grid-2 meal-actions">
      <button class="btn primary" data-action="meal-paste">${t('diet.paste')}</button>
      <button class="btn" data-action="meal-manual">${t('diet.manual')}</button>
    </section>
    <section class="card">${summary.meals.map(mealEntry).join('') || `<p class="muted">${t('diet.noMeals')}</p>`}</section>
    <section class="card">
      <div class="nutrient-row head"><span></span><span>${t('diet.today')}</span><span>${t('diet.average')}</span><span>${t('diet.target')}</span><small></small></div>
      ${nutrientRows(PRIMARY, summary, average, targets)}
      <details><summary>${t('diet.more')}</summary>${nutrientRows(SECONDARY, summary, average, targets)}</details>
      <div class="row" style="margin-top:10px">
        <span class="muted">${t('diet.days', { n: dayCount })}</span>
        <button class="plain" data-action="targets-edit">${t('diet.setTargets')}</button>
      </div>
    </section>
    <button class="textlink" data-action="prompt-show">${t('diet.prompt')}</button>`;
}

function renderWorkout() {
  const today = dateKey();
  const dates = Array.from({ length: 7 }, (_, i) => addDays(weekStart(today), i));
  const week = dates.flatMap((d) => summarizeDay(store.eventsOn(d)).workouts);
  const minutes = week.reduce((sum, w) => sum + (w.data.durationSec ?? 0) / 60, 0);
  const sets = week.reduce((sum, w) => sum + (w.data.sets ?? 0), 0);
  const activeDays = new Set(week.map((w) => w.date)).size;
  const todays = summarizeDay(store.eventsOn(today)).workouts;
  const scheduled = schedulesOn(activeSchedules(), today).filter((s) => trainingById(s.trainingId));
  const trainings = store.activeDocs('trainings');
  const recent = store.liveEvents().filter((e) => e.type === 'workout').sort((a, b) => b.occurredAt - a.occurredAt).slice(0, 10);

  const todayRows = scheduled.map((schedule) => {
    const training = trainingById(schedule.trainingId);
    const done = todays.filter((w) => w.data.trainingId === training.id);
    const count = done.reduce((sum, w) => sum + (w.data.sets ?? 0), 0);
    const status = schedule.targetSets ? t('record.target', { n: schedule.targetSets }) : '';
    return `<div class="row plan-row">
      <div><strong>${esc(training.name)}</strong>
      <div class="muted">${[count ? t('workout.setsValue', { n: count }) : '', status].filter(Boolean).join('・')}</div></div>
      <button class="btn primary" data-action="record-open" data-id="${esc(training.id)}">${t('record.done')}</button>
    </div>`;
  }).join('');

  return `
    <header class="topbar"><h1>${t('workout.title')}</h1></header>
    <section class="card">
      <h2>${t('workout.today')}</h2>
      ${todayRows || `<p class="muted">${t('workout.noneToday')}</p>`}
      <button class="textlink" data-action="workout-new">${t('workout.free')}</button>
    </section>
    <section class="card">
      <h2>${t('workout.week')}</h2>
      <div class="stat-row"><span>${t('workout.days')}</span><strong>${activeDays}</strong></div>
      <div class="stat-row"><span>${t('workout.sets')}</span><strong>${sets}</strong></div>
      <div class="stat-row"><span>${t('workout.time')}</span><strong>${t('workout.minutes', { n: Math.round(minutes) })}</strong></div>
    </section>
    <section class="card">
      <div class="row"><h2 style="margin:0">${t('workout.schedules')}</h2><button class="plain" data-action="schedule-edit">＋ ${t('common.add')}</button></div>
      ${activeSchedules().map((schedule) => {
        const left = scheduleRemaining(schedule, today);
        const label = left === null ? t('schedule.ongoing') : left ? t('schedule.remaining', { n: left }) : t('schedule.ended');
        return `<button class="plan-row" data-action="schedule-edit" data-id="${esc(schedule.id)}"><span>${esc(trainingById(schedule.trainingId)?.name)}</span><span class="muted">${label}</span></button>`;
      }).join('')}
    </section>
    <section class="card">
      <div class="row"><h2 style="margin:0">${t('workout.trainings')}</h2><button class="plain" data-action="training-edit">＋ ${t('common.add')}</button></div>
      ${trainings.map((training) => `<button class="plan-row" data-action="training-edit" data-id="${esc(training.id)}"><span>${esc(training.name)}</span><span class="muted">${t(`mode.${training.mode}`)}</span></button>`).join('') || `<p class="muted">${t('workout.noTrainings')}</p>`}
    </section>
    <section class="card">
      <h2>${t('workout.recent')}</h2>
      ${recent.map((w) => `<button class="entry" data-action="workout-open" data-id="${esc(w.id)}">
        <span class="entry-body"><strong>${esc(w.data.activity)}</strong><span class="muted">${formatDate(w.date)}・${esc(workoutText(w))}</span></span>
      </button>`).join('') || `<p class="muted">${t('workout.noRecords')}</p>`}
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
  const bmi = body?.weightKg && height ? round(body.weightKg / (height * height)) : null;
  const goalWeight = body ? targetWeight(goal, body.weightKg, body.bodyFat) : goal.body.targetWeightKg;
  const records = store.liveEvents().filter((e) => e.type === 'body').sort((a, b) => b.occurredAt - a.occurredAt).slice(0, 7);
  const hasGoal = goal.body.targetDate && (goal.body.targetWeightKg || goal.body.targetBodyFat);

  return `
    <header class="topbar"><h1>身體</h1></header>
    <section class="focus">
      <div class="focus-main">${body ? [body.weightKg && `${body.weightKg} kg`, body.bodyFat && `體脂 ${body.bodyFat}%`].filter(Boolean).join('・') : '還沒有身體數據'}</div>
      ${bmi ? `<div class="muted">BMI ${bmi}・${formatDate(body.date)}</div>` : ''}
      <div class="focus-actions">
        <button class="btn primary" data-action="body-new">記錄數據</button>
        <button class="btn" data-action="profile-edit">基本資料</button>
      </div>
    </section>
    <section class="card">
      <div class="row"><h2 style="margin:0">目標</h2><button class="btn" data-action="goal-edit">${hasGoal ? '修改' : '設定'}</button></div>
      ${hasGoal ? `<p style="margin-top:8px">${goalWeight ? `${round(goalWeight)} kg` : `體脂 ${goal.body.targetBodyFat}%`}・${formatDate(goal.body.targetDate)} 前</p>` : '<p class="muted" style="margin-top:8px">還沒有設定目標</p>'}
    </section>
    <section class="card">
      <h2>最近紀錄</h2>
      ${records.map((r) => `<button class="entry" data-action="body-open" data-id="${esc(r.id)}">
        <span class="entry-body"><strong>${[r.data.weightKg && `${r.data.weightKg} kg`, r.data.bodyFat && `體脂 ${r.data.bodyFat}%`, r.data.muscleKg && `肌肉 ${r.data.muscleKg} kg`].filter(Boolean).join('・')}</strong><span class="muted">${formatDate(r.date)}</span></span>
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
  state.recordDate = null;
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
    ${sheetHeader(t('diet.pasteTitle'))}
    <label class="field"><span>${t('diet.pasteHint')}</span><textarea name="text" id="paste-text"></textarea></label>
    <p class="muted" id="paste-error"></p>
    <button class="btn wide">確認</button>
    <button type="button" class="textlink" data-action="prompt-show">${t('diet.prompt')}</button>
  </form>`);
  sheet.querySelector('#paste-text').addEventListener('paste', (event) => {
    event.preventDefault();
    importMeal(event.clipboardData.getData('text'));
  });
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
  return { id: null, date: state.recordDate ?? (state.tab === 'diet' ? state.dietDate : dateKey()), nutrients: {}, items: [], note: '', confidence: null, source: 'manual' };
}

function renderMealSheet() {
  const { draft } = state;
  const field = ({ key, unit }) => numberField(key, `${t(`nutrient.${key}`)}（${unit}）`, draft.nutrients[key]);
  const items = draft.items.map((i) => `${esc(i.name)}（${esc(i.portion)}）`).join('、');
  const confidence = { low: '低', medium: '中', high: '高' }[draft.confidence];

  openSheet(`<form class="stack" data-form="meal">
    ${sheetHeader(t(draft.id ? 'diet.editMeal' : 'diet.confirm'))}
    ${items ? `<p class="muted">${items}${confidence ? `｜可信度：${confidence}` : ''}</p>` : ''}
    <label class="field"><span>${t('diet.note')}</span><input name="note" value="${esc(draft.note)}"></label>
    <div class="grid-2">${PRIMARY.map(field).join('')}</div>
    <details><summary>${t('diet.more')}</summary><div class="grid-2">${SECONDARY.map(field).join('')}</div></details>
    <button class="btn primary wide">${t('common.save')}</button>
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
  toast(t('diet.saved'));
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
  if (event?.data.trainingId) { openRecord(event.data.trainingId, event.date, event); return; }
  const activity = event?.data.activity ?? ACTIVITIES[0];
  const intensity = event?.data.intensity ?? 'moderate';
  state.draft = { id, date: event?.date ?? date, activity, intensity };
  const minutes = event ? Math.round(event.data.durationSec / 60) : 60;
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

// 依訓練的記錄方式顯示欄位，細節都是選填
function openRecord(trainingId, date = dateKey(), event = null) {
  const training = trainingById(trainingId);
  const moves = event?.data.moves ?? training?.moves ?? [];
  const mode = event?.data.mode ?? training.mode;
  state.draft = { id: event?.id ?? null, trainingId, mode, moves, name: event?.data.activity ?? training.name };
  const extras = moves.map((move, i) => (move.metrics?.reps ? numberField(`extra-${i}`, t('record.extra', { name: esc(move.name) }), event?.data.extra?.[i]) : '')).join('');
  const rest = training?.trackRest ? numberField('restSec', t('record.rest'), event?.data.restSec) : '';
  openSheet(`<form class="stack" data-form="record">
    ${sheetHeader(t('record.title', { name: esc(state.draft.name) }))}
    ${mode === 'sets' ? numberField('sets', t('record.sets'), event?.data.sets, 'min="1" required') : ''}
    ${extras || rest ? `<details><summary>${t('record.detail')}</summary><div class="stack" style="margin-top:10px">${extras}${rest}</div></details>` : ''}
    ${dateField(event?.date ?? date)}
    <button class="btn primary wide">${t(mode === 'sets' ? 'common.save' : 'record.done')}</button>
    ${event ? deleteButton : ''}
  </form>`);
}

function saveRecord(form) {
  const { draft } = state;
  const values = new FormData(form);
  const date = values.get('date');
  const extra = {};
  draft.moves.forEach((_, i) => {
    const value = optionalNumber(values.get(`extra-${i}`));
    if (value) extra[i] = value;
  });
  const data = {
    trainingId: draft.trainingId, activity: draft.name, mode: draft.mode, moves: draft.moves,
    sets: optionalNumber(values.get('sets')), extra, restSec: optionalNumber(values.get('restSec')),
  };
  if (draft.id) store.updateEvent(draft.id, data, { date, occurredAt: new Date(`${date}T12:00:00`).getTime() });
  else store.addEvent('workout', data, { date });
  closeSheet();
  const totals = moveTotals(data).map((m) => `${m.name} ${m.total}`).join('、');
  toast(totals || t('record.saved'));
}

function readTrainingForm(form) {
  const { draft } = state;
  const values = new FormData(form);
  draft.name = values.get('name');
  draft.trackRest = form.querySelector('[name=trackRest]').checked;
  draft.moves = draft.moves.map((move, i) => ({
    ...move,
    name: values.get(`move-name-${i}`),
    metrics: Object.fromEntries(MOVE_METRICS.flatMap(({ key, type }) => {
      const value = values.get(`move-${key}-${i}`);
      if (!value) return [];
      return [[key, type === 'number' ? Number(value) : value]];
    })),
  }));
}

function moveFields(move, i) {
  const metricField = ({ key, type }) => `<label class="field"><span>${t(`metric.${key}`)}</span>
    <input name="move-${key}-${i}" type="${type}" ${type === 'number' ? 'inputmode="decimal" min="0" step="any"' : ''} value="${esc(move.metrics?.[key])}"></label>`;
  const [reps, ...more] = MOVE_METRICS;
  return `<div class="move-row">
    <div class="grid-2">
      <label class="field"><span>${t('training.moveName')}</span><input name="move-name-${i}" value="${esc(move.name)}" required></label>
      ${metricField(reps)}
    </div>
    <details><summary>${t('training.moreMetrics')}</summary><div class="grid-2">${more.map(metricField).join('')}</div></details>
  </div>`;
}

function renderTrainingSheet() {
  const { draft } = state;
  openSheet(`<form class="stack" data-form="training">
    ${sheetHeader(t(draft.id ? 'common.edit' : 'workout.newTraining'))}
    <label class="field"><span>${t('training.name')}</span><input name="name" value="${esc(draft.name)}" required></label>
    <h2>${t('training.mode')}</h2>
    ${chipGroup('mode', MODES.map((value) => ({ value, label: t(`mode.${value}`) })), draft.mode)}
    <h2>${t('training.moves')}</h2>
    ${draft.moves.map(moveFields).join('')}
    <button type="button" class="btn wide" data-action="move-add">＋ ${t('training.addMove')}</button>
    <label class="check"><input type="checkbox" name="trackRest"${draft.trackRest ? ' checked' : ''}> ${t('training.trackRest')}</label>
    <button class="btn primary wide">${t('common.save')}</button>
    ${draft.id ? `<button type="button" class="btn danger wide" data-action="doc-delete" data-col="trainings">${t('common.delete')}</button>` : ''}
  </form>`);
}

function openTraining(id) {
  const training = id ? trainingById(id) : null;
  state.draft = training ? structuredClone(training) : { name: '', mode: 'sets', moves: [{ name: '', metrics: {} }], trackRest: false };
  renderTrainingSheet();
}

function saveTraining(form) {
  readTrainingForm(form);
  store.saveDoc('trainings', state.draft);
  closeSheet();
}

function openSchedule(id) {
  const trainings = store.activeDocs('trainings');
  if (!trainings.length) { openTraining(); return; }
  const schedule = id ? activeSchedules().find((s) => s.id === id) : null;
  state.draft = schedule ? { ...schedule } : { trainingId: trainings[0].id };
  const days = WEEK_ORDER.map((d) => `<button type="button" class="chip${schedule?.weekdays?.includes(d) ? ' active' : ''}" data-action="chip-toggle" data-value="${d}">${WEEKDAYS[d]}</button>`).join('');
  openSheet(`<form class="stack" data-form="schedule">
    ${sheetHeader(t(id ? 'common.edit' : 'workout.newSchedule'))}
    <h2>${t('schedule.training')}</h2>
    ${chipGroup('trainingId', trainings.map((x) => ({ value: x.id, label: x.name })), state.draft.trainingId)}
    <label class="field"><span>${t('schedule.start')}</span><input type="date" name="startDate" value="${schedule?.startDate ?? dateKey()}" required></label>
    ${numberField('days', `${t('schedule.days')}（${t('schedule.daysHint')}）`, schedule?.days, 'min="1"')}
    <h2>${t('schedule.weekdays')}</h2>
    <p class="muted">${t('schedule.weekdaysHint')}</p>
    <div class="chips" data-chip-list="weekdays">${days}</div>
    ${numberField('targetSets', `${t('schedule.target')}（${t('schedule.targetHint')}）`, schedule?.targetSets, 'min="1"')}
    <button class="btn primary wide">${t('common.save')}</button>
    ${id ? `<button type="button" class="btn danger wide" data-action="doc-delete" data-col="schedules">${t('common.delete')}</button>` : ''}
  </form>`);
}

function saveSchedule(form) {
  const values = new FormData(form);
  store.saveDoc('schedules', {
    ...state.draft,
    startDate: values.get('startDate'),
    days: optionalNumber(values.get('days')),
    weekdays: [...form.querySelectorAll('[data-chip-list="weekdays"] .chip.active')].map((c) => Number(c.dataset.value)),
    targetSets: optionalNumber(values.get('targetSets')),
  });
  closeSheet();
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

function openSleep(id = null) {
  const event = id ? store.events.get(id) : null;
  state.draft = { id };
  openSheet(`<form class="stack" data-form="sleep">
    ${sheetHeader('記錄睡眠')}
    ${numberField('hours', '睡了幾小時', event ? hours(event.data.durationSec) : '', 'min="0.5" required')}
    ${dateField(event?.date ?? state.recordDate ?? dateKey())}
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
  state.draft = { id };
  openSheet(`<form class="stack" data-form="body">
    ${sheetHeader('記錄身體數據')}
    ${numberField('weightKg', '體重（kg）', event?.data.weightKg)}
    <div class="grid-2">
      ${numberField('bodyFat', '體脂（%）', event?.data.bodyFat)}
      ${numberField('muscleKg', '肌肉量（kg）', event?.data.muscleKg)}
    </div>
    ${dateField(event?.date ?? state.recordDate ?? dateKey())}
    <button class="btn primary wide">儲存</button>
    ${id ? deleteButton : ''}
  </form>`);
}

function saveBody(form) {
  const { draft } = state;
  const values = new FormData(form);
  const date = values.get('date');
  const data = { weightKg: optionalNumber(values.get('weightKg')), bodyFat: optionalNumber(values.get('bodyFat')), muscleKg: optionalNumber(values.get('muscleKg')) };
  if (!data.weightKg && !data.bodyFat && !data.muscleKg) { toast('至少填一項'); return; }
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
  openSheet(`<form class="stack" data-form="targets">
    ${sheetHeader(t('diet.setTargets'))}
    <p class="muted">${t('diet.targetsHint')}</p>
    <div class="grid-2">${NUTRIENTS.map(({ key, unit }) => numberField(key, `${t(`nutrient.${key}`)}（${unit}）`, goal.nutritionTargets[key])).join('')}</div>
    <button class="btn primary wide">${t('common.save')}</button>
  </form>`);
}

function recentWaterAmounts() {
  const amounts = store.liveEvents()
    .filter((e) => e.type === 'intake' && e.data.nutrients.water_ml)
    .sort((a, b) => b.occurredAt - a.occurredAt)
    .map((e) => e.data.nutrients.water_ml);
  return [...new Set(amounts)].slice(0, 3);
}

function cardQuickActions(card, date) {
  if (card.type === 'body') return '<button class="btn primary wide" data-action="body-new">記錄身體數據</button>';
  if (card.type === 'sleep') return '<button class="btn primary wide" data-action="sleep-new">記錄睡眠</button>';
  if (card.type === 'activity') {
    const scheduled = schedulesOn(activeSchedules(), date).filter((x) => trainingById(x.trainingId));
    const buttons = scheduled.map((x) => `<button class="btn primary wide" data-action="record-open" data-id="${esc(x.trainingId)}" data-date="${date}">${esc(trainingById(x.trainingId).name)}</button>`).join('');
    return `${buttons}<button class="btn wide" data-action="workout-new" data-date="${date}">${t('workout.free')}</button>`;
  }
  const recent = recentWaterAmounts().map((ml) => `<button class="chip" data-action="water-add" data-ml="${ml}">${ml} ml</button>`).join('');
  return `<div class="water-entry">
      ${recent ? `<div class="chips">${recent}</div>` : ''}
      <div class="amount"><input id="water-input" type="number" inputmode="numeric" min="1" placeholder="喝了多少 ml" aria-label="這次喝了多少毫升">
      <button class="btn" data-action="water-add">加入水分</button></div>
    </div>
    <div class="grid-2">
      <button class="btn primary" data-action="meal-paste">貼上 Gemini</button>
      <button class="btn" data-action="meal-manual">手動記餐</button>
    </div>`;
}

function openCard(id) {
  const card = cardsList().find((c) => c.id === id);
  const type = CARD_TYPES[card.type];
  state.cardId = id;
  const date = state.recordDate ?? dateKey();
  const [year, month] = state.month.split('-').map(Number);
  const summary = summarizeDay(store.eventsOn(date));
  const today = dateKey();
  const cells = monthDates(state.month).map((d) => {
    if (!d) return '<span></span>';
    return `<button class="cal-day ${cardState(card, d)}${d === today ? ' today' : ''}${d === date ? ' selected' : ''}" data-action="card-date" data-date="${d}"${d > today ? ' disabled' : ''}>${parseDateKey(d).getDate()}</button>`;
  }).join('');
  const dows = ['一', '二', '三', '四', '五', '六', '日'].map((d) => `<span class="muted">${d}</span>`).join('');
  const values = metricsOf(card).map((m) => `<div class="stat-row"><span>${m.label}</span><strong>${formatValue(m.read(summary))} ${m.unit}</strong></div>`).join('');

  openSheet(`<div class="stack" style="--c:${type.color}">
    ${sheetHeader(type.label)}
    <div class="row">
      <button class="icon-btn" data-action="card-month" data-delta="-1" aria-label="上個月">‹</button>
      <strong>${year} 年 ${month} 月</strong>
      <button class="icon-btn" data-action="card-month" data-delta="1" aria-label="下個月">›</button>
    </div>
    <div class="card-calendar">${dows}${cells}</div>
    <p class="muted">${formatDate(date)}</p>
    <div>${values}</div>
    ${cardQuickActions(card, date)}
    <button class="textlink" data-action="card-settings">卡片設定</button>
  </div>`);
}

function openCardsEditor() {
  const cards = cardsList();
  const rows = cards.map((card, i) => `<div class="row edit-row">
    <button class="edit-name" data-action="card-settings" data-id="${esc(card.id)}">${CARD_TYPES[card.type].label}<span class="muted">${metricsOf(card).map((m) => m.label).join('・')}</span></button>
    <div class="chips">
      <button class="icon-btn" data-action="card-move" data-index="${i}" data-delta="-1" aria-label="往前"${i === 0 ? ' disabled' : ''}>↑</button>
      <button class="icon-btn" data-action="card-move" data-index="${i}" data-delta="1" aria-label="往後"${i === cards.length - 1 ? ' disabled' : ''}>↓</button>
    </div>
  </div>`).join('');
  const addButtons = Object.entries(CARD_TYPES).map(([key, type]) => `<button class="chip" data-action="card-add" data-type="${key}">＋ ${type.label}</button>`).join('');
  openSheet(`<div class="stack">
    ${sheetHeader('編輯卡片')}
    <div>${rows || '<p class="muted">目前沒有卡片</p>'}</div>
    <h2>新增卡片</h2>
    <div class="chips">${addButtons}</div>
  </div>`);
}

function openCardSettings(id) {
  const card = cardsList().find((c) => c.id === id);
  const type = CARD_TYPES[card.type];
  state.cardId = id;
  const frequency = frequencyOf(card);
  state.draft = { op: card.rule?.op ?? 'gte', frequency: frequency.type };
  const metricChips = type.metrics.map((m) => `<button type="button" class="chip${card.metrics.includes(m.key) ? ' active' : ''}" data-action="metric-toggle" data-value="${m.key}">${m.label}</button>`).join('');
  const dayChips = WEEK_ORDER.map((d) => `<button type="button" class="chip${card.days?.includes(d) ? ' active' : ''}" data-action="chip-toggle" data-value="${d}">${WEEKDAYS[d]}</button>`).join('');
  openSheet(`<form class="stack" data-form="card">
    ${sheetHeader(`${type.label}設定`)}
    <h2>顯示項目（最多 3 項）</h2>
    <div class="chips" data-chip-list="metrics">${metricChips}</div>
    <h2>${t('card.frequency')}</h2>
    ${chipGroup('frequency', FREQUENCIES.map((value) => ({ value, label: t(`frequency.${value}`) })), frequency.type)}
    ${numberField('count', t('card.count'), frequency.count, 'min="1"')}
    <p class="muted">${t('card.weekdays')}</p>
    <div class="chips" data-chip-list="days">${dayChips}</div>
    <h2>做到的條件</h2>
    <p class="muted">留空就是有紀錄算做到。填了就用第一個顯示項目判斷。</p>
    ${chipGroup('op', [{ value: 'gte', label: '至少' }, { value: 'lte', label: '至多' }], state.draft.op)}
    ${numberField('ruleValue', '數值', card.rule?.value)}
    <button class="btn primary wide">儲存</button>
    <button type="button" class="btn danger wide" data-action="card-remove">移除這張卡片</button>
  </form>`);
}

function saveCardSettings(form) {
  const selected = (list) => [...form.querySelectorAll(`[data-chip-list="${list}"] .chip.active`)].map((c) => c.dataset.value);
  const metrics = selected('metrics');
  if (!metrics.length) { toast('至少選一個顯示項目'); return; }
  const value = optionalNumber(new FormData(form).get('ruleValue'));
  const count = optionalNumber(new FormData(form).get('count'));
  const update = {
    metrics, days: selected('days').map(Number), rule: value === null ? null : { op: state.draft.op, value },
    frequency: { type: state.draft.frequency, count: count ?? 1 },
  };
  saveCards(cardsList().map((c) => (c.id === state.cardId ? { ...c, ...update } : c)));
  closeSheet();
  toast('已儲存');
}

function openSettings() {
  const account = store.user
    ? `<p class="muted">已登入：${esc(store.user.email)}</p><button type="button" class="btn wide" data-action="sign-out">登出</button>`
    : `<p class="muted">未登入，資料只存在這台裝置。登入後會自動上傳並跨裝置同步。</p>
       <button type="button" class="btn wide" data-action="sign-in"${store.canSignIn ? '' : ' disabled'}>使用 Google 登入</button>`;

  openSheet(`<div class="stack">
    ${sheetHeader('設定')}
    <section class="card stack"><h2>帳號</h2>${account}</section>
    <section class="card stack"><h2>${t('settings.language')}</h2>
      <div class="chips">${LANGUAGES.map((l) => `<button class="chip${l.value === lang ? ' active' : ''}" data-action="lang-set" data-value="${l.value}">${l.label}</button>`).join('')}</div>
    </section>
    <button class="btn wide" data-action="export">匯出全部資料（JSON）</button>
  </div>`);
}

function saveGoalForm(form, kind) {
  const values = new FormData(form);
  if (kind === 'goal') {
    store.saveGoal({ body: {
      targetWeightKg: optionalNumber(values.get('targetWeightKg')),
      targetBodyFat: optionalNumber(values.get('targetBodyFat')),
      targetDate: values.get('targetDate') || null,
    } });
  } else if (kind === 'targets') {
    const nutritionTargets = {};
    for (const { key } of NUTRIENTS) {
      const value = optionalNumber(values.get(key));
      if (value !== null) nutritionTargets[key] = value;
    }
    store.saveGoal({ nutritionTargets });
  } else {
    store.saveGoal({ sleep: { hours: Number(values.get('hours')), bedtime: values.get('bedtime') } });
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

function logWater(ml) {
  const date = state.recordDate ?? dateKey();
  const event = store.addEvent('intake', { label: '喝水', nutrients: { water_ml: ml } }, { date });
  toast(`已記錄水分 ${ml} ml`, () => store.deleteEvent(event.id));
  openCard(state.cardId);
}

const actions = {
  tab: ({ tab }) => { state.tab = tab; render(); scrollTo(0, 0); },
  settings: openSettings,
  'sheet-close': closeSheet,
  range: ({ range }) => { state.range = range; render(); },
  'card-open': ({ id }) => { state.month = dateKey().slice(0, 7); state.recordDate = null; openCard(id); },
  'card-month': ({ delta }) => {
    const date = parseDateKey(`${state.month}-01`);
    date.setMonth(date.getMonth() + Number(delta));
    state.month = dateKey(date).slice(0, 7);
    openCard(state.cardId);
  },
  'card-date': ({ date }) => { state.recordDate = date; openCard(state.cardId); },
  'water-add': ({ ml }) => {
    const value = Number(ml ?? sheet.querySelector('#water-input').value);
    if (value > 0) logWater(value);
  },
  'cards-edit': openCardsEditor,
  'card-settings': ({ id }) => openCardSettings(id ?? state.cardId),
  'card-move': ({ index, delta }) => {
    const cards = [...cardsList()];
    const from = Number(index);
    const [card] = cards.splice(from, 1);
    cards.splice(from + Number(delta), 0, card);
    saveCards(cards);
    openCardsEditor();
  },
  'card-add': ({ type }) => {
    const metrics = CARD_TYPES[type].metrics.slice(0, 1).map((m) => m.key);
    saveCards([...cardsList(), { id: crypto.randomUUID(), type, metrics }]);
    openCardsEditor();
  },
  'card-remove': () => {
    const previous = cardsList();
    saveCards(previous.filter((c) => c.id !== state.cardId));
    closeSheet();
    toast('已移除卡片', () => saveCards(previous));
  },
  'metric-toggle': (_, target) => {
    const active = target.closest('[data-chip-list]').querySelectorAll('.chip.active').length;
    if (target.classList.contains('active') || active < 3) target.classList.toggle('active');
    else toast('最多 3 項');
  },
  'chip-toggle': (_, target) => target.classList.toggle('active'),
  'diet-date': ({ delta }) => {
    const next = addDays(state.dietDate, Number(delta));
    if (next <= dateKey()) { state.dietDate = next; render(); }
  },
  'meal-paste': openPaste,
  'meal-manual': () => { state.draft = newMealDraft(); renderMealSheet(); },
  'meal-open': ({ id }) => openMeal(id),
  'prompt-show': openPrompt,
  'prompt-copy': () => navigator.clipboard.writeText(GEMINI_PROMPT).then(() => toast('已複製提示詞')).catch(() => toast('無法複製，請長按文字手動複製')),
  'event-delete': () => {
    const { id } = state.draft;
    store.deleteEvent(id);
    closeSheet();
    toast('已刪除', () => store.restoreEvent(id));
  },
  'workout-new': ({ date }) => openWorkout(null, date),
  'workout-open': ({ id }) => openWorkout(id),
  'record-open': ({ id, date }) => openRecord(id, date ?? dateKey()),
  'training-edit': ({ id }) => openTraining(id),
  'schedule-edit': ({ id }) => openSchedule(id),
  'move-add': () => {
    readTrainingForm(sheet.querySelector('form'));
    state.draft.moves.push({ name: '', metrics: {} });
    renderTrainingSheet();
  },
  'doc-delete': ({ col }) => { store.saveDoc(col, { ...state.draft, deletedAt: Date.now() }); closeSheet(); },
  'lang-set': ({ value }) => { setLang(value); store.saveProfile({ lang: value }); closeSheet(); render(); },
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
  record: saveRecord,
  training: saveTraining,
  schedule: saveSchedule,
  sleep: saveSleep,
  body: saveBody,
  profile: saveProfile,
  goal: (form) => saveGoalForm(form, 'goal'),
  targets: (form) => saveGoalForm(form, 'targets'),
  'sleep-goal': (form) => saveGoalForm(form, 'sleep-goal'),
  card: saveCardSettings,
};

sheet.addEventListener('submit', (event) => {
  event.preventDefault();
  formHandlers[event.target.dataset.form](event.target);
});

// close 事件是非同步的，此時可能已經開了下一張表單
sheet.addEventListener('close', () => {
  if (sheet.open) return;
  state.draft = null;
  state.recordDate = null;
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
