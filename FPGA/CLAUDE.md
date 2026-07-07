# FPGA Project — Claude Code Loop Rules

This file governs how Claude Code operates autonomously within the FPGA project.

The project has **two parallel tracks**. Every task in `TASKS.md` is tagged with its track:

- **[XC2064]** — Bit-accurate simulation of the Xilinx XC2064 (the first commercial FPGA, 1985): CLB internals, switch-matrix routing, IOB behaviour, and eventually a real-format configuration bitstream. Historical accuracy is the success criterion — when in doubt, match the datasheet, not intuition.
- **[GAME]** — A gamified teaching tool for modern FPGA architecture (LUT6 slices, carry chains, DSP/BRAM blocks, place-and-route) aimed at people with no FPGA background. Engagement/clarity is the success criterion — a fun, correct mental model beats datasheet completeness.

Both tracks live under `FPGA/` as separate deliverables (`FPGA/FPGA.html` for [XC2064], `FPGA/game/` for [GAME]). They share this loop machinery but not code.

---

## Loop Behaviour

When running in `/loop` mode, Claude must follow every rule in this file on **every iteration** before doing anything else.

---

## 1. Read State Before Acting

At the start of each loop turn:

1. Read `TASKS.md` to load the current task queue (both tracks).
2. Read `ERRORS.md` to load known errors and their status.
3. Read `PROGRESS.md` to understand what was completed in the previous turn.

If any of these files do not exist yet, create them with empty scaffolding before continuing (see §8).

---

## 2. Picking the Next Task

- Take the first `[ ]` task in `TASKS.md` under **Active**, regardless of track — the queue order already encodes priority.
- Do not let one track stall indefinitely: if the last 3 completed tasks were all the same track and both tracks have `[ ]` tasks available, prefer the other track next, unless a hard dependency forces otherwise.
- If a task depends on unresolved research (e.g. an uncertain XC2064 datasheet detail), do the research first, record the finding as a short note in the task's sub-bullet, then implement.

---

## 3. Task Decomposition

When a task is too large to complete in one turn, decompose it:

- Break it into subtasks of ≤ 2 hours of work each.
- Write each subtask as a new entry in `TASKS.md` with status `[ ]`, keeping the parent's `[XC2064]`/`[GAME]` tag.
- Mark the parent task as `[~]` (in progress / decomposed).
- Do NOT start implementation until decomposition is written.

**Decomposition format in TASKS.md:**

```
- [~] TASK-003 [XC2064]: Implement accurate switch-matrix routing
  - [ ] TASK-003a: Model single-length line segments per channel
  - [ ] TASK-003b: Model switch-matrix pass-transistor connectivity at each intersection
  - [ ] TASK-003c: Replace ad-hoc h_wires/v_wires toggle with matrix-driven routing
  - [ ] TASK-003d: Verify against a known routed test circuit
```

---

## 4. Auto-Create Next Task

After completing any task:

1. Mark it `[x]` in `TASKS.md`.
2. Determine the **logical next step** for that track (or the other track, per §2).
3. Append the next task to `TASKS.md` with status `[ ]`, a unique `TASK-NNN` id, and its track tag.
4. Write one sentence in `PROGRESS.md` describing what was done and what was queued.

If there is no obvious next task for either track, add:
```
- [ ] TASK-NNN: Review progress and plan next milestone
```

---

## 5. Error Recording

Whenever a command fails, a test fails, rendered output is visibly wrong, or a simulated circuit produces an incorrect result:

1. Append to `ERRORS.md` immediately (do not wait until end of turn).
2. Attempt one fix. If the fix succeeds, mark the error `[resolved]`.
3. If the fix fails, mark `[blocked]` and add a new task to investigate.

**ERRORS.md entry format:**

```
### ERR-001 [resolved | blocked | open]
- **Date:** 2026-07-07
- **Track:** XC2064 | GAME
- **Task:** TASK-002b
- **Command / file:** `FPGA/FPGA.html` — calcLut()
- **Error message:** LUT output inverted for F-function with 3 inputs
- **Root cause:** Truth-table bit order didn't match datasheet input ordering (A/B/C/D vs D/C/B/A)
- **Fix applied:** Reversed bit index mapping in calcLut()
- **Follow-up task:** (none) | TASK-005
```

---

## 6. GitHub Upload (Commit & Push)

After each completed task block (or every 3 subtasks, whichever comes first):

1. Run `git status` to confirm there are staged or unstaged changes.
2. Stage only relevant files — never use `git add -A` blindly.
3. Write a commit message in the format:

   ```
   <type>(<scope>): <short description>

   - bullet 1
   - bullet 2

   Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
   ```

   Types: `feat`, `fix`, `test`, `refactor`, `docs`, `chore`
   Scope: `fpga-xc2064` for [XC2064] work, `fpga-game` for [GAME] work.

4. Push to the current branch: `git push origin <branch>`.
5. Record the commit hash in `PROGRESS.md`.

**Do NOT push if:**
- Any test/verification circuit is currently failing.
- `ERRORS.md` has an `[open]` or `[blocked]` entry that is not yet triaged.
- The change touches both tracks in one commit (keep XC2064 and GAME commits separate — they have independent histories and audiences).

---

## 7. Progress Log

Every turn must append one entry to `PROGRESS.md`:

```
## 2026-07-07T14:00Z
- Track: XC2064
- Completed: TASK-002b (switch-matrix segment model)
- Errors: ERR-001 [resolved]
- Queued: TASK-002c
- Commit: abc1234
```

---

## 8. Stopping Conditions

Pause the loop and wait for human review if:

- Three consecutive turns produce no completed task.
- A `[blocked]` error cannot be resolved after two attempts.
- A destructive git operation (force-push, reset --hard, branch delete) would be required.
- The task queue in `TASKS.md` is empty for **both** tracks.
- A [XC2064] task hinges on a datasheet fact that cannot be confirmed — pause and ask rather than guessing at historical accuracy.

When pausing, write a `## PAUSED` section at the top of `PROGRESS.md` explaining why.

---

## 9. File Scaffolding

If `TASKS.md` does not exist, create it:

```markdown
# FPGA Task Queue

## Active
- [ ] TASK-001 [XC2064]: Audit existing FPGA/FPGA.html prototype against XC2064 datasheet
- [ ] TASK-002 [GAME]: Draft learning-progression outline for gamified modern FPGA track

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

## 10. Tool & Command Conventions

| Action | Preferred approach |
|---|---|
| Read files | `Read` tool, not `cat` |
| Search code | `Grep` tool, not `grep` in Bash |
| Find files | `Glob` tool, not `find` |
| Edit files | `Edit` tool for patches, `Write` only for new files |
| Commit | `git commit -m "$(cat <<'EOF' ... EOF)"` heredoc format |

---

## 11. Loop Self-Check (run at end of every turn)

Before finishing the turn, verify:

- [ ] `TASKS.md` updated (completed tasks marked, next task added, track tags intact)
- [ ] `ERRORS.md` updated (all errors from this turn recorded)
- [ ] `PROGRESS.md` appended with this turn's summary
- [ ] Code committed and pushed (if conditions in §6 are met, and tracks weren't mixed in one commit)
- [ ] No TODO comments left in code without a corresponding `TASKS.md` entry
