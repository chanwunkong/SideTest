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
  const timerUrl = `${process.env.PW_URL.replace('/Health', '/Training')}/index.html`;
  const healthUrl = `${process.env.PW_URL}/index.html`;
  const trainTab = (name) => page.locator('.train-tabs').getByRole('button', { name, exact: true }).click();
  const panelSave = () => page.locator('#panel .btn.primary.wide').click();
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
  // 不用排程也能新增今天的運動
  await tab('運動');
  await button('＋ 新增').click();
  await page.fill('#pick-search', '散步');
  await button('新增「散步」').click();
  await page.fill('input[name=minutes]', '10');
  await save();
  await page.locator('.entry', { hasText: '散步' }).getByText('已完成').waitFor();
  await page.screenshot({ path: `${shots}/workout-today.png`, fullPage: true });
  await page.goto(timerUrl);
  await button('＋ 新增課表').click();
  await button(/只記組數/, false).click();
  await page.fill('#panel input[name=name]', 'Cindy');
  await page.fill('input[name=move-name-0]', '引體向上');
  await page.fill('input[name=move-reps-0]', '5');
  await button(/加入動作/, false).click();
  assert.equal(await page.inputValue('input[name=move-name-0]'), '引體向上');
  await page.fill('input[name=move-name-1]', '伏地挺身');
  await page.fill('input[name=move-reps-1]', '10');
  await button(/加入動作/, false).click();
  await page.fill('input[name=move-name-2]', '深蹲');
  await page.fill('input[name=move-reps-2]', '15');
  await panelSave();

  // 排程每天 30 天，目標 5 組
  await trainTab('排程');
  await button('＋ 新增排程').click();
  await page.fill('#panel input[name=days]', '30');
  await page.fill('#panel input[name=targetSets]', '5');
  await panelSave();
  await page.getByText('還有 30 天').waitFor();
  await page.screenshot({ path: `${shots}/workout.png` });
  await page.goto(healthUrl);

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
  // 課表和動作在同一份清單：今天已記錄的 Cindy 從新增選單再記一次
  await button('＋ 新增').click();
  await page.locator('.pick-row', { hasText: 'Cindy' }).click();
  await page.fill('input[name=sets]', '2');
  await save();
  await tab('今日');
  assert.match(await activity.getAttribute('class'), /done/);
  await page.locator('.health-card', { hasText: '活動' }).getByText('6', { exact: true }).waitFor();

  // 動作自動進入動作庫；建立下層動作後，上層累計包含下層
  await page.goto(timerUrl);
  await trainTab('動作');
  const library = page.locator('.card').first();
  await library.getByRole('button', { name: '深蹲' }).waitFor();
  await button('＋ 新增動作').click();
  await page.fill('#panel input[name=name]', '槓鈴深蹲');
  await page.selectOption('select[name=parentId]', { label: '深蹲' });
  await panelSave();
  await library.getByRole('button', { name: '槓鈴深蹲' }).click();
  await page.getByText('累計次數').waitFor();
  await page.locator('#panel [data-action=back]').click();
  await library.getByRole('button', { name: '深蹲', exact: true }).click();
  await page.locator('.stat-row', { hasText: '累計次數' }).getByText('90').waitFor();
  await page.screenshot({ path: `${shots}/exercise.png` });
  await page.locator('#panel [data-action=back]').click();

  // 動作的預設持續時間會自動帶入，運動紀錄有開始時間
  await button('＋ 新增動作').click();
  await page.fill('#panel input[name=name]', '跑步');
  await page.fill('input[name=defaultMinutes]', '20');
  await panelSave();
  await library.getByRole('button', { name: '跑步' }).waitFor();
  await page.waitForTimeout(200);
  await page.goto(healthUrl);
  await tab('運動');
  await button('＋ 新增').click();
  await page.locator('.pick-row', { hasText: '跑步' }).click();
  assert.equal(await page.inputValue('input[name=minutes]'), '20');
  await page.fill('input[name=startTime]', '07:30');
  await save();
  await page.locator('.entry', { hasText: '跑步' }).getByText('07:30', { exact: false }).waitFor();
  await button('＋ 新增').click();
  await page.fill('#pick-search', '新動作');
  await button('新增「新動作」').click();
  await page.fill('input[name=minutes]', '15');
  // 選重複時才顯示後續欄位，儲存後出現在訓練 App 的排程
  assert.equal(await page.locator('input[name=repeatDays]').isVisible(), false);
  await page.selectOption('select[name=repeat]', 'daily');
  await page.fill('input[name=repeatDays]', '7');
  await save();
  await page.locator('.card', { hasText: '訓練天數' }).getByText('6', { exact: true }).waitFor();
  assert.equal(await page.getByText('動作庫').count(), 0, '運動頁不放動作庫');
  await page.waitForTimeout(300);
  await page.goto(timerUrl);
  await trainTab('動作');
  await page.getByRole('button', { name: '新動作' }).waitFor();
  await trainTab('排程');
  await page.locator('.plan-row', { hasText: '新動作' }).waitFor();
  await trainTab('紀錄');
  await page.getByText('個人最佳').waitFor();

  // 修改課表不影響舊紀錄
  await trainTab('課表');
  await page.locator('.routine', { hasText: 'Cindy' }).locator('.routine-open').click();
  await page.fill('input[name=move-reps-0]', '7');
  await panelSave();
  await page.waitForTimeout(200);
  await page.goto(healthUrl);
  await tab('運動');
  await page.locator('.entry', { hasText: 'Cindy' }).last().click();
  await page.locator('#sheet').getByRole('button', { name: '關閉' }).click();

  // 資料庫連線失效時，表單仍會關閉，畫面更新，並顯示錯誤
  await page.evaluate(() => {
    const original = IDBDatabase.prototype.transaction;
    IDBDatabase.prototype.transaction = function transaction(...args) {
      if (this.name === 'health') throw new Error('The client has already been terminated.');
      return original.apply(this, args);
    };
  });
  await button('＋ 新增').click();
  await page.fill('#pick-search', '失效測試');
  await button('新增「失效測試」').click();
  await page.fill('input[name=minutes]', '5');
  await save();
  assert.equal(await page.locator('#sheet[open]').count(), 0, '儲存要有反應');
  await page.locator('.entry', { hasText: '失效測試' }).click();
  await page.locator('#sheet .btn.danger').click();
  assert.equal(await page.locator('#sheet[open]').count(), 0, '刪除要有反應');
  assert.equal(await page.locator('.entry', { hasText: '失效測試' }).count(), 0);
  await page.reload();

  // 語言切換與重新整理
  await tab('今日');
  await button('設定').click();
  await button('English').click();
  await page.locator('.tabs').getByRole('button', { name: 'Training', exact: true }).click();
  await page.locator('.tabs').getByRole('button', { name: 'Body', exact: true }).click();
  await page.getByText('No body data yet').waitFor();
  await page.locator('.tabs').getByRole('button', { name: 'Rest', exact: true }).click();
  await page.getByText('Log sleep').waitFor();
  await page.locator('.tabs').getByRole('button', { name: 'Today', exact: true }).click();
  await page.getByText('Overview').waitFor();
  const chinese = await page.evaluate(() => document.body.innerText.match(/[\u4e00-\u9fff]/g));
  assert.equal(chinese, null, '英文模式首頁不應有中文');
  await page.locator('.tabs').getByRole('button', { name: 'Training', exact: true }).click();
  await page.reload();
  await page.locator('.tabs').getByRole('button', { name: 'Food', exact: true }).waitFor();
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  assert.deepEqual(errors, []);
  console.log('TRAINING E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
