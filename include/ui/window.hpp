#pragma once

// The window half of the UI library (User32's role): a top-level window, its
// message loop, painting, timers and input. An app derives from ui::Window,
// overrides the on_* methods it cares about and calls ui::run<App>().
//
// The loop sleeps until something happens. Each event becomes one on_* call;
// once the queued events and due timers are handled, the parts invalidate()
// marked are painted in one on_paint call, clipped to them, and only they
// are sent to the compositor. A resize gives the window a new buffer of the
// new size, calls on_size and repaints everything.

#include <cstdint.hpp>
#include <new.hpp>
#include <syscall.hpp>
#include <ui/graphics.hpp>

namespace ui {

class Button;
class Control;

struct KeyEvent {
    std::uint16_t key;          // std::KeyCode (the physical key)
    std::uint16_t modifiers;    // std::KeyModifier* held or locked
    bool repeat;                // auto-repeat of a held key
    char text[8];               // what it types (US layout), "" if nothing

    bool shift() const { return (modifiers & std::KeyModifierShift) != 0; }
    bool ctrl() const { return (modifiers & std::KeyModifierControl) != 0; }
    bool alt() const { return (modifiers & std::KeyModifierAlt) != 0; }
};

enum MouseButton : std::uint16_t {
    MouseLeft = 1,
    MouseRight = 2,
    MouseMiddle = 4,
};

struct MouseEvent {
    int x;                      // in the window's content
    int y;
    std::uint16_t buttons;      // MouseButton bits held now
    std::uint16_t button;       // the one that went down or up (0 for moves)
    int clicks;                 // 2 for the second press of a double click
};

struct WindowOptions {
    int width = 640;
    int height = 480;
    const char* title = "Window";
    std::uint32_t flags = 0;
};

class Window {
public:
    Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    virtual ~Window();

    // Opens the window and runs its loop until it is closed. Returns the
    // exit code (1 if the window could not be made).
    int run(const WindowOptions& options);

    // Marks part of the window (all of it) to be painted.
    void invalidate();
    void invalidate(const Rect& rect);
    // Paints what is marked now rather than when the loop gets to it.
    void update();

    // A timer calling on_timer(id) every `ms` (once, without repeat).
    // Returns its id (> 0), or 0 when no timer is free.
    int set_timer(int ms, bool repeat = true);
    void kill_timer(int id);

    void set_title(const char* title);
    Size client_size() const { return { width_, height_ }; }
    bool focused() const { return focused_; }
    // Ends the loop after the current message (on_close is not asked).
    void close();
    // Whether the key (a std::KeyCode) is held, as far as this window saw.
    bool key_held(std::uint16_t key) const;

    // The controls (ui/controls.hpp): the window's content is the root of
    // their tree; made with `this`, a control lands under it.
    Control* root() const { return root_; }
    Control* focused_control() const { return focus_; }
    // Moves the keyboard focus to `control` (null: to the window itself).
    void focus(Control* control);
    // Enter anywhere presses the default button, Escape the cancel button.
    void set_default_button(Button* button) { defaultButton_ = button; }
    void set_cancel_button(Button* button) { cancelButton_ = button; }
    Button* default_button() const { return defaultButton_; }
    // Mouse events go to `control` until release, wherever the pointer is.
    void capture_mouse(Control* control) { capture_ = control; }
    Control* mouse_capture() const { return capture_; }
    Control* hovered_control() const { return hover_; }

protected:
    // Called once the window has its first buffer; make the controls' layout
    // here (client_size() is known).
    virtual void on_create() {}
    // Paints `dirty` (canvas is clipped to it); the default fills it with
    // the window background.
    virtual void on_paint(Canvas& canvas, const Rect& dirty);
    // The window has a new size and buffer; everything will be repainted.
    virtual void on_size(Size size) { (void)size; }
    // Return true when handled. Keys go to the focused control first; what
    // it leaves comes here, then to the defaults (Tab, Enter, Escape). A key
    // that types something and is not handled goes on to on_char. Keys with
    // Super held are the desktop's and never arrive.
    virtual bool on_key_down(const KeyEvent& key) { (void)key; return false; }
    virtual bool on_key_up(const KeyEvent& key) { (void)key; return false; }
    virtual bool on_char(const char* text, const KeyEvent& key) { (void)text; (void)key; return false; }
    // The mouse over the window's own background (not over a control).
    virtual void on_mouse_move(const MouseEvent& mouse) { (void)mouse; }
    virtual void on_mouse_down(const MouseEvent& mouse) { (void)mouse; }
    virtual void on_mouse_up(const MouseEvent& mouse) { (void)mouse; }
    // Positive delta: away from the user (scroll up).
    virtual void on_wheel(const MouseEvent& mouse, int delta) { (void)mouse; (void)delta; }
    virtual void on_focus(bool focused) { (void)focused; }
    virtual void on_timer(int id) { (void)id; }
    // The close button: return false to stay open.
    virtual bool on_close() { return true; }
    virtual void on_destroy() {}

private:
    friend class Control;

    struct Timer {
        int id;
        std::uint64_t intervalMs;
        std::uint64_t dueMs;
        bool repeat;
    };
    static constexpr int kMaxTimers = 16;

    bool open(const WindowOptions& options);
    bool make_buffer(int width, int height);
    void teardown();
    void dispatch(const std::Event& event);
    void dispatch_key(const std::KeyEvent& event);
    void dispatch_pointer(const std::PointerEvent& event);
    void run_due_timers();
    std::uint64_t next_wait_ms() const;
    void paint();
    void move_focus(bool backwards);
    void forget(Control* control);
    void set_hover(Control* control);

    std::Handle compositor_;
    std::Handle window_;
    std::Handle surface_;          // the newest surface
    std::Handle shownSurface_;     // the one the window shows (until surface_ is painted)
    std::Handle events_;
    std::uint32_t* pixels_ = nullptr;   // surface_'s pixels
    std::uint32_t* back_ = nullptr;     // painting goes here first
    Control* root_ = nullptr;
    Control* focus_ = nullptr;
    Control* capture_ = nullptr;
    Control* hover_ = nullptr;
    Button* defaultButton_ = nullptr;
    Button* cancelButton_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int pendingWidth_ = 0;
    int pendingHeight_ = 0;
    Rect dirty_ = {};
    bool running_ = false;
    bool focused_ = false;
    std::uint16_t buttons_ = 0;
    std::uint64_t lastPressMs_ = 0;
    int lastPressX_ = 0;
    int lastPressY_ = 0;
    std::uint16_t lastPressButton_ = 0;
    int clicks_ = 0;
    std::uint8_t keysHeld_[32] = {};
    Timer timers_[kMaxTimers] = {};
    int nextTimerId_ = 1;
};

// Makes the app's window and runs it: `return ui::run<MyWindow>(options);`
// from main. The window lives for the whole program.
template <typename T>
int run(const WindowOptions& options) {
    alignas(T) static unsigned char storage[sizeof(T)];
    T* window = new (storage) T();
    return window->run(options);
}

}
