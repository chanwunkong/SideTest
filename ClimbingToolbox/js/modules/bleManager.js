// --- js/modules/bleManager.js ---

const bleManager = {
    MANUFACTURER_ID: 256,
    WEIGHT_OFFSET: 10,
    TIMEOUT_SECONDS: 30,
    VERIFY_TIMEOUT_MS: 3000,

    connectedDevice: null,
    advertisementTimeout: null,
    verifyTimeout: null,
    currentWeight: 0,

    chartData: [],
    MAX_DATA_POINTS: 100,
    ctx: null,
    canvas: null,

    // 切換連線狀態 (按鈕的主要進入點)
    async toggleConnection() {
        if (!navigator.bluetooth) {
            if (typeof showToast === 'function') showToast('瀏覽器不支援 Web Bluetooth', 'error');
            return;
        }

        if (this.connectedDevice) {
            this.disconnect();
            return;
        }

        try {
            // 【解決問題 1】：強化環境檢查。如果 getDevices 不存在，直接顯示環境要求提示。
            if (!navigator.bluetooth.getDevices) {
                this.showSetupModal();
                return;
            }

            const devices = await navigator.bluetooth.getDevices();

            if (devices && devices.length > 0) {
                this.updateButtonUI('connecting');
                this.attemptAutoConnect(devices[0]);
            } else {
                // 初次連線或無授權紀錄：顯示環境提示視窗
                this.showSetupModal();
            }
        } catch (error) {
            console.error('藍牙初始化錯誤:', error);
            this.showSetupModal(); // 出錯時彈出指引視窗
            this.updateButtonUI('disconnected');
        }
    },

    // 嘗試自動連線 (監聽廣播)
    async attemptAutoConnect(device) {
        try {
            device.addEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
            await device.watchAdvertisements();

            this.verifyTimeout = setTimeout(() => {
                device.removeEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
                this.updateButtonUI('disconnected');
                this.showFallbackModal();
            }, this.VERIFY_TIMEOUT_MS);
        } catch (error) {
            this.updateButtonUI('disconnected');
            this.showFallbackModal();
        }
    },

    // 喚起瀏覽器原生選擇視窗 (手動配對)
    async requestNewDevice() {
        this.hideSetupModal();
        this.hideFallbackModal();
        this.updateButtonUI('connecting');

        try {
            const device = await navigator.bluetooth.requestDevice({
                filters: [{ manufacturerData: [{ companyIdentifier: this.MANUFACTURER_ID }] }],
                optionalManufacturerData: [this.MANUFACTURER_ID]
            });

            if (!device.watchAdvertisements) {
                this.updateButtonUI('disconnected');
                this.showSetupModal();
                return;
            }

            device.addEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
            await device.watchAdvertisements();

            this.onConnected(device);

        } catch (error) {
            this.updateButtonUI('disconnected');
            if (error.name !== 'NotFoundError') console.error(error);
        }
    },

    // 統一處理連線成功後的初始化
    onConnected(device) {
        if (this.verifyTimeout) {
            clearTimeout(this.verifyTimeout);
            this.verifyTimeout = null;
        }

        this.connectedDevice = device;
        this.updateButtonUI('connected');
        this.initChart();
        this.resetAdvertisementTimeout();
    },

    // 處理廣播數據
    handleAdvertisement(event) {
        const data = event.manufacturerData.get(this.MANUFACTURER_ID);
        if (data && data.byteLength >= this.WEIGHT_OFFSET + 2) {
            if (this.verifyTimeout) {
                this.onConnected(event.target);
            }

            this.resetAdvertisementTimeout();
            const rawWeight = (data.getUint8(this.WEIGHT_OFFSET) << 8) | data.getUint8(this.WEIGHT_OFFSET + 1);
            let currentMass = rawWeight / 100;
            this.currentWeight = Math.max(-1000, currentMass);
            this.updateChart(this.currentWeight);
        }
    },

    // --- 圖表邏輯 ---
    initChart() {
        this.canvas = document.getElementById('active-live-chart');
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.canvas.width = this.canvas.offsetWidth;
        this.canvas.height = 150;
        this.chartData = [];
    },

    updateChart(weight) {
        this.chartData.push(weight);
        if (this.chartData.length > this.MAX_DATA_POINTS) this.chartData.shift();
        this.drawChart();
    },

    drawChart() {
        if (!this.ctx || !this.canvas || this.chartData.length < 2) return;
        const { ctx, canvas } = this;
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        const maxWeight = Math.max(...this.chartData, 10);
        const stepX = canvas.width / (this.MAX_DATA_POINTS - 1);

        ctx.beginPath();
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.lineWidth = 3;
        ctx.lineJoin = 'round';

        this.chartData.forEach((w, i) => {
            const x = i * stepX;
            const y = canvas.height - ((w / maxWeight) * canvas.height * 0.85) - 5;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    },

    resetAdvertisementTimeout() {
        if (this.advertisementTimeout) clearTimeout(this.advertisementTimeout);
        this.advertisementTimeout = setTimeout(() => {
            this.disconnect();
        }, this.TIMEOUT_SECONDS * 1000);
    },

    disconnect() {
        if (this.advertisementTimeout) clearTimeout(this.advertisementTimeout);
        if (this.verifyTimeout) clearTimeout(this.verifyTimeout);

        if (this.connectedDevice) {
            this.connectedDevice.removeEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
        }

        this.connectedDevice = null;
        this.currentWeight = 0;
        this.updateButtonUI('disconnected');
    },

    // 【解決問題 2】：控制 UI 狀態。不再使用 classList.replace，改用 add/remove 以提高穩定性。
    updateButtonUI(state) {
        const btn = document.getElementById('btn-ble-toggle');
        const countdown = document.getElementById('active-countdown');
        const chartCanvas = document.getElementById('active-live-chart');
        const statusText = document.getElementById('active-status');

        if (!btn) return;

        btn.classList.remove(
            'bg-white/10', 'text-white/50',
            'bg-yellow-500/20', 'text-yellow-400', 'animate-pulse',
            'bg-blue-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-blue-400'
        );

        if (state === 'disconnected') {
            btn.classList.add('bg-white/10', 'text-white/50');
            // 強制恢復計時器大小，移除所有縮放相關類別
            if (countdown) {
                countdown.classList.remove('text-[5rem]', 'md:text-[6rem]');
                countdown.classList.add('text-[8rem]', 'md:text-[10rem]');
            }
            if (chartCanvas) {
                chartCanvas.style.height = '0';
                chartCanvas.classList.add('opacity-0');
            }
            if (statusText) statusText.classList.remove('scale-75', '-translate-y-4');
        }
        else if (state === 'connecting') {
            btn.classList.add('bg-yellow-500/20', 'text-yellow-400', 'animate-pulse');
        }
        else if (state === 'connected') {
            btn.classList.add('bg-blue-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-blue-400');
            // 套用縮放效果：縮小倒數數字並顯示 Live Chart
            if (countdown) {
                countdown.classList.remove('text-[8rem]', 'md:text-[10rem]');
                countdown.classList.add('text-[5rem]', 'md:text-[6rem]');
            }
            if (chartCanvas) {
                chartCanvas.style.height = '150px';
                chartCanvas.classList.remove('opacity-0');
            }
            if (statusText) statusText.classList.add('scale-75', '-translate-y-4');
        }
    },

    showSetupModal() { document.getElementById('modal-ble-setup').classList.remove('hidden'); },
    hideSetupModal() { document.getElementById('modal-ble-setup').classList.add('hidden'); },
    showFallbackModal() { document.getElementById('modal-ble-fallback').classList.remove('hidden'); },
    hideFallbackModal() { document.getElementById('modal-ble-fallback').classList.add('hidden'); }
};