// 全螢幕訓練畫面：背景就是目前步驟的顏色，不看字也知道階段
import { advanceBy, flattenFlow, flowToRecord, formatClock, lastLogValue } from './flow.js';
import { t } from './i18n.js';

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const SESSION_KEY = 'timer.session';
const option = (key, fallback) => { try { return localStorage.getItem(key) ?? fallback; } catch { return fallback; } };
const store = (key, value) => { try { value === null ? localStorage.removeItem(key) : localStorage.setItem(key, value); } catch { /* 無法保存時只影響這次 */ } };

export const soundOn = () => option('timer.sound', 'on') === 'on';
export const setSound = (on) => store('timer.sound', on ? 'on' : 'off');
export const autoLogOn = () => option('timer.autoLog', 'on') === 'on';
export const setAutoLog = (on) => store('timer.autoLog', on ? 'on' : 'off');

export function savedSession() {
  try { return JSON.parse(localStorage.getItem(SESSION_KEY)); } catch { return null; }
}
export const clearSession = () => store(SESSION_KEY, null);

const stepName = (step) => step.label || t(step.type === 'reps' ? 'step.reps' : 'step.timer');
const stepAmount = (step) => (step.type === 'reps' ? t('step.times', { n: step.count }) : formatClock(step.duration));

function loopText(step) {
  const state = step.loopState ?? [];
  if (!state.length) return '';
  if (state.length === 1) return t('run.loop', { current: state[0].current, total: state[0].total });
  const [outer] = state;
  const inner = state.at(-1);
  return t('run.loops', { oc: outer.current, ot: outer.total, ic: inner.current, it: inner.total });
}

// 用時間差計算，畫面暫停或掉格也不會累積誤差
export function runFlow(training, { events, onFinish, onClose, resume = null }) {
  const queue = flattenFlow(training.flow);
  if (!queue.length) return;
  const logs = resume?.logs ?? [];
  const doneIndexes = new Set(resume?.done ?? []);
  let index = resume?.index ?? 0;
  let remaining = resume?.remaining ?? queue[index].duration ?? null;
  let endsAt = remaining === null ? null : Date.now() + remaining * 1000;
  let elapsedBefore = resume?.elapsed ?? 0;
  let startedAt = Date.now();
  let paused = false;
  let lastBeep = null;
  let wakeLock = null;
  let recognition = null;
  const audio = window.AudioContext ? new AudioContext() : null;

  const elapsed = () => elapsedBefore + (paused ? 0 : (Date.now() - startedAt) / 1000);

  const overlay = document.createElement('div');
  overlay.className = 'run';
  document.body.append(overlay);
  const keepAwake = () => navigator.wakeLock?.request('screen').then((lock) => { wakeLock = lock; }).catch(() => {});
  keepAwake();

  const tone = (frequency, delay = 0, length = 0.14) => {
    if (!audio || !soundOn()) return;
    const oscillator = audio.createOscillator();
    const gain = audio.createGain();
    gain.gain.value = 0.3;
    oscillator.frequency.value = frequency;
    oscillator.connect(gain).connect(audio.destination);
    oscillator.start(audio.currentTime + delay);
    oscillator.stop(audio.currentTime + delay + length);
  };
  const soundStep = () => { tone(660); tone(990, 0.16); };
  const soundFinish = () => { tone(660); tone(880, 0.18); tone(1100, 0.36, 0.3); };

  const persist = () => store(SESSION_KEY, JSON.stringify({ trainingId: training.id, index, remaining, elapsed: elapsed(), logs, done: [...doneIndexes] }));

  function close(completed, save) {
    clearInterval(timer);
    document.removeEventListener('visibilitychange', onVisible);
    overlay.remove();
    wakeLock?.release().catch(() => {});
    recognition?.stop();
    audio?.close();
    clearSession();
    if (save) onFinish(flowToRecord(training, [...doneIndexes].map((i) => queue[i]), logs, elapsed(), completed));
    onClose?.();
  }

  function showLog(step, stepIndex) {
    if (!autoLogOn() || !step.metrics?.length) return;
    const fields = step.metrics.map((metric) => {
      const value = lastLogValue([{ occurredAt: Infinity, data: { logs } }, ...events], step.label, metric) ?? '';
      return `<label class="field"><span>${t(`metric.${metric}`)}</span><input type="number" inputmode="decimal" min="0" step="any" name="${esc(metric)}" value="${value}"></label>`;
    }).join('');
    const panel = document.createElement('form');
    panel.className = 'run-log';
    panel.innerHTML = `<strong>${esc(stepName(step))}</strong>
      <div class="grid-2">${fields}</div>
      <label class="check"><input type="checkbox" name="failure"> ${t('run.failure')}</label>
      <button class="btn primary wide">${t('run.saveLog')}</button>`;
    panel.addEventListener('submit', (event) => {
      event.preventDefault();
      const data = new FormData(panel);
      const values = {};
      for (const metric of step.metrics) if (data.get(metric)) values[metric] = Number(data.get(metric));
      logs.push({ stepIndex, label: step.label, exerciseId: step.exerciseId ?? null, loopState: step.loopState, values, failure: data.get('failure') === 'on' });
      panel.remove();
      persist();
    });
    overlay.append(panel);
  }

  function enter(next) {
    overlay.querySelector('.run-log')?.remove();
    index = next;
    remaining = queue[index].duration ?? null;
    endsAt = remaining === null ? null : Date.now() + remaining * 1000;
    lastBeep = null;
    soundStep();
    paint();
    persist();
  }

  function advance(markDone = true) {
    const step = queue[index];
    if (markDone) doneIndexes.add(index);
    if (index + 1 >= queue.length) { soundFinish(); close(true, true); return; }
    enter(index + 1);
    if (markDone) showLog(step, index - 1);
  }

  function timeline() {
    return queue.filter((step) => step.duration).map((step) => {
      const position = queue.indexOf(step);
      return `<i class="tone-${step.color}${position < index ? ' past' : ''}" style="flex:${step.duration}"></i>`;
    }).join('');
  }

  const left = () => (remaining ?? 0) + queue.slice(index + 1).reduce((sum, step) => sum + (step.duration ?? 0), 0);

  function paint() {
    const step = queue[index];
    const next = queue[index + 1];
    overlay.className = `run tone-${step.color}`;
    overlay.innerHTML = `
      <div class="run-top"><span>${loopText(step)}</span><span id="run-clock-left">${formatClock(left())}</span></div>
      <div class="run-main">
        <h1>${esc(stepName(step))}</h1>
        <div class="run-clock">${step.type === 'reps' ? t('step.times', { n: step.count }) : formatClock(remaining)}</div>
        <div class="run-progress"><i id="run-progress" style="width:100%"></i></div>
      </div>
      <div class="run-next ${next ? `tone-${next.color}` : ''}">
        <span>${t('run.next')}</span><strong>${next ? `${esc(stepName(next))}　${stepAmount(next)}` : t('run.end')}</strong>
      </div>
      <div class="run-line">${timeline()}</div>
      <div class="run-controls">
        <button data-run="back" aria-label="${t('run.back')}"${index === 0 ? ' disabled' : ''}>‹</button>
        ${step.type === 'reps' ? `<button class="main" data-run="done">${t('run.done')}</button>` : `<button class="main" data-run="pause">${t(paused ? 'run.resume' : 'run.pause')}</button>`}
        <button data-run="skip" aria-label="${t('run.skip')}">›</button>
      </div>
      <div class="run-tools">
        <button data-run="stop">${t('run.stop')}</button>
        <button data-run="sound">${t(soundOn() ? 'run.soundOn' : 'run.soundOff')}</button>
        <button data-run="voice">${t(recognition ? 'run.voiceOn' : 'run.voiceOff')}</button>
      </div>`;
  }

  function togglePause() {
    if (paused) {
      paused = false;
      startedAt = Date.now();
      if (remaining !== null) endsAt = Date.now() + remaining * 1000;
    } else {
      elapsedBefore = elapsed();
      paused = true;
    }
    paint();
    persist();
  }

  function stop() {
    if (!paused) togglePause();
    overlay.innerHTML = `<div class="run-main"><h1>${t('run.stopTitle')}</h1>
      <div class="run-controls"><button class="main" data-run="save">${t('run.savePartial')}</button><button data-run="discard">${t('run.discard')}</button><button data-run="continue">${t('run.resume')}</button></div></div>`;
    overlay.className = 'run tone-gray';
  }

  function toggleVoice() {
    if (recognition) { recognition.stop(); recognition = null; paint(); return; }
    const Speech = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!Speech) { paint(); return; }
    recognition = new Speech();
    recognition.continuous = true;
    recognition.lang = document.documentElement.lang;
    recognition.onresult = (event) => {
      const said = event.results[event.results.length - 1][0].transcript;
      if (/暫停|pause|stop/i.test(said) && !paused) togglePause();
      else if (/繼續|開始|resume|start/i.test(said) && paused) togglePause();
      else if (/下一|跳過|next|skip/i.test(said)) advance(false);
      else if (/完成|好了|done/i.test(said) && queue[index].type === 'reps') advance();
    };
    recognition.onend = () => { if (recognition) recognition.start(); };
    recognition.start();
    paint();
  }

  const handlers = {
    done: () => advance(),
    skip: () => advance(false),
    back: () => enter(Math.max(0, index - 1)),
    pause: togglePause,
    stop,
    sound: () => { setSound(!soundOn()); paint(); },
    voice: toggleVoice,
    save: () => close(false, true),
    discard: () => close(false, false),
    continue: () => { togglePause(); },
  };

  overlay.addEventListener('click', (event) => {
    const action = event.target.closest('[data-run]')?.dataset.run;
    if (!action) return;
    audio?.resume();
    handlers[action]();
  });

  // 手機切到背景時計時器會停，回來後依實際經過時間補上
  function catchUp() {
    if (paused || endsAt === null || Date.now() < endsAt) return;
    const overdue = (Date.now() - endsAt) / 1000;
    const result = advanceBy(queue, index, 0, overdue);
    result.passed.forEach((i) => doneIndexes.add(i));
    if (result.finished) { soundFinish(); close(true, true); return; }
    index = result.index;
    remaining = result.remaining;
    endsAt = remaining === null ? null : Date.now() + remaining * 1000;
    lastBeep = null;
    soundStep();
    paint();
    persist();
  }

  function onVisible() {
    if (document.visibilityState !== 'visible') return;
    audio?.resume();
    keepAwake();
    catchUp();
  }
  document.addEventListener('visibilitychange', onVisible);

  const timer = setInterval(() => {
    if (paused) return;
    const clockLeft = overlay.querySelector('#run-clock-left');
    if (endsAt === null) return;
    remaining = Math.max(0, (endsAt - Date.now()) / 1000);
    const whole = Math.ceil(remaining);
    if (whole <= 3 && whole > 0 && whole !== lastBeep) { lastBeep = whole; tone(880); }
    if (remaining <= 0) { advance(); return; }
    overlay.querySelector('.run-clock').textContent = formatClock(remaining);
    overlay.querySelector('#run-progress').style.width = `${(remaining / queue[index].duration) * 100}%`;
    if (clockLeft) clockLeft.textContent = formatClock(left());
  }, 200);

  paint();
  persist();
}
