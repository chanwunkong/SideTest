// 逐場景列出 BNT 觸發區（s_pctstart 劇情事件、go 出口、openall、c_aigroup）：node tools/webtrig.mjs <基底網址> <CDP埠> 場景1,場景2,… → 每行 場景<TAB>觸發區編號 事件 座標
// WARN=1：改印 console 警告／錯誤（console.warn／error、例外）而不是觸發區。用同一個 headless Chrome 依序載入 ?scene=…&debug=cells（只用 node 內建 API）
import { spawn } from 'node:child_process';
const [base, portS, list] = [process.argv[2], process.argv[3], process.argv[4]]; const port = +portS;
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', ['--headless=new', `--remote-debugging-port=${port}`, '--user-data-dir=/tmp/chr-trig-' + port, '--no-first-run', '--window-size=320,240', 'about:blank'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 3000));
const tabs = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const ws = new WebSocket(tabs.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map(); let logs = [];
ws.onmessage = e => { const m = JSON.parse(e.data);
  if (m.id && pend.has(m.id)) { pend.get(m.id)(m.result); pend.delete(m.id); }
  else if (m.method === 'Runtime.consoleAPICalled') logs.push((m.params.type === 'warning' || m.params.type === 'error' ? 'WARN ' : '') + m.params.args.map(a => a.value ?? a.description).join(' '));
  else if (m.method === 'Runtime.exceptionThrown') logs.push('EXC: ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text)); };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
await send('Runtime.enable'); await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: "localStorage.setItem('qinIntro','1');" });
for (const sc of list.split(',')) {
  logs = [];
  await send('Page.navigate', { url: `${base}?scene=${sc}&debug=cells` });
  await new Promise(r => setTimeout(r, 1500));
  for (let i = 0; i < 60; i++) { await new Promise(r => setTimeout(r, 300)); const v = await send('Runtime.evaluate', { expression: "document.getElementById('loader')&&document.getElementById('loader').style.display", returnByValue: true }); if (v.result.value === 'none') break; }
  await new Promise(r => setTimeout(r, 700));
  for (const l of logs) if (process.env.WARN ? /^WARN |^EXC/.test(l) : /^trigger |^EXC/.test(l)) console.log(sc + '\t' + l);
}
chrome.kill(); process.exit(0);
