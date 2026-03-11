// --- js/modules/analytics.js ---

const analyticsManager = {
    configs: [],
    chartInstance: null,
    activeCardIndex: 0,
    currentRange: '28d', // 預設改為 4 週

    init() {
        const saved = localStorage.getItem('prCardConfigs');
        if (saved) {
            this.configs = JSON.parse(saved);
        } else {
            // 加入 timeRange 屬性
            this.configs = [
                { sourceType: 'routine', targetItem: 'Max Hangs', metric: '附加重量', aggregation: 'max', timeRange: 'all' },
                { sourceType: 'tag', targetItem: '指力', metric: 'volume_time', aggregation: 'sum', timeRange: '7d' },
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

    extractData(config) {
        if (!config) return [];
        const records = recordManager.getAllRecords();
        const dataPoints = [];

        records.forEach(rec => {
            let match = false;

            if (config.sourceType === 'routine') {
                // 優先比對 Title（相容舊紀錄），未來若 targetItem 改存 ID 則直接比對 ID
                if (rec.routineTitle === config.targetItem) match = true;
            } else if (config.sourceType === 'tag') {
                // ✨ 直接使用歷史紀錄中保存的標籤快照，不再依賴當前課表名稱
                if (rec.tags && rec.tags.length > 0) {
                    const selected = config.targetItems || [config.targetItem];
                    const op = config.operator || 'OR';

                    if (op === 'AND') {
                        match = selected.every(t => rec.tags.includes(t));
                    } else {
                        match = selected.some(t => rec.tags.includes(t));
                    }
                }
            }

            if (!match) return;

            let dayValue = null;

            if (config.metric === 'volume_time') {
                dayValue = rec.duration;
            } else if (config.metric === 'volume_reps') {
                let reps = 0;
                if (rec.executionLogs) {
                    rec.executionLogs.forEach(log => {
                        if (log.actuals && log.actuals['次數']) reps += log.actuals['次數'];
                    });
                }
                dayValue = reps;
            } else {
                if (rec.executionLogs) {
                    const vals = [];
                    rec.executionLogs.forEach(log => {
                        if (log.actuals && log.actuals[config.metric] !== undefined) {
                            const val = Number(log.actuals[config.metric]);
                            if (!isNaN(val)) vals.push(val);
                        }
                    });
                    if (vals.length > 0) {
                        // 針對單日極端值與平均值的預先處理
                        if (config.aggregation === 'min') dayValue = Math.min(...vals);
                        else if (config.aggregation === 'avg') dayValue = vals.reduce((a, b) => a + b, 0) / vals.length;
                        else if (config.aggregation === 'latest') dayValue = vals[vals.length - 1];
                        else if (config.aggregation === 'sum' || config.aggregation === 'max_daily_sum') dayValue = vals.reduce((a, b) => a + b, 0);
                        else dayValue = Math.max(...vals); // max
                    }
                }
            }

            if (dayValue !== null) {
                dataPoints.push({ date: rec.date, timestamp: rec.timestamp, value: dayValue });
            }
        });
        return dataPoints;
    },

    // 專門計算卡片單一顯示數值的引擎
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

        // 避免時區偏差，使用本地時間解析起迄日
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

            labels.push(`${m}-${d}`); // 預先裁切為 MM-DD 格式

            if (grouped[dateStr]) {
                const vals = grouped[dateStr];
                if (aggregation === 'min') data.push(Math.min(...vals));
                else if (aggregation === 'sum' || aggregation === 'max_daily_sum') data.push(vals.reduce((a, b) => a + b, 0));
                else if (aggregation === 'active_days') data.push(1);
                else if (aggregation === 'latest') data.push(vals[vals.length - 1]);
                else if (aggregation === 'avg') data.push(vals.reduce((a, b) => a + b, 0) / vals.length);
                else data.push(Math.max(...vals));
            } else {
                // 無數據的日子補上 null 以維持 X 軸的真實間隔比例
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
            let metricLabel = config.metric === 'volume_time' ? '總時數' : (config.metric === 'volume_reps' ? '總次數' : config.metric);

            if (config.metric === 'volume_time') {
                displayValue = Math.floor(displayValue / 60);
                unit = 'min';
            } else if (config.metric === 'volume_reps') {
                unit = '次';
            } else {
                displayValue = Math.round(displayValue * 10) / 10;
                unit = config.metric.includes('重') ? 'kg' : '';
            }

            // 單位覆寫 (天數與平均)
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

        this.chartInstance = new Chart(ctx, {
            type: chartType,
            data: {
                labels: chartData.labels, // 已在 processChartData 處理格式
                datasets: [{
                    label: config.metric === 'volume_time' ? '總時數(秒)' : config.metric,
                    data: chartData.data,
                    borderColor: '#3b82f6',
                    backgroundColor: chartType === 'bar' ? 'rgba(59, 130, 246, 0.5)' : 'rgba(59, 130, 246, 0.1)',
                    borderWidth: 3,
                    tension: 0.3,
                    fill: chartType === 'line',
                    pointBackgroundColor: '#ffffff',
                    pointBorderColor: '#3b82f6',
                    pointRadius: 4,
                    spanGaps: true // 允許折線跨越 null 區間相連
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: {
                        ticks: {
                            color: textColor,
                            font: { family: 'monospace' },
                            maxTicksLimit: 8 // 自動限制 X 軸標籤數量，避免日期過多時重疊
                        },
                        grid: { display: false }
                    },
                    y: {
                        ticks: { color: textColor },
                        grid: { color: gridColor },
                        beginAtZero: chartType === 'bar'
                    }
                }
            }
        });
    }
};

const analyticsUI = {
    editingIndex: null,

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
        let html = '<option value="">(請選擇)</option>'; // 改用字串拼接

        if (sourceType === 'routine') {
            store.routines.forEach(r => {
                html += `<option value="${r.title}">${r.title}</option>`;
            });
        } else if (sourceType === 'tag') {
            const tags = editor.getRoutineTagHistory();
            tags.forEach(t => {
                html += `<option value="${t}">${t}</option>`;
            });
        }

        targetSelect.innerHTML = html; // 迴圈結束後一次性寫入
        targetSelect.onchange = () => this.updateMetricDropdown();
        this.updateMetricDropdown();
    },

    updateMetricDropdown() {
        const sourceType = document.getElementById('pr-source-type').value;
        const targetItem = document.getElementById('pr-target-item').value;
        const metricSelect = document.getElementById('pr-metric');

        let html = `
            <option value="volume_time">總時數 (Time)</option>
            <option value="volume_reps">總次數 (Count)</option>
            <option disabled>── 自訂追蹤項目 ──</option>
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
                                if (k !== '次數' && k !== '秒數') customMetricsSet.add(k);
                            });
                        }
                    });
                }
            });

            customMetricsSet.forEach(m => {
                html += `<option value="${m}">${m}</option>`;
            });
        }
        metricSelect.innerHTML = html;
    },

    savePRCard() {
        const sourceType = document.getElementById('pr-source-type').value;
        const targetItem = document.getElementById('pr-target-item').value;
        const metric = document.getElementById('pr-metric').value;
        const aggregation = document.getElementById('pr-aggregation').value;
        const timeRange = document.getElementById('pr-time-range').value;

        if (!targetItem) {
            if (typeof showToast === 'function') showToast('請選擇目標項目', 'error');
            return;
        }

        analyticsManager.configs[this.editingIndex] = { sourceType, targetItem, metric, aggregation, timeRange };
        analyticsManager.saveConfigs();
        this.closePREditor();
        if (typeof showToast === 'function') showToast('看板已更新');
    },

    deletePRCard() {
        analyticsManager.configs[this.editingIndex] = null;
        analyticsManager.saveConfigs();
        this.closePREditor();
    }
};

document.addEventListener('DOMContentLoaded', () => {
    const sourceSelect = document.getElementById('pr-source-type');
    if (sourceSelect) sourceSelect.addEventListener('change', () => analyticsUI.updateTargetDropdown());

    const btnDelete = document.getElementById('btn-delete-pr');
    if (btnDelete) btnDelete.addEventListener('click', () => analyticsUI.deletePRCard());
});