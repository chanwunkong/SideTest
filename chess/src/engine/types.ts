// ─── Coordinates ────────────────────────────────────────────────────────────

export type Vector = [number, number]; // [dx, dy]
export type Coord  = [number, number]; // [col, row] — 0-indexed

// ─── Board ──────────────────────────────────────────────────────────────────

export interface BoardDef {
  cols: number;
  rows: number;
  /** Named rectangular regions, e.g. palace zones */
  regions?: Record<string, { colMin: number; colMax: number; rowMin: number; rowMax: number }>;
}

// ─── Piece state ────────────────────────────────────────────────────────────

export type PieceState = Record<string, boolean | number | string>;

export interface PieceInstance {
  id: string;           // unique instance id, e.g. "r-red-0"
  definitionName: string;
  owner: string;        // player id
  coord: Coord;
  state: PieceState;
}

// ─── Move patterns ──────────────────────────────────────────────────────────

export type PatternType = "STEP" | "SLIDE" | "LEAP" | "VECTOR";

// ─── Constraint types ───────────────────────────────────────────────────────

export type ConstraintType =
  | "REGION"
  | "PATH_EMPTY"
  | "SCREEN"
  | "BLOCK_CELL"
  | "STATE"
  | "TURN"
  | "CHECK";

export interface ConstraintDef {
  type: ConstraintType;
  params?: Record<string, unknown>;
}

// ─── Move definition (per piece DSL) ────────────────────────────────────────

export interface MoveDef {
  id?: string;
  pattern: PatternType;
  /** Explicit direction vectors; required for VECTOR / LEAP patterns */
  vectors?: Vector[];
  /** SLIDE only: max squares (omit = unlimited) */
  maxSteps?: number;
  /** For STEP / SLIDE: allow 8-directional omni movement (no vectors needed) */
  omniDirectional?: boolean;
  repeat?: boolean;
  constraints?: ConstraintDef[];
  /** If true this move entry applies only when capturing */
  captureOnly?: boolean;
  /** If true this move entry applies only without capturing */
  moveOnly?: boolean;
}

// ─── Piece definition ────────────────────────────────────────────────────────

export interface PieceDef {
  name: string;
  /** Draft point cost (for the point-budget army-building mode). Placeholder until TASK-033 computes real values. */
  cost?: number;
  /** Moves that apply for both move and capture unless overridden */
  moves: MoveDef[];
  /** If present, overrides moves[] for capture squares */
  captures?: MoveDef[];
  /** Default state values for new instances */
  defaultState?: PieceState;
}

// ─── Action types ────────────────────────────────────────────────────────────

export type ActionType = "MOVE" | "CAPTURE" | "DROP" | "COMPOSITE";

export interface ActionStep {
  action: "MOVE" | "CAPTURE" | "REMOVE" | "PROMOTE" | "SWAP";
  pieceId?: string;          // omit = the piece being moved
  from?: Coord;
  to: Coord;
  promoteTo?: string;        // piece definition name
}

export interface ActionDef {
  type: ActionType;
  steps?: ActionStep[];
  constraints?: ConstraintDef[];
}

// ─── Game rules (win/lose/draw) ───────────────────────────────────────────

export interface RuleDef {
  id: string;
  trigger: "AFTER_MOVE" | "START_OF_TURN";
  condition: string;        // e.g. "KING_IN_CHECKMATE" | "STALEMATE"
  outcome: "WIN" | "LOSE" | "DRAW";
  affectsPlayer: "CURRENT" | "OPPONENT";
}

// ─── Full game definition ─────────────────────────────────────────────────

export interface GameDef {
  id: string;
  name: string;
  board: BoardDef;
  players: string[];
  /** Initial board layout */
  setup: Array<{ pieceDefName: string; owner: string; coord: Coord; state?: PieceState }>;
  pieces: PieceDef[];
  rules: RuleDef[];
  /** Which player moves first */
  firstPlayer: string;
}

// ─── Live game state ──────────────────────────────────────────────────────

export interface GameState {
  gameDef: GameDef;
  board: Map<string, PieceInstance>;  // coordKey -> piece
  capturedPieces: Record<string, PieceInstance[]>; // owner -> pieces in hand (Shogi)
  currentPlayer: string;
  moveHistory: ExecutedMove[];
  turnNumber: number;
}

// ─── Move intent (input from UI / AI) ────────────────────────────────────

export interface MoveIntent {
  pieceId: string;
  from: Coord;
  to: Coord;
  /** For Shogi drop */
  dropPieceDefName?: string;
  /** For pawn promotion choice */
  promoteTo?: string;
}

// ─── Validated move result ────────────────────────────────────────────────

export interface ValidatedMove {
  intent: MoveIntent;
  actions: ActionStep[];
  isCapture: boolean;
  capturedPieceId?: string;
}

export interface ExecutedMove extends ValidatedMove {
  player: string;
  turnNumber: number;
}

// ─── Candidate square (used internally by geometry engine) ───────────────

export interface CandidateSquare {
  coord: Coord;
  isCapture: boolean;
  /** Intermediate squares traversed (for PATH_EMPTY checks) */
  path: Coord[];
  /** The screen piece coord (for SCREEN constraint) */
  screenCoord?: Coord;
}

// ─── Utility: coord to/from string key ───────────────────────────────────

export function coordKey(c: Coord): string {
  return `${c[0]},${c[1]}`;
}

export function keyToCoord(k: string): Coord {
  const [col, row] = k.split(",").map(Number);
  return [col, row];
}
