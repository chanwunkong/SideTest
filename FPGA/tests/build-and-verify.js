// TASK-007 標準測試電路驗證腳本
//
// 這個腳本不是模擬器的一部分，是離線驗證工具：它逐字重現 FPGA/FPGA.html 裡的核心模擬邏輯
// （BitWriter/BitReader、calcLut3、getFFInput、getIobInEffective、simulateCombinatorial 的
// 佈線鬆弛演算法、stepClock 的正反器鎖存行為），在 Node 環境下對兩個手工佈線的測試電路
// （2-to-4 解碼器、3-bit 同步計數器）跑功能驗證，並把電路配置匯出成與模擬器相容的 .bit
// 檔（供人工在瀏覽器用「匯入 Bitstream」按鈕載入互動確認）。
//
// 執行：node FPGA/tests/build-and-verify.js

const fs = require('fs');
const path = require('path');

// ---- 以下區塊逐字對應 FPGA/FPGA.html 的同名函式/常數 ----

const SWITCH_LINKS = [
    ['WE'], ['NS'], ['WN'], ['WS'], ['EN'], ['ES']
];
const FF_D_SRC_CODES = ['F', 'G', 'F_XOR_G', 'F_AND_G', 'F_OR_G'];
const THRESHOLD_CODES = ['TTL', 'CMOS'];
const BITSTREAM_MAGIC = [0x58, 0x53, 0x49, 0x4D];
const BITSTREAM_VERSION = 1;

class BitWriter {
    constructor() { this.bytes = []; this.cur = 0; this.nbits = 0; }
    writeBits(value, n) {
        for (let i = n - 1; i >= 0; i--) {
            const bit = (value >> i) & 1;
            this.cur = (this.cur << 1) | bit;
            this.nbits++;
            if (this.nbits === 8) { this.bytes.push(this.cur); this.cur = 0; this.nbits = 0; }
        }
    }
    finish() {
        if (this.nbits > 0) {
            this.cur = this.cur << (8 - this.nbits);
            this.bytes.push(this.cur);
            this.cur = 0; this.nbits = 0;
        }
        return new Uint8Array(this.bytes);
    }
}

function calcLut3(mask, v0, v1, v2) {
    const idx = ((v0 & 1) << 2) | ((v1 & 1) << 1) | (v2 & 1);
    return (mask >> idx) & 1;
}

function getFFInput(n) {
    switch (n.ff_d_src) {
        case 'G': return n.val_G;
        case 'F_XOR_G': return n.val_F ^ n.val_G;
        case 'F_AND_G': return n.val_F & n.val_G;
        case 'F_OR_G': return n.val_F | n.val_G;
        case 'F':
        default: return n.val_F;
    }
}

function getIobInEffective(inp) {
    if (inp.ff_enabled) return inp.reg_val;
    return inp.forced ? inp.val : (inp.pull_up ? 1 : 0);
}

// 對應 serializeBitstream()，改為接受傳入的狀態物件而非全域變數
function serializeBitstream(state) {
    const { GRID_SIZE, clbs, switch_box, h_wires, v_wires, iob_in, iob_out } = state;
    const w = new BitWriter();
    for (let r = 0; r < GRID_SIZE; r++) {
        for (let c = 0; c < GRID_SIZE; c++) {
            const n = clbs[r][c];
            w.writeBits(n.lut_f, 8);
            w.writeBits(n.lut_g, 8);
            w.writeBits(Math.max(0, FF_D_SRC_CODES.indexOf(n.ff_d_src)), 3);
            w.writeBits(n.mux_x === 'Q' ? 1 : 0, 1);
            w.writeBits(n.mux_y === 'Q' ? 1 : 0, 1);
        }
    }
    for (let r = 0; r <= GRID_SIZE; r++) {
        for (let c = 0; c <= GRID_SIZE; c++) {
            const sw = switch_box[r][c];
            for (const [key] of SWITCH_LINKS) w.writeBits(sw[key] ? 1 : 0, 1);
        }
    }
    for (let r = 0; r <= GRID_SIZE; r++) {
        for (let c = 0; c < GRID_SIZE; c++) w.writeBits(h_wires[r][c].on ? 1 : 0, 1);
    }
    for (let r = 0; r < GRID_SIZE; r++) {
        for (let c = 0; c <= GRID_SIZE; c++) w.writeBits(v_wires[r][c].on ? 1 : 0, 1);
    }
    for (let i = 0; i < GRID_SIZE; i++) {
        const inp = iob_in[i];
        w.writeBits(inp.forced ? 1 : 0, 1);
        w.writeBits(inp.val ? 1 : 0, 1);
        w.writeBits(inp.pull_up ? 1 : 0, 1);
        w.writeBits(Math.max(0, THRESHOLD_CODES.indexOf(inp.threshold)), 1);
        w.writeBits(inp.ff_enabled ? 1 : 0, 1);
    }
    for (let i = 0; i < GRID_SIZE; i++) {
        const out = iob_out[i];
        w.writeBits(out.tri_state ? 1 : 0, 1);
        w.writeBits(Math.max(0, THRESHOLD_CODES.indexOf(out.threshold)), 1);
    }
    const payload = w.finish();
    const full = new Uint8Array(6 + payload.length);
    full.set(BITSTREAM_MAGIC, 0);
    full[4] = BITSTREAM_VERSION;
    full[5] = GRID_SIZE;
    full.set(payload, 6);
    return full;
}

// 對應 simulateCombinatorial() 的佈線鬆弛演算法（拿掉 canvas/DOM 部分）
function simulateCombinatorial(state) {
    const { GRID_SIZE, clbs, switch_box, h_wires, v_wires, iob_in, iob_out } = state;

    for (let r = 0; r <= GRID_SIZE; r++) {
        for (let c = 0; c < GRID_SIZE; c++) h_wires[r][c].val = 0;
        for (let c = 0; c <= GRID_SIZE; c++) if (r < GRID_SIZE) v_wires[r][c].val = 0;
    }

    for (let iter = 0; iter < 6; iter++) {
        for (let r = 0; r < GRID_SIZE; r++) {
            if (h_wires[r][0].on) h_wires[r][0].val |= getIobInEffective(iob_in[r]);
        }

        for (let r = 0; r <= GRID_SIZE; r++) {
            for (let c = 0; c <= GRID_SIZE; c++) {
                const sw = switch_box[r][c];
                const pins = {
                    W: (c > 0 && h_wires[r][c - 1].on) ? h_wires[r][c - 1] : null,
                    E: (c < GRID_SIZE && h_wires[r][c].on) ? h_wires[r][c] : null,
                    N: (r > 0 && v_wires[r - 1][c].on) ? v_wires[r - 1][c] : null,
                    S: (r < GRID_SIZE && v_wires[r][c].on) ? v_wires[r][c] : null
                };
                for (const [key] of SWITCH_LINKS) {
                    const a = pins[key[0]], b = pins[key[1]];
                    if (sw[key] && a && b) {
                        const v = a.val | b.val;
                        a.val = v; b.val = v;
                    }
                }
            }
        }

        for (let r = 0; r < GRID_SIZE; r++) {
            for (let c = 0; c < GRID_SIZE; c++) {
                const n = clbs[r][c];
                n.in_A = v_wires[r][c].on ? v_wires[r][c].val : 0;
                n.in_B = h_wires[r][c].on ? h_wires[r][c].val : 0;
                n.in_C = v_wires[r][c + 1].on ? v_wires[r][c + 1].val : 0;
                n.in_D = h_wires[r + 1][c].on ? h_wires[r + 1][c].val : 0;

                n.val_F = calcLut3(n.lut_f, n.in_A, n.in_B, n.in_C);
                n.val_G = calcLut3(n.lut_g, n.in_A, n.in_B, n.in_D);

                n.out_X = (n.mux_x === 'F') ? n.val_F : n.val_Q;
                n.out_Y = (n.mux_y === 'G') ? n.val_G : n.val_Q;

                if (v_wires[r][c + 1].on) v_wires[r][c + 1].val |= n.out_X;
                if (h_wires[r + 1][c].on) h_wires[r + 1][c].val |= n.out_Y;
            }
        }
    }

    for (let r = 0; r < GRID_SIZE; r++) {
        iob_out[r].val = h_wires[r][GRID_SIZE - 1].on ? h_wires[r][GRID_SIZE - 1].val : 0;
    }
}

// 對應 stepClock() 在上升緣所做的事（不含 clockVal 切換與 DOM 顯示）
function latchRisingEdge(state) {
    state.clbs.forEach(row => row.forEach(n => { n.val_Q = getFFInput(n); }));
    state.iob_in.forEach(inp => {
        inp.reg_val = inp.forced ? inp.val : (inp.pull_up ? 1 : 0);
    });
}

// ---- 建立空白晶片狀態（對應 buildGrid() 的預設值）----
function makeBlankState(GRID_SIZE) {
    const clbs = [];
    for (let r = 0; r < GRID_SIZE; r++) {
        clbs[r] = [];
        for (let c = 0; c < GRID_SIZE; c++) {
            clbs[r][c] = {
                lut_f: 0xF0, lut_g: 0x00, ff_d_src: 'F', mux_x: 'F', mux_y: 'G',
                in_A: 0, in_B: 0, in_C: 0, in_D: 0,
                val_F: 0, val_G: 0, val_Q: 0, out_X: 0, out_Y: 0
            };
        }
    }
    const h_wires = [];
    const switch_box = [];
    for (let r = 0; r <= GRID_SIZE; r++) {
        h_wires[r] = Array.from({ length: GRID_SIZE }, () => ({ on: false, val: 0 }));
        switch_box[r] = Array.from({ length: GRID_SIZE + 1 }, () => ({ WE: false, NS: false, WN: false, WS: false, EN: false, ES: false }));
    }
    const v_wires = [];
    for (let r = 0; r < GRID_SIZE; r++) {
        v_wires[r] = Array.from({ length: GRID_SIZE + 1 }, () => ({ on: false, val: 0 }));
    }
    const iob_in = [];
    const iob_out = [];
    for (let i = 0; i < GRID_SIZE; i++) {
        iob_in.push({ val: 0, forced: true, pull_up: false, threshold: 'TTL', ff_enabled: false, reg_val: 0 });
        iob_out.push({ val: 0, tri_state: false, threshold: 'TTL' });
    }
    return { GRID_SIZE, clbs, h_wires, v_wires, switch_box, iob_in, iob_out };
}

function setWire(state, kind, r, c, on) {
    (kind === 'h' ? state.h_wires : state.v_wires)[r][c].on = on;
}

// ==================================================================================
// 電路 1：2-to-4 解碼器（純組合邏輯）
// 佈局見 FPGA/tests/README.md「電路 1」章節。GRID_SIZE=4，只用到 (row0-3, col0-1)。
// ==================================================================================
function buildDecoder() {
    const st = makeBlankState(4);
    const c = st.clbs;

    // row0/row1 對：S1(row0) 經 CLB(0,0)/(0,1) relay 給第一組 combine CLB(1,1)
    c[0][0] = { ...c[0][0], lut_f: 0xCC, lut_g: 0x00, mux_x: 'F', mux_y: 'G' }; // F=passB=S1, Y=0(不干擾 IOB1)
    c[0][1] = { ...c[0][1], lut_f: 0xF0, lut_g: 0xF0, mux_x: 'F', mux_y: 'G' }; // F=G=passA=S1（往右也往下 relay）
    c[1][0] = { ...c[1][0], lut_f: 0xCC, lut_g: 0x00, mux_x: 'F', mux_y: 'G' }; // F=passB=S0, Y=0(不干擾 IOB2)
    c[1][1] = { ...c[1][1], lut_f: 0x03, lut_g: 0x30, mux_x: 'F', mux_y: 'G' }; // F=O0=NOT A AND NOT B, G=O1=A AND NOT B

    // row2/row3 對：S1 的重複輸入（IOB2）與 S0 的重複輸入（IOB3），組成第二組 combine CLB(3,1)
    c[2][0] = { ...c[2][0], lut_f: 0xCC, lut_g: 0x00, mux_x: 'F', mux_y: 'G' };
    c[2][1] = { ...c[2][1], lut_f: 0xF0, lut_g: 0xF0, mux_x: 'F', mux_y: 'G' };
    c[3][0] = { ...c[3][0], lut_f: 0xCC, lut_g: 0x00, mux_x: 'F', mux_y: 'G' };
    c[3][1] = { ...c[3][1], lut_f: 0x0C, lut_g: 0xC0, mux_x: 'F', mux_y: 'G' }; // F=O2=B AND NOT A, G=O3=A AND B

    setWire(st, 'h', 0, 0, true); // IOB0 -> CLB(0,0).B
    setWire(st, 'v', 0, 1, true); // CLB(0,0).X -> CLB(0,1).A
    setWire(st, 'h', 1, 1, true); // CLB(0,1).Y -> CLB(1,1).B
    setWire(st, 'h', 1, 0, true); // IOB1 -> CLB(1,0).B（同時是 CLB(0,0).Y 目標，Y=0 故不干擾）
    setWire(st, 'v', 1, 1, true); // CLB(1,0).X -> CLB(1,1).A
    setWire(st, 'h', 2, 0, true); // IOB2 -> CLB(2,0).B（同時是 CLB(1,0).Y 目標，Y=0 故不干擾）
    setWire(st, 'v', 2, 1, true); // CLB(2,0).X -> CLB(2,1).A
    setWire(st, 'h', 3, 1, true); // CLB(2,1).Y -> CLB(3,1).B
    setWire(st, 'h', 3, 0, true); // IOB3 -> CLB(3,0).B（同時是 CLB(2,0).Y 目標，Y=0 故不干擾）
    setWire(st, 'v', 3, 1, true); // CLB(3,0).X -> CLB(3,1).A

    return st;
}

function runDecoderTests() {
    console.log('=== 電路 1：2-to-4 解碼器 ===');
    let allPass = true;
    for (const [s1, s0] of [[0, 0], [0, 1], [1, 0], [1, 1]]) {
        const st = buildDecoder();
        st.iob_in[0].val = s1; // S1
        st.iob_in[1].val = s0; // S0
        st.iob_in[2].val = s1; // S1（重複輸入，供第二組 combine 使用）
        st.iob_in[3].val = s0; // S0（重複輸入）
        simulateCombinatorial(st);

        const o0 = st.clbs[1][1].val_F, o1 = st.clbs[1][1].val_G;
        const o2 = st.clbs[3][1].val_F, o3 = st.clbs[3][1].val_G;
        const expected = [0, 0, 0, 0];
        expected[s1 * 2 + s0] = 1;
        const got = [o0, o1, o2, o3];
        const pass = got.every((v, i) => v === expected[i]);
        allPass = allPass && pass;
        console.log(`  S1=${s1} S0=${s0} -> O0..O3 = [${got.join(',')}] (期望 [${expected.join(',')}]) ${pass ? 'PASS' : 'FAIL'}`);
    }
    console.log(allPass ? '解碼器：全部通過\n' : '解碼器：有測項失敗\n');
    return allPass;
}

// ==================================================================================
// 電路 2：3-bit 同步二進位計數器
// 佈局見 FPGA/tests/README.md「電路 2」章節。GRID_SIZE=2，用到全部 (row0-1, col0-1)。
// 註：TASK-007 原提案為「例如 4-bit 計數器」，此處縮小為 3-bit ——
// 本模擬器只有「相鄰直接串接」的繞線（無一般性 fan-out routing），4-bit 版本需要的
// AND-tree relay 網路會大幅增加手工佈線複雜度且無助於驗證引擎本身正確性，故縮小範圍，
// 已在此註記。
// ==================================================================================
function buildCounter() {
    const st = makeBlankState(2);
    const c = st.clbs;

    // bit0: T-FF，每個 clock 都翻轉。G = NOT D（D 為自己上一輪的 Y=Q 回授）
    c[0][0] = { ...c[0][0], lut_f: 0x00, lut_g: 0x55, ff_d_src: 'G', mux_x: 'Q', mux_y: 'Q' };
    // bit1: D1 = Q0 XOR Q1。F = passA(=Q0，來自 bit0.X)，G = passD(=Q1 自身回授)
    c[0][1] = { ...c[0][1], lut_f: 0xF0, lut_g: 0xAA, ff_d_src: 'F_XOR_G', mux_x: 'F', mux_y: 'Q' };
    // Q0-relay：把 bit0 的自迴授線（同時也是 CLB(1,0) 的 B 輸入）往右送給 bit2 的 A
    c[1][0] = { ...c[1][0], lut_f: 0xCC, lut_g: 0x00, mux_x: 'F', mux_y: 'G' };
    // bit2: D2 = (Q0 AND Q1) XOR Q2。F = AND(A,B) = AND(Q0,Q1)，G = passD(=Q2 自身回授)
    c[1][1] = { ...c[1][1], lut_f: 0xC0, lut_g: 0xAA, ff_d_src: 'F_XOR_G', mux_x: 'F', mux_y: 'Q' };

    setWire(st, 'h', 1, 0, true); // bit0 自迴授（Y->D） 同時 relay 給 CLB(1,0).B
    setWire(st, 'h', 1, 1, true); // bit1 自迴授（Y->D） 同時 relay 給 bit2.B
    setWire(st, 'h', 2, 1, true); // bit2 自迴授（Y->D）
    setWire(st, 'v', 0, 1, true); // bit0.X -> bit1.A
    setWire(st, 'v', 1, 1, true); // CLB(1,0).X -> bit2.A

    return st;
}

function runCounterTest() {
    console.log('=== 電路 2：3-bit 同步計數器 ===');
    const st = buildCounter();
    let allPass = true;
    for (let expectedCount = 0; expectedCount < 16; expectedCount++) {
        simulateCombinatorial(st); // 讓組合邏輯先穩定（含 D 的計算）
        const q0 = st.clbs[0][0].val_Q, q1 = st.clbs[0][1].val_Q, q2 = st.clbs[1][1].val_Q;
        const count = q2 * 4 + q1 * 2 + q0;
        const expected = expectedCount % 8;
        const pass = count === expected;
        allPass = allPass && pass;
        console.log(`  clock edge #${expectedCount}: Q2Q1Q0=${q2}${q1}${q0} (count=${count}, 期望=${expected}) ${pass ? 'PASS' : 'FAIL'}`);
        latchRisingEdge(st); // 上升緣鎖入下一個狀態
    }
    console.log(allPass ? '計數器：全部通過（含一次完整 wrap-around 0→7→0）\n' : '計數器：有測項失敗\n');
    return allPass;
}

// ---- 執行驗證並輸出 .bit 檔 ----
const decoderPass = runDecoderTests();
const counterPass = runCounterTest();

const outDir = __dirname;
const decoderBytes = serializeBitstream(buildDecoder());
const counterBytes = serializeBitstream(buildCounter());
fs.writeFileSync(path.join(outDir, 'decoder-2to4.bit'), Buffer.from(decoderBytes));
fs.writeFileSync(path.join(outDir, 'counter-3bit.bit'), Buffer.from(counterBytes));
console.log(`已寫入 decoder-2to4.bit（${decoderBytes.length} bytes）與 counter-3bit.bit（${counterBytes.length} bytes）`);

if (!decoderPass || !counterPass) {
    console.error('驗證失敗，請檢查上方輸出');
    process.exit(1);
}
console.log('全部電路驗證通過。');
