---
name: task-new
description: Add a new unreviewed task to docs/tasks.md from a short description. Use when the user wants to capture a task, feature idea or bug for later planning. Captures only; does not research or review.
argument-hint: <description of the task>
---

# Add a task

Add the task described below to `docs/tasks.md`. This is a quick capture. The task is reviewed later with
`/task-planning`.

Description: $ARGUMENTS

## Steps

1. Read the "Task format" and "Capture" sections of `docs/workflow.md`, and read `docs/tasks.md`.
2. If the description above is empty and the conversation does not describe a task, ask the user for a
   description. Otherwise, do not ask questions.
3. Give the task the next ID: one more than the highest `T-NNN` in `docs/tasks.md`.
4. Write the task in the format from `docs/workflow.md`:
   - Heading: `### [todo] T-NNN Title`, with a short title.
   - `Review`: `unreviewed`.
   - `Goal`: the user's description. Keep their wording, and add context from the conversation if there is any.
   - `Requirements`: only what the user stated. Do not invent requirements.
   - `Depends on` and `Out of scope`: only if the user stated them.
   - `Open questions`: points that are clearly unclear, as `- Q: ...` with no answer.
   - Leave `Notes`, `Commits` and `Manual verification` empty.
5. Add the task at the end of the `## Tasks` section, before `## Discovered`.
6. Tell the user the ID and title of the new task.

## Do not

- Research the code, refine the task or mark it as reviewed. That is `/task-planning`.
- Change other tasks.
- Commit.
