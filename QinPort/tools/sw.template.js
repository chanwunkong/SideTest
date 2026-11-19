// Service worker 樣板：make web 結尾把版本佔位字（下面 VER 那行）換成 index.js／index.wasm／shell.html 的雜湊，寫成 web/sw.js（版本變了 sw.js 內容就變，瀏覽器才會更新）。
// 只快取引擎本體；/game/、/batch、/movie、sw.js 本身都不攔，走網路。沒有 skipWaiting：新版本裝好後等所有分頁關掉，下次開啟才換（不會玩到一半換程式）。
const VER = '__VER__', CACHE = 'qin-' + VER;
const URLS = ['./', 'index.html', 'index.js', 'index.wasm', 'manifest.webmanifest', 'icon-192.png', 'icon-512.png', 'icon-maskable-512.png', 'apple-touch-icon.png']
  .map(f => new URL(f, self.registration.scope).href);
self.addEventListener('install', e => e.waitUntil(caches.open(CACHE).then(c => c.addAll(URLS.map(u => new Request(u, { cache: 'reload' }))))));
self.addEventListener('activate', e => e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k.startsWith('qin-') && k !== CACHE).map(k => caches.delete(k))))));
self.addEventListener('fetch', e => {
  const u = new URL(e.request.url);
  const key = u.origin + u.pathname;  // 網址參數（?scene=…）不影響檔案內容
  if (e.request.method !== 'GET' || !URLS.includes(key)) return;
  e.respondWith(caches.open(CACHE).then(c => c.match(key)).then(m => m || fetch(e.request)));
});
