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

### [done] T-039 Open a project whose sheet image is missing

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
When opening a project or a previously exported descriptor, if the sheet image cannot be opened the project fails
to open completely. It should open with a blank sheet, still with the cells and clips. It should be able to be
saved, but exporting should fail without a valid sheet.

Project files (`.mothsprite`) already behave this way: `LoadProjectFile` warns and keeps the image path (T-028).
The failure is the descriptor import. `ImportDescriptor` uses `SpriteSheetFactory::GetSpriteSheet`, which returns
nothing when the image cannot be loaded, so the whole open is refused.

**Requirements:**
- [x] Opening an exported descriptor (`.json`) whose image cannot be loaded opens it as a new project, with the
      cells, the clips and the image path from the file, and no sheet image. It is no longer refused.
- [x] The editor reads descriptors with its own code, not with `SpriteSheetFactory`, so it decides what is fatal.
      It still refuses a file it cannot parse, one with no `image` string field, and one with no frames.
- [x] An image that cannot be loaded is a logged warning. No dialog and no banner is added.
- [x] Such a project can be saved, and saving keeps the sheet image path. Save on an imported descriptor still
      opens Save As, as it does now.
- [x] Opening a `.mothsprite` project whose sheet image cannot be loaded keeps working as it does today, with the
      cells, clips, pivots and image path kept.
- [x] File > Export refuses, and writes nothing, when the project has a sheet image path whose image could not be
      loaded. The export message says so, as it does for a project with no sheet image at all.

**Out of scope:**
- moth_graphics. `SpriteSheetFactory` is not changed, and neither is the descriptor format.
- An export that packs first. The pack makes the sheet image, so it does not need the current one, as today.
- Editing cells without a sheet image, beyond what already works. The Sheet window keeps its existing empty
  state.
- Cells from other images. A cell whose image is missing already loads as an empty cell.

**Open questions:**
- Q: `SpriteSheetFactory` refuses the descriptor, and it is in another repository. How does the editor get around
  that? A: The editor reads descriptors with its own code, as T-028 made it do for project files. moth_graphics is
  not changed.
- Q: How does the editor say that the sheet image could not be loaded? A: A logged warning only.

**Notes:**
`ImportDescriptor` now reads the file with `ReadDescriptorFile`, beside `ReadProjectFile`, and the cell and clip
reading the two share is in `ReadFramesAndClips`. A descriptor is therefore read as it was written: clips with no
steps, 0 ms steps and out-of-range step indices are kept, where `SpriteSheetFactory` skipped them. That matches
project files, and `ExportProblems` still refuses to export such data.

Assumption: the export message names the image path it could not load ("The project's sheet image '<path>' could
not be loaded."), rather than repeating the wording of "The project has no sheet image."

The descriptor import no longer flushes the sprite sheet factory cache, because it no longer uses the factory.

**Commits:**
- `9062609` fix(T-039): open a descriptor whose sheet image is missing

**Manual verification:**
1. Export a project to a descriptor, then rename or delete the image file beside it. File > Open the descriptor:
   it opens with its cells and clips, the Sheet window shows its empty state, and the log has a warning naming the
   image. No dialog appears.
2. With that project open, File > Save opens Save As. Save it, reopen the saved `.mothsprite`: the cells, clips,
   pivots and the sheet image path are kept, and the log warns about the image again.
3. With that project open, File > Export: the export message says the sheet image could not be loaded, and no
   files are written.
4. File > Open a `.json` file that is not JSON, one with no `image` field, and one with an empty `frames` array:
   each is refused with a logged error, and the project that was open is unchanged.
5. File > Open a descriptor whose image is present: it opens as before.

### [done] T-038 Only write the sheet image when exporting packs

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
When exporting the spritesheet descriptor it should only output the spritesheet image when it packs. Otherwise it
just writes the descriptor with the path to the existing sheet image.

**Requirements:**
- [x] File > Export writes a sprite sheet image only when it packs, which is when the project has cells from
      other images. Today it always copies the sheet image beside the descriptor.
- [x] When the export does not pack, it writes only the descriptor. The descriptor's `image` field is the path
      from the descriptor's folder to the existing sheet image, and an absolute path when there is no relative
      one.

**Out of scope:**
- The export that follows a pack. The pack writes the packed image beside the descriptor and names it after it,
  as it does now, and the descriptor keeps naming that file.
- `SpriteSheetFactory` in moth_graphics. It already resolves `image` against the descriptor's folder, and takes
  an absolute path.

**Open questions:**
- Q: What decides whether an export packs? A: An export packs only when the project has external images (cells
  with a source image).
- Q: What does the descriptor's `image` field hold when the export does not pack? A: The path relative to the
  descriptor, falling back to an absolute path when there is no relative one.

**Notes:**
`ExportToPath` no longer copies the sheet image, and writes the descriptor's `image` field with
`ProjectRelativePath`, the helper project files already use for their paths. The export that packs first needed no
change: the pack writes its image beside the descriptor and sets it as the project's sheet image, so the same
relative path is that file's name.

The export message no longer has a case for an image that could not be copied, because no copy is made.

**Commits:**
- `7062297` feat(T-038): export writes the sheet image only when it packs

**Manual verification:**
1. Open a project whose sheet image is not beside the descriptor's folder, File > Export As to a new folder: only
   the `.json` is written, and its `image` field is a relative path (`../sheets/hero.png`) back to the image.
   Loading it in a game finds the image.
2. Export into the folder that holds the sheet image: the `image` field is just the image's file name, and the
   image is not rewritten (its timestamp does not change).
3. Export a project with cells from other images: the pack dialog opens, and after Pack the packed image is
   written beside the descriptor and named after it, as before.
### [todo] T-036 Export the sprite sheet to a new location

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
Add an "export" option along side "import" that exports the sprite sheet to a new location. Currently the only way
to achieve this behaviour is to repack the sheet. The option sits beside each import option, both in the menu and
as a button in the window.

**Requirements:**
- [ ] An export option sits beside the sheet image import, both as `Edit > Export Spritesheet...` and as a button
      in the Sheet window beside its Import button.
- [ ] The sheet image options are named "Import Spritesheet" and "Export Spritesheet", to tell them apart from
      exporting the project (File > Export).
- [ ] Export writes the sprite sheet image to a location chosen in a save dialog, and writes no JSON.
- [ ] After a successful export the project's sheet image path is the exported file, as one undoable action, so
      the project is marked as unsaved.

**Out of scope:**
- The Cells window's Import button gets no Export beside it. Writing cells out as their own images is T-032.
- File > Export and File > Export As, which write the descriptor, keep their names and behaviour.

**Open questions:**
- Q: Does the export sit beside every import, including the Cells window's Import? A: No, the sheet image import
  only.
- Q: Does exporting the sheet image change the project? A: Yes. The project's sheet image path becomes the
  exported file, as one undoable action, which marks the project as unsaved.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-037 Preview image filtering option

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
Add an option to change the filtering of the preview images.

**Requirements:**
- [ ] `Edit > Preferences` has a "Preview filtering" option with the values Nearest and Linear, below "Preview
      background".
- [ ] The option sets the filtering of every preview image: the sheet canvas, the selected cell, the cell list
      thumbnails, the clip timeline thumbnails and the pack dialog preview.
- [ ] The setting is saved in `moth_sprite.json` with the other editor settings, and is used again the next time
      the editor starts. The default is Nearest.

**Out of scope:**
- The project file. The filtering is an editor preference, not project data, so it is not saved in `.mothsprite`
  and is not undoable.
- What games do. The filtering only changes how the editor draws.

**Open questions:**
- Q: Which previews does this cover? A: All of them. They all draw through `SpriteEditor::DrawImage`.
- Q: Is the setting per project or an editor preference that persists between sessions? A: An editor preference,
  saved in `moth_sprite.json`.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-033 Lock pivot editing behind a mode

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
It's too easy to accidentally click on the cell preview and move the pivot. Lock it behind a button/mode.

**Requirements:**
- [ ] Clicking or dragging on the Selected Cell window's preview moves the pivot only while pivot editing is on.
      With it off, the click does nothing.
- [ ] A "Pivot" toggle button turns it on and off. It is the first button of the Selected Cell window's toolbar,
      before Fit and 1:1, in the place New Cell has in the Sheet window's toolbar.
- [ ] The mode stays on until it is turned off. Esc turns it off, as it cancels New Cell mode.
- [ ] While the mode is on, the toolbar shows it: the button is highlighted and a hint beside the zoom reads that
      dragging on the cell sets the pivot, as New Cell mode does.
- [ ] The mode is off every time the editor starts. It is not saved to `moth_sprite.json`.

**Out of scope:**
- The other ways to set a pivot. The Cells form's Pivot X and Y fields, the 3x3 preset grid and Edit > Pivot keep
  working whether the mode is on or off.
- The Sheet window and its New Cell mode.

**Open questions:**
- Q: Is it a toggle button that stays on (a mode, like New Cell), or does it turn off after one pivot change? A: A
  toggle that stays on.
- Q: Where is the button? A: The Selected Cell window's toolbar, beside Fit and 1:1.
- Q: Does it have a keyboard shortcut, and does Esc turn the mode off? A: No shortcut turns it on. Esc turns it
  off.
- Q: Are the other ways to set a pivot also locked? A: No, only the click and drag in the preview.
- Q: How does the preview show that pivot editing is on? A: The button is highlighted and a hint is shown beside
  the zoom, as for New Cell.
- Q: What does a click on the preview do while the mode is off? A: Nothing.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-032 Unpack cells to their own images

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
Add an unpack step which can save cells out as their own images.

**Requirements:**
- [ ] `Tools > Unpack...` opens a dialog, beside `Tools > Pack...`. It is disabled when the project has no cells.
- [ ] The dialog asks for the output folder and the image format, offering the same formats as the pack dialog
      (PNG, BMP, TGA, JPEG, with the JPEG quality).
- [ ] Unpack writes every cell as its own image, in cell order.
- [ ] The files are named after the sheet image with a zero-padded cell number, for example `hero_000.png`, so the
      names keep the cell order, as packing already names the cells it packs.
- [ ] Before Unpack is pressed, the dialog says how many files in the folder would be overwritten. Unpack then
      overwrites them, as the pack dialog warns about overwriting a source image.
- [ ] Unpack writes files only. The cells, the sheet image and the clips do not change, nothing is added to the
      undo stack, and the project is not marked as unsaved.

**Out of scope:**
- Turning the cells into cells from their own images (the inverse of Pack). Unpack only writes files.
- Cells from other images (T-025). They are already their own image.

**Open questions:**
- Q: Where is Unpack in the UI, and does it have a dialog with settings? A: `Tools > Unpack...`, with a dialog.
- Q: Which cells are saved: every cell, or the selected cells? A: Every cell.
- Q: How are the image files named, and in which folder and format are they written? A: Named after the sheet
  image with a zero-padded cell number, in a folder chosen in the dialog, in a format chosen in the dialog.
- Q: Does Unpack change the project? A: No, it only writes files.
- Q: What happens to existing files with the same names? A: The dialog warns how many would be overwritten, and
  Unpack overwrites them.

**Notes:**

**Commits:**

**Manual verification:**

### [todo] T-034 Multi-select clip frames

**Review:** reviewed 2026-09-19

**Depends on:** none

**Goal:**
Support the ability to multi select clip frames and drag/delete/set timing.

**Requirements:**
- [ ] More than one step of a clip can be selected on its timeline. A selection is within one clip.
- [ ] A plain click selects one step, and keeps doing what it does now: it selects the step's cell and moves
      playback to the step. Ctrl+click adds or removes a step, and Shift+click takes the range from the last
      clicked step, as the Cells window works.
- [ ] The timeline shows which steps are selected.
- [ ] Dragging a selected step moves every selected step to the drop position, keeping their order. Dragging an
      unselected step keeps moving that step alone, as now.
- [ ] The x button on a selected step removes every selected step. The Delete key removes the selected steps while
      the Clips window has keyboard focus.
- [ ] Typing a duration in the field under a selected step gives every selected step that duration. No new field
      is added; the clip's "Set all" keeps applying to every step of the clip.
- [ ] A multi-step drag, a multi-step delete and a multi-step duration change are each one undo action.

**Out of scope:**
- Box selection on the timeline. It competes with dragging a step to reorder it.
- A selection that spans more than one clip.
- The Cells window's selection and the sheet's box select.

**Open questions:**
- Q: How is a multi-selection made? A: Ctrl+click to add or remove, Shift+click for a range. No box select.
- Q: Does dragging move the frames within the clip (reorder), or something else? A: It reorders. Every selected
  step moves to the drop position, keeping its order.
- Q: Does setting the timing give every selected frame the same value, or change them relative to what they have?
  A: The same value.
- Q: Which UI shows the timing for a multi-selection? A: The existing per-step duration field. Typing in the field
  of a selected step sets every selected step.
- Q: Do the drag, delete and timing changes go on the undo stack as one step? A: Yes, one action each.
- Q: How are the selected steps deleted? A: Both the x button on a selected step and the Delete key.

**Notes:**

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.

- Found in T-015: the Cells form and the Clips window commit a pending field edit when the field's widget reports
  the end of the edit. If the widget is not drawn in that frame (the edited cell or clip is deleted by a button in
  the same frame, or the window is closed), the edit stays pending until the next field is activated. Its undo step
  then also covers changes made in between.
- Found in PR #1 review: on Windows, file paths with characters outside the active code page are not handled. NFD
  returns UTF-8, but the editor stores paths as `std::string`, builds `std::filesystem::path` from them (read as the
  code page on Windows), passes `path.string()` to stb (`stb_image` in `LoadImagePixels`, `stb_image_write` in
  `packed_image_write.c`, neither built with its `*_WINDOWS_UTF8` option), and to `TextureFromFile`. A fix has to
  cover every path from the dialogs to the file calls, not only the packed image writer.
