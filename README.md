# Moth Sprite

A sprite sheet and animation clip editor for [moth_graphics](https://github.com/instinkt900/moth_graphics). Load or
import a sprite sheet image, mark out its cells and their pivots, build animation clips from those cells, and save a
JSON project that moth_graphics loads as a `SpriteSheet`.

---

## Table of Contents

- [Features](#features)
- [Usage](#usage)
  - [Windows](#windows)
  - [Projects and sheet images](#projects-and-sheet-images)
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
  - [Build and run](#build-and-run)
- [Related Projects](#related-projects)
- [License](#license)

---

## Features

- A zoomable sheet view where cells are drawn, moved and resized with the mouse.
- Cells made by hand, from a regular grid, or found automatically from the image's pixels.
- Multiple selection, with pivot presets that apply to every selected cell.
- A pivot editor with a zoomable preview of the selected cell.
- Animation clips as timelines of steps, each with a cell and a duration, with Stop, Reset and Loop playback.
- A clip preview that keeps every step on its pivot, so the animation plays in place.
- Undo and redo for every change to cells, pivots, clips and the sheet image.
- A checkerboard or a chosen color behind every preview, to show transparency.
- Recent projects, an unsaved changes prompt, and a dockable window layout.

---

## Usage

Start `moth_sprite`. It opens with an empty, untitled project.

### Windows

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
| **Load...** | Open a project file (`.json`). |
| **Open Recent** | Open one of the last 10 projects. **Clear Recent** empties the list. |
| **Save** | Save the project. An untitled project asks for a file name first. |
| **Save As...** | Save the project to a new file. |
| **Import Sheet...** | Use a different sheet image (`png`, `jpg`, `jpeg` or `bmp`). Cells and clips are kept. |
| **Export Sheet...** | Copy the sheet image to a new file, and make the project use the copy. |
| **Exit** | Quit. |

The **...** button next to the image path in the Sheet window does the same as **File > Import Sheet**.

A project can only be saved when it has a sheet image. The window title shows the project's file name, and ` *` when
it has unsaved changes. New, Load, Open Recent and quitting ask whether to save unsaved changes first.

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
  get the duration in the clip's **Set all** box.
- Each step shows its cell number, a thumbnail and its duration in milliseconds. Its **x** removes it.
- Drag a step onto another step of the same clip to move it there.
- Double-click a step, then select a cell on the sheet or in the Cells list, to change the step's cell. Esc cancels.
- Click a step to go to it. With the Clips window focused, Delete removes the current step.

**Play**/**Pause**, **Step** and **Reset** are in both the Clips and the Selected Cell windows. Playback selects each
step's cell as it plays. The Selected Cell window places each step's cell on its pivot inside the clip's bounds.

### Undo

**Edit > Undo** (Ctrl+Z) and **Edit > Redo** (Ctrl+Y) cover every change to cells, pivots, clips and steps, and
**Import Sheet** and **Export Sheet**. Typing in a field, or a drag, is one undo step. New, Load and Open Recent clear
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

A project is a JSON file:

```json
{
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
| `image` | Path to the sheet image, relative to the project file. |
| `frames` | The cells. Each has `x`, `y`, `w` and `h` in pixels, and `pivot_x` and `pivot_y` relative to the cell's top-left corner. |
| `clips` | Each clip has a `name`, a `loop` type (`stop`, `reset` or `loop`), and `frames`: its steps. |
| `clips[].frames` | Each step has `frame`, an index into `frames`, and `duration_ms`. |

Other fields already in a project file are kept when the editor saves over it.

moth_graphics loads a project with at least one cell. It skips a clip that has no steps, or a step whose duration is
0 or whose cell does not exist.

---

## Building

### Prerequisites

- A C++17 compiler and CMake 3.27 or later.
- [Conan 2](https://conan.io). For example:

  ```bash
  python3 -m venv .venv
  source .venv/bin/activate
  pip install conan
  ```

- The moth toolkit packages (moth_bridge, which brings in moth_core, moth_graphics and moth_ui) come from the moth
  Artifactory remote. Add it once:

  ```bash
  conan remote add moth https://artifactory.matthewcotton.net/artifactory/api/conan/conan-local
  ```

- On Linux, the file dialogs need GTK 3 (`libgtk-3-dev`). Conan can install it as a system requirement.
- The app uses Vulkan, so it needs a Vulkan driver.

Clone with submodules, or initialise them after cloning:

```bash
git clone --recursive https://github.com/instinkt900/moth_sprite.git
# or, in an existing clone
git submodule update --init
```

### Build and run

```bash
conan install . --build=missing -s build_type=Debug
cmake --build --preset conan-debug
./build/Debug/moth_sprite
```

For a Release build, use `-s build_type=Release` and the `conan-release` preset. The binary is then in
`build/Release`.

The Debug build treats warnings as errors and runs clang-tidy when it is installed.

The editor reads and writes `moth_sprite.json` and `imgui.ini` in the folder it is started from. Start it from a
folder where those files belong.

---

## Related Projects

| Project | Description |
|---|---|
| [moth_ui](https://github.com/instinkt900/moth_ui) | Core UI library: node graph, keyframe animation, and event system |
| [moth_graphics](https://github.com/instinkt900/moth_graphics) | Graphics and application framework. Loads the projects this editor saves as a `SpriteSheet` |
| [moth_editor](https://github.com/instinkt900/moth_editor) | Visual layout and animation editor for moth_ui layout files |
| [moth_packer](https://github.com/instinkt900/moth_packer) | Texture atlas packer for images and moth_ui layouts |
| moth_sprite | *(this project)* Sprite sheet and animation clip editor |

---

## License

MIT
