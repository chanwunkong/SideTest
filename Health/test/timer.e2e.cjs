// 訓練計時：node Health/test/timer.e2e.cjs <截圖資料夾>（在 repo 根目錄起 server，PW_URL 指向 /Health）
const { chromium, devices } = require('playwright');
const assert = require('assert');

(async () => {
  const shots = process.argv[2];
  const browser = await chromium.launch({ channel: process.env.PW_CHANNEL });
  const ctx = await browser.newContext({ ...devices['iPhone 13'], serviceWorkers: 'block', locale: 'zh-TW' });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const button = (name, exact = true) => page.getByRole('button', { name, exact });
  const timerUrl = `${process.env.PW_URL.replace('/Health', '/Timer')}/index.html`;
  await page.goto(timerUrl);

  // 建立課表：計時 + 循環 2 次（懸掛、休息最後略過）+ 次數
  // 空課表不能存，取消新課表不需要確認
  await button('＋ 新增課表').click();
  await button(/空白課表/, false).click();
  await page.locator('[data-action=save]').click();
  await page.getByText('請輸入課表名稱').waitFor();
  await page.locator('[data-action=close]').click();
  assert.equal(await page.locator('#sheet[open]').count(), 0);

  await button('＋ 新增課表').click();
  await button(/空白課表/, false).click();
  await page.fill('#routine-name', '測試課表');
  const addTimer = async (label, seconds, color) => {
    await page.locator('.editor-foot .btn.primary').click();
    await button('計時').click();
    await page.fill('input[name=label]', label);
    await page.fill('input[name=duration]', String(seconds));
    await page.locator(`.swatch.tone-${color}`).click();
    await page.locator('#panel .btn.primary.wide').click();
  };
  // 開新增面板時，後面維持編輯器，不會露出首頁
  await page.locator('.editor-foot .btn.primary').click();
  await page.locator('#panel[open]').waitFor();
  assert.equal(await page.locator('#sheet[open] .editor').count(), 1);
  assert.equal(await page.evaluate(() => document.activeElement.tagName), 'DIALOG');
  await page.screenshot({ path: `${shots}/panel.png` });
  await page.locator('#panel [data-action=back]').click();
  await addTimer('準備', 2, 'blue');
  await page.locator('.editor-foot .btn.primary').click();
  await button('循環').click();
  await page.fill('input[name=iterations]', '2');
  await page.locator('#panel .btn.primary.wide').click();
  await page.locator('.editor-foot .btn.primary').click();
  await button('次數').click();
  await page.fill('input[name=label]', '伏地挺身');
  await page.fill('input[name=count]', '5');
  await page.locator('#panel .btn.primary.wide').click();
  await page.locator('.loop-title').click();
  await addTimer('懸掛', 2, 'red');
  await addTimer('休息', 2, 'green');
  await page.locator('.flow-row', { hasText: '休息' }).click();
  await page.getByLabel('這是休息').check();
  await page.getByLabel('最後一次略過').check();
  await page.locator('#panel .btn.primary.wide').click();
  await page.locator('.flow-row', { hasText: '懸掛' }).click();
  // 設定全部在同一頁：快捷加入重量，也能自訂名稱
  await page.getByRole('button', { name: '＋ 重量' }).click();
  await page.fill('#metric-name', 'RPE');
  await page.getByRole('button', { name: '加入', exact: true }).click();
  await page.getByRole('button', { name: 'RPE ✕' }).waitFor();
  await page.screenshot({ path: `${shots}/step.png`, fullPage: true });
  await page.locator('#panel .btn.primary.wide').click();
  assert.equal(await page.locator('.flow-loop .flow-row', { hasText: '懸掛' }).count(), 1);
  assert.equal(await page.locator('.flow-loop .flow-row', { hasText: '伏地挺身' }).count(), 0);
  await page.screenshot({ path: `${shots}/editor.png` });

  // 拖曳：把次數拖進循環，再拖出來，然後再拖回去
  const drag = async (from, to, offset = 8) => {
    const a = await from.boundingBox();
    const b = await to.boundingBox();
    await page.mouse.move(a.x + 40, a.y + a.height / 2);
    await page.mouse.down();
    await page.mouse.move(a.x + 42, a.y + a.height / 2 - 8, { steps: 4 });
    await page.mouse.move(b.x + 40, b.y + offset, { steps: 12 });
    await page.waitForTimeout(250);
    await page.mouse.up();
    await page.waitForTimeout(250);
  };
  const dragBox = await page.locator('.flow-item:not(.flow-loop)', { hasText: '伏地挺身' }).boundingBox();
  await page.mouse.move(dragBox.x + 40, dragBox.y + 20);
  await page.mouse.down();
  await page.mouse.move(dragBox.x + 44, dragBox.y - 30, { steps: 6 });
  await page.waitForTimeout(150);
  assert.equal(await page.locator('.flow-drag').isVisible(), true, '拖曳中要看得到項目');
  await page.screenshot({ path: `${shots}/dragging.png` });
  await page.mouse.up();
  await page.waitForTimeout(250);
  const push = page.locator('.flow-item:not(.flow-loop)', { hasText: '伏地挺身' });
  await drag(push, page.locator('.flow-group .flow-group .flow-item', { hasText: '休息' }));
  assert.equal(await page.locator('.flow-loop .flow-row', { hasText: '伏地挺身' }).count(), 1);
  await drag(page.locator('.flow-item:not(.flow-loop)', { hasText: '伏地挺身' }), page.locator('.flow-group.root > .flow-item').last());
  assert.equal(await page.locator('.flow-loop .flow-row', { hasText: '伏地挺身' }).count(), 0);
  await drag(page.locator('.flow-item:not(.flow-loop)', { hasText: '伏地挺身' }), page.locator('.flow-group .flow-group .flow-item', { hasText: '休息' }));
  assert.equal(await page.locator('.flow-loop .flow-row', { hasText: '伏地挺身' }).count(), 1);
  const rows = await page.locator('.flow-row').allInnerTexts();
  const order = rows.map((r) => r.split('\n')[0]).filter((name) => ['懸掛', '休息', '伏地挺身'].includes(name));
  assert.equal(order.length, 3);
  // 標籤庫、釘選
  await page.locator('[data-action=tags-open]').click();
  await page.getByRole('button', { name: '體能' }).click();
  await page.locator('#panel').getByRole('button', { name: '爆發力' }).click();
  await page.fill('#tag-name', '自訂標籤');
  await page.getByRole('button', { name: '加入', exact: true }).click();
  await page.locator('#panel [data-action=back]').click();
  await page.locator('.editor-tags').getByText('爆發力').waitFor();
  await page.locator('.editor-tags').getByText('自訂標籤').waitFor();
  await page.locator('[data-action=pin]').click();

  // 有修改時取消要先確認
  await page.locator('[data-action=close]').click();
  await page.getByText('要放棄這次修改嗎？').waitFor();
  await button('繼續編輯').click();
  assert.equal(await page.locator('#sheet[open] .editor').count(), 1);
  await page.locator('[data-action=save]').click();

  // 全螢幕顏色會跟著步驟改變，次數步驟要按完成
  await page.locator('.routine', { hasText: '爆發力' }).waitFor();
  await page.locator('.routine', { hasText: '測試課表' }).getByRole('button', { name: '開始' }).click();
  await page.locator('.run.tone-blue').waitFor();
  await page.locator('.run-next.tone-red').waitFor();
  await page.screenshot({ path: `${shots}/run.png` });
  // 依編輯器的實際順序跑兩輪，休息在第二輪略過；懸掛結束時要記錄
  const tones = { 懸掛: 'red', 休息: 'green', 伏地挺身: 'blue' };
  const steps = [...order, ...order.filter((name) => name !== '休息')];
  let needLog = false;
  let logs = 0;
  for (const name of steps) {
    await page.locator(`.run.tone-${tones[name]} .run-main h1`, { hasText: name }).waitFor({ timeout: 6000 });
    if (needLog) {
      await page.locator('.run-log').waitFor({ timeout: 3000 });
      if (logs === 0) {
        await page.fill('.run-log input[name=m0]', '5');
        await page.getByLabel('力竭').check();
      } else {
        assert.equal(await page.inputValue('.run-log input[name=m0]'), '5');
      }
      await page.getByRole('button', { name: '確認紀錄' }).click();
      logs += 1;
      needLog = false;
    }
    if (name === '伏地挺身') await page.locator('.run [data-run=done]').click();
    if (name === '懸掛') needLog = true;
  }
  assert.ok(logs >= 1);
  await page.getByText('已記錄運動').waitFor();

  // 中斷續跑
  await page.locator('.routine', { hasText: '測試課表' }).getByRole('button', { name: '開始' }).click();
  await page.locator('.run').waitFor();
  await page.reload();
  await page.getByText('有未完成的訓練').waitFor();
  await page.getByRole('button', { name: '繼續' }).click();
  await page.locator('.run').waitFor();
  await page.locator('.run [data-run=stop]').click();
  await page.locator('.run [data-run=discard]').click();
  assert.equal(await page.getByText('有未完成的訓練').count(), 0);

  // 範本和健康管理互通
  await button('＋ 新增課表').click();
  await page.getByRole('button', { name: '＋ 7/3 手指耐力' }).click();
  await page.locator('.routine', { hasText: '7/3 手指耐力' }).waitFor();
  await button('＋ 新增課表').click();
  await page.getByRole('button', { name: '＋ 7/3 手指耐力' }).click();
  await page.getByText('已經有這份課表了').waitFor();
  await page.locator('#panel [data-action=back]').click();
  assert.equal(await page.locator('.routine', { hasText: '7/3 手指耐力' }).count(), 1);

  // 從連結進來時先點一下才開始，手機才能播放提示音
  const routineId = await page.locator('.routine', { hasText: '7/3 手指耐力' }).locator('[data-action=start]').getAttribute('data-id');
  await page.goto(`${timerUrl}?run=${routineId}`);
  await page.locator('#panel [data-action=start-now]').click();
  await page.locator('.run').waitFor();
  await page.locator('.run [data-run=stop]').click();
  await page.locator('.run [data-run=discard]').click();
  await page.goto(timerUrl);
  await page.screenshot({ path: `${shots}/timer-page.png` });
  // 計時器入口只有課表，沒有分頁，也不能建立只記組數的課表
  assert.equal(await page.locator('.train-tabs').count(), 0);
  await page.getByRole('heading', { name: '訓練計時' }).waitFor();
  await button('＋ 新增課表').click();
  assert.equal(await page.getByText('只記組數').count(), 0);
  await page.locator('#panel [data-action=back]').click();

  // 訓練 App 看到同一份課表，並可以使用四個分頁
  await page.goto(`${process.env.PW_URL.replace('/Health', '/Training')}/index.html`);
  await page.locator('.routine', { hasText: '測試課表' }).waitFor();
  assert.equal(await page.locator('.train-tabs button').count(), 4);
  await page.goto(`${process.env.PW_URL}/index.html`);
  await page.locator('.tabs').getByRole('button', { name: '運動', exact: true }).click();
  await page.locator('.entry', { hasText: '測試課表' }).waitFor();
  assert.match(await page.locator('.tabs').innerText(), /運動/);
  await page.locator('.tabs').getByRole('button', { name: '今日', exact: true }).click();
  assert.match(await page.locator('.tracker-row[aria-label="活動"] .mark.today').getAttribute('class'), /done/);
  assert.deepEqual(errors, []);
  console.log('TIMER E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
