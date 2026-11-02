// 訓練 App 和獨立計時器共用這份程式，資料也和健康管理共用同一個資料庫
import { CALCS, RANGES, VIEWS, byTag, collectRows, currentWeek, cycleWeeks, dailySeries, defaultCards, filterRows, headline, metricNames } from './analysis.js';
import { COLORS, TEMPLATES, flattenFlow, flowDuration, formatDuration, mapFlow, newStep, normalizeFlow } from './flow.js';
import { addDays, dateKey, parseDateKey, weekStart } from './logic.js';
import { clearSession, getSettings, runFlow, savedSession, setSetting, speak } from './flowRun.js';
import { LANGUAGES, lang, setLang, t } from './i18n.js';
import { MUSCLES, presetMuscles } from './muscles.js';
import { healthStore as store } from './store.js';
import { TAG_GROUPS, recentTags, rememberTag, tagLabel } from './tags.js';
import { MODES, MOVE_METRICS, descendantIds, exercisePath, exerciseTotal, exerciseTree, linkExercise, scheduleRemaining } from './training.js';

// 計時器入口只有課表和訓練；訓練 App 多了動作、排程和紀錄
const isTimerApp = document.documentElement.dataset.app === 'timer';
const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const panel = document.getElementById('panel');
const toastEl = document.getElementById('toast');
const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const query = new URLSearchParams(location.search);

const state = { cycle: null, phase: null, recRange: 'month', recDate: dateKey(), tab: 'routines', draft: null, routine: null, original: '', selected: null, step: null, stepList: null, adding: false, handledQuery: false };

function toast(message) {
  toastEl.hidden = false;
  // 提示要蓋在開啟中的視窗上，舊瀏覽器沒有 popover 時維持原本位置
  if (toastEl.showPopover) {
    if (toastEl.matches(':popover-open')) toastEl.hidePopover();
    toastEl.showPopover();
  }
  toastEl.textContent = message;
  setTimeout(() => { toastEl.hidden = true; }, 3000);
}

const WEEK_ORDER = [1, 2, 3, 4, 5, 6, 0];
const weekdayName = (day) => new Date(2023, 0, 1 + day).toLocaleDateString(lang, { weekday: 'narrow' });
const optionalNumber = (value) => (value === '' || value === null ? null : Number(value));
const closeButton = (action = 'back') => `<button type="button" class="icon-btn" data-action="${action}" aria-label="${t('common.close')}">✕</button>`;
const panelHeader = (title) => `<div class="row"><h2 style="margin:0">${title}</h2>${closeButton()}</div>`;
const numberField = (name, label, value, attrs = '') => `<label class="field"><span>${label}</span>
  <input ${attrs} type="number" inputmode="decimal" min="0" step="any" name="${name}" value="${value ?? ''}"></label>`;
const chipGroup = (name, options, active) => `<div class="chips" data-chip-group="${name}">${options.map((o) => `<button type="button" class="chip${o.value === active ? ' active' : ''}" data-action="chip" data-value="${esc(o.value)}">${esc(o.label)}</button>`).join('')}</div>`;
const exerciseName = (exercise) => (lang === 'en' && exercise.nameEn) || exercise.name;

const allRoutines = () => store.activeDocs('trainings');
const lastUsed = (routine) => workoutEvents().reduce((latest, e) => (e.data.trainingId === routine.id ? Math.max(latest, e.occurredAt) : latest), 0);
const routines = () => (isTimerApp ? allRoutines().filter((r) => r.flow?.length) : allRoutines())
  .sort((a, b) => Number(!!b.pinned) - Number(!!a.pinned) || lastUsed(b) - lastUsed(a));
const findRoutine = (id) => store.trainings.find((r) => r.id === id);
const workoutEvents = () => store.liveEvents().filter((e) => e.type === 'workout');
const stepName = (step) => step.label || t(step.type === 'reps' ? 'step.reps' : 'step.timer');

function start(routine, resume = null) {
  if (!flattenFlow(normalizeFlow(routine.flow)).length) { toast(t('editor.needStep')); return; }
  runFlow({ ...routine, flow: normalizeFlow(routine.flow) }, {
    events: workoutEvents(),
    resume,
    onClose: renderHome,
    onFinish: (record) => {
      store.addEvent('workout', record, { source: 'timer' });
      toast(t('workout.saved'));
    },
  });
}

function routineRow(routine) {
  const flow = normalizeFlow(routine.flow ?? []);
  const hasFlow = flow.length > 0;
  const colors = flattenFlow(flow).filter((s) => s.duration).slice(0, 40).map((s) => `<i class="tone-${s.color}" style="flex:${s.duration}"></i>`).join('');
  const detail = hasFlow ? formatDuration(flowDuration(flow), t) : t(`mode.${routine.mode}`);
  const action = hasFlow
    ? `<button class="btn primary" data-action="start" data-id="${esc(routine.id)}">${t('step.start')}</button>`
    : `<a class="btn link-btn" href="../Health/?record=${esc(routine.id)}">${t('workout.log')}</a>`;
  return `<article class="routine">
    <button class="routine-open" data-action="edit" data-id="${esc(routine.id)}">
      <strong>${routine.pinned ? '📌 ' : ''}${esc(routine.name)}</strong>
      <span class="muted">${detail}${routine.tags?.length ? `・${routine.tags.map((tag) => esc(tagLabel(tag, lang))).join('、')}` : ''}</span>
      ${colors ? `<span class="run-line small">${colors}</span>` : ''}
    </button>
    ${action}
  </article>`;
}

function renderRoutines() {
  const session = savedSession();
  const sessionRoutine = session && findRoutine(session.trainingId);
  return `
    ${sessionRoutine ? `<section class="card row"><span>${t('timer.resume', { name: esc(sessionRoutine.name) })}</span>
      <div class="chips"><button class="btn primary" data-action="resume">${t('run.resume')}</button><button class="plain" data-action="drop-session">${t('run.discard')}</button></div></section>` : ''}
    <section class="routines">${routines().map(routineRow).join('') || `<p class="muted">${t('timer.empty')}</p>`}</section>
    <button class="btn wide" data-action="new">＋ ${t('timer.new')}</button>`;
}

function renderExercises() {
  const rows = exerciseTree(store.activeDocs('exercises')).map(({ exercise, depth }) => `<button class="plan-row" style="padding-left:${depth * 18}px" data-action="exercise-edit" data-id="${esc(exercise.id)}"><span>${esc(exerciseName(exercise))}</span></button>`).join('');
  return `<section class="card">${rows || `<p class="muted">${t('exercise.empty')}</p>`}</section>
    <button class="btn wide" data-action="exercise-edit">＋ ${t('train.newExercise')}</button>`;
}

function renderSchedules() {
  const today = dateKey();
  const cycles = store.activeDocs('cycles').map((cycle) => {
    const position = currentWeek(cycle, today);
    const label = position ? t('cycle.position', position) : t('cycle.notActive');
    return `<button class="plan-row" data-action="cycle-open" data-id="${esc(cycle.id)}"><span>${esc(cycle.name)}</span><span class="muted">${esc(label)}</span></button>`;
  }).join('');
  const rows = store.activeDocs('schedules').filter((schedule) => !schedule.cycleId).map((schedule) => {
    const left = scheduleRemaining(schedule, today);
    const label = left === null ? t('schedule.ongoing') : left ? t('schedule.remaining', { n: left }) : t('schedule.ended');
    return `<button class="plan-row" data-action="schedule-edit" data-id="${esc(schedule.id)}"><span>${esc(findRoutine(schedule.trainingId)?.name)}</span><span class="muted">${label}</span></button>`;
  }).join('');
  return `<section class="card"><h2>${t('cycle.title')}</h2>${cycles || `<p class="muted">${t('cycle.none')}</p>`}</section>
    <button class="btn wide meal-actions" data-action="cycle-new">＋ ${t('cycle.new')}</button>
    <section class="card"><h2>${t('train.schedules')}</h2>${rows || `<p class="muted">${t('train.noSchedules')}</p>`}</section>
    <button class="btn wide" data-action="schedule-edit">＋ ${t('train.newSchedule')}</button>`;
}

// ---- 週期：階段的課表會變成排程，首頁和今天的運動不用另外處理 ----

const newPhase = (name = '') => ({ id: crypto.randomUUID(), name, weeks: 4, trainingIds: [], weekdays: [], planned: [] });

function renderCycleForm() {
  const { cycle } = state;
  const phases = cycle.phases.map((phase, i) => `<button type="button" class="plan-row" data-action="phase-edit" data-index="${i}"><span>${esc(phase.name || t('cycle.phase'))}</span><span class="muted">${t('cycle.weekCount', { n: phase.weeks })}</span></button>`).join('');
  openPanel(`<form class="stack" data-form="cycle">
    ${panelHeader(t(cycle.id ? 'cycle.edit' : 'cycle.new'))}
    <label class="field"><span>${t('cycle.name')}</span><input name="name" value="${esc(cycle.name)}" required></label>
    <label class="field"><span>${t('cycle.start')}</span><input type="date" name="startDate" value="${cycle.startDate}" required></label>
    <h2>${t('cycle.phases')}</h2>
    <div>${phases}</div>
    <div class="chips">
      <button type="button" class="chip" data-action="phase-add">＋ ${t('cycle.addPhase')}</button>
      ${cycle.phases.length ? '' : `<button type="button" class="chip" data-action="cycle-template">${t('cycle.template')}</button>`}
    </div>
    <div class="panel-save"><button class="btn primary wide">${t('common.save')}</button></div>
    ${cycle.id ? `<button type="button" class="btn danger wide" data-action="cycle-delete">${t('common.delete')}</button>` : ''}
  </form>`);
}

function readCycleForm() {
  const values = new FormData(panel.querySelector('form'));
  state.cycle.name = values.get('name');
  state.cycle.startDate = values.get('startDate');
}

function renderPhaseForm() {
  const { phase } = state;
  const routineChips = allRoutines().map((routine) => `<button type="button" class="chip${phase.trainingIds.includes(routine.id) ? ' active' : ''}" data-action="phase-routine" data-id="${esc(routine.id)}">${esc(routine.name)}</button>`).join('');
  const dayChips = WEEK_ORDER.map((d) => `<button type="button" class="chip${phase.weekdays.includes(d) ? ' active' : ''}" data-action="phase-day" data-value="${d}">${weekdayName(d)}</button>`).join('');
  const metrics = metricNames(analysisRows());
  const planned = phase.planned.map((item, i) => `<div class="stat-row"><span>${esc(metricLabel(item.metric))}</span><span>${item.value} <button type="button" class="plain" data-action="planned-remove" data-index="${i}" aria-label="${t('common.delete')}">✕</button></span></div>`).join('');
  openPanel(`<form class="stack" data-form="phase">
    ${panelHeader(t('cycle.phase'))}
    <div class="grid-2">
      <label class="field"><span>${t('cycle.phaseName')}</span><input name="name" value="${esc(phase.name)}" required></label>
      ${numberField('weeks', t('cycle.weeks'), phase.weeks, 'min="1" required')}
    </div>
    <h2>${t('cycle.routines')}</h2>
    <div class="chips">${routineChips || `<p class="muted">${t('train.noRoutineForSchedule')}</p>`}</div>
    <h2>${t('cycle.days')}</h2>
    <p class="muted">${t('cycle.daysHint')}</p>
    <div class="chips">${dayChips}</div>
    <h2>${t('cycle.planned')}</h2>
    ${planned}
    <div class="amount">
      <select id="planned-metric" aria-label="${t('cycle.metric')}">${optionList(metrics, metrics[0], metricLabel)}</select>
      <input id="planned-value" type="number" inputmode="decimal" min="0" step="any" placeholder="${t('cycle.value')}" aria-label="${t('cycle.value')}">
      <button type="button" class="btn" data-action="planned-add">${t('cycle.addPlanned')}</button>
    </div>
    <div class="panel-save"><button class="btn primary wide">${t('common.save')}</button></div>
    <button type="button" class="btn danger wide" data-action="phase-delete">${t('common.delete')}</button>
  </form>`);
}

function readPhaseForm() {
  const values = new FormData(panel.querySelector('form'));
  state.phase.name = values.get('name');
  state.phase.weeks = Number(values.get('weeks')) || 1;
}

function savePhase() {
  readPhaseForm();
  const { cycle, phase } = state;
  const index = cycle.phases.findIndex((item) => item.id === phase.id);
  if (index >= 0) cycle.phases[index] = phase;
  else cycle.phases.push(phase);
  state.phase = null;
  renderCycleForm();
}

function removeCycleSchedules(cycleId) {
  store.activeDocs('schedules').filter((schedule) => schedule.cycleId === cycleId).forEach((schedule) => store.saveDoc('schedules', { ...schedule, deletedAt: Date.now() }));
}

function saveCycle() {
  readCycleForm();
  const { cycle } = state;
  if (!cycle.phases.length) { toast(t('cycle.needPhase')); return; }
  const saved = store.saveDoc('cycles', cycle);
  removeCycleSchedules(saved.id);
  let startDate = saved.startDate;
  for (const phase of saved.phases) {
    for (const trainingId of phase.trainingIds) {
      store.saveDoc('schedules', { trainingId, startDate, days: phase.weeks * 7, weekdays: phase.weekdays, targetSets: null, cycleId: saved.id });
    }
    startDate = addDays(startDate, phase.weeks * 7);
  }
  state.cycle = null;
  panel.close();
}

function openCycle(id) {
  const cycle = store.activeDocs('cycles').find((item) => item.id === id);
  const weeks = cycleWeeks(cycle, analysisRows());
  const metrics = [...new Set(weeks.flatMap((week) => week.cells.map((cell) => cell.metric)))];
  const head = metrics.map((metric) => `<th colspan="2">${esc(metricLabel(metric))}</th>`).join('');
  const sub = metrics.map(() => `<th>${t('cycle.plannedShort')}</th><th>${t('cycle.actual')}</th>`).join('');
  const body = weeks.map((week) => `<tr><td>${week.index}</td><td>${esc(week.phase)}</td>${metrics.map((metric) => {
    const cell = week.cells.find((item) => item.metric === metric);
    return `<td>${cell?.planned ?? '—'}</td><td>${cell ? round(cell.actual) : '—'}</td>`;
  }).join('')}</tr>`).join('');
  openPanel(`<div class="stack">
    ${panelHeader(esc(cycle.name))}
    <div class="table-wrap"><table class="cycle-table"><thead><tr><th>${t('cycle.week')}</th><th>${t('cycle.phase')}</th>${head}</tr><tr><th></th><th></th>${sub}</tr></thead><tbody>${body}</tbody></table></div>
    <button class="btn wide" data-action="cycle-edit" data-id="${esc(cycle.id)}">${t('cycle.edit')}</button>
  </div>`);
}

function moveRecords(delta) {
  if (state.recRange === 'week') state.recDate = addDays(state.recDate, delta * 7);
  else {
    const date = parseDateKey(state.recDate);
    state.recDate = dateKey(new Date(date.getFullYear(), date.getMonth() + delta, 1));
  }
  renderHome();
}

function calendarDates() {
  if (state.recRange === 'week') {
    const start = weekStart(state.recDate);
    return Array.from({ length: 7 }, (_, i) => addDays(start, i));
  }
  const first = `${state.recDate.slice(0, 7)}-01`;
  const start = parseDateKey(first);
  const lead = (start.getDay() + 6) % 7;
  const count = new Date(start.getFullYear(), start.getMonth() + 1, 0).getDate();
  return [...Array(lead).fill(null), ...Array.from({ length: count }, (_, i) => addDays(first, i))];
}

function renderCalendar(events) {
  const today = dateKey();
  const trained = new Set(events.map((e) => e.date));
  const heads = WEEK_ORDER.map((d) => `<span class="muted">${weekdayName(d)}</span>`).join('');
  const cells = calendarDates().map((date) => {
    if (!date) return '<span></span>';
    return `<button class="rec-day${trained.has(date) ? ' trained' : ''}${date === today ? ' today' : ''}" data-action="rec-day" data-date="${date}"${date > today ? ' disabled' : ''}>${parseDateKey(date).getDate()}</button>`;
  }).join('');
  const title = parseDateKey(state.recDate).toLocaleDateString(lang, { year: 'numeric', month: 'long' });
  return `<section class="card">
    <div class="rec-head">
      <button class="icon-btn" data-action="rec-move" data-delta="-1" aria-label="${t('rec.prev')}">‹</button>
      <strong>${title}</strong>
      <button class="icon-btn" data-action="rec-move" data-delta="1" aria-label="${t('rec.next')}">›</button>
    </div>
    <div class="rec-head">
      <div class="chips">
        <button class="chip small${state.recRange === 'week' ? ' active' : ''}" data-action="rec-range" data-range="week">${t('rec.week')}</button>
        <button class="chip small${state.recRange === 'month' ? ' active' : ''}" data-action="rec-range" data-range="month">${t('rec.month')}</button>
      </div>
    </div>
    <div class="rec-calendar">${heads}${cells}</div>
  </section>`;
}

const timeText = (timestamp) => {
  const time = new Date(timestamp);
  return `${String(time.getHours()).padStart(2, '0')}:${String(time.getMinutes()).padStart(2, '0')}`;
};

function recordSummary({ data, occurredAt }) {
  const parts = [timeText(occurredAt)];
  if (data.sets && data.mode !== 'flow') parts.push(t('workout.setsValue', { n: data.sets }));
  if (data.durationSec) parts.push(t('workout.minutes', { n: Math.round(data.durationSec / 60) }));
  return parts.join('・');
}

function openDay(date) {
  state.recordDate = date;
  const records = workoutEvents().filter((e) => e.date === date).sort((a, b) => a.occurredAt - b.occurredAt);
  const rows = records.map((event) => {
    const logs = (event.data.logs ?? []).map((log) => `<div class="rec-log">${esc(log.label)}：${Object.entries(log.values ?? {}).map(([name, value]) => `${esc(name)} ${value}`).join('・')}${log.failure ? `・${t('rec.failure')}` : ''}</div>`).join('');
    return `<button class="rec-entry" data-action="rec-edit" data-id="${esc(event.id)}"><strong>${esc(event.data.activity)}</strong><span class="muted">${recordSummary(event)}</span>${logs}</button>`;
  }).join('');
  openPanel(`<div class="stack">
    ${panelHeader(parseDateKey(date).toLocaleDateString(lang, { month: 'long', day: 'numeric', weekday: 'short' }))}
    <div>${rows || `<p class="muted">${t('rec.none')}</p>`}</div>
    <button class="btn primary wide" data-action="rec-add">${t('rec.add')}</button>
  </div>`);
}

function openRecordEdit(id) {
  const event = store.events.get(id);
  state.draft = { id };
  const logs = (event.data.logs ?? []).map((log, i) => `<div class="move-row"><strong>${esc(log.label)}</strong>
    <div class="grid-2">${Object.keys(log.values ?? {}).map((name) => numberField(`log-${i}-${name}`, esc(name), log.values[name])).join('')}</div>
    <label class="check"><input type="checkbox" name="failure-${i}"${log.failure ? ' checked' : ''}> ${t('rec.failure')}</label></div>`).join('');
  openPanel(`<form class="stack" data-form="record">
    ${panelHeader(t('rec.edit'))}
    <strong>${esc(event.data.activity)}</strong>
    <label class="field"><span>${t('common.date')}</span><input type="date" name="date" max="${dateKey()}" value="${event.date}" required></label>
    <div class="grid-2">
      <label class="field"><span>${t('workout.startTime')}</span><input type="time" name="startTime" value="${timeText(event.occurredAt)}" required></label>
      ${numberField('minutes', t('workout.duration'), event.data.durationSec ? Math.round(event.data.durationSec / 60) : '', 'min="1"')}
    </div>
    ${event.data.mode === 'sets' ? numberField('sets', t('rec.sets'), event.data.sets, 'min="1"') : ''}
    ${numberField('rpe', t('rpe.label'), event.data.rpe, 'min="0" max="10" step="1"')}
    ${logs}
    <button class="btn primary wide">${t('common.save')}</button>
    <button type="button" class="btn danger wide" data-action="rec-delete">${t('common.delete')}</button>
  </form>`);
}

function saveRecordEdit(form) {
  const event = store.events.get(state.draft.id);
  const values = new FormData(form);
  const date = values.get('date');
  const logs = (event.data.logs ?? []).map((log, i) => ({
    ...log,
    failure: values.get(`failure-${i}`) === 'on',
    values: Object.fromEntries(Object.keys(log.values ?? {}).map((name) => [name, optionalNumber(values.get(`log-${i}-${name}`))]).filter(([, value]) => value !== null)),
  }));
  const minutes = optionalNumber(values.get('minutes'));
  const data = { logs, durationSec: minutes ? minutes * 60 : null, rpe: optionalNumber(values.get('rpe')) };
  if (event.data.mode === 'sets') data.sets = optionalNumber(values.get('sets'));
  store.updateEvent(event.id, data, { date, occurredAt: new Date(`${date}T${values.get('startTime')}`).getTime() });
  openDay(date);
}

function openRecordAdd() {
  const items = [
    ...allRoutines().map((routine) => ({ kind: 'routine', id: routine.id, name: routine.name })),
    ...store.activeDocs('exercises').map((exercise) => ({ kind: 'exercise', id: exercise.id, name: exerciseName(exercise) })),
  ];
  openPanel(`<div class="stack">
    ${panelHeader(t('rec.pick'))}
    <div class="pick-list">${items.map((item) => `<button class="pick-row" data-action="rec-pick" data-kind="${item.kind}" data-id="${esc(item.id)}"><span>${esc(item.name)}</span>${item.kind === 'routine' ? `<span class="muted">${t('workout.routineBadge')}</span>` : ''}</button>`).join('') || `<p class="muted">${t('rec.noOptions')}</p>`}</div>
  </div>`);
}

function addRecord(kind, id) {
  const date = state.recordDate;
  const routine = kind === 'routine' ? findRoutine(id) : null;
  const exercise = kind === 'exercise' ? store.activeDocs('exercises').find((e) => e.id === id) : null;
  const data = routine
    ? { trainingId: routine.id, activity: routine.name, mode: routine.mode, moves: routine.moves ?? [], sets: routine.mode === 'sets' ? 1 : null, extra: {} }
    : { exerciseId: exercise.id, activity: exerciseName(exercise), durationSec: (exercise.defaultMinutes ?? 30) * 60, moves: [{ exerciseId: exercise.id, name: exerciseName(exercise), metrics: {} }] };
  const added = store.addEvent('workout', data, { date, source: 'manual' });
  openRecordEdit(added.id);
}

const analysisRows = () => collectRows(workoutEvents(), store.activeDocs('exercises'), allRoutines());
const analysisCards = (rows) => store.profile.analysisCards ?? defaultCards(rows);
const round = (value) => Math.round(value * 10) / 10;
const metricLabel = (metric) => (['minutes', 'sets', 'reps'].includes(metric) ? t(`an.metric.${metric}`) : metric);
const shortDate = (date) => parseDateKey(date).toLocaleDateString(lang, { month: 'numeric', day: 'numeric' });

function sourceOptions(card) {
  const value = `${card.source.kind}:${card.source.id ?? ''}`;
  const option = (key, label) => `<option value="${esc(key)}"${key === value ? ' selected' : ''}>${esc(label)}</option>`;
  const tags = [...new Set([...store.activeDocs('exercises').flatMap((e) => e.tags ?? []), ...allRoutines().flatMap((r) => r.tags ?? [])])];
  return [
    option('all:', t('an.all')),
    ...exerciseTree(store.activeDocs('exercises')).map(({ exercise }) => option(`exercise:${exercise.id}`, exerciseName(exercise))),
    ...allRoutines().map((routine) => option(`routine:${routine.id}`, routine.name)),
    ...tags.map((tag) => option(`tag:${tag}`, tagLabel(tag, lang))),
  ].join('');
}

const selectControl = (card, field, label, options) => `<label class="an-select"><span class="sr-only">${label}</span><select data-card="${esc(card.id)}" data-field="${field}" aria-label="${label}">${options}</select></label>`;

const optionList = (values, active, labelOf) => values.map((value) => `<option value="${esc(value)}"${value === active ? ' selected' : ''}>${esc(labelOf(value))}</option>`).join('');

// 折線和長條都是單一數列，用同一個主色；點圖上的點或長條會顯示該筆數值
function chartPoints(series) {
  const peak = Math.max(...series.map((point) => point.value), 0) || 1;
  const low = Math.min(...series.map((point) => point.value), 0);
  return series.map((point, i) => ({
    ...point,
    x: series.length === 1 ? 150 : 12 + (i * 276) / (series.length - 1),
    y: 100 - ((point.value - low) / (peak - low || 1)) * 88,
  }));
}

function lineChart(series) {
  const points = chartPoints(series);
  const path = points.map((p) => `${p.x},${p.y}`).join(' ');
  const hits = points.map((p) => `<circle cx="${p.x}" cy="${p.y}" r="14" class="an-hit" data-action="an-point" data-label="${shortDate(p.date)} ${round(p.value)}"></circle><circle cx="${p.x}" cy="${p.y}" r="4" class="an-dot"></circle>`).join('');
  return `<svg class="an-chart" viewBox="0 0 300 112" role="img" aria-label="${t('an.view.line')}">
    <line x1="0" y1="104" x2="300" y2="104" class="an-axis"></line>
    <polyline points="${path}" class="an-line"></polyline>${hits}
  </svg>`;
}

function barChart(series) {
  const peak = Math.max(...series.map((point) => point.value), 0) || 1;
  const gap = 2;
  const width = Math.max(2, (300 - gap * (series.length - 1)) / series.length);
  const bars = series.map((point, i) => {
    const height = Math.max(2, (point.value / peak) * 96);
    return `<rect x="${i * (width + gap)}" y="${104 - height}" width="${width}" height="${height}" rx="4" class="an-bar" data-action="an-point" data-label="${shortDate(point.date)} ${round(point.value)}"></rect>`;
  }).join('');
  return `<svg class="an-chart" viewBox="0 0 300 112" role="img" aria-label="${t('an.view.bar')}"><line x1="0" y1="104" x2="300" y2="104" class="an-axis"></line>${bars}</svg>`;
}

const tableView = (series) => `<div class="an-table">${[...series].reverse().map((point) => `<div class="stat-row"><span>${shortDate(point.date)}</span><strong>${round(point.value)}</strong></div>`).join('')}</div>`;

function tagView(rows, metric) {
  const groups = byTag(rows, metric);
  if (!groups.length) return `<p class="muted">${t('an.noTags')}</p>`;
  const peak = groups[0].value || 1;
  return groups.map((group) => `<div class="week-row"><span>${esc(tagLabel(group.tag, lang))}</span>
    <div class="bar-track"><div class="bar-fill" style="width:${(group.value / peak) * 100}%"></div></div>
    <small>${round(group.value)}</small></div>`).join('');
}

function analysisCard(card, allRows) {
  const today = dateKey();
  const rows = filterRows(allRows, card.source, card.range, today);
  const metrics = [...new Set([card.metric, ...metricNames(allRows)])];
  const value = headline(rows, card.metric, card.calc);
  const series = dailySeries(rows, card.metric, card.calc);
  const views = { line: lineChart, bar: barChart, table: tableView };
  let body = '';
  if (card.view === 'tags') body = tagView(rows, card.metric);
  else if (series.length && views[card.view]) body = views[card.view](series);
  return `<article class="an-card">
    <div class="an-controls">
      ${selectControl(card, 'source', t('an.tag'), sourceOptions(card))}
      ${selectControl(card, 'metric', t('an.value'), optionList(metrics, card.metric, metricLabel))}
      ${selectControl(card, 'calc', t('an.value'), optionList(CALCS, card.calc, (v) => t(`an.calc.${v}`)))}
      ${selectControl(card, 'range', t('an.date'), optionList(Object.keys(RANGES), card.range, (v) => t(`an.range.${v}`)))}
      ${selectControl(card, 'view', t('an.view.number'), optionList(VIEWS, card.view, (v) => t(`an.view.${v}`)))}
    </div>
    <div class="an-value">${value === null ? `<span class="muted">${t('an.noData')}</span>` : round(value)}</div>
    <p class="muted an-point"></p>
    ${body}
    <button class="plain" data-action="an-remove" data-id="${esc(card.id)}">${t('an.remove')}</button>
  </article>`;
}

function renderAnalysis() {
  const rows = analysisRows();
  const cards = analysisCards(rows);
  return `<section class="stack">
    <div class="row"><h2 style="margin:0">${t('an.title')}</h2><button class="plain" data-action="an-add">＋ ${t('an.add')}</button></div>
    ${cards.map((card) => analysisCard(card, rows)).join('') || `<p class="muted">${t('an.empty')}</p>`}
  </section>`;
}

function saveAnalysisCards(cards) {
  store.saveProfile({ analysisCards: cards });
}

function renderRecords() {
  const events = workoutEvents();
  if (!events.length) return `${renderCalendar(events)}<section class="card"><p class="muted">${t('train.noRecords')}</p></section>`;
  const recent = [...events].sort((a, b) => b.occurredAt - a.occurredAt).slice(0, 20).map((e) => `<button class="plan-row" data-action="rec-edit" data-id="${esc(e.id)}"><span>${esc(e.data.activity)}</span><span class="muted">${shortDate(e.date)}</span></button>`).join('');
  return `${renderCalendar(events)}${renderAnalysis()}<section class="card"><h2>${t('workout.recent')}</h2>${recent}</section>`;
}

function renderHome() {
  document.documentElement.lang = lang;
  document.querySelectorAll('.train-tabs button').forEach((button) => {
    button.classList.toggle('active', button.dataset.tab === state.tab);
    button.textContent = t(`train.${button.dataset.tab}`);
  });
  const screens = { exercises: renderExercises, routines: renderRoutines, schedules: renderSchedules, records: renderRecords };
  const title = isTimerApp && state.tab === 'routines' ? t('timer.appTitle') : t(`train.${state.tab}`);
  view.innerHTML = `<header class="topbar"><h1>${title}</h1>
      <div class="chips">${isTimerApp ? '' : `<a class="plain" href="../Health/">${t('timer.health')}</a>`}<button class="icon-btn" data-action="settings" aria-label="${t('train.settings')}">⚙</button></div>
    </header>${screens[state.tab]()}`;
}

function openSettings() {
  const settings = getSettings();
  const check = (key, label) => `<label class="check"><input type="checkbox" data-setting="${key}"${settings[key] ? ' checked' : ''}> ${label}</label>`;
  const voices = window.speechSynthesis?.getVoices() ?? [];
  const voiceOptions = voices.map((v) => `<option value="${esc(v.voiceURI)}"${v.voiceURI === settings.voiceURI ? ' selected' : ''}>${esc(v.name)}</option>`).join('');
  const account = store.user
    ? `<p class="muted">${t('account.signedIn', { email: esc(store.user.email) })}</p>`
    : `<p class="muted">${t('account.signedOut')}</p>${store.canSignIn ? `<button class="btn wide" data-action="sign-in">${t('account.signIn')}</button>` : ''}`;
  openPanel(`<div class="stack">
    ${panelHeader(t('train.settings'))}
    <h2>${t('set.sound')}</h2>
    ${check('sound', t('set.soundOn'))}${check('soundStart', t('set.soundStart'))}${check('soundCountdown', t('set.soundCountdown'))}${check('soundFinish', t('set.soundFinish'))}
    ${numberField('countdownSec', t('set.countdown'), settings.countdownSec, 'min="0" max="10" data-setting="countdownSec"')}
    ${check('pulse', t('set.pulse'))}
    <h2>${t('set.voice')}</h2>
    ${check('tts', t('set.tts'))}
    <label class="field"><span>${t('set.rate')}</span><input type="range" min="0.5" max="2" step="0.1" value="${settings.rate}" data-setting="rate"></label>
    ${voices.length ? `<label class="field"><span>${t('set.voiceName')}</span><select data-setting="voiceURI"><option value="">${t('set.defaultVoice')}</option>${voiceOptions}</select></label>` : ''}
    <button class="btn wide" data-action="voice-test">${t('set.test')}</button>
    <h2>${t('set.training')}</h2>
    ${check('autoLog', t('timer.autoLog'))}
    <h2>${t('set.language')}</h2>
    <div class="chips">${LANGUAGES.map((l) => `<button class="chip${l.value === lang ? ' active' : ''}" data-action="lang-set" data-value="${l.value}">${l.label}</button>`).join('')}</div>
    <h2>${t('account.title')}</h2>
    ${account}
  </div>`);
}

function openNewRoutine() {
  const templates = Object.keys(TEMPLATES).map((key) => `<button class="chip" data-action="template" data-key="${key}">＋ ${t(`template.${key}`)}</button>`).join('');
  openPanel(`<div class="stack">
    ${panelHeader(t('timer.new'))}
    <button class="pick-row" data-action="new-blank"><span>${t('train.blank')}</span><span class="muted">${t('train.blankHint')}</span></button>
    ${isTimerApp ? '' : `<button class="pick-row" data-action="new-sets"><span>${t('train.setsOnly')}</span><span class="muted">${t('train.setsHint')}</span></button>`}
    <h2>${t('timer.examples')}</h2>
    <div class="chips">${templates}</div>
  </div>`);
}

// ---- 動作 ----

function openExercise(id) {
  const list = store.activeDocs('exercises');
  const exercise = id ? list.find((e) => e.id === id) : null;
  state.draft = { id: exercise?.id, name: exercise?.name ?? '', nameEn: exercise?.nameEn ?? '', parentId: exercise?.parentId ?? null, defaultMinutes: exercise?.defaultMinutes ?? null, tags: exercise?.tags ?? [], muscles: exercise?.muscles ?? presetMuscles(exercise?.name ?? '') };
  renderExerciseForm();
}

function renderExerciseForm() {
  const list = store.activeDocs('exercises');
  const { id } = state.draft;
  state.tagTarget = null;
  const blocked = id ? new Set([id, ...descendantIds(list, id)]) : new Set();
  const parents = exerciseTree(list).filter((row) => !blocked.has(row.exercise.id)).map(({ exercise: option, depth }) => (
    `<option value="${esc(option.id)}"${option.id === state.draft.parentId ? ' selected' : ''}>${'　'.repeat(depth)}${esc(exerciseName(option))}</option>`
  )).join('');
  const records = workoutEvents().map((e) => e.data);
  openPanel(`<form class="stack" data-form="exercise">
    ${panelHeader(t(id ? 'exercise.edit' : 'exercise.new'))}
    <label class="field"><span>${t('exercise.name')}</span><input name="name" value="${esc(state.draft.name)}" required></label>
    <label class="field"><span>${t('exercise.nameEn')}</span><input name="nameEn" value="${esc(state.draft.nameEn)}"></label>
    ${numberField('defaultMinutes', t('exercise.defaultMinutes'), state.draft.defaultMinutes, 'min="1"')}
    <label class="field"><span>${t('exercise.parent')}</span><select name="parentId"><option value="">${t('exercise.noParent')}</option>${parents}</select></label>
    <h2>${t('exercise.muscles')}</h2>
    ${state.draft.muscles.map((item, i) => `<div class="value-row">
      ${numberField(`muscle-${i}`, t('exercise.percent', { name: t(`muscle.${item.muscle}`) }), item.percent, 'min="0" max="100"')}
      <button type="button" class="icon-btn" data-action="muscle-remove" data-index="${i}" aria-label="${t('common.delete')}">✕</button>
    </div>`).join('')}
    <div class="amount">
      <select id="muscle-pick" aria-label="${t('exercise.muscles')}">${MUSCLES.filter((m) => !state.draft.muscles.some((item) => item.muscle === m.id)).map((m) => `<option value="${m.id}">${t(`muscle.${m.id}`)}</option>`).join('')}</select>
      <button type="button" class="btn" data-action="muscle-add">${t('exercise.addMuscle')}</button>
    </div>
    <h2>${t('exercise.tags')}</h2>
    <div class="editor-tags">${state.draft.tags.map((tag) => `<span class="tag">${esc(tagLabel(tag, lang))}</span>`).join('')}
      <button type="button" class="plain" data-action="exercise-tags">${t('exercise.addTag')}</button></div>
    ${id ? `<div class="stat-row"><span>${t('exercise.total')}</span><strong>${exerciseTotal(records, list, id)}</strong></div>` : ''}
    <button class="btn primary wide">${t('common.save')}</button>
    ${id ? `<button type="button" class="btn danger wide" data-action="exercise-delete">${t('common.delete')}</button><p class="muted">${t('exercise.deleteHint')}</p>` : ''}
  </form>`);
}

function readExerciseForm() {
  const values = new FormData(panel.querySelector('form'));
  state.draft.muscles = state.draft.muscles.map((item, i) => ({ ...item, percent: Number(values.get(`muscle-${i}`)) || 0 }));
  Object.assign(state.draft, { name: values.get('name'), nameEn: values.get('nameEn'), parentId: values.get('parentId') || null, defaultMinutes: optionalNumber(values.get('defaultMinutes')) });
}

function saveExercise(form) {
  readExerciseForm();
  store.saveDoc('exercises', { ...state.draft, name: state.draft.name.trim(), nameEn: state.draft.nameEn.trim() });
  panel.close();
}

function deleteExercise() {
  const { id, parentId } = state.draft;
  const list = store.activeDocs('exercises');
  list.filter((e) => e.parentId === id).forEach((child) => store.saveDoc('exercises', { ...child, parentId: parentId ?? null }));
  store.saveDoc('exercises', { ...list.find((e) => e.id === id), deletedAt: Date.now() });
  panel.close();
}

// ---- 只記組數的課表 ----

function readMovesForm(form) {
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
  const list = store.activeDocs('exercises');
  const value = move.exerciseId ? exercisePath(list, move.exerciseId, exerciseName) || move.name : move.name;
  return `<div class="move-row">
    <div class="grid-2">
      <label class="field"><span>${t('training.moveName')}</span><input name="move-name-${i}" list="exercise-options" autocomplete="off" value="${esc(value)}" required></label>
      ${metricField(reps)}
    </div>
    <details><summary>${t('training.moreMetrics')}</summary><div class="grid-2">${more.map(metricField).join('')}</div></details>
  </div>`;
}

function renderMovesForm() {
  const { draft } = state;
  openPanel(`<form class="stack" data-form="moves">
    ${panelHeader(t(draft.id ? 'common.edit' : 'train.setsOnly'))}
    <label class="field"><span>${t('training.name')}</span><input name="name" value="${esc(draft.name)}" required></label>
    <h2>${t('training.mode')}</h2>
    ${chipGroup('mode', MODES.map((value) => ({ value, label: t(`mode.${value}`) })), draft.mode)}
    <h2>${t('training.moves')}</h2>
    <datalist id="exercise-options">${exerciseOptions()}</datalist>
    ${draft.moves.map(moveFields).join('')}
    <button type="button" class="btn wide" data-action="move-add">＋ ${t('training.addMove')}</button>
    <label class="check"><input type="checkbox" name="trackRest"${draft.trackRest ? ' checked' : ''}> ${t('training.trackRest')}</label>
    <button class="btn primary wide">${t('common.save')}</button>
    ${draft.id ? `<button type="button" class="btn wide" data-action="moves-to-flow">${t('train.addTimer')}</button>
    <button type="button" class="btn danger wide" data-action="moves-delete">${t('common.delete')}</button>` : ''}
  </form>`);
}

function saveMoves(form) {
  readMovesForm(form);
  const moves = state.draft.moves.map((move) => ({ ...move, ...linkExercise(store, move.name, exerciseName) }));
  store.saveDoc('trainings', { ...state.draft, moves });
  panel.close();
}

// ---- 排程 ----

function openSchedule(id) {
  const trainings = allRoutines();
  if (!trainings.length) { toast(t('train.noRoutineForSchedule')); return; }
  const schedule = id ? store.activeDocs('schedules').find((s) => s.id === id) : null;
  state.draft = schedule ? { ...schedule } : { trainingId: trainings[0].id };
  const days = WEEK_ORDER.map((d) => `<button type="button" class="chip${schedule?.weekdays?.includes(d) ? ' active' : ''}" data-action="chip-toggle" data-value="${d}">${weekdayName(d)}</button>`).join('');
  openPanel(`<form class="stack" data-form="schedule">
    ${panelHeader(t(id ? 'common.edit' : 'train.newSchedule'))}
    <h2>${t('schedule.training')}</h2>
    ${chipGroup('trainingId', trainings.map((x) => ({ value: x.id, label: x.name })), state.draft.trainingId)}
    <label class="field"><span>${t('schedule.start')}</span><input type="date" name="startDate" value="${schedule?.startDate ?? dateKey()}" required></label>
    ${numberField('days', `${t('schedule.days')}（${t('schedule.daysHint')}）`, schedule?.days, 'min="1"')}
    <h2>${t('schedule.weekdays')}</h2>
    <p class="muted">${t('schedule.weekdaysHint')}</p>
    <div class="chips" data-chip-list="weekdays">${days}</div>
    ${numberField('targetSets', `${t('schedule.target')}（${t('schedule.targetHint')}）`, schedule?.targetSets, 'min="1"')}
    <button class="btn primary wide">${t('common.save')}</button>
    ${id ? `<button type="button" class="btn danger wide" data-action="schedule-delete">${t('common.delete')}</button>` : ''}
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
  panel.close();
}

// ---- 編輯器：整份課表一次看完，用縮排表示層級 ----

function editorRows(list, depth = 0) {
  return list.flatMap((step, index) => {
    const row = { step, list, index, depth };
    return step.type === 'loop' ? [row, ...editorRows(step.children, depth + 1)] : [row];
  });
}

function stepHtml(step) {
  const selected = state.selected === step ? ' selected' : '';
  if (step.type === 'loop') {
    return `<div class="flow-item flow-loop${selected}" data-step="${esc(step.id)}">
      <button class="flow-row loop-title" data-row="${esc(step.id)}">${t('step.loopTimes', { n: step.iterations })}</button>
      <div class="flow-group">${step.children.map(stepHtml).join('')}</div>
    </div>`;
  }
  const amount = step.type === 'reps' ? t('step.times', { n: step.count }) : formatDuration(step.duration, t);
  return `<div class="flow-item" data-step="${esc(step.id)}">
    <button class="flow-row tone-bar-${step.color}" data-row="${esc(step.id)}"><span>${esc(stepName(step))}</span><span class="muted">${amount}</span></button>
  </div>`;
}

function renderEditor() {
  const { routine } = state;
  const line = flattenFlow(routine.flow).filter((s) => s.duration).map((s) => `<i class="tone-${s.color}" style="flex:${s.duration}"></i>`).join('');
  sheet.innerHTML = `<div class="editor">
    <header class="row editor-head">
      <button class="plain" data-action="close">${t('editor.cancel')}</button>
      <input class="editor-title" id="routine-name" value="${esc(routine.name)}" placeholder="${t('editor.name')}" aria-label="${t('editor.name')}">
      <button class="plain strong" data-action="save">${t('common.save')}</button>
    </header>
    <p class="muted editor-total">${formatDuration(flowDuration(routine.flow), t)}</p>
    <div class="run-line">${line}</div>
    <div class="flow-list">
      <div class="flow-group root">${routine.flow.map(stepHtml).join('')}</div>
      ${routine.flow.length ? '' : `<p class="muted">${t('editor.empty')}</p>`}
    </div>
    <footer class="editor-foot">
      <button class="btn primary wide" data-action="add">${state.selected?.type === 'loop' ? t('editor.addInLoop') : t('editor.add')}</button>
      <div class="editor-tags">
        ${(routine.tags ?? []).map((tag) => `<span class="tag">${esc(tagLabel(tag, lang))}</span>`).join('')}
        <button class="plain" data-action="tags-open">${t('tags.add')}</button>
      </div>
      <div class="editor-tools">
        <button class="plain" data-action="pin">${t(routine.pinned ? 'editor.unpin' : 'editor.pin')}</button>
        ${routine.id ? `<button class="plain" data-action="duplicate">${t('editor.duplicate')}</button>
        <button class="plain danger" data-action="delete">${t('common.delete')}</button>` : ''}
      </div>
    </footer>
  </div>`;
  if (!sheet.open) { sheet.showModal(); sheet.focus(); }
  attachSortable();
}

function readEditorFields() {
  state.routine.name = sheet.querySelector('#routine-name')?.value ?? state.routine.name;
}

function openPanel(html) {
  panel.innerHTML = html;
  if (!panel.open) { panel.showModal(); panel.focus(); }
}

function closeEditor() {
  if (panel.open) panel.close();
  if (sheet.open) sheet.close();
}

function openRoutine(routine) {
  state.routine = routine;
  state.original = JSON.stringify(routine);
  state.selected = null;
  renderEditor();
}

// 有修改時先確認，避免取消或系統返回時內容不見
function requestClose() {
  readEditorFields();
  if (JSON.stringify(state.routine) === state.original) { closeEditor(); return; }
  openPanel(`<div class="stack">
    <h2>${t('editor.discardTitle')}</h2>
    <button class="btn danger wide" data-action="discard">${t('editor.discard')}</button>
    <button class="btn wide" data-action="back">${t('editor.keepEditing')}</button>
  </div>`);
}

function renderPicker() {
  const presets = [
    { key: 'prepare', label: t('step.prepare'), props: { label: t('step.prepare'), duration: 10, color: 'blue', rest: true } },
    { key: 'hang', label: t('template.hang'), props: { label: t('template.hang'), duration: 7, color: 'red' } },
    { key: 'rest', label: t('step.rest'), props: { label: t('step.rest'), duration: 60, color: 'green', rest: true } },
  ];
  state.presets = presets;
  openPanel(`<div class="stack">
    <div class="row"><h2 style="margin:0">${t('editor.whatToAdd')}</h2><button class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
    <div class="picker">
      <button class="btn" data-action="pick" data-type="timer">${t('step.timer')}</button>
      <button class="btn" data-action="pick" data-type="reps">${t('step.reps')}</button>
      <button class="btn" data-action="pick" data-type="loop">${t('step.loop')}</button>
    </div>
    <h2>${t('editor.common')}</h2>
    <div class="chips">${presets.map((p) => `<button class="chip tone-chip-${p.props.color}" data-action="preset" data-key="${p.key}">${p.label}</button>`).join('')}</div>
  </div>`);
}

function exerciseOptions() {
  const list = store.activeDocs('exercises');
  return exerciseTree(list).map(({ exercise }) => `<option value="${esc(exercisePath(list, exercise.id))}"></option>`).join('');
}

function renderStep() {
  const { step } = state;
  const isNew = state.adding;
  if (step.type === 'loop') {
    openPanel(`<form class="stack" data-form="step">
      <div class="row"><h2 style="margin:0">${t('step.loop')}</h2><button type="button" class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
      <label class="field"><span>${t('step.iterations')}</span><input type="number" inputmode="numeric" min="1" name="iterations" value="${step.iterations}" required></label>
      <button class="btn primary wide">${t('common.save')}</button>
      ${isNew ? '' : `<div class="grid-2"><button type="button" class="btn" data-action="copy">${t('editor.copy')}</button><button type="button" class="btn danger" data-action="remove">${t('common.delete')}</button></div>`}
    </form>`);
    return;
  }
  const amount = step.type === 'reps'
    ? `<label class="field"><span>${t('metric.reps')}</span><input type="number" inputmode="numeric" min="1" name="count" value="${step.count}" required></label>`
    : `<label class="field"><span>${t('step.seconds')}</span><input type="number" inputmode="numeric" min="1" name="duration" value="${step.duration}" required></label>`;
  const swatches = COLORS.map((color) => `<button type="button" class="swatch tone-${color}${step.color === color ? ' active' : ''}" data-action="color" data-color="${color}" aria-label="${color}"></button>`).join('');
  const names = commonNames(step.type).map((item, i) => `<button type="button" class="chip small" data-action="name-pick" data-index="${i}">${esc(item.label)}</button>`).join('');
  openPanel(`<form class="stack" data-form="step">
    <div class="row"><h2 style="margin:0">${t(step.type === 'reps' ? 'step.reps' : 'step.timer')}</h2><button type="button" class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
    <div class="grid-2">
      <label class="field"><span>${t('step.name')}</span><input name="label" list="exercise-options" autocomplete="off" value="${esc(step.label)}"></label>
      ${amount}
    </div>
    ${names ? `<div class="chips" aria-label="${t('editor.names')}">${names}</div>` : ''}
    <datalist id="exercise-options">${exerciseOptions()}</datalist>
    ${step.type === 'reps' ? numberField('estimate', t('editor.estimate'), step.estimate, 'min="1"') : ''}
    <div class="swatches">${swatches}</div>
    <h2>${t('editor.metrics')}</h2>
    <div class="stack" id="metric-box">${metricBox()}</div>
    ${step.type === 'timer' ? `<label class="check"><input type="checkbox" name="rest"${step.rest ? ' checked' : ''}> ${t('editor.isRest')}</label>
    <label class="check"><input type="checkbox" name="skipOnLast"${step.skipOnLast ? ' checked' : ''}> ${t('editor.skipOnLast')}</label>` : ''}
    ${isNew ? '' : `<div class="grid-2"><button type="button" class="btn" data-action="copy">${t('editor.copy')}</button><button type="button" class="btn danger" data-action="remove">${t('common.delete')}</button></div>`}
    <div class="panel-save"><button class="btn primary wide">${t('common.save')}</button></div>
  </form>`);
  panel.querySelector('#metric-name').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') { event.preventDefault(); addMetric(); }
  });
}

// 所有課表用過的步驟，最新的在前面；舊版的預設名稱放在最後
function usedSteps() {
  const steps = [];
  for (const routine of [...allRoutines()].reverse()) mapFlow(normalizeFlow(routine.flow ?? []), (step) => { steps.push(step); return step; });
  return steps;
}

function commonNames(type) {
  const defaults = type === 'reps'
    ? [{ label: t('template.pullUp'), color: 'indigo' }, { label: t('template.squat'), color: 'pink' }]
    : [{ label: t('step.prepare'), color: 'blue', duration: 10, rest: true }, { label: t('template.hang'), color: 'orange', duration: 7 }, { label: t('step.rest'), color: 'green', duration: 60, rest: true }];
  const seen = new Set();
  state.names = [...usedSteps().filter((step) => step.type === type), ...defaults]
    .filter((item) => item.label && !seen.has(item.label) && seen.add(item.label))
    .slice(0, 8);
  return state.names;
}

function commonMetrics() {
  const names = [...usedSteps().flatMap((step) => step.metrics.map((metric) => metric.name)), t('log.weight'), t('log.reps')];
  const chosen = new Set(state.step.metrics.map((metric) => metric.name));
  return [...new Set(names)].filter((name) => !chosen.has(name)).slice(0, 6);
}

function metricBox() {
  const chosen = state.step.metrics.map((metric, i) => `<button type="button" class="chip active" data-action="metric-remove" data-index="${i}">${esc(metric.name)} ✕</button>`).join('');
  const quick = commonMetrics().map((name) => `<button type="button" class="chip" data-action="metric-quick" data-name="${esc(name)}">＋ ${esc(name)}</button>`).join('');
  return `${chosen ? `<div class="chips">${chosen}</div>` : ''}
    <div class="chips">${quick}</div>
    <div class="amount"><input id="metric-name" autocomplete="off" placeholder="${t('editor.metricPlaceholder')}" aria-label="${t('editor.metricPlaceholder')}">
    <button type="button" class="btn" data-action="metric-add">${t('editor.addMetric')}</button></div>`;
}

function refreshMetrics() {
  const box = panel.querySelector('#metric-box');
  box.innerHTML = metricBox();
  box.querySelector('#metric-name').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') { event.preventDefault(); addMetric(); }
  });
}

function addMetric(name = panel.querySelector('#metric-name').value.trim()) {
  if (name && !state.step.metrics.some((metric) => metric.name === name)) state.step.metrics.push({ name, type: 'number' });
  refreshMetrics();
}

function renderTags() {
  const selected = new Set(tagHost().tags ?? []);
  const groups = ['history', ...Object.keys(TAG_GROUPS)];
  const list = state.tagGroup === 'history' ? recentTags() : TAG_GROUPS[state.tagGroup];
  openPanel(`<div class="stack">
    <div class="row"><h2 style="margin:0">${t('tags.title')}</h2><button class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
    <div class="chips">${groups.map((group) => `<button class="chip small${group === state.tagGroup ? ' active' : ''}" data-action="tag-group" data-group="${group}">${t(`tagGroup.${group}`)}</button>`).join('')}</div>
    <div class="tag-list chips">${list.map((tag) => `<button class="chip${selected.has(tag) ? ' active' : ''}" data-action="tag-toggle" data-tag="${esc(tag)}">${esc(tagLabel(tag, lang))}</button>`).join('') || `<p class="muted">${t('tags.none')}</p>`}</div>
    <div class="amount"><input id="tag-name" autocomplete="off" placeholder="${t('tags.custom')}" aria-label="${t('tags.custom')}"><button class="btn" data-action="tag-add">${t('tags.addCustom')}</button></div>
  </div>`);
}

const tagHost = () => (state.tagTarget === 'exercise' ? state.draft : state.routine);

function toggleTag(tag) {
  const host = tagHost();
  const tags = new Set(host.tags ?? []);
  if (tags.has(tag)) tags.delete(tag);
  else { tags.add(tag); rememberTag(tag); }
  host.tags = [...tags];
}

function refreshAfterTag() {
  renderTags();
  if (state.tagTarget !== 'exercise') renderEditor();
}

function addStep(step) {
  const { routine, selected } = state;
  const target = selected?.type === 'loop' ? selected.children : routine.flow;
  state.step = step;
  state.stepList = target;
  state.adding = true;
  renderStep();
}

function saveStep(form) {
  const { step } = state;
  const values = new FormData(form);
  if (step.type === 'loop') step.iterations = Number(values.get('iterations'));
  else {
    step.label = values.get('label').trim();
    if (step.type === 'reps') {
      step.count = Number(values.get('count'));
      step.estimate = optionalNumber(values.get('estimate'));
    } else {
      step.duration = Number(values.get('duration'));
      step.rest = values.get('rest') === 'on';
      step.skipOnLast = values.get('skipOnLast') === 'on';
    }
  }
  if (state.adding) state.stepList.push(step);
  else if (step.type === 'loop') findRow(step.id).step.iterations = step.iterations;
  else Object.assign(findRow(step.id).step, step);
  state.adding = false;
  panel.close();
  renderEditor();
}

function findRow(id) {
  return editorRows(state.routine.flow).find((row) => row.step.id === id);
}

// 拖曳沿用舊版的 SortableJS：每個循環都是一個拖曳區，可以調整順序、拖進或拖出循環
function readGroup(group, steps) {
  return [...group.children].filter((el) => el.classList.contains('flow-item')).map((el) => {
    const step = steps.get(el.dataset.step);
    if (step.type === 'loop') step.children = readGroup(el.querySelector(':scope > .flow-group'), steps);
    return step;
  });
}

function attachSortable() {
  if (!window.Sortable) return;
  sheet.querySelectorAll('.flow-group').forEach((group) => {
    new Sortable(group, {
      group: 'flow',
      draggable: '.flow-item',
      animation: 150,
      emptyInsertThreshold: 50,
      ghostClass: 'flow-ghost',
      dragClass: 'flow-drag',
      delay: 150,
      delayOnTouchOnly: true,
      fallbackTolerance: 10,
      forceFallback: true,
      supportPointer: false,
      scroll: true,
      bubbleScroll: true,
      swapThreshold: 0.65,
      onEnd: () => {
        const steps = new Map(editorRows(state.routine.flow).map(({ step }) => [step.id, step]));
        readEditorFields();
        state.routine.flow = readGroup(sheet.querySelector('.flow-group.root'), steps);
        renderEditor();
      },
    });
  });
}

function linkFlow(flow) {
  return mapFlow(flow, (step) => {
    if (step.rest || !step.label) return step;
    const { exerciseId, name } = linkExercise(store, step.label);
    return { ...step, exerciseId, label: name };
  });
}

// 舊課表的動作只有名稱，載入時連到動作庫
function linkExistingRoutines() {
  for (const routine of allRoutines()) {
    if (routine.moves?.some((move) => !move.exerciseId && move.name)) {
      const moves = routine.moves.map((move) => (move.name ? { ...move, ...linkExercise(store, move.name, exerciseName) } : move));
      store.saveDoc('trainings', { ...routine, moves });
    }
  }
}

const actions = {
  tab: ({ tab }) => { state.tab = tab; renderHome(); scrollTo(0, 0); },
  settings: openSettings,
  new: openNewRoutine,
  'new-blank': () => { panel.close(); openRoutine({ name: '', mode: 'sets', moves: [], trackRest: false, tags: [], flow: [] }); },
  'new-sets': () => { state.draft = { name: '', mode: 'sets', moves: [{ name: '', metrics: {} }], trackRest: false }; renderMovesForm(); },
  edit: ({ id }) => {
    const routine = structuredClone(findRoutine(id));
    if (!routine.flow?.length) { state.draft = routine; renderMovesForm(); return; }
    openRoutine({ ...routine, flow: normalizeFlow(routine.flow) });
  },
  'move-add': () => {
    readMovesForm(panel.querySelector('form'));
    state.draft.moves.push({ name: '', metrics: {} });
    renderMovesForm();
  },
  'moves-to-flow': () => {
    readMovesForm(panel.querySelector('form'));
    const routine = { ...state.draft, flow: [] };
    panel.close();
    openRoutine(routine);
  },
  'moves-delete': () => { store.saveDoc('trainings', { ...state.draft, deletedAt: Date.now() }); panel.close(); },
  'exercise-edit': ({ id }) => openExercise(id),
  'exercise-delete': deleteExercise,
  'schedule-edit': ({ id }) => openSchedule(id),
  'schedule-delete': () => { store.saveDoc('schedules', { ...state.draft, deletedAt: Date.now() }); panel.close(); },
  chip: ({ value }, target) => {
    const group = target.closest('[data-chip-group]');
    group.querySelectorAll('.chip').forEach((c) => c.classList.toggle('active', c === target));
    state.draft[group.dataset.chipGroup] = value;
  },
  'chip-toggle': (_, target) => target.classList.toggle('active'),
  'cycle-new': () => { state.cycle = { name: '', startDate: dateKey(), phases: [] }; renderCycleForm(); },
  'cycle-open': ({ id }) => openCycle(id),
  'cycle-edit': ({ id }) => { state.cycle = structuredClone(store.activeDocs('cycles').find((c) => c.id === id)); renderCycleForm(); },
  'cycle-template': () => {
    readCycleForm();
    state.cycle.phases = [['cycle.base', 4], ['cycle.strength', 4], ['cycle.power', 3], ['cycle.deload', 1]].map(([key, weeks]) => ({ ...newPhase(t(key)), weeks }));
    renderCycleForm();
  },
  'cycle-delete': () => {
    removeCycleSchedules(state.cycle.id);
    store.saveDoc('cycles', { ...state.cycle, deletedAt: Date.now() });
    state.cycle = null;
    panel.close();
  },
  'phase-add': () => { readCycleForm(); state.phase = newPhase(); renderPhaseForm(); },
  'phase-edit': ({ index }) => { readCycleForm(); state.phase = structuredClone(state.cycle.phases[Number(index)]); renderPhaseForm(); },
  'phase-delete': () => {
    state.cycle.phases = state.cycle.phases.filter((item) => item.id !== state.phase.id);
    state.phase = null;
    renderCycleForm();
  },
  'phase-routine': ({ id }, target) => {
    const ids = new Set(state.phase.trainingIds);
    if (ids.has(id)) ids.delete(id); else ids.add(id);
    state.phase.trainingIds = [...ids];
    target.classList.toggle('active');
  },
  'phase-day': ({ value }, target) => {
    const day = Number(value);
    const days = new Set(state.phase.weekdays);
    if (days.has(day)) days.delete(day); else days.add(day);
    state.phase.weekdays = [...days];
    target.classList.toggle('active');
  },
  'planned-add': () => {
    readPhaseForm();
    const metric = panel.querySelector('#planned-metric').value;
    const value = optionalNumber(panel.querySelector('#planned-value').value);
    if (value === null) return;
    state.phase.planned = [...state.phase.planned.filter((item) => item.metric !== metric), { metric, value }];
    renderPhaseForm();
  },
  'planned-remove': ({ index }) => {
    readPhaseForm();
    state.phase.planned.splice(Number(index), 1);
    renderPhaseForm();
  },
  'rec-range': ({ range }) => { state.recRange = range; renderHome(); },
  'rec-move': ({ delta }) => moveRecords(Number(delta)),
  'rec-day': ({ date }) => { if (!state.swiped) openDay(date); },
  'rec-add': openRecordAdd,
  'rec-pick': ({ kind, id }) => addRecord(kind, id),
  'rec-edit': ({ id }) => openRecordEdit(id),
  'rec-delete': () => {
    const { date } = store.events.get(state.draft.id);
    store.deleteEvent(state.draft.id);
    openDay(date);
  },
  close: requestClose,
  discard: closeEditor,
  back: () => {
    state.adding = false;
    if (state.tagTarget === 'exercise') { renderExerciseForm(); return; }
    if (state.phase) { state.phase = null; renderCycleForm(); return; }
    state.cycle = null;
    panel.close();
  },
  save: () => {
    readEditorFields();
    const name = state.routine.name.trim();
    if (!name) { toast(t('editor.needName')); return; }
    if (!flattenFlow(state.routine.flow).length) { toast(t('editor.needStep')); return; }
    store.saveDoc('trainings', { ...state.routine, name, flow: linkFlow(state.routine.flow) });
    closeEditor();
  },
  delete: () => { store.saveDoc('trainings', { ...state.routine, deletedAt: Date.now() }); closeEditor(); },
  add: () => { readEditorFields(); renderPicker(); },
  pick: ({ type }) => addStep(newStep(type)),
  preset: ({ key }) => addStep(newStep('timer', state.presets.find((p) => p.key === key).props)),
  'name-pick': ({ index }) => {
    const item = state.names[Number(index)];
    const form = panel.querySelector('form');
    form.elements.label.value = item.label;
    if (item.count && form.elements.count) form.elements.count.value = item.count;
    if (item.duration && form.elements.duration) form.elements.duration.value = item.duration;
    if (form.elements.rest) form.elements.rest.checked = !!item.rest;
    if (item.color) actions.color({ color: item.color }, panel.querySelector(`.swatch.tone-${item.color}`));
  },
  'metric-add': () => addMetric(),
  'metric-quick': ({ name }) => addMetric(name),
  'metric-remove': ({ index }) => { state.step.metrics.splice(Number(index), 1); refreshMetrics(); },
  'muscle-add': () => {
    readExerciseForm();
    const muscle = panel.querySelector('#muscle-pick').value;
    if (muscle && !state.draft.muscles.some((item) => item.muscle === muscle)) state.draft.muscles.push({ muscle, percent: 0 });
    renderExerciseForm();
  },
  'muscle-remove': ({ index }) => {
    readExerciseForm();
    state.draft.muscles.splice(Number(index), 1);
    renderExerciseForm();
  },
  'exercise-tags': () => {
    readExerciseForm();
    state.tagTarget = 'exercise';
    state.tagGroup = recentTags().length ? 'history' : 'zones';
    renderTags();
  },
  'an-point': ({ label }, target) => {
    target.closest('.an-card').querySelector('.an-point').textContent = label;
  },
  'an-remove': ({ id }) => {
    saveAnalysisCards(analysisCards(analysisRows()).filter((card) => card.id !== id));
  },
  'an-add': () => {
    const cards = analysisCards(analysisRows());
    saveAnalysisCards([...cards, { id: crypto.randomUUID(), source: { kind: 'all' }, metric: 'minutes', calc: 'sum', range: '4w', view: 'number' }]);
  },
  'tags-open': () => { readEditorFields(); state.tagGroup = recentTags().length ? 'history' : 'fitness'; renderTags(); },
  'tag-group': ({ group }) => { state.tagGroup = group; renderTags(); },
  'tag-toggle': ({ tag }) => { toggleTag(tag); refreshAfterTag(); },
  'tag-add': () => {
    const tag = panel.querySelector('#tag-name').value.trim();
    if (tag && !tagHost().tags?.includes(tag)) toggleTag(tag);
    refreshAfterTag();
  },
  pin: () => { readEditorFields(); state.routine.pinned = !state.routine.pinned; renderEditor(); },
  duplicate: () => {
    readEditorFields();
    const { id, updatedAt, ...copy } = state.routine;
    store.saveDoc('trainings', { ...copy, name: `${copy.name}${t('editor.copySuffix')}`, pinned: false });
    closeEditor();
  },
  color: ({ color }, target) => {
    state.step.color = color;
    panel.querySelectorAll('.swatch').forEach((el) => el.classList.toggle('active', el === target));
  },
  copy: () => {
    const fresh = (step) => ({ ...step, id: crypto.randomUUID(), ...(step.children ? { children: step.children.map(fresh) } : {}) });
    const { list, step, index } = findRow(state.step.id);
    list.splice(index + 1, 0, fresh(structuredClone(step)));
    panel.close();
    renderEditor();
  },
  remove: () => {
    const { list, index } = findRow(state.step.id);
    list.splice(index, 1);
    if (state.selected?.id === state.step.id) state.selected = null;
    panel.close();
    renderEditor();
  },
  template: ({ key }) => {
    const template = TEMPLATES[key](t);
    if (routines().some((routine) => routine.name === template.name)) { toast(t('timer.exists')); return; }
    store.saveDoc('trainings', { mode: 'sets', moves: [], trackRest: false, ...template });
    if (panel.open) panel.close();
    toast(t('common.saved'));
  },
  start: ({ id }) => start(findRoutine(id)),
  'start-now': ({ id }) => { panel.close(); start(findRoutine(id)); },
  resume: () => { const session = savedSession(); start(findRoutine(session.trainingId), session); },
  'drop-session': () => { clearSession(); renderHome(); },
  'voice-test': () => {
    if (!getSettings().tts) setSetting('tts', true);
    speak(t('set.testText'));
    openSettings();
  },
  'lang-set': (_, target) => {
    setLang(target.dataset.value);
    store.saveProfile({ lang: target.dataset.value });
    panel.close();
    renderHome();
  },
  'sign-in': () => store.signIn().catch((error) => toast(t('account.signInFailed', { error: error.message }))),
};

document.addEventListener('click', (event) => {
  const target = event.target.closest('[data-action]');
  if (target) { actions[target.dataset.action](target.dataset, target); return; }
  const row = event.target.closest('.flow-row');
  if (!row) return;
  readEditorFields();
  const { step } = findRow(row.dataset.row);
  // 循環先選取，才知道新增要放進哪裡；再點一次才編輯
  if (step.type === 'loop' && state.selected !== step) { state.selected = step; renderEditor(); return; }
  state.step = structuredClone(step);
  state.adding = false;
  renderStep();
});

// 左右滑動日曆切換前後週或前後月
let swipeStart = null;
view.addEventListener('pointerdown', (event) => {
  swipeStart = event.target.closest('.rec-calendar') ? event.clientX : null;
});
view.addEventListener('pointerup', (event) => {
  if (swipeStart === null) return;
  const moved = event.clientX - swipeStart;
  swipeStart = null;
  if (Math.abs(moved) < 50) return;
  state.swiped = true;
  moveRecords(moved < 0 ? 1 : -1);
  setTimeout(() => { state.swiped = false; }, 50);
});

view.addEventListener('change', (event) => {
  const { card: cardId, field } = event.target.dataset;
  if (!cardId) return;
  const cards = analysisCards(analysisRows()).map((card) => {
    if (card.id !== cardId) return card;
    if (field !== 'source') return { ...card, [field]: event.target.value };
    const [kind, ...rest] = event.target.value.split(':');
    return { ...card, source: kind === 'all' ? { kind } : { kind, id: rest.join(':') } };
  });
  saveAnalysisCards(cards);
});

panel.addEventListener('change', (event) => {
  const { target } = event;
  const key = target.dataset.setting;
  if (!key) return;
  if (target.type === 'checkbox') setSetting(key, target.checked);
  else if (key === 'voiceURI') setSetting(key, target.value);
  else setSetting(key, Number(target.value));
});

const formHandlers = { step: saveStep, exercise: saveExercise, moves: saveMoves, schedule: saveSchedule, record: saveRecordEdit, cycle: saveCycle, phase: savePhase };

panel.addEventListener('submit', (event) => {
  event.preventDefault();
  formHandlers[event.target.dataset.form](event.target);
});

// 系統返回或 Esc 關閉編輯器時，也走同一個確認流程
sheet.addEventListener('cancel', (event) => {
  event.preventDefault();
  requestClose();
});

function handleQuery() {
  if (state.handledQuery) return;
  const id = query.get('run') ?? query.get('edit');
  if (!id) return;
  const routine = findRoutine(id);
  if (!routine) return;
  state.handledQuery = true;
  if (query.get('run')) {
    // 手機要在這一頁點一下才能播放提示音
    openPanel(`<div class="stack">
      <h2>${esc(routine.name)}</h2>
      <button class="btn primary wide" data-action="start-now" data-id="${esc(id)}">${t('step.start')}</button>
    </div>`);
  } else actions.edit({ id });
}

store.addEventListener('change', () => {
  if (store.profile.lang && store.profile.lang !== lang) setLang(store.profile.lang);
  linkExistingRoutines();
  renderHome();
  handleQuery();
});
store.addEventListener('error', (event) => toast(t('error.save', { error: event.detail.message })));
renderHome();
store.init().catch((error) => toast(t('error.start', { error: error.message })));

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
