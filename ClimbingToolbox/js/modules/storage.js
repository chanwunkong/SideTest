// --- js/modules/storage.js ---

// --- 工具函式 ---
const uuid = () => Date.now().toString(36) + Math.random().toString(36).substr(2);
const formatTime = (s) => {
    const m = Math.floor(s / 60);
    const sec = s % 60;
    return `${m.toString().padStart(2, '0')}:${sec.toString().padStart(2, '0')}`;
};

// --- Utils: 靜態計算總時間 (Recursive) ---
const calculateDuration = (blocks) => {
    let total = 0;
    blocks.forEach(b => {
        if (b.type === 'loop') {
            const childrenTime = calculateDuration(b.children || []);
            total += childrenTime * (b.props.iterations || 1);
        } else {
            total += (b.props.duration || 0);
        }
    });
    return total;
};

// --- 2. Store ---
const store = {
    routines: [],
    user: null,

    init() {
        if (typeof auth !== 'undefined') {
            auth.onAuthStateChanged(user => {
                this.user = user;
                this.updateUserUI();
                user ? this.loadRoutines() : this.loadLocalRoutines();
            });
        } else {
            setTimeout(() => this.init(), 500);
        }
    },

    updateUserUI() {
        const emailEl = document.getElementById('settings-email');
        const btn = document.getElementById('auth-btn');
        const avatar = document.getElementById('avatar-img');
        const placeholder = document.getElementById('avatar-placeholder');

        if (this.user) {
            emailEl.textContent = this.user.email;
            btn.textContent = '登出';
            btn.onclick = () => auth.signOut();
            if (this.user.photoURL) {
                avatar.src = this.user.photoURL;
                avatar.classList.remove('hidden');
                placeholder.classList.add('hidden');
            }
        } else {
            emailEl.textContent = '未登入';
            btn.textContent = '登入';
            btn.onclick = () => {
                const provider = new firebase.auth.GoogleAuthProvider();
                auth.signInWithPopup(provider).catch(e => alert(e.message));
            };
            avatar.classList.add('hidden');
            placeholder.classList.remove('hidden');
        }
    },

    async loadRoutines() {
        const list = document.getElementById('routine-list');
        list.innerHTML = '<div class="text-center text-gray-400 mt-4">同步中...</div>';
        try {
            const snap = await db.collection('users').doc(this.user.uid).collection('routines').orderBy('updatedAt', 'desc').get();
            this.routines = snap.docs.map(d => ({ id: d.id, ...d.data() }));
            this.renderRoutines();
        } catch (e) { console.error(e); }
    },

    loadLocalRoutines() {
        const saved = localStorage.getItem('localRoutines');
        this.routines = saved ? JSON.parse(saved) : [];
        this.renderRoutines();
    },

    async duplicateRoutine(id) {
        const r = this.routines.find(x => x.id === id);
        if (!r) return;

        const newData = {
            ...r,
            title: r.title + ' (Copy)',
            updatedAt: Date.now()
        };
        delete newData.id; // ensure new ID

        if (this.user) {
            await db.collection('users').doc(this.user.uid).collection('routines').add(newData);
            this.loadRoutines();
        } else {
            const newId = 'local_' + Date.now();
            this.routines.unshift({ id: newId, ...newData });
            localStorage.setItem('localRoutines', JSON.stringify(this.routines));
            this.renderRoutines();
        }
    },

    async addTemplate(type) {
        let blocks = [];
        let title = "New Routine";

        if (type === 'max') {
            title = "Max Hangs";
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' }, // 改為 gray
                    children: [
                        { type: 'timer', id: uuid(), props: { duration: 10, label: 'Hang (Max)', color: 'red' } },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: 'Rest', color: 'green' } }
                    ]
                }
            ];
        } else if (type === 'repeaters') {
            title = "7/3 Repeaters";
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' }, // 改為 gray
                    children: [
                        {
                            type: 'loop', id: uuid(), props: { iterations: 6, color: 'gray' }, // 改為 gray
                            children: [
                                { type: 'timer', id: uuid(), props: { duration: 7, label: 'Hang', color: 'red' } },
                                { type: 'timer', id: uuid(), props: { duration: 3, label: 'Rest', color: 'green' } }
                            ]
                        },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: 'Rest (Set)', color: 'green' } }
                    ]
                }
            ];
        }

        // Load into editor immediately
        editor.currentId = null;
        document.getElementById('editor-title').value = title;
        const canvas = document.getElementById('editor-canvas');
        canvas.innerHTML = '';
        blocks.forEach(b => canvas.appendChild(editor.createBlock(b)));
        document.getElementById('modal-editor').classList.add('open');
        editor.appendFooter(false);
        editor.updateTimeline();
    },

    async deleteRoutine(id) {
        if (!confirm('確定要刪除這個課表嗎？')) return;

        if (this.user) {
            await db.collection('users').doc(this.user.uid).collection('routines').doc(id).delete();
            this.loadRoutines();
        } else {
            this.routines = this.routines.filter(r => r.id !== id);
            localStorage.setItem('localRoutines', JSON.stringify(this.routines));
            this.renderRoutines();
        }
    },

    renderRoutines() {
        // 新增：處理進行中課表 UI
        const sessionJson = localStorage.getItem('active_session');
        const sessionContainer = document.getElementById('active-session-container');
        if (sessionContainer) {
            if (sessionJson) {
                const session = JSON.parse(sessionJson);
                sessionContainer.innerHTML = `
                    <div class="bg-blue-50 border border-blue-200 p-4 rounded-xl shadow-sm flex justify-between items-center dark:bg-blue-900/30 dark:border-blue-800">
                    <div onclick="timer.resumeFromStorage()" class="flex-1 cursor-pointer">
                            <div class="text-xs font-bold text-blue-600 mb-1 dark:text-blue-400">進行中課表</div>
                            <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${session.routineTitle}</div>
                            <div class="text-xs text-gray-500 mt-1 dark:text-gray-400">已進行: ${formatTime(session.elapsed)}</div>
                        </div>
                        <button onclick="localStorage.removeItem('active_session'); store.renderRoutines();" class="p-2 text-gray-400 hover:text-red-500">
                            <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>
                        </button>
                    </div>
                `;
            } else {
                sessionContainer.innerHTML = '';
            }
        }

        const list = document.getElementById('routine-list');
        list.innerHTML = '';
        if (this.routines.length === 0) {
            list.innerHTML = '<div class="text-center text-gray-400 mt-10">尚無課表，請點選上方按鈕建立</div>';
            return;
        }

        this.routines.forEach(r => {
            const el = document.createElement('div');
            el.className = "bg-white p-4 rounded-xl shadow-sm border border-gray-100 flex justify-between items-center group dark:bg-gray-750 dark:border-gray-600";

            const blockCount = r.blocks ? r.blocks.length : 0;

            // Calculate estimated total time
            let totalSec = 0;
            if (r.blocks) totalSec = calculateDuration(r.blocks);
            // Handle Skip Last Rest for visual estimation (approximate)
            if (r.skipLastRest && r.blocks && r.blocks.length > 0) {
                const last = r.blocks[r.blocks.length - 1];
                if (last.type === 'timer') totalSec -= (last.props.duration || 0);
            }
            const timeStr = formatTime(Math.max(0, totalSec));

            // ✨ 新增：處理標籤的 HTML
            const tagsHtml = (r.tags && r.tags.length > 0)
                ? `<div class="flex flex-wrap gap-1 mt-1.5">` + r.tags.map(t => `<span class="text-[10px] bg-blue-50 text-blue-600 px-1.5 py-0.5 rounded border border-blue-100 font-bold dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800">${t}</span>`).join('') + `</div>`
                : '';

            el.innerHTML = `
                        <div onclick="timer.start('${r.id}')" class="flex-1 cursor-pointer">
                            <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${r.title}</div>
                            ${tagsHtml} <div class="flex gap-3 mt-2 text-xs text-gray-500 font-mono dark:text-gray-400">
                                <span class="bg-gray-100 px-2 py-0.5 rounded dark:bg-gray-600 dark:text-gray-300">⏱ ${timeStr}</span>
                                <span>${blockCount} 區塊</span>
                            </div>
                        </div>
                        <div class="flex items-center gap-1">
                             <button onclick="store.duplicateRoutine('${r.id}')" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="複製">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z"></path></svg>
                            </button>
                             <button onclick="editor.load('${r.id}')" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="編輯">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                            </button>
                            <button onclick="store.deleteRoutine('${r.id}')" class="text-gray-400 hover:text-red-600 p-2 dark:hover:text-red-400" title="刪除">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                            </button>
                        </div>
                    `;
            list.appendChild(el);
        });
    }
};

// --- Record Manager (整合切換與自動刷新) ---
const recordManager = {
    displayDate: new Date(), // 用於記錄目前日曆翻到哪個月
    selectedDate: null, // 新增：記錄目前選取的日期

    getAllRecords() {
        return JSON.parse(localStorage.getItem('trainingRecords') || '[]');
    },

    getRecordsByDate(dateStr) {
        return this.getAllRecords().filter(rec => rec.date === dateStr);
    },

    getThisWeekCount() {
        const records = this.getAllRecords();
        const now = new Date();
        const day = now.getDay();
        const diff = now.getDate() - day + (day === 0 ? -6 : 1);
        const monday = new Date(now.setDate(diff));
        monday.setHours(0, 0, 0, 0);
        return records.filter(rec => rec.timestamp >= monday.getTime()).length;
    },

    // 月份切換邏輯
    changeMonth(dir) {
        this.displayDate.setMonth(this.displayDate.getMonth() + dir);
        this.renderCalendar();
    },

    renderCalendar() {
        const calGrid = document.getElementById('calendar-grid');
        const titleEl = document.getElementById('calendar-month-title');
        if (!calGrid || !titleEl) return;

        calGrid.innerHTML = '';
        const year = this.displayDate.getFullYear();
        const month = this.displayDate.getMonth();

        // 更新標題顯示
        titleEl.textContent = `${year}年 ${month + 1}月`;

        // 獲取當月第一天與總天數
        let firstDay = new Date(year, month, 1).getDay();
        firstDay = (firstDay === 0) ? 7 : firstDay;
        const daysInMonth = new Date(year, month + 1, 0).getDate();

        // 填充上個月空白格
        for (let i = 1; i < firstDay; i++) {
            calGrid.appendChild(document.createElement('div'));
        }

        // 渲染每一天
        for (let day = 1; day <= daysInMonth; day++) {
            const dateStr = `${year}-${(month + 1).toString().padStart(2, '0')}-${day.toString().padStart(2, '0')}`;
            const dayRecords = this.getRecordsByDate(dateStr);
            const isToday = (new Date().toDateString() === new Date(year, month, day).toDateString());

            // 補上這行：判斷是否為已選取的日期
            const isSelected = (dateStr === this.selectedDate);

            const dayEl = document.createElement('div');

            // 修正 className：把 isSelected 的判斷與樣式加進來
            dayEl.className = `cal-day ${isToday ? 'is-today' : ''} ${isSelected ? 'ring-2 ring-blue-400 bg-blue-50 dark:bg-blue-900/30 dark:ring-blue-500' : ''} cursor-pointer hover:bg-gray-100 dark:hover:bg-gray-700`;

            // 補上這行：寫入 dataset，讓下方的 showDayDetail 可以透過 dataset.date 找到它
            dayEl.dataset.date = dateStr;

            dayEl.onclick = () => this.showDayDetail(dateStr);

            const numEl = document.createElement('span');
            numEl.className = 'cal-date-num dark:text-gray-200';
            numEl.textContent = day;
            dayEl.appendChild(numEl);

            // 渲染小點點
            const dotsContainer = document.createElement('div');
            dotsContainer.className = 'dots-container';
            const dotCount = Math.min(dayRecords.length, 3);
            for (let j = 0; j < dotCount; j++) {
                const dot = document.createElement('div');
                dot.className = 'dot';
                dotsContainer.appendChild(dot);
            }
            dayEl.appendChild(dotsContainer);
            calGrid.appendChild(dayEl);
        }
    },

    updateUI() {
        const countEl = document.getElementById('week-count-val');
        if (countEl) countEl.textContent = this.getThisWeekCount();
    },
    // 開啟明細面板
    showDayDetail(dateStr) {
        // 新增：記錄選取日期並更新日曆 UI 樣式
        this.selectedDate = dateStr;
        document.querySelectorAll('.cal-day').forEach(el => {
            el.classList.remove('ring-2', 'ring-blue-400', 'bg-blue-50', 'dark:bg-blue-900/30', 'dark:ring-blue-500');
            if (el.dataset.date === dateStr) {
                el.classList.add('ring-2', 'ring-blue-400', 'bg-blue-50', 'dark:bg-blue-900/30', 'dark:ring-blue-500');
            }
        });

        const records = this.getRecordsByDate(dateStr);
        const list = document.getElementById('detail-list');
        const title = document.getElementById('detail-date-title');
        const countLabel = document.getElementById('detail-record-count');

        // 更新標題與統計
        title.textContent = dateStr;
        countLabel.textContent = `${records.length} 筆紀錄`;

        list.innerHTML = '';

        if (records.length === 0) {
            list.innerHTML = `
                <div class="text-center py-10 bg-gray-50 rounded-2xl border border-dashed border-gray-200 dark:bg-gray-800/50 dark:border-gray-700">
                    <p class="text-sm text-gray-400">該日沒有訓練紀錄</p>
                </div>`;
        } else {
            records.forEach(rec => {
                const item = document.createElement('div');
                item.className = "p-4 bg-white rounded-2xl border border-gray-100 shadow-sm dark:bg-gray-750 dark:border-gray-700 flex flex-col";

                // ✨ 展開 executionLogs 資料
                let logsHtml = '';
                if (rec.executionLogs && rec.executionLogs.length > 0) {
                    logsHtml = '<div class="mt-3 space-y-2 border-t border-gray-50 pt-3 dark:border-gray-700">';
                    rec.executionLogs.forEach(log => {
                        if (Object.keys(log.actuals).length > 0) {
                            // 將實際數據組成字串 (例如 "負重: 20, 次數: 5")
                            const actualsStr = Object.entries(log.actuals).map(([k, v]) => `${k}: ${v}`).join(', ');
                            logsHtml += `
                                <div class="flex items-center gap-2 text-xs">
                                    <span class="text-gray-500 w-20 truncate dark:text-gray-400 font-bold">${log.label}</span>
                                    <span class="bg-blue-50 text-blue-700 px-1.5 py-0.5 rounded border border-blue-100 dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800">${actualsStr}</span>
                                </div>
                            `;
                        }
                    });
                    logsHtml += '</div>';
                }

                item.innerHTML = `
                    <div class="flex items-center justify-between">
                        <div class="flex-1">
                            <div class="font-bold text-gray-800 dark:text-gray-100">${rec.routineTitle}</div>
                            <div class="text-xs text-gray-500 font-mono mt-1">
                                ${rec.startTime} <span class="mx-1">|</span> ${formatTime(rec.duration)}
                            </div>
                        </div>
                        <div class="flex items-center gap-1">
                            <button onclick="recordEditor.open('${rec.id}')" class="p-2 text-gray-300 hover:text-blue-500 transition-colors">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                            </button>
                            <button onclick="recordManager.deleteRecord('${rec.id}', '${dateStr}')" class="p-2 text-gray-300 hover:text-red-500 transition-colors">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                            </button>
                        </div>
                    </div>
                    ${logsHtml} `;
                list.appendChild(item);
            });
        }

        // 視覺反饋：滾動到紀錄區塊 (選用)
        document.getElementById('day-detail-container').scrollIntoView({ behavior: 'smooth' });
    },

    // 關閉明細面板
    closeDetail() {
        const modal = document.getElementById('modal-day-detail');
        const sheet = document.getElementById('detail-sheet');
        sheet.classList.add('translate-y-full');
        setTimeout(() => modal.classList.add('hidden'), 300);
    },


    deleteRecord(recordId, dateStr) {
        if (!confirm('確定要刪除這筆訓練紀錄嗎？')) return;
        let records = this.getAllRecords();
        records = records.filter(rec => rec.id !== recordId);
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        // 同步刷新三處 UI
        this.updateUI();         // 黑框
        this.renderCalendar();   // 日曆點點
        this.showDayDetail(dateStr); // 下方清單
    },


    getLastBlockRecord(routineTitle, blockLabel) {
        const records = this.getAllRecords();
        // 從最新的紀錄開始往回找
        for (let i = records.length - 1; i >= 0; i--) {
            const rec = records[i];
            if (rec.routineTitle === routineTitle && rec.executionLogs) {
                // 找尋同名稱的動作
                const log = rec.executionLogs.find(l => l.label === blockLabel);
                if (log && log.actuals) return log.actuals;
            }
        }
        return null;
    },
};

// --- Record Editor (歷史數據編輯器) ---
const recordEditor = {
    currentRecordId: null,

    open(recordId) {
        const records = recordManager.getAllRecords();
        const rec = records.find(r => r.id === recordId);
        if (!rec) return;

        this.currentRecordId = recordId;
        
        document.getElementById('record-editor-title').textContent = rec.routineTitle;
        document.getElementById('record-editor-date').textContent = `${rec.date} ${rec.startTime}`;

        const form = document.getElementById('record-editor-form');
        form.innerHTML = '';

        if (!rec.executionLogs || rec.executionLogs.length === 0) {
            form.innerHTML = '<div class="text-center text-gray-400 mt-20 text-sm">此紀錄無詳細自訂指標日誌</div>';
        } else {
            rec.executionLogs.forEach((log, idx) => {
                let inputsHtml = '';
                
                // 動態生成輸入框
                Object.entries(log.actuals).forEach(([key, val]) => {
                    inputsHtml += `
                        <div class="flex items-center justify-between mt-3">
                            <span class="text-sm font-bold text-gray-600 dark:text-gray-400">${key}</span>
                            <input type="number" data-log-idx="${idx}" data-metric-name="${key}" value="${val}" 
                                class="w-24 bg-white border border-gray-200 rounded-lg p-2 text-center font-bold shadow-sm dark:bg-gray-700 dark:border-gray-600 dark:text-white focus:ring-2 focus:ring-blue-400 focus:outline-none">
                        </div>
                    `;
                });

                // 顯示計畫值以供對照
                let plannedStr = '';
                if (log.planned) {
                    plannedStr = log.planned.count ? `${log.planned.count}次` : (log.planned.duration ? `${log.planned.duration}秒` : '');
                }

                form.innerHTML += `
                    <div class="bg-gray-50 border border-gray-200 rounded-2xl p-4 dark:bg-gray-800 dark:border-gray-700">
                        <div class="flex justify-between items-end mb-2 border-b border-gray-200 pb-2 dark:border-gray-700">
                            <div class="font-bold text-gray-800 dark:text-gray-200">${log.label}</div>
                            <div class="text-[10px] font-bold text-gray-400 bg-gray-100 px-2 py-0.5 rounded dark:bg-gray-700">計畫: ${plannedStr || '-'}</div>
                        </div>
                        ${inputsHtml}
                    </div>
                `;
            });
        }

        const modal = document.getElementById('modal-record-editor');
        modal.classList.remove('hidden');
        modal.classList.add('flex');
    },

    close() {
        const modal = document.getElementById('modal-record-editor');
        modal.classList.add('hidden');
        modal.classList.remove('flex');
        this.currentRecordId = null;
    },

    save() {
        if (!this.currentRecordId) return;
        
        const records = recordManager.getAllRecords();
        const recIndex = records.findIndex(r => r.id === this.currentRecordId);
        if (recIndex === -1) return;

        const rec = records[recIndex];
        
        // 收集表單中的數值並覆寫 actuals
        const inputs = document.querySelectorAll('#record-editor-form input');
        inputs.forEach(input => {
            const logIdx = input.dataset.logIdx;
            const metricName = input.dataset.metricName;
            const val = Number(input.value);
            if (rec.executionLogs[logIdx] && rec.executionLogs[logIdx].actuals[metricName] !== undefined) {
                rec.executionLogs[logIdx].actuals[metricName] = val;
            }
        });

        // 存回 LocalStorage
        localStorage.setItem('trainingRecords', JSON.stringify(records));
        
        this.close();
        if (typeof showToast === 'function') showToast('紀錄已更新');
        
        // 即時刷新背後的明細清單，顯示最新修改的數值
        recordManager.showDayDetail(rec.date);
    }
};