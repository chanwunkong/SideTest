const CACHE_NAME = 'climbing-toolbox-v7'; // 更新版本號以強制重新整理快取

// 本地資源：必須全部快取成功，否則視為安裝失敗
const LOCAL_URLS_TO_CACHE = [
    './Hangboard.html',
    './HoldFocus.html',
    './config.js',
    './manifest-hangboard.json',
    './manifest-holdfocus.json',

    // --- 拆分後的本地資源 ---
    './assets/css/style.css',
    './js/main.js',
    './js/modules/storage.js',
    './js/modules/timer.js',
    './js/modules/ui.js',
    './js/modules/views.js',
    './js/modules/templates.js',
    './js/modules/goalManager.js',
    './js/modules/bleManager.js',
    './js/modules/hrManager.js',
    './js/modules/tagManager.js',
    './js/modules/holdfocus-engine.js',
    './js/modules/i18n.js',
    './js/modules/analytics.js',
];

// 外部 CDN 資源：逐一快取，單一資源失敗不影響其餘安裝
const CDN_URLS_TO_CACHE = [
    'https://cdn.tailwindcss.com',
    'https://cdn.jsdelivr.net/npm/sortablejs@latest/Sortable.min.js',
    'https://cdn.jsdelivr.net/npm/chart.js',
    'https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&family=JetBrains+Mono:wght@500;700&display=swap',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-app-compat.js',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-auth-compat.js',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore-compat.js'
];

// 安裝時快取資源
self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE_NAME).then(async cache => {
            console.log('Opened cache, pre-caching local modules...');
            await cache.addAll(LOCAL_URLS_TO_CACHE);

            // CDN 資源盡力快取，個別失敗不應讓整個安裝流程中止
            await Promise.all(CDN_URLS_TO_CACHE.map(url =>
                cache.add(url).catch(err => console.warn('CDN 資源快取失敗，略過:', url, err))
            ));
        })
    );
});

// 激活時清理舊快取 (確保用戶拿到最新版)
self.addEventListener('activate', event => {
    event.waitUntil(
        caches.keys().then(cacheNames => {
            return Promise.all(
                cacheNames.map(name => {
                    if (name !== CACHE_NAME) {
                        return caches.delete(name);
                    }
                })
            );
        })
    );
});

// 網路請求攔截策略 (快取優先，離線時的頁面導覽退回 Hangboard.html)
self.addEventListener('fetch', event => {
    event.respondWith(
        caches.match(event.request).then(response => {
            if (response) return response;
            return fetch(event.request).catch(() => {
                if (event.request.mode === 'navigate') {
                    return caches.match('./Hangboard.html');
                }
                throw new Error('離線且無快取可用');
            });
        })
    );
});