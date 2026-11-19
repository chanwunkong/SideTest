import { spawn } from 'node:child_process';
const url = process.argv[2], w = +process.argv[3], h = +process.argv[4], wait = +process.argv[5]*1000, out = process.argv[6];
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
 ['--headless=new','--remote-debugging-port=9333','--user-data-dir=/tmp/chr-prof2','--no-first-run',`--window-size=${w},${h}`,'about:blank'],{stdio:'ignore'});
await new Promise(r=>setTimeout(r,3000));
const tabs = await (await fetch('http://127.0.0.1:9333/json')).json();
const ws = new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);
await new Promise(r=>ws.onopen=r);
let id=0; const pend=new Map(); const logs=[];
ws.onmessage=e=>{const m=JSON.parse(e.data);
  if(m.id&&pend.has(m.id)){pend.get(m.id)(m.result);pend.delete(m.id);}
  else if(m.method==='Runtime.consoleAPICalled') logs.push(m.params.type+': '+m.params.args.map(a=>a.value??a.description).join(' '));
  else if(m.method==='Runtime.exceptionThrown') logs.push('EXC: '+(m.params.exceptionDetails.exception?.description||m.params.exceptionDetails.text));
  else if(m.method==='Log.entryAdded') logs.push('LOG: '+m.params.entry.text+' '+(m.params.entry.url||''));};
const send=(method,params={})=>new Promise(r=>{const i=++id;pend.set(i,r);ws.send(JSON.stringify({id:i,method,params}));});
await send('Runtime.enable'); await send('Log.enable'); await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride',{width:w,height:h,deviceScaleFactor:3,mobile:true}); await send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:5}); await send('Emulation.setUserAgentOverride',{userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1'});
await send('Emulation.setCPUThrottlingRate',{rate:+process.env.THR||1}); const t0=Date.now(); await send('Page.navigate',{url});
let ready=-1; let tapped=false; for(let i=0;i<Math.floor(wait/500);i++){ await new Promise(r=>setTimeout(r,500)); if(!tapped && Date.now()-t0>6000){tapped=true; for(const t of ['mousePressed','mouseReleased']) await send('Input.dispatchMouseEvent',{type:t,x:200,y:400,button:'left',clickCount:1});} const v=await send('Runtime.evaluate',{expression:"document.getElementById('loader').style.display",returnByValue:true}); if(v.result.value==='none'){ready=Date.now()-t0;break;} }
console.log('READY_MS',ready);
const st = await send('Runtime.evaluate',{expression:"document.getElementById('status').textContent+' | canvas '+document.querySelector('canvas').width+'x'+document.querySelector('canvas').height",returnByValue:true});
const shot = await send('Page.captureScreenshot',{format:'png'});
(await import('node:fs')).writeFileSync(out, Buffer.from(shot.data,'base64'));
console.log('STATUS:',st.result.value); console.log(logs.slice(0,25).join('\n')||'(no console output)');
chrome.kill(); process.exit(0);
