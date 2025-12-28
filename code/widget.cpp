#include "widget.h"
#include "app_state.h"

// Clear any global references to this widget when destroyed
Widget::~Widget() {
    AppState& state = getAppState();
    if (state.focusedWidget == this) {
        state.focusedWidget = nullptr;
    }
    if (state.hoveredWidget == this) {
        state.hoveredWidget = nullptr;
    }
    if (state.capturedWidget == this) {
        state.capturedWidget = nullptr;
    }
}
