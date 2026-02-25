// --- js/modules/holdfocus-engine.js ---

// --- Web Worker 邏輯 ---
const workerCode = `
            let gVisited = null;
            let gQueue = null;
            let gLabels = null;

            self.onmessage = function(e) {
                const { type, sel, w, h, bgData } = e.data;
                if (type === 'CALC_MASK') {
                    const size = w * h;
                    if (!gVisited || gVisited.length < size) {
                        gVisited = new Uint8Array(size);
                        gQueue = new Int32Array(size * 2);
                        gLabels = new Int32Array(size);
                    }
                    const res = calculateMaskInternal(sel, w, h, bgData);
                    let borderMask = null;
                    if (sel.params.border && res.bounds) {
                        borderMask = getBorderInternal(res.mask, w, h, sel.params.borderThickness || 5, res.bounds);
                    }
                    // 優化1: 邊緣平滑處理 (高斯/盒狀模糊近似)
                    let finalMask = res.mask;
                    if (res.bounds) {
                        finalMask = blurMaskInternal(res.mask, w, h, 2, res.bounds);
                    }
                    
                    self.postMessage({ id: sel.id, mask: finalMask, borderMask, detectedColor: res.color }, [finalMask.buffer, borderMask ? borderMask.buffer : null].filter(Boolean));
                }
            };
            
            function distToSegmentSquared(px, py, vx, vy, wx, wy) {
                const l2 = (wx - vx) ** 2 + (wy - vy) ** 2;
                if (l2 === 0) return (px - vx) ** 2 + (py - vy) ** 2;
                let t = ((px - vx) * (wx - vx) + (py - vy) * (wy - vy)) / l2;
                t = Math.max(0, Math.min(1, t));
                return (px - (vx + t * (wx - vx))) ** 2 + (py - (vy + t * (wy - vy))) ** 2;
            }

            function calculateMaskInternal(sel, w, h, bgData) {
                const mask = new Uint8Array(w * h);
                const tolerance = sel.params.tolerance;
                let rSum = 0, gSum = 0, bSum = 0, colorCount = 0;
                let bMinX = w, bMaxX = 0, bMinY = h, bMaxY = 0;

                function addPoint(x, y) {
                    if (x < bMinX) bMinX = x; if (x > bMaxX) bMaxX = x;
                    if (y < bMinY) bMinY = y; if (y > bMaxY) bMaxY = y;
                }

                if (!sel.actions || sel.actions.length === 0) return { mask, color: null, bounds: null };

                gVisited.fill(0);

                for (const action of sel.actions) {
                    if (action.type === 'lasso') {
                        let qTail = 0, qHead = 0;
                        const palette = [];
                        const path = action.path;

                        let pMinX = w, pMaxX = 0, pMinY = h, pMaxY = 0;
                        path.forEach(p => { if(p.x<pMinX) pMinX=p.x; if(p.x>pMaxX) pMaxX=p.x; if(p.y<pMinY) pMinY=p.y; if(p.y>pMaxY) pMaxY=p.y; });
                        pMinX = Math.max(0, Math.floor(pMinX)); pMinY = Math.max(0, Math.floor(pMinY));
                        pMaxX = Math.min(w - 1, Math.ceil(pMaxX)); pMaxY = Math.min(h - 1, Math.ceil(pMaxY));

                        // Scanline Fill 
                        for (let y = pMinY; y <= pMaxY; y++) {
                            let nodes = [];
                            let j = path.length - 1;
                            for (let i = 0; i < path.length; i++) {
                                if ((path[i].y < y && path[j].y >= y) || (path[j].y < y && path[i].y >= y)) {
                                    nodes.push(path[i].x + (y - path[i].y) / (path[j].y - path[i].y) * (path[j].x - path[i].x));
                                }
                                j = i;
                            }
                            nodes.sort((a, b) => a - b);
                            for (let i = 0; i < nodes.length; i += 2) {
                                if (nodes[i+1] === undefined) break;
                                let sx = Math.max(pMinX, Math.floor(nodes[i]));
                                let ex = Math.min(pMaxX, Math.ceil(nodes[i+1]));
                                for (let x = sx; x <= ex; x++) {
                                    const idx = y * w + x;
                                    mask[idx] = 255; // 內部邏輯改用 255 代表 100% 不透明
                                    addPoint(x, y);
                                    if (gVisited[idx] === 0) {
                                        gVisited[idx] = 1;
                                        gQueue[qTail++] = x; gQueue[qTail++] = y;
                                        const pIdx = idx << 2;
                                        if (palette.length < 400 && (x + y) % 4 === 0) palette.push({ r: bgData[pIdx], g: bgData[pIdx+1], b: bgData[pIdx+2] });
                                        if (colorCount < 100 && (x + y) % 7 === 0) {
                                            rSum += bgData[pIdx]; gSum += bgData[pIdx+1]; bSum += bgData[pIdx+2]; colorCount++;
                                        }
                                    }
                                }
                            }
                        }

                        if (palette.length > 0) {
                            const tolSq = tolerance * tolerance;
                            while (qHead < qTail) {
                                const x = gQueue[qHead++], y = gQueue[qHead++];
                                const process = (nx, ny) => {
                                    const nIdx = ny * w + nx;
                                    if (gVisited[nIdx] === 0) {
                                        gVisited[nIdx] = 1;
                                        const pIdx = nIdx << 2;
                                        const r = bgData[pIdx], g = bgData[pIdx+1], b = bgData[pIdx+2];
                                        let match = false;
                                        for(let s=0; s<palette.length; s++) {
                                            const dr = r - palette[s].r, dg = g - palette[s].g, db = b - palette[s].b;
                                            if ((2*dr*dr + 4*dg*dg + 3*db*db) <= tolSq) { match = true; break; }
                                        }
                                        if (match) { mask[nIdx] = 255; addPoint(nx, ny); gQueue[qTail++] = nx; gQueue[qTail++] = ny; }
                                    }
                                };
                                if (y > 0) process(x, y - 1);
                                if (x < w - 1) process(x + 1, y);
                                if (y < h - 1) process(x, y + 1);
                                if (x > 0) process(x - 1, y);
                            }
                        }
                    } else if (action.type === 'erase') {
                        const { path, size } = action;
                        const rad = size / 2, r2 = rad * rad;
                        const fPath = [path[0]];
                        for (let i = 1; i < path.length; i++) {
                            const prev = fPath[fPath.length - 1];
                            const d2 = (path[i].x - prev.x)**2 + (path[i].y - prev.y)**2;
                            if (d2 >= (rad / 3)**2 || i === path.length - 1) fPath.push(path[i]);
                        }
                        if (fPath.length === 1) {
                            const cx = Math.floor(fPath[0].x), cy = Math.floor(fPath[0].y);
                            for(let y=Math.max(0, Math.floor(cy-rad)); y<=Math.min(h-1, Math.ceil(cy+rad)); y++) {
                                for(let x=Math.max(0, Math.floor(cx-rad)); x<=Math.min(w-1, Math.ceil(cx+rad)); x++) {
                                    if ((x-cx)**2 + (y-cy)**2 <= r2) mask[y*w+x] = 0;
                                }
                            }
                        } else {
                            for (let i = 0; i < fPath.length - 1; i++) {
                                const p1 = fPath[i], p2 = fPath[i+1];
                                const cMinX = Math.max(0, Math.floor(Math.min(p1.x, p2.x) - rad));
                                const cMaxX = Math.min(w - 1, Math.ceil(Math.max(p1.x, p2.x) + rad));
                                const cMinY = Math.max(0, Math.floor(Math.min(p1.y, p2.y) - rad));
                                const cMaxY = Math.min(h - 1, Math.ceil(Math.max(p1.y, p2.y) + rad));
                                for (let y = cMinY; y <= cMaxY; y++) {
                                    for (let x = cMinX; x <= cMaxX; x++) {
                                        if (distToSegmentSquared(x, y, p1.x, p1.y, p2.x, p2.y) <= r2) mask[y * w + x] = 0;
                                    }
                                }
                            }
                        }
                    }
                }

                if (bMinX > bMaxX) return { mask, color: null, bounds: null };

                const pad = Math.max(sel.params.expand, 2);
                bMinX = Math.max(0, bMinX - pad); bMaxX = Math.min(w - 1, bMaxX + pad);
                bMinY = Math.max(0, bMinY - pad); bMaxY = Math.min(h - 1, bMaxY + pad);
                const bounds = { minX: bMinX, maxX: bMaxX, minY: bMinY, maxY: bMaxY };

                if (sel.params.expand > 0) expandInternal(mask, w, h, sel.params.expand, bounds);
                if (sel.params.fillHoles) fillHolesInternal(mask, w, h, bounds);
                keepLargestInternal(mask, w, h, bounds);
                
                const hex = colorCount > 0 ? '#' + [rSum/colorCount, gSum/colorCount, bSum/colorCount].map(v => Math.round(v).toString(16).padStart(2, '0')).join('') : null;
                return { mask, color: hex, bounds };
            }

            // Box blur 柔化邊緣
            function blurMaskInternal(mask, w, h, rad, bounds) {
                const temp = new Uint8Array(mask.length);
                const { minX, maxX, minY, maxY } = bounds;
                const bMinX = Math.max(0, minX - rad), bMaxX = Math.min(w - 1, maxX + rad);
                const bMinY = Math.max(0, minY - rad), bMaxY = Math.min(h - 1, maxY + rad);

                for (let y = bMinY; y <= bMaxY; y++) {
                    for (let x = bMinX; x <= bMaxX; x++) {
                        let sum = 0, count = 0;
                        for (let dy = -rad; dy <= rad; dy++) {
                            for (let dx = -rad; dx <= rad; dx++) {
                                const nx = x + dx, ny = y + dy;
                                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                    sum += mask[ny * w + nx] === 255 ? 1 : 0;
                                    count++;
                                }
                            }
                        }
                        temp[y * w + x] = Math.round((sum / count) * 255);
                    }
                }
                return temp;
            }

            function expandInternal(mask, w, h, rad, bounds) {
                const { minX, maxX, minY, maxY } = bounds;
                const old = new Uint8Array(mask);
                for(let y = minY; y <= maxY; y++) {
                    const row = y * w;
                    for(let x = minX; x <= maxX; x++) {
                        if(old[row + x]) {
                            let isE = (x===minX || x===maxX || y===minY || y===maxY);
                            if(!isE && (!old[row+x-1] || !old[row+x+1] || !old[row-w+x] || !old[row+w+x])) isE = true;
                            if (isE) {
                                for(let ky=-rad; ky<=rad; ky++) {
                                    for(let kx=-rad; kx<=rad; kx++) {
                                        const ny = y+ky, nx = x+kx; 
                                        if(nx>=0 && nx<w && ny>=0 && ny<h) mask[ny*w+nx] = 255;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            function fillHolesInternal(mask, w, h, bounds) {
                const { minX, maxX, minY, maxY } = bounds;
                gVisited.fill(0);
                let qHead = 0, qTail = 0;
                
                for(let x = minX; x <= maxX; x++) { 
                    if(!mask[minY*w+x]) { gQueue[qTail++]=x; gQueue[qTail++]=minY; gVisited[minY*w+x]=1; } 
                    if(!mask[maxY*w+x]) { gQueue[qTail++]=x; gQueue[qTail++]=maxY; gVisited[maxY*w+x]=1; } 
                }
                for(let y = minY; y <= maxY; y++) { 
                    if(!mask[y*w+minX] && !gVisited[y*w+minX]) { gQueue[qTail++]=minX; gQueue[qTail++]=y; gVisited[y*w+minX]=1; } 
                    if(!mask[y*w+maxX] && !gVisited[y*w+maxX]) { gQueue[qTail++]=maxX; gQueue[qTail++]=y; gVisited[y*w+maxX]=1; } 
                }
                
                while(qHead < qTail) {
                    const x = gQueue[qHead++], y = gQueue[qHead++];
                    let nIdx;
                    if(y > minY) { nIdx = (y-1)*w+x; if(!mask[nIdx] && !gVisited[nIdx]) { gVisited[nIdx] = 1; gQueue[qTail++]=x; gQueue[qTail++]=y-1; } }
                    if(x < maxX) { nIdx = y*w+(x+1); if(!mask[nIdx] && !gVisited[nIdx]) { gVisited[nIdx] = 1; gQueue[qTail++]=x+1; gQueue[qTail++]=y; } }
                    if(y < maxY) { nIdx = (y+1)*w+x; if(!mask[nIdx] && !gVisited[nIdx]) { gVisited[nIdx] = 1; gQueue[qTail++]=x; gQueue[qTail++]=y+1; } }
                    if(x > minX) { nIdx = y*w+(x-1); if(!mask[nIdx] && !gVisited[nIdx]) { gVisited[nIdx] = 1; gQueue[qTail++]=x-1; gQueue[qTail++]=y; } }
                }
                for(let y = minY; y <= maxY; y++) {
                    for(let x = minX; x <= maxX; x++) {
                        const idx = y*w+x;
                        if(!mask[idx] && !gVisited[idx]) mask[idx] = 255;
                    }
                }
            }

            function keepLargestInternal(mask, w, h, bounds) {
                const { minX, maxX, minY, maxY } = bounds;
                gVisited.fill(0);
                gLabels.fill(0);
                let maxC = 0, maxL = 0, curL = 1;
                
                for(let y = minY; y <= maxY; y++) {
                    for(let x = minX; x <= maxX; x++) {
                        const i = y * w + x;
                        if (mask[i] === 255 && gVisited[i] === 0) {
                            let c = 0, qHead = 0, qTail = 0;
                            gQueue[qTail++] = x; gQueue[qTail++] = y;
                            gVisited[i] = 1; gLabels[i] = curL;
                            
                            while(qHead < qTail) {
                                const cx = gQueue[qHead++], cy = gQueue[qHead++];
                                c++;
                                let nIdx;
                                if(cy > minY) { nIdx = (cy-1)*w+cx; if(mask[nIdx] === 255 && gVisited[nIdx] === 0) { gVisited[nIdx] = 1; gLabels[nIdx] = curL; gQueue[qTail++]=cx; gQueue[qTail++]=cy-1; } }
                                if(cx < maxX) { nIdx = cy*w+(cx+1); if(mask[nIdx] === 255 && gVisited[nIdx] === 0) { gVisited[nIdx] = 1; gLabels[nIdx] = curL; gQueue[qTail++]=cx+1; gQueue[qTail++]=cy; } }
                                if(cy < maxY) { nIdx = (cy+1)*w+cx; if(mask[nIdx] === 255 && gVisited[nIdx] === 0) { gVisited[nIdx] = 1; gLabels[nIdx] = curL; gQueue[qTail++]=cx; gQueue[qTail++]=cy+1; } }
                                if(cx > minX) { nIdx = cy*w+(cx-1); if(mask[nIdx] === 255 && gVisited[nIdx] === 0) { gVisited[nIdx] = 1; gLabels[nIdx] = curL; gQueue[qTail++]=cx-1; gQueue[qTail++]=cy; } }
                            }
                            if (c > maxC) { maxC = c; maxL = curL; }
                            curL++;
                        }
                    }
                }
                if (maxL > 0) {
                    for(let y = minY; y <= maxY; y++) {
                        for(let x = minX; x <= maxX; x++) {
                            const i = y * w + x;
                            mask[i] = (gLabels[i] === maxL) ? 255 : 0;
                        }
                    }
                }
            }

            function getBorderInternal(mask, w, h, thickness, bounds) {
                const { minX, maxX, minY, maxY } = bounds;
                const border = new Uint8Array(mask.length);
                const expanded = new Uint8Array(mask);
                // Convert soft mask back to binary for border calc
                for(let i=0; i<mask.length; i++) expanded[i] = mask[i] > 128 ? 255 : 0;
                
                expandInternal(expanded, w, h, thickness, {
                    minX: Math.max(0, minX - thickness), maxX: Math.min(w - 1, maxX + thickness),
                    minY: Math.max(0, minY - thickness), maxY: Math.min(h - 1, maxY + thickness)
                });
                
                for(let y = Math.max(0, minY - thickness); y <= Math.min(h - 1, maxY + thickness); y++) {
                    for(let x = Math.max(0, minX - thickness); x <= Math.min(w - 1, maxX + thickness); x++) {
                        const i = y * w + x;
                        if (expanded[i] && mask[i] < 128) border[i] = 1;
                    }
                }
                return border;
            }
        `;

class StateManager {
    constructor(onHistoryChange) {
        this.history = []; this.historyIndex = -1; this.onHistoryChange = onHistoryChange;
        this.data = { filters: { contrast: 100, bgSat: 50, bgDim: 50, bgBlur: 10 }, selections: [], objects: [], crop: { index: 0, bgWhite: true }, nextSelId: 1, nextObjId: 1 };
        this.view = { scale: 1, x: 0, y: 0, tool: 'lasso', activeTab: 'route', activeSelId: null, activeObjId: null, brushSize: 20, isDragging: false, dragType: null, tempPath: [], snapObjX: false, snapObjY: false };
    }
    pushHistory() {
        const safe = this.data.selections.map(s => { const { mask, borderMask, ...rest } = s; return rest; });
        // 儲存資料的同時，連同當前「選取狀態」一起儲存，符合使用者退回時的預期
        const snapshot = JSON.stringify({
            data: { ...this.data, selections: safe },
            viewState: { activeSelId: this.view.activeSelId, activeObjId: this.view.activeObjId, activeTab: this.view.activeTab }
        });
        if (this.historyIndex < this.history.length - 1) this.history = this.history.slice(0, this.historyIndex + 1);
        this.history.push(snapshot); this.historyIndex++;
        if (this.history.length > 20) { this.history.shift(); this.historyIndex--; }
        this.onHistoryChange();
    }
    undo() { if (this.historyIndex > 0) { this.historyIndex--; this.restore(this.history[this.historyIndex]); } }
    redo() { if (this.historyIndex < this.history.length - 1) { this.historyIndex++; this.restore(this.history[this.historyIndex]); } }
    restore(json) {
        const parsed = JSON.parse(json);
        if (parsed.viewState) {
            this.data = parsed.data;
            this.view.activeSelId = parsed.viewState.activeSelId;
            this.view.activeObjId = parsed.viewState.activeObjId;
            this.view.activeTab = parsed.viewState.activeTab;
        } else {
            this.data = parsed; // 相容舊紀錄
        }
        this.onHistoryChange(true);
    }
}

class LayerManager {
    constructor() {
        this.bgCtx = document.getElementById('layer-bg')?.getContext('2d');
        this.maskCtx = document.getElementById('layer-mask')?.getContext('2d');
        this.uiCtx = document.getElementById('layer-ui')?.getContext('2d');
        const loupeC = document.getElementById('loupe-canvas');
        this.loupeCtx = loupeC ? loupeC.getContext('2d') : null;
        this.width = this.height = 0;
        this.tempC = document.createElement('canvas');
        this.tCtx = this.tempC.getContext('2d', { willReadFrequently: true });
        this.borderC = document.createElement('canvas');
        this.bCtx = this.borderC.getContext('2d', { willReadFrequently: true });
    }
    resize(w, h) {
        this.width = w; this.height = h;
        const wrapper = document.getElementById('canvas-wrapper');
        if (wrapper) { wrapper.style.width = w + 'px'; wrapper.style.height = h + 'px'; }
        [this.bgCtx, this.maskCtx, this.uiCtx].forEach(ctx => { if (ctx) { ctx.canvas.width = w; ctx.canvas.height = h; } });
        this.tempC.width = w; this.tempC.height = h;
        this.iData = this.tCtx.createImageData(w, h);
        this.iData32 = new Uint32Array(this.iData.data.buffer);
        this.borderC.width = w; this.borderC.height = h;
        this.bImg = this.bCtx.createImageData(w, h);
        this.bImg32 = new Uint32Array(this.bImg.data.buffer);
    }
    drawBg(img, filters) {
        if (!img || !this.bgCtx) return;
        const ctx = this.bgCtx; ctx.clearRect(0, 0, this.width, this.height);
        ctx.filter = `contrast(${filters.contrast}%)`; ctx.drawImage(img, 0, 0); ctx.filter = 'none';
        this.bgBuffer = ctx.getImageData(0, 0, this.width, this.height).data;
    }
    drawMasks(selections, filters) {
        if (!this.bgBuffer || !this.maskCtx) return;
        const ctx = this.maskCtx; ctx.clearRect(0, 0, this.width, this.height);
        ctx.save(); ctx.filter = `saturate(${filters.bgSat}%) brightness(${100 - filters.bgDim}%) blur(${filters.bgBlur}px)`;
        ctx.drawImage(this.bgCtx.canvas, 0, 0); ctx.restore();
        ctx.globalCompositeOperation = 'destination-out';

        this.iData32.fill(0);
        let hasM = false;
        selections.forEach(s => {
            if (s.mask && s.visible !== false) {
                for (let i = 0; i < s.mask.length; i++) {
                    if (s.mask[i] > 0) {
                        // 將平滑處理後的 0-255 值賦予 alpha 通道
                        this.iData.data[(i << 2) + 3] = s.mask[i];
                        hasM = true;
                    }
                }
            }
        });

        if (hasM) {
            this.tCtx.putImageData(this.iData, 0, 0);
            ctx.drawImage(this.tempC, 0, 0);
        }

        ctx.globalCompositeOperation = 'source-over';
        selections.forEach(s => {
            if (s.params.border && s.borderMask && s.visible !== false) {
                this.bImg32.fill(0);
                let col = { r: 236, g: 72, b: 153 };
                const res = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(s.params.borderColor || '#EC4899');
                if (res) col = { r: parseInt(res[1], 16), g: parseInt(res[2], 16), b: parseInt(res[3], 16) };
                const col32 = (255 << 24) | (col.b << 16) | (col.g << 8) | col.r;

                const mask = s.borderMask;
                const len = mask.length;
                for (let i = 0; i < len; i++) {
                    if (mask[i]) this.bImg32[i] = col32;
                }
                this.bCtx.putImageData(this.bImg, 0, 0);
                ctx.drawImage(this.borderC, 0, 0);
            }
        });
    }
    drawUi(state) {
        if (!this.uiCtx) return;
        const ctx = this.uiCtx; ctx.clearRect(0, 0, this.width, this.height);

        if (state.view.isDragging && state.view.dragType === 'object') {
            const cx = this.width / 2, cy = this.height / 2;
            ctx.strokeStyle = '#3b82f6'; ctx.lineWidth = 2 / state.view.scale; ctx.setLineDash([5, 5]);
            if (state.view.snapObjX) { ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, this.height); ctx.stroke(); }
            if (state.view.snapObjY) { ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(this.width, cy); ctx.stroke(); }
            ctx.setLineDash([]);
        }

        if (state.view.isDragging && state.view.tempPath.length > 0) {
            const p = state.view.tempPath; ctx.beginPath(); ctx.moveTo(p[0].x, p[0].y);
            for (let i = 1; i < p.length; i++) ctx.lineTo(p[i].x, p[i].y);
            if (state.view.dragType === 'lasso') { ctx.strokeStyle = 'rgba(255,230,0,0.9)'; ctx.lineWidth = 2 / state.view.scale; ctx.setLineDash([5, 5]); ctx.stroke(); ctx.setLineDash([]); }
            else if (state.view.dragType === 'eraser') { ctx.lineCap = 'round'; ctx.lineJoin = 'round'; ctx.lineWidth = state.view.brushSize; ctx.strokeStyle = 'rgba(255,0,0,0.5)'; ctx.stroke(); }
        }

        state.data.objects.forEach(obj => {
            const isS = obj.id === state.view.activeObjId && state.view.activeTab === 'text';
            ctx.save(); ctx.translate(obj.x, obj.y); if (obj.rotation) ctx.rotate(obj.rotation);
            if (obj.type === 'text') {
                ctx.font = `bold ${obj.fontSize}px sans-serif`; ctx.textBaseline = 'top'; const m = ctx.measureText(obj.text);
                if (obj.bgColor) {
                    ctx.beginPath(); ctx.fillStyle = obj.bgColor; ctx.roundRect(-8, -8, m.width + 16, obj.fontSize + 12, 8); ctx.fill();
                }
                ctx.fillStyle = obj.color; ctx.fillText(obj.text, 0, 0);
                if (isS) { ctx.strokeStyle = '#3b82f6'; ctx.lineWidth = 2; ctx.setLineDash([5, 3]); ctx.strokeRect(-4, -4, m.width + 8, obj.fontSize + 8); }
            } else {
                const s = obj.fontSize / 60; ctx.scale(s, s); ctx.fillStyle = obj.color; ctx.shadowColor = 'rgba(0,0,0,0.3)'; ctx.shadowBlur = 4;
                ctx.save(); ctx.rotate(-0.26); ctx.fillRect(-25, -8, 50, 16); ctx.rotate(0.52); ctx.fillRect(-25, -8, 50, 16); ctx.restore();
                ctx.shadowBlur = 0; ctx.fillStyle = 'white'; ctx.font = 'bold 20px sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(obj.text, 0, 0);
                if (isS) { ctx.strokeStyle = '#3b82f6'; ctx.lineWidth = 3; ctx.setLineDash([5, 3]); ctx.strokeRect(-30, -30, 60, 60); }
            }
            ctx.restore();
        });
    }
    updateLoupe(pos, scale) {
        if (!this.loupeCtx || !this.bgCtx) return;
        const container = document.getElementById('canvas-container'), rect = container.getBoundingClientRect();
        const l = document.getElementById('loupe'), lc = document.getElementById('loupe-canvas');
        l.style.display = 'block'; l.style.left = (pos.screenX - rect.left - 65) + 'px'; l.style.top = (pos.screenY - rect.top - 150) + 'px';
        lc.width = lc.height = 130; const ctx = this.loupeCtx; ctx.clearRect(0, 0, 130, 130);
        [this.bgCtx, this.maskCtx, this.uiCtx].forEach(c => { if (c) ctx.drawImage(c.canvas, pos.x - 32 / scale, pos.y - 32 / scale, 64 / scale, 64 / scale, 0, 0, 130, 130); });
        ctx.strokeStyle = 'white'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(65, 0); ctx.lineTo(65, 130); ctx.moveTo(0, 65); ctx.lineTo(130, 65); ctx.stroke();
    }
    hideLoupe() { const l = document.getElementById('loupe'); if (l) l.style.display = 'none'; }
}

class HoldFocusApp {
    constructor() {
        // this.lang 移除，改由 i18n 模組管理
        this.state = new StateManager((isRestore) => {
            this.updateUI(isRestore);
            this.flags.ui = true;
            if (isRestore) {
                this.flags.bg = true;
                this.flags.mask = true;
                this.state.data.selections.forEach(sel => {
                    if (sel.actions && sel.actions.length > 0) {
                        this.requestWorker(sel);
                    }
                });
            }
        });
        this.layers = new LayerManager();
        this.flags = { bg: false, mask: false, ui: false };
        this.els = this.cacheDOM();
        this.pointers = new Map();
        this.adjustTimeout = null; this.adjustInterval = null; this.cachedRect = null;

        this.initWorker();
        this.initListeners();

        // 初始化時執行一次全頁翻譯
        this.updateAllText();
        this.loop();
    }

    // --- 修改處：使用全域 i18n ---
    t(key) {
        // 這裡做個防呆，如果 i18n 未載入則回傳 key
        return (typeof i18n !== 'undefined') ? i18n.t(key) : key;
    }

    updateAllText() {
        if (typeof i18n !== 'undefined') {
            i18n.updatePage();
            // 更新按鈕文字 (特殊處理)
            const langBtn = document.getElementById('lang-btn');
            if (langBtn) langBtn.innerText = i18n._current === 'zh' ? 'EN' : '中';
        }
        this.updateUI();
    }

    // --- js/modules/holdfocus-engine.js 內部 ---
    toggleLang() {
        if (typeof i18n !== 'undefined') {
            i18n.toggle(); // 直接呼叫全域模組的切換方法
        }
    }

    initWorker() {
        const blob = new Blob([workerCode], { type: 'application/javascript' });
        this.worker = new Worker(URL.createObjectURL(blob));
        this.worker.onmessage = (e) => {
            const sel = this.state.data.selections.find(s => s.id === e.data.id);
            if (sel) {
                sel.mask = e.data.mask; sel.borderMask = e.data.borderMask;
                if (e.data.detectedColor) sel.detectedColor = e.data.detectedColor;
            }
            this.flags.mask = true; this.updateUI();
            document.getElementById('processing-bar')?.classList.add('hidden');
        };
    }
    cacheDOM() {
        const ids = ['contrast', 'bg-sat', 'bg-dim', 'bg-blur', 'tool-hand', 'tool-lasso', 'tool-eraser', 'brush-size', 'text-input', 'text-size', 'text-rotation', 'color-list', 'file-upload', 'download-btn', 'canvas-container', 'canvas-transform-root', 'panel-route', 'panel-text', 'tab-route', 'tab-text', 'undo-btn', 'redo-btn', 'delete-item-btn', 'crop-btn', 'crop-frame', 'crop-bg-color', 'guide-v', 'guide-h', 'crop-mask-container', 'crop-inner-backdrop', 'layer-ui', 'lang-btn'];
        const els = {}; ids.forEach(id => els[id] = document.getElementById(id)); return els;
    }
    startAdjust(id, delta) { this.adjSlider(id, delta); this.adjustTimeout = setTimeout(() => { this.adjustInterval = setInterval(() => { this.adjSlider(id, delta); }, 50); }, 400); }
    stopAdjust() { clearTimeout(this.adjustTimeout); clearInterval(this.adjustInterval); }
    initListeners() {
        document.addEventListener('pointerdown', e => { const btn = e.target.closest('.slider-btn'); if (btn && btn.dataset.target) { e.preventDefault(); btn.setPointerCapture(e.pointerId); this.startAdjust(btn.dataset.target, parseFloat(btn.dataset.delta)); } });
        window.addEventListener('pointerup', () => this.stopAdjust()); window.addEventListener('pointercancel', () => this.stopAdjust());

        this.els['lang-btn']?.addEventListener('click', () => this.toggleLang());
        this.els['file-upload']?.addEventListener('change', e => this.loadFile(e.target.files[0]));
        this.els['download-btn']?.addEventListener('click', () => this.export());
        this.els['contrast']?.addEventListener('input', e => { this.state.data.filters.contrast = parseInt(e.target.value); const v = document.getElementById('contrast-val'); if (v) v.innerText = e.target.value + '%'; this.flags.bg = this.flags.mask = true; });
        ['bg-sat', 'bg-dim', 'bg-blur'].forEach(id => {
            this.els[id]?.addEventListener('input', e => {
                const k = id.replace('-s', 'S').replace('-d', 'D').replace('-b', 'B').replace('-', '');
                this.state.data.filters[k] = parseFloat(e.target.value); const v = document.getElementById(`${id}-val`); if (v) v.innerText = e.target.value + (id.includes('blur') ? 'px' : '%'); this.flags.mask = true;
            });
        });
        ['hand', 'lasso', 'eraser'].forEach(t => { const btn = document.getElementById(`tool-${t}`); if (btn) btn.addEventListener('click', (e) => { e.preventDefault(); this.setTool(t); }); });
        this.els['brush-size']?.addEventListener('input', e => { this.state.view.brushSize = parseInt(e.target.value); const v = document.getElementById('brush-size-val'); if (v) v.innerText = e.target.value + 'px'; });
        this.els['tab-route']?.addEventListener('click', () => this.switchTab('route')); this.els['tab-text']?.addEventListener('click', () => this.switchTab('text'));
        document.getElementById('add-text-btn')?.addEventListener('click', () => this.addObj('text'));
        document.getElementById('add-start-tag')?.addEventListener('click', () => this.addObj('tag', 'S', '#10B981'));
        document.getElementById('add-top-tag')?.addEventListener('click', () => this.addObj('tag', 'T', '#EF4444'));
        this.els['delete-item-btn']?.addEventListener('click', () => { if (this.state.view.activeObjId) { this.state.data.objects = this.state.data.objects.filter(o => o.id !== this.state.view.activeObjId); this.state.view.activeObjId = null; this.flags.ui = true; this.updateUI(); this.state.pushHistory(); } });

        this.els['text-size']?.addEventListener('input', e => { const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId); if (o) { o.fontSize = parseInt(e.target.value); this.flags.ui = true; } });
        this.els['text-size']?.addEventListener('change', e => { this.state.pushHistory(); });
        this.els['text-rotation']?.addEventListener('input', e => { const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId); if (o) { o.rotation = parseInt(e.target.value) * Math.PI / 180; this.flags.ui = true; } });
        this.els['text-rotation']?.addEventListener('change', e => { this.state.pushHistory(); });

        const cc = this.els['canvas-container'];
        if (cc) {
            cc.addEventListener('pointerdown', e => { this.pointerDown(e); });
            window.addEventListener('pointermove', e => { this.pointerMove(e); });
            window.addEventListener('pointerup', e => { this.pointerUp(e); });
            cc.addEventListener('pointercancel', e => { this.pointerUp(e); });
            cc.addEventListener('wheel', e => { e.preventDefault(); this.state.view.scale = Math.max(0.1, Math.min(5, this.state.view.scale * (e.deltaY > 0 ? 0.9 : 1.1))); this.updateTransform(); }, { passive: false });
        }
        this.els['undo-btn']?.addEventListener('click', () => this.state.undo());
        this.els['redo-btn']?.addEventListener('click', () => this.state.redo());
        this.els['crop-btn']?.addEventListener('click', () => this.toggleCrop());
        this.els['crop-bg-color']?.addEventListener('click', () => this.toggleCropBg());
    }
    getCoords(e) {
        const uiLayer = document.getElementById('layer-ui');
        if (!uiLayer) return { x: 0, y: 0, screenX: e.clientX, screenY: e.clientY };
        const r = this.cachedRect || uiLayer.getBoundingClientRect();
        return { x: (e.clientX - r.left) * (this.layers.width / r.width), y: (e.clientY - r.top) * (this.layers.height / r.height), screenX: e.clientX, screenY: e.clientY };
    }
    loadFile(file) {
        if (!file) return; const r = new FileReader();
        r.onload = e => {
            const img = new Image(); img.onload = () => {
                this.img = img; this.layers.resize(img.naturalWidth, img.naturalHeight); this.layers.drawBg(img, this.state.data.filters);
                this.state.data.selections = []; this.state.data.objects = []; this.state.history = []; this.state.historyIndex = -1; this.state.pushHistory();
                const cc = this.els['canvas-container'];
                if (cc) this.state.view.scale = Math.min((cc.clientWidth - 20) / img.naturalWidth, (cc.clientHeight - 20) / img.naturalHeight);
                this.state.view.x = 0; this.state.view.y = 0; this.updateTransform();
                document.getElementById('canvas-wrapper')?.classList.remove('hidden'); document.getElementById('placeholder-msg')?.classList.add('hidden');
                if (this.els['download-btn']) this.els['download-btn'].disabled = false;
                this.fullRedraw();
            }; img.src = e.target.result;
        }; r.readAsDataURL(file);
    }
    loop() {
        if (this.flags.bg) { this.layers.drawBg(this.img, this.state.data.filters); this.flags.bg = false; }
        if (this.flags.mask) { this.layers.drawMasks(this.state.data.selections, this.state.data.filters); this.flags.mask = false; }
        if (this.flags.ui) { this.layers.drawUi(this.state); this.flags.ui = false; }
        requestAnimationFrame(() => this.loop());
    }
    fullRedraw() { this.flags.bg = this.flags.mask = this.flags.ui = true; }
    setTool(t) {
        this.state.view.tool = t;
        ['hand', 'lasso', 'eraser'].forEach(tool => {
            const btn = document.getElementById(`tool-${tool}`);
            if (btn) { btn.classList.toggle('active', t === tool); btn.classList.toggle('text-white', t === tool); }
        });
        if (this.els['canvas-container']) this.els['canvas-container'].style.cursor = t === 'hand' ? 'grab' : 'crosshair';
        if (this.els['brush-controls']) this.els['brush-controls'].classList.toggle('hidden', t !== 'eraser');
        this.flags.ui = true;
    }
    switchTab(t) {
        this.state.view.activeTab = t;
        if (this.els['panel-route']) this.els['panel-route'].classList.toggle('hidden', t !== 'route');
        if (this.els['panel-text']) this.els['panel-text'].classList.toggle('hidden', t !== 'text');
        if (this.els['tab-route']) this.els['tab-route'].className = t === 'route' ? "flex-1 py-3 text-sm font-bold text-blue-600 border-b-2 border-blue-600 bg-blue-50 transition-colors" : "flex-1 py-3 text-sm font-medium text-gray-500 hover:text-gray-700 transition-colors";
        if (this.els['tab-text']) this.els['tab-text'].className = t === 'text' ? "flex-1 py-3 text-sm font-bold text-blue-600 border-b-2 border-blue-600 bg-blue-50 transition-colors" : "flex-1 py-3 text-sm font-medium text-gray-500 hover:text-gray-700 transition-colors";
        this.setTool(t === 'route' ? 'lasso' : 'hand');
    }
    addObj(type, text, color) {
        const container = document.getElementById('canvas-container'); if (!container) return;
        const rect = container.getBoundingClientRect();
        const p = this.getCoords({ clientX: rect.left + rect.width / 2, clientY: rect.top + rect.height / 2 });
        this.state.data.objects.push({ id: this.state.data.nextObjId++, type, x: p.x, y: p.y, text: text || (document.getElementById('text-input')?.value || 'Text'), color: color || '#FFFFFF', fontSize: type === 'tag' ? 60 : 40, bgColor: null, rotation: 0 });
        const ti = document.getElementById('text-input'); if (ti) ti.value = '';
        this.state.view.activeObjId = this.state.data.nextObjId - 1;
        this.state.pushHistory(); this.flags.ui = true; this.updateUI();
    }
    pointerDown(e) {
        if (!this.img) return;
        this.cachedRect = document.getElementById('layer-ui')?.getBoundingClientRect();
        this.pointers.set(e.pointerId, e);
        const p = this.getCoords(e);
        this.state.view.isDragging = true;

        if (this.pointers.size === 2) {
            const pts = Array.from(this.pointers.values());
            const p1 = this.getCoords(pts[0]), p2 = this.getCoords(pts[1]);
            this.initialPinchDist = Math.hypot(p2.x - p1.x, p2.y - p1.y);
            this.initialPinchAngle = Math.atan2(p2.y - p1.y, p2.x - p1.x);

            if (this.state.view.activeObjId && this.state.view.activeTab === 'text') {
                this.state.view.dragType = 'pinch-obj';
                const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId);
                this.initialObjFontSize = o.fontSize; this.initialObjRotation = o.rotation || 0;
            } else {
                this.state.view.dragType = 'pinch-canvas'; this.initialCanvasScale = this.state.view.scale;
            }
            this.state.view.tempPath = []; return;
        }

        if (this.state.view.activeTab === 'text') {
            const hit = this.state.data.objects.slice().reverse().find(o => {
                let rx = p.x - o.x, ry = p.y - o.y;
                if (o.rotation) {
                    const cos = Math.cos(-o.rotation), sin = Math.sin(-o.rotation);
                    const nx = rx * cos - ry * sin, ny = rx * sin + ry * cos;
                    rx = nx; ry = ny;
                }
                if (o.type === 'text') {
                    if (this.layers.uiCtx) this.layers.uiCtx.font = `bold ${o.fontSize}px sans-serif`;
                    const w = this.layers.uiCtx ? this.layers.uiCtx.measureText(o.text).width : o.fontSize * o.text.length;
                    return rx >= -20 && rx <= w + 20 && ry >= -20 && ry <= o.fontSize + 20;
                } else {
                    const s = o.fontSize / 60; return rx >= -40 * s - 20 && rx <= 40 * s + 20 && ry >= -40 * s - 20 && ry <= 40 * s + 20;
                }
            });

            if (hit) {
                this.state.view.activeObjId = hit.id; this.state.view.dragType = 'object'; this.dragStart = { x: p.x, y: p.y, ox: hit.x, oy: hit.y };
            } else {
                this.state.view.activeObjId = null; this.state.view.dragType = 'pan'; this.dragStart = { cx: e.clientX, cy: e.clientY, vx: this.state.view.x, vy: this.state.view.y };
            }
        } else {
            if (this.state.view.tool === 'hand') { this.state.view.dragType = 'pan'; this.dragStart = { cx: e.clientX, cy: e.clientY, vx: this.state.view.x, vy: this.state.view.y }; }
            else { this.state.view.dragType = this.state.view.tool; this.state.view.tempPath = [p]; this.layers.updateLoupe(p, this.state.view.scale); }
        }
        this.updateUI(); this.flags.ui = true;
    }
    pointerMove(e) {
        if (!this.state.view.isDragging) return;
        this.pointers.set(e.pointerId, e);

        if (this.pointers.size === 2) {
            const pts = Array.from(this.pointers.values());
            const p1 = this.getCoords(pts[0]), p2 = this.getCoords(pts[1]);
            const dist = Math.hypot(p2.x - p1.x, p2.y - p1.y), angle = Math.atan2(p2.y - p1.y, p2.x - p1.x);

            if (this.state.view.dragType === 'pinch-obj') {
                const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId);
                if (o) {
                    const scaleFactor = dist / this.initialPinchDist;
                    o.fontSize = Math.max(10, Math.min(300, this.initialObjFontSize * scaleFactor));
                    o.rotation = this.initialObjRotation + (angle - this.initialPinchAngle);
                    this.flags.ui = true; this.updateUI();
                }
            } else if (this.state.view.dragType === 'pinch-canvas') {
                const scaleFactor = dist / this.initialPinchDist;
                this.state.view.scale = Math.max(0.1, Math.min(5, this.initialCanvasScale * scaleFactor));
                this.updateTransform();
            }
            return;
        }

        if (this.state.view.dragType && this.state.view.dragType.startsWith('pinch')) return;

        const p = this.getCoords(e);
        if (this.state.view.dragType === 'pan') {
            let nx = this.dragStart.vx + (e.clientX - this.dragStart.cx), ny = this.dragStart.vy + (e.clientY - this.dragStart.cy);
            const snapD = 20; let snapX = false, snapY = false;
            if (Math.abs(nx) < snapD) { nx = 0; snapX = true; }
            if (Math.abs(ny) < snapD) { ny = 0; snapY = true; }
            if (this.els['guide-v']) this.els['guide-v'].classList.toggle('hidden', !snapX);
            if (this.els['guide-h']) this.els['guide-h'].classList.toggle('hidden', !snapY);
            this.state.view.x = nx; this.state.view.y = ny; this.updateTransform();
        } else if (this.state.view.dragType === 'object') {
            const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId);
            if (o) {
                let nx = this.dragStart.ox + (p.x - this.dragStart.x), ny = this.dragStart.oy + (p.y - this.dragStart.y);
                const snapD = 20 / this.state.view.scale;
                this.state.view.snapObjX = false; this.state.view.snapObjY = false;
                const cx = this.layers.width / 2, cy = this.layers.height / 2;
                if (Math.abs(nx - cx) < snapD) { nx = cx; this.state.view.snapObjX = true; }
                if (Math.abs(ny - cy) < snapD) { ny = cy; this.state.view.snapObjY = true; }
                o.x = nx; o.y = ny; this.flags.ui = true;
            }
        } else if (this.state.view.dragType === 'lasso' || this.state.view.dragType === 'eraser') {
            const lastP = this.state.view.tempPath[this.state.view.tempPath.length - 1];
            if (!lastP || (p.x - lastP.x) ** 2 + (p.y - lastP.y) ** 2 > (2 / this.state.view.scale) ** 2) {
                this.state.view.tempPath.push(p);
            }
            this.flags.ui = true; this.layers.updateLoupe(p, this.state.view.scale);
        }
    }

    // 優化3: Douglas-Peucker 路徑簡化
    getSqSegDist(p, p1, p2) {
        let x = p1.x, y = p1.y, dx = p2.x - x, dy = p2.y - y;
        if (dx !== 0 || dy !== 0) {
            let t = ((p.x - x) * dx + (p.y - y) * dy) / (dx * dx + dy * dy);
            if (t > 1) { x = p2.x; y = p2.y; } else if (t > 0) { x += dx * t; y += dy * t; }
        }
        dx = p.x - x; dy = p.y - y; return dx * dx + dy * dy;
    }
    simplifyDPStep(points, first, last, sqTolerance, simplified) {
        let maxSqDist = sqTolerance, index;
        for (let i = first + 1; i < last; i++) {
            let sqDist = this.getSqSegDist(points[i], points[first], points[last]);
            if (sqDist > maxSqDist) { index = i; maxSqDist = sqDist; }
        }
        if (maxSqDist > sqTolerance) {
            if (index - first > 1) this.simplifyDPStep(points, first, index, sqTolerance, simplified);
            simplified.push(points[index]);
            if (last - index > 1) this.simplifyDPStep(points, index, last, sqTolerance, simplified);
        }
    }
    simplifyPath(points, tolerance) {
        if (points.length <= 2) return points;
        const sqTolerance = tolerance * tolerance;
        const simplified = [points[0]];
        this.simplifyDPStep(points, 0, points.length - 1, sqTolerance, simplified);
        simplified.push(points[points.length - 1]);
        return simplified;
    }

    isPointInPoly(p, poly) {
        let inside = false;
        for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
            const xi = poly[i].x, yi = poly[i].y, xj = poly[j].x, yj = poly[j].y;
            const intersect = ((yi > p.y) !== (yj > p.y)) && (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi);
            if (intersect) inside = !inside;
        }
        return inside;
    }

    triggerFlash(id) {
        const el = document.getElementById(`sel-item-${id}`);
        if (el) {
            el.classList.remove('flash-active');
            void el.offsetWidth; // trigger reflow
            el.classList.add('flash-active');
        }
    }

    pointerUp(e) {
        this.pointers.delete(e.pointerId);
        if (this.pointers.size === 0) this.cachedRect = null;

        if (this.state.view.dragType && this.state.view.dragType.startsWith('pinch')) {
            if (this.pointers.size === 0) {
                if (this.state.view.dragType === 'pinch-obj') this.state.pushHistory();
                this.state.view.isDragging = false; this.state.view.dragType = null;
            }
            return;
        }

        if (!this.state.view.isDragging) return;

        this.layers.hideLoupe();
        if (this.els['guide-v']) this.els['guide-v'].classList.add('hidden');
        if (this.els['guide-h']) this.els['guide-h'].classList.add('hidden');

        const type = this.state.view.dragType;

        // 簡化路徑，容許度根據縮放比例動態調整
        const path = this.simplifyPath(this.state.view.tempPath, 2.5 / this.state.view.scale);

        if (type === 'lasso' && path.length > 3) {
            let mId = null; const w = this.layers.width, h = this.layers.height;
            let minX = w, maxX = 0, minY = h, maxY = 0;
            path.forEach(p => { if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x; if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y; });

            for (const s of this.state.data.selections) {
                if (!s.mask || s.locked || s.visible === false) continue; // 鎖定或隱藏則跳過碰撞

                let isOverlap = false;
                for (let i = 0; i < path.length; i += 2) {
                    const px = Math.floor(path[i].x), py = Math.floor(path[i].y);
                    for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++) {
                        const nx = px + dx, ny = py + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h && s.mask[ny * w + nx] > 0) { isOverlap = true; break; }
                    }
                    if (isOverlap) break;
                }
                if (isOverlap) { mId = s.id; break; }

                let samplePoint = null;
                for (let y = Math.floor(minY); y <= Math.ceil(maxY); y += 5) {
                    for (let x = Math.floor(minX); x <= Math.ceil(maxX); x += 5) {
                        if (y >= 0 && y < h && x >= 0 && x < w && s.mask[y * w + x] > 0) {
                            samplePoint = { x, y }; break;
                        }
                    }
                    if (samplePoint) break;
                }
                if (samplePoint && this.isPointInPoly(samplePoint, path)) {
                    mId = s.id;
                }
                if (mId) break;
            }

            let sel;
            if (mId) {
                sel = this.state.data.selections.find(s => s.id === mId);
                sel.actions.push({ type: 'lasso', path });
                sel.mask = sel.borderMask = null;
                this.state.view.activeSelId = mId;
                // 優化4: 合併閃爍反饋
                this.triggerFlash(mId);
            } else {
                let prevParams = { tolerance: 60, expand: 0, fillHoles: true, border: true, borderThickness: 5, borderColor: '#EC4899' };
                const sourceSel = this.state.data.selections.find(s => s.id === this.state.view.activeSelId) || this.state.data.selections[this.state.data.selections.length - 1];
                if (sourceSel) prevParams = { ...sourceSel.params };

                const id = this.state.data.nextSelId++;
                sel = {
                    id, actions: [{ type: 'lasso', path }], params: prevParams,
                    mask: null, borderMask: null, detectedColor: null, visible: true, locked: false
                };
                this.state.data.selections.push(sel);
                this.state.view.activeSelId = id;
            }
            this.requestWorker(sel); this.state.pushHistory(); this.updateUI();
        } else if (type === 'eraser' && path.length > 0) {
            if (this.state.view.activeSelId) {
                let sel = this.state.data.selections.find(s => s.id === this.state.view.activeSelId);
                if (sel && !sel.locked && sel.visible !== false) {
                    sel.actions.push({ type: 'erase', path, size: this.state.view.brushSize });
                    sel.mask = sel.borderMask = null;
                    this.requestWorker(sel); this.state.pushHistory();
                }
            }
        } else if (type === 'object') {
            this.state.pushHistory();
        }

        this.state.view.tempPath = []; this.state.view.dragType = null;
        this.state.view.snapObjX = false; this.state.view.snapObjY = false;
        this.state.view.isDragging = false; this.flags.ui = true;
    }
    adjSlider(id, v) {
        const el = document.getElementById(id);
        if (el) { el.value = parseFloat(el.value) + v; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change')); }
    }
    updateObjParam(k, v) {
        const o = this.state.data.objects.find(obj => obj.id === this.state.view.activeObjId);
        if (o) { o[k] = v; this.flags.ui = true; this.updateUI(); this.state.pushHistory(); }
    }
    requestWorker(sel) { const pb = document.getElementById('processing-bar'); if (pb) pb.classList.remove('hidden'); this.worker.postMessage({ type: 'CALC_MASK', sel, w: this.layers.width, h: this.layers.height, bgData: this.layers.bgBuffer }); }
    updateTransform() { const r = document.getElementById('canvas-transform-root'); if (r) r.style.transform = `translate(${this.state.view.x}px, ${this.state.view.y}px) scale(${this.state.view.scale})`; }

    toggleSelVisibility(id) {
        const s = this.state.data.selections.find(x => x.id === id);
        if (s) { s.visible = s.visible === false ? true : false; this.flags.mask = true; this.updateUI(); this.state.pushHistory(); }
    }
    toggleSelLock(id) {
        const s = this.state.data.selections.find(x => x.id === id);
        if (s) { s.locked = !s.locked; this.updateUI(); this.state.pushHistory(); }
    }

    updateUI(force) {
        if (force) this.fullRedraw();
        if (this.els['undo-btn']) this.els['undo-btn'].disabled = this.state.historyIndex <= 0; if (this.els['redo-btn']) this.els['redo-btn'].disabled = this.state.historyIndex >= this.state.history.length - 1;
        const list = this.els['color-list']; if (!list) return; list.innerHTML = '';
        if (this.state.data.selections.length === 0) { list.innerHTML = `<div class="text-center py-6 text-gray-400 text-sm italic border-2 border-dashed border-gray-100 rounded-lg">${this.t('emptySelection')}</div>`; }
        else {
            this.state.data.selections.forEach((sel, i) => {
                const active = sel.id === this.state.view.activeSelId;
                const div = document.createElement('div');
                div.id = `sel-item-${sel.id}`;
                div.className = `list-item bg-white border ${active ? 'border-blue-500 shadow-md ring-1 ring-blue-100' : 'border-gray-200'} rounded-lg overflow-hidden mb-2`;

                div.onclick = (e) => {
                    if (e.target.tagName !== 'INPUT' && e.target.tagName !== 'BUTTON' && !e.target.closest('button')) {
                        this.state.view.activeSelId = (active ? null : sel.id); this.updateUI();
                    }
                };

                const cols = SHARED_COLORS.map(c => { const isS = (sel.params.borderColor || '#EC4899').toLowerCase() === c.toLowerCase(); return `<button class="w-6 h-6 rounded-full border border-gray-200 shadow-sm ${isS ? 'ring-2 ring-offset-1 ring-blue-500 scale-110' : ''}" style="background-color: ${c}" onclick="app.updateSelParam(${sel.id}, 'borderColor', '${c}')"></button>`; }).join('');
                const badgeColor = sel.detectedColor || '#EC4899';
                const locked = sel.locked;
                const visible = sel.visible !== false;

                const expanded = active ? `
                            <div class="p-3 bg-gray-50 border-t border-gray-100 space-y-4 ${locked ? 'opacity-50 pointer-events-none' : ''}">
                                <div><div class="flex justify-between mb-1 text-[10px] font-bold text-gray-500 uppercase">${this.t('tolerance')} <span class="text-blue-600">${sel.params.tolerance}</span></div><div class="slider-group flex items-center"><button class="slider-btn text-gray-500 px-1" data-target="sel-tol-${sel.id}" data-delta="-1">-</button><input type="range" id="sel-tol-${sel.id}" min="0" max="127" value="${sel.params.tolerance}" onchange="app.updateSelParam(${sel.id}, 'tolerance', this.value)" class="h-1 bg-gray-200 rounded-lg appearance-none flex-1 accent-blue-500 mx-1"><button class="slider-btn text-gray-500 px-1" data-target="sel-tol-${sel.id}" data-delta="1">+</button></div></div>
                                <div><div class="flex justify-between mb-1 text-[10px] font-bold text-gray-500 uppercase">${this.t('expand')} <span class="text-purple-600">${sel.params.expand}px</span></div><div class="slider-group flex items-center"><button class="slider-btn text-gray-500 px-1" data-target="sel-exp-${sel.id}" data-delta="-1">-</button><input type="range" id="sel-exp-${sel.id}" min="0" max="20" value="${sel.params.expand}" onchange="app.updateSelParam(${sel.id}, 'expand', this.value)" class="h-1 bg-gray-200 rounded-lg appearance-none flex-1 accent-purple-500 mx-1"><button class="slider-btn text-gray-500 px-1" data-target="sel-exp-${sel.id}" data-delta="1">+</button></div></div>
                                <div class="flex items-center justify-between pt-1">
                                    <label class="flex items-center text-xs text-gray-700 font-bold"><input type="checkbox" onchange="app.updateSelParam(${sel.id}, 'fillHoles', this.checked)" ${sel.params.fillHoles ? 'checked' : ''} class="mr-2 w-4 h-4">${this.t('autoFill')}</label>
                                    <label class="flex items-center text-xs text-gray-700 font-bold"><input type="checkbox" onchange="app.updateSelParam(${sel.id}, 'border', this.checked)" ${sel.params.border ? 'checked' : ''} class="mr-2 w-4 h-4">${this.t('showBorder')}</label>
                                </div>
                                ${sel.params.border ? `<div class="mt-3 pt-3 border-t border-gray-100">
                                    <div class="flex items-center justify-between mb-2 text-[10px] font-bold text-gray-500 uppercase">
                                        <span>${this.t('borderThickness')} <span class="text-blue-600">${sel.params.borderThickness}px</span></span>
                                        <div class="slider-group flex items-center w-24"><button class="slider-btn text-gray-500 px-1" data-target="sel-bt-inp-${sel.id}" data-delta="-1">-</button><input type="range" id="sel-bt-inp-${sel.id}" min="1" max="20" value="${sel.params.borderThickness}" class="h-1 bg-gray-200 rounded-lg appearance-none flex-1 accent-blue-500 mx-1" onchange="app.updateSelParam(${sel.id}, 'borderThickness', this.value)"><button class="slider-btn text-gray-500 px-1" data-target="sel-bt-inp-${sel.id}" data-delta="1">+</button></div>
                                    </div>
                                    <div class="flex flex-wrap gap-2 pt-1">${cols}</div>
                                </div>` : ''}
                            </div>` : '';

                // 優化2: 圖層管理按鈕 (顯示/隱藏, 鎖定)
                div.innerHTML = `<div class="flex items-center p-3 cursor-pointer hover:bg-gray-50 transition-colors ${!visible ? 'opacity-50' : ''}">
                            <div class="w-6 h-6 rounded flex items-center justify-center font-bold text-[10px] shadow-sm border border-black/10" style="background-color: ${badgeColor}; color: ${badgeColor === '#FFFFFF' ? '#000' : '#FFF'};">${i + 1}</div>
                            <div class="flex-1 ml-3 text-sm font-bold text-gray-700 truncate">${this.t('holdNum')}${i + 1}</div>
                            
                            <div class="flex items-center gap-1">
                                <button onclick="app.toggleSelVisibility(${sel.id})" class="text-gray-400 hover:text-blue-500 p-1.5" title="Toggle Visibility">
                                    ${visible ? `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"></path></svg>` :
                        `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.543-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.88 9.88l-3.29-3.29m7.532 7.532l3.29 3.29M3 3l3.59 3.59m0 0A9.953 9.953 0 0112 5c4.478 0 8.268 2.943 9.543 7a10.025 10.025 0 01-4.132 5.411m0 0L21 21"></path></svg>`}
                                </button>
                                <button onclick="app.toggleSelLock(${sel.id})" class="text-gray-400 hover:text-blue-500 p-1.5" title="Lock/Unlock">
                                    ${locked ? `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z"></path></svg>` :
                        `<svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 11V7a4 4 0 118 0m-4 8v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2z"></path></svg>`}
                                </button>
                                <button onclick="app.removeSel(${sel.id})" class="text-gray-400 hover:text-red-500 p-1.5"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg></button>
                            </div>
                        </div>${expanded}`;
                list.appendChild(div);
            });
        }
        const actO = this.state.data.objects.find(o => o.id === this.state.view.activeObjId);
        const tc = document.getElementById('text-controls');
        if (tc) {
            if (actO && this.state.view.activeTab === 'text') {
                tc.classList.remove('opacity-50', 'pointer-events-none');
                const ts = document.getElementById('text-size'); if (ts) ts.value = actO.fontSize;
                const tr = document.getElementById('text-rotation'); if (tr) tr.value = Math.round((actO.rotation || 0) * 180 / Math.PI);
                const tcp = document.getElementById('text-color-palette');
                if (tcp) tcp.innerHTML = SHARED_COLORS.map(c => `<button class="w-6 h-6 rounded-full border border-gray-200 shadow-sm ${actO.color === c ? 'ring-2 ring-offset-1 ring-blue-500 scale-110' : ''}" style="background-color: ${c}" onclick="app.updateObjParam('color', '${c}')"></button>`).join('');
                const tbgp = document.getElementById('text-bg-color-palette');
                if (tbgp) {
                    const noneBtn = `<button class="w-6 h-6 rounded-full border border-gray-200 shadow-sm flex items-center justify-center ${!actO.bgColor ? 'ring-2 ring-offset-1 ring-red-500 scale-110' : ''}" style="background-color: white" onclick="app.updateObjParam('bgColor', null)"><svg class="w-4 h-4 text-red-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg></button>`;
                    const colorBtns = SHARED_COLORS.map(c => `<button class="w-6 h-6 rounded-full border border-gray-200 shadow-sm ${actO.bgColor === c ? 'ring-2 ring-offset-1 ring-blue-500 scale-110' : ''}" style="background-color: ${c}" onclick="app.updateObjParam('bgColor', '${c}')"></button>`).join('');
                    tbgp.innerHTML = colorBtns + noneBtn;
                }
            } else {
                tc.classList.add('opacity-50', 'pointer-events-none');
            }
        }
    }
    updateSelParam(id, k, v) { const s = this.state.data.selections.find(x => x.id === id); if (s) { s.params[k] = (k === 'fillHoles' || k === 'border' || k === 'borderColor') ? v : parseInt(v); if (k === 'borderColor') { this.flags.mask = true; this.updateUI(); } else { s.mask = s.borderMask = null; this.requestWorker(s); } this.state.pushHistory(); if (k === 'border') this.updateUI(); } }
    toggleCrop() {
        let idx = (this.state.data.crop.index + 1) % CROPS.length; this.state.data.crop.index = idx;
        const crop = CROPS[idx];
        const cl = document.getElementById('crop-label'); if (cl) { cl.innerText = crop.label; cl.classList.toggle('hidden', idx === 0); }
        const cf = document.getElementById('crop-frame'), cc = document.getElementById('crop-mask-container');
        const cbgc = document.getElementById('crop-bg-container'), cbgl = document.getElementById('crop-bg-layer');

        if (cf && cc && cbgc && cbgl) {
            if (idx === 0) {
                cc.classList.add('hidden'); cf.style.display = 'none'; cbgc.classList.add('hidden'); document.getElementById('crop-bg-color')?.classList.add('hidden');
            } else {
                cc.classList.remove('hidden'); cf.style.display = 'block'; cbgc.classList.remove('hidden');
                cbgl.style.backgroundColor = this.state.data.crop.bgWhite ? 'white' : 'black'; document.getElementById('crop-bg-color')?.classList.remove('hidden');
                const r = document.getElementById('canvas-container').getBoundingClientRect(), aspect = crop.w / crop.h;
                let w = r.width - 40, h = w / aspect; if (h > r.height - 40) { h = r.height - 40; w = h * aspect; }
                cf.style.width = w + 'px'; cf.style.height = h + 'px'; cbgl.style.width = w + 'px'; cbgl.style.height = h + 'px';
            }
        }
        if (idx !== 0 && this.state.view.activeTab === 'route') this.setTool('hand');
    }
    toggleCropBg() { this.state.data.crop.bgWhite = !this.state.data.crop.bgWhite; const cbgl = document.getElementById('crop-bg-layer'); if (cbgl) cbgl.style.backgroundColor = this.state.data.crop.bgWhite ? 'white' : 'black'; }
    removeSel(id) { this.state.data.selections = this.state.data.selections.filter(s => s.id !== id); if (this.state.view.activeSelId === id) this.state.view.activeSelId = null; this.flags.mask = true; this.updateUI(); this.state.pushHistory(); }
    export() {
        const crop = CROPS[this.state.data.crop.index];
        let outW = this.layers.width, outH = this.layers.height, srcRect = { x: 0, y: 0, w: outW, h: outH };

        if (crop.w > 0 && crop.h > 0) {
            const r = document.getElementById('canvas-container').getBoundingClientRect(), aspect = crop.w / crop.h;
            let cw = r.width - 40, ch = cw / aspect; if (ch > r.height - 40) { ch = r.height - 40; cw = ch * aspect; }
            outW = Math.round(cw / this.state.view.scale); outH = Math.round(ch / this.state.view.scale);
            srcRect.x = Math.round((this.layers.width / 2) - (this.state.view.x / this.state.view.scale) - (outW / 2));
            srcRect.y = Math.round((this.layers.height / 2) - (this.state.view.y / this.state.view.scale) - (outH / 2));
            srcRect.w = outW; srcRect.h = outH;
        }

        const outC = document.createElement('canvas'); outC.width = outW; outC.height = outH; const ctx = outC.getContext('2d');
        if (crop.w > 0 && crop.h > 0) { ctx.fillStyle = this.state.data.crop.bgWhite ? 'white' : 'black'; ctx.fillRect(0, 0, outW, outH); }

        ctx.save(); ctx.translate(-srcRect.x, -srcRect.y);
        ctx.drawImage(document.getElementById('layer-bg'), 0, 0); ctx.drawImage(document.getElementById('layer-mask'), 0, 0); ctx.drawImage(document.getElementById('layer-ui'), 0, 0);
        ctx.restore();

        const a = document.createElement('a'); a.download = 'holdfocus_export.png'; a.href = outC.toDataURL('image/png', 1.0); a.click();
    }
}
window.HoldFocusApp = HoldFocusApp;