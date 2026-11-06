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

// 關節不畫在人形圖上，只用在主觀感受
export const JOINTS = ['neck', 'shoulder', 'spine', 'elbow', 'wrist', 'hip', 'knee', 'ankle'];

export const silhouettePaths = () => SILHOUETTE;

const round1 = (n) => Math.round(n * 10) / 10;
export const shapePoints = (shape) => shape.map(([x, y]) => `${round1(x)},${round1(y)}`).join(' ');

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

// fullLoad：負荷到這個數值就是 10 分；levels：綠黃、黃橘、橘紅的分界
export const DEFAULT_FATIGUE = { detail: 'basic', halfLife: 24, fullLoad: 60, systemicLoad: 400, shortMinutes: 15, levels: [2, 5, 8], muscles: {} };

// 新增模型只要在這裡加一項
export const MODELS = {
  halfLife: { decay: (hours, params) => 0.5 ** (hours / params.halfLife) },
};

export function modelOf(settings, muscle) {
  const own = settings.muscles?.[muscle] ?? {};
  return { model: own.model ?? 'halfLife', halfLife: own.halfLife ?? settings.halfLife ?? DEFAULT_FATIGUE.halfLife };
}

const INTENSITY = { easy: 0.6, moderate: 1, hard: 1.4 };
const LIMIT_BOOST = 1.25;
export const LIMITS = ['muscle', 'cardio', 'both'];

// 沒有最大能力的測量，也不拿單項能力換算多項課表：以當天的 RPE 和停下來的原因為準
const rpeOf = (data) => data.rpe ?? (data.limit ? 9 : null);

// 短而高強度的訓練不能只看時間，RPE 7 以上至少算 shortMinutes 分鐘
function minutesOf(data, settings) {
  const reps = (data.moves ?? []).reduce((total, move, i) => total + (move.metrics?.reps ?? 0) * (data.sets ?? 0) + (data.extra?.[i] ?? 0), 0);
  const minutes = (data.durationSec ?? 0) / 60 || reps * 0.2 || 5;
  return rpeOf(data) >= 7 ? Math.max(minutes, settings.shortMinutes ?? DEFAULT_FATIGUE.shortMinutes) : minutes;
}

// 負荷：時間乘上 RPE 係數；到達極限的部分加重
export function loadOf(data, settings = DEFAULT_FATIGUE) {
  const rpe = rpeOf(data);
  const factor = rpe != null ? rpe / 5 : INTENSITY[data.intensity] ?? 1;
  return minutesOf(data, settings) * factor * (['muscle', 'both'].includes(data.limit) ? LIMIT_BOOST : 1);
}

// 全身／心肺：session-RPE（RPE × 分鐘）
export function systemicLoadOf(data, settings = DEFAULT_FATIGUE) {
  const rpe = rpeOf(data) ?? { easy: 3, moderate: 5, hard: 7 }[data.intensity] ?? 5;
  return minutesOf(data, settings) * rpe * (['cardio', 'both'].includes(data.limit) ? LIMIT_BOOST : 1);
}

// 整次訓練算一個負荷，用最多的肌肉當 100%，動作多不會把負荷稀釋掉
function muscleShares(ids, exercises, own = []) {
  const shares = new Map();
  for (const id of ids) {
    for (const { muscle, percent } of exercises.find((e) => e.id === id)?.muscles ?? []) shares.set(muscle, (shares.get(muscle) ?? 0) + percent);
  }
  // 紀錄本身選的部位，用在沒有動作清單的課程，例如瑜伽課
  for (const { muscle, percent } of own) shares.set(muscle, (shares.get(muscle) ?? 0) + percent);
  const top = Math.max(0, ...shares.values());
  return [...shares].map(([muscle, value]) => [muscle, value / top]);
}

const decayed = (settings, key, event, time) => {
  const params = modelOf(settings, key);
  return MODELS[params.model].decay((time - event.occurredAt) / 3600000, params);
};

// 計算某時間點每塊肌肉的數值，並保留來源
export function fatigueAt(events, exercises, settings, time) {
  const result = new Map();
  for (const event of events) {
    if (event.occurredAt > time) continue;
    const moves = (event.data.moves ?? []).filter((move) => move.exerciseId);
    const ids = moves.length ? moves.map((move) => move.exerciseId) : [event.data.exerciseId].filter(Boolean);
    const load = loadOf(event.data, settings);
    for (const [muscle, share] of muscleShares(ids, exercises, event.data.muscles)) {
      const value = load * share * decayed(settings, muscle, event, time);
      const entry = result.get(muscle) ?? { muscle, value: 0, sources: [] };
      entry.value += value;
      entry.sources.push({ eventId: event.id, activity: event.data.activity, date: event.date, value });
      result.set(muscle, entry);
    }
  }
  return result;
}

export function systemicAt(events, settings, time) {
  return events.filter((event) => event.occurredAt <= time)
    .reduce((total, event) => total + systemicLoadOf(event.data, settings) * decayed(settings, 'systemic', event, time), 0);
}

export function scoreOf(value, settings) {
  return Math.min(10, (value / (settings.fullLoad ?? DEFAULT_FATIGUE.fullLoad)) * 10);
}

export function systemicScore(value, settings) {
  return Math.min(10, (value / (settings.systemicLoad ?? DEFAULT_FATIGUE.systemicLoad)) * 10);
}

export function levelOf(score, settings) {
  if (score < 0.1) return 'none';
  const [low, mid, high] = settings.levels ?? DEFAULT_FATIGUE.levels;
  if (score < low) return 'low';
  if (score < mid) return 'mid';
  return score < high ? 'high' : 'top';
}

export function unmappedExercises(events, exercises) {
  const ids = new Set(events.flatMap((event) => [event.data.exerciseId, ...(event.data.moves ?? []).map((move) => move.exerciseId)]).filter(Boolean));
  return exercises.filter((exercise) => ids.has(exercise.id) && !exercise.muscles?.length);
}

// ---- 細分程度：基礎只有大部位，中等是原本的 17 區，進階再細分 ----
// 資料存在使用者選的那一層；沒有細到的資料就留在上一層顯示，不假裝更精確
export const DETAILS = ['basic', 'mid', 'advanced'];
const DEPTH = { basic: 1, mid: 2, advanced: 3 };

export const GROUPS = {
  g_chest: ['chest'],
  g_shoulder: ['deltFront', 'deltRear'],
  g_back: ['traps', 'lats'],
  g_arm: ['biceps', 'triceps', 'forearm'],
  g_core: ['abs', 'obliques', 'lowerBack'],
  g_glutes: ['glutes'],
  g_thigh: ['quads', 'hamstrings', 'adductors'],
  g_calf: ['calfFront', 'calfBack'],
};

const lerp = (p, q, t) => [p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t];

// 四邊形 [左上, 右上, 右下, 左下]：strips 切成直條（由外到內），rows 切成橫條（由上到下）
const strips = (cuts) => ([a, b, c, d]) => cuts.slice(0, -1).map((t, i) => [lerp(a, b, t), lerp(a, b, cuts[i + 1]), lerp(d, c, cuts[i + 1]), lerp(d, c, t)]);
const rows = (cuts) => ([a, b, c, d]) => cuts.slice(0, -1).map((t, i) => [lerp(a, d, t), lerp(b, c, t), lerp(b, c, cuts[i + 1]), lerp(a, d, cuts[i + 1])]);

function clipY(poly, y, below) {
  const keep = (p) => (below ? p[1] >= y : p[1] <= y);
  const out = [];
  poly.forEach((p, i) => {
    const q = poly[(i + 1) % poly.length];
    if (keep(p)) out.push(p);
    if (keep(p) !== keep(q)) out.push([p[0] + ((q[0] - p[0]) * (y - p[1])) / (q[1] - p[1]), y]);
  });
  return out;
}
const splitAtY = (y) => (poly) => [clipY(poly, y, false), clipY(poly, y, true)];

// 中等區域再切開的進階區域
const SPLITS = {
  chest: [['upperChest', 'lowerChest'], splitAtY(100)],
  abs: [['upperAbs', 'lowerAbs'], splitAtY(152)],
  traps: [['upperTraps', 'midLowerTraps'], splitAtY(90)],
  quads: [['vastusLateralis', 'rectusFemoris', 'vastusMedialis'], strips([0, 0.3, 0.65, 1])],
  hamstrings: [['bicepsFemoris', 'semitendinosus'], strips([0, 0.55, 1])],
  glutes: [['gluteMed', 'gluteMax'], rows([0, 0.4, 1])],
  calfBack: [['gastrocnemius', 'soleus'], rows([0, 0.6, 1])],
};

// 深層肌肉只能示意位置，疊在原區域上，不切開原區域
const EXTRAS = [
  { id: 'hipFlexors', parent: 'quads', view: 'front', shapes: both([[76, 184], [99, 186], [98, 194], [74, 192]]) },
  { id: 'hipRotators', parent: 'glutes', view: 'back', shapes: both([[84, 204], [98, 204], [98, 218], [84, 218]]) },
  { id: 'rotatorCuff', parent: 'deltRear', view: 'back', shapes: both([[66, 88], [82, 90], [82, 104], [66, 102]]) },
];

const partsOf = (view, shapes) => (view === 'both' ? ['front', 'back'] : [view]).flatMap((v) => shapes.map((points) => ({ view: v, points })));

const NODES = new Map();
const addNode = (node) => NODES.set(node.id, { children: [], ...node });
Object.keys(GROUPS).forEach((id) => addNode({ id, depth: 1, parent: null }));
for (const [groupId, members] of Object.entries(GROUPS)) {
  for (const id of members) {
    const muscle = MUSCLES.find((m) => m.id === id);
    addNode({ id, depth: 2, parent: groupId, parts: partsOf(muscle.view, muscle.shapes) });
  }
}
for (const [parent, [ids, slicer]] of Object.entries(SPLITS)) {
  const muscle = MUSCLES.find((m) => m.id === parent);
  const pieces = muscle.shapes.map(slicer);
  ids.forEach((id, i) => addNode({ id, depth: 3, parent, parts: partsOf(muscle.view, pieces.map((list) => list[i])) }));
}
EXTRAS.forEach((extra) => addNode({ id: extra.id, depth: 3, parent: extra.parent, overlay: true, parts: partsOf(extra.view, extra.shapes) }));
NODES.forEach((node) => { if (node.parent) NODES.get(node.parent).children.push(node); });
Object.keys(GROUPS).forEach((id) => { const group = NODES.get(id); group.parts = group.children.flatMap((child) => child.parts); });

// 要畫的區域和數值：數值 = 自己的 + 底下最高的 + 上層沒有細分的部分；深層肌肉只看自己的資料
export function regionsAt(detail, values) {
  const depth = DEPTH[detail] ?? 1;
  const own = (id) => values.get(id)?.value ?? 0;
  const memo = new Map();
  const down = (node) => {
    if (!memo.has(node.id)) memo.set(node.id, own(node.id) + Math.max(0, ...node.children.map(down)));
    return memo.get(node.id);
  };
  const inherited = (node) => {
    let total = 0;
    for (let p = NODES.get(node.parent); p; p = NODES.get(p.parent)) total += own(p.id);
    return total;
  };
  const picked = [];
  const visit = (node) => {
    const real = node.children.filter((child) => !child.overlay);
    const split = node.depth < depth && real.length > 0 && (own(node.id) === 0 || real.some((child) => down(child) > 0));
    if (split) real.forEach(visit);
    else picked.push(node);
    if (depth >= 3) node.children.filter((child) => child.overlay).forEach((child) => picked.push(child));
  };
  Object.keys(GROUPS).forEach((id) => visit(NODES.get(id)));
  return picked.map((node) => ({ id: node.id, depth: node.depth, parts: node.parts, value: down(node) + (node.overlay ? 0 : inherited(node)) }));
}

// 某區域的數值來自哪些部位：自己、底下的和上層的
export function relatedIds(id) {
  const node = NODES.get(id);
  if (!node) return [id];
  const ids = [];
  const walk = (current) => { ids.push(current.id); current.children.forEach(walk); };
  walk(node);
  for (let p = NODES.get(node.parent); p; p = NODES.get(p.parent)) ids.push(p.id);
  return ids;
}

// 設定動作或記錄感受時可以選的部位
export const pickableAt = (detail) => regionsAt(detail, new Map()).map((region) => region.id);
