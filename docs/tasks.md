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

### [done] T-009 Selected cell window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable window showing the selected cell. Same zoom options and pivot rendering. This replaces the existing
cell preview and keeps the click to set pivot behaviour.

**Requirements:**
- [x] A dockable window shows the selected cell.
- [x] The window has the same zoom options as the sheet window (Fit, 1:1, mouse-wheel zoom) and draws the pivot.
- [x] Clicking or dragging in the window sets the pivot, as the existing cell preview does.
- [x] The 3×3 pivot preset buttons (TL, T, TR, L, C, R, BL, B, BR) are in the window, below the cell image.
- [x] The window replaces the existing cell preview.
- [x] The window is part of the default layout from T-007.
- [x] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: Where does the 3×3 pivot preset grid go? A: In the selected cell window, below the cell image. T-003 adds
  the same presets to Edit > Pivot for the whole selection.

**Notes:**
- The window is named "Selected Cell". Its open state is `ShowCellWindow` in `moth_sprite.json`.
- The default layout splits the right side: Selected Cell on top (40%), Sprite Editor below.
- The frames pane no longer has the mini-preview column or the pivot preset grid. The X, Y, W, H and Pivot
  fields stay there until T-010.
- The cell is drawn with one zoom for both axes. The old mini-preview stretched each axis on its own to fit
  its column.
- The mouse-wheel zoom moved from `DrawPreview` into `SpriteEditor::ZoomWithMouseWheel`, unchanged, so the
  Sheet and Selected Cell windows share it. T-012 can use it too.
- Assumption: the cell zoom fits the window on first draw, and again after File > New, Load and Import Sheet,
  like the sheet zoom. Selecting another cell keeps the current zoom, so that the user can compare pivots at
  one zoom. Fit refits it.
- Assumption: with no image or no selected cell, the window shows a short hint.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- 182b68a feat(T-009): dockable Selected Cell window with zoom and pivot editing

**Manual verification:**
1. Start with no `imgui.ini`, or choose Window > Reset Layout. Sheet is on the left, Selected Cell is top right
   and Sprite Editor is bottom right.
2. With no sheet, Selected Cell shows the Import Sheet hint. Import a sheet: it shows "Select a cell ...".
3. Add cells with Tools > Grid and select one. The cell fills the window (Fit) with a pivot cross. The frames
   pane has no preview and no preset buttons.
4. Click 1:1, then Fit, then use the mouse wheel over the cell. The zoom changes around the cursor, and
   scrollbars appear when the cell is larger than the window.
5. Click on the cell: the pivot moves there. Drag: the pivot follows the mouse. The Pivot X and Y fields in the
   Sprite Editor window and the cross on the sheet match. Ctrl+Z undoes the whole drag in one step.
6. Click each of the nine preset buttons below the cell. The pivot moves to that corner, edge or center. Each
   click is one undo step.
7. Untick Window > Selected Cell, quit and start again. It is still closed. Tick it: it opens.

### [done] T-010 Cell list window

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
A dockable window with the current list of defined cells. Rather than a table it should be a scrollable list,
and clicking one will select the cell, update the preview, highlight the cell on the sheet and update a form
below the list showing an editable set of entries with x/y/w/h values etc. for the cell.

**Requirements:**
- [x] A dockable window shows the defined cells as a scrollable list, not a table.
- [x] Each entry shows the cell index and rect (for example `3   (32, 0) 32×32`) and a delete button.
- [x] Clicking a cell selects it, updates the preview and highlights it on the sheet.
- [x] A form below the list has editable X, Y, W, H, Pivot X and Pivot Y values for the selected cell.
- [x] The window is part of the default layout from T-007.
- [x] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**
The New Cell button. It moves to the sheet window (T-008).

**Open questions:**
- Q: What does each list entry show? A: The cell index and rect.
- Q: Does each entry keep its delete button? A: Yes. The Delete key also works.

**Notes:**
- The window is named "Cells". Its open state is `ShowCellListWindow` in `moth_sprite.json`.
- The default layout splits the right side into three: Selected Cell on top (40%), then Cells, then
  Sprite Editor.
- The frames table and the cell form moved out of the Sprite Editor window, which now holds only the path box
  and the clips. The form title says "Cell N" instead of "Frame N".
- Assumption: the rect uses a plain `x` (`3   (32, 0) 32x32`), not `×`. The requirement gives the text only as
  an example, and the font the app loads may not have the `×` glyph.
- Assumption: clicking the selected cell again clears the selection, as the old frames table did. T-002
  changes click behaviour for multiple selection.
- The list does not scroll to a cell selected on the sheet. The task does not ask for it.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- a4f9e2d feat(T-010): dockable Cells window with a cell list and form

**Manual verification:**
1. Start with no `imgui.ini`, or choose Window > Reset Layout. The right side has Selected Cell, Cells and
   Sprite Editor, from top to bottom. The Sprite Editor window has no frames table.
2. Import a sheet and add cells with Tools > Grid. The Cells window shows "Cells: N" and one row per cell, such
   as `3   (32, 0) 32x32`. With many cells the list scrolls, and the form stays visible below it.
3. Click a row. It is highlighted, the cell is highlighted on the sheet, Selected Cell shows it, and the form
   shows "Cell N" with its values. Click the same row again: the selection clears and the form shows
   "Select a cell to edit it."
4. Select a cell on the sheet. The matching row is highlighted.
5. Change X, Y, W, H, Pivot X and Pivot Y in the form. The sheet and Selected Cell follow. Ctrl+Z undoes each
   field edit in one step.
6. Click the `x` on a row. That cell is removed, and clip steps are fixed up as before. Ctrl+Z restores it.
   Select a cell and press Delete: it is removed.
7. Untick Window > Cells, quit and start again. It is still closed. Tick it: it opens.

### [done] T-011 Clip editor window

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
- [x] A dockable window where the user can create and remove clips.
- [x] Clips are stacked vertically, each shown as a horizontal timeline of frames.
- [x] Each frame on the timeline shows a thumbnail of its cell, with its duration (ms) editable below it.
- [x] Clicking a clip selects it.
- [x] Each clip can be renamed, and its loop type (Stop, Reset, Loop) changed.
- [x] Steps can be added ("+ Step" adds the selected cell) and removed.
- [x] Steps can be reordered by dragging them along the timeline.
- [x] Single click on a frame selects its cell and moves playback to that frame.
- [x] Double click on a frame starts picking a new cell for it. The frame is highlighted while picking. The next
  cell selected through any "select cell" behaviour (cell list or sheet) is assigned to the frame and picking
  ends. Esc cancels picking.
- [x] A play/pause button plays the selected clip. Pause stops on the current step, and play continues from
  there. The timeline scrolls to keep the current step visible.
- [x] A Step button advances one step.
- [x] The window is part of the default layout from T-007.
- [x] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: Which features of today's clip pane stay? A: All of them: rename, loop type, add and remove steps, and
  the Step button. Reordering steps by dragging is new.
- Q: What does each timeline frame show? A: A thumbnail of the cell, with its duration below it.
- Q: How does picking a new cell end? A: The next cell selected is assigned, or Esc cancels.
- Q: How does play/pause behave? A: Pause keeps the current step, and play continues from it. Today, Play
  always restarts from the first step.

**Notes:**
- The window is named "Clips". Its open state is `ShowClipEditorWindow` in `moth_sprite.json`. The default
  layout docks it below the Sheet window (35% of the left side).
- The clip list, the new-clip row and the Play/Step controls moved out of the Sprite Editor window. The
  pivot-anchored clip preview stays there (`DrawClipPreview`) until T-012 moves it, so no feature is lost.
- Each clip is a bordered block: name field, loop type, step count and an `X` remove button, then the
  timeline. Each step is a 72 px thumbnail with its cell index in the corner, a duration field below it and an
  `x` remove button. "+ Step" is at the end of each timeline. The selected clip's block uses the header colour.
  The current step has the selected-border colour, and the step that is picking a cell has an orange outline.
- Playback moved into `AdvanceClipPlayback`, called once per frame from `Draw()`. Before, it only ran while
  the clips pane was drawn.
- `SelectCell` is now the "select cell" behaviour for the cell list and the sheet. While picking, it assigns
  the cell to the step as one clip undo action and ends picking. While picking, a click on the sheet only picks
  a cell and never starts a move or resize, and a click on empty sheet space does nothing. Clicks on timeline
  thumbnails select the cell without assigning it.
- Picking also ends on undo, redo, New, Load, Import, and when a step is moved or removed or a clip is removed,
  because the picked step's index may no longer name the same step.
- Step reordering is drag and drop within one clip. Dropping on a step moves the dragged step to that index.
  Dropping on another clip does nothing.
- Undo for the name and duration fields uses a pending edit keyed by the widget's ImGui ID
  (`TrackClipEdit`/`CommitClipEdit`), which replaces `m_pendingClipSnapshot`. With the old shared snapshot,
  moving focus straight from one field to another could lose an undo step. The same problem in the cell form
  is logged under Discovered.
- Assumption: a single click on a step pauses playback on that step, as clicking a step row did before.
- Assumption: Play on a Stop clip that has played to its end (on the last step, with no time spent on it)
  starts again from the first step. Otherwise Play would do nothing. Pausing partway through the last step and
  pressing Play continues from there.
- Assumption: clicking the clip that is already selected does not reset playback. Selecting a different clip
  moves playback to its first step, as before.
- The timeline scrolls only while playing and after Step, and only when the current step is not fully visible,
  so the user can scroll freely while paused.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- 05ff209 feat(T-011): dockable Clips window with step timelines

**Manual verification:**
1. Start with no `imgui.ini`, or choose Window > Reset Layout. The Clips window is docked below the Sheet
   window. The Sprite Editor window shows the path box and "Select a clip in the Clips window to preview it."
2. Import a sheet and add cells with Tools > Grid. In Clips, type a name and click "+ Clip". A clip block
   appears and is selected. Add a second clip. The blocks are stacked. Click in the first block's empty area:
   it becomes selected. Click `X` on a block: that clip is removed. Ctrl+Z restores it.
3. Rename a clip, then change its loop type. Each change is one Ctrl+Z step.
4. Select a cell and click "+ Step" several times, with different cells selected. Each step shows the cell's
   thumbnail and index, and a duration below it. Edit a duration. Click `x` under a step: it is removed.
   Each of these is one Ctrl+Z step.
5. Click a step. Its cell is selected on the sheet, in Cells and in Selected Cell. The step gets the current
   outline, the Sprite Editor preview shows that step, and the Step counter matches.
6. Drag a step onto another step of the same clip. It moves to that position. Ctrl+Z restores the order.
7. Double click a step. It gets an orange outline and a hint appears. Click a different cell on the sheet. The
   step shows the new cell and the outline ends. Double click a step again and click a row in Cells: it is
   assigned. Double click again and press Esc: picking ends and nothing changes. Ctrl+Z undoes an assignment in
   one step.
8. While picking, click the selected cell on the sheet. It is assigned, and the cell does not move.
9. Add more steps than fit in the window width. Click Play. Playback runs, the outline moves along the
   timeline, and the timeline scrolls to keep it visible. Click Pause: it stops on the current step. Click
   Play: it continues from that step, not from the first. Click Step: it advances one step and pauses.
10. With a Stop clip, let it play to the end, then click Play again. It plays from the first step.
11. Close the Clips window while a clip plays. The Sprite Editor preview keeps animating.
12. Untick Window > Clips, quit and start again. It is still closed. Tick it: it opens.

### [done] T-012 Clip preview window

**Review:** reviewed 2026-09-15

**Depends on:** T-007, T-009, T-011

**Goal:**
A dockable clip preview window. Almost a duplicate of the selected cell window, but without any pivot or any
other guides or markup that might come with the cell window. This should show the clip preview with play/pause
buttons that mirror the ones on the clip window. The idea of this window is to be a dedicated "what does the
animation look like" preview.

**Requirements:**
- [x] A dockable window shows the selected clip's animation.
- [x] Each frame is anchored on its cell's pivot, as in today's clip preview, so the animation does not jump.
- [x] The window shows no pivot, guides or markup.
- [x] The window has the same zoom options as the selected cell window (Fit, 1:1, mouse-wheel zoom).
- [x] Play/pause buttons mirror the ones in the clip editor window: both control the same playback, with the
  same behaviour.
- [x] The window is part of the default layout from T-007.
- [x] The Window menu has a toggle for this window, and its open or closed state is remembered across runs.

**Out of scope:**

**Open questions:**
- Q: What zoom does the preview have? A: The same as the selected cell window.

**Notes:**
- The window is named "Clip Preview". Its open state is `ShowClipPreviewWindow` in `moth_sprite.json`. The
  default layout splits the Clips area: Clips on the left, Clip Preview on the right (30%).
- The pivot-anchored preview moved out of the Sprite Editor window, which now holds only the project path box.
  T-013 removes that window.
- The playback controls are the same `DrawClipPlaybackControls` as in the Clips window: Play/Pause, Step and
  the step counter. Both windows drive the same playback state.
- The anchoring is unchanged: a bounding box over all of the clip's steps in pivot space, with each step's
  pivot on one anchor. The pivot crosshair and the dark background fill of the old preview are removed.
- Zoom works like the Selected Cell window: it fits on first draw and after File > New, Load and Import Sheet.
  Fit, 1:1 and the shared `ZoomWithMouseWheel` helper are available. The canvas scrolls when the clip is
  larger than the window.
- Assumption: the animation is centred in the canvas when it is smaller than the canvas. A `Dummy` of the full
  bounding box keeps the scroll range the same from step to step.
- Assumption: the zoom is kept when another clip is selected, as the Selected Cell zoom is kept when another
  cell is selected. Fit refits it.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- 6582467 feat(T-012): dockable Clip Preview window

**Manual verification:**
1. Start with no `imgui.ini`, or choose Window > Reset Layout. Clips and Clip Preview are side by side below the
   Sheet window. The Sprite Editor window shows only the path box.
2. With no clip selected, Clip Preview shows Play and Step disabled and "Select a clip in the Clips window to
   preview it."
3. Make a clip with several steps whose cells have different sizes and pivots (for example, set different pivots
   in Selected Cell). Select it. The preview fits the window and shows no crosshair, border or background.
4. Click Play in Clip Preview. The animation plays, the cells stay anchored on their pivots, and the Clips
   window's button reads Pause and its outline moves. Click Pause in the Clips window: the preview stops on the
   same step. Click Play in Clip Preview: it continues from that step. Click Step in either window: both show the
   same step.
5. Click 1:1, then Fit, and use the mouse wheel over the preview. The zoom changes around the cursor. At a
   large zoom, scrollbars appear and do not jump while the clip plays.
6. Untick Window > Clip Preview, quit and start again. It is still closed. Tick it: it opens.

### [done] T-013 Remove the old UI window

**Review:** reviewed 2026-09-15

**Depends on:** T-008, T-009, T-010, T-011, T-012

**Goal:**
Once all of the new windows are added, remove the single window that holds the old UI.

**Requirements:**
- [x] The single window from T-007 is removed.
- [x] Its entry is removed from the Window menu and from the default layout.
- [x] The project path box from the old UI is replaced by the window title: `Moth Sprite - <project file name>`,
  or `Moth Sprite - Untitled` for a project that has not been saved.
- [x] No other existing functionality is lost.

**Out of scope:**
Other windows, such as tools, stay as they are.

**Open questions:**

**Notes:**
- The "Sprite Editor" window, its Window menu entry, its slot in the default layout and `ShowSpriteEditorWindow`
  are removed. A `moth_sprite.json` that still has the key loads; the key is ignored and is not written again.
- The default layout's right side is now Selected Cell (top, 40%) and Cells (below).
- The path box is replaced by the window title. `SpriteApplication` passes the editor a callback that calls
  `UiWindow::SetWindowTitle`. The editor builds the title from the project path each frame and sets it only when
  it changes: `Moth Sprite - <file name with extension>`, or `Moth Sprite - Untitled` when the project has no
  path (on start, after File > New, or after Import Sheet into a new project). Save As changes the title to the
  new file name.
- The smoke launch finds the window by the name "Moth Sprite", which still matches the new titles.
- An `imgui.ini` saved while the Sprite Editor window existed can keep an empty dock area where it was.
  Window > Reset Layout removes it.
- Found a problem outside this task: a failed File > Load still changes the project path, so the title names the
  file that failed. Logged under Discovered.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- f493150 feat(T-013): remove the Sprite Editor window and show the project in the title

**Manual verification:**
1. Start with no `imgui.ini`, or choose Window > Reset Layout. There is no Sprite Editor window. The windows are
   Sheet with Clips and Clip Preview below it on the left, and Selected Cell above Cells on the right. The Window
   menu lists Sheet, Selected Cell, Cells, Clips, Clip Preview, then Reset Layout.
2. The title bar reads "Moth Sprite - Untitled".
3. Import a sheet, add cells and a clip, then File > Save As `hero.json`. The title changes to
   "Moth Sprite - hero.json".
4. File > New: the title is "Moth Sprite - Untitled" again. File > Load `hero.json`: the title is
   "Moth Sprite - hero.json", and the cells and clips are back.
5. Check that everything else still works across the windows: import, sheet zoom and cell drag, New Cell, the
   cell list and form, pivot editing and presets, clip editing and playback, Tools > Grid and Detect Frames,
   Preferences, Undo and Redo, Save.

### [done] T-002 Multiple selection

**Review:** reviewed 2026-09-15

**Depends on:** T-013

**Goal:**
The user should be able to select multiple cells by holding ctrl while clicking cells. The selection will be
used to perform actions that can be applied to multiple cells: deleting, modifying pivot, etc. There will also
be a "prime" selected concept. For actions that can only use one cell, the "prime" selection will be used,
e.g. assigning to a clip frame.

**Requirements:**
- [x] Ctrl+click, on the sheet or in the cell list, adds or removes a cell from the selection. A plain click
  selects only the clicked cell.
- [x] The selection has a "prime" cell: the cell most recently clicked or ctrl+clicked into the selection.
- [x] If the prime cell is removed from the selection, the most recently added cell still selected becomes prime.
- [x] The prime cell is drawn in a different colour from the other selected cells, on the sheet and in the list.
- [x] Actions that take a single cell use the prime cell: the selected cell window, the cell form, "+ Step" and
  picking a cell for a clip frame.
- [x] Delete removes every selected cell, as one undo action.
- [x] Dragging inside a selected cell on the sheet moves every selected cell together, as one undo action. Resize
  handles appear only on the prime cell.
- [x] Shift+click in the cell list selects every cell between the prime cell and the clicked cell.
- [x] Ctrl+A selects every cell.
- [x] Esc clears the selection, when Esc is not already cancelling something (New Cell drawing, picking a cell).
- [x] Dragging on an empty part of the sheet draws a box, and selects every cell inside it.
- [x] Undo and redo restore the whole selection, as they restore the single selection today.

**Out of scope:**
Pivot changes for the whole selection. They are in T-003.

**Open questions:**
- Q: Which cell is prime? A: The most recently clicked or ctrl+clicked cell.
- Q: What becomes prime when the prime cell is deselected? A: The most recently added cell still selected.
- Q: What does dragging do with several cells selected? A: Moves all of them. Only the prime cell can be resized.
- Q: Which extra selection features are included? A: Shift+click range in the list, Ctrl+A, Esc to clear, and
  box select on the sheet.

**Notes:**
- `m_selectedFrame` is replaced by `m_selection`, the selected cell indices in the order they were added. The
  last one is the prime cell (`PrimeCell()`). Removing a cell from the vector leaves the most recently added
  remaining cell last, so it becomes prime.
- `PushFrameAction` and `PushFrameClipAction` now take the selection before and after, so undo and redo restore
  the whole selection. `DeleteFrame` became `DeleteFrames`. It deletes from the highest index down and applies
  the old per-cell fix-up of the selection and clip steps to each cell, in one `PushFrameClipAction`. The list's
  delete button calls it with one cell.
- Prime colour: a new editor setting `SpriteEditorPrimeColor` (default magenta), with Preferences > Prime border.
  On the sheet the prime cell's rect and pivot use it. In the Cells list the prime row's highlight uses it at
  reduced alpha, and the other selected rows use the normal highlight. The Cells header shows "(N selected)"
  when more than one cell is selected.
- Sheet mouse rules, in order:
  - While picking a cell for a clip step, a click only picks a cell.
  - Ctrl+click toggles the cell under the mouse. Ctrl+drag on empty space adds a box to the selection.
  - A plain press on the prime cell's edge or corner resizes the prime cell only.
  - A plain press inside any selected cell makes that cell prime and moves every selected cell. The move delta
    is clamped once for the whole selection, so the cells keep their layout at the sheet's edges. If the press
    does not drag, only that cell stays selected.
  - A plain press on an unselected cell selects only that cell and moves it, as before.
  - A plain press on empty space starts a box. On release, the selection is the cells fully inside the box, so a
    click on empty space clears the selection, as before.
- Cells list: a plain click selects only that cell. This replaces the T-010 behaviour where clicking the
  selected cell cleared the selection. Ctrl+click toggles. Shift+click selects from the prime cell to the clicked
  cell. While picking a cell for a clip step, every click is a plain click.
- Esc now cancels one thing per press: picking, else New Cell drawing, else the selection. It does not clear
  the selection during a drag or box select.
- Assumption: Shift+click replaces the selection with the range and keeps the prime cell as prime (the range
  anchor). With no prime cell, Shift+click selects only the clicked cell.
- Assumption: box select selects cells that are fully inside the box, not cells it only touches. The box's
  cells are added in index order, so the highest-index cell becomes prime.
- Assumption: Ctrl+A keeps the prime cell prime. With no prime cell, the last cell becomes prime.
- The pivot presets in Selected Cell still change only the prime cell. Applying them to the whole selection is
  T-003.
- `IsMouseDragPastThreshold` comes from `imgui_internal.h`, now included in `sprite_editor_preview.cpp`.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- 8209f95 feat(T-002): multiple cell selection with a prime cell

**Manual verification:**
1. Import a sheet and add a grid of cells (Tools > Grid). Click a cell on the sheet: only it is selected, in the
   prime colour (magenta by default), and it is the highlighted row in Cells.
2. Ctrl+click two more cells on the sheet. All three are selected. The last one clicked is magenta, the others
   use the selected colour, and Cells shows "(3 selected)" with the last row in the prime colour. Selected Cell
   and the form show the last cell.
3. Ctrl+click the prime cell: it is removed, and the cell added before it becomes prime. Ctrl+click it again: it
   is added back as prime.
4. In Cells, click a row, then Shift+click a row further down. Every row between them is selected, and the first
   row stays prime. Ctrl+click a row in the list: it is toggled.
5. Press Ctrl+A: every cell is selected. Press Esc: the selection clears. Click New Cell, press Esc: New Cell
   ends and the selection is kept. Double click a clip step, press Esc: picking ends and the selection is kept.
6. Drag on an empty part of the sheet: a box is drawn, and on release the cells fully inside it are selected.
   Hold Ctrl and drag another box: those cells are added. Click on empty space: the selection clears.
7. Select several cells, then drag inside one of them. All selected cells move together and stop together at the
   sheet's edge. Ctrl+Z undoes the whole move in one step, and the selection comes back as it was. Ctrl+Y redoes it.
8. With several cells selected, the resize cursor appears only on the prime cell's edges. Drag an edge: only the
   prime cell resizes.
9. With several cells selected, click (without dragging) inside a selected cell that is not prime. Only that cell
   stays selected.
10. Select several cells and press Delete. All of them are removed, and clip steps are fixed up. One Ctrl+Z
    restores all of them, with the same selection.
11. With a clip selected, click "+ Step": it adds the prime cell. Double click a step and Ctrl+click a cell on the
    sheet: it is assigned to the step, and only that cell is selected.
12. Change Preferences > Prime border. The sheet and the Cells list use the new colour. Restart: it is kept.

### [done] T-003 Pivot helpers

**Review:** reviewed 2026-09-15

**Depends on:** T-002

**Goal:**
Under the Edit menu, there should be options for setting pivots by a rule. They apply to the whole selection
and are grouped in the menu under their own "Pivot" entry.

**Requirements:**
- [x] Edit > Pivot submenu with nine options: Top Left, Top Center, Top Right, Center Left, Center,
  Center Right, Bottom Left, Bottom Center, Bottom Right.
- [x] Each option sets the pivot of every selected cell, relative to that cell's own size, as one undo action.
- [x] Pivot positions match today's 3×3 preset grid: center is half the width or height, rounded down, and
  right and bottom are the full width and height.
- [x] The 3×3 pivot preset grid in the selected cell window also applies to the whole selection.

**Out of scope:**

**Open questions:**
- Q: Does the 3×3 grid in the selected cell window also apply to the whole selection? A: Yes, so there is one
  rule for pivots.

**Notes:**
- One function, `SetSelectionPivot(PivotAnchor x, PivotAnchor y)`, does the work for both the menu and the grid.
  `PivotAnchor` is `Start` (0), `Center` (size / 2, rounded down) or `End` (full size), per axis, which are the
  positions the old grid used.
- Edit > Pivot comes after Undo and Redo, with separators between the top, center and bottom rows. It is disabled
  when no cell is selected.
- Assumption: when a rule changes no pivot (every selected cell already has that pivot), no undo action is added.
  Before, a grid button always added one, even when nothing changed.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- e92f822 feat(T-003): Edit > Pivot rules for the whole selection

**Manual verification:**
1. With no cell selected, Edit > Pivot is disabled.
2. Import a sheet and add cells of different sizes (for example, Tools > Grid, then resize some cells on the sheet).
   Select several with Ctrl+click.
3. Choose Edit > Pivot > Bottom Center. Each selected cell's pivot cross moves to the middle of its own bottom
   edge (x = width / 2 rounded down, y = height). Unselected cells do not change.
4. Press Ctrl+Z once: every selected pivot returns. Ctrl+Y: they move again.
5. Try each of the nine options and check the positions: left/top is 0, center is half the size rounded down,
   right/bottom is the full size. For an odd width such as 15, center is 7.
6. In Selected Cell, click the TL and C buttons with several cells selected. Every selected cell changes, not only
   the prime cell. Each click is one undo step.

### [done] T-004 Export sheet option

**Review:** reviewed 2026-09-15

**Depends on:** T-008

**Goal:**
Under File there should be an option to export the sprite sheet. This behaves like a "save as" for the sprite
sheet image. When the sheet is exported, the project's reference to the sheet file is updated to the new
location, so the next project save writes the new path. This is so you can import a sheet from one location,
export it to a new location, and be sure the project references the new location rather than the old.

**Requirements:**
- [x] File > Export Sheet copies the sheet image file, unchanged and in the same format, to a location chosen by
  the user. The save dialog keeps the original extension.
- [x] Export Sheet is disabled when the project has no image path.
- [x] After export, the project's sheet image path points to the exported file.
- [x] Saving the project afterwards writes the new sheet image path.
- [x] The image path change is one undo action. Undo points the project back at the old image path. The exported
  file stays on disk.
- [x] The sheet window shows the sheet image path as read-only text.

**Out of scope:**
A file selector for changing the image path from the sheet window. It may be added later.

**Open questions:**
- Q: How is the image written? A: The original file is copied. The editor never changes the image's pixels.
- Q: Is the path change undoable? A: Yes. So that the user can see which image the project uses, the sheet
  window shows the image path as read-only text.

**Notes:**
- File > Export Sheet... sits after Import Sheet. It is enabled only when the project has an image path.
- The save dialog's filter is the sheet's own extension, and it opens in the sheet's folder. The GTK dialog adds
  the filter's extension when the typed name has none. `ExportSheet` also forces the sheet's extension, replacing
  a different extension if the user typed one, so the copy always keeps the format.
- The file is copied with `std::filesystem::copy_file`, overwriting an existing file (the GTK dialog asks before
  overwriting). The pixels are never decoded or re-encoded. If the copy fails, an error is logged and the
  project's path does not change.
- Exporting onto the sheet file itself does nothing: no copy, and no undo action.
- The path change is a `BasicAction` that sets `m_imagePathBuffer` to the new or the old path. The loaded texture
  does not change, because the copy has the same pixels. `SaveSpriteSheet` already writes `image` from that
  buffer, relative to the project file.
- The Sheet window shows "Image" and a read-only text field with the path above its toolbar, when a sheet image
  is loaded. The fit zoom leaves room for the row.
- Assumption: the dialog starts in the sheet's folder. T-005 changes image dialogs to start in the last folder
  an image dialog used.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- c3ee89f feat(T-004): File > Export Sheet copies the sheet image and repoints the project

**Manual verification:**
1. File > New. File > Export Sheet is disabled.
2. File > Import Sheet a PNG from folder A. The Sheet window shows "Image" with the path to that file. Export
   Sheet is enabled.
3. File > Export Sheet. The dialog shows only `*.png` files. Go to folder B and type `copy` with no extension.
   `B/copy.png` is created, and it is byte-identical to the original (`cmp A/sheet.png B/copy.png`). The Sheet
   window's Image path now shows `B/copy.png`.
4. Add a cell, then File > Save As `B/project.json`. The file's `image` is `copy.png`.
5. Press Ctrl+Z until the Image path shows the folder A file again (the export is one undo step), then File > Save.
   `image` points at the folder A file. `B/copy.png` is still on disk. Ctrl+Y: the path is `B/copy.png` again.
6. Export again and type `copy2.jpg`. The file is written as `copy2.png`.
7. Load a project whose image is a JPG. Export Sheet filters on `*.jpg`, and the copy is a `.jpg`.

### [done] T-005 Recent files

**Review:** reviewed 2026-09-15

**Depends on:** T-007

**Goal:**
There should be a short history of recently opened files. Additionally, the load/save dialogs should remember
their last location across runs.

**Requirements:**
- [x] File > Open Recent lists the 10 most recently loaded or saved projects, newest first.
- [x] Choosing an entry loads that project. If the file no longer exists, the entry is removed and a warning is
  logged.
- [x] File > Open Recent > Clear Recent empties the list.
- [x] The project dialogs (Load, Save As) open in the folder last used by a project dialog.
- [x] The image dialogs (Import Sheet, Export Sheet) open in the folder last used by an image dialog.
- [x] The recent list and both folders are saved in `moth_sprite.json`, so they are remembered after a restart.

**Out of scope:**

**Open questions:**
- Q: How do recent files work? A: A File > Open Recent submenu with the 10 most recent projects and Clear Recent.
- Q: Do the dialogs share one remembered folder? A: No. Project dialogs and image dialogs each remember their own.

**Notes:**
- New editor settings in `moth_sprite.json`: `RecentProjects` (paths, newest first), `LastProjectDir` and
  `LastImageDir`. A settings file without them loads with an empty list and no remembered folders.
- `AddRecentProject` runs after a successful load (`LoadSpriteSheet`) and after a successful write
  (`SaveSpriteSheet`, so both Save and Save As). It stores the absolute, normalised path, removes an earlier copy
  of the same path, puts it first and keeps 10 entries.
- File > Open Recent comes after Load. It is disabled while the list is empty. Each entry shows the full path,
  then there is a separator and Clear Recent. The chosen entry is opened after the submenu is drawn, from a
  copy of the path, because opening changes the list.
- `OpenRecentProject` checks that the file exists. If not, it logs
  `[warning] SpriteEditor: recent project '<path>' no longer exists, removed it from Open Recent` and removes the
  entry. Otherwise it loads the project like File > Load: it sets the project path, then loads. A project that
  exists but fails to load keeps the behaviour logged under Discovered for T-013.
- Folders: a dialog starts in its remembered folder if that folder still exists. Otherwise Load, Save As and
  Import Sheet start in the current directory, as before, and Export Sheet starts in the sheet's folder, as in
  T-004. Choosing a file (not cancelling) remembers that file's folder for its kind of dialog. Open Recent does
  not change the remembered folders, because it is not a dialog.
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed.

**Commits:**
- 68a7df6 feat(T-005): File > Open Recent and remembered dialog folders

**Manual verification:**
1. Start with a `moth_sprite.json` from before this change. File > Open Recent is disabled.
2. Import a sheet from folder `imgA`, add a cell, and Save As `projA/one.json`. Open Recent lists
   `.../projA/one.json`.
3. File > New, then Save As again from a new import: the Save As dialog opens in `projA`. Save as
   `projB/two.json`. Open Recent lists `two.json` first, then `one.json`.
4. File > Import Sheet: the dialog opens in `imgA`. Pick a sheet from `imgB`. File > Export Sheet: the dialog opens
   in `imgB`. File > Load: the dialog opens in `projB` (the project folder is separate from the image folder).
5. Choose `one.json` from Open Recent. It loads, the title shows `one.json`, and it moves to the top of the list.
6. Save or load more than 10 different projects. The list keeps the 10 newest.
7. Quit and start again. Open Recent has the same entries, and Load and Import open in the remembered folders.
8. Delete or rename `two.json` on disk, then choose it from Open Recent. Nothing loads, the log has a `[warning]`
   line naming the path, and the entry is gone from the list.
9. Delete the remembered folder `imgB`. File > Import Sheet opens in the current directory instead.
10. File > Open Recent > Clear Recent. The list is empty and Open Recent is disabled. Restart: still empty.

### [done] T-006 Unsaved changes indicator and prompt

**Review:** reviewed 2026-09-15

**Depends on:** T-005, T-013

**Goal:**
Add an unsaved edits indicator, and a prompt on closing or starting a new project that warns of unsaved changes.

**Requirements:**
- [x] When the project has unsaved changes, the window title (from T-013) ends with ` *`.
- [x] Every undoable action and Import Sheet mark the project as changed. Saving marks it as saved.
- [x] Undoing or redoing back to the state that was last saved marks the project as saved again.
- [x] When there are unsaved changes, these actions show a prompt first: closing the window, File > Exit,
  File > New, File > Load and File > Open Recent.
- [x] The prompt has Save, Don't Save and Cancel. Save runs Save, or Save As if the project has no path, and the
  action continues only if saving succeeds. Don't Save continues without saving. Cancel returns to the editor.
- [x] If the project cannot be saved (it has no image), Save is disabled in the prompt.
- [x] Closing the window is intercepted in `SpriteApplication`, without changes to moth.

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
- Unsaved state: `AddSpriteAction` gives each action an id from a counter that is never reset (`m_undoIds`, next to
  `m_undoStack`). `CurrentUndoId()` is the id of the newest applied action, or 0. `MarkSaved()` records it.
  `HasUnsavedChanges()` is true when the current id differs from the recorded one, or when
  `m_unsavedOutsideUndo` is set. Undo or redo back to the saved position matches again. A new action after undoing
  past the saved position never matches, because its id is new. Every change to project data already goes
  through `AddSpriteAction`, including the T-004 image path change.
- `MarkSaved()` runs in `NewSpriteSheet` (so the app starts clean), after a successful `LoadSpriteSheet`, and after
  a successful write in `SaveSpriteSheet`. Import Sheet clears the undo stack and sets `m_unsavedOutsideUndo`.
- `SaveSpriteSheet` now returns whether the file was written. It also flushes the stream and checks it, so a
  failed write does not count as saved.
- The title from T-013 ends with ` *` while `HasUnsavedChanges()` is true.
- Prompt flow: New, Load and Open Recent call `RequestProjectAction`. Without unsaved changes it runs the action at
  once. With unsaved changes it keeps the action and opens the "Unsaved Changes" modal. File > Load shows the
  prompt before the file dialog. The prompt names the project file (or "Untitled"). Save calls `SaveProject()`,
  which runs Save, or Save As when there is no path. It is disabled, with a note, when the project has no sheet
  image. If saving fails or the Save As dialog is cancelled, the prompt stays open. Don't Save runs the action.
  Cancel, or Esc, closes the prompt and drops the action.
- Quit: closing the window and File > Exit both send `EventRequestQuit`. `SpriteApplication` now overrides
  `OnEvent`. For that event it calls `SpriteEditor::HoldQuitForUnsavedChanges()`. With unsaved changes the editor
  opens the prompt and the event is consumed, so the app keeps running. After Save or Don't Save, the editor sets
  `m_quitApproved` and fires `EventRequestQuit` again, which `SpriteApplication` passes to
  `Application::OnEvent`. moth is not changed. `SpriteApplication` keeps a raw pointer to the editor layer and
  clears it in `Shutdown`.
- Opening the prompt while a Tools popup (Grid, Detect Frames) is open closes that popup, because ImGui opens the
  prompt at the top popup level.
- Assumption: an edit in a text or number field that has not been committed yet (the field is still active) is
  not an unsaved change, because it is not on the undo stack until the field is left.
- Preferences changes do not mark the project as changed (out of scope).
- No NOLINT added. Build and clang-tidy clean. Smoke launch passed (the smoke launch closes a clean project, so it
  also checks that no prompt appears then).

**Commits:**
- 912ee84 feat(T-006): unsaved changes indicator and prompt

**Manual verification:**
1. Start the app. The title is "Moth Sprite - Untitled" with no ` *`. Close the window: the app quits at once.
2. File > Import Sheet. The title ends with ` *`. Click the close button: the "Unsaved Changes" prompt appears and
   the app stays open. Click Cancel: the prompt closes and nothing changes.
3. Choose File > Exit: the prompt appears. Click Don't Save: the app quits.
4. Start again and import a sheet, add a cell, then File > Save As `a.json`. The title is "Moth Sprite - a.json"
   with no ` *`.
5. Move the cell: ` *` appears. Ctrl+Z: ` *` goes away. Ctrl+Y: it comes back. Ctrl+Z, then add a different cell:
   ` *` stays, even after undoing and redoing to the same number of steps.
6. With ` *` showing, choose File > New: the prompt appears. Click Save: `a.json` is written, the project is
   replaced by a new one, and the title is "Moth Sprite - Untitled".
7. Import a sheet and add a cell (so the project has no path and has changes). Choose File > Load: the prompt
   appears before the file dialog. Click Save: the Save As dialog opens. Cancel it: the prompt is still open.
   Click Save again and choose `b.json`: it saves, then the Load file dialog opens.
8. Make a change, then choose an entry in File > Open Recent: the prompt appears. Don't Save loads the entry.
9. File > New, import a sheet, add a cell, then press Ctrl+Z until nothing is left to undo. The title still ends
   with ` *`, because Import Sheet is a change that is not on the undo stack. Close the window: the prompt appears.
10. To see Save disabled: load a project file with no `image` field, make a change (for example, add a clip), and
    close the window. Save is disabled, with the note that the project has no sheet image.
11. Make a change, close the window, and click Save for a project with a path. The file is written, and the app
    quits.

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
