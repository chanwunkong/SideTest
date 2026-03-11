// --- js/modules/timer.js ---

// --- Voice Commander ---
const voiceCommander = {
    recognition: null,
    isListening: false,
    shouldRestart: false,

    init() {
        const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
        if (!SpeechRecognition) {
            console.log("Browser does not support Speech Recognition");
            return;
        }

        this.recognition = new SpeechRecognition();
        this.recognition.continuous = false;
        this.recognition.lang = 'en-US';
        this.recognition.interimResults = false;
        this.recognition.maxAlternatives = 1;

        this.recognition.onstart = () => {
            this.isListening = true;
            this.updateUI(true);
        };

        this.recognition.onend = () => {
            this.isListening = false;
            if (this.shouldRestart) {
                try { this.recognition.start(); } catch (e) { }
            } else {
                this.updateUI(false);
            }
        };

        this.recognition.onresult = (event) => {
            const transcript = event.results[0][0].transcript.toLowerCase();
            console.log("Voice Command:", transcript);
            this.handleCommand(transcript);
        };
    },

    toggle() {
        if (!this.recognition) return alert("您的瀏覽器不支援語音控制");
        if (this.isListening) { this.stop(); } else { this.start(); }
    },

    start() {
        this.shouldRestart = true;
        try { this.recognition.start(); } catch (e) { console.error(e); }
    },

    stop() {
        this.shouldRestart = false;
        try { this.recognition.stop(); } catch (e) { console.error(e); }
    },

    updateUI(isActive) {
        const btn = document.getElementById('btn-mic-toggle');
        if (!btn) return;

        if (isActive) {
            btn.classList.add('mic-listening');
            btn.classList.remove('text-white/50');
            btn.classList.add('text-white');
        } else {
            btn.classList.remove('mic-listening');
            btn.classList.add('text-white/50');
            btn.classList.remove('text-white');
        }
    },

    handleCommand(text) {
        if (text.includes('start') || text.includes('go') || text.includes('resume') || text.includes('begin') || text.includes('開始')) {
            if (timer.isPaused) timer.toggle();
        }
        else if (text.includes('stop') || text.includes('pause') || text.includes('wait') || text.includes('hold') || text.includes('暫停')) {
            if (!timer.isPaused) timer.toggle();
        }
        else if (text.includes('next') || text.includes('skip') || text.includes('jump') || text.includes('下一步')) {
            timer.skip(1);
        }
        else if (text.includes('back') || text.includes('previous') || text.includes('上一步')) {
            timer.skip(-1);
        }
        else if (text.includes('finish') || text.includes('exit') || text.includes('end') || text.includes('結束')) {
            timer.stop();
        }
    }
};


// --- 4. Timer Engine (核心邏輯) ---
const timer = {
    queue: [],
    totalDuration: 0,
    currentIndex: 0,
    elapsed: 0,
    interval: null,
    isPaused: false,
    wakeLock: null,
    currentLogs: [],
    pendingLogBlock: null,
    pendingLogIndex: null,
    isWaitingForFinalLog: false,
    domCache: null, // DOM 快取
    lastTick: 0,    // 精確時間計算基準
    sessionValueMap: {}, // 用於存放本次訓練中各動作的最新輸入值
    editingRecordId: null, // 新增：用於追蹤正在編輯哪一筆歷史紀錄

    async requestWakeLock() {
        if ('wakeLock' in navigator) {
            try {
                this.wakeLock = await navigator.wakeLock.request('screen');
            } catch (err) {
                console.log('Wake Lock error:', err);
            }
        }
    },

    releaseWakeLock() {
        if (this.wakeLock !== null) {
            this.wakeLock.release().then(() => { this.wakeLock = null; });
        }
    },

    flatten(blocks, parentLoops = []) {
        let res = [];
        blocks.forEach(b => {
            if (b.type === 'timer' || b.type === 'reps') {
                // 檢查是否符合跳過條件：skipOnLast 為 true 且為當前迴圈的最後一次
                const isLastIteration = parentLoops.length > 0 && parentLoops[parentLoops.length - 1].current === parentLoops[parentLoops.length - 1].total;

                if (b.props.skipOnLast && isLastIteration) {
                    return; // 省略，不加入隊列
                }

                res.push({
                    ...b,
                    loopState: [...parentLoops]
                });
            } else if (b.type === 'loop') {
                const count = b.props.iterations || 1;
                for (let i = 1; i <= count; i++) {
                    const newLoopState = [...parentLoops, { current: i, total: count, id: b.id }];
                    res.push(...this.flatten(b.children || [], newLoopState));
                }
            }
        });
        return res;
    },

    // 在 timer 物件中修改 start 函式
    start(routineId) {
        this.currentRoutineId = routineId;
        const routine = store.routines.find(r => r.id === routineId);
        if (!routine || !routine.blocks) return;

        const now = new Date();
        const y = now.getFullYear();
        const m = (now.getMonth() + 1).toString().padStart(2, '0');
        const d = now.getDate().toString().padStart(2, '0');

        this.actualDate = `${y}-${m}-${d}`;
        this.actualStartTime = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
        this.actualTimestamp = now.getTime();
        this.currentRoutineTitle = routine.title;

        this.queue = this.flatten(routine.blocks);
        if (this.queue.length === 0) return alert('課表是空的');

        this.queue.push({ type: 'finish', props: { duration: 0, label: 'Done', color: 'gray' }, loopState: [] });
        this.totalDuration = this.queue.reduce((acc, item) => acc + (item.props.duration || 0), 0);

        this.currentIndex = 0;
        this.elapsed = 0;
        this.isPaused = false;
        this.currentLogs = [];
        this.pendingLogBlock = null;
        this.pendingLogIndex = null;
        this.isWaitingForFinalLog = false;
        this.sessionValueMap = {};

        this.domCache = {
            countdown: document.getElementById('active-countdown'),
            progressBar: document.getElementById('active-progress-bar'),
            status: document.getElementById('active-status'),
            remaining: document.getElementById('active-remaining'),
            elapsed: document.getElementById('active-elapsed'),
            loops: document.getElementById('active-loops'),
            nextText: document.getElementById('active-next'),
            nextBadge: document.getElementById('active-next-badge'),
            modal: document.getElementById('modal-active-timer'),
            title: document.getElementById('active-title'),
            totalDisplay: document.getElementById('active-total-display')
        };

        this.domCache.title.textContent = routine.title;
        this.domCache.totalDisplay.textContent = formatTime(this.totalDuration);
        this.domCache.modal.classList.add('open');

        this.renderTimeline();

        this.updateControlUI();
        this.runStep();
        this.startTicker();
        this.requestWakeLock();
        voiceCommander.init();
    },

    // 修改：自動判定並獲取預填數值
    getBestValue(blockLabel, metricName) {
        // 1. 優先找本次訓練已填寫過的數值 (Session Cache)
        if (this.sessionValueMap[blockLabel] && this.sessionValueMap[blockLabel][metricName] !== undefined) {
            return this.sessionValueMap[blockLabel][metricName];
        }

        // 2. 次之找該動作的上一次課程歷史紀錄 (Last Session Record)
        const lastActuals = recordManager.getLastBlockRecord(this.currentRoutineTitle, blockLabel);
        if (lastActuals && lastActuals[metricName] !== undefined) {
            return lastActuals[metricName];
        }

        // 3. 若皆無，則回傳 0 或空值
        return 0;
    },

    runStep() {
        const step = this.queue[this.currentIndex];
        if (!step) return this.stop();

        // 偵測是否為休息或結束區塊
        const isRest = step.props.label && step.props.label.toLowerCase().includes('rest');
        const isFinish = step.type === 'finish';

        // 修改攔截邏輯：增加 isFinish 的判定
        if ((isRest || isFinish) && this.currentIndex > 0) {
            const prevIndex = this.currentIndex - 1;
            const prevStep = this.queue[prevIndex];

            if (prevStep.type === 'timer' || prevStep.type === 'reps') {
                const label = (prevStep.props.label || '').toLowerCase();
                if (!label.includes('prepare')) {
                    const alreadyLogged = this.currentLogs.find(l => l.queueIndex === prevIndex);
                    if (!alreadyLogged) {
                        this.showLogPanel(prevStep, prevIndex);
                        // 如果是最後一關，開啟等待旗標
                        if (isFinish) this.isWaitingForFinalLog = true;
                    }
                }
            }
        }
        if (isFinish) {
            clearInterval(this.interval);
            this.domCache.status.textContent = "FINISHED";
            this.domCache.countdown.textContent = "--";
            this.domCache.progressBar.style.width = '0%';

            if (!this.isWaitingForFinalLog) {
                this.stop();
            }
            return;
        }

        this.stepDuration = step.props.duration || 0;
        this.stepLeft = this.stepDuration;

        // 使用 DOM Cache
        this.domCache.status.textContent = step.props.label;

        // 移除所有可能的顏色類別
        this.domCache.modal.className = this.domCache.modal.className.replace(/bg-\w+-600/g, '').trim();
        this.domCache.modal.classList.remove('pulse-urgent');

        const color = step.props.color || 'gray';
        this.domCache.modal.classList.add(`bg-${color}-600`);

        // 重置進度條
        this.domCache.progressBar.style.transition = 'none';
        this.domCache.progressBar.style.width = '100%';
        void this.domCache.progressBar.offsetWidth;
        this.domCache.progressBar.style.transition = 'width 1s linear';

        settingsManager.speak(step.props.label);
        settingsManager.beep('start');

        // 新增：更新時間軸進度視覺 (將做過的區塊變暗)
        this.queue.forEach((_, idx) => {
            const seg = document.getElementById(`timeline-seg-${idx}`);
            if (seg) {
                if (idx < this.currentIndex) {
                    seg.classList.add('opacity-30');
                } else {
                    seg.classList.remove('opacity-30');
                }
            }
        });

        this.updateDisplay();
    },

    startTicker() {
        if (this.interval) clearInterval(this.interval);

        // 紀錄開始時間，並將頻率提高至 100ms 進行高精度判斷
        this.lastTick = Date.now();

        this.interval = setInterval(() => {
            if (this.isPaused) {
                this.lastTick = Date.now(); // 暫停時重置，避免累積差值
                return;
            }

            const now = Date.now();
            const deltaSec = Math.floor((now - this.lastTick) / 1000);

            if (deltaSec >= 1) {
                this.stepLeft -= deltaSec;
                this.elapsed += deltaSec;
                this.lastTick += deltaSec * 1000; // 保留百毫秒的餘數作誤差補償

                // 倒數提示音 (Beep) & 視覺閃爍
                if (this.stepLeft <= settingsManager.data.countdownSec && this.stepLeft > 0) {
                    settingsManager.beep('countdown');
                    if (settingsManager.data.urgentPulseEnabled && !this.domCache.modal.classList.contains('pulse-urgent')) {
                        this.domCache.modal.classList.add('pulse-urgent');
                    }
                }

                if (this.stepLeft <= 0) {
                    this.currentIndex++;
                    this.runStep();
                } else {
                    this.updateDisplay();

                }
            }
        }, 100);
    },

    updateDisplay() {
        const step = this.queue[this.currentIndex];
        if (!step) return;

        const displayNum = step.type === 'reps' && this.stepLeft > 0
            ? `${step.props.count}<span class="text-4xl ml-2 opacity-50">reps</span>`
            : (this.stepLeft < 10 && this.stepLeft >= 0 ? '0' + this.stepLeft : this.stepLeft);

        // 優化：使用 DOM Cache 取代 getElementById
        this.domCache.countdown.innerHTML = displayNum;

        if (this.stepDuration > 0) {
            const progressPct = Math.max(0, (this.stepLeft / this.stepDuration) * 100);
            this.domCache.progressBar.style.width = `${progressPct}%`;
        } else {
            this.domCache.progressBar.style.width = `0%`;
        }

        let remaining = this.stepLeft || 0;
        for (let i = this.currentIndex + 1; i < this.queue.length; i++) {
            remaining += (this.queue[i].props.duration || 0);
        }

        this.domCache.remaining.textContent = formatTime(remaining);
        this.domCache.elapsed.textContent = formatTime(this.elapsed);

        if (step.loopState && step.loopState.length > 0) {
            let text = "";
            if (step.loopState.length === 1) {
                text = `ROUND ${step.loopState[0].current}/${step.loopState[0].total}`;
            } else {
                const outer = step.loopState[0];
                const inner = step.loopState[step.loopState.length - 1];
                text = `SET ${outer.current}/${outer.total} <span class="mx-3 text-white/40">|</span> REP ${inner.current}/${inner.total}`;
            }
            this.domCache.loops.innerHTML = text;
            this.domCache.loops.classList.remove('hidden');
        } else {
            this.domCache.loops.classList.add('hidden');
        }

        const nextStep = this.queue[this.currentIndex + 1];
        if (nextStep) {
            let desc = nextStep.props.label;
            if (nextStep.type === 'timer') desc += ` (${nextStep.props.duration}s)`;
            if (nextStep.type === 'reps') desc += ` x${nextStep.props.count}`;
            this.domCache.nextText.textContent = desc;

            const nc = nextStep.props.color || 'gray';
            this.domCache.nextBadge.className = `w-3 h-3 rounded-full bg-${nc}-400 shadow-sm`;
        } else {
            this.domCache.nextText.textContent = "Finish";
            this.domCache.nextBadge.className = `w-3 h-3 rounded-full bg-gray-500`;
        }
    },

    renderTimeline() {
        const timelineEl = document.getElementById('active-timeline');
        if (!timelineEl) return;

        if (this.totalDuration === 0) {
            timelineEl.innerHTML = '';
            return;
        }

        let html = '';
        this.queue.forEach((step, idx) => {
            if (step.type === 'finish') return;
            const duration = step.props.duration || 0;
            if (duration <= 0) return;

            const widthPct = (duration / this.totalDuration) * 100;
            const color = step.props.color || 'gray';

            html += `<div id="timeline-seg-${idx}" class="h-full bg-${color}-500 transition-opacity duration-300" style="width: ${widthPct}%"></div>`;
        });

        timelineEl.innerHTML = html;
    },

    // 1. 調整數值的方法 (加入浮點數處理)
    adjustLogVal(idx, delta) {
        const input = document.getElementById(`quick-log-val-${idx}`);
        if (input) {
            let val = parseFloat(input.value) || 0;
            val += delta;
            if (val < 0) val = 0;
            // 處理 JavaScript 浮點數精度問題，保留到小數點後兩位
            input.value = Math.round(val * 100) / 100;
        }
    },

    // 2. 開啟並渲染紀錄面板
    showLogPanel(block, qIndex, existingActuals = null, recordId = null) {
        const metrics = block.props.customMetrics;
        if (!metrics || metrics.length === 0) return;

        this.pendingLogBlock = block;
        this.pendingLogIndex = qIndex;
        this.editingRecordId = recordId; // 紀錄來源 ID (若為 null 則代表是當前訓練)

        const panel = document.getElementById('quick-log-panel');
        if (!panel) return;

        document.getElementById('quick-log-label').textContent = block.props.label;
        const inputsContainer = document.getElementById('quick-log-inputs');
        inputsContainer.innerHTML = '';

        metrics.forEach((m, idx) => {
            // 優先權：1. 傳入的現有值(編輯模式) 2. 最佳預填值
            const val = (existingActuals && existingActuals[m.name] !== undefined)
                ? existingActuals[m.name]
                : this.getBestValue(block.props.label, m.name);

            inputsContainer.innerHTML += `
                <div class="flex items-center justify-between bg-gray-900 rounded-2xl p-3 mb-2">
                    <span class="text-gray-300 text-sm font-bold pl-2">${m.name}</span>
                    <div class="flex items-center gap-4">
                        <button type="button" onclick="timer.adjustLogVal(${idx}, -1)" class="w-10 h-10 bg-gray-700 text-white rounded-full flex items-center justify-center font-bold text-xl active:scale-90 transition-transform">-</button>
                        <input type="number" step="any" id="quick-log-val-${idx}" data-name="${m.name}" value="${val}" 
                            class="w-20 bg-transparent text-white text-xl border-none p-0 text-center font-bold outline-none focus:ring-2 focus:ring-blue-500 rounded transition-all">
                        <button type="button" onclick="timer.adjustLogVal(${idx}, 1)" class="w-10 h-10 bg-gray-700 text-white rounded-full flex items-center justify-center font-bold text-xl active:scale-90 transition-transform">+</button>
                    </div>
                </div>
            `;
        });

        const isFailure = existingActuals && existingActuals.isFailure ? true : false;
        const failureToggle = document.getElementById('quick-log-failure');
        if (failureToggle) failureToggle.checked = isFailure;

        // 如果是編輯歷史紀錄，顯示特殊標記或調整 UI
        panel.classList.remove('hidden');
        setTimeout(() => panel.classList.remove('translate-y-[150%]'), 10);
    },

    // 關閉面板
    closeLogPanel() {
        const panel = document.getElementById('quick-log-panel');
        if (!panel) return;
        panel.classList.add('translate-y-[150%]');
        setTimeout(() => panel.classList.add('hidden'), 300);
        this.editingRecordId = null;

        if (this.isWaitingForFinalLog) {
            this.isWaitingForFinalLog = false;
            this.stop();
        }

        this.pendingLogBlock = null;
        this.pendingLogIndex = null;
    },

    // 手動召喚紀錄面板
    openManualLogPanel() {
        let targetStep = null;
        let targetIndex = -1;

        // 從當前進度往回尋找最近的一個「有效訓練區塊」
        for (let i = this.currentIndex; i >= 0; i--) {
            const step = this.queue[i];
            if (!step) continue;

            const label = (step.props.label || '').toLowerCase();
            // 必須是 timer 或 reps，且名稱不包含 prepare、rest，也不能是 finish 區塊
            if ((step.type === 'timer' || step.type === 'reps') &&
                !label.includes('prepare') &&
                !label.includes('rest') &&
                step.type !== 'finish') {
                targetStep = step;
                targetIndex = i;
                break;
            }
        }

        // 如果找到了目標動作
        if (targetStep) {
            const metrics = targetStep.props.customMetrics;
            if (!metrics || metrics.length === 0) {
                // 如果這個積木在編輯器裡沒有被加入任何追蹤指標，給予提示
                if (typeof showToast === 'function') showToast('此動作未設定追蹤指標', 'error');
                return;
            }
            this.showLogPanel(targetStep, targetIndex);
        } else {
            if (typeof showToast === 'function') showToast('目前沒有可紀錄的動作', 'error');
        }
    },

    // 帶入上次的歷史紀錄
    fillLastRecord() {
        if (!this.pendingLogBlock) return;

        const lastActuals = recordManager.getLastBlockRecord(this.currentRoutineTitle, this.pendingLogBlock.props.label);

        // 修改：使用自訂 UI 提示取代原生 alert
        if (!lastActuals) {
            showToast('未找到此動作的歷史數據', 'error');
            return;
        }

        const inputs = document.querySelectorAll('#quick-log-inputs input');
        inputs.forEach(input => {
            const name = input.dataset.name;
            if (lastActuals[name] !== undefined) input.value = lastActuals[name];
        });
    },

    // 儲存使用者輸入並加入暫存區
    saveLog() {
        if (!this.pendingLogBlock) return;
        const actuals = {};
        const blockLabel = this.pendingLogBlock.props.label;

        document.querySelectorAll('#quick-log-inputs input').forEach(input => {
            const val = input.value === '' ? 0 : Number(input.value);
            const metricName = input.dataset.name;
            actuals[metricName] = val;
        });

        // 讀取力竭開關
        const failureToggle = document.getElementById('quick-log-failure');
        if (failureToggle) actuals['isFailure'] = failureToggle.checked;

        if (this.editingRecordId) {
            // --- 模式 A: 修改歷史紀錄 ---
            recordManager.updateRecordLog(this.editingRecordId, this.pendingLogIndex, actuals);
            showToast('紀錄已更新');
        } else {
            // --- 模式 B: 當前訓練暫存 (原有邏輯) ---
            this.sessionValueMap[blockLabel] = actuals;
            this.currentLogs.push({
                blockId: this.pendingLogBlock.id,
                queueIndex: this.pendingLogIndex,
                label: blockLabel,
                planned: { duration: this.pendingLogBlock.props.duration, count: this.pendingLogBlock.props.count },
                actuals: actuals
            });
        }

        this.closeLogPanel();
        this.editingRecordId = null; // 重置
    },

    toggle() {
        this.isPaused = !this.isPaused;
        this.updateControlUI();
    },

    updateControlUI() {
        const iconEl = document.getElementById('icon-play-pause');
        const labelEl = document.getElementById('lbl-play-pause');
        const sideIconEl = document.getElementById('btn-pause-icon');
        const sideLabelEl = document.getElementById('lbl-pause-text');

        const playPath = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"></path>';
        const pausePath = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 9v6m4-6v6"></path>';

        if (this.isPaused) {
            if (iconEl) iconEl.innerHTML = playPath;
            if (labelEl) labelEl.textContent = '繼續';
            if (sideIconEl) sideIconEl.innerHTML = playPath;
            if (sideLabelEl) sideLabelEl.textContent = '繼續';
        } else {
            if (iconEl) iconEl.innerHTML = pausePath;
            if (labelEl) labelEl.textContent = '暫停';
            if (sideIconEl) sideIconEl.innerHTML = pausePath;
            if (sideLabelEl) sideLabelEl.textContent = '暫停';
        }
    },

    skip(dir) {
        // 若面板正開啟，先強制執行儲存
        const panel = document.getElementById('quick-log-panel');
        if (panel && !panel.classList.contains('hidden')) {
            this.saveLog();
        }

        const newIndex = this.currentIndex + dir;
        if (newIndex >= 0 && newIndex < this.queue.length) {
            this.currentIndex = newIndex;
            this.runStep();
        }
    },

    stop() {
        localStorage.removeItem('active_session');
        this.saveTrainingRecord();

        clearInterval(this.interval);
        voiceCommander.stop();

        const panel = document.getElementById('quick-log-panel');
        if (panel) {
            panel.classList.add('translate-y-[150%]');
            setTimeout(() => panel.classList.add('hidden'), 300);
        }

        if (this.domCache && this.domCache.modal) {
            this.domCache.modal.classList.remove('open', 'pulse-urgent');
        } else {
            const modalEl = document.getElementById('modal-active-timer');
            if (modalEl) modalEl.classList.remove('open', 'pulse-urgent');
        }

        this.releaseWakeLock();
        this.isWaitingForFinalLog = false;
    },

    suspend() {
        if (this.elapsed < 1) return this.stop(); // 剛開始就退出，直接視為結束
        const sessionData = {
            routineId: this.currentRoutineId,
            routineTitle: this.currentRoutineTitle,
            currentIndex: this.currentIndex,
            stepLeft: this.stepLeft,
            elapsed: this.elapsed,
            actualDate: this.actualDate,
            actualStartTime: this.actualStartTime,
            actualTimestamp: this.actualTimestamp,
            queue: this.queue,
            totalDuration: this.totalDuration
        };
        localStorage.setItem('active_session', JSON.stringify(sessionData));
        clearInterval(this.interval);
        voiceCommander.stop();
        document.getElementById('modal-active-timer').classList.remove('open', 'pulse-urgent');
        this.releaseWakeLock();
        store.renderRoutines(); // 重新渲染課表頁面以顯示暫存區塊
        router.go('routines');
    },

    resumeSession(sessionData) {
        this.currentRoutineId = sessionData.routineId;
        this.currentRoutineTitle = sessionData.routineTitle;
        this.currentIndex = sessionData.currentIndex;
        this.stepLeft = sessionData.stepLeft;
        this.elapsed = sessionData.elapsed;
        this.actualDate = sessionData.actualDate;
        this.actualStartTime = sessionData.actualStartTime;
        this.actualTimestamp = sessionData.actualTimestamp;
        this.queue = sessionData.queue;
        this.totalDuration = sessionData.totalDuration;

        this.isPaused = true;

        this.domCache = {
            countdown: document.getElementById('active-countdown'),
            progressBar: document.getElementById('active-progress-bar'),
            status: document.getElementById('active-status'),
            remaining: document.getElementById('active-remaining'),
            elapsed: document.getElementById('active-elapsed'),
            loops: document.getElementById('active-loops'),
            nextText: document.getElementById('active-next'),
            nextBadge: document.getElementById('active-next-badge'),
            modal: document.getElementById('modal-active-timer'),
            title: document.getElementById('active-title'),
            totalDisplay: document.getElementById('active-total-display')
        };

        this.domCache.title.textContent = this.currentRoutineTitle;
        this.domCache.totalDisplay.textContent = formatTime(this.totalDuration);
        this.domCache.modal.classList.add('open');

        this.renderTimeline();

        this.updateControlUI();
        this.restoreStep();
        this.startTicker();
        this.requestWakeLock();
        voiceCommander.init();
    },

    restoreStep() {
        const step = this.queue[this.currentIndex];
        if (!step) return this.stop();
        this.stepDuration = step.props.duration || 0;
        this.domCache.status.textContent = step.props.label;
        this.domCache.modal.className = this.domCache.modal.className.replace(/bg-\w+-600/g, '').trim();
        this.domCache.modal.classList.remove('pulse-urgent');
        const color = step.props.color || 'gray';
        this.domCache.modal.classList.add(`bg-${color}-600`);
        this.updateDisplay();
    },

    resumeFromStorage() {
        const sessionJson = localStorage.getItem('active_session');
        if (!sessionJson) return;

        try {
            const sessionData = JSON.parse(sessionJson);
            this.resumeSession(sessionData);
        } catch (error) {
            console.error('暫存資料解析失敗，已清除毀損數據', error);
            localStorage.removeItem('active_session');
        }
    },
    // 新增儲存紀錄的函式 (升級版：包含防呆與日誌寫入)
    saveTrainingRecord() {
        // 過濾：執行少於 10 秒的紀錄不予儲存，避免誤觸
        if (this.elapsed < 10) return;

        // 新增：防呆與補漏機制 (自動補齊未填寫的紀錄)

        for (let i = 0; i < this.queue.length; i++) {
            const step = this.queue[i];
            if (step.type === 'timer' || step.type === 'reps') {
                const label = (step.props.label || '').toLowerCase();

                // 排除準備 (Prepare)、休息 (Rest) 與結束 (Finish) 區塊
                if (!label.includes('prepare') && !label.includes('rest') && step.type !== 'finish') {
                    const metrics = step.props.customMetrics;
                    // 如果該積木有設定追蹤指標
                    if (metrics && metrics.length > 0) {
                        const isLogged = this.currentLogs.find(l => l.queueIndex === i);
                        // 如果在 currentLogs 裡面找不到這一組的紀錄 (代表使用者忽視了面板)
                        if (!isLogged) {
                            const actuals = {};
                            metrics.forEach(m => {
                                // 自動以計畫值作為實際值
                                actuals[m.name] = m.name === '次數' ? (step.props.count || 0) : (m.name === '秒數' ? (step.props.duration || 0) : 0);
                            });
                            this.currentLogs.push({
                                blockId: step.id,
                                queueIndex: i,
                                label: step.props.label,
                                planned: { duration: step.props.duration, count: step.props.count },
                                actuals: actuals
                            });
                        }
                    }
                }
            }
        }

        // 將所有紀錄依據在課表中的執行順序 (queueIndex) 重新排序，確保日誌時序正確
        this.currentLogs.sort((a, b) => a.queueIndex - b.queueIndex);
        // ==========================================

        // 取得當下課表的標籤快照 (補上這兩行以定義 currentTags)
        const routine = store.routines.find(r => r.id === this.currentRoutineId);
        const currentTags = routine && routine.tags ? [...routine.tags] : [];

        const newRecord = {
            id: uuid(),
            routineId: this.currentRoutineId,
            routineTitle: this.currentRoutineTitle,
            tags: currentTags,
            date: this.actualDate,
            startTime: this.actualStartTime,
            duration: this.elapsed,
            timestamp: this.actualTimestamp,
            executionLogs: this.currentLogs
        };

        // 取得現有紀錄並推入新紀錄
        const records = JSON.parse(localStorage.getItem('trainingRecords') || '[]');
        records.push(newRecord);

        // 存回 LocalStorage
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        // 清空當前暫存狀態，確保完全乾淨，不污染下一趟訓練
        this.currentLogs = [];
        this.pendingLogBlock = null;
        this.pendingLogIndex = null;

        // 存檔後立即更新 UI
        this.refreshRecordUI();
    },

    // 更新紀錄頁面的數據 (黑框與日曆)
    refreshRecordUI() {
        console.log("紀錄已儲存，正在更新 UI...");
        recordManager.updateUI(); // 呼叫剛剛建立的更新邏輯
    },
};

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') {
        const modalEl = document.getElementById('modal-active-timer');
        if (modalEl && modalEl.classList.contains('open')) {
            timer.requestWakeLock();
        }
    }
});