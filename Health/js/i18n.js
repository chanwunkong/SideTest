// 語言檔：新頁面的文字都放這裡，資料只存代碼
const STRINGS = {
  'zh-Hant': {
    'common.save': '儲存', 'common.delete': '刪除', 'common.add': '新增', 'common.edit': '修改', 'common.none': '—',
    'tab.home': '今日', 'tab.diet': '飲食', 'tab.workout': '運動', 'tab.rest': '休息', 'tab.body': '身體',
    'settings.language': '語言',
    'diet.title': '飲食', 'diet.paste': '貼上 Gemini 結果', 'diet.manual': '手動輸入', 'diet.prompt': '查看 Gemini 提示詞',
    'diet.today': '今天', 'diet.average': '7 天平均', 'diet.target': '目標', 'diet.setTargets': '設定目標', 'diet.noMeals': '這天還沒有餐點紀錄',
    'diet.days': '{n} 天有紀錄', 'diet.targetsHint': '只有你填的項目才會顯示目標，留空就不設定。', 'diet.saved': '已記錄這一餐', 'diet.more': '其他項目',
    'diet.pasteTitle': '貼上 Gemini 結果', 'diet.pasteHint': '把 Gemini 的回覆貼在這裡，貼上後會自動讀取。', 'diet.confirm': '確認餐點', 'diet.editMeal': '修改餐點', 'diet.note': '名稱或備註',
    'workout.title': '運動', 'workout.today': '今天', 'workout.noneToday': '今天沒有排程', 'workout.free': '記錄其他運動',
    'workout.trainings': '訓練', 'workout.schedules': '排程', 'workout.week': '本週', 'workout.recent': '最近紀錄', 'workout.noRecords': '還沒有運動紀錄',
    'workout.newTraining': '新增訓練', 'workout.newSchedule': '新增排程', 'workout.noTrainings': '先新增一個訓練，例如一套要做哪些動作。', 'workout.days': '訓練天數', 'workout.sets': '套數', 'workout.time': '總時間', 'workout.minutes': '{n} 分鐘', 'workout.setsValue': '{n} 套',
    'training.name': '訓練名稱', 'training.mode': '記錄方式', 'training.moves': '一套的內容', 'training.addMove': '加入動作', 'training.trackRest': '記錄休息時間', 'training.moreMetrics': '更多指標', 'training.moveName': '動作名稱',
    'mode.check': '有做就好', 'mode.sets': '累積套數',
    'metric.reps': '次數', 'metric.holdSec': '維持秒數', 'metric.weightKg': '負重（kg）', 'metric.distanceM': '距離（m）', 'metric.gripType': '握法', 'metric.edgeMm': '岩點寬度（mm）',
    'schedule.training': '要做的訓練', 'schedule.start': '開始日期', 'schedule.days': '持續幾天', 'schedule.daysHint': '留空代表一直持續', 'schedule.weekdays': '哪幾天', 'schedule.weekdaysHint': '都不選代表每天', 'schedule.target': '目標套數', 'schedule.targetHint': '留空代表有記錄就算做到', 'schedule.everyday': '每天', 'schedule.remaining': '還有 {n} 天', 'schedule.ongoing': '持續中', 'schedule.ended': '已結束',
    'record.title': '記錄 {name}', 'record.sets': '今天做了幾套', 'record.done': '完成', 'record.detail': '記錄更多細節', 'record.extra': '{name}另外多做', 'record.rest': '休息時間（秒）', 'record.date': '日期', 'record.saved': '已記錄運動', 'record.target': '目標 {n} 套',
    'unit.total': '總共',
    'nutrient.water_ml': '水分', 'nutrient.protein_g': '蛋白質', 'nutrient.carbs_g': '碳水', 'nutrient.fat_g': '脂肪', 'nutrient.kcal': '熱量', 'nutrient.caffeine_mg': '咖啡因', 'nutrient.creatine_g': '肌酸',
  },
  en: {
    'common.save': 'Save', 'common.delete': 'Delete', 'common.add': 'Add', 'common.edit': 'Edit', 'common.none': '—',
    'tab.home': 'Today', 'tab.diet': 'Food', 'tab.workout': 'Training', 'tab.rest': 'Rest', 'tab.body': 'Body',
    'settings.language': 'Language',
    'diet.title': 'Food', 'diet.paste': 'Paste Gemini result', 'diet.manual': 'Enter manually', 'diet.prompt': 'View Gemini prompt',
    'diet.today': 'Today', 'diet.average': '7-day avg', 'diet.target': 'Target', 'diet.setTargets': 'Set targets', 'diet.noMeals': 'No meals logged for this day',
    'diet.days': '{n} days logged', 'diet.targetsHint': 'Only the items you fill in will show a target. Leave the rest blank.', 'diet.saved': 'Meal saved', 'diet.more': 'Other items',
    'diet.pasteTitle': 'Paste Gemini result', 'diet.pasteHint': 'Paste the reply from Gemini here. It is read automatically.', 'diet.confirm': 'Confirm meal', 'diet.editMeal': 'Edit meal', 'diet.note': 'Name or note',
    'workout.title': 'Training', 'workout.today': 'Today', 'workout.noneToday': 'Nothing scheduled today', 'workout.free': 'Log another workout',
    'workout.trainings': 'Workouts', 'workout.schedules': 'Schedules', 'workout.week': 'This week', 'workout.recent': 'Recent', 'workout.noRecords': 'No workouts yet',
    'workout.newTraining': 'New workout', 'workout.newSchedule': 'New schedule', 'workout.noTrainings': 'Add a workout first, such as the moves in one round.', 'workout.days': 'Days trained', 'workout.sets': 'Rounds', 'workout.time': 'Total time', 'workout.minutes': '{n} min', 'workout.setsValue': '{n} rounds',
    'training.name': 'Workout name', 'training.mode': 'How to log it', 'training.moves': 'One round', 'training.addMove': 'Add move', 'training.trackRest': 'Log rest time', 'training.moreMetrics': 'More metrics', 'training.moveName': 'Move name',
    'mode.check': 'Just done', 'mode.sets': 'Count rounds',
    'metric.reps': 'Reps', 'metric.holdSec': 'Hold (sec)', 'metric.weightKg': 'Added weight (kg)', 'metric.distanceM': 'Distance (m)', 'metric.gripType': 'Grip', 'metric.edgeMm': 'Edge width (mm)',
    'schedule.training': 'Workout', 'schedule.start': 'Start date', 'schedule.days': 'Number of days', 'schedule.daysHint': 'Leave blank to keep going', 'schedule.weekdays': 'Days of the week', 'schedule.weekdaysHint': 'None selected means every day', 'schedule.target': 'Target rounds', 'schedule.targetHint': 'Leave blank to count any log as done', 'schedule.everyday': 'Every day', 'schedule.remaining': '{n} days left', 'schedule.ongoing': 'Ongoing', 'schedule.ended': 'Ended',
    'record.title': 'Log {name}', 'record.sets': 'Rounds today', 'record.done': 'Done', 'record.detail': 'Add more detail', 'record.extra': 'Extra {name}', 'record.rest': 'Rest (sec)', 'record.date': 'Date', 'record.saved': 'Workout saved', 'record.target': 'Target {n} rounds',
    'unit.total': 'Total',
    'nutrient.water_ml': 'Water', 'nutrient.protein_g': 'Protein', 'nutrient.carbs_g': 'Carbs', 'nutrient.fat_g': 'Fat', 'nutrient.kcal': 'Calories', 'nutrient.caffeine_mg': 'Caffeine', 'nutrient.creatine_g': 'Creatine',
  },
};

const KEY = 'health.lang';
const stored = () => { try { return localStorage.getItem(KEY); } catch { return null; } };

export const LANGUAGES = [{ value: 'zh-Hant', label: '繁體中文' }, { value: 'en', label: 'English' }];
export let lang = stored() ?? (navigator.language?.startsWith('zh') ? 'zh-Hant' : 'en');

export function setLang(value) {
  lang = value;
  try { localStorage.setItem(KEY, value); } catch { /* 無法保存時只影響這次 */ }
}

export function t(key, vars = {}) {
  const text = STRINGS[lang]?.[key] ?? STRINGS['zh-Hant'][key] ?? key;
  return text.replace(/\{(\w+)\}/g, (_, name) => vars[name] ?? '');
}
