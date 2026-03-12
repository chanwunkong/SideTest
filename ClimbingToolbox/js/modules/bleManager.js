// --- js/modules/bleManager.js ---

const bleManager = {
    MANUFACTURER_ID: 256,
    WEIGHT_OFFSET: 10,
    TIMEOUT_SECONDS: 4,        // 【修改這裡】從 10 秒縮短為 4 秒
    VERIFY_TIMEOUT_MS: 3000,   // 自動連線驗證時間 (3秒)

    connectedDevice: null,
    advertisementTimeout: null,
    verifyTimeout: null,
    currentWeight: 0,          // 暫存最新重量數據

    // 切換連線狀態 (按鈕的主要進入點)
    async toggleConnection() {
        if (!navigator.bluetooth) {
            if (typeof showToast === 'function') showToast('瀏覽器不支援 Web Bluetooth', 'error');
            return;
        }

        // 如果已經連線，則執行斷線
        if (this.connectedDevice) {
            this.disconnect();
            return;
        }

        this.updateButtonUI('connecting');

        try {
            // 1. 取得歷史授權設備清單
            const devices = await navigator.bluetooth.getDevices();

            if (devices && devices.length > 0) {
                // 情境 2：有歷史紀錄，嘗試靜默自動連線
                const targetDevice = devices[0]; // 取最近授權的設備
                this.attemptAutoConnect(targetDevice);
            } else {
                // 情境 1：初次使用無紀錄，直接彈出原生選擇視窗
                // 注意：因為沒有經過 await 等待太久，使用者的「點擊」權限還在，可以直接喚起視窗
                await this.requestNewDevice();
            }
        } catch (error) {
            console.error('藍牙初始化錯誤:', error);
            this.updateButtonUI('disconnected');
        }
    },

    // 嘗試自動連線 (監聽廣播)
    async attemptAutoConnect(device) {
        try {
            // 綁定事件並開啟監聽
            device.addEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
            await device.watchAdvertisements();

            // 啟動 3 秒驗證計時器
            this.verifyTimeout = setTimeout(() => {
                // 若 3 秒內未收到廣播，判定設備未開機或不在範圍內
                device.removeEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
                this.updateButtonUI('disconnected');
                this.showFallbackModal(); // 彈出備用提示視窗
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
                throw new Error('請開啟 chrome://flags/#enable-experimental-web-platform-features');
            }

            device.addEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
            await device.watchAdvertisements();

            // 手動選擇成功後，視為直接連線，設定 10 秒斷線保護
            this.connectedDevice = device;
            this.updateButtonUI('connected');
            if (typeof showToast === 'function') showToast(`已連接測力計`, 'info');
            this.resetAdvertisementTimeout();

        } catch (error) {
            console.error('手動配對失敗:', error);
            this.updateButtonUI('disconnected');
            if (error.name !== 'NotFoundError') {
                if (typeof showToast === 'function') showToast(`配對失敗: ${error.message}`, 'error');
            }
        }
    },

    // 處理廣播數據
    handleAdvertisement(event) {
        const data = event.manufacturerData.get(this.MANUFACTURER_ID);

        if (data && data.byteLength >= this.WEIGHT_OFFSET + 2) {
            // 【驗證成功機制】：如果有 verifyTimeout，代表我們正在等第一筆資料
            if (this.verifyTimeout) {
                clearTimeout(this.verifyTimeout);
                this.verifyTimeout = null;
                this.connectedDevice = event.target; // 確立連線
                this.updateButtonUI('connected');
                if (typeof showToast === 'function') showToast(`已自動連接測力計`, 'info');
            }

            // 重置斷線計時器
            this.resetAdvertisementTimeout();

            // 解析重量
            const rawWeight = (data.getUint8(this.WEIGHT_OFFSET) << 8) | data.getUint8(this.WEIGHT_OFFSET + 1);
            let currentMass = rawWeight / 100;
            this.currentWeight = Math.max(-1000, currentMass);

            // TODO: 後續可將 this.currentWeight 整合進 timer.js 的 UI 中顯示
        }
    },

    // 斷線保護計時器
    resetAdvertisementTimeout() {
        if (this.advertisementTimeout) {
            clearTimeout(this.advertisementTimeout);
        }
        this.advertisementTimeout = setTimeout(() => {
            if (typeof showToast === 'function') showToast('測力計已離線 (超過 10 秒未收到數據)', 'error');
            this.disconnect();
        }, this.TIMEOUT_SECONDS * 1000);
    },

    // 主動斷線 / 清除狀態
    disconnect() {
        if (this.advertisementTimeout) clearTimeout(this.advertisementTimeout);
        if (this.verifyTimeout) clearTimeout(this.verifyTimeout);

        if (this.connectedDevice) {
            // 由於沒有 GATT 連線，我們只需要移除監聽器即可
            this.connectedDevice.removeEventListener('advertisementreceived', this.handleAdvertisement.bind(this));
        }

        this.connectedDevice = null;
        this.currentWeight = 0;
        this.updateButtonUI('disconnected');
    },

    // 控制 UI 狀態
    // 控制 UI 狀態
    updateButtonUI(state) {
        const btn = document.getElementById('btn-ble-toggle');
        if (!btn) return;

        // 清除所有狀態 class (改為標準 Tailwind 類別)
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
            // 改用標準類別，避免主執行緒卡頓導致變色延遲
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