// --- js/modules/storage.js ---
import { editor } from './ui.js';
import { timer } from './timer.js';

// 1. 定義標準事件名稱，避免拼字錯誤
export const APP_EVENTS = {
    RECORD_SAVED: 'RECORD_SAVED',       // 訓練紀錄已儲存
    BODY_DATA_UPDATED: 'BODY_DATA_UPDATED', // 身體數據已更新
    GOAL_UPDATED: 'GOAL_UPDATED',       // 目標設定已變動
    SESSION_UPDATED: 'SESSION_UPDATED', // 暫存狀態改變
    ROUTINE_UPDATED: 'ROUTINE_UPDATED'  // 課表清單改變
};

// 2. 實作輕量級事件總線
export const EventBus = {
    events: {},
    on(event, callback) {
        if (!this.events[event]) this.events[event] = [];
        this.events[event].push(callback);
    },
    emit(event, data) {
        if (!this.events[event]) return;
        console.log(`[EventBus] 發佈事件: ${event}`, data);
        this.events[event].forEach(cb => cb(data));
    }
};

// 👇 新增專屬的 Session 存取層
export const sessionRepository = {
    get() {
        const data = localStorage.getItem('active_session');
        return data ? JSON.parse(data) : null;
    },
    save(sessionData) {
        localStorage.setItem('active_session', JSON.stringify(sessionData));
        if (typeof EventBus !== 'undefined') EventBus.emit(APP_EVENTS.SESSION_UPDATED);
    },
    clear() {
        localStorage.removeItem('active_session');
        if (typeof EventBus !== 'undefined') EventBus.emit(APP_EVENTS.SESSION_UPDATED);
    }
};

// --- 資料存取層 (Repository) ---
export const recordRepository = {
    getAll() {
        return JSON.parse(localStorage.getItem('trainingRecords') || '[]');
    },

    save(newRecord) {
        const records = this.getAll();
        records.push(newRecord);
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        if (typeof EventBus !== 'undefined') {
            EventBus.emit(APP_EVENTS.RECORD_SAVED, {
                date: newRecord.date,
                routineId: newRecord.routineId
            });
        }
    },

    updateLog(recordId, queueIndex, payload) {
        const records = this.getAll();
        const record = records.find(r => r.id === recordId);
        if (!record) return;

        const logIndex = record.executionLogs.findIndex(l => l.queueIndex === queueIndex);
        if (logIndex !== -1) {
            record.executionLogs[logIndex].actuals = payload.actuals ? payload.actuals : payload;
        } else {
            record.executionLogs.push(payload);
            record.executionLogs.sort((a, b) => a.queueIndex - b.queueIndex);
        }

        localStorage.setItem('trainingRecords', JSON.stringify(records));

        if (typeof EventBus !== 'undefined') {
            EventBus.emit(APP_EVENTS.RECORD_SAVED, { date: record.date });
        }
    },

    delete(recordId, dateStr) {
        let records = this.getAll();
        records = records.filter(rec => rec.id !== recordId);
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        if (typeof EventBus !== 'undefined') {
            EventBus.emit(APP_EVENTS.RECORD_SAVED, { date: dateStr });
        }
    }
};

// --- 工具函式 ---
export const routineUtils = {
    flattenBlocks(blocks, parentLoops = []) {
        let res = [];
        blocks.forEach(b => {
            if (b.type === 'timer' || b.type === 'reps') {
                const isLastIteration = parentLoops.length > 0 && parentLoops[parentLoops.length - 1].current === parentLoops[parentLoops.length - 1].total;
                if (b.props.skipOnLast && isLastIteration) return;

                res.push({ ...b, loopState: [...parentLoops] });
            } else if (b.type === 'loop') {
                const count = b.props.iterations || 1;
                for (let i = 1; i <= count; i++) {
                    const newLoopState = [...parentLoops, { current: i, total: count, id: b.id }];
                    res.push(...this.flattenBlocks(b.children || [], newLoopState));
                }
            }
        });
        return res;
    }
};

export const uuid = () => {
    if (typeof crypto !== 'undefined' && crypto.randomUUID) {
        return crypto.randomUUID();
    }
    return Date.now().toString(36) + Math.random().toString(36).substr(2);
};

export const formatTime = (s) => {
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
export const store = {
    routines: [],
    user: null,

    init() {
        if (typeof auth !== 'undefined' && auth.onAuthStateChanged) {
            auth.onAuthStateChanged(user => {
                this.user = user;
                this.updateUserUI();
                user ? this.loadRoutines() : this.loadLocalRoutines();
            });
        } else {
            this.loadLocalRoutines();
        }

        // 👇 新增這段：讓課表頁面成為事件驅動
        if (typeof EventBus !== 'undefined') {
            EventBus.on(APP_EVENTS.SESSION_UPDATED, () => this.renderRoutines());
            EventBus.on(APP_EVENTS.ROUTINE_UPDATED, () => this.renderRoutines());
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
        let title = "新課表";
        let tags = [];

        if (type === 'max') {
            title = "最大指力";
            tags = ['最大肌力', '手指'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' },
                    children: [
                        { type: 'timer', id: uuid(), props: { duration: 10, label: '懸掛', color: 'red', customMetrics: [{ name: '重量', type: 'number' }, { name: '邊緣', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: '休息', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        } else if (type === 'repeaters') {
            title = "7/3手指耐力";
            tags = ['耐力', '手指'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                    children: [
                        {
                            type: 'loop', id: uuid(), props: { iterations: 6, color: 'gray' },
                            children: [
                                { type: 'timer', id: uuid(), props: { duration: 7, label: '懸掛', color: 'red', customMetrics: [{ name: '重量', type: 'number' }] } },
                                { type: 'timer', id: uuid(), props: { duration: 3, label: '休息', color: 'green', skipOnLast: true } }
                            ]
                        },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: '組間休息', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        } else if (type === 'pullups') {
            title = "上肢與核心";
            tags = ['體能', '上肢力量'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                    children: [
                        { type: 'reps', id: uuid(), props: { count: 5, duration: 30, label: '引體向上', color: 'blue', customMetrics: [{ name: '引體次數', type: 'number' }, { name: '重量', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 20, label: 'L型支撐', color: 'orange', customMetrics: [{ name: '核心秒數', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 90, label: '休息', color: 'green', skipOnLast: true } }
                    ]
                }
            ];
        } else if (type === 'squat') {
            title = "深蹲 5x5";
            tags = ['體能', '下肢力量'];
            blocks = [
                { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
                {
                    type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                    children: [
                        { type: 'reps', id: uuid(), props: { count: 5, duration: 60, label: '深蹲 (熱身)', color: 'blue', customMetrics: [{ name: '熱身重量', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 120, label: '休息', color: 'green' } }
                    ]
                },
                {
                    type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' },
                    children: [
                        { type: 'reps', id: uuid(), props: { count: 5, duration: 60, label: '深蹲 (主項)', color: 'red', customMetrics: [{ name: '深蹲重量', type: 'number' }] } },
                        { type: 'timer', id: uuid(), props: { duration: 180, label: '休息', color: 'green', skipOnLast: true } }
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
        const session = sessionRepository.get();
        const sessionContainer = document.getElementById('active-session-container');

        if (sessionContainer) {
            if (session) {
                sessionContainer.innerHTML = `
                    <div class="bg-blue-50 border border-blue-200 p-4 rounded-xl shadow-sm flex justify-between items-center dark:bg-blue-900/30 dark:border-blue-800">
                        <div data-action="session-resume" class="flex-1 cursor-pointer">
                            <div class="text-xs font-bold text-blue-600 mb-1 dark:text-blue-400">進行中課表</div>
                            <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${session.routineTitle}</div>
                            <div class="text-xs text-gray-500 mt-1 dark:text-gray-400">已進行: ${formatTime(session.elapsed)}</div>
                        </div>
                        <button data-action="session-clear" class="p-2 text-gray-400 hover:text-red-500">
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
                    <button data-action="open-editor" class="bg-gray-900 text-white dark:bg-blue-600 px-6 py-4 rounded-2xl font-bold shadow-xl w-full mb-4 flex items-center justify-center gap-2 active:scale-95 transition-transform">
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
                const flattened = routineUtils.flattenBlocks(r.blocks);
                totalSec = flattened.reduce((acc, b) => acc + (b.props.duration || 0), 0);
            }
            const timeStr = formatTime(Math.max(0, totalSec));

            // 處理標籤的 HTML
            const tagsHtml = (r.tags && r.tags.length > 0)
                ? `<div class="flex flex-wrap gap-1 mt-1.5">` + r.tags.map(t => `<span class="text-[10px] bg-blue-50 text-blue-600 px-1.5 py-0.5 rounded border border-blue-100 font-bold dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800">${t}</span>`).join('') + `</div>`
                : '';

            el.innerHTML = `
                        <div data-action="routine-start" data-value="${r.id}" class="flex-1 cursor-pointer">
                            <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${r.title}</div>
                            ${tagsHtml} <div class="flex gap-3 mt-2 text-xs text-gray-500 font-mono dark:text-gray-400">
                                <span class="bg-gray-100 px-2 py-0.5 rounded dark:bg-gray-600 dark:text-gray-300">⏱ ${timeStr}</span>
                                <span>${blockCount} 區塊</span>
                            </div>
                        </div>
                        <div class="flex items-center gap-1">
                             <button data-action="routine-duplicate" data-value="${r.id}" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="複製">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z"></path></svg>
                            </button>
                             <button data-action="routine-edit" data-value="${r.id}" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="編輯">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                            </button>
                            <button data-action="routine-delete" data-value="${r.id}" class="text-gray-400 hover:text-red-600 p-2 dark:hover:text-red-400" title="刪除">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                            </button>
                        </div>
                    `;
            list.appendChild(el);
        });
    }
};

// --- Record Manager (整合切換與自動刷新) ---
export const recordManager = {
    displayDate: new Date(), // 用於記錄目前日曆翻到哪個月
    selectedDate: null, // 記錄目前選取的日期
    selectedDateNode: null, // 快取目前選取的 DOM 節點
    calendarMode: 'week', // 預設為週視圖
    isRecordsCollapsed: false,
    expandedRecordIds: new Set(),

    init() {
        if (typeof EventBus !== 'undefined') {
            EventBus.on(APP_EVENTS.RECORD_SAVED, (data) => {
                this.updateUI();
                this.renderCalendar();

                // 💡 修正邏輯：如果 data 有日期就用 data，沒有就用目前選中的
                const targetDate = data?.date || this.selectedDate;
                if (targetDate) {
                    this.showDayDetail(targetDate);
                }
            });
        }
    },

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
        return recordRepository.getAll();
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
        dayEl.dataset.action = 'record-show-day';
        dayEl.dataset.value = dateStr;

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
                        const isFailure = log.actuals && log.actuals.isFailure === true;

                        // 過濾 isFailure 鍵值，僅串接其他數值
                        const actualsStr = Object.entries(log.actuals || {})
                            .filter(([k, v]) => k !== 'isFailure')
                            .map(([k, v]) => `${k}: ${v === "" || v === undefined ? '0' : v}`)
                            .join(', ');

                        // 獨立渲染力竭徽章
                        const failureBadge = isFailure
                            ? `<span class="ml-1.5 px-1 py-0.5 rounded text-[9px] font-bold bg-red-500 text-white shadow-sm dark:bg-red-600 shrink-0">力竭</span>`
                            : '';

                        // 處理組數文字格式 (N-M 格式)
                        let loopText = '';
                        if (log.loopState && log.loopState.length > 0) {
                            loopText = log.loopState.map(s => s.current).join('-');
                            loopText = `<span class="text-gray-400 font-mono font-medium mr-1.5 shrink-0">${loopText}</span>`;
                        }

                        // 修改：移除左側 w-28，改用 flex-1 讓兩者自動分配空間
                        // 左右兩欄使用 bg 容器包覆並根據內容自動調整寬度
                        logsHtml += `
    <button data-action="record-edit-log" data-record="${rec.id}" data-index="${lIdx}"
            class="flex items-center gap-2 text-xs w-full hover:bg-gray-50 dark:hover:bg-gray-700/50 p-1 rounded transition-colors text-left">
        <div class="flex items-center min-w-0 shrink">
            ${loopText}
            <span class="text-gray-500 truncate dark:text-gray-300 font-bold">${log.label}</span>
        </div>
        
        <div class="flex-1 flex items-center justify-center bg-blue-50 text-blue-700 px-2 py-1.5 rounded-lg border border-blue-100 dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800 font-mono min-w-fit">
            <span class="truncate">${actualsStr || '0'}</span>
            ${failureBadge}
        </div>
        
        <svg class="w-3 h-3 text-gray-300 shrink-0" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>
    </button>`;
                    });
                    logsHtml += '</div>';
                }

                const durationText = typeof formatTime === 'function' ? formatTime(rec.duration) : `${Math.floor(rec.duration / 60)}m`;

                item.innerHTML = `
 <div class="flex items-start justify-between w-full">
        <div class="flex-1 cursor-pointer min-w-0" data-action="record-toggle-logs" data-value="${rec.id}">
            <div class="flex items-center gap-2">
                <div class="font-bold text-gray-800 dark:text-gray-100 truncate">${rec.routineTitle}</div>
                <svg id="arrow-${rec.id}" class="w-3 h-3 text-gray-400 transition-transform shrink-0" style="transform: ${arrowRotation};" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path d="M19 9l-7 7-7-7" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                </svg>
            </div>
            <div class="text-[10px] text-gray-500 font-mono mt-0.5">${rec.startTime} | ${durationText}</div>
        </div>

        <button data-action="record-delete" data-record="${rec.id}" data-date="${dateStr}" class="p-2 text-gray-300 hover:text-red-500 transition-colors shrink-0 -mr-2 -mt-1">
            <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
            </svg>
        </button>
    </div>
                ${logsHtml}`;
                list.appendChild(item);
            });
        }
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
        recordRepository.delete(recordId, dateStr);
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


    // 新增：供 UI 呼叫的編輯啟動器
    editLogEntry(recordId, logIndex) {
        const records = this.getAllRecords();
        const record = records.find(r => r.id === recordId);
        if (!record) return;

        const log = record.executionLogs[logIndex];

        // 優先使用儲存的源頭快照 (blockSnapshot)
        // 若沒有快照 (舊的歷史紀錄)，則降級進行基本重建
        const mockBlock = log.blockSnapshot || {
            id: log.blockId,
            props: {
                label: log.label,
                customMetrics: Object.keys(log.actuals || {})
                    .filter(name => name !== 'isFailure')
                    .map(name => ({ name, type: 'number' })),
                duration: log.planned?.duration || 0,
                count: log.planned?.count || 0
            },
            loopState: log.loopState || []
        };

        // 開啟面板並傳入現有數值 (傳入 log.queueIndex 確保對應正確的區塊順序)
        timer.showLogPanel(mockBlock, log.queueIndex, log.actuals, recordId);
    },
};

