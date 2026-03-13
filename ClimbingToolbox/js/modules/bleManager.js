// --- js/modules/bleManager.js ---

const bleManager = {
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

    // 更新指標位置
    updateGauge(weight) {
        const pointer = document.getElementById('ble-gauge-pointer');
        if (!pointer) return;

        // 1. 決定目標重量 (Target)
        // 優先抓取計時器目前的動作目標，若無則預設 20kg 方便測試
        let targetWeight = 20;
        if (typeof timer !== 'undefined' && timer.pendingLogBlock) {
            targetWeight = timer.getBestValue(timer.pendingLogBlock.props.label, '重量') || 20;
        }

        // 2. 計算百分比位置 (Mapping)
        // 0% - 40% 區間：太輕 (0kg ~ 目標的 80%)
        // 40% - 60% 區間：目標 (目標的 80% ~ 120%)
        // 60% - 100% 區間：太重 (目標的 120% ~ 2倍目標)
        let percentage = 0;
        const lowBound = targetWeight * 0.8;
        const highBound = targetWeight * 1.2;

        if (weight <= lowBound) {
            // 在「太輕」區域線性映射
            percentage = (weight / lowBound) * 40;
        } else if (weight <= highBound) {
            // 在「目標」區域線性映射
            percentage = 40 + ((weight - lowBound) / (highBound - lowBound)) * 20;
        } else {
            // 在「太重」區域線性映射 (最大顯示到 2 倍目標)
            const overWeight = weight - highBound;
            percentage = 60 + (overWeight / (targetWeight * 0.8)) * 40;
        }

        // 邊界保護：確保小球不會跑出軌道 (預留 1%-99%)
        percentage = Math.min(Math.max(percentage, 1), 99);

        // 3. 執行視覺更新
        pointer.style.left = `${percentage}%`;

        // 4. 動態顏色回饋 (隨區域變色)
        if (percentage < 40) {
            pointer.style.backgroundColor = '#facc15'; // 黃色
            pointer.style.boxShadow = '0 0 15px rgba(250, 204, 21, 0.8)';
            pointer.classList.remove('animate-pulse');
        } else if (percentage <= 60) {
            pointer.style.backgroundColor = '#4ade80'; // 綠色 (達標)
            pointer.style.boxShadow = '0 0 20px rgba(74, 222, 128, 0.9)';
            pointer.classList.add('animate-pulse'); // 達標時發光脈動
        } else {
            pointer.style.backgroundColor = '#f87171'; // 紅色
            pointer.style.boxShadow = '0 0 15px rgba(248, 113, 113, 0.8)';
            pointer.classList.remove('animate-pulse');
        }
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

        // ▼ 修改：若無藍牙功能，直接進入模擬模式 ▼
        if (!navigator.bluetooth) {
            if (typeof showToast === 'function') showToast('無藍牙環境，啟動模擬測試模式', 'info');
            this.startMockMode();
            return;
        }

        try {
            const devices = await navigator.bluetooth.getDevices();

            if (devices && devices.length > 0) {
                const targetDevice = devices[0];
                this.attemptAutoConnect(targetDevice);
            } else {
                await this.requestNewDevice();
            }
        } catch (error) {
            console.error('藍牙初始化錯誤:', error);
            // ▼ 修改：即使是取消配對或報錯，也切換至模擬模式方便測試 ▼
            if (typeof showToast === 'function') showToast('藍牙連線異常，啟動模擬測試模式', 'info');
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
        if (typeof showToast === 'function' && !this.isMockMode) {
            showToast('已連接測力計', 'info');
        }

        if (!this.isMockMode) {
            this.resetAdvertisementTimeout();
        }

        const container = document.getElementById('ble-live-container');
        if (container) container.classList.remove('hidden');
        this.initChart();
        this.currentWeight = 0;
        const weightEl = document.getElementById('ble-live-weight');
        if (weightEl) weightEl.innerText = '0.00';
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