#ifndef _H_DIALOGS_
#define _H_DIALOGS_

#include "widget.h"
#include "layouts.h"
#include "basic_widgets.h"
#include "overlay_manager.h"
#include "config.h"
#include <functional>

// Base dialog class
class Dialog : public Panel {
public:
    std::string title;
    bool modal = true;

    std::function<void()> onClose;

    Dialog(const std::string& t);

    virtual void show();
    virtual void hide();
    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
};

// New document dialog
class NewDocumentDialog : public Dialog {
public:
    TextField* widthField = nullptr;
    TextField* heightField = nullptr;
    TextField* nameField = nullptr;

    std::function<void(const std::string&, u32, u32)> onConfirm;

    NewDocumentDialog();
    void show() override;
};

// Anchor grid widget for canvas resize (3x3 Photoshop-style)
class AnchorGridWidget : public Widget {
public:
    i32 selectedX = 0;  // -1 = left, 0 = center, 1 = right
    i32 selectedY = 0;  // -1 = top, 0 = center, 1 = bottom

    std::function<void()> onChanged;

    AnchorGridWidget();
    void render(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
};

// Canvas size dialog
class CanvasSizeDialog : public Dialog {
public:
    TextField* widthField = nullptr;
    TextField* heightField = nullptr;
    AnchorGridWidget* anchorGrid = nullptr;
    u32 newWidth = 1920;
    u32 newHeight = 1080;

    std::function<void(u32, u32, i32, i32)> onConfirm;  // width, height, anchorX, anchorY

    CanvasSizeDialog();
    void show() override;
};

// Rename document dialog
class RenameDocumentDialog : public Dialog {
public:
    TextField* nameField = nullptr;

    std::function<void(const std::string&)> onConfirm;

    RenameDocumentDialog();
    void show() override;
};

// Confirm dialog
class ConfirmDialog : public Dialog {
public:
    Label* messageLabel = nullptr;
    std::function<void(bool)> onResult;

    ConfirmDialog(const std::string& message = "");
    void setMessage(const std::string& msg);
};

// Clickable hyperlink label
class LinkLabel : public Widget {
public:
    std::string text;
    std::string url;
    u32 textColor = Config::GRAY_700;
    u32 hoverColor = Config::GRAY_800;
    f32 fontSize;
    bool hovered = false;

    LinkLabel(const std::string& txt, const std::string& link);
    void renderSelf(Framebuffer& fb) override;
    bool onMouseMove(const MouseEvent& e) override;
    bool onMouseDown(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent&) override;
};

// About dialog
class AboutDialog : public Dialog {
public:
    AboutDialog();
};

#endif
