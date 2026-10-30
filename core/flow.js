// 巢狀訓練流程：純資料與計算。步驟可以是計時、休息、次數或循環
export const STEP_TYPES = ['timer', 'rest', 'reps', 'loop'];

export function newStep(type) {
  const id = crypto.randomUUID();
  if (type === 'loop') return { id, type, iterations: 3, children: [] };
  if (type === 'reps') return { id, type, label: '', count: 10 };
  return { id, type, label: '', duration: type === 'rest' ? 30 : 10 };
}

export function flattenFlow(flow = []) {
  return flow.flatMap((step) => {
    if (step.type !== 'loop') return [step];
    const inner = flattenFlow(step.children);
    return Array.from({ length: step.iterations }, () => inner).flat();
  });
}

// 次數步驟要等使用者按完成，總時間只算有秒數的步驟
export const flowDuration = (flow) => flattenFlow(flow).reduce((sum, step) => sum + (step.duration ?? 0), 0);

export function mapFlow(flow, fn) {
  return flow.map((step) => (step.type === 'loop' ? { ...step, children: mapFlow(step.children, fn) } : fn(step)));
}

// 把做完的步驟合併成紀錄格式，分析和手動紀錄共用同一套欄位
export function flowToRecord(training, doneSteps, elapsedSec, completed) {
  const moves = new Map();
  for (const step of doneSteps) {
    if (step.type === 'rest' || !step.label) continue;
    const key = step.exerciseId ?? step.label;
    const move = moves.get(key) ?? { exerciseId: step.exerciseId ?? null, name: step.label, metrics: {} };
    if (step.type === 'reps') move.metrics.reps = (move.metrics.reps ?? 0) + step.count;
    else move.metrics.holdSec = (move.metrics.holdSec ?? 0) + step.duration;
    moves.set(key, move);
  }
  return {
    trainingId: training.id, activity: training.name, mode: 'flow', sets: 1,
    moves: [...moves.values()], extra: {}, durationSec: Math.round(elapsedSec), completed,
  };
}

const loop = (iterations, children) => ({ id: crypto.randomUUID(), type: 'loop', iterations, children });
const step = (type, label, value) => ({ ...newStep(type), label, ...(type === 'reps' ? { count: value } : { duration: value }) });

// 範例：使用者按加入才會建立，內容都可以改
export const FLOW_TEMPLATES = {
  hang: (t) => ({ name: t('template.hang'), flow: [step('rest', t('step.prepare'), 10), loop(6, [step('timer', t('template.hangMove'), 7), step('rest', t('step.rest'), 3)])] }),
  interval: (t) => ({ name: t('template.interval'), flow: [step('rest', t('step.prepare'), 10), loop(8, [step('timer', t('template.fast'), 60), step('rest', t('template.easy'), 60)])] }),
  strength: (t) => ({ name: t('template.strength'), flow: [step('rest', t('step.prepare'), 10), loop(3, [step('reps', t('template.squat'), 10), step('rest', t('step.rest'), 90), step('reps', t('template.pullUp'), 5), step('rest', t('step.rest'), 90)])] }),
};
