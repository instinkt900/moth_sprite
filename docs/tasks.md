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

### [todo] T-028 Sprite project file format

**Review:** reviewed 2026-09-17

**Depends on:** none

**Goal:**
The editor saves projects in its own format, separate from the sprite sheet descriptor that moth_graphics loads in
games. The project file is the source for editing. It can hold data that games do not need: cells from other
images (T-025), packing settings (T-030) and the export path (T-029). Games load only exported data (T-029).

This replaces two earlier ideas: making moth_graphics load cells from more than one image, and warning on save
that the project does not work in game (T-027, now removed).

**Requirements:**
- [ ] Projects are saved as `.mothsprite` files with JSON content. The file has a format version field. The
  project Open and Save As dialogs filter on `mothsprite`.
- [ ] The editor reads and writes project files with its own code, not with `SpriteSheetFactory`.
- [ ] The project file stores what the current format stores: the sheet image path (relative to the project
  file), the cells with their pivots, and the clips.
- [ ] The sheet image is optional. A project with no sheet image, with or without cells and clips, is saved and
  loaded again with its cells and clips unchanged.
- [ ] A project with no cells, a clip with no steps, and a step with a 0 ms duration are saved and loaded again
  unchanged.
- [ ] File > Open also accepts sprite sheet descriptors (`.json`, the format saved before this task). Opening a
  descriptor makes a new project from it, with no project path and with unsaved changes. The first save opens
  Save As, so the descriptor is never written over. A descriptor that `SpriteSheetFactory` does not load is not
  opened, as today.
- [ ] Open Recent lists project files only. Opening a descriptor does not add it; saving the project adds the
  `.mothsprite` file, as for any save.
- [ ] `CLAUDE.md` ("Project file") and `README.md` describe the project file and the descriptor import, and say
  that games load exported descriptors, not project files.
- [ ] `docs/task-profile.md` Compatibility says: `.json` project files saved before this task still open, as a
  descriptor import.

**Out of scope:**
- Export to the game format (T-029). File > Export Sheet stays until T-029.
- Cells from other images (T-025) and packing (T-030). The format only needs to leave room for them.
- Editor features for working without a sheet image, beyond saving and loading such a project. Cells are still
  drawn on the sheet image.
- Changes to moth_graphics. Its sprite sheet descriptor format stays as it is.

**Open questions:**
- Q: Should moth_graphics load sprite sheets whose cells come from more than one image? A: No. A moth_graphics
  `SpriteSheet` is one image plus frame rectangles, and `.pak` loading (`GetSpriteSheetFromMemory`), moth_packer
  output and moth_anim rely on that. The project gets its own format, and games load exported data only.
- Q: What is the project file extension? A: `.mothsprite`, with JSON content.
- Q: Old `.json` projects? A: File > Open imports them as descriptors. The task profile's Compatibility rule is
  reworded to say so.
- Q: Does the project keep data that games cannot load (no cells, clips with no steps, 0 ms steps) through save
  and load? A: Yes. The project is the editing source. Only export (T-029) checks what games reject. This covers
  the T-024 item under Discovered for project files.
- Q: Can a project have no sheet image? A: Yes, the sheet image is optional in the project.
- Q: Does Open Recent list opened descriptors? A: No, project files only.

**Notes:**
- Written 2026-09-17 from a design discussion. Reviewed the same day.
- Today `LoadSpriteSheet` loads a project with `SpriteSheetFactory::GetSpriteSheet` (`sprite_editor_io.cpp`), and
  `SaveSpriteSheet` writes the descriptor format, keeps unknown fields of an existing file, and returns false when
  `m_spriteSheet` is null. The Open and Save dialogs filter on `json` (`sprite_editor.cpp`).
- The editor draws from `m_spriteSheet`, built from the sheet image. With no sheet image it can be null, as after
  File > New.

**Commits:**

**Manual verification:**

### [todo] T-029 Export to the game format

**Review:** reviewed 2026-09-17

**Depends on:** T-028

**Goal:**
Export writes the sprite sheet descriptor and image that moth_graphics loads in games. Export is its own operation:
saving saves only the project. The project remembers where it was last exported, so exporting again does not ask
for a path.

**Requirements:**
- [ ] File > Export... writes the descriptor to the project's export path. When the project has no export path, it
  opens a save dialog (filter `json`) first. File > Export As... always opens the dialog.
- [ ] The descriptor has `image`, `frames` and `clips`, and `SpriteSheetFactory` loads it.
- [ ] Export copies the sheet image next to the descriptor, named after the descriptor with the sheet image's
  extension (`hero.json` gets `hero.png`). An existing file is overwritten. The descriptor's `image` is that file
  name.
- [ ] Frame indices in the descriptor follow the project's cell order, so clip steps keep their frame indices.
- [ ] Export refuses, writes no files, and shows a message popup that lists the problems, when the project has no
  sheet image, has no cells, has a clip with no steps, or has a step with a 0 ms duration.
- [ ] If writing the descriptor or copying the image fails, an error is logged and shown in the same popup.
- [ ] The project stores the export path, relative to the project file. A successful export to a different path
  sets it as one undoable action. Undo does not remove exported files.
- [ ] Saving the project does not export.
- [ ] File > Export Sheet (T-004) is removed. The Sheet window's image path text no longer mentions it.
- [ ] `README.md` and `CLAUDE.md` describe Export and Export As, and no longer describe Export Sheet.

**Out of scope:**
- Packing cells into a new image (T-030). Without packing, the export copies the sheet image as it is.
- Cells from other images (T-025).
- Exporting on save.

**Open questions:**
- Q: Without packing, where does the exported image come from? A: The sheet image is copied next to the
  descriptor.
- Q: What is the copied image named? A: After the descriptor, with the sheet image's extension. Re-export
  overwrites it.
- Q: What happens to File > Export Sheet (T-004)? A: It is removed. File > Export... and File > Export As... are
  added.
- Q: Does saving export? A: No. Export is its own operation, and saving saves only the project.
- Q: Does the project remember the export path? A: Yes. Export reuses it without a dialog. Export As picks a new
  one. Changing it is undoable, as for any data saved to the project file.
- Q: What does export do with data that `SpriteSheetFactory` rejects or skips? A: It refuses with a message that
  says why, so game data never differs from the project without the user knowing.

**Notes:**
- Written 2026-09-17 from a design discussion. Reviewed the same day.
- A project imported from a descriptor (T-028) has no export path. Setting one to the descriptor it came from is
  allowed.
- `ExportSheet` (`sprite_editor_io.cpp`) and its menu item (`sprite_editor.cpp`) are removed. The tooltip text in
  `sprite_editor_preview.cpp` mentions Export Sheet.

**Commits:**

**Manual verification:**

### [todo] T-030 Pack the sprite sheet

**Review:** reviewed 2026-09-17

**Depends on:** T-029

**Goal:**
Add a Pack operation that packs all of the project's cells into a new sprite sheet image with the moth_packer
library. After a pack, the project uses the packed image as its sheet, and its cells are the packed rectangles.
This is how cells from other images (T-025) get onto one sheet, so the project can be exported for games. Packing
a project that has only sheet cells is also allowed, and lays them out again tightly.

(Split from T-026, together with T-031, on 2026-09-17.)

**Requirements:**
- [ ] moth_sprite depends on moth_packer: `self.requires("moth_packer/[>=2 <3]")` in `conanfile.py` (the default
  `with_ui=False`), and `find_package(moth_packer REQUIRED)` with `moth::packer` added to
  `target_link_libraries` in `CMakeLists.txt`. The README's dependency list names it.
- [ ] `stb_image_write.h` is vendored in `external/stb` beside `stb_image.h`, and compiled with
  `STB_IMAGE_WRITE_STATIC`, as `frame_detection.cpp` does for `stb_image`.
- [ ] File > Pack... opens a pack dialog. It is enabled when the project has cells.
- [ ] The dialog has: the packed image path, with a "..." browse button that opens an image save dialog; padding
  (px); padding type (Color, Extend, Mirror, Wrap) and a padding colour for Color; minimum and maximum width and
  height (powers of two); output format (PNG, BMP, TGA, JPEG) and JPEG quality for JPEG. It has Pack and Cancel
  buttons.
- [ ] The first time the dialog opens for a project, the path is `<project name>_packed.<format extension>` in the
  project's folder, or in the last image folder for a project with no path (`Untitled_packed` then). After that
  it shows the settings from the project.
- [ ] The dialog refuses to pack, with a message in the dialog, when the path is one of the project's current
  source images (the sheet image, or later an image used by a cell from T-025).
- [ ] Pack reads each cell's pixels from its source image and packs every cell as its own image, in one image, with
  the dialog's settings. Parts of a cell outside its source image are transparent in the packed image. The cell
  order, sizes, pivots and clips do not change.
- [ ] If the cells do not fit into one image of the maximum size, or a source image cannot be read, or the image
  cannot be written, nothing in the project changes, and the dialog shows the reason.
- [ ] A successful pack writes the image file, then, as one undoable action: sets the project's sheet image to the
  packed image, sets each cell's rectangle to its packed rectangle, and stores the pack settings and path in the
  project file. Undo restores the previous sheet image, rectangles and settings. The packed file stays on disk.
- [ ] After a pack, the Sheet window shows the packed image, fitted to the window. Selection and clip playback are
  reset, as for Import Sheet.
- [ ] Changing a setting in the dialog does not change the project until Pack succeeds.
- [ ] `README.md` and `CLAUDE.md` describe File > Pack and the pack settings in the project file.

**Out of scope:**
- A preview of the packing result in the dialog (T-031).
- Cells from other images, and opening the pack dialog from Export (T-025).
- Trimming transparent borders. moth_packer 2.0.0 does not trim.
- Sharing one packed rectangle between cells with the same source and rectangle. Each cell is packed separately.
- Changes to moth_packer or other moth repositories.

**Open questions:**
- Q: Is packing part of export? A: No. Pack is its own operation (File > Pack...). Packing changes the project: it
  uses the packed image as its sheet from then on, and stops using external images.
- Q: Which packing parameters does the dialog expose? A: Padding and padding type (with colour), minimum and
  maximum size, and output format (with JPEG quality). Plus the packed image path with a browse button.
- Q: Is moth_packer available to this project? A: Yes, `moth_packer` 2.x from the moth Artifactory remote, as
  moth_editor uses it. The dependency is written into Requirements.
- Q: How is the packed image written, when `PackToMemory` returns only pixels? A: With a vendored
  `stb_image_write.h`.
- Q: Can the packed image path be one of the project's current source images? A: No, the dialog refuses it.
  Writing over the sheet would break undo, because the old rectangles would point at new pixels.
- Q: What is the path the first time? A: `<project name>_packed.<ext>` beside the project.
- Q: Is packing allowed without external images? A: Yes, whenever the project has cells.
- Q: Is the pack one undo step? A: Yes. It restores the sheet image, cell rectangles and pack settings.
- Q: Is the task small enough for one session? A: It was split: the preview is T-031.

**Notes:**
- Rewritten 2026-09-17 by `/task-planning` from T-026 (deferred 2026-09-16 to finish smaller tasks first).
- Frame pivots are relative to the frame's top-left corner (`FrameEntry` in moth_graphics' `spritesheet.h`), so
  moving rectangles keeps pivots correct.
- `PackToMemory` in `PackType::Flipbook` mode sorts images by name, and in `PackType::Atlas` mode it can return
  more than one atlas. Either name the images so that their sorted order is the cell order (for example
  zero-padded indices), or use Atlas mode, refuse when more than one atlas is returned, and match results by name.
- `PackOptions::minWidth`/`maxWidth` are rounded up to the next power of two by the packer.
- moth_packer requires Conan's `stb` package itself; that does not give moth_sprite `stb_image_write.h`, so the
  header is vendored.
- Adding the dependency needs a new `conan install`. A session cannot run it; if the build directory lacks
  moth_packer, the session blocks. Run `conan install . --build=missing -s build_type=Debug` after the
  `conanfile.py` change is in, or before the session.
- The local `~/Development/moth/moth_packer` checkout (1.0.0) is out of date. Read
  `moth_toolkit/modules/packer/include/moth/packer/packer.h` (2.0.0).

**Commits:**

**Manual verification:**

### [todo] T-031 Preview in the pack dialog

**Review:** reviewed 2026-09-17

**Depends on:** T-030

**Goal:**
The pack dialog shows a preview of the packing result, so the user can see the layout and size before packing.

(Split from T-026, together with T-030, on 2026-09-17.)

**Requirements:**
- [ ] The pack dialog shows the packed image the current settings would produce, fitted to a preview area, on the
  preview background (`DrawImageBackground`).
- [ ] The dialog shows the packed image's width and height.
- [ ] The preview updates when a setting changes. When the cells do not fit, or a source cannot be read, the
  preview area shows the reason instead.
- [ ] The preview writes no files and does not change the project.

**Out of scope:**
- Changes to how Pack works (T-030).

**Open questions:**
- Q: Does the preview update on every setting change, or with a button? A: On every change.

**Notes:**
- Rewritten 2026-09-17 by `/task-planning` from T-026.
- `PackToMemory` does not write files, so the preview can use it with the same inputs as Pack.

**Commits:**

**Manual verification:**

### [todo] T-025 Import cells from off-sheet images

**Review:** reviewed 2026-09-17

**Depends on:** T-030

**Goal:**
Add support for importing cells from off-sheet images. The cell list window should get a new "import" button so
the user can import an image to use as a new cell.

The imported image stays a separate file, and the project file (T-028) refers to it. A project with cells from
other images is "unpacked": it must be packed (T-030) before it can be exported for games (T-029).

**Requirements:**
- [ ] The Cells window has an "Import..." button. It opens an image dialog (same filter and `LastImageDir` as
  Import Sheet) that allows several files to be chosen (`NFD_OpenDialogMultiple`).
- [ ] Each chosen image is added as a new cell at the end of the cell list, in the dialog's order. The cell is the
  whole image, with pivot (0, 0). The whole import is one undo action. An image that fails to load is skipped
  with a logged error; if none load, nothing changes.
- [ ] Import works in a project with no sheet image.
- [ ] The project file stores such a cell as its image path, relative to the project file, and its pivot. It has
  no x/y/w/h. A project whose cell image is missing still loads; the cell shows only the preview background, and
  a warning is logged.
- [ ] The Cells thumbnails, the Selected Cell window and the Clips preview draw the cell from its own image. In
  the Selected Cell window its pivot can be edited, and its x/y/w/h fields are read-only (showing 0, 0 and the
  image size).
- [ ] The Sheet window does not show cells from other images. Tools that work on the sheet (Grid Cells, Detect
  Cells, drawing and dragging cells on the sheet) do not change them.
- [ ] Pack (T-030) packs cells from other images like sheet cells. After a pack, they are sheet cells with the
  packed rectangles, and the project no longer refers to their images. Undo of the pack restores them.
- [ ] Export of a project with cells from other images opens the pack dialog. When the pack succeeds, the export
  continues as File > Export would. Cancelling the dialog, or a failed pack, cancels the export.
- [ ] `README.md` and `CLAUDE.md` describe importing cells, unpacked projects, and the cell's image path in the
  project file.

**Out of scope:**
- Changes to moth_graphics.
- Editing the rectangle of a cell from another image, or taking part of an image.
- Showing cells from other images in the Sheet window.
- Reloading a cell image that changed on disk while the project is open.

**Open questions:**
- Q: Where do the imported pixels live: composited into the sheet image, or kept as a separate image? A: Kept as a
  separate image. Pack (T-030) puts them on the sheet.
- Q: How is such a cell saved in the project file? A: In the T-028 format, as its image path (relative to the
  project file) and pivot.
- Q: Can moth_graphics load a sprite sheet whose cells come from more than one image? A: No, and it will not.
  Pack puts all cells on one sheet before export.
- Q: Where is the imported cell placed in the sheet, and what happens if there is no room? A: It is not placed in
  the sheet image until the user packs.
- Q: Is the cell the whole image, or a rectangle in it? A: The whole image.
- Q: Can the user import more than one image at once? A: Yes, one cell per image, as one undo action.
- Q: What does the Sheet window show when a cell from another image is selected? A: Nothing. The Sheet window
  shows only the sheet and its cells.
- Q: What does Export do with cells from other images? A: It opens the pack dialog, and continues with the export
  after a successful pack.
- Q: Is the task small enough for one session? A: Kept as one task. The drawing changes cannot be checked without
  the import.

**Notes:**
- Deferred on 2026-09-16 by `/task-planning`, to finish the smaller tasks first. Rewritten on 2026-09-17 by
  `/task-planning` after the project format decision (T-028, T-029, T-030). T-027 (a notice on save that external
  cells do not work in game) was removed, because games load only exported data.
- The editor draws every cell from `m_spriteSheet` (a moth_graphics `SpriteSheet` built from the sheet image,
  about 26 uses in `src/sprite_editor/`). Cells from other images need their own textures, loaded with
  `AssetContext::TextureFromFile` and kept alive by the project data (and by undo actions that remove them).
- `m_frames` holds `moth::gfx::SpriteSheet::FrameEntry`, which has no image reference. The cell type needs a
  source image reference beside it.

**Commits:**

**Manual verification:**

## Discovered

Problems noticed during sessions that are outside the current tasks. Candidates for `/task-new`.

- Found in T-015: the Cells form and the Clips window commit a pending field edit when the field's widget reports
  the end of the edit. If the widget is not drawn in that frame (the edited cell or clip is deleted by a button in
  the same frame, or the window is closed), the edit stays pending until the next field is activated. Its undo step
  then also covers changes made in between.
