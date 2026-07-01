import {
  ActionStep, Coord, ExecutedMove, GameState, MoveIntent, PieceInstance,
  ValidatedMove, coordKey
} from "./types";

// ─── Initialise GameState from a GameDef ─────────────────────────────────────

export function createGameState(gameDef: import("./types").GameDef): GameState {
  const board = new Map<string, PieceInstance>();

  for (const entry of gameDef.setup) {
    const pieceDef = gameDef.pieces.find(p => p.name === entry.pieceDefName);
    if (!pieceDef) throw new Error(`Unknown piece definition: ${entry.pieceDefName}`);

    const id = `${entry.pieceDefName}-${entry.owner}-${coordKey(entry.coord)}`;
    const instance: PieceInstance = {
      id,
      definitionName: entry.pieceDefName,
      owner: entry.owner,
      coord: entry.coord,
      state: { ...pieceDef.defaultState, ...entry.state },
    };
    board.set(coordKey(entry.coord), instance);
  }

  const capturedPieces: Record<string, PieceInstance[]> = {};
  for (const player of gameDef.players) capturedPieces[player] = [];

  return {
    gameDef,
    board,
    capturedPieces,
    currentPlayer: gameDef.firstPlayer,
    moveHistory: [],
    turnNumber: 1,
  };
}

// ─── Deep-clone a GameState (for simulation / check detection) ───────────────

export function cloneState(state: GameState): GameState {
  const board = new Map<string, PieceInstance>();
  for (const [k, v] of state.board) {
    board.set(k, { ...v, state: { ...v.state } });
  }

  const capturedPieces: Record<string, PieceInstance[]> = {};
  for (const [k, v] of Object.entries(state.capturedPieces)) {
    capturedPieces[k] = v.map(p => ({ ...p, state: { ...p.state } }));
  }

  return {
    ...state,
    board,
    capturedPieces,
    moveHistory: [...state.moveHistory],
  };
}

// ─── Apply a validated move to produce a new GameState ───────────────────────

export function applyMove(state: GameState, move: ValidatedMove): GameState {
  const next = cloneState(state);

  for (const step of move.actions) {
    executeStep(next, step);
  }

  // Mark hasMoved on the primary piece
  const movedPiece = next.board.get(coordKey(move.intent.to));
  if (movedPiece) {
    movedPiece.state = { ...movedPiece.state, hasMoved: true };
  }

  // Advance turn
  const players = state.gameDef.players;
  const idx = players.indexOf(state.currentPlayer);
  next.currentPlayer = players[(idx + 1) % players.length];
  next.turnNumber = state.turnNumber + 1;

  const executed: ExecutedMove = {
    ...move,
    player: state.currentPlayer,
    turnNumber: state.turnNumber,
  };
  next.moveHistory = [...state.moveHistory, executed];

  return next;
}

function executeStep(state: GameState, step: ActionStep): void {
  switch (step.action) {
    case "MOVE": {
      const from = step.from!;
      const piece = state.board.get(coordKey(from));
      if (!piece) throw new Error(`No piece at ${coordKey(from)} for MOVE step`);
      state.board.delete(coordKey(from));
      piece.coord = step.to;
      state.board.set(coordKey(step.to), piece);
      break;
    }
    case "CAPTURE": {
      const captured = state.board.get(coordKey(step.to));
      if (captured) {
        state.board.delete(coordKey(step.to));
        // For Shogi-style games: add to owner's hand
        const attacker = step.pieceId
          ? findById(state, step.pieceId)
          : undefined;
        if (attacker) {
          state.capturedPieces[attacker.owner].push(captured);
        }
      }
      break;
    }
    case "REMOVE": {
      state.board.delete(coordKey(step.to));
      break;
    }
    case "PROMOTE": {
      const piece = state.board.get(coordKey(step.to));
      if (!piece) throw new Error(`No piece at ${coordKey(step.to)} for PROMOTE step`);
      piece.definitionName = step.promoteTo!;
      break;
    }
    case "SWAP": {
      const a = state.board.get(coordKey(step.from!));
      const b = state.board.get(coordKey(step.to));
      if (a) { a.coord = step.to; state.board.set(coordKey(step.to), a); }
      if (b) { b.coord = step.from!; state.board.set(coordKey(step.from!), b); }
      if (!a) state.board.delete(coordKey(step.to));
      if (!b) state.board.delete(coordKey(step.from!));
      break;
    }
  }
}

function findById(state: GameState, id: string): PieceInstance | undefined {
  for (const piece of state.board.values()) {
    if (piece.id === id) return piece;
  }
  return undefined;
}

// ─── King safety check ────────────────────────────────────────────────────────

/** Returns true if `player`'s king is under attack in the given state */
export function isKingInCheck(state: GameState, player: string): boolean {
  // Find king-like piece (piece with name containing "King" or "General")
  const kingCoord = findKingCoord(state, player);
  if (!kingCoord) return false;

  const opponent = state.gameDef.players.find(p => p !== player)!;
  // Lazy import to avoid circular deps — interpreter will call this after generating enemy moves
  const { getLegalMoves } = require("./interpreter") as typeof import("./interpreter");

  for (const piece of state.board.values()) {
    if (piece.owner !== opponent) continue;
    const moves = getLegalMoves(piece.coord, state, { skipCheckValidation: true });
    if (moves.some(m => coordKey(m.intent.to) === coordKey(kingCoord))) {
      return true;
    }
  }
  return false;
}

function findKingCoord(state: GameState, player: string): Coord | null {
  const kingNames = new Set(["King", "General", "将", "帅", "王将", "玉将"]);
  for (const piece of state.board.values()) {
    if (piece.owner === player && kingNames.has(piece.definitionName)) {
      return piece.coord;
    }
  }
  return null;
}
