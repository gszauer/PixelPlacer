# PixelPlacer

A raster graphics editor built in minimal C++ with automatic memory management. Built with Claude Code Max 4.5. I never read or reviewed any of this code.

## Key Features

- Software rasterizer
- Sparse tile-based canvas
- Multi layer document model with blend modes
- Non destructive adjustment layers
- Text layers with embedded font support
- Custom brush tips with dynamics (jitter, scatter)
- Pressure-sensitive tablet support
- Selection tools with anti-aliasing and feathering
- Photoshop-style stroke buffer for brush opacity
- HiDPI/Retina display support with runtime UI scaling

## Building and Running

### Prerequisites

- g++ with C++17 support
- X11 development libraries

On Debian/Ubuntu:
```bash
sudo apt install build-essential libx11-dev
```

On Fedora:
```bash
sudo dnf install gcc-c++ libX11-devel
```

### Build Commands

Release build (optimized):
```bash
./build_linux.sh
./pixelplacer
```

Debug build (with symbols, no optimization):
```bash
./build_linux.sh debug
./pixelplacer_debug
```

### Unity Build

This project uses a unity build pattern where all source files are compiled as a single translation unit. The `main.cpp` file includes all other `.cpp` files when `UNITY_BUILD` is defined. This means you only compile one file, the compiler sees all the code at once, and it can optimize across file boundaries. The downside is any change recompiles everything, but for a project this size that's fine.

---

## Architecture Overview

```
+------------------+     +------------------+     +------------------+
|   Application    |---->|    MainWindow    |---->|   DocumentView   |
|   (lifecycle)    |     |   (widget tree)  |     |   (viewport)     |
+------------------+     +------------------+     +------------------+
         |                        |                        |
         v                        v                        v
+------------------+     +------------------+     +------------------+
|  PlatformWindow  |     |    Tool Palette  |     |     Document     |
|  (X11 backend)   |     |    Menu Bar      |     |     (layers)     |
+------------------+     |    Panels        |     +------------------+
                         +------------------+              |
                                                           v
                                                  +------------------+
                                                  |   TiledCanvas    |
                                                  |   (sparse tiles) |
                                                  +------------------+
```

---

## How the Application Runs

When you launch PixelPlacer, the `Application` class takes over. It creates a platform window (X11 on Linux), builds the entire UI as a tree of widgets, and enters the main event loop. The loop is simple: wait for an event from the OS, dispatch it to the widget tree, render the UI to a framebuffer, and blit that framebuffer to the screen. This repeats until you close the window.

The `Application` class owns everything. It holds the `PlatformWindow`, the `MainWindow` (which is the root widget), and manages the list of open documents. When you create a new document or open a file, Application creates a `Document` object and hands it to the UI. When you close the last document, the app keeps running with an empty canvas area.

All rendering is done in software. The framebuffer is just a chunk of memory containing RGBA pixels. The compositor draws each layer of the document into this buffer, the UI widgets draw themselves on top, and then the whole thing gets pushed to X11 via shared memory for display.

---

## How the Widget System Works

The UI is built as a tree of widgets. At the root is `MainWindow`, which contains the menu bar, tool palette, document view, side panels, and status bar. Each of these contains more widgets, and so on down the tree.

Every widget has a bounding rectangle (`bounds`) that defines where it lives on screen. Widgets also have a `preferredSize` which is how big they'd like to be, and the layout system uses this to arrange things. Parent widgets are responsible for positioning their children - when you call `doLayout()` on a parent, it figures out where each child should go and sets their bounds.

### Layout System

There are several layout containers:
- `HBoxLayout` arranges children horizontally, left to right
- `VBoxLayout` arranges children vertically, top to bottom
- `GridLayout` arranges children in a grid
- `ScrollView` wraps content that might be larger than the visible area

These layouts respect the `LayoutPolicy` of each child. A child can be `Fixed` (use preferredSize exactly), `Expanding` (grow to fill available space), or `Preferred` (use preferredSize but can be resized). Layouts distribute extra space among expanding widgets proportionally.

### Event Flow

When you click somewhere on the window, here's what happens:

1. X11 tells us a mouse button was pressed at coordinates (x, y)
2. We call `findWidgetAt(x, y)` on the root widget, which recursively searches the tree
3. We find the deepest widget that contains that point
4. We call `onMouseDown()` on that widget
5. If the widget returns `false` (didn't handle it), we try its parent
6. This continues up the tree until something handles it or we hit the root

This "bubbling" pattern means a button can handle its own click, but if you click on empty space inside a panel, the panel can handle it instead. Widgets that want to eat all events (like dialogs) can override `findWidgetAt` to always return themselves.

Mouse movement works similarly. When you press down on a widget, it becomes the "mouse capture" target. All subsequent mouse moves and the eventual mouse up go to that widget, even if the mouse moves outside its bounds. This is how dragging works - the slider thumb captures the mouse and keeps getting events until you release.

Keyboard events go to the focused widget. Most of the time that's a text field or the document view. The document view passes keyboard events to the current tool.

### Rendering

Widgets draw themselves via the `render()` method. The base implementation calls `renderSelf()` (which subclasses override), then renders all children. Rendering happens back-to-front, so children draw on top of parents.

All drawing goes through `Primitives`, which provides functions like `fillRect()`, `drawRect()`, `fillRoundedRect()`, `drawText()`, etc. These functions take a `Framebuffer` reference and draw pixels directly into it. There's no retained mode graphics - every frame, every visible widget redraws itself from scratch.

The color scheme comes from `config.h`, which defines an Adobe Spectrum-inspired dark theme. All the grays, blues, and semantic colors (button, hover, pressed, etc.) are defined there.

---

## How Documents Work

A `Document` represents an open image. It has a width, height, and a stack of layers. Every document has at least one layer, and there's always an "active" layer that tools draw on.

Layers come in three flavors:

**PixelLayer** - The workhorse. Contains actual pixels via a `TiledCanvas`. When you paint with a brush, you're modifying the pixels in a PixelLayer.

**AdjustmentLayer** - Contains no pixels. Instead, it applies an effect to all layers below it. Things like brightness/contrast, hue/saturation, levels, and color balance. The effect is computed on-the-fly during compositing, so adjustment layers are non-destructive.

**TextLayer** - Contains text that gets rasterized when compositing. You can edit the text, change the font size, and the rendered result updates. 

Each layer has properties like:
- `visible` - whether it's drawn at all
- `locked` - whether tools can modify it
- `opacity` - 0-100% transparency
- `blendMode` - how it combines with layers below (Normal, Multiply, Screen, Overlay, etc.)
- `name` - human-readable label

### The Observer Pattern

When something changes in a document (layer added, selection changed, pixels modified), the document notifies its observers. Any class that implements `DocumentObserver` can register itself and receive callbacks:

- `onLayerAdded()` / `onLayerRemoved()` - layer panel rebuilds its list
- `onActiveLayerChanged()` - properties panel updates its controls
- `onDocumentModified()` - navigator panel updates its thumbnail
- `onSelectionChanged()` - views redraw the selection outline

This keeps the document model clean. It doesn't know anything about UI - it just broadcasts "something changed" and the UI figures out how to respond.

---

## How the Canvas System Works

The canvas is where the actual pixels live. Rather than allocating a giant 2D array for the entire document dimensions, we use a sparse tile system.

### Tiles

The canvas is divided into a grid of 64x64 pixel tiles. A tile is only allocated when you actually draw on it. If you have a 4000x4000 document but only painted in one corner, you might only have a handful of tiles in memory instead of 16 million pixels.

Each tile is just a struct with a fixed-size pixel array. Tiles are stored in a hash map keyed by their grid coordinates. When you try to read a pixel that's in an unallocated tile, you get transparent black. When you write to an unallocated tile, one gets created automatically.

This system is great for memory efficiency, but it means you can't just index pixels by (x, y) directly. You have to compute which tile the pixel is in, check if that tile exists, then index into the tile's local coordinates.

### Reading and Writing Pixels

`TiledCanvas` provides methods like `getPixel(x, y)` and `setPixel(x, y, color)`, but these are rarely used directly. For performance, tools usually grab a tile pointer and work on it directly:

```cpp
Tile* tile = canvas.getTile(tileX, tileY);  // might create the tile
// now modify pixels in tile->pixels[] directly
```

There's also a bounds-tracking feature. The canvas remembers the bounding box of all non-empty tiles, which is useful for saving (don't write empty tiles to disk) and compositing (only composite the used region).

---

## How the Compositor Works

When it's time to display the document, the compositor flattens all the layers into a single image. This happens every frame, and it's the most CPU-intensive part of the application.

The compositor walks the layer stack from bottom to top. For each visible layer:

1. If it's a PixelLayer, composite its tiles into the output buffer using the layer's blend mode and opacity
2. If it's an AdjustmentLayer, apply the adjustment to the accumulated result so far
3. If it's a TextLayer, rasterize the text and composite it

Blend modes are implemented in `blend.h`. Each mode is a function that takes a source color (the layer being composited) and destination color (what's accumulated so far) and produces an output color. Modes like Multiply, Screen, Overlay, etc. follow the standard Photoshop formulas.

The compositor respects selections. If there's an active selection, it masks the layer content so only selected pixels are visible. Selections have an alpha mask, so feathered selections blend smoothly.

### Stroke Buffer

When painting with a brush, there's a subtlety around opacity. If you paint a stroke with 50% opacity, overlapping dabs shouldn't get darker and darker. The stroke should be uniformly 50% even where you went over the same spot twice.

This is solved with a stroke buffer. Instead of painting directly to the layer:
1. Paint the stroke to a temporary buffer at 100% opacity
2. When the stroke ends, composite the buffer to the layer at the stroke's opacity
3. Clear the buffer for the next stroke

This matches Photoshop's behavior and is essential for natural-looking brushwork.

---

## How Tools Work

Tools handle input when you're working on the document. There's always exactly one active tool. When you click on the canvas, the `DocumentView` figures out the canvas coordinates and forwards the event to the tool.

Each tool class inherits from `Tool` and implements:
- `onMouseDown()` - user clicked
- `onMouseDrag()` - user is dragging with button held
- `onMouseUp()` - user released the button
- `onMouseMove()` - user moved without pressing (for hover feedback)
- `onKeyDown()` / `onKeyUp()` - keyboard input

The tool receives coordinates in canvas space (not screen space), so it doesn't need to worry about zoom or pan. It also receives pressure from tablet input if available.

### Brush Tool

The brush is the most complex tool. When you press down:
1. Begin a new stroke (start the stroke buffer)
2. Stamp the brush tip at the starting position

As you drag:
1. Interpolate between the last position and current position
2. Place brush stamps along the path at the configured spacing
3. Each stamp is blended into the stroke buffer

When you release:
1. End the stroke (composite stroke buffer to layer)
2. Mark the document as modified

The brush tip can be a simple circle or a custom image. The brush can have dynamics that vary size, opacity, and angle based on pressure, speed, or random jitter.

### Selection Tools

Selection tools create and modify the selection mask. The document has a `Selection` object that stores an alpha mask. When you drag a rectangle selection, it:
1. Creates a temporary selection shape
2. Shows marching ants preview while dragging
3. On release, converts the shape to a mask and applies it

Selections support operations: Replace (overwrite), Add (union), Subtract (difference), and Intersect. These let you build up complex selections from simple shapes.

### Transform Tools

Transform tools modify geometry. The crop tool lets you define a region and discard everything outside it. It actually resizes the canvas and shifts all layer content. The gradient tool fills the selection (or entire layer) with a color gradient.

---

## How the Menu System Works

The menu bar is part of the window decorations - it's drawn by PixelPlacer, not the OS. This gives us full control over styling.

When you click a menu bar item:
1. The menu bar creates a `PopupMenu` and shows it below the clicked item
2. The popup menu is added to the `OverlayManager`, which ensures it renders on top of everything
3. The overlay manager sets up an "outside click" handler that closes the menu if you click elsewhere
4. Mouse hover on other menu bar items closes the current menu and opens the hovered one

Each menu item is a button inside the popup. Clicking it triggers its action callback and closes the menu. Some items open sub-menus (like Edit > ...), which are just nested popups.

The menu bar also handles the window control buttons (minimize, maximize, close) since we're using a custom titlebar. These are positioned on the right side and interact with the X11 window manager.

---

## How Dialogs Work

Dialogs are modal UI that block interaction with the rest of the app. When you open a dialog:

1. Create the dialog widget and add it to the overlay manager
2. The overlay manager shows a semi-transparent backdrop
3. All input goes to the dialog until it's closed
4. The dialog has OK/Cancel buttons that close it and invoke a callback

Dialogs inherit from `Dialog` which provides the standard frame, title bar, and button row. Subclasses fill in the content area with whatever controls they need.

### File Dialogs

File open/save dialogs use the system's native dialogs via zenity or kdialog. This is handled in `platform_linux.cpp`. The dialog call is blocking, but we have to be careful about when we invoke it - X11 has issues if you're holding a mouse grab when opening a native dialog. So file operations are "deferred" - we set a flag and the main loop opens the dialog after releasing the mouse.

---

## How Selections Work

Selections are stored as an alpha mask - a grayscale image where white means fully selected, black means not selected, and gray means partially selected (feathered edges).

When you use a selection tool:
1. The tool computes a shape (rectangle, ellipse, polygon, etc.)
2. The shape is rasterized to a temporary mask
3. The mask is combined with the existing selection based on the operation mode

The selection affects:
- Painting: only selected areas receive paint
- Fill: only selected areas get filled
- Copy/Cut: only selected pixels are copied
- Adjustments: only selected areas are adjusted

Marching ants are drawn by the document view. It finds the contour of the selection mask and animates a dashed line along it.

---

## Memory and Resource Management

The codebase uses modern C++ memory management:

**unique_ptr for ownership**: Layers are owned by the document via `unique_ptr<LayerBase>`. Widgets are owned by their parents the same way. When a parent is destroyed, all its children are automatically cleaned up.

**Raw pointers for references**: If something needs a reference to another object but doesn't own it, we use a raw pointer. For example, widgets have a `parent` pointer but don't own their parent. This is safe because the parent always outlives the child (the parent owns the child, so it won't be destroyed while children exist).

**No shared_ptr**: The ownership graph is strictly a tree. Nothing has shared ownership. This keeps things simple and avoids cycles.

---

## File Organization Quick Reference

| Area | Files |
|------|-------|
| Entry point | `main.cpp` |
| App lifecycle | `application.h/cpp`, `app_state.h/cpp` |
| Configuration | `config.h`, `types.h` |
| Math/primitives | `primitives.h/cpp` |
| Document model | `document.h/cpp`, `document_view.h/cpp`, `layer.h`, `selection.h/cpp` |
| Canvas storage | `tile.h`, `tiled_canvas.h/cpp` |
| Rendering | `framebuffer.h/cpp`, `compositor.h/cpp`, `blend.h`, `sampler.h/cpp` |
| Brush system | `brush_tip.h`, `brush_renderer.h/cpp`, `brush_tool.h/cpp`, `brush_dialogs.h/cpp` |
| Other tools | `tool.h/cpp`, `eraser_tool.h/cpp`, `fill_tool.h/cpp`, `selection_tools.h/cpp`, `transform_tools.h/cpp`, `retouch_tools.h/cpp` |
| Widget system | `widget.h/cpp`, `basic_widgets.h/cpp`, `layouts.h/cpp` |
| UI panels | `panels.h/cpp`, `dialogs.h/cpp`, `main_window.h/cpp`, `overlay_manager.h/cpp` |
| Platform layer | `platform_window.h`, `platform_linux.cpp`, `x11_window.h/cpp` |
| File I/O | `image_io.h/cpp`, `project_file.h/cpp` |
| Fonts | `inter_font.cpp`, `material_font.cpp` |

---

## Adding New Features

### Adding a New Tool

1. Create header and implementation files (`my_tool.h`, `my_tool.cpp`)
2. Add `ToolType::MyTool` to enum in `tool.h`
3. Inherit from `Tool` class
4. Implement event handlers (`onMouseDown`, `onMouseDrag`, etc.)
5. Add to unity build includes in `main.cpp`
6. Add button in `ToolPalette` constructor
7. Add tool options in `ToolOptionsBar::rebuildOptions()`

### Adding a New Widget

1. Inherit from `Widget` or appropriate base (Panel, etc.)
2. Override `renderSelf()` for custom drawing
3. Override event handlers as needed
4. Set `preferredSize` in constructor
5. Add to header file (basic_widgets.h or new file)

### Adding a New Adjustment Type

1. Add enum value to `AdjustmentType` in `layer.h`
2. Create parameter struct (e.g., `MyAdjustmentParams`)
3. Add to `AdjustmentParams` variant
4. Implement in `setDefaultParams()`
5. Add pixel processing in `compositor.cpp`
6. Add UI controls in `LayerPropsPanel`

### Adding a New Menu Item

1. Find the relevant `createXxxMenu()` function in `main_window.cpp`
2. Add `popup->addItem("Item Name", [this]() { /* action */ });`
3. For submenus, use `popup->addSubmenu("Name", createSubMenuPopup());`

### Adding a New Panel

1. Create a class inheriting from `Panel` in `panels.h/cpp`
2. Override `rebuildContent()` to create child widgets
3. Connect to document observer if it needs to react to changes
4. Add to right sidebar in `MainWindow` constructor

---

## Configuration

All tuneable values are in `config.h`:

- Canvas limits (tile size, max canvas dimensions)
- Tool defaults (brush size, opacity, hardness)
- View limits (min/max zoom)
- UI dimensions (menu bar height, panel widths, font sizes)
- Theme colors (full Adobe Spectrum dark theme)

The UI scale (`Config::uiScale`) can be changed at runtime to support HiDPI displays. All UI dimensions are defined as base values and scaled through accessor functions like `Config::menuBarHeight()`.
