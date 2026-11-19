// 產生 PWA 圖示（web/icon-192.png、icon-512.png、icon-maskable-512.png、apple-touch-icon.png）：用已安裝的 Google Chrome 無頭模式把一段 HTML 截圖。
// 用法：node tools/pwa_icons.mjs   （只用 node 內建 API；圖示設計 = 深綠底、金色「秦」，字落在可遮罩安全區內，所以同一張圖給 any／maskable 共用）
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const out = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'web');
const port = 9355;
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  ['--headless=new', `--remote-debugging-port=${port}`, '--user-data-dir=/tmp/chr-pwa-icons', '--no-first-run', 'about:blank'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 3000));
const tabs = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const ws = new WebSocket(tabs.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map();
ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id && pend.has(m.id)) { pend.get(m.id)(m.result); pend.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const html = `<body style="margin:0;width:100vw;height:100vh;display:flex;align-items:center;justify-content:center;overflow:hidden;
background:radial-gradient(circle at 50% 38%,#2f4030 0,#142018 55%,#070b08 100%)">
<div style="width:68vmin;height:68vmin;border:1.2vmin solid #b8a15a;border-radius:50%;display:flex;align-items:center;justify-content:center;box-sizing:border-box;
font:bold 44vmin/1 'Songti TC','STSong','Noto Serif CJK TC',serif;color:#e0c97f;text-shadow:0 .6vmin 1.4vmin #000">秦</div></body>`;
await send('Page.enable');
for (const [name, size] of [['icon-192.png', 192], ['icon-512.png', 512], ['icon-maskable-512.png', 512], ['apple-touch-icon.png', 180]]) {
  await send('Emulation.setDeviceMetricsOverride', { width: size, height: size, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: 'data:text/html;charset=utf-8,' + encodeURIComponent(html) });
  await new Promise(r => setTimeout(r, 800));
  const s = await send('Page.captureScreenshot', { format: 'png' });
  fs.writeFileSync(path.join(out, name), Buffer.from(s.data, 'base64'));
  console.log(name, size);
}
chrome.kill(); process.exit(0);
