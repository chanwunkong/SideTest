// --- js/modules/ui.js ---

// --- 手勢滑動元件 ---
window.initSwipeToClose = function (elementId, closeFn) {
    const el = document.getElementById(elementId);
    if (!el) return;

    let startY = 0;
    let currentY = 0;
    let isDragging = false;

    el.addEventListener('touchstart', (e) => {
        // 避免與內部滾動區域衝突
        if (e.target.closest('.overflow-y-auto') && e.target.closest('.overflow-y-auto').scrollTop > 0) return;
        startY = e.touches[0].clientY;
        isDragging = true;
        el.style.transition = 'none';
    }, { passive: true });

    el.addEventListener('touchmove', (e) => {
        if (!isDragging) return;
        currentY = e.touches[0].clientY;
        const deltaY = currentY - startY;
        if (deltaY > 0) {
            el.style.transform = `translateY(${deltaY}px)`;
        }
    }, { passive: true });

    el.addEventListener('touchend', () => {
        if (!isDragging) return;
        isDragging = false;
        el.style.transition = ''; // 恢復 CSS class 預設動畫

        if (currentY - startY > 100) {
            closeFn();
        }
        el.style.transform = '';
    });
};

// --- 全域 Toast 提示元件 ---
window.showToast = function (message, type = 'info') {
    // 若已有提示則先移除，避免堆疊
    const existing = document.getElementById('app-toast');
    if (existing) existing.remove();

    const toast = document.createElement('div');
    toast.id = 'app-toast';

    // 依據 type 設定不同的顏色與圖示
    const isError = type === 'error';
    const colors = isError
        ? 'bg-red-50 text-red-600 border border-red-200 dark:bg-red-900/90 dark:border-red-800 dark:text-red-100'
        : 'bg-gray-800 text-white border border-gray-700 dark:bg-white dark:text-gray-900';

    const icon = isError
        ? `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`
        : `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;

    toast.className = `fixed top-16 left-1/2 transform -translate-x-1/2 z-[100] px-4 py-2 rounded-full shadow-lg text-sm font-bold transition-all duration-300 opacity-0 -translate-y-4 flex items-center gap-2 ${colors}`;
    toast.innerHTML = `${icon} <span>${message}</span>`;

    document.body.appendChild(toast);

    // 觸發瀏覽器重繪以執行 CSS 動畫
    void toast.offsetWidth;
    toast.classList.remove('opacity-0', '-translate-y-4');

    // 2.5 秒後自動淡出並移除
    setTimeout(() => {
        toast.classList.add('opacity-0', '-translate-y-4');
        setTimeout(() => toast.remove(), 300);
    }, 2500);
};

// --- 0. Theme Manager ---
const themeManager = {
    mode: 'system', // system, light, dark

    init() {
        this.mode = localStorage.getItem('themeMode') || 'system';
        document.getElementById('set-theme-mode').value = this.mode;
        this.apply();

        // System listener
        window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => {
            if (this.mode === 'system') this.apply();
        });
    },

    setMode(val) {
        this.mode = val;
        localStorage.setItem('themeMode', val);
        this.apply();
    },

    apply() {
        const isDark = this.mode === 'dark' || (this.mode === 'system' && window.matchMedia('(prefers-color-scheme: dark)').matches);
        if (isDark) {
            document.documentElement.classList.add('dark');
        } else {
            document.documentElement.classList.remove('dark');
        }
    }
};

// --- 1. Settings (Sound & Profile) Manager ---
const settingsManager = {
    // Default Settings
    data: {
        // Sound
        soundEnabled: true,
        ttsEnabled: true,
        countdownSec: 3,
        speechRate: 1.0,
        voiceURI: '',
        soundStart: true,
        soundCountdown: true,
        soundFinish: true,
        urgentPulseEnabled: true,
        healthSyncEnabled: false,    // 健康 App 同步開關
        wearableDeviceConnected: false, // 穿戴裝置連接狀態

        // Profile
        weightVal: '',
        weightUnit: 'kg'
    },

    _audioCtx: null, // 新增：全域 AudioContext 實例暫存

    // 新增：取得單一 AudioContext 實例
    getAudioContext() {
        if (!this._audioCtx) {
            const AudioContext = window.AudioContext || window.webkitAudioContext;
            if (!AudioContext) return null;
            this._audioCtx = new AudioContext();
        }
        // 確保在瀏覽器自動暫停狀態下喚醒
        if (this._audioCtx.state === 'suspended') {
            this._audioCtx.resume();
        }
        return this._audioCtx;
    },

    init() {
        const saved = localStorage.getItem('appSettings');
        if (saved) {
            this.data = { ...this.data, ...JSON.parse(saved) };
        }
        this.bindUI();
        this.initVoiceSelector();
    },

    initVoiceSelector() {
        const sel = document.getElementById('set-voice-selection');
        if (!sel) return;

        const populate = () => {
            const voices = window.speechSynthesis.getVoices();
            if (voices.length === 0) return;

            if (!this.data.voiceURI) {
                const defaultZhVoice = voices.find(v => v.lang.includes('zh') || v.lang.includes('cmn'));
                if (defaultZhVoice) {
                    this.data.voiceURI = defaultZhVoice.voiceURI;
                    this.save();
                }
            }

            sel.innerHTML = voices
                .map(v => {
                    const isSelected = v.voiceURI === this.data.voiceURI ? 'selected' : '';
                    const defaultLabel = v.default ? ' (預設)' : '';
                    // 加上語言標記，方便在手機上辨識不同地區的中文模型
                    return `<option value="${v.voiceURI}" ${isSelected}>${v.name} [${v.lang}]${defaultLabel}</option>`;
                })
                .join('');

            sel.onchange = (e) => {
                this.data.voiceURI = e.target.value;
                this.save();

                // 【修正】：手機版 UI 切換需要一點微小的緩衝時間，否則容易被瀏覽器吃掉事件
                setTimeout(() => this.speak("語音測試"), 150);
            };
        };

        window.speechSynthesis.addEventListener('voiceschanged', populate);
        populate();

        // 【手機版 Hack】：iOS 有時候不會觸發 voiceschanged，主動戳它一下
        if (window.speechSynthesis.getVoices().length === 0) {
            setTimeout(populate, 500);
        }
    },

    save() {
        localStorage.setItem('appSettings', JSON.stringify(this.data));
    },

    bindUI() {
        // Helper to bind checkboxes
        const bindCheck = (id, key) => {
            const el = document.getElementById(id);
            if (!el) return;
            el.checked = this.data[key];
            el.onchange = (e) => { this.data[key] = e.target.checked; this.save(); };
        };

        // Helper to bind inputs
        const bindVal = (id, key, isNum = false) => {
            const el = document.getElementById(id);
            if (!el) return;
            el.value = this.data[key];
            el.oninput = (e) => {
                this.data[key] = isNum ? parseFloat(e.target.value) : e.target.value;
                this.save();
            };
        };

        // Sound Binds
        bindCheck('set-sound-enabled', 'soundEnabled');
        bindCheck('set-tts-enabled', 'ttsEnabled');
        bindCheck('set-sound-start', 'soundStart');
        bindCheck('set-sound-countdown', 'soundCountdown');
        bindCheck('set-sound-finish', 'soundFinish');
        bindCheck('set-pulse-enabled', 'urgentPulseEnabled');

        bindVal('set-countdown-sec', 'countdownSec', true);

        // Rate
        const elRate = document.getElementById('set-speech-rate');
        const lblRate = document.getElementById('lbl-speech-rate');
        if (elRate) {
            elRate.value = this.data.speechRate;
            lblRate.textContent = this.data.speechRate + 'x';
            elRate.oninput = (e) => {
                this.data.speechRate = parseFloat(e.target.value);
                lblRate.textContent = this.data.speechRate + 'x';
                this.save();
            };
        }

        // Profile Binds
        bindVal('set-weight-val', 'weightVal'); // Keep string for empty state
        bindVal('set-weight-unit', 'weightUnit');
    },

    // --- Sound Actions ---
    beep(type) {
        if (!this.data.soundEnabled) return;
        if (type === 'start' && !this.data.soundStart) return;
        if (type === 'countdown' && !this.data.soundCountdown) return;
        if (type === 'finish' && !this.data.soundFinish) return;

        let freq = 880; let wave = 'sine'; let dur = 0.1;
        if (type === 'countdown') { freq = 440; }
        if (type === 'start') { freq = 880; wave = 'square'; dur = 0.2; }
        if (type === 'finish') { freq = 523.25; wave = 'triangle'; dur = 0.4; }

        try {
            // 優化：使用全域單一實例
            const ctx = this.getAudioContext();
            if (!ctx) return;

            const osc = ctx.createOscillator();
            const gain = ctx.createGain();
            osc.connect(gain);
            gain.connect(ctx.destination);
            osc.frequency.value = freq;
            osc.type = wave;
            gain.gain.setValueAtTime(0.1, ctx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.00001, ctx.currentTime + dur);
            osc.start();
            osc.stop(ctx.currentTime + dur);

            if (type === 'finish') {
                setTimeout(() => {
                    const osc2 = ctx.createOscillator();
                    const gain2 = ctx.createGain();
                    osc2.connect(gain2);
                    gain2.connect(ctx.destination);
                    osc2.frequency.value = freq * 1.25;
                    osc2.type = wave;
                    gain2.gain.setValueAtTime(0.1, ctx.currentTime);
                    gain2.gain.exponentialRampToValueAtTime(0.00001, ctx.currentTime + dur);
                    osc2.start();
                    osc2.stop(ctx.currentTime + dur);
                }, 150);
            }
        } catch (e) { console.error(e); }
    },

    // 修正後的 speak 函式：使用選擇的模型
    speak(text) {
        if (!this.data.soundEnabled || !this.data.ttsEnabled) return;
        if (!window.speechSynthesis) return;

        // 【修正 1】：只在真的有聲音在播放時才 cancel，避免手機版引擎卡死
        if (window.speechSynthesis.speaking) {
            window.speechSynthesis.cancel();
        }

        const u = new SpeechSynthesisUtterance(text);
        u.rate = this.data.speechRate;

        const voices = window.speechSynthesis.getVoices();

        if (voices.length > 0) {
            const selectedVoice = voices.find(v => v.voiceURI === this.data.voiceURI);
            if (selectedVoice) {
                u.voice = selectedVoice;
                // 【修正 2】：手機版鐵律，必須同時設定 lang 才能真正切換引擎
                u.lang = selectedVoice.lang;
            } else {
                const defaultZhVoice = voices.find(v => v.lang.includes('zh') || v.lang.includes('cmn'));
                if (defaultZhVoice) {
                    u.voice = defaultZhVoice;
                    u.lang = defaultZhVoice.lang;
                } else {
                    u.lang = 'zh-TW';
                }
            }
        } else {
            u.lang = 'zh-TW';
        }

        window.speechSynthesis.speak(u);
    }
};



// --- 1. Router ---
// --- js/modules/ui.js ---

const router = {
    go(viewId) {
        // 1. 重置所有視圖與按鈕狀態
        document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
        document.querySelectorAll('.nav-btn').forEach(el => {
            el.classList.remove('text-gray-900', 'text-blue-600', 'dark:text-white');
            el.classList.add('text-gray-400', 'dark:text-gray-400');
        });

        // 2. 啟用目標視圖與按鈕
        const targetView = document.getElementById(`view-${viewId}`);
        if (targetView) targetView.classList.add('active');

        const btn = document.querySelector(`.nav-btn[data-target="view-${viewId}"]`);
        if (btn) {
            btn.classList.remove('text-gray-400', 'dark:text-gray-400');
            btn.classList.add('text-gray-900', 'dark:text-white');
        }

        // 3. 根據進入的分頁，動態刷新對應資料

        // 新看板邏輯：同步刷新日曆與目標
        if (viewId === 'dashboard') {
            if (typeof recordManager !== 'undefined') {
                recordManager.updateUI();
                recordManager.renderCalendar();
                // 同步顯示選取日期的明細
                if (recordManager.selectedDate) {
                    recordManager.showDayDetail(recordManager.selectedDate);
                } else {
                    // 若無選取，預設顯示今天
                    const now = new Date();
                    const todayStr = `${now.getFullYear()}-${(now.getMonth() + 1).toString().padStart(2, '0')}-${now.getDate().toString().padStart(2, '0')}`;
                    recordManager.showDayDetail(todayStr);
                }
            }
            if (typeof goalManager !== 'undefined') {
                goalManager.renderGoals();
            }
        }
        // 新分析邏輯：刷新 PR 數據卡片
        else if (viewId === 'insight') {
            if (typeof analyticsManager !== 'undefined') {
                analyticsManager.renderCards();
            }
        }

        // 課表管理保持不變
        else if (viewId === 'routines' && typeof store !== 'undefined') {
            store.renderRoutines();
        }
    }
};


// --- 3. Editor ---
const editor = {
    currentId: null,
    activeBlock: null,
    // 顏色列表
    colors: ['gray', 'red', 'orange', 'amber', 'green', 'teal', 'blue', 'indigo', 'violet', 'pink'],
    // 暫存選擇的顏色
    selectedColor: 'gray',
    // 暫存課程的標籤
    currentRoutineTags: [],
    // 暫存屬性面板中的自訂指標
    tempMetrics: [],

    // 預設選項 
    defaultSuggestions: {
        'timer': [
            { label: '準備', color: 'blue' },
            { label: '懸掛', color: 'orange' },
            { label: '休息', color: 'green' }
        ],
        'reps': [
            { label: '引體向上', color: 'indigo' },
            { label: '伏地挺身', color: 'pink' }
        ],
        'loop': []
    },

    init() {
        new Sortable(document.getElementById('editor-palette'), {
            group: { name: 'shared', pull: 'clone', put: false },
            sort: false,
            delay: 200,
            delayOnTouchOnly: true,
            fallbackTolerance: 10,   // 放寬容錯至 10px
            forceFallback: true,
            supportPointer: false,   // 關閉 Pointer 事件攔截
            onClone: (evt) => { evt.clone.dataset.tempId = Date.now(); }
        });
        this.initSortable(document.getElementById('editor-canvas'));
    },

    initSortable(el) {
        new Sortable(el, {
            group: 'shared',
            animation: 150,
            emptyInsertThreshold: 50,
            ghostClass: 'sortable-ghost',
            dragClass: 'sortable-drag',
            draggable: '.block-item',

            delay: 150,
            delayOnTouchOnly: true, // 僅在觸控螢幕上啟用長按 (不影響滑鼠)
            fallbackTolerance: 10,   // 允許手指點擊時有 5px 的微小晃動，不會中斷長按
            forceFallback: true, // 強制關閉原生拖曳，允許原生的頁面滾動
            supportPointer: false,   // 關閉 Pointer 事件攔截
            scroll: true,        // 拖曳到螢幕邊緣時自動上下滾動
            bubbleScroll: true,  // 允許事件冒泡

            fallbackOnBody: true, // 將拖曳的元素實體移至 body，避免受父容器 scroll 座標干擾
            swapThreshold: 0.65,  // 降低積木交換的觸發靈敏度（預設 1），避免積木頻繁上下抖動


            onAdd: (evt) => {
                if (evt.item.classList.contains('palette-item')) {
                    const type = evt.item.dataset.type;
                    let props = null;

                    if (evt.item.dataset.defaultProps) {
                        try {
                            props = JSON.parse(evt.item.dataset.defaultProps);
                        } catch (e) {
                            console.error("Props parsing error", e);
                        }
                    }
                    evt.item.replaceWith(this.createBlock({ type, id: uuid(), props }));
                }
                this.updateTimeline();
            },
            onUpdate: () => this.updateTimeline(),
            onRemove: () => this.updateTimeline()
        });
    },

    // 新增：掃描所有課表取得標籤歷史
    getRoutineTagHistory() {
        const tagsSet = new Set();
        store.routines.forEach(r => {
            if (r.tags && Array.isArray(r.tags)) {
                r.tags.forEach(t => tagsSet.add(t));
            }
        });
        return Array.from(tagsSet);
    },

    // 新增：渲染課程標籤 UI
    renderRoutineTagsUI() {
        const container = document.getElementById('editor-routine-tags');
        const suggestions = document.getElementById('editor-tag-suggestions');
        if (!container || !suggestions) return;

        // 渲染已選擇
        if (this.currentRoutineTags.length === 0) {
            container.innerHTML = '<span class="text-xs text-gray-400 font-bold">目前無標籤</span>';
        } else {
            container.innerHTML = this.currentRoutineTags.map(t => `
                <span class="inline-flex items-center gap-1 bg-blue-50 text-blue-700 px-2 py-1 rounded text-xs font-bold border border-blue-200 dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800">
                    ${t}
                    <button type="button" onclick="editor.removeRoutineTag('${t}')" class="text-blue-400 hover:text-blue-700 ml-1 dark:hover:text-blue-100">&times;</button>
                </span>
            `).join('');
        }

        // 渲染歷史建議
        const allTags = this.getRoutineTagHistory();
        const availableTags = allTags.filter(t => !this.currentRoutineTags.includes(t));

        if (availableTags.length === 0) {
            suggestions.innerHTML = '<span class="text-[10px] text-gray-400">無歷史建議</span>';
        } else {
            suggestions.innerHTML = availableTags.map(t => `
                <button type="button" onclick="editor.addRoutineTag('${t}')" class="bg-gray-100 text-gray-600 px-2.5 py-1 rounded text-xs font-bold border border-gray-200 hover:bg-gray-200 dark:bg-gray-700 dark:text-gray-300 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors">
                    + ${t}
                </button>
            `).join('');
        }
    },

    addRoutineTag(tagStr) {
        const inp = document.getElementById('inp-routine-tag');
        const val = (tagStr || inp.value).trim();
        if (val && !this.currentRoutineTags.includes(val)) {
            this.currentRoutineTags.push(val);
            this.renderRoutineTagsUI();
        }
        if (inp) inp.value = '';
    },

    removeRoutineTag(tagStr) {
        this.currentRoutineTags = this.currentRoutineTags.filter(t => t !== tagStr);
        this.renderRoutineTagsUI();
    },

    // 取得積木類別 (根據 props.color)
    getBlockClass(type, props) {
        const c = props.color || 'gray';
        return `border-l-4 border-l-${c}-500 bg-${c}-50 border-${c}-200 dark:bg-${c}-900/20 dark:border-${c}-800`;
    },

    createBlock(data) {
        const el = document.createElement('div');
        const props = data.props || this.getDefaultProps(data.type);

        el.className = `block-item touch-pan-y p-3 mb-2 rounded border bg-white ${this.getBlockClass(data.type, props)}`;

        el.dataset.type = data.type;
        el.dataset.id = data.id || uuid();
        el.dataset.props = JSON.stringify(props);

        // ▼ 移除原本在此處定義 skipButton 的程式碼區塊 ▼

        const header = document.createElement('div');
        header.className = "flex justify-between items-center cursor-pointer w-full";
        header.innerHTML = `
                <div class="flex items-center gap-2 pointer-events-none flex-1">
                    <span class="text-xs font-bold uppercase opacity-60 dark:text-gray-400">${data.type}</span>
                    <span class="font-bold text-sm block-label dark:text-white">${this.getLabel(data.type, props)}</span>
                </div>
                `;
        header.onclick = (e) => {
            e.stopPropagation();
            this.openProps(el);
        };
        el.appendChild(header);

        if (data.type === 'loop') {
            const inner = document.createElement('div');
            inner.className = "nested-container p-2 mt-2 min-h-[60px] bg-black/5 dark:bg-black/20 rounded-lg border-2 border-dashed border-gray-300 dark:border-gray-600";
            el.appendChild(inner);
            this.initSortable(inner);
            if (data.children) {
                data.children.forEach(c => inner.appendChild(this.createBlock(c)));
            }
        }

        return el;
    },

    getDefaultProps(type) {
        if (type === 'loop') return { iterations: 3, color: 'gray' };
        if (type === 'timer') return { duration: 10, label: 'Hang', color: 'orange', customMetrics: [] };
        if (type === 'reps') return { count: 5, duration: 30, label: 'Pull Ups', color: 'blue', customMetrics: [{ name: '次數', type: 'number' }] };
        return { color: 'gray' };
    },

    getLabel(type, props) {
        if (type === 'loop') return `x ${props.iterations} 次`;
        if (type === 'timer') return `${props.label} (${props.duration}s)`;
        if (type === 'reps') return `${props.label} x${props.count}`;
        return type;
    },

    // 掃描所有課表，建立歷史紀錄 (Name -> Color)
    getHistory(type) {
        const history = new Map();
        store.routines.forEach(r => {
            const traverse = (blocks) => {
                blocks.forEach(b => {
                    if (b.type === type && b.props.label) {
                        history.set(b.props.label, b.props.color || 'gray');
                    }
                    if (b.children) traverse(b.children);
                });
            };
            if (r.blocks) traverse(r.blocks);
        });
        return history;
    },

    openProps(el) {
        this.activeBlock = el;
        const type = el.dataset.type;
        const props = JSON.parse(el.dataset.props);
        const form = document.getElementById('prop-form');

        // 載入該積木現有的指標至暫存
        this.tempMetrics = props.customMetrics ? [...props.customMetrics] : [];

        let html = '';
        if (type !== 'loop') {
            let optionsHtml = '<option value="" 從歷史紀錄選擇...</option>';
            const historyMap = this.getHistory(type);
            const defaults = this.defaultSuggestions[type] || [];
            const addedLabels = new Set();

            defaults.forEach(def => {
                if (!historyMap.has(def.label)) historyMap.set(def.label, def.color);
                optionsHtml += `<option value="${def.label}">${def.label}</option>`;
                addedLabels.add(def.label);
            });

            historyMap.forEach((color, label) => {
                if (!addedLabels.has(label)) {
                    optionsHtml += `<option value="${label}">${label}</option>`;
                }
            });

            html += `
                        <div>
                            <label class="block text-sm font-bold text-gray-500 mb-1">標籤</label>
                            <div class="flex flex-col gap-2">
                                <input type="text" id="inp-label" class="w-full border rounded-lg p-3 dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.label || ''}" placeholder="輸入自訂名稱...">
                                <select id="sel-label" class="w-full border rounded-lg p-2 text-sm text-gray-600 bg-gray-50 dark:bg-gray-600 dark:text-gray-300 dark:border-gray-500">
                                    ${optionsHtml}
                                </select>
                            </div>
                        </div>`;

            this.tempHistoryMap = historyMap;
        }

        if (type === 'loop') {
            html += `
                    <div>
                        <label class="block text-sm font-bold text-gray-500 mb-1">循環次數</label>
                        <input type="number" id="inp-iterations" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.iterations}">
                    </div>`;
        } else if (type === 'timer') {
            const isSkipChecked = props.skipOnLast ? 'checked' : '';
            html += `
                    <div>
                        <label class="block text-sm font-bold text-gray-500 mb-1">時間 (秒)</label>
                        <input type="number" id="inp-duration" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.duration}">
                    </div>
                    <div class="flex items-center justify-between bg-gray-50 dark:bg-gray-700/50 p-4 rounded-lg mt-4 border border-gray-100 dark:border-gray-600">
                        <div>
                            <div class="font-bold text-sm dark:text-white">最後一趟跳過</div>
                            <div class="text-xs text-gray-500 mt-1">若位於迴圈內，最後一次循環將跳過</div>
                        </div>
                        <label class="relative inline-flex items-center cursor-pointer">
                            <input type="checkbox" id="inp-skip-last" ${isSkipChecked} class="sr-only peer">
                            <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none rounded-full peer dark:bg-gray-600 peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-500"></div>
                        </label>
                    </div>`;
        } else if (type === 'reps') {
            html += `
                    <div class="grid grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-bold text-gray-500 mb-1">次數</label>
                            <input type="number" id="inp-count" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.count}">
                        </div>
                        <div>
                            <label class="block text-sm font-bold text-gray-500 mb-1">預估時間 (秒)</label>
                            <input type="number" id="inp-duration" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.duration}" placeholder="計時用">
                        </div>
                    </div>`;
        }

        // 新增：插入自訂追蹤指標的 HTML (排除迴圈)
        if (type !== 'loop') {
            html += `
                <div class="mt-6 pt-4 border-t border-gray-100 dark:border-gray-700">
                    <label class="block text-sm font-bold text-gray-500 mb-2">訓練追蹤指標 </label>
                    <div id="prop-metrics-list" class="space-y-2 mb-3"></div>
                    <div class="flex gap-2">
                        <input type="text" id="inp-new-metric" onkeydown="if(event.key==='Enter') editor.addMetric()" class="flex-1 border border-gray-200 rounded-lg p-2 text-sm dark:bg-gray-700 dark:border-gray-600 dark:text-white" placeholder="輸入追蹤項目 (如: 負重)">
                        <button type="button" onclick="editor.addMetric()" class="bg-blue-50 text-blue-600 px-4 py-2 rounded-lg text-sm font-bold border border-blue-100 dark:bg-blue-900/30 dark:border-blue-800 dark:text-blue-300 hover:bg-blue-100 transition-colors">新增</button>
                    </div>
                </div>
            `;
        }

        form.innerHTML = html;

        // 觸發渲染已存在的指標
        if (type !== 'loop') this.renderMetricsUI();

        this.selectedColor = props.color || 'gray';
        const palette = document.getElementById('color-palette');
        palette.innerHTML = '';
        this.colors.forEach(c => {
            const btn = document.createElement('div');
            btn.className = `color-btn w-8 h-8 rounded-full cursor-pointer shrink-0 bg-${c}-500 ${c === this.selectedColor ? 'selected' : ''}`;
            btn.onclick = () => {
                this.selectedColor = c;
                Array.from(palette.children).forEach(child => child.classList.remove('selected'));
                btn.classList.add('selected');
            };
            palette.appendChild(btn);
        });

        const selLabel = document.getElementById('sel-label');
        const inpLabel = document.getElementById('inp-label');
        if (selLabel && inpLabel) {
            selLabel.addEventListener('change', (e) => {
                const val = e.target.value;
                inpLabel.value = val;
                if (this.tempHistoryMap && this.tempHistoryMap.has(val)) {
                    const historyColor = this.tempHistoryMap.get(val);
                    this.selectedColor = historyColor;
                    Array.from(palette.children).forEach(child => {
                        child.classList.remove('selected');
                        if (child.classList.contains(`bg-${historyColor}-500`)) {
                            child.classList.add('selected');
                        }
                    });
                }
            });
        }
        document.getElementById('prop-sheet').classList.remove('translate-y-full');
    },

    // 渲染指標清單的方法
    renderMetricsUI() {
        const container = document.getElementById('prop-metrics-list');
        if (!container) return;
        if (this.tempMetrics.length === 0) {
            container.innerHTML = '<div class="text-[10px] font-bold text-gray-400 bg-gray-50 p-2 rounded border border-dashed border-gray-200 dark:bg-gray-800 dark:border-gray-700">無自訂追蹤項目</div>';
            return;
        }
        container.innerHTML = this.tempMetrics.map((m, idx) => `
            <div class="flex justify-between items-center bg-gray-50 border border-gray-200 p-2.5 rounded-lg dark:bg-gray-800 dark:border-gray-700">
                <span class="text-sm font-bold text-gray-700 dark:text-gray-300">${m.name}</span>
                <button type="button" onclick="editor.removeMetric(${idx})" class="text-gray-400 hover:text-red-500 font-bold px-2 transition-colors">✕</button>
            </div>
        `).join('');
    },

    // 新增：加入與刪除指標的方法
    addMetric(nameStr) {
        const inp = document.getElementById('inp-new-metric');
        const val = (nameStr || inp.value).trim();
        // 避免重複加入相同名稱的指標
        if (val && !this.tempMetrics.find(m => m.name === val)) {
            this.tempMetrics.push({ name: val, type: 'number' });
            this.renderMetricsUI();
        }
        if (inp) inp.value = '';
    },
    removeMetric(idx) {
        this.tempMetrics.splice(idx, 1);
        this.renderMetricsUI();
    },

    // editor.saveProps
    saveProps() {
        if (!this.activeBlock) return;
        const type = this.activeBlock.dataset.type;
        let props = JSON.parse(this.activeBlock.dataset.props);

        if (type === 'loop') {
            props.iterations = Number(document.getElementById('inp-iterations').value);
        } else {
            props.label = document.getElementById('inp-label').value;
            props.duration = Number(document.getElementById('inp-duration').value);
            if (type === 'timer') {
                const skipEl = document.getElementById('inp-skip-last');
                if (skipEl) props.skipOnLast = skipEl.checked;
            }
            if (type === 'reps') props.count = Number(document.getElementById('inp-count').value);

            props.customMetrics = [...this.tempMetrics];
        }

        props.color = this.selectedColor;

        this.activeBlock.dataset.props = JSON.stringify(props);
        this.activeBlock.className = `block-item p-3 mb-2 rounded border bg-white ${this.getBlockClass(type, props)}`;
        this.activeBlock.querySelector('.block-label').textContent = this.getLabel(type, props);

        this.closeProps();
        this.updateTimeline();
    },

    deleteCurrentBlock() {
        if (this.activeBlock) {
            this.activeBlock.remove();
            this.closeProps();
            this.updateTimeline();
        }
    },

    closeProps() {
        document.getElementById('prop-sheet').classList.add('translate-y-full');
        this.activeBlock = null;
    },

    serialize(container = document.getElementById('editor-canvas')) {
        const blocks = [];
        Array.from(container.children).forEach(el => {
            if (!el.classList.contains('block-item')) return;
            const block = {
                id: el.dataset.id,
                type: el.dataset.type,
                props: JSON.parse(el.dataset.props)
            };
            if (block.type === 'loop') {
                const inner = el.querySelector('.nested-container');
                block.children = this.serialize(inner);
            }
            blocks.push(block);
        });
        return blocks;
    },

    // --- 視覺化時間軸計算 ---
    updateTimeline() {
        const blocks = this.serialize();
        const container = document.getElementById('editor-timeline');

        // Temporarily flatten to calculate segments without side effects
        // Reuse timer.flatten logic but purely for data extraction
        const flattened = timer.flatten(blocks);
        const totalDuration = flattened.reduce((acc, b) => acc + (b.props.duration || 0), 0);

        document.getElementById('editor-total-time').textContent = formatTime(totalDuration);

        if (totalDuration === 0) {
            container.innerHTML = '';
            return;
        }

        let html = '';
        flattened.forEach(b => {
            const dur = b.props.duration || 0;
            if (dur <= 0) return;
            const pct = (dur / totalDuration) * 100;
            const color = b.props.color || 'gray';
            html += `<div style="width: ${pct}%" class="h-full bg-${color}-400 border-r border-white/20" title="${b.props.label} (${dur}s)"></div>`;
        });
        container.innerHTML = html;
    },

    toggleSkip(id, event) {
        event.stopPropagation(); // 防止展開屬性面板
        const el = document.querySelector(`.block-item[data-id="${id}"]`);
        if (!el) return;

        let props = JSON.parse(el.dataset.props);
        props.skipOnLast = !props.skipOnLast;
        el.dataset.props = JSON.stringify(props);

        // 更新按鈕視覺狀態
        const btnSvg = el.querySelector('.skip-btn svg');
        if (btnSvg) {
            btnSvg.className = props.skipOnLast
                ? "w-5 h-5 text-blue-500 opacity-100"
                : "w-5 h-5 text-gray-400 opacity-40 hover:opacity-100 dark:text-gray-500";
        }

        this.updateTimeline();
    },

    open() {
        this.currentId = null;
        document.getElementById('editor-title').value = '';
        this.currentRoutineTags = [];
        document.getElementById('editor-canvas').innerHTML = '<div class="text-center text-gray-400 mt-20 text-sm pointer-events-none">從下方拖曳積木至此</div>';
        document.getElementById('modal-editor').classList.add('open');
        this.renderRoutineTagsUI();
        this.updateTimeline();
    },

    load(id) {
        const r = store.routines.find(x => x.id === id);
        if (!r) return;
        this.currentId = id;
        document.getElementById('editor-title').value = r.title;
        this.currentRoutineTags = r.tags ? [...r.tags] : [];
        const canvas = document.getElementById('editor-canvas');
        canvas.innerHTML = '';
        if (r.blocks) {
            r.blocks.forEach(b => canvas.appendChild(this.createBlock(b)));
        }
        document.getElementById('modal-editor').classList.add('open');
        this.renderRoutineTagsUI();
        this.updateTimeline();
    },

    close() { document.getElementById('modal-editor').classList.remove('open'); },

    async save() {
        const title = document.getElementById('editor-title').value || '未命名課表';
        const blocks = this.serialize();
        // 將標籤加入存檔資料
        const data = { title, blocks, tags: this.currentRoutineTags, updatedAt: Date.now() };

        if (store.user) {
            const col = db.collection('users').doc(store.user.uid).collection('routines');
            if (this.currentId) {
                col.doc(this.currentId).set(data, { merge: true });
            } else {
                col.add(data);
            }
            store.loadRoutines();
        } else {
            const newId = this.currentId || 'local_' + Date.now();
            const idx = store.routines.findIndex(r => r.id === newId);
            if (idx > -1) store.routines[idx] = { id: newId, ...data };
            else store.routines.unshift({ id: newId, ...data });
            localStorage.setItem('localRoutines', JSON.stringify(store.routines));
            store.renderRoutines();
        }
        this.close();
    }

};
