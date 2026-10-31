// 訓練 App 和獨立計時器共用這份程式，資料也和健康管理共用同一個資料庫
import { COLORS, LOG_METRICS, TEMPLATES, flattenFlow, flowDuration, formatDuration, mapFlow, newStep, normalizeFlow } from './flow.js';
import { dateKey, parseDateKey } from './logic.js';
import { autoLogOn, clearSession, runFlow, savedSession, setAutoLog, setSound, soundOn } from './flowRun.js';
import { lang, setLang, t } from './i18n.js';
import { healthStore as store } from './store.js';
import { MODES, MOVE_METRICS, descendantIds, exercisePath, exerciseTotal, exerciseTree, linkExercise, personalBests, scheduleRemaining, weeklyStats } from './training.js';

// 計時器入口只有課表和訓練；訓練 App 多了動作、排程和紀錄
const isTimerApp = document.documentElement.dataset.app === 'timer';
const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const panel = document.getElementById('panel');
const toastEl = document.getElementById('toast');
const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const query = new URLSearchParams(location.search);

const state = { tab: 'routines', draft: null, routine: null, original: '', selected: null, step: null, stepList: null, adding: false, handledQuery: false };

function toast(message) {
  toastEl.hidden = false;
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
const routines = () => (isTimerApp ? allRoutines().filter((r) => r.flow?.length) : allRoutines());
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
      <strong>${esc(routine.name)}</strong>
      <span class="muted">${detail}${routine.tags?.length ? `・${routine.tags.map(esc).join('、')}` : ''}</span>
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
  const rows = store.activeDocs('schedules').map((schedule) => {
    const left = scheduleRemaining(schedule, today);
    const label = left === null ? t('schedule.ongoing') : left ? t('schedule.remaining', { n: left }) : t('schedule.ended');
    return `<button class="plan-row" data-action="schedule-edit" data-id="${esc(schedule.id)}"><span>${esc(findRoutine(schedule.trainingId)?.name)}</span><span class="muted">${label}</span></button>`;
  }).join('');
  return `<section class="card">${rows || `<p class="muted">${t('train.noSchedules')}</p>`}</section>
    <button class="btn wide" data-action="schedule-edit">＋ ${t('train.newSchedule')}</button>`;
}

function renderRecords() {
  const events = workoutEvents();
  if (!events.length) return `<section class="card"><p class="muted">${t('train.noRecords')}</p></section>`;
  const weeks = weeklyStats(events, dateKey());
  const peak = Math.max(1, ...weeks.map((w) => w.reps));
  const rows = weeks.map((week) => `<div class="week-row">
    <span class="muted">${parseDateKey(week.start).toLocaleDateString(lang, { month: 'numeric', day: 'numeric' })}</span>
    <div class="bar-track"><div class="bar-fill" style="width:${(week.reps / peak) * 100}%"></div></div>
    <small>${t('analysis.reps', { n: week.reps })}・${t('analysis.days', { n: week.days })}</small>
  </div>`).join('');
  const nameOf = (best) => store.activeDocs('exercises').find((e) => e.id === best.exerciseId)?.name ?? best.name;
  const bests = personalBests(events).slice(0, 5).map((best) => `<div class="stat-row"><span>${esc(nameOf(best))}</span><strong>${[best.reps && t('analysis.reps', { n: best.reps }), best.weightKg && `${best.weightKg} kg`].filter(Boolean).join('・')}</strong></div>`).join('');
  const recent = [...events].sort((a, b) => b.occurredAt - a.occurredAt).slice(0, 20).map((e) => `<div class="stat-row"><span>${esc(e.data.activity)}</span><span class="muted">${parseDateKey(e.date).toLocaleDateString(lang, { month: 'numeric', day: 'numeric' })}</span></div>`).join('');
  return `<section class="card"><h2>${t('analysis.title')}</h2>${rows}
      ${bests ? `<h2 style="margin-top:14px">${t('analysis.best')}</h2>${bests}` : ''}</section>
    <section class="card"><h2>${t('workout.recent')}</h2>${recent}</section>`;
}

function renderHome() {
  document.documentElement.lang = lang;
  document.querySelectorAll('.train-tabs button').forEach((button) => {
    button.classList.toggle('active', button.dataset.tab === state.tab);
    button.textContent = t(`train.${button.dataset.tab}`);
  });
  const screens = { exercises: renderExercises, routines: renderRoutines, schedules: renderSchedules, records: renderRecords };
  const title = isTimerApp ? t('timer.appTitle') : t(`train.${state.tab}`);
  view.innerHTML = `<header class="topbar"><h1>${title}</h1>
      <div class="chips">${isTimerApp ? '' : `<a class="plain" href="../Health/">${t('timer.health')}</a>`}<button class="icon-btn" data-action="settings" aria-label="${t('train.settings')}">⚙</button></div>
    </header>${screens[state.tab]()}`;
}

function openSettings() {
  const account = store.user
    ? `<p class="muted">${t('account.signedIn', { email: esc(store.user.email) })}</p>`
    : `<p class="muted">${t('account.signedOut')}</p>${store.canSignIn ? `<button class="btn wide" data-action="sign-in">${t('account.signIn')}</button>` : ''}`;
  openPanel(`<div class="stack">
    ${panelHeader(t('train.settings'))}
    <label class="check"><input type="checkbox" data-option="sound"${soundOn() ? ' checked' : ''}> ${t('timer.sound')}</label>
    <label class="check"><input type="checkbox" data-option="log"${autoLogOn() ? ' checked' : ''}> ${t('timer.autoLog')}</label>
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
  state.draft = { id: exercise?.id, name: exercise?.name ?? '', nameEn: exercise?.nameEn ?? '', parentId: exercise?.parentId ?? null, defaultMinutes: exercise?.defaultMinutes ?? null };
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
    ${id ? `<div class="stat-row"><span>${t('exercise.total')}</span><strong>${exerciseTotal(records, list, id)}</strong></div>` : ''}
    <button class="btn primary wide">${t('common.save')}</button>
    ${id ? `<button type="button" class="btn danger wide" data-action="exercise-delete">${t('common.delete')}</button><p class="muted">${t('exercise.deleteHint')}</p>` : ''}
  </form>`);
}

function saveExercise(form) {
  const values = new FormData(form);
  store.saveDoc('exercises', { ...state.draft, name: values.get('name').trim(), nameEn: values.get('nameEn').trim(), parentId: values.get('parentId') || null, defaultMinutes: optionalNumber(values.get('defaultMinutes')) });
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
      <details class="editor-more"><summary>${t('editor.more')}</summary>
        <label class="field"><span>${t('editor.tags')}</span><input id="routine-tags" value="${esc((routine.tags ?? []).join(', '))}"></label>
        ${routine.id ? `<button class="btn danger wide" data-action="delete">${t('editor.deleteRoutine')}</button>` : ''}
      </details>
    </footer>
  </div>`;
  if (!sheet.open) { sheet.showModal(); sheet.focus(); }
  attachSortable();
}

function readEditorFields() {
  state.routine.name = sheet.querySelector('#routine-name')?.value ?? state.routine.name;
  const tags = sheet.querySelector('#routine-tags');
  if (tags) state.routine.tags = tags.value.split(/[,，、]/).map((tag) => tag.trim()).filter(Boolean);
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
  const metrics = LOG_METRICS.map((metric) => `<label class="check"><input type="checkbox" name="metric" value="${metric}"${step.metrics.includes(metric) ? ' checked' : ''}> ${t(`metric.${metric}`)}</label>`).join('');
  openPanel(`<form class="stack" data-form="step">
    <div class="row"><h2 style="margin:0">${t(step.type === 'reps' ? 'step.reps' : 'step.timer')}</h2><button type="button" class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
    <div class="grid-2">
      <label class="field"><span>${t('step.name')}</span><input name="label" list="exercise-options" autocomplete="off" value="${esc(step.label)}"></label>
      ${amount}
    </div>
    <datalist id="exercise-options">${exerciseOptions()}</datalist>
    <div class="swatches">${swatches}</div>
    <details><summary>${t('editor.moreOptions')}</summary>
      ${step.type === 'timer' ? `<label class="check"><input type="checkbox" name="rest"${step.rest ? ' checked' : ''}> ${t('editor.isRest')}</label>
      <label class="check"><input type="checkbox" name="skipOnLast"${step.skipOnLast ? ' checked' : ''}> ${t('editor.skipOnLast')}</label>` : ''}
      <p class="muted">${t('editor.logHint')}</p>${metrics}
    </details>
    <button class="btn primary wide">${t('common.save')}</button>
    ${isNew ? '' : `<div class="grid-2"><button type="button" class="btn" data-action="copy">${t('editor.copy')}</button><button type="button" class="btn danger" data-action="remove">${t('common.delete')}</button></div>`}
  </form>`);
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
    if (step.type === 'reps') step.count = Number(values.get('count'));
    else {
      step.duration = Number(values.get('duration'));
      step.rest = values.get('rest') === 'on';
      step.skipOnLast = values.get('skipOnLast') === 'on';
    }
    step.metrics = values.getAll('metric');
  }
  if (state.adding) state.stepList.push(step);
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
  close: requestClose,
  discard: closeEditor,
  back: () => { state.adding = false; panel.close(); },
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
  color: ({ color }, target) => {
    state.step.color = color;
    panel.querySelectorAll('.swatch').forEach((el) => el.classList.toggle('active', el === target));
  },
  copy: () => {
    const clone = structuredClone(state.step);
    const fresh = (step) => ({ ...step, id: crypto.randomUUID(), ...(step.children ? { children: step.children.map(fresh) } : {}) });
    const list = findRow(state.step.id).list;
    list.splice(list.indexOf(state.step) + 1, 0, fresh(clone));
    panel.close();
    renderEditor();
  },
  remove: () => {
    const { list, index } = findRow(state.step.id);
    list.splice(index, 1);
    if (state.selected === state.step) state.selected = null;
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
  state.step = step;
  state.adding = false;
  renderStep();
});

panel.addEventListener('change', (event) => {
  const { option } = event.target.dataset;
  if (option === 'sound') setSound(event.target.checked);
  if (option === 'log') setAutoLog(event.target.checked);
});

const formHandlers = { step: saveStep, exercise: saveExercise, moves: saveMoves, schedule: saveSchedule };

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
