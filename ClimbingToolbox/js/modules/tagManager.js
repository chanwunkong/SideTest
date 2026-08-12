// --- js/modules/tagManager.js ---
import { escapeHtml } from './storage.js';

const HISTORY_KEY = 'used_tags_history';
const HISTORY_LIMIT = 30;

export const tagManager = {
    data: {}, // 存放所有標籤資料 (JSON + 歷史)

    async init() {
        try {
            // 1. 讀取靜態 JSON 資料
            const response = await fetch('data/tags.json');
            if (!response.ok) throw new Error('Network response was not ok');
            const jsonData = await response.json();

            // 2. 合併靜態與動態歷史資料
            this.data = { ...jsonData };
            this.updateHistoryData(); // 初始載入歷史標籤

            // 3. 預設渲染第一個分類 (此時已是 'history')
            this.renderTags('history');
        } catch (error) {
            console.error('Failed to load tags:', error);
            const container = document.getElementById('tag-library-content');
            if (container) container.innerHTML = '<span class="text-sm text-red-500">無法載入標籤資料</span>';
        }
    },

    // 取得當前激活的分頁標籤名稱
    getActiveTab() {
        const activeTab = document.querySelector('#tag-library-panel .tab-btn.text-blue-600');
        return activeTab ? activeTab.dataset.value : null;
    },

    // 載入本地存儲的歷史標籤，並更新到 data
    updateHistoryData() {
        let historyTags = [];
        try {
            const stored = localStorage.getItem(HISTORY_KEY);
            if (stored) {
                historyTags = JSON.parse(stored);
            }
        } catch (e) {
            console.error("Failed to parse history tags", e);
            historyTags = [];
        }
        // 將歷史標籤注入到數據集中，並確保它是陣列且去重，限制數量
        this.data['history'] = [...new Set(historyTags)].slice(0, HISTORY_LIMIT);
    },

    // 保存新使用的標籤到歷史紀錄
    saveToHistory(tag) {
        if (!tag) return;
        try {
            let historyTags = JSON.parse(localStorage.getItem(HISTORY_KEY) || '[]');
            // 移除已存在的，並將新的放在最前面
            historyTags = historyTags.filter(t => t !== tag);
            historyTags.unshift(tag);
            historyTags = historyTags.slice(0, HISTORY_LIMIT);
            localStorage.setItem(HISTORY_KEY, JSON.stringify(historyTags));
        } catch (e) {
            console.error("Save history tag error", e);
        }
    },

    renderTags(categoryKey) {
        // 歷史分頁每次渲染前都先取最新資料，確保剛新增的標籤能立即顯示
        if (categoryKey === 'history') this.updateHistoryData();

        const container = document.getElementById('tag-library-content');
        if (!container) return;

        const tags = this.data[categoryKey] || [];

        if (categoryKey === 'history' && tags.length === 0) {
            container.innerHTML = '<span class="text-xs text-gray-400 p-2">暫無歷史標籤紀錄</span>';
            return;
        }

        container.innerHTML = tags.map(tag => `
        <button type="button"
                data-action="editor-add-tag"
                data-value="${escapeHtml(tag)}"
                class="text-[10px] px-2.5 py-1.5 bg-white dark:bg-gray-700 border border-gray-200 dark:border-gray-600 rounded-md shadow-sm hover:bg-blue-50 dark:hover:bg-blue-900/40 text-gray-700 dark:text-gray-200 transition-colors active:scale-95">
            ${escapeHtml(tag)}
        </button>
    `).join('');
    },

    // 標籤成功加入課表後呼叫：存入歷史紀錄，並在正顯示歷史分頁時即時刷新
    confirmTag(tag) {
        if (!tag) return;
        this.saveToHistory(tag);
        if (this.getActiveTab() === 'history') {
            this.renderTags('history');
        }
    }
};
