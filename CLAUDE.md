# moth_sprite

A standalone sprite sheet and animation clip editor built on the moth toolkit. The user loads or imports a sprite
sheet image, defines cells with pivots, builds animation clips from those cells, and saves a JSON project that
moth_graphics loads as a `SpriteSheet`.

**Naming:** The UI and the tasks say "cell". The code says "frame" (`FrameEntry`, `m_frames`). A clip is a list of
steps, and each step refers to a frame and has a duration.

## Stack

- C++17, CMake, Conan 2.
- `moth_bridge` (Conan) brings in moth_core, moth_graphics and moth_ui: a GLFW + Vulkan platform, and ImGui.
- `external/nativefiledialog` (git submodule) for file dialogs, `external/stb` for image loading.

## Build and run

- **Set up:** `conan install . --build=missing -s build_type=Debug` creates `build/Debug`.
- **Build:** `cmake --build --preset conan-debug`. Debug builds use `-Wall -Werror` and run clang-tidy.
- **Launch check:** `tools/smoke_launch.sh`. The app reads and writes `moth_sprite.json` (editor settings) and
  `imgui.ini` in the current directory, so do not launch it from the repository in scripts.
- There are no automated tests.

## Layout

| Path                                          | Content                                                        |
| --------------------------------------------- | -------------------------------------------------------------- |
| `src/main.cpp`                                | Entry point. Starts the platform and runs the application.     |
| `src/sprite_application.*`                    | Application. Loads and saves editor settings, adds the editor. |
| `src/sprite_editor_config.h`                  | Editor settings saved to `moth_sprite.json`.                   |
| `src/editor_action.h`                         | Undo interface: `IEditorAction`, `BasicAction`.                |
| `src/common.h`                                | Precompiled header.                                            |
| `src/sprite_editor/sprite_editor.*`           | `SpriteEditor` ImGui layer: state, main draw, menus.           |
| `src/sprite_editor/sprite_editor_io.cpp`      | New, load, import image, save project.                         |
| `src/sprite_editor/sprite_editor_preview.cpp` | Sheet canvas: zoom, cell drag and resize, New Cell mode.       |
| `src/sprite_editor/sprite_editor_frames.cpp`  | Cell list, cell properties, pivot editing.                     |
| `src/sprite_editor/sprite_editor_clips.cpp`   | Clips pane and clip playback.                                  |
| `src/sprite_editor/sprite_editor_tools.cpp`   | Tools menu: grid generator, detect frames.                     |
| `src/sprite_editor/sprite_editor_undo.cpp`    | Undo stack and snapshot helpers.                               |
| `src/sprite_editor/frame_detection.*`         | Frame detection from image pixels. No UI.                      |
| `tools/`                                      | Development scripts.                                           |
| `docs/`                                       | Task list, task profile and session logs.                      |

New `.cpp` files must be added to `SOURCES` in `CMakeLists.txt`.

## Project file

A project is a JSON file:

- `image`: path to the sheet image, relative to the project file.
- `frames`: cells, each `{ x, y, w, h, pivot_x, pivot_y }`.
- `clips`: each `{ name, loop, frames: [ { frame, duration_ms } ] }`, where `frame` is an index into `frames`.

Project files saved by older versions must still load.

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
