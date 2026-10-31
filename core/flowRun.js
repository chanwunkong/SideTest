// 全螢幕訓練畫面：使用者流汗或很累時不想操作，所以沒有操作也能自己跑完
import { advanceBy, flattenFlow, flowToRecord, formatClock, lastLogValue } from './flow.js';
import { t } from './i18n.js';

const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
const SESSION_KEY = 'timer.session';
const SETTINGS_KEY = 'timer.settings';
const NOTE_SECONDS = 5;
const SWIPE_PIXELS = 60;
const DEFAULTS = { sound: true, soundStart: true, soundCountdown: true, soundFinish: true, countdownSec: 3, tts: false, rate: 1, voiceURI: '', pulse: false, autoLog: true };

const read = (key) => { try { return JSON.parse(localStorage.getItem(key)); } catch { return null; } };
const write = (key, value) => { try { value === null ? localStorage.removeItem(key) : localStorage.setItem(key, JSON.stringify(value)); } catch { /* 無法保存時只影響這次 */ } };

export const getSettings = () => ({ ...DEFAULTS, ...read(SETTINGS_KEY) });
export const setSetting = (key, value) => write(SETTINGS_KEY, { ...getSettings(), [key]: value });
export const savedSession = () => read(SESSION_KEY);
export const clearSession = () => write(SESSION_KEY, null);

export function speak(text) {
  const settings = getSettings();
  if (!settings.tts || !window.speechSynthesis) return;
  window.speechSynthesis.cancel();
  const utterance = new SpeechSynthesisUtterance(text);
  utterance.rate = settings.rate;
  utterance.lang = document.documentElement.lang;
  const voice = window.speechSynthesis.getVoices().find((v) => v.voiceURI === settings.voiceURI);
  if (voice) utterance.voice = voice;
  window.speechSynthesis.speak(utterance);
}

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

const valueText = (log) => Object.entries(log.values).map(([name, value]) => `${name} ${value}`).join('・');

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
  let noteTimer = null;
  let swipeStart = null;
  let tapLocked = false;
  const audio = window.AudioContext ? new AudioContext() : null;

  const wakeAudio = () => { if (audio?.state === 'suspended') audio.resume().catch(() => {}); };
  const elapsed = () => elapsedBefore + (paused ? 0 : (Date.now() - startedAt) / 1000);

  const overlay = document.createElement('div');
  overlay.className = 'run';
  document.body.append(overlay);
  const keepAwake = () => navigator.wakeLock?.request('screen').then((lock) => { wakeLock = lock; }).catch(() => {});
  keepAwake();

  const tone = (frequency, delay = 0, length = 0.14) => {
    if (!audio || !getSettings().sound) return;
    const oscillator = audio.createOscillator();
    const gain = audio.createGain();
    gain.gain.value = 0.3;
    oscillator.frequency.value = frequency;
    oscillator.connect(gain).connect(audio.destination);
    oscillator.start(audio.currentTime + delay);
    oscillator.stop(audio.currentTime + delay + length);
  };
  const soundStep = () => { if (getSettings().soundStart) { tone(660); tone(990, 0.16); } };
  const soundCountdown = () => { if (getSettings().soundCountdown) tone(880); };
  const soundFinish = () => { if (getSettings().soundFinish) { tone(660); tone(880, 0.18); tone(1100, 0.36, 0.3); } };

  const persist = () => write(SESSION_KEY, { trainingId: training.id, index, remaining, elapsed: elapsed(), logs, done: [...doneIndexes] });

  function shutdown() {
    clearInterval(timer);
    clearTimeout(noteTimer);
    document.removeEventListener('visibilitychange', onVisible);
    overlay.remove();
    wakeLock?.release().catch(() => {});
    stopVoice();
    audio?.close();
    window.speechSynthesis?.cancel();
    onClose?.();
  }

  function finish(completed, save) {
    clearSession();
    if (!save) { shutdown(); return; }
    const elapsedSec = elapsed();
    if (!logs.length) {
      shutdown();
      onFinish(flowToRecord(training, [...doneIndexes].map((i) => queue[i]), logs, elapsedSec, completed));
      return;
    }
    showSummary(completed, elapsedSec);
  }

  // 結束時一次看完所有數值，訓練中可以完全不操作
  function showSummary(completed, elapsedSec) {
    paused = true;
    tapLocked = true;
    const rows = logs.map((log, i) => {
      const step = queue[log.stepIndex];
      const fields = step.metrics.map((metric) => `<label class="field"><span>${esc(metric.name)}</span>
        <input type="number" inputmode="decimal" step="any" data-log="${i}" data-metric="${esc(metric.name)}" value="${log.values[metric.name] ?? ''}"></label>`).join('');
      return `<div class="move-row"><strong>${esc(log.label)} ${loopText(step)}</strong>
        <div class="grid-2">${fields}</div>
        <label class="check"><input type="checkbox" data-failure="${i}"${log.failure ? ' checked' : ''}> ${t('run.failure')}</label></div>`;
    }).join('');
    overlay.className = 'run run-summary';
    overlay.innerHTML = `<h1>${t('run.summary')}</h1><div class="stack run-summary-list">${rows}</div>
      <button class="btn primary wide" data-run="save-summary">${t('run.saveLog')}</button>`;
    overlay.querySelector('[data-run="save-summary"]').addEventListener('click', () => {
      overlay.querySelectorAll('[data-log]').forEach((input) => {
        const log = logs[Number(input.dataset.log)];
        if (input.value === '') delete log.values[input.dataset.metric];
        else log.values[input.dataset.metric] = Number(input.value);
      });
      overlay.querySelectorAll('[data-failure]').forEach((input) => { logs[Number(input.dataset.failure)].failure = input.checked; });
      const record = flowToRecord(training, [...doneIndexes].map((i) => queue[i]), logs, elapsedSec, completed);
      shutdown();
      onFinish(record);
    });
  }

  // 步驟結束時直接沿用預設值，不打斷訓練；想改才點提示
  function createLog(stepIndex) {
    const step = queue[stepIndex];
    if (!getSettings().autoLog || !step.metrics?.length) return null;
    const sources = [{ occurredAt: Infinity, data: { logs } }, ...events];
    const values = {};
    for (const metric of step.metrics) {
      const value = lastLogValue(sources, step.label, metric.name);
      if (value !== null) values[metric.name] = value;
    }
    const log = { stepIndex, label: step.label, exerciseId: step.exerciseId ?? null, loopState: step.loopState, values, failure: false };
    logs.push(log);
    return log;
  }

  function showNote(log) {
    clearTimeout(noteTimer);
    overlay.querySelector('.run-note')?.remove();
    const note = document.createElement('button');
    note.className = 'run-note';
    note.dataset.run = 'note';
    note.textContent = `${log.label} ${valueText(log) || t('run.tapToFill')}`;
    note.logRef = log;
    overlay.append(note);
    noteTimer = setTimeout(() => note.remove(), NOTE_SECONDS * 1000);
  }

  function openLog(log) {
    clearTimeout(noteTimer);
    overlay.querySelector('.run-note')?.remove();
    overlay.querySelector('.run-log')?.remove();
    const step = queue[log.stepIndex];
    const fields = step.metrics.map((metric, i) => `<div class="log-row"><span>${esc(metric.name)}</span>
      <div class="stepper">
        <button type="button" data-step-field="${i}" data-delta="-1" aria-label="-">−</button>
        <input type="number" inputmode="decimal" step="any" name="m${i}" value="${log.values[metric.name] ?? ''}" aria-label="${esc(metric.name)}">
        <button type="button" data-step-field="${i}" data-delta="1" aria-label="+">＋</button>
      </div></div>`).join('');
    const panel = document.createElement('form');
    panel.className = 'run-log';
    panel.innerHTML = `<strong>${esc(stepName(step))}</strong>${fields}
      <label class="check"><input type="checkbox" name="failure"${log.failure ? ' checked' : ''}> ${t('run.failure')}</label>
      <button class="btn primary wide">${t('run.saveLog')}</button>`;
    panel.addEventListener('click', (event) => {
      const button = event.target.closest('[data-step-field]');
      if (!button) return;
      const input = panel.elements[`m${button.dataset.stepField}`];
      input.value = Math.max(0, (Number(input.value) || 0) + Number(button.dataset.delta));
    });
    panel.addEventListener('submit', (event) => {
      event.preventDefault();
      const data = new FormData(panel);
      step.metrics.forEach((metric, i) => {
        if (data.get(`m${i}`) === '') delete log.values[metric.name];
        else log.values[metric.name] = Number(data.get(`m${i}`));
      });
      log.failure = data.get('failure') === 'on';
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
    speak(stepName(queue[index]));
    paint();
    persist();
  }

  function advance(markDone = true) {
    const finished = index;
    if (markDone) doneIndexes.add(index);
    const log = markDone ? createLog(finished) : null;
    if (index + 1 >= queue.length) { soundFinish(); finish(true, true); return; }
    enter(index + 1);
    if (log) showNote(log);
  }

  function timeline() {
    return queue.filter((step) => step.duration).map((step) => {
      const position = queue.indexOf(step);
      const stateClass = position < index ? ' past' : position === index ? ' now' : '';
      return `<i class="tone-${step.color}${stateClass}" style="flex:${step.duration}"></i>`;
    }).join('');
  }

  const left = () => (remaining ?? 0) + queue.slice(index + 1).reduce((sum, step) => sum + (step.duration ?? 0), 0);

  function paint() {
    const step = queue[index];
    const next = queue[index + 1];
    const settings = getSettings();
    const note = overlay.querySelector('.run-note');
    overlay.className = `run tone-${step.color}${paused ? ' paused' : ''}`;
    overlay.innerHTML = `
      <div class="run-top"><span>${loopText(step)}</span><span id="run-clock-left">${formatClock(left())}</span></div>
      <div class="run-main">
        <h1>${esc(stepName(step))}</h1>
        <div class="run-clock">${step.type === 'reps' ? t('step.times', { n: step.count }) : formatClock(remaining)}</div>
        <div class="run-progress"><i id="run-progress" style="width:100%"></i></div>
        <p class="run-hint">${t(step.type === 'reps' ? 'run.hintReps' : 'run.hintTimer')}</p>
      </div>
      <div class="run-next ${next ? `tone-${next.color}` : ''}">
        <span>${t('run.next')}</span><strong>${next ? `${esc(stepName(next))}　${stepAmount(next)}` : t('run.end')}</strong>
      </div>
      <div class="run-line track">${timeline()}</div>
      ${paused ? `<div class="run-menu">
        <button class="main" data-run="resume">${t('run.resume')}</button>
        <button data-run="log">${t('run.logNow')}</button>
        <button data-run="sound">${t(settings.sound ? 'run.soundOn' : 'run.soundOff')}</button>
        <button data-run="voice">${t(recognition ? 'run.voiceOn' : 'run.voiceOff')}</button>
        <button data-run="leave">${t('run.leave')}</button>
        <button data-run="stop">${t('run.stop')}</button>
      </div>` : ''}`;
    if (note) overlay.append(note);
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

  function askStop() {
    tapLocked = true;
    overlay.innerHTML = `<div class="run-main"><h1>${t('run.stopTitle')}</h1>
      <div class="run-menu"><button class="main" data-run="save">${t('run.savePartial')}</button><button data-run="discard">${t('run.discard')}</button><button data-run="back-menu">${t('run.resume')}</button></div></div>`;
    overlay.className = 'run tone-gray';
  }

  function showMessage(text) {
    clearTimeout(noteTimer);
    overlay.querySelector('.run-note')?.remove();
    const note = document.createElement('div');
    note.className = 'run-note message';
    note.textContent = text;
    overlay.append(note);
    noteTimer = setTimeout(() => note.remove(), NOTE_SECONDS * 1000);
  }

  function stopVoice() {
    const active = recognition;
    recognition = null;
    if (!active) return;
    active.onend = null;
    active.onerror = null;
    try { active.stop(); } catch { /* 已經停止 */ }
  }

  function voiceFailed() {
    stopVoice();
    paint();
    showMessage(t('run.voiceFailed'));
  }

  // 語音辨識在部分手機瀏覽器不能用；失敗就關閉，不能重試到卡住畫面
  function toggleVoice() {
    if (recognition) { stopVoice(); paint(); return; }
    const Speech = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!Speech) { voiceFailed(); return; }
    let startedAt = Date.now();
    let quickEnds = 0;
    const session = new Speech();
    recognition = session;
    session.continuous = true;
    session.lang = document.documentElement.lang;
    session.onresult = (event) => {
      const said = event.results[event.results.length - 1][0].transcript;
      if (/暫停|pause|stop/i.test(said) && !paused) togglePause();
      else if (/繼續|開始|resume|start/i.test(said) && paused) togglePause();
      else if (/下一|跳過|next|skip/i.test(said)) advance(false);
      else if (/完成|好了|done/i.test(said) && queue[index].type === 'reps') advance();
    };
    session.onerror = (event) => {
      if (event.error === 'no-speech' || event.error === 'aborted') return;
      voiceFailed();
    };
    session.onend = () => {
      if (recognition !== session) return;
      quickEnds = Date.now() - startedAt < 1500 ? quickEnds + 1 : 0;
      if (quickEnds >= 3) { voiceFailed(); return; }
      setTimeout(() => {
        if (recognition !== session) return;
        startedAt = Date.now();
        try { session.start(); } catch { voiceFailed(); }
      }, 400);
    };
    try { session.start(); } catch { voiceFailed(); return; }
    paint();
  }

  const handlers = {
    resume: togglePause,
    log: () => {
      const last = logs.at(-1) ?? createLog(Math.max(0, index - 1)) ?? createLog(index);
      if (last) openLog(last);
    },
    sound: () => { setSetting('sound', !getSettings().sound); paint(); },
    voice: toggleVoice,
    leave: () => { persist(); shutdown(); },
    stop: askStop,
    save: () => finish(false, true),
    discard: () => finish(false, false),
    'back-menu': () => { tapLocked = false; paint(); },
  };

  const isControl = (target) => target.closest('[data-run], .run-log, .run-note, .run-menu');

  overlay.addEventListener('click', (event) => {
    const button = event.target.closest('[data-run]');
    if (!button) return;
    wakeAudio();
    if (button.dataset.run === 'note') { openLog(button.logRef); return; }
    if (button.dataset.run !== 'save-summary') handlers[button.dataset.run]();
  });

  // 整個畫面都是按鈕：點一下暫停或完成，左右滑動切換步驟
  overlay.addEventListener('pointerdown', (event) => {
    if (tapLocked || isControl(event.target)) return;
    swipeStart = event.clientX;
  });
  overlay.addEventListener('pointerup', (event) => {
    if (swipeStart === null || isControl(event.target)) { swipeStart = null; return; }
    const moved = event.clientX - swipeStart;
    swipeStart = null;
    wakeAudio();
    if (moved <= -SWIPE_PIXELS) advance(false);
    else if (moved >= SWIPE_PIXELS) { if (index > 0) enter(index - 1); }
    else if (paused) togglePause();
    else if (queue[index].type === 'reps') advance();
    else togglePause();
  });

  // 手機切到背景時計時器會停，回來後依實際經過時間補上
  function catchUp() {
    if (paused || endsAt === null || Date.now() < endsAt) return;
    const overdue = (Date.now() - endsAt) / 1000;
    const result = advanceBy(queue, index, 0, overdue);
    result.passed.forEach((i) => { doneIndexes.add(i); createLog(i); });
    if (result.finished) { soundFinish(); finish(true, true); return; }
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
    wakeAudio();
    keepAwake();
    catchUp();
  }
  document.addEventListener('visibilitychange', onVisible);

  const timer = setInterval(() => {
    if (paused || endsAt === null) return;
    const settings = getSettings();
    remaining = Math.max(0, (endsAt - Date.now()) / 1000);
    const whole = Math.ceil(remaining);
    overlay.classList.toggle('pulse', settings.pulse && whole <= settings.countdownSec);
    if (whole <= settings.countdownSec && whole > 0 && whole !== lastBeep) {
      lastBeep = whole;
      soundCountdown();
      speak(String(whole));
    }
    if (remaining <= 0) { advance(); return; }
    overlay.querySelector('.run-clock').textContent = formatClock(remaining);
    overlay.querySelector('#run-progress').style.width = `${(remaining / queue[index].duration) * 100}%`;
    overlay.querySelector('#run-clock-left').textContent = formatClock(left());
  }, 200);

  speak(stepName(queue[index]));
  paint();
  persist();
}
