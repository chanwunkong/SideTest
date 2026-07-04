import { BoardDef } from "./types";

// ─── Board Registry — reusable geometries for the draft/army-builder mode (TASK-029) ───
// Each entry is grid size + rectangular regions only (river-half / palace / promotion
// zones) — no piece roster is bound here. Boards are deliberately game-agnostic: any
// PieceDef from the pool (TASK-028, index.html) can be placed on any board. A piece
// whose movement needs a region a board doesn't define (e.g. a palace-only General on
// board_8x8) is simply unrestricted for that constraint — see the REGION constraint's
// "unknown region = no restriction" behaviour in constraint.ts. That is intentional:
// compatibility is left to the player, not gated here.
//
// Diagonal palace lines (Janggi) aren't representable as the rectangular regions this
// schema supports; they stay a runtime-only concern outside BoardDef.

export const BOARD_REGISTRY: Record<string, BoardDef> = {
  board_8x8: {
    cols: 8,
    rows: 8,
    regions: {
      own_half_p1: { colMin: 0, colMax: 7, rowMin: 4, rowMax: 7 },
      own_half_p2: { colMin: 0, colMax: 7, rowMin: 0, rowMax: 3 },
    },
  },

  // Xiangqi / Cờ Tướng geometry: 9×10 with a river dividing rows 4/5.
  board_9x10_river: {
    cols: 9,
    rows: 10,
    regions: {
      palace_p1: { colMin: 3, colMax: 5, rowMin: 7, rowMax: 9 },
      palace_p2: { colMin: 3, colMax: 5, rowMin: 0, rowMax: 2 },
      own_half_p1: { colMin: 0, colMax: 8, rowMin: 5, rowMax: 9 },
      own_half_p2: { colMin: 0, colMax: 8, rowMin: 0, rowMax: 4 },
    },
  },

  // Janggi geometry: 9×10, no river; both palaces present (diagonals excluded, see above).
  board_9x10_no_river: {
    cols: 9,
    rows: 10,
    regions: {
      palace_p1: { colMin: 3, colMax: 5, rowMin: 7, rowMax: 9 },
      palace_p2: { colMin: 3, colMax: 5, rowMin: 0, rowMax: 2 },
    },
  },

  // Shogi geometry: 9×9; promotion zone is the back three ranks per side.
  board_9x9: {
    cols: 9,
    rows: 9,
    regions: {
      promotion_p1: { colMin: 0, colMax: 8, rowMin: 0, rowMax: 2 },
      promotion_p2: { colMin: 0, colMax: 8, rowMin: 6, rowMax: 8 },
    },
  },
};
