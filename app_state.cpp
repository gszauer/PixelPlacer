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
