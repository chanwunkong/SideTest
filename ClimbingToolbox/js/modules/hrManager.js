// --- js/modules/hrManager.js ---
import { settingsManager } from './ui.js';

export const hrManager = {
    HR_SERVICE: 'heart_rate',
    HR_MEASUREMENT_CHAR: 'heart_rate_measurement',

    connectedDevice: null,
    heartRateCharacteristic: null,
    currentHR: 0,

    isMockMode: false,
    mockInterval: null,
    boundHandleMeasurement: null,
    boundHandleDisconnect: null,

    init() {
        this.boundHandleMeasurement = this.handleHeartRateMeasurement.bind(this);
        this.boundHandleDisconnect = this.onDisconnected.bind(this);
    },

    async toggleConnection() {
        if (!this.boundHandleMeasurement) this.init();

        if (this.connectedDevice || this.isMockMode) {
            this.disconnect();
            return;
        }

        this.updateButtonUI('connecting');

        if (!navigator.bluetooth) {
            if (typeof showToast === 'function') showToast('無藍牙環境', 'info');
            this.startMockMode();
            return;
        }

        try {
            await this.requestAndConnectDevice();
        } catch (error) {
            console.error('心率連線失敗:', error);
            this.startMockMode();
        }
    },

    async requestAndConnectDevice() {
        this.isMockMode = false;

        // 最純粹的連線請求，移除不必要的 optionalServices 避免 Garmin 拒絕
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ services: [this.HR_SERVICE] }]
        });

        device.addEventListener('gattserverdisconnected', this.boundHandleDisconnect);

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(this.HR_SERVICE);
        const characteristic = await service.getCharacteristic(this.HR_MEASUREMENT_CHAR);

        // 💡 關鍵修正：必須先綁定事件，再開啟通知 (Garmin 的相容性要求)
        characteristic.addEventListener('characteristicvaluechanged', this.boundHandleMeasurement);
        await characteristic.startNotifications();

        this.connectedDevice = device;
        this.heartRateCharacteristic = characteristic;

        this.currentHR = 0;
        this.onConnected();
    },

    handleHeartRateMeasurement(event) {
        try {
            const value = event.target.value;
            const flags = value.getUint8(0);
            const rate16Bits = flags & 0x1;

            let newHR = 0;
            if (rate16Bits) {
                newHR = value.getUint16(1, true);
            } else {
                newHR = value.getUint8(1);
            }

            this.currentHR = newHR;

            // 💡 關鍵防禦：直接先強行寫入數字，避免被後續 UI 邏輯報錯卡住
            const hrValueEl = document.getElementById('hr-live-value');
            if (hrValueEl) {
                hrValueEl.innerText = this.currentHR;
                hrValueEl.classList.remove('text-yellow-400');
                hrValueEl.classList.add('text-white');
            }

            this.updateDisplay();

        } catch (error) {
            console.error('資料解析失敗:', error);
        }
    },

    startMockMode() {
        this.isMockMode = true;
        this.currentHR = 75;
        this.onConnected();

        this.mockInterval = setInterval(() => {
            const delta = Math.floor(Math.random() * 6) - 2;
            this.currentHR = Math.max(60, Math.min(180, this.currentHR + delta));
            this.updateDisplay();
        }, 1000);
    },

    onConnected() {
        this.updateButtonUI(this.isMockMode ? 'mock' : 'connected');

        const container = document.getElementById('hr-live-container');
        if (container) {
            container.classList.remove('hidden');
            container.style.display = 'flex';
            void container.offsetWidth;
        }

        this.updateDisplay();
    },

    disconnect() {
        if (this.mockInterval) {
            clearInterval(this.mockInterval);
            this.mockInterval = null;
            this.isMockMode = false;
        }

        if (this.heartRateCharacteristic) {
            this.heartRateCharacteristic.removeEventListener('characteristicvaluechanged', this.boundHandleMeasurement);
            // 加上 catch 避免裝置已經實體斷線，強制 stopNotifications 會拋錯
            try {
                this.heartRateCharacteristic.stopNotifications();
            } catch (e) { }
        }

        if (this.connectedDevice && this.connectedDevice.gatt.connected) {
            this.connectedDevice.gatt.disconnect();
        }

        this.heartRateCharacteristic = null;
        this.connectedDevice = null;
        this.currentHR = 0;

        this.updateButtonUI('disconnected');

        const container = document.getElementById('hr-live-container');
        if (container) {
            container.style.display = '';
            container.classList.add('hidden');
        }
    },

    onDisconnected() {
        this.disconnect();
    },

    getZoneInfo(hr) {
        // 💡 關鍵防禦：確保 settingsManager 沒準備好時也不會報錯
        let maxHR = 190;
        try {
            if (settingsManager && settingsManager.data && settingsManager.data.maxHR) {
                maxHR = settingsManager.data.maxHR;
            }
        } catch (e) { }

        const percentage = hr / maxHR;

        if (percentage >= 0.9) return { name: 'Zone 5 極限', color: 'red' };
        if (percentage >= 0.8) return { name: 'Zone 4 無氧', color: 'orange' };
        if (percentage >= 0.7) return { name: 'Zone 3 有氧', color: 'green' };
        if (percentage >= 0.6) return { name: 'Zone 2 燃脂', color: 'blue' };
        return { name: 'Zone 1 暖身', color: 'gray' };
    },

    updateDisplay() {
        const hrValueEl = document.getElementById('hr-live-value');
        const hrIconEl = document.getElementById('hr-live-icon');
        const hrBoxEl = document.getElementById('hr-live-box');
        const hrZoneEl = document.getElementById('hr-live-zone');

        if (hrValueEl) {
            let displayValue = this.currentHR > 0 ? this.currentHR : '--';
            hrValueEl.innerText = this.isMockMode ? `${displayValue} (模擬)` : displayValue;

            if (this.isMockMode) {
                hrValueEl.classList.add('text-yellow-400');
                hrValueEl.classList.remove('text-white');
            } else {
                hrValueEl.classList.add('text-white');
                hrValueEl.classList.remove('text-yellow-400');
            }
        }

        if (hrBoxEl && hrIconEl && hrZoneEl) {
            let c = 'gray';
            let zoneName = '偵測中...';

            if (this.isMockMode) {
                c = 'yellow';
                zoneName = '模擬數據';
            } else if (this.currentHR > 0) {
                const zone = this.getZoneInfo(this.currentHR);
                c = zone.color;
                zoneName = zone.name;
            }

            hrZoneEl.innerText = zoneName;
            hrBoxEl.className = `flex items-center gap-3 px-5 py-2 rounded-full shadow-lg border-2 transition-colors duration-300 bg-gray-900 border-${c}-500`;

            const allColors = ['red', 'orange', 'green', 'blue', 'gray', 'yellow'];
            allColors.forEach(color => hrIconEl.classList.remove(`text-${color}-500`));
            hrIconEl.classList.add(`text-${c}-500`);

            hrZoneEl.className = `text-[10px] font-bold uppercase tracking-wider text-center transition-colors duration-300 text-${c}-400`;
        }

        if (hrIconEl && this.currentHR > 0) {
            hrIconEl.classList.remove('pulse-animation');
            void hrIconEl.offsetWidth;
            hrIconEl.classList.add('pulse-animation');
        } else if (hrIconEl && this.currentHR === 0) {
            hrIconEl.classList.remove('pulse-animation');
        }
    },

    updateButtonUI(state) {
        const btn = document.getElementById('btn-hr-toggle');
        if (!btn) return;

        btn.classList.remove(
            'bg-white/10', 'text-white/50',
            'bg-red-500/20', 'text-red-400', 'animate-pulse',
            'bg-red-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-red-400',
            'bg-yellow-600', 'ring-yellow-400'
        );

        if (state === 'disconnected') {
            btn.classList.add('bg-white/10', 'text-white/50');
        } else if (state === 'connecting') {
            btn.classList.add('bg-red-500/20', 'text-red-400', 'animate-pulse');
        } else if (state === 'connected') {
            btn.classList.add('bg-red-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-red-400');
        } else if (state === 'mock') {
            btn.classList.add('bg-yellow-600', 'text-white', 'shadow-lg', 'ring-2', 'ring-yellow-400');
        }
    }
};