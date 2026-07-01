import { PieceDef } from "../../engine/types";

// ─── 車 (Chariot / Rook) — slides any distance on rank/file ──────────────────

export const Chariot: PieceDef = {
  name: "Chariot",
  moves: [
    {
      pattern: "SLIDE",
      vectors: [[1,0],[-1,0],[0,1],[0,-1]],
      constraints: [{ type: "PATH_EMPTY" }],
    },
  ],
};

// ─── 馬 (Horse) — moves in L but can be blocked ───────────────────────────────
// VECTOR pattern stores the block cell in path[0]; BLOCK_CELL constraint checks it.

export const Horse: PieceDef = {
  name: "Horse",
  moves: [
    {
      pattern: "VECTOR",
      vectors: [
        [2,1],[2,-1],[-2,1],[-2,-1],
        [1,2],[1,-2],[-1,2],[-1,-2],
      ],
      constraints: [{ type: "BLOCK_CELL" }],
    },
  ],
};

// ─── 象 / 相 (Elephant) — moves exactly 2 diagonals, stays own side ───────────
// Blocked if the diagonal midpoint is occupied.
// Region constraint enforces staying on own half.

export const Elephant: PieceDef = {
  name: "Elephant",
  moves: [
    {
      pattern: "VECTOR",
      vectors: [[2,2],[2,-2],[-2,2],[-2,-2]],
      constraints: [
        { type: "BLOCK_CELL" },           // midpoint must be empty
        { type: "REGION", params: { name: "own_half" } },
      ],
    },
  ],
};

// ─── 士 / 仕 (Advisor) — one step diagonally, stays in palace ─────────────────

export const Advisor: PieceDef = {
  name: "Advisor",
  moves: [
    {
      pattern: "STEP",
      vectors: [[1,1],[1,-1],[-1,1],[-1,-1]],
      constraints: [{ type: "REGION", params: { name: "palace" } }],
    },
  ],
};

// ─── 將 / 帥 (General) — one step ortho, stays in palace ─────────────────────

export const General: PieceDef = {
  name: "General",
  moves: [
    {
      pattern: "STEP",
      vectors: [[1,0],[-1,0],[0,1],[0,-1]],
      constraints: [{ type: "REGION", params: { name: "palace" } }],
    },
  ],
  defaultState: { hasMoved: false },
};

// ─── 炮 (Cannon) — moves like chariot, captures by jumping exactly one screen ──

export const Cannon: PieceDef = {
  name: "Cannon",
  // Non-capture: slide freely, path must be empty
  moves: [
    {
      pattern: "SLIDE",
      vectors: [[1,0],[-1,0],[0,1],[0,-1]],
      moveOnly: true,
      constraints: [{ type: "PATH_EMPTY" }],
    },
  ],
  // Capture: slide to target with exactly one intervening piece (screen)
  captures: [
    {
      pattern: "SLIDE",
      vectors: [[1,0],[-1,0],[0,1],[0,-1]],
      captureOnly: true,
      constraints: [{ type: "SCREEN", params: { pieces_required: 1 } }],
    },
  ],
};

// ─── 兵 / 卒 (Soldier / Pawn) ─────────────────────────────────────────────────
// Before crossing river: only forward.
// After crossing river: forward + sideways.
// Direction is player-dependent; callers must register two variants or use state.

export const SoldierRed: PieceDef = {
  name: "SoldierRed",
  moves: [
    // Before river: only forward (row decreasing for Red)
    {
      pattern: "STEP",
      vectors: [[0,-1]],
      moveOnly: true,
      constraints: [{ type: "REGION", params: { name: "red_own_half" } }],
    },
    // After river: forward + sideways
    {
      pattern: "STEP",
      vectors: [[0,-1],[1,0],[-1,0]],
      constraints: [{ type: "REGION", params: { name: "black_half" } }],
    },
  ],
};

export const SoldierBlack: PieceDef = {
  name: "SoldierBlack",
  moves: [
    {
      pattern: "STEP",
      vectors: [[0,1]],
      moveOnly: true,
      constraints: [{ type: "REGION", params: { name: "black_own_half" } }],
    },
    {
      pattern: "STEP",
      vectors: [[0,1],[1,0],[-1,0]],
      constraints: [{ type: "REGION", params: { name: "red_half" } }],
    },
  ],
};

export const ALL_PIECES: PieceDef[] = [
  Chariot, Horse, Elephant, Advisor, General, Cannon, SoldierRed, SoldierBlack,
];
