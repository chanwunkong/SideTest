// --- js/main.js ---
import { store, recordManager, sessionRepository } from './modules/storage.js';
import { editor, router, initSwipeToClose, settingsManager, themeManager } from './modules/ui.js';
import { bleManager } from './modules/bleManager.js';
import { timer, voiceCommander } from './modules/timer.js';
import { goalManager } from './modules/goalManager.js';
import { analyticsManager, analyticsUI, bodyManager, insightManager } from './modules/analytics.js';
import { hrManager } from './modules/hrManager.js';
import { tagManager } from './modules/tagManager.js';

document.addEventListener('DOMContentLoaded', () => {
    // 1. 初始化各模組
    store.init();
    editor.init();
    settingsManager.init();
    themeManager.init();
    goalManager.init();
    recordManager.updateUI();
    recordManager.renderCalendar();
    analyticsManager.init();

    // 初始化標籤庫 (JSON + History)
    tagManager.init();

    const now = new Date();
    const y = now.getFullYear();
    const m = (now.getMonth() + 1).toString().padStart(2, '0');
    const d = now.getDate().toString().padStart(2, '0');
    recordManager.showDayDetail(`${y}-${m}-${d}`);

    // 2. 暫存課表檢查
    const sessionJson = localStorage.getItem('active_session');
    if (sessionJson) {
        const session = JSON.parse(sessionJson);
        if (confirm(`發現未完成的訓練「${session.routineTitle}」，是否立即繼續？`)) {
            timer.resumeSession(session);
        }
    }

    // 3. UI 綁定
    initSwipeToClose('quick-log-panel', () => timer.closeLogPanel());
    initSwipeToClose('prop-sheet', () => editor.closeProps());
    initSwipeToClose('detail-sheet', () => recordManager.closeDetail());

    // 4. 全域事件委派 (中央大腦)
    document.body.addEventListener('click', (e) => {
        const btn = e.target.closest('[data-action]');
        if (!btn) return;
        const action = btn.dataset.action;
        const value = btn.dataset.value;

        switch (action) {
            // --- 路由與全域 ---
            case 'route': router.go(value); break;
            case 'session-clear': sessionRepository.clear(); break;
            case 'session-resume': timer.resumeFromStorage(); break;

            // --- 計時器 (Timer) ---
            case 'timer-skip': timer.skip(Number(value)); break;
            case 'timer-toggle': timer.toggle(); break;
            case 'timer-stop': timer.stop(); break;
            case 'timer-suspend': timer.suspend(); break;
            case 'timer-nav-log': timer.navigateLog(Number(value)); break;
            case 'timer-save-log': timer.saveLog(true); break;
            case 'timer-autolog': timer.toggleAutoLog(); break;
            case 'timer-manual-log': timer.openManualLogPanel(); break;
            case 'timer-adjust-log-val': timer.adjustLogVal(Number(btn.dataset.index), Number(value)); break;
            case 'voice-toggle': voiceCommander.toggle(); break;

            // --- 課表清單 (Routines) ---
            case 'routine-start': timer.start(value); break;
            case 'routine-duplicate': store.duplicateRoutine(value); break;
            case 'routine-edit': editor.load(value); break;
            case 'routine-delete': store.deleteRoutine(value); break;
            case 'routine-add-template': store.addTemplate(value); break;

            // --- 編輯器 (Editor) ---
            case 'open-editor': editor.open(); break;
            case 'editor-close': editor.close(); break;
            case 'editor-save': editor.save(); break;
            case 'editor-add-tag':
                let targetTag = value;
                const tagInput = document.getElementById('inp-routine-tag');

                // 若 value 是 undefined，代表用戶是點擊輸入框旁的「新增」按鈕，改由輸入框取值
                if (!targetTag && tagInput) {
                    targetTag = tagInput.value.trim();
                }

                if (targetTag) {
                    // 1. 執行原本的新增邏輯
                    editor.addRoutineTag(targetTag);

                    // 2. 清空輸入框
                    if (tagInput) tagInput.value = '';

                    // 3. 存入歷史紀錄，並在正顯示歷史分頁時即時刷新
                    tagManager.confirmTag(targetTag);
                }
                break;
            case 'editor-remove-tag': editor.removeRoutineTag(value); break;
            case 'editor-add-metric': editor.addMetric(); break;
            case 'editor-remove-metric': editor.removeMetric(Number(value)); break;
            case 'editor-delete-block': editor.deleteCurrentBlock(); break;
            case 'editor-save-props': editor.saveProps(); break;
            case 'editor-close-props': editor.closeProps(); break;
            case 'editor-toggle-tag-lib': document.getElementById('tag-library-panel').classList.toggle('hidden'); break;

            // 標籤分頁切換
            case 'editor-switch-tab':
                const tabContainer = btn.closest('.flex');
                if (!tabContainer) break;

                // 切換按鈕樣式
                tabContainer.querySelectorAll('.tab-btn').forEach(b => {
                    b.classList.remove('text-blue-600', 'border-blue-600', 'dark:text-blue-400');
                    b.classList.add('text-gray-500', 'border-transparent', 'dark:text-gray-400');
                });
                btn.classList.remove('text-gray-500', 'border-transparent', 'dark:text-gray-400');
                btn.classList.add('text-blue-600', 'border-blue-600', 'dark:text-blue-400');

                // 重新渲染該分類的標籤 (若切換到歷史分頁，會自動先刷新最新資料)
                tagManager.renderTags(value);
                break;

            // --- 日曆與紀錄 (Record) ---
            case 'record-toggle-mode': recordManager.toggleCalendarMode(); break;
            case 'record-change-period': recordManager.changePeriod(Number(value)); break;
            case 'record-show-day': recordManager.showDayDetail(value); break;
            case 'record-toggle-logs': recordManager.toggleRecordLogs(value); break;
            case 'record-edit-log': recordManager.editLogEntry(btn.dataset.record, Number(btn.dataset.index)); break;
            case 'record-delete': recordManager.deleteRecord(btn.dataset.record, btn.dataset.date); break;

            // --- 目標管理 (Goal) ---
            case 'open-goal-editor': goalManager.openEditor(); break;
            case 'goal-toggle-show': goalManager.toggleShowAll(); break;
            case 'goal-close': goalManager.closeEditor(); break;
            case 'goal-set-op': goalManager.setOperator(value); break;
            case 'goal-save': goalManager.saveGoal(); break;
            case 'goal-toggle': goalManager.toggleGoal(value, e); break;
            case 'goal-delete': goalManager.deleteGoal(value); break;

            // --- 數據洞察 (Insight) ---
            case 'pr-open-editor': analyticsUI.openPREditor(Number(value)); break;
            case 'pr-select-card': analyticsManager.selectCard(Number(value)); break;
            case 'pr-close': analyticsUI.closePREditor(); break;
            case 'pr-set-op': analyticsUI.setOperator(value); break;
            case 'pr-save': analyticsUI.savePRCard(); break;
            case 'insight-clear-sel': insightManager.clearSelection(value); break;
            case 'insight-toggle-point': insightManager.togglePoint(btn.dataset.period, btn.dataset.logId); break;

            // --- 身體數據 (Body) ---
            case 'body-change-date': bodyManager.changeDate(Number(value)); break;
            case 'body-open-editor': bodyManager.openEditor(); break;
            case 'body-close': bodyManager.closeEditor(); break;
            case 'body-delete': bodyManager.deleteRecord(); break;
            case 'body-save': bodyManager.saveRecord(); break;

            // --- 藍牙連線 (BLE) ---
            case 'ble-toggle': bleManager.toggleConnection(); break;
            case 'hr-toggle': hrManager.toggleConnection(); break;
            case 'ble-target-modal': e.preventDefault(); e.stopPropagation(); bleManager.openTargetModal(); break;
            case 'ble-fallback-close': bleManager.hideFallbackModal(); break;
            case 'ble-fallback-retry': bleManager.requestNewDevice(); break;
            case 'ble-target-adjust': bleManager.adjustTarget(Number(value)); break;
            case 'ble-target-close': bleManager.closeTargetModal(); break;
            case 'ble-target-save': bleManager.saveTargetModal(); break;
        }
    });

    // 處理 <select> 變更事件 (取代原本的 inline onchange)
    document.body.addEventListener('change', (e) => {
        if (e.target.id === 'set-theme-mode') {
            themeManager.setMode(e.target.value);
        } else if (e.target.id === 'goal-scope-type') {
            goalManager.toggleScopeUI();
        }
    });

    // 處理 Enter 鍵輸入
    document.body.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            if (e.target.id === 'inp-routine-tag') {
                e.preventDefault();
                // 用戶手動輸入標籤並按 Enter
                const tagValue = e.target.value.trim();
                if (tagValue) {
                    editor.addRoutineTag(tagValue);
                    // 同樣保存到歷史，並更新 UI
                    tagManager.confirmTag(tagValue);
                    e.target.value = ''; // 清空輸入框
                }
            } else if (e.target.id === 'inp-new-metric') {
                e.preventDefault();
                editor.addMetric();
            }
        } else if (e.key === 'Escape') {
            closeTopmostModal();
        }
    });
});

// 依「由上而下」的疊放順序關閉目前開啟中的彈窗 (Esc 鍵支援)
// 注意：進行中的計時器全螢幕畫面 (modal-active-timer) 不在此清單內，避免誤觸中斷訓練
function closeTopmostModal() {
    const notHidden = (id) => {
        const el = document.getElementById(id);
        return el && !el.classList.contains('hidden');
    };
    const hasOpenClass = (id) => {
        const el = document.getElementById(id);
        return el && el.classList.contains('open');
    };
    const isSlidUp = (id) => {
        const el = document.getElementById(id);
        return el && !el.classList.contains('translate-y-full');
    };

    if (notHidden('modal-ble-target')) return bleManager.closeTargetModal();
    if (isSlidUp('prop-sheet')) return editor.closeProps();
    if (notHidden('modal-ble-fallback')) return bleManager.hideFallbackModal();
    if (hasOpenClass('modal-body-editor')) return bodyManager.closeEditor();
    if (hasOpenClass('modal-pr-editor')) return analyticsUI.closePREditor();
    if (hasOpenClass('modal-goal-editor')) return goalManager.closeEditor();
    if (hasOpenClass('modal-editor')) return editor.close();
}