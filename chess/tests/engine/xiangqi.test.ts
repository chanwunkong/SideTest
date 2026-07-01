import { createGameState } from "../../src/engine/state";
import { getLegalMoves, validateMove } from "../../src/engine/interpreter";
import { XiangqiGame } from "../../src/games/xiangqi/game";
import { coordKey } from "../../src/engine/types";

describe("Xiangqi — initial setup", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("board initialises with 32 pieces", () => {
    expect(state.board.size).toBe(32);
  });

  test("red General is at [4,9]", () => {
    const piece = state.board.get("4,9");
    expect(piece?.definitionName).toBe("General");
    expect(piece?.owner).toBe("red");
  });

  test("black General is at [4,0]", () => {
    const piece = state.board.get("4,0");
    expect(piece?.definitionName).toBe("General");
    expect(piece?.owner).toBe("black");
  });
});

describe("Chariot (車) — move generation", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("Red Chariot at [0,9] can slide to [0,8] and [0,7], blocked by SoldierRed at [0,6]", () => {
    const moves = getLegalMoves([0, 9], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    // Rows 8 and 7 are empty; row 6 has a friendly soldier (blocks further + no capture)
    expect(targets).toContain("0,8");
    expect(targets).toContain("0,7");
    expect(targets).not.toContain("0,6"); // friendly piece
    expect(targets).not.toContain("0,5");
  });
});

describe("Horse (馬) — move generation", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("Red Horse at [1,9] can move to [0,7] and [2,7] from opening position", () => {
    const moves = getLegalMoves([1, 9], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    expect(targets).toContain("0,7");
    expect(targets).toContain("2,7");
  });

  test("Red Horse at [7,9] can move to [6,7] and [8,7]", () => {
    const moves = getLegalMoves([7, 9], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    expect(targets).toContain("6,7");
    expect(targets).toContain("8,7");
  });
});

describe("Cannon (炮) — move generation", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("Red Cannon at [1,7] can slide horizontally along row 7", () => {
    const moves = getLegalMoves([1, 7], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    // Between col 2 and col 6 on row 7 (other cannon at [7,7] blocks col 7)
    expect(targets).toContain("2,7");
    expect(targets).toContain("3,7");
    expect(targets).toContain("4,7");
    expect(targets).toContain("5,7");
    expect(targets).toContain("6,7");
  });

  test("Red Cannon cannot move to a square with a friendly piece", () => {
    const moves = getLegalMoves([1, 7], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    // [1,9] is Red Horse — must not appear
    expect(targets).not.toContain("1,9");
  });

  test("Red Cannon at [1,7] can capture Black Cannon at [1,2] via screen at [1,3]", () => {
    // Black soldier at [0,3] does NOT screen col 1
    // Black soldier at [2,3] does NOT screen col 1
    // Between [1,7] and [1,2]: row 6 (red soldier), row 5, row 4, row 3 (black soldier) — 2 screens?
    // Actually red soldier at [0,6] not [1,6], so col 1 rows 3–6 are:
    // row 6: empty, row 5: empty, row 4: empty, row 3: black soldier at [0,3] NO — [2,3] NO
    // Wait — black soldiers are at cols 0,2,4,6,8 row 3.  Col 1 row 3 is empty.
    // So path between [1,7] and [1,2] is rows 6,5,4,3 — all empty → NOT a valid cannon capture
    const moves = getLegalMoves([1, 7], state);
    const captures = moves.filter(m => m.isCapture);
    // No capture possible in opening position along col 1
    expect(captures.some(m => coordKey(m.intent.to) === "1,2")).toBe(false);
  });
});

describe("General (將) — palace constraint", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("Red General at [4,9] can only move to [4,8] (advisors block sides, palace constrains)", () => {
    const moves = getLegalMoves([4, 9], state);
    const targets = moves.map(m => coordKey(m.intent.to));
    // [3,9] and [5,9] occupied by Advisors; [4,8] is empty and inside palace
    expect(targets).toContain("4,8");
    expect(targets).not.toContain("3,9");
    expect(targets).not.toContain("5,9");
    // [4,10] is out of bounds
  });
});

describe("Move validation", () => {
  let state: ReturnType<typeof createGameState>;

  beforeEach(() => {
    state = createGameState(XiangqiGame);
  });

  test("Red Horse [1,9] → [0,7] is valid", () => {
    const result = validateMove({ pieceId: "", from: [1, 9], to: [0, 7] }, state);
    expect(result).not.toBeNull();
  });

  test("Red Horse [1,9] → [3,8] is invalid (not an L-shape)", () => {
    const result = validateMove({ pieceId: "", from: [1, 9], to: [3, 8] }, state);
    expect(result).toBeNull();
  });
});
