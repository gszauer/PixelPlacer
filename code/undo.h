#ifndef _H_UNDO_
#define _H_UNDO_

#include "types.h"
#include "tile.h"
#include "layer.h"
#include "selection.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <optional>

// Forward declaration
class Document;

// Stores original tiles before a pixel operation
struct TileDelta {
    i32 layerIndex = -1;

    // Original tiles (before the operation)
    std::unordered_map<u64, std::unique_ptr<Tile>> originalTiles;

    // New tiles (after the operation, for redo)
    std::unordered_map<u64, std::unique_ptr<Tile>> newTiles;

    TileDelta() = default;
    TileDelta(TileDelta&&) = default;
    TileDelta& operator=(TileDelta&&) = default;

    // Non-copyable due to unique_ptr
    TileDelta(const TileDelta&) = delete;
    TileDelta& operator=(const TileDelta&) = delete;
};

// Stores a complete layer for structural operations (add/remove)
struct LayerSnapshot {
    i32 layerIndex = -1;
    std::unique_ptr<LayerBase> layer;

    LayerSnapshot() = default;
    LayerSnapshot(LayerSnapshot&&) = default;
    LayerSnapshot& operator=(LayerSnapshot&&) = default;

    // Non-copyable
    LayerSnapshot(const LayerSnapshot&) = delete;
    LayerSnapshot& operator=(const LayerSnapshot&) = delete;
};

// Stores selection state
struct SelectionSnapshot {
    Selection selection;

    SelectionSnapshot() = default;
    SelectionSnapshot(SelectionSnapshot&&) = default;
    SelectionSnapshot& operator=(SelectionSnapshot&&) = default;
    SelectionSnapshot(const SelectionSnapshot&) = default;
    SelectionSnapshot& operator=(const SelectionSnapshot&) = default;
};

// Type of undo step
enum class UndoStepType {
    PixelEdit,      // Brush stroke, eraser, fill, etc.
    LayerAdd,       // Layer was added
    LayerRemove,    // Layer was removed
    SelectionChange // Selection was modified
};

// A single undoable action
struct UndoStep {
    std::string name;  // Human readable: "Brush Stroke", "Delete Layer", etc.
    UndoStepType type = UndoStepType::PixelEdit;

    // Exactly ONE of these is populated per step (based on type)
    std::optional<TileDelta> tileDelta;
    std::optional<LayerSnapshot> layerSnapshot;
    std::optional<SelectionSnapshot> selectionSnapshot;

    UndoStep() = default;
    UndoStep(const std::string& n, UndoStepType t) : name(n), type(t) {}

    UndoStep(UndoStep&&) = default;
    UndoStep& operator=(UndoStep&&) = default;

    // Non-copyable due to TileDelta and LayerSnapshot
    UndoStep(const UndoStep&) = delete;
    UndoStep& operator=(const UndoStep&) = delete;
};

// Per-document undo history
class UndoHistory {
public:
    UndoHistory() = default;

    // Push a completed undo step
    void pushStep(UndoStep step);

    // Check if undo/redo is available
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    // Pop step from undo stack (caller is responsible for executing undo logic)
    // Returns moved step, also moves it to redo stack after modification
    UndoStep& peekUndo();
    void moveTopToRedo();

    // Pop step from redo stack (for redo execution)
    UndoStep& peekRedo();
    void moveTopToUndo();

    // Clear redo stack (called when new action is performed)
    void clearRedo() { redoStack.clear(); }

    // Clear all history
    void clear() {
        undoStack.clear();
        redoStack.clear();
    }

    // Get undo step count
    size_t undoCount() const { return undoStack.size(); }
    size_t redoCount() const { return redoStack.size(); }

    // Get name of next undo/redo action (for menu display)
    const std::string& getUndoName() const;
    const std::string& getRedoName() const;

private:
    std::vector<UndoStep> undoStack;
    std::vector<UndoStep> redoStack;

    static constexpr size_t MAX_UNDO_STEPS = 20;

    void enforceLimit();
};

#endif
