// --- js/main.js ---
import { store, recordManager, sessionRepository } from './modules/storage.js';
import { editor, router, initSwipeToClose, settingsManager, themeManager } from './modules/ui.js';
import { bleManager } from './modules/bleManager.js';
import { timer, voiceCommander } from './modules/timer.js';
import { goalManager } from './modules/goalManager.js';
import { analyticsManager, analyticsUI, bodyManager, insightManager } from './modules/analytics.js';
import { hrManager } from './modules/hrManager.js';

let tagLibraryData = {}; // 存放所有標籤資料 (JSON + 歷史)

// 取得當前激活的分頁標籤名稱
function getActiveTagTab() {
    const activeTab = document.querySelector('#tag-library-panel .tab-btn.text-blue-600');
    return activeTab ? activeTab.dataset.value : null;
}

// 載入本地存儲的歷史標籤，並更新到 tagLibraryData
function updateHistoryTagsData() {
    let historyTags = [];
    try {
        // 從 localStorage 讀取紀錄 (假設鍵名為 'used_tags_history')
        const stored = localStorage.getItem('used_tags_history');
        if (stored) {
            historyTags = JSON.parse(stored);
        }
    } catch (e) {
        console.error("Failed to parse history tags", e);
        historyTags = [];
    }
    // 將歷史標籤注入到數據集中，並確保它是陣列且去重，限制數量 (例如最近30個)
    tagLibraryData['history'] = [...new Set(historyTags)].slice(0, 30);
}

// 保存新使用的標籤到歷史紀錄
function saveTagToHistory(tag) {
    if (!tag) return;
    try {
        let historyTags = JSON.parse(localStorage.getItem('used_tags_history') || '[]');
        // 移除已存在的，並將新的放在最前面
        historyTags = historyTags.filter(t => t !== tag);
        historyTags.unshift(tag);
        // 限制儲存數量
        historyTags = historyTags.slice(0, 30);
        localStorage.setItem('used_tags_history', JSON.stringify(historyTags));
    } catch (e) {
        console.error("Save history tag error", e);
    }
}

async function loadTagLibrary() {
    try {
        // 1. 讀取靜態 JSON 資料
        const response = await fetch('data/tags.json');
        if (!response.ok) throw new Error('Network response was not ok');
        const jsonData = await response.json();

        // 2. 合併靜態與動態歷史資料
        tagLibraryData = { ...jsonData };
        updateHistoryTagsData(); // 初始載入歷史標籤

        // 3. 預設渲染第一個分類 (此時已是 'history')
        renderTags('history');
    } catch (error) {
        console.error('Failed to load tags:', error);
        const container = document.getElementById('tag-library-content');
        if (container) container.innerHTML = '<span class="text-sm text-red-500">無法載入標籤資料</span>';
    }
}

function renderTags(categoryKey) {
    const container = document.getElementById('tag-library-content');
    if (!container) return;

    const tags = tagLibraryData[categoryKey] || [];

    if (categoryKey === 'history' && tags.length === 0) {
        container.innerHTML = '<span class="text-xs text-gray-400 p-2">暫無歷史標籤紀錄</span>';
        return;
    }

    container.innerHTML = tags.map(tag => `
        <button type="button" 
                data-action="editor-add-tag" 
                data-value="${tag}" 
                class="text-[10px] px-2.5 py-1.5 bg-white dark:bg-gray-700 border border-gray-200 dark:border-gray-600 rounded-md shadow-sm hover:bg-blue-50 dark:hover:bg-blue-900/40 text-gray-700 dark:text-gray-200 transition-colors active:scale-95">
            ${tag}
        </button>
    `).join('');
}

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
    loadTagLibrary();

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

                    // 3. 將此標籤保存到歷史紀錄
                    saveTagToHistory(targetTag);

                    // 4. 若當前分頁是「歷史標籤」，即時重新渲染畫面
                    if (getActiveTagTab() === 'history') {
                        updateHistoryTagsData();
                        renderTags('history');
                    }
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

                //如果是切換到歷史分頁，先更新最新的歷史數據
                if (value === 'history') {
                    updateHistoryTagsData();
                }

                // 重新渲染該分類的標籤
                renderTags(value);
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
                    saveTagToHistory(tagValue);
                    if (getActiveTagTab() === 'history') {
                        updateHistoryTagsData();
                        renderTags('history');
                    }
                    e.target.value = ''; // 清空輸入框
                }
            } else if (e.target.id === 'inp-new-metric') {
                e.preventDefault();
                editor.addMetric();
            }
        }
    });
});