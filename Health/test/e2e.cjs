// 手機尺寸端對端測試：node test/e2e.cjs（需先在 Health/ 起 http.server 8123）
const { chromium, devices } = require('playwright');
const assert = require('assert');
const path = require('path');
(async () => {
  const browser = await chromium.launch({ channel: process.env.PW_CHANNEL });
  const ctx = await browser.newContext({ ...devices['iPhone 13'], serviceWorkers: 'block' });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  let aiBody = null;
  await page.route('https://api.anthropic.com/**', async (route) => {
    if (route.request().method() === 'OPTIONS') {
      return route.fulfill({ status: 204, headers: { 'access-control-allow-origin': '*', 'access-control-allow-headers': '*', 'access-control-allow-methods': '*' } });
    }
    aiBody = route.request().postDataJSON();
    await route.fulfill({ status: 200, contentType: 'application/json', headers: { 'access-control-allow-origin': '*' }, body: JSON.stringify({
      id: 'm', type: 'message', role: 'assistant', model: 'claude-opus-5-5', stop_reason: 'end_turn', usage: { input_tokens: 1, output_tokens: 1 },
      content: [{ type: 'text', text: JSON.stringify({ is_food: true, confidence: 'medium', items: [{ name: '<b>雞腿</b>便當', portion: '1 份' }], nutrients: { kcal: 750, carbs_g: 90, fat_g: 25, protein_g: 38, water_ml: 100, caffeine_mg: 0 } }) }],
    }) });
  });
  await page.goto('http://localhost:8123/index.html');
  await page.getByRole('button', { name: /喝水/ }).click();
  await page.getByRole('button', { name: /咖啡/ }).click();
  await page.getByText('250 ml').first().waitFor();

  // 設定金鑰後走拍照流程
  await page.getByRole('button', { name: '設定' }).click();
  await page.fill('input[name=apiKey]', 'sk-ant-test');
  await page.getByRole('button', { name: '儲存設定' }).click();
  await page.setInputFiles('#photo-input', path.join(__dirname, '..', 'icons', 'icon-512.png'));
  await page.getByText('可信度：中').waitFor({ timeout: 15000 }).catch(async (e) => { throw new Error('狀態：' + await page.locator('#ai-status').textContent()); });
  assert.equal(await page.inputValue('input[name=kcal]'), '750');
  assert.equal(await page.locator('#sheet b').count(), 0, 'AI 文字必須被跳脫');
  assert.equal(aiBody.fallbacks, 'default');
  assert.equal(aiBody.messages[0].content[0].type, 'image');
  await page.getByRole('button', { name: '儲存', exact: true }).click();
  await page.getByText('熱量 750 / 2000').waitFor();

  // 飲食頁：其餘兩餐標記沒吃，今日應達成
  await page.getByRole('button', { name: '飲食', exact: true }).click();
  for (const slot of ['早餐', '午餐', '晚餐']) {
    await page.getByRole('button', { name: `新增${slot}` }).click();
    const skip = page.getByRole('button', { name: '沒吃', exact: true });
    if (await skip.count()) await skip.click();
    else await page.getByRole('button', { name: '關閉' }).click();
  }
  await page.getByRole('button', { name: '今日', exact: true }).click();
  assert.equal(await page.locator('.slot-pill.done:not(.move)').count(), 3);

  // 運動
  await page.getByRole('button', { name: '動', exact: true }).click();
  await page.getByRole('button', { name: '重訓' }).click();
  await page.fill('input[name=minutes]', '45');
  await page.getByRole('button', { name: '儲存', exact: true }).click();
  await page.locator('.slot-pill.move.done').waitFor();
  await page.getByRole('button', { name: '運動', exact: true }).click();
  await page.getByText('45 分鐘', { exact: true }).first().waitFor();
  await page.screenshot({ path: process.argv[2] + '/workout.png' });

  // 重新整理後資料仍在
  await page.reload();
  await page.getByText('熱量 750 / 2000').waitFor();
  const overflowHome = await page.evaluate(() => document.documentElement.scrollWidth > innerWidth);
  assert.equal(overflowHome, false, '不可有橫向捲動');
  await page.screenshot({ path: process.argv[2] + '/home.png' });
  await page.getByRole('button', { name: '月曆', exact: true }).click();
  assert.equal(await page.locator('.dot.diet.done').count() >= 1, true);
  await page.screenshot({ path: process.argv[2] + '/calendar.png' });
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > innerWidth);
  assert.equal(overflow, false, '不可有橫向捲動');
  await page.getByRole('button', { name: '飲食', exact: true }).click();
  await page.screenshot({ path: process.argv[2] + '/diet.png' });
  assert.deepEqual(errors, []);
  console.log('E2E OK');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
