// --- js/modules/hrManager.js ---
import { settingsManager } from './ui.js';

export const hrManager = {
    // 藍牙 GATT 標準 UUID
    HR_SERVICE: 'heart_rate',
    HR_MEASUREMENT_CHAR: 'heart_rate_measurement',

    connectedDevice: null,
    heartRateCharacteristic: null,
    currentHR: 0,

    // 模擬模式變數
    isMockMode: false,
    mockInterval: null,

    // 綁定的事件處理函數，便於後續移除
    boundHandleMeasurement: null,
    boundHandleDisconnect: null,

    init() {
        this.boundHandleMeasurement = this.handleHeartRateMeasurement.bind(this);
        this.boundHandleDisconnect = this.onDisconnected.bind(this);
    },

    // 切換連線狀態 (按鈕的主要進入點)
    async toggleConnection() {
        if (!this.boundHandleMeasurement) this.init();

        if (this.connectedDevice || this.isMockMode) {
            this.disconnect();
            return;
        }

        this.updateButtonUI('connecting');

        // 無藍牙環境，直接進入模擬模式
        if (!navigator.bluetooth) {
            if (typeof showToast === 'function') showToast('無藍牙環境，啟動心率模擬模式', 'info');
            this.startMockMode();
            return;
        }

        try {
            await this.requestAndConnectDevice();
        } catch (error) {
            console.error('心率裝置連線錯誤:', error);
            if (typeof showToast === 'function') showToast('心率連線取消或失敗，啟動模擬模式', 'info');
            this.startMockMode();
        }
    },

    // 請求藍牙裝置並建立連線
    async requestAndConnectDevice() {
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ services: [this.HR_SERVICE] }],
            optionalServices: ['generic_access']
        });

        device.addEventListener('gattserverdisconnected', this.boundHandleDisconnect);

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(this.HR_SERVICE);
        const characteristic = await service.getCharacteristic(this.HR_MEASUREMENT_CHAR);

        await characteristic.startNotifications();
        characteristic.addEventListener('characteristicvaluechanged', this.boundHandleMeasurement);

        this.connectedDevice = device;
        this.heartRateCharacteristic = characteristic;

        this.onConnected();
    },

    // 處理 GATT 心率封包
    handleHeartRateMeasurement(event) {
        const value = event.target.value;
        const flags = value.getUint8(0);
        const rate16Bits = flags & 0x1;

        if (rate16Bits) {
            this.currentHR = value.getUint16(1, true); // Little-endian
        } else {
            this.currentHR = value.getUint8(1);
        }

        this.updateDisplay();
    },

    // 啟動模擬模式
    startMockMode() {
        this.isMockMode = true;
        this.currentHR = 75; // 基礎心率
        this.onConnected();

        this.mockInterval = setInterval(() => {
            // 模擬心率波動 (-2 到 +3)
            const delta = Math.floor(Math.random() * 6) - 2;
            this.currentHR = Math.max(60, Math.min(180, this.currentHR + delta));
            this.updateDisplay();
        }, 1000);
    },

    // 連線成功共通邏輯
    onConnected() {
        this.updateButtonUI('connected');
        if (typeof showToast === 'function' && !this.isMockMode) {
            showToast('已連接心率裝置', 'info');
        }

        const container = document.getElementById('hr-live-container');
        if (container) container.classList.remove('hidden');

        this.updateDisplay();
    },

    // 斷線處理
    disconnect() {
        if (this.mockInterval) {
            clearInterval(this.mockInterval);
            this.mockInterval = null;
            this.isMockMode = false;
        }

        if (this.heartRateCharacteristic) {
            this.heartRateCharacteristic.removeEventListener('characteristicvaluechanged', this.boundHandleMeasurement);
            this.heartRateCharacteristic.stopNotifications().catch(err => console.error("停止通知失敗", err));
        }

        if (this.connectedDevice && this.connectedDevice.gatt.connected) {
            this.connectedDevice.gatt.disconnect();
        }

        this.heartRateCharacteristic = null;
        this.connectedDevice = null;
        this.currentHR = 0;

        this.updateButtonUI('disconnected');

        const container = document.getElementById('hr-live-container');
        if (container) container.classList.add('hidden');
    },

    // 處理裝置意外斷線
    onDisconnected() {
        if (typeof showToast === 'function') showToast('心率裝置已斷線', 'error');
        this.disconnect();
    },

    // 判斷心率區間與對應顏色
    getZoneInfo(hr) {
        // 從設定取得最大心率，若無則預設 190
        const maxHR = settingsManager.data.maxHR || 190;
        const percentage = hr / maxHR;

        if (percentage >= 0.9) return { name: 'Zone 5 極限', color: 'red' };
        if (percentage >= 0.8) return { name: 'Zone 4 無氧', color: 'orange' };
        if (percentage >= 0.7) return { name: 'Zone 3 有氧', color: 'green' };
        if (percentage >= 0.6) return { name: 'Zone 2 燃脂', color: 'blue' };
        return { name: 'Zone 1 暖身', color: 'gray' };
    },

    // 更新數值顯示、觸發跳動動畫與變更區間顏色
    updateDisplay() {
        const hrValueEl = document.getElementById('hr-live-value');
        const hrIconEl = document.getElementById('hr-live-icon');
        const hrBoxEl = document.getElementById('hr-live-box');
        const hrZoneEl = document.getElementById('hr-live-zone');

        if (hrValueEl) {
            hrValueEl.innerText = this.currentHR;
        }

        if (hrBoxEl && hrIconEl && hrZoneEl && this.currentHR > 0) {
            const zone = this.getZoneInfo(this.currentHR);
            const c = zone.color;

            hrZoneEl.innerText = zone.name;

            // 改用不透明深色底，避免被計時器背景色污染
            hrBoxEl.className = `flex items-center gap-3 px-5 py-2 rounded-full shadow-lg border-2 transition-colors duration-300 bg-gray-900 border-${c}-500`;

            // 修復 TypeError: 使用 setAttribute 覆寫 SVG 的 class
            hrIconEl.setAttribute('class', `w-6 h-6 transition-colors duration-300 text-${c}-500`);

            hrZoneEl.className = `text-[10px] font-bold uppercase tracking-wider text-center transition-colors duration-300 text-${c}-400`;
        }

        if (hrIconEl) {
            hrIconEl.classList.remove('pulse-animation');
            void hrIconEl.offsetWidth;
            hrIconEl.classList.add('pulse-animation');
        }
    },

    // 控制按鈕 UI 狀態
    updateButtonUI(state) {
        const btn = document.getElementById('btn-hr-toggle');
        if (!btn) return;

        btn.classList.remove(
            'bg-white/10', 'text-white/50',
            'bg-red-500/20', 'text-red-400', 'animate-pulse',
            'bg-red-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-red-400'
        );

        if (state === 'disconnected') {
            btn.classList.add('bg-white/10', 'text-white/50');
        } else if (state === 'connecting') {
            btn.classList.add('bg-red-500/20', 'text-red-400', 'animate-pulse');
        } else if (state === 'connected') {
            btn.classList.add('bg-red-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-red-400');
        }
    }
};