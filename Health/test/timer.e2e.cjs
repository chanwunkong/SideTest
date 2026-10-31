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
  await button('＋ 新增課表').click();
  await page.fill('#routine-name', '測試課表');
  const addTimer = async (label, seconds, color) => {
    await page.locator('.editor-foot .btn.primary').click();
    await button('計時').click();
    await page.fill('input[name=label]', label);
    await page.fill('input[name=duration]', String(seconds));
    await page.locator(`.swatch.tone-${color}`).click();
    await button('儲存').click();
  };
  await addTimer('準備', 1, 'blue');
  await page.locator('.editor-foot .btn.primary').click();
  await button('循環').click();
  await page.fill('input[name=iterations]', '2');
  await button('儲存').click();
  await page.locator('.editor-foot .btn.primary').click();
  await button('次數').click();
  await page.fill('input[name=label]', '伏地挺身');
  await page.fill('input[name=count]', '5');
  await button('儲存').click();
  await page.locator('.flow-row.loop').click();
  await addTimer('懸掛', 1, 'red');
  await addTimer('休息', 1, 'green');
  await page.locator('.flow-row', { hasText: '休息' }).click();
  await page.getByText('更多選項').click();
  await page.getByLabel('這是休息').check();
  await page.getByLabel('最後一次略過').check();
  await button('儲存').click();
  await page.locator('.flow-row', { hasText: '懸掛' }).click();
  await page.getByText('更多選項').click();
  await page.getByLabel('負重').check();
  await button('儲存').click();
  assert.deepEqual(await page.locator('.flow-row').evaluateAll((rows) => rows.map((r) => r.style.getPropertyValue('--depth'))), ['0', '0', '1', '1', '0']);
  await page.screenshot({ path: `${shots}/editor.png` });

  // 按住拖曳：把次數拖到循環裡
  const source = await page.locator('.flow-row', { hasText: '伏地挺身' }).boundingBox();
  const loopBox = await page.locator('.flow-row.loop').boundingBox();
  await page.mouse.move(source.x + 30, source.y + 20);
  await page.mouse.down();
  await page.waitForTimeout(450);
  await page.mouse.move(loopBox.x + 30, loopBox.y + 20, { steps: 5 });
  await page.mouse.up();
  assert.equal(await page.locator('.flow-row', { hasText: '伏地挺身' }).evaluate((r) => r.style.getPropertyValue('--depth')), '1');
  await button('儲存').click();

  // 全螢幕顏色會跟著步驟改變，次數步驟要按完成
  await page.locator('.routine', { hasText: '測試課表' }).getByRole('button', { name: '開始' }).click();
  await page.locator('.run.tone-blue').waitFor();
  await page.locator('.run-next.tone-red').waitFor();
  await page.screenshot({ path: `${shots}/run.png` });
  await page.locator('.run.tone-red').waitFor({ timeout: 5000 });
  await page.getByText('第 1/2 次').waitFor();
  await page.locator('.run.tone-green').waitFor({ timeout: 5000 });
  await page.locator('.run-log').waitFor();
  await page.fill('.run-log input[name=weightKg]', '5');
  await page.getByLabel('力竭').check();
  await page.getByRole('button', { name: '確認紀錄' }).click();
  await page.locator('.run.tone-blue').waitFor({ timeout: 5000 });
  await page.getByRole('button', { name: '完成' }).click();
  await page.locator('.run.tone-red').waitFor();
  await page.locator('.run-log').waitFor({ state: 'detached', timeout: 3000 }).catch(() => {});
  await page.locator('.run.tone-blue').waitFor({ timeout: 5000 });
  await page.locator('.run-log input[name=weightKg]').waitFor();
  assert.equal(await page.inputValue('.run-log input[name=weightKg]'), '5');
  await page.getByRole('button', { name: '完成' }).click();
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
  await page.getByRole('button', { name: '＋ 7/3 手指耐力' }).click();
  await page.locator('.routine', { hasText: '7/3 手指耐力' }).waitFor();
  await page.screenshot({ path: `${shots}/timer-page.png` });
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
