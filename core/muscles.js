// 肌肉疲勞是依使用者規則算出的數值，不是測量。模型用清單管理，每塊肌肉可以用不同模型
const mirror = (points) => points.map(([x, y]) => [200 - x, y]);
const both = (points) => [points, mirror(points)];

export const MUSCLES = [
  { id: 'chest', view: 'front', shapes: both([[78, 80], [100, 84], [100, 116], [78, 118], [66, 104]]) },
  { id: 'deltFront', view: 'front', shapes: both([[56, 80], [76, 72], [78, 80], [64, 104], [50, 98]]) },
  { id: 'biceps', view: 'front', shapes: both([[50, 102], [64, 108], [62, 146], [46, 142]]) },
  { id: 'forearm', view: 'both', shapes: both([[46, 146], [62, 150], [56, 200], [40, 196]]) },
  { id: 'abs', view: 'front', shapes: [[[86, 120], [114, 120], [112, 184], [88, 184]]] },
  { id: 'obliques', view: 'front', shapes: both([[70, 120], [84, 120], [86, 184], [72, 180]]) },
  { id: 'quads', view: 'front', shapes: both([[72, 192], [96, 196], [92, 290], [70, 286]]) },
  { id: 'adductors', view: 'front', shapes: both([[97, 196], [100, 194], [100, 268], [94, 280]]) },
  { id: 'calfFront', view: 'front', shapes: both([[72, 298], [92, 300], [88, 386], [74, 386]]) },
  { id: 'traps', view: 'back', shapes: [[[78, 72], [122, 72], [112, 114], [88, 114]]] },
  { id: 'deltRear', view: 'back', shapes: both([[56, 80], [76, 72], [84, 112], [64, 104], [50, 98]]) },
  { id: 'triceps', view: 'back', shapes: both([[50, 102], [64, 108], [62, 146], [46, 142]]) },
  { id: 'lats', view: 'back', shapes: both([[66, 108], [86, 116], [98, 164], [74, 160]]) },
  { id: 'lowerBack', view: 'back', shapes: [[[84, 164], [116, 164], [114, 190], [86, 190]]] },
  { id: 'glutes', view: 'back', shapes: both([[70, 192], [99, 192], [99, 234], [68, 232]]) },
  { id: 'hamstrings', view: 'back', shapes: both([[70, 238], [97, 238], [92, 294], [70, 290]]) },
  { id: 'calfBack', view: 'back', shapes: both([[72, 298], [92, 300], [88, 386], [74, 386]]) },
];

// 男女共用的中性輪廓
const SILHOUETTE = ['M100 12a21 21 0 1 0 .01 0Z', 'M90 56h20v18H90Z',
  'M60 74Q100 62 140 74L152 98 156 146 162 196 166 228H148L142 200 138 150 134 112V192L132 300 128 392H108L104 300 100 272 96 300 92 392H72L68 300 66 192V112L62 150 58 200 52 228H34L38 196 44 146 48 98Z'];

export const silhouettePaths = () => SILHOUETTE;

export const shapePoints = (shape) => shape.map(([x, y]) => `${x},${y}`).join(' ');

// 常見動作的起始對應，使用者可以改
export const EXERCISE_PRESETS = {
  引體向上: { lats: 50, biceps: 30, forearm: 20 },
  伏地挺身: { chest: 50, triceps: 30, deltFront: 20 },
  深蹲: { quads: 50, glutes: 35, hamstrings: 15 },
  硬舉: { hamstrings: 35, glutes: 30, lowerBack: 35 },
  走路: { calfBack: 40, quads: 30, glutes: 30 },
  跑步: { quads: 35, calfBack: 35, hamstrings: 30 },
  懸掛: { forearm: 80, lats: 20 },
  懸吊: { forearm: 80, lats: 20 },
  攀岩: { forearm: 50, lats: 30, biceps: 20 },
};

export function presetMuscles(name) {
  const key = Object.keys(EXERCISE_PRESETS).find((item) => name.includes(item));
  return key ? Object.entries(EXERCISE_PRESETS[key]).map(([muscle, percent]) => ({ muscle, percent })) : [];
}

export const DEFAULT_FATIGUE = { halfLife: 24, muscles: {} };

// 新增模型只要在這裡加一項
export const MODELS = {
  halfLife: { decay: (hours, params) => 0.5 ** (hours / params.halfLife) },
};

export function modelOf(settings, muscle) {
  const own = settings.muscles?.[muscle] ?? {};
  return { model: own.model ?? 'halfLife', halfLife: own.halfLife ?? settings.halfLife ?? DEFAULT_FATIGUE.halfLife };
}

const INTENSITY = { easy: 0.6, moderate: 1, hard: 1.4 };

// 負荷：有時間用時間，沒有時間用次數；有 RPE 時用 RPE 當係數
export function loadOf(data) {
  const minutes = (data.durationSec ?? 0) / 60;
  const reps = (data.moves ?? []).reduce((total, move, i) => total + (move.metrics?.reps ?? 0) * (data.sets ?? 0) + (data.extra?.[i] ?? 0), 0);
  const base = minutes || reps * 0.2 || 5;
  const factor = data.rpe != null ? data.rpe / 5 : INTENSITY[data.intensity] ?? 1;
  return base * factor;
}

// 計算某時間點每塊肌肉的數值，並保留來源
export function fatigueAt(events, exercises, settings, time) {
  const result = new Map();
  for (const event of events) {
    if (event.occurredAt > time) continue;
    const moves = (event.data.moves ?? []).filter((move) => move.exerciseId);
    const ids = moves.length ? moves.map((move) => move.exerciseId) : [event.data.exerciseId].filter(Boolean);
    const load = loadOf(event.data) / (ids.length || 1);
    for (const id of ids) {
      for (const { muscle, percent } of exercises.find((e) => e.id === id)?.muscles ?? []) {
        const params = modelOf(settings, muscle);
        const hours = (time - event.occurredAt) / 3600000;
        const value = load * (percent / 100) * MODELS[params.model].decay(hours, params);
        const entry = result.get(muscle) ?? { muscle, value: 0, sources: [] };
        entry.value += value;
        entry.sources.push({ eventId: event.id, activity: event.data.activity, date: event.date, value });
        result.set(muscle, entry);
      }
    }
  }
  return result;
}

export function unmappedExercises(events, exercises) {
  const ids = new Set(events.flatMap((event) => [event.data.exerciseId, ...(event.data.moves ?? []).map((move) => move.exerciseId)]).filter(Boolean));
  return exercises.filter((exercise) => ids.has(exercise.id) && !exercise.muscles?.length);
}
