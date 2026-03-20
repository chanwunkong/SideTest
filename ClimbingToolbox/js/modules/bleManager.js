// --- js/modules/bleManager.js ---
export const bleManager = {
    MANUFACTURER_ID: 256,
    WEIGHT_OFFSET: 10,
    TIMEOUT_SECONDS: 10,
    VERIFY_TIMEOUT_MS: 3000,

    connectedDevice: null,
    advertisementTimeout: null,
    verifyTimeout: null,
    currentWeight: 0,

    MAX_DATA_POINTS: 100,
    chartData: [],
    chartCtx: null,
    boundHandleAdvertisement: null,
    manualTargetWeight: null,

    isMockMode: false,
    mockInterval: null,

    initChart() {
        const canvas = document.getElementById('ble-live-chart');
        if (canvas && !this.chartCtx) {
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

        const maxWeight = Math.max(...this.chartData, 10);
        const stepX = canvas.width / (this.MAX_DATA_POINTS - 1);

        const peakWeight = Math.max(...this.chartData);
        if (peakWeight > 0) {
            const peakY = canvas.height - ((peakWeight / maxWeight) * canvas.height * 0.9);

            ctx.save();
            ctx.beginPath();
            ctx.setLineDash([5, 5]);
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.4)';
            ctx.lineWidth = 1;
            ctx.moveTo(0, peakY);
            ctx.lineTo(canvas.width, peakY);
            ctx.stroke();

            ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
            ctx.font = '10px tabular-nums font-bold';
            ctx.textAlign = 'right';
            ctx.fillText(`${peakWeight.toFixed(1)} Peak`, canvas.width - 5, peakY - 5);
            ctx.restore();
        }

        ctx.beginPath();
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.lineWidth = 4;
        ctx.lineJoin = 'round';

        this.chartData.forEach((w, index) => {
            const x = index * stepX;
            const y = canvas.height - ((w / maxWeight) * canvas.height * 0.9);
            if (index === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    },

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

        let targetWeight = 20;
        const currentBlock = this.getCurrentBlock();

        if (this.manualTargetWeight !== null) {
            targetWeight = this.manualTargetWeight;
        } else if (currentBlock) {
            targetWeight = timer.getBestValue(currentBlock.props.label, '重量') || 20;
        }

        if (targetWeight <= 0) targetWeight = 1;

        if (displayEl) displayEl.textContent = targetWeight;

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

    applyHistoryTarget(val) {
        const input = document.getElementById('ble-target-input');
        if (input) input.value = val;
    },

    openTargetModal() {
        const modal = document.getElementById('modal-ble-target');
        const sheet = document.getElementById('ble-target-sheet');
        const input = document.getElementById('ble-target-input');
        const blockNameEl = document.getElementById('ble-target-block-name');

        const historyContainer = document.getElementById('ble-target-history-container');
        const historyList = document.getElementById('ble-target-history-list');

        let currentTarget = 20;
        let blockName = "未知動作";
        const currentBlock = this.getCurrentBlock();

        if (this.manualTargetWeight !== null) {
            currentTarget = this.manualTargetWeight;
            blockName = currentBlock && currentBlock.props ? currentBlock.props.label : "自訂目標";
        } else if (currentBlock && currentBlock.props) {
            currentTarget = timer.getBestValue(currentBlock.props.label, '重量') || 20;
            blockName = currentBlock.props.label;
        }

        if (blockNameEl) blockNameEl.textContent = `目前動作: ${blockName}`;
        if (input) input.value = currentTarget;

        if (historyContainer && historyList) {
            historyList.innerHTML = '';
            let hasHistory = false;
            let availableHistory = {};

            if (typeof timer !== 'undefined' && timer.currentRoutineTitle) {
                if (typeof recordManager !== 'undefined') {
                    const records = recordManager.getAllRecords();
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

            Object.entries(availableHistory).forEach(([displayLabel, val]) => {
                hasHistory = true;
                const btn = document.createElement('button');
                btn.type = 'button';
                btn.className = 'px-3 py-2 bg-gray-700 hover:bg-gray-600 text-white text-xs font-bold rounded-lg border border-gray-600 transition-colors flex items-center gap-1.5 active:scale-95 whitespace-nowrap';

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
            this.updateGauge(this.currentWeight || 0);
        }
        this.closeTargetModal();
    },

    async toggleConnection() {
        if (this.connectedDevice || this.isMockMode) {
            this.disconnect(true);
            return;
        }

        if (!this.boundHandleAdvertisement) {
            this.boundHandleAdvertisement = this.handleAdvertisement.bind(this);
        }

        this.updateButtonUI('connecting');

        if (!navigator.bluetooth) {
            this.startMockMode();
            return;
        }

        try {
            await this.requestNewDevice();
        } catch (error) {
            console.error('藍牙配對錯誤:', error);
            this.startMockMode();
        }
    },

    startMockMode() {
        this.isMockMode = true;
        this.onConnected();

        this.mockInterval = setInterval(() => {
            const time = Date.now() / 1000;
            let mockWeight = 20 + 15 * Math.sin(time * 1.5) + (Math.random() * 2);
            this.currentWeight = Math.max(0, mockWeight);

            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            this.updateChart(this.currentWeight);
            this.updateGauge(this.currentWeight);
        }, 100);
    },

    async requestNewDevice() {
        this.hideFallbackModal();
        this.updateButtonUI('connecting');

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
    },

    onConnected() {
        this.updateButtonUI('connected');

        const container = document.getElementById('ble-live-container');
        if (container) {
            container.classList.remove('hidden');
            container.style.display = 'flex';
            void container.offsetWidth;
        }

        setTimeout(() => {
            this.initChart();
            this.currentWeight = 0;
            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = '0.00';
        }, 50);
    },

    handleAdvertisement(event) {
        const data = event.manufacturerData.get(this.MANUFACTURER_ID);
        if (data && data.byteLength >= this.WEIGHT_OFFSET + 2) {
            this.resetAdvertisementTimeout();

            const rawWeight = (data.getUint8(this.WEIGHT_OFFSET) << 8) | data.getUint8(this.WEIGHT_OFFSET + 1);
            this.currentWeight = Math.max(0, rawWeight / 100);

            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            this.updateChart(this.currentWeight);
            this.updateGauge(this.currentWeight);
        }
    },

    resetAdvertisementTimeout() {
        if (this.advertisementTimeout) {
            clearTimeout(this.advertisementTimeout);
        }
        this.advertisementTimeout = setTimeout(() => {
            this.disconnect(false);
        }, this.TIMEOUT_SECONDS * 1000);
    },

    disconnect(isManual = false) {
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

        const container = document.getElementById('ble-live-container');
        if (container) {
            container.style.display = '';
            container.classList.add('hidden');
        }
    },

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