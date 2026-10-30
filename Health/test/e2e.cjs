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
  const tab = (name) => page.locator('.tabs').getByRole('button', { name, exact: true }).click();

  await page.goto(`${process.env.PW_URL}/index.html`);

  // 今日：空白狀態
  await page.getByText('今天還沒量體重').waitFor();
  await page.getByText('昨晚睡眠尚未記錄').waitFor();
  await page.getByText('今天休息').waitFor();
  await page.screenshot({ path: `${shots}/home-empty.png` });

  // 格式錯誤不能存
  await button('記一餐').click();
  await page.fill('textarea[name=text]', '今天吃得不錯');
  await button('確認').click();
  await page.getByText('找不到資料').waitFor();

  // Gemini 貼上後確認存檔
  await page.fill('textarea[name=text]', PASTE);
  await button('確認').click();
  await page.getByText('可信度：中').waitFor();
  assert.equal(await page.inputValue('input[name=kcal]'), '750');
  await save();
  await page.getByText('蛋白質 38 / 120 g').waitFor();

  await button(/喝水/, false).click();
  await page.getByText('水分 350 / 2500 ml').waitFor();

  // 身體：基本資料、數據、目標，每日標準應改成個人估算
  await tab('身體');
  await page.getByText('通用標準', { exact: false }).first().waitFor();
  await button('基本資料').click();
  await page.fill('input[name=heightCm]', '175');
  await page.fill('input[name=birthYear]', '1996');
  await button('男').click();
  await save();
  await button('記錄數據').click();
  await page.fill('input[name=weightKg]', '70');
  await page.fill('input[name=bodyFat]', '18');
  await save();
  await page.getByText('依你的數據估算', { exact: false }).waitFor();
  await button('設定').click();
  await page.fill('input[name=targetWeightKg]', '68');
  await page.fill('input[name=targetDate]', '2027-03-01');
  await save();
  await page.screenshot({ path: `${shots}/body.png`, fullPage: true });

  // 週課表：今天排定後，今日顯示提醒
  await tab('運動');
  const today = new Date().getDay();
  await page.locator(`.plan-row[data-day="${today}"]`).click();
  await button('攀岩').click();
  await page.fill('input[name=minutes]', '45');
  await save();
  await page.getByText('今天：攀岩 45 分鐘').waitFor();
  await page.screenshot({ path: `${shots}/workout.png`, fullPage: true });
  await tab('今日');
  await page.getByText('今天排定：攀岩 45 分鐘').waitFor();
  await page.screenshot({ path: `${shots}/home.png` });
  await button('完成訓練').click();
  await save();
  await page.getByText('已完成 45 分鐘').waitFor();

  // 休息
  await tab('休息');
  await button('記錄睡眠').click();
  await page.fill('input[name=hours]', '6.5');
  await save();
  await page.getByText('平均').waitFor();
  await page.screenshot({ path: `${shots}/rest.png`, fullPage: true });

  // 重新整理後資料仍在；月曆與飲食
  await page.reload();
  await page.getByText('已完成 45 分鐘').waitFor();
  await page.getByText('昨晚睡 6.5 小時', { exact: false }).waitFor();
  await page.getByRole('button', { name: '月曆' }).click();
  assert.equal(await page.locator('.dot.move.done').count() >= 2, true);
  await page.screenshot({ path: `${shots}/calendar.png`, fullPage: true });
  await tab('飲食');
  await page.getByText('雞腿便當').waitFor();
  await page.screenshot({ path: `${shots}/diet.png`, fullPage: true });
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, '不可有橫向捲動');
  await button('查看 Gemini 提示詞').click();
  await page.getByText('複製提示詞').waitFor();
  assert.deepEqual(errors, []);
  console.log('E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
