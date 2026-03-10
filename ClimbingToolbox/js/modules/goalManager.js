// --- js/modules/goalManager.js ---

const goalManager = {
    goals: [],

    init() {
        const saved = localStorage.getItem('userGoals');
        this.goals = saved ? JSON.parse(saved) : [];
        this.renderGoals();
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
        targetContainer.innerHTML = '';

        if (type === 'all') {
            targetContainer.classList.add('hidden');
            return;
        }

        targetContainer.classList.remove('hidden');

        if (type === 'routine') {
            if (store.routines.length === 0) {
                targetContainer.innerHTML = '<div class="text-xs text-gray-400">目前沒有可選擇的課表</div>';
                return;
            }
            targetContainer.innerHTML = store.routines.map(r => `
                <label class="flex items-center gap-3 p-2 hover:bg-gray-100 dark:hover:bg-gray-700 rounded cursor-pointer">
                    <input type="checkbox" value="${r.id}" class="goal-target-cb w-4 h-4 text-blue-600 rounded">
                    <span class="text-sm font-bold dark:text-gray-200">${r.title}</span>
                </label>
            `).join('');
        } else if (type === 'tag') {
            const allTags = editor.getRoutineTagHistory();
            if (allTags.length === 0) {
                targetContainer.innerHTML = '<div class="text-xs text-gray-400">目前沒有已建立的標籤</div>';
                return;
            }
            targetContainer.innerHTML = allTags.map(t => `
                <label class="flex items-center gap-3 p-2 hover:bg-gray-100 dark:hover:bg-gray-700 rounded cursor-pointer">
                    <input type="checkbox" value="${t}" class="goal-target-cb w-4 h-4 text-blue-600 rounded">
                    <span class="text-sm font-bold dark:text-gray-200">${t}</span>
                </label>
            `).join('');
        }
    },

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
            id: uuid(),
            title,
            scope: { type, targets },
            criteria: { value, period }
        };

        this.goals.push(newGoal);
        this.save();
        this.closeEditor();
        showToast('目標已建立');
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
            if (goal.scope.type === 'all') {
                matchedCount++;
            } else if (goal.scope.type === 'routine' && goal.scope.targets.includes(r.routineId || r.routineTitle)) {
                // 為了兼容舊數據，可用 title 比對，若有存入 routineId 則更精準
                matchedCount++;
            } else if (goal.scope.type === 'tag') {
                const routine = store.routines.find(x => x.title === r.routineTitle);
                if (routine && routine.tags) {
                    const hasMatch = routine.tags.some(t => goal.scope.targets.includes(t));
                    if (hasMatch) matchedCount++;
                }
            }
        });

        return matchedCount;
    },

    renderGoals() {
        const list = document.getElementById('goals-list');
        if (!list) return;

        if (this.goals.length === 0) {
            list.innerHTML = `<div class="text-center text-gray-400 mt-10 text-sm">目前沒有設定目標。<br>點擊右上角新增。</div>`;
            return;
        }

        list.innerHTML = this.goals.map(g => {
            const current = this.calculateProgress(g);
            const target = g.criteria.value;
            const pct = Math.min((current / target) * 100, 100);
            const isCompleted = current >= target;

            let scopeText = '全部訓練';
            if (g.scope.type === 'routine') scopeText = `指定 ${g.scope.targets.length} 個課表`;
            if (g.scope.type === 'tag') scopeText = `包含標籤: ${g.scope.targets.join(', ')}`;

            let periodText = '';
            if (g.criteria.period === 'daily') periodText = '今日';
            else if (g.criteria.period === 'weekly') periodText = '本週';
            else if (g.criteria.period === 'monthly') periodText = '本月';

            // 確保這裡的 HTML 標籤完整閉合
            return `
            <div class="bg-white p-4 rounded-2xl shadow-sm border border-gray-100 dark:bg-gray-750 dark:border-gray-600 relative overflow-hidden">
                <div class="flex justify-between items-start mb-3">
                    <div>
                        <h4 class="font-bold text-gray-900 dark:text-gray-100">${g.title}</h4>
                        <p class="text-[10px] text-gray-400 font-bold uppercase tracking-wider mt-1">${periodText} • ${scopeText}</p>
                    </div>
                    <button onclick="goalManager.deleteGoal('${g.id}')" class="text-gray-300 hover:text-red-500 transition-colors">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                    </button>
                </div>
                
                <div class="flex items-end justify-between mb-2">
                    <span class="text-3xl font-black ${isCompleted ? 'text-green-500' : 'text-blue-600 dark:text-blue-400'}">
                        ${current}<span class="text-sm text-gray-400 font-bold ml-1">/ ${target} 次</span>
                    </span>
                </div>

                <div class="w-full h-2 bg-gray-100 rounded-full overflow-hidden dark:bg-gray-800">
                    <div class="h-full transition-all duration-500 ${isCompleted ? 'bg-green-500' : 'bg-blue-600 dark:bg-blue-500'}" style="width: ${pct}%"></div>
                </div>
            </div>
            `;
        }).join('');
    }
};