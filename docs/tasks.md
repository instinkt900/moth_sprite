# Tasks

The task list for moth_sprite. The process, the status values and the field definitions are in
[workflow.md](workflow.md).

- `/task-new <description>` adds a task.
- `/task-planning [IDs]` reviews tasks.
- `/task-session` implements reviewed tasks.

Session reports are in [sessions/](sessions/). `/task-planning` moves finished tasks to
[tasks-done.md](tasks-done.md).

## Tasks

### [todo] T-001 Editor windows

**Review:** unreviewed

**Depends on:**

**Goal:**
The editor should use the docking branch of ImGui and be made up of dockable windows. The initial windows are:

- The full tilesheet where the cells are shown, zoom options, etc.
- The selected cell. Same zoom options and pivot rendering. This replaces the existing cell preview and
  keeps the click to set pivot behaviour.
- A cell list, showing the current list of defined cells. Rather than a table it should be a scrollable list,
  and clicking one will select the cell, update the preview, highlight the cell on the sheet and update a form
  below the list showing an editable set of entries with x/y/w/h values etc. for the cell.
- A clip editor window. This window should allow the user to create/remove clips. Clips should be stacked one
  on top of the other. Each clip should be shown like a timeline of frames horizontally. Single clicking a
  frame should select the cell. Double clicking should allow the user to select a new cell for that frame,
  either through the cell list or clicking the tilesheet cells - anything that invokes the "select cell"
  behaviour. Timing will also need to be editable per frame on each clip. A play/pause button should also
  exist that auto scrolls through the selected clip.
- A clip preview window. Almost a duplicate of the selected cell window, but without any pivot or any other
  guides or markup that might come with the cell window. This should show the clip preview with play/pause
  buttons that mirror the ones on the clip window. The idea of this window is to be a dedicated "what does the
  animation look like" preview.

**Requirements:**
- [ ] The editor uses the ImGui docking branch and is made up of dockable windows.
- [ ] Sheet window: the full tilesheet with cells shown and zoom options.
- [ ] Selected cell window: zoom options and pivot rendering. Replaces the existing cell preview. Clicking sets
  the pivot.
- [ ] Cell list window: a scrollable list (not a table) of defined cells. Clicking a cell selects it, updates
  the preview and highlights it on the sheet.
- [ ] Cell list window: a form below the list with editable x/y/w/h values etc. for the selected cell.
- [ ] Clip editor window: create and remove clips. Clips are stacked vertically, each shown as a horizontal
  timeline of frames.
- [ ] Clip editor window: single click on a frame selects its cell.
- [ ] Clip editor window: double click on a frame lets the user pick a new cell for it through any "select
  cell" behaviour (cell list or sheet).
- [ ] Clip editor window: timing is editable per frame.
- [ ] Clip editor window: play/pause button that steps through the selected clip.
- [ ] Clip preview window: shows the clip animation with no pivot, guides or markup. Its play/pause mirrors the
  clip editor.

**Out of scope:**
Other windows, such as tools, stay as they are.

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-002 Multiple selection

**Review:** unreviewed

**Depends on:**

**Goal:**
The user should be able to select multiple cells by holding ctrl while clicking cells. The selection will be
used to perform actions that can be applied to multiple cells: deleting, modifying pivot, etc. There will also
be a "prime" selected concept. For actions that can only use one cell, the "prime" selection will be used,
e.g. assigning to a clip frame.

**Requirements:**
- [ ] Ctrl+click adds or removes cells from the selection.
- [ ] Multi-cell actions (delete, modify pivot, etc.) apply to the whole selection.
- [ ] The selection has a "prime" cell, used by actions that take a single cell (e.g. assigning to a clip frame).

**Out of scope:**

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-003 Pivot helpers

**Review:** unreviewed

**Depends on:**

**Goal:**
Under the Edit menu, there should be options for setting pivots by a rule. They apply to the whole selection
and are grouped in the menu under their own "Pivot" entry.

**Requirements:**
- [ ] Edit > Pivot submenu with nine options: Top Left, Top Center, Top Right, Center Left, Center,
  Center Right, Bottom Left, Bottom Center, Bottom Right.
- [ ] Each option applies to the whole selection.

**Out of scope:**

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-004 Export sheet option

**Review:** unreviewed

**Depends on:**

**Goal:**
Under File there should be an option to export the sprite sheet. This behaves like a "save as" for the sprite
sheet image. When the sheet is exported, the project's reference to the sheet file is updated to the new
location, so the next project save writes the new path. This is so you can import a sheet from one location,
export it to a new location, and be sure the project references the new location rather than the old.

**Requirements:**
- [ ] File > Export sheet writes the sprite sheet image to a location chosen by the user.
- [ ] After export, the project's sheet path points to the exported file.
- [ ] Saving the project afterwards writes the new sheet path.

**Out of scope:**

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-005 Recent files

**Review:** unreviewed

**Depends on:**

**Goal:**
There should be a short history of recently opened files. Additionally, the load/save dialogs should remember
their last location across runs.

**Requirements:**
- [ ] A short history of recently opened files is available.
- [ ] Load and save dialogs open at their last used location, including after a restart.

**Out of scope:**

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.
