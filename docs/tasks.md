# Tasks

The task list for moth_sprite. The process, the status values and the field definitions are in
[workflow.md](workflow.md).

- `/task-new <description>` adds a task.
- `/task-planning [IDs]` reviews tasks.
- `/task-session` implements reviewed tasks.

Session reports are in [sessions/](sessions/). `/task-planning` moves finished tasks to
[tasks-done.md](tasks-done.md).

## Tasks

### [done] T-007 Docking setup

**Review:** reviewed 2026-09-15

**Depends on:** none

**Goal:**
Set up the editor to use the docking branch of ImGui, so that the editor can be made up of dockable windows. The
existing UI all goes into a single window, so no functionality is lost. The later window tasks move parts of it
into their own windows, and T-013 removes the single window.

Also add a Window menu with a toggle for every window, so the user can turn off every window and have only an
empty application window.

**Requirements:**
- [x] The editor uses the ImGui docking branch, with docking enabled.
- [x] All of the existing UI is in a single dockable window.
- [x] No existing functionality is lost.
- [x] A Window menu has a toggle for every window, starting with the single window.
- [x] With every window turned off, only the empty application window is shown.
- [x] Whether each window is open or closed is remembered across runs, in `moth_sprite.json`.
- [x] The menus (File, Edit, Tools, Preferences, Window) are in the application's main menu bar, so they stay
  available with every window turned off.
- [x] The area below the main menu bar is a dock space.
- [x] Ctrl+Z, Ctrl+Y, Delete and Esc work whenever the app has focus, except while a text field is active.
- [x] On first run (no `imgui.ini`), the windows start docked in a built-in default layout.
- [x] Window > Reset Layout restores the default layout.

**Out of scope:**
Other windows, such as tools, stay as they are.

**Open questions:**
- Q: Does the ImGui that comes with `moth_bridge` include the docking branch, or does the dependency need to
  change? A: It includes it. moth_toolkit's GLFW platform already sets `ImGuiConfigFlags_DockingEnable`. It does
  not create a dock space or set `IniFilename`.
- Q: When every part has moved to its own window, is the single window removed? A: Yes, in T-013.
- Q: Is the on/off state of each window remembered between runs? A: Yes.
- Q: Where is the open/closed state saved? A: In `moth_sprite.json`, with the other editor settings.
- Q: When do the keyboard shortcuts work? A: Whenever the app has focus, unless a text field is active. Today
  they only work while the single editor window has focus.
- Q: What is the layout on first run? A: A built-in default layout, with Window > Reset Layout to restore it.

**Notes:**
- The single window is named "Sprite Editor". Its open state is `ShowSpriteEditorWindow` in `moth_sprite.json`.
  A settings file without the key opens the window.
- The dock space is a borderless host window with a fixed dock space ID, not `DockSpaceOverViewport`. The
  package has no `imgui.cpp`, so the ID that `DockSpaceOverViewport` uses cannot be checked, and the default
  layout needs a known ID. The default layout is built when `imgui.ini` has no node for that ID: on first run,
  or with an `imgui.ini` written before this change.
- Assumption: Window > Reset Layout also opens every window, so the result matches the first-run layout.
- Assumption: the shortcuts are also skipped while a modal tool popup (Grid, Detect Frames) is open. Before this
  change they did not work there either, and Delete would otherwise remove a cell the user cannot see.
- As the requirement says, Ctrl+Z and Ctrl+Y no longer undo the project while a text field is active. They
  used to.
- The Grid and Detect Frames popups are drawn outside every window, so Tools works with every window closed.
- The DockBuilder functions come from `imgui_internal.h`.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- a3badf4 feat(T-007): dock space, main menu bar and Window menu

**Manual verification:**
1. Delete (or move away) `imgui.ini` in the folder you start the app from. Start the app. The File, Edit,
   Tools, Preferences and Window menus are in the main menu bar. The "Sprite Editor" window fills the area
   below it, docked.
2. Check that the existing features still work in the window: File > Import Sheet, the sheet view (Fit, 1:1,
   wheel zoom, select, drag, resize), New Cell, the frame list and form, the pivot preview and presets, clips
   and playback, Tools > Grid and Tools > Detect Frames, File > Save and Load.
3. Drag the window's tab out of the dock space and dock it again on a side. It docks.
4. Window > Sprite Editor: untick it. Only the empty application window and the main menu bar are left. The
   menus still work (for example Tools > Grid with a sheet loaded, or Edit > Undo).
5. With the window closed, quit the app. Start it again. The window is still closed. Tick
   Window > Sprite Editor. It opens where it was.
6. Move the window out of the dock, then choose Window > Reset Layout. It returns to the default docked
   layout. Close the window and choose Reset Layout again. It opens, docked.
7. Select a cell. Click on an empty part of the dock space or on the main menu bar, so that the Sprite Editor
   window does not have focus. Press Delete: the cell is removed. Press Ctrl+Z: it comes back. Press Ctrl+Y:
   it is removed again.
8. Click in a cell's X field so the text field is active. Press Delete and Ctrl+Z. They edit the text and do
   not change the project's undo history.
9. Click New Cell, move focus away from the window, then press Esc. New Cell mode ends.

### [done] T-008 Sheet window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable window with the full tilesheet where the cells are shown, zoom options, etc.

**Requirements:**
- [x] A dockable window shows the full tilesheet with the cells.
- [x] The window has the zoom options of today's sheet view: Fit, 1:1 and mouse-wheel zoom.
- [x] All existing sheet behaviour works in the window: clicking a cell selects it, dragging moves or resizes the
  selected cell, and cell rects and pivot markers are drawn.
- [x] The New Cell button is in the window's toolbar, next to the zoom options. New Cell drawing works as today.
- [x] The window is part of the default layout from T-007.
- [x] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: Which window has the New Cell button? A: The sheet window, in its toolbar.

**Notes:**
- The window is named "Sheet". Its open state is `ShowSheetWindow` in `moth_sprite.json`.
- The default layout splits the dock space: Sheet on the left (60%), Sprite Editor on the right.
- The Sprite Editor window no longer has the two-column table. It holds only the path box, frames and clips.
- The New Cell button is removed from the frames pane. In the Sheet toolbar it comes first, then Fit, 1:1 and
  the zoom percentage, so that the zoom text changing width does not move the button. The New Cell hint is at
  the end of the toolbar.
- Assumption: closing the Sheet window ends New Cell mode, because the mode draws on that window's canvas.
- Assumption: with no sheet image, the Sheet window shows "Use File > Import Sheet to add a sheet image." instead
  of staying blank.
- An `imgui.ini` saved by the T-007 build already has a dock space node, so the default layout is not rebuilt
  there, and the Sheet window first opens floating. Window > Reset Layout docks it.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- c08200b feat(T-008): dockable Sheet window with New Cell in its toolbar

**Manual verification:**
1. Start the app with no `imgui.ini` (or choose Window > Reset Layout). The Sheet window is docked on the left
   and the Sprite Editor window on the right.
2. With no sheet image, the Sheet window shows the Import Sheet hint. File > Import Sheet: the sheet appears,
   fitted to the window.
3. Click Fit, 1:1 and use the mouse wheel over the sheet. The zoom changes as before, and the wheel zooms around
   the cursor.
4. Add cells (Tools > Grid). Click a cell on the sheet: it is selected, and it is also selected in the frame
   list. Drag inside it to move it, and drag an edge or corner to resize it. Ctrl+Z undoes each drag as one step.
   Cell rects and pivot markers are drawn.
5. Click New Cell in the Sheet toolbar. The hint appears, and the button is disabled. Drag on the sheet: a new
   cell is added and selected. Click New Cell again and press Esc: the mode ends. The frames pane in the
   Sprite Editor window has no New Cell button.
6. Click New Cell, then untick Window > Sheet. Tick it again: New Cell mode has ended.
7. Untick Window > Sheet, quit and start again. The Sheet window is still closed. Tick it: it opens.

### [todo] T-009 Selected cell window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable window showing the selected cell. Same zoom options and pivot rendering. This replaces the existing
cell preview and keeps the click to set pivot behaviour.

**Requirements:**
- [ ] A dockable window shows the selected cell.
- [ ] The window has the same zoom options as the sheet window (Fit, 1:1, mouse-wheel zoom) and draws the pivot.
- [ ] Clicking or dragging in the window sets the pivot, as the existing cell preview does.
- [ ] The 3×3 pivot preset buttons (TL, T, TR, L, C, R, BL, B, BR) are in the window, below the cell image.
- [ ] The window replaces the existing cell preview.
- [ ] The window is part of the default layout from T-007.
- [ ] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: Where does the 3×3 pivot preset grid go? A: In the selected cell window, below the cell image. T-003 adds
  the same presets to Edit > Pivot for the whole selection.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-010 Cell list window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable window with the current list of defined cells. Rather than a table it should be a scrollable list,
and clicking one will select the cell, update the preview, highlight the cell on the sheet and update a form
below the list showing an editable set of entries with x/y/w/h values etc. for the cell.

**Requirements:**
- [ ] A dockable window shows the defined cells as a scrollable list, not a table.
- [ ] Each entry shows the cell index and rect (for example `3   (32, 0) 32×32`) and a delete button.
- [ ] Clicking a cell selects it, updates the preview and highlights it on the sheet.
- [ ] A form below the list has editable X, Y, W, H, Pivot X and Pivot Y values for the selected cell.
- [ ] The window is part of the default layout from T-007.
- [ ] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**
The New Cell button. It moves to the sheet window (T-008).

**Open questions:**
- Q: What does each list entry show? A: The cell index and rect.
- Q: Does each entry keep its delete button? A: Yes. The Delete key also works.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-011 Clip editor window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable clip editor window. This window should allow the user to create/remove clips. Clips should be
stacked one on top of the other. Each clip should be shown like a timeline of frames horizontally. Single
clicking a frame should select the cell. Double clicking should allow the user to select a new cell for that
frame, either through the cell list or clicking the tilesheet cells - anything that invokes the "select cell"
behaviour. Timing will also need to be editable per frame on each clip. A play/pause button should also exist
that auto scrolls through the selected clip.

**Requirements:**
- [ ] A dockable window where the user can create and remove clips.
- [ ] Clips are stacked vertically, each shown as a horizontal timeline of frames.
- [ ] Each frame on the timeline shows a thumbnail of its cell, with its duration (ms) editable below it.
- [ ] Clicking a clip selects it.
- [ ] Each clip can be renamed, and its loop type (Stop, Reset, Loop) changed.
- [ ] Steps can be added ("+ Step" adds the selected cell) and removed.
- [ ] Steps can be reordered by dragging them along the timeline.
- [ ] Single click on a frame selects its cell and moves playback to that frame.
- [ ] Double click on a frame starts picking a new cell for it. The frame is highlighted while picking. The next
  cell selected through any "select cell" behaviour (cell list or sheet) is assigned to the frame and picking
  ends. Esc cancels picking.
- [ ] A play/pause button plays the selected clip. Pause stops on the current step, and play continues from
  there. The timeline scrolls to keep the current step visible.
- [ ] A Step button advances one step.
- [ ] The window is part of the default layout from T-007.
- [ ] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: Which features of today's clip pane stay? A: All of them: rename, loop type, add and remove steps, and
  the Step button. Reordering steps by dragging is new.
- Q: What does each timeline frame show? A: A thumbnail of the cell, with its duration below it.
- Q: How does picking a new cell end? A: The next cell selected is assigned, or Esc cancels.
- Q: How does play/pause behave? A: Pause keeps the current step, and play continues from it. Today, Play
  always restarts from the first step.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-012 Clip preview window

**Review:** reviewed 2026-09-15

**Depends on:** T-007, T-009, T-011

**Goal:**
A dockable clip preview window. Almost a duplicate of the selected cell window, but without any pivot or any
other guides or markup that might come with the cell window. This should show the clip preview with play/pause
buttons that mirror the ones on the clip window. The idea of this window is to be a dedicated "what does the
animation look like" preview.

**Requirements:**
- [ ] A dockable window shows the selected clip's animation.
- [ ] Each frame is anchored on its cell's pivot, as in today's clip preview, so the animation does not jump.
- [ ] The window shows no pivot, guides or markup.
- [ ] The window has the same zoom options as the selected cell window (Fit, 1:1, mouse-wheel zoom).
- [ ] Play/pause buttons mirror the ones in the clip editor window: both control the same playback, with the
  same behaviour.
- [ ] The window is part of the default layout from T-007.
- [ ] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: What zoom does the preview have? A: The same as the selected cell window.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-013 Remove the old UI window

**Review:** reviewed 2026-09-15

**Depends on:** T-008, T-009, T-010, T-011, T-012

**Goal:**
Once all of the new windows are added, remove the single window that holds the old UI.

**Requirements:**
- [ ] The single window from T-007 is removed.
- [ ] Its entry is removed from the Window menu and from the default layout.
- [ ] The project path box from the old UI is replaced by the window title: `Moth Sprite - <project file name>`,
  or `Moth Sprite - Untitled` for a project that has not been saved.
- [ ] No other existing functionality is lost.

**Out of scope:**
Other windows, such as tools, stay as they are.

**Open questions:**

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-002 Multiple selection

**Review:** reviewed 2026-09-15

**Depends on:** T-013

**Goal:**
The user should be able to select multiple cells by holding ctrl while clicking cells. The selection will be
used to perform actions that can be applied to multiple cells: deleting, modifying pivot, etc. There will also
be a "prime" selected concept. For actions that can only use one cell, the "prime" selection will be used,
e.g. assigning to a clip frame.

**Requirements:**
- [ ] Ctrl+click, on the sheet or in the cell list, adds or removes a cell from the selection. A plain click
  selects only the clicked cell.
- [ ] The selection has a "prime" cell: the cell most recently clicked or ctrl+clicked into the selection.
- [ ] If the prime cell is removed from the selection, the most recently added cell still selected becomes prime.
- [ ] The prime cell is drawn in a different colour from the other selected cells, on the sheet and in the list.
- [ ] Actions that take a single cell use the prime cell: the selected cell window, the cell form, "+ Step" and
  picking a cell for a clip frame.
- [ ] Delete removes every selected cell, as one undo action.
- [ ] Dragging inside a selected cell on the sheet moves every selected cell together, as one undo action. Resize
  handles appear only on the prime cell.
- [ ] Shift+click in the cell list selects every cell between the prime cell and the clicked cell.
- [ ] Ctrl+A selects every cell.
- [ ] Esc clears the selection, when Esc is not already cancelling something (New Cell drawing, picking a cell).
- [ ] Dragging on an empty part of the sheet draws a box, and selects every cell inside it.
- [ ] Undo and redo restore the whole selection, as they restore the single selection today.

**Out of scope:**
Pivot changes for the whole selection. They are in T-003.

**Open questions:**
- Q: Which cell is prime? A: The most recently clicked or ctrl+clicked cell.
- Q: What becomes prime when the prime cell is deselected? A: The most recently added cell still selected.
- Q: What does dragging do with several cells selected? A: Moves all of them. Only the prime cell can be resized.
- Q: Which extra selection features are included? A: Shift+click range in the list, Ctrl+A, Esc to clear, and
  box select on the sheet.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-003 Pivot helpers

**Review:** reviewed 2026-09-15

**Depends on:** T-002

**Goal:**
Under the Edit menu, there should be options for setting pivots by a rule. They apply to the whole selection
and are grouped in the menu under their own "Pivot" entry.

**Requirements:**
- [ ] Edit > Pivot submenu with nine options: Top Left, Top Center, Top Right, Center Left, Center,
  Center Right, Bottom Left, Bottom Center, Bottom Right.
- [ ] Each option sets the pivot of every selected cell, relative to that cell's own size, as one undo action.
- [ ] Pivot positions match today's 3×3 preset grid: center is half the width or height, rounded down, and
  right and bottom are the full width and height.
- [ ] The 3×3 pivot preset grid in the selected cell window also applies to the whole selection.

**Out of scope:**

**Open questions:**
- Q: Does the 3×3 grid in the selected cell window also apply to the whole selection? A: Yes, so there is one
  rule for pivots.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-004 Export sheet option

**Review:** reviewed 2026-09-15

**Depends on:** T-008

**Goal:**
Under File there should be an option to export the sprite sheet. This behaves like a "save as" for the sprite
sheet image. When the sheet is exported, the project's reference to the sheet file is updated to the new
location, so the next project save writes the new path. This is so you can import a sheet from one location,
export it to a new location, and be sure the project references the new location rather than the old.

**Requirements:**
- [ ] File > Export Sheet copies the sheet image file, unchanged and in the same format, to a location chosen by
  the user. The save dialog keeps the original extension.
- [ ] Export Sheet is disabled when the project has no image path.
- [ ] After export, the project's sheet image path points to the exported file.
- [ ] Saving the project afterwards writes the new sheet image path.
- [ ] The image path change is one undo action. Undo points the project back at the old image path. The exported
  file stays on disk.
- [ ] The sheet window shows the sheet image path as read-only text.

**Out of scope:**
A file selector for changing the image path from the sheet window. It may be added later.

**Open questions:**
- Q: How is the image written? A: The original file is copied. The editor never changes the image's pixels.
- Q: Is the path change undoable? A: Yes. So that the user can see which image the project uses, the sheet
  window shows the image path as read-only text.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-005 Recent files

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
There should be a short history of recently opened files. Additionally, the load/save dialogs should remember
their last location across runs.

**Requirements:**
- [ ] File > Open Recent lists the 10 most recently loaded or saved projects, newest first.
- [ ] Choosing an entry loads that project. If the file no longer exists, the entry is removed and a warning is
  logged.
- [ ] File > Open Recent > Clear Recent empties the list.
- [ ] The project dialogs (Load, Save As) open in the folder last used by a project dialog.
- [ ] The image dialogs (Import Sheet, Export Sheet) open in the folder last used by an image dialog.
- [ ] The recent list and both folders are saved in `moth_sprite.json`, so they are remembered after a restart.

**Out of scope:**

**Open questions:**
- Q: How do recent files work? A: A File > Open Recent submenu with the 10 most recent projects and Clear Recent.
- Q: Do the dialogs share one remembered folder? A: No. Project dialogs and image dialogs each remember their own.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-006 Unsaved changes indicator and prompt

**Review:** reviewed 2026-09-15

**Depends on:** T-005, T-013

**Goal:**
Add an unsaved edits indicator, and a prompt on closing or starting a new project that warns of unsaved changes.

**Requirements:**
- [ ] When the project has unsaved changes, the window title (from T-013) ends with ` *`.
- [ ] Every undoable action and Import Sheet mark the project as changed. Saving marks it as saved.
- [ ] Undoing or redoing back to the state that was last saved marks the project as saved again.
- [ ] When there are unsaved changes, these actions show a prompt first: closing the window, File > Exit,
  File > New, File > Load and File > Open Recent.
- [ ] The prompt has Save, Don't Save and Cancel. Save runs Save, or Save As if the project has no path, and the
  action continues only if saving succeeds. Don't Save continues without saving. Cancel returns to the editor.
- [ ] If the project cannot be saved (it has no image), Save is disabled in the prompt.
- [ ] Closing the window is intercepted in `SpriteApplication`, without changes to moth.

**Out of scope:**
Changes to Preferences. They are editor settings, not project data.

**Open questions:**
- Q: Where is the indicator shown (window title, menu bar, a window)? A: In the window title.
- Q: Does "close" mean closing the app window, File > Exit, or both? A: Both. They raise the same
  `EventRequestQuit`. `Application::OnEvent` is virtual, so `SpriteApplication` can hold the request back.
- Q: Should loading another project also show the prompt? A: Yes, for File > Load and File > Open Recent.
- Q: Which choices does the prompt give (e.g. Save, Discard, Cancel)? A: Save, Don't Save, Cancel.
- Q: If the user undoes back to the saved state, is the project still marked as unsaved? A: No. Undoing or
  redoing back to the saved state marks it as saved again.

**Notes:**

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.
