// --- js/modules/i18n.js ---
const i18n = {
    _current: (function () {
        try {
            const saved = localStorage.getItem('userLang');
            if (saved) return saved;
            return navigator.language.toLowerCase().startsWith('zh') ? 'zh' : 'en';
        } catch (e) {
            return 'en';
        }
    })(),

    dict: {
        zh: {
            // --- Index / Global ---
            heroTitle: "為攀岩者打造的數位工具箱",
            heroDesc: "AI 輔助開發：設計給所有熱愛攀爬的人",
            coreTitle: "核心工具",
            badgeLive: "版本 1.1 已上線",
            hbTitle: "指力訓練計時 (Hangboard v1.2)",
            hbPain: "解決傳統工具設定繁瑣、訓練時需頻繁分心記錄，以及數據零散難以追蹤進度的痛點。",
            hbF1: "<b>積木式架構</b>：像玩積木般自由組合「計時」與「次數」模組，輕鬆建構複雜的週期化課表。",
            hbF2: "<b>串接藍牙裝置</b>：支援即時顯示 WH-C06 測力計數據與 Garmin 手錶心率廣播，隨時掌握訓練強度。",
            hbF3: "<b>完整的成長軌跡</b>：自定義指標並整合 PR 卡片與趨勢圖，讓長期訓練數據一目了然。",
            hbFuture: "未來規劃：將藍牙裝置數據寫入訓練紀錄，提供更深度的圖表與進階數據分析。", hfTitle: "路線編輯器 (HoldFocus v1)",
            hfPain: "解決岩館路線更新頻繁難以留存細節，以及向他人解釋動作時溝通成本高的問題。",
            hfF1: "手動標記岩點，快速產出路線示意圖",
            hfF2: "支援圖片導出與即時紀錄分享",
            hfFuture: "未來規劃：考慮加入智慧岩點辨識與個人雲端相簿。",
            changelogTitle: "更新紀錄",
            logHbV1_2: "Hangboard v1.2 上線",
            logHbDesc1_2: "串接即時 WH-C06 測力計與 Garmin 手錶心率廣播（目前僅供即時顯示，尚未納入紀錄）。",
            logHbV1_1: "Hangboard v1.1 上線",
            logHbDesc1_1: "強化訓練紀錄機制，新增自定義指標掛載與日誌自動彙整功能。",
            logHbV1: "Hangboard v1 上線",
            logHbDesc: "開放無限制結構化編輯器、聲控系統優化、訓練紀錄日曆功能正式上線。",
            logHfV1: "HoldFocus v1 上線",
            logHfDesc: "手動標記功能發布，支援自訂路線繪製與高清圖片導出。",
            queueTitle: "開發中規劃",
            q1Title: "動作追蹤與姿勢分析",
            q1Desc: "分析攀爬軌跡，偵測重心偏移與動作準確度。",
            q2Title: "岩鞋規格對比",
            q2Desc: "收錄各品牌數據，根據腳型與風格提供選購建議。",
            contactUs: "聯絡我們",
            footerCopy: "© 2026 Climbing Toolbox. 設計給所有熱愛攀爬的人。",
            login: "登入",
            logout: "登出",
            langText: "English",

            // --- HoldFocus UI ---
            open: '開啟',
            download: '下載',
            tabRoute: '路線編輯',
            tabText: '文字/標記',
            instruction: '1. 右上角開啟照片<br>2. 點擊下方 <b class="inline-flex items-center justify-center w-4 h-4 bg-blue-600 text-white rounded-full text-[10px] align-middle shadow-sm">+</b> 號，開始圈選岩點',
            selectedHolds: '已選岩點',
            emptySelection: '尚未圈選',
            eraserSize: '橡皮擦大小',
            imageAdjustment: '影像調整 (全域)',
            contrast: '對比度',
            bgStyle: '背景風格',
            saturation: '飽滿度',
            dim: '壓暗',
            blur: '模糊',
            tags: '標記 (Tags)',
            text: '文字 (Text)',
            textPlaceholder: '輸入文字...',
            textColor: '文字顏色',
            textBgColor: '文字底色',
            size: '大小',
            rotation: '旋轉',
            deleteObj: '刪除物件',
            processing: '處理中...',
            openPhotoPrompt: '請開啟照片',
            holdNum: '岩點 #',
            tolerance: '容許度',
            expand: '擴展',
            autoFill: '自動補洞',
            showBorder: '顯示框線',
            borderThickness: '框線粗細',

            // --- HandBoard UI ---
            navRecord: "紀錄",
            navRoutine: "課表",
            navSetting: "設定",
            recordTitle: "訓練紀錄",
            weeklyTrain: "本週訓練",
            times: "次",
            calendarHint: "點擊日曆日期以管理訓練紀錄",
            myRoutines: "我的課表",
            templateMax: "最大指力",
            templateRepeaters: "間歇訓練",
            appearance: "個人化",
            themeMode: "外觀模式",
            themeSystem: "跟隨系統",
            themeLight: "淺色",
            themeDark: "深色",
            weightSet: "體重設定",
            soundSet: "音效與語音",
            masterSound: "主音效開關",
            soundStart: "開始音",
            soundCount: "倒數音",
            soundFinish: "結束音",
            pulseHint: "倒數閃爍提示",
            ttsLabel: "朗讀項目 (TTS)",
            countTiming: "倒數提示時機",
            sec: "秒",
            speechRate: "語音語速",
            accountSync: "帳號與裝置",
            notLoggedIn: "未登入",
            cloudSync: "雲端同步",
            btScale: "連接藍芽吊秤",
            sensorZero: "感測器歸零",
            disconnected: "未連線",
            lastSeconds: "最後"
        },
        en: {
            // --- Index / Global ---
            heroTitle: "Digital Toolbox for Climbers",
            heroDesc: "AI-Assisted Development: Designed for those who love to climb.",
            coreTitle: "Core Tools",
            badgeLive: "Version 1.1 Live",
            hbTitle: "Hangboard Timer (v1.2)",
            hbPain: "Solves the pain of tedious setups, distracted logging, and fragmented data tracking.",
            hbF1: "<b>Block-Based Architecture</b>: Nest timers and reps like building blocks to easily create complex routines.",
            hbF2: "<b>Bluetooth Integration</b>: Real-time display for WH-C06 dynamometers and Garmin HR broadcast.",
            hbF3: "<b>Complete Growth Track</b>: Custom metrics, integrated PR cards, and trend charts.",
            hbFuture: "Future: Log Bluetooth device data into training records for deeper analysis.", hfTitle: "Route Editor (HoldFocus v1)",
            hfPain: "Hard to recall beta or explain movements? Create route maps manually to stay focused.",
            hfF1: "Manual hold marking for precise route mapping",
            hfF2: "Support for image export and instant sharing",
            hfFuture: "Future: Considering AI hold recognition and cloud albums.",
            changelogTitle: "Changelog",
            logHbV1_2: "Hangboard v1.2 Live",
            logHbDesc1_2: "Integrated real-time WH-C06 and Garmin heart rate broadcast (display only).",
            logHbV1_1: "Hangboard v1.1 Live",
            logHbDesc1_1: "Enhanced training log mechanism, adding custom metric integration and automated logging.",
            logHbV1: "Hangboard v1 Live",
            logHbDesc: "Unrestricted structured editor, voice command optimization, and training calendar are now live.",
            logHfV1: "HoldFocus v1 Live",
            logHfDesc: "Manual marking features released. Supports custom drawing and high-res exports.",
            queueTitle: "Features in Queue",
            q1Title: "Motion & Pose Analysis",
            q1Desc: "Analyze climbing trajectory and center of gravity to improve precision.",
            q2Title: "Shoe Spec Comparison",
            q2Desc: "Comprehensive database providing shoe sizing recommendations.",
            contactUs: "Contact Us",
            footerCopy: "© 2026 Climbing Toolbox. Designed for those who love to climb.",
            login: "Login",
            logout: "Logout",
            langText: "中文",

            // --- HoldFocus UI ---
            open: 'Open',
            download: 'Save',
            tabRoute: 'Route',
            tabText: 'Text/Tags',
            instruction: '1. Open a photo (top right)<br>2. Click <b class="inline-flex items-center justify-center w-4 h-4 bg-blue-600 text-white rounded-full text-[10px] align-middle shadow-sm">+</b> to start lassoing holds',
            selectedHolds: 'Selected Holds',
            emptySelection: 'No holds selected',
            eraserSize: 'Eraser Size',
            imageAdjustment: 'Global Adjustments',
            contrast: 'Contrast',
            bgStyle: 'Background Style',
            saturation: 'Saturation',
            dim: 'Dim',
            blur: 'Blur',
            tags: 'Tags',
            text: 'Text',
            textPlaceholder: 'Type here...',
            textColor: 'Text Color',
            textBgColor: 'Background',
            size: 'Size',
            rotation: 'Rotation',
            deleteObj: 'Delete Item',
            processing: 'Processing...',
            openPhotoPrompt: 'Please open a photo',
            holdNum: 'Hold #',
            tolerance: 'Tolerance',
            expand: 'Expand',
            autoFill: 'Fill Holes',
            showBorder: 'Show Border',
            borderThickness: 'Border Width',

            // --- HandBoard UI ---
            navRecord: "Log",
            navRoutine: "Routines",
            navSetting: "Settings",
            recordTitle: "Training Log",
            weeklyTrain: "This Week",
            times: "times",
            calendarHint: "Tap a date to manage records",
            myRoutines: "My Routines",
            templateMax: "Max Hangs",
            templateRepeaters: "Repeaters",
            appearance: "Appearance",
            themeMode: "Theme",
            themeSystem: "System",
            themeLight: "Light",
            themeDark: "Dark",
            weightSet: "Weight",
            soundSet: "Sound & Voice",
            masterSound: "Master Audio",
            soundStart: "Start",
            soundCount: "Countdown",
            soundFinish: "Finish",
            pulseHint: "Pulse Alert",
            ttsLabel: "Speech (TTS)",
            countTiming: "Countdown Trigger",
            sec: "s",
            speechRate: "Speech Rate",
            accountSync: "Account & Device",
            notLoggedIn: "Not Logged In",
            cloudSync: "Cloud Sync",
            btScale: "Bluetooth Scale",
            sensorZero: "Zero Sensor",
            disconnected: "Disconnected",
            lastSeconds: "Last"
        }
    },

    t(path) {
        const keys = path.split('.');
        let res = this.dict[this._current];
        for (const k of keys) {
            if (res && res[k]) res = res[k];
            else return path;
        }
        return res;
    },

    toggle() {
        this._current = this._current === 'zh' ? 'en' : 'zh';
        localStorage.setItem('userLang', this._current);
        this.updatePage();
    },

    updatePage() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            const txt = this.t(key);
            if (txt && el.innerHTML !== txt && el.textContent !== txt) {
                if (txt.includes('<')) el.innerHTML = txt;
                else el.textContent = txt;
            }
        });

        document.querySelectorAll('[data-i18n-ph]').forEach(el => {
            el.placeholder = this.t(el.getAttribute('data-i18n-ph'));
        });

        document.documentElement.lang = this._current === 'zh' ? 'zh-TW' : 'en';
        document.body.classList.remove('i18n-loading');
    }
};

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => i18n.updatePage());
} else {
    i18n.updatePage();
}