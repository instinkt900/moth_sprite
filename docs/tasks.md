# Tasks

The task list for moth_sprite. The process, the status values and the field definitions are in the
shared workflow, `/home/mcotton/Development/moth/.claude/tasks/workflow.md`. Project-specific build and
launch details are in [task-profile.md](task-profile.md).

- `/task-new <description>` adds a task.
- `/task-planning [IDs]` reviews tasks.
- `/task-session` implements reviewed tasks.

Session reports are in [sessions/](sessions/). `/task-planning` moves finished tasks to
[tasks-done.md](tasks-done.md).

## Tasks

### [done] T-020 Rename Tools > Grid to Grid Cells

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
Rename the Tools > Grid option to "Grid Cells".

**Requirements:**
- [x] The Tools menu item reads "Grid Cells...".
- [x] The dialog it opens has the title "Grid Cells" (now "Grid Tool").

**Out of scope:**
- Code names (`DrawGridTool`, `GridToolState`, `m_gridTool`) and the popup's `##` ID part.
- The tool's behaviour and layout.

**Open questions:**
- Q: Does the dialog title change too? A: Yes, the menu item and the dialog title. Code names stay.

**Notes:**
- The popup's visible label changed from "Grid Tool" to "Grid Cells". Its `##tool_grid` part is kept. The popup uses
  `NoSavedSettings`, so no `imgui.ini` entry depends on the label.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.

**Commits:**
- fb6cf59 feat(T-020): rename Tools > Grid to Grid Cells

**Manual verification:**
1. Import a sheet image. Open the Tools menu. The first item reads "Grid Cells...".
2. Choose it. The dialog title reads "Grid Cells", and the tool works as before.

### [done] T-021 Rename Tools > Detect Frames to Detect Cells

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
Rename Tools > Detect Frames to "Detect Cells".

**Requirements:**
- [x] The Tools menu item reads "Detect Cells...".
- [x] The dialog it opens has the title "Detect Cells" (now "Detect Frames").

**Out of scope:**
- Code names (`DrawDetectFramesTool`, `DetectToolState`, `frame_detection.*`) and the popup's `##` ID part.
- The tool's behaviour and layout.

**Open questions:**
- Q: Does the dialog title change too? A: Yes, the menu item and the dialog title. Code names stay.

**Notes:**
- The popup's visible label changed from "Detect Frames" to "Detect Cells". Its `##tool_detect` part is kept. The
  popup uses `NoSavedSettings`.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.

**Commits:**
- 6d462cf feat(T-021): rename Tools > Detect Frames to Detect Cells

**Manual verification:**
1. Import a sheet image. Open the Tools menu. The second item reads "Detect Cells...".
2. Choose it. The dialog title reads "Detect Cells", and the tool works as before.

### [done] T-022 Move Preferences under Edit

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
Move the "Preferences" menu to under "Edit" so it becomes Edit > Preferences.

**Requirements:**
- [x] Edit > Preferences is a submenu at the end of the Edit menu, after a separator below Pivot.
- [x] The submenu has the same controls, in the same order, as the Preferences menu has now: the three border
  colors, the border thickness and the preview background color.
- [x] The top-level Preferences menu is removed. The menu bar is File, Edit, Tools, Window.
- [x] The settings and `moth_sprite.json` do not change.

**Out of scope:**
- A Preferences window or dialog.
- New settings, or changes to how the settings work or are saved.

**Open questions:**
- Q: A submenu with the controls, or an item that opens a window? A: A submenu at the end of Edit.

**Notes:**
- The Preferences block moved unchanged into the Edit menu, after a new separator below the Pivot submenu. The
  Preferences submenu is always enabled.
- `moth_sprite.json` and `SpriteEditorConfig` did not change.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.

**Commits:**
- 832eb97 feat(T-022): move Preferences under the Edit menu

**Manual verification:**
1. The menu bar reads File, Edit, Tools, Window. There is no Preferences menu.
2. Open Edit. Below Pivot there is a separator, then Preferences. Preferences is enabled with no cell selected.
3. Open Edit > Preferences. It shows Normal border, Selected border, Prime border, Border thickness, a separator,
   and Preview background. Change the border thickness and a color. The Sheet window uses them.
4. Quit and start the app again. The changed settings are kept.

### [done] T-019 Browse button on the sprite sheet path

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
Add a button to the sprite sheet path entry so that the user can load a different sprite sheet image.

**Requirements:**
- [x] The Sheet window's Image row has a "..." button to the right of the read-only path field. The path field
  still fills the rest of the row.
- [x] The button does the same as File > Import Sheet: an image dialog with the same filter, starting in
  `LastImageDir` (or the current folder), and remembering the chosen folder. The chosen image replaces the sheet
  image, and the cells and clips are kept.
- [x] The result is the same as Import Sheet in every other way: the project is marked as having unsaved changes,
  the undo stack is cleared, the selection and clip playback are reset, and the zoom fits the new image.
- [x] The button and the menu item share one code path, so they cannot differ.
- [x] Cancelling the dialog, or choosing an image that fails to load, changes nothing (as Import Sheet).

**Out of scope:**
- Making Import Sheet undoable. It stays outside the undo stack, as now.
- An Image row when no sheet image is loaded. The Sheet window still shows its "Use File > Import Sheet" hint.
- Export Sheet, and the project file format.

**Open questions:**
- Q: What happens to the existing cells and clips when a different image is loaded? A: They are kept, as Import
  Sheet does.
- Q: Does the button open a file dialog, and where does it start? A: It does exactly what File > Import Sheet
  does, including the dialog folder.

**Notes:**
- The dialog code of File > Import Sheet moved, unchanged, into `ImportSheetWithDialog()`. The menu item and the new
  "..." button both call it, so they share one code path.
- The button is to the right of the path field, which now fills the row minus the button's width. It has the
  tooltip "Import a different sheet image (File > Import Sheet)".
- After the button runs an import, `DrawPreview` returns for that frame. An import replaces `m_spriteSheet`, and the
  image reference used by the rest of `DrawPreview` would otherwise point at the old sheet.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.
- Changed after the session, at the user's request, in e9b9838. The Requirements above are kept as reviewed, so they
  still say that Import Sheet clears the undo stack.
  - Import Sheet, from the menu or the "..." button, is one undoable action. Undo gives back the previous sheet image
    and path, and Redo the imported one. Both sides keep their sheet (and texture) alive on the undo stack. The undo
    history is no longer cleared, and `m_unsavedOutsideUndo` is removed, because no change is outside the undo stack
    now. The import still clears the selection and resets clip playback. Undo and Redo re-fit the zoom.

**Commits:**
- f1744fc feat(T-019): browse button on the Sheet window's image path
- e9b9838 feat: Import Sheet is undoable (after the session)

**Manual verification:**
1. Start with no sheet image. The Sheet window shows the "Use File > Import Sheet" hint and no Image row.
2. Import a sheet with File > Import Sheet, add two cells and a clip with steps. The Sheet window's Image row shows
   the path, with a "..." button at its right end. Resize the window. The field fills the row up to the button.
3. Click "...". The image dialog opens in the folder of the last image dialog. Cancel. Nothing changes.
4. Click "..." and choose a different image. The Sheet shows the new image, fitted to the window. The cells and the
   clip are kept, the selection is cleared, and the title gets " *". Edit > Undo gives back the previous image, with
   the path and the title's " *" as before the import. Edit > Redo shows the imported image again. Do the same with
   File > Import Sheet. Earlier edits can still be undone after an import.
5. Click "..." again. The dialog starts in the folder of the image chosen in step 4. Cancel.
6. Choose a file that is not a valid image (for example, a renamed text file with a .png extension). The log shows
   "failed to load image", and the sheet does not change.

### [done] T-017 New clip and +step use all selected cells

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
When creating a new clip, or pressing the +step button, with multiple cells selected (more than 1), all the
selected cells get inserted to the new clip. With only one cell selected, the new clip button still creates an
empty clip and the +step button still adds the single selected cell.

**Requirements:**
- [x] With more than one cell selected, "+ Clip" creates a clip with one step for each selected cell.
- [x] With more than one cell selected, "+ Step" adds one step for each selected cell at the end of that clip's
  timeline.
- [x] Steps are added in selection order: the order in which the cells were added to the selection
  (`m_selection`), so the prime cell's step is last.
- [x] With zero or one cell selected, "+ Clip" still creates an empty clip.
- [x] With one cell selected, "+ Step" still adds one step for that cell. With no cell selected it still adds a
  step for cell 0, as now.
- [x] Each step added by "+ Step" gets the duration in that clip's Set all box (the value it shows, 100 until it
  is changed), for one cell and for several. This replaces "the last step's duration, or 100".
- [x] Each step of a new clip gets 100 ms, the Set all box's starting value.
- [x] "+ Clip" with an empty name creates the clip with the name `clip_N`, where N is the lowest number from 1 up
  that no other clip uses as `clip_N`. This is true with any selection. A typed name is used as now.
- [x] Each "+ Clip" or "+ Step" click is one undo step, however many steps it adds.
- [x] A new clip is selected, as now.

**Out of scope:**
- The Set all button and box, other than reading the box's value.
- How the selection is made or ordered.
- Unique names for typed clip names. Only auto-names avoid clashes.
- The project file format.

**Open questions:**
- Q: In what order are the steps added when several cells are selected? A: Selection order.
- Q: What duration does each added step get? A: The value in the clip's Set all box. A new clip has no box yet,
  so its steps get 100, the box's starting value. A single-cell "+ Step" also uses the box.
- Q: "+ Clip" needs a typed name. What happens with no name? A: The clip is auto-named `clip_N`, with any
  selection.

**Notes:**
- "+ Clip" with more than one cell selected adds a step for each selected cell, in `m_selection` order, with 100 ms
  each (`kDefaultStepDurationMs`, also the Set all box's starting value).
- "+ Step" reads the clip's Set all value (clamped to at least 0, as Set all does) in the frame of the click, and
  adds a step for each selected cell, or the prime cell (or cell 0) as before.
- "+ Clip" no longer needs a name. An empty name becomes `clip_N`, the lowest N from 1 that no clip has as its name.
- Assumption: the "No clips" hint said to enter a name first. It now reads "No clips. Click + Clip to add one.",
  because a name is no longer needed.
- Each click still adds one `PushClipAction`. Selection and playback handling did not change.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.
- Changed after the session, at the user's request, in d9fd5d1. The Requirements above are kept as reviewed, so they
  still say that "+ Step" always uses the Set all value.
  - "+ Step" gives each added step the clip's last step's duration. Only a clip with no steps uses its Set all value.
    A new clip's steps still get 100 ms, the Set all box's starting value.

**Commits:**
- 8cc8bdd feat(T-017): + Clip and + Step add a step for each selected cell
- d9fd5d1 feat: + Step uses the last step's duration, or Set all for an empty clip (after the session)

**Manual verification:**
1. Import a sheet and add at least four cells. With no clip name typed and no cell selected, click "+ Clip". A clip
   named `clip_1` is created with no steps, and is selected. Click "+ Clip" again: `clip_2`.
2. Rename `clip_1` to `walk`. Click "+ Clip" with an empty name. The new clip is `clip_1`.
3. Select one cell and click "+ Clip". The clip has no steps.
4. Click cell 3, then Ctrl+click cell 1, then Ctrl+click cell 2. Type `run` and click "+ Clip". The clip `run` has
   three steps, for cells 3, 1, 2 in that order, each 100 ms. Edit > Undo removes the whole clip in one step.
5. Set `run`'s last step to 150 ms. In its header, set the Set all box to 250 (without clicking Set all). Select
   cells 0 and 2 (Ctrl+click) and click `run`'s "+ Step". Two steps are added at the end, for cells 0 and 2, each
   150 ms (the last step's duration). One Undo removes both.
6. Select only cell 1 and click "+ Step". One step for cell 1, 150 ms.
7. Clear the selection (Esc) and click "+ Step". One step for cell 0, with the last step's duration.
8. On `clip_2`, which has no steps, set the Set all box to 300 and click "+ Step". The step gets 300 ms.

### [done] T-018 Cell list with thumbnails

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
The cell list should change from a text only list to a list with thumbnails of each cell on the left, and on the
right a listing of the cell index, cell offset and cell size. Keep the x button for deletion.

**Requirements:**
- [x] Each row of the Cells list has a 48 x 48 px thumbnail box on the left.
- [x] The box shows the preview background (`DrawImageBackground`, with 8 px checker squares) and the cell's image,
  scaled to fit the box, keeping its aspect ratio, and centered. Parts of a cell outside the sheet image are
  clamped, as in the Clips thumbnails. With no sheet image, the box shows only the background.
- [x] To the right of the box are two lines: line 1 is the index, as `#3`; line 2 is the offset and size, as
  `(x, y)  w x h`.
- [x] The x button that deletes the cell stays at the right end of the row.
- [x] Clicking anywhere on the row (box or text) selects as now: click, Ctrl+click, Shift+click, and picking a cell
  for a clip step. The selected and prime highlights cover the whole row.
- [x] The Cells form below the list still fits, and the list still scrolls.

**Out of scope:**
- The Cells form, the selection rules and the delete behaviour.
- Thumbnails anywhere else.

**Open questions:**
- Q: How large is the thumbnail, and what happens to cells that are not square? A: A 48 px box; the cell is fitted
  inside, keeping its aspect ratio.
- Q: How are the index, offset and size laid out and labelled in the row? A: Two lines: `#3`, then
  `(x, y)  w x h`.

**Notes:**
- Each row is one 48 px high `Selectable` (label `##cell_row`) over the whole row, so the click handling did not
  change. The thumbnail and the two text lines are drawn over it: the background with `DrawImageBackground` (8 px
  squares), the image with `DrawImage`, and the text with the window draw list in the style's text color.
- The selected and prime highlights fill the whole row behind the thumbnail. The thumbnail's background covers the
  highlight inside its 48 px box.
- The x button is centred on the row's height. After the thumbnail, the cursor goes back to where it was after the
  x button, so the rows lay out as before. This ImGui build (1.90.4) does not define
  `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, so moving the cursor this way does not assert.
- Assumption: text that is wider than the row (a narrow window, or large numbers) is drawn under the x button, and
  is clipped by the list. It is not shortened.
- The Cells form's height estimate did not change. The list keeps its minimum height of three frame heights, which is
  now less than one row, and it scrolls.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed (it has no cells, so the rows were not drawn).

**Commits:**
- da59349 feat(T-018): thumbnails in the Cells list

**Manual verification:**
1. Import a sheet image with transparent areas, and add cells of different shapes: one wide, one tall, one square,
   and one that extends past the right edge of the image.
2. Each Cells row has a 48 px box on the left with an 8 px checkerboard. The cell's image is fitted in it, centred,
   keeping its aspect ratio. The cell that extends past the image shows only the part inside the image, stretched as
   in the Clips thumbnails.
3. To the right of the box, line 1 is `#0`, `#1`, and so on, and line 2 is `(x, y)  w x h`, matching the Cells form.
4. Click the thumbnail, then the text, of different rows. Each selects its cell. Ctrl+click and Shift+click work as
   before. The prime row has the prime color across the whole row.
5. The x button is at the right end of each row, vertically centred, and deletes that cell.
6. Double-click a clip step, then click a row. The cell is picked for the step.
7. Add many cells (Tools > Grid Cells). The list scrolls, and the Cells form below it is still fully visible.
8. File > New. The list is empty.

### [done] T-023 Help menu with About dialog

**Review:** reviewed 2026-09-16

**Depends on:** T-022

**Goal:**
Add a "Help" menu with one option "About" for now. It should open a small dialog with the tool name, its version,
a short description of what it is for, and the author (eventually the GitHub repo too when we have a remote).

**Requirements:**
- [x] The menu bar has a Help menu, last: File, Edit, Tools, Window, Help.
- [x] The Help menu has one item, "About...".
- [x] About opens a modal "About Moth Sprite" dialog, centered, sized to its content, with a Close button. Esc also
  closes it.
- [x] The dialog shows: "Moth Sprite"; "Version 0.1.0", from `version.txt`; the description from the CMake
  `project(... DESCRIPTION ...)`; "Author: Matthew Cotton"; and `https://github.com/instinkt900/moth_sprite` as
  text.
- [x] The version and the description come from CMake at build time (for example, compile definitions from
  `MOTH_SPRITE_VERSION_FULL` and `PROJECT_DESCRIPTION`), so changing `version.txt` changes the dialog. They are
  not typed into the source.
- [x] Keyboard shortcuts do not fire while the dialog is open, as with the other modal popups.

**Out of scope:**
- A clickable link, or opening a browser.
- Other Help items (documentation, shortcuts list).
- Changes to how the version is set in `version.txt` or `conanfile.py`.

**Open questions:**
- Q: Where does the version come from, and what is it for the first release? A: `version.txt`, now 0.1.0, passed
  in by CMake.
- Q: What text is used for the description and the author? A: The CMake project description; "Matthew Cotton".
- Q: Is the GitHub repo link part of this task? A: Yes. The remote exists: `github.com/instinkt900/moth_sprite`.

**Notes:**
- `CMakeLists.txt` passes `MOTH_SPRITE_VERSION_STRING` (from `MOTH_SPRITE_VERSION_FULL`, the stripped contents of
  `version.txt`, including any `-` or `+` suffix) and `MOTH_SPRITE_DESCRIPTION` (`PROJECT_DESCRIPTION`) as compile
  definitions. `version.txt` is added to `CMAKE_CONFIGURE_DEPENDS`, so a change to it runs CMake again and the dialog
  gets the new version on the next build. The definitions apply to every source file, so a version change rebuilds
  all of them.
- The dialog is a modal popup like the unsaved changes prompt: centred, sized to its content, `NoSavedSettings`. It is
  opened with a flag set by the menu item, and drawn at the end of `Draw()`.
- The popup title is "About Moth Sprite". Close and Esc close it. `HandleShortcuts` already does nothing while a modal
  popup is open.
- Build and clang-tidy: no findings, no NOLINT. Smoke launch passed.

**Commits:**
- a5ba938 feat(T-023): Help > About dialog

**Manual verification:**
1. The menu bar reads File, Edit, Tools, Window, Help. Help has one item, "About...".
2. Choose Help > About. A dialog titled "About Moth Sprite" opens in the middle of the window. It shows "Moth Sprite",
   "Version 0.1.0", "A sprite sheet and animation clip editor for moth", "Author: Matthew Cotton" and
   `https://github.com/instinkt900/moth_sprite`.
3. Click Close. The dialog closes. Open it again and press Esc. It closes.
4. With the dialog open, press Ctrl+N, Ctrl+Z and Delete. Nothing happens.
5. Optional: change `version.txt` to `0.1.1`, build, and open About. It shows 0.1.1. Change it back.

### [done] T-024 Add a README.md

**Review:** reviewed 2026-09-16

**Depends on:** T-017, T-018, T-019, T-020, T-021, T-022, T-023

**Goal:**
Add a `README.md`.

**Requirements:**
- [x] The repository root has a `README.md`, for users of the tool, in the style of moth_packer's README
  (`~/Development/moth/moth_packer/README.md`), with a table of contents.
- [x] It covers: what the tool is and what it is for (a sprite sheet and clip editor whose projects moth_graphics
  loads as a `SpriteSheet`); features; usage: the windows (Sheet, Selected Cell, Cells, Clips), making cells
  (New Cell, Tools > Grid Cells, Tools > Detect Cells), selection, pivots, clips and playback, undo, and the
  keyboard shortcuts; the project file format (`image`, `frames`, `clips`, as in `CLAUDE.md`); editor settings
  (`moth_sprite.json` and `imgui.ini` in the current folder); building with Conan and CMake; related moth
  projects; and the license (MIT, as in `conanfile.py`).
- [x] Everything it says matches the app at the commit that adds it: menu names, window names, shortcuts and
  file fields. Check each against the source.
- [x] No screenshots and no badges for CI that does not exist.

**Out of scope:**
- A `LICENSE` file, CI, and changes to `CLAUDE.md` or `docs/`.
- Developer workflow docs beyond a pointer to `docs/workflow.md`.

**Open questions:**
- Q: What does the README cover? A: What the tool is, features, usage, project file format, building, related
  projects, license. No screenshots.
- Q: Who is it for? A: Users of the tool, with build steps.

**Notes:**
- Each statement was checked against the source at this commit: menu items, window names, toolbar buttons, the Grid
  Cells and Detect Cells options and their "Add Frames" button, the selection rules in `sprite_editor_preview.cpp`
  and `sprite_editor_frames.cpp`, the clip controls, `HandleShortcuts`, `SpriteEditorConfig`, `SaveSpriteSheet` and
  moth_graphics' `SpriteSheetFactory` (for what a game loads).
- Assumption: the Conan remote is given as in moth_packer's README
  (`https://artifactory.matthewcotton.net/artifactory/api/conan/conan-local`, named `moth`). The local Conan setup
  names this remote `artifactory` and uses `http://`.
- Assumption: the related projects table links moth_ui, moth_graphics, moth_editor and moth_packer on GitHub, as
  moth_packer's README does. moth_core and moth_bridge are named in the prerequisites without links, because their
  URLs were not found.
- The license is given as MIT, from `conanfile.py`. The repository has no `LICENSE` file (out of scope).
- The README says that moth_graphics does not load a project with no cells, and skips empty clips and steps with a
  0 ms duration. The editor allows all three. Recorded under `## Discovered`.
- No code changed. Build and smoke launch passed.

**Commits:**
- 0cbdfbb docs(T-024): add README.md

**Manual verification:**
1. Read `README.md` on GitHub, or in a Markdown viewer. The table of contents links work, and the tables and code
   blocks render.
2. Check the Artifactory remote URL and name, and the related project links, against what you publish.
3. Follow "Build and run" in a fresh clone.

### [deferred] T-025 Import cells from off-sheet images

**Review:** unreviewed

**Depends on:** none

**Goal:**
Add support for importing cells from off-sheet images. The cell list window should get a new "import" button so
the user can import an image to use as a new cell.

**Requirements:**
- [ ] The cell list window has an "import" button.
- [ ] The button lets the user choose an image that is not the sprite sheet, and adds it as a new cell.

**Open questions:**
- Q: Where do the imported pixels live: composited into the sheet image, or kept as a separate image?
- Q: How is such a cell saved in the project file, which stores cells as a rectangle in the sheet image?
- Q: Can moth_graphics load a sprite sheet whose cells come from more than one image?
- Q: Where is the imported cell placed in the sheet, and what happens if there is no room?

**Notes:**
- Deferred on 2026-09-16 by `/task-planning`, to finish the smaller tasks first. The open questions above are
  still open.
- moth_graphics (`SpriteSheetFactory`) reads one `image` and each frame as a rectangle in it. A cell from another
  image needs a new project file field that moth_graphics would ignore, so such a cell would draw the wrong pixels
  in game until the sheet is re-packed (T-026, T-027). Decide the file format before this task is reviewed.

**Commits:**

**Manual verification:**

### [deferred] T-026 Re-pack the sprite sheet

**Review:** unreviewed

**Depends on:** T-025

**Goal:**
Add support for re-packing the sprite sheet (sheet cells and external cells). This will mirror features from
moth_packer (`~/Development/moth/moth_packer`) and can probably use its library. A dialog should pop up allowing
the user to specify the packing parameters with a preview.

**Requirements:**
- [ ] The sprite sheet can be re-packed, covering both sheet cells and external cells imported by T-025.
- [ ] A dialog lets the user specify the packing parameters.
- [ ] The dialog shows a preview of the packing result.

**Open questions:**
- Q: Which moth_packer features are mirrored, and which packing parameters does the dialog expose?
- Q: Is moth_packer's library available as a Conan package or another dependency this project can use?
- Q: Where does the re-packed image get written, and what happens to the original sheet image?
- Q: How is the re-pack undone: one undo step for the whole operation?

**Notes:**
- Deferred on 2026-09-16 by `/task-planning`, to finish the smaller tasks first. The open questions above are
  still open.
- The packer library is now part of the moth toolkit, and links like the other modules. moth_editor uses it:
  `self.requires("moth_packer/[>=2 <3]")` in `conanfile.py`, and `find_package(moth_packer REQUIRED)` with
  `target_link_libraries(... moth::packer)` in `CMakeLists.txt`. moth_editor also sets
  `self.options["moth_packer"].with_ui = True`, which re-packing sheet cells probably does not need. The local
  `~/Development/moth/moth_packer` checkout (1.0.0, moth_ui 1.x) is out of date. Read the 2.x API, not that one.
- Adding the dependency is a change to `conanfile.py` and `CMakeLists.txt`, and needs a new `conan install`. When
  this task is reviewed, write the dependency into `Requirements`, so that a session does not block on it.

**Commits:**

**Manual verification:**

### [deferred] T-027 Notice when saving a project with external cells

**Review:** unreviewed

**Depends on:** T-025

**Goal:**
When saving a project with external cells, pop up a notice dialog that notifies the user that external cells are
only supported by the tool and will not work in game until the sheet is repacked.

**Requirements:**
- [ ] Saving a project that has external cells (from T-025) opens a notice dialog.
- [ ] The notice says that external cells are only supported by the tool and will not work in game until the sheet
  is repacked.
- [ ] Saving a project with no external cells does not show the notice.

**Open questions:**
- Q: Does the notice appear before or after the file is written, and can the user cancel the save from it?
- Q: Does it appear on every save, or is there a way to stop it showing again?

**Notes:**
- Deferred on 2026-09-16 by `/task-planning`, to finish the smaller tasks first. The open questions above are
  still open.
- Depends on the external cells design in T-025.

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.

- Found in T-015: the Cells form and the Clips window commit a pending field edit when the field's widget reports
  the end of the edit. If the widget is not drawn in that frame (the edited cell or clip is deleted by a button in
  the same frame, or the window is closed), the edit stays pending until the next field is activated. Its undo step
  then also covers changes made in between.
- Found in T-024: the editor can save projects that moth_graphics' `SpriteSheetFactory` does not load as saved. A
  project with no cells fails to load ("frames array is empty"), in the editor too. A clip with no steps, and a clip
  with a step whose duration is 0 ms (the duration fields allow 0), are skipped with a warning, so they are lost
  when the project is loaded again.
