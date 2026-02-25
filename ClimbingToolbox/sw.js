const CACHE_NAME = 'climbing-toolbox-v4'; // 更新版本號以強制重新整理快取

const URLS_TO_CACHE = [
    './Hangboard.html',
    './config.js',
    './manifest.json',

    // --- 新增：拆分後的本地資源 ---
    './assets/css/style.css',
    './js/modules/storage.js',
    './js/modules/timer.js',
    './js/modules/ui.js',
    './js/modules/holdfocus-engine.js',
    './js/modules/i18n.js',

    // --- 外部 CDN 資源 ---
    'https://cdn.tailwindcss.com',
    'https://cdn.jsdelivr.net/npm/sortablejs@latest/Sortable.min.js',
    'https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&family=JetBrains+Mono:wght@500;700&display=swap',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-app-compat.js',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-auth-compat.js',
    'https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore-compat.js'
];

// 安裝時快取資源
self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE_NAME).then(cache => {
            console.log('Opened cache, pre-caching all modules...');
            return cache.addAll(URLS_TO_CACHE);
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

// 網路請求攔截策略 (快取優先)
self.addEventListener('fetch', event => {
    event.respondWith(
        caches.match(event.request).then(response => {
            return response || fetch(event.request);
        })
    );
});