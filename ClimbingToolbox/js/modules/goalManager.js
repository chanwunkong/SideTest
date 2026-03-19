// --- js/modules/goalManager.js ---

export const goalManager = {
    goals: [],
    tempOperator: 'OR',
    showOnlyActive: true,

    init() {
        const saved = localStorage.getItem('userGoals');
        this.goals = saved ? JSON.parse(saved) : [];
        this.renderGoals();

        // ✅ 補上監聽：有新的訓練紀錄時，自動刷新目標進度
        if (typeof EventBus !== 'undefined') {
            EventBus.on(APP_EVENTS.RECORD_SAVED, () => {
                this.renderGoals();
            });
        }
    },

    save() {
        localStorage.setItem('userGoals', JSON.stringify(this.goals));
        this.renderGoals();
    },

    openEditor() {
        document.getElementById('goal-title').value = '';
        document.getElementById('goal-scope-type').value = 'all';
        document.getElementById('goal-period').value = 'weekly';
        document.getElementById('goal-value').value = '3';

        this.toggleScopeUI();

        const modal = document.getElementById('modal-goal-editor');
        const sheet = document.getElementById('goal-sheet');

        modal.classList.remove('hidden');
        modal.classList.add('open');

        setTimeout(() => sheet.classList.remove('translate-y-full'), 10);
    },

    closeEditor() {
        const modal = document.getElementById('modal-goal-editor');
        const sheet = document.getElementById('goal-sheet');

        sheet.classList.add('translate-y-full');
        modal.classList.remove('open');

        setTimeout(() => modal.classList.add('hidden'), 300);
    },

    toggleScopeUI() {
        const type = document.getElementById('goal-scope-type').value;
        const targetContainer = document.getElementById('goal-scope-targets');
        const operatorContainer = document.getElementById('goal-tag-operator-container'); // ✨ 新增：邏輯切換容器

        targetContainer.innerHTML = '';

        // 每次切換類型時，預設重置為 OR 邏輯
        if (typeof this.setOperator === 'function') {
            this.setOperator('OR');
        }

        if (type === 'all') {
            targetContainer.classList.add('hidden');
            if (operatorContainer) operatorContainer.classList.add('hidden');
            return;
        }

        targetContainer.classList.remove('hidden');

        // ✨ 只有選擇「標籤」時才顯示交集/聯集切換開關
        if (type === 'tag') {
            if (operatorContainer) operatorContainer.classList.remove('hidden');

            const allTags = editor.getRoutineTagHistory();
            if (allTags.length === 0) {
                targetContainer.innerHTML = '<div class="text-xs text-gray-400 p-2">目前沒有已建立的標籤</div>';
                return;
            }
            targetContainer.innerHTML = allTags.map(t => `
                <label class="flex items-center gap-3 p-2 hover:bg-gray-100 dark:hover:bg-gray-700 rounded cursor-pointer transition-colors">
                    <input type="checkbox" value="${t}" class="goal-target-cb w-4 h-4 text-blue-600 rounded border-gray-300">
                    <span class="text-sm font-bold dark:text-gray-200">${t}</span>
                </label>
            `).join('');

        } else {
            // 選擇「課表」時隱藏邏輯開關
            if (operatorContainer) operatorContainer.classList.add('hidden');

            if (store.routines.length === 0) {
                targetContainer.innerHTML = '<div class="text-xs text-gray-400 p-2">目前沒有可選擇的課表</div>';
                return;
            }
            targetContainer.innerHTML = store.routines.map(r => `
                <label class="flex items-center gap-3 p-2 hover:bg-gray-100 dark:hover:bg-gray-700 rounded cursor-pointer transition-colors">
                    <input type="checkbox" value="${r.id}" class="goal-target-cb w-4 h-4 text-blue-600 rounded border-gray-300">
                    <span class="text-sm font-bold dark:text-gray-200">${r.title}</span>
                </label>
            `).join('');
        }
    },

    // 在 goalManager 物件內
    saveGoal() {
        const title = document.getElementById('goal-title').value.trim() || '未命名目標';
        const type = document.getElementById('goal-scope-type').value;
        const period = document.getElementById('goal-period').value;
        const value = parseInt(document.getElementById('goal-value').value) || 1;

        const targets = [];
        if (type !== 'all') {
            document.querySelectorAll('.goal-target-cb:checked').forEach(cb => targets.push(cb.value));
            if (targets.length === 0) {
                showToast('請至少選擇一個關聯項目', 'error');
                return;
            }
        }

        const newGoal = {
            id: 'goal_' + Date.now(),
            title,
            scope: {
                type,
                targets,
                operator: (type === 'tag') ? this.tempOperator : 'OR' // ✨ 儲存邏輯運算子
            },
            isActive: true,
            criteria: { value, period }
        };

        this.goals.push(newGoal);
        this.save();
        this.closeEditor();
        showToast('目標已建立');
    },

    setOperator(op) {
        this.tempOperator = op;
        const btnOr = document.getElementById('op-or');
        const btnAnd = document.getElementById('op-and');
        const desc = document.getElementById('op-desc');

        if (op === 'AND') {
            btnAnd.className = "flex-1 py-1.5 text-xs font-bold rounded-lg bg-white shadow-sm border border-blue-200 text-blue-600";
            btnOr.className = "flex-1 py-1.5 text-xs font-bold rounded-lg text-gray-400 border border-transparent";
            desc.textContent = "課表必須同時包含所有勾選的標籤才會計算進度。";
        } else {
            btnOr.className = "flex-1 py-1.5 text-xs font-bold rounded-lg bg-white shadow-sm border border-blue-200 text-blue-600";
            btnAnd.className = "flex-1 py-1.5 text-xs font-bold rounded-lg text-gray-400 border border-transparent";
            desc.textContent = "只要符合勾選的其中一個標籤即計算進度。";
        }
    },

    toggleGoal(id, event) {
        if (event) event.stopPropagation();
        const goal = this.goals.find(g => g.id === id);
        if (goal) {
            goal.isActive = !goal.isActive;
            this.save(); // 儲存並觸發重新渲染
            showToast(goal.isActive ? '目標已啟動' : '目標已暫停');
        }
    },

    deleteGoal(id) {
        if (!confirm('確定刪除此目標？')) return;
        this.goals = this.goals.filter(g => g.id !== id);
        this.save();
    },

    calculateProgress(goal) {
        const records = recordManager.getAllRecords();
        const now = new Date();
        let startTime = 0;

        if (goal.criteria.period === 'daily') {
            const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
            startTime = today.getTime();
        }
        else if (goal.criteria.period === 'weekly') {
            const day = now.getDay();
            const diff = now.getDate() - day + (day === 0 ? -6 : 1);
            const monday = new Date(now.setDate(diff));
            monday.setHours(0, 0, 0, 0);
            startTime = monday.getTime();
        } else if (goal.criteria.period === 'monthly') {
            const firstDay = new Date(now.getFullYear(), now.getMonth(), 1);
            startTime = firstDay.getTime();
        }

        // 過濾時間範圍
        const validRecords = records.filter(r => r.timestamp >= startTime);

        // 過濾關聯條件 (1對0 或 1對多)
        let matchedCount = 0;
        validRecords.forEach(r => {
            if (goal.scope.type === 'routine') {
                // 目標若是指定特定課表 ID，優先比對 routineId
                if (goal.scope.targets.includes(r.routineId)) {
                    matchedCount++;
                }
            } else if (goal.scope.type === 'tag') {
                // ✨ 改用訓練當下的標籤快照，即使後來課表改名或刪除也不受影響
                if (r.tags && r.tags.length > 0) {
                    const selectedTags = goal.scope.targets;
                    const recordTags = r.tags;

                    if (goal.scope.operator === 'AND') {
                        const hasAll = selectedTags.every(t => recordTags.includes(t));
                        if (hasAll) matchedCount++;
                    } else {
                        const hasAny = selectedTags.some(t => recordTags.includes(t));
                        if (hasAny) matchedCount++;
                    }
                }
            }
        });

        return matchedCount;
    },

    toggleShowAll() {
        this.showOnlyActive = !this.showOnlyActive;

        // 更新圖示狀態
        const icon = document.getElementById('icon-goal-filter');
        const btnText = document.getElementById('goal-filter-text'); // 增加文字輔助

        if (this.showOnlyActive) {
            // 顯示「僅看啟動」的圖示 (眼睛)
            icon.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" /><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />';
            if (btnText) btnText.textContent = '進行中';
        } else {
            // 顯示「看全部」的圖示 (眼睛斜線)
            icon.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.542-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.88 9.88l-3.29-3.29m7.532 7.532l3.29 3.29M3 3l18 18" />';
            if (btnText) btnText.textContent = '顯示全部';
        }

        this.renderGoals();
    },

    renderGoals() {
        const list = document.getElementById('goals-list');
        if (!list) return;

        // 🌟 1. 根據 showOnlyActive 過濾目標
        let displayGoals = this.showOnlyActive
            ? this.goals.filter(g => g.isActive !== false)
            : this.goals;

        if (displayGoals.length === 0) {
            const msg = this.showOnlyActive ? "目前沒有進行中的目標" : "目前沒有設定目標";
            list.innerHTML = `<div class="text-center text-gray-400 py-8 text-xs font-bold bg-gray-50/50 rounded-2xl border border-dashed border-gray-200 dark:bg-gray-800/30 dark:border-gray-700">${msg}</div>`;
            return;
        }

        // 計算本週日期區間 (週一至週日)
        const now = new Date();
        const currentDay = now.getDay();
        const diffToMonday = now.getDate() - currentDay + (currentDay === 0 ? -6 : 1);
        const monday = new Date(new Date().setDate(diffToMonday));
        const sunday = new Date(monday);
        sunday.setDate(monday.getDate() + 6);
        const dateRangeStr = `${monday.getMonth() + 1}/${monday.getDate()} - ${sunday.getMonth() + 1}/${sunday.getDate()}`;

        // 🌟 2. 排序：進行中優先，且已達成的排在進行中之後
        const sortedGoals = [...displayGoals].sort((a, b) => {
            const aActive = a.isActive !== false;
            const bActive = b.isActive !== false;
            if (aActive !== bActive) return (bActive ? 1 : 0) - (aActive ? 1 : 0);

            // 同樣狀態下，未完成的優先顯示
            const aDone = this.calculateProgress(a) >= a.criteria.value;
            const bDone = this.calculateProgress(b) >= b.criteria.value;
            return (aDone ? 1 : 0) - (bDone ? 1 : 0);
        });

        list.innerHTML = sortedGoals.map(g => {
            const isActive = g.isActive !== false;
            const current = this.calculateProgress(g);
            const target = g.criteria.value;
            const pct = Math.min((current / target) * 100, 100);
            const isCompleted = current >= target;

            let periodLabel = '';
            let dateInfo = '';
            if (g.criteria.period === 'daily') periodLabel = '今日計數';
            else if (g.criteria.period === 'weekly') {
                periodLabel = '本週進度 (7d)';
                dateInfo = `(${dateRangeStr})`;
            } else if (g.criteria.period === 'monthly') periodLabel = '本月累積 (4w)';

            let scopeText = '全部訓練';
            if (g.scope.type === 'routine') scopeText = `指定 ${g.scope.targets.length} 課表`;
            if (g.scope.type === 'tag') {
                const opText = g.scope.operator === 'AND' ? ' (全部符合)' : '';
                scopeText = `#${g.scope.targets.join(', #')}${opText}`;
            }

            const activeClass = isActive ? '' : 'opacity-50 grayscale';
            const checkedAttr = isActive ? 'checked' : '';

            return `
            <div class="bg-white p-4 rounded-2xl shadow-sm border border-gray-100 dark:bg-gray-750 dark:border-gray-600 relative overflow-hidden transition-all ${activeClass}">
                <div class="flex justify-between items-start mb-3">
                    <div class="flex-1 pr-4">
                        <div class="flex items-center gap-2">
                            <h4 class="font-bold text-sm text-gray-900 dark:text-gray-100">${g.title}</h4>
                            ${isCompleted && isActive ? '<span class="text-[9px] bg-orange-100 text-orange-600 px-1.5 py-0.5 rounded font-black dark:bg-orange-900/40">🔥 已達成</span>' : ''}
                        </div>
                        <p class="text-[9px] text-gray-400 font-bold uppercase tracking-wider mt-1">
                            ${periodLabel} <span class="normal-case font-mono text-[8px]">${dateInfo}</span> • ${scopeText}
                        </p>
                    </div>
                    
                    <div class="flex items-center gap-2">
                        <label class="relative inline-flex items-center cursor-pointer">
                            <input type="checkbox" class="sr-only peer" ${checkedAttr} data-action="goal-toggle" data-value="${g.id}"
                            <div class="w-7 h-4 bg-gray-200 peer-focus:outline-none rounded-full peer dark:bg-gray-700 peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-3 after:w-3 after:transition-all peer-checked:bg-blue-600"></div>
                        </label>
                        <button data-action="goal-delete" data-value="${g.id}" class="text-gray-300 hover:text-red-500 transition-colors p-1">
                            <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                        </button>
                    </div>
                </div>
                
                <div class="flex items-end justify-between mb-2">
                    <span class="text-2xl font-black ${isCompleted ? 'text-green-500' : 'text-blue-600 dark:text-blue-400'}">
                        ${current}<span class="text-[10px] text-gray-400 font-bold ml-1">/ ${target} 次</span>
                    </span>
                    <span class="text-[10px] font-bold text-gray-400">${Math.round(pct)}%</span>
                </div>

                <div class="w-full h-1.5 bg-gray-100 rounded-full overflow-hidden dark:bg-gray-800">
                    <div class="h-full transition-all duration-500 ${isCompleted ? 'bg-green-500' : 'bg-blue-600 dark:bg-blue-500'}" style="width: ${pct}%"></div>
                </div>
            </div>`;
        }).join('');
    },
};