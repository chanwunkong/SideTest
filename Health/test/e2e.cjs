// 手機尺寸端對端測試：node test/e2e.cjs <截圖資料夾>（需先在 Health/ 起 http.server，網址用 PW_URL）
const { chromium, devices } = require('playwright');
const assert = require('assert');

const PASTE = `好的，結果如下：
\`\`\`json
{"format":"health-meal-v1","items":[{"name":"雞腿便當","portion":"1 份"}],"nutrients":{"water_ml":100,"protein_g":38,"carbs_g":90,"fat_g":25,"kcal":750,"caffeine_mg":0},"confidence":"medium"}
\`\`\``;

(async () => {
  const shots = process.argv[2];
  const browser = await chromium.launch({ channel: process.env.PW_CHANNEL });
  const ctx = await browser.newContext({ ...devices['iPhone 13'], serviceWorkers: 'block' });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const button = (name, exact = true) => page.getByRole('button', { name, exact });
  const save = () => button('儲存').click();
  const card = (name) => page.locator('.health-card', { hasText: name }).first();
  const row = (name) => page.locator(`.tracker-row[aria-label="${name}"]`);
  const closeSheet = () => page.locator('#sheet').getByRole('button', { name: '關閉' }).click();

  const y = new Date(Date.now() - 86400000);
  const yesterday = `${y.getFullYear()}-${String(y.getMonth() + 1).padStart(2, '0')}-${String(y.getDate()).padStart(2, '0')}`;
  const day = (d) => `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
  const now = new Date();
  const monday = new Date(now.getFullYear(), now.getMonth(), now.getDate() - ((now.getDay() + 6) % 7));
  const week = Array.from({ length: 7 }, (_, i) => day(new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() + i)));
  await page.goto(`${process.env.PW_URL}/index.html`);

  // 打開就是總覽，不問問題
  await page.getByText('生理功能總覽').waitFor();
  assert.equal(await page.locator('.health-card').count(), 4);
  assert.equal(await row('飲食').locator('.mark').count(), 7);
  assert.equal(await page.locator('.tracker-row[aria-label]').count(), 4);
  assert.equal(await page.locator('#sheet[open]').count(), 0);
  await page.screenshot({ path: `${shots}/home-empty.png` });

  // 飲食卡：水量每次自由輸入，之後出現最近份量
  await card('飲食').click();
  await page.fill('#water-input', '330');
  await button('加入水分').click();
  await button('330 ml').waitFor();
  await page.screenshot({ path: `${shots}/diet-card.png` });

  // Gemini 貼上
  await button('貼上 Gemini').click();
  await page.fill('textarea[name=text]', PASTE);
  await button('確認').click();
  await save();
  await card('飲食').getByText('38', { exact: true }).waitFor();
  await card('飲食').getByText('430', { exact: true }).waitFor();
  assert.equal(await card('飲食').locator('.card-icon.on').count(), 1);
  assert.equal(await row('飲食').locator('.mark.done').count(), 1);

  // 身體組成：沒做到的卡片維持灰色
  assert.equal(await card('身體組成').locator('.card-icon.on').count(), 0);
  await card('身體組成').click();
  await button('記錄身體數據').click();
  await page.fill('input[name=weightKg]', '70');
  await page.fill('input[name=bodyFat]', '18');
  await page.fill('input[name=muscleKg]', '32.5');
  await save();
  await card('身體組成').getByText('32.5').waitFor();

  // 只量體脂也能存；舊日期的體重繼續顯示並標示幾天前
  await card('身體組成').click();
  await button('記錄身體數據').click();
  await page.fill('input[name=bodyFat]', '17.5');
  await page.fill('input[name=date]', yesterday);
  await save();
  await card('身體組成').getByText('18', { exact: true }).waitFor();
  assert.equal(await page.locator('.tracker-row[aria-label="身體組成"] .mark.done').count() >= 1, true);
  assert.equal(await page.locator('.tracker-row[aria-label="飲食"] .mark.missed').count(), 0);

  // 睡眠：設定條件後，沒達到就不算做到
  await card('睡眠').click();
  await button('記錄睡眠').click();
  await page.fill('input[name=hours]', '6.5');
  await save();
  await card('睡眠').getByText('6.5').waitFor();
  assert.equal(await row('睡眠').locator('.mark.done').count(), 1);
  await card('睡眠').click();
  await button('卡片設定').click();
  await page.fill('input[name=ruleValue]', '7');
  await save();
  assert.equal(await row('睡眠').locator('.mark.done').count(), 0);

  // 活動
  await card('活動').click();
  await button('記錄其他運動').click();
  await page.fill('input[name=minutes]', '45');
  await save();
  await card('活動').getByText('45', { exact: true }).waitFor();

  // 點表格的前一天：卡片切換成那天，身體數值顯示當時最新，也能補記
  await page.locator('.tracker-row[aria-label="睡眠"] .mark').nth(week.indexOf(yesterday)).click();
  await page.getByText('回到今天').waitFor();
  await card('身體組成').getByText('17.5').waitFor();
  assert.equal(await card('飲食').getByText('—').count(), 3);
  await card('睡眠').click();
  await button('記錄睡眠').click();
  assert.equal(await page.inputValue('input[name=date]'), yesterday);
  await page.fill('input[name=hours]', '8');
  await save();
  await card('睡眠').getByText(/^8 小時/).waitFor();
  await page.getByText('回到今天').click();
  await card('身體組成').getByText('18', { exact: true }).waitFor();

  // 週月一起切換
  await button('月').click();
  const days = new Date(new Date().getFullYear(), new Date().getMonth() + 1, 0).getDate();
  assert.equal(await row('活動').locator('.mark').count(), days);
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, '月表格不可橫向捲動');
  await page.screenshot({ path: `${shots}/home-month.png` });
  await button('週').click();

  // 編輯：排序、新增、自訂項目、移除
  await button('編輯').click();
  await page.locator('.edit-row', { hasText: '活動' }).getByRole('button', { name: '往前' }).click();
  await button('＋ 身體組成').click();
  assert.equal(await page.locator('.edit-row').count(), 5);
  await closeSheet();
  assert.equal(await page.locator('.tracker-row[aria-label]').count(), 5);
  assert.equal(await page.locator('.health-card').nth(2).locator('.card-head').innerText(), '活動');
  await page.locator('.health-card').nth(4).click();
  await button('卡片設定').click();
  await button('移除這張卡片').click();
  assert.equal(await page.locator('.health-card').count(), 4);

  await page.screenshot({ path: `${shots}/home.png` });

  // 點卡片看整月，並能補記過去日期
  await card('活動').click();
  assert.equal(await page.locator('.cal-day.done').count() >= 1, true);
  await page.screenshot({ path: `${shots}/card-detail.png` });
  await closeSheet();

  // 重新整理後設定與資料都還在
  await page.reload();
  await card('身體組成').getByText('32.5').waitFor();
  assert.equal(await page.locator('.health-card').nth(2).locator('.card-head').innerText(), '活動');
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, '不可有橫向捲動');
  assert.deepEqual(errors, []);
  console.log('E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
