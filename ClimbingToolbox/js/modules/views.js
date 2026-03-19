// --- js/modules/views.js ---
import { formatTime } from './storage.js';

export const views = {
    /** 訓練紀錄卡片 (看板分頁) */
    recordCard(rec, dateStr, arrowRotation, hiddenClass, logsHtml) {
        const durationText = formatTime(rec.duration);
        return `
            <div class="p-4 bg-white rounded-2xl border border-gray-100 shadow-sm dark:bg-gray-750 dark:border-gray-700 flex flex-col">
                <div class="flex items-start justify-between w-full">
                    <div class="flex-1 cursor-pointer min-w-0" data-action="record-toggle-logs" data-value="${rec.id}">
                        <div class="flex items-center gap-2">
                            <div class="font-bold text-gray-800 dark:text-gray-100 truncate">${rec.routineTitle}</div>
                            <svg id="arrow-${rec.id}" class="w-3 h-3 text-gray-400 transition-transform shrink-0" 
                                 style="transform: ${arrowRotation};" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                <path d="M19 9l-7 7-7-7" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                            </svg>
                        </div>
                        <div class="text-[10px] text-gray-500 font-mono mt-0.5">${rec.startTime} | ${durationText}</div>
                    </div>

                    <button data-action="record-delete" data-record="${rec.id}" data-date="${dateStr}" 
                            class="p-2 text-gray-300 hover:text-red-500 transition-colors shrink-0 -mr-2 -mt-1">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" 
                                  stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                        </svg>
                    </button>
                </div>
                <div id="logs-${rec.id}" class="${hiddenClass} mt-3 space-y-2 border-t border-gray-50 pt-3 dark:border-gray-700">
                    ${logsHtml}
                </div>
            </div>`;
    },

    /** 單條執行日誌 (recordCard 內) */
    executionLogItem(recId, lIdx, loopText, label, actualsStr, failureBadge) {
        return `
            <button data-action="record-edit-log" data-record="${recId}" data-index="${lIdx}"
                    class="flex items-center gap-2 text-xs w-full hover:bg-gray-50 dark:hover:bg-gray-700/50 p-1 rounded transition-colors text-left">
                <div class="flex items-center min-w-0 shrink">
                    ${loopText}
                    <span class="text-gray-500 truncate dark:text-gray-300 font-bold">${label}</span>
                </div>
                <div class="flex-1 flex items-center justify-center bg-blue-50 text-blue-700 px-2 py-1.5 rounded-lg border border-blue-100 dark:bg-blue-900/30 dark:text-blue-300 dark:border-blue-800 font-mono min-w-fit">
                    <span class="truncate">${actualsStr || '0'}</span>
                    ${failureBadge}
                </div>
                <svg class="w-3 h-3 text-gray-300 shrink-0" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z" 
                          stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                </svg>
            </button>`;
    },

    /** 數據看板 PR 卡片 (分析分頁) */
    prCard(idx, config, isLarge, displayValue, unit, metricLabel, subtitle, prefix, isActive) {
        const spanClass = isLarge ? 'col-span-2' : '';
        const activeClass = isActive ? 'ring-2 ring-blue-500 bg-blue-50 dark:bg-blue-900/20' : 'bg-white hover:border-blue-300 dark:bg-gray-750';
        const fontSize = isLarge ? '4xl' : '2xl';

        return `
            <div class="${spanClass} ${activeClass} rounded-2xl p-5 shadow-sm border border-gray-100 dark:border-gray-600 relative overflow-hidden transition-all group">
                <div class="absolute inset-0 cursor-pointer" data-action="pr-select-card" data-value="${idx}"></div>
                <div class="relative flex justify-between items-start mb-2 pointer-events-none">
                    <span class="text-[10px] sm:text-xs text-gray-400 font-bold tracking-wider uppercase truncate pr-6">${prefix}${config.targetItem}</span>
                </div>
                <button data-action="pr-open-editor" data-value="${idx}" 
                        class="absolute top-3 right-3 p-1.5 text-gray-300 hover:text-blue-500 hover:bg-blue-50 rounded-lg transition-colors dark:hover:bg-gray-700 z-10">
                    <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                </button>
                <div class="relative text-${fontSize} font-black text-gray-800 dark:text-gray-100 pointer-events-none leading-none mb-1">
                    ${displayValue} <span class="text-xs font-bold text-gray-400">${unit}</span>
                </div>
                <div class="text-[10px] text-gray-400 font-bold uppercase tracking-widest pointer-events-none">${subtitle}${metricLabel}</div>
            </div>`;
    },

    /** 進行中課表 (Session) 提示卡片 */
    activeSession(session) {
        return `
            <div class="bg-blue-50 border border-blue-200 p-4 rounded-xl shadow-sm flex justify-between items-center dark:bg-blue-900/30 dark:border-blue-800">
                <div data-action="session-resume" class="flex-1 cursor-pointer">
                    <div class="text-xs font-bold text-blue-600 mb-1 dark:text-blue-400">進行中課表</div>
                    <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${session.routineTitle}</div>
                    <div class="text-xs text-gray-500 mt-1 dark:text-gray-400">已進行: ${formatTime(session.elapsed)}</div>
                </div>
                <button data-action="session-clear" class="p-2 text-gray-400 hover:text-red-500">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>
                </button>
            </div>`;
    },

    /** 課表列表為空時的顯示 */
    emptyRoutineState() {
        return `
            <div class="text-center mt-12 px-4">
                <div class="text-gray-400 mb-6 text-sm">目前尚無課表，馬上建立一個吧！</div>
                <button data-action="open-editor" class="bg-gray-900 text-white dark:bg-blue-600 px-6 py-4 rounded-2xl font-bold shadow-xl w-full mb-4 flex items-center justify-center gap-2 active:scale-95 transition-transform">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4"></path></svg>
                    建立新課表
                </button>
            </div>`;
    },

    /** 單個課表項目卡片 */
    routineItem(r, timeStr, blockCount, tagsHtml) {
        return `
            <div data-action="routine-start" data-value="${r.id}" class="flex-1 cursor-pointer">
                <div class="font-bold text-lg text-gray-800 dark:text-gray-100">${r.title}</div>
                ${tagsHtml}
                <div class="flex gap-3 mt-2 text-xs text-gray-500 font-mono dark:text-gray-400">
                    <span class="bg-gray-100 px-2 py-0.5 rounded dark:bg-gray-600 dark:text-gray-300">⏱ ${timeStr}</span>
                    <span>${blockCount} 區塊</span>
                </div>
            </div>
            <div class="flex items-center gap-1">
                 <button data-action="routine-duplicate" data-value="${r.id}" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="複製">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z"></path></svg>
                </button>
                 <button data-action="routine-edit" data-value="${r.id}" class="text-gray-400 hover:text-blue-600 p-2 dark:hover:text-blue-400" title="編輯">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                </button>
                <button data-action="routine-delete" data-value="${r.id}" class="text-gray-400 hover:text-red-600 p-2 dark:hover:text-red-400" title="刪除">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                </button>
            </div>`;
    },

    /** 數據看板：新增項目的虛線卡片 */
    emptyPRCard(idx, isLarge) {
        return `
            <div data-action="pr-open-editor" data-value="${idx}" 
                 class="${isLarge ? 'col-span-2' : ''} bg-gray-50 rounded-2xl p-4 border-2 border-dashed border-gray-200 flex flex-col items-center justify-center dark:bg-gray-800/50 dark:border-gray-600 cursor-pointer hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors">
                <div class="w-8 h-8 rounded-full bg-white flex items-center justify-center shadow-sm mb-1 dark:bg-gray-700">
                    <svg class="w-4 h-4 text-blue-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4"></path></svg>
                </div>
                <span class="text-[10px] text-gray-400 font-bold uppercase tracking-wider">新增看板項目</span>
            </div>`;
    },

    /** 數據看板：正式的 PR 數據卡片 */
    prCard(idx, config, isLarge, displayValue, unit, metricLabel, subtitle, prefix, isActive) {
        const spanClass = isLarge ? 'col-span-2' : '';
        const activeClass = isActive ? 'ring-2 ring-blue-500 bg-blue-50 dark:bg-blue-900/20' : 'bg-white hover:border-blue-300 dark:bg-gray-750';
        const fontSize = isLarge ? '4xl' : '2xl';

        return `
            <div class="${spanClass} ${activeClass} rounded-2xl p-5 shadow-sm border border-gray-100 dark:border-gray-600 relative overflow-hidden transition-all group">
                <div class="absolute inset-0 cursor-pointer" data-action="pr-select-card" data-value="${idx}"></div>
                
                <div class="relative flex justify-between items-start mb-2 pointer-events-none">
                    <span class="text-[10px] sm:text-xs text-gray-400 font-bold tracking-wider uppercase truncate pr-6">${prefix}${config.targetItem}</span>
                </div>
                
                <button data-action="pr-open-editor" data-value="${idx}" 
                        class="absolute top-3 right-3 p-1.5 text-gray-300 hover:text-blue-500 hover:bg-blue-50 rounded-lg transition-colors dark:hover:bg-gray-700 z-10">
                    <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                </button>

                <div class="relative text-${fontSize} font-black text-gray-800 dark:text-gray-100 pointer-events-none leading-none mb-1">
                    ${displayValue} <span class="text-xs font-bold text-gray-400">${unit}</span>
                </div>
                <div class="text-[10px] text-gray-400 font-bold uppercase tracking-widest pointer-events-none">${subtitle}${metricLabel}</div>
            </div>`;
    },

    /** 數據洞察：數據點勾選項目 */
    insightPointItem(log, period, isSelected) {
        const failureBadge = log.isFailure
            ? `<span class="ml-1 px-1 bg-red-500 text-white rounded text-[8px] font-black shrink-0">力竭</span>`
            : '';

        return `
            <label class="flex items-start gap-2 bg-white p-2 rounded-xl border border-gray-100 cursor-pointer hover:bg-gray-50 dark:bg-gray-800 transition-colors w-full">
                <input type="checkbox" 
                       data-action="insight-toggle-point" 
                       data-period="${period}" 
                       data-log-id="${log.id}" 
                       ${isSelected ? 'checked' : ''} 
                       class="mt-1 w-4 h-4 rounded text-blue-600">
                <div class="flex-1 min-w-0">
                    <div class="text-[10px] font-bold text-gray-700 dark:text-gray-300 flex items-center">
                        <span class="text-gray-400 mr-1.5">${log.date.substring(5)}</span>
                        <span class="truncate">${log.routineTitle}</span>
                        ${failureBadge}
                    </div>
                    <div class="text-[10px] text-gray-500 font-mono mt-0.5">${log.reps}RM @ ${log.weight.toFixed(1)}kg</div>
                </div>
            </label>`;
    },

    /** 目標管理：無目標時的顯示狀態 */
    emptyGoalState(showOnlyActive) {
        const msg = showOnlyActive ? "目前沒有進行中的目標" : "目前沒有設定目標";
        return `<div class="text-center text-gray-400 py-8 text-xs font-bold bg-gray-50/50 rounded-2xl border border-dashed border-gray-200 dark:bg-gray-800/30 dark:border-gray-700">${msg}</div>`;
    },

    /** 目標管理：單個目標進度卡片 */
    goalCard(g, current, target, pct, isCompleted, periodLabel, dateInfo, scopeText) {
        const isActive = g.isActive !== false;
        const activeClass = isActive ? '' : 'opacity-50 grayscale';
        const checkedAttr = isActive ? 'checked' : '';
        const statusColorClass = isCompleted ? 'text-green-500' : 'text-blue-600 dark:text-blue-400';
        const barColorClass = isCompleted ? 'bg-green-500' : 'bg-blue-600 dark:bg-blue-500';

        return `
            <div class="bg-white p-4 rounded-2xl shadow-sm border border-gray-100 dark:bg-gray-750 dark:border-gray-600 relative overflow-hidden transition-all ${activeClass}">
                <div class="flex justify-between items-start mb-3">
                    <div class="flex-1 pr-4">
                        <div class="flex items-center gap-2">
                            <h4 class="font-bold text-sm text-gray-900 dark:text-gray-100">${g.title}</h4>
                            ${isCompleted && isActive ? '<span class="text-[9px] bg-orange-100 text-orange-600 px-1.5 py-0.5 rounded font-black dark:bg-orange-900/40">🔥 已達成</span>' : ''}
                        </div>
                        <p class="text-[9px] text-gray-400 font-bold uppercase tracking-wider mt-1">
                            ${periodLabel} <span class="normal-case font-mono text-[8px]">${dateInfo}</span> • ${scopeText}
                        </p>
                    </div>
                    
                    <div class="flex items-center gap-2">
                        <label class="relative inline-flex items-center cursor-pointer">
                            <input type="checkbox" class="sr-only peer" ${checkedAttr} data-action="goal-toggle" data-value="${g.id}">
                            <div class="w-7 h-4 bg-gray-200 peer-focus:outline-none rounded-full peer dark:bg-gray-700 peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-3 after:w-3 after:transition-all peer-checked:bg-blue-600"></div>
                        </label>
                        <button data-action="goal-delete" data-value="${g.id}" class="text-gray-300 hover:text-red-500 transition-colors p-1">
                            <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                        </button>
                    </div>
                </div>
                
                <div class="flex items-end justify-between mb-2">
                    <span class="text-2xl font-black ${statusColorClass}">
                        ${current}<span class="text-[10px] text-gray-400 font-bold ml-1">/ ${target} 次</span>
                    </span>
                    <span class="text-[10px] font-bold text-gray-400">${Math.round(pct)}%</span>
                </div>

                <div class="w-full h-1.5 bg-gray-100 rounded-full overflow-hidden dark:bg-gray-800">
                    <div class="h-full transition-all duration-500 ${barColorClass}" style="width: ${pct}%"></div>
                </div>
            </div>`;
    },

    quickLogInput(m, idx, initialValue) {
        return `
            <div class="flex items-center justify-between bg-gray-900 rounded-xl p-3 mb-2">
                <span class="text-gray-300 text-lg font-bold pl-2">${m.name}</span>
                <div class="flex items-center gap-4" onmousedown="event.stopPropagation()" ontouchstart="event.stopPropagation()">
                    <button type="button" data-action="timer-adjust-log-val" data-index="${idx}" data-value="-1" 
                            class="w-10 h-10 bg-gray-700 text-white rounded-full flex items-center justify-center font-bold text-lg active:scale-90 transition-transform">
                        -
                    </button>
                    
                    <input type="number" step="any" id="quick-log-val-${idx}" data-name="${m.name}" value="${initialValue}" 
                           class="w-20 bg-transparent text-white text-lg border-none p-0 text-center font-bold outline-none focus:ring-2 focus:ring-blue-500 rounded transition-all">
                    
                    <button type="button" data-action="timer-adjust-log-val" data-index="${idx}" data-value="1" 
                            class="w-10 h-10 bg-gray-700 text-white rounded-full flex items-center justify-center font-bold text-lg active:scale-90 transition-transform">
                        +
                    </button>
                </div>
            </div>`;
    }
};