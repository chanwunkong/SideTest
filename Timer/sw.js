// 先向伺服器確認新版，離線時才用快取
const CACHE_NAME = 'timer-v15';
const SHELL = ['./', './index.html', './manifest.json', '../core/trainingApp.js', '../core/health.css', '../core/i18n.js', '../core/logic.js', '../core/registry.js', '../core/store.js', '../core/training.js', '../core/flow.js', '../core/flowRun.js', '../core/tags.js', '../core/analysis.js', '../core/firebase-config.js'];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(SHELL)));
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k.startsWith('timer-') && k !== CACHE_NAME).map((k) => caches.delete(k)))));
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  if (event.request.method !== 'GET' || url.origin !== location.origin) return;
  event.respondWith(
    fetch(event.request, { cache: 'no-cache' })
      .then((response) => {
        const copy = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
        return response;
      })
      .catch(() => caches.match(event.request)),
  );
});
