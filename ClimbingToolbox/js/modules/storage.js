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
                // 加入黃色準備時間
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 5, color: 'violet' },
                    children: [
                        { type: 'timer', id: uuid(), props: { duration: 10, label: 'Hang (Max)', color: 'red' } }, // 改為紅色
                        { type: 'timer', id: uuid(), props: { duration: 180, label: 'Rest', color: 'green' } }   // 改為綠色
                    ]
                }
            ];
        } else if (type === 'repeaters') {
            title = "7/3 Repeaters";
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: 'Prepare', color: 'amber' } }, // 準備時間
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'indigo' },
                    children: [
                        {
                            type: 'loop', id: uuid(), props: { iterations: 6, color: 'violet' },
                            children: [
                                { type: 'timer', id: uuid(), props: { duration: 7, label: 'Hang', color: 'red' } },  // 紅色
                                { type: 'timer', id: uuid(), props: { duration: 3, label: 'Rest', color: 'green' } } // 綠色
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
        editor.appendFooter(false); // Default false for skip rest
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

            el.innerHTML = `
                        <div onclick="timer.start('${r.id}')" class="flex-1 cursor-pointer">
                            <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${r.title}</div>
                            <div class="flex gap-3 mt-1 text-xs text-gray-500 font-mono dark:text-gray-400">
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

            const dayEl = document.createElement('div');
            // 點擊日期觸發明細 (步驟 8)
            dayEl.className = `cal-day ${isToday ? 'is-today' : ''} cursor-pointer hover:bg-gray-100 dark:hover:bg-gray-700`;
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
                item.className = "flex items-center justify-between p-4 bg-white rounded-2xl border border-gray-100 shadow-sm dark:bg-gray-750 dark:border-gray-700";
                item.innerHTML = `
                    <div class="flex-1">
                        <div class="font-bold text-gray-800 dark:text-gray-100">${rec.routineTitle}</div>
                        <div class="text-xs text-gray-500 font-mono mt-1">
                            ${rec.startTime} <span class="mx-1">|</span> ${formatTime(rec.duration)}
                        </div>
                    </div>
                    <button onclick="recordManager.deleteRecord('${rec.id}', '${dateStr}')" class="p-2 text-gray-300 hover:text-red-500 transition-colors">
                        <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path>
                        </svg>
                    </button>
                `;
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

    // 在 recordManager 中新增
    deleteRecord(recordId, dateStr) {
        if (!confirm('確定要刪除這筆訓練紀錄嗎？')) return;
        let records = this.getAllRecords();
        records = records.filter(rec => rec.id !== recordId);
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        // 同步刷新三處 UI
        this.updateUI();         // 黑框
        this.renderCalendar();   // 日曆點點
        this.showDayDetail(dateStr); // 下方清單
    }
};
