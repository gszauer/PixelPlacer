#include "win32_window.h"
#include "keycodes.h"
#include <windowsx.h>
#include <algorithm>
#include <cstdio>

// Static member
bool Win32Window::classRegistered = false;

void Win32Window::registerWindowClass() {
    if (classRegistered) return;

    // Enable DPI awareness (Per-Monitor V2 on Win10 1703+, Per-Monitor on Win8.1+)
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(HANDLE);
    auto pSetDpiContext = reinterpret_cast<SetProcessDpiAwarenessContextProc>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"));
    if (pSetDpiContext) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ((DPI_AWARENESS_CONTEXT)-4)
        pSetDpiContext(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)));
    } else {
        // Fallback for older Windows versions
        typedef HRESULT (WINAPI *SetProcessDpiAwarenessProc)(int);
        HMODULE shcore = LoadLibraryA("shcore.dll");
        if (shcore) {
            auto pSetDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessProc>(
                GetProcAddress(shcore, "SetProcessDpiAwareness"));
            if (pSetDpiAwareness) {
                pSetDpiAwareness(2);  // PROCESS_PER_MONITOR_DPI_AWARE
            }
        }
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS;
    wc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconA(nullptr, IDI_APPLICATION);

    if (RegisterClassExW(&wc)) {
        classRegistered = true;
    } else {
        DWORD err = GetLastError();
        // ERROR_CLASS_ALREADY_EXISTS is OK
        if (err == ERROR_CLASS_ALREADY_EXISTS) {
            classRegistered = true;
        } else {
            fprintf(stderr, "Failed to register window class (error %lu)\n", err);
        }
    }
}

bool Win32Window::create(u32 w, u32 h, const char* title) {
    registerWindowClass();

    if (!classRegistered) {
        fprintf(stderr, "Window class not registered\n");
        return false;
    }

    // Get DPI scale before creating window
    updateDpiScale();

    // Auto-calculate window size if w or h is 0
    if (w == 0 || h == 0) {
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        u32 screenW = workArea.right - workArea.left;
        u32 screenH = workArea.bottom - workArea.top;
        constexpr u32 MIN_WIDTH = 1280;
        constexpr u32 MIN_HEIGHT = 800;
        w = (std::max)(MIN_WIDTH, screenW / 2);
        h = (std::max)(MIN_HEIGHT, screenH / 2);
    }

    // Window style for custom title bar with resize support
    // WS_POPUP: No default title bar
    // WS_THICKFRAME: Enables resize and Aero Snap
    // WS_MINIMIZEBOX | WS_MAXIMIZEBOX: Enable minimize/maximize buttons (for Aero Snap)
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    DWORD exStyle = WS_EX_APPWINDOW;

    // Convert title to wide string
    i32 titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring wideTitle(titleLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], titleLen);

    // Calculate center position (CW_USEDEFAULT doesn't work with WS_POPUP)
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    i32 screenW = workArea.right - workArea.left;
    i32 screenH = workArea.bottom - workArea.top;
    i32 posX = workArea.left + (screenW - static_cast<i32>(w)) / 2;
    i32 posY = workArea.top + (screenH - static_cast<i32>(h)) / 2;

    // Create window
    hwnd = CreateWindowExW(
        exStyle,
        WINDOW_CLASS,
        wideTitle.c_str(),
        style,
        posX, posY,
        static_cast<i32>(w), static_cast<i32>(h),
        nullptr, nullptr,
        GetModuleHandle(nullptr),
        this  // Pass this pointer for WndProc
    );

    if (!hwnd) {
        DWORD err = GetLastError();
        fprintf(stderr, "Failed to create window (error %lu)\n", err);
        return false;
    }

    // Get actual client size
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    width = clientRect.right;
    height = clientRect.bottom;

    // Get window DC
    hdcWindow = GetDC(hwnd);
    if (!hdcWindow) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
        return false;
    }

    // Create back buffer
    if (!createBackBuffer(width, height)) {
        ReleaseDC(hwnd, hdcWindow);
        DestroyWindow(hwnd);
        hwnd = nullptr;
        hdcWindow = nullptr;
        return false;
    }

    // Show window (already centered during creation)
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Enable drag and drop file support
    DragAcceptFiles(hwnd, TRUE);

    // Set high-resolution timer for smoother input
    timeBeginPeriod(1);

    return true;
}

void Win32Window::destroy() {
    // Restore default timer resolution
    timeEndPeriod(1);

    destroyBackBuffer();

    if (hdcWindow && hwnd) {
        ReleaseDC(hwnd, hdcWindow);
        hdcWindow = nullptr;
    }

    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

void Win32Window::setTitle(const char* title) {
    if (!hwnd) return;

    i32 len = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring wideTitle(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], len);
    SetWindowTextW(hwnd, wideTitle.c_str());
}

void Win32Window::resize(u32 w, u32 h) {
    if (!hwnd) return;
    SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    width = w;
    height = h;
}

void Win32Window::getScreenSize(u32& outWidth, u32& outHeight) const {
    outWidth = static_cast<u32>(GetSystemMetrics(SM_CXSCREEN));
    outHeight = static_cast<u32>(GetSystemMetrics(SM_CYSCREEN));
}

void Win32Window::setMinSize(u32 minW, u32 minH) {
    minWidth = minW;
    minHeight = minH;
}

void Win32Window::centerOnScreen() {
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    i32 windowW = rect.right - rect.left;
    i32 windowH = rect.bottom - rect.top;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    i32 screenW = workArea.right - workArea.left;
    i32 screenH = workArea.bottom - workArea.top;

    i32 x = workArea.left + (screenW - windowW) / 2;
    i32 y = workArea.top + (screenH - windowH) / 2;

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void Win32Window::setDecorated(bool decor) {
    decorated = decor;
    // For our custom title bar, we already use WS_POPUP style
    // No further action needed since we handle decorations via WM_NCHITTEST
}

void Win32Window::startDrag(i32 /*rootX*/, i32 /*rootY*/) {
    if (!hwnd || maximized) return;

    // Release any mouse capture and trigger a window move
    ReleaseCapture();
    SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

void Win32Window::startResize(i32 direction) {
    if (!hwnd || maximized) return;
    if (direction < 0 || direction > 7) return;

    // Map our resize direction to Windows HT* values
    // Our directions: 0=TOPLEFT, 1=TOP, 2=TOPRIGHT, 3=RIGHT, 4=BOTTOMRIGHT, 5=BOTTOM, 6=BOTTOMLEFT, 7=LEFT
    static const WPARAM htMap[] = {
        HTTOPLEFT, HTTOP, HTTOPRIGHT, HTRIGHT,
        HTBOTTOMRIGHT, HTBOTTOM, HTBOTTOMLEFT, HTLEFT
    };

    ReleaseCapture();
    SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE | htMap[direction], 0);
}

void Win32Window::minimize() {
    if (!hwnd) return;
    ShowWindow(hwnd, SW_MINIMIZE);
}

void Win32Window::maximize() {
    if (!hwnd || maximized) return;

    // Store current geometry for restore
    GetWindowRect(hwnd, &restoreRect);

    ShowWindow(hwnd, SW_MAXIMIZE);
    maximized = true;
}

void Win32Window::restore() {
    if (!hwnd || !maximized) return;

    ShowWindow(hwnd, SW_RESTORE);

    // Restore previous geometry
    if (restoreRect.right > restoreRect.left && restoreRect.bottom > restoreRect.top) {
        SetWindowPos(hwnd, nullptr,
            restoreRect.left, restoreRect.top,
            restoreRect.right - restoreRect.left,
            restoreRect.bottom - restoreRect.top,
            SWP_NOZORDER);
    }

    maximized = false;
}

void Win32Window::toggleMaximize() {
    if (maximized) {
        restore();
    } else {
        maximize();
    }
}

void Win32Window::setCursor(i32 resizeDirection) {
    LPCSTR cursorId;

    switch (resizeDirection) {
        case RESIZE_TOPLEFT:
        case RESIZE_BOTTOMRIGHT:
            cursorId = IDC_SIZENWSE;
            break;
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            cursorId = IDC_SIZENS;
            break;
        case RESIZE_TOPRIGHT:
        case RESIZE_BOTTOMLEFT:
            cursorId = IDC_SIZENESW;
            break;
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            cursorId = IDC_SIZEWE;
            break;
        default:
            cursorId = IDC_ARROW;
            break;
    }

    ::SetCursor(LoadCursorA(nullptr, cursorId));
}

bool Win32Window::createBackBuffer(u32 w, u32 h) {
    destroyBackBuffer();

    // Create DIB section for direct pixel access
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);  // Negative = top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hdcBackBuffer = CreateCompatibleDC(hdcWindow);
    if (!hdcBackBuffer) {
        fprintf(stderr, "Failed to create back buffer DC\n");
        return false;
    }

    hBitmap = CreateDIBSection(hdcBackBuffer, &bmi, DIB_RGB_COLORS,
                               reinterpret_cast<void**>(&backBufferPixels), nullptr, 0);
    if (!hBitmap) {
        DeleteDC(hdcBackBuffer);
        hdcBackBuffer = nullptr;
        fprintf(stderr, "Failed to create back buffer bitmap\n");
        return false;
    }

    hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcBackBuffer, hBitmap));
    backBufferWidth = w;
    backBufferHeight = h;

    return true;
}

void Win32Window::destroyBackBuffer() {
    if (hdcBackBuffer) {
        if (hOldBitmap) {
            SelectObject(hdcBackBuffer, hOldBitmap);
            hOldBitmap = nullptr;
        }
        DeleteDC(hdcBackBuffer);
        hdcBackBuffer = nullptr;
    }

    if (hBitmap) {
        DeleteObject(hBitmap);
        hBitmap = nullptr;
    }

    backBufferPixels = nullptr;
    backBufferWidth = 0;
    backBufferHeight = 0;
}

void Win32Window::present(const u32* pixels, u32 w, u32 h) {
    if (!hwnd || !hdcWindow || !pixels) return;

    // Resize back buffer if needed
    if (w != backBufferWidth || h != backBufferHeight) {
        if (!createBackBuffer(w, h)) {
            return;
        }
    }

    // Copy with RGBA to BGR conversion
    // App pixel format: 0xRRGGBBAA (R in high byte, A in low byte)
    // Windows DIB format: 0x00RRGGBB (B in low byte, high byte unused)
    const u32* src = pixels;
    u32* dst = backBufferPixels;
    u32 count = w * h;

    for (u32 i = 0; i < count; ++i) {
        u32 rgba = src[i];
        u8 r = (rgba >> 24) & 0xFF;
        u8 g = (rgba >> 16) & 0xFF;
        u8 b = (rgba >> 8) & 0xFF;
        // Alpha is ignored for window content
        dst[i] = (r << 16) | (g << 8) | b;
    }

    // Blit to window
    BitBlt(hdcWindow, 0, 0, w, h, hdcBackBuffer, 0, 0, SRCCOPY);
}

void Win32Window::updateDpiScale() {
    dpiScale = 1.0f;

    // Try Windows 10 1607+ API first
    typedef UINT (WINAPI *GetDpiForWindowProc)(HWND);
    static auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowProc>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow"));

    if (pGetDpiForWindow && hwnd) {
        UINT dpi = pGetDpiForWindow(hwnd);
        dpiScale = static_cast<f32>(dpi) / 96.0f;
    } else {
        // Fallback: use system DPI
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            i32 dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            dpiScale = static_cast<f32>(dpi) / 96.0f;
            ReleaseDC(nullptr, hdc);
        }
    }

    // Clamp to reasonable range
    if (dpiScale < 0.5f) dpiScale = 0.5f;
    if (dpiScale > 4.0f) dpiScale = 4.0f;
}

KeyMods Win32Window::getCurrentMods() const {
    KeyMods mods;
    mods.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    mods.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    mods.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    return mods;
}

i32 Win32Window::mapVirtualKey(WPARAM vk) const {
    // Map Windows virtual key codes to our key codes (X11 keysym values)
    switch (vk) {
        // Special keys
        case VK_BACK:   return Key::BACKSPACE;
        case VK_TAB:    return Key::TAB;
        case VK_RETURN: return Key::RETURN;
        case VK_ESCAPE: return Key::ESCAPE;
        case VK_DELETE: return Key::DELETE;
        case VK_SPACE:  return Key::SPACE;

        // Navigation
        case VK_HOME:   return Key::HOME;
        case VK_END:    return Key::END;
        case VK_LEFT:   return Key::LEFT;
        case VK_UP:     return Key::UP;
        case VK_RIGHT:  return Key::RIGHT;
        case VK_DOWN:   return Key::DOWN;
        case VK_PRIOR:  return Key::PAGE_UP;
        case VK_NEXT:   return Key::PAGE_DOWN;

        // Function keys
        case VK_F1:  return Key::F1;
        case VK_F2:  return Key::F2;
        case VK_F3:  return Key::F3;
        case VK_F4:  return Key::F4;
        case VK_F5:  return Key::F5;
        case VK_F6:  return Key::F6;
        case VK_F7:  return Key::F7;
        case VK_F8:  return Key::F8;
        case VK_F9:  return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;

        // Modifiers
        case VK_SHIFT:   return Key::SHIFT_L;
        case VK_LSHIFT:  return Key::SHIFT_L;
        case VK_RSHIFT:  return Key::SHIFT_R;
        case VK_CONTROL: return Key::CONTROL_L;
        case VK_LCONTROL: return Key::CONTROL_L;
        case VK_RCONTROL: return Key::CONTROL_R;
        case VK_MENU:    return Key::ALT_L;
        case VK_LMENU:   return Key::ALT_L;
        case VK_RMENU:   return Key::ALT_R;

        // Number keys (same as ASCII/our values)
        case '0': return Key::KEY_0;
        case '1': return Key::KEY_1;
        case '2': return Key::KEY_2;
        case '3': return Key::KEY_3;
        case '4': return Key::KEY_4;
        case '5': return Key::KEY_5;
        case '6': return Key::KEY_6;
        case '7': return Key::KEY_7;
        case '8': return Key::KEY_8;
        case '9': return Key::KEY_9;

        // Letter keys - Windows uses uppercase, we use lowercase
        case 'A': return Key::A;
        case 'B': return Key::B;
        case 'C': return Key::C;
        case 'D': return Key::D;
        case 'E': return Key::E;
        case 'F': return Key::F;
        case 'G': return Key::G;
        case 'H': return Key::H;
        case 'I': return Key::I;
        case 'J': return Key::J;
        case 'K': return Key::K;
        case 'L': return Key::L;
        case 'M': return Key::M;
        case 'N': return Key::N;
        case 'O': return Key::O;
        case 'P': return Key::P;
        case 'Q': return Key::Q;
        case 'R': return Key::R;
        case 'S': return Key::S;
        case 'T': return Key::T;
        case 'U': return Key::U;
        case 'V': return Key::V;
        case 'W': return Key::W;
        case 'X': return Key::X;
        case 'Y': return Key::Y;
        case 'Z': return Key::Z;

        // Punctuation (OEM keys - US keyboard layout)
        case VK_OEM_1:      return Key::SEMICOLON;    // ;:
        case VK_OEM_PLUS:   return Key::EQUALS;       // =+
        case VK_OEM_COMMA:  return Key::COMMA;        // ,<
        case VK_OEM_MINUS:  return Key::MINUS;        // -_
        case VK_OEM_PERIOD: return Key::PERIOD;       // .>
        case VK_OEM_2:      return Key::SLASH;        // /?
        case VK_OEM_3:      return Key::BACKQUOTE;    // `~
        case VK_OEM_4:      return Key::LEFTBRACKET;  // [{
        case VK_OEM_5:      return Key::BACKSLASH;    // \|
        case VK_OEM_6:      return Key::RIGHTBRACKET; // ]}
        case VK_OEM_7:      return Key::QUOTE;        // '"

        default:
            return static_cast<i32>(vk);
    }
}

bool Win32Window::processEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            if (onCloseRequested) onCloseRequested();
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        // Store window pointer on creation
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<Win32Window*>(cs->lpCreateParams);
        if (window) {
            window->hwnd = hwnd;  // Set hwnd early so handleMessage can use it
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
        // Let Windows do its default NC creation
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (window && window->hwnd) {
        return window->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            if (onCloseRequested) onCloseRequested();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND:
            // Prevent flickering - we handle all drawing
            return TRUE;

        case WM_NCCALCSIZE: {
            // Remove the entire non-client area (title bar, borders)
            // This eliminates the white bar on Windows 11
            if (wParam == TRUE) {
                // Return 0 to indicate we want the entire window as client area
                // But we need to handle maximized windows properly
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                if (IsZoomed(hwnd)) {
                    // When maximized, adjust for the window border that goes off-screen
                    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetMonitorInfo(monitor, &mi)) {
                        params->rgrc[0] = mi.rcWork;
                    }
                }
                return 0;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            // Actual rendering is done via present() from the app
            if (onExpose) onExpose();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE: {
            u32 newWidth = LOWORD(lParam);
            u32 newHeight = HIWORD(lParam);

            // Handle maximize state
            if (wParam == SIZE_MAXIMIZED) {
                maximized = true;
            } else if (wParam == SIZE_RESTORED) {
                maximized = false;
            }

            if (newWidth != width || newHeight != height) {
                width = newWidth;
                height = newHeight;
                if (onResize) onResize(width, height);
            }
            // Force redraw during resize
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_SIZING: {
            // Force continuous redraws during resize drag
            if (onExpose) onExpose();
            return TRUE;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = static_cast<LONG>(minWidth);
            mmi->ptMinTrackSize.y = static_cast<LONG>(minHeight);
            return 0;
        }

        case WM_NCHITTEST: {
            // Custom hit testing for resize borders only
            // Return HTCLIENT for everything else so app receives mouse events
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            // Border size for resize handles
            const i32 borderSize = static_cast<i32>(6 * dpiScale);

            i32 w = static_cast<i32>(width);
            i32 h = static_cast<i32>(height);

            // Check resize edges (unless maximized)
            if (!maximized) {
                bool onLeft = pt.x < borderSize;
                bool onRight = pt.x >= w - borderSize;
                bool onTop = pt.y < borderSize;
                bool onBottom = pt.y >= h - borderSize;

                if (onTop && onLeft) return HTTOPLEFT;
                if (onTop && onRight) return HTTOPRIGHT;
                if (onBottom && onLeft) return HTBOTTOMLEFT;
                if (onBottom && onRight) return HTBOTTOMRIGHT;
                if (onTop) return HTTOP;
                if (onBottom) return HTBOTTOM;
                if (onLeft) return HTLEFT;
                if (onRight) return HTRIGHT;
            }

            // Everything else is client area - app handles dragging via startDrag()
            return HTCLIENT;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            i32 keyCode = mapVirtualKey(wParam);
            KeyMods mods = getCurrentMods();
            bool repeat = (lParam & 0x40000000) != 0;

            if (onKeyDown) {
                onKeyDown(keyCode, 0, mods, repeat);
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            i32 keyCode = mapVirtualKey(wParam);
            KeyMods mods = getCurrentMods();

            if (onKeyUp) {
                onKeyUp(keyCode, 0, mods);
            }
            return 0;
        }

        case WM_CHAR: {
            // Text input (Unicode character)
            if (onTextInput && wParam >= 32) {
                // Convert UTF-16 to UTF-8
                wchar_t wc[2] = { static_cast<wchar_t>(wParam), 0 };
                char utf8[8] = {0};
                WideCharToMultiByte(CP_UTF8, 0, wc, 1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
                onTextInput(utf8);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (onMouseDown) {
                onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Left);
            }
            return 0;

        case WM_LBUTTONUP:
            if (onMouseUp) {
                onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Left);
            }
            return 0;

        case WM_MBUTTONDOWN:
            if (onMouseDown) {
                onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Middle);
            }
            return 0;

        case WM_MBUTTONUP:
            if (onMouseUp) {
                onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Middle);
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (onMouseDown) {
                onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Right);
            }
            return 0;

        case WM_RBUTTONUP:
            if (onMouseUp) {
                onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton::Right);
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (!onMouseMove) return 0;

            i32 currentX = GET_X_LPARAM(lParam);
            i32 currentY = GET_Y_LPARAM(lParam);

            // Get mouse position history to recover coalesced points
            MOUSEMOVEPOINT currentPt = {0};
            currentPt.x = currentX;
            currentPt.y = currentY;
            currentPt.time = GetMessageTime();

            // GetMouseMovePointsEx uses screen coordinates with extra resolution
            POINT screenPt = { currentX, currentY };
            ClientToScreen(hwnd, &screenPt);
            currentPt.x = screenPt.x;
            currentPt.y = screenPt.y;

            MOUSEMOVEPOINT history[64];
            int count = GetMouseMovePointsEx(sizeof(MOUSEMOVEPOINT), &currentPt, history, 64, GMMP_USE_DISPLAY_POINTS);

            if (count > 1 && lastMouseTime != 0) {
                // Find points newer than our last processed point
                // History is returned newest-first, so we need to reverse
                int startIdx = count - 1;
                for (int i = 0; i < count; i++) {
                    // Stop at or before our last processed time
                    if (history[i].time <= lastMouseTime) {
                        startIdx = i - 1;
                        break;
                    }
                }

                // Process historical points oldest-first (skip the current one at index 0)
                for (int i = startIdx; i > 0; i--) {
                    POINT clientPt = { history[i].x, history[i].y };
                    // Handle coordinate wrap-around (coordinates can be negative or > 65535)
                    if (clientPt.x > 32767) clientPt.x -= 65536;
                    if (clientPt.y > 32767) clientPt.y -= 65536;
                    ScreenToClient(hwnd, &clientPt);
                    onMouseMove(clientPt.x, clientPt.y);
                }
            }

            // Always process the current point
            onMouseMove(currentX, currentY);

            // Update last processed position
            lastMouseTime = GetMessageTime();
            lastMouseX = currentX;
            lastMouseY = currentY;

            return 0;
        }

        case WM_MOUSEWHEEL: {
            // Get wheel delta (positive = up, negative = down)
            i32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
            i32 clicks = delta / WHEEL_DELTA;

            // Convert screen coords to client coords
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            if (onMouseWheel) {
                onMouseWheel(pt.x, pt.y, clicks);
            }
            return 0;
        }

        case WM_DROPFILES: {
            // Handle file drag and drop
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

            if (fileCount > 0 && onFileDrop) {
                // Get first file
                UINT len = DragQueryFileW(hDrop, 0, nullptr, 0) + 1;
                std::wstring widePath(len, 0);
                DragQueryFileW(hDrop, 0, &widePath[0], len);

                // Convert to UTF-8
                i32 utf8Len = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string path(utf8Len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, &path[0], utf8Len, nullptr, nullptr);

                // Check if it's a supported file type
                size_t dotPos = path.rfind('.');
                if (dotPos != std::string::npos) {
                    std::string ext = path.substr(dotPos);
                    // Convert to lowercase
                    for (char& c : ext) {
                        if (c >= 'A' && c <= 'Z') c += 32;
                    }
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                        ext == ".bmp" || ext == ".gif" || ext == ".pp") {
                        onFileDrop(path);
                    }
                }
            }

            DragFinish(hDrop);
            return 0;
        }

        case WM_DPICHANGED: {
            // Handle DPI change (e.g., moving window to a different monitor)
            updateDpiScale();

            // Resize window to suggested size
            RECT* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_SETCURSOR: {
            // Handle cursor for client area - set default arrow
            if (LOWORD(lParam) == HTCLIENT) {
                ::SetCursor(LoadCursorA(nullptr, IDC_ARROW));
                return TRUE;
            }
            // Let Windows handle non-client area cursors (resize cursors)
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
