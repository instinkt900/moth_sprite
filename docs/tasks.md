# Tasks

The task list for moth_sprite. The process, the status values and the field definitions are in
[workflow.md](workflow.md).

- `/task-new <description>` adds a task.
- `/task-planning [IDs]` reviews tasks.
- `/task-session` implements reviewed tasks.

Session reports are in [sessions/](sessions/). `/task-planning` moves finished tasks to
[tasks-done.md](tasks-done.md).

## Tasks

### [todo] T-014 Preview background color and checkerboard

**Review:** unreviewed

**Depends on:**

**Goal:**
All preview windows should get controls to control the background color of the preview image. There should
also be a gray and white checkerboard option to indicate transparency.

A preview window is any window that shows an image from the sprite sheet, including the sheet itself. It might
be worth having a helper for common image drawing with a background.

**Requirements:**
- [ ] Every window that shows an image from the sprite sheet, including the Sheet window, draws a background
  behind the image.
- [ ] The background color is set with a color picker in Preferences.
- [ ] All windows share one background setting.
- [ ] The background setting is remembered across runs.
- [ ] When the chosen color has alpha 0, the background is a gray and white checkerboard, to show transparency.
  Any other alpha draws the color, not the checkerboard.
- [ ] Each checkerboard square is 128 screen pixels, and does not change with the zoom.
- [ ] The checker colors are always gray and white. They cannot be set.

**Out of scope:**

**Open questions:**
- Q: Which windows count as preview windows: Sheet, Selected Cell, Clip Preview, the clip timeline thumbnails,
  the Tools > Grid and Detect Frames previews? A: Any window that shows an image from the sprite sheet,
  including the sprite sheet itself.
- Q: Does each window have its own background setting, or do all windows share one? A: All windows share the
  background color and settings.
- Q: Is the background setting remembered across runs, in `moth_sprite.json`? A: Yes, remembered between runs.
- Q: What do the controls look like: a color picker, a few preset colors, a toolbar button or a menu?
  A: A color picker. If a transparent color is used, the checkerboard is drawn.
- Q: What size are the checkerboard squares, and do they scale with the zoom or stay a fixed size on screen?
  A: Not sure of the scale; start with 128. The checkers do not change with zoom.
- Q: Is 128 the size of one checker square in screen pixels, or the size of a 128×128 checkerboard texture?
  A: 128 screen pixels.
- Q: Where is the color picker: in each window's toolbar, in Preferences, or both? A: In Preferences.
- Q: Does "transparent" mean alpha 0 only? What is drawn for a partly transparent color: the color over the
  checkerboard, or the color alone? A: The checkerboard is drawn only at alpha 0.
- Q: Are the gray and white checker colors fixed, or can they be set? A: Always gray and white.

**Notes:**

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.

- **Cell form undo can lose an edit (found in T-011).** The X, Y, W, H and Pivot fields in the Cells window share
  one `m_pendingFrameSnapshot`. It is taken when a field is activated only if none is pending, and it is dropped
  only after an edit. If a field is focused without editing, the old snapshot stays and a later edit's undo step
  also reverts changes made in between. If focus moves straight from a later field to an earlier one, one of the
  two edits can end up with no undo step. The Clips window uses a per-widget pending edit
  (`TrackClipEdit`/`CommitClipEdit`) that avoids both problems. The form could use the same pattern.
- **A failed File > Load still changes the project path (found in T-013).** The Load menu item copies the chosen
  path into `m_pathBuffer` before `LoadSpriteSheet` runs. If the load fails, the previous project stays open, but
  the window title (before T-013, the path box) shows the file that failed to load, and File > Save writes the
  open project to that file.
