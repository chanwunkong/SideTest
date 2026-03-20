// --- js/modules/bleManager.js ---

export const bleManager = {
    MANUFACTURER_ID: 256,
    WEIGHT_OFFSET: 10,
    TIMEOUT_SECONDS: 10,       // 斷線超時判定 (10秒)
    VERIFY_TIMEOUT_MS: 3000,   // 自動連線驗證時間 (3秒)

    connectedDevice: null,
    advertisementTimeout: null,
    verifyTimeout: null,
    currentWeight: 0,          // 暫存最新重量數據

    // 圖表相關變數
    MAX_DATA_POINTS: 100,
    chartData: [],
    chartCtx: null,
    boundHandleAdvertisement: null, // 用於正確綁定與移除監聽器
    manualTargetWeight: null, // 用於記錄當前動作手動覆寫的目標重量

    // 模擬模式專用變數
    isMockMode: false,
    mockInterval: null,

    initChart() {
        const canvas = document.getElementById('ble-live-chart');
        if (canvas && !this.chartCtx) {
            // 根據設備像素比例設定畫布，確保渲染清晰
            canvas.width = canvas.offsetWidth * window.devicePixelRatio;
            canvas.height = canvas.offsetHeight * window.devicePixelRatio;
            this.chartCtx = canvas.getContext('2d');
        }
        this.chartData = [];
    },

    updateChart(weight) {
        if (!this.chartCtx) this.initChart();
        const canvas = document.getElementById('ble-live-chart');
        if (!canvas || !this.chartCtx) return;

        const ctx = this.chartCtx;
        this.chartData.push(weight);
        if (this.chartData.length > this.MAX_DATA_POINTS) this.chartData.shift();

        ctx.clearRect(0, 0, canvas.width, canvas.height);
        if (this.chartData.length < 2) return;

        // 動態計算 Y 軸範圍 (至少預留 10kg 的視覺空間)
        const maxWeight = Math.max(...this.chartData, 10);
        const stepX = canvas.width / (this.MAX_DATA_POINTS - 1);

        // --- 1. 繪製 Peak 峰值線段 ---
        const peakWeight = Math.max(...this.chartData);
        if (peakWeight > 0) {
            const peakY = canvas.height - ((peakWeight / maxWeight) * canvas.height * 0.9);

            ctx.save(); // 儲存狀態以避免影響後續繪圖
            ctx.beginPath();
            ctx.setLineDash([5, 5]); // 設定虛線樣式 [實線長度, 空格長度]
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.4)'; // 使用半透明白色
            ctx.lineWidth = 1;
            ctx.moveTo(0, peakY);
            ctx.lineTo(canvas.width, peakY);
            ctx.stroke();

            // 繪製 Peak 數值標籤
            ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
            ctx.font = '10px tabular-nums font-bold';
            ctx.textAlign = 'right';
            ctx.fillText(`${peakWeight.toFixed(1)} Peak`, canvas.width - 5, peakY - 5);
            ctx.restore(); // 恢復狀態 (清除虛線設定)
        }

        // --- 2. 繪製即時力量曲線 ---
        ctx.beginPath();
        // 使用半透明白色以適應計時器各種背景色
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.lineWidth = 4;
        ctx.lineJoin = 'round';

        this.chartData.forEach((w, index) => {
            const x = index * stepX;
            // 數值映射至 Y 軸 (底部為 0，留 10% 頂部邊距)
            const y = canvas.height - ((w / maxWeight) * canvas.height * 0.9);
            if (index === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    },

    // ▼ 新增：輔助抓取目前正在執行的計時器動作 ▼
    getCurrentBlock() {
        if (typeof timer !== 'undefined' && timer.queue && timer.queue.length > 0) {
            const step = timer.queue[timer.currentIndex];
            if (step && (step.type === 'timer' || step.type === 'reps')) {
                return step;
            }
        }
        return null;
    },

    updateGauge(weight) {
        const pointer = document.getElementById('ble-gauge-pointer');
        const displayEl = document.getElementById('ble-target-display');
        if (!pointer) return;

        // 1. 決定目標重量 (改用 getCurrentBlock)
        let targetWeight = 20;
        const currentBlock = this.getCurrentBlock();

        if (this.manualTargetWeight !== null) {
            targetWeight = this.manualTargetWeight;
        } else if (currentBlock) {
            targetWeight = timer.getBestValue(currentBlock.props.label, '重量') || 20;
        }

        // ▼ 新增這行：防呆機制，防止 targetWeight 為 0 導致除以零崩潰 ▼
        if (targetWeight <= 0) targetWeight = 1;

        // 更新介面上的目標數字
        if (displayEl) displayEl.textContent = targetWeight;

        // 2. 計算百分比位置 (Mapping)
        let percentage = 0;
        const lowBound = targetWeight * 0.8;
        const highBound = targetWeight * 1.2;

        if (weight <= lowBound) {
            percentage = (weight / lowBound) * 40;
        } else if (weight <= highBound) {
            percentage = 40 + ((weight - lowBound) / (highBound - lowBound)) * 20;
        } else {
            const overWeight = weight - highBound;
            percentage = 60 + (overWeight / (targetWeight * 0.8)) * 40;
        }

        percentage = Math.min(Math.max(percentage, 1), 99);
        pointer.style.left = `${percentage}%`;

        // 4. 動態顏色回饋
        if (percentage < 40) {
            pointer.style.backgroundColor = '#facc15';
            pointer.style.boxShadow = '0 0 15px rgba(250, 204, 21, 0.8)';
            pointer.classList.remove('animate-pulse');
        } else if (percentage <= 60) {
            pointer.style.backgroundColor = '#4ade80';
            pointer.style.boxShadow = '0 0 20px rgba(74, 222, 128, 0.9)';
            pointer.classList.add('animate-pulse');
        } else {
            pointer.style.backgroundColor = '#f87171';
            pointer.style.boxShadow = '0 0 15px rgba(248, 113, 113, 0.8)';
            pointer.classList.remove('animate-pulse');
        }
    },

    // ▼ 將歷史數值快速填入輸入框 ▼
    applyHistoryTarget(val) {
        const input = document.getElementById('ble-target-input');
        if (input) input.value = val;
    },

    // 藍牙目標面板控制邏輯
    openTargetModal() {
        const modal = document.getElementById('modal-ble-target');
        const sheet = document.getElementById('ble-target-sheet');
        const input = document.getElementById('ble-target-input');
        const blockNameEl = document.getElementById('ble-target-block-name');

        // 歷史紀錄 DOM
        const historyContainer = document.getElementById('ble-target-history-container');
        const historyList = document.getElementById('ble-target-history-list');

        let currentTarget = 20;
        let blockName = "未知動作";
        const currentBlock =， this.getCurrentBlock();

        // 帶入目前的目標設定值
        if (this.manualTargetWeight !== null) {
            currentTarget = this.manualTargetWeight;
            blockName = currentBlock && currentBlock.props ? currentBlock.props.label : "自訂目標";
        } else if (currentBlock && currentBlock.props) {
            currentTarget = timer.getBestValue(currentBlock.props.label, '重量') || 20;
            blockName = currentBlock.props.label;
        }

        if (blockNameEl) blockNameEl.textContent = `目前動作: ${blockName}`;
        if (input) input.value = currentTarget;

        // ▼ 強化版：不限當前區塊，抓取整個課表的所有數字指標 ▼
        if (historyContainer && historyList) {
            historyList.innerHTML = '';
            let hasHistory = false;
            let availableHistory = {}; // 暫存所有找到的指標 { "動作名-指標名": 數值 }

            if (typeof timer !== 'undefined' && timer.currentRoutineTitle) {
                // 1. 先抓取「上一次練這個課表」的所有紀錄
                if (typeof recordManager !== 'undefined') {
                    const records = recordManager.getAllRecords();
                    // 找到該課表最新的一筆完成紀錄
                    const lastRecord = [...records].reverse().find(r => r.routineTitle === timer.currentRoutineTitle && r.executionLogs);
                    if (lastRecord) {
                        lastRecord.executionLogs.forEach(log => {
                            if (log.actuals) {
                                Object.entries(log.actuals).forEach(([key, val]) => {
                                    const numVal = Number(val);
                                    if (key !== 'isFailure' && key !== '次數' && key !== '秒數' && !isNaN(numVal)) {
                                        availableHistory[`${log.label}-${key}`] = numVal;
                                    }
                                });
                            }
                        });
                    }
                }

                // 2. 優先覆蓋：抓取「本次訓練稍早」已經填寫的紀錄
                if (timer.sessionValueMap) {
                    Object.entries(timer.sessionValueMap).forEach(([bLabel, actuals]) => {
                        Object.entries(actuals).forEach(([key, val]) => {
                            const numVal = Number(val);
                            if (key !== 'isFailure' && key !== '次數' && key !== '秒數' && !isNaN(numVal)) {
                                availableHistory[`${bLabel}-${key}`] = numVal;
                            }
                        });
                    });
                }
            }

            // 將收集到的歷史指標轉換成按鈕
            Object.entries(availableHistory).forEach(([displayLabel, val]) => {
                hasHistory = true;
                const btn = document.createElement('button');
                btn.type = 'button';
                // 加入 whitespace-nowrap 避免按鈕文字被擠到換行
                btn.className = 'px-3 py-2 bg-gray-700 hover:bg-gray-600 text-white text-xs font-bold rounded-lg border border-gray-600 transition-colors flex items-center gap-1.5 active:scale-95 whitespace-nowrap';

                // 將 "動作名-指標名" 拆開顯示，增加易讀性
                const parts = displayLabel.split('-');
                btn.innerHTML = `<span class="text-gray-400">${parts[0]} <span class="text-gray-500 mx-0.5">|</span> ${parts[1]}</span> <span>${val} kg</span>`;
                btn.onclick = () => this.applyHistoryTarget(val);

                historyList.appendChild(btn);
            });

            if (hasHistory) {
                historyContainer.classList.remove('hidden');
            } else {
                historyContainer.classList.add('hidden');
            }
        }

        if (modal) {
            modal.classList.remove('hidden');
            void modal.offsetWidth;
            modal.classList.remove('opacity-0');
            modal.classList.add('opacity-100');
            if (sheet) {
                sheet.classList.remove('scale-95');
                sheet.classList.add('scale-100');
            }
        }
    },

    closeTargetModal() {
        const modal = document.getElementById('modal-ble-target');
        const sheet = document.getElementById('ble-target-sheet');
        if (modal) {
            modal.classList.remove('opacity-100');
            modal.classList.add('opacity-0');
            if (sheet) {
                sheet.classList.remove('scale-100');
                sheet.classList.add('scale-95');
            }

            // 等待動畫結束後再隱藏
            setTimeout(() => {
                modal.classList.add('hidden');
            }, 200);
        }
    },

    adjustTarget(delta) {
        const input = document.getElementById('ble-target-input');
        if (input) {
            let val = parseFloat(input.value) || 0;
            val += delta;
            if (val < 0) val = 0;
            input.value = Math.round(val * 100) / 100;
        }
    },

    saveTargetModal() {
        const input = document.getElementById('ble-target-input');
        if (input) {
            this.manualTargetWeight = parseFloat(input.value) || 0;
            this.updateGauge(this.currentWeight || 0); // 立即刷新儀表板
        }
        this.closeTargetModal();
    },

    // 切換連線狀態 (按鈕的主要進入點)
        async toggleConnection() {
        // 若已連線或正在模擬中，則執行手動斷線
        if (this.connectedDevice || this.isMockMode) {
            this.disconnect(true);
            return;
        }

        if (!this.boundHandleAdvertisement) {
            this.boundHandleAdvertisement = this.handleAdvertisement.bind(this);
        }

        this.updateButtonUI('connecting');

        // 若無藍牙功能，直接進入模擬模式
        if (!navigator.bluetooth) {
            this.startMockMode();
            return;
        }

        try {
            // 💡 核心修正：刪除 getDevices() 自動連線邏輯
            // 強制每次都像心率帶一樣，呼叫原生的選擇視窗
            await this.requestNewDevice();
            
        } catch (error) {
            console.error('藍牙配對錯誤:', error);
            // 配對取消或失敗時，退回模擬模式
            this.startMockMode();
        }
    },

    // 模擬數據生成邏輯
    startMockMode() {
        this.isMockMode = true;
        this.onConnected();

        this.mockInterval = setInterval(() => {
            const time = Date.now() / 1000;
            // 產生隨時間波動的模擬重量
            let mockWeight = 20 + 15 * Math.sin(time * 1.5) + (Math.random() * 2);
            this.currentWeight = Math.max(0, mockWeight);

            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            this.updateChart(this.currentWeight);
            this.updateGauge(this.currentWeight); // 模擬小球左右滑動
        }, 100);
    },

    // 嘗試自動連線 (監聽廣播)
    async attemptAutoConnect(device) {
        try {
            device.addEventListener('advertisementreceived', this.boundHandleAdvertisement);
            await device.watchAdvertisements();

            this.verifyTimeout = setTimeout(() => {
                device.removeEventListener('advertisementreceived', this.boundHandleAdvertisement);
                this.updateButtonUI('disconnected');
                this.showFallbackModal();
            }, this.VERIFY_TIMEOUT_MS);

        } catch (error) {
            console.error('自動連線嘗試失敗:', error);
            this.updateButtonUI('disconnected');
            this.showFallbackModal();
        }
    },

    // 喚起瀏覽器原生選擇視窗 (手動配對)
    async requestNewDevice() {
        this.hideFallbackModal();
        this.updateButtonUI('connecting');

        try {
            const device = await navigator.bluetooth.requestDevice({
                filters: [{
                    manufacturerData: [{ companyIdentifier: this.MANUFACTURER_ID }]
                }],
                optionalManufacturerData: [this.MANUFACTURER_ID]
            });

            if (!device.watchAdvertisements) {
                throw new Error('未開啟 Web Bluetooth 實驗性功能');
            }

            device.addEventListener('advertisementreceived', this.boundHandleAdvertisement);
            await device.watchAdvertisements();

            this.connectedDevice = device;
            this.onConnected();

        } catch (error) {
            console.error('手動配對失敗:', error);
            this.updateButtonUI('disconnected');
            throw error; // 將錯誤往上拋，讓 toggleConnection 接住並啟動模擬模式
        }
    },

    // 提取連線成功的共通邏輯
    onConnected() {
        this.updateButtonUI('connected');
        
        // 💡 確保強制移除隱藏狀態，並立刻讓資訊欄位出現
        const container = document.getElementById('ble-live-container');
        if (container) {
            container.classList.remove('hidden');
            container.style.display = 'flex'; // 強制給予排版屬性避免瀏覽器忽略
        }

        // 稍微延遲 50 毫秒初始化畫布，等 DOM 真的展開了再畫圖，避免畫布寬度為 0
        setTimeout(() => {
            this.initChart();
            this.currentWeight = 0;
            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = '0.00';
        }, 50);
    },

    // 處理廣播數據
    handleAdvertisement(event) {
        const data = event.manufacturerData.get(this.MANUFACTURER_ID);
        if (data && data.byteLength >= this.WEIGHT_OFFSET + 2) {
            this.resetAdvertisementTimeout();

            const rawWeight = (data.getUint8(this.WEIGHT_OFFSET) << 8) | data.getUint8(this.WEIGHT_OFFSET + 1);
            this.currentWeight = Math.max(0, rawWeight / 100);

            // 更新數值、圖表與小球
            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            this.updateChart(this.currentWeight);
            this.updateGauge(this.currentWeight); // 觸發左右移動
        }
    },

    // 斷線保護計時器
    resetAdvertisementTimeout() {
        if (this.advertisementTimeout) {
            clearTimeout(this.advertisementTimeout);
        }
        this.advertisementTimeout = setTimeout(() => {
            if (typeof showToast === 'function') showToast('測力計已離線 (超過 10 秒未收到數據)', 'error');
            this.disconnect(false);
        }, this.TIMEOUT_SECONDS * 1000);
    },

    // 主動斷線 / 清除狀態
    disconnect(isManual = false) {
        // ▼ 修改：清除模擬模式狀態 ▼
        if (this.mockInterval) {
            clearInterval(this.mockInterval);
            this.mockInterval = null;
            this.isMockMode = false;
        }

        if (this.advertisementTimeout) clearTimeout(this.advertisementTimeout);
        if (this.verifyTimeout) clearTimeout(this.verifyTimeout);

        if (this.connectedDevice && this.boundHandleAdvertisement) {
            this.connectedDevice.removeEventListener('advertisementreceived', this.boundHandleAdvertisement);
        }

        this.connectedDevice = null;
        this.updateButtonUI('disconnected');

        if (isManual) {
            const container = document.getElementById('ble-live-container');
            if (container) container.classList.add('hidden');
        }
    },


    // 控制 UI 狀態
    updateButtonUI(state) {
        const btn = document.getElementById('btn-ble-toggle');
        if (!btn) return;

        btn.classList.remove(
            'bg-white/10', 'text-white/50',
            'bg-yellow-500/20', 'text-yellow-400', 'animate-pulse',
            'bg-blue-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-blue-400'
        );

        if (state === 'disconnected') {
            btn.classList.add('bg-white/10', 'text-white/50');
        } else if (state === 'connecting') {
            btn.classList.add('bg-yellow-500/20', 'text-yellow-400', 'animate-pulse');
        } else if (state === 'connected') {
            btn.classList.add('bg-blue-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-blue-400');
        }
    },

    showFallbackModal() {
        const modal = document.getElementById('modal-ble-fallback');
        if (modal) modal.classList.remove('hidden');
    },

    hideFallbackModal() {
        const modal = document.getElementById('modal-ble-fallback');
        if (modal) modal.classList.add('hidden');
    }
};