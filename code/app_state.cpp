#include "app_state.h"
#include "document.h"
#include "config.h"

// Runtime UI scale definition
namespace Config {
    f32 uiScale = 2.0f;
}

// Global application state
static AppState g_appState;

AppState& getAppState() {
    return g_appState;
}

Document* AppState::createDocument(u32 width, u32 height, const std::string& name) {
    auto doc = std::make_unique<Document>(width, height, name);
    Document* ptr = doc.get();
    documents.push_back(std::move(doc));
    setActiveDocument(ptr);
    return ptr;
}

void AppState::closeDocument(Document* doc) {
    for (size_t i = 0; i < documents.size(); ++i) {
        if (documents[i].get() == doc) {
            closeDocument(static_cast<i32>(i));
            return;
        }
    }
}

void AppState::closeDocument(i32 index) {
    if (index < 0 || index >= static_cast<i32>(documents.size())) return;

    documents.erase(documents.begin() + index);

    // Update active document
    if (documents.empty()) {
        activeDocument = nullptr;
        activeDocumentIndex = -1;
    } else {
        if (activeDocumentIndex >= static_cast<i32>(documents.size())) {
            activeDocumentIndex = documents.size() - 1;
        }
        activeDocument = documents[activeDocumentIndex].get();
    }
}

void AppState::setActiveDocument(i32 index) {
    if (index < 0 || index >= static_cast<i32>(documents.size())) {
        activeDocument = nullptr;
        activeDocumentIndex = -1;
        if (onActiveDocumentChanged) onActiveDocumentChanged();
        return;
    }
    activeDocumentIndex = index;
    activeDocument = documents[index].get();
    if (onActiveDocumentChanged) onActiveDocumentChanged();
}

void AppState::setActiveDocument(Document* doc) {
    for (size_t i = 0; i < documents.size(); ++i) {
        if (documents[i].get() == doc) {
            setActiveDocument(static_cast<i32>(i));
            return;
        }
    }
}

void AppState::requestScaleChange(f32 newScale) {
    pendingScaleChange = true;
    pendingScaleValue = newScale;
}

void AppState::requestOpenFileDialog(const std::string& title, const std::string& filters,
                                     std::function<void(const std::string&)> callback) {
    pendingFileDialog.active = true;
    pendingFileDialog.isSaveDialog = false;
    pendingFileDialog.title = title;
    pendingFileDialog.defaultName = "";
    pendingFileDialog.filters = filters;
    pendingFileDialog.callback = callback;
}

void AppState::requestSaveFileDialog(const std::string& title, const std::string& defaultName,
                                     const std::string& filters,
                                     std::function<void(const std::string&)> callback) {
    pendingFileDialog.active = true;
    pendingFileDialog.isSaveDialog = true;
    pendingFileDialog.title = title;
    pendingFileDialog.defaultName = defaultName;
    pendingFileDialog.filters = filters;
    pendingFileDialog.callback = callback;
}

void AppState::requestFileDialog(const std::string& title, const std::string& filters,
                                 std::function<void(const std::string&)> callback) {
    requestOpenFileDialog(title, filters, callback);
}

void AppState::swapColors() {
    std::swap(foregroundColor, backgroundColor);
}

void AppState::resetColors() {
    foregroundColor = Color::black();
    backgroundColor = Color::white();
}

// Evaluate cubic bezier pressure curve
// Input: raw pressure (0-1), control points
// Output: adjusted pressure (0-1)
f32 evaluatePressureCurve(f32 inputPressure, Vec2 cp1, Vec2 cp2) {
    // Fixed endpoints: P0=(0,0), P3=(1,1)
    // Control points: P1=cp1, P2=cp2
    // We need to find Y given X (the input pressure)
    // Use binary search to find t where bezierX(t) ≈ inputPressure

    f32 t = inputPressure;  // Initial guess

    // Binary search for t
    f32 low = 0.0f, high = 1.0f;
    for (int i = 0; i < 10; ++i) {  // 10 iterations gives ~0.001 precision
        t = (low + high) * 0.5f;

        // Calculate bezier X at t
        f32 mt = 1.0f - t;
        f32 mt2 = mt * mt;
        f32 mt3 = mt2 * mt;
        f32 t2 = t * t;
        f32 t3 = t2 * t;

        // X = (1-t)³*0 + 3(1-t)²t*cp1.x + 3(1-t)t²*cp2.x + t³*1
        f32 x = 3.0f * mt2 * t * cp1.x + 3.0f * mt * t2 * cp2.x + t3;

        if (x < inputPressure) {
            low = t;
        } else {
            high = t;
        }
    }

    // Now calculate Y at the found t
    f32 mt = 1.0f - t;
    f32 mt2 = mt * mt;
    f32 mt3 = mt2 * mt;
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    // Y = (1-t)³*0 + 3(1-t)²t*cp1.y + 3(1-t)t²*cp2.y + t³*1
    f32 y = 3.0f * mt2 * t * cp1.y + 3.0f * mt * t2 * cp2.y + t3;

    return std::max(0.0f, std::min(1.0f, y));
}
