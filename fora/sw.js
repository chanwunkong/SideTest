// 本站檔案：先向伺服器確認新版，離線時才用快取。CDN 元件網址有版本號，先用快取。
const CACHE_NAME = 'fora-v1';
const SHELL = ['./fora.html', './manifest.json', './icons/icon-180.png', './icons/icon-192.png', './icons/icon-512.png'];
const CDN = ['https://cdn.tailwindcss.com', 'https://unpkg.com/html5-qrcode@2.3.8/html5-qrcode.min.js', 'https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js'];
const CDN_HOSTS = new Set(CDN.map((u) => new URL(u).host));

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => Promise.all([
    cache.addAll(SHELL),
    // CDN 是跨網域，拿到的是 opaque 回應，addAll 不收，只能逐一 put
    ...CDN.map((u) => fetch(u, { mode: 'no-cors' }).then((r) => cache.put(u, r)).catch(() => {})),
  ])));
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k.startsWith('fora-') && k !== CACHE_NAME).map((k) => caches.delete(k)))));
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  const req = event.request;
  const url = new URL(req.url);
  if (req.method !== 'GET') return;
  if (CDN_HOSTS.has(url.host)) {
    event.respondWith(caches.match(req).then((hit) => hit || fetch(req).then((r) => {
      const copy = r.clone();
      caches.open(CACHE_NAME).then((cache) => cache.put(req, copy));
      return r;
    })));
    return;
  }
  if (url.origin !== location.origin) return;
  event.respondWith(
    fetch(req, { cache: 'no-cache' })
      .then((response) => {
        const copy = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(req, copy));
        return response;
      })
      .catch(() => caches.match(req, { ignoreSearch: true })),
  );
});
