// 先用網路，離線時才用快取，避免舊版畫面卡在手機上
const CACHE_NAME = 'health-v25';
const SHELL = ['./', './index.html', './manifest.json', './icons/icon-180.png', './icons/icon-192.png', './icons/icon-512.png', './js/app.js', './js/cards.js', './js/mealImport.js', '../core/health.css', '../core/i18n.js', '../core/logic.js', '../core/registry.js', '../core/store.js', '../core/training.js', '../core/flow.js', '../core/flowRun.js', '../core/tags.js', '../core/firebase-config.js'];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(SHELL)));
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k.startsWith('health-') && k !== CACHE_NAME).map((k) => caches.delete(k)))));
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  if (event.request.method !== 'GET' || url.origin !== location.origin) return;
  event.respondWith(
    // no-cache 會先向伺服器確認，避免 GitHub Pages 的快取讓手機停在舊版
    fetch(event.request, { cache: 'no-cache' })
      .then((response) => {
        const copy = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
        return response;
      })
      .catch(() => caches.match(event.request)),
  );
});
