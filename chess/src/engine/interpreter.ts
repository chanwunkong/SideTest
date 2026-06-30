import {
  ActionStep, CandidateSquare, Coord, GameState, MoveDef, MoveIntent,
  PieceDef, PieceInstance, ValidatedMove, coordKey
} from "./types";
import { generateCandidates } from "./geometry";
import { applyConstraints, hasCheckConstraint } from "./constraint";
import { applyMove, cloneState, isKingInCheck } from "./state";

export interface GetLegalMovesOptions {
  /** Skip the "would leave king in check" simulation — used internally */
  skipCheckValidation?: boolean;
}

// ─── Main public API ──────────────────────────────────────────────────────────

/**
 * Returns all legal ValidatedMoves for the piece at `from` in the given state.
 * This is the single entry point the UI / AI calls.
 */
export function getLegalMoves(
  from: Coord,
  state: GameState,
  options: GetLegalMovesOptions = {}
): ValidatedMove[] {
  const piece = state.board.get(coordKey(from));
  if (!piece) return [];
  if (piece.owner !== state.currentPlayer && !options.skipCheckValidation) return [];

  const pieceDef = state.gameDef.pieces.find(p => p.name === piece.definitionName);
  if (!pieceDef) return [];

  const result: ValidatedMove[] = [];

  // Separate move defs from capture defs
  const moveDefs = pieceDef.moves;
  const captureDefs = pieceDef.captures ?? pieceDef.moves;

  const allCandidates = collectCandidates(from, piece, moveDefs, captureDefs, state);

  for (const candidate of allCandidates) {
    const validated = buildValidatedMove(piece, from, candidate, state);
    if (!validated) continue;

    // Deferred check validation: simulate and verify king safety
    if (!options.skipCheckValidation) {
      const simState = applyMove(cloneState(state), validated);
      // After move, it's opponent's turn — so we check the player who just moved
      const playerWhoMoved = state.currentPlayer;
      if (isKingInCheck(simState, playerWhoMoved)) continue;
    }

    result.push(validated);
  }

  return result;
}

/**
 * Validate a specific MoveIntent (returns null if illegal).
 */
export function validateMove(
  intent: MoveIntent,
  state: GameState
): ValidatedMove | null {
  const legal = getLegalMoves(intent.from, state);
  return legal.find(m => coordKey(m.intent.to) === coordKey(intent.to)) ?? null;
}

// ─── Step 1: Collect candidates from move defs ───────────────────────────────

function collectCandidates(
  from: Coord,
  piece: PieceInstance,
  moveDefs: MoveDef[],
  captureDefs: MoveDef[],
  state: GameState
): CandidateSquare[] {
  const seen = new Set<string>();
  const result: CandidateSquare[] = [];

  const process = (defs: MoveDef[], captureAllowed: boolean, moveAllowed: boolean) => {
    for (const def of defs) {
      let candidates = generateCandidates(from, def, state);

      // Apply captureOnly / moveOnly flags from def
      if (def.captureOnly) candidates = candidates.filter(c => c.isCapture);
      if (def.moveOnly)    candidates = candidates.filter(c => !c.isCapture);

      // Apply inline constraints
      if (def.constraints && def.constraints.length > 0) {
        candidates = applyConstraints(candidates, def.constraints, from, piece, state);
      }

      for (const c of candidates) {
        // Skip if target is friendly piece
        const occupant = state.board.get(coordKey(c.coord));
        if (occupant && occupant.owner === piece.owner) continue;

        // Enforce capture/move intent
        if (c.isCapture && !captureAllowed) continue;
        if (!c.isCapture && !moveAllowed) continue;

        const key = `${coordKey(c.coord)}:${c.isCapture}`;
        if (seen.has(key)) continue;
        seen.add(key);
        result.push(c);
      }
    }
  };

  // Standard moves (non-capture squares) from moveDefs
  process(moveDefs, false, true);
  // Capture squares from captureDefs
  process(captureDefs, true, false);

  return result;
}

// ─── Step 2: Build ValidatedMove from a CandidateSquare ──────────────────────

function buildValidatedMove(
  piece: PieceInstance,
  from: Coord,
  candidate: CandidateSquare,
  state: GameState
): ValidatedMove | null {
  const actions: ActionStep[] = [];
  let capturedPieceId: string | undefined;

  if (candidate.isCapture) {
    const captured = state.board.get(coordKey(candidate.coord));
    if (!captured) return null; // stale candidate
    capturedPieceId = captured.id;
    actions.push({ action: "CAPTURE", from, to: candidate.coord });
  }

  actions.push({ action: "MOVE", from, to: candidate.coord });

  const intent: MoveIntent = {
    pieceId: piece.id,
    from,
    to: candidate.coord,
  };

  return {
    intent,
    actions,
    isCapture: candidate.isCapture,
    capturedPieceId,
  };
}

// ─── Composite Action: Castling ───────────────────────────────────────────────
// This is registered separately and called by the interpreter when a COMPOSITE
// rule is triggered. Games that support castling declare it in rules[].

export function buildCastlingMove(
  king: PieceInstance,
  rook: PieceInstance,
  kingTo: Coord,
  rookTo: Coord,
  state: GameState
): ValidatedMove | null {
  // Guard: neither piece must have moved
  if (king.state["hasMoved"] || rook.state["hasMoved"]) return null;

  // Guard: path between king and rook must be empty
  const [kc, kr] = king.coord;
  const [rc] = rook.coord;
  const dir = Math.sign(rc - kc);
  for (let c = kc + dir; c !== rc; c += dir) {
    if (state.board.has(coordKey([c, kr]))) return null;
  }

  // Guard: king must not be in check
  if (isKingInCheck(state, king.owner)) return null;

  // Guard: king must not pass through check
  const passThrough: Coord = [kc + dir, kr];
  const simPass = cloneState(state);
  simPass.board.delete(coordKey(king.coord));
  simPass.board.set(coordKey(passThrough), { ...king, coord: passThrough });
  if (isKingInCheck(simPass, king.owner)) return null;

  const actions: ActionStep[] = [
    { action: "MOVE", from: king.coord, to: kingTo },
    { action: "MOVE", from: rook.coord, to: rookTo, pieceId: rook.id },
  ];

  return {
    intent: { pieceId: king.id, from: king.coord, to: kingTo },
    actions,
    isCapture: false,
  };
}
