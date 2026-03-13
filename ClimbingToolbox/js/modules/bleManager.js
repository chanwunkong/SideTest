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
        this.onConnected(); // 觸發連線成功的 UI 更新

        this.mockInterval = setInterval(() => {
            // 利用時間產生正弦波，加上一點隨機數，模擬出力抖動
            const time = Date.now() / 1000;
            let mockWeight = 20 + 15 * Math.sin(time * 2) + (Math.random() * 3 - 1.5);
            mockWeight = Math.max(0, mockWeight); // 防止負數

            this.currentWeight = mockWeight;

            // 更新 UI
            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            // 更新圖表
            this.updateChart(this.currentWeight);
        }, 100); // 模擬 10Hz 更新頻率
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
            if (this.verifyTimeout) {
                clearTimeout(this.verifyTimeout);
                this.verifyTimeout = null;
                this.connectedDevice = event.target;
                this.onConnected();
            }

            this.resetAdvertisementTimeout();

            const rawWeight = (data.getUint8(this.WEIGHT_OFFSET) << 8) | data.getUint8(this.WEIGHT_OFFSET + 1);
            let currentMass = rawWeight / 100;
            this.currentWeight = Math.max(-1000, currentMass);

            const weightEl = document.getElementById('ble-live-weight');
            if (weightEl) weightEl.innerText = this.currentWeight.toFixed(2);

            this.updateChart(this.currentWeight);
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