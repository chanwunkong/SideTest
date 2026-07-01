import {
  BoardDef, CandidateSquare, Coord, GameState, MoveDef, Vector,
  coordKey, PieceInstance
} from "./types";

const ALL_8: Vector[] = [
  [1,0], [-1,0], [0,1], [0,-1],
  [1,1], [1,-1], [-1,1], [-1,-1]
];

const ALL_4: Vector[] = [[1,0], [-1,0], [0,1], [0,-1]];

function inBounds(coord: Coord, board: BoardDef): boolean {
  return coord[0] >= 0 && coord[0] < board.cols &&
         coord[1] >= 0 && coord[1] < board.rows;
}

function addVec(coord: Coord, vec: Vector): Coord {
  return [coord[0] + vec[0], coord[1] + vec[1]];
}

function pieceAt(coord: Coord, state: GameState): PieceInstance | undefined {
  return state.board.get(coordKey(coord));
}

// ─── STEP ────────────────────────────────────────────────────────────────────
// One square in specified (or all 8) directions.

function generateStep(from: Coord, def: MoveDef, state: GameState): CandidateSquare[] {
  const dirs: Vector[] = def.vectors ?? (def.omniDirectional !== false ? ALL_8 : ALL_4);
  const result: CandidateSquare[] = [];

  for (const vec of dirs) {
    const to = addVec(from, vec);
    if (!inBounds(to, state.gameDef.board)) continue;
    const occupant = pieceAt(to, state);
    const isCapture = occupant !== undefined;
    result.push({ coord: to, isCapture, path: [] });
  }
  return result;
}

// ─── SLIDE ───────────────────────────────────────────────────────────────────
// Unlimited (or maxSteps) extension along direction vectors.

function generateSlide(from: Coord, def: MoveDef, state: GameState): CandidateSquare[] {
  const dirs: Vector[] = def.vectors ?? ALL_8;
  const maxSteps = def.maxSteps ?? Infinity;
  const result: CandidateSquare[] = [];

  for (const vec of dirs) {
    const path: Coord[] = [];
    let current = from;

    for (let step = 0; step < maxSteps; step++) {
      current = addVec(current, vec);
      if (!inBounds(current, state.gameDef.board)) break;

      const occupant = pieceAt(current, state);
      if (occupant) {
        // Blocked — add as potential capture, then stop
        result.push({ coord: current, isCapture: true, path: [...path] });
        break;
      }
      result.push({ coord: current, isCapture: false, path: [...path] });
      path.push(current);
    }
  }
  return result;
}

// ─── LEAP ────────────────────────────────────────────────────────────────────
// Jump directly to target (ignores intermediate squares).
// Used for international Chess Knight, etc.

function generateLeap(from: Coord, def: MoveDef, state: GameState): CandidateSquare[] {
  const vectors = def.vectors ?? [];
  const result: CandidateSquare[] = [];

  for (const vec of vectors) {
    const to = addVec(from, vec);
    if (!inBounds(to, state.gameDef.board)) continue;
    const occupant = pieceAt(to, state);
    result.push({ coord: to, isCapture: occupant !== undefined, path: [] });
  }
  return result;
}

// ─── VECTOR ──────────────────────────────────────────────────────────────────
// Like LEAP but the constraint engine records an explicit block cell.
// Used for Xiangqi Horse (馬) — needs BLOCK_CELL constraint to filter.
// The path[] here carries the intermediate cell for the constraint engine.

function generateVector(from: Coord, def: MoveDef, state: GameState): CandidateSquare[] {
  const vectors = def.vectors ?? [];
  const result: CandidateSquare[] = [];

  for (const vec of vectors) {
    const to = addVec(from, vec);
    if (!inBounds(to, state.gameDef.board)) continue;
    const occupant = pieceAt(to, state);
    // Store the first-step cell as the path (used by BLOCK_CELL constraint)
    const blockCell = addVec(from, normalise(vec));
    result.push({ coord: to, isCapture: occupant !== undefined, path: [blockCell] });
  }
  return result;
}

/** Reduce a vector to its first-step unit (e.g. [2,1] → [1,0]) */
function normalise(vec: Vector): Vector {
  const [dx, dy] = vec;
  if (Math.abs(dx) >= Math.abs(dy)) {
    return [Math.sign(dx), 0];
  }
  return [0, Math.sign(dy)];
}

// ─── Public API ──────────────────────────────────────────────────────────────

export function generateCandidates(
  from: Coord,
  def: MoveDef,
  state: GameState
): CandidateSquare[] {
  switch (def.pattern) {
    case "STEP":   return generateStep(from, def, state);
    case "SLIDE":  return generateSlide(from, def, state);
    case "LEAP":   return generateLeap(from, def, state);
    case "VECTOR": return generateVector(from, def, state);
    default:
      throw new Error(`Unknown pattern: ${(def as MoveDef).pattern}`);
  }
}
