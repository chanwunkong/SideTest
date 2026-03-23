// --- js/modules/templates.js ---
import { uuid } from './storage.js';

export const templateLibrary = {
    max: () => ({
        title: "最大指力",
        tags: ["最大肌力", "手指"],
        blocks: [
            { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
            {
                type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' },
                children: [
                    { type: 'timer', id: uuid(), props: { duration: 10, label: '懸掛', color: 'red', customMetrics: [{ name: '重量', type: 'number' }, { name: '邊緣', type: 'number' }] } },
                    { type: 'timer', id: uuid(), props: { duration: 180, label: '休息', color: 'green', skipOnLast: true } }
                ]
            }
        ]
    }),

    repeaters: () => ({
        title: "7/3手指耐力",
        tags: ["耐力", "手指"],
        blocks: [
            { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
            {
                type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                children: [
                    {
                        type: 'loop', id: uuid(), props: { iterations: 6, color: 'gray' },
                        children: [
                            { type: 'timer', id: uuid(), props: { duration: 7, label: '懸掛', color: 'red', customMetrics: [{ name: '重量', type: 'number' }] } },
                            { type: 'timer', id: uuid(), props: { duration: 3, label: '休息', color: 'green', skipOnLast: true } }
                        ]
                    },
                    { type: 'timer', id: uuid(), props: { duration: 180, label: '組間休息', color: 'green', skipOnLast: true } }
                ]
            }
        ]
    }),

    pullups: () => ({
        title: "上肢與核心",
        tags: ["體能", "上肢力量"],
        blocks: [
            { type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } },
            {
                type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                children: [
                    { type: 'reps', id: uuid(), props: { count: 5, duration: 30, label: '引體向上', color: 'blue', customMetrics: [{ name: '引體次數', type: 'number' }, { name: '重量', type: 'number' }] } },
                    { type: 'timer', id: uuid(), props: { duration: 20, label: 'L型支撐', color: 'orange', customMetrics: [{ name: '核心秒數', type: 'number' }] } },
                    { type: 'timer', id: uuid(), props: { duration: 90, label: '休息', color: 'green', skipOnLast: true } }
                ]
            }
        ]
    }),

    squat: () => {
        const blocks = [{ type: 'timer', id: uuid(), props: { duration: 10, label: '準備', color: 'amber' } }];
        const exercises = ['引體向上', '深蹲', '肩推', '划船', '硬舉', '臥推'];

        exercises.forEach((ex, index) => {
            blocks.push({
                type: 'loop', id: uuid(), props: { iterations: 3, color: 'gray' },
                children: [
                    { type: 'reps', id: uuid(), props: { count: 5, duration: 60, label: `${ex} (熱身)`, color: 'blue', customMetrics: [{ name: '次數', type: 'number' }, { name: '重量', type: 'number' }] } },
                    { type: 'timer', id: uuid(), props: { duration: 120, label: '休息', color: 'green' } }
                ]
            });
            blocks.push({
                type: 'loop', id: uuid(), props: { iterations: 5, color: 'gray' },
                children: [
                    { type: 'reps', id: uuid(), props: { count: 5, duration: 60, label: `${ex} (主項)`, color: 'red', customMetrics: [{ name: '次數', type: 'number' }, { name: '重量', type: 'number' }] } },
                    { type: 'timer', id: uuid(), props: { duration: 180, label: '休息', color: 'green', skipOnLast: true } }
                ]
            });
            if (index < exercises.length - 1) {
                blocks.push({ type: 'timer', id: uuid(), props: { duration: 300, label: '器材轉換', color: 'teal' } });
            }
        });

        return {
            title: "全身肌力 5x5",
            tags: ["全身", "肌力", "最大肌力"],
            blocks: blocks
        };
    }
};