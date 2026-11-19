// PWA 檢查（headless Chrome + CDP，只用 node 內建 API）：node tools/pwa_check.mjs <網址，例如 http://127.0.0.1:8651/> [輸出前綴]
// 檢查：manifest（Page.getAppManifest 無錯誤、圖示齊全）、service worker 註冊與快取內容（只有引擎檔，沒有 /game/）、
// 離線重開仍能載入引擎、安全區（Emulation.setSafeAreaInsetsOverride）讓畫布縮進去。全部通過印 pwa ok
import { spawn } from 'node:child_process';
import fs from 'node:fs';
const url = process.argv[2], prefix = process.argv[3] || '/tmp/pwa';
const port = 9356;
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  ['--headless=new', `--remote-debugging-port=${port}`, '--user-data-dir=/tmp/chr-pwa-check', '--no-first-run', 'about:blank'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 3000));
const tabs = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const ws = new WebSocket(tabs.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map();
ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id && pend.has(m.id)) { pend.get(m.id)(m.result ?? m.error); pend.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const sleep = ms => new Promise(r => setTimeout(r, ms));
const ev = async expr => (await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true })).result?.value;
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
await send('Page.enable'); await send('Runtime.enable'); await send('Network.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 844, height: 390, deviceScaleFactor: 1, mobile: true });
await send('Page.addScriptToEvaluateOnNewDocument', { source: "localStorage.setItem('qinIntro','1');" });
await send('Page.navigate', { url }); await sleep(5000);
const mf = await send('Page.getAppManifest');
const m = mf.data ? JSON.parse(mf.data) : {};
ok(!(mf.errors || []).length, 'manifest 無錯誤 ' + JSON.stringify(mf.errors || []));
ok(m.name === '秦殤' && m.display === 'standalone' && m.orientation === 'any' && m.background_color === '#000000', 'manifest 名稱／display／orientation／背景色');
ok(['192x192', '512x512'].every(s => m.icons?.some(i => i.sizes === s)) && m.icons?.some(i => i.purpose === 'maskable'), 'manifest 圖示 192／512／maskable');
for (const i of m.icons || []) { const r = await fetch(new URL(i.src, url)); ok(r.ok && r.headers.get('content-type') === 'image/png', '圖示可下載 ' + i.src); }
ok(await ev("!!document.querySelector('link[rel=apple-touch-icon]') && document.querySelector('meta[name=viewport]').content.includes('viewport-fit=cover') && document.querySelector('meta[name=apple-mobile-web-app-status-bar-style]').content === 'black-translucent'"), 'iOS meta／apple-touch-icon／viewport-fit=cover');
const secure = await ev('isSecureContext');
console.log('isSecureContext =', secure);
if (secure) {
  await ev("navigator.serviceWorker.ready.then(() => 1)"); await sleep(3000);
  const keys = await ev("caches.keys().then(async ks => { const o = {}; for (const k of ks) o[k] = (await (await caches.open(k)).keys()).map(r => new URL(r.url).pathname); return o; })");
  const names = Object.keys(keys || {});
  ok(names.length === 1 && names[0].startsWith('qin-'), 'service worker 快取一份 ' + names);
  const files = keys[names[0]] || [];
  ok(['/index.js', '/index.wasm', '/index.html', '/manifest.webmanifest'].every(f => files.includes(f)) && !files.some(f => /^\/(game|batch|movie)/.test(f)), '快取只有引擎檔：' + files.join(' '));
  await send('Network.emulateNetworkConditions', { offline: true, latency: 0, downloadThroughput: 0, uploadThroughput: 0 });
  await send('Page.navigate', { url: url + '?offline=1' }); await sleep(4000);
  ok(await ev("typeof Module === 'object' && !!document.getElementById('canvas')"), '離線重開：引擎仍載入（index.html／index.js 來自快取）');
  await send('Network.emulateNetworkConditions', { offline: false, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
}
// 安全區：模擬 iPhone 橫向（左右瀏海 47、底部橫條 21）
const so = await send('Emulation.setSafeAreaInsetsOverride', { insets: { top: 0, left: 47, right: 47, bottom: 21 } });
await send('Page.navigate', { url }); await sleep(5000);
const geo = await ev("(() => { const r = document.getElementById('canvas').getBoundingClientRect(); return [r.left, r.top, r.right, r.bottom, innerWidth, innerHeight, window.qinW, window.qinH]; })()");
console.log('canvas rect / inner / qin =', JSON.stringify(geo), JSON.stringify(so));
ok(geo && Math.abs(geo[0] - 47) < 1 && Math.abs(geo[2] - (844 - 47)) < 1 && Math.abs(geo[3] - (390 - 21)) < 1 && geo[6] === 844 - 94 && geo[7] === 390 - 21, '安全區：畫布避開左右瀏海與底部橫條');
const s = await send('Page.captureScreenshot', { format: 'png' }); fs.writeFileSync(prefix + '_safe.png', Buffer.from(s.data, 'base64'));
console.log(bad ? 'pwa FAILED ' + bad : 'pwa ok');
chrome.kill(); process.exit(bad ? 1 : 0);
