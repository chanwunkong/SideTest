// --- js/modules/storage.js ---

// --- 工具函式 ---
const uuid = () => {
    if (typeof crypto !== 'undefined' && crypto.randomUUID) {
        return crypto.randomUUID();
    }
    return Date.now().toString(36) + Math.random().toString(36).substr(2);
};
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
        let tags = [];

        if (type === 'max') {
            // 例子 1：手指極限力量
            // 處理狀況：區分「全身力量」與「手指專項力量」
            title = "Max Hangs (手指極限)";
            tags = ['最大肌力', '手指'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' },
                    children: [
                        { type: 'timer', id: uuid(), props: { duration: 10, label: 'Hang (Max)', color: 'red', customMetrics: [{ name: '附加重量', type: 'number' }, { name: '邊緣大小', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: 'Rest', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        } else if (type === 'repeaters') {
            // 例子 2：手指耐力訓練
            // 處理狀況：區分「手指耐力」與「全身攀爬耐力」
            title = "7/3 Repeaters (手指耐力)";
            tags = ['耐力', '手指'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                    children: [
                        {
                            type: 'loop', id: uuid(), props: { iterations: 6, color: 'gray' },
                            children: [
                                { type: 'timer', id: uuid(), props: { duration: 7, label: 'Hang', color: 'red', customMetrics: [{ name: '附加重量', type: 'number' }] } },
                                { type: 'timer', id: uuid(), props: { duration: 3, label: 'Rest', color: 'green', skipOnLast: true } }
                            ]
                        },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: 'Rest (Set)', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        } else if (type === 'pullups') {
            // 例子 3：上肢體能與核心
            // 處理狀況：區分「廣義體能累積」與「專項動作進度」
            title = "Pull-ups & Core (上肢體能)";
            tags = ['體能', '上肢力量'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                    children: [
                        { type: 'reps', id: uuid(), props: { count: 5, duration: 30, label: 'Pull-ups', color: 'blue', customMetrics: [{ name: '引體次數', type: 'number' }, { name: '負重重量', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 20, label: 'L-Sit', color: 'orange', customMetrics: [{ name: '核心支撐秒數', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 90, label: 'Rest', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        }

        // 載入至編輯器 UI
        editor.currentId = null;
        document.getElementById('editor-title').value = title;
        editor.currentRoutineTags = tags;

        const canvas = document.getElementById('editor-canvas');
        canvas.innerHTML = '';
        blocks.forEach(b => canvas.appendChild(editor.createBlock(b)));

        document.getElementById('modal-editor').classList.add('open');
        editor.renderRoutineTagsUI();
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
            list.innerHTML = `
                <div class="text-center mt-12 px-4">
                    <div class="text-gray-400 mb-6 text-sm">目前尚無課表，馬上建立一個吧！</div>
                    <button onclick="editor.open()" class="bg-gray-900 text-white dark:bg-blue-600 px-6 py-4 rounded-2xl font-bold shadow-xl w-full mb-4 flex items-center justify-center gap-2 active:scale-95 transition-transform">
                        <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4"></path></svg>
                        建立新課表
                    </button>
                </div>`;
            return;
        }

        this.routines.forEach(r => {
            const el = document.createElement('div');
            el.className = "bg-white p-4 rounded-xl shadow-sm border border-gray-100 flex justify-between items-center group dark:bg-gray-750 dark:border-gray-600";

            const blockCount = r.blocks ? r.blocks.length : 0;

            // Calculate estimated total time
            let totalSec = 0;
            if (r.blocks) {
                if (typeof timer !== 'undefined' && timer.flatten) {
                    const flattened = timer.flatten(r.blocks);
                    totalSec = flattened.reduce((acc, b) => acc + (b.props.duration || 0), 0);
                } else {
                    totalSec = calculateDuration(r.blocks); // 降級備案
                }
            }
            const timeStr = formatTime(Math.max(0, totalSec));

            // 處理標籤的 HTML
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
    selectedDate: null, // 記錄目前選取的日期
    selectedDateNode: null, // 快取目前選取的 DOM 節點
    calendarMode: 'week', // 預設為週視圖
    isRecordsCollapsed: false,
    expandedRecordIds: new Set(),

    // 支援跨週/月模式導覽
    changePeriod(dir) {
        if (this.calendarMode === 'month') {
            this.displayDate.setMonth(this.displayDate.getMonth() + dir);
        } else {
            // 週模式直接增減 7 天
            this.displayDate.setDate(this.displayDate.getDate() + (dir * 7));
        }
        this.renderCalendar();
    },

    // 紀錄摺疊控制
    toggleRecordsCollapse() {
        this.isRecordsCollapsed = !this.isRecordsCollapsed;
        const list = document.getElementById('detail-list');
        const icon = document.getElementById('icon-record-collapse');

        if (this.isRecordsCollapsed) {
            list.classList.add('hidden');
            if (icon) icon.style.transform = 'rotate(-90deg)';
        } else {
            list.classList.remove('hidden');
            if (icon) icon.style.transform = 'rotate(0deg)';
        }
    },

    toggleCalendarMode() {
        this.calendarMode = this.calendarMode === 'week' ? 'month' : 'week';
        const btnText = document.getElementById('calendar-mode-text');
        if (btnText) btnText.textContent = this.calendarMode === 'week' ? '週視圖' : '月視圖';
        this.renderCalendar();
    },

    toggleRecordLogs(recordId) {
        const logSection = document.getElementById(`logs-${recordId}`);
        const arrow = document.getElementById(`arrow-${recordId}`);
        if (logSection) {
            const isHidden = logSection.classList.contains('hidden');
            if (isHidden) {
                logSection.classList.remove('hidden');
                arrow.style.transform = 'rotate(180deg)';
                this.expandedRecordIds.add(recordId);
            } else {
                logSection.classList.add('hidden');
                arrow.style.transform = 'rotate(0deg)';
                this.expandedRecordIds.delete(recordId);
            }
        }
    },

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

        // 取得錨點日期的年、月
        const year = this.displayDate.getFullYear();
        const month = this.displayDate.getMonth();

        // 修正 1：月份標題在週視圖下也要反映目前的月份
        titleEl.textContent = this.calendarMode === 'week'
            ? `${this.displayDate.getMonth() + 1}月 訓練節奏`
            : `${year}年 ${month + 1}月`;

        if (this.calendarMode === 'month') {
            // --- 月視圖邏輯：保持原樣 ---
            let firstDay = new Date(year, month, 1).getDay();
            firstDay = (firstDay === 0) ? 7 : firstDay;
            const daysInMonth = new Date(year, month + 1, 0).getDate();

            for (let i = 1; i < firstDay; i++) {
                calGrid.appendChild(document.createElement('div'));
            }

            for (let day = 1; day <= daysInMonth; day++) {
                this.createDayNode(new Date(year, month, day), calGrid);
            }
        } else {
            // 修正 2：週視圖邏輯 - 改為直接找出 Monday 並連跑 7 天
            // 不再受限於當月 1~31 號的迴圈
            const anchor = new Date(this.displayDate);
            const dayOfWeek = anchor.getDay();
            const diffToMon = anchor.getDate() - dayOfWeek + (dayOfWeek === 0 ? -6 : 1);
            const monday = new Date(anchor.setDate(diffToMon));

            for (let i = 0; i < 7; i++) {
                const current = new Date(monday);
                current.setDate(monday.getDate() + i);
                this.createDayNode(current, calGrid);
            }
        }
    },

    // 輔助函式：生成單日節點 (統一視覺樣式)
    createDayNode(dateObj, container) {
        const y = dateObj.getFullYear();
        const m = (dateObj.getMonth() + 1).toString().padStart(2, '0');
        const d = dateObj.getDate().toString().padStart(2, '0');
        const dateStr = `${y}-${m}-${d}`;

        const dayRecords = this.getRecordsByDate(dateStr);
        const isToday = (new Date().toDateString() === dateObj.toDateString());
        const isSelected = (dateStr === this.selectedDate);

        const dayEl = document.createElement('div');
        // 加入藍色小點 (bg-blue-500) 與 修正框框寬度
        dayEl.className = `calendar-day relative flex flex-col items-center justify-center h-10 w-full rounded-xl transition-all ${isToday ? 'is-today ring-1 ring-blue-200' : ''} ${isSelected ? 'ring-2 ring-blue-400 bg-blue-50 dark:bg-blue-900/30 dark:ring-blue-500' : 'hover:bg-gray-50 dark:hover:bg-gray-700/50'}`;
        dayEl.dataset.date = dateStr;
        dayEl.onclick = () => this.showDayDetail(dateStr);

        if (isSelected) this.selectedDateNode = dayEl;

        const numEl = document.createElement('span');
        numEl.className = 'text-xs font-bold dark:text-gray-200';
        numEl.textContent = dateObj.getDate();
        dayEl.appendChild(numEl);

        const dotsContainer = document.createElement('div');
        dotsContainer.className = 'flex gap-0.5 mt-1 h-1 justify-center';
        const dotCount = Math.min(dayRecords.length, 3);
        for (let j = 0; j < dotCount; j++) {
            const dot = document.createElement('div');
            dot.className = 'w-1 h-1 rounded-full bg-blue-500 dark:bg-blue-400';
            dotsContainer.appendChild(dot);
        }
        dayEl.appendChild(dotsContainer);
        container.appendChild(dayEl);
    },

    updateUI() {
        const countEl = document.getElementById('week-count-val');
        if (countEl) countEl.textContent = this.getThisWeekCount();
    },
    // 開啟明細面板
    showDayDetail(dateStr) {
        if (this.selectedDate !== dateStr) {
            this.expandedRecordIds.clear(); // 換日期時，清空上一次的展開記憶
        }
        this.selectedDate = dateStr;

        // 1. 樣式更新邏輯
        if (this.selectedDateNode) {
            this.selectedDateNode.classList.remove('ring-2', 'ring-blue-400', 'bg-blue-50', 'dark:bg-blue-900/30', 'dark:ring-blue-500');
        }
        const newSelectedNode = document.querySelector(`.calendar-day[data-date="${dateStr}"]`);
        if (newSelectedNode) {
            newSelectedNode.classList.add('ring-2', 'ring-blue-400', 'bg-blue-50', 'dark:bg-blue-900/30', 'dark:ring-blue-500');
            this.selectedDateNode = newSelectedNode;
        }

        const records = this.getRecordsByDate(dateStr);
        const list = document.getElementById('detail-list');
        const title = document.getElementById('detail-date-title');
        const countLabel = document.getElementById('detail-record-count');

        if (!list || !title || !countLabel) return;

        title.textContent = dateStr;
        countLabel.textContent = `${records.length} 筆紀錄`;
        list.innerHTML = '';

        // 修正 3：渲染紀錄與數據容錯
        if (records.length === 0) {
            list.innerHTML = `<div class="text-center py-10 text-xs text-gray-400">該日沒有訓練紀錄</div>`;
        } else {
            records.forEach(rec => {
                const item = document.createElement('div');
                item.className = "p-4 bg-white rounded-2xl border border-gray-100 shadow-sm dark:bg-gray-750 dark:border-gray-700 flex flex-col";

                // 讀取該紀錄的展開狀態
                const isExpanded = this.expandedRecordIds.has(rec.id);
                const hiddenClass = isExpanded ? '' : 'hidden';
                const arrowRotation = isExpanded ? 'rotate(180deg)' : 'rotate(0deg)';

                let logsHtml = '';
                if (rec.executionLogs && rec.executionLogs.length > 0) {
                    // 動態套用 hiddenClass
                    logsHtml = `
                    <div id="logs-${rec.id}" class="${hiddenClass} mt-3 space-y-2 border-t border-gray-50 pt-3 dark:border-gray-700">`;
                    rec.executionLogs.forEach((log, lIdx) => {
                        const actualsStr = Object.entries(log.actuals || {})
                            .map(([k, v]) => `${k}: ${v === "" || v === undefined ? '0' : v}`)
                            .join(', ');

                        logsHtml += `
                        <button onclick="recordManager.editLogEntry('${rec.id}', ${lIdx})" 
                                class="flex items-center gap-2 text-xs w-full hover:bg-gray-50 dark:hover:bg-gray-700/50 p-1 rounded transition-colors text-left">
                            <span class="text-gray-500 w-16 truncate dark:text-gray-400 font-bold">${log.label}</span>
                            <span class="bg-blue-50 text-blue-700 px-2 py-1 rounded-lg border border-blue-100 dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800 flex-1 text-center font-mono">
                                ${actualsStr || '0'}
                            </span>
                            <svg class="w-3 h-3 text-gray-300" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>
                        </button>`;
                    });
                    logsHtml += '</div>';
                }

                const durationText = typeof formatTime === 'function' ? formatTime(rec.duration) : `${Math.floor(rec.duration / 60)}m`;

                item.innerHTML = `
                <div class="flex items-center justify-between">
                    <div class="flex-1 cursor-pointer" onclick="recordManager.toggleRecordLogs('${rec.id}')">
                        <div class="flex items-center gap-2">
                            <div class="font-bold text-gray-800 dark:text-gray-100">${rec.routineTitle}</div>
                            <svg id="arrow-${rec.id}" class="w-3 h-3 text-gray-400 transition-transform" style="transform: ${arrowRotation};" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path d="M19 9l-7 7-7-7" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>
                        </div>
                        <div class="text-[10px] text-gray-500 font-mono mt-0.5">${rec.startTime} | ${durationText}</div>
                    </div>
                    <button onclick="recordManager.deleteRecord('${rec.id}', '${dateStr}')" class="p-2 text-gray-300 hover:text-red-500">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>
                    </button>
                </div>
                ${logsHtml}`;
                list.appendChild(item);
            });
        }
    },

    //     // 4. 視覺反饋：僅在未摺疊時捲動
    //     if (!this.isRecordsCollapsed) {
    //         title.scrollIntoView({ behavior: 'smooth', block: 'start' });
    //     }
    // },



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

    // 新增：更新特定紀錄中的某一項 Log
    updateRecordLog(recordId, logIndex, newActuals) {
        let records = this.getAllRecords();
        const recIdx = records.findIndex(r => r.id === recordId);
        if (recIdx === -1) return;

        // 更新該筆紀錄中的 executionLogs
        records[recIdx].executionLogs[logIndex].actuals = newActuals;

        localStorage.setItem('trainingRecords', JSON.stringify(records));

        // 刷新明細 UI 與 分析數據
        this.showDayDetail(this.selectedDate);
        if (typeof analyticsManager !== 'undefined' && analyticsManager.renderCards) {
            analyticsManager.renderCards();
        }
    },

    // 新增：供 UI 呼叫的編輯啟動器
    editLogEntry(recordId, logIndex) {
        const records = this.getAllRecords();
        const record = records.find(r => r.id === recordId);
        if (!record) return;

        const log = record.executionLogs[logIndex];

        // 模擬一個 Block 物件供 timer.showLogPanel 使用
        const mockBlock = {
            id: log.blockId,
            props: {
                label: log.label,
                customMetrics: Object.keys(log.actuals || {}).map(name => ({ name, type: 'number' })),
                duration: log.planned.duration,
                count: log.planned.count
            }
        };

        // 開啟面板並傳入現有數值
        timer.showLogPanel(mockBlock, logIndex, log.actuals, recordId);
    },
};

