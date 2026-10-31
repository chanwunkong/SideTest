// 課表內容：純資料與計算。步驟是計時、次數或循環，循環可以放循環
export const COLORS = ['gray', 'red', 'orange', 'amber', 'green', 'teal', 'blue', 'indigo', 'violet', 'pink'];

// 上一版的記錄數值是代碼，讀取時轉成名稱
const OLD_METRICS = { reps: '次數', weightKg: '重量', holdSec: '秒數', distanceM: '距離' };
const toMetric = (metric) => (typeof metric === 'string' ? { name: OLD_METRICS[metric] ?? metric } : metric);

export function newStep(type, props = {}) {
  const id = crypto.randomUUID();
  if (type === 'loop') return { id, type, iterations: 3, children: [], ...props };
  if (type === 'reps') return { id, type, label: '', count: 5, estimate: null, color: 'blue', metrics: [], ...props };
  return { id, type: 'timer', label: '', duration: 10, color: 'orange', rest: false, skipOnLast: false, metrics: [], ...props };
}

// 上一版的 rest 步驟轉成計時步驟
export function normalizeFlow(flow = []) {
  return flow.map((step) => {
    if (step.type === 'loop') return { ...step, children: normalizeFlow(step.children) };
    if (step.type === 'rest') return { ...newStep('timer'), ...step, type: 'timer', rest: true, color: step.color ?? 'green', metrics: (step.metrics ?? []).map(toMetric) };
    const normalized = { ...newStep(step.type), ...step };
    return { ...normalized, metrics: normalized.metrics.map(toMetric) };
  });
}

// 展開成實際順序，每步帶著循環位置；最後一次略過的步驟不會出現在最後一輪
export function flattenFlow(flow = [], loops = []) {
  return flow.flatMap((step) => {
    if (step.type !== 'loop') {
      const last = loops.at(-1);
      if (step.skipOnLast && last && last.current === last.total) return [];
      return [{ ...step, loopState: loops }];
    }
    return Array.from({ length: step.iterations }, (_, i) => flattenFlow(step.children, [...loops, { current: i + 1, total: step.iterations }])).flat();
  });
}

export const flowDuration = (flow) => flattenFlow(flow).reduce((sum, step) => sum + (step.duration ?? step.estimate ?? 0), 0);

export const formatClock = (seconds) => {
  const whole = Math.ceil(seconds);
  return `${Math.floor(whole / 60)}:${String(whole % 60).padStart(2, '0')}`;
};

export function formatDuration(seconds, t) {
  const minutes = Math.floor(seconds / 60);
  const rest = seconds % 60;
  return [minutes && t('time.minutes', { n: minutes }), rest && t('time.seconds', { n: rest })].filter(Boolean).join(' ') || t('time.seconds', { n: 0 });
}

// 依經過秒數往前推算步驟；次數步驟要等使用者按完成，所以停在那裡
export function advanceBy(queue, index, remaining, seconds) {
  const passed = [];
  let left = seconds;
  let current = index;
  let stepLeft = remaining;
  while (stepLeft !== null && left >= stepLeft) {
    left -= stepLeft;
    passed.push(current);
    current += 1;
    if (current >= queue.length) return { index: current, remaining: 0, passed, finished: true };
    stepLeft = queue[current].duration ?? null;
  }
  return { index: current, remaining: stepLeft === null ? null : stepLeft - left, passed, finished: false };
}

export function mapFlow(flow, fn) {
  return flow.map((step) => (step.type === 'loop' ? { ...step, children: mapFlow(step.children, fn) } : fn(step)));
}

// 舊紀錄裡同一步驟上次填的值，用來預先帶入
export function lastLogValue(events, label, metric) {
  const records = [...events].sort((a, b) => b.occurredAt - a.occurredAt);
  for (const event of records) {
    const log = event.data.logs?.findLast((entry) => entry.label === label && entry.values?.[metric] != null);
    if (log) return log.values[metric];
  }
  return null;
}

// 做完的步驟合併成運動紀錄，分析和手動紀錄共用同一套欄位
export function flowToRecord(training, doneSteps, logs, elapsedSec, completed) {
  const moves = new Map();
  for (const step of doneSteps) {
    if (step.rest || !step.label) continue;
    const key = step.exerciseId ?? step.label;
    const move = moves.get(key) ?? { exerciseId: step.exerciseId ?? null, name: step.label, metrics: {} };
    if (step.type === 'reps') move.metrics.reps = (move.metrics.reps ?? 0) + step.count;
    else move.metrics.holdSec = (move.metrics.holdSec ?? 0) + step.duration;
    moves.set(key, move);
  }
  return {
    trainingId: training.id, activity: training.name, mode: 'flow', sets: 1, extra: {},
    moves: [...moves.values()], logs, durationSec: Math.round(elapsedSec), completed,
  };
}

const timer = (label, duration, color, extra = {}) => newStep('timer', { label, duration, color, ...extra });
const rest = (label, duration, extra = {}) => timer(label, duration, 'green', { rest: true, ...extra });
const reps = (label, count, color, metricName) => newStep('reps', { label, count, color, metrics: [{ name: metricName }] });
const loop = (iterations, children) => newStep('loop', { iterations, children });

// 內建範本：按加入才建立，內容都可以改
export const TEMPLATES = {
  max: (t) => ({ name: t('template.max'), tags: [t('tag.strength'), t('tag.finger')], flow: [rest(t('step.prepare'), 10), loop(5, [timer(t('template.hang'), 10, 'red', { metrics: [{ name: t('log.weight') }] }), rest(t('step.rest'), 180, { skipOnLast: true })])] }),
  repeaters: (t) => ({ name: t('template.repeaters'), tags: [t('tag.endurance'), t('tag.finger')], flow: [rest(t('step.prepare'), 10), loop(3, [loop(6, [timer(t('template.hang'), 7, 'red'), rest(t('step.rest'), 3, { skipOnLast: true })]), rest(t('template.setRest'), 180, { skipOnLast: true })])] }),
  interval: (t) => ({ name: t('template.interval'), tags: [t('tag.cardio')], flow: [rest(t('step.prepare'), 10), loop(8, [timer(t('template.fast'), 60, 'orange'), rest(t('template.easy'), 60, { skipOnLast: true })])] }),
  strength: (t) => ({ name: t('template.strength'), tags: [t('tag.strength')], flow: [rest(t('step.prepare'), 10), loop(5, [reps(t('template.squat'), 5, 'blue', t('log.reps')), rest(t('step.rest'), 180, { skipOnLast: true })]), rest(t('template.change'), 300), loop(5, [reps(t('template.pullUp'), 5, 'indigo', t('log.reps')), rest(t('step.rest'), 180, { skipOnLast: true })])] }),
};
