// 訓練計時：獨立網頁，資料和健康管理共用
import { COLORS, LOG_METRICS, TEMPLATES, flattenFlow, flowDuration, formatDuration, mapFlow, newStep, normalizeFlow } from '../core/flow.js';
import { autoLogOn, clearSession, runFlow, savedSession, setAutoLog, setSound, soundOn } from '../core/flowRun.js';
import { lang, t } from '../core/i18n.js';
import { healthStore as store } from '../core/store.js';
import { exercisePath, exerciseTree, resolveExercise } from '../core/training.js';

const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const toastEl = document.getElementById('toast');
const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const query = new URLSearchParams(location.search);

const state = { routine: null, selected: null, step: null, stepList: null, adding: false, handledQuery: false };

function toast(message) {
  toastEl.hidden = false;
  toastEl.textContent = message;
  setTimeout(() => { toastEl.hidden = true; }, 3000);
}

const routines = () => store.activeDocs('trainings').filter((r) => r.flow?.length);
const findRoutine = (id) => store.trainings.find((r) => r.id === id);
const workoutEvents = () => store.liveEvents().filter((e) => e.type === 'workout');
const stepName = (step) => step.label || t(step.type === 'reps' ? 'step.reps' : 'step.timer');

function start(routine, resume = null) {
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

function renderHome() {
  document.documentElement.lang = lang;
  const session = savedSession();
  const sessionRoutine = session && findRoutine(session.trainingId);
  const rows = routines().map((routine) => {
    const flow = normalizeFlow(routine.flow);
    const colors = flattenFlow(flow).filter((s) => s.duration).slice(0, 40).map((s) => `<i class="tone-${s.color}" style="flex:${s.duration}"></i>`).join('');
    return `<article class="routine">
      <button class="routine-open" data-action="edit" data-id="${esc(routine.id)}">
        <strong>${esc(routine.name)}</strong>
        <span class="muted">${formatDuration(flowDuration(flow), t)}${routine.tags?.length ? `・${routine.tags.map(esc).join('、')}` : ''}</span>
        <span class="run-line small">${colors}</span>
      </button>
      <button class="btn primary" data-action="start" data-id="${esc(routine.id)}">${t('step.start')}</button>
    </article>`;
  }).join('');
  const examples = Object.keys(TEMPLATES).map((key) => `<button class="chip" data-action="template" data-key="${key}">＋ ${t(`template.${key}`)}</button>`).join('');
  view.innerHTML = `
    <header class="topbar"><h1>${t('timer.title')}</h1><a class="plain" href="../Health/">${t('timer.health')}</a></header>
    ${sessionRoutine ? `<section class="card row"><span>${t('timer.resume', { name: esc(sessionRoutine.name) })}</span>
      <div class="chips"><button class="btn primary" data-action="resume">${t('run.resume')}</button><button class="plain" data-action="drop-session">${t('run.discard')}</button></div></section>` : ''}
    <section class="routines">${rows || `<p class="muted">${t('timer.empty')}</p>`}</section>
    <button class="btn wide meal-actions" data-action="new">＋ ${t('timer.new')}</button>
    <section class="card stack"><h2>${t('timer.examples')}</h2><div class="chips">${examples}</div></section>
    <section class="card stack"><h2>${t('timer.options')}</h2>
      <label class="check"><input type="checkbox" data-option="sound"${soundOn() ? ' checked' : ''}> ${t('timer.sound')}</label>
      <label class="check"><input type="checkbox" data-option="log"${autoLogOn() ? ' checked' : ''}> ${t('timer.autoLog')}</label>
    </section>
    <p class="muted">${store.user ? t('account.signedIn', { email: esc(store.user.email) }) : t('account.signedOut')}</p>
    ${store.user || !store.canSignIn ? '' : `<button class="btn wide" data-action="sign-in">${t('account.signIn')}</button>`}`;
}

// ---- 編輯器：整份課表一次看完，用縮排表示層級 ----

function editorRows(list, depth = 0) {
  return list.flatMap((step, index) => {
    const row = { step, list, index, depth };
    return step.type === 'loop' ? [row, ...editorRows(step.children, depth + 1)] : [row];
  });
}

function rowHtml({ step, depth }) {
  const selected = state.selected === step ? ' selected' : '';
  if (step.type === 'loop') {
    return `<button class="flow-row loop${selected}" style="--depth:${depth}" data-step="${esc(step.id)}"><span>${t('step.loopTimes', { n: step.iterations })}</span></button>`;
  }
  const amount = step.type === 'reps' ? t('step.times', { n: step.count }) : formatDuration(step.duration, t);
  return `<button class="flow-row tone-bar-${step.color}${selected}" style="--depth:${depth}" data-step="${esc(step.id)}"><span>${esc(stepName(step))}</span><span class="muted">${amount}</span></button>`;
}

function renderEditor() {
  const { routine } = state;
  const rows = editorRows(routine.flow);
  const line = flattenFlow(routine.flow).filter((s) => s.duration).map((s) => `<i class="tone-${s.color}" style="flex:${s.duration}"></i>`).join('');
  sheet.className = 'full';
  sheet.innerHTML = `<div class="editor">
    <header class="row editor-head">
      <button class="plain" data-action="close">${t('editor.cancel')}</button>
      <input class="editor-title" id="routine-name" value="${esc(routine.name)}" placeholder="${t('editor.name')}" aria-label="${t('editor.name')}">
      <button class="plain strong" data-action="save">${t('common.save')}</button>
    </header>
    <p class="muted editor-total">${formatDuration(flowDuration(routine.flow), t)}</p>
    <div class="run-line">${line}</div>
    <div class="flow-list">${rows.map(rowHtml).join('') || `<p class="muted">${t('editor.empty')}</p>`}</div>
    <footer class="editor-foot">
      <button class="btn primary wide" data-action="add">${state.selected?.type === 'loop' ? t('editor.addInLoop') : t('editor.add')}</button>
      <details class="editor-more"><summary>${t('editor.more')}</summary>
        <label class="field"><span>${t('editor.tags')}</span><input id="routine-tags" value="${esc((routine.tags ?? []).join(', '))}"></label>
        ${routine.id ? `<button class="btn danger wide" data-action="delete">${t('editor.deleteRoutine')}</button>` : ''}
      </details>
    </footer>
  </div>`;
  if (!sheet.open) sheet.showModal();
  attachDrag();
}

function readEditorFields() {
  state.routine.name = sheet.querySelector('#routine-name')?.value ?? state.routine.name;
  const tags = sheet.querySelector('#routine-tags');
  if (tags) state.routine.tags = tags.value.split(/[,，、]/).map((tag) => tag.trim()).filter(Boolean);
}

function renderPicker() {
  const presets = [
    { key: 'prepare', label: t('step.prepare'), props: { label: t('step.prepare'), duration: 10, color: 'blue', rest: true } },
    { key: 'hang', label: t('template.hang'), props: { label: t('template.hang'), duration: 7, color: 'red' } },
    { key: 'rest', label: t('step.rest'), props: { label: t('step.rest'), duration: 60, color: 'green', rest: true } },
  ];
  state.presets = presets;
  sheet.className = '';
  sheet.innerHTML = `<div class="stack">
    <div class="row"><h2 style="margin:0">${t('editor.whatToAdd')}</h2><button class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
    <div class="picker">
      <button class="btn" data-action="pick" data-type="timer">${t('step.timer')}</button>
      <button class="btn" data-action="pick" data-type="reps">${t('step.reps')}</button>
      <button class="btn" data-action="pick" data-type="loop">${t('step.loop')}</button>
    </div>
    <h2>${t('editor.common')}</h2>
    <div class="chips">${presets.map((p) => `<button class="chip tone-chip-${p.props.color}" data-action="preset" data-key="${p.key}">${p.label}</button>`).join('')}</div>
  </div>`;
}

function exerciseOptions() {
  const list = store.activeDocs('exercises');
  return exerciseTree(list).map(({ exercise }) => `<option value="${esc(exercisePath(list, exercise.id))}"></option>`).join('');
}

function renderStep() {
  const { step } = state;
  const isNew = state.adding;
  sheet.className = '';
  if (step.type === 'loop') {
    sheet.innerHTML = `<form class="stack" data-form="step">
      <div class="row"><h2 style="margin:0">${t('step.loop')}</h2><button type="button" class="icon-btn" data-action="back" aria-label="${t('common.close')}">✕</button></div>
      <label class="field"><span>${t('step.iterations')}</span><input type="number" inputmode="numeric" min="1" name="iterations" value="${step.iterations}" required></label>
      <button class="btn primary wide">${t('common.save')}</button>
      ${isNew ? '' : `<div class="grid-2"><button type="button" class="btn" data-action="copy">${t('editor.copy')}</button><button type="button" class="btn danger" data-action="remove">${t('common.delete')}</button></div>`}
    </form>`;
    return;
  }
  const amount = step.type === 'reps'
    ? `<label class="field"><span>${t('metric.reps')}</span><input type="number" inputmode="numeric" min="1" name="count" value="${step.count}" required></label>`
    : `<label class="field"><span>${t('step.seconds')}</span><input type="number" inputmode="numeric" min="1" name="duration" value="${step.duration}" required></label>`;
  const swatches = COLORS.map((color) => `<button type="button" class="swatch tone-${color}${step.color === color ? ' active' : ''}" data-action="color" data-color="${color}" aria-label="${color}"></button>`).join('');
  const metrics = LOG_METRICS.map((metric) => `<label class="check"><input type="checkbox" name="metric" value="${metric}"${step.metrics.includes(metric) ? ' checked' : ''}> ${t(`metric.${metric}`)}</label>`).join('');
  sheet.innerHTML = `<form class="stack" data-form="step">
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
  </form>`;
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
  renderEditor();
}

function findRow(id) {
  return editorRows(state.routine.flow).find((row) => row.step.id === id);
}

// 按住一行拖曳：放到循環上就進入循環，放到其他步驟就排到它前面
function attachDrag() {
  const list = sheet.querySelector('.flow-list');
  let dragId = null;
  let holdTimer = null;
  let target = null;

  const clear = () => {
    clearTimeout(holdTimer);
    list.querySelectorAll('.drag-over,.dragging').forEach((el) => el.classList.remove('drag-over', 'dragging'));
    dragId = null;
    target = null;
  };

  list.addEventListener('pointerdown', (event) => {
    const row = event.target.closest('.flow-row');
    if (!row) return;
    holdTimer = setTimeout(() => {
      dragId = row.dataset.step;
      row.classList.add('dragging');
      list.classList.add('drag-mode');
    }, 350);
  });
  list.addEventListener('pointermove', (event) => {
    if (!dragId) return;
    event.preventDefault();
    const over = document.elementFromPoint(event.clientX, event.clientY)?.closest('.flow-row');
    list.querySelectorAll('.drag-over').forEach((el) => el.classList.remove('drag-over'));
    target = over && over.dataset.step !== dragId ? over : null;
    target?.classList.add('drag-over');
  });
  list.addEventListener('touchmove', (event) => { if (dragId) event.preventDefault(); }, { passive: false });
  list.addEventListener('pointerup', () => {
    clearTimeout(holdTimer);
    if (!dragId) return;
    list.classList.remove('drag-mode');
    state.justDragged = true;
    if (target) {
      const from = findRow(dragId);
      const to = findRow(target.dataset.step);
      const inside = from.step.type === 'loop' && JSON.stringify(from.step).includes(to.step.id);
      if (!inside) {
        from.list.splice(from.index, 1);
        if (to.step.type === 'loop') to.step.children.push(from.step);
        else to.list.splice(to.list.indexOf(to.step), 0, from.step);
      }
    }
    clear();
    renderEditor();
  });
  list.addEventListener('pointercancel', clear);
}

function linkFlow(flow) {
  return mapFlow(flow, (step) => {
    if (step.rest || !step.label) return step;
    const list = store.activeDocs('exercises');
    const exercise = resolveExercise(list, step.label) ?? store.saveDoc('exercises', { name: step.label, nameEn: '', parentId: null });
    return { ...step, exerciseId: exercise.id, label: exercise.name };
  });
}

const actions = {
  new: () => { state.routine = { name: '', mode: 'sets', moves: [], trackRest: false, tags: [], flow: [] }; state.selected = null; renderEditor(); },
  edit: ({ id }) => {
    const routine = structuredClone(findRoutine(id));
    state.routine = { ...routine, flow: normalizeFlow(routine.flow) };
    state.selected = null;
    renderEditor();
  },
  close: () => sheet.close(),
  back: () => { state.adding = false; renderEditor(); },
  save: () => {
    readEditorFields();
    const name = state.routine.name.trim();
    if (!name) { toast(t('editor.needName')); return; }
    if (!state.routine.flow.length) { toast(t('editor.needStep')); return; }
    store.saveDoc('trainings', { ...state.routine, name, flow: linkFlow(state.routine.flow) });
    sheet.close();
  },
  delete: () => { store.saveDoc('trainings', { ...state.routine, deletedAt: Date.now() }); sheet.close(); },
  add: () => { readEditorFields(); renderPicker(); },
  pick: ({ type }) => addStep(newStep(type)),
  preset: ({ key }) => addStep(newStep('timer', state.presets.find((p) => p.key === key).props)),
  color: ({ color }, target) => {
    state.step.color = color;
    sheet.querySelectorAll('.swatch').forEach((el) => el.classList.toggle('active', el === target));
  },
  copy: () => {
    const clone = structuredClone(state.step);
    const fresh = (step) => ({ ...step, id: crypto.randomUUID(), ...(step.children ? { children: step.children.map(fresh) } : {}) });
    const list = findRow(state.step.id).list;
    list.splice(list.indexOf(state.step) + 1, 0, fresh(clone));
    renderEditor();
  },
  remove: () => {
    const { list, index } = findRow(state.step.id);
    list.splice(index, 1);
    if (state.selected === state.step) state.selected = null;
    renderEditor();
  },
  template: ({ key }) => {
    store.saveDoc('trainings', { mode: 'sets', moves: [], trackRest: false, ...TEMPLATES[key](t) });
    toast(t('common.saved'));
  },
  start: ({ id }) => start(findRoutine(id)),
  resume: () => { const session = savedSession(); start(findRoutine(session.trainingId), session); },
  'drop-session': () => { clearSession(); renderHome(); },
  'sign-in': () => store.signIn().catch((error) => toast(t('account.signInFailed', { error: error.message }))),
};

document.addEventListener('click', (event) => {
  const target = event.target.closest('[data-action]');
  if (target) { actions[target.dataset.action](target.dataset, target); return; }
  const row = event.target.closest('.flow-row');
  if (!row) return;
  if (state.justDragged) { state.justDragged = false; return; }
  readEditorFields();
  const { step } = findRow(row.dataset.step);
  // 循環先選取，才知道新增要放進哪裡；再點一次才編輯
  if (step.type === 'loop' && state.selected !== step) { state.selected = step; renderEditor(); return; }
  state.step = step;
  state.adding = false;
  renderStep();
});

view.addEventListener('change', (event) => {
  const { option } = event.target.dataset;
  if (option === 'sound') setSound(event.target.checked);
  if (option === 'log') setAutoLog(event.target.checked);
});

sheet.addEventListener('submit', (event) => {
  event.preventDefault();
  saveStep(event.target);
});

function handleQuery() {
  if (state.handledQuery) return;
  const id = query.get('run') ?? query.get('edit');
  if (!id) return;
  const routine = findRoutine(id);
  if (!routine) return;
  state.handledQuery = true;
  if (query.get('run')) start(routine);
  else actions.edit({ id });
}

store.addEventListener('change', () => { renderHome(); handleQuery(); });
store.addEventListener('error', (event) => toast(t('error.save', { error: event.detail.message })));
renderHome();
store.init().catch((error) => toast(t('error.start', { error: error.message })));
