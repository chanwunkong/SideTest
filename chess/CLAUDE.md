# Chess Project — Claude Code Loop Rules

This file governs how Claude Code operates autonomously within the chess project.

---

## Loop Behaviour

When running in `/loop` mode, Claude must follow every rule in this file on **every iteration** before doing anything else.

---

## 1. Read State Before Acting

At the start of each loop turn:

1. Read `TASKS.md` to load the current task queue.
2. Read `ERRORS.md` to load known errors and their status.
3. Read `PROGRESS.md` to understand what was completed in the previous turn.

If any of these files do not exist yet, create them with empty scaffolding before continuing.

---

## 2. Task Decomposition

When a task is too large to complete in one turn, decompose it:

- Break it into subtasks of ≤ 2 hours of work each.
- Write each subtask as a new entry in `TASKS.md` with status `[ ]`.
- Mark the parent task as `[~]` (in progress / decomposed).
- Do NOT start implementation until decomposition is written.

**Decomposition format in TASKS.md:**

```
- [~] TASK-003: Implement game engine
  - [ ] TASK-003a: Define board representation (bitboard or array)
  - [ ] TASK-003b: Implement move generation for each piece type
  - [ ] TASK-003c: Add check / checkmate detection
  - [ ] TASK-003d: Write unit tests for move generation
```

---

## 3. Auto-Create Next Task

After completing any task:

1. Mark it `[x]` in `TASKS.md`.
2. Determine the **logical next step** based on project context.
3. Append the next task to `TASKS.md` with status `[ ]` and a unique `TASK-NNN` id.
4. Write one sentence in `PROGRESS.md` describing what was done and what was queued.

If there is no obvious next task, add:
```
- [ ] TASK-NNN: Review progress and plan next milestone
```

---

## 4. Error Recording

Whenever a command fails, a test fails, or an unexpected result occurs:

1. Append to `ERRORS.md` immediately (do not wait until end of turn).
2. Attempt one fix. If the fix succeeds, mark the error `[resolved]`.
3. If the fix fails, mark `[blocked]` and add a new task to investigate.

**ERRORS.md entry format:**

```
### ERR-001 [resolved | blocked | open]
- **Date:** 2026-06-30
- **Task:** TASK-002b
- **Command / file:** `npm test src/engine.test.ts`
- **Error message:** `TypeError: board.getSquare is not a function`
- **Root cause:** Method renamed to getCell in refactor
- **Fix applied:** Renamed all callers to getCell
- **Follow-up task:** (none) | TASK-005
```

---

## 5. GitHub Upload (Commit & Push)

After each completed task block (or every 3 subtasks, whichever comes first):

1. Run `git status` to confirm there are staged or unstaged changes.
2. Stage only relevant files — never use `git add -A` blindly.
3. Write a commit message in the format:

   ```
   <type>(<scope>): <short description>

   - bullet 1
   - bullet 2

   Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
   ```

   Types: `feat`, `fix`, `test`, `refactor`, `docs`, `chore`

4. Push to the current branch: `git push origin <branch>`.
5. Record the commit hash in `PROGRESS.md`.

**Do NOT push if:**
- Any test is currently failing.
- `ERRORS.md` has an `[open]` or `[blocked]` entry that is not yet triaged.

---

## 6. Progress Log

Every turn must append one entry to `PROGRESS.md`:

```
## 2026-06-30T14:00Z
- Completed: TASK-002b (move generation for pawns)
- Errors: ERR-001 [resolved]
- Queued: TASK-002c
- Commit: abc1234
```

---

## 7. Stopping Conditions

Pause the loop and wait for human review if:

- Three consecutive turns produce no completed task.
- An `[blocked]` error cannot be resolved after two attempts.
- A destructive git operation (force-push, reset --hard, branch delete) would be required.
- The task queue in `TASKS.md` is empty.

When pausing, write a `## PAUSED` section at the top of `PROGRESS.md` explaining why.

---

## 8. File Scaffolding

If `TASKS.md` does not exist, create it:

```markdown
# Task Queue

## Active
- [ ] TASK-001: Set up project structure and tooling

## Completed
(none yet)
```

If `ERRORS.md` does not exist, create it:

```markdown
# Error Log

(no errors recorded yet)
```

If `PROGRESS.md` does not exist, create it:

```markdown
# Progress Log

(no turns completed yet)
```

---

## 9. Tool & Command Conventions

| Action | Preferred approach |
|---|---|
| Read files | `Read` tool, not `cat` |
| Search code | `Grep` tool, not `grep` in Bash |
| Find files | `Glob` tool, not `find` |
| Edit files | `Edit` tool for patches, `Write` only for new files |
| Run tests | `npm test` / `npx jest` — always check exit code |
| Commit | `git commit -m "$(cat <<'EOF' ... EOF)"` heredoc format |

---

## 10. Loop Self-Check (run at end of every turn)

Before finishing the turn, verify:

- [ ] `TASKS.md` updated (completed tasks marked, next task added)
- [ ] `ERRORS.md` updated (all errors from this turn recorded)
- [ ] `PROGRESS.md` appended with this turn's summary
- [ ] Code committed and pushed (if conditions in §5 are met)
- [ ] No TODO comments left in code without a corresponding `TASKS.md` entry
