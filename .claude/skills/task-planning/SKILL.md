---
name: task-planning
description: Supervised review of tasks in docs/tasks.md. Reads the tasks and the code they affect, asks the user questions, refines the tasks and marks them as reviewed so /task-session can work on them.
argument-hint: "[task IDs, e.g. T-003 T-004]"
disable-model-invocation: true
---

# Plan and review tasks

This is a supervised session. The user is present and answers questions. Make no code changes.

Tasks to review: $ARGUMENTS

## Steps

1. Read all of `docs/workflow.md` and `docs/tasks.md`.
2. Archive finished tasks, as described in "Planning" in `docs/workflow.md`. Tell the user which tasks were moved.
3. Decide the scope:
   - If task IDs are given above, review those tasks.
   - Otherwise, review every task whose `Review` is `unreviewed`, plus any `[blocked]` or `[partial]` task whose
     `Notes` ask for a decision.
   - Mention items under `## Discovered` and ask the user whether any should become tasks.
   - Tell the user the scope before you start.
4. Read the code the tasks affect, and the recent git history, so you understand the current behaviour. Read the
   latest session log in `docs/sessions/` if there is one.
5. For each task in scope, check the points in the "Planning" section of `docs/workflow.md`. Also look for:
   - conflicts with other tasks or with existing code conventions,
   - risks that could block a session, such as changes to external dependencies,
   - requirements that cannot be checked without running the app, and what the user would need to verify.
6. Ask the user about anything that is unclear. Group questions by task. Offer choices where you can, with a
   recommendation.
7. Update `docs/tasks.md` as answers come in:
   - Write each answer into `Requirements` or `Out of scope`, or next to its question under `Open questions`.
   - Fill in `Depends on` (`none` if there are no dependencies).
   - To split a task, create new tasks with new IDs, delete the original, and update any `Depends on` that
     referred to it.
   - Put tasks in execution order, with each task after the tasks it depends on.
8. When a task has no unanswered questions, summarize its final requirements and ask the user to confirm. After
   confirmation, set `Review` to `reviewed YYYY-MM-DD` with today's date.
9. At the end, list the tasks that are now reviewed, and the tasks still unreviewed with the reason.

## Do not

- Change source code or build files.
- Mark a task as reviewed without the user's confirmation.
- Leave decisions only in the conversation. If it is not in `docs/tasks.md`, a session does not know it.
- Commit.
