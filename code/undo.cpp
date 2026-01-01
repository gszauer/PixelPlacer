#include "undo.h"

// Static empty string for when there's no undo/redo available
static const std::string emptyString;

void UndoHistory::pushStep(UndoStep step) {
    // Clear redo stack when new action is performed
    clearRedo();

    // Add to undo stack
    undoStack.push_back(std::move(step));

    // Enforce limit
    enforceLimit();
}

UndoStep& UndoHistory::peekUndo() {
    return undoStack.back();
}

void UndoHistory::moveTopToRedo() {
    if (!undoStack.empty()) {
        redoStack.push_back(std::move(undoStack.back()));
        undoStack.pop_back();
    }
}

UndoStep& UndoHistory::peekRedo() {
    return redoStack.back();
}

void UndoHistory::moveTopToUndo() {
    if (!redoStack.empty()) {
        undoStack.push_back(std::move(redoStack.back()));
        redoStack.pop_back();
    }
}

const std::string& UndoHistory::getUndoName() const {
    if (undoStack.empty()) return emptyString;
    return undoStack.back().name;
}

const std::string& UndoHistory::getRedoName() const {
    if (redoStack.empty()) return emptyString;
    return redoStack.back().name;
}

void UndoHistory::enforceLimit() {
    // Remove oldest entries if we exceed the limit
    while (undoStack.size() > MAX_UNDO_STEPS) {
        undoStack.erase(undoStack.begin());
    }
}
