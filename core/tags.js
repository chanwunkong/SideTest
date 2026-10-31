// 舊版標籤庫：內容沿用舊版，使用者新增的標籤維持原文
export const TAG_GROUPS = {
  'muscle-group': ['全身', '核心', '上肢', '下肢', '背部肌群', '胸部肌群', '腹部肌群', '肩部肌群', '二頭肌', '三頭肌', '小臂肌群', '臀肌', '股四頭肌', '膕繩肌', '小腿肌群'],
  muscle: ['斜方肌', '菱形肌', '背闊肌', '豎脊肌', '大圓肌', '胸大肌上束', '前臂屈肌群', '前臂伸肌群', '臀大肌'],
  fitness: ['肌力', '耐力', '速度', '柔軟度', '最大肌力', '爆發力', '肌耐力', '有氧耐力', '活動度'],
  zones: ['Zone 1', 'Zone 2', 'Zone 3', 'Zone 4', 'Zone 5', 'ATP-PC', '糖解系統'],
  climbing: ['抱石', '上攀', '系統牆', '指力板', '動態發力', '靜態控制', '鎖力', '捏力'],
  equipment: ['自體重', '啞鈴', '壺鈴', '槓鈴', '彈力帶', 'TRX', '滾筒'],
};

const EN = {
  全身: 'Full body', 核心: 'Core', 上肢: 'Upper body', 下肢: 'Lower body', 背部肌群: 'Back', 胸部肌群: 'Chest', 腹部肌群: 'Abs', 肩部肌群: 'Shoulders',
  二頭肌: 'Biceps', 三頭肌: 'Triceps', 小臂肌群: 'Forearms', 臀肌: 'Glutes', 股四頭肌: 'Quads', 膕繩肌: 'Hamstrings', 小腿肌群: 'Calves',
  斜方肌: 'Trapezius', 菱形肌: 'Rhomboids', 背闊肌: 'Lats', 豎脊肌: 'Erector spinae', 大圓肌: 'Teres major', 胸大肌上束: 'Upper chest', 前臂屈肌群: 'Forearm flexors', 前臂伸肌群: 'Forearm extensors', 臀大肌: 'Gluteus maximus',
  肌力: 'Strength', 耐力: 'Endurance', 速度: 'Speed', 柔軟度: 'Flexibility', 最大肌力: 'Max strength', 爆發力: 'Power', 肌耐力: 'Muscular endurance', 有氧耐力: 'Aerobic endurance', 活動度: 'Mobility',
  糖解系統: 'Glycolytic', 抱石: 'Bouldering', 上攀: 'Lead climbing', 系統牆: 'System wall', 指力板: 'Hangboard', 動態發力: 'Dynamic power', 靜態控制: 'Static control', 鎖力: 'Lock-off', 捏力: 'Pinch',
  自體重: 'Bodyweight', 啞鈴: 'Dumbbell', 壺鈴: 'Kettlebell', 槓鈴: 'Barbell', 彈力帶: 'Resistance band', 滾筒: 'Foam roller',
};

export const tagLabel = (tag, lang) => (lang === 'en' && EN[tag]) || tag;

// 和舊版共用同一個儲存鍵，用過的標籤會自動帶過來
const HISTORY_KEY = 'used_tags_history';

export function recentTags() {
  try { return JSON.parse(localStorage.getItem(HISTORY_KEY)) ?? []; } catch { return []; }
}

export function rememberTag(tag) {
  const history = [tag, ...recentTags().filter((item) => item !== tag)].slice(0, 30);
  try { localStorage.setItem(HISTORY_KEY, JSON.stringify(history)); } catch { /* 無法保存時只影響排序 */ }
}
