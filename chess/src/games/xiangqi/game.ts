import { GameDef } from "../../engine/types";
import { ALL_PIECES } from "./pieces";

// ─── Board: 9 cols × 10 rows (0-indexed) ─────────────────────────────────────
// col 0–8, row 0–9
// Red starts at rows 7–9 (bottom), Black at rows 0–2 (top)

export const XiangqiGame: GameDef = {
  id: "xiangqi",
  name: "中國象棋",
  players: ["red", "black"],
  firstPlayer: "red",

  board: {
    cols: 9,
    rows: 10,
    regions: {
      // Palaces (3×3 middle of back rank)
      palace_red:   { colMin: 3, colMax: 5, rowMin: 7, rowMax: 9 },
      palace_black: { colMin: 3, colMax: 5, rowMin: 0, rowMax: 2 },
      // Halves (for elephant stay-own-side + soldier crossing river)
      red_own_half:  { colMin: 0, colMax: 8, rowMin: 5, rowMax: 9 },
      black_own_half:{ colMin: 0, colMax: 8, rowMin: 0, rowMax: 4 },
      red_half:      { colMin: 0, colMax: 8, rowMin: 5, rowMax: 9 },
      black_half:    { colMin: 0, colMax: 8, rowMin: 0, rowMax: 4 },
      // Combined half for elephant (own side only)
      own_half_red:  { colMin: 0, colMax: 8, rowMin: 5, rowMax: 9 },
      own_half_black:{ colMin: 0, colMax: 8, rowMin: 0, rowMax: 4 },
    },
  },

  pieces: ALL_PIECES,

  setup: [
    // ── Black (top, rows 0–2) ──
    { pieceDefName: "Chariot",     owner: "black", coord: [0, 0] },
    { pieceDefName: "Horse",       owner: "black", coord: [1, 0] },
    { pieceDefName: "Elephant",    owner: "black", coord: [2, 0] },
    { pieceDefName: "Advisor",     owner: "black", coord: [3, 0] },
    { pieceDefName: "General",     owner: "black", coord: [4, 0] },
    { pieceDefName: "Advisor",     owner: "black", coord: [5, 0] },
    { pieceDefName: "Elephant",    owner: "black", coord: [6, 0] },
    { pieceDefName: "Horse",       owner: "black", coord: [7, 0] },
    { pieceDefName: "Chariot",     owner: "black", coord: [8, 0] },
    { pieceDefName: "Cannon",      owner: "black", coord: [1, 2] },
    { pieceDefName: "Cannon",      owner: "black", coord: [7, 2] },
    { pieceDefName: "SoldierBlack",owner: "black", coord: [0, 3] },
    { pieceDefName: "SoldierBlack",owner: "black", coord: [2, 3] },
    { pieceDefName: "SoldierBlack",owner: "black", coord: [4, 3] },
    { pieceDefName: "SoldierBlack",owner: "black", coord: [6, 3] },
    { pieceDefName: "SoldierBlack",owner: "black", coord: [8, 3] },
    // ── Red (bottom, rows 7–9) ──
    { pieceDefName: "Chariot",     owner: "red", coord: [0, 9] },
    { pieceDefName: "Horse",       owner: "red", coord: [1, 9] },
    { pieceDefName: "Elephant",    owner: "red", coord: [2, 9] },
    { pieceDefName: "Advisor",     owner: "red", coord: [3, 9] },
    { pieceDefName: "General",     owner: "red", coord: [4, 9] },
    { pieceDefName: "Advisor",     owner: "red", coord: [5, 9] },
    { pieceDefName: "Elephant",    owner: "red", coord: [6, 9] },
    { pieceDefName: "Horse",       owner: "red", coord: [7, 9] },
    { pieceDefName: "Chariot",     owner: "red", coord: [8, 9] },
    { pieceDefName: "Cannon",      owner: "red", coord: [1, 7] },
    { pieceDefName: "Cannon",      owner: "red", coord: [7, 7] },
    { pieceDefName: "SoldierRed",  owner: "red", coord: [0, 6] },
    { pieceDefName: "SoldierRed",  owner: "red", coord: [2, 6] },
    { pieceDefName: "SoldierRed",  owner: "red", coord: [4, 6] },
    { pieceDefName: "SoldierRed",  owner: "red", coord: [6, 6] },
    { pieceDefName: "SoldierRed",  owner: "red", coord: [8, 6] },
  ],

  rules: [
    {
      id: "checkmate",
      trigger: "START_OF_TURN",
      condition: "KING_IN_CHECKMATE",
      outcome: "LOSE",
      affectsPlayer: "CURRENT",
    },
    {
      id: "stalemate",
      trigger: "START_OF_TURN",
      condition: "STALEMATE",
      outcome: "DRAW",
      affectsPlayer: "CURRENT",
    },
  ],
};
