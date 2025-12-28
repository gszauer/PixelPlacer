#include "application.h"
#include "main_window.h"
#include "basic_widgets.h"
#include "overlay_manager.h"
#include "platform.h"
#include "keycodes.h"
#include <cstdio>


Application::Application() = default;
Application::~Application() = default;

bool Application::initialize(u32 width, u32 height, const char* title) {
    // Create platform window
    // Pass 0,0 for auto-sizing (half screen, min 1280x800, centered)
    window = Platform::createWindow();
    if (!window || !window->create(width, height, title)) {
        fprintf(stderr, "Failed to create window\n");
        return false;
    }

    // Remove system decorations - we render our own title bar
    window->setDecorated(false);

    // Get actual window size (may have been auto-calculated)
    windowWidth = window->getWidth();
    windowHeight = window->getHeight();

    // Get DPI scale from platform window
    dpiScale = window->getDpiScale();
    drawableWidth = windowWidth;
    drawableHeight = windowHeight;

    framebuffer.resize(drawableWidth, drawableHeight);

    // Load default font
    loadDefaultFont();

    // Create main window layout
    createMainWindow();

    // Create initial document
    getAppState().createDocument(Config::DEFAULT_CANVAS_WIDTH, Config::DEFAULT_CANVAS_HEIGHT);

    // Sync the main window with the initial document
    if (auto* mainWindow = dynamic_cast<MainWindow*>(rootWidget.get())) {
        mainWindow->connectToDocument();
    }

    // Set up drag and drop file handling
    window->onFileDrop = [this](const std::string& path) {
        AppState& state = getAppState();
        std::unique_ptr<Document> doc;

        // Load based on file extension
        if (Platform::getFileExtension(path) == ".pp") {
            doc = ProjectFile::load(path);
        } else {
            doc = ImageIO::loadAsDocument(path);
        }

        if (doc) {
            doc->filePath = path;
            doc->name = Platform::getFileName(path);

            // Register embedded fonts with FontRenderer
            for (const auto& [fontName, fontData] : doc->embeddedFonts) {
                FontRenderer::instance().loadCustomFont(fontName, fontData.data(), static_cast<i32>(fontData.size()));
            }

            state.documents.push_back(std::move(doc));
            state.activeDocument = state.documents.back().get();

            if (auto* mainWindow = dynamic_cast<MainWindow*>(rootWidget.get())) {
                mainWindow->syncTabs();
                // Select the newly added tab
                if (mainWindow->tabBar) {
                    mainWindow->tabBar->setActiveTab(static_cast<i32>(mainWindow->tabBar->tabs.size()) - 1);
                }
                mainWindow->connectToDocument();
            }

            state.needsRedraw = true;
        }
    };

    // Set up event callbacks
    window->onCloseRequested = [this]() {
        getAppState().running = false;
    };

    window->onKeyDown = [this](i32 keyCode, i32 scanCode, KeyMods mods, bool repeat) {
        currentMods = mods;
        handleKeyDown(keyCode, scanCode, repeat);
    };

    window->onKeyUp = [this](i32 keyCode, i32 scanCode, KeyMods mods) {
        currentMods = mods;
        handleKeyUp(keyCode, scanCode);
    };

    window->onTextInput = [this](const char* text) {
        handleTextInput(text);
    };

    window->onMouseDown = [this](i32 x, i32 y, MouseButton button) {
        handleMouseDown(x, y, button);
    };

    window->onMouseUp = [this](i32 x, i32 y, MouseButton button) {
        handleMouseUp(x, y, button);
    };

    window->onMouseMove = [this](i32 x, i32 y) {
        handleMouseMove(x, y);
    };

    window->onMouseWheel = [this](i32 x, i32 y, i32 deltaY) {
        handleMouseWheel(x, y, deltaY);
    };

    window->onResize = [this](u32 w, u32 h) {
        handleWindowResize(w, h);
    };

    window->onExpose = [this]() {
        getAppState().needsRedraw = true;
    };

    initialized = true;
    return true;
}

void Application::loadDefaultFont() {
    // Load embedded Inter font as default
    extern const u8 inter_ttf[];
    extern const u32 inter_ttf_size;
    FontRenderer::instance().loadFont(inter_ttf, inter_ttf_size);
    fprintf(stderr, "Loaded Inter font (%u bytes, embedded)\n", inter_ttf_size);

    // Load Material Icons font for UI icons (embedded)
    extern const u8 material_ttf[];
    extern const u32 material_ttf_size;
    if (FontRenderer::instance().loadCustomFont("Material Icons", material_ttf, material_ttf_size)) {
        fprintf(stderr, "Loaded Material Icons font (%u bytes, embedded)\n", material_ttf_size);
    }
}

void Application::createMainWindow() {
    auto mainWindow = std::make_unique<MainWindow>();
    mainWindow->setBounds(0, 0, drawableWidth, drawableHeight);
    mainWindow->layout();

    // Connect UI scale change callback (deferred to avoid destroying widgets during click)
    if (mainWindow->statusBar) {
        mainWindow->statusBar->onScaleChanged = [](f32 newScale) {
            getAppState().requestScaleChange(newScale);
        };
    }

    // Connect window control callbacks in menu bar
    if (mainWindow->menuBar) {
        mainWindow->menuBar->onWindowDrag = [this](i32 rootX, i32 rootY) {
            window->startDrag(rootX, rootY);
        };
        mainWindow->menuBar->onWindowMinimize = [this]() {
            window->minimize();
        };
        mainWindow->menuBar->onWindowMaximize = [this]() {
            window->toggleMaximize();
            // Update the maximize button icon
            if (auto* mw = dynamic_cast<MainWindow*>(rootWidget.get())) {
                if (mw->menuBar) mw->menuBar->updateMaximizeButton();
            }
            getAppState().needsRedraw = true;
        };
        mainWindow->menuBar->onWindowClose = [this]() {
            getAppState().running = false;
        };
        mainWindow->menuBar->isWindowMaximized = [this]() {
            return window->isMaximized();
        };
    }

    setRootWidget(std::move(mainWindow));
}

void Application::rebuildUIWithScale(f32 newScale) {
    // Update the global UI scale
    Config::uiScale = newScale;

    // Store current document to reconnect after rebuild
    Document* activeDoc = getAppState().activeDocument;

    // Rebuild the entire UI with the new scale
    createMainWindow();

    // Reconnect to the active document
    if (auto* mainWindow = dynamic_cast<MainWindow*>(rootWidget.get())) {
        mainWindow->connectToDocument();

        // Update the scale slider to reflect current scale
        if (mainWindow->statusBar && mainWindow->statusBar->scaleSlider) {
            mainWindow->statusBar->scaleSlider->setValue(newScale);
        }
    }

    getAppState().needsRedraw = true;
}

void Application::updateDPIScale() {
    if (windowWidth > 0 && windowHeight > 0) {
        dpiScale = static_cast<f32>(drawableWidth) / static_cast<f32>(windowWidth);
    } else {
        dpiScale = 1.0f;
    }
}

void Application::run() {
    if (!initialized) return;

    AppState& state = getAppState();

    while (state.running) {
        window->processEvents();

        // Apply deferred UI changes (must be after events, before render)
        if (auto* mainWindow = dynamic_cast<MainWindow*>(rootWidget.get())) {
            mainWindow->applyDeferredChanges();
        }

        // Process deferred file dialog (must be outside event handling)
        // Wait for mouse button release to ensure implicit grab is released
        if (state.pendingFileDialog.active && !state.mouseDown) {
            state.pendingFileDialog.active = false;
            std::string path;
            if (state.pendingFileDialog.isSaveDialog) {
                path = Platform::saveFileDialog(
                    state.pendingFileDialog.title.c_str(),
                    state.pendingFileDialog.defaultName.c_str(),
                    state.pendingFileDialog.filters.c_str()
                );
            } else {
                path = Platform::openFileDialog(
                    state.pendingFileDialog.title.c_str(),
                    state.pendingFileDialog.filters.c_str()
                );
            }
            if (state.pendingFileDialog.callback) {
                state.pendingFileDialog.callback(path);
            }
            state.needsRedraw = true;
        }

        // Process deferred UI scale change (must be outside event handling to avoid use-after-free)
        if (state.pendingScaleChange) {
            state.pendingScaleChange = false;
            rebuildUIWithScale(state.pendingScaleValue);
        }

        // Force redraw if there's an active selection (for marching ants animation)
        if (state.activeDocument && state.activeDocument->selection.hasSelection) {
            state.needsRedraw = true;
        }

        if (state.needsRedraw) {
            render();
            present();
            state.needsRedraw = false;
        }

        // Small delay to prevent 100% CPU usage
        // Use longer delay when not animating to save power
        if (state.activeDocument && state.activeDocument->selection.hasSelection) {
            Platform::sleepMs(16);  // ~60 FPS for animation
        } else {
            Platform::sleepMs(1);
        }
    }
}

void Application::shutdown() {
    rootWidget.reset();
    window.reset();  // Platform window destructor handles cleanup
}

void Application::handleKeyDown(i32 keyCode, i32 scanCode, bool repeat) {
    AppState& state = getAppState();

    // Space key for temporary pan
    if (keyCode == Key::SPACE) {
        state.spaceHeld = true;
    }

    KeyEvent e;
    e.keyCode = keyCode;
    e.scanCode = scanCode;
    e.mods = currentMods;
    e.repeat = repeat;

    // First try focused widget
    if (state.focusedWidget && state.focusedWidget->onKeyDown(e)) {
        state.needsRedraw = true;
        return;
    }

    // Then root widget
    if (rootWidget && rootWidget->onKeyDown(e)) {
        state.needsRedraw = true;
    }
}

void Application::handleKeyUp(i32 keyCode, i32 scanCode) {
    AppState& state = getAppState();

    if (keyCode == Key::SPACE) {
        state.spaceHeld = false;
    }

    KeyEvent e;
    e.keyCode = keyCode;
    e.scanCode = scanCode;
    e.mods = currentMods;

    if (state.focusedWidget) {
        state.focusedWidget->onKeyUp(e);
    }
    if (rootWidget) {
        rootWidget->onKeyUp(e);
    }
}

// Returns resize direction (0-7) or -1 if not on an edge
i32 Application::getResizeDirection(i32 x, i32 y) const {
    const i32 borderSize = static_cast<i32>(5 * Config::uiScale);
    i32 w = static_cast<i32>(drawableWidth);
    i32 h = static_cast<i32>(drawableHeight);

    bool onLeft = x < borderSize;
    bool onRight = x >= w - borderSize;
    bool onTop = y < borderSize;
    bool onBottom = y >= h - borderSize;

    if (onTop && onLeft) return PlatformWindow::RESIZE_TOPLEFT;
    if (onTop && onRight) return PlatformWindow::RESIZE_TOPRIGHT;
    if (onBottom && onLeft) return PlatformWindow::RESIZE_BOTTOMLEFT;
    if (onBottom && onRight) return PlatformWindow::RESIZE_BOTTOMRIGHT;
    if (onTop) return PlatformWindow::RESIZE_TOP;
    if (onBottom) return PlatformWindow::RESIZE_BOTTOM;
    if (onLeft) return PlatformWindow::RESIZE_LEFT;
    if (onRight) return PlatformWindow::RESIZE_RIGHT;

    return -1;  // Not on an edge
}

void Application::handleMouseDown(i32 x, i32 y, MouseButton button) {
    AppState& state = getAppState();

    // Check for window resize edges first (only with left button, not when maximized)
    if (button == MouseButton::Left && !window->isMaximized()) {
        i32 resizeDir = getResizeDirection(x, y);
        if (resizeDir >= 0) {
            window->startResize(resizeDir);
            return;
        }
    }

    state.mouseDown = true;
    state.mouseButton = button;
    state.mousePosition = scaleMouseCoords(x, y);

    MouseEvent e;
    e.position = state.mousePosition;
    e.globalPosition = state.mousePosition;
    e.button = button;
    e.mods = currentMods;

    // If there's a captured widget, send event there
    if (state.capturedWidget) {
        Vec2 local = state.capturedWidget->globalToLocal(e.globalPosition);
        e.position = local;
        state.capturedWidget->onMouseDown(e);
        state.needsRedraw = true;
        return;
    }

    // Route through overlay manager first
    if (OverlayManager::instance().routeMouseDown(e)) {
        state.needsRedraw = true;
        return;
    }

    // Otherwise, find widget under mouse and bubble up until handled
    if (rootWidget) {
        Widget* target = rootWidget->findWidgetAt(e.position);
        while (target) {
            // Update focus if widget is focusable and enabled
            if (target->focusable && target->enabled && target != state.focusedWidget) {
                setFocus(target);
            }

            Vec2 local = target->globalToLocal(e.globalPosition);
            MouseEvent localEvent = e;
            localEvent.position = local;
            if (target->onMouseDown(localEvent)) {
                state.needsRedraw = true;
                return;  // Event was handled
            }
            // Bubble up to parent
            target = target->parent;
        }
        state.needsRedraw = true;
    }
}

void Application::handleMouseUp(i32 x, i32 y, MouseButton button) {
    AppState& state = getAppState();

    state.mouseDown = false;
    state.mousePosition = scaleMouseCoords(x, y);

    MouseEvent e;
    e.position = state.mousePosition;
    e.globalPosition = state.mousePosition;
    e.button = button;
    e.mods = currentMods;

    if (state.capturedWidget) {
        Vec2 local = state.capturedWidget->globalToLocal(e.globalPosition);
        e.position = local;
        state.capturedWidget->onMouseUp(e);
        state.needsRedraw = true;
        return;
    }

    // Route through overlay manager first
    if (OverlayManager::instance().routeMouseUp(e)) {
        state.needsRedraw = true;
        return;
    }

    if (rootWidget) {
        Widget* target = rootWidget->findWidgetAt(e.position);
        while (target) {
            Vec2 local = target->globalToLocal(e.globalPosition);
            MouseEvent localEvent = e;
            localEvent.position = local;
            if (target->onMouseUp(localEvent)) {
                state.needsRedraw = true;
                return;
            }
            target = target->parent;
        }
        state.needsRedraw = true;
    }
}

void Application::handleMouseMove(i32 x, i32 y) {
    AppState& state = getAppState();

    // Update cursor based on resize edge detection (only when not maximized)
    static i32 currentCursor = PlatformWindow::CURSOR_DEFAULT;
    i32 newCursor = window->isMaximized() ? PlatformWindow::CURSOR_DEFAULT : getResizeDirection(x, y);
    if (newCursor != currentCursor) {
        window->setCursor(newCursor);
        currentCursor = newCursor;
    }

    Vec2 newPos = scaleMouseCoords(x, y);
    Vec2 delta = newPos - state.mousePosition;
    state.mousePosition = newPos;

    MouseEvent e;
    e.position = state.mousePosition;
    e.globalPosition = state.mousePosition;
    e.button = state.mouseButton;
    e.mods = currentMods;

    if (state.capturedWidget) {
        Vec2 local = state.capturedWidget->globalToLocal(e.globalPosition);
        e.position = local;
        if (state.mouseDown) {
            state.capturedWidget->onMouseDrag(e);
        } else {
            state.capturedWidget->onMouseMove(e);
        }
        state.needsRedraw = true;
        return;
    }

    // Route through overlay manager (for hover tracking on overlays)
    bool modalBlocking = OverlayManager::instance().hasBlockingModal();

    // Route drag events to overlays when modal is blocking
    if (modalBlocking) {
        if (state.mouseDown) {
            OverlayManager::instance().routeMouseDrag(e);
        } else {
            OverlayManager::instance().routeMouseMove(e);
        }
        // Clear hover state on background widgets
        state.hoveredWidget = nullptr;
        state.needsRedraw = true;
        return;
    }

    OverlayManager::instance().routeMouseMove(e);

    if (rootWidget) {
        if (state.mouseDown) {
            Widget* target = rootWidget->findWidgetAt(e.globalPosition);
            while (target) {
                Vec2 local = target->globalToLocal(e.globalPosition);
                MouseEvent dragEvent = e;
                dragEvent.position = local;
                if (target->onMouseDrag(dragEvent)) {
                    break;  // Event was handled
                }
                target = target->parent;
            }
        }
        // Always update hover state with global coordinates
        rootWidget->onMouseMove(e);
        state.needsRedraw = true;
    }
}

void Application::handleMouseWheel(i32 x, i32 y, i32 deltaY) {
    AppState& state = getAppState();

    // Block wheel events when a modal is open
    if (OverlayManager::instance().hasBlockingModal()) {
        return;
    }

    MouseEvent e;
    e.position = scaleMouseCoords(x, y);
    e.globalPosition = e.position;
    e.wheelDelta = deltaY;
    e.mods = currentMods;

    // Find widget under mouse and bubble up until handled
    if (rootWidget) {
        Widget* target = rootWidget->findWidgetAt(e.position);
        while (target) {
            Vec2 local = target->globalToLocal(e.globalPosition);
            MouseEvent localEvent = e;
            localEvent.position = local;
            if (target->onMouseWheel(localEvent)) {
                state.needsRedraw = true;
                return;
            }
            target = target->parent;
        }
        state.needsRedraw = true;
    }
}

void Application::handleTextInput(const char* text) {
    AppState& state = getAppState();

    if (state.focusedWidget) {
        if (state.focusedWidget->onTextInput(std::string(text))) {
            state.needsRedraw = true;
        }
    }
}

void Application::handleWindowResize(i32 width, i32 height) {
    if (width <= 0 || height <= 0) return;

    windowWidth = width;
    windowHeight = height;

    // For X11, drawable size equals window size
    drawableWidth = width;
    drawableHeight = height;

    framebuffer.resize(drawableWidth, drawableHeight);

    // Update root widget bounds
    if (rootWidget) {
        rootWidget->setBounds(0, 0, drawableWidth, drawableHeight);
        rootWidget->layout();
    }

    getAppState().needsRedraw = true;
}

void Application::render() {
    // Clear to background color
    framebuffer.clear(Config::COLOR_BACKGROUND);

    // Render widget tree
    if (rootWidget) {
        rootWidget->render(framebuffer);
    }

    // Render overlays on top (popups, dropdowns, dialogs)
    OverlayManager::instance().renderOverlays(framebuffer);
}

void Application::present() {
    window->present(framebuffer.data(), framebuffer.width, framebuffer.height);
}

void Application::setTitle(const std::string& title) {
    window->setTitle(title.c_str());
}

Vec2 Application::getWindowSize() const {
    return Vec2(static_cast<f32>(windowWidth), static_cast<f32>(windowHeight));
}

void Application::resize(u32 width, u32 height) {
    window->resize(width, height);
    handleWindowResize(width, height);
}

void Application::setRootWidget(std::unique_ptr<Widget> root) {
    rootWidget = std::move(root);
    if (rootWidget) {
        rootWidget->setBounds(0, 0, drawableWidth, drawableHeight);
        rootWidget->layout();
    }
}

void Application::setFocus(Widget* widget) {
    AppState& state = getAppState();

    if (state.focusedWidget == widget) return;

    if (state.focusedWidget) {
        state.focusedWidget->onBlur();
    }

    state.focusedWidget = widget;

    if (widget) {
        widget->onFocus();
    }
}

void Application::captureMouse(Widget* widget) {
    getAppState().capturedWidget = widget;
}

void Application::releaseMouse() {
    getAppState().capturedWidget = nullptr;
}
