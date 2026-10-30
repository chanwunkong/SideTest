// 飲食分析、Cindy 排程與語言切換：node test/training.e2e.cjs <截圖資料夾>
const { chromium, devices } = require('playwright');
const assert = require('assert');

const meal = (protein) => JSON.stringify({ format: 'health-meal-v1', items: [{ name: '便當', portion: '1 份' }], nutrients: { water_ml: 0, protein_g: protein, carbs_g: 50, fat_g: 10, kcal: 500, caffeine_mg: 0 }, confidence: 'high' });

(async () => {
  const shots = process.argv[2];
  const browser = await chromium.launch({ channel: process.env.PW_CHANNEL });
  const ctx = await browser.newContext({ ...devices['iPhone 13'], serviceWorkers: 'block', locale: 'zh-TW' });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const button = (name, exact = true) => page.getByRole('button', { name, exact });
  const tab = (name) => page.locator('.tabs').getByRole('button', { name, exact: true }).click();
  const save = () => button('儲存').click();
  await page.goto(`${process.env.PW_URL}/index.html`);

  // 貼上時自動讀取，直接進確認畫面
  await tab('飲食');
  await button('貼上 Gemini 結果').click();
  await page.evaluate((text) => {
    const area = document.querySelector('#paste-text');
    const event = new Event('paste', { bubbles: true, cancelable: true });
    event.clipboardData = { getData: () => text };
    area.dispatchEvent(event);
  }, meal(40));
  await page.getByText('可信度：高').waitFor();
  await save();

  // 補記昨天，7 天平均要算進去；目標由使用者設定
  await button('‹').click();
  await button('手動輸入').click();
  await page.fill('input[name=protein_g]', '20');
  await save();
  await button('›').click();
  const row = page.locator('.nutrient-row', { hasText: '蛋白質' }).first();
  assert.deepEqual((await row.innerText()).split(/\s+/).slice(1, 4), ['40', '30', '—']);
  await button('設定目標').click();
  await page.fill('input[name=min-protein_g]', '140');
  await page.fill('input[name=min-kcal]', '2300');
  await page.fill('input[name=max-kcal]', '2700');
  await page.fill('input[name=max-caffeine_mg]', '400');
  await save();
  assert.deepEqual((await row.innerText()).split(/\s+/).slice(1, 4), ['40', '30', '≥140']);
  await page.locator('.nutrient-row', { hasText: '熱量' }).getByText('2300–2700').waitFor();

  // 飲食卡預設有紀錄就亮，開啟全部達成後未達標變空框
  await tab('今日');
  const food = page.locator('.tracker-row[aria-label="飲食"] .mark.today');
  assert.match(await food.getAttribute('class'), /done/);
  await page.locator('.health-card', { hasText: '飲食' }).click();
  await button('卡片設定').click();
  await page.getByLabel('飲食目標全部達成才亮').check();
  await save();
  assert.match(await food.getAttribute('class'), /missed/);
  await tab('飲食');
  await page.screenshot({ path: `${shots}/diet.png`, fullPage: true });

  // 建立 Cindy
  await tab('運動');
  await page.locator('.card', { hasText: '排程' }).getByRole('button', { name: /新增/ }).click();
  await page.fill('input[name=name]', 'Cindy');
  await page.fill('input[name=move-name-0]', '引體向上');
  await page.fill('input[name=move-reps-0]', '5');
  await button(/加入動作/, false).click();
  assert.equal(await page.inputValue('input[name=move-name-0]'), '引體向上');
  await page.fill('input[name=move-name-1]', '伏地挺身');
  await page.fill('input[name=move-reps-1]', '10');
  await button(/加入動作/, false).click();
  await page.fill('input[name=move-name-2]', '深蹲');
  await page.fill('input[name=move-reps-2]', '15');
  await save();

  // 排程每天 30 天，目標 5 套
  await page.locator('.card', { hasText: '排程' }).getByRole('button', { name: /新增/ }).click();
  await page.fill('input[name=days]', '30');
  await page.fill('input[name=targetSets]', '5');
  await save();
  await page.getByText('還有 30 天').waitFor();
  await page.screenshot({ path: `${shots}/workout.png`, fullPage: true });

  // 首頁今天是空框
  await tab('今日');
  const activity = page.locator('.tracker-row[aria-label="活動"] .mark.today');
  assert.match(await activity.getAttribute('class'), /missed/);

  // 從活動卡記錄：未達目標仍是空框，達標才亮
  await page.locator('.health-card', { hasText: '活動' }).click();
  await button('Cindy').click();
  await page.fill('input[name=sets]', '4');
  await page.getByText('記錄更多細節').click();
  await page.fill('input[name=extra-0]', '3');
  await save();
  await page.getByText('引體向上 23').waitFor();
  assert.match(await activity.getAttribute('class'), /missed/);
  await tab('運動');
  await page.locator('.plan-row', { hasText: 'Cindy' }).first().getByRole('button', { name: '完成' }).click();
  await page.fill('input[name=sets]', '2');
  await save();
  await tab('今日');
  assert.match(await activity.getAttribute('class'), /done/);
  await page.locator('.health-card', { hasText: '活動' }).getByText('6', { exact: true }).waitFor();

  // 修改訓練不影響舊紀錄
  await tab('運動');
  await page.locator('.card', { hasText: '訓練天數' }).getByText('6', { exact: true }).waitFor();
  await page.locator('.plan-row', { hasText: '累積套數' }).click();
  await page.fill('input[name=move-reps-0]', '7');
  await save();
  await page.locator('.entry', { hasText: 'Cindy' }).last().click();
  await button('記錄更多細節', false).waitFor().catch(() => {});
  await page.locator('#sheet').getByRole('button', { name: '關閉' }).click();

  // 語言切換與重新整理
  await tab('今日');
  await button('設定').click();
  await button('English').click();
  await page.locator('.tabs').getByRole('button', { name: 'Training', exact: true }).click();
  await page.getByText('Schedules').waitFor();
  await page.reload();
  await page.locator('.tabs').getByRole('button', { name: 'Food', exact: true }).waitFor();
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  assert.deepEqual(errors, []);
  console.log('TRAINING E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
