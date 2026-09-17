# Task profile

What `/task-session` needs to know about moth_sprite. The process is in the shared
workflow, `/home/mcotton/Development/moth/.claude/tasks/workflow.md`.

## Setup

`conan install . --build=missing -s build_type=Debug` creates `build/Debug`. The
session checks that `build/Debug` exists.

## Build

- Incremental: `cmake --build --preset conan-debug`
- Clean: `cmake --build --preset conan-debug --clean-first`

The Debug build uses `-Wall -Werror` and runs clang-tidy.

## Tests

none. The project has no automated tests.

## Launch check

`tools/smoke_launch.sh`. It:

1. Starts `build/Debug/moth_sprite` in a new temporary directory, because the app
   reads and writes `moth_sprite.json` and `imgui.ini` in the current directory.
2. Waits for the "Moth Sprite" window, then lets the app run for 5 seconds.
3. Closes the app with `tools/close_window.py`, which sends `WM_DELETE_WINDOW`, so
   the app's normal shutdown runs.
4. Passes when the app exits with code 0, the log has no `[warning]` or `[error]`
   lines (Debug enables the Vulkan validation layers), and `moth_sprite.json` was
   written, which shows that `Shutdown()` finished.

It prints `SMOKE LAUNCH PASSED` and exits 0, or prints `SMOKE LAUNCH FAILED` with the
reasons and the log path. Exit code 2 means it could not run (no display, no build,
missing tool). It needs an X11 display, `xdotool` and `python3`.

## Rules

- Every change to the sprite sheet project (cells, pivots, clips, clip steps, any
  data saved to the project file) goes through the `SpriteEditor` undo stack. See
  "Undo stack" in `CLAUDE.md`.
- New `.cpp` files are added to `SOURCES` in `CMakeLists.txt`.

## Compatibility

`.json` project files saved before the `.mothsprite` format (T-028) still open, as a
descriptor import. `.mothsprite` project files saved by older versions still load.
