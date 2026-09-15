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

**Review:** reviewed 2026-09-15

**Depends on:** none

**Goal:**
All preview windows should get controls to control the background color of the preview image. There should
also be a gray and white checkerboard option to indicate transparency.

A preview window is any window that shows an image from the sprite sheet, including the sheet itself. It might
be worth having a helper for common image drawing with a background.

**Requirements:**
- [ ] Every window that shows an image from the sprite sheet draws a background behind the image: Sheet, Selected
  Cell, Clip Preview, the Clips timeline thumbnails, and the Tools > Grid and Tools > Detect Frames previews.
- [ ] The background covers these areas:
  - Sheet, Selected Cell, Grid and Detect Frames: the rectangle where the image is drawn.
  - Clip Preview: the clip's whole bounding box (the area the `Dummy` reserves). It does not change size or
    position from step to step. This replaces "no background" from T-012.
  - Clips timeline: the whole 72 px thumbnail box. It replaces the dark fill that the box has now.
- [ ] Overlays (cell borders, pivots, grid lines, the overflow tint, box select) are still drawn over the image
  and the background.
- [ ] The background color is set with a color picker, with alpha, in the Preferences menu.
- [ ] All windows share one background setting.
- [ ] The background setting is saved in `moth_sprite.json` and remembered across runs. A settings file without
  it still loads, and gets the default.
- [ ] The default color has alpha 0, so the checkerboard shows on first run.
- [ ] When the chosen color has alpha 0, the background is a gray and white checkerboard, to show transparency.
  Any other alpha draws the color, not the checkerboard.
- [ ] Each checkerboard square is 128 screen pixels, except in the Clips timeline thumbnails, where it is 16 screen
  pixels. The size does not change with the zoom.
- [ ] A square corner is at the top-left of the background area, and the pattern scrolls with the image.
- [ ] The checker colors are always light gray (192, 192, 192) and white. They cannot be set.

**Out of scope:**
- The background setting is an editor setting, not project data. It is not undoable, not saved to the project
  file, and does not mark the project as having unsaved changes.
- Per-window background settings, preset color buttons, and toolbar controls.
- Changes to how the image, zoom, scrolling or overlays work.
- Changes to the moth_bridge dependency. The background is drawn with the ImGui draw list.

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
- Q: What area does the background fill? A: The image area in Sheet, Selected Cell, Grid and Detect Frames. The
  clip's whole bounding box in Clip Preview, so it does not change between steps. The whole thumbnail box in the
  Clips timeline, in place of its dark fill.
- Q: What is the default for a new or older `moth_sprite.json`? A: The checkerboard (alpha 0).
- Q: Where does the checker pattern start? A: At the top-left of the background area. It scrolls with the image.
- Q: The Clips thumbnails are 72 px, so a 128 px square shows one color. What size do they use? A: Smaller squares
  in the thumbnails (16 px). 128 px everywhere else.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-015 Cell form undo per field

**Review:** reviewed 2026-09-15

**Depends on:** none

**Goal:**
Found in T-011. The X, Y, W, H and Pivot fields in the Cells window share one `m_pendingFrameSnapshot`. It is
taken when a field is activated only if none is pending, and it is dropped only after an edit. If a field is
focused without editing, the old snapshot stays and a later edit's undo step also reverts changes made in between.
If focus moves straight from a later field to an earlier one, one of the two edits can end up with no undo step.
The Clips window uses a per-widget pending edit (`TrackClipEdit`/`CommitClipEdit`) that avoids both problems. The
form could use the same pattern.

**Requirements:**
- [ ] New `TrackFrameEdit`/`CommitFrameEdit` helpers follow the pattern of `TrackClipEdit`/`CommitClipEdit`: a
  pending edit per widget, keyed by its ImGuiID, with a `FrameVec` snapshot and an `edited` flag.
- [ ] The X, Y, W, H, Pivot X and Pivot Y fields in the Cells form each use the new helpers.
- [ ] `m_pendingFrameSnapshot` is replaced by the new pending edit, and `ClearSpriteActions` resets it.
- [ ] An edit in one field that changes the value adds one undo step. Undoing it reverts only that edit.
- [ ] Focusing a field and leaving it without a change adds no undo step and leaves no snapshot. A later change
  (a field edit, a drag on the sheet, a pivot preset) stays its own undo step.
- [ ] When focus moves straight from one field to another, by Tab or by a click, in either direction, each
  changed field gets its own undo step, and no edit is lost.
- [ ] A click on a field's - or + button that changes the value adds one undo step.
- [ ] W and H are still at least 1.

**Out of scope:**
- The Clips window and `TrackClipEdit`/`CommitClipEdit`. They do not change.
- Other ways to edit cells: drag and resize on the sheet, the pivot drag in Selected Cell, the pivot presets and
  Edit > Pivot.
- The form's layout.

**Open questions:**
- Q: A separate helper for frames, or one shared with the Clips window? A: A separate frame helper. The Clips code
  does not change.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-016 Failed Load or Save As keeps the project path

**Review:** reviewed 2026-09-15

**Depends on:** none

**Goal:**
Found in T-013. The Load menu item copies the chosen path into `m_pathBuffer` before `LoadSpriteSheet` runs. If
the load fails, the previous project stays open, but the window title shows the file that failed to load, and
File > Save writes the open project to that file. File > Open Recent has the same problem.

File > Save As has the same problem: it sets the path before it writes the file, so a failed write changes the
title and the file that later saves write to.

**Requirements:**
- [ ] When File > Load fails, the previous project stays open with its path. The window title and File > Save
  still use the previous project's file, or Untitled, where Save opens Save As.
- [ ] The same is true when File > Open Recent fails for a file that exists.
- [ ] When File > Save As fails to write the file, the project path does not change. The title and File > Save
  still use the previous file. The unsaved changes prompt still stays open when the save fails.
- [ ] A successful Load, Open Recent or Save As works as now: it sets the path, updates the title and adds the
  file to Open Recent.
- [ ] A failed Load or Open Recent does not add the file to Open Recent.
- [ ] An Open Recent entry whose file exists but fails to load stays in the list. An entry whose file no longer
  exists is still removed.
- [ ] The Load and Save As dialogs still remember the chosen folder in `LastProjectDir`, even when the load or
  the save fails.
- [ ] A failed load or save is still reported only by its existing log line.

**Out of scope:**
- Error popups or any other new UI.
- How projects are loaded and saved, and the project file format.
- Import Sheet and Export Sheet.

**Open questions:**
- Q: Should Save As be fixed in this task too? A: Yes.
- Q: What does the user see when a load fails? A: Only the existing log line, as now.
- Q: What happens to an Open Recent entry whose file exists but fails to load? A: It stays in the list.

**Notes:**

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.
