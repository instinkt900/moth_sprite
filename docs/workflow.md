# Development workflow

This document describes how work is planned and done in this project. It is the source of truth for the
`/task-new`, `/task-planning` and `/task-session` skills. The task list is in [tasks.md](tasks.md).

## Overview

Work moves through three phases.

| Phase    | Skill            | Supervised | Purpose                                                                    |
| -------- | ---------------- | ---------- | -------------------------------------------------------------------------- |
| Capture  | `/task-new`      | Yes        | Add a task to the list quickly. No review, no research.                    |
| Planning | `/task-planning` | Yes        | Review tasks against the code, answer questions, refine, mark as reviewed. |
| Session  | `/task-session`  | No         | Implement reviewed tasks without supervision.                              |

A session works only on tasks that have been reviewed. Anything a session needs to know must be written in
`tasks.md`. Answers given only in conversation do not count.

## Task format

Each task is a level-3 heading under `## Tasks` in `tasks.md`, followed by its fields:

```markdown
### [todo] T-000 Title

**Review:** unreviewed

**Depends on:** T-000, T-000

**Goal:**
What the task is for and why. Free prose, in the words of whoever asked for it.

**Requirements:**
- [ ] A concrete, checkable requirement.

**Out of scope:**
Things this task must not change.

**Open questions:**
- Q: A question. A: The answer.

**Notes:**

**Commits:**

**Manual verification:**
```

### Heading

`### [status] T-NNN Title`

- **ID:** `T-` followed by three digits. IDs are given in order and never reused, including for deleted or
  split tasks. Use the ID in `Depends on`, in commit messages and in the session log.
- **Order:** The order of tasks in `## Tasks` is the execution order. Planning sets the order, and a task always
  comes after the tasks it depends on.

### Status

| Status          | Meaning                                                                     |
| --------------- | --------------------------------------------------------------------------- |
| `[todo]`        | Not started.                                                                |
| `[in-progress]` | A session is working on it now.                                             |
| `[done]`        | All requirements are met and committed.                                     |
| `[partial]`     | Some requirements are met and committed. Notes list what is left, and why.  |
| `[blocked]`     | Cannot continue without a decision or outside change. Notes give the cause. |
| `[deferred]`    | Deliberately postponed. Notes give the reason.                              |

### Review

- `unreviewed`: New, or changed since its last review. Sessions skip it.
- `reviewed YYYY-MM-DD`: Set only by `/task-planning`, with the user's confirmation.

Changes to `Goal`, `Requirements`, `Out of scope` or `Depends on` made outside `/task-planning` reset the
field to `unreviewed`.

### Fields

| Field                 | Written by        | Content                                                            |
| --------------------- | ----------------- | ------------------------------------------------------------------ |
| `Review`              | New, planning     | See [Review](#review).                                             |
| `Depends on`          | New, planning     | IDs of tasks that must be `[done]` first, or `none`.               |
| `Goal`                | New, planning     | Intent and context.                                                |
| `Requirements`        | New, planning     | Checklist. Each item can be verified. Sessions tick items.         |
| `Out of scope`        | New, planning     | Limits of the task.                                                |
| `Open questions`      | New, planning     | Questions and their answers. Reviewed tasks have no unanswered Q.  |
| `Notes`               | Session           | Assumptions made, problems met, reasons for the status.            |
| `Commits`             | Session           | Short hashes and subjects of the commits for this task.            |
| `Manual verification` | Session           | Steps for the user to check the result in the running app.         |

## Capture (`/task-new`)

- Write down what was said. Do not research the code, and do not refine or review the task.
- Set status `[todo]` and review `unreviewed`.
- Do not invent requirements. Record unclear points under `Open questions`.
- Add the task at the end of `## Tasks`.

## Planning (`/task-planning`)

- Make no code changes.
- **Archive finished tasks first.** A `[done]` task is finished when every commit in its `Commits` field is in the
  current branch (`git merge-base --is-ancestor <hash> HEAD`). Move each finished task from `tasks.md` to the end
  of `docs/tasks-done.md`, and create that file if it does not exist. Sessions never move tasks, because the user
  may not merge the session branch.
- Read the tasks in scope and the code they affect.
- For each task, check that:
  - the goal is clear,
  - each requirement is concrete and can be verified,
  - dependencies on other tasks are listed,
  - limits are stated under `Out of scope`,
  - the task is small enough to reach `[done]` in one session. If not, split it into new tasks with new IDs.
- Ask the user about anything that is unclear. Write every answer into the task.
- Put the tasks in execution order.
- Mark a task as reviewed only when it has no unanswered questions and the user confirms it.

## Session (`/task-session`)

The session runs without supervision. After it starts, it does not ask the user questions.

### Before starting

Do these checks in order. If a check fails, stop the session before any change is made, and tell the user what
failed and how to fix it.

1. **Clean working tree.** `git status --porcelain` shows no output. Uncommitted changes and untracked files
   that are not ignored both fail the check. Do not stash, commit or discard them. That decision is the user's.
2. **Build directory exists.** `build/Debug` is configured by Conan. The session does not run `conan install`
   and does not download or install anything. If the directory is missing, tell the user to run
   `conan install . --build=missing -s build_type=Debug` and try again.
3. **Baseline build passes.** `cmake --build --preset conan-debug` succeeds with no changes made. If it fails,
   report the errors. Do not try to fix them.
4. **Baseline smoke launch passes.** `tools/smoke_launch.sh` passes with no changes made. If it fails, or cannot
   run, report the output. Do not try to fix it.
5. **Create the session branch.** Create and check out `session/YYYY-MM-DD` from the current branch, using
   today's date. If that branch exists, add `-2`, `-3` and so on. All session commits go on this branch, except work saved
   to a `wip/T-NNN` branch (see [Unfinished work](#unfinished-work)).

The session never commits to the branch it started from, and never merges, rebases or pushes. The user reviews
the session branch and decides whether to merge it. Record the branch name and its base branch in the session
log.

### Selecting tasks

A task is eligible when its status is `[todo]` or `[partial]`, its review is `reviewed`, and every task in
`Depends on` is `[done]`, or is eligible and comes before it in the session.

### Per task

1. Re-read the task in `tasks.md`.
2. Set the status to `[in-progress]`.
3. Implement the requirements. Tick each one when it is met.
4. Check the work against the [definition of done](#definition-of-done). Fix what fails.
5. Commit the code. See [Commits](#commits).
6. Update the task: status, `Notes`, `Commits`, `Manual verification`. Commit `tasks.md` on its own.

`tasks.md` holds the session's progress. Keep it up to date as you work, and commit it after each task, so that
progress survives a context reset or a crash.

### Commits

- **Format:** `<type>(T-NNN): <subject>`, for example `feat(T-003): Edit > Pivot submenu with nine anchor
  options`. Use `feat`, `fix`, `refactor`, `build` or `docs` as the type. Add a body when the reason for the
  change is not obvious from the subject.
- **Size:** A task can have more than one commit. Each commit must build. The full
  [definition of done](#definition-of-done) is checked before the task's last code commit.
- **Content:** Stage files by path. Commit only files changed for the task.
- **Task status:** After the task's last code commit, update the task in `tasks.md` and commit only that file as
  `docs(T-NNN): update task status`. This commit records the hashes of the code commits.
- **History:** Do not amend, rebase, merge or push.

### Definition of done

A task is `[done]` only when all of these are true. If some requirements are met and the others cannot be, the
status is `[partial]`.

1. **Requirements.** Every item is ticked.
2. **Build.** `cmake --build --preset conan-debug` succeeds. The Debug build treats warnings as errors and runs
   clang-tidy.
3. **clang-tidy.** The task adds no findings. A `// NOLINT(check-name)` is allowed only with a comment that
   explains why. List each one in `Notes`.
4. **Smoke launch.** The app passes the [smoke launch](#smoke-launch).
5. **Self-review.** Read the full diff of the task and check that:
   - each requirement is met, and nothing listed in `Out of scope` changed,
   - there are no unrelated changes, reformatting or refactors,
   - there is no debug output, commented-out code or unexplained `TODO`,
   - the rules in `CLAUDE.md` are followed, in particular that every change to the sprite sheet project goes
     through the undo stack,
   - project files saved before the change still load.

   Fix what you find, then do the build and the smoke launch again.
6. **Manual verification.** `Manual verification` has steps for each requirement that can only be checked by
   using the app.

There are no automated tests. Adding tests is a separate task.

### Smoke launch

Run `tools/smoke_launch.sh`. Do not launch the app any other way. The script:

1. Starts `build/Debug/moth_sprite` in a new temporary directory. The app reads and writes `moth_sprite.json`
   and `imgui.ini` in the current directory, and launching from the repository would change the user's files.
2. Waits for the "Moth Sprite" window, then lets the app run for 5 seconds.
3. Closes the app with `tools/close_window.py`, which sends `WM_DELETE_WINDOW`, the same request as clicking the
   window's close button. The app's normal shutdown runs.
4. Passes when all of these are true:
   - the app exits with code 0,
   - the log has no `[warning]` or `[error]` lines (the Debug build enables the Vulkan validation layers, so
     Vulkan misuse shows here),
   - `moth_sprite.json` was written, which shows that `Shutdown()` finished.

The script prints `SMOKE LAUNCH PASSED` and exits with 0, or prints `SMOKE LAUNCH FAILED` with the reasons and the
path of the log. Quote the relevant log lines in `Notes`. Exit code 2 means the script could not run (no display,
no build, missing tool).

The script needs an X11 display, `xdotool` and `python3`.

### Ambiguity and blockers

- **Minor ambiguity** (layout, naming, small defaults): choose the simplest reasonable option, record the
  assumption in `Notes`, and continue.
- **Block** only when a choice is expensive to reverse: file format changes, dependency changes, removal of
  existing behaviour, or conflict with another task. Set `[blocked]`, write the cause in `Notes`, and continue
  with the next eligible task.
- **Retry limit.** If a failure (build, clang-tidy, smoke launch, or a requirement that does not work) is still
  there after three different approaches, block the task. Record each approach in `Notes`. Repeating the same
  fix with small changes counts as one approach.
- When a task is blocked, tasks that depend on it are blocked too.
- A session must not change `Goal`, `Requirements`, `Out of scope` or `Depends on`. If a requirement proves
  wrong, block the task and explain why in `Notes`.

### Unfinished work

When a task is blocked partway through, save its work and return the session branch to a good state before the
next task.

1. **Save everything to a wip branch.** Create `wip/T-NNN` from the current commit, commit all uncommitted changes
   of the task there as `wip(T-NNN): <what the changes are>`, then switch back to the session branch. The wip
   commit does not need to build.
2. **Decide about the task's commits on the session branch.** Keep them only if all of these are true:
   - the build passes,
   - the smoke launch passes,
   - the self-review from the [definition of done](#definition-of-done) passes for the work that exists,
   - features that worked before the task still work.

   If they are kept, the status is `[partial]` and the ticked requirements show what is done. If not, undo them
   with `git revert`, newest first. They stay available on `wip/T-NNN`. The status is `[blocked]`.
3. **Check the session branch.** The build and the smoke launch pass before the next task starts.
4. **Write `Notes`.** Give the cause of the block, the approaches tried, what is done, what is left, and the name
   of the wip branch. Commit `tasks.md` as described in [Commits](#commits).

### End of session

When no eligible tasks are left:

1. **Final checks.**
   - `cmake --build --preset conan-debug --clean-first` succeeds.
   - `tools/smoke_launch.sh` passes.
   - `git status --porcelain` shows no output.
   - `git log --oneline <base branch>..HEAD` shows only commits that the session log accounts for.

   Do not fix a failed final check. Record it under "Needs attention" in the session log.
2. **Session log.** Write the session log in the format below. Name it after the session branch:
   `session/2026-09-15-2` has the log `docs/sessions/2026-09-15-2.md`. Commit it as `docs: session log <name>`.
3. **Summary.** Give the user a short summary in the conversation: the final status of each task, what needs
   their attention, and the path of the session log.

If the session stops during "Before starting", it creates no branch and no log. Report the failed check in the
conversation only.

#### Session log format

```markdown
# Session 2026-09-15

**Branch:** `session/2026-09-15`, from `master` at `abc1234`

**Final checks:** clean build passed, smoke launch passed

## Tasks

| Task                     | Status    | Commits                     |
| ------------------------ | --------- | --------------------------- |
| T-002 Multiple selection | [done]    | abc1234, def5678            |
| T-003 Pivot helpers      | [blocked] | (reverted), see `wip/T-003` |

## Needs attention

Blocked and partial tasks with the reason, wip branches, failed final checks, NOLINT suppressions, and
decisions the user must make.

## Skipped

Tasks not worked on, with the reason: unreviewed, dependency not ready, or status.

## Assumptions

Assumptions made during the session, grouped by task.

## Manual verification

The tasks to check by hand, in a sensible order. The steps are in each task's `Manual verification` field in
`docs/tasks.md`.
```

## General rules

- Do only what the task asks. Do not refactor unrelated code. Record problems found along the way under
  `## Discovered` in `tasks.md`, where they can be turned into tasks later.
