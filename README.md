# Moth Sprite

[![Build Tests](https://github.com/instinkt900/moth_sprite/actions/workflows/build-test.yml/badge.svg)](https://github.com/instinkt900/moth_sprite/actions/workflows/build-test.yml)
[![Release](https://github.com/instinkt900/moth_sprite/actions/workflows/upload-release.yml/badge.svg)](https://github.com/instinkt900/moth_sprite/actions/workflows/upload-release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A sprite sheet and animation clip editor for [moth::gfx](https://github.com/instinkt900/moth_toolkit). Load or import
a sprite sheet image, mark out its cells and their pivots, build animation clips from those cells, and save them as
a `.mothsprite` project. Games do not load project files: moth::gfx loads a sprite sheet descriptor as a
`SpriteSheet`.

The editor is a separate application, not a toolkit one: it depends on the toolkit's modules but is not built as part
of it.

---

## Table of Contents

- [Features](#features)
  - [AI Disclosure](#ai-disclosure)
- [Usage](#usage)
  - [Editor windows](#editor-windows)
  - [Projects and sheet images](#projects-and-sheet-images)
  - [Packing](#packing)
  - [Exporting](#exporting)
  - [Making cells](#making-cells)
  - [Selecting and editing cells](#selecting-and-editing-cells)
  - [Pivots](#pivots)
  - [Clips and playback](#clips-and-playback)
  - [Undo](#undo)
  - [Keyboard shortcuts](#keyboard-shortcuts)
  - [Preferences and editor settings](#preferences-and-editor-settings)
- [Project file format](#project-file-format)
- [Building](#building)
  - [Prerequisites](#prerequisites)
  - [Linux](#linux)
  - [Windows](#windows)
  - [Running](#running)
- [Related Projects](#related-projects)
- [License](#license)

---

## Features

**Sheet editing:** draw, move and resize cells on a zoomable view of the sheet image. Select many cells at once with
Ctrl+click or a selection box, and move them together.

**Cell generation:** add a regular grid of cells in one step, or detect cells from the image's pixels by alpha or by a
background color, with a live preview before the cells are added.

**Pivots:** set each cell's pivot by clicking in a zoomable preview of the cell, by typing it, or with nine presets
that apply to every selected cell.

**Animation clips:** build clips as timelines of steps, each with a cell and a duration. Reorder steps by dragging,
change a step's cell by picking it on the sheet, and choose Stop, Reset or Loop playback.

**Live preview:** play clips inside the editor. Each step is kept on its pivot, so the animation plays in place, as it
will in the application.

**Undo and redo:** every change to cells, pivots, clips and the sheet image can be undone. A text edit or a drag is one
undo step.

**Editor comforts:** recent projects, an unsaved changes prompt, a dockable window layout, and a checkerboard or chosen
color behind every preview to show transparency.

### AI Disclosure

AI agents (primarily Claude) are used as tools in this project for tasks such as refactoring, documentation writing,
and test implementation. The architecture, design decisions, and direction of the project are human-driven. This is
not a vibe-coded project.

---

## Usage

Start `moth_sprite`. It opens with an empty, untitled project.

### Editor windows

The editor has four dockable windows. Close and reopen them from the **Window** menu. **Window > Reset Layout**
restores the default layout and opens every window.

| Window | Content |
|---|---|
| **Sheet** | The sheet image with every cell drawn over it. The sheet image path, New Cell, Fit and 1:1 are above it. |
| **Selected Cell** | Clip playback buttons, and the prime cell (see [Selecting and editing cells](#selecting-and-editing-cells)) with its pivot. With a clip selected, it previews the clip. |
| **Cells** | The list of cells, each with a thumbnail, its index, offset and size, and a form for the prime cell below. |
| **Clips** | Clip playback buttons, a new clip row, and each clip as a timeline of steps. |

In the Sheet and Selected Cell windows, the mouse wheel zooms around the cursor, **Fit** fits the image in the
window, and **1:1** shows it at its real size.

### Projects and sheet images

The **File** menu has:

| Item | Action |
|---|---|
| **New** | Start an empty, untitled project. |
| **Load...** | Open a project file (`.mothsprite`), or import a sprite sheet descriptor (`.json`). |
| **Open Recent** | Open one of the last 10 projects. **Clear Recent** empties the list. |
| **Save** | Save the project. An untitled project asks for a file name first. |
| **Save As...** | Save the project to a new file. |
| **Import Sheet...** | Use a different sheet image (`png`, `jpg`, `jpeg` or `bmp`). Cells and clips are kept. |
| **Pack...** | Pack every cell into a new sheet image (see [Packing](#packing)). Enabled when the project has cells. |
| **Export...** | Export the sprite sheet descriptor that games load (see [Exporting](#exporting)) to the project's export path. The first export asks for a file name. |
| **Export As...** | Export to a new file name, which becomes the project's export path. |
| **Exit** | Quit. |

The **...** button next to the image path in the Sheet window does the same as **File > Import Sheet**.

A project can be saved without a sheet image. The window title shows the project's file name, and ` *` when it has
unsaved changes. New, Load, Open Recent and quitting ask whether to save unsaved changes first.

### Packing

**File > Pack...** packs every cell, as its own image, into one new image, and makes the project use it: the new
image becomes the sheet image, and each cell's rectangle becomes its place in it. Cell order, sizes, pivots and clips
do not change. Parts of a cell outside the sheet image are transparent in the packed image.

The pack dialog has:

| Setting | Content |
|---|---|
| **Packed image** | The file to write. **...** chooses it in a dialog. The first time, it is `<project name>_packed.png` beside the project (in the last image folder for an untitled project). It cannot be the sheet image. |
| **Padding (px)** | Space around each cell. |
| **Padding type** | How the padding is filled: **Color** (with **Padding color**, which is also the background), **Extend**, **Mirror** or **Wrap**. |
| **Min width**, **Min height**, **Max width**, **Max height** | The size limits of the packed image, in powers of two. |
| **Format** | PNG, BMP, TGA or JPEG (with **JPEG quality**). Changing it changes the path's extension. |

Beside the settings, the dialog previews the packed image that the settings would produce, with its width and
height. The preview updates when a setting changes, and shows the reason instead when the cells do not fit or an
image cannot be read. The preview writes no files.

**Pack** writes the image and changes the project as one undo step. Undo restores the previous sheet image, cell
rectangles and pack settings; the packed file stays on disk. If the cells do not fit into one image of the maximum
size, or an image cannot be read or written, the dialog shows why and nothing changes. The dialog changes nothing
until **Pack** succeeds, and the project saves the settings of its last pack.

### Exporting

Games do not load project files. **File > Export...** writes a sprite sheet descriptor (`.json`) that moth::gfx loads
as a `SpriteSheet`, and copies the sheet image beside it, named after the descriptor with the image's extension:
exporting `hero.json` writes `hero.png`. Files already there are overwritten. Saving the project does not export.

The project remembers its export path, relative to the project file, so **Export...** does not ask again. **Export
As...** always asks. Setting a new export path is an undo step; undo does not remove exported files.

Export refuses, writes no files, and lists the problems when the project has no sheet image, has no cells, has a cell
with no width or height, has a clip with no steps, or has a step with a duration of 0 ms or with no cell. These are the
things moth::gfx rejects or skips, so the game data never differs from the project without a warning. The same
message shows when the image cannot be copied or the descriptor cannot be written.

### Making cells

- **New Cell** (Sheet window): drag a rectangle on the sheet to add a cell. The mode stays on, so more cells can be
  drawn. Esc ends it.
- **Tools > Grid Cells...**: add a regular grid of cells. Set the cell size, the offset of the first cell, the
  spacing between cells, and the number of rows and columns. **Fit to image** sets the rows and columns that fit.
  A preview shows the grid on the sheet. **Add Frames** adds the cells.
- **Tools > Detect Cells...**: find cells from the image's pixels. Choose how the background is found: by **Alpha**,
  by the **Corner color**, or by a chosen **Color**, with a tolerance. **Merge gap** joins nearby shapes into one
  cell, the size filter drops cells that are too small or too large, and **Buffer** adds space around each cell. A
  preview shows the cells found. **Add Frames** adds them.

New cells have their pivot at the top-left corner.

### Selecting and editing cells

One or more cells can be selected. The **prime** cell is the last one selected. It has its own border color, the
Cells form and the Selected Cell window show it, and only it has resize handles on the sheet.

On the **Sheet**:

- Click a cell to select only that cell. Click empty space to clear the selection.
- Ctrl+click a cell to add it to the selection, or remove it.
- Drag on empty space to select the cells that are fully inside the box. Hold Ctrl to add them to the selection.
- Drag a selected cell to move every selected cell.
- Drag an edge or a corner of the prime cell to resize it.

In the **Cells** list:

- Click a row to select only that cell. Ctrl+click adds or removes it. Shift+click selects every cell from the prime
  cell to that row.
- The **x** button on a row deletes that cell.

The **Cells** form below the list edits the prime cell's X, Y, W, H, Pivot X and Pivot Y. It is disabled while a clip
plays.

Ctrl+A selects every cell. Delete deletes the selected cells. Esc clears the selection. Deleting a cell updates the
clip steps that use the cells after it.

### Pivots

A pivot is a point relative to the cell's top-left corner. Set it in one of these ways:

- Click or drag on the cell in the **Selected Cell** window.
- Type it in the Cells form.
- Use the nine **Pivot presets** buttons in the Cells form, or **Edit > Pivot**. These set the pivot of every selected
  cell, relative to each cell's own size: top left, top center, top right, and so on to bottom right.

### Clips and playback

In the **Clips** window:

- **+ Clip** adds a clip with the name typed next to it, or `clip_1`, `clip_2` and so on when the name is empty.
  With more than one cell selected, the clip gets a step for each selected cell, in the order they were selected.
- Each clip has a name, a loop type (**Stop**, **Reset** or **Loop**), its step count, a duration box with
  **Set all** to give every step that duration, and **X** to remove the clip.
- **+ Step** adds the selected cells to the end of the clip, or the prime cell when one cell is selected. New steps
  get the duration of the clip's last step, or the duration in the **Set all** box when the clip has no steps. The
  steps of a new clip get 100 ms, the box's starting value.
- Each step shows its cell number, a thumbnail and its duration in milliseconds. Its **x** removes it.
- Drag a step onto another step of the same clip to move it there.
- Double-click a step, then select a cell on the sheet or in the Cells list, to change the step's cell. Esc cancels.
- Click a step to go to it. With the Clips window focused, Delete removes the current step.

**Play**/**Pause**, **Step** and **Reset** are in both the Clips and the Selected Cell windows. Playback selects each
step's cell as it plays. The Selected Cell window places each step's cell on its pivot inside the clip's bounds.

### Undo

**Edit > Undo** (Ctrl+Z) and **Edit > Redo** (Ctrl+Y) cover every change to cells, pivots, clips and steps, and
**Import Sheet**, **Pack**, and a change of the export path. Typing in a field, or a drag, is one undo step. New, Load and Open Recent clear
the undo history.

### Keyboard shortcuts

Shortcuts do not work while a text field is being edited or a dialog is open.

| Shortcut | Action |
|---|---|
| Ctrl+N | New |
| Ctrl+L | Load |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+X | Exit |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+A | Select every cell |
| Delete | Delete the selected cells, or the current clip step when the Clips window is focused |
| Esc | Cancel picking a cell for a step, else end New Cell, else clear the selection |

### Preferences and editor settings

**Edit > Preferences** sets the normal, selected and prime cell border colors, the border thickness, and the preview
background color. A background color with alpha 0 shows a gray and white checkerboard.

The editor keeps its settings in `moth_sprite.json` and its window layout in `imgui.ini`, both in the folder it is
started from.

**Help > About** shows the version.

---

## Project file format

A project is a `.mothsprite` file with JSON content. It is the source for editing, and it keeps data that games do
not load. Games load sprite sheet descriptors, not project files.

```json
{
  "version": 1,
  "image": "hero.png",
  "frames": [
    { "x": 0,  "y": 0, "w": 32, "h": 48, "pivot_x": 16, "pivot_y": 48 },
    { "x": 32, "y": 0, "w": 32, "h": 48, "pivot_x": 16, "pivot_y": 48 }
  ],
  "clips": [
    {
      "name": "walk",
      "loop": "loop",
      "frames": [
        { "frame": 0, "duration_ms": 100 },
        { "frame": 1, "duration_ms": 100 }
      ]
    }
  ]
}
```

| Field | Content |
|---|---|
| `version` | The format version. The editor does not load a file with a version newer than it knows. |
| `image` | Path to the sheet image, relative to the project file. Optional: a project can have no sheet image. |
| `export_path` | Path of the last exported descriptor, relative to the project file. Optional. |
| `pack` | The settings of the last pack. Optional. `image` (the packed image, relative to the project file), `padding`, `padding_type` (`color`, `extend`, `mirror` or `wrap`), `padding_color` (`RRGGBBAA` hex), `min_width`, `min_height`, `max_width`, `max_height`, `format` (`png`, `bmp`, `tga` or `jpeg`) and `jpeg_quality`. |
| `frames` | The cells. Each has `x`, `y`, `w` and `h` in pixels, and `pivot_x` and `pivot_y` relative to the cell's top-left corner. |
| `clips` | Each clip has a `name`, a `loop` type (`stop`, `reset` or `loop`), and `frames`: its steps. |
| `clips[].frames` | Each step has `frame`, an index into `frames`, and `duration_ms`. |

The editor writes the whole file when it saves. A project keeps what games reject: no cells, clips with no steps, and
steps with a 0 ms duration are saved and loaded unchanged.

### Sprite sheet descriptors

A sprite sheet descriptor is the JSON file that moth::gfx loads as a `SpriteSheet`, written by
[File > Export](#exporting). It has the `image`, `frames` and `clips` fields above, with no `version` or
`export_path`. Frame indices follow the project's cell order. Projects saved before the `.mothsprite` format are descriptors.

**File > Load** opens a descriptor (`.json`) as a new project: the project has no file name and has unsaved changes,
so the first save asks for a `.mothsprite` file name and never writes over the descriptor. A descriptor that
moth::gfx does not load is not opened. Opened descriptors are not added to Open Recent.

moth::gfx loads a descriptor with at least one cell. It skips a clip that has no steps, or a step whose duration is
0 or whose cell does not exist.

---

## Building

Pre-built binaries for Windows and Linux are attached to each [GitHub Release](https://github.com/instinkt900/moth_sprite/releases) if you'd rather not build from source.

### Prerequisites

Set up a Python virtual environment and install Conan:

```bash
# Linux / macOS
python3 -m venv .venv
source .venv/bin/activate
pip install conan

# Windows (PowerShell)
python3 -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install conan
```

**C++17 is required.** The recipe checks for it. On Linux, Conan's detected profile already uses `gnu17`. On
Windows, MSVC's detected profile defaults to C++14, so pass `-s compiler.cppstd=17` or set it in your Conan profile.
The app renders with Vulkan, so it also needs a Vulkan driver.

moth_sprite depends on the [moth_toolkit](https://github.com/instinkt900/moth_toolkit) modules `moth_bridge`, which
brings in `moth_core`, `moth_graphics` and `moth_ui`, and `moth_packer`, which packs cells into a new sheet image. These are published to an Artifactory remote rather than Conan
Center. Register the remote once before installing (it is publicly readable, so no login is required):

```bash
conan remote add moth https://artifactory.matthewcotton.net/artifactory/api/conan/conan-local
```

The file dialogs come from a git submodule. Clone with submodules, or initialise them after cloning:

```bash
git clone --recursive https://github.com/instinkt900/moth_sprite.git
# or, in an existing clone
git submodule update --init
```

### Linux

Several system packages are required on Linux. GTK3 is needed by nativefiledialog; GLFW, FreeType, and HarfBuzz are
pulled in transitively via `moth_graphics` (see the [moth_toolkit README](https://github.com/instinkt900/moth_toolkit)
for background on why these must come from the system).

Conan installs these through `apt` when you allow it to manage system packages:

```bash
conan install . -s build_type=Release --build=missing \
    -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
cmake --preset conan-release
cmake --build --preset conan-release
```

If you'd rather install them yourself first:

```bash
sudo apt install libgtk-3-dev libglfw3-dev libfreetype-dev libharfbuzz-dev
```

For a Debug build, use `-s build_type=Debug` and the `conan-debug` preset. The Debug build treats warnings as errors
and runs clang-tidy when it is installed.

### Windows

```bash
conan install . -s compiler.cppstd=17 -s build_type=Release --build=missing
cmake --preset conan-default
cmake --build --preset conan-release
```

### Running

The binary is in `build/Release` (or `build/Debug`). The editor reads and writes `moth_sprite.json` and `imgui.ini` in
the folder it is started from, so start it from a folder where those files belong.

---

## Related Projects

| Project | Description |
|---|---|
| [moth_toolkit](https://github.com/instinkt900/moth_toolkit) | The modular 2D engine toolkit this editor builds against |
| `moth::gfx` | Vulkan-backed 2D renderer, window management, and the platform bootstrap. Loads sprite sheet descriptors as a `SpriteSheet` |
| `moth::ui` | Core UI library: node graph, keyframe animation, and event system |
| `moth::bridge` | Adapts `moth::ui` onto `moth::gfx` and provides the application loop |
| [moth_editor](https://github.com/instinkt900/moth_editor) | Visual layout and animation editor for `moth::ui` layout files |
| moth_sprite | *(this project)* Sprite sheet and animation clip editor |

---

## License

MIT. See [LICENSE](LICENSE).
