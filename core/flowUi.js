// 計時流程的編輯器和執行畫面，健康管理和獨立計時網頁共用
import { STEP_TYPES, flattenFlow, flowDuration, flowToRecord, newStep } from './flow.js';
import { t } from './i18n.js';

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);

function stepFields(step, path) {
  if (step.type === 'loop') {
    return `<label class="field"><span>${t('step.iterations')}</span><input type="number" inputmode="numeric" min="1" data-path="${path}" data-field="iterations" value="${step.iterations}"></label>`;
  }
  const valueField = step.type === 'reps' ? 'count' : 'duration';
  const list = step.type === 'rest' ? '' : 'list="exercise-options"';
  return `<div class="grid-2">
    <label class="field"><span>${t(step.type === 'rest' ? 'step.name' : 'training.moveName')}</span><input ${list} autocomplete="off" data-path="${path}" data-field="label" value="${esc(step.label)}"></label>
    <label class="field"><span>${t(step.type === 'reps' ? 'metric.reps' : 'step.seconds')}</span><input type="number" inputmode="numeric" min="1" data-path="${path}" data-field="${valueField}" value="${step[valueField]}"></label>
  </div>`;
}

function renderSteps(steps, parent) {
  const rows = steps.map((step, i) => {
    const path = parent === '' ? String(i) : `${parent}.${i}`;
    return `<div class="flow-step ${step.type}">
      <div class="row">
        <strong>${t(`step.${step.type}`)}</strong>
        <div class="chips">
          <button type="button" class="plain" data-flow="move" data-path="${path}" data-delta="-1" aria-label="${t('cards.up')}">↑</button>
          <button type="button" class="plain" data-flow="move" data-path="${path}" data-delta="1" aria-label="${t('cards.down')}">↓</button>
          <button type="button" class="plain" data-flow="remove" data-path="${path}" aria-label="${t('common.delete')}">✕</button>
        </div>
      </div>
      ${stepFields(step, path)}
      ${step.type === 'loop' ? renderSteps(step.children, path) : ''}
    </div>`;
  }).join('');
  const add = STEP_TYPES.map((type) => `<button type="button" class="chip" data-flow="add" data-parent="${parent}" data-type="${type}">＋ ${t(`step.${type}`)}</button>`).join('');
  return `${rows}<div class="chips flow-add">${add}</div>`;
}

export const renderFlowEditor = (flow) => `<div id="flow-editor">${renderSteps(flow, '')}</div>`;

function listAt(flow, path) {
  return path === '' ? flow : path.split('.').reduce((list, i) => list[Number(i)].children, flow);
}

function locate(flow, path) {
  const parts = path.split('.');
  const index = Number(parts.pop());
  return { list: listAt(flow, parts.join('.')), index };
}

// 編輯器直接改 flow 物件；結構改變時由 rerender 重畫
export function attachFlowEditor(root, getFlow, rerender) {
  root.addEventListener('input', (event) => {
    const { path, field } = event.target.dataset;
    if (!path) return;
    const { list, index } = locate(getFlow(), path);
    list[index][field] = field === 'label' ? event.target.value : Number(event.target.value) || 1;
  });
  root.addEventListener('click', (event) => {
    const button = event.target.closest('[data-flow]');
    if (!button) return;
    const flow = getFlow();
    const { flow: action, path, parent, type, delta } = button.dataset;
    if (action === 'add') listAt(flow, parent).push(newStep(type));
    if (action === 'remove') { const { list, index } = locate(flow, path); list.splice(index, 1); }
    if (action === 'move') {
      const { list, index } = locate(flow, path);
      const next = index + Number(delta);
      if (next >= 0 && next < list.length) list.splice(next, 0, list.splice(index, 1)[0]);
    }
    rerender();
  });
}

const formatClock = (seconds) => `${Math.floor(seconds / 60)}:${String(Math.ceil(seconds) % 60).padStart(2, '0')}`;

export const flowSummary = (flow) => formatClock(flowDuration(flow));

// 執行畫面用時間差計算，畫面暫停或掉格也不會累積誤差
export function runFlow(training, { onFinish }) {
  const queue = flattenFlow(training.flow);
  if (!queue.length) return;
  const done = [];
  const started = Date.now();
  let index = 0;
  let remaining = queue[0].duration ?? null;
  let endsAt = remaining === null ? null : Date.now() + remaining * 1000;
  let paused = false;
  let lastBeep = null;
  let wakeLock = null;
  const audio = window.AudioContext ? new AudioContext() : null;

  const overlay = document.createElement('div');
  overlay.className = 'flow-run';
  document.body.append(overlay);
  navigator.wakeLock?.request('screen').then((lock) => { wakeLock = lock; }).catch(() => {});

  const beep = (frequency = 880) => {
    if (!audio) return;
    const oscillator = audio.createOscillator();
    oscillator.frequency.value = frequency;
    oscillator.connect(audio.destination);
    oscillator.start();
    oscillator.stop(audio.currentTime + 0.12);
  };

  function close(completed, save) {
    clearInterval(timer);
    overlay.remove();
    wakeLock?.release().catch(() => {});
    audio?.close();
    if (save) onFinish(flowToRecord(training, done, (Date.now() - started) / 1000, completed));
  }

  function advance(markDone = true) {
    if (markDone) done.push(queue[index]);
    index += 1;
    if (index >= queue.length) { close(true, true); return; }
    remaining = queue[index].duration ?? null;
    endsAt = remaining === null ? null : Date.now() + remaining * 1000;
    beep(660);
    paint();
  }

  function paint() {
    const step = queue[index];
    const next = queue[index + 1];
    const big = step.type === 'reps' ? `${step.count}` : formatClock(remaining);
    overlay.className = `flow-run ${step.type}`;
    overlay.innerHTML = `
      <p class="flow-progress">${index + 1} / ${queue.length}</p>
      <h1 class="flow-label">${esc(step.label || t(`step.${step.type}`))}</h1>
      <div class="flow-clock">${big}</div>
      <p class="flow-next">${next ? t('step.next', { name: esc(next.label || t(`step.${next.type}`)) }) : ''}</p>
      <div class="flow-controls">
        ${step.type === 'reps' ? `<button class="btn primary" data-run="done">${t('record.done')}</button>` : `<button class="btn primary" data-run="pause">${t(paused ? 'step.resume' : 'step.pause')}</button>`}
        <button class="btn" data-run="skip">${t('step.skip')}</button>
        <button class="btn" data-run="stop">${t('step.stop')}</button>
      </div>`;
  }

  overlay.addEventListener('click', (event) => {
    const action = event.target.closest('[data-run]')?.dataset.run;
    if (!action) return;
    audio?.resume();
    if (action === 'done') advance();
    if (action === 'skip') advance(false);
    if (action === 'pause') {
      paused = !paused;
      if (!paused) endsAt = Date.now() + remaining * 1000;
      paint();
    }
    if (action === 'stop') {
      paused = true;
      overlay.innerHTML = `<h1 class="flow-label">${t('step.stopTitle')}</h1>
        <div class="flow-controls">
          <button class="btn primary" data-run="save">${t('step.savePartial')}</button>
          <button class="btn" data-run="discard">${t('step.discard')}</button>
        </div>`;
    }
    if (action === 'save') close(false, true);
    if (action === 'discard') close(false, false);
  });

  const timer = setInterval(() => {
    if (paused || endsAt === null) return;
    remaining = Math.max(0, (endsAt - Date.now()) / 1000);
    const whole = Math.ceil(remaining);
    if (whole <= 3 && whole > 0 && whole !== lastBeep) { lastBeep = whole; beep(); }
    if (remaining <= 0) { lastBeep = null; advance(); return; }
    overlay.querySelector('.flow-clock').textContent = formatClock(remaining);
  }, 200);

  paint();
}
