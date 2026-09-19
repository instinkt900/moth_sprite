# moth_sprite

A standalone sprite sheet and animation clip editor built on the moth toolkit. The user loads or imports a sprite
sheet image, defines cells with pivots, builds animation clips from those cells, and saves a `.mothsprite` project.
Games do not load project files: moth_graphics loads a sprite sheet descriptor as a `SpriteSheet`.

**Naming:** The UI and the tasks say "cell". The code says "frame" (`FrameEntry`, `m_frames`). A clip is a list of
steps, and each step refers to a frame and has a duration.

## Stack

- C++17, CMake, Conan 2.
- `moth_bridge` (Conan) brings in moth_core, moth_graphics and moth_ui: a GLFW + Vulkan platform, and ImGui.
- `moth_packer` (Conan) packs cells into a new sheet image (Tools > Pack).
- `external/nativefiledialog` (git submodule) for file dialogs, `external/stb` for image loading and writing.

## Build and run

- **Set up:** `conan install . --build=missing -s build_type=Debug` creates `build/Debug`.
- **Build:** `cmake --build --preset conan-debug`. Debug builds use `-Wall -Werror` and run clang-tidy.
- **Launch check:** `tools/smoke_launch.sh`. The app reads and writes `moth_sprite.json` (editor settings) and
  `imgui.ini` in the current directory, so do not launch it from the repository in scripts.
- There are no automated tests.

## Layout

| Path                                          | Content                                                          |
| --------------------------------------------- | ---------------------------------------------------------------- |
| `src/main.cpp`                                | Entry point. Starts the platform and runs the application.       |
| `src/sprite_application.*`                    | Application. Loads and saves editor settings, adds the editor.   |
| `src/sprite_editor_config.h`                  | Editor settings saved to `moth_sprite.json`.                     |
| `src/editor_action.h`                         | Undo interface: `IEditorAction`, `BasicAction`.                  |
| `src/common.h`                                | Precompiled header.                                              |
| `src/sprite_editor/sprite_editor.*`           | `SpriteEditor` ImGui layer: state, main draw, menus.             |
| `src/sprite_editor/sprite_editor_io.cpp`      | Load and save project files, import descriptors and images.      |
| `src/sprite_editor/sprite_editor_preview.cpp` | Sheet canvas: zoom, cell drag and resize, New Cell mode.         |
| `src/sprite_editor/sprite_editor_frames.cpp`  | Cell list, cell properties, pivot editing.                       |
| `src/sprite_editor/sprite_editor_clips.cpp`   | Clips pane and clip playback.                                    |
| `src/sprite_editor/sprite_editor_tools.cpp`   | Tools menu: grid generator, detect frames.                       |
| `src/sprite_editor/sprite_editor_undo.cpp`    | Undo stack and snapshot helpers.                                 |
| `src/sprite_editor/sprite_editor_pack.cpp`    | Tools > Pack dialog with preview; applying a pack as one undo.    |
| `src/sprite_editor/frame_detection.*`         | Frame detection from image pixels. No UI.                        |
| `src/sprite_editor/sheet_packing.*`           | Packing cells with moth_packer, writing the packed image. No UI. |
| `src/sprite_editor/packed_image_write.*`      | stb_image_write, compiled as C (outside clang-tidy).             |
| `tools/`                                      | Development scripts.                                             |
| `docs/`                                       | Task list, task profile and session logs.                        |

New `.cpp` files must be added to `SOURCES` in `CMakeLists.txt`. The precompiled header `src/common.h` applies to
C++ sources only.

## Project file

A project is a `.mothsprite` file with JSON content, read and written by the editor's own code in
`sprite_editor_io.cpp` (not by `SpriteSheetFactory`):

- `version`: the format version (`kProjectFormatVersion`). A newer version is not loaded.
- `image`: optional path to the sheet image, relative to the project file.
- `export_path`: optional path of the last exported descriptor, relative to the project file. Changing it is
  undoable.
- `pack`: optional settings of the last Tools > Pack (`PackSettings`): the packed image path, padding, padding type
  and colour, minimum and maximum size, format and JPEG quality. A pack sets the sheet image, the cell rectangles
  and these settings as one undo action.
- `frames`: cells, each `{ x, y, w, h, pivot_x, pivot_y }`. A cell imported from another image (Cells window >
  Import) is `{ image, pivot_x, pivot_y }`, with the image path relative to the project file.
- `clips`: each `{ name, loop, frames: [ { frame, duration_ms } ] }`, where `frame` is an index into `frames`.

The project is the editing source. It keeps data that games reject (no cells, clips with no steps, 0 ms steps)
through save and load unchanged.

In memory a cell is a `CellEntry`: a `FrameEntry` plus an optional `source` (`CellImage`: path and texture). A cell
with a source is the whole image; its rectangle is (0, 0) and the image size, and does not change. The Sheet window
and the sheet tools skip such cells, and `GetCellDrawSource` gives the image to draw any cell with. A project with
such cells is unpacked: Tools > Pack makes them sheet cells. Export chooses the descriptor path first, then opens the
pack dialog with the packed image named after the descriptor, and exports after a successful pack
(`m_exportAfterPack`).

Games load sprite sheet descriptors, not project files. A descriptor has `image`, `frames` and `clips` and no
`version`; `SpriteSheetFactory` loads it. File > Export writes one to the export path (File > Export As picks a new
path) and copies the sheet image beside it, named after the descriptor. Export refuses, writing nothing, when the
project has data that `SpriteSheetFactory` rejects or skips (`ExportProblems`), and when the sheet image path names
an image that could not be loaded. Saving does not export. File > Open imports a `.json` descriptor as a new project
with no path and unsaved changes, so the first save opens Save As. Imported descriptors are not added to Open
Recent. The editor reads a descriptor with its own code, as it does a project file, so it decides what is fatal: it
refuses a file it cannot parse, one with no `image` string field and one with no frames, while a sheet image that
cannot be loaded is a logged warning and the project opens without it.

`.json` project files saved before the `.mothsprite` format still open, as a descriptor import.

## Rules

### Undo stack

Every action that changes the sprite sheet project must be undoable through the `SpriteEditor` undo stack. This
includes cells, pivots, clips, clip steps, and any other data saved to the project file. Never change `m_frames`
or `m_clips` without adding an action.

- **Pattern:** Copy the state, make the change, then call `PushFrameAction`, `PushClipAction` or
  `PushFrameClipAction` with the copy. The helpers take the current state as the "after" state.
- **One action per user operation:** An operation that changes frames and clips together is one action
  (`PushFrameClipAction`), not two.
- **Continuous edits:** For text and number inputs and for drags, take the snapshot when the edit starts and add
  one action when it ends, so that one gesture is one undo step. See `m_pendingFrameSnapshot`,
  `m_pivotDragSnapshot` and `m_frameDrag`.
- **New kinds of data:** If a change affects project data the helpers do not cover, add a helper or an
  `IEditorAction`. Do not skip undo.
- View state (zoom, playback, open windows) is not undoable.

### Code style

- Follow `.clang-format` and `.clang-tidy`, and match the surrounding code.
- Warnings are errors in Debug builds.

## Workflow

Work is planned and done with the `/task-new`, `/task-planning` and `/task-session` skills, which are shared by
all moth projects and live in the workspace (`/home/mcotton/Development/moth/.claude/`). The process is in
`/home/mcotton/Development/moth/.claude/tasks/workflow.md`. The task list is `docs/tasks.md`, and the build and
launch details a session uses are in `docs/task-profile.md`.
