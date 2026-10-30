// 計時流程：node Health/test/timer.e2e.cjs <截圖資料夾>（在 repo 根目錄起 server，PW_URL 指向 /Health）
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
  const tab = (name) => page.locator('.tabs').getByRole('button', { name, exact: true }).click();
  await page.goto(`${process.env.PW_URL}/index.html`);

  // 在訓練裡加入計時流程：循環 2 次（懸掛 1 秒、休息 1 秒），再做 5 下
  await tab('運動');
  await page.locator('.card', { hasText: '訓練天數' }).waitFor();
  await page.locator('.card', { hasText: '先新增一個訓練' }).getByRole('button', { name: /新增/ }).click();
  await page.fill('input[name=name]', '混合訓練');
  await page.fill('input[name=move-name-0]', '懸吊');
  await page.getByText('計時流程（選填）').click();
  await page.locator('#flow-editor > .flow-add').getByRole('button', { name: '＋ 循環' }).click();
  await page.fill('[data-path="0"][data-field="iterations"]', '2');
  await page.locator('.flow-step.loop .flow-add').getByRole('button', { name: '＋ 動作計時' }).click();
  await page.fill('[data-path="0.0"][data-field="label"]', '懸吊');
  await page.fill('[data-path="0.0"][data-field="duration"]', '1');
  await page.locator('.flow-step.loop .flow-add').getByRole('button', { name: '＋ 休息' }).click();
  await page.fill('[data-path="0.1"][data-field="duration"]', '1');
  await page.locator('#flow-editor > .flow-add').getByRole('button', { name: '＋ 次數' }).click();
  await page.fill('[data-path="1"][data-field="label"]', '伏地挺身');
  await page.fill('[data-path="1"][data-field="count"]', '5');
  assert.equal(await page.inputValue('input[name=name]'), '混合訓練');
  await button('儲存').click();

  // 跑計時器：計時步驟自動前進，次數步驟要按完成
  await page.locator('.plan-row', { hasText: '混合訓練' }).getByRole('button', { name: '開始' }).click();
  await page.locator('.flow-clock').waitFor();
  await page.screenshot({ path: `${shots}/timer-run.png` });
  await button('完成').waitFor({ timeout: 10000 });
  assert.equal(await page.locator('.flow-clock').innerText(), '5');
  await button('完成').click();
  await page.getByText('已記錄運動').waitFor();

  // 紀錄、分析和首頁都連動
  await page.locator('.entry', { hasText: '混合訓練' }).waitFor();
  await page.locator('.stat-row', { hasText: '伏地挺身' }).getByText('5 下').waitFor();
  await page.screenshot({ path: `${shots}/analysis.png`, fullPage: true });
  await tab('今日');
  assert.match(await page.locator('.tracker-row[aria-label="活動"] .mark.today').getAttribute('class'), /done/);

  // 獨立網頁看到同一個訓練，中途結束可保存
  await page.goto(`${process.env.PW_URL.replace('/Health', '/Timer')}/index.html`);
  await page.getByText('訓練計時').first().waitFor();
  await page.locator('.plan-row', { hasText: '混合訓練' }).getByRole('button', { name: '開始' }).click();
  await button('結束').click();
  await button('保存').click();
  await page.getByText('已記錄運動').waitFor();
  await button('＋ 懸吊 7/3').click();
  await page.locator('.plan-row', { hasText: '懸吊 7/3' }).waitFor();
  await page.screenshot({ path: `${shots}/timer-page.png` });
  await page.goto(`${process.env.PW_URL}/index.html`);
  await tab('運動');
  assert.equal(await page.locator('.entry', { hasText: '混合訓練' }).count(), 2);
  assert.deepEqual(errors, []);
  console.log('TIMER E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
