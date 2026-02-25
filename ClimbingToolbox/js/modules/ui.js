// --- js/modules/ui.js ---

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
        soundStart: true,
        soundCountdown: true,
        soundFinish: true,
        urgentPulseEnabled: true,

        // Profile
        weightVal: '',
        weightUnit: 'kg'
    },

    init() {
        const saved = localStorage.getItem('appSettings');
        if (saved) {
            this.data = { ...this.data, ...JSON.parse(saved) };
        }
        this.bindUI();
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

        // Check granular settings
        if (type === 'start' && !this.data.soundStart) return;
        if (type === 'countdown' && !this.data.soundCountdown) return;
        if (type === 'finish' && !this.data.soundFinish) return;

        // Freq Logic
        let freq = 880;
        let wave = 'sine';
        let dur = 0.1;

        if (type === 'countdown') { freq = 440; }
        if (type === 'start') { freq = 880; wave = 'square'; dur = 0.2; }
        if (type === 'finish') { freq = 523.25; wave = 'triangle'; dur = 0.4; } // C5

        try {
            const AudioContext = window.AudioContext || window.webkitAudioContext;
            if (!AudioContext) return;
            const ctx = new AudioContext();
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

            // Double beep for finish
            if (type === 'finish') {
                setTimeout(() => {
                    const osc2 = ctx.createOscillator();
                    const gain2 = ctx.createGain();
                    osc2.connect(gain2);
                    gain2.connect(ctx.destination);
                    osc2.frequency.value = freq * 1.25; // E5
                    osc2.type = wave;
                    gain2.gain.setValueAtTime(0.1, ctx.currentTime);
                    gain2.gain.exponentialRampToValueAtTime(0.00001, ctx.currentTime + dur);
                    osc2.start();
                    osc2.stop(ctx.currentTime + dur);
                }, 150);
            }
        } catch (e) { console.error(e); }
    },

    speak(text) {
        if (!this.data.soundEnabled || !this.data.ttsEnabled) return;
        if (!window.speechSynthesis) return;

        window.speechSynthesis.cancel();
        const u = new SpeechSynthesisUtterance(text);
        u.rate = this.data.speechRate;
        u.lang = 'en-US';
        window.speechSynthesis.speak(u);
    }
};



// --- 1. Router ---
const router = {
    go(viewId) {
        document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
        document.querySelectorAll('.nav-btn').forEach(el => {
            el.classList.remove('text-gray-900', 'text-blue-600');
            el.classList.add('text-gray-400');
            // Reset Dark Mode active state styles
            el.classList.remove('dark:text-white');
            el.classList.add('dark:text-gray-400');
        });
        document.getElementById(`view-${viewId}`).classList.add('active');
        const btn = document.querySelector(`.nav-btn[data-target="view-${viewId}"]`);
        if (btn) {
            btn.classList.remove('text-gray-400', 'dark:text-gray-400');
            btn.classList.add('text-gray-900', 'dark:text-white');
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

    // 預設選項 (統一為 Title Case English)
    defaultSuggestions: {
        'timer': [
            { label: 'Prepare', color: 'blue' },
            { label: 'Hang', color: 'orange' },
            { label: 'Rest', color: 'green' }
        ],
        'reps': [
            { label: 'Pull Ups', color: 'indigo' },
            { label: 'Push Ups', color: 'pink' }
        ],
        'loop': []
    },

    init() {
        new Sortable(document.getElementById('editor-palette'), {
            group: { name: 'shared', pull: 'clone', put: false },
            sort: false,
            onClone: (evt) => { evt.clone.dataset.tempId = Date.now(); }
        });
        this.initSortable(document.getElementById('editor-canvas'));
    },

    initSortable(el) {
        new Sortable(el, {
            group: 'shared',
            animation: 150,
            fallbackOnBody: true,
            ghostClass: 'sortable-ghost',
            dragClass: 'sortable-drag',
            draggable: '.block-item', // 限制只有 block-item 可以拖曳
            onAdd: (evt) => {
                const type = evt.item.dataset.type;
                evt.item.replaceWith(this.createBlock({ type, id: uuid() }));
                this.updateTimeline();
            },
            onUpdate: () => this.updateTimeline(),
            onRemove: () => this.updateTimeline()
        });
    },

    // 取得積木類別 (根據 props.color)
    getBlockClass(type, props) {
        const c = props.color || 'gray';
        return `border-l-4 border-l-${c}-500 bg-${c}-50 border-${c}-200 dark:bg-${c}-900/20 dark:border-${c}-800`;
    },

    createBlock(data) {
        const el = document.createElement('div');
        const props = data.props || this.getDefaultProps(data.type);

        el.className = `block-item p-3 mb-2 rounded border bg-white ${this.getBlockClass(data.type, props)}`;
        el.dataset.type = data.type;
        el.dataset.id = data.id || uuid();
        el.dataset.props = JSON.stringify(props);

        // 生成外部跳過按鈕 (僅限 Timer)
        const isSkip = !!props.skipOnLast;
        const skipColor = isSkip ? 'text-blue-500 opacity-100' : 'text-gray-400 opacity-40 hover:opacity-100 dark:text-gray-500';
        const skipButton = data.type === 'timer' ? `
 
                ` : '';

        const header = document.createElement('div');
        header.className = "flex justify-between items-center cursor-pointer w-full";
        header.innerHTML = `
                    <div class="flex items-center gap-2 pointer-events-none flex-1">
                        <span class="text-xs font-bold uppercase opacity-60 dark:text-gray-400">${data.type}</span>
                        <span class="font-bold text-sm block-label dark:text-white">${this.getLabel(data.type, props)}</span>
                    </div>
                    ${skipButton}
                `;
        header.onclick = (e) => {
            e.stopPropagation();
            this.openProps(el);
        };
        el.appendChild(header);

        if (data.type === 'loop') {
            const inner = document.createElement('div');
            inner.className = "nested-container pl-2 pr-2 pb-2 pt-2";
            el.appendChild(inner);
            this.initSortable(inner);
            if (data.children) {
                data.children.forEach(c => inner.appendChild(this.createBlock(c)));
            }
        }

        return el;
    },

    getDefaultProps(type) {
        if (type === 'loop') return { iterations: 3, color: 'violet' };
        if (type === 'timer') return { duration: 10, label: 'Hang', color: 'orange' };
        if (type === 'reps') return { count: 5, duration: 30, label: 'Pull Ups', color: 'blue' };
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

        // 1. 生成表單內容
        let html = '';
        if (type !== 'loop') {
            // Create Options String for Select
            let optionsHtml = '<option value="" disabled selected>Select from history...</option>';

            const historyMap = this.getHistory(type);
            const defaults = this.defaultSuggestions[type] || [];

            // Add Defaults to history map for color lookup
            defaults.forEach(def => {
                if (!historyMap.has(def.label)) historyMap.set(def.label, def.color);
            });

            // Build Options (Unique)
            const addedLabels = new Set();

            // Add Defaults
            defaults.forEach(def => {
                optionsHtml += `<option value="${def.label}">${def.label}</option>`;
                addedLabels.add(def.label);
            });

            // Add History
            historyMap.forEach((color, label) => {
                if (!addedLabels.has(label)) {
                    optionsHtml += `<option value="${label}">${label}</option>`;
                }
            });

            html += `
                        <div>
                            <label class="block text-sm font-bold text-gray-500 mb-1">標籤 (Label)</label>
                            <div class="flex flex-col gap-2">
                                <input type="text" id="inp-label" class="w-full border rounded-lg p-3 dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.label || ''}" placeholder="Type custom name...">
                                <select id="sel-label" class="w-full border rounded-lg p-2 text-sm text-gray-600 bg-gray-50 dark:bg-gray-600 dark:text-gray-300 dark:border-gray-500">
                                    ${optionsHtml}
                                </select>
                            </div>
                        </div>`;

            // Store map for color lookup in event listener
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
                            <div class="text-xs text-gray-500 mt-1">若位於迴圈內，最後一次迭代將自動省略此動作</div>
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
                            <label class="block text-sm font-bold text-gray-500 mb-1">次數 (Reps)</label>
                            <input type="number" id="inp-count" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.count}">
                        </div>
                        <div>
                            <label class="block text-sm font-bold text-gray-500 mb-1">預估時間 (秒)</label>
                            <input type="number" id="inp-duration" class="w-full border rounded-lg p-3 text-lg dark:bg-gray-700 dark:border-gray-600 dark:text-white" value="${props.duration}" placeholder="計時用">
                        </div>
                    </div>`;
        }
        form.innerHTML = html;

        // 2. 生成顏色選擇器
        this.selectedColor = props.color || 'gray';
        const palette = document.getElementById('color-palette');
        palette.innerHTML = '';
        this.colors.forEach(c => {
            const btn = document.createElement('div');
            btn.className = `color-btn w-8 h-8 rounded-full cursor-pointer shrink-0 bg-${c}-500 ${c === this.selectedColor ? 'selected' : ''}`;
            btn.onclick = () => {
                this.selectedColor = c;
                // Update UI selection
                Array.from(palette.children).forEach(child => child.classList.remove('selected'));
                btn.classList.add('selected');
            };
            palette.appendChild(btn);
        });

        // 3. 綁定事件 (Select -> Input & Color)
        const selLabel = document.getElementById('sel-label');
        const inpLabel = document.getElementById('inp-label');

        if (selLabel && inpLabel) {
            selLabel.addEventListener('change', (e) => {
                const val = e.target.value;
                inpLabel.value = val;

                // Auto-select color
                if (this.tempHistoryMap && this.tempHistoryMap.has(val)) {
                    const historyColor = this.tempHistoryMap.get(val);
                    this.selectedColor = historyColor;
                    // Update Palette UI
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

    // 替換 editor.saveProps
    saveProps() {
        if (!this.activeBlock) return;
        const type = this.activeBlock.dataset.type;
        let props = JSON.parse(this.activeBlock.dataset.props);

        // 更新基本屬性
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
        }

        // 更新顏色
        props.color = this.selectedColor;

        // 寫回 DOM
        this.activeBlock.dataset.props = JSON.stringify(props);
        this.activeBlock.className = `block-item p-3 mb-2 rounded border bg-white ${this.getBlockClass(type, props)}`;
        this.activeBlock.querySelector('.block-label').textContent = this.getLabel(type, props);

        // 同步外部跳過按鈕狀態
        const btnSvg = this.activeBlock.querySelector('.skip-btn svg');
        if (btnSvg) {
            btnSvg.className = props.skipOnLast
                ? "w-5 h-5 text-blue-500 opacity-100"
                : "w-5 h-5 text-gray-400 opacity-40 hover:opacity-100 dark:text-gray-500";
        }

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
        document.getElementById('editor-canvas').innerHTML = '<div class="text-center text-gray-400 mt-20 text-sm pointer-events-none">從下方拖曳積木至此</div>';
        document.getElementById('modal-editor').classList.add('open');
        this.updateTimeline();
    },

    load(id) {
        const r = store.routines.find(x => x.id === id);
        if (!r) return;
        this.currentId = id;
        document.getElementById('editor-title').value = r.title;
        const canvas = document.getElementById('editor-canvas');
        canvas.innerHTML = '';
        if (r.blocks) {
            r.blocks.forEach(b => canvas.appendChild(this.createBlock(b)));
        }
        document.getElementById('modal-editor').classList.add('open');
        this.updateTimeline();
    },

    close() { document.getElementById('modal-editor').classList.remove('open'); },

    async save() {
        const title = document.getElementById('editor-title').value || '未命名課表';
        const blocks = this.serialize();
        const data = { title, blocks, updatedAt: Date.now() };

        if (store.user) {
            const col = db.collection('users').doc(store.user.uid).collection('routines');
            // 移除 await，讓 Firebase 在背景處理同步，避免斷網時卡住 UI
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
