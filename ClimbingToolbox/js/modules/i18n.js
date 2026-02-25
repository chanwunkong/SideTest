// --- js/modules/i18n.js ---
const i18n = {
    _current: localStorage.getItem('userLang') || (navigator.language.startsWith('zh') ? 'zh' : 'en'),

    // 統一字典
    dict: {
        zh: {
            // --- Index / Global ---
            heroTitle: "為攀岩者打造的數位工具箱",
            heroDesc: "AI 輔助開發：設計給所有熱愛攀爬的人",
            coreTitle: "核心工具",
            badgeLive: "正式版已上線",
            hbTitle: "指力訓練計時 (Hangboard v1)",
            hbPain: "解決訓練時雙手滿是粉末不便觸碰螢幕，以及複雜巢狀訓練難以精準計時的痛點。",
            hbF1: "無限制結構化計時器，支持離線或聲控",
            hbF2: "全自動訓練日誌與日曆統計系統",
            hbFuture: "未來規劃：支援藍芽吊秤連動，讓訓練量自動量化紀錄。",
            hfTitle: "路線編輯器 (HoldFocus v1)",
            hfPain: "解決岩館路線更新頻繁難以留存細節，以及向他人解釋動作時溝通成本高的問題。",
            hfF1: "手動標記岩點，快速產出路線示意圖",
            hfF2: "支援圖片導出與即時紀錄分享",
            hfFuture: "未來規劃：考慮加入智慧岩點辨識與個人雲端相簿。",
            changelogTitle: "更新紀錄",
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
            saturation: '飽和度',
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
            templateMax: "⚡️ 最大指力",
            templateRepeaters: "🔄 間歇訓練",
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
            heroDesc: "AI-Assisted Development:Designed for those who love to climb.",
            coreTitle: "Core Tools",
            badgeLive: "Version 1.0 Live",
            hbTitle: "Hangboard Timer (v1)",
            hbPain: "Hands covered in chalk? Voice-controlled structured timer eliminates the need to touch your screen.",
            hbF1: "Unlimited structured timer with Offline & Voice support",
            hbF2: "Automated training logs and calendar analytics",
            hbFuture: "Future: Bluetooth scale integration for automated load tracking.",
            hfTitle: "Route Editor (HoldFocus v1)",
            hfPain: "Hard to recall beta or explain movements? Create route maps manually to stay focused.",
            hfF1: "Manual hold marking for precise route mapping",
            hfF2: "Support for image export and instant sharing",
            hfFuture: "Future: Considering AI hold recognition and cloud albums.",
            changelogTitle: "Changelog",
            logHbV1: "Hangboard v1 Live",
            logHbDesc: "Structured editor, voice command optimization, and training calendar are now live.",
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
            templateMax: "⚡️ Max Hangs",
            templateRepeaters: "🔄 Repeaters",
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

    // 核心邏輯 (支援 . 符號巢狀搜尋)
    t(path) {
        const keys = path.split('.');
        let res = this.dict[this._current];
        for (const k of keys) {
            if (res && res[k]) res = res[k];
            else return path; // 找不到則回傳 key
        }
        return res;
    },

    toggle() {
        this._current = this._current === 'zh' ? 'en' : 'zh';
        localStorage.setItem('userLang', this._current);
        location.reload();
    },

    // 統一更新頁面文字的方法
    updatePage() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            const txt = this.t(key);
            if (txt) {
                // 優化：如果文字沒變，就不重新賦值，減少 DOM 閃爍
                if (el.innerHTML !== txt && el.textContent !== txt) {
                    if (el.innerHTML.includes('<') || txt.includes('<')) el.innerHTML = txt;
                    else el.textContent = txt;
                }
            }
        });

        // 處理 placeholder
        document.querySelectorAll('[data-i18n-ph]').forEach(el => {
            const key = el.getAttribute('data-i18n-ph');
            el.placeholder = this.t(key);
        });

        document.documentElement.lang = this._current === 'zh' ? 'zh-TW' : 'en';
    }
};