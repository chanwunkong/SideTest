import {
  CandidateSquare, ConstraintDef, Coord, GameState, PieceInstance,
  coordKey, Vector
} from "./types";

type ConstraintFilter = (
  candidate: CandidateSquare,
  from: Coord,
  piece: PieceInstance,
  state: GameState,
  params: Record<string, unknown>
) => boolean;

// ─── REGION ──────────────────────────────────────────────────────────────────
// Restrict movement to a named region on the board.

const regionFilter: ConstraintFilter = (candidate, from, piece, state, params) => {
  const regionName = params["name"] as string;
  const region = state.gameDef.board.regions?.[regionName];
  if (!region) return true; // unknown region = no restriction

  const [col, row] = candidate.coord;
  return col >= region.colMin && col <= region.colMax &&
         row >= region.rowMin && row <= region.rowMax;
};

// ─── PATH_EMPTY ──────────────────────────────────────────────────────────────
// All squares in candidate.path must be empty.
// Used by Xiangqi Rook (車), Bishop (象 leg), etc.

const pathEmptyFilter: ConstraintFilter = (candidate, _from, _piece, state, params) => {
  const beforeCapture = params["before_capture"] as boolean | undefined;

  for (const cell of candidate.path) {
    if (state.board.has(coordKey(cell))) {
      if (beforeCapture && cell === candidate.path[candidate.path.length - 1]) {
        // for cannon: the last path cell being occupied is the screen — handled by SCREEN
        continue;
      }
      return false;
    }
  }
  return true;
};

// ─── SCREEN ──────────────────────────────────────────────────────────────────
// Exactly N pieces must be on the path between from and to.
// Classic use: Xiangqi Cannon (炮) capture requires exactly 1 screen piece.

const screenFilter: ConstraintFilter = (candidate, from, _piece, state, params) => {
  if (!candidate.isCapture) return true; // screen only matters for captures

  const required = (params["pieces_required"] as number) ?? 1;
  const [fc, fr] = from;
  const [tc, tr] = candidate.coord;

  // Build path from from → to (exclusive of both ends)
  const cells: Coord[] = [];
  const dc = Math.sign(tc - fc);
  const dr = Math.sign(tr - fr);
  let c = fc + dc;
  let r = fr + dr;
  while (c !== tc || r !== tr) {
    cells.push([c, r]);
    c += dc;
    r += dr;
  }

  const screens = cells.filter(cell => state.board.has(coordKey(cell)));
  return screens.length === required;
};

// ─── BLOCK_CELL ──────────────────────────────────────────────────────────────
// A specific offset cell relative to `from` must be empty.
// Used by Xiangqi Horse (馬): the cell in the primary movement direction must be clear.

const blockCellFilter: ConstraintFilter = (candidate, _from, _piece, state, _params) => {
  // The geometry engine stores the block cell in candidate.path[0] for VECTOR pattern
  if (candidate.path.length === 0) return true;
  const blockCell = candidate.path[0];
  return !state.board.has(coordKey(blockCell));
};

// ─── STATE ───────────────────────────────────────────────────────────────────
// Check a boolean flag on the moving piece's state.
// e.g. { key: "hasMoved", value: false } means piece must not have moved yet.

const stateFilter: ConstraintFilter = (_candidate, _from, piece, _state, params) => {
  const key = params["key"] as string;
  const expected = params["value"];
  return piece.state[key] === expected;
};

// ─── TURN ────────────────────────────────────────────────────────────────────
// Restrict to a specific player's turn (useful for asymmetric pieces).

const turnFilter: ConstraintFilter = (_candidate, _from, piece, state, _params) => {
  return state.currentPlayer === piece.owner;
};

// ─── CAPTURE_ONLY ────────────────────────────────────────────────────────────
// This move def only applies when the target square has an enemy piece.

const captureOnlyFilter: ConstraintFilter = (candidate, _from, piece, state, _params) => {
  if (!candidate.isCapture) return false;
  const occupant = state.board.get(coordKey(candidate.coord));
  return occupant !== undefined && occupant.owner !== piece.owner;
};

// ─── MOVE_ONLY ───────────────────────────────────────────────────────────────
// This move def only applies when the target square is empty.

const moveOnlyFilter: ConstraintFilter = (candidate, _from, _piece, _state, _params) => {
  return !candidate.isCapture;
};

// ─── CHECK (simulate + validate king safety) ─────────────────────────────────
// Applied after all other constraints pass; must be deferred to the interpreter.
// Returning true here means "don't filter yet — interpreter will handle it".

const checkFilter: ConstraintFilter = () => true;

// ─── Registry ─────────────────────────────────────────────────────────────────

const FILTERS: Record<string, ConstraintFilter> = {
  REGION:      regionFilter,
  PATH_EMPTY:  pathEmptyFilter,
  SCREEN:      screenFilter,
  BLOCK_CELL:  blockCellFilter,
  STATE:       stateFilter,
  TURN:        turnFilter,
  CHECK:       checkFilter,
};

// ─── Public API ───────────────────────────────────────────────────────────────

export function applyConstraints(
  candidates: CandidateSquare[],
  constraints: ConstraintDef[],
  from: Coord,
  piece: PieceInstance,
  state: GameState
): CandidateSquare[] {
  return candidates.filter(candidate =>
    constraints.every(def => {
      const filter = FILTERS[def.type];
      if (!filter) throw new Error(`Unknown constraint type: ${def.type}`);
      return filter(candidate, from, piece, state, def.params ?? {});
    })
  );
}

/** Returns true if this constraint set has a CHECK constraint (needs deferred simulation) */
export function hasCheckConstraint(constraints: ConstraintDef[]): boolean {
  return constraints.some(c => c.type === "CHECK");
}
