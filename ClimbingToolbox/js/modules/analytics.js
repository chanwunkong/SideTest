// --- js/modules/analytics.js ---

// --- js/modules/analytics.js ---

const analyticsManager = {
    configs: [],
    chartInstance: null,
    activeCardIndex: 0,
    currentRange: '28d',

    init() {
        const saved = localStorage.getItem('prCardConfigs');
        if (saved) {
            this.configs = JSON.parse(saved);
        } else {
            this.configs = [
                { sourceType: 'routine', targetItem: '最大懸垂', metric: 'relative_strength', aggregation: 'max', timeRange: 'all' },
                { sourceType: 'tag', targetItem: '耐力', metric: 'decay_rate', aggregation: 'avg', timeRange: '28d' },
                null
            ];
        }

        const rangeFilter = document.getElementById('chart-range-filter');
        if (rangeFilter) {
            rangeFilter.value = this.currentRange;
            rangeFilter.addEventListener('change', (e) => {
                this.currentRange = e.target.value;
                this.renderChart(this.activeCardIndex);
            });
        }
        this.renderCards();
    },

    saveConfigs() {
        localStorage.setItem('prCardConfigs', JSON.stringify(this.configs));
        this.renderCards();
    },

    // [新增] 尋找距離特定訓練日期最近的體重紀錄，消除體重噪音
    getClosestWeight(targetDateStr) {
        const bodyLogs = JSON.parse(localStorage.getItem('bodyLogs') || '{}');
        const dates = Object.keys(bodyLogs).sort();
        if (dates.length === 0) return 60; // 預設防呆值

        let bestDate = dates[0];
        for (let d of dates) {
            if (d <= targetDateStr) bestDate = d;
            else break;
        }
        return bodyLogs[bestDate].weight || 60;
    },

    extractData(config) {
        if (!config) return [];
        const records = recordManager.getAllRecords();
        const dataPoints = [];

        records.forEach(rec => {
            let match = false;

            if (config.sourceType === 'routine') {
                if (rec.routineTitle === config.targetItem) match = true;
            } else if (config.sourceType === 'tag') {
                if (rec.tags && rec.tags.length > 0) {
                    const selected = config.targetItems || [config.targetItem];
                    const op = config.operator || 'OR';
                    match = op === 'AND'
                        ? selected.every(t => rec.tags.includes(t))
                        : selected.some(t => rec.tags.includes(t));
                }
            }

            if (!match) return;

            let dayValue = null;
            const logs = rec.executionLogs || [];

            // 基礎容量指標
            if (config.metric === 'volume_time') {
                dayValue = rec.duration;
            } else if (config.metric === 'volume_reps') {
                dayValue = logs.reduce((sum, log) => sum + (log.actuals && log.actuals['次數'] ? log.actuals['次數'] : 0), 0);
            }
            // [新增] 生理映射：力竭率 (神經疲勞指標)
            else if (config.metric === 'failure_rate') {
                if (logs.length > 0) {
                    const fails = logs.filter(l => l.isFailure).length;
                    dayValue = (fails / logs.length) * 100;
                }
            }
            // [新增] 生理映射：組間衰減率 (代謝抗疲勞指標)
            else if (config.metric === 'decay_rate') {
                // 自動尋找主要負荷指標 (例如附加重量或次數)
                const validLogs = logs.filter(l => l.actuals && (l.actuals['附加重量'] !== undefined || l.actuals['次數'] !== undefined));
                if (validLogs.length >= 2) {
                    const metricKey = validLogs[0].actuals['附加重量'] !== undefined ? '附加重量' : '次數';
                    const firstVal = Number(validLogs[0].actuals[metricKey]);
                    const lastVal = Number(validLogs[validLogs.length - 1].actuals[metricKey]);

                    if (!isNaN(firstVal) && firstVal > 0) {
                        dayValue = ((firstVal - lastVal) / firstVal) * 100;
                        if (dayValue < 0) dayValue = 0; // 若越做越重，衰減率記為 0
                    }
                }
            }
            // [新增] 生理映射：相對強度係數 (體重對齊)
            else if (config.metric === 'relative_strength') {
                const vals = logs.map(l => Number(l.actuals && l.actuals['附加重量'])).filter(v => !isNaN(v));
                if (vals.length > 0) {
                    const maxLoad = Math.max(...vals);
                    const bw = this.getClosestWeight(rec.date);
                    dayValue = (maxLoad + bw) / bw; // 計算 (附加重量+體重)/體重
                }
            }
            // 傳統自訂絕對指標
            else {
                const vals = logs.map(l => Number(l.actuals && l.actuals[config.metric])).filter(v => !isNaN(v));
                if (vals.length > 0) {
                    if (config.aggregation === 'min') dayValue = Math.min(...vals);
                    else if (config.aggregation === 'avg') dayValue = vals.reduce((a, b) => a + b, 0) / vals.length;
                    else if (config.aggregation === 'latest') dayValue = vals[vals.length - 1];
                    else if (config.aggregation === 'sum' || config.aggregation === 'max_daily_sum') dayValue = vals.reduce((a, b) => a + b, 0);
                    else dayValue = Math.max(...vals);
                }
            }

            if (dayValue !== null) {
                dataPoints.push({ date: rec.date, timestamp: rec.timestamp, value: dayValue });
            }
        });
        return dataPoints;
    },

    calculateCardValue(dataPoints, timeRange, aggregation) {
        let filtered = dataPoints;

        if (timeRange === '10t') {
            filtered = dataPoints.slice(-10);
        } else {
            const now = Date.now();
            let limit = 0;
            if (timeRange === '7d') limit = now - 7 * 86400000;
            if (timeRange === '28d') limit = now - 28 * 86400000;
            if (timeRange === '84d') limit = now - 84 * 86400000;
            if (limit > 0) filtered = dataPoints.filter(d => d.timestamp >= limit);
        }

        if (filtered.length === 0) return 0;

        if (aggregation === 'latest') return filtered[filtered.length - 1].value;
        if (aggregation === 'active_days') return new Set(filtered.map(d => d.date)).size;

        if (aggregation === 'max_daily_sum') {
            const grouped = {};
            filtered.forEach(d => {
                if (!grouped[d.date]) grouped[d.date] = 0;
                grouped[d.date] += d.value;
            });
            return Math.max(...Object.values(grouped));
        }

        const vals = filtered.map(d => d.value);
        if (aggregation === 'max') return Math.max(...vals);
        if (aggregation === 'min') return Math.min(...vals);
        if (aggregation === 'sum') return vals.reduce((a, b) => a + b, 0);
        if (aggregation === 'avg') return vals.reduce((a, b) => a + b, 0) / vals.length;

        return 0;
    },

    processChartData(dataPoints, range, aggregation) {
        if (range === '10t') {
            const recent = dataPoints.slice(-10);
            const labels = recent.map(d => {
                const t = new Date(d.timestamp);
                const hh = t.getHours().toString().padStart(2, '0');
                const mm = t.getMinutes().toString().padStart(2, '0');
                return `${d.date.substring(5)} ${hh}:${mm}`;
            });
            const data = recent.map(d => d.value);
            return { labels, data };
        }

        const now = Date.now();
        let limit = 0;
        if (range === '7d') limit = now - 7 * 86400000;
        if (range === '28d') limit = now - 28 * 86400000;
        if (range === '84d') limit = now - 84 * 86400000;

        let filtered = limit > 0 ? dataPoints.filter(d => d.timestamp >= limit) : dataPoints;
        if (filtered.length === 0) return { labels: [], data: [] };

        filtered.sort((a, b) => a.timestamp - b.timestamp);

        const grouped = {};
        filtered.forEach(d => {
            if (!grouped[d.date]) grouped[d.date] = [];
            grouped[d.date].push(d.value);
        });

        const [sY, sM, sD] = filtered[0].date.split('-').map(Number);
        const startDate = new Date(sY, sM - 1, sD);
        const [eY, eM, eD] = filtered[filtered.length - 1].date.split('-').map(Number);
        const endDate = new Date(eY, eM - 1, eD);

        const labels = [];
        const data = [];

        let curr = new Date(startDate);
        while (curr <= endDate) {
            const y = curr.getFullYear();
            const m = String(curr.getMonth() + 1).padStart(2, '0');
            const d = String(curr.getDate()).padStart(2, '0');
            const dateStr = `${y}-${m}-${d}`;

            labels.push(`${m}-${d}`);

            if (grouped[dateStr]) {
                const vals = grouped[dateStr];
                if (aggregation === 'min') data.push(Math.min(...vals));
                else if (aggregation === 'sum' || aggregation === 'max_daily_sum') data.push(vals.reduce((a, b) => a + b, 0));
                else if (aggregation === 'active_days') data.push(1);
                else if (aggregation === 'latest') data.push(vals[vals.length - 1]);
                else if (aggregation === 'avg') data.push(vals.reduce((a, b) => a + b, 0) / vals.length);
                else data.push(Math.max(...vals));
            } else {
                data.push(null);
            }
            curr.setDate(curr.getDate() + 1);
        }

        return { labels, data };
    },

    renderCards() {
        const container = document.getElementById('pr-cards-container');
        if (!container) return;
        container.innerHTML = '';

        this.configs.forEach((config, idx) => {
            const isLarge = idx === 0;

            if (!config) {
                container.innerHTML += `
                    <div class="${isLarge ? 'col-span-2' : ''} bg-gray-50 rounded-2xl p-4 border-2 border-dashed border-gray-200 flex flex-col items-center justify-center dark:bg-gray-800/50 dark:border-gray-600 cursor-pointer hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors" onclick="analyticsUI.openPREditor(${idx})">
                        <div class="w-8 h-8 rounded-full bg-white flex items-center justify-center shadow-sm mb-1 dark:bg-gray-700">
                            <svg class="w-4 h-4 text-blue-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4"></path></svg>
                        </div>
                        <span class="text-[10px] text-gray-400 font-bold uppercase tracking-wider">新增看板項目</span>
                    </div>`;
                return;
            }

            const rawData = this.extractData(config);
            let displayValue = this.calculateCardValue(rawData, config.timeRange || 'all', config.aggregation);

            let unit = '';
            let metricLabel = config.metric;

            // 處理顯示單位與標籤
            if (config.metric === 'volume_time') {
                displayValue = Math.floor(displayValue / 60);
                unit = 'min';
                metricLabel = '總時數';
            } else if (config.metric === 'volume_reps') {
                unit = '次';
                metricLabel = '總次數';
            } else if (config.metric === 'failure_rate') {
                displayValue = Math.round(displayValue);
                unit = '%';
                metricLabel = '力竭率';
            } else if (config.metric === 'decay_rate') {
                displayValue = Math.round(displayValue * 10) / 10;
                unit = '%';
                metricLabel = '表現衰減';
            } else if (config.metric === 'relative_strength') {
                displayValue = (Math.round(displayValue * 100) / 100).toFixed(2);
                unit = 'xBW';
                metricLabel = '相對強度';
            } else {
                displayValue = Math.round(displayValue * 10) / 10;
                unit = config.metric.includes('重') ? 'kg' : '';
            }

            if (config.aggregation === 'active_days') {
                unit = '天';
                metricLabel += ' (天數)';
            } else if (config.aggregation === 'latest') {
                metricLabel += ' (最新)';
            } else if (config.aggregation === 'avg') {
                metricLabel += ' (平均)';
            }

            let prefix = config.sourceType === 'tag' ? '#' : '';
            let subtitle = '';
            if (config.timeRange === '10t') subtitle = '近10次 • ';
            else if (config.timeRange === '7d') subtitle = '近7天 • ';
            else if (config.timeRange === '28d') subtitle = '近4週 • ';
            else if (config.timeRange === '84d') subtitle = '近12週 • ';
            else subtitle = '歷史 • ';

            let isActive = this.activeCardIndex === idx ? 'ring-2 ring-blue-500 bg-blue-50 dark:bg-blue-900/20' : 'bg-white hover:border-blue-300 dark:bg-gray-750';

            container.innerHTML += `
                <div class="${isLarge ? 'col-span-2' : ''} ${isActive} rounded-2xl p-5 shadow-sm border border-gray-100 dark:border-gray-600 relative overflow-hidden transition-all group">
                    <div class="absolute inset-0 cursor-pointer" onclick="analyticsManager.selectCard(${idx})"></div>
                    
                    <div class="relative flex justify-between items-start mb-2 pointer-events-none">
                        <span class="text-[10px] sm:text-xs text-gray-400 font-bold tracking-wider uppercase truncate pr-6">${prefix}${config.targetItem}</span>
                    </div>
                    
                    <button onclick="analyticsUI.openPREditor(${idx})" class="absolute top-3 right-3 p-1.5 text-gray-300 hover:text-blue-500 hover:bg-blue-50 rounded-lg transition-colors dark:hover:bg-gray-700 z-10">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                    </button>

                    <div class="relative text-${isLarge ? '4xl' : '2xl'} font-black text-gray-800 dark:text-gray-100 pointer-events-none leading-none mb-1">
                        ${displayValue} <span class="text-xs font-bold text-gray-400">${unit}</span>
                    </div>
                    <div class="text-[10px] text-gray-400 font-bold uppercase tracking-widest pointer-events-none">${subtitle}${metricLabel}</div>
                </div>`;
        });

        this.renderChart(this.activeCardIndex);
    },

    selectCard(idx) {
        if (!this.configs[idx]) return;
        this.activeCardIndex = idx;
        this.renderCards();
    },

    renderChart(idx) {
        const config = this.configs[idx];
        const canvas = document.getElementById('trend-chart');
        const placeholder = document.getElementById('chart-placeholder');

        if (!canvas || !placeholder) return;

        if (!config) {
            canvas.classList.add('hidden');
            placeholder.classList.remove('hidden');
            return;
        }

        const rawData = this.extractData(config);
        const chartData = this.processChartData(rawData, this.currentRange, config.aggregation);

        if (chartData.labels.length === 0) {
            canvas.classList.add('hidden');
            placeholder.innerHTML = '尚無足夠的訓練數據';
            placeholder.classList.remove('hidden');
            return;
        }

        canvas.classList.remove('hidden');
        placeholder.classList.add('hidden');

        const isDark = document.documentElement.classList.contains('dark');
        const textColor = isDark ? '#9ca3af' : '#6b7280';
        const gridColor = isDark ? 'rgba(255,255,255,0.05)' : 'rgba(0,0,0,0.05)';

        if (this.chartInstance) {
            this.chartInstance.destroy();
        }

        const ctx = canvas.getContext('2d');
        const chartType = (config.metric === 'volume_time' || config.metric === 'volume_reps' || config.aggregation === 'active_days') ? 'bar' : 'line';

        // 針對生理指標調整圖表 Y 軸起始點
        const beginAtZero = (chartType === 'bar' || config.metric === 'failure_rate' || config.metric === 'decay_rate');

        this.chartInstance = new Chart(ctx, {
            type: chartType,
            data: {
                labels: chartData.labels,
                datasets: [{
                    label: config.metric,
                    data: chartData.data,
                    borderColor: '#3b82f6',
                    backgroundColor: chartType === 'bar' ? 'rgba(59, 130, 246, 0.5)' : 'rgba(59, 130, 246, 0.1)',
                    borderWidth: 3,
                    tension: 0.3,
                    fill: chartType === 'line',
                    pointBackgroundColor: '#ffffff',
                    pointBorderColor: '#3b82f6',
                    pointRadius: 4,
                    spanGaps: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: {
                        ticks: { color: textColor, font: { family: 'monospace' }, maxTicksLimit: 8 },
                        grid: { display: false }
                    },
                    y: {
                        ticks: { color: textColor },
                        grid: { color: gridColor },
                        beginAtZero: beginAtZero
                    }
                }
            }
        });
    }
};

const analyticsUI = {
    editingIndex: null,
    currentOperator: 'OR',

    setOperator(op) {
        this.currentOperator = op;
        const btnOr = document.getElementById('pr-op-or');
        const btnAnd = document.getElementById('pr-op-and');
        if (!btnOr || !btnAnd) return;
        if (op === 'OR') {
            btnOr.className = "flex-1 py-1.5 text-[10px] font-bold rounded-md bg-white shadow-sm text-blue-600 transition-all";
            btnAnd.className = "flex-1 py-1.5 text-[10px] font-bold rounded-md text-gray-400 border border-transparent transition-all";
        } else {
            btnAnd.className = "flex-1 py-1.5 text-[10px] font-bold rounded-md bg-white shadow-sm text-blue-600 transition-all";
            btnOr.className = "flex-1 py-1.5 text-[10px] font-bold rounded-md text-gray-400 border border-transparent transition-all";
        }
    },

    openPREditor(index) {
        this.editingIndex = index;
        const config = analyticsManager.configs[index];

        document.getElementById('pr-source-type').value = config ? config.sourceType : 'routine';
        this.updateTargetDropdown();

        if (config) {
            document.getElementById('pr-target-item').value = config.targetItem;
            this.updateMetricDropdown();
            setTimeout(() => {
                document.getElementById('pr-metric').value = config.metric;
                document.getElementById('pr-aggregation').value = config.aggregation || 'max';
                document.getElementById('pr-time-range').value = config.timeRange || 'all';
            }, 50);
        } else {
            document.getElementById('pr-time-range').value = 'all';
            document.getElementById('pr-aggregation').value = 'max';
        }

        const modal = document.getElementById('modal-pr-editor');
        const sheet = document.getElementById('pr-sheet');
        modal.classList.remove('hidden');
        modal.classList.add('open');
        setTimeout(() => sheet.classList.remove('translate-y-full'), 10);
    },

    closePREditor() {
        const modal = document.getElementById('modal-pr-editor');
        const sheet = document.getElementById('pr-sheet');
        sheet.classList.add('translate-y-full');
        modal.classList.remove('open');
        setTimeout(() => modal.classList.add('hidden'), 300);
    },

    updateTargetDropdown() {
        const sourceType = document.getElementById('pr-source-type').value;
        const targetSelect = document.getElementById('pr-target-item');
        let html = '<option value="">(請選擇)</option>';

        if (sourceType === 'routine') {
            store.routines.forEach(r => { html += `<option value="${r.title}">${r.title}</option>`; });
        } else if (sourceType === 'tag') {
            const tags = editor.getRoutineTagHistory();
            tags.forEach(t => { html += `<option value="${t}">${t}</option>`; });
        }

        targetSelect.innerHTML = html;
        targetSelect.onchange = () => this.updateMetricDropdown();
        this.updateMetricDropdown();
    },

    updateMetricDropdown() {
        const sourceType = document.getElementById('pr-source-type').value;
        const targetItem = document.getElementById('pr-target-item').value;
        const metricSelect = document.getElementById('pr-metric');

        // [修改] 注入生理狀態分析指標
        let html = `
            <optgroup label="基礎容量">
                <option value="volume_time">總時數 (Time)</option>
                <option value="volume_reps">總次數 (Reps)</option>
            </optgroup>
            <optgroup label="生理決策映射">
                <option value="relative_strength">相對強度係數 (Load/BW)</option>
                <option value="decay_rate">組間表現衰減率 (%)</option>
                <option value="failure_rate">力竭與疲勞率 (%)</option>
            </optgroup>
            <optgroup label="自訂絕對數值">
        `;

        if (targetItem) {
            const records = recordManager.getAllRecords();
            const customMetricsSet = new Set();

            records.forEach(rec => {
                let match = false;
                if (sourceType === 'routine' && rec.routineTitle === targetItem) match = true;
                if (sourceType === 'tag') {
                    const r = store.routines.find(x => x.title === rec.routineTitle);
                    if (r && r.tags && r.tags.includes(targetItem)) match = true;
                }
                if (match && rec.executionLogs) {
                    rec.executionLogs.forEach(log => {
                        if (log.actuals) {
                            Object.keys(log.actuals).forEach(k => {
                                if (k !== '次數' && k !== '秒數' && k !== 'isFailure') customMetricsSet.add(k);
                            });
                        }
                    });
                }
            });

            customMetricsSet.forEach(m => { html += `<option value="${m}">${m}</option>`; });
        }
        html += `</optgroup>`;
        metricSelect.innerHTML = html;
    },

    savePRCard() {
        const sourceType = document.getElementById('pr-source-type').value;
        const targetItem = document.getElementById('pr-target-item').value;
        const metric = document.getElementById('pr-metric').value;
        const aggregation = document.getElementById('pr-aggregation').value;
        const timeRange = document.getElementById('pr-time-range').value;
        const operator = this.currentOperator;

        if (!targetItem) return;

        analyticsManager.configs[this.editingIndex] = { sourceType, targetItem, metric, aggregation, timeRange, operator };
        analyticsManager.saveConfigs();
        this.closePREditor();
    },

    deletePRCard() {
        analyticsManager.configs[this.editingIndex] = null;
        analyticsManager.saveConfigs();
        this.closePREditor();
    }
};

const bodyManager = {
    displayDate: new Date(),
    latestHeight: null,

    init() {
        // 1. 初始化：預設尋找「最新的一筆紀錄」來顯示
        const logs = this.getAllLogs();
        const dates = Object.keys(logs).sort();
        if (dates.length > 0) {
            this.displayDate = this.parseDate(dates[dates.length - 1]);
        } else {
            this.displayDate = new Date();
        }

        this.renderCard();

        if (typeof initSwipeToClose === 'function') {
            initSwipeToClose('body-sheet', () => this.closeEditor());
        }

        // 2. 綁定連動：當使用者在 Modal 改變日期時，自動讀取該日數據
        const dateInput = document.getElementById('body-editor-date');
        if (dateInput) {
            dateInput.addEventListener('change', (e) => {
                if (e.target.value) {
                    this.populateEditorFields(e.target.value);
                }
            });
        }
    },

    // 輔助函式：安全解析日期 (避免時區跑版)
    parseDate(dateStr) {
        const [y, m, d] = dateStr.split('-');
        return new Date(y, m - 1, d);
    },

    getFormatDate(dateObj) {
        const y = dateObj.getFullYear();
        const m = (dateObj.getMonth() + 1).toString().padStart(2, '0');
        const d = dateObj.getDate().toString().padStart(2, '0');
        return `${y}-${m}-${d}`;
    },

    getAllLogs() {
        return JSON.parse(localStorage.getItem('bodyLogs') || '{}');
    },

    // 3. 跳躍式切換日期 (直接跳過沒有紀錄的日子)
    changeDate(dir) {
        const logs = this.getAllLogs();
        const dates = Object.keys(logs).sort();
        if (dates.length === 0) return;

        const currentStr = this.getFormatDate(this.displayDate);
        let targetStr = currentStr;

        if (dir === -1) { // 上一筆紀錄
            const past = dates.filter(d => d < currentStr);
            if (past.length > 0) targetStr = past[past.length - 1];
        } else if (dir === 1) { // 下一筆紀錄
            const future = dates.filter(d => d > currentStr);
            if (future.length > 0) targetStr = future[0];
        }

        if (targetStr !== currentStr) {
            this.displayDate = this.parseDate(targetStr);
            this.renderCard();
        }
    },

    calculateMetrics(weight, height, bodyFat) {
        if (!weight || !height) return { bmi: null, lbm: null, ffmi: null };
        const hMeter = height / 100;
        const bmi = weight / (hMeter * hMeter);
        let lbm = null; let ffmi = null;

        if (bodyFat) {
            lbm = weight * (1 - (bodyFat / 100));
            ffmi = lbm / (hMeter * hMeter);
        }
        return {
            bmi: parseFloat(bmi.toFixed(1)),
            lbm: lbm ? parseFloat(lbm.toFixed(1)) : null,
            ffmi: ffmi ? parseFloat(ffmi.toFixed(1)) : null
        };
    },

    renderCard() {
        const dateStr = this.getFormatDate(this.displayDate);
        const dateLabel = document.getElementById('body-date-label');
        if (dateLabel) dateLabel.textContent = dateStr;

        const logs = this.getAllLogs();
        const data = logs[dateStr];

        const elFfmi = document.getElementById('body-val-ffmi');
        const elLbm = document.getElementById('body-val-lbm');
        const elWeight = document.getElementById('body-val-weight');
        const elFat = document.getElementById('body-val-fat');
        const elMuscle = document.getElementById('body-val-muscle');
        const elBmi = document.getElementById('body-val-bmi');

        if (!elFfmi) return;

        if (data) {
            const metrics = this.calculateMetrics(data.weight, data.height, data.bodyFat);
            elWeight.textContent = data.weight || '--';
            elFat.textContent = data.bodyFat || '--';
            elMuscle.textContent = data.muscleMass || '--';
            elBmi.textContent = metrics.bmi || '--';
            elLbm.textContent = metrics.lbm || '--';
            elFfmi.textContent = metrics.ffmi || '--';

            if (data.height) this.latestHeight = data.height;
        } else {
            elFfmi.textContent = '--'; elLbm.textContent = '--';
            elWeight.textContent = '--'; elFat.textContent = '--';
            elMuscle.textContent = '--'; elBmi.textContent = '--';
        }
    },

    openEditor() {
        const modal = document.getElementById('modal-body-editor');
        const sheet = document.getElementById('body-sheet');

        if (!modal || !sheet) return;

        // 預設將 Modal 的日期設定為目前看的那一天，或是今天
        const dateStr = this.getFormatDate(this.displayDate);
        const dateInput = document.getElementById('body-editor-date');
        if (dateInput) dateInput.value = dateStr;

        // 執行填寫邏輯
        this.populateEditorFields(dateStr);

        modal.classList.remove('hidden');
        modal.classList.add('open');
        requestAnimationFrame(() => sheet.classList.remove('translate-y-full'));
    },

    // 4. 獨立的填表邏輯 (供切換日期時呼叫)
    populateEditorFields(dateStr) {
        const logs = this.getAllLogs();
        const data = logs[dateStr];

        let defaultHeight = this.latestHeight;
        if (!defaultHeight) {
            const dates = Object.keys(logs).sort().reverse();
            for (let d of dates) {
                if (logs[d].height) { defaultHeight = logs[d].height; break; }
            }
        }

        document.getElementById('inp-body-height').value = data?.height || defaultHeight || '';
        document.getElementById('inp-body-weight').value = data?.weight || '';
        document.getElementById('inp-body-fat').value = data?.bodyFat || '';
        document.getElementById('inp-body-muscle').value = data?.muscleMass || '';

        const deleteBtn = document.getElementById('btn-body-delete');
        const saveBtn = document.getElementById('btn-body-save');

        if (data) {
            if (deleteBtn) deleteBtn.classList.remove('hidden');
            if (saveBtn) {
                saveBtn.classList.remove('flex-1');
                saveBtn.classList.add('w-2/3');
                saveBtn.textContent = '更新紀錄';
            }
        } else {
            if (deleteBtn) deleteBtn.classList.add('hidden');
            if (saveBtn) {
                saveBtn.classList.add('flex-1');
                saveBtn.classList.remove('w-2/3');
                saveBtn.textContent = '儲存紀錄';
            }
        }
    },

    closeEditor() {
        const modal = document.getElementById('modal-body-editor');
        const sheet = document.getElementById('body-sheet');
        sheet.classList.add('translate-y-full');
        modal.classList.remove('open');
        setTimeout(() => modal.classList.add('hidden'), 300);
    },

    saveRecord() {
        const h = parseFloat(document.getElementById('inp-body-height').value);
        const w = parseFloat(document.getElementById('inp-body-weight').value);
        const bf = parseFloat(document.getElementById('inp-body-fat').value);
        const mm = parseFloat(document.getElementById('inp-body-muscle').value);

        if (isNaN(h) || isNaN(w)) {
            alert("身高與體重為必填項目。");
            return;
        }

        // 5. 儲存時，以 Modal 上選擇的日期為主
        const dateInput = document.getElementById('body-editor-date');
        const dateStr = dateInput ? dateInput.value : this.getFormatDate(this.displayDate);

        const logs = this.getAllLogs();
        logs[dateStr] = {
            height: h,
            weight: w,
            bodyFat: isNaN(bf) ? null : bf,
            muscleMass: isNaN(mm) ? null : mm,
            updatedAt: Date.now()
        };

        localStorage.setItem('bodyLogs', JSON.stringify(logs));
        this.latestHeight = h;

        // 存檔後將背景卡片切換至剛儲存/更新的那一天
        this.displayDate = this.parseDate(dateStr);
        this.renderCard();
        this.closeEditor();
    },

    deleteRecord() {
        if (!confirm("確定要刪除這天的身體組成紀錄嗎？")) return;

        // 刪除時，同樣以 Modal 上選擇的日期為主
        const dateInput = document.getElementById('body-editor-date');
        const dateStr = dateInput ? dateInput.value : this.getFormatDate(this.displayDate);

        const logs = this.getAllLogs();
        if (logs[dateStr]) {
            delete logs[dateStr];
            localStorage.setItem('bodyLogs', JSON.stringify(logs));
        }

        // 若刪除的是目前正在看的日期，嘗試重新載入最新日期
        if (dateStr === this.getFormatDate(this.displayDate)) {
            const newDates = Object.keys(logs).sort();
            this.displayDate = newDates.length > 0 ? this.parseDate(newDates[newDates.length - 1]) : new Date();
        }

        this.renderCard();
        this.closeEditor();
    }
};

// 獨立綁定初始化，避免在主 HTML 遺漏執行導致無法編輯
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(() => {
        if (typeof bodyManager !== 'undefined') {
            bodyManager.init();
        }
    }, 100);
});

document.addEventListener('DOMContentLoaded', () => {
    const sourceSelect = document.getElementById('pr-source-type');
    if (sourceSelect) sourceSelect.addEventListener('change', () => analyticsUI.updateTargetDropdown());

    const btnDelete = document.getElementById('btn-delete-pr');
    if (btnDelete) btnDelete.addEventListener('click', () => analyticsUI.deletePRCard());
});