# Tasks

The task list for moth_sprite. The process, the status values and the field definitions are in
[workflow.md](workflow.md).

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

### [todo] T-019 Browse button on the sprite sheet path

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
Add a button to the sprite sheet path entry so that the user can load a different sprite sheet image.

**Requirements:**
- [ ] The Sheet window's Image row has a "..." button to the right of the read-only path field. The path field
  still fills the rest of the row.
- [ ] The button does the same as File > Import Sheet: an image dialog with the same filter, starting in
  `LastImageDir` (or the current folder), and remembering the chosen folder. The chosen image replaces the sheet
  image, and the cells and clips are kept.
- [ ] The result is the same as Import Sheet in every other way: the project is marked as having unsaved changes,
  the undo stack is cleared, the selection and clip playback are reset, and the zoom fits the new image.
- [ ] The button and the menu item share one code path, so they cannot differ.
- [ ] Cancelling the dialog, or choosing an image that fails to load, changes nothing (as Import Sheet).

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

**Commits:**

**Manual verification:**

### [todo] T-017 New clip and +step use all selected cells

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
When creating a new clip, or pressing the +step button, with multiple cells selected (more than 1), all the
selected cells get inserted to the new clip. With only one cell selected, the new clip button still creates an
empty clip and the +step button still adds the single selected cell.

**Requirements:**
- [ ] With more than one cell selected, "+ Clip" creates a clip with one step for each selected cell.
- [ ] With more than one cell selected, "+ Step" adds one step for each selected cell at the end of that clip's
  timeline.
- [ ] Steps are added in selection order: the order in which the cells were added to the selection
  (`m_selection`), so the prime cell's step is last.
- [ ] With zero or one cell selected, "+ Clip" still creates an empty clip.
- [ ] With one cell selected, "+ Step" still adds one step for that cell. With no cell selected it still adds a
  step for cell 0, as now.
- [ ] Each step added by "+ Step" gets the duration in that clip's Set all box (the value it shows, 100 until it
  is changed), for one cell and for several. This replaces "the last step's duration, or 100".
- [ ] Each step of a new clip gets 100 ms, the Set all box's starting value.
- [ ] "+ Clip" with an empty name creates the clip with the name `clip_N`, where N is the lowest number from 1 up
  that no other clip uses as `clip_N`. This is true with any selection. A typed name is used as now.
- [ ] Each "+ Clip" or "+ Step" click is one undo step, however many steps it adds.
- [ ] A new clip is selected, as now.

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

**Commits:**

**Manual verification:**

### [todo] T-018 Cell list with thumbnails

**Review:** reviewed 2026-09-16

**Depends on:** none

**Goal:**
The cell list should change from a text only list to a list with thumbnails of each cell on the left, and on the
right a listing of the cell index, cell offset and cell size. Keep the x button for deletion.

**Requirements:**
- [ ] Each row of the Cells list has a 48 x 48 px thumbnail box on the left.
- [ ] The box shows the preview background (`DrawImageBackground`, with 8 px checker squares) and the cell's image,
  scaled to fit the box, keeping its aspect ratio, and centered. Parts of a cell outside the sheet image are
  clamped, as in the Clips thumbnails. With no sheet image, the box shows only the background.
- [ ] To the right of the box are two lines: line 1 is the index, as `#3`; line 2 is the offset and size, as
  `(x, y)  w x h`.
- [ ] The x button that deletes the cell stays at the right end of the row.
- [ ] Clicking anywhere on the row (box or text) selects as now: click, Ctrl+click, Shift+click, and picking a cell
  for a clip step. The selected and prime highlights cover the whole row.
- [ ] The Cells form below the list still fits, and the list still scrolls.

**Out of scope:**
- The Cells form, the selection rules and the delete behaviour.
- Thumbnails anywhere else.

**Open questions:**
- Q: How large is the thumbnail, and what happens to cells that are not square? A: A 48 px box; the cell is fitted
  inside, keeping its aspect ratio.
- Q: How are the index, offset and size laid out and labelled in the row? A: Two lines: `#3`, then
  `(x, y)  w x h`.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-023 Help menu with About dialog

**Review:** reviewed 2026-09-16

**Depends on:** T-022

**Goal:**
Add a "Help" menu with one option "About" for now. It should open a small dialog with the tool name, its version,
a short description of what it is for, and the author (eventually the GitHub repo too when we have a remote).

**Requirements:**
- [ ] The menu bar has a Help menu, last: File, Edit, Tools, Window, Help.
- [ ] The Help menu has one item, "About...".
- [ ] About opens a modal "About Moth Sprite" dialog, centered, sized to its content, with a Close button. Esc also
  closes it.
- [ ] The dialog shows: "Moth Sprite"; "Version 0.1.0", from `version.txt`; the description from the CMake
  `project(... DESCRIPTION ...)`; "Author: Matthew Cotton"; and `https://github.com/instinkt900/moth_sprite` as
  text.
- [ ] The version and the description come from CMake at build time (for example, compile definitions from
  `MOTH_SPRITE_VERSION_FULL` and `PROJECT_DESCRIPTION`), so changing `version.txt` changes the dialog. They are
  not typed into the source.
- [ ] Keyboard shortcuts do not fire while the dialog is open, as with the other modal popups.

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

**Commits:**

**Manual verification:**

### [todo] T-024 Add a README.md

**Review:** reviewed 2026-09-16

**Depends on:** T-017, T-018, T-019, T-020, T-021, T-022, T-023

**Goal:**
Add a `README.md`.

**Requirements:**
- [ ] The repository root has a `README.md`, for users of the tool, in the style of moth_packer's README
  (`~/Development/moth/moth_packer/README.md`), with a table of contents.
- [ ] It covers: what the tool is and what it is for (a sprite sheet and clip editor whose projects moth_graphics
  loads as a `SpriteSheet`); features; usage: the windows (Sheet, Selected Cell, Cells, Clips), making cells
  (New Cell, Tools > Grid Cells, Tools > Detect Cells), selection, pivots, clips and playback, undo, and the
  keyboard shortcuts; the project file format (`image`, `frames`, `clips`, as in `CLAUDE.md`); editor settings
  (`moth_sprite.json` and `imgui.ini` in the current folder); building with Conan and CMake; related moth
  projects; and the license (MIT, as in `conanfile.py`).
- [ ] Everything it says matches the app at the commit that adds it: menu names, window names, shortcuts and
  file fields. Check each against the source.
- [ ] No screenshots and no badges for CI that does not exist.

**Out of scope:**
- A `LICENSE` file, CI, and changes to `CLAUDE.md` or `docs/`.
- Developer workflow docs beyond a pointer to `docs/workflow.md`.

**Open questions:**
- Q: What does the README cover? A: What the tool is, features, usage, project file format, building, related
  projects, license. No screenshots.
- Q: Who is it for? A: Users of the tool, with build steps.

**Notes:**
- Depends on the UI tasks so that it describes the final menus and windows.

**Commits:**

**Manual verification:**

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
