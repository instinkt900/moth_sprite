---
name: task-session
description: Start an unsupervised implementation session over the reviewed tasks in docs/tasks.md, following docs/workflow.md.
disable-model-invocation: true
---

# Run a task session

This session runs without supervision. After the start check, do not ask the user questions. Follow
`docs/workflow.md` for all decisions.

## Start check

1. Read all of `docs/workflow.md` and `docs/tasks.md`.
2. Find the eligible tasks, as defined in "Selecting tasks" in `docs/workflow.md`.
3. If no task is eligible, stop. Tell the user which tasks are waiting for review and suggest `/task-planning`.
4. Tell the user:
   - the eligible tasks, in the order they will be done,
   - the tasks that will be skipped, with the reason (unreviewed, dependency not ready, status).
5. Continue to "Before starting". Do not wait for a reply.

## Before starting

Do the checks in "Before starting" in `docs/workflow.md`, in order: clean working tree, build directory, baseline
build, baseline smoke launch, session branch. If any check fails, stop and report. Do not try to fix a failed check.

## Work

For each eligible task, in order, follow "Per task" and "Ambiguity and blockers" in `docs/workflow.md`.

- Work only on eligible tasks. Never work on an `unreviewed` task, even if it looks simple.
- Before each task, re-read it in `docs/tasks.md`. The file, not your memory, holds the session's progress.
- If a task becomes blocked, follow "Unfinished work" in `docs/workflow.md`, then re-check which of the remaining
  tasks are still eligible.

## End of session

Follow "End of session" in `docs/workflow.md`: final checks, session log in `docs/sessions/`, then a short summary
for the user. Do not fix a failed final check. Report it.
