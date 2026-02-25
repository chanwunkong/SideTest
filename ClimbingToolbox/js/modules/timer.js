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
        if (timer.isLocked) return; // 鎖定時禁用語音控制

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
    isLocked: false,
    wakeLock: null,

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
        const routine = store.routines.find(r => r.id === routineId);
        if (!routine || !routine.blocks) return;

        // --- 修正：使用在地日期而非 ISO 字串 ---
        const now = new Date();
        const y = now.getFullYear();
        const m = (now.getMonth() + 1).toString().padStart(2, '0');
        const d = now.getDate().toString().padStart(2, '0');

        this.actualDate = `${y}-${m}-${d}`; // 產出正確的 YYYY-MM-DD
        this.actualStartTime = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
        this.actualTimestamp = now.getTime();
        this.currentRoutineTitle = routine.title;
        // ------------------------------------

        this.queue = this.flatten(routine.blocks);
        if (this.queue.length === 0) return alert('課表是空的');

        this.queue.push({ type: 'finish', props: { duration: 0, label: 'Done', color: 'gray' }, loopState: [] });
        this.totalDuration = this.queue.reduce((acc, item) => acc + (item.props.duration || 0), 0);

        this.currentIndex = 0;
        this.elapsed = 0;
        this.isPaused = false;
        this.isLocked = false;

        document.getElementById('active-title').textContent = routine.title;
        document.getElementById('active-total-display').textContent = formatTime(this.totalDuration);
        document.getElementById('modal-active-timer').classList.add('open');

        this.updateLockUI();
        this.updateControlUI();
        this.runStep();
        this.startTicker();
        this.requestWakeLock();
        voiceCommander.init();
    },

    toggleLock() {
        this.isLocked = !this.isLocked;
        this.updateLockUI();
    },

    updateLockUI() {
        const overlay = document.getElementById('lock-overlay');
        const lockBtn = document.getElementById('btn-lock');

        if (this.isLocked) {
            overlay.classList.remove('hidden');
            // 變更鎖頭圖示為 "解鎖"
            lockBtn.innerHTML = '<svg class="w-6 h-6 text-red-400" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 11V7a4 4 0 118 0m-4 8v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2z"></path></svg>';
            lockBtn.classList.add('bg-white', 'hover:bg-gray-100');
            lockBtn.classList.remove('bg-white/10', 'hover:bg-white/20');
        } else {
            overlay.classList.add('hidden');
            // 變更鎖頭圖示為 "鎖定"
            lockBtn.innerHTML = '<svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z"></path></svg>';
            lockBtn.classList.remove('bg-white', 'hover:bg-gray-100');
            lockBtn.classList.add('bg-white/10', 'hover:bg-white/20');
        }
    },

    runStep() {
        const step = this.queue[this.currentIndex];
        if (!step) return this.stop();

        this.stepDuration = step.props.duration || 0;
        this.stepLeft = this.stepDuration;

        document.getElementById('active-status').textContent = step.props.label;

        const statusEl = document.getElementById('active-status');
        const modalEl = document.getElementById('modal-active-timer');

        // 移除所有可能的顏色類別
        modalEl.className = modalEl.className.replace(/bg-\w+-600/g, '').trim();
        // 移除急迫閃爍
        modalEl.classList.remove('pulse-urgent');

        const color = step.props.color || 'gray';
        modalEl.classList.add(`bg-${color}-600`);

        // 調整文字顏色
        if (step.type === 'finish') {
            statusEl.textContent = "FINISHED";
            this.stop();
            return;
        }

        // 重置進度條
        const progressBar = document.getElementById('active-progress-bar');
        progressBar.style.transition = 'none';
        progressBar.style.width = '100%';
        void progressBar.offsetWidth; // 強制重繪
        progressBar.style.transition = 'width 1s linear';

        // TTS 朗讀 (使用 settingsManager)
        settingsManager.speak(step.props.label);

        // Play Start Sound
        settingsManager.beep('start');

        this.updateDisplay();
    },

    startTicker() {
        if (this.interval) clearInterval(this.interval);
        this.interval = setInterval(() => {
            if (this.isPaused) return;

            if (this.stepLeft > 0) {
                this.stepLeft--;
                this.elapsed++;

                // 倒數提示音 (Beep) & 視覺閃爍
                if (this.stepLeft <= settingsManager.data.countdownSec) {
                    settingsManager.beep('countdown');

                    // 最後3秒觸發背景閃爍
                    const modalEl = document.getElementById('modal-active-timer');
                    if (settingsManager.data.urgentPulseEnabled && !modalEl.classList.contains('pulse-urgent')) {
                        modalEl.classList.add('pulse-urgent');
                    }
                }

            } else {
                // 轉換邏輯：結束當前步驟
                this.currentIndex++;
                if (this.currentIndex < this.queue.length) {
                    this.runStep();
                } else {
                    // 完成
                    settingsManager.beep('finish');
                    this.stop();
                }
            }
            this.updateDisplay();
        }, 1000);
    },

    updateDisplay() {
        const step = this.queue[this.currentIndex];
        if (!step) return;

        const displayNum = step.type === 'reps' && this.stepLeft > 0
            ? `${step.props.count}<span class="text-4xl ml-2 opacity-50">reps</span>`
            : (this.stepLeft < 10 ? '0' + this.stepLeft : this.stepLeft);

        document.getElementById('active-countdown').innerHTML = displayNum;

        // 更新進度條
        if (this.stepDuration > 0) {
            const progressPct = (this.stepLeft / this.stepDuration) * 100;
            document.getElementById('active-progress-bar').style.width = `${progressPct}%`;
        } else {
            document.getElementById('active-progress-bar').style.width = `0%`;
        }

        const remaining = Math.max(0, this.totalDuration - this.elapsed);
        document.getElementById('active-elapsed').textContent = formatTime(this.elapsed);
        document.getElementById('active-remaining').textContent = formatTime(remaining);
        // active-total-display is static per routine start, but nice to keep formatted

        const loopState = step.loopState;
        const loopEl = document.getElementById('active-loops');
        if (step.loopState && step.loopState.length > 0) {
            let text = "";
            if (step.loopState.length === 1) {
                text = `ROUND ${step.loopState[0].current}/${step.loopState[0].total}`;
            } else {
                const outer = step.loopState[0];
                const inner = step.loopState[step.loopState.length - 1];
                text = `SET ${outer.current}/${outer.total} <span class="mx-3 text-white/40">|</span> REP ${inner.current}/${inner.total}`;
            }
            loopEl.innerHTML = text;
            loopEl.classList.remove('hidden');
        } else {
            loopEl.classList.add('hidden');
        }

        // 下一步預告與顏色點
        const nextStep = this.queue[this.currentIndex + 1];
        const nextEl = document.getElementById('active-next');
        const nextBadge = document.getElementById('active-next-badge');
        if (nextStep) {
            let desc = nextStep.props.label;
            if (nextStep.type === 'timer') desc += ` (${nextStep.props.duration}s)`;
            if (nextStep.type === 'reps') desc += ` x${nextStep.props.count}`;
            nextEl.textContent = desc;

            // 下一步顏色
            const nc = nextStep.props.color || 'gray';
            nextBadge.className = `w-3 h-3 rounded-full bg-${nc}-400 shadow-sm`;
        } else {
            nextEl.textContent = "Finish";
            nextBadge.className = `w-3 h-3 rounded-full bg-gray-500`;
        }
    },

    toggle() {
        if (this.isLocked) return; // 鎖定狀態下無法暫停/繼續 (除非解鎖)
        this.isPaused = !this.isPaused;
        this.updateControlUI();
    },

    updateControlUI() {
        const iconEl = document.getElementById('icon-play-pause');
        const labelEl = document.getElementById('lbl-play-pause');

        if (this.isPaused) {
            iconEl.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"></path>';
            labelEl.textContent = '繼續';
        } else {
            iconEl.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 9v6m4-6v6"></path>';
            labelEl.textContent = '暫停';
        }
    },

    skip(dir) {
        if (this.isLocked) return; // 鎖定狀態下無法跳過
        const newIndex = this.currentIndex + dir;
        if (newIndex >= 0 && newIndex < this.queue.length) {
            this.currentIndex = newIndex;
            this.runStep();
        }
    },

    stop() {
        // 1. 產生訓練紀錄
        this.saveTrainingRecord();

        // 2. 原有的停止邏輯
        clearInterval(this.interval);
        voiceCommander.stop();
        document.getElementById('modal-active-timer').classList.remove('open');
        document.getElementById('modal-active-timer').classList.remove('pulse-urgent');
        this.releaseWakeLock();
    },

    // 新增儲存紀錄的函式
    saveTrainingRecord() {
        // 過濾：執行少於 10 秒的紀錄不予儲存，避免誤觸
        if (this.elapsed < 10) return;

        const newRecord = {
            id: uuid(),
            routineTitle: this.currentRoutineTitle,
            date: this.actualDate,
            startTime: this.actualStartTime,
            duration: this.elapsed,
            timestamp: this.actualTimestamp
        };

        // 取得現有紀錄
        const records = JSON.parse(localStorage.getItem('trainingRecords') || '[]');
        records.push(newRecord);

        // 存回 LocalStorage (若有 Firebase 則在此處呼叫 db.collection)
        localStorage.setItem('trainingRecords', JSON.stringify(records));

        // 存檔後立即更新 UI
        this.refreshRecordUI();
    },

    // 更新紀錄頁面的數據 (黑框與日曆)
    refreshRecordUI() {
        console.log("紀錄已儲存，正在更新 UI...");
        recordManager.updateUI(); // 呼叫剛剛建立的更新邏輯
    },
};

