// 獨立訓練計時：只有計時功能，資料和健康管理共用
import { FLOW_TEMPLATES, flattenFlow } from '../core/flow.js';
import { attachFlowEditor, flowSummary, renderFlowEditor, runFlow } from '../core/flowUi.js';
import { lang, t } from '../core/i18n.js';
import { healthStore as store } from '../core/store.js';

const view = document.getElementById('view');
const sheet = document.getElementById('sheet');
const toastEl = document.getElementById('toast');
const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
let draft = null;

function toast(message) {
  toastEl.hidden = false;
  toastEl.textContent = message;
  setTimeout(() => { toastEl.hidden = true; }, 3000);
}

const flows = () => store.activeDocs('trainings').filter((training) => training.flow?.length);

function render() {
  document.documentElement.lang = lang;
  const rows = flows().map((training) => `<div class="row plan-row">
    <button class="edit-name" data-action="edit" data-id="${esc(training.id)}">${esc(training.name)}<span class="muted">${flowSummary(training.flow)}・${flattenFlow(training.flow).length} ${t('step.steps')}</span></button>
    <button class="btn primary" data-action="start" data-id="${esc(training.id)}">${t('step.start')}</button>
  </div>`).join('');
  const examples = Object.keys(FLOW_TEMPLATES).map((key) => `<button class="chip" data-action="template" data-key="${key}">＋ ${t(`template.${key}`)}</button>`).join('');
  view.innerHTML = `
    <header class="topbar"><h1>${t('timer.title')}</h1><a class="plain" href="../Health/">${t('timer.health')}</a></header>
    <button class="btn primary wide meal-actions" data-action="edit">${t('timer.new')}</button>
    <section class="card">${rows || `<p class="muted">${t('timer.empty')}</p>`}</section>
    <section class="card stack"><h2>${t('timer.examples')}</h2><div class="chips">${examples}</div></section>
    <p class="muted">${store.user ? t('account.signedIn', { email: esc(store.user.email) }) : t('account.signedOut')}</p>
    ${store.user || !store.canSignIn ? '' : `<button class="btn wide" data-action="sign-in">${t('account.signIn')}</button>`}`;
}

function renderSheet() {
  sheet.innerHTML = `<form class="stack">
    <div class="row"><h2 style="margin:0">${t(draft.id ? 'common.edit' : 'timer.new')}</h2><button type="button" class="icon-btn" data-action="close" aria-label="${t('common.close')}">✕</button></div>
    <label class="field"><span>${t('training.name')}</span><input name="name" value="${esc(draft.name)}" required></label>
    ${renderFlowEditor(draft.flow)}
    <button class="btn primary wide">${t('common.save')}</button>
    ${draft.id ? `<button type="button" class="btn danger wide" data-action="delete">${t('common.delete')}</button>` : ''}
  </form>`;
  sheet.querySelector('[name=name]').addEventListener('input', (event) => { draft.name = event.target.value; });
  if (!sheet.open) sheet.showModal();
}

attachFlowEditor(sheet, () => draft.flow, renderSheet);

const actions = {
  edit: ({ id }) => {
    const training = store.trainings.find((x) => x.id === id);
    draft = training ? structuredClone(training) : { name: '', mode: 'sets', moves: [], trackRest: false, flow: [] };
    renderSheet();
  },
  close: () => sheet.close(),
  delete: () => { store.saveDoc('trainings', { ...draft, deletedAt: Date.now() }); sheet.close(); },
  template: ({ key }) => {
    store.saveDoc('trainings', { mode: 'sets', moves: [], trackRest: false, ...FLOW_TEMPLATES[key](t) });
    toast(t('common.saved'));
  },
  start: ({ id }) => runFlow(store.trainings.find((x) => x.id === id), {
    onFinish: (record) => {
      store.addEvent('workout', record);
      toast(t('workout.saved'));
    },
  }),
  'sign-in': () => store.signIn().catch((error) => toast(t('account.signInFailed', { error: error.message }))),
};

document.addEventListener('click', (event) => {
  const target = event.target.closest('[data-action]');
  if (target) actions[target.dataset.action](target.dataset);
});

sheet.addEventListener('submit', (event) => {
  event.preventDefault();
  store.saveDoc('trainings', draft);
  sheet.close();
});

store.addEventListener('change', render);
store.addEventListener('error', (event) => toast(t('error.save', { error: event.detail.message })));
render();
store.init().catch((error) => toast(t('error.start', { error: error.message })));
