# CLAUDE.md - Agent Instructions for PixelPlacer

This file contains instructions for AI agents (Claude Code, Cursor, etc.) working on the PixelPlacer codebase.

## Quick Reference

### Build and Test
```bash
# Linux native build
./build_linux.sh        # Release build -> ./pixelplacer
./build_linux.sh debug  # Debug build -> ./pixelplacer_debug

# Windows native build (from Windows cmd or cross-compile from Linux)
build_windows.bat              # On Windows -> pixelplacer.exe
./build_windows_cross.sh       # Cross-compile on Linux -> pixelplacer.exe

# WebAssembly build (requires Emscripten SDK)
./build_wasm.sh         # Release build -> www/index.html
./build_wasm.sh debug   # Debug build -> www/index.html

# Test WASM locally
cd www && python3 -m http.server 8080
# Open http://localhost:8080
```

### Key Directories
- All source files are in the `code/` directory
- Headers: `code/*.h`
- Implementations: `code/*.cpp`
- Linux build output: `pixelplacer`, `pixelplacer_debug` (in root)
- Windows build output: `pixelplacer.exe`, `pixelplacer_debug.exe` (in root)
- WASM build output: `www/` (transient, contains index.html, pixelplacer.js, pixelplacer.wasm)

### Most Frequently Modified Files
| Task | Primary Files |
|------|--------------|
| UI changes | `code/main_window.cpp`, `code/panels.cpp`, `code/basic_widgets.cpp` |
| Tool behavior | `code/tool.cpp`, `code/brush_tool.cpp`, `code/selection_tools.cpp` |
| Document operations | `code/document.cpp`, `code/layer.h` |
| Undo/Redo | `code/undo.h`, `code/undo.cpp`, `code/document.cpp` |
| Rendering | `code/compositor.cpp`, `code/framebuffer.cpp` |
| Configuration | `code/config.h` |
| Application lifecycle | `code/application.cpp` |
| Linux platform | `code/platform_linux.cpp`, `code/x11_window.cpp` |
| Windows platform | `code/platform_windows.cpp`, `code/win32_window.cpp` |
| WASM platform | `code/platform_wasm.cpp`, `code/wasm_window.cpp`, `code/shell.html` |

---

## Strict Code Conventions

### Type Aliases - ALWAYS Use These
```cpp
// CORRECT
u32 width;
f32 opacity;
i32 index;

// WRONG - never use raw C++ types
unsigned int width;
float opacity;
int index;
```

Full list: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64`

### Naming Rules

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `class BrushTool` |
| Structs | PascalCase | `struct LayerBase` |
| Methods | camelCase | `void handleMouseDown()` |
| Functions | camelCase | `f32 lerp()` |
| Variables | camelCase | `f32 brushSize` |
| Constants | UPPER_SNAKE | `constexpr u32 MAX_SIZE` |
| Enums | PascalCase | `enum class BlendMode` |
| Enum values | PascalCase | `BlendMode::Multiply` |

### Include Guards - Exact Format
```cpp
#ifndef _H_MYFILE_
#define _H_MYFILE_

// content

#endif
```

### Header vs Implementation Split

**Put in headers (.h):**
- Class/struct definitions
- Method declarations
- Small inline methods (Vec2, Rect operators)
- Constants
- Forward declarations

**Put in implementations (.cpp):**
- Method bodies longer than 3-4 lines
- Static data
- Complex logic
- Helper functions

---

## Unity Build System

This project uses unity build. Key points:

1. **Only `code/main.cpp` is compiled** - all other `.cpp` files are `#include`d
2. **Adding a new .cpp file requires updating `code/main.cpp`**:
   ```cpp
   #ifdef UNITY_BUILD
   // ... existing includes ...
   #include "my_new_file.cpp"  // ADD THIS
   #endif
   ```
3. **Order matters** - files are included in dependency order
4. **No separate compilation** - changes to any file trigger full rebuild

### When Adding New Files

1. Create both `.h` and `.cpp` files in the `code/` directory
2. Add `#include "myfile.cpp"` to `code/main.cpp` in the `UNITY_BUILD` block
3. Place include after any dependencies
4. Test build immediately

---

## Common Tasks

### Adding a New Tool

1. **Create files**: `code/my_tool.h`, `code/my_tool.cpp`

2. **Add to ToolType enum** (`code/tool.h`):
   ```cpp
   enum class ToolType {
       // ... existing ...
       MyTool,  // ADD
   };
   ```

3. **Implement the tool**:
   ```cpp
   // my_tool.h
   #ifndef _H_MY_TOOL_
   #define _H_MY_TOOL_

   #include "tool.h"

   class MyTool : public Tool {
   public:
       MyTool();
       void onMouseDown(Document& doc, const ToolEvent& e) override;
       void onMouseDrag(Document& doc, const ToolEvent& e) override;
       void onMouseUp(Document& doc, const ToolEvent& e) override;
   private:
       Vec2 startPos;
       bool active = false;
   };

   #endif
   ```

4. **Add to unity build** (`code/main.cpp`):
   ```cpp
   #include "my_tool.cpp"
   ```

5. **Add toolbar button** (`code/main_window.cpp` in `ToolPalette` constructor):
   ```cpp
   addToolButton(ToolType::MyTool, "icon_name", "My Tool");
   ```

6. **Add tool options** (`code/main_window.cpp` in `ToolOptionsBar::rebuildOptions()`):
   ```cpp
   case ToolType::MyTool:
       buildMyToolOptions();
       break;
   ```

7. **Add undo support** (see [Undo/Redo System](#undoredo-system) section):
   - Call `doc.beginPixelUndo()` in `onMouseDown`
   - Call `doc.captureOriginalTilesInRect()` before modifying pixels
   - Call `doc.commitUndo()` in `onMouseUp`

### Adding a New Widget

1. **Add to `code/basic_widgets.h`** or create new header:
   ```cpp
   class MyWidget : public Widget {
   public:
       MyWidget();
       void renderSelf(Framebuffer& fb) override;
       bool onMouseDown(const MouseEvent& e) override;
   private:
       // state
   };
   ```

2. **Implement in `code/basic_widgets.cpp`** or new `.cpp`:
   ```cpp
   MyWidget::MyWidget() {
       preferredSize = Vec2(100 * Config::uiScale, 30 * Config::uiScale);
   }

   void MyWidget::renderSelf(Framebuffer& fb) {
       Rect gb = globalBounds();
       // draw using fb.fillRect(), fb.drawRect(), FontRenderer, etc.
   }
   ```

3. **Use with createChild**:
   ```cpp
   auto* myWidget = parent->createChild<MyWidget>();
   myWidget->someCallback = [this]() { /* ... */ };
   ```

### Adding a Menu Item

In `code/main_window.cpp`, find the appropriate `create*Menu()` function:

```cpp
PopupMenu* MenuBar::createEditMenu() {
    auto* menu = new PopupMenu();

    // Add new item (label, shortcut, callback)
    menu->addItem("My Action", "", [this]() {
        closeActiveMenu();
        // Do something
    });

    return menu;
}
```

### Adding an Adjustment Type

1. **Add enum value** (`code/layer.h`):
   ```cpp
   enum class AdjustmentType {
       // ... existing ...
       MyAdjustment,
   };
   ```

2. **Create parameter struct** (`code/layer.h`):
   ```cpp
   struct MyAdjustmentParams {
       f32 amount = 0.0f;
       bool enabled = true;
   };
   ```

3. **Add to variant** (`code/layer.h`):
   ```cpp
   using AdjustmentParams = std::variant<
       // ... existing ...
       MyAdjustmentParams
   >;
   ```

4. **Set defaults** (`code/layer.cpp`):
   ```cpp
   void AdjustmentLayer::setDefaultParams() {
       switch (type) {
           // ... existing ...
           case AdjustmentType::MyAdjustment:
               params = MyAdjustmentParams();
               break;
       }
   }
   ```

5. **Apply effect** (`code/compositor.cpp`):
   ```cpp
   u32 applyAdjustment(u32 pixel, const AdjustmentLayer& adj) {
       switch (adj.type) {
           // ... existing ...
           case AdjustmentType::MyAdjustment: {
               auto* p = std::get_if<MyAdjustmentParams>(&adj.params);
               // modify pixel based on p->amount
               break;
           }
       }
   }
   ```

6. **Add UI** (`code/panels.cpp` in `LayerPropsPanel`):
   ```cpp
   void LayerPropsPanel::buildMyAdjustmentControls(AdjustmentLayer* layer) {
       auto* params = getAdjustmentParams<MyAdjustmentParams>(layer);
       auto* slider = addSliderRow("Amount", -100, 100, params->amount);
       slider->onChanged = [this, layer, params](f32 v) {
           params->amount = v;
           notifyAdjustmentChanged(layer);
       };
   }
   ```

---

## Undo/Redo System

The undo system uses a hybrid approach: **tile deltas** for pixel operations (memory efficient) and **layer snapshots** for structural changes.

### Key Files
- `code/undo.h` - Data structures: `UndoStep`, `TileDelta`, `LayerSnapshot`, `UndoHistory`
- `code/undo.cpp` - `UndoHistory` implementation
- `code/document.cpp` - Undo/redo methods integrated into Document

### Undo Step Types

| Type | Used For | Storage |
|------|----------|---------|
| `PixelEdit` | Brush, eraser, fill, retouch tools | Only changed tiles (before/after) |
| `LayerAdd` | Adding layers | Layer index (layer captured on undo) |
| `LayerRemove` | Deleting layers | Full layer clone |
| `SelectionChange` | Selection modifications | Selection mask copy |

### Adding Undo to a Pixel Tool

Tools that modify pixels follow this pattern:

```cpp
void MyTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // 1. Begin undo step
    doc.beginPixelUndo("My Tool", doc.activeLayerIndex);

    // 2. Capture tiles BEFORE modifying them
    f32 size = getAppState().brushSize;
    Recti affectedRect(
        static_cast<i32>(e.position.x - size),
        static_cast<i32>(e.position.y - size),
        static_cast<i32>(size * 2) + 1,
        static_cast<i32>(size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, affectedRect);

    // 3. Now modify pixels...
    doMyOperation(layer->canvas, e.position);
}

void MyTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    // Capture tiles along stroke path before modifying
    Recti strokeRect(...);  // Calculate bounding rect of stroke segment
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, strokeRect);

    // Modify pixels...
}

void MyTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // 4. Commit the undo step
    doc.commitUndo();
}
```

### Key Methods

| Method | Purpose |
|--------|---------|
| `doc.beginPixelUndo(name, layerIndex)` | Start a new pixel edit undo step |
| `doc.captureOriginalTilesInRect(layerIndex, bounds)` | Capture tiles before modification |
| `doc.captureOriginalTile(layerIndex, tileKey)` | Capture a single tile by key |
| `doc.commitUndo()` | Finalize and push to undo stack |
| `doc.cancelUndo()` | Discard pending undo step |
| `doc.recordLayerAdd(index)` | Record layer addition (called automatically) |
| `doc.recordLayerRemove(index)` | Record layer removal (called automatically) |
| `doc.recordSelectionChange(name)` | Record selection change |

### Selection Undo

For selection tools, record the selection state BEFORE modifying:

```cpp
void MySelectionTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // Record current selection before changing it
    doc.recordSelectionChange("My Selection");

    // Now modify selection...
    doc.selection.setRectangle(myRect);
    doc.notifySelectionChanged();
}
```

### Layer Operations

Layer add/remove undo is automatic - `addLayer()` and `removeLayer()` call the appropriate record functions internally.

### Memory Considerations

- Tiles are 64×64 pixels × 4 bytes = 16KB each
- With 20 undo steps and typical brush strokes (10-50 tiles), expect 3-16MB per document
- Large operations (full canvas fill) capture all tiles - can use significant memory
- Undo history is per-document and cleared when document closes

---

## Common Pitfalls

### 1. Forgetting Unity Build Include
**Symptom**: Linker errors about undefined symbols
**Fix**: Add `#include "newfile.cpp"` to `code/main.cpp`

### 2. Wrong Include Order
**Symptom**: Compilation errors about unknown types
**Fix**: Ensure dependencies are included first in `code/main.cpp`

### 3. Using Wrong Type Aliases
**Symptom**: Style inconsistency
**Fix**: Use `u32`, `f32`, etc. instead of `unsigned int`, `float`

### 4. Modifying Widgets During Event Handlers
**Symptom**: Crashes or use-after-free
**Fix**: Use deferred operations:
```cpp
// WRONG
button->onClick = [this]() {
    delete someWidget;  // Might still be in use
};

// CORRECT
button->onClick = [this]() {
    getAppState().pendingOperation = true;
    // Handle in application main loop
};
```

### 5. Not Requesting Redraw
**Symptom**: UI doesn't update after state change
**Fix**: Set the dirty flag:
```cpp
getAppState().needsRedraw = true;
```

### 6. Ignoring DPI Scaling
**Symptom**: UI too small on HiDPI displays
**Fix**: Always multiply by `Config::uiScale`:
```cpp
preferredSize = Vec2(100 * Config::uiScale, 30 * Config::uiScale);
```

### 7. Breaking Menu Close Behavior
**Symptom**: Menus don't close properly
**Fix**: Always call `closeActiveMenu()` in menu item callbacks:
```cpp
menu->addItem("Action", "", [this]() {
    closeActiveMenu();  // REQUIRED
    doSomething();
});
```

---

## File Modification Checklist

### When Modifying Headers
- [ ] Check if changes affect other files that include this header
- [ ] Rebuild and test (unity build recompiles everything)
- [ ] Update forward declarations if adding new types

### When Adding New Files
- [ ] Create both `.h` and `.cpp` in `code/` directory
- [ ] Use correct include guard format: `_H_FILENAME_`
- [ ] Add to `code/main.cpp` unity build block
- [ ] Place in correct order (after dependencies)
- [ ] Rebuild and test

### When Changing Config Values
- [ ] Check if value needs DPI scaling
- [ ] Update both constant and accessor if applicable
- [ ] Test at different UI scales

### When Adding UI Elements
- [ ] Use `Config::uiScale` for all dimensions
- [ ] Set appropriate `SizePolicy`
- [ ] Handle `enabled` state properly
- [ ] Call `requestRedraw()` or set `needsRedraw = true`

---

## Debugging Tips

### Build Errors
1. Check `code/main.cpp` for missing includes
2. Verify include guard format
3. Check for circular dependencies

### Runtime Crashes
1. Build with debug: `./build_linux.sh debug`
2. Run with gdb: `gdb ./pixelplacer_debug`
3. Common causes:
   - Null pointer dereference (check `activeDocument`)
   - Widget deleted during event handling
   - Out of bounds tile access

### Visual Issues
1. Check clipping: `fb.pushClip()` / `fb.popClip()` balanced?
2. Verify coordinates: local vs global?
3. Check visibility flags: `visible`, `enabled`

### Performance Issues
1. Check `getTileCount()` and `getMemoryUsage()`
2. Look for unnecessary redraws
3. Profile compositing for large documents

---

## Platform-Specific Code

The application supports three platforms: Linux (X11), Windows (Win32/GDI), and WebAssembly (browser).

### Platform Abstraction

- `PlatformWindow` - abstract interface in `code/platform_window.h`
- `Platform::` namespace - utility functions in `code/platform.h`
- Platform implementations selected via `#ifdef` in `code/main.cpp`

### Writing Platform-Specific Code

Use preprocessor checks for platform-specific code:

```cpp
#ifdef __EMSCRIPTEN__
    // WASM-specific: use memory-based file loading
    std::vector<u8> fileData = Platform::readFile(path);
    data = stbi_load_from_memory(fileData.data(), fileData.size(), &w, &h, &channels, 4);
#elif defined(_WIN32)
    // Windows-specific code here
    data = stbi_load(path.c_str(), &w, &h, &channels, 4);
#else
    // Linux: direct filesystem access
    data = stbi_load(path.c_str(), &w, &h, &channels, 4);
#endif
```

### WASM Considerations

1. **No filesystem**: Files come from JavaScript via `Platform::readFile()`. File dialogs trigger browser file picker, data is passed to WASM memory.

2. **Async with ASYNCIFY**: Emscripten's ASYNCIFY allows sync-looking code to await JS promises. Used for file dialogs and clipboard.

3. **File save = download**: `Platform::writeFile()` with `__download__:` prefix triggers browser download.

4. **Event handling**: Browser events are queued via JavaScript and processed in C++ main loop. See `code/shell.html` for JS event bindings.

5. **Canvas rendering**: Framebuffer is copied to HTML canvas via `js_render_frame()` in shell.html.

### Adding Platform-Specific Features

1. Add function declaration to `code/platform.h`
2. Implement in all platform files: `code/platform_linux.cpp`, `code/platform_windows.cpp`, and `code/platform_wasm.cpp`
3. For WASM, may need JavaScript interop via `EM_ASM` or exported functions
4. For Windows, use Win32 APIs (file dialogs via GetOpenFileName, clipboard via OpenClipboard, etc.)

---

## Architecture Quick Reference

### Event Flow
```
Platform Event (X11/Win32/Browser) -> PlatformWindow callback -> Application handler
    -> OverlayManager (popups/dialogs)
    -> Widget tree (findWidgetAt + bubble up)
    -> Tool (if on canvas)
```

### Rendering Flow
```
Application::render()
    -> Widget::render() (recursive)
        -> Widget::renderSelf() (custom drawing)
        -> Widget::renderChildren()
    -> Compositor::compositeDocument() (canvas)
    -> OverlayManager::renderOverlays() (popups)
```

### Document Structure
```
Document
    -> layers: vector<unique_ptr<LayerBase>>
        -> PixelLayer (contains TiledCanvas)
        -> TextLayer (contains text + cache)
        -> AdjustmentLayer (contains params)
    -> selection: Selection
    -> currentTool: unique_ptr<Tool>
    -> undoHistory: UndoHistory (20 steps max)
```

### Widget Ownership
```
MainWindow (owns all UI)
    -> MenuBar, ToolPalette, ToolOptionsBar, StatusBar
    -> DocumentViewWidget (canvas display)
    -> NavigatorPanel, LayerPropsPanel, LayerPanel
    -> Dialogs (NewDocumentDialog, etc.)
```

---

## Testing Changes

After any modification:

1. **Build**: `./build_linux.sh`
2. **Run**: `./pixelplacer`
3. **Test affected functionality**:
   - For tool changes: use the tool on canvas
   - For UI changes: interact with modified widgets
   - For document changes: create/modify/save documents
   - For layer changes: add/remove/modify layers

4. **Test edge cases**:
   - Empty document (no layers)
   - Very small canvas (1x1)
   - Very large canvas (4096x4096)
   - Rapid clicking/dragging
   - Window resize during operation
