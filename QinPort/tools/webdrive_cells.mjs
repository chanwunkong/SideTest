// 網頁版自動操作（headless Chrome + CDP，只用 node 內建 API）：node tools/webdrive_cells.mjs <網址> <寬> <高> <輸出前綴> "<步驟>"
// 步驟以 ; 分隔：wait:毫秒 | tap:x,y | click:x,y（滑鼠）| key:字母 | hold:x,y,毫秒（按住）| drag:x0,y0,x1,y1,毫秒（手指）| mdrag:x0,y0,x1,y1,毫秒（滑鼠拖放）| shot:名字 | eval:JS 運算式（印出結果）
// 環境變數 NOCLICK = 不在開頭點一下（用 ?scene= 直接進場景、要看過場時用）；INJECT = 每頁載入前先執行的 JS（例如監看載入畫面）；CDP_PORT = 除錯埠（預設 9344）；THROTTLE=1 = 模擬 4G
// 第一次玩的片頭影片：預設先設好 localStorage.qinIntro，並在開頭點一下跳過 logo 影片。console 輸出存到 <前綴>.log
import { spawn } from 'node:child_process';
import fs from 'node:fs';
const [url, w, h, prefix, steps] = [process.argv[2], +process.argv[3], +process.argv[4], process.argv[5], process.argv[6] || ''];
const port = +process.env.CDP_PORT || 9344;
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  ['--headless=new', `--remote-debugging-port=${port}`, '--user-data-dir=/tmp/chr-qb5-' + port, '--no-first-run', `--window-size=${w},${h}`, 'about:blank'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 3000));
const tabs = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const ws = new WebSocket(tabs.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map(); const logs = [];
ws.onmessage = e => { const m = JSON.parse(e.data);
  if (m.id && pend.has(m.id)) { pend.get(m.id)(m.result); pend.delete(m.id); }
  else if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.type + ': ' + m.params.args.map(a => a.value ?? a.description).join(' '));
  else if (m.method === 'Runtime.exceptionThrown') logs.push('EXC: ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text));
  else if (m.method === 'Log.entryAdded') logs.push('LOG: ' + m.params.entry.text + ' ' + (m.params.entry.url || '')); };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const sleep = ms => new Promise(r => setTimeout(r, ms));
await send('Runtime.enable'); await send('Log.enable'); await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: w, height: h, deviceScaleFactor: 1, mobile: true });
await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
await send('Page.addScriptToEvaluateOnNewDocument', { source: "localStorage.setItem('qinIntro','1');" + (process.env.INJECT || '') });
if (process.env.THROTTLE) await send('Network.enable'), await send('Network.emulateNetworkConditions', { offline: false, latency: 100, downloadThroughput: 500000, uploadThroughput: 250000 });  // THROTTLE=1：模擬 4G（4Mbps、延遲 100ms）
await send('Page.navigate', { url });
const touch = (type, x, y) => send('Input.dispatchTouchEvent', { type, touchPoints: type === 'touchEnd' ? [] : [{ x, y, id: 1 }] });
let shots = 0;
await sleep(5000);
if (!process.env.NOCLICK) for (const t of ['mousePressed', 'mouseReleased']) await send('Input.dispatchMouseEvent', { type: t, x: w / 2, y: h / 2, button: 'left', clickCount: 1 });  // 跳過 logo 影片（NOCLICK=1：直接進場景的測試不要點，會把過場跳掉）
for (let i = 0; i < 120; i++) { await sleep(500); const v = await send('Runtime.evaluate', { expression: "document.getElementById('loader').style.display", returnByValue: true }); if (v.result.value === 'none') break; }
for (const st of steps.split(';').filter(Boolean)) {
  const [cmd, ...rest] = st.split(':'); const arg = rest.join(':');
  if (cmd === 'wait') await sleep(+arg);
  else if (cmd === 'tap') { const [x, y] = arg.split(',').map(Number); await touch('touchStart', x, y); await sleep(80); await touch('touchEnd'); }
  else if (cmd === 'click') { const [x, y] = arg.split(',').map(Number); await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x, y }); for (const t of ['mousePressed', 'mouseReleased']) { await send('Input.dispatchMouseEvent', { type: t, x, y, button: 'left', buttons: t === 'mousePressed' ? 1 : 0, clickCount: 1 }); await sleep(80); } }  // 滑鼠點擊（會收起觸控介面，不點到觸控按鈕）
  else if (cmd === 'key') { for (const t of ['keyDown', 'keyUp']) { await send('Input.dispatchKeyEvent', { type: t, key: arg, code: 'Key' + arg.toUpperCase(), text: t === 'keyDown' ? arg : undefined, windowsVirtualKeyCode: arg.toUpperCase().charCodeAt(0) }); await sleep(60); } }  // 按鍵（例如 h = 鏡頭回到主角）
  else if (cmd === 'hold') { const [x, y, ms] = arg.split(',').map(Number); await touch('touchStart', x, y); await sleep(ms); await touch('touchEnd'); }
  else if (cmd === 'drag') { const [x0, y0, x1, y1, ms] = arg.split(',').map(Number); await touch('touchStart', x0, y0);
    for (let k = 1; k <= 10; k++) { await touch('touchMove', x0 + (x1 - x0) * k / 10, y0 + (y1 - y0) * k / 10); await sleep(ms / 10); } await sleep(300); await touch('touchEnd'); }
  else if (cmd === 'mdrag') { const [x0, y0, x1, y1, ms] = arg.split(',').map(Number); const m = (type, x, y, b) => send('Input.dispatchMouseEvent', { type, x, y, button: 'left', buttons: b, clickCount: 1 });
    await m('mouseMoved', x0, y0, 0); await m('mousePressed', x0, y0, 1);
    for (let k = 1; k <= 10; k++) { await m('mouseMoved', x0 + (x1 - x0) * k / 10, y0 + (y1 - y0) * k / 10, 1); await sleep(ms / 10); } await m('mouseReleased', x1, y1, 0); await sleep(200); }
  else if (cmd === 'shot') { const s = await send('Page.captureScreenshot', { format: 'png' }); fs.writeFileSync(`${prefix}_${arg || shots++}.png`, Buffer.from(s.data, 'base64')); }
  else if (cmd === 'eval') { const r = await send('Runtime.evaluate', { expression: arg, returnByValue: true }); logs.push('eval: ' + JSON.stringify(r.result.value)); }
}
fs.writeFileSync(prefix + '.log', logs.join('\n'));
console.log(logs.slice(-40).join('\n'));
chrome.kill(); process.exit(0);
