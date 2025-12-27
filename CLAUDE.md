# CLAUDE.md - Agent Instructions for PixelPlacer

This file contains instructions for AI agents (Claude Code, Cursor, etc.) working on the PixelPlacer codebase.

## Quick Reference

### Build and Test
```bash
./build_linux.sh        # Release build -> ./pixelplacer
./build_linux.sh debug  # Debug build -> ./pixelplacer_debug
```

### Key Directories
- All source files are in the root directory (no subdirectories)
- Headers: `*.h`
- Implementations: `*.cpp`
- Build output: `pixelplacer`, `pixelplacer_debug`

### Most Frequently Modified Files
| Task | Primary Files |
|------|--------------|
| UI changes | `main_window.cpp`, `panels.cpp`, `basic_widgets.cpp` |
| Tool behavior | `tool.cpp`, `brush_tool.cpp`, `selection_tools.cpp` |
| Document operations | `document.cpp`, `layer.h` |
| Rendering | `compositor.cpp`, `framebuffer.cpp` |
| Configuration | `config.h` |
| Application lifecycle | `application.cpp` |

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

1. **Only `main.cpp` is compiled** - all other `.cpp` files are `#include`d
2. **Adding a new .cpp file requires updating `main.cpp`**:
   ```cpp
   #ifdef UNITY_BUILD
   // ... existing includes ...
   #include "my_new_file.cpp"  // ADD THIS
   #endif
   ```
3. **Order matters** - files are included in dependency order
4. **No separate compilation** - changes to any file trigger full rebuild

### When Adding New Files

1. Create both `.h` and `.cpp` files
2. Add `#include "myfile.cpp"` to `main.cpp` in the `UNITY_BUILD` block
3. Place include after any dependencies
4. Test build immediately

---

## Common Tasks

### Adding a New Tool

1. **Create files**: `my_tool.h`, `my_tool.cpp`

2. **Add to ToolType enum** (`tool.h`):
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

4. **Add to unity build** (`main.cpp`):
   ```cpp
   #include "my_tool.cpp"
   ```

5. **Add toolbar button** (`main_window.cpp` in `ToolPalette` constructor):
   ```cpp
   addToolButton(ToolType::MyTool, "icon_name", "My Tool");
   ```

6. **Add tool options** (`main_window.cpp` in `ToolOptionsBar::rebuildOptions()`):
   ```cpp
   case ToolType::MyTool:
       buildMyToolOptions();
       break;
   ```

### Adding a New Widget

1. **Add to `basic_widgets.h`** or create new header:
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

2. **Implement in `basic_widgets.cpp`** or new `.cpp`:
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

In `main_window.cpp`, find the appropriate `create*Menu()` function:

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

1. **Add enum value** (`layer.h`):
   ```cpp
   enum class AdjustmentType {
       // ... existing ...
       MyAdjustment,
   };
   ```

2. **Create parameter struct** (`layer.h`):
   ```cpp
   struct MyAdjustmentParams {
       f32 amount = 0.0f;
       bool enabled = true;
   };
   ```

3. **Add to variant** (`layer.h`):
   ```cpp
   using AdjustmentParams = std::variant<
       // ... existing ...
       MyAdjustmentParams
   >;
   ```

4. **Set defaults** (`layer.cpp`):
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

5. **Apply effect** (`compositor.cpp`):
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

6. **Add UI** (`panels.cpp` in `LayerPropsPanel`):
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

## Common Pitfalls

### 1. Forgetting Unity Build Include
**Symptom**: Linker errors about undefined symbols
**Fix**: Add `#include "newfile.cpp"` to `main.cpp`

### 2. Wrong Include Order
**Symptom**: Compilation errors about unknown types
**Fix**: Ensure dependencies are included first in `main.cpp`

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
- [ ] Create both `.h` and `.cpp`
- [ ] Use correct include guard format: `_H_FILENAME_`
- [ ] Add to `main.cpp` unity build block
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
1. Check `main.cpp` for missing includes
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

## Architecture Quick Reference

### Event Flow
```
X11 Event -> PlatformWindow callback -> Application handler
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
