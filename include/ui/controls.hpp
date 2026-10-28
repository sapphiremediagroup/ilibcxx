#pragma once

// Controls: rectangles inside a window that draw themselves and take input
// (Win32's child windows, without a kernel window each). They form a tree
// under the window; a control is made with its parent (the window or a
// Panel) and lives as long as it is in scope, usually as a member of the
// app's window class:
//
//     class App : public ui::Window {
//         ui::Button ok_{this, "OK"};
//         void on_create() override {
//             ok_.set_bounds({ 8, 8, 80, 30 }).set_anchor(ui::AnchorRight | ui::AnchorBottom);
//             ok_.on_click = [this] { close(); };
//         }
//     };
//
// The window routes input: the mouse to the control under it (a press holds
// it until release), keys to the focused control and then to the window;
// Tab and Shift+Tab move the focus, Enter presses the default button and
// Escape the cancel button.

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <new.hpp>
#include <ui/theme.hpp>
#include <ui/window.hpp>

namespace ui {

// ---- callbacks ---------------------------------------------------------------

// A callable kept by value (a lambda with a few captures); empty until set.
template <typename Signature>
class Callback;

template <typename R, typename... Args>
class Callback<R(Args...)> {
public:
    Callback() = default;
    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;
    ~Callback() { reset(); }

    template <typename F>
    Callback& operator=(F function) {
        static_assert(sizeof(F) <= sizeof(storage_), "ui::Callback: the lambda captures too much");
        static_assert(alignof(F) <= alignof(void*), "ui::Callback: over-aligned lambda");
        reset();
        new (storage_) F(static_cast<F&&>(function));
        invoke_ = [](void* self, Args... args) -> R { return (*static_cast<F*>(self))(args...); };
        destroy_ = [](void* self) { static_cast<F*>(self)->~F(); };
        return *this;
    }

    explicit operator bool() const { return invoke_ != nullptr; }
    R operator()(Args... args) const { return invoke_(const_cast<unsigned char*>(storage_), args...); }

    void reset() {
        if (destroy_) {
            destroy_(storage_);
        }
        invoke_ = nullptr;
        destroy_ = nullptr;
    }

private:
    alignas(void*) unsigned char storage_[48] = {};
    R (*invoke_)(void*, Args...) = nullptr;
    void (*destroy_)(void*) = nullptr;
};

// ---- the base ----------------------------------------------------------------

// Which parent edges a control keeps its distance to as the parent resizes
// (WinForms anchors). Left|Right stretches it, Top|Bottom too.
enum Anchor : std::uint32_t {
    AnchorLeft = 1,
    AnchorTop = 2,
    AnchorRight = 4,
    AnchorBottom = 8,
    AnchorAll = 15,
};

// Docked controls take a strip of their parent's remaining space, in the
// order they were made; Fill takes what is left (WinForms docking).
enum class Dock : std::uint8_t { None, Top, Bottom, Left, Right, Fill };

class Control {
public:
    explicit Control(Window* window);
    explicit Control(Control* parent);
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    virtual ~Control();

    // Where it is in its parent. Setting it also records the distances the
    // anchors keep.
    Control& set_bounds(const Rect& bounds);
    const Rect& bounds() const { return bounds_; }
    Control& set_anchor(std::uint32_t anchor);
    Control& set_dock(Dock dock);
    Control& set_visible(bool visible);
    Control& set_enabled(bool enabled);
    bool visible() const { return visible_; }
    bool enabled() const;

    bool focused() const;
    void focus();
    // Marks the control (or part of it, in its own coordinates) for repaint.
    void invalidate();
    void invalidate(const Rect& rect);

    // Its rectangle in the window's content.
    Rect window_rect() const;
    Window* window() const { return window_; }
    Control* parent() const { return parent_; }

    // Places the children (anchors and docking) in this control's size; the
    // window does it on every resize.
    void layout();

protected:
    // Paints the control at (0,0)..(width,height) of the canvas.
    virtual void paint(Canvas& canvas);
    virtual bool key_down(const KeyEvent& key) { (void)key; return false; }
    virtual bool char_typed(const char* text) { (void)text; return false; }
    // Mouse positions are in the control's own coordinates.
    virtual void mouse_down(const MouseEvent& mouse) { (void)mouse; }
    virtual void mouse_up(const MouseEvent& mouse) { (void)mouse; }
    virtual void mouse_move(const MouseEvent& mouse) { (void)mouse; }
    virtual void mouse_enter() {}
    virtual void mouse_leave() {}
    virtual bool wheel(int delta) { (void)delta; return false; }
    virtual void focus_changed(bool focused) { (void)focused; }
    virtual bool focusable() const { return false; }
    // The control's size changed (it may place its own parts).
    virtual void resized() {}

    bool hovered() const;
    bool pressed_capture() const;

private:
    friend class Window;

    void attach(Control* parent);
    Control* hit(int x, int y);
    void paint_tree(Canvas& canvas);
    void apply_anchor(const Size& parent);
    void record_anchor();

    Window* window_ = nullptr;
    Control* parent_ = nullptr;
    Control* firstChild_ = nullptr;
    Control* lastChild_ = nullptr;
    Control* next_ = nullptr;
    Control* previous_ = nullptr;
    Rect bounds_ = {};
    std::uint32_t anchor_ = AnchorLeft | AnchorTop;
    Dock dock_ = Dock::None;
    bool visible_ = true;
    bool enabled_ = true;
    // Distances to the parent's right and bottom when the bounds were set.
    int rightGap_ = 0;
    int bottomGap_ = 0;
};

// ---- controls ---------------------------------------------------------------

// Groups controls; can paint a background and a border.
class Panel : public Control {
public:
    using Control::Control;
    Panel& set_background(Color color) { background_ = color; hasBackground_ = true; invalidate(); return *this; }
    Panel& set_border(bool border) { border_ = border; invalidate(); return *this; }

protected:
    void paint(Canvas& canvas) override;

private:
    Color background_ = {};
    bool hasBackground_ = false;
    bool border_ = false;
};

// One line of text (cut with ".." when it does not fit).
class Label : public Control {
public:
    Label(Window* window, const char* text = "");
    Label(Control* parent, const char* text = "");
    Label& set_text(const char* text);
    const char* text() const { return text_; }
    Label& set_align(std::uint32_t align) { align_ = align; invalidate(); return *this; }
    Label& set_color(Color color) { color_ = color; hasColor_ = true; invalidate(); return *this; }

protected:
    void paint(Canvas& canvas) override;

private:
    char text_[160] = {};
    std::uint32_t align_ = AlignLeft | AlignMiddle;
    Color color_ = {};
    bool hasColor_ = false;
};

class Button : public Control {
public:
    Button(Window* window, const char* text = "");
    Button(Control* parent, const char* text = "");
    Button& set_text(const char* text);
    const char* text() const { return text_; }
    // Pressed by the mouse, Space or Enter when focused, or Enter (the
    // window's default button) / Escape (its cancel button) anywhere.
    void click();

    Callback<void()> on_click;

protected:
    void paint(Canvas& canvas) override;
    bool key_down(const KeyEvent& key) override;
    void mouse_down(const MouseEvent& mouse) override;
    void mouse_up(const MouseEvent& mouse) override;
    void mouse_move(const MouseEvent& mouse) override;
    void mouse_enter() override { invalidate(); }
    void mouse_leave() override { invalidate(); }
    void focus_changed(bool) override { invalidate(); }
    bool focusable() const override { return true; }

private:
    char text_[64] = {};
    bool pressed_ = false;   // held down with the pointer still on it
};

class CheckBox : public Control {
public:
    CheckBox(Window* window, const char* text = "");
    CheckBox(Control* parent, const char* text = "");
    CheckBox& set_checked(bool checked);
    bool checked() const { return checked_; }
    CheckBox& set_text(const char* text);

    Callback<void(bool)> on_change;

protected:
    void paint(Canvas& canvas) override;
    bool key_down(const KeyEvent& key) override;
    void mouse_up(const MouseEvent& mouse) override;
    void mouse_enter() override { invalidate(); }
    void mouse_leave() override { invalidate(); }
    void focus_changed(bool) override { invalidate(); }
    bool focusable() const override { return true; }

private:
    char text_[96] = {};
    bool checked_ = false;
};

// One line of editable text: caret, selection (Shift with the keys, or the
// mouse), Home/End, Ctrl+Left/Right by word, Ctrl+A, Backspace/Delete.
class TextBox : public Control {
public:
    static constexpr std::size_t kMaxLength = 255;

    using Control::Control;
    TextBox& set_text(const char* text);
    const char* text() const { return text_; }
    TextBox& set_placeholder(const char* text);
    void select_all();

    Callback<void(const char*)> on_change;
    Callback<void(const char*)> on_enter;

protected:
    void paint(Canvas& canvas) override;
    bool key_down(const KeyEvent& key) override;
    bool char_typed(const char* text) override;
    void mouse_down(const MouseEvent& mouse) override;
    void mouse_move(const MouseEvent& mouse) override;
    void focus_changed(bool) override { invalidate(); }
    bool focusable() const override { return true; }

private:
    std::size_t index_at(int x) const;
    void move_caret(std::size_t to, bool extend);
    void erase_selection();
    void insert(const char* text);
    void keep_caret_visible();
    void changed();

    char text_[kMaxLength + 1] = {};
    char placeholder_[64] = {};
    std::size_t length_ = 0;
    std::size_t caret_ = 0;
    std::size_t anchor_ = 0;     // the other end of the selection
    int scroll_ = 0;             // pixels of text hidden on the left
};

class ScrollBar : public Control {
public:
    ScrollBar(Window* window, bool vertical = true);
    ScrollBar(Control* parent, bool vertical = true);
    // `total` units, of which `page` show at a time, the first at `value`.
    ScrollBar& set_range(int total, int page);
    ScrollBar& set_value(int value);
    int value() const { return value_; }
    int max_value() const { return total_ > page_ ? total_ - page_ : 0; }

    Callback<void(int)> on_scroll;

protected:
    void paint(Canvas& canvas) override;
    void mouse_down(const MouseEvent& mouse) override;
    void mouse_move(const MouseEvent& mouse) override;
    void mouse_up(const MouseEvent& mouse) override;
    bool wheel(int delta) override;

private:
    Rect thumb() const;
    int length() const;
    void scroll_to(int value);

    bool vertical_;
    int total_ = 0;
    int page_ = 0;
    int value_ = 0;
    int dragOffset_ = -1;        // pointer to thumb start while dragging
};

// Rows from callbacks, so a long list costs nothing: `row_text` names a row
// (for the default drawing and type-to-jump), or `draw_row` draws it.
// Arrows, Page Up/Down, Home/End, a letter jumps; Enter or a double click
// activates.
class ListBox : public Control {
public:
    ListBox(Window* window);
    ListBox(Control* parent);
    ListBox& set_count(int count);
    int count() const { return count_; }
    ListBox& set_selected(int row);
    int selected() const { return selected_; }
    void ensure_visible(int row);

    Callback<const char*(int)> row_text;
    Callback<void(Canvas&, int, const Rect&, bool)> draw_row;
    Callback<void(int)> on_select;
    Callback<void(int)> on_activate;

protected:
    void paint(Canvas& canvas) override;
    bool key_down(const KeyEvent& key) override;
    bool char_typed(const char* text) override;
    void mouse_down(const MouseEvent& mouse) override;
    bool wheel(int delta) override;
    void focus_changed(bool) override { invalidate(); }
    bool focusable() const override { return true; }
    void resized() override;

private:
    int visible_rows() const;
    void set_top(int top);
    void update_scroll_bar();

    ScrollBar scrollBar_;
    int count_ = 0;
    int selected_ = -1;
    int top_ = 0;
};

class ImageView : public Control {
public:
    enum class Fit : std::uint8_t { Contain, Cover, Center, Stretch };

    using Control::Control;
    // The image is not copied: it must outlive the view.
    ImageView& set_image(const Image* image) { image_ = image; scaled_.reset(); invalidate(); return *this; }
    ImageView& set_fit(Fit fit) { fit_ = fit; invalidate(); return *this; }

protected:
    void paint(Canvas& canvas) override;

private:
    const Image* image_ = nullptr;
    Fit fit_ = Fit::Contain;
    Image scaled_;          // image_ at the size last drawn, made once
};

}
